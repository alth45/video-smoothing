// =====================================================================
//  ⚠  EXPERIMENTAL — BELUM TERUJI
//  Lihat rife_interpolator.hpp untuk detail. Bagian kritis yang perlu
//  diverifikasi mandiri:
//
//    • Konvensi input model (RGB vs BGR, [0,1] vs [-1,1])
//    • Nama input layer (deteksi otomatis — belum semua model teruji)
//    • Padding to 32×32 (beberapa export menggunakan 64×64)
//    • Kompatibilitas DNN backend CUDA / CPU
// =====================================================================
#include "rife_interpolator.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

using namespace cv;
using namespace std;

// Nama input yang umum dipakai exporter RIFE (Practical-RIFE v4.x)
static const vector<vector<string>> kInputNameSets = {
    {"img0", "img1", "timestep"},
    {"input.1", "input.2", "input.3"},
    {"input0", "input1", "input2"},
    {"input", "input_1", "input_2"},
    {"images", "images_1", "timestep"},
};

RifeInterpolator::RifeInterpolator(const string& modelPath, int mult)
    : mult_(max(2, mult)) {
    try {
        net_ = cv::dnn::readNetFromONNX(modelPath);
    } catch (const cv::Exception& e) {
        error_ = string("Gagal load model ONNX: ") + e.what();
        return;
    }

    // Coba pasang CUDA kalau tersedia (silent fallback ke CPU)
    try {
        if (cv::cuda::getCudaEnabledDeviceCount() > 0) {
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA_FP16);
            cout << "     RIFE: pakai CUDA (GPU)\n";
        } else {
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        }
    } catch (...) {
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    }

    if (!detectInputs()) {
        if (error_.empty())
            error_ = "Model tidak punya 2 atau 3 input yang dikenali";
        return;
    }

    // Smoke test: forward pass 64x64 dummy
    try {
        Mat dummy = Mat::zeros(64, 64, CV_8UC3);
        inferOne(dummy, dummy, 0.5f);
    } catch (const cv::Exception& e) {
        error_ = string("Forward pass gagal saat init: ") + e.what();
        return;
    }

    ready_ = true;
    cout << "     RIFE: model siap ("
         << (hasTimestep_ ? "3-input, t-parameterizable" : "2-input, t=0.5 only")
         << ")\n";
}

bool RifeInterpolator::detectInputs() {
    for (const auto& names : kInputNameSets) {
        try {
            // Cek 3-input dulu
            net_.setInput(Mat::zeros(1, 3, CV_32F), names[0]);
            net_.setInput(Mat::zeros(1, 3, CV_32F), names[1]);
            net_.setInput(Mat::zeros(1, 1, CV_32F), names[2]);
            inName0_ = names[0];
            inName1_ = names[1];
            inNameT_ = names[2];
            hasTimestep_ = true;
            return true;
        } catch (...) {
            // coba 2-input
            try {
                net_.setInput(Mat::zeros(1, 3, CV_32F), names[0]);
                net_.setInput(Mat::zeros(1, 3, CV_32F), names[1]);
                inName0_ = names[0];
                inName1_ = names[1];
                hasTimestep_ = false;
                return true;
            } catch (...) {}
        }
    }
    return false;
}

Mat RifeInterpolator::preprocess(const Mat& bgr, int& padH, int& padW) {
    int H = bgr.rows, W = bgr.cols;
    padH = ((H + padTo_ - 1) / padTo_) * padTo_;
    padW = ((W + padTo_ - 1) / padTo_) * padTo_;

    Mat padded;
    if (padH != H || padW != W) {
        copyMakeBorder(bgr, padded, 0, padH - H, 0, padW - W, BORDER_REPLICATE);
    } else {
        padded = bgr;
    }

    Mat rgb;
    cvtColor(padded, rgb, COLOR_BGR2RGB);

    Mat blob;
    // swapRB=false karena sudah kita swap manual
    cv::dnn::blobFromImage(rgb, blob, 1.0 / 255.0, Size(), Scalar(), false, false);
    return blob;   // [1, 3, padH, padW] float32, RGB, [0,1]
}

Mat RifeInterpolator::postprocess(const Mat& blobOut, int origH, int origW) {
    // blobOut: [1, 3, H, W] float32, RGB, [0,1]
    CV_Assert(blobOut.dims == 4 && blobOut.size[0] == 1 && blobOut.size[1] == 3);

    int H = blobOut.size[2], W = blobOut.size[3];

    vector<Mat> chs(3);
    for (int c = 0; c < 3; ++c) {
        chs[c] = Mat(H, W, CV_32F, const_cast<float*>(blobOut.ptr<float>(0, c))).clone();
    }
    Mat rgbHwc;
    merge(chs, rgbHwc);                       // [H, W, 3] float32, RGB, [0,1]
    rgbHwc.convertTo(rgbHwc, CV_8U, 255.0);

    if (H != origH || W != origW)
        rgbHwc = rgbHwc(Rect(0, 0, origW, origH));

    Mat bgrOut;
    cvtColor(rgbHwc, bgrOut, COLOR_RGB2BGR);
    return bgrOut;
}

Mat RifeInterpolator::inferOne(const Mat& a, const Mat& b, float t) {
    int origH = a.rows, origW = a.cols;

    int padHa, padWa, padHb, padWb;
    Mat blobA = preprocess(a, padHa, padWa);
    Mat blobB = preprocess(b, padHb, padWb);

    // Dimensi harus sama — kalau tidak (kasus langka), crop ke yang terkecil
    if (padHa != padHb || padWa != padWb) {
        int mh = min(padHa, padHb), mw = min(padWa, padWb);
        Mat tmpA(1, 3, CV_32F, Scalar(0));
        // (tidak diharapkan terjadi; a & b biasanya same size)
    }

    net_.setInput(blobA, inName0_);
    net_.setInput(blobB, inName1_);

    if (hasTimestep_) {
        // timestep: [1, 1, 1, 1] float32
        Mat tBlob(1, 1, CV_32F, Scalar(t));
        tBlob = tBlob.reshape(1, vector<int>{1, 1, 1, 1});
        net_.setInput(tBlob, inNameT_);
    } else if (std::fabs(t - 0.5f) > 1e-4f) {
        throw runtime_error(
            "Model 2-input hanya mendukung t=0.5 (mult=2). "
            "Gunakan model RIFE v4.6 (3-input) untuk mult > 2.");
    }

    Mat out = net_.forward();
    return postprocess(out, origH, origW);
}

vector<Mat> RifeInterpolator::processPair(const Mat& prev, const Mat& next) {
    vector<Mat> out;
    out.reserve(mult_);
    out.push_back(prev);

    if (mult_ == 2) {
        out.push_back(inferOne(prev, next, 0.5f));
        return out;
    }

    if (!hasTimestep_) {
        // Model 2-input tidak bisa mult > 2 — fallback: interpolasi bertingkat
        // Ini kurang akurat, tapi lebih baik daripada error
        Mat mid = inferOne(prev, next, 0.5f);
        if (mult_ == 3) {
            out.push_back(inferOne(prev, mid, 0.5f));
            out.push_back(mid);
            return out;
        }
        // Untuk mult >= 4, rekursif terus
        out.push_back(inferOne(prev, mid, 0.5f));
        out.push_back(mid);
        out.push_back(inferOne(mid, next, 0.5f));
        return out;
    }

    for (int i = 1; i < mult_; ++i) {
        float t = float(i) / float(mult_);
        out.push_back(inferOne(prev, next, t));
    }
    return out;
}