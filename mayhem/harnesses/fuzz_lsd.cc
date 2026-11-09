// LSDDetector: the multi-octave LSD line segment detector of the line_descriptor module.
#include "fuzz_common.h"
#include <opencv2/line_descriptor.hpp>
#include <vector>

using namespace cv;
using namespace cv::line_descriptor;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return -1;
    int octaves = 1 + data[0] % 3;
    Mat img;
    if (!parse_gray(data + 1, size - 1, img)) return -1;
    try {
        Ptr<LSDDetector> lsd = LSDDetector::createLSDDetector();
        std::vector<KeyLine> keylines;
        lsd->detect(img, keylines, 2, octaves);
    } catch (const cv::Exception &) {
    }
    return 0;
}
