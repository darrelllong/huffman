#include "huffman.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

treeNode *newNode(uint8_t s, bool l, uint64_t c) {
    treeNode *n = (treeNode *) malloc(sizeof(treeNode));
    if (n) {
        n->symbol = s;
        n->count = c;
        n->leaf = l;
        n->left = NULL;
        n->right = NULL;
        return n;
    } else {
        return NULL;
    }
}

treeNode *join(treeNode *l, treeNode *r) {
    treeNode *n = newNode('$', false, l->count + r->count);
    if (n) {
        n->left = l;
        n->right = r;
        return n;
    } else {
        return NULL;
    }
}

static inline void spaces(int c) {
    for (int i = 0; i < c; i += 1) {
        fputc(' ', stderr);
    }
    return;
}

void printTree(treeNode *t, int depth) {
    if (t) {
        printTree(t->left, depth + 1); spaces(4 * depth);
        if (t->leaf) {
            if (isgraph(t->symbol)) {
                fprintf(stderr, "'%c' (%" PRIu64 ")\n", t->symbol, t->count);
            } else {
                fprintf(stderr, "0x%02X (%" PRIu64 ")\n", t->symbol, t->count);
            }
        } else {
            fprintf(stderr, "$ (%" PRIu64 ")\n", t->count);
        }
        printTree(t->right, depth + 1);
    }
    return;
}
