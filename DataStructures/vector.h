#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>

typedef struct {
    void* arr;
    size_t cap;
    size_t len;
    size_t type;  
} Vector;

int vector_init(Vector* vec, size_t type);
int vector_push(Vector* vec, const void* elem);
void* vector_get(const Vector* vec, size_t idx);
void vector_destroy(Vector* vec);
size_t vector_get_size(const Vector* vec);

#endif