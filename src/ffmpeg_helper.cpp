// Copyright (c) 2026 Nurhadi
// SPDX-License-Identifier: MIT

#include "ffmpeg_helper.hpp"

#include <opencv2/opencv.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
  #include <io.h>
  #define POPEN  _popen
  #define PCLOSE _pclose
#else
  #include <stdio.h>
  #define POPEN  popen
  #define PCLOSE pclose
#endif

namespace fs = std::filesystem;
using namespace std;

namespace ffmpeg {

// ---------- internal helpers ----------
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

// ---------- public API ----------
bool isAvailable() {
#ifdef _WIN32
    return system("ffmpeg -version > nul 2>&1") == 0;
#else
    return system("ffmpeg -version > /dev/null 2>&1") == 0;
#endif
}

CmdResult run(const string& cmd) {
    CmdResult r;

    string stamp   = to_string(chrono::steady_clock::now().time_since_epoch().count());
    string outFile = "temp/_ffmpeg_" + stamp + ".out";
    string errFile = "temp/_ffmpeg_" + stamp + ".log";

    string fullCmd = cmd + " > \"" + outFile + "\" 2> \"" + errFile + "\"";
    cout << "     >> " << cmd << "\n";
    r.exitCode = system(fullCmd.c_str());

    { ifstream f(outFile); string line;
      while (getline(f, line)) if (!line.empty()) r.stdoutLines.push_back(line); }
    { ifstream f(errFile); string line;
      while (getline(f, line)) if (!line.empty()) r.stderrLines.push_back(line); }

    try { fs::remove(outFile); } catch (...) {}
    try { fs::remove(errFile); } catch (...) {}

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

void showFriendlyError(const CmdResult& r,
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
    cerr << "    2. Update ffmpeg ke build terbaru (gyan.dev, release-full).\n";
    cerr << "    3. Kalau hanya butuh video, jalankan dengan --no-audio:\n";
    cerr << "       ffmpeg -i \"" << inputFile
         << "\" -an -c:v copy \"noaudio.mp4\"\n";
    cerr << "\n";
    cerr << "  Program dihentikan. Tidak ada output yang dihasilkan.\n\n";
}

bool hasEncoder(const string& name) {
#ifdef _WIN32
    string cmd = "ffmpeg -hide_banner -encoders 2>&1 | find \""
               + name + "\" >nul 2>&1";
#else
    string cmd = "ffmpeg -hide_banner -encoders 2>&1 | grep -q \""
               + name + "\"";
#endif
    return system(cmd.c_str()) == 0;
}

AudioStatus extractAudio(const string& video, const string& outAudio) {
    string probeCmd =
        "ffprobe -v error -select_streams a:0 "
        "-show_entries stream=codec_name "
        "-of default=noprint_wrappers=1:nokey=1 \"" + video + "\"";

    CmdResult probe = run(probeCmd);
    if (probe.exitCode != 0) {
        showFriendlyError(probe, "Probe input file", video);
        return AudioStatus::Failed;
    }

    string codec;
    if (!probe.stdoutLines.empty()) {
        codec = probe.stdoutLines[0];
        size_t a = codec.find_first_not_of(" \t\r\n");
        size_t b = codec.find_last_not_of(" \t\r\n");
        codec = (a == string::npos) ? "" : codec.substr(a, b - a + 1);
    }
    if (codec.empty() || codec == "N/A") return AudioStatus::NoAudio;

    cout << "     -> Stream audio: " << codec << "\n";

    string cmd = "ffmpeg -y -v error -i \"" + video + "\" -vn "
                 "-c:a aac -b:a 192k \"" + outAudio + "\"";
    CmdResult r = run(cmd);
    if (!r.ok) {
        showFriendlyError(r, "Ekstrak audio (AAC decode)", video);
        return AudioStatus::Failed;
    }

    try {
        if (!fs::exists(outAudio) || fs::file_size(outAudio) < 1024) {
            cerr << "\n  [!!] Ekstrak audio selesai tapi hasilnya kosong/rusak.\n"
                 << "  Kemungkinan stream audio bermasalah. Program dihentikan.\n\n";
            return AudioStatus::Failed;
        }
    } catch (...) { return AudioStatus::Failed; }

    return AudioStatus::OK;
}

bool muxAudio(const string& videoOnly, const string& audioFile,
              const string& finalOut) {
    string cmd = "ffmpeg -y -v error -i \"" + videoOnly + "\" -i \"" + audioFile + "\" "
                 "-map 0:v:0 -map 1:a:0 "
                 "-c:v libx264 -crf 18 -preset medium -pix_fmt yuv420p "
                 "-c:a aac -b:a 192k -shortest \"" + finalOut + "\"";
    CmdResult r = run(cmd);
    if (!r.ok) {
        cerr << "\n  [!!] Gagal menggabung audio ke video.\n";
        if (!r.firstErrorLine.empty())
            cerr << "  [ffmpeg] " << r.firstErrorLine << "\n";
        cerr << "  Fallback: menghasilkan video tanpa audio.\n\n";
        return false;
    }
    return true;
}

bool remuxVideoOnly(const string& videoOnly, const string& finalOut) {
    string cmd = "ffmpeg -y -v error -i \"" + videoOnly + "\" -c copy \""
                 + finalOut + "\"";
    CmdResult r = run(cmd);
    if (!r.ok && !r.firstErrorLine.empty())
        cerr << "     [ffmpeg] " << r.firstErrorLine << "\n";
    return r.ok;
}

// ==================================================================
//  FrameWriter
// ==================================================================
FrameWriter::FrameWriter() = default;

FrameWriter::~FrameWriter() {
    if (opened_ && !closed_) close();
}

bool FrameWriter::open(const string& outputPath,
                       int width, int height, double fps,
                       const string& audioPath,
                       int crf,
                       const string& preset) {
    if (opened_) { errorMsg_ = "FrameWriter sudah dibuka"; return false; }

    outputPath_ = outputPath;
    width_  = width;
    height_ = height;

    try { fs::create_directories("temp"); } catch (...) {}

    string stamp = to_string(
        chrono::steady_clock::now().time_since_epoch().count());
    errFile_ = "temp/_ffmpeg_pipe_" + stamp + ".log";

    string codec = hasEncoder("libx264") ? "libx264" : "mpeg4";
    bool hasAudio = !audioPath.empty();

    // Bangun command
    string cmd = "ffmpeg -y -v error"
               + string(" -f rawvideo -pix_fmt bgr24 -s ")
               + to_string(width) + "x" + to_string(height)
               + " -r " + to_string(fps) + " -i -";

    if (hasAudio) cmd += " -i \"" + audioPath + "\"";
    if (hasAudio) cmd += " -map 0:v:0 -map 1:a:0";

    if (codec == "libx264")
        cmd += " -c:v libx264 -crf " + to_string(crf)
             + " -preset " + preset;
    else
        cmd += " -c:v mpeg4 -q:v 2";

    cmd += " -pix_fmt yuv420p";

    if (hasAudio) cmd += " -c:a aac -b:a 192k -shortest";

    // +faststart kalau output .mp4
    string lower = outputPath;
    transform(lower.begin(), lower.end(), lower.begin(),
              [](unsigned char c){ return (char)tolower(c); });
    bool isMp4 = lower.size() >= 4 && lower.substr(lower.size()-4) == ".mp4";
    if (isMp4) cmd += " -movflags +faststart";

    cmd += " \"" + outputPath + "\"";

    string fullCmd = cmd + " 2> \"" + errFile_ + "\"";
    cout << "     >> [pipe] " << cmd << "\n";

    pipe_ = POPEN(fullCmd.c_str(), "wb");
    if (!pipe_) {
        errorMsg_ = "Gagal membuka pipe ffmpeg (apakah ffmpeg ada di PATH?)";
        return false;
    }

    opened_ = true;
    return true;
}

bool FrameWriter::writeFrame(const cv::Mat& frame) {
    if (!opened_ || closed_ || !pipe_) {
        errorMsg_ = "FrameWriter belum dibuka atau sudah ditutup";
        return false;
    }

    if (frame.cols != width_ || frame.rows != height_) {
        errorMsg_ = "Dimensi frame tidak sesuai ("
                  + to_string(frame.cols) + "x" + to_string(frame.rows)
                  + " vs " + to_string(width_) + "x" + to_string(height_) + ")";
        return false;
    }

    // Pastikan BGR + continuous
    cv::Mat bgr;
    if (frame.channels() == 3 && frame.isContinuous()) {
        bgr = frame;
    } else if (frame.channels() == 4) {
        cv::cvtColor(frame, bgr, cv::COLOR_BGRA2BGR);
    } else if (frame.channels() == 1) {
        cv::cvtColor(frame, bgr, cv::COLOR_GRAY2BGR);
    } else {
        bgr = frame.clone();  // membuat continuous
    }

    size_t expected = (size_t)width_ * (size_t)height_ * 3;
    size_t written = fwrite(bgr.data, 1, expected, pipe_);
    if (written != expected || ferror(pipe_)) {
        errorMsg_ = "Penulisan frame ke pipe gagal (ffmpeg mungkin sudah tutup)";
        return false;
    }
    return true;
}

bool FrameWriter::close() {
    if (!opened_ || closed_) return false;
    closed_ = true;

    if (!pipe_) return false;

    int rc = PCLOSE(pipe_);
    pipe_ = nullptr;
    opened_ = false;

    // Baca stderr
    vector<string> errLines;
    { ifstream f(errFile_); string line;
      while (getline(f, line)) if (!line.empty()) errLines.push_back(line); }
    try { fs::remove(errFile_); } catch (...) {}

    bool critical = hasCriticalFfmpegError(errLines);

    if (rc != 0 || critical) {
        for (const auto& l : errLines) {
            if (isFfmpegBannerLine(l)) continue;
            errorMsg_ = l;
            break;
        }
        if (errorMsg_.empty() && !errLines.empty())
            errorMsg_ = errLines.back();

        CmdResult cr;
        cr.exitCode = rc;
        cr.firstErrorLine = errorMsg_;
        cr.stderrLines = errLines;
        showFriendlyError(cr, "Encode video (pipe ffmpeg)", outputPath_);

        try { fs::remove(outputPath_); } catch (...) {}
        return false;
    }

    return true;
}

} // namespace ffmpeg