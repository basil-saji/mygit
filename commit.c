#include "commit.h"
#include "hash.h"
#include "object.h"
#include "tree.h"
#include "utils.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COLOR_RESET "\033[0m"
#define COLOR_BOLD "\033[1m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"

typedef struct {
    char *filename;
    char hash[SHA1_HEX_LENGTH + 1];
} TreeEntry;

/* Read HEAD so a new commit or log command can find the latest commit. */
static void read_head_hash(char head_hash[SHA1_HEX_LENGTH + 1])
{
    unsigned char *head_data;
    size_t head_size;
    size_t hash_size;

    head_hash[0] = '\0';

    if (!path_exists(".mygit/HEAD")) {
        return;
    }

    if (!read_file(".mygit/HEAD", &head_data, &head_size)) {
        return;
    }

    while (head_size > 0 &&
           (head_data[head_size - 1] == '\n' || head_data[head_size - 1] == '\r')) {
        head_size--;
    }

    if (head_size > 0) {
        hash_size = head_size;
        if (hash_size > SHA1_HEX_LENGTH) {
            hash_size = SHA1_HEX_LENGTH;
        }

        memcpy(head_hash, head_data, hash_size);
        head_hash[hash_size] = '\0';
    }

    free(head_data);
}

/* Combine message, parent, timestamp, and tree hash into commit bytes. */
static unsigned char *build_commit_payload(const char *message,
                                           const char *parent_hash,
                                           const char *tree_hash,
                                           size_t *payload_size)
{
    time_t now;
    int header_size;
    char header[1024];
    unsigned char *payload;

    now = time(NULL);
    if (parent_hash[0] != '\0') {
        header_size = snprintf(header, sizeof(header),
                               "message %s\n"
                               "timestamp %ld\n"
                               "parent %s\n"
                               "tree %s\n",
                               message, (long)now, parent_hash, tree_hash);
    } else {
        header_size = snprintf(header, sizeof(header),
                               "message %s\n"
                               "timestamp %ld\n"
                               "tree %s\n",
                               message, (long)now, tree_hash);
    }

    if (header_size < 0 || (size_t)header_size >= sizeof(header)) {
        fprintf(stderr, "Error: commit message is too long\n");
        return NULL;
    }

    *payload_size = (size_t)header_size;
    payload = malloc(*payload_size);
    if (payload == NULL) {
        fprintf(stderr, "Error: out of memory\n");
        return NULL;
    }

    memcpy(payload, header, (size_t)header_size);
    return payload;
}

/* Convert an object hash to its path in .mygit/objects. */
static void object_path_from_hash(const char *hash,
                                  char *file_path,
                                  size_t file_path_size)
{
    snprintf(file_path, file_path_size, ".mygit/objects/%c%c/%s",
             hash[0], hash[1], hash + 2);
}

/* Read the raw object file for a hash. */
static unsigned char *read_object_file(const char *hash, size_t *object_size)
{
    unsigned char *object_data;
    unsigned char *header_end;
    char file_path[512];

    if (strlen(hash) != SHA1_HEX_LENGTH) {
        fprintf(stderr, "Error: invalid object hash '%s'\n", hash);
        return NULL;
    }

    object_path_from_hash(hash, file_path, sizeof(file_path));

    if (!read_file(file_path, &object_data, object_size)) {
        return NULL;
    }

    header_end = memchr(object_data, '\0', *object_size);
    if (header_end == NULL) {
        fprintf(stderr, "Error: invalid object '%s'\n", hash);
        free(object_data);
        return NULL;
    }

    return object_data;
}

/* Return the bytes after the object header's NUL separator. */
static unsigned char *get_object_payload(unsigned char *object_data,
                                         size_t object_size)
{
    unsigned char *header_end;

    header_end = memchr(object_data, '\0', object_size);
    if (header_end == NULL) {
        return NULL;
    }

    return header_end + 1;
}

/* Return the payload size so checkout can restore exact file bytes. */
static size_t get_object_payload_size(unsigned char *object_data,
                                      size_t object_size)
{
    unsigned char *payload;

    payload = get_object_payload(object_data, object_size);
    if (payload == NULL) {
        return 0;
    }

    return object_size - (size_t)(payload - object_data);
}

/* Check the object header type before parsing its payload. */
static int object_has_type(unsigned char *object_data,
                           size_t object_size,
                           const char *type)
{
    size_t type_size;

    type_size = strlen(type);
    if (object_size <= type_size) {
        return 0;
    }

    return strncmp((char *)object_data, type, type_size) == 0 &&
           object_data[type_size] == ' ';
}

/* Copy the value after a named line prefix, such as "parent ". */
static void copy_line_value(char *line,
                            const char *prefix,
                            char *out,
                            size_t out_size)
{
    size_t prefix_size;

    prefix_size = strlen(prefix);
    if (strncmp(line, prefix, prefix_size) != 0) {
        return;
    }

    snprintf(out, out_size, "%s", line + prefix_size);
}

/* Print one commit payload and return its parent hash, if any. */
static void print_commit_payload(const char *hash,
                                 char *payload,
                                 char parent_hash[SHA1_HEX_LENGTH + 1],
                                 char tree_hash[SHA1_HEX_LENGTH + 1])
{
    char message[1024];
    char timestamp[128];
    char *line;

    message[0] = '\0';
    timestamp[0] = '\0';
    parent_hash[0] = '\0';
    tree_hash[0] = '\0';

    line = strtok(payload, "\n");
    while (line != NULL) {
        copy_line_value(line, "message ", message, sizeof(message));
        copy_line_value(line, "timestamp ", timestamp, sizeof(timestamp));
        copy_line_value(line, "parent ", parent_hash, SHA1_HEX_LENGTH + 1);
        copy_line_value(line, "tree ", tree_hash, SHA1_HEX_LENGTH + 1);
        line = strtok(NULL, "\n");
    }

    printf(COLOR_BOLD "commit %s" COLOR_RESET "\n", hash);
    printf("Message: %s\n", message);
    printf("Date:    %s\n\n", timestamp);
}

/* Read a tree object and print each filename listed in it. */
static int print_tree_files(const char *tree_hash)
{
    unsigned char *object_data;
    size_t object_size;
    char *payload;
    char *line;
    char *space;

    if (tree_hash[0] == '\0') {
        return 1;
    }

    object_data = read_object_file(tree_hash, &object_size);
    if (object_data == NULL) {
        return 0;
    }

    if (!object_has_type(object_data, object_size, "tree")) {
        fprintf(stderr, "Error: object '%s' is not a tree\n", tree_hash);
        free(object_data);
        return 0;
    }

    payload = (char *)get_object_payload(object_data, object_size);
    if (payload == NULL) {
        fprintf(stderr, "Error: invalid tree object '%s'\n", tree_hash);
        free(object_data);
        return 0;
    }

    printf(COLOR_BOLD "Files:" COLOR_RESET "\n\n");

    line = strtok(payload, "\n");
    while (line != NULL) {
        space = strchr(line, ' ');
        if (space != NULL) {
            *space = '\0';
            printf("* %s\n", line);
        }

        line = strtok(NULL, "\n");
    }

    printf("\n");

    free(object_data);
    return 1;
}

/* Extract the tree hash from a commit payload. */
static void find_tree_hash(char *payload, char tree_hash[SHA1_HEX_LENGTH + 1])
{
    char *line;

    tree_hash[0] = '\0';

    line = strtok(payload, "\n");
    while (line != NULL) {
        copy_line_value(line, "tree ", tree_hash, SHA1_HEX_LENGTH + 1);
        line = strtok(NULL, "\n");
    }
}

/* Extract the parent hash from a commit payload. */
static void find_parent_hash(char *payload, char parent_hash[SHA1_HEX_LENGTH + 1])
{
    char *line;

    parent_hash[0] = '\0';

    line = strtok(payload, "\n");
    while (line != NULL) {
        copy_line_value(line, "parent ", parent_hash, SHA1_HEX_LENGTH + 1);
        line = strtok(NULL, "\n");
    }
}

/* Read a tree object and print only the filenames it contains. */
static int print_tree_file_names(const char *tree_hash)
{
    unsigned char *object_data;
    size_t object_size;
    char *payload;
    char *line;
    char *space;

    object_data = read_object_file(tree_hash, &object_size);
    if (object_data == NULL) {
        return 0;
    }

    if (!object_has_type(object_data, object_size, "tree")) {
        fprintf(stderr, "Error: object '%s' is not a tree\n", tree_hash);
        free(object_data);
        return 0;
    }

    payload = (char *)get_object_payload(object_data, object_size);
    if (payload == NULL) {
        fprintf(stderr, "Error: invalid tree object '%s'\n", tree_hash);
        free(object_data);
        return 0;
    }

    line = strtok(payload, "\n");
    while (line != NULL) {
        space = strchr(line, ' ');
        if (space != NULL) {
            *space = '\0';
            printf("%s\n", line);
        }

        line = strtok(NULL, "\n");
    }

    free(object_data);
    return 1;
}

/* Restore one blob object into its working-tree file. */
static int checkout_blob(const char *file_name, const char *blob_hash)
{
    unsigned char *object_data;
    unsigned char *payload;
    size_t object_size;
    size_t payload_size;
    int ok;

    object_data = read_object_file(blob_hash, &object_size);
    if (object_data == NULL) {
        return 0;
    }

    if (!object_has_type(object_data, object_size, "blob")) {
        fprintf(stderr, "Error: object '%s' is not a blob\n", blob_hash);
        free(object_data);
        return 0;
    }

    payload = get_object_payload(object_data, object_size);
    if (payload == NULL) {
        fprintf(stderr, "Error: invalid blob object '%s'\n", blob_hash);
        free(object_data);
        return 0;
    }

    payload_size = get_object_payload_size(object_data, object_size);
    ok = write_file(file_name, payload, payload_size);

    free(object_data);
    return ok;
}

/* Restore every file listed by a tree object. */
static int checkout_tree(const char *tree_hash)
{
    unsigned char *object_data;
    size_t object_size;
    char *payload;
    char *line;
    char *space;

    object_data = read_object_file(tree_hash, &object_size);
    if (object_data == NULL) {
        return 0;
    }

    if (!object_has_type(object_data, object_size, "tree")) {
        fprintf(stderr, "Error: object '%s' is not a tree\n", tree_hash);
        free(object_data);
        return 0;
    }

    payload = (char *)get_object_payload(object_data, object_size);
    if (payload == NULL) {
        fprintf(stderr, "Error: invalid tree object '%s'\n", tree_hash);
        free(object_data);
        return 0;
    }

    line = strtok(payload, "\n");
    while (line != NULL) {
        space = strchr(line, ' ');
        if (space != NULL) {
            *space = '\0';
            if (!checkout_blob(line, space + 1)) {
                free(object_data);
                return 0;
            }
        }

        line = strtok(NULL, "\n");
    }

    free(object_data);
    return 1;
}

/* Copy a filename out of a strtok-mutated tree line. */
static char *copy_filename(const char *file_name, size_t name_size)
{
    char *copy;

    copy = malloc(name_size + 1);
    if (copy == NULL) {
        fprintf(stderr, "Error: out of memory\n");
        return NULL;
    }

    memcpy(copy, file_name, name_size);
    copy[name_size] = '\0';
    return copy;
}

/* Free the flat filename list built from a tree object. */
static void free_tree_files(char **tree_files, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        free(tree_files[i]);
    }

    free(tree_files);
}

int file_in_tree(const char *filename, char **tree_files, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        if (strcmp(filename, tree_files[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

/* Build a simple list of filenames from a flat tree object. */
static int collect_tree_files(const char *tree_hash,
                              char ***out_files,
                              int *out_count)
{
    unsigned char *object_data;
    size_t object_size;
    size_t payload_size;
    char **tree_files;
    char *payload;
    char *line;
    char *space;
    int count;

    *out_files = NULL;
    *out_count = 0;

    object_data = read_object_file(tree_hash, &object_size);
    if (object_data == NULL) {
        return 0;
    }

    if (!object_has_type(object_data, object_size, "tree")) {
        fprintf(stderr, "Error: object '%s' is not a tree\n", tree_hash);
        free(object_data);
        return 0;
    }

    payload = (char *)get_object_payload(object_data, object_size);
    if (payload == NULL) {
        fprintf(stderr, "Error: invalid tree object '%s'\n", tree_hash);
        free(object_data);
        return 0;
    }

    payload_size = get_object_payload_size(object_data, object_size);
    tree_files = malloc(sizeof(char *) * (payload_size + 1));
    if (tree_files == NULL) {
        fprintf(stderr, "Error: out of memory\n");
        free(object_data);
        return 0;
    }

    count = 0;
    line = strtok(payload, "\n");
    while (line != NULL) {
        space = strchr(line, ' ');
        if (space != NULL) {
            tree_files[count] = copy_filename(line, (size_t)(space - line));
            if (tree_files[count] == NULL) {
                free_tree_files(tree_files, count);
                free(object_data);
                return 0;
            }
            count++;
        }

        line = strtok(NULL, "\n");
    }

    free(object_data);
    *out_files = tree_files;
    *out_count = count;
    return 1;
}

static void free_tree_entries(TreeEntry *entries, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        free(entries[i].filename);
    }

    free(entries);
}

/* Build filename -> blob hash entries from the last commit tree. */
static int collect_tree_entries(const char *tree_hash,
                                TreeEntry **out_entries,
                                int *out_count)
{
    unsigned char *object_data;
    size_t object_size;
    size_t payload_size;
    TreeEntry *entries;
    char *payload;
    char *line;
    char *space;
    int count;

    *out_entries = NULL;
    *out_count = 0;

    object_data = read_object_file(tree_hash, &object_size);
    if (object_data == NULL) {
        return 0;
    }

    if (!object_has_type(object_data, object_size, "tree")) {
        fprintf(stderr, "Error: object '%s' is not a tree\n", tree_hash);
        free(object_data);
        return 0;
    }

    payload = (char *)get_object_payload(object_data, object_size);
    if (payload == NULL) {
        fprintf(stderr, "Error: invalid tree object '%s'\n", tree_hash);
        free(object_data);
        return 0;
    }

    payload_size = get_object_payload_size(object_data, object_size);
    entries = calloc(payload_size + 1, sizeof(TreeEntry));
    if (entries == NULL) {
        fprintf(stderr, "Error: out of memory\n");
        free(object_data);
        return 0;
    }

    count = 0;
    line = strtok(payload, "\n");
    while (line != NULL) {
        space = strchr(line, ' ');
        if (space != NULL) {
            entries[count].filename = copy_filename(line, (size_t)(space - line));
            if (entries[count].filename == NULL) {
                free_tree_entries(entries, count);
                free(object_data);
                return 0;
            }

            snprintf(entries[count].hash, sizeof(entries[count].hash),
                     "%s", space + 1);
            count++;
        }

        line = strtok(NULL, "\n");
    }

    free(object_data);
    *out_entries = entries;
    *out_count = count;
    return 1;
}

static const char *tree_hash_for_file(const char *filename,
                                      TreeEntry *entries,
                                      int count)
{
    int i;

    for (i = 0; i < count; i++) {
        if (strcmp(filename, entries[i].filename) == 0) {
            return entries[i].hash;
        }
    }

    return NULL;
}

/* Return the tree hash for HEAD, or an empty string if there are no commits. */
static int get_head_tree_hash(char tree_hash[SHA1_HEX_LENGTH + 1])
{
    unsigned char *object_data;
    size_t object_size;
    char head_hash[SHA1_HEX_LENGTH + 1];
    char *payload;

    tree_hash[0] = '\0';
    read_head_hash(head_hash);
    if (head_hash[0] == '\0') {
        return 1;
    }

    object_data = read_object_file(head_hash, &object_size);
    if (object_data == NULL) {
        return 0;
    }

    if (!object_has_type(object_data, object_size, "commit")) {
        fprintf(stderr, "Error: object '%s' is not a commit\n", head_hash);
        free(object_data);
        return 0;
    }

    payload = (char *)get_object_payload(object_data, object_size);
    if (payload == NULL) {
        fprintf(stderr, "Error: invalid commit object '%s'\n", head_hash);
        free(object_data);
        return 0;
    }

    find_tree_hash(payload, tree_hash);
    free(object_data);
    return 1;
}

/* This checkout only handles flat files, so directories are skipped. */
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

/* Delete working-tree files that are not present in the target tree. */
static int remove_extra_files(char **tree_files, int count)
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir(".");
    if (dir == NULL) {
        fprintf(stderr, "Error: cannot open current directory\n");
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, ".mygit") == 0) {
            continue;
        }

        if (is_directory(entry->d_name)) {
            continue;
        }

        if (!file_in_tree(entry->d_name, tree_files, count) &&
            remove(entry->d_name) != 0) {
            fprintf(stderr, "Error: cannot remove '%s'\n", entry->d_name);
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return 1;
}

/* Compute the same hash that a blob object would have, without storing it. */
static int compute_blob_hash(const unsigned char *file_data,
                             size_t file_size,
                             char out_hash[SHA1_HEX_LENGTH + 1])
{
    char header[64];
    unsigned char *object_data;
    size_t object_size;
    int header_size;

    header_size = snprintf(header, sizeof(header), "blob %zu", file_size);
    if (header_size < 0 || (size_t)header_size >= sizeof(header)) {
        fprintf(stderr, "Error: object header is too large\n");
        return 0;
    }

    object_size = (size_t)header_size + 1 + file_size;
    object_data = malloc(object_size);
    if (object_data == NULL) {
        fprintf(stderr, "Error: out of memory\n");
        return 0;
    }

    memcpy(object_data, header, (size_t)header_size);
    object_data[header_size] = '\0';
    memcpy(object_data + header_size + 1, file_data, file_size);

    sha1_hex(object_data, object_size, out_hash);
    free(object_data);
    return 1;
}

/* Add one bullet to a grouped status section. */
static int append_status_line(char *buffer,
                              size_t buffer_size,
                              const char *file_name,
                              const char *color)
{
    size_t used;
    int written;

    used = strlen(buffer);
    written = snprintf(buffer + used, buffer_size - used,
                       "%s* %s%s\n", color, file_name, COLOR_RESET);
    return written >= 0 && (size_t)written < buffer_size - used;
}

/* Return m, u, or d for modified, unchanged, or deleted. */
static char get_file_status(const char *expected_hash, const char *file_name)
{
    unsigned char *file_data;
    size_t file_size;
    char actual_hash[SHA1_HEX_LENGTH + 1];
    char status;

    if (!path_exists(file_name)) {
        return 'd';
    }

    if (!read_file(file_name, &file_data, &file_size)) {
        return 'e';
    }

    if (!compute_blob_hash(file_data, file_size, actual_hash)) {
        free(file_data);
        return 'e';
    }

    if (strcmp(expected_hash, actual_hash) == 0) {
        status = 'u';
    } else {
        status = 'm';
    }

    free(file_data);
    return status;
}

/* Store the current index snapshot as a commit object and move HEAD to it. */
int mygit_commit(const char *message)
{
    unsigned char *payload;
    size_t payload_size;
    char hash[SHA1_HEX_LENGTH + 1];
    char head_contents[SHA1_HEX_LENGTH + 2];
    char parent_hash[SHA1_HEX_LENGTH + 1];
    char tree_hash[SHA1_HEX_LENGTH + 1];

    if (!require_repository()) {
        return 1;
    }

    read_head_hash(parent_hash);

    if (!build_tree_from_index(tree_hash)) {
        return 1;
    }

    payload = build_commit_payload(message, parent_hash, tree_hash, &payload_size);

    if (payload == NULL) {
        return 1;
    }

    if (!write_object("commit", payload, payload_size, hash)) {
        free(payload);
        return 1;
    }

    free(payload);

    snprintf(head_contents, sizeof(head_contents), "%s\n", hash);
    if (!write_file(".mygit/HEAD",
                    (const unsigned char *)head_contents,
                    strlen(head_contents))) {
        return 1;
    }

    printf("[%s] %s\n", hash, message);
    return 0;
}

/* Walk parent links from HEAD and print each commit's basic metadata. */
int mygit_log(void)
{
    unsigned char *object_data;
    size_t object_size;
    char current_hash[SHA1_HEX_LENGTH + 1];
    char parent_hash[SHA1_HEX_LENGTH + 1];
    char tree_hash[SHA1_HEX_LENGTH + 1];
    char *payload;

    if (!require_repository()) {
        return 1;
    }

    read_head_hash(current_hash);
    if (current_hash[0] == '\0') {
        printf("No commits yet\n");
        return 0;
    }

    while (current_hash[0] != '\0') {
        object_data = read_object_file(current_hash, &object_size);
        if (object_data == NULL) {
            return 1;
        }

        if (!object_has_type(object_data, object_size, "commit")) {
            fprintf(stderr, "Error: object '%s' is not a commit\n", current_hash);
            free(object_data);
            return 1;
        }

        payload = (char *)get_object_payload(object_data, object_size);
        if (payload == NULL) {
            fprintf(stderr, "Error: invalid commit object '%s'\n", current_hash);
            free(object_data);
            return 1;
        }

        print_commit_payload(current_hash, payload, parent_hash, tree_hash);
        free(object_data);

        if (!print_tree_files(tree_hash)) {
            return 1;
        }

        snprintf(current_hash, sizeof(current_hash), "%s", parent_hash);
    }

    return 0;
}

/* Restore all files from the tree referenced by a commit. */
int mygit_checkout(const char *commit_hash)
{
    unsigned char *object_data;
    size_t object_size;
    char **tree_files;
    char target_hash[SHA1_HEX_LENGTH + 1];
    const char *target_commit_hash;
    char tree_hash[SHA1_HEX_LENGTH + 1];
    int tree_file_count;
    int ok;
    char *payload;

    if (!require_repository()) {
        return 1;
    }

    if (strcmp(commit_hash, "HEAD") == 0) {
        read_head_hash(target_hash);
        if (target_hash[0] == '\0') {
            fprintf(stderr, "Error: no commits yet\n");
            return 1;
        }
        target_commit_hash = target_hash;
    } else {
        target_commit_hash = commit_hash;
    }

    object_data = read_object_file(target_commit_hash, &object_size);
    if (object_data == NULL) {
        return 1;
    }

    if (!object_has_type(object_data, object_size, "commit")) {
        fprintf(stderr, "Error: object '%s' is not a commit\n", target_commit_hash);
        free(object_data);
        return 1;
    }

    payload = (char *)get_object_payload(object_data, object_size);
    if (payload == NULL) {
        fprintf(stderr, "Error: invalid commit object '%s'\n", target_commit_hash);
        free(object_data);
        return 1;
    }

    find_tree_hash(payload, tree_hash);
    free(object_data);

    if (tree_hash[0] == '\0') {
        fprintf(stderr, "Error: commit '%s' has no tree\n", target_commit_hash);
        return 1;
    }

    if (!collect_tree_files(tree_hash, &tree_files, &tree_file_count)) {
        return 1;
    }

    ok = remove_extra_files(tree_files, tree_file_count) &&
         checkout_tree(tree_hash);

    free_tree_files(tree_files, tree_file_count);

    if (!ok) {
        return 1;
    }

    printf("Switched to commit %s\n", target_commit_hash);
    return 0;
}

/* Compare the current working tree with the hashes recorded in the index. */
int mygit_status(void)
{
    unsigned char *index_data;
    size_t index_size;
    size_t section_size;
    TreeEntry *commit_entries;
    int commit_entry_count;
    char *modified;
    char *unchanged;
    char *deleted;
    char *staged;
    char tree_hash[SHA1_HEX_LENGTH + 1];
    const char *committed_hash;
    char *line;
    char *space;
    char status;
    int is_staged;
    int ok;

    if (!require_repository()) {
        return 1;
    }

    if (!read_file(".mygit/index", &index_data, &index_size)) {
        return 1;
    }

    if (index_size == 0) {
        printf("Index is empty\n");
        free(index_data);
        return 0;
    }

    if (!get_head_tree_hash(tree_hash)) {
        free(index_data);
        return 1;
    }

    commit_entries = NULL;
    commit_entry_count = 0;
    if (tree_hash[0] != '\0' &&
        !collect_tree_entries(tree_hash, &commit_entries, &commit_entry_count)) {
        free(index_data);
        return 1;
    }

    section_size = (index_size * 3) + 256;
    staged = calloc(section_size, 1);
    modified = calloc(section_size, 1);
    unchanged = calloc(section_size, 1);
    deleted = calloc(section_size, 1);
    if (staged == NULL || modified == NULL ||
        unchanged == NULL || deleted == NULL) {
        fprintf(stderr, "Error: out of memory\n");
        free(staged);
        free(modified);
        free(unchanged);
        free(deleted);
        free_tree_entries(commit_entries, commit_entry_count);
        free(index_data);
        return 1;
    }

    ok = 1;
    line = strtok((char *)index_data, "\n");
    while (line != NULL) {
        space = strchr(line, ' ');
        if (space != NULL) {
            *space = '\0';
            committed_hash = tree_hash_for_file(space + 1,
                                                commit_entries,
                                                commit_entry_count);
            is_staged = committed_hash == NULL ||
                        strcmp(line, committed_hash) != 0;

            if (is_staged) {
                ok = append_status_line(staged, section_size,
                                        space + 1, COLOR_BOLD);
                if (!ok) {
                    break;
                }
            }

            status = get_file_status(line, space + 1);
            if (status == 'm') {
                if (!is_staged) {
                    ok = append_status_line(modified, section_size,
                                            space + 1, COLOR_YELLOW);
                }
            } else if (status == 'u') {
                if (!is_staged) {
                    ok = append_status_line(unchanged, section_size,
                                            space + 1, COLOR_GREEN);
                }
            } else if (status == 'd') {
                ok = append_status_line(deleted, section_size,
                                        space + 1, COLOR_RED);
            } else {
                ok = 0;
            }

            if (!ok) {
                break;
            }
        }

        line = strtok(NULL, "\n");
    }

    if (ok) {
        printf(COLOR_BOLD "Staged:" COLOR_RESET "\n\n%s\n", staged);
        printf(COLOR_BOLD "Modified:" COLOR_RESET "\n\n%s\n", modified);
        printf(COLOR_BOLD "Unchanged:" COLOR_RESET "\n\n%s\n", unchanged);
        printf(COLOR_BOLD "Deleted:" COLOR_RESET "\n\n%s", deleted);
    }

    free(staged);
    free(modified);
    free(unchanged);
    free(deleted);
    free_tree_entries(commit_entries, commit_entry_count);
    free(index_data);
    return ok ? 0 : 1;
}

/* Count commits by following parent links from HEAD. */
int mygit_count(void)
{
    unsigned char *object_data;
    size_t object_size;
    char current_hash[SHA1_HEX_LENGTH + 1];
    char parent_hash[SHA1_HEX_LENGTH + 1];
    char *payload;
    int count;

    if (!require_repository()) {
        return 1;
    }

    read_head_hash(current_hash);
    if (current_hash[0] == '\0') {
        printf("No commits yet\n");
        return 0;
    }

    count = 0;
    while (current_hash[0] != '\0') {
        object_data = read_object_file(current_hash, &object_size);
        if (object_data == NULL) {
            return 1;
        }

        if (!object_has_type(object_data, object_size, "commit")) {
            fprintf(stderr, "Error: object '%s' is not a commit\n", current_hash);
            free(object_data);
            return 1;
        }

        payload = (char *)get_object_payload(object_data, object_size);
        if (payload == NULL) {
            fprintf(stderr, "Error: invalid commit object '%s'\n", current_hash);
            free(object_data);
            return 1;
        }

        find_parent_hash(payload, parent_hash);
        free(object_data);

        count++;
        snprintf(current_hash, sizeof(current_hash), "%s", parent_hash);
    }

    printf("Total commits: %d\n", count);
    return 0;
}

/* Print filenames from the tree referenced by HEAD. */
int mygit_files(void)
{
    unsigned char *object_data;
    size_t object_size;
    char current_hash[SHA1_HEX_LENGTH + 1];
    char tree_hash[SHA1_HEX_LENGTH + 1];
    char *payload;

    if (!require_repository()) {
        return 1;
    }

    read_head_hash(current_hash);
    if (current_hash[0] == '\0') {
        printf("No commits yet\n");
        return 0;
    }

    object_data = read_object_file(current_hash, &object_size);
    if (object_data == NULL) {
        return 1;
    }

    if (!object_has_type(object_data, object_size, "commit")) {
        fprintf(stderr, "Error: object '%s' is not a commit\n", current_hash);
        free(object_data);
        return 1;
    }

    payload = (char *)get_object_payload(object_data, object_size);
    if (payload == NULL) {
        fprintf(stderr, "Error: invalid commit object '%s'\n", current_hash);
        free(object_data);
        return 1;
    }

    find_tree_hash(payload, tree_hash);
    free(object_data);

    if (tree_hash[0] == '\0') {
        fprintf(stderr, "Error: commit '%s' has no tree\n", current_hash);
        return 1;
    }

    printf("Files in current commit:\n");
    if (!print_tree_file_names(tree_hash)) {
        return 1;
    }

    return 0;
}
