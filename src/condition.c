#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <error.h>
#include "zmalloc.h"
#include "condition.h"

static marshall_t *match_select(const char *element, marshall_t *marshall, void *parent) {
	marshall_t *selection = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), parent);

	if (marshall_type_hasdata(marshall->type)) {
		if (marshall->name && !strcmp(marshall->name, element)) {
			selection->name = marshall->name;
			selection->name_len = marshall->name_len;
			selection->data = marshall->data;
			selection->data_len = marshall->data_len;
			selection->type = marshall->type;
			selection->size = 1;
		}
	} else if (marshall_type_hasdescent(marshall->type)) {
		selection->child = (marshall_t **)tree_zcalloc(marshall->size, sizeof(marshall_t *), selection);
		selection->name = marshall->name;
		selection->name_len = marshall->name_len;
		selection->type = marshall->type;
		for (unsigned int i = 0; i < marshall->size; ++i) {
			if (marshall_type_hasdescent(marshall->child[i]->type)) {
				selection->child[selection->size++] = match_select(element, marshall->child[i], selection);
			} else {
				if (marshall->child[i]->name && !strcmp(marshall->child[i]->name, element)) {
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
	} else {
		if (marshall->name && !strcmp(marshall->name, element)) {
			selection->name = marshall->name;
			selection->name_len = marshall->name_len;
			selection->type = marshall->type;
			selection->size = 1;
		}
	}

	return selection;
}

marshall_t *condition_select(const char *element, marshall_t *marshall) {
	marshall_t *selection = NULL;

	selection = match_select(element, marshall, NULL);

	return selection;
}
