#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zmalloc.h"
#include "stack.h"

void stack_init(qstack_t *stack, int max_size) {
	stack->contents = zmalloc(sizeof(void *) * max_size);
	stack->max_size = max_size;
	stack->size = 0;
	stack->top = -1;
	stack->rtop = 0;
	memset(stack->contents, '\0', sizeof(void *) * max_size);
}

void stack_destroy(qstack_t *stack) {
	for (int i = 0; i < stack->max_size; ++i) {
		if (stack->contents[i])
			tree_zfree(stack->contents[i]);
	}
	zfree(stack->contents);
	stack->contents = NULL;
	stack->max_size = 0;
	stack->size = 0;
	stack->top = -1;
	stack->rtop = 0;
}

void stack_push(qstack_t *stack, void *element) {
	if (stack->top >= stack->max_size - 1)
		stack->contents = zrealloc(stack->contents, sizeof(void *) * (stack->max_size *= 2));

	stack->size++;
	stack->contents[++stack->top] = element;
}

void *stack_pop(qstack_t *stack) {
	if (stack->size <= 0)
		return NULL;

	stack->size--;
	return stack->contents[stack->top--];
}

void *stack_offset_peek(qstack_t *stack, int offset) {
	if (stack->size <= 0)
		return NULL;

	return stack->contents[stack->top - offset];
}

void *stack_offset_rpeek(qstack_t *stack, int offset) {
	if (stack->size <= 0)
		return NULL;

	return stack->contents[stack->rtop + offset];
}

void *stack_peek(qstack_t *stack) {
	if (stack->size <= 0)
		return NULL;

	return stack->contents[stack->top];
}

void *stack_rpeek(qstack_t *stack) {
	if (stack->size <= 0)
		return NULL;

	return stack->contents[0];
}

void *stack_rpop(qstack_t *stack) {
	if (stack->size <= 0)
		return NULL;

	stack->size--;
	return stack->contents[stack->rtop++];
}
