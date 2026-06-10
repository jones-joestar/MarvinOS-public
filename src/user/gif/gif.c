#include "gif.h"
#include "lzw.h"
#include <syscall.h>
#include <malloc.h>

typedef struct {
    uint32_t       pixel_pos;
    uint32_t       img_w, img_h;
    unsigned int   screen_x, screen_y;
    const uint8_t *ctab;
    uint8_t        trans_idx;
    uint8_t        has_trans;
    uint32_t      *fb;
    unsigned int   fb_pitch;
    unsigned int   fb_width;
    unsigned int   fb_height;
} emit_ctx_t;

static void emit_pixel(uint8_t idx, void *user) {
    emit_ctx_t *c = (emit_ctx_t *)user;
    if (c->pixel_pos >= c->img_w * c->img_h) return;
    uint32_t col = c->pixel_pos % c->img_w;
    uint32_t row = c->pixel_pos / c->img_w;
    c->pixel_pos++;
    if (c->has_trans && idx == c->trans_idx) return;
    const uint8_t *rgb   = c->ctab + (uint32_t)idx * 3;
    uint32_t       color = ((uint32_t)rgb[0] << 16) | ((uint32_t)rgb[1] << 8) | rgb[2];
    unsigned int x = c->screen_x + col;
    unsigned int y = c->screen_y + row;
    if (x < c->fb_width && y < c->fb_height)
        c->fb[y * c->fb_pitch + x] = color;
}

static void skip_subblocks(int fd) {
    uint8_t buf[256];
    uint8_t blen;
    while (sys_fread(fd, &blen, 1) == 1 && blen != 0)
        sys_fread(fd, buf, blen);
}

static void fill_rect(uint32_t *fb32, unsigned int pitch,
                      unsigned int fb_w, unsigned int fb_h,
                      unsigned int x, unsigned int y,
                      unsigned int w, unsigned int h, uint32_t color) {
    for (unsigned int row = 0; row < h; row++)
        for (unsigned int col = 0; col < w; col++) {
            unsigned int px = x + col, py = y + row;
            if (px < fb_w && py < fb_h)
                fb32[py * pitch + px] = color;
        }
}

int gif_draw(const char *path, unsigned int screen_x, unsigned int screen_y, unsigned int time) {
    fb_info_t fb;
    sys_get_framebuffer(&fb);
    uint32_t *fb32 = (uint32_t *)fb.fb;

    int fd = sys_fopen(path);
    if (fd < 0) return -1;

    uint8_t hdr[6];
    if (sys_fread(fd, hdr, 6) != 6)                         { sys_fclose(fd); return -2; }
    if (hdr[0] != 'G' || hdr[1] != 'I' || hdr[2] != 'F')  { sys_fclose(fd); return -3; }

    uint8_t lsd[7];
    if (sys_fread(fd, lsd, 7) != 7) { sys_fclose(fd); return -4; }
    uint8_t  has_gct  = (lsd[4] >> 7) & 1;
    uint8_t  gct_size = (lsd[4] & 7) + 1;
    uint8_t  bg_idx   = lsd[5];

    uint8_t *gct_buf = NULL;
    if (has_gct) {
        uint32_t entries = 1u << gct_size;
        gct_buf = malloc(entries * 3);
        if (!gct_buf) { sys_fclose(fd); return -5; }
        if (sys_fread(fd, gct_buf, entries * 3) != entries * 3) {
            free(gct_buf);
            sys_fclose(fd);
            return -6;
        }
    }

    uint32_t gct_skip = 6 + 7 + (has_gct ? (1u << gct_size) * 3 : 0);

    uint32_t bg_color = 0;
    if (gct_buf) {
        const uint8_t *rgb = gct_buf + (uint32_t)bg_idx * 3;
        bg_color = ((uint32_t)rgb[0] << 16) | ((uint32_t)rgb[1] << 8) | rgb[2];
    }

    int       ret        = 0;
    uint8_t  *lct_buf    = NULL;
    lzw_ctx_t lzw;
    int       lzw_active = 0;
    uint8_t   sub[256];

    while (time--) {
        if (lzw_active) { lzw_end(&lzw); lzw_active = 0; }

        if (sys_fseek(fd, gct_skip) != 0) { ret = -7; break; }

        uint8_t  trans_idx     = 0, has_trans  = 0;
        uint16_t delay_cs      = 10;
        uint8_t  disposal      = 0;
        uint8_t  prev_disposal = 0;
        uint16_t prev_left = 0, prev_top = 0, prev_w = 0, prev_h = 0;

        while (1) {
            uint8_t blk;
            if (sys_fread(fd, &blk, 1) != 1) break;

            if (blk == 0x3B) break;

            if (blk == 0x21) {
                uint8_t label;
                if (sys_fread(fd, &label, 1) != 1) break;

                if (label == 0xF9) {
                    uint8_t gce[6];
                    if (sys_fread(fd, gce, 6) != 6) break;
                    disposal  = (gce[1] >> 2) & 0x07;
                    delay_cs  = (uint16_t)((uint16_t)gce[2] | ((uint16_t)gce[3] << 8));
                    if (delay_cs == 0) delay_cs = 2;
                    if (gce[1] & 0x01) { has_trans = 1; trans_idx = gce[4]; }
                    else               { has_trans = 0; }
                } else {
                    skip_subblocks(fd);
                }
                continue;
            }

            if (blk == 0x2C) {
                uint8_t id[9];
                if (sys_fread(fd, id, 9) != 9) break;

                uint16_t img_left = (uint16_t)(id[0] | (id[1] << 8));
                uint16_t img_top  = (uint16_t)(id[2] | (id[3] << 8));
                uint16_t img_w    = (uint16_t)(id[4] | (id[5] << 8));
                uint16_t img_h    = (uint16_t)(id[6] | (id[7] << 8));
                uint8_t  ipacked  = id[8];
                uint8_t  has_lct  = (ipacked >> 7) & 1;
                // interlaced = (ipacked >> 6) & 1 — not yet supported
                uint8_t  lct_size = (ipacked & 7) + 1;

                const uint8_t *ctab = gct_buf;

                if (has_lct) {
                    uint32_t entries = 1u << lct_size;
                    if (!lct_buf) {
                        lct_buf = malloc(256 * 3);
                        if (!lct_buf) break;
                    }
                    if (sys_fread(fd, lct_buf, entries * 3) != entries * 3) break;
                    ctab = lct_buf;
                }

                if (!ctab) break;

                if (prev_disposal == 2)
                    fill_rect(fb32, fb.pitch, fb.width, fb.height,
                              screen_x + prev_left, screen_y + prev_top,
                              prev_w, prev_h, bg_color);

                uint8_t min_cs;
                if (sys_fread(fd, &min_cs, 1) != 1) break;

                if (lzw_begin(&lzw, min_cs) != 0) break;
                lzw_active = 1;

                emit_ctx_t ec;
                ec.pixel_pos = 0;
                ec.img_w     = img_w;
                ec.img_h     = img_h;
                ec.screen_x  = screen_x + img_left;
                ec.screen_y  = screen_y + img_top;
                ec.ctab      = ctab;
                ec.trans_idx = trans_idx;
                ec.has_trans = has_trans;
                ec.fb        = fb32;
                ec.fb_pitch  = fb.pitch;
                ec.fb_width  = fb.width;
                ec.fb_height = fb.height;

                int frame_ok = 1;
                while (1) {
                    uint8_t blen;
                    if (sys_fread(fd, &blen, 1) != 1) { frame_ok = 0; break; }
                    if (blen == 0) break;
                    if (sys_fread(fd, sub, blen) != blen) { frame_ok = 0; break; }
                    if (!lzw.done)
                        if (lzw_feed(&lzw, sub, blen, emit_pixel, &ec) < 0) { frame_ok = 0; break; }
                }

                lzw_end(&lzw);
                lzw_active = 0;

                if (!frame_ok) break;

                sys_sleep_ms((unsigned int)delay_cs * 10u);

                prev_disposal = disposal;
                prev_left     = img_left;
                prev_top      = img_top;
                prev_w        = img_w;
                prev_h        = img_h;

                disposal  = 0;
                has_trans = 0;
                delay_cs  = 10;
                continue;
            }

            break;
        }
    }

    if (lzw_active) lzw_end(&lzw);
    if (lct_buf)    free(lct_buf);
    if (gct_buf)    free(gct_buf);
    sys_fclose(fd);
    return ret;
}
