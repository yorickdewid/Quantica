#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"
#include "zmalloc.h"
#include "marshall.h"

slay_serialize_t *marshall_decode(char *data, size_t data_len, char *name, void *parent) {
	dict_parser p;
	dict_token_t t[data_len];

	dict_init(&p);
	int o = dict_parse(&p, data, data_len, t, data_len);
	if (o < 1)
		return NULL;

	slay_serialize_t *rtobj = (slay_serialize_t *)tree_zmalloc(sizeof(slay_serialize_t), parent);
	memset(rtobj, 0, sizeof(slay_serialize_t));
	rtobj->child = (struct slay_serialize **)tree_zmalloc(o * sizeof(struct slay_serialize *), rtobj);
	memset(rtobj->child, 0, o * sizeof(struct slay_serialize *));
	if (name)
		rtobj->name = name;

	switch (t[0].type) {
		case DICT_ARRAY: {
			int i;
			rtobj->type = DICT_ARR;
			for (i = 1; i < o; ++i) {
				switch (t[i].type) {
					case DICT_PRIMITIVE:
						if (dict_cmp(data, &t[i], "null")) {
							rtobj->child[rtobj->sz] = tree_zmalloc(sizeof(slay_serialize_t), rtobj);
							memset(rtobj->child[rtobj->sz], 0, sizeof(slay_serialize_t));
							rtobj->child[rtobj->sz]->type = DICT_NULL;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = NULL;
							rtobj->child[rtobj->sz]->data = NULL;
							rtobj->sz++;
						} else if (dict_cmp(data, &t[i], "true")) {
							rtobj->child[rtobj->sz] = tree_zmalloc(sizeof(slay_serialize_t), rtobj);
							memset(rtobj->child[rtobj->sz], 0, sizeof(slay_serialize_t));
							rtobj->child[rtobj->sz]->type = DICT_TRUE;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = NULL;
							rtobj->child[rtobj->sz]->data = NULL;
							rtobj->sz++;
						} else if (dict_cmp(data, &t[i], "false")) {
							rtobj->child[rtobj->sz] = tree_zmalloc(sizeof(slay_serialize_t), rtobj);
							memset(rtobj->child[rtobj->sz], 0, sizeof(slay_serialize_t));
							rtobj->child[rtobj->sz]->type = DICT_FALSE;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = NULL;
							rtobj->child[rtobj->sz]->data = NULL;
							rtobj->sz++;
						} else {
							rtobj->child[rtobj->sz] = tree_zmalloc(sizeof(slay_serialize_t), rtobj);
							memset(rtobj->child[rtobj->sz], 0, sizeof(slay_serialize_t));
							rtobj->child[rtobj->sz]->type = DICT_INT;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = NULL;
							rtobj->child[rtobj->sz]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
							rtobj->sz++;
						}
						break;
					case DICT_STRING:
						rtobj->child[rtobj->sz] = tree_zmalloc(sizeof(slay_serialize_t), rtobj);
						memset(rtobj->child[rtobj->sz], 0, sizeof(slay_serialize_t));
						rtobj->child[rtobj->sz]->type = DICT_STR;
						rtobj->child[rtobj->sz]->child = NULL;
						rtobj->child[rtobj->sz]->sz = 0;
						rtobj->child[rtobj->sz]->name = NULL;
						rtobj->child[rtobj->sz]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
						rtobj->sz++;
						break;
					case DICT_OBJECT: {
						rtobj->child[rtobj->sz] = marshall_decode(data + t[i].start, t[i].end - t[i].start, NULL, rtobj);
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
						rtobj->child[rtobj->sz] = marshall_decode(data + t[i].start, t[i].end - t[i].start, NULL, rtobj);
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
			rtobj->type = DICT_OBJ;
			unsigned char setname = 0;
			for (i = 1; i < o; ++i) {
				switch (t[i].type) {
					case DICT_PRIMITIVE:
						if (dict_cmp(data, &t[i], "null")) {
							rtobj->child[rtobj->sz]->type = DICT_NULL;
							rtobj->sz++;
							setname = 0;
						} else if (dict_cmp(data, &t[i], "true")) {
							rtobj->child[rtobj->sz]->type = DICT_TRUE;
							rtobj->sz++;
							setname = 0;
						} else if (dict_cmp(data, &t[i], "false")) {
							rtobj->child[rtobj->sz]->type = DICT_FALSE;
							rtobj->sz++;
							setname = 0;
						} else {
							rtobj->child[rtobj->sz]->type = DICT_INT;
							rtobj->child[rtobj->sz]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
							rtobj->sz++;
							setname = 0;
						}
						break;
					case DICT_STRING:
						if (!setname) {
							rtobj->child[rtobj->sz] = tree_zmalloc(sizeof(slay_serialize_t), rtobj);
							memset(rtobj->child[rtobj->sz], 0, sizeof(slay_serialize_t));
							rtobj->child[rtobj->sz]->type = DICT_STR;
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
						rtobj->child[rtobj->sz] = marshall_decode(data + t[i].start, t[i].end - t[i].start, rtobj->child[rtobj->sz]->name, rtobj);
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
						rtobj->child[rtobj->sz] = marshall_decode(data + t[i].start, t[i].end - t[i].start, rtobj->child[rtobj->sz]->name, rtobj);
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