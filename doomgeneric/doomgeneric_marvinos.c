#include "doomgeneric.h"
#include "doomkeys.h"
#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"
#include "snd_mixer.h"
#include <syscall.h>
#include <stdio.h>
#include <string.h>

static fb_info_t fb;
static boolean   sb16_found = false;

void DG_Init() {
    sys_get_framebuffer(&fb);
}

void DG_DrawFrame() {
    if (!fb.fb) return;

    unsigned int *dst = (unsigned int *)fb.fb;

    unsigned int scale_x = fb.width  / DOOMGENERIC_RESX;
    unsigned int scale_y = fb.height / DOOMGENERIC_RESY;
    unsigned int scale   = scale_x < scale_y ? scale_x : scale_y;
    if (scale < 1) scale = 1;

    // Write one complete framebuffer row at a time so WC buffers fill and
    // flush in large bursts rather than scattering writes across multiple rows.
    for (unsigned int y = 0; y < DOOMGENERIC_RESY; y++) {
        const unsigned int *src_row = DG_ScreenBuffer + y * DOOMGENERIC_RESX;
        for (unsigned int sy = 0; sy < scale; sy++) {
            unsigned int *fb_row = dst + (y * scale + sy) * fb.pitch;
            for (unsigned int x = 0; x < DOOMGENERIC_RESX; x++) {
                unsigned int color = src_row[x];
                for (unsigned int sx = 0; sx < scale; sx++)
                    fb_row[x * scale + sx] = color;
            }
        }
    }

    if (sb16_found) sys_sync();
}

void DG_SleepMs(unsigned int ms) {
    sys_sleep_ms(ms);
}

unsigned int DG_GetTicksMs() {
    return sys_get_ticks_ms();
}

 static unsigned char scancode_to_doomkey(unsigned char sc) {
    if (sc & 0x80) {
        switch (sc & 0x7F) {
            case 0x47: return KEY_HOME;
            case 0x48: return KEY_UPARROW;
            case 0x49: return KEY_PGUP;
            case 0x50: return KEY_DOWNARROW;
            case 0x4B: return KEY_LEFTARROW;
            case 0x4D: return KEY_RIGHTARROW;
            case 0x1D: return KEY_FIRE;
            case 0x38: return KEY_RALT;
            case 0x4F: return KEY_END;
            case 0x51: return KEY_PGDN;
            case 0x52: return KEY_INS;
            case 0x53: return KEY_DEL;
            default:   return 0;
        }
    }
    switch (sc) {
        case 0x01: return KEY_ESCAPE;
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x0B: return '0';
        case 0x0C: return '-';
        case 0x0D: return '=';
        case 0x0E: return KEY_BACKSPACE;
        case 0x0F: return KEY_TAB;

        case 0x10: return 'q';
        case 0x11: return 'w';
        case 0x12: return 'e';
        case 0x13: return 'r';
        case 0x14: return 't';
        case 0x15: return 'z';
        case 0x16: return 'u';
        case 0x17: return 'i';
        case 0x18: return 'o';
        case 0x19: return 'p';
        case 0x1A: return '[';
        case 0x1B: return ']';
        case 0x1C: return KEY_ENTER;
        case 0x1D: return KEY_FIRE;
        case 0x1E: return 'a';
        case 0x1F: return 's';

        case 0x20: return 'd';
        case 0x21: return 'f';
        case 0x22: return 'g';
        case 0x23: return 'h';
        case 0x24: return 'j';
        case 0x25: return 'k';
        case 0x26: return 'l';
        case 0x2A: case 0x36: return KEY_RSHIFT;
        case 0x2B: return '\\';
        case 0x2C: return 'y';
        case 0x2D: return 'x';
        case 0x2E: return 'c';
        case 0x2F: return 'v';

        case 0x30: return 'b';
        case 0x31: return 'n';
        case 0x32: return 'm';

        case 0x35: return '/';

        case 0x38: return KEY_RALT;
        case 0x39: return KEY_USE;

        case 0x3B: return KEY_F1;
        case 0x3C: return KEY_F2;
        case 0x3D: return KEY_F3;
        case 0x3E: return KEY_F4;
        case 0x3F: return KEY_F5;

        case 0x40: return KEY_F6;
        case 0x41: return KEY_F7;
        case 0x42: return KEY_F8;
        case 0x43: return KEY_F9;
        case 0x44: return KEY_F10;

        case 0x57: return KEY_F11;
        case 0x58: return KEY_F12;

        default:   return 0;
    }
}


int DG_GetKey(int *pressed, unsigned char *key) {
    key_event_t ev;
    while (sys_key_event(&ev))
    {
        unsigned char dk = scancode_to_doomkey(ev.scancode);
        if (!dk)
        {
            continue;
        }
        *pressed = ev.pressed;
        *key = dk;
        return 1;
    }

    return 0;
}

void DG_SetWindowTitle(const char * title)
{
    (void)title;
}

// Sound

#define AUDIO_SAMPLE_RATE  22050
#define AUDIO_UPDATE_CHUNK 630

int   use_libsamplerate   = 0;
float libsamplerate_scale = 1.0f;

static boolean sfx_prefix  = true;
static boolean audio_ready  = false;
static short   mix_buf[AUDIO_UPDATE_CHUNK];

typedef struct __attribute__((packed)) {
    uint16_t format;
    uint16_t samplerate;
    uint32_t numsamples;
} doom_sfx_hdr_t;

typedef struct {
    unsigned len;
    short    pcm[];
} sfx_pcm_t;

static void cache_sfx(sfxinfo_t *sfx) {
    if (sfx->driver_data) return;
    if (sfx->lumpnum < 0) return;

    int lumplen = W_LumpLength(sfx->lumpnum);
    if (lumplen <= (int)sizeof(doom_sfx_hdr_t)) return;

    const uint8_t *data = W_CacheLumpNum(sfx->lumpnum, PU_STATIC);
    const doom_sfx_hdr_t *hdr = (const doom_sfx_hdr_t *)data;
    if (hdr->format != 3) return;

    uint32_t num = hdr->numsamples;
    uint32_t max = (uint32_t)(lumplen - (int)sizeof(doom_sfx_hdr_t));
    if (num > max) num = max;
    if (num == 0) return;

    const uint8_t *raw = data + sizeof(doom_sfx_hdr_t);
    uint32_t src_rate = hdr->samplerate ? hdr->samplerate : 11025;
    uint32_t out_num  = (uint32_t)((uint64_t)num * AUDIO_SAMPLE_RATE / src_rate);
    if (out_num == 0) return;

    sfx_pcm_t *buf = Z_Malloc(sizeof(sfx_pcm_t) + out_num * sizeof(short), PU_STATIC, NULL);
    buf->len = out_num;
    for (uint32_t i = 0; i < out_num; i++) {
        uint32_t src_i = (uint32_t)((uint64_t)i * src_rate / AUDIO_SAMPLE_RATE);
        if (src_i >= num - 1) { buf->pcm[i] = ((int)raw[num - 1] - 128) * 256; continue; }
        uint32_t frac = (uint32_t)(((uint64_t)i * src_rate % AUDIO_SAMPLE_RATE) * 256 / AUDIO_SAMPLE_RATE);
        int s = (int)raw[src_i] + (((int)raw[src_i + 1] - (int)raw[src_i]) * (int)frac >> 8);
        buf->pcm[i] = (short)((s - 128) * 256);
    }
    sfx->driver_data = buf;
}

static snddevice_t marvin_snd_devices[] = { SNDDEVICE_SB };

static boolean Marvin_SoundInit(boolean use_prefix) {
    sfx_prefix = use_prefix;
    mixer_init();
    return true;
}

static void Marvin_SoundShutdown(void) {}

static int Marvin_GetSfxLumpNum(sfxinfo_t *sfx) {
    char name[16];
    if (sfx_prefix) snprintf(name, sizeof(name), "ds%s", sfx->name);
    else            snprintf(name, sizeof(name), "%s",   sfx->name);
    return W_CheckNumForName(name);
}

static int Marvin_StartSound(sfxinfo_t *sfx, int channel, int vol, int sep) {
    if (!sfx || sfx->lumpnum < 0) return -1;
    if (channel < 0 || channel >= MIX_CHANNELS) return -1;
    cache_sfx(sfx);
    if (!sfx->driver_data) return -1;
    sfx_pcm_t *buf = (sfx_pcm_t *)sfx->driver_data;
    mixer_start(channel, buf->pcm, buf->len, vol, sep);
    return channel;
}

static void    Marvin_StopSound(int ch)                  { mixer_stop(ch); }
static void    Marvin_UpdateSoundParams(int ch, int v, int s) { mixer_update_params(ch, v, s); }
static boolean Marvin_SoundIsPlaying(int ch)             { return mixer_is_active(ch); }

static void Marvin_Update(void) {
    if (!audio_ready) {
        sb16_found = (sys_audio_init(AUDIO_SAMPLE_RATE) != 0);
        audio_ready = true;
    }
    if (!sb16_found) return;
    mixer_fill(mix_buf, AUDIO_UPDATE_CHUNK);
    sys_audio_write(mix_buf, AUDIO_UPDATE_CHUNK);
}

sound_module_t DG_sound_module = {
    marvin_snd_devices,
    (int)(sizeof(marvin_snd_devices) / sizeof(*marvin_snd_devices)),
    Marvin_SoundInit,
    Marvin_SoundShutdown,
    Marvin_GetSfxLumpNum,
    Marvin_Update,
    Marvin_UpdateSoundParams,
    Marvin_StartSound,
    Marvin_StopSound,
    Marvin_SoundIsPlaying,
    NULL
};

// Music (stub)

static snddevice_t marvin_music_devices[] = { SNDDEVICE_NONE };

static boolean Marvin_MusicInit(void)                       { return false; }
static void    Marvin_MusicShutdown(void)                   {}
static void    Marvin_SetMusicVolume(int v)                 { (void)v; }
static void    Marvin_PauseMusic(void)                      {}
static void    Marvin_ResumeMusic(void)                     {}
static void   *Marvin_RegisterSong(void *d, int l)         { (void)d; (void)l; return NULL; }
static void    Marvin_UnRegisterSong(void *h)               { (void)h; }
static void    Marvin_PlaySong(void *h, boolean loop)      { (void)h; (void)loop; }
static void    Marvin_StopSong(void)                        {}
static boolean Marvin_MusicIsPlaying(void)                  { return false; }

music_module_t DG_music_module = {
    marvin_music_devices,
    (int)(sizeof(marvin_music_devices) / sizeof(*marvin_music_devices)),
    Marvin_MusicInit,
    Marvin_MusicShutdown,
    Marvin_SetMusicVolume,
    Marvin_PauseMusic,
    Marvin_ResumeMusic,
    Marvin_RegisterSong,
    Marvin_UnRegisterSong,
    Marvin_PlaySong,
    Marvin_StopSong,
    Marvin_MusicIsPlaying,
    NULL
};