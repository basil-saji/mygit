#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <sys/stat.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

/* Load a working-tree file or mygit metadata file into memory. */
int read_file(const char *path, unsigned char **data, size_t *size)
{
    FILE *file;
    long file_size;
    unsigned char *buffer;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "error: cannot open '%s'\n", path);
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "error: cannot seek '%s'\n", path);
        fclose(file);
        return 0;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fprintf(stderr, "error: cannot measure '%s'\n", path);
        fclose(file);
        return 0;
    }

    rewind(file);

    buffer = malloc((size_t)file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "error: out of memory\n");
        fclose(file);
        return 0;
    }

    if (file_size > 0 &&
        fread(buffer, 1, (size_t)file_size, file) != (size_t)file_size) {
        fprintf(stderr, "error: cannot read '%s'\n", path);
        free(buffer);
        fclose(file);
        return 0;
    }

    buffer[file_size] = '\0';
    fclose(file);

    *data = buffer;
    *size = (size_t)file_size;
    return 1;
}

/* Store bytes exactly as given; object files use this to preserve binary data. */
int write_file(const char *path, const unsigned char *data, size_t size)
{
    FILE *file;

    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "error: cannot write '%s'\n", path);
        return 0;
    }

    if (size > 0 && fwrite(data, 1, size, file) != size) {
        fprintf(stderr, "error: failed writing '%s'\n", path);
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
}

/* Add one staged index entry without parsing the old index. */
int append_line(const char *path, const char *line)
{
    FILE *file;

    file = fopen(path, "ab");
    if (file == NULL) {
        fprintf(stderr, "error: cannot append to '%s'\n", path);
        return 0;
    }

    if (fprintf(file, "%s\n", line) < 0) {
        fprintf(stderr, "error: failed writing '%s'\n", path);
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
}

/* Create repository directories, matching Git's hidden metadata layout. */
int ensure_dir(const char *path)
{
    if (path_exists(path)) {
        return 1;
    }

    if (MKDIR(path) != 0) {
        fprintf(stderr, "error: cannot create directory '%s': %s\n",
                path, strerror(errno));
        return 0;
    }

    return 1;
}

/* Check for files or directories before creating or reading repository data. */
int path_exists(const char *path)
{
    FILE *file;

    file = fopen(path, "rb");
    if (file != NULL) {
        fclose(file);
        return 1;
    }

#ifdef _WIN32
    {
        struct _stat info;
        return _stat(path, &info) == 0;
    }
#else
    {
        struct stat info;
        return stat(path, &info) == 0;
    }
#endif
}

/* Refuse commands that need .mygit metadata when init has not been run. */
int require_repository(void)
{
    if (!path_exists(".mygit")) {
        fprintf(stderr, "error: not a mygit repository; run 'mygit init' first\n");
        return 0;
    }

    return 1;
}
