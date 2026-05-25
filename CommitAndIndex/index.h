#ifndef INDEX_H
#define INDEX_H

#include "DataStructures/vector.h"
#include "fileEntry.h"

#define INDEX_TYPE_ADD "add"
#define INDEX_TYPE_DEL "del"

typedef FileEntry IndexEntry;

Vector* index_read(void);
int index_write(const Vector* entries);
int index_add(const char* filename);
int index_del(const char* filename);

#endif