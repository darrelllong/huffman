#pragma once

#include <stdint.h>

typedef struct Header {
    uint32_t magic;
    uint16_t permissions;
    uint16_t tree_size;
    uint64_t file_size;
} Header;
