/* Zapis WAV (RIFF PCM, mono, 16 bit, little endian). Cisty C; jedno vlakno naraz. */
#ifndef PICOPAD_PLATFORM_WAV_H
#define PICOPAD_PLATFORM_WAV_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wav_writer wav_writer;

/* Otevre soubor a zapise hlavicku; NULL pri chybe. */
wav_writer* wav_open(const char* path, int rate);
/* Pripoji n vzorku; 0 = OK, -1 = chyba. */
int wav_write(wav_writer* w, const short* s, int n);
/* Doplni velikosti v hlavicce a zavre soubor; 0 = OK, -1 = chyba. */
int wav_close(wav_writer* w);

#ifdef __cplusplus
}
#endif

#endif
