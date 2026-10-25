# VideoFlowSmooth

A lightweight C++17 command-line tool that **smooths video by interpolating
intermediate frames** using optical flow estimation. Given an input video it
produces a higher-frame-rate output that plays back smoothly — ideal for
turning 30 fps footage into 60 fps (or more) content.

It uses **OpenCV** for frame processing and **ffmpeg / ffprobe** to extract,
probe and re-mux the original audio track so no sound is lost — with robust
error handling and graceful fallbacks when things go wrong.

---

## Features

- **Optical-flow-based frame interpolation** — truly *creates* new frames,
  unlike simple frame duplication
- **Two interchangeable algorithms:**
  - **Farneback** (default) — fast, CPU-only, good visual quality
  - **DIS** — higher quality, selected via the `--dis` flag
- **Configurable frame-rate multiplier** (`-m 2` → 2×, `-m 4` → 4×, etc.)
- **Scene-change detection** — automatically detects hard cuts between
  scenes and **duplicates** frames at those points instead of interpolating,
  which avoids ghosting / blending artefacts. Threshold is configurable.
  - `[-s <thresh>]` — set scene-change threshold (0.0–1.0); lower = more
    cuts detected
  - `[--no-scene]` — disable scene detection entirely
- **Audio handling via ffmpeg (optional):**
  - **Probes** the input first (ffprobe) to detect whether an audio stream
    exists — videos without audio are handled gracefully, no error
  - **Extracts** audio to AAC @ 192 kbps and stores it in `audio/`
  - **Re-muxes** the smoothed video and audio back together as the final
    output (H.264 / aac for broad compatibility)
  - `--no-audio` — skip audio entirely (video-only output)
  - If ffmpeg is not installed, the tool still renders the smoothed video
    (audio is simply omitted — see [Requirements](#requirements))
- **Robust ffmpeg error handling:**
  - Captures stdout + stderr, filters out ffmpeg's banner lines, and checks
    against a list of known critical errors (e.g. *moov atom not found*,
    *No such file*, *could not find codec*, *Invalid data found*…)
  - On failure, prints a **friendly, actionable error message** with likely
    causes and suggested fixes (re-download, update ffmpeg, retry with
    `--no-audio`)
- **Progress reporting** during processing:
  - Percentage complete (when total frame count is available)
  - Real-time processing fps
  - Estimated time of arrival (**ETA**) formatted as `1m 30s`, `2h 15m`, etc.
  - Scene-cut annotations
- **Automatic cleanup** of intermediate files and folders (`audio/`, `temp/`)
- **VideoWriter codec fallback** — tries H.264/mp4v first, falls back to
  MJPEG/AVI if the primary codec is unavailable
- Cross-platform CMake build (Windows-focused; easy to adapt to Linux/macOS)

---

## Requirements

| Dependency       | Version / Notes                                            |
|------------------|------------------------------------------------------------|
| C++ compiler     | C++17 support (MSVC, GCC 9+, Clang 10+)                   |
| CMake            | ≥ 3.15                                                     |
| OpenCV           | 4.x (prebuilt or built from source)                        |
| ffmpeg + ffprobe | *(optional but recommended)* — needed for audio handling   |

> **ffmpeg is optional.** Without it the tool still produces a smoothed,
> audio-less video. Download from [gyan.dev](https://www.gyan.dev/ffmpeg/builds/)
> (choose the **full** build), extract, and add the `bin` folder to your `PATH`.
> `ffprobe` is included in the same package.

---

## Building

### Windows (MSVC + prebuilt OpenCV)

1. Install OpenCV 4.x (prebuilt Windows package).
2. Set the OpenCV build directory (default assumed: `C:/opencv/opencv/build`).

```powershell
cd video-smoothing
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

If your OpenCV is installed elsewhere, point CMake to it:

```powershell
cmake -S . -B build -DOpenCV_DIR="C:/path/to/opencv/build" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

After building, the executable and required OpenCV DLLs are copied to
`build/Release/video_flow_smooth.exe`.

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
# ensure ffmpeg + ffprobe are installed, e.g.:
#   sudo apt install ffmpeg
```

---
## CLI Options

```
video_flow_smooth -i <input> -o <output> -m <multiplier> [options]
```

| Option          | Description                                                                  | Default              |
|-----------------|------------------------------------------------------------------------------|----------------------|
| `-i <file>`     | Input video path                                                             | `input.mp4`          |
| `-o <file>`     | Output video path (must differ from input)                                   | `output.mp4`         |
| `-m <N>`        | Frame-rate multiplier (minimum 2)                                            | `2`                  |
| `-s <thresh>`   | Scene-change threshold, 0.0–1.0 (lower = more cuts detected)                | `0.4`                |
| `--dis`         | Use the DIS optical-flow algorithm instead of Farneback                     | Farneback            |
| `--no-scene`    | Disable scene-change detection (always interpolate)                         | *(detection on)*     |
| `--no-audio`    | Skip audio extraction / muxing (video-only output)                          | *(audio on if avail)*|
| `-h`/`--help`   | Print usage                                                                  | —                    |

### Examples

```powershell
# Double the frame rate (30 → 60 fps), keeping audio
.\build\Release\video_flow_smooth.exe -i input.mp4 -o output.mp4 -m 2

# 4× frame rate with the higher-quality DIS algorithm
.\build\Release\video_flow_smooth.exe -i input.mp4 -o smooth.mp4 -m 4 --dis

# 4× with aggressive scene-cut detection (threshold 0.2)
.\build\Release\video_flow_smooth.exe -i input.mp4 -o smooth.mp4 -m 4 -s 0.2

# Video-only — skip audio entirely
.\build\Release\video_flow_smooth.exe -i input.mp4 -o smooth.mp4 -m 3 --no-audio
```

---

## How It Works

The tool processes consecutive frame pairs and synthesises new frames in
between:

```
pair (frame[n], frame[n+1])
        │
        ├── compute forward  optical flow  frame[n]    → frame[n+1]
        ├── compute backward optical flow  frame[n+1] → frame[n]
        ├── (optional) Gaussian-smoothen the flow fields (sigma = 1.5)
        ├── compute scene-change correlation (histogram comparison)
        │
        ├── if SCENE CUT detected  →  duplicate frame[n] (avoid ghosting)
        │
        └── if NO cut  →  for each intermediate step t in (0..1):
                warp frame[n]    forward  by  t
                warp frame[n+1]  backward by  1-t
                blend the two warped frames → new intermediate frame
```

The interpolated frames are written to a **temporary video** (`temp/`), then
the original audio is **extracted up-front** (`audio/`) and **muxed back in**
as a final step (re-encoded to H.264 / aac for broad compatibility).

---

## Output Directories

| Folder   | Purpose                                         |
|----------|-------------------------------------------------|
| `audio/` | Extracted audio files (reusable across runs)  |
| `temp/`  | Intermediate video + ffmpeg log capture       |

Intermediate video files are deleted automatically after the final output is
written.

---

## Build Profile Reference

The CMake configuration assumes an OpenCV prebuilt at
`C:/opencv/opencv/build` by default. Override it with `-DOpenCV_DIR=...` if
yours lives elsewhere. On Windows, OpenCV DLLs are automatically copied next to
the executable during the post-build step.

---

## Performance Notes

- **Farneback** is the fastest option and runs on CPU only.
- **DIS** produces sharper motion but is slower; still CPU-only.
- Processing time grows roughly linearly with input frame count and the
  multiplier value.
- The optical flow is computed at **full resolution**; for large videos consider
  downsampling the source beforehand.
- Scene-change detection adds a small per-frame cost (histogram comparison) but
  prevents artefacts at hard cuts — disable with `--no-scene` if every frame
  should be interpolated unconditionally.

---

## Troubleshooting

| Problem | Likely Cause / Fix |
|---------|--------------------|
| *moov atom not found* | File is corrupt or incomplete — re-download or open in VLC to verify |
| *No audio in output* | Run with the default (audio on); if it fails, try `--no-audio` |
| *Audio extraction fails* | Update ffmpeg to the latest **full** build from [gyan.dev](https://www.gyan.dev/ffmpeg/builds/) |
| *VideoWriter won't open* (mp4v) | Codec unavailable — the tool auto-falls-back to MJPEG/AVI |
| *ffmpeg: command not found* | ffmpeg not on `PATH` — the tool will skip audio and continue |

---

## License

This project is provided as-is for personal / educational use.