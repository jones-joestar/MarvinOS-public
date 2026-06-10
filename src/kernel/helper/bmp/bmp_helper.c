#include "bmp_helper.h"
#include "../gop/gop_helper.h"
#include "../disk/fat32.h"
#include "../memory/malloc.h"

#define BMP_SIGNATURE  0x4D42  // 'BM' little-endian
#define BI_RGB         0       // uncompressed
#define BI_BITFIELDS   3       // uncompressed with explicit color masks

typedef struct __attribute__((packed)) {
    uint16_t signature;
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t data_offset;
} bmp_file_header_t;

typedef struct __attribute__((packed)) {
    uint32_t header_size;
    int      width;
    int      height;       // negative means top-down storage
    uint16_t planes;
    uint16_t bit_count;    // 24 or 32
    uint32_t compression;
    uint32_t image_size;
    int      x_ppm;
    int      y_ppm;
    uint32_t clr_used;
    uint32_t clr_important;
} bmp_dib_header_t;

int bmp_draw(const char *path, uint32_t x, uint32_t y) {
    fat32_file_t f = fat32_open(path);
    if (!f.valid)
        return -1;

    bmp_file_header_t fhdr;
    bmp_dib_header_t  dhdr;

    if (fat32_read(&f, &fhdr, sizeof(fhdr)) != sizeof(fhdr))
        return -1;
    if (fhdr.signature != BMP_SIGNATURE)
        return -1;

    if (fat32_read(&f, &dhdr, sizeof(dhdr)) != sizeof(dhdr))
        return -1;
    if (dhdr.compression != BI_RGB && dhdr.compression != BI_BITFIELDS)
        return -1;
    if (dhdr.bit_count != 24 && dhdr.bit_count != 32)
        return -1;

    int      img_w    = dhdr.width;
    int      img_h    = dhdr.height;
    int      top_down = (img_h < 0);
    if (top_down) img_h = -img_h;

    uint32_t bpp       = dhdr.bit_count / 8;
    uint32_t row_bytes = ((uint32_t)img_w * bpp + 3u) & ~3u;

    uint8_t *row_buf = (uint8_t *)malloc(row_bytes);
    if (!row_buf) {
        return -1;
    }

    // Skip optional color table between headers and pixel data
    uint32_t pos = sizeof(fhdr) + sizeof(dhdr);
    while (pos < fhdr.data_offset) {
        uint8_t tmp;
        fat32_read(&f, &tmp, 1);
        pos++;
    }

    for (int row = 0; row < img_h; row++) {
        if (fat32_read(&f, row_buf, row_bytes) == 0)
            break;

        // BMP rows are stored bottom-up unless top_down flag is set
        uint32_t screen_y = top_down
            ? (y + (uint32_t)row)
            : (y + (uint32_t)(img_h - 1 - row));

        for (int col = 0; col < img_w; col++) {
            uint8_t  b     = row_buf[col * bpp + 0];
            uint8_t  g     = row_buf[col * bpp + 1];
            uint8_t  r     = row_buf[col * bpp + 2];
            uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            draw_pixel(x + (uint32_t)col, screen_y, color);
        }
    }

    free(row_buf);
    return 0;
}
