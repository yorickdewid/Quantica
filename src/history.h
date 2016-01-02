#ifndef HISTORY_H_INCLUDED
#define HISTORY_H_INCLUDED

unsigned short history_get_last_version(base_t *base, const quid_t *c_quid);
int history_add(base_t *base, const quid_t *c_quid, unsigned long long offset);

#endif // HISTORY_H_INCLUDED
