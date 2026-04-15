#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

/* Read a whole file into memory. The caller owns *data and must free it. */
int read_file(const char *path, unsigned char **data, size_t *size);

/* Write exactly size bytes to a file, replacing any existing contents. */
int write_file(const char *path, const unsigned char *data, size_t size);

/* Append a text line to a file, creating it if needed. */
int append_line(const char *path, const char *line);

/* Create a directory if it does not already exist. */
int ensure_dir(const char *path);

/* Return 1 if a path exists, 0 otherwise. */
int path_exists(const char *path);

/* Return 1 if .mygit exists in the current directory. */
int require_repository(void);

#endif
