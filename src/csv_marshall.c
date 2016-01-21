#include <stdio.h>
#include <stdlib.h>

#include "zmalloc.h"
#include "vector.h"
#include "marshall.h"
#include "csv.h"

void marshall_csv_parse_options(csv_t *csvopt, marshall_t *options) {
	if (options->type == MTYPE_OBJECT) {
		csvopt->delimiter = ',';
	}
}

marshall_t *marshall_csv_decode(csv_t *csvopt, char *data) {
	int cnt = strccnt(data, '\n');

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(cnt, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_ARRAY;

	char *pdata = strtok(data, "\r\n");
	size_t fields = csv_getfieldcount(csvopt, pdata);
	while (pdata != NULL) {
		vector_t *field_array = alloc_vector(fields);
		csv_getfield(csvopt, pdata, field_array);

		marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
		marshall->child[marshall->size]->child = (marshall_t **)tree_zcalloc(field_array->size, sizeof(marshall_t *), marshall);
		marshall->child[marshall->size]->type = MTYPE_ARRAY;

		for (unsigned int i = 0; i < field_array->size; ++i) {
			char *field = (char *)vector_at(field_array, i);

			marshall->child[marshall->size]->child[marshall->child[marshall->size]->size] = marshall_convert_parent(field, strlen(field), marshall);
			marshall->child[marshall->size]->size++;
		}

		marshall->size++;
		vector_free(field_array);

		pdata = strtok(NULL, "\r\n");
	}

	return marshall;
}
