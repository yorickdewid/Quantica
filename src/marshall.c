#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"
#include "quid.h"
#include "json_check.h"
#include "zmalloc.h"
#include "marshall.h"

#define VECTOR_SIZE	1024

static marshall_t *marshall_obect_decode(char *data, size_t data_len, char *name, void *parent) {
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
	if (name)
		obj->name = name;

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
						if (strquid_format(obj->child[obj->size]->data) > 0)
							obj->child[obj->size]->type = MTYPE_QUID;
						obj->size++;
						break;
					case DICT_OBJECT: {
						obj->child[obj->size] = marshall_obect_decode(data + t[i].start, t[i].end - t[i].start, NULL, obj);
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
						obj->child[obj->size] = marshall_obect_decode(data + t[i].start, t[i].end - t[i].start, NULL, obj);
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
							setname = 1;
						} else {
							obj->child[obj->size]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, obj);
							if (strquid_format(obj->child[obj->size]->data) > 0)
								obj->child[obj->size]->type = MTYPE_QUID;
							obj->size++;
							setname = 0;
						}
						break;
					case DICT_OBJECT: {
						obj->child[obj->size] = marshall_obect_decode(data + t[i].start, t[i].end - t[i].start, obj->child[obj->size]->name, obj);
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
						obj->child[obj->size] = marshall_obect_decode(data + t[i].start, t[i].end - t[i].start, obj->child[obj->size]->name, obj);
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
	return MTYPE_STRING;
}

/* Count elements by level */
unsigned int marshall_get_count(marshall_t *obj, int depth, unsigned _depth) {
	switch (obj->type) {
		case MTYPE_NULL:
		case MTYPE_TRUE:
		case MTYPE_FALSE:
		case MTYPE_FLOAT:
		case MTYPE_INT:
		case MTYPE_QUID:
		case MTYPE_STRING:
			return 1;
		case MTYPE_ARRAY:
		case MTYPE_OBJECT: {
			unsigned int n = 1;
			if (depth == -1 || ((unsigned int)depth) > _depth) {
				for (unsigned int i = 0; i < obj->size; ++i) {
					n += marshall_get_count(obj->child[i], depth, _depth + 1);
				}
			}
			return n;
		}
		default:
			return 0;
	}
	return 0;
}

/* Convert string to object */
marshall_t *marshall_convert(char *data, size_t data_len) {
	marshall_t *marshall = NULL;
	marshall_type_t type = autoscalar(data, data_len);

	switch (type) {
		case MTYPE_NULL:
		case MTYPE_TRUE:
		case MTYPE_FALSE: {
			marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), NULL);
			memset(marshall, 0, sizeof(marshall_t));
			marshall->type = type;
			break;
		}
		case MTYPE_INT:
		case MTYPE_FLOAT:
		case MTYPE_STRING:
		case MTYPE_QUID: {
			marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), NULL);
			memset(marshall, 0, sizeof(marshall_t));
			marshall->data = tree_zstrndup(data, data_len, marshall);
			marshall->type = type;
			break;
		}
		case MTYPE_ARRAY:
		case MTYPE_OBJECT:
			marshall = marshall_obect_decode(data, data_len, NULL, NULL);
			break;
		default:
			//TODO: throw error
			break;
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
				size_t len = strlen(obj->name) + 8;
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
				size_t len = strlen(obj->name) + 8;
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
				size_t len = strlen(obj->name) + 10;
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
				size_t len = strlen(obj->name) + strlen((char *)obj->data) + 4;
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
				size_t len = strlen(obj->name) + strlen((char *)obj->data) + 6;
				char *data = (char *)zmalloc(len + 1);
				memset(data, 0, len + 1);
				sprintf(data, "\"%s\":\"%s\"", obj->name, (char *)obj->data);
				return data;
			} else {
				size_t len = strlen((char *)obj->data) + 4;
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
					size_t len = strlen(obj->name) + 8;
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
				nsz += strlen(obj->name);

			nsz += obj->size + 2;

			char *data = (char *)zmalloc(nsz + 1);
			memset(data, 0, nsz + 1);
			size_t curr_sz = 0;

			if (obj->name) {
				sprintf(data, "\"%s\":", obj->name);
				curr_sz += strlen(obj->name);
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
					size_t len = strlen(obj->name) + 8;
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
				nsz += strlen(obj->name);

			nsz += obj->size + 2;

			char *data = (char *)zmalloc(nsz + 1);
			memset(data, 0, nsz + 1);
			size_t curr_sz = 0;

			if (obj->name) {
				sprintf(data, "\"%s\":", obj->name);
				curr_sz += strlen(obj->name);
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
