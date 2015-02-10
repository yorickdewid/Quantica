#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED

struct stats {
	unsigned int commit;
};

void start_core();
int store(char *quid, const void *data, size_t len);
void *request(char *quid, size_t *len);
void detach_core();

#endif // CORE_H_INCLUDED
