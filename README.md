# VideoFlowSmooth

A lightweight C++17 command-line tool that **smooths video by interpolating
intermediate frames** using optical flow estimation. Given an input video it
generates a higher-frame-rate output that plays back smoothly — ideal for
turning 30 fps footage into 60 fps content.

It uses **OpenCV** for frame processing and (optionally) **ffmpeg** to extract
and re-mux the original audio track so no sound is lost.

---

## Features

- Optical-flow-based frame interpolation (true frame creation, not simple duplication)
- Two interchangeable algorithms:
  - **Farneback** (default) — fast, CPU-only, good visual quality
  - **DIS** — higher quality, optional via `--dis` flag
- Configurable frame-rate multiplier (`-m 2` → 2×, `-m 4` → 4×, etc.)
- Automatic **audio extraction** and **re-muxing** through ffmpeg
  - Falls back gracefully when audio is absent or ffmpeg is not installed
- Automatic directory creation (`audio/`, `temp/`) and intermediate-file cleanup
- Cross-platform CMake build (Windows-focused; easy to adapt to Linux/macOS)

---

## Requirements

| Dependency       | Version / Notes                                            |
|------------------|------------------------------------------------------------|
| C++ compiler     | C++17 support (MSVC, GCC 9+, Clang 10+)                   |
| CMake            | ≥ 3.15                                                     |
| OpenCV           | 4.x (prebuilt or built from source)                       |
| ffmpeg           | *(optional but recommended)* — only needed for audio handling |

> **ffmpeg is optional.** Without it the tool still produces a smoothed video,
> just without audio. Download from [gyan.dev](https://www.gyan.dev/ffmpeg/builds/)
> and add the `bin` folder to your `PATH`.

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
# ensure ffmpeg is installed, e.g.: sudo apt install ffmpeg
```

---

## Usage

```
video_flow_smooth -i <input> -o <output> -m <multiplier> [--dis]
```

| Option       | Description                                                         | Default        |
|--------------|---------------------------------------------------------------------|----------------|
| `-i <file>`  | Input video path                                                    | `input.mp4`    |
| `-o <file>`  | Output video path (must differ from input)                          | `output.mp4`   |
| `-m <N>`     | Frame-rate multiplier (minimum 2)                                   | `2`            |
| `--dis`      | Use the DIS optical-flow algorithm instead of Farneback            | Farneback      |
| `-h`/`--help`| Print usage                                                         | —              |

### Examples

```powershell
# Double the frame rate (30 → 60 fps), keeping audio
.\build\Release\video_flow_smooth.exe -i input.mp4 -o output.mp4 -m 2

# 4× frame rate with the higher-quality DIS algorithm
.\build\Release\video_flow_smooth.exe -i input.mp4 -o smooth.mp4 -m 4 --dis
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
        ├── (optional) Gaussian-smoothen the flow fields
        └── for each intermediate step t in (0..1):
              warp frame[n]    forward  by  t
              warp frame[n+1]  backward by  1-t
              blend the two warped frames → new intermediate frame
```

The interpolated frames are written to a temporary video, then the original
audio is extracted up-front and muxed back in as a final step (re-encoded to
H.264/aac for broad compatibility).

---

## Output Directories

| Folder   | Purpose                                         |
|----------|-------------------------------------------------|
| `audio/` | Extracted audio files (reusable across runs)   |
| `temp/`  | Intermediate video before audio is muxed in  |

Intermediate video files are deleted automatically after the final output is
written.

---

## Build Profile Reference

The CMake configuration assumes an OpenCV prebuilt at
`C:/opencv/opencv/build` by default. Override it with `-DOpenCV_DIR=...` if
yours lives elsewhere. On Windows, OpenCV DLLs are automatically copied next
to the executable during the post-build step.

---

## Performance Notes

- **Farneback** is the fastest option and runs on CPU only.
- **DIS** produces sharper motion but is slower; still CPU-only.
- Processing time grows roughly linearly with input frame count and the
  multiplier value.
- The optical flow is computed at full resolution; for large videos consider
  downsampling the source beforehand.

---

## License

This project is provided as-is for personal / educational use.
