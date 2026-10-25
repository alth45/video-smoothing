#pragma once
#include <opencv2/opencv.hpp>
#include <vector>

class FlowInterpolator {
public:
    FlowInterpolator(int mult = 2,
                     float smoothSigma = 1.5f,
                     bool useFarneback = true,
                     bool useOcclusion = true,
                     float occlusionThresh = 2.0f);

    std::vector<cv::Mat> processPair(const cv::Mat& prev,
                                     const cv::Mat& next);

    // Rata-rata % piksel yang dianggap occluded oleh FB check
    // (0.0 = semua reliable, 1.0 = semua occluded).
    // Di-reset setiap processPair().
    float lastOcclusionRatio() const { return lastOcclusionRatio_; }

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

    cv::Mat computeOcclusionMask(const cv::Mat& f_ab, const cv::Mat& f_ba,
                                  float thresh);

    // maskPrev / maskNext boleh kosong (artinya occlusion off)
    cv::Mat interpAt(const cv::Mat& prev, const cv::Mat& next,
                     const cv::Mat& f_ab, const cv::Mat& f_ba,
                     const cv::Mat& maskPrev, const cv::Mat& maskNext,
                     float t);
};