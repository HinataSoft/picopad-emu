/* Keyboard mapping and tap latch: pure C, no SDK. */
#include "check.h"
#include "../platform/keymap.h"

int main(void)
{
    keylatch l;
    unsigned int a = PLAT_KEY_BIT(7), b = PLAT_KEY_BIT(8);

    CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 4),  PLAT_KEY_BIT(7));  /* A -> KEY_A */
    CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 28), PLAT_KEY_BIT(6));  /* Y -> KEY_Y */
    CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 41), PLAT_KEY_BIT(6));  /* Esc -> KEY_Y */
    CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 29), 0);                /* Z unmapped */
    CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 5),  PLAT_KEY_BIT(8));  /* B */
    CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 27), PLAT_KEY_BIT(5));  /* X */
    CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 4),   PLAT_KEY_BIT(5));  /* A -> KEY_X */
    CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 22),  PLAT_KEY_BIT(6));  /* S -> KEY_Y */
    CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 29),  PLAT_KEY_BIT(7));  /* Z -> KEY_A */
    CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 27),  PLAT_KEY_BIT(8));  /* X -> KEY_B */
    CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 41),  PLAT_KEY_BIT(6));  /* Esc -> KEY_Y */
    CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 82),  PLAT_KEY_BIT(1));  /* Up -> KEY_UP */
    CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 80),  PLAT_KEY_BIT(2));  /* Left */
    CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 79),  PLAT_KEY_BIT(3));  /* Right */
    CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 81),  PLAT_KEY_BIT(4));  /* Down */
    CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 82), PLAT_KEY_BIT(1));
    CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 81), PLAT_KEY_BIT(4));
    CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 0),  0);
    CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 999), 0);

    /* (1) tap: press t=0, release t=1000 */
    l = (keylatch){0};
    CHECK_EQ(keylatch_update(&l, a, 0), a);
    CHECK_EQ(keylatch_update(&l, 0, 1000), a);
    CHECK_EQ(keylatch_update(&l, 0, 4999), a);
    CHECK_EQ(keylatch_update(&l, 0, 5000), 0);
    CHECK_EQ(keylatch_update(&l, 0, 9000), 0);

    /* (2) long hold */
    l = (keylatch){0};
    CHECK_EQ(keylatch_update(&l, a, 100), a);
    CHECK_EQ(keylatch_update(&l, a, 6000), a);
    CHECK_EQ(keylatch_update(&l, a, 100000), a);
    CHECK_EQ(keylatch_update(&l, 0, 100000), 0);   /* long press: release is immediate */

    /* (3) two keys independently */
    l = (keylatch){0};
    CHECK_EQ(keylatch_update(&l, a, 0), a);
    CHECK_EQ(keylatch_update(&l, a | b, 3000), a | b);
    CHECK_EQ(keylatch_update(&l, b, 4000), a | b);   /* A released, latched until 5000 */
    CHECK_EQ(keylatch_update(&l, 0, 5000), b);       /* A expired, B pressed at 3000 till 8000 */
    CHECK_EQ(keylatch_update(&l, 0, 7999), b);
    CHECK_EQ(keylatch_update(&l, 0, 8000), 0);

    /* (4) re-press after release starts a new window */
    l = (keylatch){0};
    CHECK_EQ(keylatch_update(&l, a, 0), a);
    CHECK_EQ(keylatch_update(&l, 0, 1000), a);
    CHECK_EQ(keylatch_update(&l, 0, 5000), 0);
    CHECK_EQ(keylatch_update(&l, a, 10000), a);
    CHECK_EQ(keylatch_update(&l, 0, 10500), a);
    CHECK_EQ(keylatch_update(&l, 0, 14999), a);
    CHECK_EQ(keylatch_update(&l, 0, 15000), 0);

    return CHECK_DONE();
}
