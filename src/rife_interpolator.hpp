// =====================================================================
//  ⚠  EXPERIMENTAL — BELUM TERUJI
// ---------------------------------------------------------------------
//  Implementasi RIFE AI ini BELUM diuji oleh developer karena tidak
//  tersedia hardware GPU. Kode disertakan sebagai referensi dan siap
//  pakai, tapi:
//
//    • Kualitas output belum diverifikasi
//    • Stabilitas antar model ONNX belum diuji
//    • Performa CPU-only belum diukur
//    • CUDA path belum dijalankan
//
//  Pakai dengan flag: --ai-experimental (wajib) + --ai-ack (scripting)
//  Bukan untuk production tanpa pengujian mandiri.
// =====================================================================
#pragma once
#include "interpolator.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <string>
#include <vector>

class RifeInterpolator : public IInterpolator {
public:
    RifeInterpolator(const std::string& modelPath, int mult = 2);
    ~RifeInterpolator() override = default;

    bool isReady() const { return ready_; }
    const std::string& error() const { return error_; }

    std::vector<cv::Mat> processPair(const cv::Mat& prev,
                                     const cv::Mat& next) override;
    float lastOcclusionRatio() const override { return 0.0f; }
    const char* name() const override { return "RIFE"; }

private:
    cv::dnn::Net net_;
    bool        ready_ = false;
    std::string error_;
    int         mult_ = 2;
    bool        hasTimestep_ = false;
    std::string inName0_, inName1_, inNameT_;
    int         padTo_ = 32;

    cv::Mat inferOne(const cv::Mat& a, const cv::Mat& b, float t);
    cv::Mat preprocess(const cv::Mat& bgr, int& padH, int& padW);
    cv::Mat postprocess(const cv::Mat& blobOut, int origH, int origW);
    bool    detectInputs();
};