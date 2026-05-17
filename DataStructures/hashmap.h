#ifndef HASHMAP_H
#define HASHMAP_H

#include "CommitAndIndex/fileEntry.h"

#define MAP_INIT_SIZE 113
#define LOAD_FACTOR 0.75

typedef enum {
    EMPTY,      
    OCCUPIED,   
    DELETED     
} EntryStatus;

int hashmap_init ();
int hashmap_insert_entry (const FileEntry* entry);
int hashmap_delete_entry(const char* name);
FileEntry* hashmap_get_entry(const char *name);
void hashmap_destroy ();

#endif