// Shared input decoding for the opencv_contrib harnesses. The fuzzer's bytes ARE the pixels (no file
// I/O, no codec): byte 0 picks the width (8..64) and the rest is a row-major 8-bit grayscale image whose
// height is whatever the remaining length allows (at least 8 rows). Anything shorter is rejected.
#pragma once
#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>
#include <cstddef>
#include <cstdint>

// Run OpenCV single-threaded so each input is reproducible and crashes land on the fuzzing thread.
static const int single_threaded = (cv::setNumThreads(0), 0);

static inline bool parse_gray(const uint8_t *data, size_t size, cv::Mat &img) {
    if (size < 1) return false;
    int w = 8 + data[0] % 57;
    size_t px = size - 1;
    int h = (int)(px / (size_t)w);
    if (h < 8) return false;
    img = cv::Mat(h, w, CV_8UC1, const_cast<uint8_t *>(data + 1)).clone();
    return true;
}
