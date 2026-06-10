#ifndef SND_MIXER_H
#define SND_MIXER_H

#include "doomtype.h"

#define MIX_CHANNELS  8
#define MIX_MAX_CHUNK 1024

void    mixer_init(void);
void    mixer_start(int ch, const short *pcm, unsigned len, int vol, int sep);
void    mixer_stop(int ch);
void    mixer_update_params(int ch, int vol, int sep);
boolean mixer_is_active(int ch);
boolean mixer_any_active(void);
void    mixer_fill(short *out, unsigned count);

#endif /* SND_MIXER_H */
