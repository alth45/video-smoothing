#pragma once
#include <opencv2/opencv.hpp>
#include <vector>

class IInterpolator {
public:
    virtual ~IInterpolator() = default;
    virtual std::vector<cv::Mat> processPair(const cv::Mat& prev,
                                              const cv::Mat& next) = 0;
    virtual float lastOcclusionRatio() const = 0;
    virtual const char* name() const = 0;
};