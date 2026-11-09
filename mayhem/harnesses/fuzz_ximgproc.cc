// A selection of ximgproc filters, segmenters and line/edge detectors over a fuzzer-supplied image.
// Byte 0 selects the operation, byte 1 is a parameter byte, the rest is the grayscale image
// (see fuzz_common.h).
#include "fuzz_common.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/ximgproc.hpp>
#include <vector>

using namespace cv;
using namespace cv::ximgproc;

static void run(int op, int p, const Mat &gray) {
    Mat bgr, dst;
    cvtColor(gray, bgr, COLOR_GRAY2BGR);
    switch (op) {
    case 0:
        thinning(gray, dst, p & 1 ? THINNING_GUOHALL : THINNING_ZHANGSUEN);
        break;
    case 1:
        niBlackThreshold(gray, dst, 255, THRESH_BINARY, 3 + 2 * (p % 8), (p - 128) / 64.0, p >> 5 & 3);
        break;
    case 2:
        guidedFilter(gray, gray, dst, 1 + p % 8, 100.0 + p);
        break;
    case 3:
        fastGlobalSmootherFilter(gray, gray, dst, 100.0 + p, 5.0);
        break;
    case 4: {
        Ptr<FastLineDetector> d = createFastLineDetector(5 + p % 16, 1.5f, 50.0, 50.0, 3, p & 1);
        std::vector<Vec4f> lines;
        d->detect(gray, lines);
        d->drawSegments(dst = bgr.clone(), lines);
        break;
    }
    case 5: {
        Ptr<EdgeDrawing> ed = createEdgeDrawing();
        ed->params.EdgeDetectionOperator = p & 3;
        ed->params.MinPathLength = 5 + p % 20;
        ed->params.GradientThresholdValue = 10 + p % 40;
        ed->params.AnchorThresholdValue = 4 + p % 16;
        ed->detectEdges(gray);
        std::vector<Vec4f> lines;
        ed->detectLines(lines);
        std::vector<Vec6d> ellipses;
        ed->detectEllipses(ellipses);
        break;
    }
    case 6: {
        Ptr<SuperpixelSLIC> s = createSuperpixelSLIC(bgr, SLIC + p % 3, 4 + p % 12, 10.0f);
        s->iterate(3);
        s->enforceLabelConnectivity();
        s->getLabels(dst);
        break;
    }
    case 7: {
        Ptr<SuperpixelLSC> s = createSuperpixelLSC(bgr, 4 + p % 12, 0.075f);
        s->iterate(3);
        s->enforceLabelConnectivity();
        s->getLabels(dst);
        break;
    }
    case 8: {
        Ptr<SuperpixelSEEDS> s = createSuperpixelSEEDS(bgr.cols, bgr.rows, 3, 2 + p % 40, 1 + p % 5, 2 + p % 8, p & 1);
        s->iterate(bgr, 3);
        s->getLabels(dst);
        break;
    }
    case 9:
        l0Smooth(bgr, dst, 0.01 + p / 255.0, 2.0);
        break;
    case 10:
        weightedMedianFilter(gray, gray, dst, 1 + p % 8, 25.5, p % 5);
        break;
    case 11:
        edgePreservingFilter(bgr, dst, 3 + 2 * (p % 4), 10.0 + p);
        break;
    case 12:
        FastHoughTransform(gray, dst, CV_32S, ARO_315_135 + p % 4, FHT_ADD + p % 4, HDO_DESKEW + (p >> 2 & 1));
        break;
    case 13:
        GradientDericheX(gray, dst, 0.5 + p / 64.0, 0.001 + p / 255.0);
        break;
    case 14:
        anisotropicDiffusion(bgr, dst, 0.1f + p / 512.0f, 1.0f + p % 16, 1 + p % 6);
        break;
    case 15:
        amFilter(gray, gray, dst, 1.0 + p % 32, 0.1 + p / 64.0, p & 1);
        break;
    case 16:
        rollingGuidanceFilter(gray, dst, -1, 25.5 + p % 16, 3.0, 1 + p % 4);
        break;
    case 17:
        bilateralTextureFilter(bgr, dst, 1 + p % 4, 1, 0.5 + p / 64.0, -1);
        break;
    case 18:
        findEllipses(gray, dst, 0.5f + p / 512.0f, 0.5f, 0.3f);
        break;
    case 19: {
        Ptr<RidgeDetectionFilter> rd = RidgeDetectionFilter::create(CV_32FC1, 1, 1, 3 + 2 * (p % 3));
        rd->getRidgeFilteredImage(gray, dst);
        break;
    }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 3) return -1;
    int op = data[0] % 20;
    int p = data[1];
    Mat gray;
    if (!parse_gray(data + 2, size - 2, gray)) return -1;
    try {
        run(op, p, gray);
    } catch (const cv::Exception &) {
    }
    return 0;
}
