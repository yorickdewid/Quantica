#ifndef JWT_H_INCLUDED
#define JWT_H_INCLUDED

#define JWT_TOKEN_VALID		3600

char *jwt_encode(marshall_t *data, const unsigned char *key);
marshall_t *jwt_decode(char *token, const unsigned char *key);

#endif // JWT_H_INCLUDED
