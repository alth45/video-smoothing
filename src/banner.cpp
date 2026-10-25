// Copyright (c) 2026 Nurhadi
// SPDX-License-Identifier: MIT

#include "banner.hpp"
#include <iostream>
#include <ctime>


 

namespace banner {

    // di banner.cpp
#ifdef _WIN32
  #include <io.h>
  #define IS_TTY _isatty(_fileno(stdout))
#else
  #include <unistd.h>
  #define IS_TTY isatty(STDOUT_FILENO)
#endif


void print() {
    if(!IS_TTY) return;
    std::cout << R"(
 __   __ _     _                 ____                       _     _
 \ \ / /(_) __| | ___  ___      / ___| _ __ ___   ___  ___ | |__ (_)_ __   __ _
  \ V / | |/ _` |/ _ \/ _ \_____\___ \| '_ ` _ \ / _ \/ __|| '_ \| | '_ \ / _` |
   | || | | (_| |  __/ (_) |_____|___) | | | | | | (_) \__ \| | | | | | | | (_| |
   |_||_|_|\__,_|\___|\___/     |____/|_| |_| |_|\___/|___/|_| |_|_|_| |_|\__, |
                                                                          |___/
)";

   std::time_t t = std::time(nullptr);
   std::tm tm{};
    #ifdef _WIN32
        localtime_s(&tm, &t);
    #else
        localtime_r(&t, &tm);
    #endif
        int year = 1900 + tm.tm_year;

    std::cout << "                    Video Frame Interpolation Engine\n";
    std::cout << "       Powered by Muhamad Nurhadi -> github(alth45)  ·  "
              << "© "<<  year +  "Video Flow Smooth\n";
    std::cout << "       ==========================================================\n\n";
}

} // namespace banner