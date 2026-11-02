#pragma once
#include <stdint.h>
#include <string.h>

// All arithmetic uses big-endian byte arrays, never a narrowed machine integer.
namespace ExtranonceRange {
constexpr int MAX_BYTES = 32;
constexpr int STEPS = 20;
inline void decrement(uint8_t *value, int bytes) {
    for (int i = bytes - 1; i >= 0; --i) if (value[i]-- != 0) break;
}
inline void boundaryBits(int bits, int basisPoints, uint8_t *out) {
    uint8_t wide[MAX_BYTES + 1]{};
    unsigned remainder = 0;
    const int whole = bits / 8, partial = bits % 8;
    for (int i = 0; i <= whole; ++i) {
        unsigned value = remainder * 256 + (i == 0 ? unsigned(basisPoints) << partial : 0);
        wide[i] = value / 10000;
        remainder = value % 10000;
    }
    memcpy(out, wide + (partial ? 0 : 1), (bits + 7) / 8);
}
inline bool boundsBits(int bits, int step, uint8_t *low, uint8_t *high, int start = 10000) {
    if (bits < 1 || bits > 256) return false;
    const int bytes = (bits + 7) / 8;
    const int upper = start - step * 500;
    if (upper <= 0 || upper > 10000) return false;
    boundaryBits(bits, upper > 500 ? upper - 500 : 0, low);
    boundaryBits(bits, upper, high);
    if (upper < 10000 && !memcmp(low, high, bytes)) return false;
    decrement(high, bytes);
    return true;
}
inline bool bounds(int bytes, int step, uint8_t *low, uint8_t *high, int start = 10000) {
    return boundsBits(bytes * 8, step, low, high, start);
}
inline void percentage(int bits, unsigned basisPoints, uint8_t *out) {
    const int bytes = (bits + 7) / 8;
    uint8_t product[MAX_BYTES + 2]{};
    unsigned carry = 0;
    for (int i = bytes + 1; i >= 2; --i) {
        unsigned digit = i == 2 && bits % 8 ? (1u << (bits % 8)) - 1 : 255;
        unsigned v = digit * basisPoints + carry;
        product[i] = v & 255;
        carry = v >> 8;
    }
    product[1] = carry & 255; product[0] = carry >> 8;
    unsigned rem = 0;
    for (int i = 0; i < bytes + 2; ++i) {
        unsigned v = rem * 256 + product[i];
        product[i] = v / 10000; rem = v % 10000;
    }
    memcpy(out, product + 2, bytes);
}
inline void hex(const uint8_t *value, int bytes, char *out) {
    constexpr char digits[] = "0123456789ABCDEF";
    for (int i = 0; i < bytes; ++i) {
        out[2*i] = digits[value[i] >> 4];
        out[2*i+1] = digits[value[i] & 15];
    }
    out[bytes*2] = 0;
}
inline int digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
inline bool validHex(const char *text, int bytes) {
    if (!text || bytes < 1 || bytes > MAX_BYTES || strlen(text) != size_t(bytes * 2)) return false;
    for (int i = 0; i < bytes * 2; ++i) if (digit(text[i]) < 0) return false;
    return true;
}
inline bool decode(const char *text, int bytes, uint8_t *out) {
    if (!validHex(text, bytes)) return false;
    for (int i = 0; i < bytes; ++i) out[i] = (digit(text[i*2]) << 4) | digit(text[i*2+1]);
    return true;
}
}
