
# VideoFlowSmooth

Tool command-line C++17 ringan untuk **menghaluskan video dengan menyisipkan
frame di antara dua frame** menggunakan estimasi *optical flow*. Input video
30 fps bisa diubah menjadi 60 fps (atau lebih) dengan gerakan yang jauh lebih
mulus — cocok untuk *slow-motion*, konten sosial media, atau restorasi video
ber-frame-rate rendah.

Menggunakan **OpenCV** untuk pemrosesan frame dan **ffmpeg / ffprobe** untuk
ekstraksi, probing, dan re-muxing audio supaya tidak ada suara yang hilang —
dengan penanganan error yang tangguh dan fallback otomatis saat ada masalah.

---

> ⚠️ **RIFE AI (EXPERIMENTAL — BELUM TERUJI)**
>
> Fitur RIFE AI tersedia lewat flag `--ai-experimental`, tapi **belum
> diuji oleh developer** karena tidak ada hardware GPU. Kualitas,
> stabilitas, dan performa **tidak dijamin**.
>
> Untuk mode stabil, gunakan default (Farneback) atau `--dis`.
> Detail lengkap + flag `--ai-ack` untuk scripting di
> [docs-html/rife.html](docs-html/rife.html).
---

## Fitur

### Inti — Interpolasi Frame
- **Optical-flow-based frame interpolation** — benar-benar *membuat* frame
  baru, bukan duplikasi frame sederhana
- **Dua algoritma yang bisa dipilih:**
  - **Farneback** (default) — cepat, CPU-only, kualitas visual baik
  - **DIS** — kualitas lebih tinggi, dipilih via flag `--dis`
- **Multiplier frame-rate yang bisa dikonfigurasi** (`-m 2` → 2×, `-m 4` → 4×, dst)

### Deteksi Pergantian Scene
- **Scene-change detection** — mendeteksi cut/pergantian shot secara otomatis
  via korelasi histogram grayscale, lalu **duplikasi frame** di titik cut
  (bukan interpolasi) untuk menghindari *ghosting* dan artefak blending
- `[-s <thresh>]` — set threshold scene-change (0.0–1.0); makin kecil =
  makin sensitif mendeteksi cut
- `[--no-scene]` — matikan scene detection sepenuhnya

### Occlusion Masking — Anti Ghosting di Tepi Objek
- **Forward-backward consistency check** — untuk tiap pixel, periksa apakah
  `flow_ab(x) + flow_ba(x + flow_ab(x)) ≈ 0`. Kalau tidak konsisten → pixel
  dianggap *occluded* (tertutup objek lain), lalu **tidak di-blend 50/50**,
  tapi di-*favor*-kan frame yang lebih dekat
- Hasil: tepi objek yang bergerak cepat **bersih dari ghosting** tanpa
  perlu post-processing tambahan
- `[--no-occlusion]` — matikan occlusion masking (perilaku lama, ~20% lebih cepat)
- `[--occ-thresh <px>]` — threshold FB-consistency dalam piksel (default `2.0`);
  turunkan kalau masih ghost, naikkan kalau area freeze/kaku
- Statistik rata-rata % pixel occluded ditampilkan di summary akhir

### Penanganan Audio via FFmpeg (opsional)
- **Probe** input dulu (ffprobe) untuk deteksi apakah ada stream audio —
  video tanpa audio ditangani graceful, bukan dianggap error
- **Ekstrak** audio ke AAC @ 192 kbps, disimpan di folder `audio/`
- **Re-mux** video yang sudah dihaluskan + audio sebagai output final
  (H.264 / aac, kompatibel luas)
- `--no-audio` — skip audio sepenuhnya (output video-only)
- Kalau ffmpeg tidak terpasang, tool tetap merender video yang dihaluskan
  (audio dihilangkan — lihat [Requirements](#requirements))

### Error Handling FFmpeg yang Tangguh
- Menangkap **stdout + stderr** FFmpeg, memfilter baris banner, lalu
  memeriksa terhadap daftar pattern error kritis
  (*moov atom not found*, *No such file*, *could not find codec*,
  *Invalid data found*, *Input buffer exhausted*, dst)
- Saat gagal, tampilkan **1 baris error asli** dari FFmpeg + **pesan ramah**
  dengan penyebab yang mungkin dan saran perbaikan (unduh ulang, update
  ffmpeg, atau jalankan ulang dengan `--no-audio`)
- **Menghentikan program** dengan exit code 1 (bukan melanjutkan loop error
  panjang atau menghasilkan output setengah jadi)

### Progress & UX
- **Banner ASCII** keren saat program dibuka (dapat dikustomisasi di
  `src/banner.cpp`)
- **Progress reporting** real-time saat pemrosesan:
  - Persentase selesai (bila total frame count tersedia)
  - fps pemrosesan real-time
  - Estimasi waktu sisa (**ETA**) dengan format `1m 30s`, `2h 15m`, dst
  - Anotasi scene-cut di console
- **Ringkasan akhir**: input frame, output frame, fps, jumlah scene cut,
  rata-rata occlusion %, waktu proses, status audio, path file output
- **Cleanup otomatis** file intermediate dan folder `temp/`

### Kualitas & Robustness
- **Codec fallback VideoWriter** — coba H.264/mp4v dulu, fallback ke
  MJPEG/AVI kalau codec primer tidak tersedia
- **Cross-platform CMake build** — Windows (MSVC), Linux, macOS,
  **Android (Termux)**

### Arsitektur & Dokumentasi
- **Struktur multi-file** dengan *single responsibility per modul*
  (lihat [Struktur Proyek](#struktur-proyek))
- **Dokumentasi HTML statis** di folder `docs-html/` — setiap method/fungsi
  punya halaman sendiri dan **saling ter-link** sesuai pemakaian
- **Dukungan Termux/Android** — bisa di-build dan dijalankan langsung di HP
  tanpa root

---

## Requirements

| Dependency       | Versi / Catatan                                            |
|------------------|------------------------------------------------------------|
| C++ compiler     | Dukungan C++17 (MSVC 2019+, GCC 9+, Clang 10+)             |
| CMake            | ≥ 3.15                                                     |
| OpenCV           | 4.x (prebuilt atau build dari source)                      |
| ffmpeg + ffprobe | *(opsional tapi direkomendasikan)* — untuk penanganan audio |

> **ffmpeg opsional.** Tanpa ffmpeg, tool tetap menghasilkan video yang
> dihaluskan (tanpa audio). Unduh dari
> [gyan.dev](https://www.gyan.dev/ffmpeg/builds/) (pilih build **full**),
> ekstrak, dan tambahkan folder `bin` ke `PATH`.
> `ffprobe` sudah termasuk dalam paket yang sama.

---

## Building

### Windows (MSVC + prebuilt OpenCV)

1. Install OpenCV 4.x (paket prebuilt Windows).
2. Set direktori build OpenCV (default diasumsikan: `C:/opencv/build`).

```powershell
cd video-smoothing
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DOpenCV_DIR=C:/opencv/build
cmake --build build --config Release --parallel
```

Kalau OpenCV terpasang di lokasi lain, arahkan CMake ke sana:

```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DOpenCV_DIR="C:/path/ke/opencv/build"
cmake --build build --config Release --parallel
```

Setelah build, executable dan DLL OpenCV otomatis disalin ke
`build/Release/video_flow_smooth.exe`.

### Linux / macOS

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
# pastikan ffmpeg + ffprobe terpasang, contoh:
#   sudo apt install ffmpeg
```

### Android (Termux)

Lihat panduan lengkap di [`docs-html/termux.html`](docs-html/termux.html).
Ringkasan:

```bash
# Di Termux (F-Droid, bukan Play Store)
pkg install -y cmake clang make opencv ffmpeg
termux-setup-storage          # opsional, untuk akses /sdcard

cd ~/video-smoothing
chmod +x build-termux.sh
./build-termux.sh

# Jalankan
video_flow_smooth -i ~/storage/downloads/in.mp4 \
                  -o ~/storage/downloads/out.mp4 -m 2 --dis
```

---

## CLI Options

```
video_flow_smooth -i <input> -o <output> -m <multiplier> [options]
```

| Option              | Deskripsi                                                          | Default               |
|---------------------|--------------------------------------------------------------------|-----------------------|
| `-i <file>`         | Path video input                                                   | `input.mp4`           |
| `-o <file>`         | Path video output (harus berbeda dari input)                       | `output.mp4`          |
| `-m <N>`            | Multiplier frame-rate (minimum 2)                                  | `2`                   |
| `-s <thresh>`       | Threshold scene-change, 0.0–1.0 (makin kecil = makin sensitif)     | `0.4`                 |
| `--occ-thresh <px>` | Threshold FB-consistency untuk occlusion masking (piksel)          | `2.0`                 |
| `--dis`             | Pakai algoritma DIS, bukan Farneback                               | Farneback             |
| `--no-scene`        | Matikan scene-change detection (selalu interpolasi)                | *(deteksi aktif)*     |
| `--no-occlusion`    | Matikan occlusion masking (perilaku lama, lebih cepat)             | *(occlusion aktif)*   |
| `--no-audio`        | Skip ekstraksi audio / muxing (output video-only)                  | *(audio aktif jika ada)* |
| `-h`/`--help`       | Tampilkan usage                                                    | —                     |
| `--ai-experimental` | ⚠ RIFE AI (EXPERIMENTAL — BELUM TERUJI)                            | *(nonaktif)*          |
| `--ai-ack`          | Skip konfirmasi prompt RIFE (scripting)                            | *(prompt aktif)*      |
| `--ai`              | ❌ Deprecated — error, pakai `--ai-experimental`                   | —                     |
| `--rife-model <p>`  | Path model ONNX untuk RIFE                                          | `models/rife.onnx`    |

### Contoh

```powershell
# Gandakan frame-rate (30 → 60 fps), audio tetap disertakan
.\build\Release\video_flow_smooth.exe -i input.mp4 -o output.mp4 -m 2

# 4× frame-rate dengan algoritma DIS (kualitas lebih baik)
.\build\Release\video_flow_smooth.exe -i input.mp4 -o smooth.mp4 -m 4 --dis

# 4× dengan deteksi cut agresif (threshold 0.2)
.\build\Release\video_flow_smooth.exe -i input.mp4 -o smooth.mp4 -m 4 -s 0.2

# Occlusion agresif — untuk video dengan gerakan tepi objek cepat
.\build\Release\video_flow_smooth.exe -i input.mp4 -o smooth.mp4 -m 2 --occ-thresh 1.5

# Video-only — skip audio sepenuhnya
.\build\Release\video_flow_smooth.exe -i input.mp4 -o smooth.mp4 -m 3 --no-audio

# Mode cepat (untuk test atau Termux): DIS + tanpa occlusion
.\build\Release\video_flow_smooth.exe -i input.mp4 -o smooth.mp4 -m 2 --dis --no-occlusion
```

---
# Auto-tune berdasarkan chip

Tidak yakin parameter apa yang cocok untuk HP kamu? Jalankan detector:

```bash
./detect-mobile.sh                    # tampilkan rekomendasi
./detect-mobile.sh in.mp4 --exec      # deteksi + langsung proses
```

**Deteksi 4K otomatis:** kalau input video 4K/1440p dan chip tidak sanggup,
script otomatis downscale via ffmpeg sebelum proses. Override dengan
`--no-downscale` (paksa native) atau `--downscale-to 1280x720` (target custom).

Script ini mendeteksi:

- **Chip family & model** (`getprop ro.soc.model` / `ro.board.platform`)
- **Jumlah core & frekuensi maksimum** (`/proc/cpuinfo`, sysfs)
- **RAM total** (`/proc/meminfo`)

Lalu mengklasifikasi ke tier (**high / midhigh / mid / low**) dan merekomendasikan flag yang pas — bukan sekadar *"pakai `-m 2` untuk semua"*.

---

<h2 id="auto-tune">Auto-tune dengan <code>detect-mobile.sh</code></h2>
<p>Script ini mendeteksi chip dan merekomendasikan flag yang pas:</p>
<pre><code>./detect-mobile.sh                    # lihat rekomendasi
./detect-mobile.sh in.mp4 --exec      # deteksi + jalankan</code></pre>

<h3>Contoh output</h3>
<table>
  <thead>
    <tr><th>Tier</th><th>Flag rekomendasi</th><th>Video max</th></tr>
  </thead>
  <tbody>
    <tr><td>high (8 Gen 2/3)</td><td><code>-m 3 --dis</code></td><td>1080p native</td></tr>
    <tr><td>midhigh (7 Gen 2/3)</td><td><code>-m 2 --dis</code></td><td>1080p</td></tr>
    <tr><td>mid (6xx)</td><td><code>-m 2 --dis</code></td><td>1080p (4K → downscale)</td></tr>
    <tr><td>low (4xx, Helio)</td><td><code>-m 2 --dis --no-occlusion</code></td><td>720p</td></tr>
  </tbody>
</table>

---
## Cara Kerja

Tool memproses pasangan frame berurutan dan menyintesis frame baru di antaranya:

```
pasangan (frame[n], frame[n+1])
        │
        ├── hitung optical flow forward  frame[n]   → frame[n+1]
        ├── hitung optical flow backward frame[n+1] → frame[n]
        ├── (opsional) Gaussian blur flow field (sigma = 1.5)
        ├── hitung korelasi scene-change (perbandingan histogram)
        │
        ├── kalau SCENE CUT terdeteksi → duplikasi frame[n]
        │                                 (hindari ghosting di transisi)
        │
        └── kalau TIDAK ada cut → untuk tiap step t ∈ (0..1):
                ├── bangun occlusion mask via forward-backward consistency
                ├── warp frame[n]   forward  by t
                ├── warp frame[n+1] backward by 1-t
                └── blend adaptif:
                      weight frame yang "reliable" di area occluded
                      (bukan 50/50 statis — hindari ghost di tepi objek)
```

Frame hasil interpolasi ditulis ke **video temporary** (`temp/`). Sementara
itu, audio asli **diekstrak lebih awal** (`audio/`) dan **di-mux kembali**
sebagai langkah terakhir (di-re-encode ke H.264 / aac untuk kompatibilitas
luas).

### Ringkasan matematis

Untuk pixel `x` dan waktu `t`:

```
flow_ab(x)    = posisi_x_di_B - posisi_x_di_A
flow_ba(x)    = posisi_x_di_A - posisi_x_di_B

warp_A_t(x)   = A( x - t · flow_ab(x) )
warp_B_t(x)   = B( x - (1-t) · flow_ba(x) )

I_t(x)        = w_A(x)·warp_A_t(x) + w_B(x)·warp_B_t(x)

w_A(x)        = reliability_A(x) · (1 - t)
w_B(x)        = reliability_B(x) · t

reliability_A(x) = 255 jika |flow_ab(x) + flow_ba(x + flow_ab(x))| < thresh
                 = 0   kalau tidak (area occluded)
```

Tanpa occlusion masking (`--no-occlusion`), `reliability_A = reliability_B = 1`,
sehingga blending menjadi `(1-t)·warp_A + t·warp_B` seperti versi lama.

---

## Struktur Proyek

```
video-smoothing/
├── CMakeLists.txt
├── build-termux.sh                 ← build script otomatis untuk Termux
├── README.md
├── src/
│   ├── main.cpp                    ← entry point + parsing CLI
│   ├── banner.hpp / .cpp           ← ASCII banner startup
│   ├── pipeline.hpp / .cpp         ← orkestrasi 4 tahap (extract → process → mux → summary)
│   ├── flow_interpolator.hpp/.cpp  ← optical flow + occlusion mask + blending
│   ├── scene_detector.hpp/.cpp     ← deteksi scene cut (histogram correlation)
│   ├── ffmpeg_helper.hpp/.cpp      ← wrapper ffmpeg/ffprobe + error capture
│   └── util.hpp / .cpp             ← formatDuration + utilitas kecil
└── docs-html/
    ├── index.html                  ← overview + arsitektur
    ├── main.html
    ├── pipeline.html
    ├── flow_interpolator.html
    ├── scene_detector.html
    ├── ffmpeg_helper.html
    ├── util.html
    ├── banner.html
    ├── termux.html
    └── style.css
```

Setiap modul punya **satu tanggung jawab**. Kalau mau ganti FFmpeg dengan
lib lain (mis. `avformat`), cukup ganti `ffmpeg_helper.*`. Kalau mau ganti
optical flow ke RIFE, cukup ganti `flow_interpolator.*`. Tidak ada modul
lain yang perlu disentuh.

---

## Dokumentasi Kode

Dokumentasi interaktif tersedia di folder **`docs-html/`**. Setiap halaman
berisi:

- Signature lengkap (parameter + return)
- Alur internal method
- Rumus matematis untuk bagian rumit (occlusion mask, blending)
- Tabel tuning parameter
- **Hyperlink antar halaman** sesuai pemakaian method

Buka langsung di browser:

```
docs-html/index.html
```

Tidak perlu server, tidak perlu npm — semuanya static file.

---

## Direktori Output

| Folder   | Fungsi                                              |
|----------|-----------------------------------------------------|
| `audio/` | File audio hasil ekstraksi (bisa dipakai ulang)    |
| `temp/`  | Video intermediate + log capture ffmpeg sementara  |

File video intermediate dihapus otomatis setelah output final ditulis.
File audio di `audio/` **tidak** dihapus otomatis (bisa dihapus manual
kalau tidak diperlukan).

---

## Catatan Performa

- **Farneback** adalah opsi tercepat, CPU-only.
- **DIS** menghasilkan gerakan lebih tajam, tapi lebih lambat; tetap CPU-only.
- **Occlusion masking** menambah ~20% waktu proses (loop per-pixel FB check),
  tapi menghilangkan ghosting. Matikan dengan `--no-occlusion` kalau kecepatan
  lebih penting dari kualitas.
- **Scene-change detection** menambah biaya kecil per frame (histogram
  comparison) tapi mencegah artefak di cut keras — matikan dengan
  `--no-scene` kalau semua frame harus diinterpolasi tanpa kecuali.
- Waktu proses tumbuh hampir linear dengan jumlah frame input dan nilai
  multiplier.
- Optical flow dihitung pada **resolusi penuh**. Untuk video besar
  (4K), pertimbangkan downscale dulu dengan ffmpeg:
  `ffmpeg -i in.mp4 -vf scale=1920:-2 small.mp4`

### Ekspektasi performa

| Platform | 1080p + `--dis` | 1080p Farneback |
|---|---|---|
| Desktop (Ryzen 5 / i5 modern) | ~15–25 fps | ~5–10 fps |
| Laptop (i5 gen 8) | ~8–15 fps | ~2–4 fps |
| Android (Snapdragon 8 Gen 2) | ~3–5 fps | ~1–2 fps |
| Android (Snapdragon 7xx) | ~1.5–2 fps | ~0.5–0.8 fps |

---

## Troubleshooting

| Masalah | Penyebab / Solusi |
|---------|-------------------|
| *moov atom not found* | File korup/tidak lengkap — unduh ulang atau buka di VLC untuk verifikasi |
| *Input buffer exhausted before END element found* (AAC) | Stream audio korup — jalankan dengan `--no-audio` atau perbaiki file dulu |
| *Tidak ada audio di output* | Jalankan dengan default (audio aktif); kalau gagal, coba `--no-audio` |
| *Audio extraction gagal* | Update ffmpeg ke build **full** terbaru dari [gyan.dev](https://www.gyan.dev/ffmpeg/builds/) |
| *VideoWriter tidak bisa dibuka* (mp4v) | Codec tidak tersedia — tool otomatis fallback ke MJPEG/AVI |
| *ffmpeg: command not found* | ffmpeg tidak ada di `PATH` — tool akan skip audio dan melanjutkan |
| **Ghosting di tepi objek** | Turunkan `--occ-thresh` (mis. 1.5) supaya occlusion mask lebih agresif |
| **Area freeze / kaku setelah occlusion ON** | Naikkan `--occ-thresh` (mis. 3.0) — kurangi false positive |
| **Durasi output memendek** | Pastikan pakai versi terbaru (bug sudah diperbaiki); verifikasi dengan `ffprobe` |
| **Banget lambat di Termux** | Pakai `--dis --no-occlusion -m 2`; downscale video ke 720p dulu |

---

## Lisensi

Project ini dilisensikan di bawah **MIT License** — bebas dipakai, dimodifikasi,
dan didistribusikan ulang (termasuk untuk keperluan komersial), selama
copyright notice asli tetap disertakan.

Lihat file [LICENSE](LICENSE) untuk teks lengkap.

Copyright (c) 2025 **Nurhadi**

---

## Kontribusi

Kalau mau berkontribusi:

1. Fork repo
2. Buat branch fitur (`git checkout -b feature/namafitur`)
3. Commit perubahan (`git commit -am 'Tambah fitur X'`)
4. Push (`git push origin feature/namafitur`)
5. Buka Pull Request

Pastikan:
- Kode C++ tetap C++17-clean
- Kalau menambah method baru, **update juga `docs-html/`** (tambahkan
  halaman/anchor + link di modul lain yang memanggilnya)
- Jalankan build di Windows dan (kalau bisa) Termux sebelum PR


---
