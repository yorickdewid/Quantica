#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slay.h"
#include "zmalloc.h"

void *slay_wrap(void *data, size_t *len, datatype_t dt) {
	size_t slay_size = sizeof(struct value_slay)+*len;
	struct value_slay *slay = zmalloc(slay_size);
	slay->val_type = dt;
	slay->size = *len;

	uintptr_t *dest = ((uintptr_t *)slay)+sizeof(struct value_slay);
	memcpy(dest, data, *len);
	*len = slay_size;

	return (void *)slay;
}

void *slay_unwrap(void *value_slay, size_t *len, datatype_t *dt) {
	struct value_slay *slay = (struct value_slay *)value_slay;

	void *src = ((uintptr_t *)value_slay)+sizeof(struct value_slay);
	void *data = zmalloc(slay->size);
	memcpy(data, src, slay->size);

	*dt = slay->val_type;
	*len = slay->size;
	zfree(value_slay);

	return data;
}
