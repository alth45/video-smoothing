#pragma once
#include <opencv2/opencv.hpp>

// Histogram correlation antar dua frame.
// 1.0 = sama persis, 0.0 = tidak terkorelasi, <0 = berlawanan.
// Scene cut tipikal < 0.4; high-motion same-scene biasanya > 0.5
double sceneDiff(const cv::Mat& a, const cv::Mat& b);