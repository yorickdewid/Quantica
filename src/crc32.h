#ifndef CRC32_H_INCLUDED
#define CRC32_H_INCLUDED

unsigned long crc32_calculate(unsigned long in_crc32, const void *buf, size_t len);

#endif // CRC32_H_INCLUDED
