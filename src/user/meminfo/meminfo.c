#include <syscall.h>
#include <string.h>

static void draw_text(const char *s, unsigned int x, unsigned int y) {
    for (unsigned int i = 0; s[i]; i++) {
        sys_put_char((char *)&s[i], x + i * 8, y);
    }
}

static char *u64_to_str(unsigned long long val, char *buf) {
    int i = 20;
    buf[i] = '\0';
    if (val == 0) {
        buf[--i] = '0';
    } else {
        while (val > 0) {
            buf[--i] = '0' + (val % 10);
            val /= 10;
        }
    }
    return &buf[i];
}

static void draw_info_line(const char *label, unsigned long long val, unsigned int y, fb_info_t *fb) {
    char line[48];
    char num_buf[21];
    
    strcpy(line, label);
    strcat(line, u64_to_str(val, num_buf));
    strcat(line, " MB  ");

    unsigned int x = (fb->width - (unsigned int)strlen(line) * 8) / 2;
    draw_text(line, x, y);
}

int main(void) {
    fb_info_t fb;
    sys_get_framebuffer(&fb);

    unsigned long long total_mb = sys_get_total_pages() * 4096 / 1000000;
    unsigned long long temp = 0;

    while (1) {
        unsigned long long used_mb = sys_get_allocated_pages() * 4096 / 1000000;

        if (temp != used_mb) {
            temp = used_mb;
            char erase[21];
            memset(erase, '\b', 20);
            erase[20] = '\0';
            unsigned int x = (fb.width - 20 * 8) / 2;
            draw_text(erase, x, fb.height - 26);
            
        }

        draw_info_line("MEM used:  ", used_mb, fb.height - 26, &fb);
        draw_info_line("MEM total: ", total_mb, fb.height - 14, &fb);

        sys_sleep_ms(500);
    }

    return 0;
}