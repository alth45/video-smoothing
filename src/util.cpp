#include "util.hpp"
#include <cstdio>

std::string formatDuration(double sec) {
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