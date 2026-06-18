/*============================================================================
 * CK_OPTI.C - OPTi Chipset HAL Implementations
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 10:45:00 EST
 *
 * Supported Chipsets:
 * - OPTi 82C391 (SYSC)      - 486 ISA, Write-Back, 2 NC regions, 8KB min
 * - OPTi 82C381 (Symphony)  - 486 ISA, Write-Through, 2 NC regions, 512KB min
 * - OPTi 82C596/597 (Viper) - 486 VLB/PCI, Write-Back, 4 NC regions, 8KB min
 * - OPTi 82C212             - 386SX-era, Info-only
 * - OPTi 82C682 (EISA 486WB)   - EISA, Info-only
 * - OPTi 82C683 (EISA 486AWB)  - EISA, Info-only
 * - OPTi 82C691/696 (Hunter)   - EISA, Info-only
 * - OPTi 82C693/6/7 (Pentium)  - EISA, Info-only
 *
 * OPTi Quirks:
 * - Uses Index 0x22 / Data 0x24 (NOT 0x23 like SiS!)
 * - Cache control at register 0x20, bit 0
 * - NC regions at 0x52-0x59 with OPTi-specific encoding
 *============================================================================*/

#include "CK_HAL.H"

/*============================================================================
 * EXTERNAL DECLARATIONS
 *============================================================================*/

/* From CK_IO.C */
extern unsigned char legacy_read_22_24(unsigned char reg);
extern void legacy_write_22_24(unsigned char reg, unsigned char val);
extern int legacy_port_valid(unsigned int index_port, unsigned int data_port);

/* From CK_HAL.C */
extern int generic_wbinvd_flush(void);
extern int generic_port92_a20_get(void);
extern int generic_port92_a20_set(int enable);
extern int hal_stub_unsupported_i(int x);
extern int hal_stub_unsupported_ii(int x, int y);
extern int hal_stub_nc_read(int idx, nc_region_t *r);

/*============================================================================
 * OPTi REGISTER DEFINITIONS
 *
 * Port mapping:
 *   Index port: 0x22
 *   Data port:  0x24 (NOT 0x23!)
 *
 * Key registers:
 *   0x20: CPU/Cache Control
 *         Bit 0: L2 Cache Enable (1=enabled)
 *         Bits 7:5: Chip ID (001=381, 010=391, 110=Viper)
 *   0x52-0x53: NC Region 0
 *   0x54-0x55: NC Region 1
 *   0x56-0x57: NC Region 2 (Viper only)
 *   0x58-0x59: NC Region 3 (Viper only)
 *
 * NC Region encoding (per region, 2 bytes):
 *   Byte 0 (base): Bits 7:0 = Address bits A23:A16 (64KB units)
 *   Byte 1 (size): Bits 7:4 = Size code (0=disabled, 1=8K, 2=16K, 3=32K,
 *                                        4=64K, 5=128K, 6=256K, 7=512K)
 *                  Bits 3:0 = Address bits A27:A24 (high nibble of base)
 *
 * Size calculation: 8KB << (size_code - 1)
 * Base calculation: ((high_nibble << 12) | (low_byte << 4)) = KB
 *============================================================================*/

#define OPTI_INDEX_PORT     0x22
#define OPTI_DATA_PORT      0x24

/* Register indices */
#define OPTI_REG_CACHE      0x20    /* CPU/Cache control */
#define OPTI_REG_NC0_BASE   0x52    /* NC Region 0 base */
#define OPTI_REG_NC0_SIZE   0x53    /* NC Region 0 size/hi-addr */
#define OPTI_REG_NC1_BASE   0x54    /* NC Region 1 base */
#define OPTI_REG_NC1_SIZE   0x55    /* NC Region 1 size/hi-addr */
#define OPTI_REG_NC2_BASE   0x56    /* NC Region 2 base (Viper) */
#define OPTI_REG_NC2_SIZE   0x57    /* NC Region 2 size (Viper) */
#define OPTI_REG_NC3_BASE   0x58    /* NC Region 3 base (Viper) */
#define OPTI_REG_NC3_SIZE   0x59    /* NC Region 3 size (Viper) */

/* Bit definitions */
#define OPTI_CACHE_ENABLE   0x01    /* Bit 0 of reg 0x20 */

/* Chip ID patterns at reg 0x20 bits 7:5 */
#define OPTI_ID_381         0x20    /* 001b << 5 */
#define OPTI_ID_391         0x40    /* 010b << 5 */
#define OPTI_ID_VIPER       0x60    /* 011b << 5 (0x6x pattern) */

/*============================================================================
 * OPTi SHARED HELPER FUNCTIONS
 *============================================================================*/

/*
 * Read OPTi chipset register
 */
static unsigned char opti_read_reg(unsigned char reg)
{
    return legacy_read_22_24(reg);
}

/*
 * Write OPTi chipset register
 */
static void opti_write_reg(unsigned char reg, unsigned char val)
{
    legacy_write_22_24(reg, val);
}

/*
 * Get cache state for OPTi chipsets
 * Register 0x20, bit 0 = L2 cache enable
 */
static int opti_cache_get(void)
{
    unsigned char val = opti_read_reg(OPTI_REG_CACHE);
    return (val & OPTI_CACHE_ENABLE) ? CACHE_ENABLED : CACHE_DISABLED;
}

/*
 * Set cache state for OPTi chipsets
 */
static int opti_cache_set(int enable)
{
    unsigned char val = opti_read_reg(OPTI_REG_CACHE);

    if (enable) {
        val |= OPTI_CACHE_ENABLE;
    } else {
        val &= ~OPTI_CACHE_ENABLE;
    }

    opti_write_reg(OPTI_REG_CACHE, val);
    return HAL_OK;
}

/*
 * Read OPTi NC region configuration
 *
 * NC region encoding:
 *   Byte 0: Base address A23:A16 (in 64KB units, stored as lower byte)
 *   Byte 1: Bits 7:4 = size code, Bits 3:0 = Base A27:A24
 *
 * Size: 8KB << (size_code - 1), where 0 = disabled
 * Base: lo byte = A23:A16, size-reg hi nibble = A27:A24
 */

/*
 * OPTi NC base unit. Per CLAUDE.md / the OPTi datasheet the base field is
 * A23:A16 = 64KB units, i.e. base_kb >> 6 (and the A27:A24 nibble is
 * base_kb >> 14). The original code used >>4 / >>12 (16KB units), which scales
 * the base 4x too small. Centralized here so it is one line to flip.
 * !!! TODO: VERIFY on 86Box/PCem against the OPTi 82C391/82C596 datasheet
 * !!! before trusting on real hardware (Eteq uses the same encoding).
 */
#define OPTI_NC_BASE_SHIFT  6           /* 64KB units (A16) */
#define OPTI_NC_BASE_MAXU   0xFFFUL     /* 12-bit base field (A27:A16) */

static int opti_nc_read(int idx, nc_region_t *r, int max_regions)
{
    unsigned char base_reg, size_reg;
    unsigned char size_code;

    if (idx < 0 || idx >= max_regions || !r) {
        return HAL_ERR_PARAM;
    }

    /* Calculate register addresses for this region */
    base_reg = OPTI_REG_NC0_BASE + (idx * 2);
    size_reg = OPTI_REG_NC0_SIZE + (idx * 2);

    r->base_kb = 0;
    r->size_kb = 0;
    r->active = 0;

    /* Read the registers */
    {
        unsigned char lo = opti_read_reg(base_reg);
        unsigned char hi = opti_read_reg(size_reg);

        size_code = hi >> 4;
        if (size_code > 7)              /* codes 8-15 are reserved (OA-C3) */
            size_code = 7;              /* clamp so a junk reg can't report */
                                        /* a bogus multi-MB region */
        if (size_code != 0) {
            r->active = 1;
            /* Base: A27:A24 in hi nibble, A23:A16 in lo byte. */
            r->base_kb =
                ((unsigned long)(hi & 0x0F) << (OPTI_NC_BASE_SHIFT + 8)) |
                ((unsigned long)lo << OPTI_NC_BASE_SHIFT);
            /* Size: 8KB << (code - 1) */
            r->size_kb = 8UL << (size_code - 1);
        }
    }

    return HAL_OK;
}

/*
 * Write OPTi NC region configuration
 */
static int opti_nc_write(int idx, unsigned long base_kb, unsigned long size_kb, int max_regions)
{
    unsigned char base_reg_addr, size_reg_addr;
    unsigned char base_val, size_val;
    unsigned char size_code;

    if (idx < 0 || idx >= max_regions) {
        return HAL_ERR_PARAM;
    }

    /* Calculate register addresses */
    base_reg_addr = OPTI_REG_NC0_BASE + (idx * 2);
    size_reg_addr = OPTI_REG_NC0_SIZE + (idx * 2);

    /* size 0 = disable (size code 0) */
    if (size_kb == 0) {
        opti_write_reg(size_reg_addr, 0x00);
        return HAL_OK;
    }

    /* Align base to the 64KB base unit and REJECT (don't truncate) a base
       that overflows the 12-bit base field - a truncated base would fence the
       wrong physical address (OA-H1). */
    base_kb = (base_kb + 63) & ~63UL;
    if ((base_kb >> OPTI_NC_BASE_SHIFT) > OPTI_NC_BASE_MAXU) {
        return HAL_ERR_PARAM;
    }

    /* Size code: code N => 8KB << (N-1), capped at code 7 = 512KB/region.
       Reject larger rather than silently using 512KB. */
    if (size_kb <= 8) size_code = 1;
    else if (size_kb <= 16) size_code = 2;
    else if (size_kb <= 32) size_code = 3;
    else if (size_kb <= 64) size_code = 4;
    else if (size_kb <= 128) size_code = 5;
    else if (size_kb <= 256) size_code = 6;
    else if (size_kb <= 512) size_code = 7;
    else return HAL_ERR_PARAM;

    /* Encode base: lo byte = A23:A16, size-reg hi nibble = A27:A24. */
    base_val = (unsigned char)((base_kb >> OPTI_NC_BASE_SHIFT) & 0xFF);
    size_val = (unsigned char)((size_code << 4) |
               ((base_kb >> (OPTI_NC_BASE_SHIFT + 8)) & 0x0F));

    /* Write to registers */
    opti_write_reg(base_reg_addr, base_val);
    opti_write_reg(size_reg_addr, size_val);

    return HAL_OK;
}

/*
 * Clear OPTi NC region
 */
static int opti_nc_clear(int idx, int max_regions)
{
    if (idx < 0 || idx >= max_regions) {
        return HAL_ERR_PARAM;
    }

    /* Clear by writing 0 to both registers */
    opti_write_reg(OPTI_REG_NC0_BASE + (idx * 2), 0x00);
    opti_write_reg(OPTI_REG_NC0_SIZE + (idx * 2), 0x00);

    return HAL_OK;
}

/*
 * Read OPTi register for F4 dump screen
 */
static int opti_reg_read(int reg)
{
    if (reg < 0 || reg > 0xFF) {
        return -1;
    }
    return opti_read_reg((unsigned char)reg);
}

/*
 * Write OPTi register (for expert mode)
 */
static int opti_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 0xFF || val < 0 || val > 0xFF) {
        return HAL_ERR_PARAM;
    }
    opti_write_reg((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

/*============================================================================
 * OPTi 82C391 (SYSC) - 486 ISA Chipset
 *
 * Features:
 * - Write-back L2 cache support
 * - 2 NC regions, 8KB minimum granularity
 * - Uses 0x22/0x24 ports
 *============================================================================*/

static int opti391_probe(void)
{
    unsigned char id;

    /* Check if 0x22/0x24 port pair is responsive */
    if (!legacy_port_valid(OPTI_INDEX_PORT, OPTI_DATA_PORT)) {
        return 0;
    }

    /* Read chip ID from register 0x20 */
    id = opti_read_reg(OPTI_REG_CACHE);

    /* OPTi 391: bits 7:5 = 010b (0x40) */
    return ((id & 0xE0) == OPTI_ID_391) ? 1 : 0;
}

static int opti391_nc_read(int idx, nc_region_t *r)
{
    return opti_nc_read(idx, r, 2);
}

static int opti391_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    return opti_nc_write(idx, base_kb, size_kb, 2);
}

static int opti391_nc_clear(int idx)
{
    return opti_nc_clear(idx, 2);
}

const chipset_ops_t ops_opti_391 = {
    /* Identity */
    .name = "OPTi 82C391 (SYSC)",
    .vendor = "OPTi",
    .tier = "S",
    .score_x10 = 92,

    /* Detection */
    .probe = opti391_probe,

    /* Cache control */
    .cache_get = opti_cache_get,
    .cache_set = opti_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions */
    .nc_count = 2,
    .nc_granularity = 8,
    .nc_max_kb = 512,
    .nc_read = opti391_nc_read,
    .nc_write = opti391_nc_write,
    .nc_clear = opti391_nc_clear,

    /* Shadow RAM (info-only) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 128,
    .reg_base = 0x00,
    .reg_read = opti_reg_read,
    .reg_write = opti_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = OPTI_INDEX_PORT,
    .data_port = OPTI_DATA_PORT
};

/*============================================================================
 * OPTi 82C381 (Symphony) - 486 ISA Chipset
 *
 * Features:
 * - Write-through cache only (no WB)
 * - 2 NC regions, 512KB minimum granularity (larger minimum than 391)
 * - Uses 0x22/0x24 ports
 *============================================================================*/

static int opti381_probe(void)
{
    unsigned char id;

    /* Check if 0x22/0x24 port pair is responsive */
    if (!legacy_port_valid(OPTI_INDEX_PORT, OPTI_DATA_PORT)) {
        return 0;
    }

    /* Read chip ID from register 0x20 */
    id = opti_read_reg(OPTI_REG_CACHE);

    /* OPTi 381: bits 7:5 = 001b (0x20) */
    return ((id & 0xE0) == OPTI_ID_381) ? 1 : 0;
}

static int opti381_nc_read(int idx, nc_region_t *r)
{
    return opti_nc_read(idx, r, 2);
}

static int opti381_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    /* OPTi 381 has 512KB minimum granularity */
    if (size_kb != 0 && size_kb < 512) {
        size_kb = 512;  /* Round up to minimum */
    }
    return opti_nc_write(idx, base_kb, size_kb, 2);
}

static int opti381_nc_clear(int idx)
{
    return opti_nc_clear(idx, 2);
}

const chipset_ops_t ops_opti_381 = {
    /* Identity */
    .name = "OPTi 82C381 (Symphony)",
    .vendor = "OPTi",
    .tier = "B",
    .score_x10 = 78,

    /* Detection */
    .probe = opti381_probe,

    /* Cache control */
    .cache_get = opti_cache_get,
    .cache_set = opti_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 0,  /* Write-through only */

    /* NC regions */
    .nc_count = 2,
    .nc_granularity = 512,  /* Larger minimum */
    .nc_max_kb = 512,
    .nc_read = opti381_nc_read,
    .nc_write = opti381_nc_write,
    .nc_clear = opti381_nc_clear,

    /* Shadow RAM (info-only) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 128,
    .reg_base = 0x00,
    .reg_read = opti_reg_read,
    .reg_write = opti_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = OPTI_INDEX_PORT,
    .data_port = OPTI_DATA_PORT
};

/*============================================================================
 * OPTi 82C596/597 (Viper) - 486 VLB/PCI Chipset
 *
 * Features:
 * - Write-back L2 cache support
 * - 4 NC regions (vs 2 for earlier OPTi)
 * - 8KB minimum granularity
 * - Uses 0x22/0x24 ports (legacy detection, not PCI config)
 * - ID pattern: 0x6x at register 0x20 (bits 7:5 = 011b)
 *============================================================================*/

static int opti_viper_probe(void)
{
    unsigned char id;

    /* Check if 0x22/0x24 port pair is responsive */
    if (!legacy_port_valid(OPTI_INDEX_PORT, OPTI_DATA_PORT)) {
        return 0;
    }

    /* Read chip ID from register 0x20 */
    id = opti_read_reg(OPTI_REG_CACHE);

    /* OPTi Viper: (id & 0xF0) == 0x60 pattern */
    return ((id & 0xF0) == OPTI_ID_VIPER) ? 1 : 0;
}

static int opti_viper_nc_read(int idx, nc_region_t *r)
{
    return opti_nc_read(idx, r, 4);  /* 4 regions */
}

static int opti_viper_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    return opti_nc_write(idx, base_kb, size_kb, 4);  /* 4 regions */
}

static int opti_viper_nc_clear(int idx)
{
    return opti_nc_clear(idx, 4);  /* 4 regions */
}

const chipset_ops_t ops_opti_viper = {
    /* Identity */
    .name = "OPTi 82C596/597 (Viper)",
    .vendor = "OPTi",
    .tier = "A",
    .score_x10 = 85,

    /* Detection */
    .probe = opti_viper_probe,

    /* Cache control */
    .cache_get = opti_cache_get,
    .cache_set = opti_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions - 4 regions on Viper */
    .nc_count = 4,
    .nc_granularity = 8,
    .nc_max_kb = 512,
    .nc_read = opti_viper_nc_read,
    .nc_write = opti_viper_nc_write,
    .nc_clear = opti_viper_nc_clear,

    /* Shadow RAM (info-only) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 128,
    .reg_base = 0x00,
    .reg_read = opti_reg_read,
    .reg_write = opti_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = OPTI_INDEX_PORT,
    .data_port = OPTI_DATA_PORT
};

/*============================================================================
 * OPTi 82C212 - 386SX-era Chipset
 *
 * Early OPTi chipset for 386SX systems.
 * Info-only: Limited documentation, A20 control available.
 * Uses 0x22/0x24 ports, ID pattern 0x1x at register 0x20.
 *============================================================================*/

static int opti212_probe(void)
{
    unsigned char id;

    /* Check if 0x22/0x24 port pair is responsive */
    if (!legacy_port_valid(OPTI_INDEX_PORT, OPTI_DATA_PORT)) {
        return 0;
    }

    /* Read chip ID from register 0x20 */
    id = opti_read_reg(OPTI_REG_CACHE);

    /* OPTi 212: (id & 0xF0) == 0x10 */
    return ((id & 0xF0) == 0x10) ? 1 : 0;
}

/* OPTi 212 A20 at reg 0x20 bit 0 */
static int opti212_a20_get(void)
{
    unsigned char val = opti_read_reg(OPTI_REG_CACHE);
    return (val & 0x01) ? 1 : 0;
}

static int opti212_a20_set(int enable)
{
    unsigned char val = opti_read_reg(OPTI_REG_CACHE);

    if (enable) {
        val |= 0x01;
    } else {
        val &= ~0x01;
    }

    opti_write_reg(OPTI_REG_CACHE, val);
    return HAL_OK;
}

const chipset_ops_t ops_opti_212 = {
    /* Identity */
    .name = "OPTi 82C212",
    .vendor = "OPTi",
    .tier = "I",
    .score_x10 = 30,

    /* Detection */
    .probe = opti212_probe,

    /* Cache (info-only) */
    .cache_get = opti_cache_get,
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

    /* Shadow RAM (none) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate (via chipset) */
    .a20_get = opti212_a20_get,
    .a20_set = opti212_a20_set,

    /* Register access */
    .reg_count = 128,
    .reg_base = 0x00,
    .reg_read = opti_reg_read,
    .reg_write = opti_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = OPTI_INDEX_PORT,
    .data_port = OPTI_DATA_PORT
};

/*============================================================================
 * OPTi 82C682 (EISA 486WB) - EISA Chipset
 *
 * EISA-bus OPTi chipset with write-back cache support.
 * Info-only: Limited NC region support in this implementation.
 * ID: 0x60 at register 0x20.
 *============================================================================*/

static int opti682_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(OPTI_INDEX_PORT, OPTI_DATA_PORT)) {
        return 0;
    }

    id = opti_read_reg(OPTI_REG_CACHE);

    /* OPTi 682: ID == 0x60 (exactly) */
    return (id == 0x60) ? 1 : 0;
}

const chipset_ops_t ops_opti_682 = {
    /* Identity */
    .name = "OPTi 82C682 (EISA 486WB)",
    .vendor = "OPTi",
    .tier = "I",
    .score_x10 = 35,

    /* Detection */
    .probe = opti682_probe,

    /* Cache */
    .cache_get = opti_cache_get,
    .cache_set = opti_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (none in info-only mode) */
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
    .reg_count = 128,
    .reg_base = 0x00,
    .reg_read = opti_reg_read,
    .reg_write = opti_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = OPTI_INDEX_PORT,
    .data_port = OPTI_DATA_PORT
};

/*============================================================================
 * OPTi 82C683 (EISA 486AWB) - EISA Chipset
 *
 * Enhanced EISA-bus OPTi chipset.
 * ID: 0x61 at register 0x20.
 *============================================================================*/

static int opti683_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(OPTI_INDEX_PORT, OPTI_DATA_PORT)) {
        return 0;
    }

    id = opti_read_reg(OPTI_REG_CACHE);

    /* OPTi 683: ID == 0x61 */
    return (id == 0x61) ? 1 : 0;
}

const chipset_ops_t ops_opti_683 = {
    /* Identity */
    .name = "OPTi 82C683 (EISA 486AWB)",
    .vendor = "OPTi",
    .tier = "I",
    .score_x10 = 35,

    /* Detection */
    .probe = opti683_probe,

    /* Cache */
    .cache_get = opti_cache_get,
    .cache_set = opti_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

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
    .reg_count = 128,
    .reg_base = 0x00,
    .reg_read = opti_reg_read,
    .reg_write = opti_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = OPTI_INDEX_PORT,
    .data_port = OPTI_DATA_PORT
};

/*============================================================================
 * OPTi 82C691/696 (Hunter) - EISA Chipset
 *
 * Hunter-series EISA chipset.
 * ID: 0x68 at register 0x20.
 *============================================================================*/

static int opti_hunter_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(OPTI_INDEX_PORT, OPTI_DATA_PORT)) {
        return 0;
    }

    id = opti_read_reg(OPTI_REG_CACHE);

    /* OPTi Hunter: ID == 0x68 */
    return (id == 0x68) ? 1 : 0;
}

const chipset_ops_t ops_opti_hunter = {
    /* Identity */
    .name = "OPTi 82C691/696 (Hunter)",
    .vendor = "OPTi",
    .tier = "I",
    .score_x10 = 35,

    /* Detection */
    .probe = opti_hunter_probe,

    /* Cache */
    .cache_get = opti_cache_get,
    .cache_set = opti_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

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
    .reg_count = 128,
    .reg_base = 0x00,
    .reg_read = opti_reg_read,
    .reg_write = opti_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = OPTI_INDEX_PORT,
    .data_port = OPTI_DATA_PORT
};

/*============================================================================
 * OPTi 82C693/6/7 (Pentium EISA) - EISA Chipset
 *
 * Pentium-class EISA chipset with write-back support.
 * ID: 0x6C at register 0x20.
 *============================================================================*/

static int opti_pent_eisa_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(OPTI_INDEX_PORT, OPTI_DATA_PORT)) {
        return 0;
    }

    id = opti_read_reg(OPTI_REG_CACHE);

    /* OPTi Pentium EISA: ID == 0x6C */
    return (id == 0x6C) ? 1 : 0;
}

const chipset_ops_t ops_opti_pent_eisa = {
    /* Identity */
    .name = "OPTi 82C693/6/7 (Pentium EISA)",
    .vendor = "OPTi",
    .tier = "I",
    .score_x10 = 35,

    /* Detection */
    .probe = opti_pent_eisa_probe,

    /* Cache */
    .cache_get = opti_cache_get,
    .cache_set = opti_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

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
    .reg_count = 128,
    .reg_base = 0x00,
    .reg_read = opti_reg_read,
    .reg_write = opti_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = OPTI_INDEX_PORT,
    .data_port = OPTI_DATA_PORT
};

/*============================================================================
 * END OF CK_OPTI.C
 *============================================================================*/
