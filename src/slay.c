#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <log.h>
#include <error.h>
#include "dstype.h"
#include "slay.h"
#include "quid.h"
#include "dict.h"
#include "marshall.h"
#include "json_encode.h"
#include "core.h"
#include "zmalloc.h"

#define VECTOR_SIZE	1024

// DEPRECATED
void slay_parse_object(char *data, size_t data_len, size_t *slay_len, struct slay_result *rs) {
	int i;
	int r;
	dict_parser p;
	dict_token_t t[data_len];

	dict_init(&p);
	r = dict_parse(&p, data, data_len, t, data_len);
	if (r < 1) {
		lprint("[erro] Failed to parse dict\n");
		return;
	}

	if (t[0].type == DICT_ARRAY) {
		int cnt = 0;
		int objcnt = 0;

		dict_levelcount(t, 0, 2, &cnt);
		rs->items = cnt;

		int cobj[cnt][2];
		rs->slay = create_row(SCHEMA_ARRAY, cnt, data_len, slay_len);
		void *next = movetodata_row(rs->slay);
		for (i = 1; i < r; ++i) {
			if (t[i].type == DICT_PRIMITIVE) {
				if (dict_cmp(data, &t[i], "null")) {
					next = slay_wrap(next, NULL, 0, NULL, 0, DT_NULL);
				} else if (dict_cmp(data, &t[i], "true")) {
					next = slay_wrap(next, NULL, 0, NULL, 0, DT_BOOL_T);
				} else if (dict_cmp(data, &t[i], "false")) {
					next = slay_wrap(next, NULL, 0, NULL, 0, DT_BOOL_F);
				} else {
					next = slay_wrap(next, NULL, 0, data + t[i].start, t[i].end - t[i].start, DT_INT);
				}
			} else if (t[i].type == DICT_STRING) {
				char *squid = data + t[i].start;
				squid[t[i].end - t[i].start] = '\0';
				if (strquid_format(data + t[i].start) > 0)
					next = slay_wrap(next, NULL, 0, data + t[i].start, t[i].end - t[i].start, DT_QUID);
				else
					next = slay_wrap(next, NULL, 0, data + t[i].start, t[i].end - t[i].start, DT_TEXT);
			} else if (t[i].type == DICT_OBJECT) {
				cobj[objcnt][0] = t[i].start;
				cobj[objcnt][1] = (t[i].end - t[i].start);
				next = slay_wrap(next, NULL, 0, data + t[i].start, t[i].end - t[i].start, DT_JSON);
				int x, j = 0;
				for (x = 0; x < t[i].size; x++) {
					j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
					j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
				}
				i += j;
				objcnt++;
			} else if (t[i].type == DICT_ARRAY) {
				next = slay_wrap(next, NULL, 0, data + t[i].start, t[i].end - t[i].start, DT_JSON);
				int x, j = 0;
				for (x = 0; x < t[i].size; x++) {
					j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
				}
				i += j;
			}
		}
		if (objcnt == cnt) {
			zfree(rs->slay);

			rs->slay = create_row(SCHEMA_TABLE, objcnt, (objcnt * QUID_LENGTH), slay_len);
			rs->table = TRUE;
			void *_next = movetodata_row(rs->slay);
			int rows = 0;
			struct slay_result crs;
			for (; rows < objcnt; ++rows) {
				size_t clen = 0;
				slay_parse_object(data + cobj[rows][0], cobj[rows][1], &clen, &crs);
				char squid[QUID_LENGTH + 1];
				_db_put(squid, crs.slay, clen);
				_next = slay_wrap(_next, NULL, 0, squid, QUID_LENGTH, DT_QUID);
			}
		}
	} else if (t[0].type == DICT_OBJECT) {
		int cnt = 0;
		dict_levelcount(t, 0, 2, &cnt);
		cnt /= 2;
		rs->items = cnt;

		rs->slay = create_row(SCHEMA_OBJECT, cnt, data_len, slay_len);
		void *next = movetodata_row(rs->slay);
		for (i = 1; i < r; ++i) {
			if (i % 2 == 0) {
				if (t[i].type == DICT_PRIMITIVE) {
					if (dict_cmp(data, &t[i], "null")) {
						next = slay_wrap(next, data + t[i - 1].start, t[i - 1].end - t[i - 1].start, NULL, 0, DT_NULL);
					} else if (dict_cmp(data, &t[i], "true")) {
						next = slay_wrap(next, data + t[i - 1].start, t[i - 1].end - t[i - 1].start, NULL, 0, DT_BOOL_T);
					} else if (dict_cmp(data, &t[i], "false")) {
						next = slay_wrap(next, data + t[i - 1].start, t[i - 1].end - t[i - 1].start, NULL, 0, DT_BOOL_F);
					} else {
						next = slay_wrap(next, data + t[i - 1].start, t[i - 1].end - t[i - 1].start, data + t[i].start, t[i].end - t[i].start, DT_INT);
					}
				} else if (t[i].type == DICT_STRING) {
					char *squid = data + t[i].start;
					squid[t[i].end - t[i].start] = '\0';
					if (strquid_format(data + t[i].start) > 0)
						next = slay_wrap(next, data + t[i - 1].start, t[i - 1].end - t[i - 1].start, data + t[i].start, t[i].end - t[i].start, DT_QUID);
					else
						next = slay_wrap(next, data + t[i - 1].start, t[i - 1].end - t[i - 1].start, data + t[i].start, t[i].end - t[i].start, DT_TEXT);
				} else if (t[i].type == DICT_OBJECT) {
					next = slay_wrap(next, data + t[i - 1].start, t[i - 1].end - t[i - 1].start, data + t[i].start, t[i].end - t[i].start, DT_JSON);
					int x, j = 0;
					for (x = 0; x < t[i].size; x++) {
						j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
					}
					i += j;
				} else if (t[i].type == DICT_ARRAY) {
					next = slay_wrap(next, data + t[i - 1].start, t[i - 1].end - t[i - 1].start, data + t[i].start, t[i].end - t[i].start, DT_JSON);
					int x, j = 0;
					for (x = 0; x < t[i].size; x++) {
						j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
					}
					i += j;
				}
			}
		}
	}
}

// DEPRECATED by slay_put()
void *slay_parse_quid(char *data, size_t data_len, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, data_len, slay_len);

	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, data, data_len, DT_QUID);
	return slay;
}

// DEPRECATED by slay_put()
void *slay_parse_text(char *data, size_t data_len, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, data_len, slay_len);

	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, data, data_len, DT_TEXT);
	return slay;
}

// DEPRECATED by slay_put()
void *slay_bool(bool boolean, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, 0, slay_len);

	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, NULL, 0, boolean ? DT_BOOL_T : DT_BOOL_F);
	return slay;
}

// DEPRECATED by slay_put()
void *slay_null(size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, 0, slay_len);

	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, NULL, 0, DT_NULL);
	return slay;
}

// DEPRECATED by slay_put()
void *slay_char(char *data, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, 1, slay_len);

	((uint8_t *)data)[1] = '\0';
	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, data, 1, DT_CHAR);
	return slay;
}

// DEPRECATED by slay_put()
void *slay_float(char *data, size_t data_len, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, data_len, slay_len);

	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, data, data_len, DT_FLOAT);
	return slay;
}

// DEPRECATED by slay_put()
void *slay_integer(char *data, size_t data_len, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, data_len, slay_len);

	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, data, data_len, DT_INT);
	return slay;
}

int object_descent_count(marshall_t *obj) {
	int cnt = 0;
	for (unsigned int i = 0; i < obj->size; ++i) {
		if (obj->child[i]->type == MTYPE_OBJECT || obj->child[i]->type == MTYPE_ARRAY)
			cnt++;
	}
	return cnt;
}

//TODO rewrite switches
void *slay_put(marshall_t *marshall, size_t *len, slay_result_t *rs) {
	void *slay = NULL;

	switch (marshall->type) {
		case MTYPE_NULL:
		case MTYPE_TRUE:
		case MTYPE_FALSE: {
			rs->schema = SCHEMA_FIELD;
			rs->items = 1;

			slay = create_row(rs->schema, 1, 0, len);
			if (marshall->type == MTYPE_NULL)
				slay_wrap(movetodata_row(slay), NULL, 0, NULL, 0, DT_NULL);
			else if (marshall->type == MTYPE_TRUE)
				slay_wrap(movetodata_row(slay), NULL, 0, NULL, 0, DT_BOOL_T);
			else if (marshall->type == MTYPE_FALSE)
				slay_wrap(movetodata_row(slay), NULL, 0, NULL, 0, DT_BOOL_F);
			return slay;
		}
		case MTYPE_INT:
		case MTYPE_FLOAT:
		case MTYPE_STRING:
		case MTYPE_QUID: {
			rs->schema = SCHEMA_FIELD;
			rs->items = 1;

			slay = create_row(rs->schema, 1, strlen(marshall->data), len);
			if (marshall->type == MTYPE_INT)
				slay_wrap(movetodata_row(slay), NULL, 0, marshall->data, strlen(marshall->data), DT_INT);
			else if (marshall->type == MTYPE_FLOAT)
				slay_wrap(movetodata_row(slay), NULL, 0, marshall->data, strlen(marshall->data), DT_FLOAT);
			else if (marshall->type == MTYPE_STRING)
				slay_wrap(movetodata_row(slay), NULL, 0, marshall->data, strlen(marshall->data), DT_TEXT);
			else if (marshall->type == MTYPE_QUID)
				slay_wrap(movetodata_row(slay), NULL, 0, marshall->data, strlen(marshall->data), DT_QUID);
			return slay;
		}
		case MTYPE_ARRAY: 
		case MTYPE_OBJECT: {
			if (marshall->type == MTYPE_ARRAY) {
				rs->schema = SCHEMA_ARRAY;
			} else if (marshall->type == MTYPE_OBJECT)
				rs->schema = SCHEMA_OBJECT;

			size_t desccnt = object_descent_count(marshall);
			rs->items = marshall_get_count(marshall, 1, 0) - 1;
			//TODO sz should be counter by traversal {
			char *dataserial = marshall_serialize(marshall);
			size_t data_len = strlen(dataserial);
			zfree(dataserial);
			// }
			data_len += desccnt * QUID_LENGTH;
			if (desccnt == rs->items && rs->items > 1) {
				data_len = rs->items * QUID_LENGTH;
				if (rs->schema == SCHEMA_ARRAY)
					rs->schema = SCHEMA_TABLE;
				else
					rs->schema = SCHEMA_SET;

				for (unsigned int i = 0; i < marshall->size; ++i) {
					data_len += strlen(marshall->child[i]->name);
				}
			}

			slay = create_row(rs->schema, rs->items, data_len, len);
			void *next = movetodata_row(slay);
			for (unsigned int i = 0; i < marshall->size; ++i) {
				char *name = NULL;
				size_t name_len = 0;
				if (marshall->child[i]->name) {
					name = marshall->child[i]->name;
					name_len = strlen(marshall->child[i]->name);
				}

				switch (marshall->child[i]->type) {
					case MTYPE_NULL:
					case MTYPE_TRUE:
					case MTYPE_FALSE: {
						if (marshall->child[i]->type == MTYPE_NULL)
							next = slay_wrap(next, name, name_len, NULL, 0, DT_NULL);
						else if (marshall->child[i]->type == MTYPE_TRUE)
							next = slay_wrap(next, name, name_len, NULL, 0, DT_BOOL_T);
						else if (marshall->child[i]->type == MTYPE_FALSE)
							next = slay_wrap(next, name, name_len, NULL, 0, DT_BOOL_F);
						break;
					}
					case MTYPE_INT:
					case MTYPE_FLOAT:
					case MTYPE_STRING:
					case MTYPE_QUID: {
						if (marshall->child[i]->type == MTYPE_INT)
							next = slay_wrap(next, name, name_len, marshall->child[i]->data, strlen(marshall->child[i]->data), DT_INT);
						else if (marshall->child[i]->type == MTYPE_FLOAT)
							next = slay_wrap(next, name, name_len, marshall->child[i]->data, strlen(marshall->child[i]->data), DT_FLOAT);
						else if (marshall->child[i]->type == MTYPE_STRING)
							next = slay_wrap(next, name, name_len, marshall->child[i]->data, strlen(marshall->child[i]->data), DT_TEXT);
						else if (marshall->child[i]->type == MTYPE_QUID)
							next = slay_wrap(next, name, name_len, marshall->child[i]->data, strlen(marshall->child[i]->data), DT_QUID);
						break;
					}
					case MTYPE_ARRAY:
					case MTYPE_OBJECT: {
						size_t _len = 0;
						slay_result_t _rs;
						char squid[QUID_LENGTH + 1];
						void *_slay = slay_put(marshall->child[i], &_len, &_rs);
						_db_put(squid, _slay, _len);
						next = slay_wrap(next, name, name_len, squid, QUID_LENGTH, DT_QUID);
						break;
					}
					default:
						break;
				}
			}

			return slay;
		}
		default:
			//TODO: throw error
			break;
	}

	return slay;
}

// DEPRECATED by slay_put()
void slay_put_data(char *data, size_t data_len, size_t *len, struct slay_result *rs) {
	rs->slay = NULL;
	dstype_t adt = autotype(data, data_len);
	switch (adt) {
		case DT_QUID:
			rs->slay = slay_parse_quid((char *)data, data_len, len);
			rs->items = 1;
			break;
		case DT_JSON: {
			slay_parse_object(data, data_len, len, rs);
			break;
		}
		case DT_NULL:
			rs->slay = slay_null(len);
			break;
		case DT_CHAR:
			rs->slay = slay_char((char *)data, len);
			rs->items = 1;
			break;
		case DT_BOOL_F:
			rs->slay = slay_bool(FALSE, len);
			break;
		case DT_BOOL_T:
			rs->slay = slay_bool(TRUE, len);
			break;
		case DT_FLOAT:
			rs->slay = slay_float((char *)data, data_len, len);
			rs->items = 1;
			break;
		case DT_INT:
			rs->slay = slay_integer((char *)data, data_len, len);
			rs->items = 1;
			break;
		case DT_TEXT:
			rs->slay = slay_parse_text((char *)data, data_len, len);
			rs->items = 1;
			break;
		default:
			rs->slay = slay_null(len);
			break;
	}
}

dict_t *resolv_quid(vector_t *v, char *buf, size_t buflen, char *name, dstype_t dt) {
	switch (dt) {
		case DT_QUID: {
			//dstype_t _dt;
			//char *rbuf = NULL;//_db_get(buf, &_dt);
			//size_t rbuflen = strlen(rbuf);
			return NULL;//resolv_quid(v, rbuf, rbuflen, name, _dt);
		}
		case DT_NULL:
			zfree(buf);
			return dict_element_cnew(v, FALSE, name, "null");
		case DT_BOOL_F:
			zfree(buf);
			return dict_element_cnew(v, FALSE, name, "false");
		case DT_BOOL_T:
			zfree(buf);
			return dict_element_cnew(v, FALSE, name, "true");
		case DT_FLOAT:
		case DT_INT:
		case DT_JSON: {
			buf = (char *)zrealloc(buf, buflen + 1);
			((char *)buf)[buflen] = '\0';
			dict_t *elm = dict_element_new(v, FALSE, name, buf);
			zfree(buf);
			return elm;
		}
		case DT_CHAR:
		case DT_TEXT: {
			buf = (char *)zrealloc(buf, buflen + 1);
			((char *)buf)[buflen] = '\0';
			dict_t *elm = dict_element_new(v, FALSE, name, buf);
			zfree(buf);
			return elm;
		}
		default:
			zfree(buf);
			return dict_element_cnew(v, FALSE, name, "null");

	}
	return dict_element_cnew(v, FALSE, name, "null");
}

marshall_t *slay_get(void *data, void *parent) {
	marshall_t *marshall = NULL;
	uint64_t elements;
	schema_t schema;
	void *name = NULL;
	size_t namelen;
	size_t val_len;
	dstype_t val_dt;

	void *slay = get_row(data, &schema, &elements);
	void *next = movetodata_row(slay);

	switch (schema) {
		case SCHEMA_FIELD: {
			void *val_data = slay_unwrap(next, &name, &namelen, &val_len, &val_dt);
			if (val_data) {
				val_data = (void *)zrealloc(val_data, val_len + 1);
				((uint8_t *)val_data)[val_len] = '\0';
			}

			if (val_dt == DT_QUID) {
				marshall = _db_get(val_data, NULL);
				if (!marshall) {
					marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
					memset(marshall, 0, sizeof(marshall_t));
					marshall->type = MTYPE_NULL;
					marshall->size = 1;
				}
				zfree(val_data);
				return marshall;
			}

			marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
			memset(marshall, 0, sizeof(marshall_t));

			if (val_dt == DT_NULL)
				marshall->type = MTYPE_NULL;
			else if (val_dt == DT_BOOL_T)
				marshall->type = MTYPE_TRUE;
			else if (val_dt == DT_BOOL_F)
				marshall->type = MTYPE_FALSE;
			else if (val_dt == DT_INT)
				marshall->type = MTYPE_INT;
			else if (val_dt == DT_FLOAT)
				marshall->type = MTYPE_FLOAT;
			else if (val_dt == DT_TEXT) {
				marshall->type = MTYPE_STRING;
				char *_tmp = stresc(val_data);
				zfree(val_data);
				val_data = _tmp;
			}

			marshall->data = tree_zstrndup(val_data, val_len, marshall);
			marshall->size = 1;
			if (val_data)
				zfree(val_data);
			break;
		}
		case SCHEMA_ARRAY: {
			marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
			memset(marshall, 0, sizeof(marshall_t));
			marshall->child = (marshall_t **)tree_zmalloc(elements * sizeof(marshall_t *), marshall);
			memset(marshall->child, 0, elements * sizeof(marshall_t *));
			marshall->type = MTYPE_ARRAY;
			
			for (unsigned int i = 0; i < elements; ++i) {
				void *val_data = slay_unwrap(next, &name, &namelen, &val_len, &val_dt);
				next = next_row(next);

				if (val_data) {
					val_data = (void *)zrealloc(val_data, val_len + 1);
					((uint8_t *)val_data)[val_len] = '\0';
				}

				if (val_dt == DT_QUID) {
					marshall->child[marshall->size] = _db_get(val_data, marshall);
					if (!marshall->child[marshall->size]) {
						marshall->child[marshall->size] = tree_zmalloc(sizeof(marshall_t), marshall);
						memset(marshall->child[marshall->size], 0, sizeof(marshall_t));
						marshall->child[marshall->size]->type = MTYPE_NULL;
						marshall->child[marshall->size]->size = 1;
					}
					marshall->size++;
					zfree(val_data);
					continue;
				}

				marshall->child[marshall->size] = tree_zmalloc(sizeof(marshall_t), marshall);
				memset(marshall->child[marshall->size], 0, sizeof(marshall_t));

				if (val_dt == DT_NULL)
					marshall->child[marshall->size]->type = MTYPE_NULL;
				else if (val_dt == DT_BOOL_T)
					marshall->child[marshall->size]->type = MTYPE_TRUE;
				else if (val_dt == DT_BOOL_F)
					marshall->child[marshall->size]->type = MTYPE_FALSE;
				else if (val_dt == DT_INT)
					marshall->child[marshall->size]->type = MTYPE_INT;
				else if (val_dt == DT_FLOAT)
					marshall->child[marshall->size]->type = MTYPE_FLOAT;
				else if (val_dt == DT_TEXT) {
					marshall->child[marshall->size]->type = MTYPE_STRING;
					char *_tmp = stresc(val_data);
					zfree(val_data);
					val_data = _tmp;
				}

				marshall->child[marshall->size]->data = tree_zstrndup(val_data, val_len, marshall);
				marshall->size++;
				if (val_data)
					zfree(val_data);
			}
			break;
		}
		case SCHEMA_OBJECT: {
			marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
			memset(marshall, 0, sizeof(marshall_t));
			marshall->child = (marshall_t **)tree_zmalloc(elements * sizeof(marshall_t *), marshall);
			memset(marshall->child, 0, elements * sizeof(marshall_t *));
			marshall->type = MTYPE_OBJECT;

			for (unsigned int i = 0; i < elements; ++i) {
				void *val_data = slay_unwrap(next, &name, &namelen, &val_len, &val_dt);
				next = next_row(next);

				name = (char *)zrealloc(name, namelen + 1);
				((char *)name)[namelen] = '\0';

				if (val_data) {
					val_data = (void *)zrealloc(val_data, val_len + 1);
					((uint8_t *)val_data)[val_len] = '\0';
				}

				if (val_dt == DT_QUID) {
					marshall->child[marshall->size] = _db_get(val_data, marshall);
					if (!marshall->child[marshall->size]) {
						marshall->child[marshall->size] = tree_zmalloc(sizeof(marshall_t), marshall);
						memset(marshall->child[marshall->size], 0, sizeof(marshall_t));
						marshall->child[marshall->size]->type = MTYPE_NULL;
						marshall->child[marshall->size]->size = 1;
					}
					marshall->child[marshall->size]->name = tree_zstrdup(name, marshall);
					marshall->size++;
					zfree(name);
					zfree(val_data);
					continue;
				}

				marshall->child[marshall->size] = tree_zmalloc(sizeof(marshall_t), marshall);
				memset(marshall->child[marshall->size], 0, sizeof(marshall_t));
				marshall->child[marshall->size]->name = tree_zstrdup(name, marshall);
				zfree(name);

				if (val_dt == DT_NULL)
					marshall->child[marshall->size]->type = MTYPE_NULL;
				else if (val_dt == DT_BOOL_T)
					marshall->child[marshall->size]->type = MTYPE_TRUE;
				else if (val_dt == DT_BOOL_F)
					marshall->child[marshall->size]->type = MTYPE_FALSE;
				else if (val_dt == DT_INT)
					marshall->child[marshall->size]->type = MTYPE_INT;
				else if (val_dt == DT_FLOAT)
					marshall->child[marshall->size]->type = MTYPE_FLOAT;
				else if (val_dt == DT_TEXT) {
					marshall->child[marshall->size]->type = MTYPE_STRING;
					char *_tmp = stresc(val_data);
					zfree(val_data);
					val_data = _tmp;
				}

				marshall->child[marshall->size]->data = tree_zstrndup(val_data, val_len, marshall);
				marshall->size++;
				if (val_data)
					zfree(val_data);
			}
			break;
		}
		case SCHEMA_TABLE: {
			marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
			memset(marshall, 0, sizeof(marshall_t));
			marshall->child = (marshall_t **)tree_zmalloc(elements * sizeof(marshall_t *), marshall);
			memset(marshall->child, 0, elements * sizeof(marshall_t *));
			marshall->type = MTYPE_ARRAY;

			for (unsigned int i = 0; i < elements; ++i) {
				void *val_data = slay_unwrap(next, &name, &namelen, &val_len, &val_dt);
				next = next_row(next);

				marshall->child[marshall->size] = tree_zmalloc(sizeof(marshall_t), marshall);
				memset(marshall->child[marshall->size], 0, sizeof(marshall_t));

				val_data = (void *)zrealloc(val_data, val_len + 1);
				((uint8_t *)val_data)[val_len] = '\0';

				marshall->child[marshall->size] = _db_get(val_data, marshall);
				marshall->size++;
				zfree(val_data);
			}
			break;			
		}
		case SCHEMA_SET: {
			marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
			memset(marshall, 0, sizeof(marshall_t));
			marshall->child = (marshall_t **)tree_zmalloc(elements * sizeof(marshall_t *), marshall);
			memset(marshall->child, 0, elements * sizeof(marshall_t *));
			marshall->type = MTYPE_OBJECT;

			for (unsigned int i = 0; i < elements; ++i) {
				void *val_data = slay_unwrap(next, &name, &namelen, &val_len, &val_dt);
				next = next_row(next);

				marshall->child[marshall->size] = tree_zmalloc(sizeof(marshall_t), marshall);
				memset(marshall->child[marshall->size], 0, sizeof(marshall_t));

				val_data = (void *)zrealloc(val_data, val_len + 1);
				((uint8_t *)val_data)[val_len] = '\0';

				marshall->child[marshall->size] = _db_get(val_data, marshall);
				marshall->child[marshall->size]->name = tree_zstrndup(name, namelen, marshall);
				marshall->size++;
				zfree(name);
				zfree(val_data);
			}
			break;
		}
		default:
			break;
	}

	return marshall;
}

void *slay_get_data(void *data, dstype_t *dt) {
	uint64_t elements;
	schema_t schema;
	void *slay = get_row(data, &schema, &elements);
	void *next = (void *)(((uint8_t *)slay) + sizeof(struct row_slay));

	char *buf = NULL;
	switch (schema) {
		case SCHEMA_FIELD: {
			size_t val_len;
			size_t namelen;
			dstype_t val_dt;

			void *val_data = slay_unwrap(next, NULL, &namelen, &val_len, &val_dt);
			next = next_row(next);
			switch (val_dt) {
				case DT_NULL:
					buf = zstrdup(str_null());
					break;
				case DT_BOOL_T:
					buf = zstrdup(str_bool(TRUE));
					break;
				case DT_BOOL_F:
					buf = zstrdup(str_bool(FALSE));
					break;
				case DT_INT:
				case DT_FLOAT:
					val_data = (void *)zrealloc(val_data, val_len + 1);
					((uint8_t *)val_data)[val_len] = '\0';
					buf = zstrdup(val_data);
					break;
				case DT_CHAR:
				case DT_TEXT: {
					val_data = (char *)zrealloc(val_data, val_len + 1);
					((uint8_t *)val_data)[val_len] = '\0';
					char *escdata = stresc(val_data);
					buf = zmalloc(strlen(escdata) + 3);
					snprintf(buf, strlen(escdata) + 3, "\"%s\"", escdata);
					zfree(escdata);
					break;
				}
				case DT_JSON:
					val_data = (void *)zrealloc(val_data, val_len + 1);
					((uint8_t *)val_data)[val_len] = '\0';
					buf = zstrdup(val_data);
					break;
				case DT_QUID: {
					//dstype_t _dt;
					//val_data = (char *)zrealloc(val_data, val_len + 1);
					//((char *)val_data)[val_len] = '\0';
					//buf = _db_get(val_data, &_dt);
					buf = NULL;//_db_get(val_data, &_dt);
				}
				default:
					buf = zstrdup(str_null());
					break;
			}

			*dt = val_dt;
			zfree(val_data);
			break;
		}
		case SCHEMA_ARRAY: {
			size_t val_len;
			dstype_t val_dt;
			unsigned int i;
			size_t total_len = 0;

			vector_t *arr = alloc_vector(VECTOR_SIZE);
			for (i = 0; i < elements; ++i) {
				size_t namelen;
				void *val_data = slay_unwrap(next, NULL, &namelen, &val_len, &val_dt);
				next = next_row(next);
				total_len += (val_len + 8);

				switch (val_dt) {
					case DT_NULL: {
						dict_t *element = dict_element_cnew(arr, FALSE, NULL, "null");
						vector_append(arr, (void *)element);
						break;
					}
					case DT_BOOL_T: {
						dict_t *element = dict_element_cnew(arr, FALSE, NULL, "true");
						vector_append(arr, (void *)element);
						break;
					}
					case DT_BOOL_F: {
						dict_t *element = dict_element_cnew(arr, FALSE, NULL, "false");
						vector_append(arr, (void *)element);
						break;
					}
					case DT_CHAR:
					case DT_TEXT: {
						val_data = (char *)zrealloc(val_data, val_len + 1);
						((char *)val_data)[val_len] = '\0';
						dict_t *element = dict_element_new(arr, TRUE, NULL, val_data);
						vector_append(arr, (void *)element);
						break;
					}
					case DT_INT:
					case DT_FLOAT:
					case DT_JSON: {
						val_data = (char *)zrealloc(val_data, val_len + 1);
						((char *)val_data)[val_len] = '\0';
						dict_t *element = dict_element_new(arr, FALSE, NULL, val_data);
						vector_append(arr, (void *)element);
						break;
					}
					case DT_QUID: {
						/*dstype_t _dt;
						dict_t *element = NULL;
						val_data = (char *)zrealloc(val_data, val_len + 1);
						((char *)val_data)[val_len] = '\0';
						//void *qbuf = _db_get(val_data, &_dt);
						void *qbuf = NULL;//_db_get(val_data, &_dt);
						if (!qbuf)
							element = dict_element_cnew(arr, FALSE, NULL, "null");
						else {
							size_t buflen = strlen(qbuf);
							element = resolv_quid(arr, qbuf, buflen, NULL, _dt);
						}
						vector_append(arr, (void *)element);*/
						break;
					}
					default: {
						dict_t *element = dict_element_cnew(arr, FALSE, NULL, "null");
						vector_append(arr, (void *)element);
						break;
					}
				}
				zfree(val_data);
			}

			*dt = DT_JSON;
			buf = zmalloc(total_len + 1);
			memset(buf, 0, total_len + 1);
			buf = dict_array(arr, buf);
			vector_free(arr);

			break;
		}
		case SCHEMA_OBJECT: {
			size_t val_len;
			dstype_t val_dt;
			unsigned int i;
			size_t total_len = 0;

			vector_t *obj = alloc_vector(VECTOR_SIZE);
			for (i = 0; i < elements; ++i) {
				void *name = NULL;
				size_t namelen;
				void *val_data = slay_unwrap(next, &name, &namelen, &val_len, &val_dt);
				next = next_row(next);
				name = (char *)zrealloc(name, namelen + 1);
				((char *)name)[namelen] = '\0';
				total_len += (val_len + namelen + 8);

				switch (val_dt) {
					case DT_NULL: {
						dict_t *element = dict_element_cnew(obj, FALSE, name, "null");
						vector_append(obj, (void *)element);

						break;
					}
					case DT_BOOL_T: {
						dict_t *element = dict_element_cnew(obj, FALSE, name, "true");
						vector_append(obj, (void *)element);
						break;
					}
					case DT_BOOL_F: {
						dict_t *element = dict_element_cnew(obj, FALSE, name, "false");
						vector_append(obj, (void *)element);
						break;
					}
					case DT_CHAR:
					case DT_TEXT: {
						val_data = (char *)zrealloc(val_data, val_len + 1);
						((char *)val_data)[val_len] = '\0';
						dict_t *element = dict_element_new(obj, TRUE, name, val_data);
						vector_append(obj, (void *)element);
						break;
					}
					case DT_FLOAT:
					case DT_INT:
					case DT_JSON: {
						val_data = (char *)zrealloc(val_data, val_len + 1);
						((char *)val_data)[val_len] = '\0';
						dict_t *element = dict_element_new(obj, FALSE, name, val_data);
						vector_append(obj, (void *)element);
						break;
					}
					case DT_QUID: {
						/*dstype_t _dt;
						dict_t *element = NULL;
						val_data = (char *)zrealloc(val_data, val_len + 1);
						((char *)val_data)[val_len] = '\0';
						//void *qbuf = _db_get(val_data, &_dt);
						void *qbuf = NULL;//_db_get(val_data, &_dt);
						if (!qbuf)
							element = dict_element_cnew(obj, FALSE, name, "null");
						else {
							size_t buflen = strlen(qbuf);
							element = resolv_quid(obj, qbuf, buflen, name, _dt);
						}
						vector_append(obj, (void *)element);*/
						break;
					}
					default: {
						dict_t *element = dict_element_cnew(obj, FALSE, name, "null");
						vector_append(obj, (void *)element);
						break;
					}
				}
				zfree(name);
				zfree(val_data);
			}

			*dt = DT_JSON;
			buf = zmalloc(total_len + 1);
			memset(buf, 0, total_len + 1);
			buf = dict_object(obj, buf);
			vector_free(obj);
			break;
		}
		case SCHEMA_TABLE: {
			/*size_t val_len;
			dstype_t val_dt;
			unsigned int i;
			size_t total_len = 0;

			vector_t *arr = alloc_vector(VECTOR_SIZE);
			for (i = 0; i < elements; ++i) {
				size_t namelen;
				dstype_t _dt;
				dict_t *element = NULL;
				void *val_data = slay_unwrap(next, NULL, &namelen, &val_len, &val_dt);
				next = next_row(next);
				val_data = (char *)zrealloc(val_data, val_len + 1);
				((char *)val_data)[val_len] = '\0';
				//void *qbuf = _db_get(val_data, &_dt);
				void *qbuf = NULL;//_db_get(val_data, &_dt);
				if (!qbuf) {
					element = dict_element_cnew(arr, FALSE, NULL, "null");
					total_len += 8;
				} else {
					size_t buflen = strlen(qbuf);
					element = resolv_quid(arr, qbuf, buflen, NULL, _dt);
					total_len += (buflen + 4);
				}
				vector_append(arr, (void *)element);
				zfree(val_data);
			}

			*dt = DT_JSON;
			buf = zmalloc(total_len + 1);
			memset(buf, 0, total_len + 1);
			buf = dict_array(arr, buf);
			vector_free(arr);*/
			break;
		}
		default:
			buf = zstrdup(str_null());
			break;
	}

	return (void *)buf;
}

void *create_row(schema_t schema, uint64_t el, size_t data_len, size_t *len) {
	zassert(el >= 1);
	struct row_slay *row = zcalloc(1, sizeof(struct row_slay) + (el * sizeof(struct value_slay)) + data_len);
	row->elements = el;
	row->schema = schema;
	*len = sizeof(struct row_slay) + (el * sizeof(struct value_slay)) + data_len;
	return (void *)row;
}

void *get_row(void *val, schema_t *schema, uint64_t *el) {
	struct row_slay *row = (struct row_slay *)val;
	*el = row->elements;
	*schema = row->schema;
	return (void *)row;
}

uint8_t *slay_wrap(void *arrp, void *name, size_t namelen, void *data, size_t len, dstype_t dt) {
	size_t data_size = len;
	if (isdata(dt))
		data_size = 0;

	struct value_slay *slay = (struct value_slay *)arrp;
	slay->val_type = dt;
	slay->size = data_size;
	slay->namesize = namelen;

	uint8_t *dest = ((uint8_t *)slay) + sizeof(struct value_slay);
	if (slay->size) {
		memcpy(dest, data, data_size);
	}

	if (slay->namesize) {
		uint8_t *namedest = dest + slay->size;
		memcpy(namedest, name, namelen);
	}

	return (uint8_t *)dest + slay->size + namelen;
}

void *slay_unwrap(void *arrp, void **name, size_t *namelen, size_t *len, dstype_t *dt) {
	struct value_slay *slay = (struct value_slay *)arrp;
	void *data = NULL;

	if (slay->size) {
		void *src = ((uint8_t *)arrp) + sizeof(struct value_slay);
		data = zmalloc(slay->size);
		memcpy(data, src, slay->size);
	}

	if (slay->namesize) {
		void *src = ((uint8_t *)arrp) + sizeof(struct value_slay) + slay->size;
		*name = zmalloc(slay->namesize);
		memcpy(*name, src, slay->namesize);
	}

	*dt = slay->val_type;
	*len = slay->size;
	*namelen = slay->namesize;

	return data;
}

char *str_schema(schema_t schema) {
	switch (schema) {
		case SCHEMA_FIELD:
			return "FIELD";
		case SCHEMA_ARRAY:
			return "ARRAY";
		case SCHEMA_OBJECT:
			return "OBJECT";
		case SCHEMA_TABLE:
			return "TABLE";
		default:
			return "NULL";
	}
}
