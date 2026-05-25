#include "hashmap.h"
#include "CommitAndIndex/fileEntry.h"
#include "ErrorsDefenition/errors.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAP_INIT_SIZE 113
#define LOAD_FACTOR 0.75

typedef enum {
    EMPTY,      
    OCCUPIED,   
    DELETED     
} EntryStatus;

static FileEntry* map = NULL;
static EntryStatus* status = NULL;
static size_t hashmap_size = 0;
static size_t total_occupied = 0;
static char initialized = 0;


static uint32_t hash_djb2(const unsigned char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash;
}


static size_t next_prime(size_t n) {
    static const size_t primes[] = { 113, 223, 449, 907, 1823, 3659, 7321, 14639, 29287, 58579, 117191, 234383, 468779, 937567, 1875149, 3750311, 7500637, 15001289, 30002599, 60005209, 120010423, 240020851, 480041711, 960083357, 1920166723, 3840333467 };
    for (size_t i = 0; i < sizeof(primes)/sizeof(primes[0]); ++i) {
        if (primes[i] >= n) return primes[i];
    }
    return n;
}


static int hashmap_rehash(size_t new_size) {
    FileEntry* new_map = (FileEntry*)malloc(new_size * sizeof(FileEntry));
    EntryStatus* new_status = (EntryStatus*)malloc(new_size * sizeof(EntryStatus));
    if (new_map == NULL || new_status == NULL) {
        int errno_s = errno;
        free(new_map); free(new_status);
        errno = errno_s;
        return -1;
    }
    for (size_t i = 0; i < new_size; ++i) {
        new_status[i] = EMPTY;
        fileEntry_init(&new_map[i]);
    }
    
    // Перехешируем все занятые элементы из старой таблицы
    for (size_t i = 0; i < hashmap_size; ++i) {
        if (status[i] == OCCUPIED) {
            const FileEntry* entry = &map[i];
            uint32_t hash = hash_djb2((const unsigned char*)entry->name);
            int index = (int)(hash % new_size);
            int original = index;
            while (new_status[index] == OCCUPIED) {
                index = (index + 1) % new_size;
                if (index == original) {
                    int errno_s = errno;
                    free(new_map); free(new_status);
                    errno = errno_s;
                    return -1;
                }
            }
            new_map[index] = *entry;
            new_status[index] = OCCUPIED;
        }
    }
    
    free(map); free(status);
    map = new_map;
    status = new_status;
    hashmap_size = new_size;
    return 0;
}


int hashmap_init (void) {
    map = (FileEntry*)malloc(MAP_INIT_SIZE * sizeof(FileEntry));
    if (map == NULL) return -1;
    status = (EntryStatus*)malloc(MAP_INIT_SIZE * sizeof(EntryStatus));
    if (status == NULL) { free(map); return -1; }
    for (int i = 0; i < MAP_INIT_SIZE; ++i) {
        status[i] = EMPTY;
        fileEntry_init(&map[i]);
    }
    initialized = 1;
    total_occupied = 0;
    hashmap_size = MAP_INIT_SIZE;
    return 0;
} 


static int hashmap_find_entry (const char* name) {
    if (initialized == 0) { SET_ERR(MY_ERR_HASHMAP_DOES_NOT_EXISTS); return -1; }

    uint32_t hash = hash_djb2((const unsigned char*)name);
    int index = (int)(hash % hashmap_size);
    int original = index;

    while (status[index] != EMPTY) {
        if (status[index] == OCCUPIED && strcmp(map[index].name, name) ==  0) {
            return index;
        }
        index = (index + 1) % hashmap_size;
        if (index == original) { SET_ERR(MY_ERR_HASHMAP_FULL); break; }
    }
    
    SET_ERR(MY_ERR_HASHMAP_NOT_FOUND);
    return -1;
}


int hashmap_insert_entry (const FileEntry* entry) {
    if (entry == NULL) { SET_ERR(EINVAL); return -1; }
    if (initialized == 0) { SET_ERR(MY_ERR_HASHMAP_DOES_NOT_EXISTS); return -1; }

    uint32_t hash = hash_djb2((const unsigned char*)entry->name);
    int index = (int)(hash % hashmap_size);
    int original = index;
    int first_deleted = -1;

    while (status[index] != EMPTY) {
        if (status[index] == OCCUPIED && strcmp(map[index].name, entry->name) == 0) {
            map[index] = *entry;
            return 0;
        }
        else if (status[index] == DELETED && first_deleted == -1) {
            first_deleted = index;
        }
        index = (index + 1) % hashmap_size;
        if (index == original) { 
            if (first_deleted == -1) {
                SET_ERR(MY_ERR_HASHMAP_FULL); 
                return -1; 
            }
            else {
                break;
            }
        }
    }

    int pos;
    if (first_deleted != -1) {
        pos = first_deleted;
    } 
    else {
        pos = index;
    }
    map[pos] = *entry;
    status[pos] = OCCUPIED;
    total_occupied++;

    if ((double)total_occupied / hashmap_size > LOAD_FACTOR) {
        size_t new_size = next_prime(hashmap_size * 2);
        int ret_c = hashmap_rehash(new_size);
        if (ret_c != 0) {
            SET_ERR(MY_ERR_REHASH_FAILED);
        }
    }

    return 0;
}


int hashmap_delete_entry(const char* name) {
    if (name == NULL) { SET_ERR(EINVAL); return -1; }
    int idx = hashmap_find_entry(name);
    if (idx == -1) return -1;
    status[idx] = DELETED;
    total_occupied--;
    return 0;
}


FileEntry* hashmap_get_entry(const char *name) {
    if (name == NULL) { SET_ERR(EINVAL); return NULL; }
    int idx = hashmap_find_entry(name);
    if (idx == -1) return NULL;
    return &map[idx];
}


void hashmap_destroy (void) {
    if (initialized == 0) { SET_ERR(MY_ERR_HASHMAP_DOES_NOT_EXISTS); return; }
    free(map);
    free(status);
    map = NULL;
    status = NULL;
    hashmap_size = 0;
    initialized = 0;
}
