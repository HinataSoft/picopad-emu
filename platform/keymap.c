/* Keyboard mapping and tap latch (pure C, no SDL, no SDK). */
#include "keymap.h"

/* SDL2 scancodes (physical position, US QWERTY names) */
enum {
    SC_A = 4, SC_S = 22, SC_X = 27, SC_Z = 29, SC_ESC = 41,
    SC_RIGHT = 79, SC_LEFT = 80, SC_DOWN = 81, SC_UP = 82
};
/* SDL2 keycodes (key label); arrows are SDLK = scancode | (1 << 30) */
enum {
    KC_ESC = 27, KC_A = 97, KC_B = 98, KC_X = 120, KC_Y = 121,
    KC_RIGHT = 0x4000004F, KC_LEFT = 0x40000050, KC_DOWN = 0x40000051, KC_UP = 0x40000052
};
/* Picopad key codes */
enum { K_UP = 1, K_LEFT, K_RIGHT, K_DOWN, K_X, K_Y, K_A, K_B };

/* layout mode: by physical position */
static int layout_code(int sc)
{
    switch (sc) {
    case SC_UP:    return K_UP;
    case SC_LEFT:  return K_LEFT;
    case SC_RIGHT: return K_RIGHT;
    case SC_DOWN:  return K_DOWN;
    case SC_ESC:   return K_Y;
    case SC_A:     return K_X;
    case SC_S:     return K_Y;
    case SC_Z:     return K_A;
    case SC_X:     return K_B;
    }
    return 0;
}

/* letters mode: by key label */
static int letters_code(int kc)
{
    switch (kc) {
    case KC_UP:    return K_UP;
    case KC_LEFT:  return K_LEFT;
    case KC_RIGHT: return K_RIGHT;
    case KC_DOWN:  return K_DOWN;
    case KC_ESC:   return K_Y;
    case KC_A:     return K_A;
    case KC_B:     return K_B;
    case KC_X:     return K_X;
    case KC_Y:     return K_Y;
    }
    return 0;
}

unsigned int keymap_bit(keymap_mode mode, int sc, int kc)
{
    int code = (mode == KEYMAP_LAYOUT) ? layout_code(sc) : letters_code(kc);
    return code ? PLAT_KEY_BIT(code) : 0;
}

unsigned int keylatch_update(keylatch* l, unsigned int raw, unsigned long long now)
{
    unsigned int pressed = raw & ~l->prev_raw;
    int i;
    for (i = 0; i < 8; i++) {
        unsigned int bit = 1u << i;
        if (pressed & bit) {
            l->press_us[i] = now;
            l->pending |= bit;
        }
        /* the latch window of a press ends HOLD_US after the press */
        if ((l->pending & bit) && now - l->press_us[i] >= KEYLATCH_HOLD_US)
            l->pending &= ~bit;
    }
    l->prev_raw = raw;
    return raw | l->pending;
}
