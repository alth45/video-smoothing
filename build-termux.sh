#!/data/data/com.termux/files/usr/bin/bash
# Build script untuk Termux (Android)
set -e

# ---------- help ----------
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    echo "Usage: ./build-termux.sh [--clean]"
    echo "  --clean   : hapus build/ sebelum build ulang"
    exit 0
fi

if [ "$1" = "--clean" ]; then
    echo "Cleaning build/..."
    rm -rf build
fi

cd "$(dirname "$0")"

echo "=================================================="
echo "  Video Flow Smooth — Termux build"
echo "=================================================="
echo ""

# ---------- 1. dependencies ----------
echo "[1/3] Cek dependencies..."
MISSING=""
for pkg in cmake clang make opencv ffmpeg; do
    if ! dpkg -s "$pkg" >/dev/null 2>&1; then
        MISSING="$MISSING $pkg"
    fi
done

if [ -n "$MISSING" ]; then
    echo "      Install:$MISSING"
    pkg update -y >/dev/null 2>&1 || true
    pkg install -y $MISSING
else
    echo "      Semua sudah terpasang ✓"
fi

# ---------- 2. akses storage ----------
echo "[2/3] Cek akses storage..."
if [ ! -d "$HOME/storage" ]; then
    echo ""
    echo "  ⚠  Belum ada akses storage."
    echo "     Untuk memproses video dari /sdcard, jalankan dulu:"
    echo "       termux-setup-storage"
    echo ""
    echo "     (Proses dari folder \$HOME juga OK tanpa setup storage.)"
    echo ""
else
    echo "      Akses storage OK ✓"
fi

# ---------- 3. build ----------
echo "[3/3] Build..."
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# ---------- copy ke $PREFIX/bin (opsional) ----------
if [ -w "$PREFIX/bin" ]; then
    cp build/video_flow_smooth "$PREFIX/bin/video_flow_smooth" 2>/dev/null && \
        echo "      Binary diinstall ke: $PREFIX/bin/video_flow_smooth"
fi

echo ""
echo "=================================================="
echo "  Selesai!"
echo "=================================================="
echo "  Binary : $(pwd)/build/video_flow_smooth"
echo "  Global : $PREFIX/bin/video_flow_smooth"
echo ""
echo "  Contoh pakai:"
echo "    video_flow_smooth -i \$HOME/storage/shared/in.mp4 \\"
echo "                      -o \$HOME/storage/shared/out.mp4 \\"
echo "                      -m 2 --dis"
echo ""
echo "  Tips Termux:"
echo "    • Pakai --dis (lebih cepat, hemat baterai)"
echo "    • Mult 2 saja; >2 bisa sangat lambat"
echo "    • Video 4K → downscale dulu ke 720p/1080p"
echo "=================================================="