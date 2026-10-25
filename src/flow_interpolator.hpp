#pragma once
#include <opencv2/opencv.hpp>
#include <vector>

class FlowInterpolator {
public:
    FlowInterpolator(int mult = 2,
                     float smoothSigma = 1.5f,
                     bool useFarneback = true);

    std::vector<cv::Mat> processPair(const cv::Mat& prev,
                                     const cv::Mat& next);

private:
    int   mult_;
    float smoothSigma_;
    bool  useFarneback_;

    void computeFlows(const cv::Mat& a, const cv::Mat& b,
                      cv::Mat& f_ab, cv::Mat& f_ba);

    cv::Mat warpWithFlow(const cv::Mat& img, const cv::Mat& flow, float scale);

    cv::Mat interpAt(const cv::Mat& prev, const cv::Mat& next,
                     const cv::Mat& f_ab, const cv::Mat& f_ba, float t);
};