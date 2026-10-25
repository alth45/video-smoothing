#!/usr/bin/env bash
# download-rife.sh — Panduan + helper untuk mendapatkan model RIFE ONNX
set -euo pipefail

MODEL_DIR="models"
MODEL_FILE="$MODEL_DIR/rife.onnx"

mkdir -p "$MODEL_DIR"

if [ -f "$MODEL_FILE" ]; then
    echo "Model sudah ada: $MODEL_FILE"
    ls -lh "$MODEL_FILE"
    exit 0
fi

cat <<'EOF'
======================================================================
  Model RIFE ONNX tidak ditemukan
======================================================================

File yang dibutuhkan: models/rife.onnx (~50–80 MB)

Ada 2 cara memperolehnya:

CARA 1 (direkomendasikan) — Export sendiri dari Practical-RIFE:

  1. Clone repo:
       git clone https://github.com/hzwer/Practical-RIFE
       cd Practical-RIFE

  2. Download model .pth dari README repo (rife-v4.6.pth)

  3. Export ke ONNX (butuh Python + PyTorch):
       python train_log/export_onnx.py \
         --model train_log/rife-v4.6.pth \
         --output /path/ke/video-smoothing/models/rife.onnx

CARA 2 — Download ONNX yang sudah di-export:

  Cari "RIFE v4.6 ONNX" di Hugging Face atau GitHub releases.
  Pastikan input model:
    - img0     : [1, 3, H, W], float32, RGB, [0,1]
    - img1     : [1, 3, H, W], float32, RGB, [0,1]
    - timestep : [1, 1, 1, 1], float32, [0,1]

  Simpan ke: models/rife.onnx

======================================================================
Setelah model tersedia, jalankan:
  ./build/Release/video_flow_smooth -i in.mp4 -o out.mp4 -m 2 --ai
======================================================================
EOF