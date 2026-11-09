// Known-answer probe for the contrib modules the harnesses fuzz, built with normal flags (no sanitizers).
// Prints one "name=value" line per probe; mayhem/test.sh compares each line with its expected value.
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/line_descriptor.hpp>
#include <cstdio>
#include <string>
#include <vector>

using namespace cv;

int main() {
    Ptr<line_descriptor::BinaryDescriptor> bd = line_descriptor::BinaryDescriptor::createBinaryDescriptor();
    std::vector<line_descriptor::KeyLine> kl;
    Mat big_r(160, 160, CV_8UC1, Scalar(20));
    rectangle(big_r, Point(30, 40), Point(120, 110), Scalar(220), FILLED);
    line(big_r, Point(10, 150), Point(150, 130), Scalar(255), 3);
    GaussianBlur(big_r, big_r, Size(3, 3), 0.8);
    bd->detect(big_r, kl);
    printf("edlines_keylines=%zu\n", kl.size());

    return 0;
}
