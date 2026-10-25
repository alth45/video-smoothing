#!/usr/bin/env bash
# =====================================================================
#  detect-mobile.sh — Deteksi chip smartphone & rekomendasi parameter
#                     optimal untuk video_flow_smooth (+ auto-downscale)
#
#  Usage:
#    ./detect-mobile.sh                          # tampilkan rekomendasi
#    ./detect-mobile.sh --json                   # output JSON
#    ./detect-mobile.sh in.mp4 out.mp4 --exec    # langsung jalankan
#    ./detect-mobile.sh in.mp4 --exec            # auto-derive output
#    ./detect-mobile.sh in.mp4 --exec --no-downscale
#    ./detect-mobile.sh in.mp4 --exec --downscale-to 1280x720
#    ./detect-mobile.sh --extra "--no-audio"
# =====================================================================
set -uo pipefail

# ---------- defaults ----------
EXEC_MODE=0
JSON_MODE=0
INPUT_FILE=""
OUTPUT_FILE=""
EXTRA_ARGS=""
USE_DOWNSCALE=1          # default ON di exec mode
FORCE_DOWNSCALE_TO=""    # --downscale-to WxH

# ---------- parse args ----------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --exec)          EXEC_MODE=1; shift ;;
        --json)          JSON_MODE=1; shift ;;
        --input|-i)      INPUT_FILE="$2"; shift 2 ;;
        -o)              OUTPUT_FILE="$2"; shift 2 ;;
        --extra)         EXTRA_ARGS="$2"; shift 2 ;;
        --no-downscale)  USE_DOWNSCALE=0; shift ;;
        --auto-downscale) USE_DOWNSCALE=1; shift ;;
        --downscale-to)  FORCE_DOWNSCALE_TO="$2"; USE_DOWNSCALE=1; shift 2 ;;
        -h|--help)
            sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *)
            if [[ -z "$INPUT_FILE" ]]; then
                INPUT_FILE="$1"
            elif [[ -z "$OUTPUT_FILE" ]]; then
                OUTPUT_FILE="$1"
            else
                echo "Argumen berlebih: $1" >&2; exit 1
            fi
            shift ;;
    esac
done

# ---------- environment check ----------
IS_TERMUX=0
if [[ -n "${TERMUX_VERSION:-}" ]] || [[ "${PREFIX:-}" == *"com.termux"* ]]; then
    IS_TERMUX=1
fi

# =====================================================================
#  Detection helpers (CPU / RAM / chip)
# =====================================================================
getprop_safe() {
    if command -v getprop >/dev/null 2>&1; then
        getprop "$1" 2>/dev/null || echo ""
    else
        echo ""
    fi
}

detect_cores() {
    local n
    n=$(nproc 2>/dev/null) || n=""
    if [[ -z "$n" ]] && [[ -f /proc/cpuinfo ]]; then
        n=$(grep -c '^processor' /proc/cpuinfo 2>/dev/null || echo 0)
    fi
    echo "${n:-0}"
}

detect_max_freq_mhz() {
    local max_khz=0 f v
    for f in /sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_max_freq; do
        [[ -f "$f" ]] || continue
        v=$(cat "$f" 2>/dev/null || echo 0)
        [[ "$v" -gt "$max_khz" ]] && max_khz="$v"
    done
    if [[ "$max_khz" -eq 0 ]]; then
        for f in /sys/devices/system/cpu/cpufreq/policy*/cpuinfo_max_freq; do
            [[ -f "$f" ]] || continue
            v=$(cat "$f" 2>/dev/null || echo 0)
            [[ "$v" -gt "$max_khz" ]] && max_khz="$v"
        done
    fi
    if [[ "$max_khz" -eq 0 ]] && [[ -f /proc/cpuinfo ]]; then
        local mhz
        mhz=$(grep -m1 'cpu MHz' /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs | cut -d. -f1 || echo 0)
        max_khz=$(( mhz * 1000 ))
    fi
    echo $(( max_khz / 1000 ))
}

detect_ram_gb() {
    if [[ -f /proc/meminfo ]]; then
        local kb
        kb=$(grep -m1 '^MemTotal:' /proc/meminfo 2>/dev/null | awk '{print $2}' || echo 0)
        echo $(( kb / 1024 / 1024 ))
    else
        echo 0
    fi
}

detect_family() {
    local hw="$1" model="$2" board="$3"
    local hl ml bl
    hl=$(echo "$hw"    | tr '[:upper:]' '[:lower:]')
    ml=$(echo "$model" | tr '[:upper:]' '[:lower:]')
    bl=$(echo "$board" | tr '[:upper:]' '[:lower:]')

    [[ "$hl" == *"qcom"* || "$ml" == *"snapdragon"* || "$ml" == *"qualcomm"* ]] && { echo "qualcomm"; return; }
    [[ "$hl" == mt* || "$ml" == *"mediatek"* || "$ml" == *"dimensity"* || "$ml" == *"helio"* ]] && { echo "mediatek"; return; }
    [[ "$hl" == *"exynos"* || "$ml" == *"exynos"* || "$hl" == universal* || "$hl" == s5e* ]] && { echo "samsung"; return; }
    [[ "$hl" == *"kirin"* || "$ml" == *"kirin"* || "$hl" == hi* ]] && { echo "huawei"; return; }
    [[ "$hl" == *"tensor"* || "$ml" == *"tensor"* || "$hl" == gs101 || "$hl" == gs201 ]] && { echo "google"; return; }
    echo "unknown"
}

board_to_model() {
    local b; b=$(echo "$1" | tr '[:upper:]' '[:lower:]')
    case "$b" in
        kalama)    echo "Snapdragon 8 Gen 2" ;;
        crow)      echo "Snapdragon 7 Gen 3" ;;
        pineapple) echo "Snapdragon 8 Gen 3" ;;
        taro)      echo "Snapdragon 8 Gen 1" ;;
        lahaina)   echo "Snapdragon 888" ;;
        kona)      echo "Snapdragon 865" ;;
        msmnile)   echo "Snapdragon 855" ;;
        holi)      echo "Snapdragon 695" ;;
        parrot)    echo "Snapdragon 6 Gen 1" ;;
        blair)     echo "Snapdragon 4 Gen 1" ;;
        cape|yupik) echo "Snapdragon 778G" ;;
        kailua)    echo "Snapdragon 7 Gen 1" ;;
        bengal)    echo "Snapdragon 665/680" ;;
        khaje)     echo "Snapdragon 6 Gen 1 / 4 Gen 1" ;;
        "")        echo "" ;;
        *)         echo "Qualcomm ($b)" ;;
    esac
}

classify_tier() {
    local model="$1" cores="$2" freq="$3" ram="$4"
    local m; m=$(echo "$model" | tr '[:upper:]' '[:lower:]')

    case "$m" in
        *"8 gen 3"*|*"8 gen 2"*|*"8 gen 1"*|*"snapdragon 888"*|*"snapdragon 870"*) echo "high"; return ;;
        *"snapdragon 865"*|*"snapdragon 860"*|*"snapdragon 855"*)                  echo "midhigh"; return ;;
        *"snapdragon 7 gen"*|*"snapdragon 778"*|*"snapdragon 750"*)                echo "midhigh"; return ;;
        *"snapdragon 6 gen"*|*"snapdragon 69"*|*"snapdragon 68"*|*"snapdragon 67"*) echo "mid"; return ;;
        *"snapdragon 4"*)   echo "low"; return ;;
        *"dimensity 9"*)    echo "high"; return ;;
        *"dimensity 8"*)    echo "midhigh"; return ;;
        *"dimensity 7"*|*"dimensity 6"*) echo "mid"; return ;;
        *"helio g8"*|*"helio g9"*) echo "mid"; return ;;
        *"helio g"*)        echo "low"; return ;;
        *"exynos 2"*|*"exynos 1"*) echo "high"; return ;;
        *"exynos 9"*)       echo "midhigh"; return ;;
        *"exynos 8"*|*"exynos 7"*) echo "mid"; return ;;
        *"tensor"*)         echo "high"; return ;;
        *"kirin 9"*)        echo "high"; return ;;
        *"kirin 8"*)        echo "midhigh"; return ;;
        *"kirin 7"*)        echo "mid"; return ;;
    esac

    if   [[ "$cores" -ge 8 ]] && [[ "$freq" -ge 2800 ]] && [[ "$ram" -ge 8 ]]; then echo "high"
    elif [[ "$cores" -ge 8 ]] && [[ "$freq" -ge 2200 ]] && [[ "$ram" -ge 6 ]]; then echo "midhigh"
    elif [[ "$cores" -ge 6 ]] && [[ "$freq" -ge 1800 ]] && [[ "$ram" -ge 4 ]]; then echo "mid"
    else echo "low"
    fi
}

# =====================================================================
#  [NEW] Video resolution detection
# =====================================================================
probe_video_info() {
    local file="$1"
    [[ -f "$file" ]] || { echo ""; return; }
    command -v ffprobe >/dev/null 2>&1 || { echo ""; return; }
    ffprobe -v error -select_streams v:0 \
        -show_entries stream=width,height,duration,nb_frames \
        -of csv=p=0 "$file" 2>/dev/null | head -1
}

res_category() {
    local w="$1" h="$2"
    local pixels=$(( w * h ))
    if   [[ $pixels -ge 3500000 ]]; then echo "4k"
    elif [[ $pixels -ge 2200000 ]]; then echo "1440p"
    elif [[ $pixels -ge 1300000 ]]; then echo "1080p"
    elif [[ $pixels -ge 600000  ]]; then echo "720p"
    else echo "sd"
    fi
}

recommend_target_res() {
    local tier="$1" res_cat="$2"
    if [[ -n "$FORCE_DOWNSCALE_TO" ]]; then
        echo "$FORCE_DOWNSCALE_TO"; return
    fi
    case "$tier" in
        high|midhigh|mid)
            case "$res_cat" in
                4k|1440p) echo "1920x1080" ;;
                *)        echo "ok" ;;
            esac ;;
        *)
            case "$res_cat" in
                4k|1440p|1080p) echo "1280x720" ;;
                *)              echo "ok" ;;
            esac ;;
    esac
}

# Estimasi waktu/durasi kasar (frame rate pemrosesan)
estimate_speed() {
    local tier="$1" res_cat="$2"
    case "$tier:$res_cat" in
        high:720p|high:sd)         echo "~6–10 fps" ;;
        high:1080p)                echo "~3–5 fps" ;;
        midhigh:720p|midhigh:sd)   echo "~4–7 fps" ;;
        midhigh:1080p)             echo "~2–4 fps" ;;
        mid:720p)                  echo "~3–5 fps" ;;
        mid:1080p)                 echo "~1–2 fps" ;;
        mid:4k|mid:1440p)          echo "~0.3–0.6 fps (di-rekomendasikan downscale)" ;;
        low:720p|low:sd)           echo "~1.5–2.5 fps" ;;
        low:1080p)                 echo "~0.5–1 fps (di-rekomendasikan downscale)" ;;
        low:4k|low:1440p)          echo "< 0.5 fps (wajib downscale)" ;;
        *)                         echo "~2 fps (estimasi kasar)" ;;
    esac
}

# =====================================================================
#  Main detection
# =====================================================================
BOARD=$(getprop_safe ro.board.platform)
SOC_MODEL=$(getprop_safe ro.soc.model)
HARDWARE=$(getprop_safe ro.hardware)
MANUF=$(getprop_safe ro.product.manufacturer)
DEVICE=$(getprop_safe ro.product.model)

[[ -z "$SOC_MODEL" ]] && [[ -n "$BOARD" ]] && SOC_MODEL=$(board_to_model "$BOARD")
if [[ -z "$SOC_MODEL" ]] && [[ -f /proc/cpuinfo ]]; then
    hw_line=$(grep -m1 '^Hardware' /proc/cpuinfo 2>/dev/null | cut -d: -f2- | xargs || echo "")
    [[ -n "$hw_line" ]] && SOC_MODEL="$hw_line"
fi
[[ -z "$SOC_MODEL" ]] && SOC_MODEL="Unknown"

FAMILY=$(detect_family "$HARDWARE" "$SOC_MODEL" "$BOARD")
CORES=$(detect_cores)
FREQ=$(detect_max_freq_mhz)
RAM=$(detect_ram_gb)
[[ "$CORES" -lt 1 ]]  && CORES=0
[[ "$FREQ" -lt 100 ]] && FREQ=0
TIER=$(classify_tier "$SOC_MODEL" "$CORES" "$FREQ" "$RAM")

# =====================================================================
#  Recommendation matrix (base tier)
# =====================================================================
case "$TIER" in
    high)
        PRIMARY_FLAGS="-m 3 --dis"
        QUALITY_FLAGS="-m 3 -s 0.4"
        SPEED_FLAGS="-m 2 --dis --no-occlusion"
        VIDEO_NOTE="1080p native OK; 4K → downscale ke 1080p"
        ;;
    midhigh)
        PRIMARY_FLAGS="-m 2 --dis"
        QUALITY_FLAGS="-m 2 -s 0.4"
        SPEED_FLAGS="-m 2 --dis --no-occlusion"
        VIDEO_NOTE="1080p native OK; 4K → downscale ke 1080p"
        ;;
    mid)
        PRIMARY_FLAGS="-m 2 --dis"
        QUALITY_FLAGS="-m 2 -s 0.4"
        SPEED_FLAGS="-m 2 --dis --no-occlusion"
        VIDEO_NOTE="1080p native OK; 4K/1440p → downscale ke 1080p"
        ;;
    *)
        PRIMARY_FLAGS="-m 2 --dis --no-occlusion"
        QUALITY_FLAGS="-m 2 --no-occlusion"
        SPEED_FLAGS="-m 2 --dis --no-occlusion"
        VIDEO_NOTE="720p native; 1080p+ → wajib downscale"
        TIER="low"
        ;;
esac

# =====================================================================
#  [NEW] Probe input video
# =====================================================================
INPUT_W=0; INPUT_H=0; INPUT_DUR=""; INPUT_FRAMES=""
INPUT_CAT=""; INPUT_TARGET=""; INPUT_NEEDS_DOWNSCALE=0

if [[ -n "$INPUT_FILE" ]]; then
    probe_str=$(probe_video_info "$INPUT_FILE")
    if [[ -n "$probe_str" ]]; then
        IFS=',' read -r INPUT_W INPUT_H INPUT_DUR INPUT_FRAMES <<< "$probe_str"
        INPUT_W=${INPUT_W:-0}; INPUT_H=${INPUT_H:-0}
        if [[ "$INPUT_W" -gt 0 ]] && [[ "$INPUT_H" -gt 0 ]]; then
            INPUT_CAT=$(res_category "$INPUT_W" "$INPUT_H")
            INPUT_TARGET=$(recommend_target_res "$TIER" "$INPUT_CAT")
            [[ "$INPUT_TARGET" != "ok" ]] && INPUT_NEEDS_DOWNSCALE=1
        fi
    fi
fi

# =====================================================================
#  JSON mode
# =====================================================================
if [[ "$JSON_MODE" -eq 1 ]]; then
    TGT_JSON="\"$INPUT_TARGET\""
    cat <<EOF
{
  "environment": {
    "termux": $([[ "$IS_TERMUX" -eq 1 ]] && echo true || echo false),
    "device_manufacturer": "$MANUF",
    "device_model": "$DEVICE"
  },
  "chip": {
    "family": "$FAMILY",
    "model": "$SOC_MODEL",
    "board": "$BOARD",
    "hardware": "$HARDWARE"
  },
  "cpu": { "cores": $CORES, "max_freq_mhz": $FREQ },
  "ram_gb": $RAM,
  "tier": "$TIER",
  "input_video": {
    "path": "$INPUT_FILE",
    "width": $INPUT_W,
    "height": $INPUT_H,
    "category": "$INPUT_CAT",
    "target_resolution": $TGT_JSON,
    "needs_downscale": $([[ "$INPUT_NEEDS_DOWNSCALE" -eq 1 ]] && echo true || echo false)
  },
  "recommended": {
    "primary_flags": "$PRIMARY_FLAGS",
    "quality_flags": "$QUALITY_FLAGS",
    "speed_flags": "$SPEED_FLAGS",
    "video_note": "$VIDEO_NOTE"
  }
}
EOF
    exit 0
fi

# =====================================================================
#  Human-readable output
# =====================================================================
if [[ "$IS_TERMUX" -eq 0 ]]; then
    echo ""
    echo "  ⚠  Bukan Termux/Android — deteksi chip via /proc/cpuinfo saja."
    echo "     Rekomendasi mungkin kurang akurat."
    echo ""
fi

case "$TIER" in
    high)    TIER_LABEL="HIGH-END (flagship)" ;;
    midhigh) TIER_LABEL="MID-HIGH" ;;
    mid)     TIER_LABEL="MID-RANGE" ;;
    *)       TIER_LABEL="LOW-END" ;;
esac

echo "======================================================="
echo "  Deteksi Perangkat"
echo "======================================================="
printf "  %-14s: %s\n" "Chip family" "$FAMILY"
printf "  %-14s: %s\n" "Chip model"  "$SOC_MODEL"
[[ -n "$BOARD" ]] && printf "  %-14s: %s\n" "Board" "$BOARD"
printf "  %-14s: %s\n" "CPU cores"   "$CORES"
printf "  %-14s: %s MHz\n" "Max freq" "$FREQ"
printf "  %-14s: %s GB\n" "RAM"       "$RAM"
echo "  -----------------------------------------------------"
printf "  %-14s: %s\n" "Tier" "$TIER_LABEL"
echo "======================================================="
echo ""

# [NEW] Video info section
if [[ -n "$INPUT_FILE" ]] && [[ "$INPUT_W" -gt 0 ]]; then
    echo "======================================================="
    echo "  Deteksi Video Input"
    echo "======================================================="
    printf "  %-14s: %sx%s\n" "Resolution" "$INPUT_W" "$INPUT_H"
    printf "  %-14s: %s\n" "Kategori" "$INPUT_CAT"
    [[ -n "$INPUT_DUR" ]] && [[ "$INPUT_DUR" != "N/A" ]] && \
        printf "  %-14s: %s detik\n" "Durasi" "$INPUT_DUR"
    [[ -n "$INPUT_FRAMES" ]] && [[ "$INPUT_FRAMES" != "N/A" ]] && \
        printf "  %-14s: %s\n" "Total frame" "$INPUT_FRAMES"
    echo "-------------------------------------------------------"

    if [[ "$INPUT_NEEDS_DOWNSCALE" -eq 1 ]]; then
        echo "  ⚠  Video ${INPUT_CAT} terlalu besar untuk tier '$TIER'."
        echo ""
        echo "     Rekomendasi: downscale ke ${INPUT_TARGET} dulu"
        echo "     Estimasi setelah downscale: $(estimate_speed "$TIER" "$(res_category "${INPUT_TARGET%x*}" "${INPUT_TARGET#*x}")")"
        echo ""
        if [[ "$USE_DOWNSCALE" -eq 1 ]] && [[ "$EXEC_MODE" -eq 1 ]]; then
            echo "     → Auto-downscale AKAN diterapkan sebelum proses."
        else
            echo "     → Downscale manual (kalau tidak pakai --exec):"
            TGT_W="${INPUT_TARGET%x*}"
            echo "       ffmpeg -i \"$INPUT_FILE\" -vf \"scale=${TGT_W}:-2\" \\"
            echo "           -c:v libx264 -crf 20 -preset veryfast -c:a copy downscaled.mp4"
            echo "       video_flow_smooth -i downscaled.mp4 -o ${OUTPUT_FILE:-out.mp4} $PRIMARY_FLAGS"
            echo ""
            echo "     → Atau pakai --exec (auto-downscale aktif):"
            echo "       ./detect-mobile.sh \"$INPUT_FILE\" --exec"
        fi
    else
        echo "  ✓  Resolution ${INPUT_CAT} sesuai dengan kemampuan chip."
        echo "     Estimasi kecepatan: $(estimate_speed "$TIER" "$INPUT_CAT")"
    fi
    echo "======================================================="
    echo ""
fi

echo "  Rekomendasi parameter:"
echo ""
echo "  ► Primary (balance):"
echo "      $PRIMARY_FLAGS"
echo ""
echo "  ► Quality (lebih halus, lebih lambat):"
echo "      $QUALITY_FLAGS"
echo ""
echo "  ► Speed (paling cepat):"
echo "      $SPEED_FLAGS"
echo ""
echo "  Video resolution : $VIDEO_NOTE"
echo ""

if [[ -n "$INPUT_FILE" ]] && [[ "$EXEC_MODE" -eq 0 ]]; then
    echo "======================================================="
    echo "  Contoh command untuk file Anda"
    echo "======================================================="
    OUT_EXAMPLE="${OUTPUT_FILE:-${INPUT_FILE%.*}_smooth.${INPUT_FILE##*.}}"
    echo "  video_flow_smooth -i \"$INPUT_FILE\" -o \"$OUT_EXAMPLE\" $PRIMARY_FLAGS"
    echo ""
fi

# =====================================================================
#  [NEW] Exec mode with auto-downscale
# =====================================================================
if [[ "$EXEC_MODE" -eq 1 ]]; then
    if [[ -z "$INPUT_FILE" ]]; then
        echo "  [!!] --exec butuh input file." >&2
        echo "       Contoh: ./detect-mobile.sh in.mp4 --exec" >&2
        exit 1
    fi
    if [[ ! -f "$INPUT_FILE" ]]; then
        echo "  [!!] File input tidak ditemukan: $INPUT_FILE" >&2
        exit 1
    fi
    if ! command -v video_flow_smooth >/dev/null 2>&1; then
        echo "  [!!] 'video_flow_smooth' tidak ada di PATH." >&2
        echo "       Build dulu: ./build-termux.sh" >&2
        exit 1
    fi
    if [[ -z "$OUTPUT_FILE" ]]; then
        OUTPUT_FILE="${INPUT_FILE%.*}_smooth.${INPUT_FILE##*.}"
    fi

    ACTUAL_INPUT="$INPUT_FILE"
    NEED_CLEANUP=0
    DOWNSCALED_PATH=""

    # Auto-downscale kalau perlu
    if [[ "$USE_DOWNSCALE" -eq 1 ]] && [[ "$INPUT_NEEDS_DOWNSCALE" -eq 1 ]]; then
        if ! command -v ffmpeg >/dev/null 2>&1; then
            echo ""
            echo "  ⚠  ffmpeg tidak ditemukan — auto-downscale dilewati."
            echo "     Proses native mungkin sangat lambat / OOM."
        else
            mkdir -p temp
            DOWNSCALED_PATH="temp/_downscaled_$(basename "$INPUT_FILE")"
            TGT_W="${INPUT_TARGET%x*}"
            TGT_H="${INPUT_TARGET#*x}"

            echo "======================================================="
            echo "  Auto-downscale ke ${INPUT_TARGET}"
            echo "======================================================="
            echo "  ffmpeg -i \"$INPUT_FILE\" -vf \"scale=${TGT_W}:${TGT_H}\" \\"
            echo "         -c:v libx264 -crf 20 -preset veryfast -c:a copy"
            echo "         → $DOWNSCALED_PATH"
            echo ""

            if ffmpeg -y -v error -i "$INPUT_FILE" \
                    -vf "scale=${TGT_W}:${TGT_H}" \
                    -c:v libx264 -crf 20 -preset veryfast \
                    -c:a copy "$DOWNSCALED_PATH" ; then
                ACTUAL_INPUT="$DOWNSCALED_PATH"
                NEED_CLEANUP=1
                echo "  ✓ Downscale selesai ($(du -h "$DOWNSCALED_PATH" 2>/dev/null | cut -f1))"
                echo ""
            else
                echo "  ⚠  Downscale gagal — pakai input asli (mungkin lambat)."
                echo ""
                DOWNSCALED_PATH=""
            fi
        fi
    fi

    FULL_CMD="video_flow_smooth -i \"$ACTUAL_INPUT\" -o \"$OUTPUT_FILE\" $PRIMARY_FLAGS"
    [[ -n "$EXTRA_ARGS" ]] && FULL_CMD="$FULL_CMD $EXTRA_ARGS"

    echo "======================================================="
    echo "  Menjalankan (exec mode)"
    echo "======================================================="
    echo "  $FULL_CMD"
    echo ""

    eval "$FULL_CMD"
    EXIT_CODE=$?

    # Cleanup
    if [[ "$NEED_CLEANUP" -eq 1 ]] && [[ -n "$DOWNSCALED_PATH" ]]; then
        rm -f "$DOWNSCALED_PATH"
        echo ""
        echo "  (temp file downscale dihapus)"
    fi

    exit $EXIT_CODE
fi