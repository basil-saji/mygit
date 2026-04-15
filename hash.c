#include "hash.h"

#include <openssl/sha.h>
#include <stdio.h>

/* Convert OpenSSL's 20-byte SHA-1 digest into Git's 40-char hex form. */
void sha1_hex(const unsigned char *data, size_t size,
              char out_hash[SHA1_HEX_LENGTH + 1])
{
    unsigned char digest[SHA_DIGEST_LENGTH];
    int i;

    SHA1(data, size, digest);

    for (i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(out_hash + (i * 2), "%02x", digest[i]);
    }

    out_hash[SHA1_HEX_LENGTH] = '\0';
}
