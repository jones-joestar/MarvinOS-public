#include <syscall.h>
#include "tetris.h"

static fb_info_t fb;
#define BLACK 0x00000000

static void put_pixel(int x, int y, uint32_t color) {
    if (!fb.fb) return;
    if (x < 0 || y < 0 || x >= (int)fb.width || y >= (int)fb.height) return;

    uint32_t *dst = (uint32_t *)fb.fb;
    dst[y * fb.pitch + x] = color;
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            put_pixel(x + xx, y + yy, color);
        }
    }
}

static uint32_t color_for_cell(int8_t cell) {
    if (cell < 0) return 0x00000000;

    static uint32_t colors[] = {
        0x0000FFFF, 0x00FF0000, 0x00AA00FF,
        0x00FF8800, 0x000000FF, 0x00FFFF00,
        0x0000FF00
    };

    return colors[cell % 7];
}

static void draw_text(const char *s, unsigned int x, unsigned int y) {
    unsigned int i = 0;
    while (s[i]) {
        sys_put_char((char *)&s[i], x + i * 8, y);
        i++;
    }
}

static void uint_to_str(uint32_t n, char *buf) {
    char tmp[16];
    int i = 0;
    int j = 0;

    if (n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (n > 0) {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    }

    while (i > 0) {
        buf[j++] = tmp[--i];
    }

    buf[j] = '\0';
}

static void draw_score(TetrisGame *tg, int x, int y) {
    char score_buf[16];
    char level_buf[16];

    uint_to_str(tg->score, score_buf);
    uint_to_str(tg->level, level_buf);

    draw_text("ESC FOR SETTINGS", x, y);

    draw_text("SCORE", x, y + 24);
    draw_text(score_buf, x, y + 36);

    draw_text("LEVEL", x, y + 56);
    draw_text(level_buf, x, y + 68);
}

static void draw_board(TetrisGame *tg) {
    int block_x = fb.width / TETRIS_COLS;
    int block_y = (fb.height - 40) / TETRIS_ROWS;
    int block = block_x < block_y ? block_x : block_y;

    if (block > 16) block = 16;

    int ox = (fb.width - TETRIS_COLS * block) / 2;
    int oy = 20;

    for (int r = 0; r < TETRIS_ROWS; r++) {
        for (int c = 0; c < TETRIS_COLS; c++) {
            int x = ox + c * block;
            int y = oy + r * block;

            fill_rect(x, y, block, block, 0x00303030);

            int8_t cell = tg->active_board.board[r][c];
            if (cell >= 0) {
                fill_rect(x + 1, y + 1, block - 2, block - 2, color_for_cell(cell));
            } else {
                fill_rect(x + 1, y + 1, block - 2, block - 2, BLACK);
            }
        }
    }

    int score_x = ox + TETRIS_COLS * block + 30;
    int score_y = oy;

    draw_score(tg, score_x, score_y);
}

static enum player_move read_move(void) {
    key_event_t ev;
    enum player_move move = T_NONE;

    while (sys_key_event(&ev)) {
        if (!ev.pressed) continue;

        unsigned char sc = ev.scancode;

        if (sc & 0x80) {
            switch (sc & 0x7F) {
            case 0x4B: move = T_LEFT;  break;
            case 0x4D: move = T_RIGHT; break;
            case 0x50: move = T_DOWN;  break;
            case 0x48: move = T_UP;    break;
            }
        } else {
            switch (sc) {
            case 0x10: move = T_QUIT; break; // q
            case 0x01: move = T_PLAYPAUSE; break; // ESC
            case 0x39: move = T_UP;   break; // Space = rotate
            case 0x1E: move = T_LEFT; break; // a
            case 0x20: move = T_RIGHT;break; // d
            case 0x1F: move = T_DOWN; break; // s
            case 0x11: move = T_UP;   break; // w
            }
        }
    }

    return move;
}

static bool pause_menu(void) {
    sys_key_event_flush();

    fill_rect(0, 0, fb.width, fb.height, BLACK);

    draw_text("PAUSED", 40, 40);
    draw_text("ESC         - CONTINUE", 40, 70);
    draw_text("Q           - QUIT", 40, 90);
    draw_text("left arrow  - move left", 40,110);
    draw_text("right arrow - move right", 40,130);
    draw_text("up arrow    - turn peace", 40,150);
    draw_text("down arrow  - speed up peace", 40,170);

    while (1) {
        key_event_t ev;

        while (sys_key_event(&ev)) {
            if (!ev.pressed) continue;

            unsigned char sc = ev.scancode;

            if (sc == 0x01) {
                sys_key_event_flush();
                return false;
            }

            if (sc == 0x10) {
                sys_key_event_flush();
                return true;
            }
        }

        sys_sleep_ms(30);
    }
}

int main(void) {
    sys_get_framebuffer(&fb);

    sys_kill(2);
    sys_clear();
    fill_rect(0, 0, fb.width, fb.height, BLACK);
    sys_key_event_flush();

    sys_clear();
    fill_rect(0, 0, fb.width, fb.height, BLACK);
    sys_key_event_flush();

    srand((unsigned int)sys_get_ticks_ms());

    TetrisGame *tg = create_game();
    create_rand_piece(tg);

    while (!tg->game_over) {
        enum player_move move = read_move();

        if (move == T_QUIT) {
            break;
        }

        if (move == T_PLAYPAUSE) {
            bool should_quit = pause_menu();

            if (should_quit) {
                break;
            }

            tg->last_gravity_tick_ms = sys_get_ticks_ms();
            draw_board(tg);
            continue;
        }

        tg_tick(tg, move);
        draw_board(tg);

        sys_sleep_ms(30);
    }

    sys_key_event_flush();
    end_game(tg);

    fill_rect(0, 0, fb.width, fb.height, BLACK);
    sys_clear();

    sys_execbg("/bin/meminfo");

    sys_write("\n");
    sys_write("$ MSH $:");

    sys_exit();
}