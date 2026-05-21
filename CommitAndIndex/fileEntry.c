#include "ErrorsDefenition/errors.h"
#include "fileEntry.h"
#include <string.h>

void fileEntry_init(FileEntry* entry) {
    if (entry == NULL) { SET_ERR(EINVAL); return; }
    memset(entry->name, 0, FILE_NAME_MAX); 
    memset(entry->type, 0, FILE_TYPE_MAX);
    memset(entry->hash, 0, HASH_LEN);
}

void fileEntry_set(FileEntry* entry, const char* name, const char* type, const char* hash) {
    if (entry == NULL) { SET_ERR(EINVAL); return; }
    strncpy(entry->name, name, FILE_NAME_MAX - 1);
    entry->name[FILE_NAME_MAX - 1] = '\0';

    strncpy(entry->type, type, FILE_TYPE_MAX - 1);
    entry->type[FILE_TYPE_MAX - 1] = '\0';

    if (hash != NULL) {
        strncpy(entry->hash, hash, HASH_LEN - 1);
        entry->hash[HASH_LEN - 1] = '\0';
    }
    else {
        memset(entry->hash, '0', HASH_LEN);
        entry->hash[HASH_LEN - 1] = '\0';
    }
    
}

