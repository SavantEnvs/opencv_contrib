// BinaryDescriptor (EDLines line detector + binary line descriptors) and the Mihasher-based matcher
// over a fuzzer-supplied grayscale image.
#include "fuzz_common.h"
#include <opencv2/line_descriptor.hpp>
#include <vector>

using namespace cv;
using namespace cv::line_descriptor;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    Mat img;
    if (!parse_gray(data, size, img)) return -1;
    try {
        Ptr<BinaryDescriptor> bd = BinaryDescriptor::createBinaryDescriptor();
        std::vector<KeyLine> keylines;
        bd->detect(img, keylines);
        if (keylines.empty()) return 0;
        Mat descr;
        bd->compute(img, keylines, descr);
        if (descr.empty()) return 0;
        Ptr<BinaryDescriptorMatcher> matcher = BinaryDescriptorMatcher::createBinaryDescriptorMatcher();
        std::vector<DMatch> matches;
        matcher->match(descr, descr, matches);
    } catch (const cv::Exception &) {
    }
    return 0;
}
