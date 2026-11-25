/* Zapis PNG (RGB 8 bit, nekomprimovany zlib) z framebufferu RGB565. Cisty C. */
#ifndef PICOPAD_PLATFORM_PNG_H
#define PICOPAD_PLATFORM_PNG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Zapise w x h pixelu RGB565 (stride = pocet pixelu na radek v fb) do path.
   Vraci 0 = OK, -1 = chyba (errno zachovano). Vystup je bajtove deterministicky. */
int png_write_rgb565(const char* path, const unsigned short* fb, int w, int h, int stride);

#ifdef __cplusplus
}
#endif

#endif
