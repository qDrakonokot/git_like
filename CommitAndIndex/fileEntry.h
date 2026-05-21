#ifndef FILEENTRY_H
#define FILEENTRY_H

#define FILE_NAME_MAX 4096
#define HASH_LEN 41
#define FILE_TYPE_MAX 5
#define ENTRY_TYPE_BLOB "blob"
#define ENTRY_TYPE_REF "ref"
#define ENTRY_TYPE_DEL "del"
#define NULL_HASH "0000000000000000000000000000000000000000"

typedef struct {
    char name[FILE_NAME_MAX];
    char type[FILE_TYPE_MAX];
    char hash[HASH_LEN];
} FileEntry;

void fileEntry_init(FileEntry* entry);
void fileEntry_set(FileEntry* entry, const char* name, const char* type, const char* hash);

#endif
