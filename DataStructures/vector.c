#include "ErrorsDefenition/errors.h"
#include "vector.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static inline int vector_increase(Vector* vec) {
    vec->cap *= 2;
    vec->arr = (void*)realloc(vec->arr, vec->cap * vec->type);
    if (vec->arr == NULL) return -1;
    return 0;
}

int vector_init(Vector* vec, size_t type) {
    if (vec == NULL) { SET_ERR(EINVAL); return -1; }
    vec->cap = 16;
    vec->len = 0;
    vec->type = type;
    vec->arr = (void*)malloc(vec->cap * vec->type);
    if (vec->arr == NULL) return -1;
    return 0;
}

int vector_push(Vector* vec, const void* elem) {
    if (elem == NULL) { SET_ERR(EINVAL); return -1; }
    if (vec->len >= vec->cap) {
        int ret_c = vector_increase(vec);
        if (ret_c != 0) return -1;
    }

    void* target = (char*)vec->arr + vec->len * vec->type;
    memcpy(target, elem, vec->type); 
    vec->len++;
    return 0;
}

void* vector_get(const Vector* vec, size_t idx){
    if (idx >= vec->len) { SET_ERR(MY_ERR_OUT_OF_VEC_SIZE); return NULL; }
    return (char*)vec->arr + idx * vec->type;
}

size_t vector_get_size(const Vector* vec){
    return vec->len;
}

void vector_destroy(Vector* vec) {
    vec->len = 0;
    vec->cap = 0;
    vec->type = 0;
    free(vec->arr);
    vec->arr = NULL;
}