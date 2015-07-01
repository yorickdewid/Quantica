#ifndef CRC64_H_INCLUDED
#define CRC64_H_INCLUDED

uint64_t crc64(uint64_t crc, void *buf, size_t len);
bool crc_file(int fd, uint64_t *rscrc64);

#endif // CRC64_H_INCLUDED
