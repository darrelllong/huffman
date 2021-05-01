#pragma once

#include "sizes.h"

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

typedef struct code {
    uint8_t bits[CODE / 8];
    uint32_t l;
} code;

// We are just initializing, the actual code will be allocated on the stack
// and copied upon assignment which is exactly the behavior that we want.

static inline code newCode(void) {
    code t;
    for (int i = 0; i < CODE / 8; i += 1) {
        t.bits[i] = 0;
    }
    t.l = 0;
    return t;
}

static inline bool pushCode(code *c, uint32_t k) {
    if (c->l == CODE) {
        return false;
    } else if (k == 0) {
        c->bits[c->l / 8] &= ~(0x1 << (c->l % 8)); // Push 0
        c->l += 1;
    } else {
        c->bits[c->l / 8] |= (0x1 << (c->l % 8)); // Push 1
        c->l += 1;
    }
    return true;
}

static inline bool popCode(code *c, uint32_t *k) {
    if (c->l == 0) {
        return false;
    } else {
        c->l -= 1;
        *k = ((0x1 << (c->l % 8)) & c->bits[c->l / 8]) >> (c->l % 8);
        return true;
    }
}

static inline bool emptyCode(code *c) {
    return c->l == 0;
}

static inline bool fullCode(code *c) {
    return c->l == CODE;
}

// State for writing the code to a file efficiently. codeB is a buffer of bits
// to be written. codeP is a pointer to the end of the buffer, and codeC is a
// count of the total number of code bits.

static uint8_t codeB[KB];
static uint32_t codeP = 0;
static uint64_t codeC = 0;

// flushCode will write any code bytes that have not already been written.

static inline void flushCode(int file) {
    write(file, codeB, codeP / 8 + 1);
    return;
}

// appendCode is a bit tricky: append the bits from codes into a buffer. If
// the buffer fills, then write it and continue appending code bits (if there
// are any left in the current code).

static inline void appendCode(int file, code c) {
    codeC += c.l;
    for (uint32_t i = 0; i < c.l; i += 1) {
        if (c.bits[i / 8] & (0x1 << (i % 8))) // Bit set?
        {
            codeB[codeP / 8] |= (0x1 << (codeP % 8)); // Append 1
        } else {
            codeB[codeP / 8] &= ~(0x1 << (codeP % 8)); // Append 0
        }

        codeP += 1;
        if (codeP == KB * 8) { // Flush if the buffer is full
            write(file, codeB, KB);
            codeP = 0;
        }
    }
    return;
}
