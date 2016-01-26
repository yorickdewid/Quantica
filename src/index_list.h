#ifndef INDEX_LIST_H_INCLUDED
#define INDEX_LIST_H_INCLUDED

#include "marshall.h"

typedef enum {
	INDEX_BTREE,
	INDEX_HASH,
} index_type_t;

int index_list_add(base_t *base, const quid_t *index, const quid_t *group, char *element, index_type_t type, uint64_t offset);
quid_t *index_list_get_index(base_t *base, const quid_t *c_quid);
marshall_t *index_list_get_element(base_t *base, const quid_t *c_quid);
marshall_t *index_list_on_group(base_t *base, const quid_t *c_quid);
uint64_t index_list_get_index_offset(base_t *base, const quid_t *c_quid);
char *index_list_get_index_element(base_t *base, const quid_t *c_quid);
quid_t *index_list_get_index_group(base_t *base, const quid_t *c_quid);
int index_list_update_offset(base_t *base, const quid_t *index, uint64_t index_offset);
int index_list_delete(base_t *base, const quid_t *index);
marshall_t *index_list_all(base_t *base);
void index_list_rebuild(base_t *base, base_t *new_base);
char *index_type(index_type_t type);

#endif // INDEX_LIST_H_INCLUDED
