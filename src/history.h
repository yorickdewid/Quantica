#ifndef HISTORY_H_INCLUDED
#define HISTORY_H_INCLUDED

unsigned long long history_get_version_offset(base_t *base, const quid_t *c_quid, unsigned short version);
int history_count(base_t *base, const quid_t *c_quid);
int history_add(base_t *base, const quid_t *c_quid, unsigned long long offset);
marshall_t *history_all(base_t *base, const quid_t *c_quid);

#endif // HISTORY_H_INCLUDED
