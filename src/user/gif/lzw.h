#pragma once
#include <stdint.h>

typedef void (*lzw_emit_fn)(uint8_t index, void *user);

typedef struct {
    uint8_t  *plo, *phi, *sfx, *stk;
    uint16_t  clear_code, eoi_code, next_code;
    uint8_t   code_size, min_code_size;
    uint32_t  bit_buf;
    uint8_t   bit_count;
    uint16_t  prev_code;
    uint8_t   has_prev;
    uint8_t   done;
} lzw_ctx_t;

int  lzw_begin(lzw_ctx_t *ctx, uint8_t min_code_size);
int  lzw_feed(lzw_ctx_t *ctx, const uint8_t *in, uint32_t in_len,
              lzw_emit_fn cb, void *user);
void lzw_end(lzw_ctx_t *ctx);
