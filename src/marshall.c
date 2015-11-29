#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <error.h>
#include "quid.h"
#include "json_check.h"
#include "vector.h"
#include "zmalloc.h"
#include "marshall.h"

#define VECTOR_SIZE	1024

struct {
	marshall_type_t type;
	bool data;
	bool descent;
} type_info[] = {

	/* No data */
	{MTYPE_NULL,	FALSE, FALSE},
	{MTYPE_TRUE,	FALSE, FALSE},
	{MTYPE_FALSE,	FALSE, FALSE},

	/* Containing data */
	{MTYPE_INT,		TRUE, FALSE},
	{MTYPE_FLOAT,	TRUE, FALSE},
	{MTYPE_STRING,	TRUE, FALSE},
	{MTYPE_QUID,	TRUE, FALSE},

	/* Containing descending data */
	{MTYPE_ARRAY,	FALSE, TRUE},
	{MTYPE_OBJECT,	FALSE, TRUE},
};

/*
 * Does marshall type require additional data
 */
bool marshall_type_hasdata(marshall_type_t type) {
	for (unsigned int i = 0; i < RSIZE(type_info); ++i) {
		if (type_info[i].type == type)
			return type_info[i].data;
	}
	return FALSE;
}

/*
 * Can marshall type contain one or more marshall objects
 */
bool marshall_type_hasdescent(marshall_type_t type) {
	for (unsigned int i = 0; i < RSIZE(type_info); ++i) {
		if (type_info[i].type == type)
			return type_info[i].descent;
	}
	return FALSE;
}


/*
 * Detect datatype based on characteristics
 */
marshall_type_t autoscalar(const char *data, size_t len) {
	if (!len)
		return MTYPE_NULL;
	if (len == 1) {
		int fchar = data[0];
		switch (fchar) {
			case '0':
			case 'f':
			case 'F':
				return MTYPE_FALSE;
			case '1':
			case 't':
			case 'T':
				return MTYPE_TRUE;
			default:
				if (strisdigit((char *)data))
					return MTYPE_INT;
				return MTYPE_STRING;
		}
	}
	int8_t b = strisbool((char *)data);
	if (b != -1)
		return b ? MTYPE_TRUE : MTYPE_FALSE;
	if (strisdigit((char *)data))
		return MTYPE_INT;
	if (strismatch(data, "1234567890.")) {
		if (strccnt(data, '.') == 1)
			if (data[0] != '.' && data[len - 1] != '.')
				return MTYPE_FLOAT;
	}
	if (strquid_format(data) > 0)
		return MTYPE_QUID;
	if (json_valid(data))
		return MTYPE_OBJECT;
	if (!strcmp(data, "null") || !strcmp(data, "NULL"))
		return MTYPE_NULL;
	return MTYPE_STRING;
}

/*
 * Count elements by level
 */
unsigned int marshall_get_count(marshall_t *obj, int depth, unsigned _depth) {

	/* Only descending scalars need to be counted */
	if (marshall_type_hasdescent(obj->type)) {
		unsigned int n = 1;
		if (depth == -1 || ((unsigned int)depth) > _depth) {
			for (unsigned int i = 0; i < obj->size; ++i) {
				n += marshall_get_count(obj->child[i], depth, _depth + 1);
			}
		}
		return n;
	}

	return 1;
}

/*
 * Convert object data to string
 */
char *marshall_strdata(marshall_t *obj, size_t *len) {
	switch (obj->type) {
		case MTYPE_NULL:
			*len = 4;
			return "null";

		case MTYPE_TRUE:
			*len = 4;
			return "true";

		case MTYPE_FALSE:
			*len = 5;
			return "false";

		case MTYPE_FLOAT:
		case MTYPE_INT:
		case MTYPE_QUID:
		case MTYPE_STRING:
			*len = obj->data_len;
			return (char *)obj->data;

		default:
			error_throw("70bef771b0a3", "Invalid datatype");
			return NULL;
	}
	return NULL;
}

marshall_t *marshall_filter(marshall_t *element, marshall_t *marshall, void *parent) {
	marshall_t *selection = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), parent);

	vector_t *selectors = alloc_vector(element->size);
	if (element->type == MTYPE_STRING) {
		vector_append(selectors, (void *)element->data);
	} else {
		for (unsigned int j = 0; j < element->size; ++j) {
			vector_append(selectors, (void *)element->child[j]->data);
		}
	}

	if (marshall_type_hasdescent(marshall->type)) {
		selection->child = (marshall_t **)tree_zcalloc(marshall->size, sizeof(marshall_t *), selection);
		selection->name = marshall->name;
		selection->name_len = marshall->name_len;
		selection->type = marshall->type;
		for (unsigned int i = 0; i < marshall->size; ++i) {
			if (marshall_type_hasdescent(marshall->child[i]->type)) {
				marshall_t *descentobj = marshall_filter(element, marshall->child[i], selection);
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
				selection->data = marshall->data;
				selection->data_len = marshall->data_len;
				selection->type = marshall->type;
				selection->size = 1;
			}
		}
	}

	vector_free(selectors);
	return selection;
}

/*
 * Merge two marshall structutes and return result
 */
marshall_t *marshall_merge(marshall_t *newobject, marshall_t *marshall) {
	if (marshall_type_hasdescent(marshall->type)) { /* Multiple existing values */
		if (newobject->type == MTYPE_OBJECT) {
			for (unsigned int i = 0; i < newobject->size; ++i) {
				if (marshall_type_hasdescent(newobject->child[i]->type)) {
					if (marshall->type == MTYPE_ARRAY) {
						marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
						marshall->child[marshall->size]->type = newobject->type;
						marshall->child[marshall->size]->child = (marshall_t **)tree_zcalloc(1, sizeof(marshall_t *), marshall);
						marshall->child[marshall->size]->child[0] = newobject->child[i];
						marshall->child[marshall->size]->size = 1;
						marshall->size++;
					} else {
						marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
						marshall->child[marshall->size]->name = newobject->child[i]->name;
						marshall->child[marshall->size]->name_len = newobject->child[i]->name_len;
						marshall->child[marshall->size]->type = newobject->child[i]->type;
						marshall->child[marshall->size]->child = (marshall_t **)tree_zcalloc(newobject->child[i]->size, sizeof(marshall_t *), marshall);
						for (unsigned int j = 0; j < newobject->child[i]->size; ++j) {
							marshall->child[marshall->size]->child[marshall->child[marshall->size]->size++] = newobject->child[i]->child[j];
						}
						marshall->size++;
					}
				} else if (marshall->type == MTYPE_OBJECT && newobject->type == MTYPE_OBJECT) {
					marshall->child = (marshall_t **)tree_zrealloc(marshall->child, (marshall->size + newobject->size) * sizeof(marshall_t *));
					marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
					marshall->child[marshall->size]->name = newobject->child[i]->name;
					marshall->child[marshall->size]->name_len = newobject->child[i]->name_len;
					marshall->child[marshall->size]->data = newobject->child[i]->data;
					marshall->child[marshall->size]->data_len = newobject->child[i]->data_len;
					marshall->child[marshall->size]->type = newobject->child[i]->type;
					marshall->size++;
				} else if (marshall->type == MTYPE_ARRAY && newobject->type == MTYPE_OBJECT) {
					marshall->child = (marshall_t **)tree_zrealloc(marshall->child, (marshall->size + newobject->size) * sizeof(marshall_t *));
					marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
					marshall->child[marshall->size]->type = MTYPE_OBJECT;
					marshall->child[marshall->size]->child = (marshall_t **)tree_zcalloc(1, sizeof(marshall_t *), marshall);
					marshall->child[marshall->size]->child[0] = tree_zcalloc(1, sizeof(marshall_t), marshall);
					marshall->child[marshall->size]->child[0]->name = newobject->child[i]->name;
					marshall->child[marshall->size]->child[0]->name_len = newobject->child[i]->name_len;
					marshall->child[marshall->size]->child[0]->data = newobject->child[i]->data;
					marshall->child[marshall->size]->child[0]->data_len = newobject->child[i]->data_len;
					marshall->child[marshall->size]->child[0]->type = newobject->child[i]->type;
					marshall->child[marshall->size]->size++;
					marshall->size++;
				} else {
					error_throw("04904b8810ed", "Cannot merge structures");
					return NULL;
				}
			}
		} else if (newobject->type == MTYPE_ARRAY) {
			for (unsigned int i = 0; i < newobject->size; ++i) {
				if (marshall_type_hasdescent(newobject->child[i]->type)) {
					if (marshall->type == MTYPE_OBJECT && !newobject->child[i]->name) {
						error_throw("04904b8810ed", "Cannot merge structures");
						return NULL;
					}
					marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
					marshall->child[marshall->size]->type = newobject->child[i]->type;
					marshall->child[marshall->size]->child = (marshall_t **)tree_zcalloc(newobject->child[i]->size, sizeof(marshall_t *), marshall);
					for (unsigned int j = 0; j < newobject->child[i]->size; ++j) {
						marshall->child[marshall->size]->child[marshall->child[marshall->size]->size++] = newobject->child[i]->child[j];
					}
					marshall->size++;
				} else  if (marshall->type == MTYPE_ARRAY && newobject->type == MTYPE_ARRAY) {
					marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
					marshall->child[marshall->size]->name = newobject->child[i]->name;
					marshall->child[marshall->size]->name_len = newobject->child[i]->name_len;
					marshall->child[marshall->size]->data = newobject->child[i]->data;
					marshall->child[marshall->size]->data_len = newobject->child[i]->data_len;
					marshall->child[marshall->size]->type = newobject->child[i]->type;
					marshall->size++;
				} else {
					error_throw("04904b8810ed", "Cannot merge structures");
					return NULL;
				}
			}
		} else { /* Single new value, multiple existing */
			if (marshall->type == MTYPE_OBJECT) {
				error_throw("04904b8810ed", "Cannot merge structures");
				return NULL;
			}

			marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->name = newobject->name;
			marshall->child[marshall->size]->name_len = newobject->name_len;
			marshall->child[marshall->size]->data = newobject->data;
			marshall->child[marshall->size]->data_len = newobject->data_len;
			marshall->child[marshall->size]->type = newobject->type;
			marshall->size++;
		}
	} else { /* Single existing values */
		if (newobject->type == MTYPE_OBJECT) {
			error_throw("04904b8810ed", "Cannot merge structures");
			return NULL;
		}

		/* Make current structure an array */
		marshall->child = (marshall_t **)tree_zcalloc(newobject->size + 1, sizeof(marshall_t *), marshall);
		marshall->child[0] = tree_zcalloc(1, sizeof(marshall_t), marshall);
		marshall->child[0]->data = marshall->data;
		marshall->child[0]->data_len = marshall->data_len;
		marshall->child[0]->type = marshall->type;

		marshall->type = MTYPE_ARRAY;
		marshall->size = 1;

		if (newobject->type == MTYPE_ARRAY) { /* Multiple new value, single existing */
			for (unsigned int j = 0; j < newobject->size; ++j) {
				marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
				marshall->child[marshall->size]->data = newobject->child[j]->data;
				marshall->child[marshall->size]->data_len = newobject->child[j]->data_len;
				marshall->child[marshall->size]->type = newobject->child[j]->type;
				marshall->size++;
			}
		} else { /* Single new value, single existing */
			marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->data = newobject->data;
			marshall->child[marshall->size]->data_len = newobject->data_len;
			marshall->child[marshall->size]->type = newobject->type;
			marshall->size++;
		}
	}

	return marshall;
}

bool marshall_equal(marshall_t *object_1, marshall_t *object_2) {
	if (object_2->type == MTYPE_OBJECT) {
		if (object_1->type != MTYPE_OBJECT) {
			return FALSE;
		} else {
			if (object_1->size != object_2->size)
				return FALSE;
			for (unsigned int i = 0; i < object_1->size; ++i) {
				if (!marshall_equal(object_1->child[i], object_2->child[i]))
					return FALSE;
			}
			return TRUE;
		}
	} else if (object_2->type == MTYPE_ARRAY) {
		if (object_1->type != MTYPE_ARRAY) {
			return FALSE;
		} else {
			if (object_1->size != object_2->size)
				return FALSE;
			for (unsigned int i = 0; i < object_1->size; ++i) {
				if (!marshall_equal(object_1->child[i], object_2->child[i]))
					return FALSE;
			}
			return TRUE;
		}
	} else {
		if (object_1->type == MTYPE_OBJECT || object_1->type == MTYPE_ARRAY) {
			return FALSE;
		} else {
			//if (object_1->size != object_2->size)
			//	return FALSE;
			if (object_1->type != object_2->type)
				return FALSE;
			if (object_1->name_len != object_2->name_len)
				return FALSE;
			if (object_1->name && object_2->name) {
				if (strcmp(object_1->name, object_2->name))
					return FALSE;
			}
			if (object_1->data_len != object_2->data_len)
				return FALSE;
			if (strcmp(object_1->data, object_2->data))
				return FALSE;
			return TRUE;
		}
	}
	return FALSE;
}

static void shift_left(marshall_t *marshall, int offset) {
	for (unsigned int i = offset; i < marshall->size - 1; ++i) {
		marshall->child[i]->data = marshall->child[i + 1]->data;
		marshall->child[i]->data_len = marshall->child[i + 1]->data_len;
		marshall->child[i]->type = marshall->child[i + 1]->type;
		marshall->child[i]->child = marshall->child[i + 1]->child;
	}
}

marshall_t *marshall_separate(marshall_t *filterobject, marshall_t *marshall, bool *changed) {
	if (marshall->type == MTYPE_OBJECT) {
		if (filterobject->type == MTYPE_OBJECT) {
			if (marshall_equal(marshall, filterobject)) {
				marshall->data = NULL;
				marshall->data_len = 0;
				marshall->type = MTYPE_NULL;
				marshall->size = 0;
				*changed = TRUE;
			}
		} else if (filterobject->type == MTYPE_ARRAY) {
			for (unsigned int j = 0; j < filterobject->size; ++j) {
				if (marshall_equal(marshall, filterobject->child[j])) {
					marshall->data = NULL;
					marshall->data_len = 0;
					marshall->type = MTYPE_NULL;
					marshall->size = 0;
					*changed = TRUE;
				}
			}
		}
	} else if (marshall->type == MTYPE_ARRAY) {
		for (unsigned int i = 0; i < marshall->size; ++i) {
			if (filterobject->type == MTYPE_ARRAY) {
arr_again:
				for (unsigned int j = 0; j < filterobject->size; ++j) {
					if (marshall_equal(marshall->child[i], filterobject->child[j])) {
						marshall->child[i]->data = NULL;
						marshall->child[i]->data_len = 0;
						marshall->child[i]->type = MTYPE_NULL;

						shift_left(marshall, i);

						marshall->size--;
						*changed = TRUE;
						goto arr_again;
					}
				}
			} else {
				if (marshall_equal(marshall->child[i], filterobject)) {
					marshall->child[i]->data = NULL;
					marshall->child[i]->data_len = 0;
					marshall->child[i]->type = MTYPE_NULL;

					shift_left(marshall, i);

					marshall->size--;
					*changed = TRUE;
				}
			}
		}
	} else {
		if (filterobject->type == MTYPE_ARRAY) {
			for (unsigned int j = 0; j < filterobject->size; ++j) {
				if (marshall_equal(marshall, filterobject->child[j])) {
					marshall->data = NULL;
					marshall->data_len = 0;
					marshall->type = MTYPE_NULL;
					marshall->size = 0;
					*changed = TRUE;
				}
			}
		} else {
			if (marshall_equal(marshall, filterobject)) {
				marshall->data = NULL;
				marshall->data_len = 0;
				marshall->type = MTYPE_NULL;
				marshall->size = 0;
				*changed = TRUE;
			}
		}
	}

	return marshall;
}

char *marshall_get_strtype(marshall_type_t type) {
	switch (type) {
		case MTYPE_NULL:
			return "NULL";
		case MTYPE_TRUE:
			return "TRUE";
		case MTYPE_FALSE:
			return "FALSE";
		case MTYPE_INT:
			return "INTEGER";
		case MTYPE_FLOAT:
			return "FLOAT";
		case MTYPE_STRING:
			return "STRING";
		case MTYPE_QUID:
			return "QUID";
		case MTYPE_ARRAY:
			return "ARRAY";
		case MTYPE_OBJECT:
			return "OBJECT";
		default:
			return "NULL";
	}
	return "NULL";
}
