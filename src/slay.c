#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dstype.h"
#include "slay.h"
#include "zmalloc.h"

void *slay_wrap(void *data, size_t *len, dstype_t dt) {
	size_t data_size = *len;
	if (dt < DT_INT)
		data_size = 0;

	size_t slay_size = sizeof(struct value_slay)+data_size;
	struct value_slay *slay = zmalloc(slay_size);
	slay->val_type = dt;
	slay->size = data_size;
	printf("dsize %ld\n", sizeof(struct value_slay));
	if (!slay->size)
		goto done;

	uintptr_t *dest = ((uintptr_t *)slay)+sizeof(struct value_slay);
	memcpy(dest, data, data_size);
	puts("copy data");

done:
	*len = slay_size;
	return (void *)slay;
}

void *slay_unwrap(void *value_slay, size_t *len, dstype_t *dt) {
	struct value_slay *slay = (struct value_slay *)value_slay;
	void *data = NULL;

	if (slay->size) {
		void *src = ((uintptr_t *)value_slay)+sizeof(struct value_slay);
		data = zmalloc(slay->size);
		memcpy(data, src, slay->size);
		puts("copy data");
	}

	*dt = slay->val_type;
	*len = slay->size;
	zfree(value_slay);

	return data;
}
