#ifndef COMMIT_H
#define COMMIT_H

/* Create a commit object from the current index and update .mygit/HEAD. */
int mygit_commit(const char *message);

#endif
