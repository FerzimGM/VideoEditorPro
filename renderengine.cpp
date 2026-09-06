#include "renderengine.h"
#include "mediadecoder.h"
#include "mediaencoder.h"
#include <QDebug>
#include <QColor>
#include <algorithm>
#include <cmath>

// RenderWorker runs on a background thread.

// Forward declarations for Effects functions used before namespace definition
namespace Effects {
    QImage transitionFade(const QImage& frame, float progress, bool fadeIn);
    QImage transitionWipe(const QImage& from, const QImage& to, float progress, bool rightToLeft);
    QImage transitionZoom(const QImage& from, const QImage& to, float progress, bool zoomIn);
    QImage transitionFlash(const QImage& from, const QImage& to, float progress);
    QImage grain(const QImage& frame, double strength, int frameIndex);
    QImage chromaKey(const QImage& frame, double threshold, double smoothness);
}

RenderWorker::RenderWorker(QObject* parent)
    : QObject(parent)
    , m_outputWidth(1920)
    , m_outputHeight(1080)
    , m_fps(30.0)
    , m_bitrate(5000000)
    , m_cancelled(false)
{}

RenderWorker::~RenderWorker()
{
    closeAllDecoders();
}

void RenderWorker::process()
{
    qDebug() << "RenderWorker::process() START";
    m_cancelled = false;

    if (m_clips.isEmpty()) {
        emit errorOccurred("No clips to render");
        emit renderFinished(false);
        return;
    }

    double totalDuration = 0.0;
    for (const TimelineClip& clip : m_clips)
    {
        double end = clip.endTime();
        if (end > totalDuration) totalDuration = end;
    }

    MediaEncoder encoder;
    encoder.setFrameRate(m_fps);
    encoder.setBitrate(m_bitrate);
    encoder.setAudioEnabled(true);
    encoder.setFormat(m_format); // Explicit format, independent of the file extension.

    if (!encoder.createOutputFile(m_outputPath, m_outputWidth, m_outputHeight))
    {
        emit errorOccurred("Failed to create output file");
        emit renderFinished(false);
        return;
    }

    double frameTime = 1.0 / m_fps;
    int totalFrames = static_cast<int>(std::ceil(totalDuration * m_fps));
    int currentFrame = 0;
    int lastPercent = -1;

    // Audio sample counter: computed as an exact sample count rather than
    // float * SR. Truncating int totalFloats = (int)(frameTime * SR * CH)
    // drops roughly 2 samples per frame, so the audio track ends up
    // shorter than the video and finishes early.
    int64_t audioSamplesWritten = 0;
    const int SR = MediaDecoder::OUTPUT_SAMPLE_RATE;
    const int CH = MediaDecoder::OUTPUT_CHANNELS;

    for (int f = 0; f < totalFrames && !m_cancelled; ++f)
    {
        // Derive time from the frame index rather than accumulating a
        // float each iteration, to avoid compounding rounding error.
        double time = (double)f / m_fps;

        QImage frame = compositeVideoAt(time);
        if (frame.isNull())
        {
            frame = QImage(m_outputWidth, m_outputHeight, QImage::Format_RGB888);
            frame.fill(Qt::black);
        }

        if (!encoder.writeVideoFrame(frame))
        {
            emit errorOccurred("Video frame write error");
            encoder.finish();
            closeAllDecoders();
            emit renderFinished(false);
            return;
        }

        // Audio: exact sample count for this frame.
        // At 30fps: frame 0 -> samples 0..1470, frame 1 -> 1470..2940, ...
        // No float -> int truncation loss.
        int64_t audioSampleEnd = (int64_t)((f + 1) / m_fps * SR + 0.5);
        int samplesThisFrame = (int)(audioSampleEnd - audioSamplesWritten);
        int floatsThisFrame  = samplesThisFrame * CH;
        double audioDuration = (double)samplesThisFrame / SR;

        QVector<float> audio = mixAudioAt(time, audioDuration);
        // Guarantee the exact expected size — pad or trim as needed.
        if (audio.size() < floatsThisFrame)
            audio.resize(floatsThisFrame, 0.0f);
        else if (audio.size() > floatsThisFrame)
            audio.resize(floatsThisFrame);

        encoder.writeAudioSamples(audio);
        audioSamplesWritten += samplesThisFrame;

        currentFrame++;
        int percent = (totalFrames > 0) ? (currentFrame * 100) / totalFrames : 0;
        if (percent != lastPercent)
        {
            lastPercent = percent;
            emit progressChanged(percent);
        }
    }

    if (m_cancelled)
    {
        encoder.finish(); closeAllDecoders();
        emit renderFinished(false);
        return;
    }

    encoder.finish();
    closeAllDecoders();
    emit progressChanged(100);
    emit renderFinished(true);
}

void RenderWorker::cancel() { m_cancelled = true; }

MediaDecoder* RenderWorker::getVideoDecoder(const QString& filepath)
{
    if (m_videoDecoders.contains(filepath)) return m_videoDecoders[filepath];
    auto* decoder = new MediaDecoder();
    if (!decoder->openFile(filepath))
    {
        delete decoder;
        return nullptr;
    }
    m_videoDecoders[filepath] = decoder;
    return decoder;
}

MediaDecoder* RenderWorker::getAudioDecoder(const QString& filepath)
{
    if (m_audioDecoders.contains(filepath)) return m_audioDecoders[filepath];
    auto* decoder = new MediaDecoder();
    if (!decoder->openFile(filepath))
    {
        delete decoder;
        return nullptr;
    }
    m_audioDecoders[filepath] = decoder;
    return decoder;
}

void RenderWorker::closeAllDecoders() {
    for (auto* d : m_videoDecoders) { d->closeFile(); delete d; }
    for (auto* d : m_audioDecoders) { d->closeFile(); delete d; }
    m_videoDecoders.clear();
    m_audioDecoders.clear();
    m_videoPositions.clear();
    m_audioDelayBufs.clear();
    m_audioDelayPos.clear();
}

TimelineClip* RenderWorker::findActiveClip(double time, int trackIndex)
{
    for (int i = 0; i < m_clips.size(); ++i) {
        TimelineClip& clip = m_clips[i];
        if (clip.trackIndex == trackIndex && clip.isActiveAt(time)) return &clip;
    }
    return nullptr;
}

QImage RenderWorker::compositeVideoAt(double time)
{
    auto scaleFrame = [&](QImage frame) -> QImage
    {
        if (frame.isNull()) return frame;
        if (frame.width() == m_outputWidth && frame.height() == m_outputHeight) return frame;

        bool hasAlpha = (frame.format() == QImage::Format_ARGB32
                      || frame.format() == QImage::Format_ARGB32_Premultiplied);

        frame = frame.scaled(m_outputWidth, m_outputHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (frame.width() == m_outputWidth && frame.height() == m_outputHeight) return frame;

        // Letterboxing: if the source has an alpha channel (chroma key),
        // the canvas must be ARGB32 with a transparent background too —
        // an RGB888 canvas would discard the alpha channel and break
        // chroma key compositing downstream.
        QImage::Format canvasFmt = hasAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB888;
        QImage canvas(m_outputWidth, m_outputHeight, canvasFmt);
        canvas.fill(hasAlpha ? qRgba(0, 0, 0, 0) : qRgb(0, 0, 0));

        int dx = (m_outputWidth  - frame.width())  / 2;
        int dy = (m_outputHeight - frame.height()) / 2;

        // Copy scanlines directly to preserve the alpha channel correctly.
        for (int y = 0; y < frame.height(); ++y) {
            const uchar* src = frame.constScanLine(y);
            uchar* dst = canvas.scanLine(y + dy);
            int bpp = frame.depth() / 8;
            memcpy(dst + dx * bpp, src, frame.width() * bpp);
        }
        return canvas;
    };

    // Compute a clip's transition progress (0..1).
    auto transitionProgress = [&](const TimelineClip* clip, bool forIn) -> float
    {
        if (!clip) return -1.0f;
        QString key = forIn ? "transition_in" : "transition_out";
        QString durKey = "transition_duration";
        if (!clip->effects.contains(key) || clip->effects.value(key, 0.0) < 0.5) return -1.0f;
        double dur = clip->effects.value(durKey, 0.5);
        if (dur < 0.01) return -1.0f;
        if (forIn) {
            double t = time - clip->startTime;
            if (t < 0 || t > dur) return -1.0f;
            return static_cast<float>(t / dur);
        } else {
            double clipEnd = clip->startTime + clip->duration;
            double t = dur - (clipEnd - time);
            if (t < 0 || t > dur) return -1.0f;
            return static_cast<float>(t / dur);
        }
    };

    // Look up the transition type (stored as an int enum):
    // 0=none, 1=fade, 2=wipe_right, 3=wipe_left, 4=zoom_in, 5=zoom_out, 6=flash
    auto transitionType = [&](const TimelineClip* clip, bool forIn) -> int
    {
        if (!clip) return 0;
        QString key = forIn ? "transition_in" : "transition_out";
        return static_cast<int>(clip->effects.value(key, 0.0));
    };

    // Apply a transition to a frame.
    auto applyTransition = [&](QImage frame, const TimelineClip* clip, bool forIn) -> QImage
    {
        float prog = transitionProgress(clip, forIn);
        if (prog < 0.0f) return frame;
        int type = transitionType(clip, forIn);
        QImage black(frame.size(), QImage::Format_RGB888);
        black.fill(Qt::black);
        switch (type) {
            case 1: return Effects::transitionFade(frame, prog, forIn);       // fade_in / fade_out
            case 2: return Effects::transitionWipe(black, frame, prog, false); // wipe ->
            case 3: return Effects::transitionWipe(frame, black, prog, true);  // wipe <-
            case 4: return Effects::transitionZoom(black, frame, prog, true);  // zoom in
            case 5: return Effects::transitionZoom(frame, black, prog, false); // zoom out
            case 6: return Effects::transitionFlash(black, frame, prog);       // flash
            default: return frame;
        }
    };

    // Fetch and process each track's frame.
    QImage frame1, frame2;
    const TimelineClip* c1 = nullptr;
    const TimelineClip* c2 = nullptr;

    TimelineClip* clip1 = findActiveClip(time, 1);
    if (clip1 && !clip1->isVideoHidden) {
        c1 = clip1;
        QImage raw = decodeVideoFrame(clip1, time);
        if (!raw.isNull())
        {
            frame1 = applyClipEffects(raw, *clip1); // May return ARGB32 (chroma key applied).
            frame1 = scaleFrame(frame1);
        }
    }

    TimelineClip* clip2 = findActiveClip(time, 2);
    if (clip2 && !clip2->isVideoHidden)
    {
        c2 = clip2;
        QImage raw = decodeVideoFrame(clip2, time);
        if (!raw.isNull())
        {
            frame2 = applyClipEffects(raw, *clip2);
            frame2 = scaleFrame(frame2);
        }
    }

    // Compositing: Track 2 is the background, Track 1 is the primary
    // layer on top.
    //
    // Layering rules:
    //   Track 1 only          -> show Track 1.
    //   Track 2 only          -> show Track 2.
    //   Track 1 + Track 2,
    //   no alpha on Track 1   -> Track 2 as background, Track 1 fully opaque on top.
    //   Track 1 has alpha
    //   (chroma key)          -> per-pixel alpha blend of Track 1 over Track 2.
    //
    QImage result;

    auto alphaBlend = [&](QImage fg, QImage bg) -> QImage
    {
        // fg is ARGB32 (alpha from chroma key), bg is the background frame.
        QImage b = bg.convertToFormat(QImage::Format_ARGB32);
        QImage f = fg; // Already ARGB32.
        if (f.size() != b.size())
            f = f.scaled(b.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QImage out(b.size(), QImage::Format_RGB888);
        for (int y = 0; y < b.height(); ++y)
        {
            const QRgb* bl = reinterpret_cast<const QRgb*>(b.constScanLine(y));
            const QRgb* fl = reinterpret_cast<const QRgb*>(f.constScanLine(y));
            uchar* dl = out.scanLine(y);
            for (int x = 0; x < b.width(); ++x)
            {
                float a = qAlpha(fl[x]) / 255.0f, ia = 1.0f - a;
                dl[x*3] = (uchar)(qRed(fl[x]) * a + qRed(bl[x])   * ia);
                dl[x*3+1] = (uchar)(qGreen(fl[x]) * a + qGreen(bl[x]) * ia);
                dl[x*3+2] = (uchar)(qBlue(fl[x]) * a + qBlue(bl[x])  * ia);
            }
        }
        return out;
    };

    bool f1HasAlpha = !frame1.isNull() && frame1.format() == QImage::Format_ARGB32;

    if (!frame1.isNull() && !frame2.isNull()) {
        if (f1HasAlpha) {
            // Chroma key: track 1 (transparent foreground) over track 2 (background).
            result = alphaBlend(frame1, frame2);
        } else {
            // Both opaque: track 2 as background, track 1 fully on top.
            // (Track 2 is only visible where track 1 has no clip active.)
            result = frame1.convertToFormat(QImage::Format_RGB888);
        }
    } else if (!frame1.isNull()) {
        result = frame1.convertToFormat(QImage::Format_RGB888);
    } else if (!frame2.isNull()) {
        result = frame2.convertToFormat(QImage::Format_RGB888);
    } else {
        return QImage();
    }

    // ── Transitions are applied to each frame before the final composite ──
    // Transitions run on frame1/frame2 individually, and the composite is
    // then rebuilt from the transitioned frames.
    bool hadTrans = false;
    if (c1 && !frame1.isNull()) {
        QImage f1t = frame1;
        f1t = applyTransition(f1t, c1, true);
        f1t = applyTransition(f1t, c1, false);
        if (f1t.cacheKey() != frame1.cacheKey())
        {
            frame1 = f1t;
            hadTrans = true;
        }
    }
    if (c2 && !frame2.isNull())
    {
        QImage f2t = frame2;
        f2t = applyTransition(f2t, c2, true);
        f2t = applyTransition(f2t, c2, false);
        if (f2t.cacheKey() != frame2.cacheKey())
        {
            frame2 = f2t;
            hadTrans = true;
        }
    }
    if (hadTrans)
    {
        // Rebuild the composite using the transitioned frames.
        if (!frame1.isNull() && !frame2.isNull()) {
            result = (frame1.format()==QImage::Format_ARGB32)
                     ? alphaBlend(frame1, frame2)
                     : frame1.convertToFormat(QImage::Format_RGB888);
        }
        else if (!frame1.isNull())
        {
            result = frame1.convertToFormat(QImage::Format_RGB888);
        }
        else if (!frame2.isNull())
        {
            result = frame2.convertToFormat(QImage::Format_RGB888);
        }
    }

    return result;
}

QImage RenderWorker::decodeVideoFrame(TimelineClip* clip, double timelineTime)
{
    QString key = clip->filepath
                  + "|" + QString::number(clip->startTime, 'f', 4)
                  + "|" + QString::number(clip->trimStart, 'f', 4);

    MediaDecoder* decoder;
    if (m_videoDecoders.contains(key)) {
        decoder = m_videoDecoders[key];
    } else {
        decoder = new MediaDecoder();
        if (!decoder->openFile(clip->filepath)) {
            delete decoder;
            return QImage();
        }
        m_videoDecoders[key] = decoder;
    }
    if (!decoder->hasVideo()) return QImage();

    double sourceTime = clip->sourceTimeAt(timelineTime);
    double fps = decoder->getFrameRate();
    double frameDur = (fps > 0) ? (1.0 / fps) : 0.04;
    double lastPos = m_videoPositions.value(key, -1.0);

    QImage frame;

    // ── PTS-synchronized reading ────────────────────────────────────────
    // getNextFrame() returns frames timed to the source file's own PTS.
    // When the render fps (e.g. 30) differs from the source fps (e.g.
    // 29.97), getNextFrame() yields a frame every 33.37ms while the
    // renderer advances every 33.33ms. Over 1000 frames that drift adds
    // up to 0.33s, which is enough to visibly desync.
    //
    // Fix: after each getNextFrame() call, check the frame's actual PTS.
    // If the decoder is behind (PTS < sourceTime), read another frame.
    // If the decoder is ahead, accept the frame — the next request will
    // catch up.

    bool isSequential = (lastPos >= 0.0 &&
                         sourceTime >= lastPos - frameDur * 0.5 &&
                         sourceTime <= lastPos + frameDur * 4.0);

    if (isSequential)
    {
        // Sequential playback: read via getNextFrame(), correcting for PTS drift.
        for (int attempt = 0; attempt < 5; ++attempt)
        {
            frame = decoder->getNextFrame();
            if (frame.isNull()) break;

            double pts = decoder->getLastVideoPts();
            // Accept the frame once its PTS is close enough to sourceTime.
            if (pts < 0 || pts >= sourceTime - frameDur * 0.5)
                break;
            // PTS still behind — discard this frame and read the next one.
            frame = QImage();
        }
    }

    // If the sequential path didn't produce a frame, fall back to an exact seek.
    if (frame.isNull())
    {
        frame = decoder->getFrameAt(sourceTime);
    }

    if (!frame.isNull()) m_videoPositions[key] = sourceTime;
    return frame;
}

QVector<float> RenderWorker::mixAudioAt(double time, double frameDuration)
{
    int totalFloats = static_cast<int>(frameDuration * MediaDecoder::OUTPUT_SAMPLE_RATE
                                   * MediaDecoder::OUTPUT_CHANNELS + 0.5);
    QVector<float> mixed(totalFloats, 0.0f);
    bool hasAudio = false;

    auto mixIn = [&](TimelineClip* clip)
    {
        if (!clip || clip->isMuted || clip->isAudioHidden) return;
        QVector<float> a = decodeAudioChunk(clip, time, frameDuration);
        if (a.isEmpty()) return;
        hasAudio = true;
        int len = qMin(mixed.size(), a.size());
        for (int i = 0; i < len; ++i) mixed[i] += a[i];
    };

    mixIn(findActiveClip(time, 1));
    mixIn(findActiveClip(time, 2));

    if (!hasAudio) return mixed;
    for (float& s : mixed) s = qBound(-1.0f, s, 1.0f);
    return mixed;
}

QVector<float> RenderWorker::decodeAudioChunk(TimelineClip* clip,
                                              double timelineTime,
                                              double duration)
{
    // Per-clip key, unique for every split clip. Without this, two clips
    // sharing the same source file would share a decoder, and one clip's
    // m_audioOverflow / m_lastAudioPos state would corrupt the other's audio.
    QString key = clip->filepath
                  + "|" + QString::number(clip->startTime, 'f', 4)
                  + "|" + QString::number(clip->trimStart, 'f', 4);

    MediaDecoder* decoder;
    if (m_audioDecoders.contains(key)) {
        decoder = m_audioDecoders[key];
    } else {
        decoder = new MediaDecoder();
        if (!decoder->openFile(clip->filepath)) {
            delete decoder;
            return QVector<float>();
        }
        m_audioDecoders[key] = decoder;
    }
    if (!decoder->hasAudio()) return QVector<float>();

    double sourceTime = clip->sourceTimeAt(timelineTime);

    // Clamp to the clip's end — never read past trimEnd.
    double timeToEnd = clip->endTime() - timelineTime;
    double readDur = qMin(duration, timeToEnd);
    if (readDur <= 0.0) return QVector<float>();

    QVector<float> audio = decoder->decodeAudioRange(sourceTime, readDur);
    if (audio.isEmpty()) return audio;

    // ── Fade-out as the clip approaches its end ───────────────────────────
    // Without this, the clip's audio would end abruptly, producing an
    // audible click. Fading out the last ~3ms removes that click at
    // clip boundaries.
    if (timeToEnd < duration && !audio.isEmpty())
    {
        const int FADE_SAMPLES = 132; // =~ 3ms at 44100Hz
        int fadeFloats = qMin(FADE_SAMPLES * 2, audio.size());
        int fadeStart = audio.size() - fadeFloats;
        for (int i = 0; i < fadeFloats; ++i)
        {
            float t = 1.0f - (float)i / (float)fadeFloats; // 1.0 -> 0.0
            audio[fadeStart + i] *= t;
        }
    }

    // Pad with silence to a full frame's worth if the clip ended early.
    int wantFloats = static_cast<int>(duration * 44100 * 2 + 0.5);
    if (audio.size() < wantFloats)
        audio.resize(wantFloats, 0.0f);

    const int SR  = 44100;
    const int CH  = 2;
    // Per-clip key for time-based effect buffers (reverb, echo).
    const QString& clipBufKey = key;

    // ── Helper: fetch or (re)initialize a persistent ring buffer ─────────
    auto getCBuf = [&](const QString& key, int size) -> QVector<float>&
    {
        auto& buf = m_audioDelayBufs[key];
        if (buf.size() != size)
        {
            buf.assign(size, 0.0f); m_audioDelayPos[key] = 0;
        }
        return buf;
    };
    auto getPos = [&](const QString& key) -> int& {
        return m_audioDelayPos[key];
    };


    // 1. VOLUME — simple gain multiplication (works per-chunk).

    auto it = clip->effects.find("volume");
    if (it != clip->effects.end())
    {
        float vol = static_cast<float>(it.value());
        if (qAbs(vol - 1.0f) > 0.01f)
            for (float& s : audio) s = qBound(-1.0f, s * vol, 1.0f);
    }


    // 2. MONO — averages the two channels (per-chunk).

    it = clip->effects.find("mono");
    if (it != clip->effects.end() && it.value() > 0.5)
    {
        for (int i = 0; i + 1 < audio.size(); i += 2)
        {
            float m = (audio[i] + audio[i+1]) * 0.5f;
            audio[i] = audio[i+1] = m;
        }
    }


    // 3. STEREO WIDEN — Mid/Side processing (per-chunk, no delay involved).

    it = clip->effects.find("stereo_widen");
    if (it != clip->effects.end() && it.value() > 0.01)
    {
        float width = static_cast<float>(it.value());
        float midGain  = 1.0f;
        float sideGain = 1.0f + width * 2.5f; // Boost the side (stereo difference) component.
        for (int i = 0; i + 1 < audio.size(); i += 2)
        {
            float mid  = (audio[i] + audio[i+1]) * 0.5f;
            float side = (audio[i] - audio[i+1]) * 0.5f;
            audio[i]   = qBound(-1.0f, mid * midGain + side * sideGain, 1.0f);
            audio[i+1] = qBound(-1.0f, mid * midGain - side * sideGain, 1.0f);
        }
    }

    // 4. REVERB — comb filter with a persistent ring buffer.
    //    y[n] = x[n]*dry + (x[n] + g*y[n-D])*wet
    //    The buffer's state carries over between chunks, so delays of
    //    any length work correctly across chunk boundaries.

    it = clip->effects.find("reverb");
    if (it != clip->effects.end() && it.value() > 0.01)
    {
        double roomSize = it.value();                       // 0..1
        int delayMs = static_cast<int>(25 + roomSize * 75); // 25..100ms
        int D = qMax(CH * 2, SR / 1000 * delayMs * CH); // Samples (stereo), min = CH*2.
        float g = static_cast<float>(roomSize * 0.65f); // Feedback < 1 keeps the filter stable.
        float wet = static_cast<float>(roomSize * 0.45f);
        float dry = 1.0f - wet * 0.6f;

        QVector<float>& buf = getCBuf(clipBufKey + "_reverb", D);
        int& wp = getPos(clipBufKey + "_reverb");

        for (int i = 0; i < audio.size(); ++i)
        {
            float delayed = buf[wp];
            float reverbed = audio[i] + delayed * g;
            buf[wp] = reverbed;                             // Store into the ring buffer.
            audio[i] = qBound(-1.0f, audio[i]*dry + reverbed*wet, 1.0f);
            wp = (wp + 1) % D;
        }
    }


    // 5. ECHO — delay line with a persistent ring buffer.
    //    y[n] = x[n] + feedback * y[n-D]

    it = clip->effects.find("echo");
    if (it != clip->effects.end() && it.value() > 0.01)
    {
        double strength  = it.value();                          // 0..1
        int delayMs = static_cast<int>(150 + strength * 350); // 150..500ms
        int D = qMax(CH * 2, SR / 1000 * delayMs * CH); // min = CH*2.
        float feedback = static_cast<float>(strength * 0.55f);  // < 1 so the tail decays.

        QVector<float>& buf = getCBuf(clipBufKey + "_echo", D);
        int& wp = getPos(clipBufKey + "_echo");

        for (int i = 0; i < audio.size(); ++i) {
            float delayed = buf[wp];
            float out = audio[i] + delayed * feedback;
            buf[wp] = qBound(-1.0f, out, 1.0f);            // Store for the next cycle.
            audio[i] = qBound(-1.0f, out, 1.0f);
            wp = (wp + 1) % D;
        }
    }


    // 6. PITCH — resampling-based shift (simple approach; changes speed and tone together).

    it = clip->effects.find("pitch");
    if (it != clip->effects.end() && qAbs(it.value()) > 0.1) {
        double semitones = it.value();
        double ratio = std::pow(2.0, semitones / 12.0);
        int outSize = audio.size(); // Keep the chunk length unchanged.
        QVector<float> shifted(outSize, 0.0f);
        for (int i = 0; i < outSize; ++i) {
            double srcIdx = i * ratio;
            int s0 = static_cast<int>(srcIdx);
            int s1 = qMin(s0 + 1, audio.size() - 1);
            if (s0 >= audio.size()) break;
            float t = static_cast<float>(srcIdx - s0);
            shifted[i] = audio[s0] * (1.0f - t) + audio[s1] * t;
        }
        audio = shifted;
    }


    // 7. NORMALIZE — peak-based gain (per-chunk, with a gain cap).

    it = clip->effects.find("normalize");
    if (it != clip->effects.end() && it.value() > 0.01)
    {
        float target = static_cast<float>(it.value());
        float peak = 0.0f;
        for (float s : audio) peak = qMax(peak, qAbs(s));
        if (peak > 1e-5f)
        {
            float gain = qMin(target / peak, 6.0f); // Cap at +6x to avoid amplifying noise.
            for (float& s : audio) s = qBound(-1.0f, s * gain, 1.0f);
        }
    }


    // 8. FADE IN / FADE OUT — based on the clip's position on the timeline.

    it = clip->effects.find("fade_in");
    if (it != clip->effects.end() && it.value() > 0.01)
    {
        double fadeFraction = it.value();        // Fraction of the clip's total duration.
        double clipDur  = clip->duration;
        double posStart = timelineTime - clip->startTime;
        double fadeEnd  = clipDur * fadeFraction;
        if (posStart < fadeEnd)
        {
            for (int i = 0; i < audio.size(); ++i)
            {
                double t = posStart + (double)i / (SR * CH);
                float gain = static_cast<float>(qBound(0.0, t / fadeEnd, 1.0));
                audio[i] = qBound(-1.0f, audio[i] * gain, 1.0f);
            }
        }
    }

    it = clip->effects.find("fade_out");
    if (it != clip->effects.end() && it.value() > 0.01)
    {
        double fadeFraction = it.value();
        double clipDur  = clip->duration;
        double posStart = timelineTime - clip->startTime;
        double fadeStart = clipDur * (1.0 - fadeFraction);
        if (posStart + duration > fadeStart)
        {
            for (int i = 0; i < audio.size(); ++i)
            {
                double t = posStart + (double)i / (SR * CH);
                double fadeLen = clipDur - fadeStart;
                float gain = (fadeLen > 0)
                                 ? static_cast<float>(qBound(0.0, 1.0 - (t - fadeStart) / fadeLen, 1.0))
                                 : 0.0f;
                audio[i] = qBound(-1.0f, audio[i] * gain, 1.0f);
            }
        }
    }

    return audio;
}


// namespace Effects — free functions shared by both RenderWorker's
// per-clip effect pipeline and RenderEngine's static preview methods.

namespace Effects
{

// ── Alpha-preservation helper ─────────────────────────────────────────────
// Every effect function in this namespace converts its frame to RGB888,
// which discards any alpha channel. After chroma key is applied, a frame
// is ARGB32. To keep effects like grayscale/sepia/blur from destroying
// that alpha, this helper captures the alpha mask before running the
// effect and reapplies it afterward.
// Usage: auto result = preserveAlpha(frame, [&](QImage& f){ f = blur(f, r); });
template<typename Func>
static QImage preserveAlpha(const QImage& frame, Func applyEffect)
{
    if (frame.format() != QImage::Format_ARGB32)
    {
        // No alpha channel — just run the effect directly.
        QImage copy = frame;
        applyEffect(copy);
        return copy;
    }
    // Capture the alpha channel per pixel.
    int w = frame.width(), h = frame.height();
    QVector<uchar> alphaMap(w * h);
    for (int y = 0; y < h; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(frame.constScanLine(y));
        for (int x = 0; x < w; ++x)
            alphaMap[y * w + x] = (uchar)qAlpha(line[x]);
    }
    // Run the effect (it converts the frame to RGB888 internally).
    QImage copy = frame;
    applyEffect(copy);
    // Restore alpha: convert the result back to ARGB32 and reapply the mask.
    QImage result = copy.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            uchar a = alphaMap[y * w + x];
            line[x] = qRgba(qRed(line[x]), qGreen(line[x]), qBlue(line[x]), a);
        }
    }
    return result;
}

static void rgbToHsv(int r, int g, int b, double& h, double& s, double& v)
{
    double rd = r/255.0, gd = g/255.0, bd = b/255.0;
    double mx = std::max({rd,gd,bd}), mn = std::min({rd,gd,bd}), d = mx - mn;
    v = mx;
    s = (mx > 0) ? d/mx : 0;
    if (d < 1e-6)
    {
        h = 0; return;
    }
    if (mx == rd) h = 60.0*((gd-bd)/d + (gd<bd ? 6 : 0));
    else if (mx == gd) h = 60.0*((bd-rd)/d + 2);
    else h = 60.0*((rd-gd)/d + 4);
}

static void hsvToRgb(double h, double s, double v, int& r, int& g, int& b)
{
    h = fmod(h, 360.0); if (h < 0) h += 360.0;
    int i = (int)(h/60) % 6;
    double f = h/60 - (int)(h/60);
    double p = v*(1-s), q = v*(1-f*s), t = v*(1-(1-f)*s);
    double rd,gd,bd;
    switch(i)
    {
    case 0: rd=v;gd=t;bd=p;break; case 1: rd=q;gd=v;bd=p;break;
    case 2: rd=p;gd=v;bd=t;break; case 3: rd=p;gd=q;bd=v;break;
    case 4: rd=t;gd=p;bd=v;break; default:rd=v;gd=p;bd=q;break;
    }
    r=qBound(0,(int)(rd*255),255);
    g=qBound(0,(int)(gd*255),255);
    b=qBound(0,(int)(bd*255),255);
}

QImage blur(const QImage& frame, double radius)
{
    auto doBlur = [&](QImage& src) {
        src = src.convertToFormat(QImage::Format_RGB888);
        QImage result = src.copy();
        int r = qMax(1, static_cast<int>(radius));
        int w = src.width(), h = src.height();
        QImage temp = src.copy();
        for (int y = 0; y < h; ++y) {
            const uchar* sl = src.constScanLine(y);
            uchar* dl = temp.scanLine(y);
            for (int x = 0; x < w; ++x) {
                int sR=0,sG=0,sB=0,cnt=0;
                for (int dx=-r;dx<=r;++dx) {
                    int nx=qBound(0,x+dx,w-1);
                    sR+=sl[nx*3]; sG+=sl[nx*3+1]; sB+=sl[nx*3+2]; ++cnt;
                }
                if (cnt>0) { dl[x*3]=sR/cnt; dl[x*3+1]=sG/cnt; dl[x*3+2]=sB/cnt; }
            }
        }
        for (int x=0;x<w;++x) for (int y=0;y<h;++y) {
            int sR=0,sG=0,sB=0,cnt=0;
            for (int dy=-r;dy<=r;++dy) {
                int ny=qBound(0,y+dy,h-1);
                const uchar*l=temp.constScanLine(ny);
                sR+=l[x*3]; sG+=l[x*3+1]; sB+=l[x*3+2]; ++cnt;
            }
            uchar* out=result.scanLine(y);
            if (cnt>0) { out[x*3]=sR/cnt; out[x*3+1]=sG/cnt; out[x*3+2]=sB/cnt; }
        }
        src = result;
    };
    return preserveAlpha(frame, doBlur);
}

QImage sharpness(const QImage& frame, double strength)
{
    return preserveAlpha(frame, [&](QImage& f) {
        QImage blurred = blur(f, 1.0);
        QImage src  = f.convertToFormat(QImage::Format_RGB888);
        QImage bsrc = blurred.convertToFormat(QImage::Format_RGB888);
        QImage result = src.copy();
        for (int y=0;y<src.height();++y) {
            const uchar*s=src.constScanLine(y); const uchar*b=bsrc.constScanLine(y); uchar*d=result.scanLine(y);
            for (int x=0;x<src.width()*3;++x) {
                int v=(int)s[x]+(int)((s[x]-b[x])*strength);
                d[x]=(uchar)qBound(0,v,255);
            }
        }
        f = result;
    });
}

QImage hue(const QImage& frame, double degrees)
{
    return preserveAlpha(frame, [&](QImage& f) {
        f = f.convertToFormat(QImage::Format_RGB888);
        for (int y=0;y<f.height();++y) {
            uchar* line=f.scanLine(y);
            for (int x=0;x<f.width();++x) {
                int idx=x*3; double h,s,v;
                rgbToHsv(line[idx],line[idx+1],line[idx+2],h,s,v);
                h = fmod(h + degrees + 360.0, 360.0);
                int r,g,b; hsvToRgb(h,s,v,r,g,b);
                line[idx]=r; line[idx+1]=g; line[idx+2]=b;
            }
        }
    });
}

QImage sepia(const QImage& frame, double intensity)
{
    return preserveAlpha(frame, [&](QImage& f) {
        f = f.convertToFormat(QImage::Format_RGB888);
        for (int y=0;y<f.height();++y) {
            uchar* line=f.scanLine(y);
            for (int x=0;x<f.width();++x) {
                int idx=x*3, r=line[idx], g=line[idx+1], b=line[idx+2];
                int sr=qBound(0,(int)(r*0.393+g*0.769+b*0.189),255);
                int sg=qBound(0,(int)(r*0.349+g*0.686+b*0.168),255);
                int sb=qBound(0,(int)(r*0.272+g*0.534+b*0.131),255);
                line[idx]  =(uchar)(r+(sr-r)*intensity);
                line[idx+1]=(uchar)(g+(sg-g)*intensity);
                line[idx+2]=(uchar)(b+(sb-b)*intensity);
            }
        }
    });
}

QImage vignette(const QImage& frame, double strength)
{
    return preserveAlpha(frame, [&](QImage& f) {
        f = f.convertToFormat(QImage::Format_RGB888);
        int w=f.width(), h=f.height();
        double cx=w/2.0, cy=h/2.0, maxDist2=cx*cx+cy*cy;
        for (int y=0;y<h;++y) {
            uchar* line=f.scanLine(y);
            for (int x=0;x<w;++x) {
                double dx=x-cx, dy=y-cy;
                double factor=qBound(0.0, 1.0-strength*(dx*dx+dy*dy)/maxDist2, 1.0);
                int idx=x*3;
                line[idx]  =(uchar)(line[idx]  *factor);
                line[idx+1]=(uchar)(line[idx+1]*factor);
                line[idx+2]=(uchar)(line[idx+2]*factor);
            }
        }
    });
}

QImage invert(const QImage& frame)
{
    return preserveAlpha(frame, [](QImage& f) {
        f = f.convertToFormat(QImage::Format_RGB888);
        for (int y=0;y<f.height();++y) {
            uchar* line=f.scanLine(y);
            for (int x=0;x<f.width()*3;++x) line[x] = 255 - line[x];
        }
    });
}

QImage posterize(const QImage& frame, double levels)
{
    return preserveAlpha(frame, [&](QImage& f) {
        int lvl = qBound(2, static_cast<int>(levels), 16);
        int step = 256 / lvl;
        f = f.convertToFormat(QImage::Format_RGB888);
        for (int y=0;y<f.height();++y) {
            uchar* line=f.scanLine(y);
            for (int x=0;x<f.width()*3;++x)
                line[x] = (uchar)qBound(0, (line[x] / step) * step, 255);
        }
    });
}

QImage pixelate(const QImage& frame, double blockSize)
{
    return preserveAlpha(frame, [&](QImage& f) {
        int bs = qBound(2, static_cast<int>(blockSize), 64);
        f = f.convertToFormat(QImage::Format_RGB888);
        int w=f.width(), h=f.height();
        for (int by=0;by<h;by+=bs) for (int bx=0;bx<w;bx+=bs) {
            long sR=0,sG=0,sB=0,cnt=0;
            for (int dy=0;dy<bs&&by+dy<h;++dy) for (int dx=0;dx<bs&&bx+dx<w;++dx) {
                const uchar*l=f.constScanLine(by+dy); int idx=(bx+dx)*3;
                sR+=l[idx]; sG+=l[idx+1]; sB+=l[idx+2]; ++cnt;
            }
            uchar r=(uchar)(sR/cnt), g=(uchar)(sG/cnt), b=(uchar)(sB/cnt);
            for (int dy=0;dy<bs&&by+dy<h;++dy) {
                uchar*l=f.scanLine(by+dy);
                for (int dx=0;dx<bs&&bx+dx<w;++dx) {
                    int idx=(bx+dx)*3; l[idx]=r; l[idx+1]=g; l[idx+2]=b;
                }
            }
        }
    });
}

QImage temperature(const QImage& frame, double value)
{
    return preserveAlpha(frame, [&](QImage& f) {
        f = f.convertToFormat(QImage::Format_RGB888);
        int warmR = static_cast<int>( value * 30);
        int warmB = static_cast<int>(-value * 20);
        for (int y=0;y<f.height();++y) {
            uchar* line=f.scanLine(y);
            for (int x=0;x<f.width();++x) {
                int idx=x*3;
                line[idx]   = (uchar)qBound(0, (int)line[idx]   + warmR, 255);
                line[idx+2] = (uchar)qBound(0, (int)line[idx+2] + warmB, 255);
            }
        }
    });
}

QImage tint(const QImage& frame, double hue, double strength)
{
    return preserveAlpha(frame, [&](QImage& f) {
        int tr,tg,tb; hsvToRgb(hue, 1.0, 1.0, tr, tg, tb);
        f = f.convertToFormat(QImage::Format_RGB888);
        for (int y=0;y<f.height();++y) {
            uchar* line=f.scanLine(y);
            for (int x=0;x<f.width();++x) {
                int idx=x*3, r=line[idx], g=line[idx+1], b=line[idx+2];
                int gray=(int)(0.299*r+0.587*g+0.114*b);
                line[idx]  =(uchar)qBound(0,(int)(gray+(tr-gray)*strength),255);
                line[idx+1]=(uchar)qBound(0,(int)(gray+(tg-gray)*strength),255);
                line[idx+2]=(uchar)qBound(0,(int)(gray+(tb-gray)*strength),255);
            }
        }
    });
}

QImage grain(const QImage& frame, double strength, int frameIndex)
{
    return preserveAlpha(frame, [&](QImage& f) {
        f = f.convertToFormat(QImage::Format_RGB888);
        int w = f.width(), h = f.height();
        float s = static_cast<float>(strength * 60.0);
        for (int y = 0; y < h; ++y) {
            uchar* line = f.scanLine(y);
            for (int x = 0; x < w; ++x) {
                uint32_t seed = static_cast<uint32_t>(y * 7919 + x * 6271 + frameIndex * 1013);
                seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
                float noise = (static_cast<float>(seed & 0xFFFF) / 32767.5f - 1.0f) * s;
                int idx = x * 3;
                line[idx]   = (uchar)qBound(0, (int)line[idx]   + (int)noise, 255);
                line[idx+1] = (uchar)qBound(0, (int)line[idx+1] + (int)noise, 255);
                line[idx+2] = (uchar)qBound(0, (int)line[idx+2] + (int)noise, 255);
            }
        }
    });
}


// CHROMA KEY (green screen removal)
//
// Algorithm, working in an RGB-derived chrominance space:
//   1. Estimate a pixel's "greenness" from how far G dominates over R and B.
//   2. If it falls within the green range, make the pixel transparent.
//   3. smoothness controls edge softening / spill suppression.
//
// threshold : 0.05..0.8  (sensitivity; typically 0.3-0.5)
// smoothness: 0.0..0.3   (mask edge softening)
//
// Returns ARGB32, with transparent pixels wherever the green background was.
// compositeVideoAt() then composites Track 1 over Track 2 using this alpha.

QImage chromaKey(const QImage& frame, double threshold, double smoothness)
{
    QImage result = frame.convertToFormat(QImage::Format_ARGB32);
    int w = result.width(), h = result.height();
    float thr  = static_cast<float>(threshold);
    float soft = static_cast<float>(qMax(smoothness, 0.01)); // Width of the soft-edge zone.

    for (int y = 0; y < h; ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x)
        {
            QRgb px = line[x];
            float r = qRed(px)   / 255.0f;
            float g = qGreen(px) / 255.0f;
            float b = qBlue(px)  / 255.0f;

            // Greenness: how much G dominates over R and B.
            // A strongly green pixel (typical chroma-key background) has
            // high G and low R/B relative to G.
            float greenness = g - qMax(r, b); // > 0 when G dominates.

            float alpha;
            if (greenness < thr - soft)
            {
                alpha = 1.0f; // Clearly not green — keep it opaque.
            }
            else if (greenness > thr + soft)
            {
                alpha = 0.0f; // Clearly green — make it fully transparent.
            }
            else
            {
                // Soft transition (edge anti-aliasing).
                alpha = 1.0f - (greenness - (thr - soft)) / (2.0f * soft);
                alpha = qBound(0.0f, alpha, 1.0f);
            }

            // Spill suppression: remove residual green tinting near the
            // edges of the subject by clamping the green channel toward
            // the neighboring red/blue values.
            if (alpha > 0.0f && alpha < 1.0f)
            {
                float spillG = qMin(r, b); // Replace excess green with the min of R/B.
                r = qBound(0.0f, r, 1.0f);
                b = qBound(0.0f, b, 1.0f);
                g = qBound(0.0f, qMin(g, spillG * 1.2f), 1.0f);
            }

            line[x] = qRgba(
                static_cast<int>(r * 255),
                static_cast<int>(g * 255),
                static_cast<int>(b * 255),
                static_cast<int>(alpha * 255)
            );
        }
    }
    return result;
}


// TRANSITIONS — intermediate-frame generation.
//
// progress runs from 0.0 (transition start) to 1.0 (transition end).
// Every transition function takes two frames and returns a blended one.


// Alpha-blends two frames together.
static QImage blendFrames(const QImage& a, const QImage& b, float alpha)
{
    QImage fa = a.convertToFormat(QImage::Format_RGB888);
    QImage fb = b.convertToFormat(QImage::Format_RGB888).scaled(fa.size());
    QImage result(fa.size(), QImage::Format_RGB888);
    float ia = 1.0f - alpha;
    for (int y = 0; y < fa.height(); ++y) {
        const uchar* la = fa.constScanLine(y);
        const uchar* lb = fb.constScanLine(y);
        uchar*       lr = result.scanLine(y);
        int n = fa.width() * 3;
        for (int i = 0; i < n; ++i)
            lr[i] = (uchar)(la[i] * ia + lb[i] * alpha);
    }
    return result;
}

// Fade (through black).
QImage transitionFade(const QImage& frame, float progress, bool fadeIn)
{
    QImage black(frame.size(), QImage::Format_RGB888);
    black.fill(Qt::black);
    return fadeIn ? blendFrames(black, frame, progress)
                  : blendFrames(frame, black, progress);
}

// Wipe.
QImage transitionWipe(const QImage& from, const QImage& to, float progress, bool rightToLeft)
{
    QImage fa = from.convertToFormat(QImage::Format_RGB888);
    QImage fb = to.convertToFormat(QImage::Format_RGB888).scaled(fa.size());
    QImage result = fa.copy();
    int cutX = static_cast<int>(fa.width() * progress);
    for (int y = 0; y < fa.height(); ++y)
    {
        uchar* lr = result.scanLine(y);
        const uchar* lb = fb.constScanLine(y);
        if (rightToLeft) {
            // Right to left: the new frame reveals from the right edge.
            int startX = fa.width() - cutX;
            for (int x = startX; x < fa.width(); ++x)
                for (int c = 0; c < 3; ++c) lr[x*3+c] = lb[x*3+c];
        } else {
            for (int x = 0; x < cutX; ++x)
                for (int c = 0; c < 3; ++c) lr[x*3+c] = lb[x*3+c];
        }
    }
    return result;
}

// Zoom in/out — the destination frame grows out of (or the source shrinks into) the center.
QImage transitionZoom(const QImage& from, const QImage& to, float progress, bool zoomIn)
{
    QImage fa = from.convertToFormat(QImage::Format_RGB888);
    QImage fb = to.convertToFormat(QImage::Format_RGB888).scaled(fa.size());
    int w = fa.width(), h = fa.height();
    float scale = zoomIn ? (0.1f + 0.9f * progress) : (1.0f + 0.5f * progress);
    int sw = static_cast<int>(w * scale), sh = static_cast<int>(h * scale);
    if (sw < 1) sw = 1; if (sh < 1) sh = 1;
    QImage scaled = zoomIn
        ? fb.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        : fa.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage result = zoomIn ? fa.copy() : fb.copy();
    int ox = (w - sw) / 2, oy = (h - sh) / 2;
    for (int y = 0; y < sh && oy+y < h; ++y)
    {
        if (oy+y < 0) continue;
        const uchar* src = scaled.constScanLine(y);
        uchar* dst = result.scanLine(oy + y);
        int startX = qMax(0, ox), endX = qMin(w, ox + sw);
        int srcOff = (startX - ox) * 3;
        memcpy(dst + startX * 3, src + srcOff, (endX - startX) * 3);
    }
    // Smooth cross-fade over the edges of the zoomed region.
    return blendFrames(zoomIn ? fa : result, zoomIn ? result : fb, progress);
}

// Flash — a white frame at the peak of the transition.
QImage transitionFlash(const QImage& from, const QImage& to, float progress)
{
    QImage white(from.size(), QImage::Format_RGB888);
    white.fill(Qt::white);
    if (progress < 0.5f)
    {
        float t = progress * 2.0f; // 0->1 ramping toward white.
        return blendFrames(from, white, t);
    } else
    {
        float t = (progress - 0.5f) * 2.0f; // 0->1 ramping from white to the destination.
        return blendFrames(white, to, t);
    }
}

} // namespace Effects

// RenderWorker — effect methods.


QImage RenderWorker::applyClipEffects(const QImage& frame, const TimelineClip& clip)
{
    if (clip.effects.isEmpty()) return frame;
    QImage result = frame;

    // ── Chroma key runs last ──────────────────────────────────────────────
    // Every other visual effect (brightness, temperature, etc.) converts
    // the frame to RGB888. If chroma key (which produces ARGB32) ran
    // before them, the next effect would strip the alpha channel and the
    // green background would reappear.
    // Solution: run all other effects first (on RGB888), then chroma key
    // last (converting to ARGB32).
    bool hasChromaKey = false;
    double chromaThr = 0.35, chromaSoft = 0.10;

    for (auto it = clip.effects.begin(); it != clip.effects.end(); ++it)
    {
        const QString& name = it.key();
        double value = it.value();

        // Skip chroma key here — it's applied at the end.
        if (name == "chroma_key" && value > 0.5) {
            hasChromaKey = true;
            chromaThr  = clip.effects.value("chroma_threshold",  0.35);
            chromaSoft = clip.effects.value("chroma_smoothness", 0.10);
            continue;
        }
        if (name == "chroma_threshold" || name == "chroma_smoothness") continue;

        if (name == "brightness") result = applyBrightness(result, value);
        else if (name == "contrast") result = applyContrast(result, value);
        else if (name == "saturation") result = applySaturation(result, value);
        else if (name == "grayscale" && value > 0.5) result = applyGrayscale(result);
        else if (name == "blur" && value > 0.0) result = Effects::blur(result, value);
        else if (name == "sharpness" && value > 0.0) result = Effects::sharpness(result, value);
        else if (name == "hue" && value != 0.0) result = Effects::hue(result, value);
        else if (name == "sepia" && value > 0.0) result = Effects::sepia(result, value);
        else if (name == "vignette" && value > 0.0) result = Effects::vignette(result, value);
        else if (name == "invert" && value > 0.5) result = Effects::invert(result);
        else if (name == "posterize" && value >= 2.0)  result = Effects::posterize(result, value);
        else if (name == "pixelate" && value >= 2.0)  result = Effects::pixelate(result, value);
        else if (name == "temperature"&& value != 0.0)  result = Effects::temperature(result, value);
        else if (name == "tint_hue" )
        {
            double tintStr = clip.effects.value("tint_strength", 0.5);
            if (tintStr > 0.0) result = Effects::tint(result, value, tintStr);
        }
        else if (name == "grain" && value > 0.0)
        {
            int fi = static_cast<int>(clip.effects.value("_frameIdx", 0));
            result = Effects::grain(result, value, fi);
        }
        else if (name == "auto_enhance" && value > 0.0)
        {
            double s = value;
            if (s > 0.01) result = Effects::sharpness(result, 0.3 + s * 0.7);
            if (s > 0.01) result = applyContrast(result, 1.0 + s * 0.15);
            if (s > 0.01) result = applySaturation(result, 1.0 + s * 0.2);
        }
    }

    // Chroma key runs last: its ARGB32 output won't be overwritten by
    // any other effect afterward.
    if (hasChromaKey)
    {
        result = Effects::chromaKey(result, chromaThr, chromaSoft);
    }

    return result;
}

// Brightness: additive shift, -1.0..+1.0 maps to +-255.
QImage RenderWorker::applyBrightness(const QImage& frame, double value)
{
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    int shift = static_cast<int>(value * 255.0);
    for (int y=0;y<result.height();++y)
    {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width()*3;++x)
        {
            int v=(int)line[x]+shift;
            line[x]=(uchar)qBound(0,v,255);
        }
    }
    return result;
}

// Contrast: value 0..3, 1.0 = unchanged.
QImage RenderWorker::applyContrast(const QImage& frame, double value)
{
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y)
    {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width()*3;++x)
        {
            int v=(int)((line[x]-128)*value+128);
            line[x]=(uchar)qBound(0,v,255);
        }
    }
    return result;
}

QImage RenderWorker::applySaturation(const QImage& frame, double value)
{
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y)
    {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width();++x)
        {
            int idx=x*3, r=line[idx], g=line[idx+1], b=line[idx+2];
            int gray=(int)(0.299*r+0.587*g+0.114*b);
            line[idx] =(uchar)qBound(0,gray+(int)((r-gray)*value),255);
            line[idx+1]=(uchar)qBound(0,gray+(int)((g-gray)*value),255);
            line[idx+2]=(uchar)qBound(0,gray+(int)((b-gray)*value),255);
        }
    }
    return result;
}

QImage RenderWorker::applyGrayscale(const QImage& frame)
{
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y)
    {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width();++x)
        {
            int idx=x*3,gray=(int)(0.299*line[idx]+0.587*line[idx+1]+0.114*line[idx+2]);
            line[idx]=line[idx+1]=line[idx+2]=(uchar)gray;
        }
    }
    return result;
}


// RenderEngine — the manager, running on the main thread.


RenderEngine::RenderEngine(QObject *parent)
    : QObject(parent)
    , m_outputWidth()
    , m_outputHeight()
    , m_fps()
    , m_bitrate()
    , m_thread(nullptr)
    , m_worker(nullptr)
{}

RenderEngine::~RenderEngine()
{
    cancel();
    if (m_thread)
    {
        m_thread->quit();
        m_thread->wait(3000);
        delete m_thread;
    }
}

void RenderEngine::setClips(const QList<TimelineClip>& clips) { m_clips = clips; }
void RenderEngine::setOutputPath(const QString& path) { m_outputPath = path; }
void RenderEngine::setOutputResolution(int w, int h) { m_outputWidth = w; m_outputHeight = h; }
void RenderEngine::setOutputCodec(const QString& codec) { m_codec = codec; }
void RenderEngine::setOutputFormat(const QString& format) { m_format = format; }
void RenderEngine::setFps(double fps) { m_fps = fps; }
void RenderEngine::setBitrate(int bitrate) { m_bitrate = bitrate; }

bool RenderEngine::startRender()
{
    if (m_clips.isEmpty())
    {
        emit error("No clips to render");
        emit renderFinished(false);
        return false;
    }
    if (m_outputPath.isEmpty())
    {
        emit error("No output path specified");
        emit renderFinished(false);
        return false;
    }

    if (m_thread)
    {
        m_thread->quit();
        m_thread->wait(2000);
        delete m_thread;
        m_thread = nullptr;
    }

    m_thread = new QThread();
    m_worker = new RenderWorker();
    m_worker->moveToThread(m_thread);

    m_worker->setClips(m_clips);
    m_worker->setOutputPath(m_outputPath);
    m_worker->setOutputResolution(m_outputWidth, m_outputHeight);
    m_worker->setFps(m_fps);
    m_worker->setBitrate(m_bitrate);
    m_worker->setFormat(m_format); // Pass the explicit format ("MP4", "WebM", etc.)

    connect(m_thread, &QThread::started, m_worker, &RenderWorker::process);
    connect(m_worker, &RenderWorker::progressChanged, this, &RenderEngine::progressChanged);
    connect(m_worker, &RenderWorker::renderFinished, this, &RenderEngine::renderFinished);
    connect(m_worker, &RenderWorker::errorOccurred, this, &RenderEngine::error);
    connect(m_worker, &RenderWorker::renderFinished, m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_thread->start();
    return true;
}

void RenderEngine::cancel()
{
    if (m_worker) m_worker->cancel();
}

// STATIC PREVIEW EFFECTS

QImage RenderEngine::applyBrightness(const QImage& frame, double value)
{
    // If the frame is ARGB32 (post chroma-key), operate on it directly
    // to preserve alpha. Otherwise convert to RGB888 as before.
    bool hasAlpha = (frame.format() == QImage::Format_ARGB32);
    QImage result = hasAlpha ? frame.copy() : frame.convertToFormat(QImage::Format_RGB888);
    int shift = static_cast<int>(value * 255.0);
    for (int y = 0; y < result.height(); ++y)
    {
        if (hasAlpha) {
            QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < result.width(); ++x) {
                int a = qAlpha(line[x]);
                if (a == 0) continue; // Fully transparent pixel — leave it alone.
                int r = qBound(0, qRed(line[x])   + shift, 255);
                int g = qBound(0, qGreen(line[x]) + shift, 255);
                int b = qBound(0, qBlue(line[x])  + shift, 255);
                line[x] = qRgba(r, g, b, a);
            }
        } else {
            uchar* line = result.scanLine(y);
            for (int x = 0; x < result.width() * 3; ++x) {
                int v = (int)line[x] + shift; line[x] = (uchar)qBound(0, v, 255);
            }
        }
    }
    return result;
}

QImage RenderEngine::applyContrast(const QImage& frame, double value)
{
    bool hasAlpha = (frame.format() == QImage::Format_ARGB32);
    QImage result = hasAlpha ? frame.copy() : frame.convertToFormat(QImage::Format_RGB888);
    for (int y = 0; y < result.height(); ++y)
    {
        if (hasAlpha) {
            QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < result.width(); ++x) {
                int a = qAlpha(line[x]);
                if (a == 0) continue;
                int r = qBound(0, (int)((qRed(line[x])   - 128) * value + 128), 255);
                int g = qBound(0, (int)((qGreen(line[x]) - 128) * value + 128), 255);
                int b = qBound(0, (int)((qBlue(line[x])  - 128) * value + 128), 255);
                line[x] = qRgba(r, g, b, a);
            }
        } else {
            uchar* line = result.scanLine(y);
            for (int x = 0; x < result.width() * 3; ++x) {
                int v = (int)((line[x] - 128) * value + 128);
                line[x] = (uchar)qBound(0, v, 255);
            }
        }
    }
    return result;
}

QImage RenderEngine::applySaturation(const QImage& frame, double value)
{
    bool hasAlpha = (frame.format() == QImage::Format_ARGB32);
    QImage result = hasAlpha ? frame.copy() : frame.convertToFormat(QImage::Format_RGB888);
    for (int y = 0; y < result.height(); ++y)
    {
        if (hasAlpha) {
            QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < result.width(); ++x) {
                int a = qAlpha(line[x]);
                if (a == 0) continue;
                int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
                int gray = (int)(0.299*r + 0.587*g + 0.114*b);
                line[x] = qRgba(qBound(0, gray + (int)((r-gray)*value), 255),
                                 qBound(0, gray + (int)((g-gray)*value), 255),
                                 qBound(0, gray + (int)((b-gray)*value), 255), a);
            }
        } else {
            uchar* line = result.scanLine(y);
            for (int x = 0; x < result.width(); ++x) {
                int idx = x*3, r = line[idx], g = line[idx+1], b = line[idx+2];
                int gray = (int)(0.299*r + 0.587*g + 0.114*b);
                line[idx]   = (uchar)qBound(0, gray + (int)((r-gray)*value), 255);
                line[idx+1] = (uchar)qBound(0, gray + (int)((g-gray)*value), 255);
                line[idx+2] = (uchar)qBound(0, gray + (int)((b-gray)*value), 255);
            }
        }
    }
    return result;
}

QImage RenderEngine::applyGrayscale(const QImage& frame)
{
    bool hasAlpha = (frame.format() == QImage::Format_ARGB32);
    QImage result = hasAlpha ? frame.copy() : frame.convertToFormat(QImage::Format_RGB888);
    for (int y = 0; y < result.height(); ++y)
    {
        if (hasAlpha) {
            QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < result.width(); ++x) {
                int a = qAlpha(line[x]);
                if (a == 0) continue;
                int gray = (int)(0.299*qRed(line[x]) + 0.587*qGreen(line[x]) + 0.114*qBlue(line[x]));
                line[x] = qRgba(gray, gray, gray, a);
            }
        } else {
            uchar* line = result.scanLine(y);
            for (int x = 0; x < result.width(); ++x) {
                int idx = x*3;
                int gray = (int)(0.299*line[idx] + 0.587*line[idx+1] + 0.114*line[idx+2]);
                line[idx] = line[idx+1] = line[idx+2] = (uchar)gray;
            }
        }
    }
    return result;
}

// Static methods delegate to namespace Effects.
QImage RenderEngine::applyBlur(const QImage& frame, double radius) { return Effects::blur(frame, radius); }
QImage RenderEngine::applySharpness(const QImage& frame, double strength) { return Effects::sharpness(frame, strength); }
QImage RenderEngine::applyHue(const QImage& frame, double degrees) { return Effects::hue(frame, degrees); }
QImage RenderEngine::applySepia(const QImage& frame, double intensity) { return Effects::sepia(frame, intensity); }
QImage RenderEngine::applyVignette(const QImage& frame, double strength) { return Effects::vignette(frame, strength); }
QImage RenderEngine::applyInvert(const QImage& frame) { return Effects::invert(frame); }
QImage RenderEngine::applyPosterize(const QImage& frame, double levels) { return Effects::posterize(frame, levels); }
QImage RenderEngine::applyPixelate(const QImage& frame, double blockSize) { return Effects::pixelate(frame, blockSize); }
QImage RenderEngine::applyTemperature(const QImage& frame, double value) { return Effects::temperature(frame, value); }
QImage RenderEngine::applyTint(const QImage& frame, double hue, double s) { return Effects::tint(frame, hue, s); }
QImage RenderEngine::applyGrain(const QImage& frame, double strength, int fi) { return Effects::grain(frame, strength, fi); }
QImage RenderEngine::applyChromaKey(const QImage& frame, double thr, double smooth) { return Effects::chromaKey(frame, thr, smooth); }
QImage RenderEngine::applyTransition(const QImage& from, const QImage& to, int type, float prog)
{
    switch (type)
    {
        case 1: return prog < 0.5f ? Effects::transitionFade(from, prog*2.0f, false)
                                   : Effects::transitionFade(to,   (prog-0.5f)*2.0f, true);
        case 2: return Effects::transitionWipe(from, to, prog, false);
        case 3: return Effects::transitionWipe(from, to, prog, true);
        case 4: return Effects::transitionZoom(from, to, prog, true);
        case 5: return Effects::transitionZoom(from, to, prog, false);
        case 6: return Effects::transitionFlash(from, to, prog);
        default: return from;
    }
}

// RenderEngine::applyEffectsToFrame
// EFFECT APPLICATION ORDER:
//   1. Chroma key runs first, while the source colors are still untouched.
//      It converts the frame to ARGB32 with a transparent background.
//   2. Every other effect preserves the alpha channel via its hasAlpha
//      branch — grayscale/sepia/invert operate on the RGB channels only
//      and leave alpha untouched.

QImage RenderEngine::applyEffectsToFrame(const QImage& frame,
                                         const QMap<QString, double>& effects,
                                         int frameIndex)
{
    QImage result = frame;

    // Step 1: chroma key first, operating on the original colors.
    if (effects.value("chroma_key", 0.0) > 0.5)
    {
        double thr  = effects.value("chroma_threshold",  0.35);
        double soft = effects.value("chroma_smoothness", 0.10);
        result = applyChromaKey(result, thr, soft);
        // result is now ARGB32; every apply* function below handles ARGB32 correctly.
    }

    // Step 2: remaining effects (each apply* function preserves alpha when the input is ARGB32).
    for (auto it = effects.constBegin(); it != effects.constEnd(); ++it) {
        const QString& name = it.key();
        double value = it.value();

        if (name.startsWith('_')) continue;
        if (name == "chroma_key" || name == "chroma_threshold" || name == "chroma_smoothness") continue;

        if (name == "brightness"  && qAbs(value) > 0.001)
            result = applyBrightness(result, value);
        else if (name == "contrast"    && qAbs(value - 1.0) > 0.001)
            result = applyContrast(result, value);
        else if (name == "saturation"  && qAbs(value - 1.0) > 0.001)
            result = applySaturation(result, value);
        else if (name == "grayscale"   && value > 0.5)
            result = applyGrayscale(result);
        else if (name == "blur"        && value > 0.1)
            result = applyBlur(result, value);
        else if (name == "sharpness"   && value > 0.1)
            result = applySharpness(result, value);
        else if (name == "hue"         && qAbs(value) > 0.1)
            result = applyHue(result, value);
        else if (name == "sepia"       && value > 0.01)
            result = applySepia(result, value);
        else if (name == "vignette"    && value > 0.01)
            result = applyVignette(result, value);
        else if (name == "invert"      && value > 0.5)
            result = applyInvert(result);
        else if (name == "posterize"   && value > 0.5)
            result = applyPosterize(result, value);
        else if (name == "pixelate"    && value > 1.0)
            result = applyPixelate(result, value);
        else if (name == "temperature" && qAbs(value) > 0.01)
            result = applyTemperature(result, value);
        else if (name == "tint_hue"    && effects.value("tint_strength", 0.0) > 0.01)
            result = applyTint(result, value, effects.value("tint_strength", 0.0));
        else if (name == "grain"       && value > 0.01)
            result = applyGrain(result, value, frameIndex);
        else if (name == "auto_enhance" && value > 0.0) {
            double s = value;
            if (s > 0.01) result = applySharpness(result, 0.3 + s * 0.7);
            if (s > 0.01) result = applyContrast(result,  1.0 + s * 0.15);
            if (s > 0.01) result = applySaturation(result, 1.0 + s * 0.2);
        }
    }

    return result;
}

