#pragma once
#include <string>
#include <vector>

namespace ffmpeg {

struct CmdResult {
    bool   ok = false;
    int    exitCode = -1;
    std::string firstErrorLine;         // 1 baris error asli
    std::vector<std::string> stdoutLines; // output stdout
    std::vector<std::string> stderrLines; // output stderr
};

bool isAvailable();

CmdResult run(const std::string& cmd);

void showFriendlyError(const CmdResult& r,
                       const std::string& stageName,
                       const std::string& inputFile);

enum class AudioStatus { OK, NoAudio, Failed };

AudioStatus extractAudio(const std::string& video,
                         const std::string& outAudio);

bool muxAudio(const std::string& videoOnly,
              const std::string& audioFile,
              const std::string& finalOut);

bool remuxVideoOnly(const std::string& videoOnly,
                    const std::string& finalOut);

} // namespace ffmpeg