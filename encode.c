#include "code.h"
#include "endian.h"
#include "header.h"
#include "huffman.h"
#include "queue.h"
#include "sizes.h"

#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define strdup(s) strcpy(malloc(strlen(s) + 1), s)

static int verbose = false;
static int print = false;
static int fullTree = false;

static uint32_t magicNumber = MAGIC;
static uint16_t leaves = 0;
static uint16_t treeBytes;

// A temporary file, since mkstemp() is not ANSI.

int Mymktemp() {
    char tmpFile[KB];

    snprintf(tmpFile, KB, "/tmp/encode.%u.%ld", getpid(), time(0));

    int fileOut = open(tmpFile, O_CREAT | O_EXCL | O_RDWR | O_TRUNC, 0644);
    unlink(tmpFile);
    return fileOut;
}

// Count the number of occurences of each symbol in the file.

static uint8_t freqCnt(int file, uint64_t hist[]) {
    uint8_t b[KB] = { 0 };
    uint8_t unique = 0;

    lseek(file, 0, SEEK_SET); // Start of the file

    long count;
    while ((count = read(file, b, KB)) > 0) {
        for (int i = 0; i < count; i += 1) {
            if (hist[b[i]] == 0) {
                unique += 1;
            }
            hist[b[i]] += 1;
        }
    }

    return unique;
}

bool buffered_write(int file, uint8_t b[], uint32_t l, bool flush) {
    static char buffer[BLK];
    static int blkP = 0;
    for (uint32_t i = 0; i < l; i += 1) {
        buffer[blkP] = b[i];
        blkP += 1;
        if (blkP == BLK) { // Buffer is full
            if (write(file, buffer, BLK) < BLK) { // Clear the buffer
                return false; // write failed
            }
            blkP = 0;
        }
    }
    if (flush && blkP > 0) { // Flush the buffer
        if (write(file, buffer, blkP) < blkP) {
            return false; // write failed
        }
        blkP = 0;
    }
    return true;
}

static treeNode *buildTree(int inFile, int outFile, Header *h) {
    uint64_t hist[BYTE] = { 0 };

    uint8_t unique = freqCnt(inFile, hist);

    queue *q = newQueue(BYTE + 1);

    // The tree must have at least two symbols in order to be valid. We
    // could special-case a zero symbol tree, but that is a waste of code
    // for a single case.

    if (unique == 0) // Zero symbols, two stand-ins
    {
        hist[0x00] += 1;
        hist[0xFF] += 1;
    } else if (unique == 1) // One symbol, one stand-in
    {
        if (hist[0x00] == 0) {
            hist[0x00] += 1; // 0x00 is the stand-in
        } else {
            hist[0xFF] += 1; // 0xFF is the stand-in
        }
    }

    // We provide the option to building a full tree or a minimal tree.

    for (uint32_t i = 0; i < BYTE; i += 1) {
        if (fullTree || hist[i] > 0) {
            enqueue(q, newNode(i, true, hist[i]));
            leaves += 1;
        }
    }

    // Save the size of the tree:
    //   1. Two bytes for each leaf
    //   2. One byte for each internal node
    //   3. leaves - 1 internal nodes
    //   4. Zero is the minimum

    treeBytes = leaves > 0 ? 3 * leaves - 1 : 0;
    h->tree_size = isBig() ? swap16(treeBytes) : treeBytes;
    buffered_write(outFile, (uint8_t *) h, sizeof(Header), true);

    treeNode *t = NULL;

    while (!empty(q)) {
        treeNode *l, *r;

        dequeue(q, &l); // Left child

        if (!empty(q)) {
            dequeue(q, &r); // Right child
            enqueue(q, join(l, r)); // Interior node
        } else {
            t = l;
        } // Singleton is the root
    }
    return t;
}

void dumpTree(int fileOut, treeNode *t) {
    static char L = 'L', I = 'I';

    if (t) {
        if (t->leaf) {
            (void) buffered_write(fileOut, (uint8_t *) &L, 1, false); // Leaf indicator
            (void) buffered_write(fileOut, (uint8_t *) &t->symbol, 1, false); // Symbol
        } else {
            (void) dumpTree(fileOut, t->left);
            (void) dumpTree(fileOut, t->right);
            (void) buffered_write(fileOut, (uint8_t *) &I, 1, false); // Interior node indicator
        }
    }
    return;
}

static void buildCode(code s, treeNode *t, code c[]) {
    if (t) {
        if (t->leaf) {
            c[t->symbol] = s; // Found it
            return;
        } else {
            uint32_t tmp;

            pushCode(&s, 0); // Go left
            buildCode(s, t->left, c);
            popCode(&s, &tmp);

            pushCode(&s, 1); // Go right
            buildCode(s, t->right, c);
            popCode(&s, &tmp);
        }
    } else {
        return;
    }
}

static void encodeFile(int fileIn, int fileOut, code c[]) {

    uint8_t b[KB];
    long count;

    lseek(fileIn, 0, SEEK_SET); // Start of the file

    while ((count = read(fileIn, b, KB)) > 0) {
        for (int i = 0; i < count; i += 1) {
            appendCode(fileOut, c[b[i]]);
        }
    }
    flushCode(fileOut);
    return;
}

int main(int argc, char **argv) {
    int fileIn = 0;
    int fileOut = 1;
    char *inputFile = NULL;
    char *outputFile = NULL;

    static struct option options[]
        = { { "input", required_argument, NULL, 'i' }, { "output", required_argument, NULL, 'o' },
              { "verbose", no_argument, &verbose, 'v' }, { "print", no_argument, &print, 'p' },
              { "full", no_argument, &fullTree, 'f' }, { NULL, 0, NULL, 0 } };

    int c;
    while ((c = getopt_long(argc, argv, "-fpvi:o:", options, NULL)) != -1) {
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
        case 'f': {
            fullTree = true;
            break;
        }
        }
    }

    if (inputFile) {
        if ((fileIn = open(inputFile, O_RDONLY)) < 0) {
            char s[1024] = { 0 };
            strcat(s, argv[0]);
            strcat(s, ": ");
            strcat(s, inputFile);
            perror(s);
            exit(1);
        }
        free(inputFile);
    } else {
        uint8_t buffer[KB];

        int tmpFile = Mymktemp();
        if (tmpFile < 0) {
            char s[1024] = { 0 };
            strcat(s, argv[0]);
            strcat(s, ": ");
            strcat(s, "stdin (/tmp)");
            perror(s);
            exit(1);
        }

        long len;
        while ((len = read(STDIN_FILENO, buffer, KB)) > 0) {
            write(tmpFile, buffer, len);
        }
        fileIn = tmpFile;
    }

    // How big was the original file?

    struct stat fileStat;
    fstat(fileIn, &fileStat);
    fchmod(fileOut, fileStat.st_mode);
    uint64_t origSize = fileStat.st_size;

    if (outputFile) {

        if ((fileOut = open(outputFile, O_CREAT | O_EXCL | O_RDWR | O_TRUNC, 0644)) < 0) {
            char s[1024] = { 0 };
            strcat(s, argv[0]);
            strcat(s, ": ");
            strcat(s, outputFile);
            perror(s);
            exit(1);
        }
        free(outputFile);
    } else {
        fileOut = STDOUT_FILENO;
    }

    // Build header, canonical is "Little Endian".
    Header h = {
        .magic = isBig() ? swap32(magicNumber) : magicNumber,
        .permissions = isBig() ? swap16(fileStat.st_mode) : fileStat.st_mode,
        .file_size = isBig() ? swap64(origSize) : origSize,
    };

    // Build a Huffman tree, finish header and write it out.
    treeNode *t = buildTree(fileIn, fileOut, &h);

    // Walk the tree to find the codes for each symbol
    code s = newCode();
    code builtCode[BYTE];
    buildCode(s, t, builtCode);

    // Output the tree
    dumpTree(fileOut, t);
    buffered_write(fileOut, (uint8_t *) 0, 0, true);

    // Output the encoded file
    encodeFile(fileIn, fileOut, builtCode);

    if (verbose) {
        fprintf(stderr, "Original %" PRIu64 " bits: ", 8 * origSize);
        fprintf(stderr, "leaves %u ", leaves);
        fprintf(stderr, "(%u bytes) ", treeBytes);
        fprintf(stderr, "encoding %" PRIu64 " bit%s", codeC, codeC == 1 ? "" : "s");
        if (origSize > 0) {
            fprintf(stderr, " (%2.4lf%%)", 100 * (double) codeC / (8 * fileStat.st_size));
        }
        fprintf(stderr, ".\n");
    }

    if (print) {
        printTree(t, 0);
    }

    close(fileIn);
    close(fileOut);

    delTree(t);
    exit(0);
}
