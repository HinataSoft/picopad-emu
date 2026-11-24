/* Keyboard mapping and tap latch (pure C, no SDL, no SDK). */
#include "keymap.h"

/* SDL2 scancodes */
enum {
    SC_A = 4, SC_B = 5, SC_S = 22, SC_X = 27, SC_Y = 28, SC_Z = 29, SC_ESC = 41,
    SC_RIGHT = 79, SC_LEFT = 80, SC_DOWN = 81, SC_UP = 82
};
/* Picopad key codes */
enum { K_UP = 1, K_LEFT, K_RIGHT, K_DOWN, K_X, K_Y, K_A, K_B };

unsigned int keymap_bit(keymap_mode mode, int sc)
{
    int code = 0;
    switch (sc) {
    case SC_UP:    code = K_UP;    break;
    case SC_LEFT:  code = K_LEFT;  break;
    case SC_RIGHT: code = K_RIGHT; break;
    case SC_DOWN:  code = K_DOWN;  break;
    case SC_ESC:   code = K_Y;     break;
    default:
        if (mode == KEYMAP_LAYOUT) {
            switch (sc) {
            case SC_A: code = K_X; break;
            case SC_S: code = K_Y; break;
            case SC_Z: code = K_A; break;
            case SC_X: code = K_B; break;
            }
        } else {
            switch (sc) {
            case SC_A: code = K_A; break;
            case SC_B: code = K_B; break;
            case SC_X: code = K_X; break;
            case SC_Y: code = K_Y; break;
            }
        }
    }
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
