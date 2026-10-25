// Copyright (c) 2026 Nurhadi
// SPDX-License-Identifier: MIT

#include "pipeline.hpp"
#include "rife_interpolator.hpp"
#include "ffmpeg_helper.hpp"
#include "flow_interpolator.hpp"
#include "scene_detector.hpp"
#include "util.hpp"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
  #include <io.h>
  #include <cstdio>
#else
  #include <unistd.h>
#endif

using namespace cv;
using namespace std;
namespace fs = std::filesystem;


// ==================================================================
//  Konfirmasi penggunaan RIFE (EXPERIMENTAL — belum teruji)
// ==================================================================
static bool confirmExperimentalAi(bool ack) {
    cout << "\n";
    cout << "  ╔══════════════════════════════════════════════════════════╗\n";
    cout << "  ║  ⚠  RIFE AI — KODE BELUM TERUJI (EXPERIMENTAL)           ║\n";
    cout << "  ╠══════════════════════════════════════════════════════════╣\n";
    cout << "  ║                                                          ║\n";
    cout << "  ║  Fitur ini BELUM diuji oleh developer (tidak ada GPU).  ║\n";
    cout << "  ║  Kualitas, stabilitas, dan performa TIDAK dijamin.      ║\n";
    cout << "  ║                                                          ║\n";
    cout << "  ║  Komponen yang BELUM TERUJI:                            ║\n";
    cout << "  ║    • Forward pass model ONNX (variasi RIFE v4.x)        ║\n";
    cout << "  ║    • Deteksi otomatis input layer model                 ║\n";
    cout << "  ║    • Integrasi dengan pipe ffmpeg + audio mux           ║\n";
    cout << "  ║    • Mode CUDA / GPU backend (belum ada hardware uji)   ║\n";
    cout << "  ║    • Mode CPU-only (akurasi belum diverifikasi)         ║\n";
    cout << "  ║    • Kompatibilitas OpenCV DNN (semua versi 4.x)        ║\n";
    cout << "  ║                                                          ║\n";
    cout << "  ║  Kalau bermasalah, laporkan dengan output lengkap.      ║\n";
    cout << "  ║  Untuk mode stabil: jalankan tanpa --ai-experimental.   ║\n";
    cout << "  ║                                                          ║\n";
    cout << "  ╚══════════════════════════════════════════════════════════╝\n";
    cout << "\n";

    if (ack) {
        cout << "  (--ai-ack: konfirmasi dilewati)\n\n";
        return true;
    }

#ifdef _WIN32
    bool interactive = _isatty(_fileno(stdin));
#else
    bool interactive = isatty(STDIN_FILENO);
#endif

    if (!interactive) {
        cerr << "  [!!] Tidak bisa meminta konfirmasi (stdin bukan terminal).\n"
             << "       Untuk scripting / Termux, tambahkan --ai-ack:\n"
             << "         video_flow_smooth -i in.mp4 -o out.mp4 --ai-experimental --ai-ack\n\n";
        return false;
    }

    cout << "  Ketik 'yes' untuk melanjutkan, atau apa pun untuk batal: ";
    string ans;
    getline(cin, ans);

    if (ans != "yes") {
        cout << "\n  Dibatalkan oleh user.\n\n";
        return false;
    }
    cout << "\n";
    return true;
}

VideoSmoothingPipeline::VideoSmoothingPipeline(const PipelineOptions& opts)
    : opts_(opts) {}

int VideoSmoothingPipeline::run() {
    const auto& in  = opts_.input;
    const auto& out = opts_.output;
    const int   mult = opts_.mult;
    if(opts_.useAI){
        if (!confirmExperimentalAi(opts_.aiAcknowledged)) {
            return 1;
        }
    }

    // ---- cek ffmpeg ----
    bool haveFFmpeg = ffmpeg::isAvailable();
    if (!haveFFmpeg) {
        cerr << "WARNING: ffmpeg tidak ditemukan di PATH.\n"
             << "         Audio TIDAK akan disertakan.\n"
             << "         Mode pipe tidak tersedia — pakai VideoWriter.\n\n";
    }

    // Mode pipe: hanya kalau ffmpeg ada + ada encoder
    bool pipeViable = false;
    if (haveFFmpeg && opts_.usePipe) {
        pipeViable = ffmpeg::hasEncoder("libx264")
                  || ffmpeg::hasEncoder("mpeg4");
    }

    fs::create_directories("audio");
    fs::create_directories("temp");

    string stem      = fs::path(in).stem().string();
    string audioFile = "audio/" + stem + "_audio.m4a";
    string tempVideo = "temp/"  + stem + "_intermediate.mp4";

    // ================= [1/4] Ekstrak audio =================
    bool haveAudio = false;
    if (opts_.skipAudio) {
        cout << "[1/4] Audio: dilewati (--no-audio)\n";
    } else if (haveFFmpeg) {
        cout << "[1/4] Ekstrak audio ke folder audio/...\n";
        ffmpeg::AudioStatus as = ffmpeg::extractAudio(in, audioFile);
        if (as == ffmpeg::AudioStatus::Failed) {
            try { fs::remove(audioFile); } catch (...) {}
            try { fs::remove(tempVideo); } catch (...) {}
            return 1;
        }
        haveAudio = (as == ffmpeg::AudioStatus::OK);
        if (haveAudio) cout << "     -> Audio disimpan: " << audioFile << "\n";
        else           cout << "     -> Tidak ada audio di input, skip.\n";
    } else {
        cout << "[1/4] Skip ekstrak audio (ffmpeg tidak tersedia)\n";
    }

    // ================= [2/4] Proses video =================
    cout << "[2/4] Proses frame interpolation...\n";
    VideoCapture cap(in);
    if (!cap.isOpened()) { cerr << "Tidak bisa buka: " << in << "\n"; return 1; }

    int    W   = (int)cap.get(CAP_PROP_FRAME_WIDTH);
    int    H   = (int)cap.get(CAP_PROP_FRAME_HEIGHT);
    double fps = cap.get(CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;

    if (W % 2 != 0 || H % 2 != 0) {
        cerr << "\n  [!!] Dimensi video ganjil (" << W << "x" << H << ").\n"
             << "  H.264 membutuhkan dimensi genap.\n"
             << "  Downscale dulu dengan ffmpeg:\n"
             << "    ffmpeg -i \"" << in
             << "\" -vf \"pad=ceil(iw/2)*2:ceil(ih/2)*2\" in_even.mp4\n\n";
        return 1;
    }

    cout << "     Input: " << W << "x" << H << " @ " << fps
         << " fps -> Output: " << fps * mult << " fps\n";

    if (opts_.sceneDetect)
        cout << "     Scene detection: ON (threshold=" << opts_.sceneThresh << ")\n";
    else
        cout << "     Scene detection: OFF\n";

    if (opts_.useOcclusion)
        cout << "     Occlusion    : ON (thresh=" << opts_.occlusionThresh << " px)\n";
    else
        cout << "     Occlusion    : OFF\n";

    // ---- Siapkan writer: pipe ffmpeg atau fallback VideoWriter ----
    ffmpeg::FrameWriter pipeWriter;
    VideoWriter         fileWriter;
    bool usingPipe = false;

    if (pipeViable) {
        cout << "     Encoder      : ffmpeg pipe (single-pass H.264)\n";
        if (pipeWriter.open(out, W, H, fps * mult,
                            (haveAudio ? audioFile : string()),
                            18, "medium")) {
            usingPipe = true;
        } else {
            cerr << "     [!!] Gagal membuka pipe ffmpeg: "
                 << pipeWriter.lastError() << "\n"
                 << "     Fallback ke VideoWriter + mux terpisah.\n";
        }
    }

    if (!usingPipe) {
        cout << "     Encoder      : VideoWriter (temp file, fallback)\n";
        int fourcc = VideoWriter::fourcc('m', 'p', '4', 'v');
        fileWriter.open(tempVideo, fourcc, fps * mult, Size(W, H));
        if (!fileWriter.isOpened()) {
            tempVideo = "temp/" + stem + "_intermediate.avi";
            fourcc = VideoWriter::fourcc('M', 'J', 'P', 'G');
            fileWriter.open(tempVideo, fourcc, fps * mult, Size(W, H));
        }
        if (!fileWriter.isOpened()) { cerr << "Gagal buka VideoWriter\n"; return 1; }
    }

    unique_ptr<IInterpolator> interp;

    if (opts_.useAI) {
        cout << "     Interpolator : RIFE (AI)  [⚠ EXPERIMENTAL — BELUM TERUJI]\n";
        cout << "     Model        : " << opts_.rifeModelPath << "\n";

        auto rife = make_unique<RifeInterpolator>(opts_.rifeModelPath, mult);
        if (!rife->isReady()) {
            cerr << "\n  [!!] Gagal memuat model RIFE.\n";
            cerr << "  [rife] " << rife->error() << "\n\n";
            cerr << "  Solusi:\n";
            cerr << "    1. Download / export model RIFE:\n";
            cerr << "         ./download-rife.sh\n";
            cerr << "    2. Atau tentukan path manual dengan --rife-model <path>\n";
            cerr << "    3. Atau jalankan tanpa --ai untuk memakai Farneback/DIS\n\n";
            return 1;
        }
        interp = move(rife);
    } else {
        interp = make_unique<FlowInterpolator>(
            mult, 1.5f,
            opts_.useFarneback,
            opts_.useOcclusion,
            opts_.occlusionThresh);
    }

    // Lambda dispatcher: tulis frame ke pipe ATAU file
    auto emitFrame = [&](const Mat& f) -> bool {
        if (usingPipe) return pipeWriter.writeFrame(f);
        fileWriter.write(f);
        return true;
    };

    Mat prev, next;
    if (!cap.read(prev)) { cerr << "Video kosong\n"; return 1; }

    long frames = 1, written = 0, sceneCuts = 0;
    double occlusionRatioSum = 0.0;
    long   occlusionPairCount = 0;

    double totalFrames = cap.get(CAP_PROP_FRAME_COUNT);
    bool   haveTotal   = totalFrames > 1.0;
    auto   procStart   = chrono::steady_clock::now();
    bool   reprintBar  = false;

    while (cap.read(next)) {
        double corr = opts_.sceneDetect ? sceneDiff(prev, next) : 1.0;
        bool   cut  = opts_.sceneDetect && (corr < opts_.sceneThresh);

        if (cut) {
            sceneCuts++;
            printf("\r%*s\r", 78, "");
            printf("     [scene cut @ frame %ld, corr=%.3f]\n", frames, corr);
            reprintBar = true;
            for (int i = 0; i < mult; ++i) {
                if (!emitFrame(prev)) {
                    cerr << "\n  [!!] Gagal menulis frame: "
                         << pipeWriter.lastError() << "\n";
                    if (usingPipe) pipeWriter.close();
                    try { fs::remove(out); } catch (...) {}
                    return 1;
                }
                written++;
            }
        } else {
            auto outFrames = interp->processPair(prev, next);
            if (opts_.useOcclusion) {
                occlusionRatioSum += interp->lastOcclusionRatio();
                occlusionPairCount++;
            }
            for (const auto& f : outFrames) {
                if (!emitFrame(f)) {
                    cerr << "\n  [!!] Gagal menulis frame: "
                         << pipeWriter.lastError() << "\n";
                    if (usingPipe) pipeWriter.close();
                    try { fs::remove(out); } catch (...) {}
                    return 1;
                }
                written++;
            }
        }

        prev = next.clone();
        frames++;

        if (frames % 10 == 0 || reprintBar) {
            reprintBar = false;
            double elapsed = chrono::duration<double>(
                chrono::steady_clock::now() - procStart).count();
            double procFps = (elapsed > 0.01) ? frames / elapsed : 0.0;

            if (haveTotal && frames > 10) {
                double pct = std::min(100.0, 100.0 * frames / totalFrames);
                double eta = (frames < totalFrames)
                           ? (elapsed / frames) * (totalFrames - frames) : 0.0;
                printf("\r     [%5.1f%%] %ld/%ld | %5.1f fps proses | ETA %s   ",
                       pct, frames, (long)totalFrames, procFps,
                       formatDuration(eta).c_str());
            } else {
                printf("\r     frame %ld | %5.1f fps proses   ", frames, procFps);
            }
            fflush(stdout);
        }
    }
    // frame terakhir
    if (!emitFrame(prev)) {
        cerr << "\n  [!!] Gagal menulis frame terakhir.\n";
        if (usingPipe) pipeWriter.close();
        try { fs::remove(out); } catch (...) {}
        return 1;
    }
    written++;

    double totalElapsed = chrono::duration<double>(
        chrono::steady_clock::now() - procStart).count();
    printf("\r%*s\r", 78, "");
    printf("     100%% — %ld frame diproses dalam %s (%.1f fps rata-rata)\n",
           frames, formatDuration(totalElapsed).c_str(),
           frames / std::max(totalElapsed, 0.001));

    // ---- Tutup writer ----
    bool encodeOK = true;
    if (usingPipe) {
        encodeOK = pipeWriter.close();
    } else {
        fileWriter.release();
    }

    if (!encodeOK) {
        cerr << "\n  [!!] Encoding video gagal.\n";
        try { fs::remove(out); } catch (...) {}
        return 1;
    }
    cout << "\n";

    // ================= [3/4] Finalisasi =================
    bool finalOK = true;
    if (usingPipe) {
        cout << "[3/4] Finalisasi: audio sudah ter-mux dalam single-pass ✓\n";
    } else {
        cout << "[3/4] Gabung audio ke video...\n";
        if (haveFFmpeg && haveAudio) {
            finalOK = ffmpeg::muxAudio(tempVideo, audioFile, out);
            if (!finalOK) {
                cerr << "     Gagal mux audio, fallback ke video tanpa audio.\n";
                finalOK = ffmpeg::remuxVideoOnly(tempVideo, out);
            }
        } else if (haveFFmpeg) {
            finalOK = ffmpeg::remuxVideoOnly(tempVideo, out);
        } else {
            try {
                fs::copy_file(tempVideo, out, fs::copy_options::overwrite_existing);
                finalOK = true;
            } catch (...) { finalOK = false; }
        }
        if (!finalOK) { cerr << "Gagal menghasilkan output final.\n"; return 1; }
        try { fs::remove(tempVideo); } catch (...) {}
    }

    // ================= [4/4] Info =================
    cout << "\n[4/4] ============ SELESAI ============\n";
    cout << "     Input frame  : " << frames     << "\n";
    cout << "     Output frame : " << written    << "\n";
    cout << "     FPS          : " << (fps * mult) << "\n";
    if (opts_.sceneDetect)
        cout << "     Occlusion    : ON (thresh=" << opts_.occlusionThresh << " px)\n";
    else
        cout << "     Occlusion    : OFF\n";
    if (opts_.useOcclusion) {
        double avgOccl = (occlusionPairCount > 0)
            ? 100.0 * occlusionRatioSum / occlusionPairCount : 0.0;
        cout << "     Occlusion    : rata-rata " << fixed << setprecision(1)
             << avgOccl << "% per frame\n" << defaultfloat;
    } else {
        cout << "     Occlusion    : detection dimatikan\n";
    }
    cout << "     Encoding     : "
         << (usingPipe ? "single-pass (pipe ffmpeg)"
                       : "two-pass (VideoWriter + ffmpeg mux)")
         << "\n";
    cout << "     Interpolator : " << interp->name()
     << (opts_.useAI ? "  [⚠ EXPERIMENTAL]" : "") << "\n";
    cout << "     Waktu proses : " << formatDuration(totalElapsed)
         << " (" << (frames / std::max(totalElapsed, 0.001)) << " fps rata-rata)\n";
    if (haveAudio) cout << "     Audio        : disertakan (" << audioFile << ")\n";
    else           cout << "     Audio        : tidak ada\n";
    cout << "     File output  : " << out << "\n";
    cout << "======================================\n";
    return 0;
}