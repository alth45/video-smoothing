#include "pipeline.hpp"

#include <iostream>
#include <string>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

static void printHelp(const char* argv0) {
    cout << "Usage: " << argv0
         << " -i in.mp4 -o out.mp4 -m <mult>\n"
         << "       [--dis] [--no-scene] [-s <thresh>] [--no-audio]\n"
         << "  -m <n>      : multiplier (2,3,4,...)\n"
         << "  -s <thresh> : scene-change threshold (0.0-1.0, default 0.4)\n"
         << "  --dis       : pakai DIS optical flow (lebih cepat)\n"
         << "  --no-scene  : matikan scene-change detection\n"
         << "  --no-audio  : skip audio (video-only)\n";
}

int main(int argc, char** argv) {
    PipelineOptions opts;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if      (arg == "-i" && i + 1 < argc) opts.input  = argv[++i];
        else if (arg == "-o" && i + 1 < argc) opts.output = argv[++i];
        else if (arg == "-m" && i + 1 < argc) opts.mult   = stoi(argv[++i]);
        else if (arg == "-s" && i + 1 < argc) opts.sceneThresh = stod(argv[++i]);
        else if (arg == "--dis")              opts.useFarneback = false;
        else if (arg == "--no-scene")         opts.sceneDetect  = false;
        else if (arg == "--no-audio")         opts.skipAudio    = true;
        else if (arg == "-h" || arg == "--help") {
            printHelp(argv[0]);
            return 0;
        }
    }

    if (opts.mult < 2) { cerr << "Multiplier minimal 2\n"; return 1; }
    if (opts.sceneThresh < 0.0 || opts.sceneThresh > 1.0) opts.sceneThresh = 0.4;

    try {
        if (fs::absolute(opts.input) == fs::absolute(opts.output)) {
            cerr << "Output tidak boleh sama dengan input\n"; return 1;
        }
    } catch (...) {}

    VideoSmoothingPipeline pipeline(opts);
    return pipeline.run();
}