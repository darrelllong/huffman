#include "code.h"
#include "crc16.h"
#include "endian.h"
#include "header.h"
#include "huffman.h"
#include "queue.h"
#include "sizes.h"
#include "stack.h"

#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ERROR(X) { fprintf(stderr, "%s\n", X); exit(EXIT_FAILURE); }

#ifndef HAVE_STRDUP
#define strdup(s) strdup_fallback(s)

static inline char *strdup_fallback(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (!copy) {
        perror("strdup malloc failed");
        exit(EXIT_FAILURE);
    }
    return memcpy(copy, s, len); // memcpy avoids redundant NULL checks
}
#endif

static int verbose = false;
static int print = false;

static treeNode *loadTree(uint8_t savedTree[], uint16_t treeBytes) {
    uint32_t count = 0;
    stack *s = newStack();
    if (s == NULL) {
        ERROR("Failed to allocate stack");
    }

    while (count < treeBytes) {
        if (savedTree[count] == 'L') {
            if (count + 1 >= treeBytes) {
                ERROR("Incorrect tree");
            }
            count += 1;
            treeNode *t = newNode(savedTree[count], true, 1);
            if (t == NULL) {
                ERROR("Failed to allocate tree node");
            }
            push(s, t);
        } else if (savedTree[count] == 'I') {
            treeNode *a = NULL, *b = NULL;

            if (emptyS(s)) // Right node
            {
                ERROR("Incorrect tree");
            } else {
                b = pop(s);
            }

            if (emptyS(s)) // Left node
            {
                ERROR("Incorrect tree");
            } else {
                a = pop(s);
            }

            treeNode *t = join(a, b);
            if (t == NULL) {
                ERROR("Failed to allocate tree node");
            }
            push(s, t);
        } else {
            ERROR("Incorrect tree");
        }
        count += 1;
    }

    treeNode *tmp = pop(s);
    if (tmp == NULL || !emptyS(s)) {
        ERROR("Incorrect tree");
    }
    delStack(s);
    return tmp;
}

static int8_t nextBit(int file) {
    // State required for nextBit

    static long length = 0;
    static int bitNo = 0;

    static uint8_t bytes[KB];

    int8_t bit;
    if (bitNo == 8 * length) {
        if ((length = read(file, bytes, KB)) > 0) {
            bitNo = 0;
        } else {
            return -1;
        } // We're done
    }
    bit = (bytes[bitNo / 8] >> (bitNo % 8)) & 0x1;
    bitNo += 1;
    return bit;
}

static void decodeFile(treeNode *root, int fileIn, int fileOut, uint64_t len) {
    treeNode *r = root;

    uint8_t buffer[BLK];
    uint32_t bP = 0;

    if (r) {
        int8_t b;

        // Do not decode extra bits (len)

        while (len > 0 && (b = nextBit(fileIn)) >= 0) {
            // Walk left on 0, right on 1.
            // This is safe because the root can never decode to a symbol.
            r = (b == 0) ? r->left : r->right;
            if (r == NULL) {
                ERROR("Incorrect tree");
            }

            // Emit symbol when leaf is reached, reset to root.
            if (r->leaf) {
                len -= 1;
                buffer[bP++] = r->symbol;
                if (bP == BLK) {
                    write(fileOut, buffer, bP);
                    bP = 0;
                }
                r = root;
            }
        }
    }
    if (bP != 0) // Remainder
    {
        write(fileOut, buffer, bP);
    }
    return;
}

int main(int argc, char **argv) {
    int fileIn = 0, fileOut = 1;
    char *inputFile = NULL, *outputFile = NULL;

    static struct option options[] = { { "input", required_argument, NULL, 'i' },
        { "output", required_argument, NULL, 'o' }, { "verbose", no_argument, &verbose, 'v' },
        { "print", no_argument, &print, 'p' }, { NULL, 0, NULL, 0 } };

    int c;
    while ((c = getopt_long(argc, argv, "-pvi:o:", options, NULL)) != -1) {
        switch (c) {
        case 'i': {
            inputFile = strdup(optarg);
            break;
        }
        case 'o': {
            outputFile = strdup(optarg);
            break;
        }
        case 'v': {
            verbose = true;
            break;
        }
        case 'p': {
            print = true;
            break;
        }
        }
    }

    if (inputFile) {
        if ((fileIn = open(inputFile, O_RDONLY)) < 0) {
            char s[KB] = { 0 };
            strncat(s, argv[0], sizeof(s) - strlen(s) - 1);
            strncat(s, ": ", sizeof(s) - strlen(s) - 1);
            strncat(s, inputFile, sizeof(s) - strlen(s) - 1);
            perror(s);
            exit(1);
        }
    } else {
        fileIn = STDIN_FILENO;
    }

    if (outputFile) {
        char s[KB] = { 0 };
        strncat(s, argv[0], sizeof(s) - strlen(s) - 1);
        strncat(s, ": ", sizeof(s) - strlen(s) - 1);
        strncat(s, outputFile, sizeof(s) - strlen(s) - 1);

        if ((fileOut = open(outputFile, O_CREAT | O_EXCL | O_RDWR | O_TRUNC, 0644)) < 0) {
            perror(s);
            exit(1);
        }
    } else {
        fileOut = STDOUT_FILENO;
    }

    // Read and validate header.
    Header h;
    if (read(fileIn, &h, sizeof(Header)) < (ssize_t) sizeof(Header)) {
        ERROR("Read of header failed");
    }

    uint32_t magic = isBig() ? swap32(h.magic) : h.magic;
    uint16_t treeBytes = isBig() ? swap16(h.tree_size) : h.tree_size;
    uint16_t permissions = isBig() ? swap16(h.permissions) : h.permissions;
    uint64_t origSize = isBig() ? swap64(h.file_size) : h.file_size;

    if (magic != MAGIC_V1 && magic != MAGIC_V2) {
        ERROR("Read of magic number failed");
    }

    if (magic == MAGIC_V2) {
        uint16_t headerCrcStored = 0;
        if (read(fileIn, &headerCrcStored, sizeof(headerCrcStored)) < (ssize_t) sizeof(headerCrcStored)) {
            ERROR("Read of header CRC failed");
        }
        headerCrcStored = isBig() ? swap16(headerCrcStored) : headerCrcStored;
        uint16_t headerCrcComputed = crc16_ccitt((uint8_t *) &h, sizeof(Header));
        if (headerCrcStored != headerCrcComputed) {
            // Do not trust metadata from a damaged header.
            // Keep decoding, but use read-only fallback permissions.
            permissions = 0444;
            fprintf(stderr, "Warning: header CRC mismatch, using safe fallback permissions 0444.\n");
        }
    }

    if (treeBytes < 5 || treeBytes > (3 * BYTE - 1) || treeBytes % 3 != 2) {
        ERROR("Incorrect tree");
    }

    if (fileOut != STDOUT_FILENO && fchmod(fileOut, permissions & 07777) == -1) {
        ERROR("Change of output file permissions failed");
    }

    uint8_t *savedTree = (uint8_t *) malloc(treeBytes);
    if (savedTree == NULL) {
        ERROR("Allocation of tree failed");
    }

    // Pipes require this since they may return less than requested

    long readSz = 0, sum = 0;
    do {
        readSz = read(fileIn, savedTree + sum, treeBytes - sum);
        sum += readSz;
    } while (sum < treeBytes && readSz > 0);

    if (sum < treeBytes) {
        ERROR("Read of tree failed");
    }

    // Build a new tree

    treeNode *t = loadTree(savedTree, treeBytes);
    free(savedTree);
    if (t == NULL) {
        ERROR("Loading tree failed");
    }

    if (verbose) {
        fprintf(stderr, "Original %" PRIu64 " bits: ", origSize * 8);
        fprintf(stderr, "tree (%u)\n", treeBytes);
    }

    // Decode to the original content

    decodeFile(t, fileIn, fileOut, origSize);

    if (print) {
        printTree(t, 0);
    }

    close(fileIn);
    close(fileOut);
    delTree(t);
    exit(EXIT_SUCCESS);
}
