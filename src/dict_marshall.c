#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <error.h>
#include "dict.h"
#include "quid.h"
#include "zmalloc.h"
#include "marshall.h"
#include "dict_marshall.h"

/*
 * Recursively parse dictionary into marshall object
 */
marshall_t *marshall_dict_decode(char *data, size_t data_len, char *name, size_t name_len, void *parent) {
	dict_parser p;
	dict_token_t t[data_len];

	dict_init(&p);
	int o = dict_parse(&p, data, data_len, t, data_len);
	if (o < 1)
		return NULL;

	marshall_t *obj = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), parent);
	obj->child = (marshall_t **)tree_zcalloc(o, sizeof(marshall_t *), obj);

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
						obj->child[obj->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), obj);
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
							if (strismatch(obj->child[obj->size]->data, "-1234567890.")) {
								if (strccnt(obj->child[obj->size]->data, '.') == 1) {
									obj->child[obj->size]->type = MTYPE_FLOAT;
								}
							}
						}
						obj->child[obj->size]->size = 1;
						obj->size++;
						break;
					case DICT_STRING:
						obj->child[obj->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), obj);
						obj->child[obj->size]->type = MTYPE_STRING;
						obj->child[obj->size]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, obj);
						obj->child[obj->size]->data_len = t[i].end - t[i].start;
						if (strquid_format(obj->child[obj->size]->data) > 0)
							obj->child[obj->size]->type = MTYPE_QUID;
						obj->child[obj->size]->size = 1;
						obj->size++;
						break;
					case DICT_OBJECT: {
						obj->child[obj->size] = marshall_dict_decode(data + t[i].start, t[i].end - t[i].start, NULL, 0, obj);
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
						obj->child[obj->size] = marshall_dict_decode(data + t[i].start, t[i].end - t[i].start, NULL, 0, obj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						obj->size++;
						break;
					}
					default:
						error_throw("70bef771b0a3", "Invalid datatype");
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
							if (strismatch(obj->child[obj->size]->data, "-1234567890.")) {
								if (strccnt(obj->child[obj->size]->data, '.') == 1) {
									obj->child[obj->size]->type = MTYPE_FLOAT;
								}
							}
						}
						obj->child[obj->size]->size = 1;
						obj->size++;
						setname = 0;
						break;
					case DICT_STRING:
						if (!setname) {
							obj->child[obj->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), obj);
							obj->child[obj->size]->type = MTYPE_STRING;
							obj->child[obj->size]->name = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, obj);
							obj->child[obj->size]->name_len = t[i].end - t[i].start;
							setname = 1;
						} else {
							obj->child[obj->size]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, obj);
							obj->child[obj->size]->data_len = t[i].end - t[i].start;
							if (strquid_format(obj->child[obj->size]->data) > 0)
								obj->child[obj->size]->type = MTYPE_QUID;
							obj->child[obj->size]->size = 1;
							obj->size++;
							setname = 0;
						}
						break;
					case DICT_OBJECT: {
						obj->child[obj->size] = marshall_dict_decode(data + t[i].start, t[i].end - t[i].start, obj->child[obj->size]->name, obj->child[obj->size]->name_len, obj);
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
						obj->child[obj->size] = marshall_dict_decode(data + t[i].start, t[i].end - t[i].start, obj->child[obj->size]->name, obj->child[obj->size]->name_len, obj);
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
						error_throw("70bef771b0a3", "Invalid datatype");
						break;
				}
			}
			break;
		}
		default:
			error_throw("70bef771b0a3", "Invalid datatype");
			break;
	}
	return obj;
}

/*
 * Convert marshall object to string
 */
char *marshall_serialize(marshall_t *obj) {
	if (!obj)
		return zstrdup("null");

	switch (obj->type) {
		case MTYPE_NULL: {
			if (obj->name) {
				size_t len = obj->name_len + 8;
				char *data = (char *)zcalloc(len + 1, sizeof(char));
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
				char *data = (char *)zcalloc(len + 1, sizeof(char));
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
				char *data = (char *)zcalloc(len + 1, sizeof(char));
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
				char *data = (char *)zcalloc(len + 1, sizeof(char));
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
				char *data = (char *)zcalloc(len + 1, sizeof(char));
				memset(data, 0, len + 1);
				sprintf(data, "\"%s\":\"%s\"", obj->name, (char *)obj->data);
				return data;
			} else {
				size_t nlen = 0;
				char *escdata = stresc(obj->data, &nlen);
				size_t len = obj->data_len + nlen + 4;
				char *data = (char *)zcalloc(len + 1, sizeof(char));
				memset(data, 0, len + 1);
				sprintf(data, "\"%s\"", (char *)escdata);
				zfree(escdata);
				return data;
			}
			break;
		}
		case MTYPE_ARRAY: {
			size_t nsz = 0;

			if (!obj->size) {
				if (obj->name) {
					size_t len = obj->name_len + 8;
					char *data = (char *)zcalloc(len + 1, sizeof(char));
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
				if (elm) {
					nsz += strlen(elm) + 2;
					zfree(elm);
				} else {
					nsz += 5;
				}
			}

			if (obj->name)
				nsz += obj->name_len;

			nsz += obj->size + 2;

			char *data = (char *)zcalloc(nsz + 1, sizeof(char));
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
				if (elm) {
					strcat(data, elm);
					curr_sz += strlen(elm);
					zfree(elm);
				} else {
					strcat(data, "null");
					curr_sz += 5;
				}
			}
			strcat(data, "]");
			return data;
		}
		case MTYPE_OBJECT: {
			size_t nsz = 0;

			if (!obj->size) {
				if (obj->name) {
					size_t len = obj->name_len + 8;
					char *data = (char *)zcalloc(len + 1, sizeof(char));
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
				if (elm) {
					nsz += strlen(elm) + 2;
					zfree(elm);
				} else {
					nsz += 5;
				}
			}

			if (obj->name)
				nsz += obj->name_len;

			nsz += obj->size + 2;

			char *data = (char *)zcalloc(nsz + 1, sizeof(char));
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
				if (elm) {
					strcat(data, elm);
					curr_sz += strlen(elm);
					zfree(elm);
				} else {
					strcat(data, "null");
					curr_sz += 5;
				}
			}
			strcat(data, "}");
			return data;
		}
		default:
			error_throw("70bef771b0a3", "Invalid datatype");
			return NULL;
	}
	return NULL;
}
