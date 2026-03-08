#pragma once

#include <stdint.h>

#define MAGIC_V1 0xBEEFD00D
#define MAGIC_V2 0xBEEFD00E
#define MAGIC MAGIC_V2

typedef struct Header {
    uint32_t magic;
    uint16_t permissions;
    uint16_t tree_size;
    uint64_t file_size;
} Header;

_Static_assert(sizeof(Header) == 16, "Header must remain 16 bytes");
