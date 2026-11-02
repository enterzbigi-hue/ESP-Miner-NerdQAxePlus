#pragma once
#include <stdint.h>
#include <string.h>

// Bitcoin wire serialization, including the internal-order hash bytes.
inline void write_header_le32(uint8_t *out, uint32_t value) {
    for (int i = 0; i < 4; ++i) out[i] = uint8_t(value >> (8 * i));
}
inline void serialize_block_header(uint8_t out[80], uint32_t version,
    const uint8_t prev[32], const uint8_t merkle[32], uint32_t ntime,
    uint32_t nbits, uint32_t nonce) {
    write_header_le32(out, version);
    memcpy(out + 4, prev, 32);
    memcpy(out + 36, merkle, 32);
    write_header_le32(out + 68, ntime);
    write_header_le32(out + 72, nbits);
    write_header_le32(out + 76, nonce);
}
