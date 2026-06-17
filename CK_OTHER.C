/*============================================================================
 * CK_OTHER.C - Miscellaneous 486-Era Chipset HAL Implementations
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 11:15:00 EST
 *
 * Supported Chipsets:
 * - UMC UM82C491     - 486 ISA, WB, 1 NC region, 8KB min
 * - Eteq 82C495WB    - 486 ISA (OPTi clone), WB, 2 NC regions, 8KB min
 * - Faraday FE3600   - 486 ISA, WT, 3 NC regions, 128KB min
 * - MIC MIC9391      - 486 ISA, WB, 1 NC region, 64KB min
 * - Contaq 82C596    - 486 ISA, WT, no NC regions
 * - Forex FRX-386DX  - 386, boundary NC, 1MB max
 * - Suntac ST62C301  - 386/486, shadow-based NC
 * - IBM PS/2 MCA     - Observer-only (hardware snooping)
 *
 * All use legacy 0x22/0x23 index/data ports except Eteq (0x22/0x24).
 *============================================================================*/

#include "CK_HAL.H"

/*============================================================================
 * EXTERNAL DECLARATIONS
 *============================================================================*/

/* From CK_IO.C */
extern unsigned char legacy_read_22_23(unsigned char reg);
extern void legacy_write_22_23(unsigned char reg, unsigned char val);
extern unsigned char legacy_read_22_24(unsigned char reg);
extern void legacy_write_22_24(unsigned char reg, unsigned char val);
extern int legacy_port_valid(unsigned int index_port, unsigned int data_port);

/* From CK_IO.C */
extern unsigned char io_read_byte(unsigned int port);
extern void io_write_byte(unsigned int port, unsigned char val);

/* From CK_HAL.C */
extern int generic_wbinvd_flush(void);
extern int generic_port92_a20_get(void);
extern int generic_port92_a20_set(int enable);
extern int hal_stub_unsupported_i(int x);
extern int hal_stub_unsupported_ii(int x, int y);
extern int hal_stub_unsupported_iull(int x, unsigned long y, unsigned long z);
extern int hal_stub_nc_read(int idx, nc_region_t *r);

/*============================================================================
 * PORT DEFINITIONS
 *============================================================================*/

#define LEGACY_INDEX_PORT   0x22
#define LEGACY_DATA_PORT    0x23
#define OPTI_DATA_PORT      0x24    /* Eteq uses OPTi-style ports */

/*============================================================================
 * UMC UM82C491 - 486 ISA CHIPSET
 *
 * Features:
 * - Write-back L2 cache support
 * - 1 NC region at Index 0x50-0x51
 * - 8KB minimum granularity
 *
 * Register layout:
 *   0x00: Cache control (bit 7 = enable)
 *   0x50: NC base address bits 23:16
 *   0x51: NC size/enable (bit 7=enable, bits 6:4=size code)
 *
 * Size calculation: 8KB << size_code
 *============================================================================*/

#define UMC491_CACHE_REG        0x00
#define UMC491_CACHE_ENABLE     0x80    /* Bit 7 */
#define UMC491_NC_BASE_REG      0x50
#define UMC491_NC_SIZE_REG      0x51

static int umc491_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    /* UMC 491 has characteristic ID at register 0x01 */
    id = legacy_read_22_23(0x01);

    /* UMC pattern: 0x4x or 0x9x */
    return ((id & 0xF0) == 0x40 || (id & 0xF0) == 0x90) ? 1 : 0;
}

static int umc491_cache_get(void)
{
    unsigned char val = legacy_read_22_23(UMC491_CACHE_REG);
    return (val & UMC491_CACHE_ENABLE) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int umc491_cache_set(int enable)
{
    unsigned char val = legacy_read_22_23(UMC491_CACHE_REG);

    if (enable) {
        val |= UMC491_CACHE_ENABLE;
    } else {
        val &= ~UMC491_CACHE_ENABLE;
    }

    legacy_write_22_23(UMC491_CACHE_REG, val);
    return HAL_OK;
}

static int umc491_nc_read(int idx, nc_region_t *r)
{
    unsigned char base_val, size_val;
    unsigned char size_code;

    if (idx != 0 || !r) {
        return HAL_ERR_PARAM;
    }

    r->base_kb = 0;
    r->size_kb = 0;
    r->active = 0;

    base_val = legacy_read_22_23(UMC491_NC_BASE_REG);
    size_val = legacy_read_22_23(UMC491_NC_SIZE_REG);

    if (size_val & 0x80) {  /* Enable bit */
        r->active = 1;
        /* Base: 16KB units (shift by 4 to get KB) */
        r->base_kb = (unsigned long)base_val << 4;
        /* Size: 8KB << size_code */
        size_code = (size_val >> 4) & 0x07;
        r->size_kb = 8UL << size_code;
    }

    return HAL_OK;
}

static int umc491_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    unsigned char base_val, size_val;
    unsigned char size_code;

    if (idx != 0) {
        return HAL_ERR_PARAM;
    }

    if (size_kb == 0) {
        /* Disable region */
        legacy_write_22_23(UMC491_NC_BASE_REG, 0x00);
        legacy_write_22_23(UMC491_NC_SIZE_REG, 0x00);
        return HAL_OK;
    }

    /* Calculate size code */
    if (size_kb <= 8) size_code = 0;
    else if (size_kb <= 16) size_code = 1;
    else if (size_kb <= 32) size_code = 2;
    else if (size_kb <= 64) size_code = 3;
    else if (size_kb <= 128) size_code = 4;
    else if (size_kb <= 256) size_code = 5;
    else if (size_kb <= 512) size_code = 6;
    else size_code = 7;

    /* Encode base (16KB units) and size with enable */
    base_val = (unsigned char)((base_kb >> 4) & 0xFF);
    size_val = 0x80 | (size_code << 4);  /* Enable + size code */

    legacy_write_22_23(UMC491_NC_BASE_REG, base_val);
    legacy_write_22_23(UMC491_NC_SIZE_REG, size_val);

    return HAL_OK;
}

static int umc491_nc_clear(int idx)
{
    if (idx != 0) {
        return HAL_ERR_PARAM;
    }

    legacy_write_22_23(UMC491_NC_BASE_REG, 0x00);
    legacy_write_22_23(UMC491_NC_SIZE_REG, 0x00);

    return HAL_OK;
}

static int umc491_reg_read(int reg)
{
    if (reg < 0 || reg > 0xFF) {
        return -1;
    }
    return legacy_read_22_23((unsigned char)reg);
}

static int umc491_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 0xFF || val < 0 || val > 0xFF) {
        return HAL_ERR_PARAM;
    }
    legacy_write_22_23((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

const chipset_ops_t ops_umc_491 = {
    /* Identity */
    .name = "UMC UM82C491",
    .vendor = "UMC",
    .tier = "A",
    .score_x10 = 85,

    /* Detection */
    .probe = umc491_probe,

    /* Cache control */
    .cache_get = umc491_cache_get,
    .cache_set = umc491_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions */
    .nc_count = 1,
    .nc_granularity = 8,
    .nc_max_kb = 1024,
    .nc_read = umc491_nc_read,
    .nc_write = umc491_nc_write,
    .nc_clear = umc491_nc_clear,

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
    .reg_read = umc491_reg_read,
    .reg_write = umc491_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * ETEQ 82C495WB (BENGAL) - OPTi CLONE
 *
 * Features:
 * - Write-back L2 cache support
 * - 2 NC regions at Index 0x52-0x55 (OPTi-compatible encoding)
 * - 8KB minimum granularity
 * - Uses 0x22/0x24 ports (same as OPTi!)
 *
 * Register layout (same as OPTi 391):
 *   0x20: Cache control (bit 0 = enable)
 *   0x52-0x53: NC Region 0
 *   0x54-0x55: NC Region 1
 *============================================================================*/

#define ETEQ_CACHE_REG      0x20
#define ETEQ_CACHE_ENABLE   0x01
#define ETEQ_NC0_BASE_REG   0x52
#define ETEQ_NC0_SIZE_REG   0x53
#define ETEQ_NC1_BASE_REG   0x54
#define ETEQ_NC1_SIZE_REG   0x55

static int eteq_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, OPTI_DATA_PORT)) {
        return 0;
    }

    /* Eteq Bengal: ID pattern 0x8x at register 0x20 (bits 7:5 = 100b) */
    id = legacy_read_22_24(ETEQ_CACHE_REG);

    return ((id & 0xE0) == 0x80) ? 1 : 0;
}

static int eteq_cache_get(void)
{
    unsigned char val = legacy_read_22_24(ETEQ_CACHE_REG);
    return (val & ETEQ_CACHE_ENABLE) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int eteq_cache_set(int enable)
{
    unsigned char val = legacy_read_22_24(ETEQ_CACHE_REG);

    if (enable) {
        val |= ETEQ_CACHE_ENABLE;
    } else {
        val &= ~ETEQ_CACHE_ENABLE;
    }

    legacy_write_22_24(ETEQ_CACHE_REG, val);
    return HAL_OK;
}

/*
 * Eteq NC regions use OPTi-compatible encoding
 */
static int eteq_nc_read(int idx, nc_region_t *r)
{
    unsigned char base_reg, size_reg;
    unsigned char lo, hi, size_code;

    if (idx < 0 || idx >= 2 || !r) {
        return HAL_ERR_PARAM;
    }

    r->base_kb = 0;
    r->size_kb = 0;
    r->active = 0;

    base_reg = (idx == 0) ? ETEQ_NC0_BASE_REG : ETEQ_NC1_BASE_REG;
    size_reg = (idx == 0) ? ETEQ_NC0_SIZE_REG : ETEQ_NC1_SIZE_REG;

    lo = legacy_read_22_24(base_reg);
    hi = legacy_read_22_24(size_reg);

    size_code = hi >> 4;

    if (size_code != 0) {
        r->active = 1;
        /* OPTi encoding: base = ((hi & 0x0F) << 12) | (lo << 4) */
        r->base_kb = ((unsigned long)(hi & 0x0F) << 12) |
                     ((unsigned long)lo << 4);
        r->size_kb = 8UL << (size_code - 1);
    }

    return HAL_OK;
}

static int eteq_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    unsigned char base_reg, size_reg;
    unsigned char base_val, size_val;
    unsigned char size_code;

    if (idx < 0 || idx >= 2) {
        return HAL_ERR_PARAM;
    }

    base_reg = (idx == 0) ? ETEQ_NC0_BASE_REG : ETEQ_NC1_BASE_REG;
    size_reg = (idx == 0) ? ETEQ_NC0_SIZE_REG : ETEQ_NC1_SIZE_REG;

    if (size_kb == 0) {
        legacy_write_22_24(base_reg, 0x00);
        legacy_write_22_24(size_reg, 0x00);
        return HAL_OK;
    }

    /* Calculate size code (OPTi style) */
    if (size_kb <= 8) size_code = 1;
    else if (size_kb <= 16) size_code = 2;
    else if (size_kb <= 32) size_code = 3;
    else if (size_kb <= 64) size_code = 4;
    else if (size_kb <= 128) size_code = 5;
    else if (size_kb <= 256) size_code = 6;
    else size_code = 7;  /* 512KB max */

    /* Encode (OPTi style) */
    base_val = (unsigned char)((base_kb >> 4) & 0xFF);
    size_val = (size_code << 4) | ((unsigned char)((base_kb >> 12) & 0x0F));

    legacy_write_22_24(base_reg, base_val);
    legacy_write_22_24(size_reg, size_val);

    return HAL_OK;
}

static int eteq_nc_clear(int idx)
{
    unsigned char base_reg, size_reg;

    if (idx < 0 || idx >= 2) {
        return HAL_ERR_PARAM;
    }

    base_reg = (idx == 0) ? ETEQ_NC0_BASE_REG : ETEQ_NC1_BASE_REG;
    size_reg = (idx == 0) ? ETEQ_NC0_SIZE_REG : ETEQ_NC1_SIZE_REG;

    legacy_write_22_24(base_reg, 0x00);
    legacy_write_22_24(size_reg, 0x00);

    return HAL_OK;
}

static int eteq_reg_read(int reg)
{
    if (reg < 0 || reg > 0xFF) {
        return -1;
    }
    return legacy_read_22_24((unsigned char)reg);
}

static int eteq_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 0xFF || val < 0 || val > 0xFF) {
        return HAL_ERR_PARAM;
    }
    legacy_write_22_24((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

const chipset_ops_t ops_eteq_bengal = {
    /* Identity */
    .name = "Eteq 82C495WB (Bengal)",
    .vendor = "Eteq",
    .tier = "A",
    .score_x10 = 80,

    /* Detection */
    .probe = eteq_probe,

    /* Cache control */
    .cache_get = eteq_cache_get,
    .cache_set = eteq_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions */
    .nc_count = 2,
    .nc_granularity = 8,
    .nc_max_kb = 512,
    .nc_read = eteq_nc_read,
    .nc_write = eteq_nc_write,
    .nc_clear = eteq_nc_clear,

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
    .reg_read = eteq_reg_read,
    .reg_write = eteq_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = OPTI_DATA_PORT
};

/*============================================================================
 * FARADAY FE3600 - 486 ISA CHIPSET
 *
 * Features:
 * - Write-through cache
 * - 3 NC regions, 128KB minimum granularity
 * - Uses 0x22/0x23 ports
 *
 * Register layout:
 *   0x15: Cache control (bit 0 = enable)
 *============================================================================*/

#define FARADAY_CACHE_REG       0x15
#define FARADAY_CACHE_ENABLE    0x01

static int faraday_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    /* Faraday has characteristic ID at register 0x00 */
    id = legacy_read_22_23(0x00);

    /* Faraday pattern: 0x3x */
    return ((id & 0xF0) == 0x30) ? 1 : 0;
}

static int faraday_cache_get(void)
{
    unsigned char val = legacy_read_22_23(FARADAY_CACHE_REG);
    return (val & FARADAY_CACHE_ENABLE) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int faraday_cache_set(int enable)
{
    unsigned char val = legacy_read_22_23(FARADAY_CACHE_REG);

    if (enable) {
        val |= FARADAY_CACHE_ENABLE;
    } else {
        val &= ~FARADAY_CACHE_ENABLE;
    }

    legacy_write_22_23(FARADAY_CACHE_REG, val);
    return HAL_OK;
}

static int faraday_reg_read(int reg)
{
    if (reg < 0 || reg > 0xFF) {
        return -1;
    }
    return legacy_read_22_23((unsigned char)reg);
}

static int faraday_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 0xFF || val < 0 || val > 0xFF) {
        return HAL_ERR_PARAM;
    }
    legacy_write_22_23((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

const chipset_ops_t ops_faraday = {
    /* Identity */
    .name = "Faraday FE3600",
    .vendor = "Faraday",
    .tier = "A",
    .score_x10 = 88,

    /* Detection */
    .probe = faraday_probe,

    /* Cache control */
    .cache_get = faraday_cache_get,
    .cache_set = faraday_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 0,

    /* NC regions: Faraday hardware has 3, but the ops are unimplemented
       stubs. Report 0 so the UI shows no phantom editable regions.
       TODO: implement real Faraday NC support, then restore nc_count = 3. */
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
    .reg_count = 64,
    .reg_base = 0x00,
    .reg_read = faraday_reg_read,
    .reg_write = faraday_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * MIC MIC9391 - 486 ISA CHIPSET
 *
 * Features:
 * - Write-back L2 cache support
 * - 1 NC region, 64KB minimum granularity
 * - Hardware cache flush trigger at register 0x40 bit 1
 * - Uses 0x22/0x23 ports
 *
 * Register layout:
 *   0x40: Cache control (bit 0 = enable, bit 1 = HW flush trigger)
 *============================================================================*/

#define MIC_CACHE_REG           0x40
#define MIC_CACHE_ENABLE        0x01
#define MIC_CACHE_FLUSH_BIT     0x02

static int mic9391_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    /* MIC 9391 has characteristic ID at register 0x00 */
    id = legacy_read_22_23(0x00);

    /* MIC pattern: 0x9x */
    return ((id & 0xF0) == 0x90) ? 1 : 0;
}

static int mic9391_cache_get(void)
{
    unsigned char val = legacy_read_22_23(MIC_CACHE_REG);
    return (val & MIC_CACHE_ENABLE) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int mic9391_cache_set(int enable)
{
    unsigned char val = legacy_read_22_23(MIC_CACHE_REG);

    if (enable) {
        val |= MIC_CACHE_ENABLE;
    } else {
        val &= ~MIC_CACHE_ENABLE;
    }

    legacy_write_22_23(MIC_CACHE_REG, val);
    return HAL_OK;
}

/*
 * MIC9391 has hardware flush trigger
 */
static int mic9391_cache_flush(void)
{
    unsigned char val = legacy_read_22_23(MIC_CACHE_REG);

    /* Toggle flush bit to trigger hardware flush */
    val |= MIC_CACHE_FLUSH_BIT;
    legacy_write_22_23(MIC_CACHE_REG, val);

    val &= ~MIC_CACHE_FLUSH_BIT;
    legacy_write_22_23(MIC_CACHE_REG, val);

    /* Also do WBINVD for safety */
    return generic_wbinvd_flush();
}

static int mic9391_reg_read(int reg)
{
    if (reg < 0 || reg > 0xFF) {
        return -1;
    }
    return legacy_read_22_23((unsigned char)reg);
}

static int mic9391_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 0xFF || val < 0 || val > 0xFF) {
        return HAL_ERR_PARAM;
    }
    legacy_write_22_23((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

const chipset_ops_t ops_mic_9391 = {
    /* Identity */
    .name = "MIC MIC9391",
    .vendor = "MIC",
    .tier = "A",
    .score_x10 = 83,

    /* Detection */
    .probe = mic9391_probe,

    /* Cache control */
    .cache_get = mic9391_cache_get,
    .cache_set = mic9391_cache_set,
    .cache_flush = mic9391_cache_flush,  /* Custom flush with HW trigger */
    .is_writeback = 1,

    /* NC regions: unimplemented stub; report 0 (no phantom editable regions). */
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
    .reg_count = 128,
    .reg_base = 0x00,
    .reg_read = mic9391_reg_read,
    .reg_write = mic9391_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * CONTAQ 82C596 - 486 ISA CHIPSET
 *
 * Features:
 * - Write-through cache
 * - No NC regions (shadow RAM only)
 * - Uses 0x22/0x23 ports (NOT PCI!)
 *
 * Register layout:
 *   0x11: Cache control (bit 0 = enable)
 *   0x15: Shadow RAM control
 *============================================================================*/

#define CONTAQ_CACHE_REG        0x11
#define CONTAQ_CACHE_ENABLE     0x01

static int contaq596_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    /* Contaq 596 has characteristic ID at register 0x00 */
    id = legacy_read_22_23(0x00);

    /* Contaq pattern: 0x59 or 0x5x */
    return ((id & 0xF0) == 0x50) ? 1 : 0;
}

static int contaq596_cache_get(void)
{
    unsigned char val = legacy_read_22_23(CONTAQ_CACHE_REG);
    return (val & CONTAQ_CACHE_ENABLE) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int contaq596_cache_set(int enable)
{
    unsigned char val = legacy_read_22_23(CONTAQ_CACHE_REG);

    if (enable) {
        val |= CONTAQ_CACHE_ENABLE;
    } else {
        val &= ~CONTAQ_CACHE_ENABLE;
    }

    legacy_write_22_23(CONTAQ_CACHE_REG, val);
    return HAL_OK;
}

static int contaq596_reg_read(int reg)
{
    if (reg < 0 || reg > 0xFF) {
        return -1;
    }
    return legacy_read_22_23((unsigned char)reg);
}

static int contaq596_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 0xFF || val < 0 || val > 0xFF) {
        return HAL_ERR_PARAM;
    }
    legacy_write_22_23((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

const chipset_ops_t ops_contaq_596 = {
    /* Identity */
    .name = "Contaq 82C596",
    .vendor = "Contaq",
    .tier = "C",
    .score_x10 = 65,

    /* Detection */
    .probe = contaq596_probe,

    /* Cache control */
    .cache_get = contaq596_cache_get,
    .cache_set = contaq596_cache_set,
    .cache_flush = generic_wbinvd_flush,
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
    .reg_count = 64,
    .reg_base = 0x00,
    .reg_read = contaq596_reg_read,
    .reg_write = contaq596_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * FOREX FRX-386DX - 386DX CHIPSET
 *
 * Taiwanese chipset manufacturer.
 * Uses boundary-based NC with 1MB maximum.
 * Uses 0x22/0x23 ports.
 *============================================================================*/

static int forex_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    /* Forex has characteristic ID at register 0x00 */
    id = legacy_read_22_23(0x00);

    /* Forex pattern: 0xFx */
    return ((id & 0xF0) == 0xF0) ? 1 : 0;
}

static int forex_cache_get(void)
{
    unsigned char val = legacy_read_22_23(0x10);
    return (val & 0x01) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int forex_cache_set(int enable)
{
    unsigned char val = legacy_read_22_23(0x10);

    if (enable) {
        val |= 0x01;
    } else {
        val &= ~0x01;
    }

    legacy_write_22_23(0x10, val);
    return HAL_OK;
}

static int forex_reg_read(int reg)
{
    if (reg < 0 || reg > 0xFF) {
        return -1;
    }
    return legacy_read_22_23((unsigned char)reg);
}

static int forex_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 0xFF || val < 0 || val > 0xFF) {
        return HAL_ERR_PARAM;
    }
    legacy_write_22_23((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

const chipset_ops_t ops_forex = {
    /* Identity */
    .name = "Forex FRX-386DX",
    .vendor = "Forex",
    .tier = "D",
    .score_x10 = 45,

    /* Detection */
    .probe = forex_probe,

    /* Cache control */
    .cache_get = forex_cache_get,
    .cache_set = forex_cache_set,
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
    .reg_count = 64,
    .reg_base = 0x00,
    .reg_read = forex_reg_read,
    .reg_write = forex_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * SUNTAC ST62C301 - 386/486 CHIPSET
 *
 * Taiwanese chipset.
 * Uses shadow-based NC (memory type determined by shadow setting).
 * Uses 0x22/0x23 ports.
 *============================================================================*/

static int suntac_probe(void)
{
    unsigned char id;

    if (!legacy_port_valid(LEGACY_INDEX_PORT, LEGACY_DATA_PORT)) {
        return 0;
    }

    /* Suntac has characteristic ID */
    id = legacy_read_22_23(0x00);

    /* Suntac pattern: 0x6x (similar to some OPTi but different signature) */
    if ((id & 0xF0) == 0x60) {
        /* Additional check to differentiate from OPTi */
        unsigned char id2 = legacy_read_22_23(0x01);
        if ((id2 & 0xF0) == 0x30) {  /* Suntac secondary ID */
            return 1;
        }
    }

    return 0;
}

static int suntac_cache_get(void)
{
    unsigned char val = legacy_read_22_23(0x10);
    return (val & 0x01) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int suntac_cache_set(int enable)
{
    unsigned char val = legacy_read_22_23(0x10);

    if (enable) {
        val |= 0x01;
    } else {
        val &= ~0x01;
    }

    legacy_write_22_23(0x10, val);
    return HAL_OK;
}

static int suntac_reg_read(int reg)
{
    if (reg < 0 || reg > 0xFF) {
        return -1;
    }
    return legacy_read_22_23((unsigned char)reg);
}

static int suntac_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 0xFF || val < 0 || val > 0xFF) {
        return HAL_ERR_PARAM;
    }
    legacy_write_22_23((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

const chipset_ops_t ops_suntac = {
    /* Identity */
    .name = "Suntac ST62C301",
    .vendor = "Suntac",
    .tier = "D",
    .score_x10 = 45,

    /* Detection */
    .probe = suntac_probe,

    /* Cache control */
    .cache_get = suntac_cache_get,
    .cache_set = suntac_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 0,

    /* NC regions: shadow-based, unimplemented stub; report 0. */
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
    .reg_read = suntac_reg_read,
    .reg_write = suntac_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = LEGACY_INDEX_PORT,
    .data_port = LEGACY_DATA_PORT
};

/*============================================================================
 * IBM PS/2 MCA (MICRO CHANNEL ARCHITECTURE)
 *
 * IBM PS/2 systems with Micro Channel Architecture.
 * MCA has hardware-enforced cache coherency via bus snooping, so NC regions
 * are not needed - the bus protocol handles coherency automatically.
 *
 * This is an observer-only implementation for identification purposes.
 *============================================================================*/

#define MCA_POS_BASE        0x100   /* POS register base */
#define MCA_SYSTEM_POS      0x94    /* System board POS */

/*
 * Detect MCA bus presence by checking for characteristic POS registers
 */
static int mca_probe(void)
{
    unsigned char pos0, pos1;

    /* Try to read POS ID bytes from system board */
    /* First, enable system board POS access */
    io_write_byte(0x94, 0x00);  /* Select system board */

    pos0 = io_read_byte(MCA_POS_BASE + 0);
    pos1 = io_read_byte(MCA_POS_BASE + 1);

    /* Deselect */
    io_write_byte(0x94, 0xFF);

    /* MCA systems have recognizable POS ID patterns */
    /* IBM PS/2 typically has non-FF values here */
    if (pos0 != 0xFF && pos1 != 0xFF && pos0 != 0x00 && pos1 != 0x00) {
        return 1;
    }

    return 0;
}

static int mca_cache_get(void)
{
    /* MCA systems typically have cache enabled by BIOS */
    return CACHE_ENABLED;
}

const chipset_ops_t ops_mca_generic = {
    /* Identity */
    .name = "IBM PS/2 MCA",
    .vendor = "IBM",
    .tier = "I",
    .score_x10 = 25,

    /* Detection */
    .probe = mca_probe,

    /* Cache (info-only - controlled by BIOS/hardware) */
    .cache_get = mca_cache_get,
    .cache_set = hal_stub_unsupported_i,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 0,

    /* NC regions (not needed - MCA has hardware snooping) */
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

    /* A20 (keyboard controller on PS/2) */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access (POS registers, not indexed) */
    .reg_count = 8,
    .reg_base = 0x100,
    .reg_read = hal_stub_unsupported_i,
    .reg_write = hal_stub_unsupported_ii,

    /* Metadata */
    .info_only = 1,
    .index_port = 0x94,
    .data_port = 0x100
};

/*============================================================================
 * END OF CK_OTHER.C
 *============================================================================*/
