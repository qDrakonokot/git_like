#ifndef COMMIT_H
#define COMMIT_H

#include <time.h>
#include "DataStructures/vector.h"
#include "fileEntry.h"

#define BUFFER_MAX_LEN (1<<16)

typedef struct {
    char own_hash[HASH_LEN];
    char parent[HASH_LEN];
    time_t timestamp;
    char* message;
    Vector files;
} Commit;

Commit* commit_init(void);
int commit_set_message(Commit* commit, const char* msg);
int commit_add_file(Commit* commit, const char* name, const char* type, const char* hash);
int commit_parse(const char* text, Commit* commit);
void commit_destroy(Commit* commit);
int commit_write_file(Commit* commit, char* out_hash, const char* branchName);

#endif