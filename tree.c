#include "tree.h"
#include "object.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Convert one index line from "<hash> <filename>" to "<filename> <hash>". */
static int append_tree_line(char *tree_data,
                            size_t *tree_size,
                            const char *line_start,
                            size_t line_size)
{
    const char *space;
    const char *file_name;
    size_t hash_size;
    size_t name_size;

    space = memchr(line_start, ' ', line_size);
    if (space == NULL) {
        return 1;
    }

    hash_size = (size_t)(space - line_start);
    file_name = space + 1;
    name_size = line_size - hash_size - 1;

    while (name_size > 0 &&
           (file_name[name_size - 1] == '\n' ||
            file_name[name_size - 1] == '\r')) {
        name_size--;
    }

    memcpy(tree_data + *tree_size, file_name, name_size);
    *tree_size += name_size;
    tree_data[(*tree_size)++] = ' ';
    memcpy(tree_data + *tree_size, line_start, hash_size);
    *tree_size += hash_size;
    tree_data[(*tree_size)++] = '\n';

    return 1;
}

/* Read the staging index and store a simple flat tree object. */
int build_tree_from_index(char tree_hash[SHA1_HEX_LENGTH + 1])
{
    unsigned char *index_data;
    char *tree_data;
    size_t index_size;
    size_t tree_size;
    size_t offset;
    size_t line_size;
    char *line_start;
    char *line_end;
    int ok;

    if (!read_file(".mygit/index", &index_data, &index_size)) {
        return 0;
    }

    if (index_size == 0) {
        fprintf(stderr, "error: nothing to commit; index is empty\n");
        free(index_data);
        return 0;
    }

    tree_data = malloc(index_size + 1);
    if (tree_data == NULL) {
        fprintf(stderr, "error: out of memory\n");
        free(index_data);
        return 0;
    }

    tree_size = 0;
    offset = 0;

    while (offset < index_size) {
        line_start = (char *)index_data + offset;
        line_end = memchr(line_start, '\n', index_size - offset);

        if (line_end == NULL) {
            line_size = index_size - offset;
        } else {
            line_size = (size_t)(line_end - line_start) + 1;
        }

        if (line_size > 0) {
            append_tree_line(tree_data, &tree_size, line_start, line_size);
        }

        offset += line_size;
    }

    ok = write_object("tree", (const unsigned char *)tree_data,
                      tree_size, tree_hash);

    free(tree_data);
    free(index_data);
    return ok;
}
