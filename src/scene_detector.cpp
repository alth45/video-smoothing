#include "scene_detector.hpp"

using namespace cv;

double sceneDiff(const Mat& a, const Mat& b) {
    Mat ga, gb;
    cvtColor(a, ga, COLOR_BGR2GRAY);
    cvtColor(b, gb, COLOR_BGR2GRAY);

    const int histSize = 64;
    float range[] = {0, 256};
    const float* ranges[] = {range};
    int channels[] = {0};

    Mat ha, hb;
    calcHist(&ga, 1, channels, noArray(), ha, 1, &histSize, ranges);
    calcHist(&gb, 1, channels, noArray(), hb, 1, &histSize, ranges);

    normalize(ha, ha, 0, 1, NORM_L1);
    normalize(hb, hb, 0, 1, NORM_L1);

    return compareHist(ha, hb, HISTCMP_CORREL);
}