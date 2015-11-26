#ifndef DICT_MARSHALL_H_INCLUDED
#define DICT_MARSHALL_H_INCLUDED

#include <config.h>
#include <common.h>

#include "marshall.h"

#define marshall_free(v) tree_zfree(v);

marshall_t *marshall_convert(char *data, size_t data_len);
char *marshall_serialize(marshall_t *obj);

#endif // DICT_MARSHALL_H_INCLUDED
