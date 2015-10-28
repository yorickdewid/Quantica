#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"
#include "quid.h"
#include "json_check.h"
#include "zmalloc.h"
#include "marshall.h"

#define VECTOR_SIZE	1024

static serialize_t *marshall_obect_decode(char *data, size_t data_len, char *name, void *parent) {
	dict_parser p;
	dict_token_t t[data_len];

	dict_init(&p);
	int o = dict_parse(&p, data, data_len, t, data_len);
	if (o < 1)
		return NULL;

	serialize_t *rtobj = (serialize_t *)tree_zmalloc(sizeof(serialize_t), parent);
	memset(rtobj, 0, sizeof(serialize_t));
	rtobj->child = (struct serialize **)tree_zmalloc(o * sizeof(struct serialize *), rtobj);
	memset(rtobj->child, 0, o * sizeof(struct serialize *));
	if (name)
		rtobj->name = name;

	switch (t[0].type) {
		case DICT_ARRAY: {
			int i;
			rtobj->type = MTYPE_ARRAY;
			for (i = 1; i < o; ++i) {
				switch (t[i].type) {
					case DICT_PRIMITIVE:
						if (dict_cmp(data, &t[i], "null")) {
							rtobj->child[rtobj->sz] = tree_zmalloc(sizeof(serialize_t), rtobj);
							memset(rtobj->child[rtobj->sz], 0, sizeof(serialize_t));
							rtobj->child[rtobj->sz]->type = MTYPE_NULL;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = NULL;
							rtobj->child[rtobj->sz]->data = NULL;
							rtobj->sz++;
						} else if (dict_cmp(data, &t[i], "true")) {
							rtobj->child[rtobj->sz] = tree_zmalloc(sizeof(serialize_t), rtobj);
							memset(rtobj->child[rtobj->sz], 0, sizeof(serialize_t));
							rtobj->child[rtobj->sz]->type = MTYPE_TRUE;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = NULL;
							rtobj->child[rtobj->sz]->data = NULL;
							rtobj->sz++;
						} else if (dict_cmp(data, &t[i], "false")) {
							rtobj->child[rtobj->sz] = tree_zmalloc(sizeof(serialize_t), rtobj);
							memset(rtobj->child[rtobj->sz], 0, sizeof(serialize_t));
							rtobj->child[rtobj->sz]->type = MTYPE_FALSE;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = NULL;
							rtobj->child[rtobj->sz]->data = NULL;
							rtobj->sz++;
						} else {
							rtobj->child[rtobj->sz] = tree_zmalloc(sizeof(serialize_t), rtobj);
							memset(rtobj->child[rtobj->sz], 0, sizeof(serialize_t));
							rtobj->child[rtobj->sz]->type = MTYPE_INT;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = NULL;
							rtobj->child[rtobj->sz]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
							rtobj->sz++;
						}
						break;
					case DICT_STRING:
						rtobj->child[rtobj->sz] = tree_zmalloc(sizeof(serialize_t), rtobj);
						memset(rtobj->child[rtobj->sz], 0, sizeof(serialize_t));
						rtobj->child[rtobj->sz]->type = MTYPE_STRING;
						rtobj->child[rtobj->sz]->child = NULL;
						rtobj->child[rtobj->sz]->sz = 0;
						rtobj->child[rtobj->sz]->name = NULL;
						rtobj->child[rtobj->sz]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
						rtobj->sz++;
						break;
					case DICT_OBJECT: {
						rtobj->child[rtobj->sz] = marshall_obect_decode(data + t[i].start, t[i].end - t[i].start, NULL, rtobj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						rtobj->sz++;
						break;
					}
					case DICT_ARRAY: {
						rtobj->child[rtobj->sz] = marshall_obect_decode(data + t[i].start, t[i].end - t[i].start, NULL, rtobj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						rtobj->sz++;
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
			rtobj->type = MTYPE_OBJECT;
			unsigned char setname = 0;
			for (i = 1; i < o; ++i) {
				switch (t[i].type) {
					case DICT_PRIMITIVE:
						if (dict_cmp(data, &t[i], "null")) {
							rtobj->child[rtobj->sz]->type = MTYPE_NULL;
							rtobj->sz++;
							setname = 0;
						} else if (dict_cmp(data, &t[i], "true")) {
							rtobj->child[rtobj->sz]->type = MTYPE_TRUE;
							rtobj->sz++;
							setname = 0;
						} else if (dict_cmp(data, &t[i], "false")) {
							rtobj->child[rtobj->sz]->type = MTYPE_FALSE;
							rtobj->sz++;
							setname = 0;
						} else {
							rtobj->child[rtobj->sz]->type = MTYPE_INT;
							rtobj->child[rtobj->sz]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
							rtobj->sz++;
							setname = 0;
						}
						break;
					case DICT_STRING:
						if (!setname) {
							rtobj->child[rtobj->sz] = tree_zmalloc(sizeof(serialize_t), rtobj);
							memset(rtobj->child[rtobj->sz], 0, sizeof(serialize_t));
							rtobj->child[rtobj->sz]->type = MTYPE_STRING;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
							rtobj->child[rtobj->sz]->data = NULL;
							setname = 1;
						} else {
							rtobj->child[rtobj->sz]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
							rtobj->sz++;
							setname = 0;
						}
						break;
					case DICT_OBJECT: {
						rtobj->child[rtobj->sz] = marshall_obect_decode(data + t[i].start, t[i].end - t[i].start, rtobj->child[rtobj->sz]->name, rtobj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						rtobj->sz++;
						setname = 0;
						break;
					}
					case DICT_ARRAY: {
						rtobj->child[rtobj->sz] = marshall_obect_decode(data + t[i].start, t[i].end - t[i].start, rtobj->child[rtobj->sz]->name, rtobj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						rtobj->sz++;
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
	return rtobj;
}

static serialize_type_t autoscalar(const char *data, size_t len) {
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
	if (b!=-1)
		return b ? MTYPE_TRUE : MTYPE_FALSE;
	if (strisdigit((char *)data))
		return MTYPE_INT;
	if (strismatch(data, "1234567890.")) {
		if(strccnt(data, '.') == 1)
			if (data[0] != '.' && data[len-1] != '.')
				return MTYPE_FLOAT;
	}
	if (strquid_format(data)>0)
		return MTYPE_QUID;
	if (json_valid(data))
		return MTYPE_OBJECT;
    return MTYPE_STRING;
}

marshall_t *marshall_convert(char *data, size_t data_len) {
	serialize_type_t type = autoscalar(data, data_len);
	serialize_t *serial = NULL;

	switch (type) {
		case MTYPE_NULL:
		case MTYPE_TRUE:
		case MTYPE_FALSE: {
			break;
		}
		case MTYPE_INT:
		case MTYPE_FLOAT:
		case MTYPE_STRING:
		case MTYPE_QUID: {
			serial = (serialize_t *)tree_zmalloc(sizeof(serialize_t), NULL);
			memset(serial, 0, sizeof(serialize_t));
			serial->data = tree_zstrndup(data, data_len, serial);
			serial->sz = 1;
			serial->type = type;
			break;
		}
		case MTYPE_ARRAY:
		case MTYPE_OBJECT:
			serial = marshall_obect_decode(data, data_len, NULL, NULL);
			break;
		default:
			//TODO: throw error
			break;
	}

	marshall_t *marshall = (marshall_t *)zcalloc(1, sizeof(marshall_t));
	marshall->type = type;
	marshall->data = serial;
	return marshall;
}

static char *marshall_object_serialize(serialize_t *obj) {
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
			unsigned int i;
			for (i = 0; i < obj->sz; ++i) {
				char *elm = marshall_object_serialize(obj->child[i]);
				nsz += strlen(elm) + 2;
				zfree(elm);
			}

			if (obj->name)
				nsz += strlen(obj->name);

			nsz += obj->sz + 2;

			char *data = (char *)zmalloc(nsz + 1);
			memset(data, 0, nsz + 1);
			size_t curr_sz = 0;

			if (obj->name) {
				sprintf(data, "\"%s\":", obj->name);
				curr_sz += strlen(obj->name);
			}

			strcat(data, "[");
			curr_sz++;

			for (i = 0; i < obj->sz; ++i) {
				if (i > 0) {
					strcat(data, ",");
					curr_sz++;
				}
				char *elm = marshall_object_serialize(obj->child[i]);
				strcat(data, elm);
				curr_sz += strlen(elm);
				zfree(elm);
			}
			strcat(data, "]");
			return data;
		}
		case MTYPE_OBJECT: {
			size_t nsz = 0;
			unsigned int i;
			for (i = 0; i < obj->sz; ++i) {
				char *elm = marshall_object_serialize(obj->child[i]);
				nsz += strlen(elm) + 2;
				zfree(elm);
			}

			if (obj->name)
				nsz += strlen(obj->name);

			nsz += obj->sz + 2;

			char *data = (char *)zmalloc(nsz + 1);
			memset(data, 0, nsz + 1);
			size_t curr_sz = 0;

			if (obj->name) {
				sprintf(data, "\"%s\":", obj->name);
				curr_sz += strlen(obj->name);
			}

			strcat(data, "{");
			curr_sz++;

			for (i = 0; i < obj->sz; ++i) {
				if (i > 0) {
					strcat(data, ",");
					curr_sz++;
				}
				char *elm = marshall_object_serialize(obj->child[i]);
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

char *marshall_serialize(marshall_t *marshall) {
	char *data = NULL;

	switch (marshall->type) {
		case MTYPE_NULL:
			data = zstrdup("null");
			break;
		case MTYPE_TRUE:
			data = zstrdup("true");
			break;
		case MTYPE_FALSE:
			data = zstrdup("false");
			break;
		case MTYPE_INT:
		case MTYPE_FLOAT:
		case MTYPE_STRING:
		case MTYPE_QUID:
			data = zstrdup(marshall->data->data);
			break;
		case MTYPE_ARRAY:
		case MTYPE_OBJECT:
			data = marshall_object_serialize(marshall->data);
			break;
		default:
			//TODO: throw error
			break;
	}

	return data;
}