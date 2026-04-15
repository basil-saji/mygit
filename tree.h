#ifndef TREE_H
#define TREE_H

#include "hash.h"

/* Build a flat tree object from the current index and return its hash. */
int build_tree_from_index(char tree_hash[SHA1_HEX_LENGTH + 1]);

#endif
