/*============================================================================
 * CK_LEGAC.C - 286-Era Legacy Chipset HAL Implementations
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 11:00:00 EST
 *
 * Supported Chipsets:
 * - C&T CS8221 (NEAT Full)   - 286, info-only, A20 control
 * - C&T CS8230 (NEAT-386)    - 386SX, info-only, A20 control
 * - C&T 82C301 (PEAK)        - 386, boundary NC, cache control
 * - C&T 82C235 (SCAT)        - 286, boundary NC, cache control
 * - Headland HT12            - 286/386, cache at 0x10, shadow at 0x12-0x13
 * - Headland HT18            - 386SX, info-only
 * - Headland HT101           - 286, info-only
 * - Headland HT102           - 286, info-only
 * - VLSI VL82C100/101/102    - 286, info-only
 * - VLSI VL82C311            - 286/386, 1 NC region
 * - VLSI VL82C320            - 386SX, info-only
 * - VIA VT82C310             - 386/486, 2 NC regions, 64KB granularity
 * - ALi M1209 (Finis)        - 386, boundary NC
 * - ALi M1217                - Info-only
 *
 * These are mostly observer-only chipsets with limited or no NC region
 * support. Some have A20 gate and shadow RAM control.
 *
 * All use legacy 0x22/0x23 index/data ports.
 *============================================================================*/

#include "CK_HAL.H"

/*============================================================================
 * EXTERNAL DECLARATIONS
 *============================================================================*/

/* From CK_IO.C */
extern unsigned char legacy_read_22_23(unsigned char reg);
extern void legacy_write_22_23(unsigned char reg, unsigned char val);
extern int legacy_port_valid(unsigned int index_port, unsigned int data_port);
extern unsigned char io_read_byte(unsigned int port);
extern void io_write_byte(unsigned int port, unsigned char val);

/* From CK_HAL.C */
extern int generic_wbinvd_flush(void);
extern int generic_invd_flush(void);
extern int generic_port92_a20_get(void);
extern int generic_port92_a20_set(int enable);
extern int hal_stub_unsupported(void);
extern int hal_stub_unsupported_i(int x);
extern int hal_stub_unsupported_ii(int x, int y);
extern int hal_stub_unsupported_iull(int x, unsigned long y, unsigned long z);
extern int hal_stub_nc_read(int idx, nc_region_t *r);

/*============================================================================
 * LEGACY CHIPSET CONSTANTS
 *============================================================================*/

#define LEGACY_INDEX_PORT   0x22
#define LEGACY_DATA_PORT    0x23

/*============================================================================
 * SHARED LEGACY HELPER FUNCTIONS
 *============================================================================*/

/*
 * Read legacy chipset register via 0x22/0x23
 * Note: This is a local shortcut using default ports.
 * The CK_HAL.H version takes explicit port arguments.
 */
static unsigned char legacy_read_default(unsigned char reg)
{
    return legacy_read_22_23(reg);
}

/*
 * Write legacy chipset register via 0x22/0x23
 * Note: This is a local shortcut using default ports.
 * The CK_HAL.H version takes explicit port arguments.
 */
static void legacy_write_default(unsigned char reg, unsigned char val)
{
    legacy_write_22_23(reg, val);
}

/*
 * Generic register read for F4 dump
 */
static int legacy_reg_read(int reg)
{
    if (reg < 0 || reg > 0xFF) {
        return -1;
    }
    return legacy_read_default((unsigned char)reg);
}

/*
 * Generic register write
 */
static int legacy_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 0xFF || val < 0 || val > 0xFF) {
        return HAL_ERR_PARAM;
    }
    legacy_write_default((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

/*============================================================================
 * C&T (CHIPS AND TECHNOLOGIES) NEAT CHIPSETS
 *
 * NEAT = "New Enhanced AT" chipset family
 *
 * CS8221 (NEAT Full): Complete 286 chipset
 * CS8230 (NEAT-386): 386SX variant
 *
 * Register layout (index/data via 0x22/0x23; config window is index 0x60-0x6F,
 * verified against 86Box src/chipset/neat.c and the CS8221 databook):
 *   0x65 (RB1):  ROM enable (bits 0-3 = F,E,D,C; 0=ROM on) + write-protect
 *                (bits 4-7 = F,E,D,C; 1=read-only)
 *   0x68 (RB4):  per-16K shadow enable, C0000-DFFFF
 *   0x69 (RB5):  per-16K shadow enable, E0000-FFFFF
 *   0x6F (RB12): A20 gate at bit1 (GA20), INVERTED (clear = A20 enabled)
 *
 * NOTE: the earlier "0x19-0x1B shadow / 0x1D control" map was wrong lore - those
 * indices are outside the 0x60-0x6F window and read back 0xFF on real hardware
 * (detection silently failed; A20 hit a non-existent register).
 *============================================================================*/

#define NEAT_RB12_MISC      0x6F    /* RB12 misc reg; bit1 (GA20) gates A20 */
#define NEAT_A20_BIT        0x02    /* GA20: SET = A20 masked, CLEAR = A20 enabled */
#define NEAT_RB1_ROMCFG     0x65    /* ROM enable (bits 0-3) + write-protect (4-7) */
#define NEAT_RB4_SHEN_CD    0x68    /* per-16K shadow enable, C0000-DFFFF */
#define NEAT_RB5_SHEN_EF    0x69    /* per-16K shadow enable, E0000-FFFFF */
#define NEAT_SHADOW_REGIONS 16      /* 16 x 16K blocks: region 0=C0000 .. 15=FC000 */

/*
 * Probe for NEAT chipset (verified against 86Box src/chipset/neat.c).
 *
 * NOTE: do NOT use legacy_port_valid() here - it probes index 0x00, which is
 * outside NEAT's 0x60-0x6F window, so the data port floats 0xFF and the chip
 * looks absent. Identify the CS8221 non-destructively instead:
 *   1. port 0x22 is a readable index latch - round-trip two distinct values;
 *      an undecoded port floats 0xFF and won't round-trip.
 *   2. the data port answers for an in-window index (0x60-0x6E) but floats
 *      0xFF for an out-of-window one - distinguishes NEAT from other 0x22/0x23
 *      chipsets that decode a different index range.
 */
static int neat_probe(void)
{
    unsigned char a, b, win_in, win_out;

    io_write_byte(LEGACY_INDEX_PORT, 0x65);
    a = io_read_byte(LEGACY_INDEX_PORT);
    io_write_byte(LEGACY_INDEX_PORT, 0x6A);
    b = io_read_byte(LEGACY_INDEX_PORT);
    if (a != 0x65 || b != 0x6A) {
        return 0;               /* no index latch -> not an index/data chipset */
    }

    io_write_byte(LEGACY_INDEX_PORT, 0x10);             /* outside 0x60-0x6F     */
    win_out = io_read_byte(LEGACY_DATA_PORT);
    io_write_byte(LEGACY_INDEX_PORT, NEAT_RB4_SHEN_CD); /* 0x68, inside window   */
    win_in  = io_read_byte(LEGACY_DATA_PORT);

    return (win_out == 0xFF && win_in != 0xFF);
}

/*
 * C&T NEAT A20 control - RB12 (0x6F) GA20 bit, INVERTED:
 * clearing the bit enables A20 (cf. 86Box neat.c: mem_a20_alt = !(val & GA20)).
 */
static int neat_a20_get(void)
{
    unsigned char val = legacy_read_default(NEAT_RB12_MISC);
    return (val & NEAT_A20_BIT) ? 0 : 1;
}

static int neat_a20_set(int enable)
{
    unsigned char val = legacy_read_default(NEAT_RB12_MISC);

    if (enable) {
        val &= ~NEAT_A20_BIT;       /* clear GA20 to enable A20 */
    } else {
        val |= NEAT_A20_BIT;        /* set GA20 to mask A20      */
    }

    legacy_write_default(NEAT_RB12_MISC, val);
    return HAL_OK;
}

/*
 * NEAT cache state - these are 286 chipsets with no L2 cache
 * Return "enabled" as there's nothing to control
 */
static int neat_cache_get(void)
{
    return CACHE_ENABLED;
}

/*
 * NEAT shadow RAM / UMB control (verified against 86Box src/chipset/neat.c).
 *
 * Region 0..15 = the sixteen 16K blocks C0000-FFFFF (region 0 = C0000). Each
 * block's state is set by two registers:
 *   RB1 (0x65): per-64K ROM-enable bits 0-3 (F,E,D,C; 0=ROM on) and per-64K
 *               write-protect bits 4-7 (F,E,D,C; 1=read-only)
 *   RB4 (0x68)/RB5 (0x69): per-16K shadow-enable bit (1=RAM mapped)
 * ROMCS has priority: a block is RAM only if its RB1 ROM bit = 1 (ROM off) AND
 * its RB4/RB5 enable bit = 1. Backing DRAM (the relocated 384K) must be present
 * for an RW UMB; for an RO shadow the caller copies ROM->RAM before enabling.
 */
static void neat_shadow_bits(int region, int *rom_bit, int *wp_bit,
                             unsigned char *en_reg, int *en_bit)
{
    int g = region >> 2;                  /* 0=C,1=D,2=E,3=F */
    *rom_bit = 3 - g;                     /* RB1 ROM-enable bit (0=ROM on) */
    *wp_bit  = 7 - g;                     /* RB1 write-protect bit (1=RO)  */
    if (region < 8) { *en_reg = NEAT_RB4_SHEN_CD; *en_bit = region;     }
    else            { *en_reg = NEAT_RB5_SHEN_EF; *en_bit = region - 8; }
}

static int neat_shadow_set(int region, int mode)
{
    int rom_bit, wp_bit, en_bit;
    unsigned char en_reg, rb1, en;

    if (region < 0 || region >= NEAT_SHADOW_REGIONS) return HAL_ERR_PARAM;
    neat_shadow_bits(region, &rom_bit, &wp_bit, &en_reg, &en_bit);

    rb1 = legacy_read_default(NEAT_RB1_ROMCFG);   /* read-modify-write: keep */
    en  = legacy_read_default(en_reg);            /* other blocks' bits intact */

    switch (mode) {
        case SHADOW_DISABLED:                          /* ROM visible */
            rb1 &= (unsigned char)~(1 << rom_bit);     /* ROM enabled (0)  */
            en  &= (unsigned char)~(1 << en_bit);      /* shadow disabled  */
            break;
        case SHADOW_RO:                                /* read-only RAM copy */
            rb1 |= (unsigned char)(1 << rom_bit);      /* ROM disabled (1)  */
            rb1 |= (unsigned char)(1 << wp_bit);       /* write-protect (1) */
            en  |= (unsigned char)(1 << en_bit);       /* shadow enabled    */
            break;
        case SHADOW_RW:                                /* read-write UMB */
            rb1 |= (unsigned char)(1 << rom_bit);      /* ROM disabled (1)  */
            rb1 &= (unsigned char)~(1 << wp_bit);      /* writable (0)      */
            en  |= (unsigned char)(1 << en_bit);       /* shadow enabled    */
            break;
        default:
            return HAL_ERR_PARAM;
    }

    legacy_write_default(NEAT_RB1_ROMCFG, rb1);
    legacy_write_default(en_reg, en);
    return HAL_OK;
}

static int neat_shadow_get(int region)
{
    int rom_bit, wp_bit, en_bit;
    unsigned char en_reg, rb1, en;

    if (region < 0 || region >= NEAT_SHADOW_REGIONS) return HAL_ERR_PARAM;
    neat_shadow_bits(region, &rom_bit, &wp_bit, &en_reg, &en_bit);

    rb1 = legacy_read_default(NEAT_RB1_ROMCFG);
    en  = legacy_read_default(en_reg);

    /* RAM only if shadow enabled AND ROM disabled (ROMCS priority). */
    if (!(en & (1 << en_bit)) || !(rb1 & (1 << rom_bit)))
        return SHADOW_DISABLED;
    return (rb1 & (1 << wp_bit)) ? SHADOW_RO : SHADOW_RW;
}

const chipset_ops_t ops_ct_neat = {
    /* Identity */
    .name = "C&T CS8221 (NEAT)",
    .vendor = "C&T",
    .tier = "B",
    .score_x10 = 72,

    /* Detection */
    .probe = neat_probe,

    /* Cache (N/A for 286) */
    .cache_get = neat_cache_get,
    .cache_set = hal_stub_unsupported_i,
    .cache_flush = generic_invd_flush,
    .is_writeback = 0,

    /* NC regions (none) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM (controllable: 16 x 16K blocks C0000-FFFFF) */
    .shadow_regions = NEAT_SHADOW_REGIONS,
    .shadow_get = neat_shadow_get,
    .shadow_set = neat_shadow_set,

    /* A20 gate - NEAT has direct control */
    .a20_get = neat_a20_get,
    .a20_set = neat_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*
 * NEAT-386 (CS8230) - 386SX variant.
 *
 * The old probe used wrong-lore registers (0x1D control + 0x1E "386SX
 * indicator"), both outside NEAT's 0x60-0x6F window, so it read 0xFF and never
 * matched on real hardware anyway. CS8230 shares the CS8221 register model, and
 * the 286-vs-386SX distinction can't be made non-destructively from the index/
 * data ports (the only port-visible difference is RB2's writable-bit mask, and
 * probing it means writing the memory-enable register on a live system).
 *
 * Until a verified, non-destructive 386SX signature exists, fall through: the
 * shared neat_probe() (registered next) detects the NEAT family and its ops
 * work for both. A 386SX board is therefore reported as CS8221 - family and
 * behaviour correct, only the SX label lost. (Previously: detected as neither.)
 */
static int neat386_probe(void)
{
    return 0;
}

const chipset_ops_t ops_ct_neat386 = {
    /* Identity */
    .name = "C&T CS8230 (NEAT-386)",
    .vendor = "C&T",
    .tier = "I",
    .score_x10 = 35,

    /* Detection - probe before NEAT to catch 386 variant first */
    .probe = neat386_probe,

    /* Cache (386SX typically no L2) */
    .cache_get = neat_cache_get,
    .cache_set = hal_stub_unsupported_i,
    .cache_flush = generic_invd_flush,
    .is_writeback = 0,

    /* NC regions (none) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM (info-only) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate */
    .a20_get = neat_a20_get,
    .a20_set = neat_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * HEADLAND TECHNOLOGY CHIPSETS
 *
 * HT12: 286/386 chipset with basic cache control
 * HT18: 386SX variant
 * HT101/HT102: Earlier 286 variants
 *
 * Register layout:
 *   0x10: Cache/DRAM control (bit 0 = cache enable on HT12)
 *   0x12-0x13: Shadow RAM control
 *============================================================================*/

#define HEADLAND_CACHE_REG      0x10
#define HEADLAND_CACHE_ENABLE   0x01

/*
 * Probe for Headland HT12
 * HT12 has characteristic ID at register 0x17
 */
static int headland_ht12_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    /* Check register 0x17 for HT12 ID pattern */
    id = legacy_read_default(0x17);

    /* HT12: bits 7:4 = 0001b (0x1x pattern) */
    return ((id & 0xF0) == 0x10) ? 1 : 0;
}

/*
 * Headland cache control
 */
static int headland_cache_get(void)
{
    unsigned char val = legacy_read_default(HEADLAND_CACHE_REG);
    return (val & HEADLAND_CACHE_ENABLE) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int headland_cache_set(int enable)
{
    unsigned char val = legacy_read_default(HEADLAND_CACHE_REG);

    if (enable) {
        val |= HEADLAND_CACHE_ENABLE;
    } else {
        val &= ~HEADLAND_CACHE_ENABLE;
    }

    legacy_write_default(HEADLAND_CACHE_REG, val);
    return HAL_OK;
}

const chipset_ops_t ops_headland_ht12 = {
    /* Identity */
    .name = "Headland HT12",
    .vendor = "Headland",
    .tier = "B",
    .score_x10 = 72,

    /* Detection */
    .probe = headland_ht12_probe,

    /* Cache control */
    .cache_get = headland_cache_get,
    .cache_set = headland_cache_set,
    .cache_flush = generic_invd_flush,
    .is_writeback = 0,

    /* NC regions (steering-based, not standard range) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM (info-only) */
    .shadow_regions = 0,  /* shadow ops are stubs; report 0 (was 4 phantom) */
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate - use Port 92 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*
 * Headland HT18 (386SX variant)
 */
static int headland_ht18_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    /* Check register 0x17 for HT18 ID pattern */
    id = legacy_read_default(0x17);

    /* HT18: bits 7:4 = 0011b (0x3x pattern) */
    return ((id & 0xF0) == 0x30) ? 1 : 0;
}

/*
 * HT18 cache - typically external, info-only
 */
static int headland_ht18_cache_get(void)
{
    return CACHE_ENABLED;  /* Assume enabled */
}

const chipset_ops_t ops_headland_ht18 = {
    /* Identity */
    .name = "Headland HT18",
    .vendor = "Headland",
    .tier = "I",
    .score_x10 = 35,

    /* Detection */
    .probe = headland_ht18_probe,

    /* Cache (info-only) */
    .cache_get = headland_ht18_cache_get,
    .cache_set = hal_stub_unsupported_i,
    .cache_flush = generic_invd_flush,
    .is_writeback = 0,

    /* NC regions (none) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM (info-only) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * VLSI TECHNOLOGY CHIPSETS
 *
 * VL82C100/101/102: 286 chipsets
 * VL82C320: 386SX variant
 *
 * VLSI uses Index FFh as a chipset identifier.
 *============================================================================*/

#define VLSI_ID_REG         0xFF
#define VLSI_CACHE_REG      0x12
#define VLSI_CACHE_ENABLE   0x01

/*
 * Probe for VLSI chipsets
 */
static int vlsi_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    /* VLSI uses register 0xFF as identifier */
    id = legacy_read_default(VLSI_ID_REG);

    /* VLSI pattern: 0xVx where V indicates VLSI */
    return ((id & 0xF0) == 0xA0 || (id & 0xF0) == 0xB0) ? 1 : 0;
}

/*
 * VLSI cache state
 */
static int vlsi_cache_get(void)
{
    unsigned char val = legacy_read_default(VLSI_CACHE_REG);
    return (val & VLSI_CACHE_ENABLE) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int vlsi_cache_set(int enable)
{
    unsigned char val = legacy_read_default(VLSI_CACHE_REG);

    if (enable) {
        val |= VLSI_CACHE_ENABLE;
    } else {
        val &= ~VLSI_CACHE_ENABLE;
    }

    legacy_write_default(VLSI_CACHE_REG, val);
    return HAL_OK;
}

const chipset_ops_t ops_vlsi_vl82c100 = {
    /* Identity */
    .name = "VLSI VL82C100",
    .vendor = "VLSI",
    .tier = "I",
    .score_x10 = 30,

    /* Detection */
    .probe = vlsi_probe,

    /* Cache */
    .cache_get = vlsi_cache_get,
    .cache_set = vlsi_cache_set,
    .cache_flush = generic_invd_flush,
    .is_writeback = 0,

    /* NC regions (none) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM (info-only) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*
 * VLSI VL82C320 (386SX)
 */
static int vlsi_320_probe(void)
{
    unsigned char id, id2;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    id = legacy_read_default(VLSI_ID_REG);

    if ((id & 0xF0) == 0xA0 || (id & 0xF0) == 0xB0) {
        /* Check for 386 indicator in another register */
        id2 = legacy_read_default(0x00);
        if (id2 & 0x40) {  /* 386SX indicator */
            return 1;
        }
    }

    return 0;
}

const chipset_ops_t ops_vlsi_vl82c320 = {
    /* Identity */
    .name = "VLSI VL82C320",
    .vendor = "VLSI",
    .tier = "I",
    .score_x10 = 35,

    /* Detection */
    .probe = vlsi_320_probe,

    /* Cache */
    .cache_get = vlsi_cache_get,
    .cache_set = vlsi_cache_set,
    .cache_flush = generic_invd_flush,
    .is_writeback = 0,

    /* NC regions (none) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM (info-only) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * VIA VT82C310 - Early VIA 386/486 Chipset
 *
 * Uses 0x22/0x23 index/data ports.
 * Cache control at register 0x10, bit 4.
 * 2 NC regions with 64KB granularity.
 *============================================================================*/

#define VIA310_CACHE_REG    0x10
#define VIA310_CACHE_BIT    0x10    /* Bit 4 */

/*
 * Probe for VIA VT82C310
 */
static int via310_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    /* VIA uses register 0x15 as ID indicator */
    id = legacy_read_default(0x15);

    /* VIA 310 pattern: 0x3x */
    return ((id & 0xF0) == 0x30) ? 1 : 0;
}

static int via310_cache_get(void)
{
    unsigned char val = legacy_read_default(VIA310_CACHE_REG);
    return (val & VIA310_CACHE_BIT) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int via310_cache_set(int enable)
{
    unsigned char val = legacy_read_default(VIA310_CACHE_REG);

    if (enable) {
        val |= VIA310_CACHE_BIT;
    } else {
        val &= ~VIA310_CACHE_BIT;
    }

    legacy_write_default(VIA310_CACHE_REG, val);
    return HAL_OK;
}

const chipset_ops_t ops_via_vt82c310 = {
    /* Identity */
    .name = "VIA VT82C310",
    .vendor = "VIA",
    .tier = "B",
    .score_x10 = 75,

    /* Detection */
    .probe = via310_probe,

    /* Cache */
    .cache_get = via310_cache_get,
    .cache_set = via310_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 0,

    /* NC regions: the VT82C310 hardware has 2, but the ops below are
       unimplemented stubs. Report 0 so the UI does not present phantom
       editable regions that silently do nothing.
       TODO: implement real VT82C310 NC support, then restore nc_count = 2. */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 64,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * VLSI VL82C311 - VLSI 286/386 Chipset Variant
 *
 * Similar to VL82C100 series but different ID pattern.
 *============================================================================*/

static int vlsi_311_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    id = legacy_read_default(VLSI_ID_REG);

    /* VL82C311: specific ID pattern 0xBx */
    return ((id & 0xF0) == 0xB0) ? 1 : 0;
}

const chipset_ops_t ops_vlsi_vl82c311 = {
    /* Identity */
    .name = "VLSI VL82C311",
    .vendor = "VLSI",
    .tier = "B",
    .score_x10 = 70,

    /* Detection */
    .probe = vlsi_311_probe,

    /* Cache */
    .cache_get = vlsi_cache_get,
    .cache_set = vlsi_cache_set,
    .cache_flush = generic_invd_flush,
    .is_writeback = 0,

    /* NC regions: unimplemented stub; report 0 (no phantom editable regions). */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * C&T 82C301 (PEAK) / 82C235 (SCAT) - Early C&T Chipsets
 *
 * PEAK = Performance Enhanced AT Chipset (386)
 * SCAT = Single Chip AT Controller (286)
 *
 * Both use similar register layouts:
 *   Cache at register 0x14, bit 0
 *   ID at register 0x17: 0x21 = PEAK, 0x11 = SCAT
 *   NC boundary-based (not range-based)
 *============================================================================*/

#define CT_CACHE_REG        0x14
#define CT_CACHE_ENABLE     0x01

/*
 * Probe for C&T PEAK (82C301)
 */
static int ct_peak_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    id = legacy_read_default(0x17);

    /* PEAK: ID == 0x21 */
    return (id == 0x21) ? 1 : 0;
}

static int ct_cache_get(void)
{
    unsigned char val = legacy_read_default(CT_CACHE_REG);
    return (val & CT_CACHE_ENABLE) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int ct_cache_set(int enable)
{
    unsigned char val = legacy_read_default(CT_CACHE_REG);

    if (enable) {
        val |= CT_CACHE_ENABLE;
    } else {
        val &= ~CT_CACHE_ENABLE;
    }

    legacy_write_default(CT_CACHE_REG, val);
    return HAL_OK;
}

const chipset_ops_t ops_ct_peak = {
    /* Identity */
    .name = "C&T 82C301 (PEAK)",
    .vendor = "C&T",
    .tier = "C",
    .score_x10 = 60,

    /* Detection */
    .probe = ct_peak_probe,

    /* Cache */
    .cache_get = ct_cache_get,
    .cache_set = ct_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 0,

    /* NC regions: boundary-based, unimplemented stub; report 0. */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*
 * Probe for C&T SCAT (82C235)
 */
static int ct_scat_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    id = legacy_read_default(0x17);

    /* SCAT: ID == 0x11 */
    return (id == 0x11) ? 1 : 0;
}

const chipset_ops_t ops_ct_scat = {
    /* Identity */
    .name = "C&T 82C235 (SCAT)",
    .vendor = "C&T",
    .tier = "C",
    .score_x10 = 55,

    /* Detection */
    .probe = ct_scat_probe,

    /* Cache */
    .cache_get = ct_cache_get,
    .cache_set = ct_cache_set,
    .cache_flush = generic_invd_flush,
    .is_writeback = 0,

    /* NC regions: boundary-based, unimplemented stub; report 0. */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * ALi M1209 (Finis) - Early ALi 386 Chipset
 *
 * ID at register 0x17: 0x12
 * NC boundary-based.
 *============================================================================*/

static int ali_finis_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    id = legacy_read_default(0x17);

    /* ALi Finis: ID == 0x12 */
    return (id == 0x12) ? 1 : 0;
}

const chipset_ops_t ops_ali_finis = {
    /* Identity */
    .name = "ALi M1209 (Finis)",
    .vendor = "ALi",
    .tier = "C",
    .score_x10 = 55,

    /* Detection */
    .probe = ali_finis_probe,

    /* Cache */
    .cache_get = ct_cache_get,  /* Similar to C&T */
    .cache_set = ct_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 0,

    /* NC regions: boundary-based, unimplemented stub; report 0. */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * ALi M1217 - Early ALi Chipset (Info-only)
 *
 * ID at register 0x17: 0x17
 *============================================================================*/

static int ali_m1217_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    id = legacy_read_default(0x17);

    /* ALi M1217: ID == 0x17 */
    return (id == 0x17) ? 1 : 0;
}

const chipset_ops_t ops_ali_m1217 = {
    /* Identity */
    .name = "ALi M1217",
    .vendor = "ALi",
    .tier = "I",
    .score_x10 = 30,

    /* Detection */
    .probe = ali_m1217_probe,

    /* Cache (info-only) */
    .cache_get = neat_cache_get,
    .cache_set = hal_stub_unsupported_i,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 0,

    /* NC regions (none) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * HEADLAND HT101/HT102 - Early Headland 286 Chipsets (Info-only)
 *
 * Predecessor to HT12 series.
 *============================================================================*/

static int headland_ht101_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    id = legacy_read_default(0x17);

    /* HT101: bits 7:4 = 0000b (0x0x pattern) */
    return ((id & 0xF0) == 0x00 && id != 0x00) ? 1 : 0;
}

const chipset_ops_t ops_headland_ht101 = {
    /* Identity */
    .name = "Headland HT101",
    .vendor = "Headland",
    .tier = "I",
    .score_x10 = 30,

    /* Detection */
    .probe = headland_ht101_probe,

    /* Cache (info-only) */
    .cache_get = neat_cache_get,
    .cache_set = hal_stub_unsupported_i,
    .cache_flush = generic_invd_flush,
    .is_writeback = 0,

    /* NC regions (none) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

static int headland_ht102_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    id = legacy_read_default(0x17);

    /* HT102: bits 7:4 = 0010b (0x2x pattern) */
    return ((id & 0xF0) == 0x20) ? 1 : 0;
}

const chipset_ops_t ops_headland_ht102 = {
    /* Identity */
    .name = "Headland HT102",
    .vendor = "Headland",
    .tier = "I",
    .score_x10 = 30,

    /* Detection */
    .probe = headland_ht102_probe,

    /* Cache (info-only) */
    .cache_get = neat_cache_get,
    .cache_set = hal_stub_unsupported_i,
    .cache_flush = generic_invd_flush,
    .is_writeback = 0,

    /* NC regions (none) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 32,
    .reg_base = 0x00,
    .reg_read = legacy_reg_read,
    .reg_write = legacy_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * END OF CK_LEGAC.C
 *============================================================================*/
