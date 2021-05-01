#include "stack.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define MIN_STACK 256

// Encapsulate and localize dyanmic allocations. This way you can check them,
// and fix them once when you make a mistake.

stack *newStack(void) {
    stack *s = (stack *) calloc(MIN_STACK, sizeof(stack));
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
    if (s->top > 0) {
        s->top -= 1;
        return s->entries[s->top];
    } else {
        return NULL;
    }
}

// push will continue to grow the stack. It will simply fail to push if you
// run out of memory. But if that happens, then you have bigger problems.

void push(stack *s, item i) {
    if (s->top == s->size) {
        s->size *= 2;
        s->entries = (item *) realloc(s->entries, s->size * sizeof(item));
    }
    if (s && s->entries) {
        s->entries[s->top] = i;
        s->top += 1;
    }
    return;
}
