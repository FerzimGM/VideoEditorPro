#include "renderengine.h"
#include "mediadecoder.h"
#include "mediaencoder.h"
#include <QDebug>
#include <QColor>
#include <algorithm>
#include <cmath>

// =====================================================================
//  RenderWorker — выполняется в фоновом потоке
// =====================================================================

RenderWorker::RenderWorker(QObject* parent)
    : QObject(parent)
    , m_outputWidth(1920)
    , m_outputHeight(1080)
    , m_fps(30.0)
    , m_bitrate(5000000)
    , m_cancelled(false)
{
}

RenderWorker::~RenderWorker() {
    closeAllDecoders();
}

void RenderWorker::process() {
    qDebug() << "RenderWorker::process() START";
    m_cancelled = false;

    if (m_clips.isEmpty()) {
        emit errorOccurred("Нет клипов для рендеринга");
        emit renderFinished(false);
        return;
    }

    double totalDuration = 0.0;
    for (const TimelineClip& clip : m_clips) {
        double end = clip.endTime();
        if (end > totalDuration) totalDuration = end;
    }

    MediaEncoder encoder;
    encoder.setFrameRate(m_fps);
    encoder.setBitrate(m_bitrate);
    encoder.setAudioEnabled(true);

    if (!encoder.createOutputFile(m_outputPath, m_outputWidth, m_outputHeight)) {
        emit errorOccurred("Не могу создать выходной файл");
        emit renderFinished(false);
        return;
    }

    double frameTime  = 1.0 / m_fps;
    int totalFrames   = static_cast<int>(totalDuration * m_fps);
    int currentFrame  = 0;
    int lastPercent   = -1;

    for (double time = 0.0; time < totalDuration && !m_cancelled; time += frameTime)
    {
        QImage frame = compositeVideoAt(time);
        if (frame.isNull()) {
            frame = QImage(m_outputWidth, m_outputHeight, QImage::Format_RGB888);
            frame.fill(Qt::black);
        }

        if (!encoder.writeVideoFrame(frame)) {
            emit errorOccurred("Ошибка записи видеокадра");
            encoder.finish();
            closeAllDecoders();
            emit renderFinished(false);
            return;
        }

        QVector<float> audio = mixAudioAt(time, frameTime);
        if (!audio.isEmpty()) encoder.writeAudioSamples(audio);

        currentFrame++;
        int percent = (totalFrames > 0) ? (currentFrame * 100) / totalFrames : 0;
        if (percent != lastPercent) { lastPercent = percent; emit progressChanged(percent); }
    }

    if (m_cancelled) {
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

MediaDecoder* RenderWorker::getVideoDecoder(const QString& filepath) {
    if (m_videoDecoders.contains(filepath)) return m_videoDecoders[filepath];
    auto* decoder = new MediaDecoder();
    if (!decoder->openFile(filepath)) { delete decoder; return nullptr; }
    m_videoDecoders[filepath] = decoder;
    return decoder;
}

MediaDecoder* RenderWorker::getAudioDecoder(const QString& filepath) {
    if (m_audioDecoders.contains(filepath)) return m_audioDecoders[filepath];
    auto* decoder = new MediaDecoder();
    if (!decoder->openFile(filepath)) { delete decoder; return nullptr; }
    m_audioDecoders[filepath] = decoder;
    return decoder;
}

void RenderWorker::closeAllDecoders() {
    for (auto* d : m_videoDecoders) { d->closeFile(); delete d; }
    for (auto* d : m_audioDecoders) { d->closeFile(); delete d; }
    m_videoDecoders.clear(); m_audioDecoders.clear(); m_videoPositions.clear();
    m_audioDelayBufs.clear(); m_audioDelayPos.clear();
}

TimelineClip* RenderWorker::findActiveClip(double time, int trackIndex) {
    for (int i = 0; i < m_clips.size(); ++i) {
        TimelineClip& clip = m_clips[i];
        if (clip.trackIndex == trackIndex && clip.isActiveAt(time)) return &clip;
    }
    return nullptr;
}

QImage RenderWorker::compositeVideoAt(double time) {
    auto scaleFrame = [&](QImage frame) -> QImage {
        if (frame.width() == m_outputWidth && frame.height() == m_outputHeight) return frame;
        frame = frame.scaled(m_outputWidth, m_outputHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (frame.width() == m_outputWidth && frame.height() == m_outputHeight) return frame;
        QImage canvas(m_outputWidth, m_outputHeight, QImage::Format_RGB888);
        canvas.fill(Qt::black);
        int dx = (m_outputWidth - frame.width()) / 2;
        int dy = (m_outputHeight - frame.height()) / 2;
        for (int y = 0; y < frame.height(); ++y) {
            const uchar* src = frame.constScanLine(y);
            uchar* dst = canvas.scanLine(y + dy);
            memcpy(dst + dx * (frame.depth()/8), src, frame.width() * (frame.depth()/8));
        }
        return canvas;
    };

    TimelineClip* clip1 = findActiveClip(time, 1);
    if (clip1 && !clip1->isVideoHidden) {
        QImage frame = decodeVideoFrame(clip1, time);
        if (!frame.isNull()) return scaleFrame(applyClipEffects(frame, *clip1));
    }

    TimelineClip* clip2 = findActiveClip(time, 2);
    if (clip2 && !clip2->isVideoHidden) {
        QImage frame = decodeVideoFrame(clip2, time);
        if (!frame.isNull()) return scaleFrame(applyClipEffects(frame, *clip2));
    }

    return QImage();
}

QImage RenderWorker::decodeVideoFrame(TimelineClip* clip, double timelineTime) {
    MediaDecoder* decoder = getVideoDecoder(clip->filepath);
    if (!decoder || !decoder->hasVideo()) return QImage();

    double sourceTime = clip->sourceTimeAt(timelineTime);
    double fps = decoder->getFrameRate();
    double frameDur = (fps > 0) ? (1.0 / fps) : 0.04;
    QString key = clip->filepath;
    double lastPos = m_videoPositions.value(key, -1.0);

    bool needSeek = (lastPos < 0.0) ||
                    (sourceTime < lastPos - frameDur * 0.5) ||
                    (sourceTime > lastPos + frameDur * 5.0);

    QImage frame = needSeek ? decoder->getFrameAt(sourceTime) : decoder->getNextFrame();
    if (!frame.isNull()) m_videoPositions[key] = sourceTime;
    return frame;
}

QVector<float> RenderWorker::mixAudioAt(double time, double frameDuration) {
    int totalFloats = static_cast<int>(frameDuration * MediaDecoder::OUTPUT_SAMPLE_RATE)
    * MediaDecoder::OUTPUT_CHANNELS;
    QVector<float> mixed(totalFloats, 0.0f);
    bool hasAudio = false;

    auto mixIn = [&](TimelineClip* clip) {
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
                                              double duration) {
    MediaDecoder* decoder = getAudioDecoder(clip->filepath);
    if (!decoder || !decoder->hasAudio()) return QVector<float>();

    double sourceTime = clip->sourceTimeAt(timelineTime);
    QVector<float> audio = decoder->decodeAudioRange(sourceTime, duration);
    if (audio.isEmpty()) return audio;

    const int SR  = 44100;
    const int CH  = 2; // стерео (interleaved L,R)
    const QString fp = clip->filepath;

    // ── Вспомогательная лямбда: получить/инициализировать кольцевой буфер ──
    auto getCBuf = [&](const QString& key, int size) -> QVector<float>& {
        auto& buf = m_audioDelayBufs[key];
        if (buf.size() != size) { buf.assign(size, 0.0f); m_audioDelayPos[key] = 0; }
        return buf;
    };
    auto getPos = [&](const QString& key) -> int& {
        return m_audioDelayPos[key];
    };

    // ═══════════════════════════════════════════
    // 1. ГРОМКОСТЬ — простое умножение (работает per-chunk)
    // ═══════════════════════════════════════════
    auto it = clip->effects.find("volume");
    if (it != clip->effects.end()) {
        float vol = static_cast<float>(it.value());
        if (qAbs(vol - 1.0f) > 0.01f)
            for (float& s : audio) s = qBound(-1.0f, s * vol, 1.0f);
    }

    // ═══════════════════════════════════════════
    // 2. МОНО — усреднение каналов (per-chunk)
    // ═══════════════════════════════════════════
    it = clip->effects.find("mono");
    if (it != clip->effects.end() && it.value() > 0.5) {
        for (int i = 0; i + 1 < audio.size(); i += 2) {
            float m = (audio[i] + audio[i+1]) * 0.5f;
            audio[i] = audio[i+1] = m;
        }
    }

    // ═══════════════════════════════════════════
    // 3. РАСШИРЕНИЕ СТЕРЕО — M/S обработка (per-chunk, без задержки)
    // ═══════════════════════════════════════════
    it = clip->effects.find("stereo_widen");
    if (it != clip->effects.end() && it.value() > 0.01) {
        float width = static_cast<float>(it.value());
        float midGain  = 1.0f;
        float sideGain = 1.0f + width * 2.5f; // усиливаем боковую составляющую
        for (int i = 0; i + 1 < audio.size(); i += 2) {
            float mid  = (audio[i] + audio[i+1]) * 0.5f;
            float side = (audio[i] - audio[i+1]) * 0.5f;
            audio[i]   = qBound(-1.0f, mid * midGain + side * sideGain, 1.0f);
            audio[i+1] = qBound(-1.0f, mid * midGain - side * sideGain, 1.0f);
        }
    }

    // ═══════════════════════════════════════════
    // 4. РЕВЕРБЕРАЦИЯ — comb-filter с КОЛЬЦЕВЫМ БУФЕРОМ (персистентный)
    //    y[n] = x[n]*dry + (x[n] + g*y[n-D])*wet
    //    Буфер сохраняет состояние между чанками → задержка любой длины работает
    // ═══════════════════════════════════════════
    it = clip->effects.find("reverb");
    if (it != clip->effects.end() && it.value() > 0.01) {
        double roomSize = it.value();                       // 0..1
        int delayMs  = static_cast<int>(25 + roomSize * 75); // 25..100мс
        int D        = qMax(CH * 2, SR / 1000 * delayMs * CH); // семплов (стерео), min=CH*2
        float g      = static_cast<float>(roomSize * 0.65f); // feedback < 1 → стабильно
        float wet    = static_cast<float>(roomSize * 0.45f);
        float dry    = 1.0f - wet * 0.6f;

        QVector<float>& buf = getCBuf(fp + "_reverb", D);
        int& wp = getPos(fp + "_reverb");

        for (int i = 0; i < audio.size(); ++i) {
            float delayed = buf[wp];
            float reverbed = audio[i] + delayed * g;
            buf[wp] = reverbed;                             // записываем в буфер
            audio[i] = qBound(-1.0f, audio[i]*dry + reverbed*wet, 1.0f);
            wp = (wp + 1) % D;
        }
    }

    // ═══════════════════════════════════════════
    // 5. ЭХО — delay line с КОЛЬЦЕВЫМ БУФЕРОМ (персистентный)
    //    y[n] = x[n] + feedback * y[n-D]
    // ═══════════════════════════════════════════
    it = clip->effects.find("echo");
    if (it != clip->effects.end() && it.value() > 0.01) {
        double strength  = it.value();                          // 0..1
        int delayMs      = static_cast<int>(150 + strength * 350); // 150..500мс
        int D            = qMax(CH * 2, SR / 1000 * delayMs * CH); // min=CH*2
        float feedback   = static_cast<float>(strength * 0.55f);  // < 1 → затухает

        QVector<float>& buf = getCBuf(fp + "_echo", D);
        int& wp = getPos(fp + "_echo");

        for (int i = 0; i < audio.size(); ++i) {
            float delayed = buf[wp];
            float out = audio[i] + delayed * feedback;
            buf[wp] = qBound(-1.0f, out, 1.0f);            // запись для следующего цикла
            audio[i] = qBound(-1.0f, out, 1.0f);
            wp = (wp + 1) % D;
        }
    }

    // ═══════════════════════════════════════════
    // 6. ПИТЧ — ресэмплинг (меняет скорость+тон, простой вариант)
    // ═══════════════════════════════════════════
    it = clip->effects.find("pitch");
    if (it != clip->effects.end() && qAbs(it.value()) > 0.1) {
        double semitones = it.value();
        double ratio = std::pow(2.0, semitones / 12.0);
        int outSize = audio.size(); // сохраняем длину чанка
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

    // ═══════════════════════════════════════════
    // 7. НОРМАЛИЗАЦИЯ — пиковая (per-chunk, с ограничением усиления)
    // ═══════════════════════════════════════════
    it = clip->effects.find("normalize");
    if (it != clip->effects.end() && it.value() > 0.01) {
        float target = static_cast<float>(it.value());
        float peak = 0.0f;
        for (float s : audio) peak = qMax(peak, qAbs(s));
        if (peak > 1e-5f) {
            float gain = qMin(target / peak, 6.0f); // не более +6x во избежание шума
            for (float& s : audio) s = qBound(-1.0f, s * gain, 1.0f);
        }
    }

    // ═══════════════════════════════════════════
    // 8. ФЕЙД-ИН / ФЕЙД-АУТ — по позиции клипа
    // ═══════════════════════════════════════════
    it = clip->effects.find("fade_in");
    if (it != clip->effects.end() && it.value() > 0.01) {
        double fadeFraction = it.value();        // доля длительности клипа
        double clipDur  = clip->duration;
        double posStart = timelineTime - clip->startTime;
        double fadeEnd  = clipDur * fadeFraction;
        if (posStart < fadeEnd) {
            for (int i = 0; i < audio.size(); ++i) {
                double t = posStart + (double)i / (SR * CH);
                float gain = static_cast<float>(qBound(0.0, t / fadeEnd, 1.0));
                audio[i] = qBound(-1.0f, audio[i] * gain, 1.0f);
            }
        }
    }

    it = clip->effects.find("fade_out");
    if (it != clip->effects.end() && it.value() > 0.01) {
        double fadeFraction = it.value();
        double clipDur  = clip->duration;
        double posStart = timelineTime - clip->startTime;
        double fadeStart = clipDur * (1.0 - fadeFraction);
        if (posStart + duration > fadeStart) {
            for (int i = 0; i < audio.size(); ++i) {
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

// =====================================================================
//  namespace Effects — свободные функции, доступны всем
//  (RenderWorker::applyClipEffects + RenderEngine статические методы)
// =====================================================================
namespace Effects {

static void rgbToHsv(int r, int g, int b, double& h, double& s, double& v) {
    double rd = r/255.0, gd = g/255.0, bd = b/255.0;
    double mx = std::max({rd,gd,bd}), mn = std::min({rd,gd,bd}), d = mx - mn;
    v = mx;
    s = (mx > 0) ? d/mx : 0;
    if (d < 1e-6) { h = 0; return; }
    if      (mx == rd) h = 60.0*((gd-bd)/d + (gd<bd ? 6 : 0));
    else if (mx == gd) h = 60.0*((bd-rd)/d + 2);
    else               h = 60.0*((rd-gd)/d + 4);
}

static void hsvToRgb(double h, double s, double v, int& r, int& g, int& b) {
    h = fmod(h, 360.0); if (h < 0) h += 360.0;
    int i = (int)(h/60) % 6;
    double f = h/60 - (int)(h/60);
    double p = v*(1-s), q = v*(1-f*s), t = v*(1-(1-f)*s);
    double rd,gd,bd;
    switch(i) {
    case 0: rd=v;gd=t;bd=p;break; case 1: rd=q;gd=v;bd=p;break;
    case 2: rd=p;gd=v;bd=t;break; case 3: rd=p;gd=q;bd=v;break;
    case 4: rd=t;gd=p;bd=v;break; default:rd=v;gd=p;bd=q;break;
    }
    r=qBound(0,(int)(rd*255),255); g=qBound(0,(int)(gd*255),255); b=qBound(0,(int)(bd*255),255);
}

QImage blur(const QImage& frame, double radius) {
    QImage src = frame.convertToFormat(QImage::Format_RGB888);
    QImage result = src.copy();
    int r = qMax(1, static_cast<int>(radius));
    int w = src.width(), h = src.height();
    QImage temp = src.copy();
    for (int y = 0; y < h; ++y) {
        const uchar* sl = src.constScanLine(y);
        uchar* dl = temp.scanLine(y);
        for (int x = 0; x < w; ++x) {
            int sR=0,sG=0,sB=0,cnt=0;
            for (int dx=-r;dx<=r;++dx){int nx=qBound(0,x+dx,w-1);sR+=sl[nx*3];sG+=sl[nx*3+1];sB+=sl[nx*3+2];++cnt;}
            dl[x*3]=sR/cnt; dl[x*3+1]=sG/cnt; dl[x*3+2]=sB/cnt;
        }
    }
    for (int x=0;x<w;++x) for (int y=0;y<h;++y) {
            int sR=0,sG=0,sB=0,cnt=0;
            for (int dy=-r;dy<=r;++dy){int ny=qBound(0,y+dy,h-1);const uchar*l=temp.constScanLine(ny);sR+=l[x*3];sG+=l[x*3+1];sB+=l[x*3+2];++cnt;}
            uchar* out=result.scanLine(y); out[x*3]=sR/cnt; out[x*3+1]=sG/cnt; out[x*3+2]=sB/cnt;
        }
    return result;
}

QImage sharpness(const QImage& frame, double strength) {
    QImage blurred = blur(frame, 1.0);
    QImage src  = frame.convertToFormat(QImage::Format_RGB888);
    QImage bsrc = blurred.convertToFormat(QImage::Format_RGB888);
    QImage result = src.copy();
    for (int y=0;y<src.height();++y) {
        const uchar*s=src.constScanLine(y); const uchar*b=bsrc.constScanLine(y); uchar*d=result.scanLine(y);
        for (int x=0;x<src.width()*3;++x) { int v=(int)s[x]+(int)((s[x]-b[x])*strength); d[x]=(uchar)qBound(0,v,255); }
    }
    return result;
}

QImage hue(const QImage& frame, double degrees) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width();++x) {
            int idx=x*3; double h,s,v;
            rgbToHsv(line[idx],line[idx+1],line[idx+2],h,s,v);
            h = fmod(h + degrees + 360.0, 360.0);
            int r,g,b; hsvToRgb(h,s,v,r,g,b);
            line[idx]=r; line[idx+1]=g; line[idx+2]=b;
        }
    }
    return result;
}

QImage sepia(const QImage& frame, double intensity) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width();++x) {
            int idx=x*3, r=line[idx], g=line[idx+1], b=line[idx+2];
            int sr=qBound(0,(int)(r*0.393+g*0.769+b*0.189),255);
            int sg=qBound(0,(int)(r*0.349+g*0.686+b*0.168),255);
            int sb=qBound(0,(int)(r*0.272+g*0.534+b*0.131),255);
            line[idx]  =(uchar)(r+(sr-r)*intensity);
            line[idx+1]=(uchar)(g+(sg-g)*intensity);
            line[idx+2]=(uchar)(b+(sb-b)*intensity);
        }
    }
    return result;
}

QImage vignette(const QImage& frame, double strength) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    int w=result.width(), h=result.height();
    double cx=w/2.0, cy=h/2.0, maxDist2=cx*cx+cy*cy;
    for (int y=0;y<h;++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<w;++x) {
            double dx=x-cx, dy=y-cy;
            double factor=qBound(0.0, 1.0-strength*(dx*dx+dy*dy)/maxDist2, 1.0);
            int idx=x*3;
            line[idx]  =(uchar)(line[idx]  *factor);
            line[idx+1]=(uchar)(line[idx+1]*factor);
            line[idx+2]=(uchar)(line[idx+2]*factor);
        }
    }
    return result;
}

// Инверсия цветов
QImage invert(const QImage& frame) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width()*3;++x)
            line[x] = 255 - line[x];
    }
    return result;
}

// Постеризация: levels 2..8 — уменьшает количество цветов
QImage posterize(const QImage& frame, double levels) {
    int lvl = qBound(2, static_cast<int>(levels), 16);
    int step = 256 / lvl;
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width()*3;++x)
            line[x] = (uchar)qBound(0, (line[x] / step) * step, 255);
    }
    return result;
}

// Пикселизация: blockSize 1..64
QImage pixelate(const QImage& frame, double blockSize) {
    int bs = qBound(2, static_cast<int>(blockSize), 64);
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    int w=result.width(), h=result.height();
    for (int by=0;by<h;by+=bs) for (int bx=0;bx<w;bx+=bs) {
            // Средний цвет блока
            long sR=0,sG=0,sB=0,cnt=0;
            for (int dy=0;dy<bs&&by+dy<h;++dy) for (int dx=0;dx<bs&&bx+dx<w;++dx) {
                    const uchar*l=result.constScanLine(by+dy); int idx=(bx+dx)*3;
                    sR+=l[idx]; sG+=l[idx+1]; sB+=l[idx+2]; ++cnt;
                }
            uchar r=(uchar)(sR/cnt), g=(uchar)(sG/cnt), b=(uchar)(sB/cnt);
            // Заливаем блок
            for (int dy=0;dy<bs&&by+dy<h;++dy) {
                uchar*l=result.scanLine(by+dy);
                for (int dx=0;dx<bs&&bx+dx<w;++dx) {
                    int idx=(bx+dx)*3; l[idx]=r; l[idx+1]=g; l[idx+2]=b;
                }
            }
        }
    return result;
}

// Цветовая температура: value > 0 = тёплый (больше красного), < 0 = холодный (больше синего)
QImage temperature(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    int warmR = static_cast<int>( value * 30);  // +/- до 30 единиц
    int warmB = static_cast<int>(-value * 20);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width();++x) {
            int idx=x*3;
            line[idx]   = (uchar)qBound(0, (int)line[idx]   + warmR, 255);
            line[idx+2] = (uchar)qBound(0, (int)line[idx+2] + warmB, 255);
        }
    }
    return result;
}

// Цветовой тинт: окрашивает изображение в заданный цвет (hue 0..360, strength 0..1)
QImage tint(const QImage& frame, double hue, double strength) {
    // Конвертируем hue в RGB-цвет тинта
    int tr,tg,tb;
    hsvToRgb(hue, 1.0, 1.0, tr, tg, tb);
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width();++x) {
            int idx=x*3;
            int r=line[idx], g=line[idx+1], b=line[idx+2];
            int gray=(int)(0.299*r+0.587*g+0.114*b);
            // Смешиваем серый с цветом тинта
            line[idx]  =(uchar)qBound(0,(int)(gray+(tr-gray)*strength),255);
            line[idx+1]=(uchar)qBound(0,(int)(gray+(tg-gray)*strength),255);
            line[idx+2]=(uchar)qBound(0,(int)(gray+(tb-gray)*strength),255);
        }
    }
    return result;
}


} // namespace Effects

// =====================================================================
//  RenderWorker — методы эффектов
// =====================================================================

QImage RenderWorker::applyClipEffects(const QImage& frame, const TimelineClip& clip) {
    if (clip.effects.isEmpty()) return frame;
    QImage result = frame;
    for (auto it = clip.effects.begin(); it != clip.effects.end(); ++it) {
        const QString& name = it.key();
        double value = it.value();
        if      (name == "brightness")                  result = applyBrightness(result, value);
        else if (name == "contrast")                    result = applyContrast(result, value);
        else if (name == "saturation")                  result = applySaturation(result, value);
        else if (name == "grayscale"  && value > 0.5)   result = applyGrayscale(result);
        else if (name == "blur"       && value > 0.0)   result = Effects::blur(result, value);
        else if (name == "sharpness"  && value > 0.0)   result = Effects::sharpness(result, value);
        else if (name == "hue"        && value != 0.0)  result = Effects::hue(result, value);
        else if (name == "sepia"      && value > 0.0)   result = Effects::sepia(result, value);
        else if (name == "vignette"   && value > 0.0)   result = Effects::vignette(result, value);
        else if (name == "invert"     && value > 0.5)   result = Effects::invert(result);
        else if (name == "posterize"  && value >= 2.0)  result = Effects::posterize(result, value);
        else if (name == "pixelate"   && value >= 2.0)  result = Effects::pixelate(result, value);
        else if (name == "temperature"&& value != 0.0)  result = Effects::temperature(result, value);
        else if (name == "tint_hue"                  )  {
            double tintStr = clip.effects.value("tint_strength", 0.5);
            if (tintStr > 0.0) result = Effects::tint(result, value, tintStr);
        }
        // volume/reverb/echo/mono/stereo/pitch/normalize/fade обрабатываются в decodeAudioChunk
    }
    return result;
}

// Яркость: аддитивный сдвиг -1.0..+1.0 → ±255
QImage RenderWorker::applyBrightness(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    int shift = static_cast<int>(value * 255.0);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width()*3;++x) { int v=(int)line[x]+shift; line[x]=(uchar)qBound(0,v,255); }
    }
    return result;
}

// Контраст: value 0..3, 1.0 = оригинал
QImage RenderWorker::applyContrast(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width()*3;++x) { int v=(int)((line[x]-128)*value+128); line[x]=(uchar)qBound(0,v,255); }
    }
    return result;
}

QImage RenderWorker::applySaturation(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width();++x) {
            int idx=x*3, r=line[idx], g=line[idx+1], b=line[idx+2];
            int gray=(int)(0.299*r+0.587*g+0.114*b);
            line[idx]  =(uchar)qBound(0,gray+(int)((r-gray)*value),255);
            line[idx+1]=(uchar)qBound(0,gray+(int)((g-gray)*value),255);
            line[idx+2]=(uchar)qBound(0,gray+(int)((b-gray)*value),255);
        }
    }
    return result;
}

QImage RenderWorker::applyGrayscale(const QImage& frame) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width();++x) {
            int idx=x*3, gray=(int)(0.299*line[idx]+0.587*line[idx+1]+0.114*line[idx+2]);
            line[idx]=line[idx+1]=line[idx+2]=(uchar)gray;
        }
    }
    return result;
}

// =====================================================================
//  RenderEngine — менеджер (главный поток)
// =====================================================================

RenderEngine::RenderEngine(QObject *parent)
    : QObject(parent)
    , m_outputWidth(1920)
    , m_outputHeight(1080)
    , m_fps(30.0)
    , m_bitrate(5000000)
    , m_thread(nullptr)
    , m_worker(nullptr)
{}

RenderEngine::~RenderEngine() {
    cancel();
    if (m_thread) { m_thread->quit(); m_thread->wait(3000); delete m_thread; }
}

void RenderEngine::setClips(const QList<TimelineClip>& clips) { m_clips = clips; }
void RenderEngine::setOutputPath(const QString& path) { m_outputPath = path; }
void RenderEngine::setOutputResolution(int w, int h) { m_outputWidth = w; m_outputHeight = h; }
void RenderEngine::setOutputCodec(const QString& codec) { m_codec = codec; }
void RenderEngine::setOutputFormat(const QString& format) { m_format = format; }
void RenderEngine::setFps(double fps) { m_fps = fps; }
void RenderEngine::setBitrate(int bitrate) { m_bitrate = bitrate; }

bool RenderEngine::startRender() {
    if (m_clips.isEmpty())     { emit error("Нет клипов для рендеринга"); emit renderFinished(false); return false; }
    if (m_outputPath.isEmpty()) { emit error("Не указан путь для сохранения"); emit renderFinished(false); return false; }

    if (m_thread) { m_thread->quit(); m_thread->wait(2000); delete m_thread; m_thread = nullptr; }

    m_thread = new QThread();
    m_worker = new RenderWorker();
    m_worker->moveToThread(m_thread);

    m_worker->setClips(m_clips);
    m_worker->setOutputPath(m_outputPath);
    m_worker->setOutputResolution(m_outputWidth, m_outputHeight);
    m_worker->setFps(m_fps);
    m_worker->setBitrate(m_bitrate);

    connect(m_thread, &QThread::started,              m_worker, &RenderWorker::process);
    connect(m_worker, &RenderWorker::progressChanged, this,     &RenderEngine::progressChanged);
    connect(m_worker, &RenderWorker::renderFinished,  this,     &RenderEngine::renderFinished);
    connect(m_worker, &RenderWorker::errorOccurred,   this,     &RenderEngine::error);
    connect(m_worker, &RenderWorker::renderFinished,  m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished,             m_worker, &QObject::deleteLater);

    m_thread->start();
    return true;
}

void RenderEngine::cancel() { if (m_worker) m_worker->cancel(); }

// ===== СТАТИЧЕСКИЕ ЭФФЕКТЫ ДЛЯ ПРЕВЬЮ =====

QImage RenderEngine::applyBrightness(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    int shift = static_cast<int>(value * 255.0);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width()*3;++x) { int v=(int)line[x]+shift; line[x]=(uchar)qBound(0,v,255); }
    }
    return result;
}

QImage RenderEngine::applyContrast(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width()*3;++x) { int v=(int)((line[x]-128)*value+128); line[x]=(uchar)qBound(0,v,255); }
    }
    return result;
}

QImage RenderEngine::applySaturation(const QImage& frame, double value) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width();++x) {
            int idx=x*3, r=line[idx], g=line[idx+1], b=line[idx+2];
            int gray=(int)(0.299*r+0.587*g+0.114*b);
            line[idx]  =(uchar)qBound(0,gray+(int)((r-gray)*value),255);
            line[idx+1]=(uchar)qBound(0,gray+(int)((g-gray)*value),255);
            line[idx+2]=(uchar)qBound(0,gray+(int)((b-gray)*value),255);
        }
    }
    return result;
}

QImage RenderEngine::applyGrayscale(const QImage& frame) {
    QImage result = frame.convertToFormat(QImage::Format_RGB888);
    for (int y=0;y<result.height();++y) {
        uchar* line=result.scanLine(y);
        for (int x=0;x<result.width();++x) {
            int idx=x*3, gray=(int)(0.299*line[idx]+0.587*line[idx+1]+0.114*line[idx+2]);
            line[idx]=line[idx+1]=line[idx+2]=(uchar)gray;
        }
    }
    return result;
}

// Новые статические методы — делегируют в namespace Effects
QImage RenderEngine::applyBlur(const QImage& frame, double radius)        { return Effects::blur(frame, radius); }
QImage RenderEngine::applySharpness(const QImage& frame, double strength)  { return Effects::sharpness(frame, strength); }
QImage RenderEngine::applyHue(const QImage& frame, double degrees)         { return Effects::hue(frame, degrees); }
QImage RenderEngine::applySepia(const QImage& frame, double intensity)     { return Effects::sepia(frame, intensity); }
QImage RenderEngine::applyVignette(const QImage& frame, double strength)   { return Effects::vignette(frame, strength); }
QImage RenderEngine::applyInvert(const QImage& frame)                      { return Effects::invert(frame); }
QImage RenderEngine::applyPosterize(const QImage& frame, double levels)    { return Effects::posterize(frame, levels); }
QImage RenderEngine::applyPixelate(const QImage& frame, double blockSize)  { return Effects::pixelate(frame, blockSize); }
QImage RenderEngine::applyTemperature(const QImage& frame, double value)   { return Effects::temperature(frame, value); }
QImage RenderEngine::applyTint(const QImage& frame, double hue, double s)  { return Effects::tint(frame, hue, s); }
