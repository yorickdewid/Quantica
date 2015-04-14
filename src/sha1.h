#ifndef SHA1_H_INCLUDED
#define SHA1_H_INCLUDED

#define SHA1_LENGTH		40

struct sha {
	unsigned int digest[5]; /* Message Digest (output)          */
	unsigned int szlow;        /* Message length in bits           */
	unsigned int szhigh;       /* Message length in bits           */
	unsigned char message_block[64]; /* 512-bit message blocks      */
	int message_block_idx;    /* Index into message block array   */
	int computed;               /* Is the digest computed?          */
	int corrupted;              /* Is the message digest corruped?  */
};

void sha1_reset(struct sha *ctx);
int sha1_result(struct sha *ctx);
void sha1_input(struct sha *ctx, const unsigned char *message_array, unsigned int length);
void sha1_strsum(char *s, struct sha *ctx);

#endif
