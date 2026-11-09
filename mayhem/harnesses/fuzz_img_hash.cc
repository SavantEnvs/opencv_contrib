// All img_hash perceptual hashers over a fuzzer-supplied image, plus compare() against the hash of the
// mirrored image.
#include "fuzz_common.h"
#include <opencv2/img_hash.hpp>
#include <opencv2/imgproc.hpp>

using namespace cv;
using namespace cv::img_hash;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    Mat gray;
    if (!parse_gray(data, size, gray)) return -1;
    Mat bgr, flipped, flippedBgr;
    cvtColor(gray, bgr, COLOR_GRAY2BGR);
    flip(gray, flipped, 1);
    flip(bgr, flippedBgr, 1);
    try {
        Ptr<ImgHashBase> hashers[] = {
            AverageHash::create(),
            BlockMeanHash::create(BLOCK_MEAN_HASH_MODE_0),
            BlockMeanHash::create(BLOCK_MEAN_HASH_MODE_1),
            MarrHildrethHash::create(),
            PHash::create(),
            RadialVarianceHash::create(),
        };
        for (auto &h : hashers) {
            Mat a, b;
            h->compute(gray, a);
            h->compute(flipped, b);
            h->compare(a, b);
            h->compare(a, a);
        }
        Ptr<ImgHashBase> cm = ColorMomentHash::create();
        Mat a, b;
        cm->compute(bgr, a);
        cm->compute(flippedBgr, b);
        cm->compare(a, b);
    } catch (const cv::Exception &) {
    }
    return 0;
}
