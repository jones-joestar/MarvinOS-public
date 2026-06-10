#include <syscall.h>
#include <stdlib.h>
#include "../gif/gif.h"


// RTTTL player

// Semitone layout per octave
static const unsigned int rtttl_freq[8][12] = {
     {  16, 17, 18, 19, 21, 22, 23, 24, 26, 27, 29, 31},
     {  33, 35, 37, 39, 41, 44, 46, 49, 52, 55, 58, 62},
     {  65, 69, 73, 78, 82, 87, 92, 98,104,110,117,123},
     { 131,139,147,156,165,175,185,196,208,220,233,247},
     { 262,277,294,311,330,349,370,392,415,440,466,494},
     { 523,554,587,622,659,698,740,784,831,880,932,988},
     {1047,1109,1175,1245,1319,1397,1480,1568,1661,1760,1865,1976},
     {2093,2217,2349,2489,2637,2794,2960,3136,3322,3520,3729,3951},
};

static int rtttl_note_semitone(char c) {
    switch (c) {
        case 'c': case 'C': return 0;
        case 'd': case 'D': return 2;
        case 'e': case 'E': return 4;
        case 'f': case 'F': return 5;
        case 'g': case 'G': return 7;
        case 'a': case 'A': return 9;
        case 'b': case 'B': return 11;
        default: return -1;
    }
}

static int rtttl_parse_int(const char **p) {
    int n = 0;
    while (**p >= '0' && **p <= '9') n = n * 10 + (*(*p)++ - '0');
    return n;
}

static void play_rtttl(const char *buf) {
    const char *p = buf;

    // skip name
    while (*p && *p != ':') p++;
    if (*p == ':') p++;

    // parse header defaults
    int def_dur = 4, def_oct = 6, bpm = 63;
    while (*p && *p != ':') {
        while (*p == ' ' || *p == ',') p++;
        char key = *p;
        if ((key == 'd' || key == 'o' || key == 'b') && *(p+1) == '=') {
            p += 2;
            int val = rtttl_parse_int(&p);
            if (key == 'd') def_dur = val;
            else if (key == 'o') def_oct = val;
            else                 bpm     = val;
        } else {
            p++;
        }
    }
    if (*p == ':') p++;

    // whole-note duration in ms
    unsigned int whole_ms = 240000 / (unsigned int)bpm;

    // parse and play notes
    while (*p && *p != '\r' && *p != '\n') {
        while (*p == ' ') p++;
        if (!*p || *p == '\r' || *p == '\n') break;

        // optional duration
        int dur = def_dur;
        if (*p >= '1' && *p <= '9') dur = rtttl_parse_int(&p);

        // note letter
        char nc = *p++;
        if (!nc) break;

        // optional sharp
        int semitone = rtttl_note_semitone(nc);
        int sharp = (*p == '#') ? (p++, 1) : 0;

        // optional dot before octave
        int dot = (*p == '.') ? (p++, 1) : 0;

        // optional octave
        int oct = def_oct;
        if (*p >= '4' && *p <= '7') oct = *p++ - '0';

        // optional dot after octave
        if (*p == '.') { dot = 1; p++; }

        // skip comma
        if (*p == ',') p++;

        unsigned int dur_ms = whole_ms / (unsigned int)dur;
        if (dot) dur_ms += dur_ms / 2;

        if (nc == 'p' || nc == 'P') {
            sys_sleep_ms(dur_ms);
        } else if (semitone >= 0) {
            int st = semitone + sharp;
            if (st > 11) { st -= 12; oct++; }
            if (oct < 0) oct = 0;
            if (oct > 7) oct = 7;
            sys_beep(rtttl_freq[oct][st], dur_ms);
            sys_sleep_ms(30);
        }
    }
}

// end RTTTL player

#define TREE_MAX_ENTRIES 32
#define TREE_MAX_DEPTH    5

#define LINEBUF_SIZE 256

#define HISTORY_SIZE 10

#define KEY_UP     0x80
#define KEY_DOWN   0x81
#define KEY_LEFT   0x82
#define KEY_RIGHT  0x83

static char history[HISTORY_SIZE][LINEBUF_SIZE];
static int history_count = 0;
static int history_next = 0;

static int slen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int scmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}


static void scpy(char *dst, const char *src) {
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
}

static void clear_current_input(int len) {
    for (int j = 0; j < len; j++) {
        sys_write("\b \b");
    }
}

static const char *history_get(int offset_from_newest) {
    int index = history_next - 1 - offset_from_newest;

    while (index < 0) {
        index += HISTORY_SIZE;
    }

    index %= HISTORY_SIZE;
    return history[index];
}

// null terminate command word in place
static char *split_cmd(char *line) {
    char *p = line;
    while (*p && *p != ' ') p++;
    if (*p == ' ') { *p = '\0'; return p + 1; }
    return p;
}

// null terminate first token in place
static char *split_arg(char *args) {
    char *p = args;
    while (*p && *p != ' ') p++;
    if (*p == ' ') { *p = '\0'; return p + 1; }
    return p;
}

static int satoi(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return n;
}

static void scat(char *dst, const char *src) {
    int n = 0;
    while (dst[n]) n++;
    int i = 0;
    while (src[i]) dst[n++] = src[i++];
    dst[n] = '\0';
}

static void print_tree(const char *path, const char *prefix, int depth) {
    if (depth > TREE_MAX_DEPTH) return;
    fat_dirent_t entries[TREE_MAX_ENTRIES];
    int count = sys_readdir(path, entries, TREE_MAX_ENTRIES);
    if (count < 0) return;
    for (int i = 0; i < count; i++) {
        if (scmp(entries[i].name, "EFI")  == 0 ||
            scmp(entries[i].name, "efi")  == 0 ||
            scmp(entries[i].name, "BOOT") == 0 ||
            scmp(entries[i].name, "boot") == 0 ||
            scmp(entries[i].name, "GRUB") == 0 ||
            scmp(entries[i].name, "grub") == 0) continue;
        int is_last = (i == count - 1);
        sys_write(prefix);
        sys_write(is_last ? "\\-- " : "|-- ");
        sys_write(entries[i].name);
        sys_write("\n");
        if (entries[i].is_dir) {
            char child_path[256];
            scpy(child_path, path);
            int plen = slen(child_path);
            if (plen == 0 || child_path[plen - 1] != '/') {
                child_path[plen]     = '/';
                child_path[plen + 1] = '\0';
            }
            scat(child_path, entries[i].name);
            char child_prefix[64];
            scpy(child_prefix, prefix);
            scat(child_prefix, is_last ? "    " : "|   ");
            print_tree(child_path, child_prefix, depth + 1);
        }
    }
}

static void write_char(char c) {
    char buf[2] = {c, '\0'};
    sys_write(buf);
}

static void getline(char *buf, int max) {
    int i = 0;
    int history_pos = -1;

    while (i < max - 1) {
        unsigned char c = (unsigned char)sys_read_char();
        if (c == 0) continue;
        if (c == '\r' || c == '\n') { sys_write("\n"); break; }
        if (c == KEY_UP) {
            if (history_count == 0) continue;
            if (history_pos < history_count - 1) {history_pos++;}
            clear_current_input(i);
            const char *h = history_get(history_pos);
            scpy(buf, h);
            i = slen(buf);
            sys_write(buf);
            continue;
        }

        if (c == KEY_DOWN) {
            if (history_count == 0) continue;
            clear_current_input(i);
            if (history_pos > 0) {
                history_pos--;
                const char *h = history_get(history_pos);
                scpy(buf, h);
                i = slen(buf);
                sys_write(buf);
            } else {
                history_pos = -1;
                buf[0] = '\0';
                i = 0;
            }
            continue;
        }

        if (c == '\b' || c == 127) {
            if (i > 0) {
                history_pos = -1;
                i--;
                buf[i] = '\0';
                sys_write("\b \b");
            }
            continue;
        }

        if (c >= 32 && c <= 126) {
            history_pos = -1;
            buf[i++] = (char)c;
            buf[i] = '\0';
            write_char((char)c);
        }
    }
    buf[i] = '\0';
}

// LCG random
static unsigned long rand_state = 1;

int rand(void) {
    rand_state = rand_state * 6364136223846793005UL + 1442695040888963407UL;
    return (int)((rand_state >> 33) & RAND_MAX);
}

void srand(unsigned int seed) {
    rand_state = seed;
}

static unsigned int random_color() {
    srand(sys_get_ticks_ms());
    unsigned int r = rand() % 256;
    unsigned int g = rand() % 256;
    unsigned int b = rand() % 256;

    return (r<<16) | (g<<8) | b;
}

static void add_history(const char *cmd) {
    if (cmd[0] == '\0') return;

    if (history_count > 0) {
        int last = (history_next + HISTORY_SIZE - 1) % HISTORY_SIZE;
        if (scmp(history[last], cmd) == 0) return;
    }

    scpy(history[history_next], cmd);
    history_next = (history_next + 1) % HISTORY_SIZE;

    if (history_count < HISTORY_SIZE) {
        history_count++;
    }
}

static void help_list(void) {
    sys_write("Available commands:\n");
    sys_write("\n");
    sys_write("help                 -> lists all the available comands\n");
    sys_write("clear                -> clears the screen\n");
    sys_write("echo <text>          -> prints text to the console\n");
    sys_write("run <path>           -> executes a program\n");
    sys_write("                        <bin/sh>         starts new shell\n");
    sys_write("                        <bin/test>       starts test-Userprogramm\n");
    sys_write("doom                 -> starts the game doom\n");
    sys_write("tetris               -> starts the game tetris\n");
    sys_write("spawn10              -> spawns 10 background instances of count\n");
    sys_write("gif <path> <times>   -> plays a GIF at the top-left corner\n");
    sys_write("beep [freq] [ms]     -> play a tone (default: 440Hz for 300ms)\n");
    sys_write("sound <path>         -> play an RTTTL ringtone file\n");
    sys_write("tree [path]          -> prints directory tree (default: /)\n");
    sys_write("shutdown             -> powers of the machine\n");
    sys_write("color <color>        -> changes shell text color\n");
    sys_write("                        <green|white|red|blue|yellow|pink|random>\n");
}

int main(void) {
    sys_write("MarvinShell\n");
    sys_write("Type 'help' to see commands.\n\n");
    sys_execbg("/bin/meminfo");

    char line[LINEBUF_SIZE];
    while (1) {
        sys_write("$ MSH $:");
        getline(line, LINEBUF_SIZE);

        if (line[0] == '\0') continue;

        add_history(line);

        char *args = split_cmd(line);

        if  (scmp(line , "help") == 0) {
            help_list();
        } else if (scmp(line, "clear") == 0) {
            sys_clear();
        } else if (scmp(line, "echo") == 0) {
            sys_write(args);
            sys_write("\n");
        } else if (scmp(line, "run") == 0) {
            if (*args == '\0') {
                sys_write("usage: run <path>\n");
            } else {
                sys_exec(args);
            }
        } else if (scmp(line, "doom") == 0) {
            sys_exec("/bin/doom");
            sys_clear();
            sys_key_event_flush();
        } else if (scmp(line, "tetris") == 0) {
            sys_exec("/bin/tetris");
        } else if (scmp(line, "test") == 0) {
            sys_exec("/bin/test");
        } else if (scmp(line, "spawn10") == 0) {
            sys_exec("/bin/spawn10");
        } else if (scmp(line, "gif") == 0) {
            if (*args == '\0') {
                sys_write("usage: gif <path> <times>\n");
            } else {
                char *time_str = split_arg(args);
                int times = (*time_str != '\0') ? satoi(time_str) : 1;
                if (times <= 0) times = 1;
                gif_draw(args, 0, 0, (unsigned int)times);
                sys_clear();
            }
        } else if (scmp(line, "tree") == 0) {
            const char *path = (*args != '\0') ? args : "/";
            sys_write(path);
            sys_write("\n");
            print_tree(path, "", 0);
        } else if (scmp(line, "sound") == 0) {
            if (*args == '\0') {
                sys_write("usage: sound <path>\n");
            } else {
                int fd = sys_fopen(args);
                if (fd < 0) {
                    sys_write("sound: file not found\n");
                } else {
                    static char rtttl_buf[2048];
                    unsigned int n = sys_fread(fd, rtttl_buf, sizeof(rtttl_buf) - 1);
                    rtttl_buf[n] = '\0';
                    play_rtttl(rtttl_buf);
                }
            }
        } else if (scmp(line, "beep") == 0) {
            unsigned int freq = 440;
            unsigned int ms   = 300;
            if (*args != '\0') {
                char *ms_str = split_arg(args);
                freq = (unsigned int)satoi(args);
                if (*ms_str != '\0') ms = (unsigned int)satoi(ms_str);
            }
            sys_beep(freq, ms);
        } else if (scmp(line, "shutdown") == 0) {
            sys_write("shutting down...\n");
            gif_draw("/gif/delete.gif", 100, 100, 1);
            sys_shutdown();
        }else if (scmp(line, "color") == 0) {
            if (scmp(args, "green") == 0) {
                sys_set_color(0x0000FF00, 0x00000000);
            } else if (scmp(args, "white") == 0) {
                sys_set_color(0x00FFFFFF, 0x00000000);
            } else if (scmp(args, "red") == 0) {
                sys_set_color(0x00FF0000, 0x00000000);
            } else if (scmp(args, "blue") == 0) {
                sys_set_color(0x000000FF, 0x00000000);
            } else if (scmp(args, "yellow") == 0) {
                sys_set_color(0x00FFFF00, 0x00000000);
            } else if (scmp(args, "pink") == 0) {
                sys_set_color(0xFFC0CB, 0x00000000);
            } else if (scmp(args, "random") == 0) {
                unsigned int fg = random_color();
                sys_set_color(fg, 0x00000000);
            }else{
                sys_write("usage: color <red|green|blue|yellow|pink|random>\n");
            }
        } else if (scmp(line, "kill") == 0) {
            if (*args == '\0') {
                sys_write("usage: kill <pid>\n");
            } else {
                int pid = satoi(args);
                sys_kill(pid);
            } 
        } else if (scmp(line, "runbg") == 0) {
            if (*args == '\0') {
                sys_write("usage: runbg <path>\n");
            } else {
                sys_execbg(args);
            }
        } else {
            sys_write("unknown command: ");
            sys_write(line);
            sys_write("\n");
        }
    }

    return 0;
}
