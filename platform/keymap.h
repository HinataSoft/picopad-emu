/* Keyboard mapping and tap latch. Pure C: no SDL, no SDK headers.
   Scancodes are plain ints with SDL2 values. */
#ifndef PICOPAD_PLATFORM_KEYMAP_H
#define PICOPAD_PLATFORM_KEYMAP_H

#include "../include/plat_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { KEYMAP_LETTERS = 0, KEYMAP_LAYOUT = 1 } keymap_mode;

/* PLAT_KEY_BIT(code) for the Picopad key mapped to the scancode, or 0 */
unsigned int keymap_bit(keymap_mode mode, int sdl_scancode);

/* Tap latch: keeps a pressed bit set for at least KEYLATCH_HOLD_US after its
   press, so a tap shorter than one 5 ms SysTick period is not lost. */
#define KEYLATCH_HOLD_US 5000ull
typedef struct keylatch {
    unsigned int prev_raw;             /* raw bits at the previous update */
    unsigned int pending;              /* bits pressed less than HOLD_US ago */
    unsigned long long press_us[8];    /* time of the last press of each bit */
} keylatch;

/* returns the effective bits (raw, plus recently pressed bits still held) */
unsigned int keylatch_update(keylatch* l, unsigned int raw_bits, unsigned long long now_us);

#ifdef __cplusplus
}
#endif
#endif
