#include "add.h"
#include "hash.h"
#include "object.h"
#include "utils.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_directory(const char *path)
{
    DIR *dir;

    dir = opendir(path);
    if (dir == NULL) {
        return 0;
    }

    closedir(dir);
    return 1;
}

/* Check whether an index line already refers to the same working-tree path. */
static int index_line_matches_file(const char *line_start,
                                   size_t line_length,
                                   const char *file_path)
{
    const char *space;
    const char *name_start;
    size_t name_length;

    space = memchr(line_start, ' ', line_length);
    if (space == NULL) {
        return 0;
    }

    name_start = space + 1;
    name_length = line_length - (size_t)(name_start - line_start);

    while (name_length > 0 &&
           (name_start[name_length - 1] == '\n' ||
            name_start[name_length - 1] == '\r')) {
        name_length--;
    }

    return strlen(file_path) == name_length &&
           strncmp(name_start, file_path, name_length) == 0;
}

/* Rewrite the index without stale entries for this file, then add the new one. */
static int write_updated_index(const char *file_path, const char *index_line)
{
    unsigned char *old_index;
    char *new_index;
    size_t old_size;
    size_t new_size;
    size_t line_size;
    size_t offset;
    char *line_start;
    char *line_end;

    if (!read_file(".mygit/index", &old_index, &old_size)) {
        return 0;
    }

    new_index = malloc(old_size + strlen(index_line) + 2);
    if (new_index == NULL) {
        fprintf(stderr, "error: out of memory\n");
        free(old_index);
        return 0;
    }

    new_size = 0;
    offset = 0;

    while (offset < old_size) {
        line_start = (char *)old_index + offset;
        line_end = memchr(line_start, '\n', old_size - offset);

        if (line_end == NULL) {
            line_size = old_size - offset;
        } else {
            line_size = (size_t)(line_end - line_start) + 1;
        }

        if (!index_line_matches_file(line_start, line_size, file_path)) {
            memcpy(new_index + new_size, line_start, line_size);
            new_size += line_size;
        }

        offset += line_size;
    }

    new_size += (size_t)sprintf(new_index + new_size, "%s\n", index_line);

    if (!write_file(".mygit/index", (const unsigned char *)new_index, new_size)) {
        free(new_index);
        free(old_index);
        return 0;
    }

    free(new_index);
    free(old_index);
    return 1;
}

int add_all_files(void)
{
    DIR *dir;
    struct dirent *entry;
    int status;

    dir = opendir(".");
    if (dir == NULL) {
        fprintf(stderr, "error: cannot open current directory\n");
        return 1;
    }

    status = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, ".mygit") == 0) {
            continue;
        }

        if (is_directory(entry->d_name)) {
            continue;
        }

        if (mygit_add(entry->d_name) != 0) {
            status = 1;
        }
    }

    closedir(dir);
    return status;
}

/* Store a file as a blob, then stage its hash and path in the index. */
int mygit_add(const char *file_path)
{
    unsigned char *file_data;
    size_t file_size;
    char hash[SHA1_HEX_LENGTH + 1];
    char index_line[1024];

    if (!require_repository()) {
        return 1;
    }

    if (strcmp(file_path, ".") == 0) {
        return add_all_files();
    }

    if (!read_file(file_path, &file_data, &file_size)) {
        return 1;
    }

    if (!write_object("blob", file_data, file_size, hash)) {
        free(file_data);
        return 1;
    }

    free(file_data);

    if (snprintf(index_line, sizeof(index_line), "%s %s", hash, file_path) >=
        (int)sizeof(index_line)) {
        fprintf(stderr, "error: index entry is too long\n");
        return 1;
    }

    if (!write_updated_index(file_path, index_line)) {
        return 1;
    }

    printf("added %s %s\n", hash, file_path);
    return 0;
}
