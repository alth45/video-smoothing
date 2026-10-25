#pragma once
#include "interpolator.hpp"
#include <opencv2/opencv.hpp>
#include <vector>

class FlowInterpolator : public IInterpolator {
public:
    FlowInterpolator(int mult = 2,
                     float smoothSigma = 1.5f,
                     bool useFarneback = true,
                     bool useOcclusion = true,
                     float occlusionThresh = 2.0f);

    std::vector<cv::Mat> processPair(const cv::Mat& prev,
                                     const cv::Mat& next) override;
    float lastOcclusionRatio() const override { return lastOcclusionRatio_; }
    const char* name() const override { return "Flow"; }

private:
    int   mult_;
    float smoothSigma_;
    bool  useFarneback_;
    bool  useOcclusion_;
    float occlusionThresh_;
    float lastOcclusionRatio_ = 0.0f;

    void computeFlows(const cv::Mat& a, const cv::Mat& b,
                      cv::Mat& f_ab, cv::Mat& f_ba);
    cv::Mat warpWithFlow(const cv::Mat& img, const cv::Mat& flow, float scale);
    cv::Mat warpMaskWithFlow(const cv::Mat& mask, const cv::Mat& flow, float scale);
    cv::Mat computeOcclusionMask(const cv::Mat& f_ab, const cv::Mat& f_ba, float thresh);
    cv::Mat interpAt(const cv::Mat& prev, const cv::Mat& next,
                     const cv::Mat& f_ab, const cv::Mat& f_ba,
                     const cv::Mat& maskPrev, const cv::Mat& maskNext, float t);
};