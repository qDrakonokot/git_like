// хеш функция SHA-1 для хеширования файлов

#include "hashFunc.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_transform(SHA1_CTX *ctx) {
    uint32_t w[80];
    uint32_t a, b, c, d, e, t;
    int i;

    uint8_t *buf = (uint8_t*)ctx->buffer;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)buf[4*i] << 24) |
            ((uint32_t)buf[4*i+1] << 16) |
            ((uint32_t)buf[4*i+2] << 8) |
            (uint32_t)buf[4*i+3];
    for (i = 16; i < 80; i++)
        w[i] = ROTL32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

    a = ctx->hash[0];
    b = ctx->hash[1];
    c = ctx->hash[2];
    d = ctx->hash[3];
    e = ctx->hash[4];

    #define F1(x, y, z) (z ^ (x & (y ^ z)))
    #define F2(x, y, z) (x ^ y ^ z)
    #define F3(x, y, z) ((x & y) | (z & (x | y)))
    #define F4(x, y, z) (x ^ y ^ z)

    for (i = 0; i < 80; i++) {
        if (i < 20) {
            t = ROTL32(a, 5) + F1(b, c, d) + e + w[i] + 0x5A827999;
        } else if (i < 40) {
            t = ROTL32(a, 5) + F2(b, c, d) + e + w[i] + 0x6ED9EBA1;
        } else if (i < 60) {
            t = ROTL32(a, 5) + F3(b, c, d) + e + w[i] + 0x8F1BBCDC;
        } else {
            t = ROTL32(a, 5) + F4(b, c, d) + e + w[i] + 0xCA62C1D6;
        }
        e = d;
        d = c;
        c = ROTL32(b, 30);
        b = a;
        a = t;
    }

    ctx->hash[0] += a;
    ctx->hash[1] += b;
    ctx->hash[2] += c;
    ctx->hash[3] += d;
    ctx->hash[4] += e;
}

static void sha1_init(SHA1_CTX *ctx) {
    ctx->length = 0;
    ctx->buffer_offset = 0;
    ctx->hash[0] = 0x67452301;
    ctx->hash[1] = 0xEFCDAB89;
    ctx->hash[2] = 0x98BADCFE;
    ctx->hash[3] = 0x10325476;
    ctx->hash[4] = 0xC3D2E1F0;
}

static void sha1_update(SHA1_CTX *ctx, const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t*)data;
    size_t i;

    for (i = 0; i < len; i++) {
        ((uint8_t*)ctx->buffer)[ctx->buffer_offset++] = bytes[i];
        if (ctx->buffer_offset == 64) {
            sha1_transform(ctx);
            ctx->buffer_offset = 0;
            ctx->length += 64;
        }
    }
}

static void sha1_final(SHA1_CTX *ctx, uint8_t out[SHA1_BLOCK_SIZE]) {
    uint64_t total_bits = (ctx->length + ctx->buffer_offset) * 8;
    int i;

    // padding
    uint8_t padding_byte = 0x80;
    sha1_update(ctx, &padding_byte, 1);
    while (ctx->buffer_offset != 56) {
        if (ctx->buffer_offset == 64) {
            sha1_transform(ctx);
            ctx->buffer_offset = 0;
        }
        uint8_t zero = 0;
        sha1_update(ctx, &zero, 1);
    }

    // append length
    for (i = 0; i < 8; i++) {
        uint8_t byte = (total_bits >> (56 - i * 8)) & 0xFF;
        sha1_update(ctx, &byte, 1);
    }

    // copy hash
    for (i = 0; i < 5; i++) {
        out[i*4]   = (ctx->hash[i] >> 24) & 0xFF;
        out[i*4+1] = (ctx->hash[i] >> 16) & 0xFF;
        out[i*4+2] = (ctx->hash[i] >> 8)  & 0xFF;
        out[i*4+3] = ctx->hash[i] & 0xFF;
    }
}

static void hash_to_hex(const uint8_t hash[20], char hex_out[41]) {
    for (int i = 0; i < 20; i++) {
        sprintf(hex_out + i * 2, "%02x", hash[i]);
    }
    hex_out[40] = '\0';
}


void compute_hash(const void *data, size_t len, char hex_out[41]) {
    SHA1_CTX ctx;
    uint8_t hash[20];
    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(&ctx, hash);
    hash_to_hex(hash, hex_out);
}