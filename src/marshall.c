#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"
#include "quid.h"
#include "json_check.h"
#include "zmalloc.h"
#include "marshall.h"

#define VECTOR_SIZE	1024

struct {
	marshall_type_t type;
	bool data;
	bool descent;
} type_info[] = {
	/* No data */
	{MTYPE_NULL, FALSE, FALSE},
	{MTYPE_TRUE, FALSE, FALSE},
	{MTYPE_FALSE, FALSE, FALSE},
	/* Containing data */
	{MTYPE_INT, TRUE, FALSE},
	{MTYPE_FLOAT, TRUE, FALSE},
	{MTYPE_STRING, TRUE, FALSE},
	{MTYPE_QUID, TRUE, FALSE},
	/* Containing descending data */
	{MTYPE_ARRAY, FALSE, TRUE},
	{MTYPE_OBJECT, FALSE, TRUE},
};

bool marshall_type_hasdata(marshall_type_t type) {
	for(unsigned int i = 0; i < RSIZE(type_info); ++i) {
		if (type_info[i].type == type)
			return type_info[i].data;
	}
	return FALSE;
}

bool marshall_type_hasdescent(marshall_type_t type) {
	for(unsigned int i = 0; i < RSIZE(type_info); ++i) {
		if (type_info[i].type == type)
			return type_info[i].descent;
	}
	return FALSE;
}

static marshall_t *marshall_obect_decode(char *data, size_t data_len, char *name, size_t name_len, void *parent) {
	dict_parser p;
	dict_token_t t[data_len];

	dict_init(&p);
	int o = dict_parse(&p, data, data_len, t, data_len);
	if (o < 1)
		return NULL;

	marshall_t *obj = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
	memset(obj, 0, sizeof(marshall_t));
	obj->child = (marshall_t **)tree_zmalloc(o * sizeof(marshall_t *), obj);
	memset(obj->child, 0, o * sizeof(marshall_t *));

	if (name && name_len) {
		obj->name = name;
		obj->name_len = name_len;
	}

	switch (t[0].type) {
		case DICT_ARRAY: {
			int i;
			obj->type = MTYPE_ARRAY;
			for (i = 1; i < o; ++i) {
				switch (t[i].type) {
					case DICT_PRIMITIVE:
						obj->child[obj->size] = tree_zmalloc(sizeof(marshall_t), obj);
						memset(obj->child[obj->size], 0, sizeof(marshall_t));
						if (dict_cmp(data, &t[i], "null")) {
							obj->child[obj->size]->type = MTYPE_NULL;
						} else if (dict_cmp(data, &t[i], "true")) {
							obj->child[obj->size]->type = MTYPE_TRUE;
						} else if (dict_cmp(data, &t[i], "false")) {
							obj->child[obj->size]->type = MTYPE_FALSE;
						} else {
							obj->child[obj->size]->type = MTYPE_INT;
							obj->child[obj->size]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, obj);
							obj->child[obj->size]->data_len = t[i].end - t[i].start;
							if (strismatch(obj->child[obj->size]->data, "1234567890."))
								obj->child[obj->size]->type = MTYPE_FLOAT;
						}
						obj->size++;
						break;
					case DICT_STRING:
						obj->child[obj->size] = tree_zmalloc(sizeof(marshall_t), obj);
						memset(obj->child[obj->size], 0, sizeof(marshall_t));
						obj->child[obj->size]->type = MTYPE_STRING;
						obj->child[obj->size]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, obj);
						obj->child[obj->size]->data_len = t[i].end - t[i].start;
						if (strquid_format(obj->child[obj->size]->data) > 0)
							obj->child[obj->size]->type = MTYPE_QUID;
						obj->size++;
						break;
					case DICT_OBJECT: {
						obj->child[obj->size] = marshall_obect_decode(data + t[i].start, t[i].end - t[i].start, NULL, 0, obj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						obj->size++;
						break;
					}
					case DICT_ARRAY: {
						obj->child[obj->size] = marshall_obect_decode(data + t[i].start, t[i].end - t[i].start, NULL, 0, obj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						obj->size++;
						break;
					}
					default:
						break;
				}
			}
			break;
		}
		case DICT_OBJECT: {
			int i;
			obj->type = MTYPE_OBJECT;
			unsigned char setname = 0;
			for (i = 1; i < o; ++i) {
				switch (t[i].type) {
					case DICT_PRIMITIVE:
						if (dict_cmp(data, &t[i], "null")) {
							obj->child[obj->size]->type = MTYPE_NULL;
						} else if (dict_cmp(data, &t[i], "true")) {
							obj->child[obj->size]->type = MTYPE_TRUE;
						} else if (dict_cmp(data, &t[i], "false")) {
							obj->child[obj->size]->type = MTYPE_FALSE;
						} else {
							obj->child[obj->size]->type = MTYPE_INT;
							obj->child[obj->size]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, obj);
							obj->child[obj->size]->data_len = t[i].end - t[i].start;
							if (strismatch(obj->child[obj->size]->data, "1234567890."))
								obj->child[obj->size]->type = MTYPE_FLOAT;
						}
						obj->size++;
						setname = 0;
						break;
					case DICT_STRING:
						if (!setname) {
							obj->child[obj->size] = tree_zmalloc(sizeof(marshall_t), obj);
							memset(obj->child[obj->size], 0, sizeof(marshall_t));
							obj->child[obj->size]->type = MTYPE_STRING;
							obj->child[obj->size]->name = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, obj);
							obj->child[obj->size]->name_len = t[i].end - t[i].start;
							setname = 1;
						} else {
							obj->child[obj->size]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, obj);
							obj->child[obj->size]->data_len = t[i].end - t[i].start;
							if (strquid_format(obj->child[obj->size]->data) > 0)
								obj->child[obj->size]->type = MTYPE_QUID;
							obj->size++;
							setname = 0;
						}
						break;
					case DICT_OBJECT: {
						obj->child[obj->size] = marshall_obect_decode(data + t[i].start, t[i].end - t[i].start, obj->child[obj->size]->name, obj->child[obj->size]->name_len, obj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						obj->size++;
						setname = 0;
						break;
					}
					case DICT_ARRAY: {
						obj->child[obj->size] = marshall_obect_decode(data + t[i].start, t[i].end - t[i].start, obj->child[obj->size]->name, obj->child[obj->size]->name_len, obj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						obj->size++;
						setname = 0;
						break;
					}
					default:
						break;
				}
			}
			break;
		}
		default:
			break;
	}
	return obj;
}

static marshall_type_t autoscalar(const char *data, size_t len) {
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

/* Count elements by level */
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

/* Convert string to object */
marshall_t *marshall_convert(char *data, size_t data_len) {
	marshall_t *marshall = NULL;
	marshall_type_t type = autoscalar(data, data_len);

	/* Create marshall object based on scalar */
	if (marshall_type_hasdata(type)) {
		marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), NULL);
		memset(marshall, 0, sizeof(marshall_t));
		marshall->data = tree_zstrndup(data, data_len, marshall);
		marshall->data_len = data_len;
		marshall->type = type;
	} else if (marshall_type_hasdescent(type)) {
		marshall = marshall_obect_decode(data, data_len, NULL, 0, NULL);
	} else {
		marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), NULL);
		memset(marshall, 0, sizeof(marshall_t));
		marshall->type = type;
	}

	return marshall;
}

/* Convert marshall object to string */
char *marshall_serialize(marshall_t *obj) {
	if (!obj)
		return NULL;

	switch (obj->type) {
		case MTYPE_NULL: {
			if (obj->name) {
				size_t len = obj->name_len + 8;
				char *data = (char *)zmalloc(len + 1);
				memset(data, 0, len + 1);
				sprintf(data, "\"%s\":null", obj->name);
				return data;
			} else {
				return zstrdup("null");
			}
			break;
		}
		case MTYPE_TRUE: {
			if (obj->name) {
				size_t len = obj->name_len + 8;
				char *data = (char *)zmalloc(len + 1);
				memset(data, 0, len + 1);
				sprintf(data, "\"%s\":true", obj->name);
				return data;
			} else {
				return zstrdup("true");
			}
			break;
		}
		case MTYPE_FALSE: {
			if (obj->name) {
				size_t len = obj->name_len + 10;
				char *data = (char *)zmalloc(len + 1);
				memset(data, 0, len + 1);
				sprintf(data, "\"%s\":false", obj->name);
				return data;
			} else {
				return zstrdup("false");
			}
			break;
		}
		case MTYPE_FLOAT:
		case MTYPE_INT: {
			if (obj->name) {
				size_t len = obj->name_len + obj->data_len + 4;
				char *data = (char *)zmalloc(len + 1);
				memset(data, 0, len + 1);
				sprintf(data, "\"%s\":%s", obj->name, (char *)obj->data);
				return data;
			} else {
				return zstrdup((char *)obj->data);
			}
			break;
		}
		case MTYPE_QUID:
		case MTYPE_STRING: {
			if (obj->name) {
				size_t len = obj->name_len + obj->data_len + 6;
				char *data = (char *)zmalloc(len + 1);
				memset(data, 0, len + 1);
				sprintf(data, "\"%s\":\"%s\"", obj->name, (char *)obj->data);
				return data;
			} else {
				size_t len = obj->data_len + 4;
				char *data = (char *)zmalloc(len + 1);
				memset(data, 0, len + 1);
				sprintf(data, "\"%s\"", (char *)obj->data);
				return data;
			}
			break;
		}
		case MTYPE_ARRAY: {
			size_t nsz = 0;

			if (!obj->size) {
				if (obj->name) {
					size_t len = obj->name_len + 8;
					char *data = (char *)zmalloc(len + 1);
					memset(data, 0, len + 1);
					sprintf(data, "\"%s\":null", obj->name);
					return data;
				} else {
					return zstrdup("null");
				}
			}

			unsigned int i;
			for (i = 0; i < obj->size; ++i) {
				char *elm = marshall_serialize(obj->child[i]);
				nsz += strlen(elm) + 2;
				zfree(elm);
			}

			if (obj->name)
				nsz += obj->name_len;

			nsz += obj->size + 2;

			char *data = (char *)zmalloc(nsz + 1);
			memset(data, 0, nsz + 1);
			size_t curr_sz = 0;

			if (obj->name) {
				sprintf(data, "\"%s\":", obj->name);
				curr_sz += obj->name_len;
			}

			strcat(data, "[");
			curr_sz++;

			for (i = 0; i < obj->size; ++i) {
				if (i > 0) {
					strcat(data, ",");
					curr_sz++;
				}
				char *elm = marshall_serialize(obj->child[i]);
				strcat(data, elm);
				curr_sz += strlen(elm);
				zfree(elm);
			}
			strcat(data, "]");
			return data;
		}
		case MTYPE_OBJECT: {
			size_t nsz = 0;

			if (!obj->size) {
				if (obj->name) {
					size_t len = obj->name_len + 8;
					char *data = (char *)zmalloc(len + 1);
					memset(data, 0, len + 1);
					sprintf(data, "\"%s\":null", obj->name);
					return data;
				} else {
					return zstrdup("null");
				}
			}

			unsigned int i;
			for (i = 0; i < obj->size; ++i) {
				char *elm = marshall_serialize(obj->child[i]);
				nsz += strlen(elm) + 2;
				zfree(elm);
			}

			if (obj->name)
				nsz += obj->name_len;

			nsz += obj->size + 2;

			char *data = (char *)zmalloc(nsz + 1);
			memset(data, 0, nsz + 1);
			size_t curr_sz = 0;

			if (obj->name) {
				sprintf(data, "\"%s\":", obj->name);
				curr_sz += obj->name_len;
			}

			strcat(data, "{");
			curr_sz++;

			for (i = 0; i < obj->size; ++i) {
				if (i > 0) {
					strcat(data, ",");
					curr_sz++;
				}
				char *elm = marshall_serialize(obj->child[i]);
				strcat(data, elm);
				curr_sz += strlen(elm);
				zfree(elm);
			}
			strcat(data, "}");
			return data;
		}
		default:
			break;
	}
	return NULL;
}

char *marshall_get_type(marshall_type_t type) {
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
