#ifndef JWT_H_INCLUDED
#define JWT_H_INCLUDED

#define JWT_TOKEN_VALID		3600

char *jwt_encode(marshall_t *data, const unsigned char *key);

#endif // JWT_H_INCLUDED
