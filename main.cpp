#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>

using namespace cv;
using namespace std;
namespace fs = std::filesystem;

// ==================================================================
//  Helper ffmpeg
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

// Ekstrak audio ke file terpisah (re-encode aac 192k, selalu kompatibel)
static bool extractAudio(const string& video, const string& outAudio) {
    //  -map 0:a?  -> audio opsional, tidak error kalau input tanpa audio
    string cmd = "ffmpeg -y -v error -i \"" + video + "\" -vn "
                 "-c:a aac -b:a 192k -map 0:a? \"" + outAudio + "\"";
    if (!runCmd(cmd)) return false;
    try {
        if (!fs::exists(outAudio)) return false;
        return fs::file_size(outAudio) > 1024;   // file ~0 byte = tidak ada audio
    } catch (...) { return false; }
}

// Gabung video + audio → output final (re-encode ke H.264 supaya universal)
static bool muxAudio(const string& videoOnly, const string& audioFile,
                     const string& finalOut) {
    string cmd = "ffmpeg -y -v error -i \"" + videoOnly + "\" -i \"" + audioFile + "\" "
                 "-map 0:v:0 -map 1:a:0 "
                 "-c:v libx264 -crf 18 -preset medium -pix_fmt yuv420p "
                 "-c:a aac -b:a 192k -shortest "
                 "\"" + finalOut + "\"";
    return runCmd(cmd);
}

// Tanpa audio: cukup stream copy
static bool remuxVideoOnly(const string& videoOnly, const string& finalOut) {
    string cmd = "ffmpeg -y -v error -i \"" + videoOnly + "\" -c copy \""
                 + finalOut + "\"";
    return runCmd(cmd);
}

// ==================================================================
//  Optical flow interpolator (tidak berubah)
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
//  main
// ==================================================================
int main(int argc, char** argv) {
    string input  = "input.mp4";
    string output = "output.mp4";
    int    mult   = 2;
    bool   useFarneback = true;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if      (arg == "-i" && i + 1 < argc) input  = argv[++i];
        else if (arg == "-o" && i + 1 < argc) output = argv[++i];
        else if (arg == "-m" && i + 1 < argc) mult   = stoi(argv[++i]);
        else if (arg == "--dis")              useFarneback = false;
        else if (arg == "-h" || arg == "--help") {
            cout << "Usage: " << argv[0]
                 << " -i input.mp4 -o output.mp4 -m <multiplier> [--dis]\n";
            return 0;
        }
    }
    if (mult < 2) { cerr << "Multiplier minimal 2\n"; return 1; }

    // Cegah input == output
    try {
        if (fs::absolute(input) == fs::absolute(output)) {
            cerr << "Output tidak boleh sama dengan input\n"; return 1;
        }
    } catch (...) {}

    // ---- Cek ffmpeg & siapkan folder ----
    bool haveFFmpeg = ffmpegAvailable();
    if (!haveFFmpeg) {
        cerr << "WARNING: ffmpeg tidak ditemukan di PATH.\n"
             << "         Audio TIDAK akan disertakan.\n"
             << "         Download: https://www.gyan.dev/ffmpeg/builds/  "
             << "(extract, tambahkan folder bin ke PATH)\n\n";
    }
    fs::create_directories("audio");
    fs::create_directories("temp");

    string stem      = fs::path(input).stem().string();
    string audioFile = "audio/" + stem + "_audio.m4a";
    string tempVideo = "temp/"  + stem + "_intermediate.mp4";

    // ================= [1/4] Ekstrak audio =================
    bool haveAudio = false;
    if (haveFFmpeg) {
        cout << "[1/4] Ekstrak audio ke folder audio/...\n";
        haveAudio = extractAudio(input, audioFile);
        if (haveAudio)
            cout << "     -> Audio disimpan: " << audioFile << "\n";
        else
            cout << "     -> Tidak ada audio di input, skip.\n";
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

    long frames = 1, written = 0;
    while (cap.read(next)) {
        auto out = interp.processPair(prev, next);
        for (const auto& f : out) { writer.write(f); written++; }
        prev = next.clone();
        frames++;
        if (frames % 30 == 0)
            cout << "     Processed " << frames << " frames\r" << flush;
    }
    writer.write(prev); written++;
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

    // Hapus intermediate, simpan audio/ sesuai permintaan (bisa dihapus manual)
    try { fs::remove(tempVideo); } catch (...) {}

    // ================= [4/4] Info selesai =================
    cout << "\n[4/4] ============ SELESAI ============\n";
    cout << "     Input frame  : " << frames     << "\n";
    cout << "     Output frame : " << written    << "\n";
    cout << "     FPS          : " << (fps * mult) << "\n";
    if (haveAudio) cout << "     Audio        : disertakan (" << audioFile << ")\n";
    else           cout << "     Audio        : tidak ada\n";
    cout << "     File output  : " << output     << "\n";
    cout << "======================================\n";
    return 0;
}


