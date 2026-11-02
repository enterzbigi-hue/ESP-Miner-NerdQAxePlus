#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
namespace HashCapture {
inline bool isEnabled() { return true; }
inline std::vector<uint8_t> capturedCoinbase;
inline size_t capturedOffset, capturedLength;
inline uint32_t inputs(const uint8_t *p, size_t n, const uint8_t (*)[32], size_t, size_t offset, size_t length) {
    capturedCoinbase.assign(p, p+n); capturedOffset=offset; capturedLength=length; return 42;
}
}
