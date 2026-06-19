/*============================================================================
 * test_neat.c - Host unit test for the C&T CS8221 (NEAT) chipset ops
 *
 * Exercises the real ops_ct_neat from CK_LEGAC.C against the CK_IOSIM.C
 * simulated 0x22/0x23 register file: detection (index round-trip + 0x60-0x6F
 * window), A20 (RB12 0x6F GA20, inverted), and shadow/UMB control (RB1 0x65 +
 * RB4 0x68 / RB5 0x69). Verified against 86Box src/chipset/neat.c.
 *
 * Build:  cc -std=c89 -Wall -Wextra CK_LEGAC.C CK_IOSIM.C test_neat.c -o test_neat
 *============================================================================*/

#include <stdio.h>
#include "CK_HAL.H"
#include "CK_IOSIM.H"

static int g_pass = 0, g_fail = 0;

static void chk(const char *what, int ok)
{
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (ok) g_pass++; else g_fail++;
}

int main(void)
{
    const chipset_ops_t *cs = &ops_ct_neat;

    printf("NEAT (CS8221) host unit test\n");
    printf("============================\n");

    /* ---- detection ---- */
    printf("\n[detection]\n");
    cksim_reset();                       /* full window: out-of-window reads 0x00 */
    chk("empty bus -> probe() == 0", cs->probe() == 0);

    cksim_reset();
    cksim_set_reg_window(0x60, 0x6F);    /* model the real NEAT decoded window */
    chk("NEAT present -> probe() == 1", cs->probe() == 1);
    chk("info_only == 0 (controllable)", cs->info_only == 0);
    chk("shadow_regions == 16", cs->shadow_regions == 16);

    /* ---- A20: RB12 (0x6F) GA20 bit1, inverted (clear = enabled) ---- */
    printf("\n[A20 gate - RB12 0x6F GA20]\n");
    cs->a20_set(1);
    chk("a20_set(1) clears 0x6F bit1", (cksim_get_reg(0x6F) & 0x02) == 0);
    chk("a20_get() == 1", cs->a20_get() == 1);
    cs->a20_set(0);
    chk("a20_set(0) sets 0x6F bit1", (cksim_get_reg(0x6F) & 0x02) != 0);
    chk("a20_get() == 0", cs->a20_get() == 0);

    /* ---- shadow: region 4 = D0000 (RB1 0x65 + RB4 0x68 bit4) ---- */
    printf("\n[shadow RAM - region 4 (D0000)]\n");
    chk("shadow_set(4, RW) == HAL_OK", cs->shadow_set(4, SHADOW_RW) == HAL_OK);
    chk("  RB1 0x65 bit2 set (ROM D disabled)", (cksim_get_reg(0x65) & 0x04) != 0);
    chk("  RB1 0x65 bit6 clear (writable)",     (cksim_get_reg(0x65) & 0x40) == 0);
    chk("  RB4 0x68 bit4 set (D0000 shadow)",   (cksim_get_reg(0x68) & 0x10) != 0);
    chk("shadow_get(4) == SHADOW_RW", cs->shadow_get(4) == SHADOW_RW);

    cs->shadow_set(4, SHADOW_RO);
    chk("RO -> RB1 0x65 bit6 set (write-protect)", (cksim_get_reg(0x65) & 0x40) != 0);
    chk("shadow_get(4) == SHADOW_RO", cs->shadow_get(4) == SHADOW_RO);

    cs->shadow_set(4, SHADOW_DISABLED);
    chk("DISABLED -> RB4 0x68 bit4 clear", (cksim_get_reg(0x68) & 0x10) == 0);
    chk("shadow_get(4) == SHADOW_DISABLED", cs->shadow_get(4) == SHADOW_DISABLED);

    /* ---- shadow: region 8 = E0000 exercises the RB5 (0x69) path ---- */
    printf("\n[shadow RAM - region 8 (E0000) -> RB5]\n");
    chk("shadow_set(8, RW) == HAL_OK", cs->shadow_set(8, SHADOW_RW) == HAL_OK);
    chk("  RB5 0x69 bit0 set (E0000 shadow)", (cksim_get_reg(0x69) & 0x01) != 0);
    chk("shadow_get(8) == SHADOW_RW", cs->shadow_get(8) == SHADOW_RW);

    /* ---- range guards ---- */
    printf("\n[range guards]\n");
    chk("shadow_set(16, RW) == HAL_ERR_PARAM", cs->shadow_set(16, SHADOW_RW) == HAL_ERR_PARAM);
    chk("shadow_set(-1, RW) == HAL_ERR_PARAM", cs->shadow_set(-1, SHADOW_RW) == HAL_ERR_PARAM);
    chk("shadow_get(99) == HAL_ERR_PARAM",     cs->shadow_get(99) == HAL_ERR_PARAM);

    printf("\n----------------------------\n");
    printf("result: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
