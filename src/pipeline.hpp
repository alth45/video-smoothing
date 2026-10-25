// Copyright (c) 2026 Nurhadi
// SPDX-License-Identifier: MIT

#pragma once
#include <string>

struct PipelineOptions {
    std::string input        = "input.mp4";
    std::string output       = "output.mp4";
    int         mult         = 2;
    bool        useFarneback = true;
    bool        sceneDetect  = true;
    bool        skipAudio    = false;
    double      sceneThresh  = 0.4;
    bool        useOcclusion = true;    
    float       occlusionThresh = 2.0f; 
    bool        usePipe      = true;   
    bool        useAI           = false;    
    bool        aiAcknowledged  = false;                   
    std::string rifeModelPath   = "models/rife.onnx";       
};
class VideoSmoothingPipeline {
public:
    explicit VideoSmoothingPipeline(const PipelineOptions& opts);
    int run();   // 0 = sukses, 1 = gagal

private:
    PipelineOptions opts_;
    int processVideo(const std::string& tempVideo);
};