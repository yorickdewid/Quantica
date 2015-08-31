#ifndef STACK_H_INCLUDED
#define STACK_H_INCLUDED

typedef struct {
	void **contents;
	int max_size;
	int size;
	int top;
	int rtop;
} qstack_t;

void stack_init(qstack_t *stack, int max_size);
void stack_destroy(qstack_t *stack);
void stack_push(qstack_t *stack, void *elm);
void *stack_pop(qstack_t *stack);
void *stack_rpop(qstack_t *stack);
void *stack_offset_peek(qstack_t *stack, int offset);
void *stack_offset_rpeek(qstack_t *stack, int offset);
void *stack_peek(qstack_t *stack);
void *stack_rpeek(qstack_t *stack);

#endif // STACK_H_INCLUDED
