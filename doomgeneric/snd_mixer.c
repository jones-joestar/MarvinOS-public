#include "snd_mixer.h"
#include "doomtype.h"
#include <string.h>

typedef struct {
    const short *pcm;
    unsigned     len;
    unsigned     pos;
    int          vol;    /* 0-127 */
    int          sep;    /* 0-254, 127=center (unused for mono output) */
    boolean      active;
} mix_ch_t;

static mix_ch_t ch[MIX_CHANNELS];

void mixer_init(void) {
    memset(ch, 0, sizeof(ch));
}

void mixer_start(int i, const short *pcm, unsigned len, int vol, int sep) {
    if (i < 0 || i >= MIX_CHANNELS) return;
    ch[i].pcm    = pcm;
    ch[i].len    = len;
    ch[i].pos    = 0;
    ch[i].vol    = vol;
    ch[i].sep    = sep;
    ch[i].active = true;
}

void mixer_stop(int i) {
    if (i < 0 || i >= MIX_CHANNELS) return;
    ch[i].active = false;
}

void mixer_update_params(int i, int vol, int sep) {
    if (i < 0 || i >= MIX_CHANNELS) return;
    ch[i].vol = vol;
    ch[i].sep = sep;
}

boolean mixer_is_active(int i) {
    if (i < 0 || i >= MIX_CHANNELS) return false;
    return ch[i].active;
}

boolean mixer_any_active(void) {
    for (int i = 0; i < MIX_CHANNELS; i++) {
        if (ch[i].active) return true;
    }
    return false;
}

void mixer_fill(short *out, unsigned count) {
    if (count > MIX_MAX_CHUNK) count = MIX_MAX_CHUNK;
    static int acc[MIX_MAX_CHUNK];
    memset(acc, 0, count * sizeof(int));

    for (int i = 0; i < MIX_CHANNELS; i++) {
        if (!ch[i].active) continue;
        unsigned avail = ch[i].len - ch[i].pos;
        unsigned n = count < avail ? count : avail;
        const short *pcm = ch[i].pcm + ch[i].pos;
        int vol = ch[i].vol;
        for (unsigned j = 0; j < n; j++)
            acc[j] += ((int)pcm[j] * vol) >> 7;
        ch[i].pos += n;
        if (ch[i].pos >= ch[i].len)
            ch[i].active = false;
    }

    for (unsigned i = 0; i < count; i++) {
        int s = acc[i];
        if (s >  32767) s =  32767;
        if (s < -32767) s = -32767;
        out[i] = (short)s;
    }
}
