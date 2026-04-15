#include "init.h"
#include "utils.h"

#include <stdio.h>

/* Create the minimal repository metadata that later commands depend on. */
int mygit_init(void)
{
    if (!ensure_dir(".mygit")) {
        return 1;
    }

    if (!ensure_dir(".mygit/objects")) {
        return 1;
    }

    if (!write_file(".mygit/index", (const unsigned char *)"", 0)) {
        return 1;
    }

    if (!write_file(".mygit/HEAD", (const unsigned char *)"", 0)) {
        return 1;
    }

    if (!write_file(".mygitignore",
                    (const unsigned char *)".env\n.env.local\n",
                    16)) {
        return 1;
    }

    printf("Initialized empty mygit repository in .mygit\n");
    return 0;
}
