#pragma once

#include "huffman.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct queue {
    uint32_t size;
    uint32_t head, tail;
    item *Q;
} queue;

extern queue *newQueue(uint32_t size);

extern void delQueue(queue *q);

extern bool empty(queue *q);
extern bool full(queue *q);

extern bool enqueue(queue *q, item i);
extern bool dequeue(queue *q, item *i);
