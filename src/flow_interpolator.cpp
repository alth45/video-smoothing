#include "flow_interpolator.hpp"

#include <cmath>
#include <vector>

using namespace cv;

FlowInterpolator::FlowInterpolator(int mult, float smoothSigma, bool useFarneback,
                                   bool useOcclusion, float occlusionThresh)
    : mult_(mult), smoothSigma_(smoothSigma), useFarneback_(useFarneback),
      useOcclusion_(useOcclusion), occlusionThresh_(occlusionThresh) {}

std::vector<Mat> FlowInterpolator::processPair(const Mat& prev, const Mat& next) {
    Mat f_ab, f_ba;
    computeFlows(prev, next, f_ab, f_ba);

    if (smoothSigma_ > 0.0f) {
        GaussianBlur(f_ab, f_ab, Size(0, 0), smoothSigma_);
        GaussianBlur(f_ba, f_ba, Size(0, 0), smoothSigma_);
    }

    Mat maskPrev, maskNext;
    if (useOcclusion_) {
        maskPrev = computeOcclusionMask(f_ab, f_ba, occlusionThresh_);
        maskNext = computeOcclusionMask(f_ba, f_ab, occlusionThresh_);

        // statistik: rata-rata dari dua mask
        int totalPx = maskPrev.rows * maskPrev.cols;
        int occP = totalPx - countNonZero(maskPrev);
        int occN = totalPx - countNonZero(maskNext);
        lastOcclusionRatio_ = float(occP + occN) / float(totalPx * 2);
    } else {
        lastOcclusionRatio_ = 0.0f;
    }

    std::vector<Mat> out;
    out.reserve(mult_);
    out.push_back(prev);
    for (int i = 1; i < mult_; ++i)
        out.push_back(interpAt(prev, next, f_ab, f_ba, maskPrev, maskNext,
                               float(i) / float(mult_)));
    return out;
}

void FlowInterpolator::computeFlows(const Mat& a, const Mat& b,
                                    Mat& f_ab, Mat& f_ba) {
    Mat ga, gb;
    cvtColor(a, ga, COLOR_BGR2GRAY);
    cvtColor(b, gb, COLOR_BGR2GRAY);

    if (useFarneback_) {
        calcOpticalFlowFarneback(ga, gb, f_ab, 0.5, 5, 21, 3, 7, 1.5, 0);
        calcOpticalFlowFarneback(gb, ga, f_ba, 0.5, 5, 21, 3, 7, 1.5, 0);
    } else {
        auto dis = DISOpticalFlow::create(DISOpticalFlow::PRESET_MEDIUM);
        dis->calc(ga, gb, f_ab);
        dis->calc(gb, ga, f_ba);
    }
}

Mat FlowInterpolator::warpWithFlow(const Mat& img, const Mat& flow, float scale) {
    Mat mapX(img.size(), CV_32F), mapY(img.size(), CV_32F);
    for (int y = 0; y < img.rows; ++y) {
        const Point2f* frow = flow.ptr<Point2f>(y);
        float* mx = mapX.ptr<float>(y);
        float* my = mapY.ptr<float>(y);
        for (int x = 0; x < img.cols; ++x) {
            mx[x] = x - scale * frow[x].x;
            my[x] = y - scale * frow[x].y;
        }
    }
    Mat out;
    remap(img, out, mapX, mapY, INTER_LINEAR, BORDER_REPLICATE);
    return out;
}

Mat FlowInterpolator::warpMaskWithFlow(const Mat& mask, const Mat& flow, float scale) {
    Mat mapX(mask.size(), CV_32F), mapY(mask.size(), CV_32F);
    for (int y = 0; y < mask.rows; ++y) {
        const Point2f* frow = flow.ptr<Point2f>(y);
        float* mx = mapX.ptr<float>(y);
        float* my = mapY.ptr<float>(y);
        for (int x = 0; x < mask.cols; ++x) {
            mx[x] = x - scale * frow[x].x;
            my[x] = y - scale * frow[x].y;
        }
    }
    Mat out;
    // INTER_LINEAR: tepi mask jadi soft (255 → 128 → 0), blending jadi mulus
    // BORDER_CONSTANT 0: piksel di luar frame = tidak terlihat = unreliable
    remap(mask, out, mapX, mapY, INTER_LINEAR, BORDER_CONSTANT, Scalar(0));
    return out;
}

// Forward-backward consistency check.
// Untuk tiap pixel (x,y) di A:
//   f_ab(x,y) + f_ba(x + f_ab(x,y))  harus ≈ 0 kalau flow konsisten.
// Kalau magnitude-nya > thresh (piksel) → pixel unreliable (occluded).
Mat FlowInterpolator::computeOcclusionMask(const Mat& f_ab, const Mat& f_ba,
                                            float thresh) {
    Mat mask(f_ab.size(), CV_8U, Scalar(255));
    const int W = f_ab.cols, H = f_ab.rows;

    for (int y = 0; y < H; ++y) {
        const Point2f* frow = f_ab.ptr<Point2f>(y);
        uchar* mrow = mask.ptr<uchar>(y);
        for (int x = 0; x < W; ++x) {
            Point2f v = frow[x];
            float bx = x + v.x;
            float by = y + v.y;

            // out of bounds → tidak ada pasangan di frame lain
            if (bx < 1.f || bx >= W - 2.f || by < 1.f || by >= H - 2.f) {
                mrow[x] = 0;
                continue;
            }

            // bilinear sample f_ba di (bx, by)
            int x0 = (int)bx, y0 = (int)by;
            int x1 = x0 + 1, y1 = y0 + 1;
            float fx = bx - x0, fy = by - y0;

            Point2f b00 = f_ba.at<Point2f>(y0, x0);
            Point2f b01 = f_ba.at<Point2f>(y0, x1);
            Point2f b10 = f_ba.at<Point2f>(y1, x0);
            Point2f b11 = f_ba.at<Point2f>(y1, x1);

            Point2f b;
            b.x = (1 - fy) * ((1 - fx) * b00.x + fx * b01.x)
                + fy       * ((1 - fx) * b10.x + fx * b11.x);
            b.y = (1 - fy) * ((1 - fx) * b00.y + fx * b01.y)
                + fy       * ((1 - fx) * b10.y + fx * b11.y);

            float ex = v.x + b.x;
            float ey = v.y + b.y;
            float err = std::sqrt(ex * ex + ey * ey);
            mrow[x] = (err < thresh) ? 255 : 0;
        }
    }
    return mask;
}

Mat FlowInterpolator::interpAt(const Mat& prev, const Mat& next,
                               const Mat& f_ab, const Mat& f_ba,
                               const Mat& maskPrev, const Mat& maskNext,
                               float t) {
    Mat wp = warpWithFlow(prev, f_ab, t);
    Mat wn = warpWithFlow(next, f_ba, 1.0f - t);

    // Tidak pakai occlusion → perilaku lama
    if (maskPrev.empty() || maskNext.empty()) {
        Mat result;
        addWeighted(wp, 1.0f - t, wn, t, 0.0, result);
        return result;
    }

    // Occlusion ON: hitung weight adaptif per pixel
    Mat mp_w = warpMaskWithFlow(maskPrev, f_ab, t);
    Mat mn_w = warpMaskWithFlow(maskNext, f_ba, 1.0f - t);

    Mat mpf, mnf;
    mp_w.convertTo(mpf, CV_32F, 1.0 / 255.0);   // [0,1]
    mn_w.convertTo(mnf, CV_32F, 1.0 / 255.0);

    // Weight dasar * reliability + epsilon supaya tidak pernah 0 mutlak
    Mat w_p = mpf * (1.0 - t) + 1e-3;
    Mat w_n = mnf * t         + 1e-3;

    Mat w_sum = w_p + w_n;
    Mat wp_n  = w_p / w_sum;
    Mat wn_n  = w_n / w_sum;

    // 3-channel weight
    std::vector<Mat> ch_p = {wp_n, wp_n, wp_n};
    std::vector<Mat> ch_n = {wn_n, wn_n, wn_n};
    Mat wp3, wn3;
    merge(ch_p, wp3);
    merge(ch_n, wn3);

    Mat wp_f, wn_f;
    wp.convertTo(wp_f, CV_32FC3);
    wn.convertTo(wn_f, CV_32FC3);

    Mat result32 = wp_f.mul(wp3) + wn_f.mul(wn3);
    Mat result;
    result32.convertTo(result, CV_8UC3);
    return result;
}