// Copyright (c) 2026 Nurhadi
// SPDX-License-Identifier: MIT

#pragma once
#include <string>
#include <vector>
#include <cstdio>

// Forward declaration — cukup untuk parameter `const cv::Mat&`
namespace cv { class Mat; }

namespace ffmpeg {

struct CmdResult {
    bool   ok = false;
    int    exitCode = -1;
    std::string firstErrorLine;
    std::vector<std::string> stdoutLines;
    std::vector<std::string> stderrLines;
};

bool isAvailable();

CmdResult run(const std::string& cmd);

void showFriendlyError(const CmdResult& r,
                       const std::string& stageName,
                       const std::string& inputFile);

// Cek apakah ffmpeg punya encoder tertentu (mis. "libx264")
bool hasEncoder(const std::string& name);

enum class AudioStatus { OK, NoAudio, Failed };

AudioStatus extractAudio(const std::string& video,
                         const std::string& outAudio);

bool muxAudio(const std::string& videoOnly,
              const std::string& audioFile,
              const std::string& finalOut);

bool remuxVideoOnly(const std::string& videoOnly,
                    const std::string& finalOut);

// ==================================================================
//  FrameWriter — pipe raw BGR frames ke ffmpeg (single-pass encode)
// ==================================================================
class FrameWriter {
public:
    FrameWriter();
    ~FrameWriter();

    FrameWriter(const FrameWriter&) = delete;
    FrameWriter& operator=(const FrameWriter&) = delete;

    // Buka pipe ke ffmpeg. audioPath kosong = tanpa audio.
    bool open(const std::string& outputPath,
              int width, int height, double fps,
              const std::string& audioPath = "",
              int crf = 18,
              const std::string& preset = "medium");

    // Kirim satu frame BGR (CV_8UC3). Mengembalikan false bila pipe putus.
    bool writeFrame(const cv::Mat& frame);

    // Tutup pipe, tunggu ffmpeg selesai. Mengembalikan true bila sukses.
    bool close();

    bool isOpen() const { return opened_ && !closed_; }
    std::string lastError() const { return errorMsg_; }

private:
    std::FILE* pipe_ = nullptr;
    std::string errFile_;
    std::string outputPath_;
    int width_ = 0, height_ = 0;
    bool opened_ = false;
    bool closed_ = false;
    std::string errorMsg_;
};

} // namespace ffmpeg