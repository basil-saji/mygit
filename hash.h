#ifndef HASH_H
#define HASH_H

#include <stddef.h>

/* SHA-1 hashes are 20 bytes, shown as 40 hexadecimal characters. */
#define SHA1_HEX_LENGTH 40

/* Compute the SHA-1 hash of bytes and write a null-terminated hex string. */
void sha1_hex(const unsigned char *data, size_t size,
              char out_hash[SHA1_HEX_LENGTH + 1]);

#endif
