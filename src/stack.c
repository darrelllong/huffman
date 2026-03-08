#include "stack.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define MIN_STACK 256

// Encapsulate and localize dyanmic allocations. This way you can check them,
// and fix them once when you make a mistake.

stack *newStack(void) {
    stack *s = (stack *) calloc(1, sizeof(stack));
    if (s) {
        s->size = MIN_STACK;
        s->top = 0;
        s->entries = (item *) calloc(MIN_STACK, sizeof(item));
        if (s->entries) {
            return s;
        }
    }
    free(s);
    return (void *) 0;
}

// pop will return a NULL pointer if the stack is empty.

item pop(stack *s) {
    if (!s || s->top <= 0) {
        return NULL;
    } else {
        s->top -= 1;
        return s->entries[s->top];
    }
}

// push will continue to grow the stack. It will simply fail to push if you
// run out of memory. But if that happens, then you have bigger problems.

void push(stack *s, item i) {
    if (!s || !s->entries) {
        return;
    }
    if (s->top == s->size) {
        uint32_t newSize = s->size * 2;
        item *tmp = (item *) realloc(s->entries, newSize * sizeof(item));
        if (tmp == NULL) {
            return; // Out of memory: preserve previous stack state.
        }
        s->entries = tmp;
        s->size = newSize;
    }
    s->entries[s->top] = i;
    s->top += 1;
    return;
}
