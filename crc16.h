#pragma once

#include <stddef.h>
#include <stdint.h>

// CRC-16/CCITT-FALSE for protecting the serialized header.
//   Polynomial: 0x1021
//   Initial value: 0xFFFF
//   No reflection, no xorout
static inline uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i += 1) {
        crc ^= (uint16_t) data[i] << 8;
        for (int bit = 0; bit < 8; bit += 1) {
            if (crc & 0x8000) {
                crc = (uint16_t) ((crc << 1) ^ 0x1021);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
