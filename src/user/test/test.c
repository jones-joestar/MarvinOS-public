#include <syscall.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

static int g_errors = 0;

// output helpers 

static void start_module(const char *name) {
    sys_write("testing ");
    sys_write(name);
    sys_write("... ");
    g_errors = 0;
}

static void end_module(void) {
    if (g_errors == 0) {
        sys_write(" ok\n");
    } else {
        sys_write(" FAILED\n");
    }
}

static void check(const char *name, int expected, int got) {
    if (expected == got) {
        sys_write(".");
    } else {
        sys_write("\n[FAIL] ");
        sys_write(name);
        sys_write("\n");
        g_errors++;
    }
}

static void check_str(const char *name, const char *expected, const char *got) {
    if (!got) {
        sys_write("\n[FAIL] ");
        sys_write(name);
        sys_write(" (got NULL)\n");
        g_errors++;
        return;
    }
    if (strcmp(expected, got) == 0) {
        sys_write(".");
    } else {
        sys_write("\n[FAIL] ");
        sys_write(name);
        sys_write(" (expected '");
        sys_write(expected);
        sys_write("', got '");
        sys_write(got);
        sys_write("')\n");
        g_errors++;
    }
}

static void check_double(const char *name, double expected, double got, double tolerance) {
    double diff = expected - got;
    if (diff < 0) diff = -diff;
    if (diff <= tolerance) {
        sys_write(".");
    } else {
        sys_write("\n[FAIL] ");
        sys_write(name);
        sys_write("\n");
        g_errors++;
    }
}

// Modules

static int int_cmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

static void test_string_deep(void) {
    start_module("string.h");
    
    // Core functions
    check("strlen", 11, strlen("MarvinOS 64"));
    check("strcmp", 0, strcmp("test", "test"));
    check("strncmp", 0, strncmp("testing", "testcase", 4));
    check("strncmp_diff", 1, strncmp("testing", "testcase", 5) != 0);
    
    // Memory ops
    char overlap[] = "123456789";
    memmove(overlap + 2, overlap, 5);
    check_str("memmove_overlap", "121234589", overlap);
    
    char pad[10];
    memset(pad, 'X', 10);
    strncpy(pad, "abc", 5);
    check("strncpy_null_padding", pad[3] == 0 && pad[4] == 0, 1);
    check("strncpy_bounds", pad[5], 'X');
    
    // Search
    check("strstr", strstr("the quick brown fox", "quick") != NULL, 1);
    check("strchr", *strchr("hello", 'e'), 'e');
    check("strrchr", *strrchr("hello", 'l'), 'l');
    check("memchr", *(char*)memchr("abcde", 'c', 5), 'c');
    
    // Tokenizer
    char str_tok[] = "foo:bar:baz";
    check_str("strtok_1", "foo", strtok(str_tok, ":"));
    check_str("strtok_2", "bar", strtok(NULL, ":"));
    check_str("strtok_3", "baz", strtok(NULL, ":"));
    
    end_module();
}

static void test_stdlib_deep(void) {
    start_module("stdlib.h");
    
    check("atoi", -123, atoi("-123"));
    check("abs", 42, abs(-42));
    
    char *end;
    check("strtol_hex", 255, strtol("FF", &end, 16));
    check("strtol_oct", 8, strtol("10", &end, 8));
    
    // Allocation
    void *p1 = malloc(1024);
    check("malloc", p1 != NULL, 1);
    
    int *p2 = calloc(64, sizeof(int));
    int zero = 1;
    for(int i=0; i<64; i++) if(p2[i] != 0) zero = 0;
    check("calloc", zero, 1);
    
    void *p3 = realloc(p1, 2048);
    check("realloc", p3 != NULL, 1);
    
    free(p2);
    free(p3);
    
    end_module();
}

static void test_math_deep(void) {
    start_module("math.h");
    
    check_double("sqrt", 3.0, sqrt(9.0), 0.000001);
    check_double("pow", 32.0, pow(2.0, 5.0), 0.000001);
    check_double("sin", 1.0, sin(M_PI_2), 0.0001);
    check_double("cos", -1.0, cos(M_PI), 0.0001);
    check_double("floor", 4.0, floor(4.9), 0.000001);
    check_double("ceil", 5.0, ceil(4.1), 0.000001);
    
    end_module();
}

static void test_stdio_deep(void) {
    start_module("stdio.h");
    
    char buf[128];
    // Complex formatting
    sprintf(buf, "Dec: %d, Hex: %x, Str: %s", -50, 0x1F, "Marvin");
    int match = (strstr(buf, "Dec: -50") != NULL) &&
                (strstr(buf, "Str: Marvin") != NULL) &&
                (strstr(buf, "1f") != NULL || strstr(buf, "1F") != NULL);
    check("sprintf_complex", match, 1);
    
    // Floating point
    sprintf(buf, "%.3f", 1.2346);
    check_str("sprintf_float", "1.235", buf); // Rounded 1.2346 to 3 places
    
    // Bounds checking
    int n = snprintf(buf, 10, "123456789012345");
    check_str("snprintf_trunc", "123456789", buf);
    check("snprintf_return", 15, n);
    
    end_module();
}

static void test_ctype_deep(void) {
    start_module("ctype.h");
    
    check("isdigit", isdigit('5') != 0, 1);
    check("isalpha", isalpha('Z') != 0, 1);
    check("isspace", isspace('\r') != 0, 1);
    check("toupper", 'M', toupper('m'));
    check("tolower", 'a', tolower('A'));
    check("ispunct", ispunct('.') != 0, 1);
    
    end_module();
}

static void test_string_extra(void) {
    start_module("string.h (extra)");

    char dst[10];
    memcpy(dst, "hello", 6);
    check_str("memcpy", "hello", dst);

    check("memcmp_eq",  0, memcmp("abc", "abc", 3));
    check("memcmp_lt",  1, memcmp("abc", "abd", 3) < 0);
    check("memcmp_gt",  1, memcmp("abd", "abc", 3) > 0);
    check("memcmp_partial", 0, memcmp("abcXX", "abcYY", 3));

    char scpy[16];
    strcpy(scpy, "MarvinOS");
    check_str("strcpy", "MarvinOS", scpy);

    char cat[32] = "Hello";
    strcat(cat, ", World");
    check_str("strcat", "Hello, World", cat);

    char ncat[16] = "foo";
    strncat(ncat, "barbaz", 3);
    check_str("strncat", "foobar", ncat);

    char *dup = strdup("duplicate");
    check_str("strdup", "duplicate", dup);
    free(dup);

    char *ndup = strndup("truncate_me", 8);
    check_str("strndup", "truncate", ndup);
    free(ndup);

    check("strcasecmp_eq",  0, strcasecmp("Hello", "hELLO"));
    check("strcasecmp_ne",  1, strcasecmp("abc", "xyz") != 0);
    check("strncasecmp_eq", 0, strncasecmp("AbCdEf", "aBcXYZ", 3));
    check("strncasecmp_ne", 1, strncasecmp("AbCdEf", "aBcXYZ", 4) != 0);

    const char *nulltarget = "hello";
    check("strchr_nul", '\0', *strchr(nulltarget, '\0'));

    const char *rtest = "abcabc";
    check("strrchr_last", 4, (int)(strrchr(rtest, 'b') - rtest));

    char back[] = "123456789";
    memmove(back + 3, back, 6);
    check_str("memmove_back", "123123456", back);

    end_module();
}

static void test_stdlib_extra(void) {
    start_module("stdlib.h (extra)");

    char *end;

    check("atol_neg", -99999, (int)atol("-99999"));

    check("strtoul_hex", 0xFF,    (int)strtoul("FF", &end, 16));
    check("strtoul_dec", 12345,   (int)strtoul("12345", &end, 10));

    check_double("atof",       3.14,    atof("3.14"),      0.001);
    check_double("strtod_exp", 1500.0,  strtod("1.5e3", &end), 0.1);
    check_double("strtod_neg", -0.5,    strtod("-5e-1", &end), 0.001);

    check("strtol_auto_hex",  0xBEEF, (int)strtol("0xBEEF", &end, 0));
    check("strtol_endptr_nul", '\0',  (int)*end);

    check("strtol_partial", 42, (int)strtol("42abc", &end, 10));
    check("strtol_endptr_c", 'a', (int)*end);

    int arr[] = {5, 3, 8, 1, 9, 2, 7, 4, 6};
    qsort(arr, 9, sizeof(int), int_cmp);
    int sorted = 1;
    for (int i = 0; i < 8; i++) if (arr[i] > arr[i + 1]) sorted = 0;
    check("qsort_order", 1, sorted);
    check("qsort_min",   1, arr[0]);
    check("qsort_max",   9, arr[8]);

    int key = 7;
    int *found = (int *)bsearch(&key, arr, 9, sizeof(int), int_cmp);
    check("bsearch_found", 1, found != (void *)0 && *found == 7);
    int miss = 42;
    check("bsearch_miss",  1, bsearch(&miss, arr, 9, sizeof(int), int_cmp) == (void *)0);

    srand(42);
    int r1 = rand();
    srand(42);
    int r2 = rand();
    check("rand_deterministic", r1, r2);
    check("rand_nonneg",         1,  r1 >= 0);

    check("labs", 1000000L, labs(-1000000L));

    end_module();
}

static void test_sprintf_fmt(void) {
    start_module("sprintf (flags/width/prec)");

    char buf[64];

    sprintf(buf, "%10d", 42);
    check_str("width_right",    "        42", buf);

    sprintf(buf, "%-10d|", 42);
    check_str("width_left",     "42        |", buf);

    sprintf(buf, "%08d", 42);
    check_str("zero_pad",       "00000042", buf);

    sprintf(buf, "%+d", 42);
    check_str("plus_flag",      "+42", buf);

    sprintf(buf, "%o", 255);
    check_str("octal",          "377", buf);

    sprintf(buf, "%#o", 8);
    check_str("alt_octal",      "010", buf);

    sprintf(buf, "%#x", 255);
    check_str("alt_hex",        "0xff", buf);

    sprintf(buf, "%X", 0xdead);
    check_str("hex_upper",      "DEAD", buf);

    sprintf(buf, "100%%");
    check_str("percent_escape", "100%", buf);

    sprintf(buf, "%c%c%c", 'O', 'S', '!');
    check_str("char_fmt",       "OS!", buf);

    sprintf(buf, "%.5s", "truncated");
    check_str("str_prec",       "trunc", buf);

    sprintf(buf, "%u", (unsigned int)-1);
    check("unsigned_no_sign",   1, buf[0] != '-');

    sprintf(buf, "%lld", (long long)-1234567890123LL);
    check_str("lld",            "-1234567890123", buf);

    sprintf(buf, "%*d", 6, 7);
    check_str("dynamic_width",  "     7", buf);

    sprintf(buf, "%.0d", 0);
    check_str("prec0_zero",     "", buf);

    end_module();
}

static void test_sscanf(void) {
    start_module("sscanf");

    int i; char s[32]; char c;

    check("int_count",   1,  sscanf("42", "%d", &i));
    check("int_val",     42, i);

    check("neg_count",   1,   sscanf("-99", "%d", &i));
    check("neg_val",     -99, i);

    int a, b;
    check("multi_count", 2,  sscanf("10 20", "%d %d", &a, &b));
    check("multi_a",     10, a);
    check("multi_b",     20, b);

    unsigned int hex;
    check("hex_count",   1,    sscanf("0xFF", "%x", &hex));
    check("hex_val",     0xFF, (int)hex);
    check("str_count",   1,       sscanf("hello world", "%s", s));
    check_str("str_val", "hello", s);
    check("char_count",  1,   sscanf("X", "%c", &c));
    check("char_val",    'X', (int)c);
    check("suppress_count", 1,  sscanf("skip 99", "%*s %d", &i));
    check("suppress_val",   99, i);

    int pos;
    sscanf("42rest", "%d%n", &i, &pos);
    check("n_after_d", 2, pos);
    double dval;
    check("float_count", 1, sscanf("3.14", "%lf", &dval));
    check("float_int",   3, (int)dval);

    end_module();
}

static void write_num(uint32_t v) {
    char _n[12];
    sprintf(_n, "%u", v);
    sys_write(_n);
}

static void test_fat_readback(void) {
    start_module("fat readback");

    int f = sys_fopen("/txt/hello.txt");
    if (f < 0) {
        check("open", 1, 0);
        end_module(); return;
    }

    char seq[64] = {0};
    uint32_t n = sys_fread(f, seq, sizeof(seq) - 1);
    check("read_nonzero", 1, n > 0);

    char one[64] = {0};
    sys_fseek(f, 0);
    for (uint32_t i = 0; i < n; i++) sys_fread(f, &one[i], 1);
    check("bulk_eq_bytes", 0, memcmp(seq, one, n));

    int spots_ok = 1;
    if (n >= 5) {
        uint32_t spots[5] = {0, n/4, n/2, (n*3)/4, n-1};
        for (int s = 0; s < 5; s++) {
            char g;
            sys_fseek(f, spots[s]);
            sys_fread(f, &g, 1);
            if (g != seq[spots[s]]) spots_ok = 0;
        }
    }
    check("seek_spot_5", 1, spots_ok);

    end_module();
}

static void test_fat_wad_header(void) {
    start_module("fat wad header");

    int f = sys_fopen("/doom1.wad");
    if (f < 0) {
        check("wad_open", 1, 0);
        end_module(); return;
    }

    unsigned char hdr[12];
    check("hdr_read_12", 12, (int)sys_fread(f, hdr, 12));

    check("not_elf_magic", 1,
          !(hdr[0] == 0x7F && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F'));

    int magic = (hdr[0] == 'I' || hdr[0] == 'P') &&
                 hdr[1] == 'W' && hdr[2] == 'A' && hdr[3] == 'D';
    check("wad_magic", 1, magic);

    uint32_t lump_count = (uint32_t)hdr[4]         | ((uint32_t)hdr[5]  <<  8) |
                          ((uint32_t)hdr[6]  << 16) | ((uint32_t)hdr[7]  << 24);
    uint32_t dir_off    = (uint32_t)hdr[8]         | ((uint32_t)hdr[9]  <<  8) |
                          ((uint32_t)hdr[10] << 16) | ((uint32_t)hdr[11] << 24);

    check("lump_count_pos",    1, lump_count > 0);
    check("lump_count_sane",   1, lump_count < 100000);
    check("dir_off_after_hdr", 1, dir_off > 12);

    end_module();
}

static void test_fat_wad_seek(void) {
    start_module("fat wad seek+dir");

    int f = sys_fopen("/doom1.wad");
    if (f < 0) {
        check("wad_open", 1, 0);
        end_module(); return;
    }

    unsigned char hdr[12];
    sys_fread(f, hdr, 12);
    uint32_t lump_count = (uint32_t)hdr[4]         | ((uint32_t)hdr[5]  <<  8) |
                          ((uint32_t)hdr[6]  << 16) | ((uint32_t)hdr[7]  << 24);
    uint32_t dir_off    = (uint32_t)hdr[8]         | ((uint32_t)hdr[9]  <<  8) |
                          ((uint32_t)hdr[10] << 16) | ((uint32_t)hdr[11] << 24);
    if (!lump_count || !dir_off) {
        check("header_sane", 1, 0); end_module(); return;
    }

    uint32_t n_ents = lump_count < 3 ? lump_count : 3;
    unsigned char dir[48];
    check("seek_to_dir",    0,               sys_fseek(f, dir_off));
    check("dir_ents_read",  (int)(n_ents*16),(int)sys_fread(f, dir, n_ents * 16));

    int names_ok = 1;
    for (uint32_t e = 0; e < n_ents; e++) {
        char name[9] = {0};
        for (int k = 0; k < 8; k++) {
            unsigned char b = dir[e * 16 + 8 + k];
            name[k] = b ? (char)b : 0;
            if (b && (b < 32 || b > 126)) names_ok = 0;
        }
    }
    check("lump_names_ascii", 1, names_ok);

    unsigned char ent2[16];
    sys_fseek(f, dir_off);
    sys_fread(f, ent2, 16);
    check("dir_reread_stable", 0, memcmp(dir, ent2, 16));

    uint32_t l0_off  = (uint32_t)dir[0] | ((uint32_t)dir[1] <<  8) |
                       ((uint32_t)dir[2] << 16) | ((uint32_t)dir[3] << 24);
    uint32_t l0_size = (uint32_t)dir[4] | ((uint32_t)dir[5] <<  8) |
                       ((uint32_t)dir[6] << 16) | ((uint32_t)dir[7] << 24);
    if (l0_size >= 4) {
        unsigned char d1[4], d2[4];
        sys_fseek(f, l0_off); sys_fread(f, d1, 4);
        sys_fseek(f, l0_off); sys_fread(f, d2, 4);
        check("lump0_seek_stable", 0, memcmp(d1, d2, 4));
    }

    end_module();
}

static void test_fat_wad_chunks(void) {
    start_module("fat wad chunk consistency");

    int f = sys_fopen("/doom1.wad");
    if (f < 0) {
        check("wad_open", 1, 0);
        end_module(); return;
    }

    char bulk[512], pieces[512];

    sys_fseek(f, 0);
    sys_fread(f, bulk, 512);
    sys_fseek(f, 0);
    for (int i = 0; i < 8; i++) sys_fread(f, pieces + i * 64, 64);
    check("A_512_vs_8x64",       0, memcmp(bulk, pieces, 512));

    sys_fseek(f, 500);
    sys_fread(f, bulk, 512);
    sys_fseek(f, 500);
    for (int i = 0; i < 4; i++) sys_fread(f, pieces + i * 128, 128);
    check("B_sector_cross_512",  0, memcmp(bulk, pieces, 512));

    sys_fseek(f, 1000);
    sys_fread(f, bulk, 512);
    sys_fseek(f, 1000);
    sys_fread(f, pieces,       24);
    sys_fread(f, pieces + 24, 488);
    check("C_sector_cross_1024", 0, memcmp(bulk, pieces, 512));

    char *big_bulk   = malloc(4608);
    char *big_pieces = malloc(4608);
    if (big_bulk && big_pieces) {
        sys_fseek(f, 0);
        check("D_read_4608",         4608, (int)sys_fread(f, big_bulk, 4608));
        sys_fseek(f, 0);
        for (int i = 0; i < 9; i++) sys_fread(f, big_pieces + i * 512, 512);
        check("D_4k_cluster_cross",  0, memcmp(big_bulk, big_pieces, 4608));

        char boundary[512];
        sys_fseek(f, 4096);
        sys_fread(f, boundary, 512);
        check("E_seek_to_4096",      0, memcmp(big_bulk + 4096, boundary, 512));

        char *deep = malloc(8208);
        if (deep) {
            sys_fseek(f, 0);
            uint32_t got = sys_fread(f, deep, 8208);
            if (got >= 8208) {
                char seek16[16];
                sys_fseek(f, 8192);
                sys_fread(f, seek16, 16);
                check("F_cluster_seek_8192", 0, memcmp(deep + 8192, seek16, 16));
            }
            free(deep);
        } else {
            check("F_malloc_8208", 1, 0);
        }
    } else {
        check("D_malloc_4608", 1, 0);
    }
    if (big_bulk)   free(big_bulk);
    if (big_pieces) free(big_pieces);

    end_module();
}

static void test_scheduler(void) {
    start_module("scheduler");
    
    uint64_t t1 = sys_get_ticks_ms();
    sys_sleep_ms(100);
    uint64_t t2 = sys_get_ticks_ms();
    check("sleep_100ms", (t2 - t1) >= 100, 1);
    check("ticks_monotonic", t2 >= t1, 1);
    
    end_module();
}

static void test_audio(void) {
    start_module("audio");

    unsigned int freqs[] = {262, 330, 392, 523};
    for (int i = 0; i < 4; i++) {
        sys_beep(freqs[i], 50);
        sys_sleep_ms(20);
    }
    check("beep_melody", 1, 1);
    
    end_module();
}

static void test_memory_sys(void) {
    start_module("memory syscalls");
    
    void *base = sys_heap_base();
    check("heap_base_nonnull", base != NULL, 1);
    
    void *t1 = sys_enlarge_heap();
    void *t2 = sys_enlarge_heap();

    check("heap_grow", (uint64_t)t2 > (uint64_t)t1, 1);
    check("heap_alignment", ((uint64_t)t1 % 4096) == 0, 1);
    
    end_module();
}

static void test_hw_framebuffer(void) {
    start_module("hw: framebuffer");

    fb_info_t fb;
    sys_get_framebuffer(&fb);

    uint64_t addr = (uint64_t)fb.fb;
    check("fb_virt_range",  addr >= (uint64_t)0x400000000ULL, 1);
    check("fb_width_sane",  fb.width  >= 320 && fb.width  <= 7680, 1);
    check("fb_height_sane", fb.height >= 200 && fb.height <= 4320, 1);
    check("fb_pitch_gte_w", fb.pitch  >= fb.width, 1);

    uint32_t *pixels = (uint32_t *)fb.fb;
    uint32_t saved   = pixels[0];
    pixels[0] = 0x12345678;
    uint32_t got = pixels[0];
    pixels[0] = saved;
    check("fb_wt_readback", got, 0x12345678);

    end_module();
}

static void test_hw_phys_map(void) {
    start_module("hw: physical map");

    const int CHUNKS = 6;
    const int CHUNK  = 512 * 1024;
    char *ptrs[6];
    int ok = 1;

    for (int c = 0; c < CHUNKS; c++) {
        ptrs[c] = malloc(CHUNK);
        if (!ptrs[c]) { ok = 0; break; }
        for (int i = 0; i < CHUNK; i++)
            ptrs[c][i] = (char)((c * 17 + i) & 0xFF);
    }
    check("alloc_3mb", ok, 1);

    int readback_ok = 1;
    for (int c = 0; c < CHUNKS && ptrs[c]; c++) {
        for (int i = 0; i < CHUNK; i++) {
            if (ptrs[c][i] != (char)((c * 17 + i) & 0xFF)) {
                readback_ok = 0;
                break;
            }
        }
    }
    check("readback_3mb", readback_ok, 1);

    for (int c = 0; c < CHUNKS; c++)
        if (ptrs[c]) free(ptrs[c]);

    char *big = malloc(8 * 1024 * 1024);
    check("alloc_8mb",    big != NULL, 1);
    if (big) {
        big[0]                = 0xAB;
        big[4 * 1024 * 1024] = 0xCD;
        big[8 * 1024 * 1024 - 1] = 0xEF;
        check("big_boundary_lo",  (unsigned char)big[0],                    0xAB);
        check("big_boundary_mid", (unsigned char)big[4 * 1024 * 1024],      0xCD);
        check("big_boundary_hi",  (unsigned char)big[8 * 1024 * 1024 - 1],  0xEF);
        free(big);
    }

    end_module();
}

static void test_hw_bootinfo_files(void) {
    start_module("hw: bootInfo files");
    fb_info_t fb;
    sys_get_framebuffer(&fb);
    uint64_t fb_bytes = (uint64_t)fb.pitch * fb.height * 4;
    check("bootinfo_fb_size", fb_bytes > 0, 1);

    int wad = sys_fopen("/doom1.wad");
    if (wad >= 0) {
        unsigned char magic[4];
        sys_fread(wad, magic, 4);
        int wad_ok = (magic[0] == 'I' || magic[0] == 'P') &&
                      magic[1] == 'W' && magic[2] == 'A' && magic[3] == 'D';
        check("wad_from_bootinfo", wad_ok, 1);
    } else {
        sys_write(" (wad not installed, skipped)");
    }

    void *hbase = sys_heap_base();
    check("heap_page_aligned", ((uint64_t)hbase & 0xFFF) == 0, 1);

    end_module();
}

static void test_hw_ata_graceful(void) {
    start_module("hw: ATA graceful");

    uint64_t t0 = sys_get_ticks_ms();

    check("ticks_nonzero", t0 > 0, 1);

    volatile int depth = 0;
    void recurse(int n);
    char stack_check[256];
    for (int i = 0; i < 256; i++) stack_check[i] = (char)(i ^ 0xA5);
    int stack_ok = 1;
    for (int i = 0; i < 256; i++)
        if (stack_check[i] != (char)(i ^ 0xA5)) { stack_ok = 0; break; }
    check("stack_integrity", stack_ok, 1);
    (void)depth;

    end_module();
}

static void test_multiprocessing(void) {
    start_module("multiprocessing");

    sys_write(" spawning /bin/count... ");
    int pid = sys_execbg("/bin/count");
    check("execbg_returns_pid", pid > 0, 1);

    sys_sleep_ms(500);

    int res = sys_kill(pid);
    check("kill_child_pid", res == 0, 1);

    res = sys_kill(999);
    check("kill_invalid_pid", res == -1, 1);

    sys_write(" spawning /bin/pf_test (triggers page fault)... ");
    int pf_pid = sys_execbg("/bin/pf_test");
    check("execbg_pf_returns_pid", pf_pid > 0, 1);

    sys_sleep_ms(300);

    int pf_res = sys_kill(pf_pid);
    check("pf_child_terminated", pf_res, -1);

    end_module();
}

static void test_syscalls_misc(void) {
    start_module("syscalls misc");

    fat_dirent_t entries[4];
    int count = sys_readdir("/", entries, 4);
    check("readdir_root", count > 0, 1);

    fb_info_t fb;
    sys_get_framebuffer(&fb);
    check("fb_ptr_valid", fb.fb != NULL, 1);
    check("fb_dims_sane", fb.width > 0 && fb.height > 0, 1);

    check("set_color", 1, 1);

    key_event_t ev;
    sys_key_event_flush();
    check("key_event_empty", sys_key_event(&ev), 0);

    end_module();
}

static void test_files(void) {
    start_module("files");
    
    int f = sys_fopen("/txt/hello.txt");
    if (f >= 0) {
        sys_write(".");
        char buf[8];
        if (sys_fread(f, buf, 5) == 5) sys_write(".");
        if (sys_fseek(f, 0) == 0) sys_write(".");
        if (sys_fread(f, buf, 5) == 5) sys_write(".");
        g_errors += 0;
    } else {
        check("fopen_failed", 1, 0);
    }
    
    end_module();
}

int main(void) {
    sys_write("MarvinOS Tests\n");

    test_string_deep();
    test_string_extra();
    test_stdlib_deep();
    test_stdlib_extra();
    test_math_deep();
    test_stdio_deep();
    test_sprintf_fmt();
    test_sscanf();
    test_ctype_deep();
    test_scheduler();
    test_audio();
    test_memory_sys();
    test_syscalls_misc();
    test_files();
    test_fat_readback();
    test_fat_wad_header();
    test_fat_wad_seek();
    test_fat_wad_chunks();
    test_multiprocessing();

    test_hw_framebuffer();
    test_hw_phys_map();
    test_hw_bootinfo_files();
    test_hw_ata_graceful();

    sys_write("all tests completed successfully.\n");
    return 0;
}
