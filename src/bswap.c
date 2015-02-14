#include "bswap.h"

inline uint16_t ntohs(uint16_t x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	unsigned char *s = (unsigned char *)&x;
	return (uint16_t)(s[0] << 8 | s[1]);
#else
	return x;
#endif
}

inline uint32_t htonl(x) uint32_t x;
{
#if BYTE_ORDER == LITTLE_ENDIAN
	unsigned char *s = (unsigned char  *)&x;
	return (uint32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
#else
	return x;
#endif
}

inline uint32_t ntohl(x) uint32_t x;
{
#if BYTE_ORDER == LITTLE_ENDIAN
	unsigned char *s = (unsigned char *)&x;
	return (uint32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
#else
	return x;
#endif
}

inline uint16_t htons(uint16_t x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	unsigned char *s = (unsigned char *)&x;
	return (uint16_t)(s[0] << 8 | s[1]);
#else
	return x;
#endif
}

__be32 to_be32(uint32_t x)
{
	return (FORCE __be32) htonl(x);
}

__be16 to_be16(uint16_t x)
{
	return (FORCE __be16) htons(x);
}

__be64 to_be64(uint64_t x)
{
#if (BYTE_ORDER == LITTLE_ENDIAN)
	return (FORCE __be64) (((uint64_t) htonl((uint32_t) x) << 32) |
	                       htonl((uint32_t) (x >> 32)));
#else
	return (FORCE __be64) x;
#endif
}

uint32_t from_be32(__be32 x)
{
	return ntohl((FORCE uint32_t) x);
}

uint16_t from_be16(__be16 x)
{
	return ntohs((FORCE uint16_t) x);
}

uint64_t from_be64(__be64 x)
{
#if (BYTE_ORDER == LITTLE_ENDIAN)
	return ((uint64_t) ntohl((uint32_t) (FORCE uint64_t) x) << 32) |
	       ntohl((uint32_t) ((FORCE uint64_t) x >> 32));
#else
	return (FORCE uint64_t) x;
#endif
}
