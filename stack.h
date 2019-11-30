#ifndef _STACK_H_DL
#define _STACK_H_DL

#include "huffman.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef _ITEM_DL
#define _ITEM_DL
typedef treeNode *item;
#endif

typedef struct stack {
  uint32_t size;
  uint32_t top;
  item *entries;
} stack;

extern stack *newStack();

extern item pop(stack *);

extern void push(stack *, item);

static inline void delStack(stack *s) {
  free(s->entries);
  free(s);
  return;
}

static inline bool emptyS(stack *s) { return s->top == 0; }

#endif
