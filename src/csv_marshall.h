#ifndef CSV_MARSHALL_H_INCLUDED
#define CSV_MARSHALL_H_INCLUDED

#include "marshall.h"

void marshall_csv_parse_options(csv_t *csvopt, marshall_t *options);
marshall_t *marshall_csv_decode(csv_t *csvopt, char *data);

#endif // CSV_MARSHALL_H_INCLUDED
