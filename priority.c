#include "huffman.h"
#include "queue.h"

#include <stdlib.h>

static inline uint32_t succ(uint32_t x, uint32_t n) { return (x + n + 1) % n; }

// Number theory says modulo should be positive, but in C we have -1 % n = -1,
// and we want n - 1.

static inline uint32_t pred(uint32_t x, uint32_t n) { return (x + n - 1) % n; }

// Encapsulate and localize dynamic allocations. This way you can check them,
// and fix them once when you make a mistake.

queue *newQueue(uint32_t size) {
    queue *q = (queue *) malloc(sizeof(queue));
    if (q) {
        q->size = size;
        q->head = 0;
        q->tail = 0;
        q->Q = (item *) calloc(size, sizeof(item));
        if (q->Q) {
            return q;
        }
    }
    free(q);
    return (queue *) 0;
}

// The same goes for cleaning up your mess. This way you do not make the
// mistake of freeing memory out of order, or forgetting to free.

void delQueue(queue *q) {
    if (q) {
        free(q->Q);
        free(q);
    }
    return;
}

// Both head and tail point to an empty slot.

bool empty(queue *q) {
    if (q) {
        return q->head == q->tail;
    } else {
        return true; // NULL queues are empty
    }
}

// full sacrifices one slot to make the computation easier.

bool full(queue *q) {
    if (q) {
        return succ(q->head, q->size) == q->tail;
    } else {
        return true; // NULL queues are full
    }
}

// enqueue places the smallest element at the tail using the equivalent of an
// insertion sort. This is preferable to a more sophisticated method when you
// understand that there are never more than 257 entries (including the empty
// slot) in the queue and the average case will be less than 500 operations,
// with the worst case being 32896 operations.

bool enqueue(queue *q, item i) {
    if (full(q)) {
        return false;
    } else {
        if (q && q->Q) {
            uint32_t slot = q->head;

            while (slot != q->tail && compare(q->Q[pred(slot, q->size)], i) > 0) {
                q->Q[slot] = q->Q[pred(slot, q->size)];
                slot = pred(slot, q->size);
            }
            q->Q[slot] = i;
            q->head = succ(q->head, q->size);
        }
        return true;
    }
}

bool dequeue(queue *q, item *i) {
    if (empty(q)) {
        return false;
    } else {
        if (q && q->Q) {
            *i = q->Q[q->tail];
            q->tail = succ(q->tail, q->size);
        }
        return true;
    }
}
