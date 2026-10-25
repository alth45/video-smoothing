#include "flow_interpolator.hpp"

using namespace cv;

FlowInterpolator::FlowInterpolator(int mult, float smoothSigma, bool useFarneback)
    : mult_(mult), smoothSigma_(smoothSigma), useFarneback_(useFarneback) {}

std::vector<Mat> FlowInterpolator::processPair(const Mat& prev, const Mat& next) {
    Mat f_ab, f_ba;
    computeFlows(prev, next, f_ab, f_ba);

    if (smoothSigma_ > 0.0f) {
        GaussianBlur(f_ab, f_ab, Size(0, 0), smoothSigma_);
        GaussianBlur(f_ba, f_ba, Size(0, 0), smoothSigma_);
    }

    std::vector<Mat> out;
    out.reserve(mult_);
    out.push_back(prev);
    for (int i = 1; i < mult_; ++i)
        out.push_back(interpAt(prev, next, f_ab, f_ba,
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

Mat FlowInterpolator::interpAt(const Mat& prev, const Mat& next,
                               const Mat& f_ab, const Mat& f_ba, float t) {
    Mat wp = warpWithFlow(prev, f_ab, t);
    Mat wn = warpWithFlow(next, f_ba, 1.0f - t);
    Mat result;
    addWeighted(wp, 1.0f - t, wn, t, 0.0, result);
    return result;
}