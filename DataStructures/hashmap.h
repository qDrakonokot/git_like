#ifndef HASHMAP_H
#define HASHMAP_H

#include "CommitAndIndex/fileEntry.h"

int hashmap_init (void);
int hashmap_insert_entry (const FileEntry* entry);
int hashmap_delete_entry(const char* name);
FileEntry* hashmap_get_entry(const char *name);
void hashmap_destroy (void);

#endif