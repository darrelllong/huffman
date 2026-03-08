#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

static inline bool io_write_full(int file, const uint8_t *buf, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(file, buf + written, len - written);
        if (n <= 0) {
            return false;
        }
        written += (size_t) n;
    }
    return true;
}

static inline bool io_read_full(int file, uint8_t *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(file, buf + total, len - total);
        if (n <= 0) {
            return false;
        }
        total += (size_t) n;
    }
    return true;
}
