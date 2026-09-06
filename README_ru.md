# VideoEditor Pro
<kbd>[<img title="English (United States)" alt="English (United States)" src="https://flagcdn.com" width="22">]()</kbd>


A cross-platform non-linear video editor built in C++17 with Qt 6 and FFmpeg.  
Designed as a lightweight, offline-first alternative to proprietary editors.

![License](https://img.shields.io/badge/license-MIT-blue)
![Qt](https://img.shields.io/badge/Qt-6.x-green)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![FFmpeg](https://img.shields.io/badge/FFmpeg-6.x-orange)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)

---

## Overview

VideoEditor Pro is a desktop video editing application that provides a full
non-linear editing workflow: import media, arrange clips on a multi-track
timeline, apply real-time effects, preview with audio/video sync, and export
to common container formats.

The project was developed as a Bachelor's thesis at Moscow Polytechnic
University (2026), direction 09.03.02 "Information Systems and Technologies".

---

## Features

### Video editing
- **Two-track non-linear timeline** — arrange, move, trim, and split clips
  on two independent video tracks
- **Non-destructive editing** — all operations reference source files;
  originals are never modified
- **Clip operations** — add, remove, move, trim (left/right edge), split at
  arbitrary position, drag-and-drop reorder

### Decoding & playback
- **Hardware-accelerated decoding** with automatic fallback chain:
  D3D11VA → DXVA2 → software (CPU)
- **Prefetch frame cache** (`FrameCache`) — sliding-window cache of 150
  decoded frames per track, keeps playback smooth without re-decoding
- **Background decoder thread** (`DecoderThread`) — decodes 2.5 s ahead of
  the playback position; fast seek via `seekAndDecode()` jumps directly to
  the nearest I-frame
- **Audio/video synchronization** — push-model audio engine
  (`AudioPlaybackEngine`) feeds 20 ms chunks to `QAudioSink`; audible
  position is corrected for sink buffer latency on every tick
- **Preview mode** — decoder outputs half-resolution frames (÷4 pixel count)
  to reduce CPU load during live playback; full resolution is used only during
  export

### Video effects (14)
| Effect | Description |
|--------|-------------|
| Brightness | Linear shift of all channels |
| Contrast | Scale around mid-point 128 |
| Saturation | HSV-space S-component scaling (BT.601 luma) |
| Grayscale | BT.601 weighted luminance |
| Blur | Separable box filter (2 passes, O(2r+1) per pixel) |
| Sharpness | Unsharp mask (original − blurred) × strength |
| Hue | HSV hue rotation by arbitrary degrees |
| Sepia | 3×3 empirical mixing matrix + intensity blend |
| Vignette | Radial darkening (squared distance, no sqrt) |
| Invert | 255 − I per channel |
| Posterize | Uniform quantization to N levels |
| Pixelate | Block-average downscaling |
| Temperature | Asymmetric R/B channel shift |
| Grain | Deterministic XOR-shift noise keyed on (x, y, frame) |
| ChromaKey | RGB greenness metric + soft edge + spill suppression |

### Audio effects
- Reverb — comb filter with persistent ring buffer (feedback < 1)
- Echo — delay line with configurable feedback
- Stereo Widen — Mid/Side encoding with Side gain boost
- Pitch Shift — linear-interpolation resampling (equal temperament: 2^(s/12))
- Normalize — peak-based gain with ×6 cap
- Fade In / Fade Out — per-sample linear gain ramp
- Automatic micro-fade — 3 ms tail fade on every clip boundary (eliminates
  clicks)

### Export
- Video codec: **H.264** (libx264, VBR)
- Audio codec: **AAC**
- Containers: **MP4**, **MKV**, **AVI**, **MOV**, **WebM**
- Resolution: configurable (default 1920×1080)
- Progress reporting and cancellation at any time
- Exact audio sample counting per frame (prevents audio/video drift at non-integer fps)

### Project persistence
- Projects saved as **JSON** (clip paths, positions, trim points, effects map)
- Portable: file contains only paths + parameters, no media data
- Typical project file size: a few kilobytes

### UI
- QML / Qt Quick interface with hardware-accelerated rendering (OpenGL)
- Dark theme (Steam-inspired navy + ruby accent palette)
- All colors, fonts, and spacing defined in `theme.js`
- Components: `TopMenuBar`, `LeftSidebar`, `VideoPlayer`, `PlaybackControls`,
  `Timeline`, `Track`, `VideoClip`, `ClipEffectsDialog`

---

## Architecture

```
QML (UI layer)
    │  signals / slots / Q_INVOKABLE
    ▼
Timeline  ──────────────────────────────► EffectImageProvider
(C++ core, QObject, exported to QML)          ▲ setFrame()
    │                                          │ image://effects/frame
    ├── QList<TimelineClip>                    │
    │                                     QML VideoPlayer
    ├── MediaDecoder  ◄── DecoderThread ──► FrameCache
    │   (GPU / CPU fallback, FFmpeg)
    │
    ├── AudioPlaybackEngine
    │   (QAudioSink push-mode, 44100 Hz stereo float32)
    │
    ├── RenderEngine
    │   └── RenderWorker  (QThread)
    │       ├── MediaDecoder  (full-res video)
    │       ├── MediaDecoder  (audio)
    │       └── MediaEncoder  → output file
    │
    └── FFmpeg
        libavformat · libavcodec · libswscale · libswresample
```

### Key design decisions

| Problem | Solution |
|---------|----------|
| Windows WASAPI thread limit (~64 threads) exhausted after ~20 seeks | Reuse single `QAudioSink` via `reset()` + `start()` instead of recreating |
| FrameCache eviction deleting frames just ahead of playback → infinite re-decode loop | Evict only frames > KEEP_BEHIND positions *behind* play position; delete furthest-ahead only when no old frames exist |
| Audio/video drift at non-integer fps | Count audio samples per frame as `round((f+1)/fps × SR) − samplesWritten` instead of `(int)(frameTime × SR)` |
| Seek between clips from the same file on track 2 → decoder sequential-decodes the gap → stutter | Detect position jump > 2 s in `updatePlayPosition()` and trigger a full seek |
| Slow seek (`getFrameAt`) vs fast prefetch decode | `seekAndDecode()` seeks to I-frame and skips intermediate frames without `sws_scale`, then `getNextFrame()` continues sequentially |

---

## Tech Stack

| Component | Technology |
|-----------|-----------|
| Language | C++17 |
| UI framework | Qt 6.5+ (Qt Quick, Qt Multimedia, Qt Concurrent) |
| UI language | QML |
| Media decoding/encoding | FFmpeg 6.x (libavformat, libavcodec, libswscale, libswresample) |
| GPU decoding | D3D11VA, DXVA2 (Windows) |
| Build system | qmake |
| IDE | Qt Creator |

---

## Project Structure

```
VideoEditorProject/
├── main.cpp                  # Entry point — init OpenGL, register QML context
├── timeline.h / .cpp         # Core coordinator — all clip operations, playback control
├── TimelineClip.h            # Data structure for a single clip on the timeline
├── mediadecoder.h / .cpp     # FFmpeg decoder wrapper (video + audio, GPU fallback)
├── mediaencoder.h / .cpp     # FFmpeg encoder wrapper (H.264 + AAC muxer)
├── renderengine.h / .cpp     # Export engine — compositing, effects, RenderWorker thread
├── audioplaybackengine.h/.cpp# Push-mode audio output (QAudioSink)
├── decoderthread.h           # Background prefetch decode thread
├── FrameCache.h              # Thread-safe sliding-window frame cache
├── Effectimageprovider.h     # QQuickImageProvider bridge — C++ frame → QML Image
├── qml/
│   ├── main.qml              # Root window layout
│   ├── VideoPlayer.qml       # Preview area (image://effects/ source)
│   ├── Timeline.qml          # Timeline with zoom and playhead
│   ├── Track.qml             # Single video track
│   ├── VideoClip.qml         # Draggable clip block with trim handles
│   ├── PlaybackControls.qml  # Play/pause/seek/speed/volume bar
│   ├── LeftSidebar.qml       # Media import panel
│   ├── TopMenuBar.qml        # File / Export / Settings menu
│   ├── ClipEffectsDialog.qml # Per-clip effect sliders
│   ├── ModeSwitcher.qml      # Track mode controls
│   └── theme.js              # Design tokens (colors, fonts, spacing)
└── VideoEditorProject.pro    # qmake build file
```

---

## Build Instructions

### Prerequisites

| Dependency | Version | Notes |
|-----------|---------|-------|
| Qt | 6.5+ | Modules: Quick, Multimedia, Concurrent, Widgets |
| FFmpeg | 6.x | Built with libx264 and AAC encoder |
| Compiler | MSVC 2019/2022 (Windows) or GCC 10+ / Clang 12+ | C++17 required |

### Windows (Qt Creator)

1. Install Qt 6.5+ via the Qt online installer
2. Download a pre-built FFmpeg Windows build with shared libraries
   ([gyan.dev](https://www.gyan.dev/ffmpeg/builds/) recommended)
3. Extract FFmpeg to `C:/FFmpegForQt/` (or edit `INCLUDEPATH` / `LIBS` in `.pro`)
4. Open `VideoEditorProject.pro` in Qt Creator and press **Build**

The post-build step in `.pro` automatically copies FFmpeg DLLs to the output
directory:
```
xcopy /Y /D "C:/FFmpegForQt/bin/*.dll" "<output_dir>"
```

### Linux / macOS

Install FFmpeg via package manager:
```bash
# Ubuntu / Debian
sudo apt install libavcodec-dev libavformat-dev libavutil-dev \
                 libswscale-dev libswresample-dev

# macOS (Homebrew)
brew install ffmpeg
```

Edit `.pro` to replace Windows `INCLUDEPATH` / `LIBS` with pkg-config output:
```pro
LIBS += $$system(pkg-config --libs libavcodec libavformat libavutil libswscale libswresample)
INCLUDEPATH += $$system(pkg-config --cflags-only-I libavcodec | sed 's/-I//g')
```

Then build:
```bash
qmake VideoEditorProject.pro && make -j$(nproc)
```

> **Note:** GPU hardware decoding (D3D11VA / DXVA2) is Windows-only. On Linux
> and macOS the decoder automatically falls back to software (CPU) mode. VA-API
> support for Linux is a planned future improvement.

---

## Performance

Tested on two machines:

| Resolution | Decode mode | CPU load (IdeaPad L340, i3) | CPU load (Desktop, i5-14500 + RTX 5060 Ti) |
|-----------|------------|----------------------------|---------------------------------------------|
| 720p | CPU software | 85% | 9.1% |
| 1080p | CPU software | 93% | 11.8% |
| 1080p | GPU D3D11VA | 87% | 5.4% |
| 4K | GPU D3D11VA | 98% | 9.6% |

RAM usage during preview: 620–770 MB (dominated by FrameCache — up to 150
decoded frames per track).

---

## Roadmap

- [ ] More than 2 video tracks
- [ ] Text overlays and title cards
- [ ] Subtitle support (libass)
- [ ] GPU-accelerated export (NVENC for NVIDIA, AMF for AMD)
- [ ] VA-API hardware decoding on Linux
- [ ] Transition effects between adjacent clips
- [ ] Audio waveform visualization in timeline clips

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

## Author

**Ilya Kartashov** — [github.com/kartashov](https://github.com/)  
Bachelor's thesis, Moscow Polytechnic University, 2026  
Direction: 09.03.02 "Information Systems and Technologies"
