#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define MAGIC 0xdeaddead

typedef struct DAH treeNode;

typedef treeNode *item;

struct DAH {
    uint8_t symbol;
    uint64_t count;
    bool leaf;
    treeNode *left, *right;
};

extern treeNode *newNode(uint8_t s, bool l, uint64_t c);

extern treeNode *join(treeNode *l, treeNode *r);

static inline void delNode(treeNode *h) {
    free(h);
    return;
}

static inline void delTree(treeNode *h) {
    if (h) {
        delTree(h->left);
        delTree(h->right);
        delNode(h);
    }
    return;
}

static inline int compare(treeNode *l, treeNode *r) {
    return (int) (l->count - r->count);
}

extern void printTree(treeNode *t, int depth);
