#ifndef INDEX_LIST_H_INCLUDED
#define INDEX_LIST_H_INCLUDED

#include "marshall.h"

int index_list_add(base_t *base, const quid_t *index, const quid_t *group, char *element, unsigned long long offset);
quid_t *index_list_get_index(base_t *base, const quid_t *c_quid);
marshall_t *index_list_get_element(base_t *base, const quid_t *c_quid);
unsigned long long index_list_get_index_offset(base_t *base, const quid_t *c_quid);
int index_list_delete(base_t *base, const quid_t *index);
marshall_t *index_list_all(base_t *base);
void index_list_rebuild(base_t *base, base_t *new_base);

#endif // INDEX_LIST_H_INCLUDED
