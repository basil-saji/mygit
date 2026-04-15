#ifndef ADD_H
#define ADD_H

/* Store a file as a blob and add "<hash> <filename>" to .mygit/index. */
int mygit_add(const char *file_path);

#endif
