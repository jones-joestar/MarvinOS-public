#include "lzw.h"
#include <malloc.h>

#define DICT_SIZE 4096

int lzw_begin(lzw_ctx_t *ctx, uint8_t min_code_size) {
    ctx->plo = ctx->phi = ctx->sfx = ctx->stk = NULL;
    if (min_code_size < 2 || min_code_size > 8) return -1;

    ctx->plo = malloc(DICT_SIZE);
    ctx->phi = malloc(DICT_SIZE);
    ctx->sfx = malloc(DICT_SIZE);
    ctx->stk = malloc(DICT_SIZE);

    if (!ctx->plo || !ctx->phi || !ctx->sfx || !ctx->stk) {
        lzw_end(ctx);
        return -1;
    }

    ctx->min_code_size = min_code_size;
    ctx->clear_code    = (uint16_t)(1u << min_code_size);
    ctx->eoi_code      = ctx->clear_code + 1;
    ctx->next_code     = ctx->eoi_code + 1;
    ctx->code_size     = min_code_size + 1;
    ctx->bit_buf       = 0;
    ctx->bit_count     = 0;
    ctx->has_prev      = 0;
    ctx->done          = 0;

    for (uint16_t i = 0; i < ctx->clear_code; i++)
        ctx->sfx[i] = (uint8_t)i;

    return 0;
}

int lzw_feed(lzw_ctx_t *ctx, const uint8_t *in, uint32_t in_len,
             lzw_emit_fn cb, void *user)
{
    if (ctx->done) return 0;

    uint32_t in_pos = 0;

    while (!ctx->done) {
        while (ctx->bit_count < ctx->code_size && in_pos < in_len) {
            ctx->bit_buf   |= (uint32_t)in[in_pos++] << ctx->bit_count;
            ctx->bit_count += 8;
        }
        if (ctx->bit_count < ctx->code_size)
            break;

        uint16_t code = (uint16_t)(ctx->bit_buf & ((1u << ctx->code_size) - 1));
        ctx->bit_buf   >>= ctx->code_size;
        ctx->bit_count  -= ctx->code_size;

        if (code == ctx->clear_code) {
            ctx->next_code = ctx->eoi_code + 1;
            ctx->code_size = ctx->min_code_size + 1;
            ctx->has_prev  = 0;
            continue;
        }
        if (code == ctx->eoi_code) {
            ctx->done = 1;
            break;
        }

        if (code > ctx->next_code || (code == ctx->next_code && !ctx->has_prev))
            return -1;

        int      is_kwkwk = (code == ctx->next_code);
        uint16_t entry    = is_kwkwk ? ctx->prev_code : code;

        int      top = 0;
        uint16_t c   = entry;
        while (top < DICT_SIZE) {
            ctx->stk[top++] = ctx->sfx[c];
            if (c < ctx->clear_code) break;
            c = (uint16_t)ctx->plo[c] | ((uint16_t)(ctx->phi[c] & 0x0F) << 8);
        }

        uint8_t first_byte = ctx->stk[top - 1];

        for (int i = top - 1; i >= 0; i--)
            cb(ctx->stk[i], user);
        if (is_kwkwk)
            cb(first_byte, user);

        if (ctx->has_prev && ctx->next_code < DICT_SIZE) {
            ctx->plo[ctx->next_code] = (uint8_t)(ctx->prev_code & 0xFF);
            ctx->phi[ctx->next_code] = (uint8_t)((ctx->prev_code >> 8) & 0x0F);
            ctx->sfx[ctx->next_code] = first_byte;
            ctx->next_code++;
            if (ctx->code_size < 12 && ctx->next_code == (uint16_t)(1u << ctx->code_size))
                ctx->code_size++;
        }

        ctx->prev_code = code;
        ctx->has_prev  = 1;
    }

    return 0;
}

void lzw_end(lzw_ctx_t *ctx) {
    if (ctx->plo) { free(ctx->plo); ctx->plo = NULL; }
    if (ctx->phi) { free(ctx->phi); ctx->phi = NULL; }
    if (ctx->sfx) { free(ctx->sfx); ctx->sfx = NULL; }
    if (ctx->stk) { free(ctx->stk); ctx->stk = NULL; }
}
