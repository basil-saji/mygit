#include "utils.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_IGNORED_FILES 128
#define MAX_LINE_LENGTH 4096

static char *ignored_files[MAX_IGNORED_FILES];
static int ignored_count = 0;

static void trim_line(char *line)
{
    size_t length;

    length = strlen(line);
    while (length > 0 && (line[length - 1] == '\n' ||
                          line[length - 1] == '\r')) {
        line[length - 1] = '\0';
        length--;
    }
}

static char *copy_string(const char *text)
{
    char *copy;
    size_t length;

    length = strlen(text);
    copy = malloc(length + 1);
    if (copy == NULL) {
        fprintf(stderr, "Error: out of memory\n");
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static void free_ignored_files(void)
{
    int i;

    for (i = 0; i < ignored_count; i++) {
        free(ignored_files[i]);
        ignored_files[i] = NULL;
    }

    ignored_count = 0;
}

static int load_mygitignore(void)
{
    FILE *file;
    char line[MAX_LINE_LENGTH];

    free_ignored_files();

    file = fopen(".mygitignore", "r");
    if (file == NULL) {
        return 1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        trim_line(line);
        if (line[0] == '\0') {
            continue;
        }

        if (ignored_count >= MAX_IGNORED_FILES) {
            fprintf(stderr, "Error: too many .mygitignore entries\n");
            fclose(file);
            free_ignored_files();
            return 0;
        }

        ignored_files[ignored_count] = copy_string(line);
        if (ignored_files[ignored_count] == NULL) {
            fclose(file);
            free_ignored_files();
            return 0;
        }

        ignored_count++;
    }

    fclose(file);
    return 1;
}

int is_ignored(const char *filename)
{
    int i;

    for (i = 0; i < ignored_count; i++) {
        if (strcmp(filename, ignored_files[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

static int contains_any_pattern(const char *line,
                                const char **patterns,
                                int pattern_count)
{
    int i;

    for (i = 0; i < pattern_count; i++) {
        if (strstr(line, patterns[i]) != NULL) {
            return 1;
        }
    }

    return 0;
}

static int contains_suspicious_assignment(const char *line)
{
    return strchr(line, '=') != NULL &&
           (strchr(line, '"') != NULL || strchr(line, '\'') != NULL);
}

int contains_secret_pattern(const char *line)
{
    const char *keywords[] = {
        "API_KEY",
        "SECRET",
        "TOKEN",
        "PASSWORD",
        "PASS",
        "AUTH",
        "ACCESS_KEY",
        "PRIVATE_KEY",
        "CLIENT_SECRET"
    };
    const char *prefixes[] = {
        "AKIA",
        "AIza",
        "ghp_",
        "sk-"
    };

    return contains_any_pattern(line, keywords, 9) ||
           contains_any_pattern(line, prefixes, 4) ||
           contains_suspicious_assignment(line);
}

static int line_looks_binary(const char *line)
{
    const unsigned char *cursor;

    cursor = (const unsigned char *)line;
    while (*cursor != '\0') {
        if (*cursor < 32 && *cursor != '\n' &&
            *cursor != '\r' && *cursor != '\t') {
            return 1;
        }
        cursor++;
    }

    return 0;
}

int audit_file(const char *filename)
{
    FILE *file;
    char line[MAX_LINE_LENGTH];
    int line_number;

    if (is_ignored(filename)) {
        return 0;
    }

    file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: cannot open '%s'\n", filename);
        return 1;
    }

    line_number = 1;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (line_looks_binary(line)) {
            fclose(file);
            return 0;
        }

        trim_line(line);
        if (contains_secret_pattern(line)) {
            printf("[WARNING] %s:%d %s\n", filename, line_number, line);
        }

        line_number++;
    }

    fclose(file);
    return 0;
}

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

int audit_directory(void)
{
    DIR *dir;
    struct dirent *entry;
    int status;

    dir = opendir(".");
    if (dir == NULL) {
        fprintf(stderr, "Error: cannot open current directory\n");
        return 1;
    }

    status = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, ".mygit") == 0) {
            continue;
        }

        if (is_directory(entry->d_name) || is_ignored(entry->d_name)) {
            continue;
        }

        if (audit_file(entry->d_name) != 0) {
            status = 1;
        }
    }

    closedir(dir);
    return status;
}

int mygit_audit(const char *path)
{
    int status;

    if (!load_mygitignore()) {
        return 1;
    }

    if (strcmp(path, ".") == 0) {
        status = audit_directory();
    } else {
        status = audit_file(path);
    }

    free_ignored_files();
    return status;
}
