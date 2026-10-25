#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <cstdio>

using namespace cv;
using namespace std;
namespace fs = std::filesystem;

// Forward declaration
static double sceneDiff(const cv::Mat& a, const cv::Mat& b);

// ==================================================================
//  Basic helpers
// ==================================================================
static bool runCmd(const string& cmd) {
    cout << "     >> " << cmd << "\n";
    return system(cmd.c_str()) == 0;
}

static bool ffmpegAvailable() {
#ifdef _WIN32
    return system("ffmpeg -version > nul 2>&1") == 0;
#else
    return system("ffmpeg -version > /dev/null 2>&1") == 0;
#endif
}

// ==================================================================
//  FFmpeg command runner dengan capture stdout + stderr
// ==================================================================
struct CmdResult {
    bool   ok = false;
    int    exitCode = -1;
    string firstErrorLine;         // baris stderr pertama yang meaningful
    vector<string> stdoutLines;    // output stdout (untuk ffprobe)
    vector<string> stderrLines;    // output stderr (untuk deteksi error)
};

static bool isFfmpegBannerLine(const string& l) {
    return l.find("ffmpeg version") != string::npos
        || l.find("ffprobe version") != string::npos
        || l.find("built with")     != string::npos
        || l.find("configuration:") != string::npos
        || l.find("Copyright (c)")  != string::npos
        || l.find("Press [q]")      != string::npos;
}

static bool hasCriticalFfmpegError(const vector<string>& lines) {
    static const char* kPatterns[] = {
        "Error submitting packet",
        "Invalid data found",
        "Input buffer exhausted",
        "could not find codec",
        "No such file",
        "Permission denied",
        "Unknown decoder",
        "moov atom not found",
        "Invalid NAL unit",
        "partial file",
        "does not contain any stream",
        "Could not open",
        "Error opening",
    };
    for (const auto& l : lines)
        for (auto p : kPatterns)
            if (l.find(p) != string::npos) return true;
    return false;
}

static CmdResult runFfmpegCmd(const string& cmd) {
    CmdResult r;

    // File sementara unik untuk stdout & stderr
    string stamp   = to_string(chrono::steady_clock::now().time_since_epoch().count());
    string outFile = "temp/_ffmpeg_" + stamp + ".out";
    string errFile = "temp/_ffmpeg_" + stamp + ".log";

    string fullCmd = cmd + " > \"" + outFile + "\" 2> \"" + errFile + "\"";
    cout << "     >> " << cmd << "\n";
    r.exitCode = system(fullCmd.c_str());

    // Baca stdout
    {
        ifstream f(outFile);
        string line;
        while (getline(f, line))
            if (!line.empty()) r.stdoutLines.push_back(line);
    }
    // Baca stderr
    {
        ifstream f(errFile);
        string line;
        while (getline(f, line))
            if (!line.empty()) r.stderrLines.push_back(line);
    }
    try { fs::remove(outFile); } catch (...) {}
    try { fs::remove(errFile); } catch (...) {}

    // Baris error pertama (skip banner)
    for (const auto& l : r.stderrLines) {
        if (isFfmpegBannerLine(l)) continue;
        r.firstErrorLine = l;
        break;
    }
    if (r.firstErrorLine.empty() && !r.stderrLines.empty())
        r.firstErrorLine = r.stderrLines.back();

    r.ok = (r.exitCode == 0) && !hasCriticalFfmpegError(r.stderrLines);
    return r;
}

// Tampilkan error 1-baris + pesan ramah, lalu caller tinggal `return 1`
static void showFriendlyFfmpegError(const CmdResult& r,
                                     const string& stageName,
                                     const string& inputFile) {
    cerr << "\n";
    cerr << "  [!!] GAGAL: " << stageName << "\n";
    if (!r.firstErrorLine.empty())
        cerr << "  [ffmpeg] " << r.firstErrorLine << "\n";
    cerr << "\n";
    cerr << "  Kemungkinan penyebab:\n";
    cerr << "    - Stream audio di file input korup / terputus\n";
    cerr << "    - File belum sepenuhnya terunduh\n";
    cerr << "    - Codec audio tidak didukung versi ffmpeg ini\n";
    cerr << "    - Container bermasalah\n";
    cerr << "\n";
    cerr << "  Saran:\n";
    cerr << "    1. Coba putar file di VLC / MPC-HC untuk verifikasi.\n";
    cerr << "       Kalau di sana juga rusak, file memang bermasalah —\n";
    cerr << "       unduh ulang atau cari sumber lain.\n";
    cerr << "    2. Kalau VLC normal, update ffmpeg ke build terbaru:\n";
    cerr << "       https://www.gyan.dev/ffmpeg/builds/ (pilih release-full)\n";
    cerr << "    3. Kalau hanya butuh video tanpa audio, jalankan ulang\n";
    cerr << "       dengan flag --no-audio, atau hapus audio manual dulu:\n";
    cerr << "       ffmpeg -i \"" << inputFile
         << "\" -an -c:v copy \"noaudio.mp4\"\n";
    cerr << "\n";
    cerr << "  Program dihentikan. Tidak ada output yang dihasilkan.\n";
    cerr << "\n";
}

// ==================================================================
//  Audio helpers
// ==================================================================
enum class AudioStatus { OK, NoAudio, Failed };

static AudioStatus extractAudio(const string& video, const string& outAudio) {
    // 1. Probe: ada stream audio atau tidak? (pakai stdoutLines, bukan stderr)
    string probeCmd =
        "ffprobe -v error -select_streams a:0 "
        "-show_entries stream=codec_name "
        "-of default=noprint_wrappers=1:nokey=1 \"" + video + "\"";

    CmdResult probe = runFfmpegCmd(probeCmd);
    if (probe.exitCode != 0) {
        // file tidak ada, corrupt, permission, dsb
        showFriendlyFfmpegError(probe, "Probe input file", video);
        return AudioStatus::Failed;
    }

    string codec;
    if (!probe.stdoutLines.empty()) {
        codec = probe.stdoutLines[0];
        size_t a = codec.find_first_not_of(" \t\r\n");
        size_t b = codec.find_last_not_of(" \t\r\n");
        codec = (a == string::npos) ? "" : codec.substr(a, b - a + 1);
    }
    if (codec.empty() || codec == "N/A")
        return AudioStatus::NoAudio;   // tanpa audio = normal

    cout << "     -> Stream audio: " << codec << "\n";

    // 2. Ekstrak audio
    string cmd = "ffmpeg -y -v error -i \"" + video + "\" -vn "
                 "-c:a aac -b:a 192k \"" + outAudio + "\"";
    CmdResult r = runFfmpegCmd(cmd);
    if (!r.ok) {
        showFriendlyFfmpegError(r, "Ekstrak audio (AAC decode)", video);
        return AudioStatus::Failed;
    }

    // 3. Sanity check hasil
    try {
        if (!fs::exists(outAudio) || fs::file_size(outAudio) < 1024) {
            cerr << "\n  [!!] Ekstrak audio selesai tapi hasilnya kosong/rusak.\n"
                 << "  Kemungkinan stream audio bermasalah. Program dihentikan.\n\n";
            return AudioStatus::Failed;
        }
    } catch (...) { return AudioStatus::Failed; }

    return AudioStatus::OK;
}

// Gabung video + audio → output final
static bool muxAudio(const string& videoOnly, const string& audioFile,
                     const string& finalOut) {
    string cmd = "ffmpeg -y -v error -i \"" + videoOnly + "\" -i \"" + audioFile + "\" "
                 "-map 0:v:0 -map 1:a:0 "
                 "-c:v libx264 -crf 18 -preset medium -pix_fmt yuv420p "
                 "-c:a aac -b:a 192k -shortest \"" + finalOut + "\"";
    CmdResult r = runFfmpegCmd(cmd);
    if (!r.ok) {
        cerr << "\n  [!!] Gagal menggabung audio ke video.\n";
        if (!r.firstErrorLine.empty())
            cerr << "  [ffmpeg] " << r.firstErrorLine << "\n";
        cerr << "  Fallback: menghasilkan video tanpa audio.\n\n";
        return false;
    }
    return true;
}

// Tanpa audio: stream copy
static bool remuxVideoOnly(const string& videoOnly, const string& finalOut) {
    string cmd = "ffmpeg -y -v error -i \"" + videoOnly + "\" -c copy \""
                 + finalOut + "\"";
    CmdResult r = runFfmpegCmd(cmd);
    if (!r.ok && !r.firstErrorLine.empty())
        cerr << "     [ffmpeg] " << r.firstErrorLine << "\n";
    return r.ok;
}

// ==================================================================
//  Optical flow interpolator
// ==================================================================
class FlowInterpolator {
public:
    FlowInterpolator(int mult = 2, float smoothSigma = 1.5f, bool useFarneback = true)
        : mult_(mult), smoothSigma_(smoothSigma), useFarneback_(useFarneback) {}

    vector<Mat> processPair(const Mat& prev, const Mat& next) {
        Mat f_ab, f_ba;
        computeFlows(prev, next, f_ab, f_ba);
        if (smoothSigma_ > 0.0f) {
            GaussianBlur(f_ab, f_ab, Size(0, 0), smoothSigma_);
            GaussianBlur(f_ba, f_ba, Size(0, 0), smoothSigma_);
        }
        vector<Mat> out;
        out.reserve(mult_);
        out.push_back(prev);
        for (int i = 1; i < mult_; ++i)
            out.push_back(interpAt(prev, next, f_ab, f_ba, float(i) / float(mult_)));
        return out;
    }

private:
    int   mult_;
    float smoothSigma_;
    bool  useFarneback_;

    void computeFlows(const Mat& a, const Mat& b, Mat& f_ab, Mat& f_ba) {
        Mat ga, gb;
        cvtColor(a, ga, COLOR_BGR2GRAY);
        cvtColor(b, gb, COLOR_BGR2GRAY);
        if (useFarneback_) {
            calcOpticalFlowFarneback(ga, gb, f_ab, 0.5, 5, 21, 3, 7, 1.5, 0);
            calcOpticalFlowFarneback(gb, ga, f_ba, 0.5, 5, 21, 3, 7, 1.5, 0);
        } else {
            auto dis = DISOpticalFlow::create(DISOpticalFlow::PRESET_MEDIUM);
            dis->calc(ga, gb, f_ab);
            dis->calc(gb, ga, f_ba);
        }
    }

    Mat warpWithFlow(const Mat& img, const Mat& flow, float scale) {
        Mat mapX(img.size(), CV_32F), mapY(img.size(), CV_32F);
        for (int y = 0; y < img.rows; ++y) {
            const Point2f* frow = flow.ptr<Point2f>(y);
            float* mx = mapX.ptr<float>(y);
            float* my = mapY.ptr<float>(y);
            for (int x = 0; x < img.cols; ++x) {
                mx[x] = x - scale * frow[x].x;
                my[x] = y - scale * frow[x].y;
            }
        }
        Mat out;
        remap(img, out, mapX, mapY, INTER_LINEAR, BORDER_REPLICATE);
        return out;
    }

    Mat interpAt(const Mat& prev, const Mat& next,
                 const Mat& f_ab, const Mat& f_ba, float t) {
        Mat wp = warpWithFlow(prev, f_ab, t);
        Mat wn = warpWithFlow(next, f_ba, 1.0f - t);
        Mat result;
        addWeighted(wp, 1.0f - t, wn, t, 0.0, result);
        return result;
    }
};

// ==================================================================
//  Scene diff & format helpers
// ==================================================================
static double sceneDiff(const Mat& a, const Mat& b) {
    Mat ga, gb;
    cvtColor(a, ga, COLOR_BGR2GRAY);
    cvtColor(b, gb, COLOR_BGR2GRAY);

    const int histSize = 64;
    float range[] = {0, 256};
    const float* ranges[] = {range};
    int channels[] = {0};

    Mat ha, hb;
    calcHist(&ga, 1, channels, noArray(), ha, 1, &histSize, ranges);
    calcHist(&gb, 1, channels, noArray(), hb, 1, &histSize, ranges);

    normalize(ha, ha, 0, 1, NORM_L1);
    normalize(hb, hb, 0, 1, NORM_L1);

    return compareHist(ha, hb, HISTCMP_CORREL);
}

static string formatDuration(double sec) {
    if (sec < 0 || sec > 1e9) return "--";
    if (sec < 1.0)  return "<1s";
    if (sec < 60.0) {
        char buf[16]; snprintf(buf, sizeof(buf), "%.0fs", sec); return buf;
    }
    int m = (int)(sec / 60.0);
    int s = (int)(sec - m * 60);
    if (m < 60) {
        char buf[24]; snprintf(buf, sizeof(buf), "%dm %02ds", m, s); return buf;
    }
    int h = m / 60; m = m % 60;
    char buf[24]; snprintf(buf, sizeof(buf), "%dh %02dm", h, m); return buf;
}

// ==================================================================
//  main
// ==================================================================
int main(int argc, char** argv) {
    string input  = "input.mp4";
    string output = "output.mp4";
    int    mult   = 2;
    bool   useFarneback = true;
    bool   sceneDetect  = true;
    bool   skipAudio    = false;
    double sceneThresh  = 0.4;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if      (arg == "-i" && i + 1 < argc) input  = argv[++i];
        else if (arg == "-o" && i + 1 < argc) output = argv[++i];
        else if (arg == "-m" && i + 1 < argc) mult   = stoi(argv[++i]);
        else if (arg == "-s" && i + 1 < argc) sceneThresh = stod(argv[++i]);
        else if (arg == "--dis")              useFarneback = false;
        else if (arg == "--no-scene")         sceneDetect  = false;
        else if (arg == "--no-audio")         skipAudio    = true;
        else if (arg == "-h" || arg == "--help") {
            cout << "Usage: " << argv[0]
                 << " -i in.mp4 -o out.mp4 -m <mult>\n"
                 << "       [--dis] [--no-scene] [-s <thresh>] [--no-audio]\n"
                 << "  -m <n>      : multiplier (2,3,4,...)\n"
                 << "  -s <thresh> : scene-change threshold (0.0-1.0, default 0.4)\n"
                 << "  --dis       : pakai DIS optical flow (lebih cepat)\n"
                 << "  --no-scene  : matikan scene-change detection\n"
                 << "  --no-audio  : skip audio (video-only)\n";
            return 0;
        }
    }
    if (mult < 2) { cerr << "Multiplier minimal 2\n"; return 1; }
    if (sceneThresh < 0.0 || sceneThresh > 1.0) sceneThresh = 0.4;

    try {
        if (fs::absolute(input) == fs::absolute(output)) {
            cerr << "Output tidak boleh sama dengan input\n"; return 1;
        }
    } catch (...) {}

    bool haveFFmpeg = ffmpegAvailable();
    if (!haveFFmpeg) {
        cerr << "WARNING: ffmpeg tidak ditemukan di PATH.\n"
             << "         Audio TIDAK akan disertakan.\n\n";
    }
    fs::create_directories("audio");
    fs::create_directories("temp");

    string stem      = fs::path(input).stem().string();
    string audioFile = "audio/" + stem + "_audio.m4a";
    string tempVideo = "temp/"  + stem + "_intermediate.mp4";

    // ================= [1/4] Ekstrak audio =================
    bool haveAudio = false;
    if (skipAudio) {
        cout << "[1/4] Audio: dilewati (--no-audio)\n";
    } else if (haveFFmpeg) {
        cout << "[1/4] Ekstrak audio ke folder audio/...\n";
        AudioStatus as = extractAudio(input, audioFile);
        if (as == AudioStatus::Failed) {
            try { fs::remove(audioFile); } catch (...) {}
            try { fs::remove(tempVideo); } catch (...) {}
            return 1;   // STOP
        }
        haveAudio = (as == AudioStatus::OK);
        if (haveAudio) cout << "     -> Audio disimpan: " << audioFile << "\n";
        else           cout << "     -> Tidak ada audio di input, skip.\n";
    } else {
        cout << "[1/4] Skip ekstrak audio (ffmpeg tidak tersedia)\n";
    }

    // ================= [2/4] Proses video =================
    cout << "[2/4] Proses frame interpolation...\n";
    VideoCapture cap(input);
    if (!cap.isOpened()) { cerr << "Tidak bisa buka: " << input << "\n"; return 1; }

    int    W   = (int)cap.get(CAP_PROP_FRAME_WIDTH);
    int    H   = (int)cap.get(CAP_PROP_FRAME_HEIGHT);
    double fps = cap.get(CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;
    cout << "     Input: " << W << "x" << H << " @ " << fps
         << " fps -> Output: " << fps * mult << " fps\n";
    if (sceneDetect)
        cout << "     Scene detection: ON (threshold=" << sceneThresh << ")\n";
    else
        cout << "     Scene detection: OFF\n";

    int fourcc = VideoWriter::fourcc('m', 'p', '4', 'v');
    VideoWriter writer(tempVideo, fourcc, fps * mult, Size(W, H));
    if (!writer.isOpened()) {
        tempVideo = "temp/" + stem + "_intermediate.avi";
        fourcc = VideoWriter::fourcc('M', 'J', 'P', 'G');
        writer.open(tempVideo, fourcc, fps * mult, Size(W, H));
    }
    if (!writer.isOpened()) { cerr << "Gagal buka VideoWriter\n"; return 1; }

    FlowInterpolator interp(mult, 1.5f, useFarneback);

    Mat prev, next;
    if (!cap.read(prev)) { cerr << "Video kosong\n"; return 1; }

    long frames = 1, written = 0, sceneCuts = 0;

    double totalFrames = cap.get(CAP_PROP_FRAME_COUNT);
    bool   haveTotal   = totalFrames > 1.0;
    auto   procStart   = chrono::steady_clock::now();
    bool   reprintBar  = false;

    while (cap.read(next)) {
        double corr = sceneDetect ? sceneDiff(prev, next) : 1.0;
        bool   cut  = sceneDetect && (corr < sceneThresh);

        if (cut) {
            sceneCuts++;
            printf("\r%*s\r", 78, "");
            printf("     [scene cut @ frame %ld, corr=%.3f]\n", frames, corr);
            reprintBar = true;
            for (int i = 0; i < mult; ++i) { writer.write(prev); written++; }
        } else {
            auto out = interp.processPair(prev, next);
            for (const auto& f : out) { writer.write(f); written++; }
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
    writer.write(prev); written++;

    double totalElapsed = chrono::duration<double>(
        chrono::steady_clock::now() - procStart).count();
    printf("\r%*s\r", 78, "");
    printf("     100%% — %ld frame diproses dalam %s (%.1f fps rata-rata)\n",
           frames, formatDuration(totalElapsed).c_str(),
           frames / std::max(totalElapsed, 0.001));

    writer.release();
    cout << "\n";

    // ================= [3/4] Gabung audio =================
    cout << "[3/4] Gabung audio ke video...\n";
    bool finalOK = false;
    if (haveFFmpeg && haveAudio) {
        finalOK = muxAudio(tempVideo, audioFile, output);
        if (!finalOK) {
            cerr << "     Gagal mux audio, fallback ke video tanpa audio.\n";
            finalOK = remuxVideoOnly(tempVideo, output);
        }
    } else if (haveFFmpeg) {
        finalOK = remuxVideoOnly(tempVideo, output);
    } else {
        try {
            fs::copy_file(tempVideo, output, fs::copy_options::overwrite_existing);
            finalOK = true;
        } catch (...) { finalOK = false; }
    }
    if (!finalOK) { cerr << "Gagal menghasilkan output final.\n"; return 1; }
    try { fs::remove(tempVideo); } catch (...) {}

    // ================= [4/4] Info =================
    cout << "\n[4/4] ============ SELESAI ============\n";
    cout << "     Input frame  : " << frames     << "\n";
    cout << "     Output frame : " << written    << "\n";
    cout << "     FPS          : " << (fps * mult) << "\n";
    if (sceneDetect)
        cout << "     Scene cuts   : " << sceneCuts << " terdeteksi\n";
    else
        cout << "     Scene cuts   : detection dimatikan\n";
    cout << "     Waktu proses : " << formatDuration(totalElapsed)
         << " (" << (frames / std::max(totalElapsed, 0.001)) << " fps rata-rata)\n";
    if (haveAudio) cout << "     Audio        : disertakan (" << audioFile << ")\n";
    else           cout << "     Audio        : tidak ada\n";
    cout << "     File output  : " << output     << "\n";
    cout << "======================================\n";
    return 0;
}