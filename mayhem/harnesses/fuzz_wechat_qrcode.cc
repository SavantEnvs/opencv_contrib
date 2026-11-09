// WeChat QR code detector + zxing-derived decoder, fed a rendered module grid.
// Byte 0: (low nibble % 10) picks the QR version-like side (21+4*v modules), bits 4-5 the pixel scale (3..6).
// The following ceil(n*n/8) bytes are the module bits (1 = dark), drawn row-major on a white canvas with a
// 4-module quiet zone. No models are loaded, so only the classical detector/decoder path runs.
#include "fuzz_common.h"
#include <opencv2/wechat_qrcode.hpp>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return -1;
    int n = 21 + 4 * ((data[0] & 0x0f) % 10);
    int scale = 3 + ((data[0] >> 4) & 3);
    size_t need = ((size_t)n * n + 7) / 8;
    if (size - 1 < need) return -1;
    const int quiet = 4;
    int side = (n + 2 * quiet) * scale;
    cv::Mat img(side, side, CV_8UC1, cv::Scalar(255));
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            size_t bit = (size_t)y * n + x;
            if ((data[1 + bit / 8] >> (bit % 8)) & 1) {
                img(cv::Rect((x + quiet) * scale, (y + quiet) * scale, scale, scale)).setTo(0);
            }
        }
    }
    try {
        cv::wechat_qrcode::WeChatQRCode detector;
        std::vector<cv::Mat> points;
        detector.detectAndDecode(img, points);
    } catch (const cv::Exception &) {
    }
    return 0;
}
