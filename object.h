#ifndef OBJECT_H
#define OBJECT_H

#include "hash.h"

#include <stddef.h>

/*
 * Build a Git-style object:
 *     "<type> <size>\0<payload>"
 * Then hash and store it under .mygit/objects.
 */
int write_object(const char *type, const unsigned char *payload,
                 size_t payload_size,
                 char out_hash[SHA1_HEX_LENGTH + 1]);

#endif
