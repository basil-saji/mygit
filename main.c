#include "add.h"
#include "commit.h"
#include "hash.h"
#include "init.h"
#include "object.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mygit_log(void);
int mygit_checkout(const char *commit_hash);
int mygit_status(void);
int mygit_audit(const char *path);
int mygit_count(void);
int mygit_files(void);

/* Show the small command set supported by this learning implementation. */
static void print_usage(void)
{
    printf("usage:\n");
    printf("  mygit init\n");
    printf("  mygit hash-object <file>\n");
    printf("  mygit add <file>\n");
    printf("  mygit commit -m \"message\"\n");
    printf("  mygit log\n");
    printf("  mygit checkout <commit_hash|HEAD>\n");
    printf("  mygit status\n");
    printf("  mygit audit <file|.>\n");
    printf("  mygit count\n");
    printf("  mygit files\n");
}

/* Implement hash-object: create a blob object and print its object id. */
static int mygit_hash_object(const char *file_path)
{
    unsigned char *file_data;
    size_t file_size;
    char hash[SHA1_HEX_LENGTH + 1];

    if (!require_repository()) {
        return 1;
    }

    if (!read_file(file_path, &file_data, &file_size)) {
        return 1;
    }

    if (!write_object("blob", file_data, file_size, hash)) {
        free(file_data);
        return 1;
    }

    free(file_data);
    printf("%s\n", hash);
    return 0;
}

/* Parse argv and dispatch each subcommand to its focused module. */
int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "init") == 0) {
        if (argc != 2) {
            print_usage();
            return 1;
        }
        return mygit_init();
    }

    if (strcmp(argv[1], "hash-object") == 0) {
        if (argc != 3) {
            print_usage();
            return 1;
        }
        return mygit_hash_object(argv[2]);
    }

    if (strcmp(argv[1], "add") == 0) {
        if (argc != 3) {
            print_usage();
            return 1;
        }
        return mygit_add(argv[2]);
    }

    if (strcmp(argv[1], "commit") == 0) {
        if (argc != 4 || strcmp(argv[2], "-m") != 0) {
            print_usage();
            return 1;
        }
        return mygit_commit(argv[3]);
    }

    if (strcmp(argv[1], "log") == 0) {
        if (argc != 2) {
            print_usage();
            return 1;
        }
        return mygit_log();
    }

    if (strcmp(argv[1], "checkout") == 0) {
        if (argc != 3) {
            print_usage();
            return 1;
        }
        return mygit_checkout(argv[2]);
    }

    if (strcmp(argv[1], "status") == 0) {
        if (argc != 2) {
            print_usage();
            return 1;
        }
        return mygit_status();
    }

    if (strcmp(argv[1], "audit") == 0) {
        if (argc != 3) {
            print_usage();
            return 1;
        }
        return mygit_audit(argv[2]);
    }

    if (strcmp(argv[1], "count") == 0) {
        if (argc != 2) {
            print_usage();
            return 1;
        }
        return mygit_count();
    }

    if (strcmp(argv[1], "files") == 0) {
        if (argc != 2) {
            print_usage();
            return 1;
        }
        return mygit_files();
    }

    print_usage();
    return 1;
}
