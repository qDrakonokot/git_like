#ifndef WORKWITHFILES_H
#define WORKWITHFILES_H

#include "CommitAndIndex/fileEntry.h"
#include "DataStructures/vector.h"
#include <stddef.h>

#define PATH_MAX 4096
#define CHANGED_FILE_TYPE_ADD "add"
#define CHANGED_FILE_TYPE_DEL "del"
#define CHANGED_FILE_TYPE_MOD "mod"

typedef struct {
    char name[FILE_NAME_MAX];
    char hash[HASH_LEN];
} FileInfo;

int mkdir_p (const char* dir_path);
int write_file (const char* path, const void* data, size_t size);
int write_text_file (const char *path, const char *text);
char* read_file (const char* path, size_t* out_size);
void join_path(char *out, const char *base, const char *name);
int create_and_write_file (const char* path, const void* data, size_t size);
int get_all_files (const char* path, Vector* vec);
int check_dir (const char* path);
int remove_all_repofiles (const char* path);
char** split_buf_by_lines (char* buf, int* out_lines_cnt);
int is_file_binary (const char* filepath);

#endif