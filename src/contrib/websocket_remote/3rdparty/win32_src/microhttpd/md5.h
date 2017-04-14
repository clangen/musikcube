/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.	This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

#ifndef MHD_MD5_H
#define MHD_MD5_H

#include "platform.h"

#define	MD5_BLOCK_SIZE              64
#define	MD5_DIGEST_SIZE             16
#define	MD5_DIGEST_STRING_LENGTH    (MD5_DIGEST_SIZE * 2 + 1)

struct MD5Context
{
  uint32_t state[4];			/* state */
  uint64_t count;			/* number of bits, mod 2^64 */
  uint8_t buffer[MD5_BLOCK_SIZE];	/* input buffer */
};

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void MD5Init(struct MD5Context *ctx);

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
void MD5Update(struct MD5Context *ctx, const unsigned char *input, size_t len);

/*
 * Pad pad to 64-byte boundary with the bit pattern
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
void MD5Pad(struct MD5Context *ctx);

/*
 * Final wrapup--call MD5Pad, fill in digest and zero out ctx.
 */
void MD5Final(unsigned char digest[MD5_DIGEST_SIZE], struct MD5Context *ctx);

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
void MD5Transform(uint32_t state[4], const uint8_t block[MD5_BLOCK_SIZE]);

#endif /* !MHD_MD5_H */
