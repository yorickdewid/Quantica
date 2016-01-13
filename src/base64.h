#ifndef BASE64_H_INCLUDED
#define BASE64_H_INCLUDED

int base64_decode_len(const char *bufcoded);
int base64_decode(char *bufplain, const char *bufcoded);
int base64_encode_len(int len);
int base64_encode(char *encoded, const char *string, int len);
void base64url_encode(char *encoded, int len);
void base64url_decode(char *decoded, int len);

#endif // BASE64_H_INCLUDED
