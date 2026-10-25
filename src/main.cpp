// Copyright (c) 2026 Nurhadi
// SPDX-License-Identifier: MIT

#include "pipeline.hpp"
#include "banner.hpp"
#include <iostream>
#include <string>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

static void printHelp(const char* argv0) {
    cout << "Usage: " << argv0
         << " -i in.mp4 -o out.mp4 -m <mult>\n"
         << "       [--dis] [--no-scene] [-s <thresh>]\n"
         << "       [--no-occlusion] [--occ-thresh <px>]\n"
         << "       [--no-pipe] [--no-audio]\n"
         << "       [--ai-experimental [--ai-ack]] [--rife-model <path>]\n"
         << "\n"
         << "  -m <n>              : multiplier (2,3,4,...)\n"
         << "  -s <thresh>         : scene-change threshold (0.0-1.0, default 0.4)\n"
         << "  --occ-thresh <px>   : occlusion FB-check threshold (default 2.0)\n"
         << "  --dis               : pakai DIS optical flow (lebih cepat)\n"
         << "  --no-scene          : matikan scene-change detection\n"
         << "  --no-occlusion      : matikan occlusion masking\n"
         << "  --no-pipe           : paksa mode VideoWriter (fallback lama)\n"
         << "  --no-audio          : skip audio (video-only)\n"
         << "\n"
         << "  --ai-experimental   : RIFE (AI) — ⚠ EXPERIMENTAL, BELUM TERUJI\n"
         << "  --ai-ack            : skip konfirmasi prompt (untuk scripting)\n"
         << "  --rife-model <path> : path model ONNX (default models/rife.onnx)\n";
}

int main(int argc, char** argv) {
    banner::print(); //tampilkan banner
    PipelineOptions opts;

    bool sawAiExperimental = false;
    bool sawAiAck          = false;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if      (arg == "-i" && i + 1 < argc) opts.input  = argv[++i];
        else if (arg == "-o" && i + 1 < argc) opts.output = argv[++i];
        else if (arg == "-m" && i + 1 < argc) opts.mult   = stoi(argv[++i]);
        else if (arg == "-s" && i + 1 < argc) opts.sceneThresh = stod(argv[++i]);
        else if (arg == "--dis")              opts.useFarneback = false;
        else if (arg == "--no-scene")         opts.sceneDetect  = false;
        else if (arg == "--no-audio")         opts.skipAudio    = true;
        else if (arg == "--occ-thresh" && i + 1 < argc) opts.occlusionThresh = stof(argv[++i]);
        else if (arg == "--no-occlusion")               opts.useOcclusion = false;
        else if (arg == "--no-pipe")          opts.usePipe      = false;
        else if (arg == "--ai") {
            cerr << "\n";
            cerr << "  [!!] Flag --ai sudah deprecated.\n";
            cerr << "       RIFE AI belum teruji — gunakan --ai-experimental\n";
            cerr << "       (atau tanpa AI untuk Farneback/DIS).\n";
            cerr << "\n";
            return 1;
        }
        else if (arg == "--ai-experimental")              sawAiExperimental = true;
        else if (arg == "--ai-ack")                       sawAiAck = true;
        else if (arg == "--rife-model" && i + 1 < argc)   opts.rifeModelPath = argv[++i];
        else if (arg == "-h" || arg == "--help") {
            printHelp(argv[0]);
            return 0;
        }
    }

    opts.useAI          = sawAiExperimental; 
    opts.aiAcknowledged = sawAiAck;

    if (opts.mult < 2) { cerr << "Multiplier minimal 2\n"; return 1; }
    if (opts.sceneThresh < 0.0 || opts.sceneThresh > 1.0) opts.sceneThresh = 0.4;
    if (opts.occlusionThresh < 0.1f) opts.occlusionThresh = 0.1f;
    if (opts.occlusionThresh > 10.0f) opts.occlusionThresh = 10.0f;

    try {
        if (fs::absolute(opts.input) == fs::absolute(opts.output)) {
            cerr << "Output tidak boleh sama dengan input\n"; return 1;
        }
    } catch (...) {}

    VideoSmoothingPipeline pipeline(opts);
    return pipeline.run();
}