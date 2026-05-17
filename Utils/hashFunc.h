// Хеш функция SHA-1 для хеширования файлов

#ifndef HASHFUNC_H
#define HASHFUNC_H

#include <stdint.h>
#include <stddef.h>

#define SHA1_BLOCK_SIZE 20  // SHA-1 outputs 20 bytes

typedef struct {
    uint64_t length;
    uint32_t buffer[16];
    uint32_t hash[5];
    uint8_t buffer_offset;
} SHA1_CTX;

void compute_hash(const void *data, size_t len, char hex_out[41]);

#endif