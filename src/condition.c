#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <error.h>
#include "zmalloc.h"
#include "vector.h"
#include "condition.h"

marshall_t *marshall_select(marshall_t *element, marshall_t *marshall, void *parent) {
	marshall_t *selection = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), parent);

	vector_t *selectors = alloc_vector(element->size);
	if (element->type == MTYPE_STRING) {
		vector_append(selectors, (void *)element->data);
	} else {
		for (unsigned int j = 0; j < element->size; ++j) {
			vector_append(selectors, (void *)element->child[j]->data);
		}
	}

	if (marshall_type_hasdata(marshall->type)) {
		for (unsigned int j = 0; j < selectors->size; ++j) {
			if (marshall->name && !strcmp(marshall->name, (char *)(vector_at(selectors, j)))) {
				selection->name = marshall->name;
				selection->name_len = marshall->name_len;
				selection->data = marshall->data;
				selection->data_len = marshall->data_len;
				selection->type = marshall->type;
				selection->size = 1;
			}
		}
	} else if (marshall_type_hasdescent(marshall->type)) {
		selection->child = (marshall_t **)tree_zcalloc(marshall->size, sizeof(marshall_t *), selection);
		selection->name = marshall->name;
		selection->name_len = marshall->name_len;
		selection->type = marshall->type;
		for (unsigned int i = 0; i < marshall->size; ++i) {
			if (marshall_type_hasdescent(marshall->child[i]->type)) {
				marshall_t *descentobj = marshall_select(element, marshall->child[i], selection);
				if (descentobj->size > 0)
					selection->child[selection->size++] = descentobj;
			} else {
				for (unsigned int j = 0; j < selectors->size; ++j) {
					if (marshall->child[i]->name && !strcmp(marshall->child[i]->name, (char *)(vector_at(selectors, j)))) {
						selection->child[selection->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
						selection->child[selection->size]->name = marshall->child[i]->name;
						selection->child[selection->size]->name_len = marshall->child[i]->name_len;
						selection->child[selection->size]->data = marshall->child[i]->data;
						selection->child[selection->size]->data_len = marshall->child[i]->data_len;
						selection->child[selection->size]->type = marshall->child[i]->type;
						selection->size++;
					}
				}
			}
		}
	} else {
		for (unsigned int j = 0; j < selectors->size; ++j) {
			if (marshall->name && !strcmp(marshall->name, (char *)(vector_at(selectors, j)))) {
				selection->name = marshall->name;
				selection->name_len = marshall->name_len;
				selection->type = marshall->type;
				selection->size = 1;
			}
		}
	}

	vector_free(selectors);
	return selection;
}
