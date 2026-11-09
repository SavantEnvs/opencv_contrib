// Known-answer probe for the contrib modules the harnesses fuzz, built with normal flags (no sanitizers).
// Prints one "name=value" line per probe; mayhem/test.sh compares each line with its expected value.
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/img_hash.hpp>
#include <opencv2/line_descriptor.hpp>
#include <opencv2/wechat_qrcode.hpp>
#include <opencv2/ximgproc.hpp>
#include <cstdio>
#include <string>
#include <vector>

using namespace cv;

static Mat gradient() {
    Mat m(32, 32, CV_8UC1);
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) m.at<uchar>(y, x) = (uchar)(x * 8 + (y % 4));
    return m;
}

static Mat rectangleImage() {
    Mat m(64, 64, CV_8UC1, Scalar(0));
    rectangle(m, Point(12, 16), Point(50, 46), Scalar(255), FILLED);
    return m;
}

static std::string hex(const Mat &h) {
    std::string s;
    char b[3];
    for (int i = 0; i < (int)h.total() * (int)h.elemSize(); i++) {
        snprintf(b, sizeof b, "%02x", h.data[i]);
        s += b;
    }
    return s;
}

int main() {
    Mat g = gradient();
    Mat h;
    img_hash::AverageHash::create()->compute(g, h);
    printf("average_hash=%s\n", hex(h).c_str());
    img_hash::PHash::create()->compute(g, h);
    printf("phash=%s\n", hex(h).c_str());
    Mat flipped, h2;
    flip(g, flipped, 1);
    Ptr<img_hash::ImgHashBase> ah = img_hash::AverageHash::create();
    ah->compute(g, h);
    ah->compute(flipped, h2);
    printf("average_hash_compare_mirrored=%g\n", ah->compare(h, h2));

    Mat r = rectangleImage();
    Mat thin;
    ximgproc::thinning(r, thin, ximgproc::THINNING_ZHANGSUEN);
    printf("thinning_nonzero=%d\n", countNonZero(thin));

    Ptr<line_descriptor::BinaryDescriptor> bd = line_descriptor::BinaryDescriptor::createBinaryDescriptor();
    std::vector<line_descriptor::KeyLine> kl;
    Mat big_r(160, 160, CV_8UC1, Scalar(20));
    rectangle(big_r, Point(30, 40), Point(120, 110), Scalar(220), FILLED);
    line(big_r, Point(10, 150), Point(150, 130), Scalar(255), 3);
    GaussianBlur(big_r, big_r, Size(3, 3), 0.8);
    bd->detect(big_r, kl);
    printf("edlines_keylines=%zu\n", kl.size());

    Ptr<ximgproc::FastLineDetector> fld = ximgproc::createFastLineDetector();
    std::vector<Vec4f> lines;
    fld->detect(r, lines);
    printf("fld_lines=%zu\n", lines.size());

    Ptr<QRCodeEncoder> enc = QRCodeEncoder::create();
    Mat qr;
    enc->encode("mayhem-opencv", qr);
    Mat big;
    copyMakeBorder(qr, qr, 4, 4, 4, 4, BORDER_CONSTANT, Scalar(255));
    resize(qr, big, Size(), 6, 6, INTER_NEAREST);
    wechat_qrcode::WeChatQRCode wq;
    std::vector<Mat> pts;
    std::vector<std::string> out = wq.detectAndDecode(big, pts);
    printf("wechat_decoded=%s\n", out.empty() ? "<none>" : out[0].c_str());
    return 0;
}
