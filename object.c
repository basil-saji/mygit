#include "object.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Build the exact bytes Git hashes: a header, a NUL byte, then payload. */
static unsigned char *build_object_buffer(const char *type,
                                          const unsigned char *payload,
                                          size_t payload_size,
                                          size_t *object_size)
{
    char header[64];
    int header_size;
    unsigned char *object_data;

    header_size = snprintf(header, sizeof(header), "%s %zu", type, payload_size);
    if (header_size < 0 || (size_t)header_size >= sizeof(header)) {
        fprintf(stderr, "error: object header is too large\n");
        return NULL;
    }

    *object_size = (size_t)header_size + 1 + payload_size;
    object_data = malloc(*object_size);
    if (object_data == NULL) {
        fprintf(stderr, "error: out of memory\n");
        return NULL;
    }

    memcpy(object_data, header, (size_t)header_size);
    object_data[header_size] = '\0';
    memcpy(object_data + header_size + 1, payload, payload_size);

    return object_data;
}

/* Split a hash into .mygit/objects/XX/YYYY..., like Git's object store. */
static void object_path_from_hash(const char *hash,
                                  char *dir_path,
                                  size_t dir_path_size,
                                  char *file_path,
                                  size_t file_path_size)
{
    snprintf(dir_path, dir_path_size, ".mygit/objects/%c%c", hash[0], hash[1]);
    snprintf(file_path, file_path_size, "%s/%s", dir_path, hash + 2);
}

/* Hash an object and write it to the content-addressed object database. */
int write_object(const char *type, const unsigned char *payload,
                 size_t payload_size,
                 char out_hash[SHA1_HEX_LENGTH + 1])
{
    unsigned char *object_data;
    size_t object_size;
    char dir_path[256];
    char file_path[512];
    int ok;

    object_data = build_object_buffer(type, payload, payload_size, &object_size);
    if (object_data == NULL) {
        return 0;
    }

    sha1_hex(object_data, object_size, out_hash);
    object_path_from_hash(out_hash, dir_path, sizeof(dir_path),
                          file_path, sizeof(file_path));

    if (!ensure_dir(dir_path)) {
        free(object_data);
        return 0;
    }

    /*
     * Real Git compresses objects. This learning version stores the exact
     * object bytes so the content-addressed model is visible on disk.
     */
    ok = write_file(file_path, object_data, object_size);
    free(object_data);
    return ok;
}
