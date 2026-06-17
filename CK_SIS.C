/*============================================================================
 * CK_SIS.C - SiS Chipset HAL Implementations
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-05 19:30:00 EST
 *
 * Supported chipsets:
 *   - SiS 85C460         - Legacy ISA, 486, 1992
 *   - SiS 85C310 (Rabbit) - Legacy ISA, 486 VLB, 1993
 *   - SiS 85C496         - PCI, 486, 1994
 *   - SiS 5598           - PCI, Super7, 1997
 *   - SiS 530            - PCI, Super7 integrated, 1998
 *
 * SiS chipsets generally have:
 *   - Good NC region support (2-4 regions)
 *   - Write-back cache support
 *   - 64KB granularity for NC regions
 *============================================================================*/

#include <conio.h>
#include "CK_HAL.H"

/*============================================================================
 * EXTERNAL DECLARATIONS (from CK_IO.C)
 *============================================================================*/

extern unsigned char pci_read_config_byte(unsigned char bus, unsigned char dev,
                                          unsigned char func, unsigned char reg);
extern void pci_write_config_byte(unsigned char bus, unsigned char dev,
                                  unsigned char func, unsigned char reg,
                                  unsigned char val);
extern unsigned long pci_read_config_dword(unsigned char bus, unsigned char dev,
                                           unsigned char func, unsigned char reg);
extern int pci_bus_present(void);
extern unsigned char legacy_read_22_23(unsigned char reg);
extern void legacy_write_22_23(unsigned char reg, unsigned char val);

/*============================================================================
 * SIS PCI VENDOR AND DEVICE IDS
 *============================================================================*/

#define SIS_VENDOR_ID       0x1039

#define SIS_496_DEV         0x0496  /* SiS 85C496 */
#define SIS_5598_DEV        0x5598  /* SiS 5598 */
#define SIS_530_DEV         0x0530  /* SiS 530 */

/*============================================================================
 * SIS LEGACY ID VALUES (at register 0x00 via 0x22/0x23)
 *============================================================================*/

#define SIS_460_ID_40       0x40    /* SiS 460 variant */
#define SIS_460_ID_41       0x41    /* SiS 460 variant */
#define SIS_RABBIT_ID       0x31    /* SiS Rabbit (needs secondary check) */

/*============================================================================
 * SIS 460/RABBIT REGISTER DEFINITIONS (Legacy 0x22/0x23)
 *============================================================================*/

#define SIS460_CACHE_REG        0x10    /* Cache control register */
#define SIS460_CACHE_EN         0x01    /* Bit 0: Cache enable */

/* NC regions at 0x14-0x1B: 4 regions, 2 bytes each */
#define SIS460_NC_BASE          0x14    /* First NC region */
#define SIS460_NC_COUNT         4

/*============================================================================
 * SIS 496 REGISTER DEFINITIONS (PCI config space)
 *============================================================================*/

#define SIS496_CACHE_REG        0x42    /* Cache control register */
#define SIS496_CACHE_EN         0x01    /* Bit 0: Cache enable */

/* NC regions ("Exclusive Areas") at 0x50-0x55: 3 regions, 2 bytes each */
#define SIS496_NC_BASE          0x50
#define SIS496_NC_COUNT         3

/*============================================================================
 * SIS 5598/530 REGISTER DEFINITIONS (PCI config space)
 *============================================================================*/

#define SIS5598_CACHE_REG       0x51    /* Cache control register */
#define SIS5598_CACHE_BIT       6       /* Bit 6: L2 cache enable (5598) */
#define SIS530_CACHE_BIT        7       /* Bit 7: L2 cache enable (530) */

/* NC regions with special 16-bit packed encoding */
#define SIS5598_NC_ENABLE       0x77    /* NC region enable register */
#define SIS5598_NC_AREA1_LO     0x78    /* NC Area I low byte */
#define SIS5598_NC_AREA1_HI     0x79    /* NC Area I high byte */
#define SIS5598_NC_AREA2_LO     0x7A    /* NC Area II low byte */
#define SIS5598_NC_AREA2_HI     0x7B    /* NC Area II high byte */
#define SIS5598_NC_COUNT        2

/*============================================================================
 * SIS 460/RABBIT LEGACY FUNCTIONS
 *============================================================================*/

static int sis460_cache_get(void)
{
    unsigned char val = legacy_read_22_23(SIS460_CACHE_REG);
    return (val & SIS460_CACHE_EN) ? (CACHE_ENABLED | CACHE_WRITEBACK) : CACHE_DISABLED;
}

static int sis460_cache_set(int enable)
{
    unsigned char val = legacy_read_22_23(SIS460_CACHE_REG);
    if (enable) {
        val |= SIS460_CACHE_EN;
    } else {
        val &= ~SIS460_CACHE_EN;
    }
    legacy_write_22_23(SIS460_CACHE_REG, val);
    return HAL_OK;
}

/*
 * SiS 460/Rabbit NC region encoding:
 * - Register pair: base (reg+0), control (reg+1)
 * - Base: Address bits A21:A14 (64KB units, max 16MB addressable)
 * - Control: Bits 7:4 = size code (0=disabled, 1=64KB, 2=128KB, ...)
 */
static int sis460_nc_read(int idx, nc_region_t *r)
{
    unsigned char base_reg, ctrl_reg;
    unsigned char size_code;

    if (idx < 0 || idx >= SIS460_NC_COUNT || !r) return HAL_ERR_PARAM;

    base_reg = legacy_read_22_23(SIS460_NC_BASE + idx * 2);
    ctrl_reg = legacy_read_22_23(SIS460_NC_BASE + idx * 2 + 1);

    size_code = ctrl_reg >> 4;

    if (size_code == 0) {
        r->base_kb = 0;
        r->size_kb = 0;
        r->active = 0;
    } else {
        r->base_kb = (unsigned long)base_reg << 6;  /* 64KB units */
        r->size_kb = 64UL << (size_code - 1);       /* 64KB * 2^(code-1) */
        r->active = 1;
    }

    return HAL_OK;
}

static int sis460_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    unsigned char base_reg, ctrl_reg;
    unsigned char size_code;

    if (idx < 0 || idx >= SIS460_NC_COUNT) return HAL_ERR_PARAM;

    /* size 0 = disable; don't program a spurious 64KB region. */
    if (size_kb == 0) {
        legacy_write_22_23(SIS460_NC_BASE + idx * 2 + 1, 0x00);
        return HAL_OK;
    }

    /* Align base to 64KB and REJECT (don't silently truncate) a base that
       won't fit the 8-bit base field (64KB units => ~16MB max). A truncated
       base would fence the wrong physical address. TODO: confirm the base
       unit (code uses 64KB/A23:A16; the comment says A21:A14) vs datasheet. */
    base_kb = (base_kb + 63) & ~63UL;
    if ((base_kb >> 6) > 0xFFUL) return HAL_ERR_PARAM;

    /* Size code: code N => 64KB << (N-1). Reject sizes beyond the field. */
    if (size_kb <= 64) size_code = 1;
    else if (size_kb <= 128) size_code = 2;
    else if (size_kb <= 256) size_code = 3;
    else if (size_kb <= 512) size_code = 4;
    else if (size_kb <= 1024) size_code = 5;
    else if (size_kb <= 2048) size_code = 6;
    else if (size_kb <= 4096) size_code = 7;
    else if (size_kb <= 8192) size_code = 8;
    else if (size_kb <= 16384) size_code = 9;
    else if (size_kb <= 32768) size_code = 10;
    else if (size_kb <= 65536) size_code = 11;
    else return HAL_ERR_PARAM;

    base_reg = (unsigned char)(base_kb >> 6);
    ctrl_reg = (unsigned char)(size_code << 4);

    legacy_write_22_23(SIS460_NC_BASE + idx * 2, base_reg);
    legacy_write_22_23(SIS460_NC_BASE + idx * 2 + 1, ctrl_reg);

    return HAL_OK;
}

static int sis460_nc_clear(int idx)
{
    if (idx < 0 || idx >= SIS460_NC_COUNT) return HAL_ERR_PARAM;

    /* Clear size code to 0 = disabled */
    legacy_write_22_23(SIS460_NC_BASE + idx * 2 + 1, 0x00);

    return HAL_OK;
}

static int sis460_reg_read(int reg)
{
    if (reg < 0 || reg > 255) return -1;
    return legacy_read_22_23((unsigned char)reg);
}

static int sis460_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 255) return HAL_ERR_PARAM;
    legacy_write_22_23((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

/*============================================================================
 * SIS 496 PCI FUNCTIONS
 *============================================================================*/

static int sis496_cache_get(void)
{
    unsigned char val = pci_read_config_byte(0, 0, 0, SIS496_CACHE_REG);
    return (val & SIS496_CACHE_EN) ? (CACHE_ENABLED | CACHE_WRITEBACK) : CACHE_DISABLED;
}

static int sis496_cache_set(int enable)
{
    unsigned char val = pci_read_config_byte(0, 0, 0, SIS496_CACHE_REG);
    if (enable) {
        val |= SIS496_CACHE_EN;
    } else {
        val &= ~SIS496_CACHE_EN;
    }
    pci_write_config_byte(0, 0, 0, SIS496_CACHE_REG, val);
    return HAL_OK;
}

/*
 * SiS 496 NC region encoding:
 * Same as 460 but via PCI config space at 0x50-0x55 (3 regions).
 * Note: Region 2 (0x54-0x55) only uses upper nibble of high byte.
 */
static int sis496_nc_read(int idx, nc_region_t *r)
{
    unsigned char base_reg, ctrl_reg;
    unsigned char size_code;

    if (idx < 0 || idx >= SIS496_NC_COUNT || !r) return HAL_ERR_PARAM;

    base_reg = pci_read_config_byte(0, 0, 0, SIS496_NC_BASE + idx * 2);
    ctrl_reg = pci_read_config_byte(0, 0, 0, SIS496_NC_BASE + idx * 2 + 1);

    /* Region 2 only uses upper nibble of high byte */
    if (idx == 2) ctrl_reg &= 0xF0;

    size_code = ctrl_reg >> 4;

    if (size_code == 0) {
        r->base_kb = 0;
        r->size_kb = 0;
        r->active = 0;
    } else {
        r->base_kb = (unsigned long)base_reg << 6;  /* 64KB units */
        r->size_kb = 64UL << (size_code - 1);
        r->active = 1;
    }

    return HAL_OK;
}

static int sis496_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    unsigned char base_reg, ctrl_reg;
    unsigned char size_code;

    if (idx < 0 || idx >= SIS496_NC_COUNT) return HAL_ERR_PARAM;

    /* size 0 = disable */
    if (size_kb == 0) {
        pci_write_config_byte(0, 0, 0, SIS496_NC_BASE + idx * 2 + 1, 0x00);
        return HAL_OK;
    }

    /* Align base to 64KB; reject (don't truncate) if it overflows the 8-bit
       base field. */
    base_kb = (base_kb + 63) & ~63UL;
    if ((base_kb >> 6) > 0xFFUL) return HAL_ERR_PARAM;

    /* Calculate size code (same ladder as 460, capped at the 496's 8MB max) */
    if (size_kb <= 64) size_code = 1;
    else if (size_kb <= 128) size_code = 2;
    else if (size_kb <= 256) size_code = 3;
    else if (size_kb <= 512) size_code = 4;
    else if (size_kb <= 1024) size_code = 5;
    else if (size_kb <= 2048) size_code = 6;
    else if (size_kb <= 4096) size_code = 7;
    else if (size_kb <= 8192) size_code = 8;
    else return HAL_ERR_PARAM;

    base_reg = (unsigned char)(base_kb >> 6);
    ctrl_reg = (unsigned char)(size_code << 4);

    /* Region 2's high byte (0x55) carries the size code only in its upper
       nibble; the read path masks `&0xF0`, so preserve the low nibble
       (reserved/base-extension) on write to keep read/write symmetric (IS-C2). */
    if (idx == 2) {
        unsigned char cur =
            pci_read_config_byte(0, 0, 0, SIS496_NC_BASE + idx * 2 + 1);
        ctrl_reg = (unsigned char)((ctrl_reg & 0xF0) | (cur & 0x0F));
    }

    pci_write_config_byte(0, 0, 0, SIS496_NC_BASE + idx * 2, base_reg);
    pci_write_config_byte(0, 0, 0, SIS496_NC_BASE + idx * 2 + 1, ctrl_reg);

    return HAL_OK;
}

static int sis496_nc_clear(int idx)
{
    if (idx < 0 || idx >= SIS496_NC_COUNT) return HAL_ERR_PARAM;

    /* Clear size code to 0 = disabled */
    pci_write_config_byte(0, 0, 0, SIS496_NC_BASE + idx * 2 + 1, 0x00);

    return HAL_OK;
}

static int sis496_reg_read(int reg)
{
    if (reg < 0 || reg > 255) return -1;
    return pci_read_config_byte(0, 0, 0, (unsigned char)reg);
}

static int sis496_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 255) return HAL_ERR_PARAM;
    pci_write_config_byte(0, 0, 0, (unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

/*============================================================================
 * SIS 5598/530 PCI FUNCTIONS (Super7 with special NC encoding)
 *============================================================================*/

static int sis5598_cache_get(void)
{
    unsigned char val = pci_read_config_byte(0, 0, 0, SIS5598_CACHE_REG);
    return (val & (1 << SIS5598_CACHE_BIT)) ? (CACHE_ENABLED | CACHE_WRITEBACK) : CACHE_DISABLED;
}

static int sis5598_cache_set(int enable)
{
    unsigned char val = pci_read_config_byte(0, 0, 0, SIS5598_CACHE_REG);
    if (enable) {
        val |= (1 << SIS5598_CACHE_BIT);
    } else {
        val &= ~(1 << SIS5598_CACHE_BIT);
    }
    pci_write_config_byte(0, 0, 0, SIS5598_CACHE_REG, val);
    return HAL_OK;
}

static int sis530_cache_get(void)
{
    unsigned char val = pci_read_config_byte(0, 0, 0, SIS5598_CACHE_REG);
    return (val & (1 << SIS530_CACHE_BIT)) ? (CACHE_ENABLED | CACHE_WRITEBACK) : CACHE_DISABLED;
}

static int sis530_cache_set(int enable)
{
    unsigned char val = pci_read_config_byte(0, 0, 0, SIS5598_CACHE_REG);
    if (enable) {
        val |= (1 << SIS530_CACHE_BIT);
    } else {
        val &= ~(1 << SIS530_CACHE_BIT);
    }
    pci_write_config_byte(0, 0, 0, SIS5598_CACHE_REG, val);
    return HAL_OK;
}

/*
 * SiS 5598/530 NC region encoding (16-bit packed):
 * - Enable register at 0x77: bit 0 = Area I, bit 1 = Area II
 * - Area I: 0x78 (low), 0x79 (high)
 * - Area II: 0x7A (low), 0x7B (high)
 * - 16-bit value: bits 15:13 = size code, bits 12:0 = base A28:A16
 * - Size: 0=64KB, 1=128KB, 2=256KB, 3=512KB, 4=1MB, 5=2MB, 6=4MB, 7=8MB
 * - Base: in 64KB units, max 384MB (13 bits = 8192 * 64KB)
 */
static int sis5598_nc_read(int idx, nc_region_t *r)
{
    unsigned char enable, lo, hi;
    unsigned int val;
    unsigned char size_code;

    if (idx < 0 || idx >= SIS5598_NC_COUNT || !r) return HAL_ERR_PARAM;

    enable = pci_read_config_byte(0, 0, 0, SIS5598_NC_ENABLE);

    /* Check if region is enabled */
    if (!(enable & (1 << idx))) {
        r->base_kb = 0;
        r->size_kb = 0;
        r->active = 0;
        return HAL_OK;
    }

    /* Read the 16-bit value */
    if (idx == 0) {
        lo = pci_read_config_byte(0, 0, 0, SIS5598_NC_AREA1_LO);
        hi = pci_read_config_byte(0, 0, 0, SIS5598_NC_AREA1_HI);
    } else {
        lo = pci_read_config_byte(0, 0, 0, SIS5598_NC_AREA2_LO);
        hi = pci_read_config_byte(0, 0, 0, SIS5598_NC_AREA2_HI);
    }

    val = ((unsigned int)hi << 8) | lo;

    /* Decode: bits 15:13 = size code, bits 12:0 = base in 64KB units */
    size_code = (val >> 13) & 0x07;
    r->base_kb = (unsigned long)(val & 0x1FFF) << 6;  /* 64KB units to KB */
    r->size_kb = 64UL << size_code;                    /* 64KB * 2^code */
    r->active = 1;

    return HAL_OK;
}

static int sis5598_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    unsigned char enable, lo, hi;
    unsigned int val;
    unsigned char size_code;
    unsigned int base_unit;

    if (idx < 0 || idx >= SIS5598_NC_COUNT) return HAL_ERR_PARAM;

    /* size 0 = disable via the enable register */
    if (size_kb == 0) {
        enable = pci_read_config_byte(0, 0, 0, SIS5598_NC_ENABLE);
        enable &= (unsigned char)~(1 << idx);
        pci_write_config_byte(0, 0, 0, SIS5598_NC_ENABLE, enable);
        return HAL_OK;
    }

    /* Align base to 64KB. The base is a 13-bit field in 64KB units; REJECT an
       out-of-range base instead of silently clamping to 384MB and masking
       `& 0x1FFF` (which would fence a different, unintended address). */
    base_kb = (base_kb + 63) & ~63UL;
    if ((base_kb >> 6) > 0x1FFFUL) return HAL_ERR_PARAM;

    /* Calculate size code: 0=64K, 1=128K, 2=256K, 3=512K, 4=1M, 5=2M, 6=4M, 7=8M */
    if (size_kb <= 64) size_code = 0;
    else if (size_kb <= 128) size_code = 1;
    else if (size_kb <= 256) size_code = 2;
    else if (size_kb <= 512) size_code = 3;
    else if (size_kb <= 1024) size_code = 4;
    else if (size_kb <= 2048) size_code = 5;
    else if (size_kb <= 4096) size_code = 6;
    else if (size_kb <= 8192) size_code = 7;
    else return HAL_ERR_PARAM;

    /* Reject a region that would run past the 13-bit (512MB) base window. */
    if ((base_kb + size_kb) > ((unsigned long)0x2000 * 64UL))
        return HAL_ERR_PARAM;

    /* Convert base to 64KB units (guaranteed <= 0x1FFF by the check above). */
    base_unit = (unsigned int)(base_kb >> 6);

    /* Encode: bits 15:13 = size, bits 12:0 = base */
    val = ((unsigned int)size_code << 13) | base_unit;
    lo = val & 0xFF;
    hi = (val >> 8) & 0xFF;

    /* Write register pair */
    if (idx == 0) {
        pci_write_config_byte(0, 0, 0, SIS5598_NC_AREA1_LO, lo);
        pci_write_config_byte(0, 0, 0, SIS5598_NC_AREA1_HI, hi);
    } else {
        pci_write_config_byte(0, 0, 0, SIS5598_NC_AREA2_LO, lo);
        pci_write_config_byte(0, 0, 0, SIS5598_NC_AREA2_HI, hi);
    }

    /* Enable the region */
    enable = pci_read_config_byte(0, 0, 0, SIS5598_NC_ENABLE);
    enable |= (1 << idx);
    pci_write_config_byte(0, 0, 0, SIS5598_NC_ENABLE, enable);

    return HAL_OK;
}

static int sis5598_nc_clear(int idx)
{
    unsigned char enable;

    if (idx < 0 || idx >= SIS5598_NC_COUNT) return HAL_ERR_PARAM;

    /* Disable the region via enable register */
    enable = pci_read_config_byte(0, 0, 0, SIS5598_NC_ENABLE);
    enable &= ~(1 << idx);
    pci_write_config_byte(0, 0, 0, SIS5598_NC_ENABLE, enable);

    return HAL_OK;
}

static int sis5598_reg_read(int reg)
{
    if (reg < 0 || reg > 255) return -1;
    return pci_read_config_byte(0, 0, 0, (unsigned char)reg);
}

static int sis5598_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 255) return HAL_ERR_PARAM;
    pci_write_config_byte(0, 0, 0, (unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

/*============================================================================
 * PROBE FUNCTIONS
 *============================================================================*/

static int sis460_probe(void)
{
    unsigned char id = legacy_read_22_23(0x00);
    return (id == SIS_460_ID_40 || id == SIS_460_ID_41);
}

static int sis_rabbit_probe(void)
{
    unsigned char id = legacy_read_22_23(0x00);
    unsigned char id2;

    if (id != SIS_RABBIT_ID) return 0;

    /* Secondary check at register 0x11 */
    id2 = legacy_read_22_23(0x11);
    return ((id2 & 0x80) || (id2 & 0x30));
}

static int sis496_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) return 0;

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)SIS_496_DEV << 16 | SIS_VENDOR_ID));
}

static int sis5598_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) return 0;

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)SIS_5598_DEV << 16 | SIS_VENDOR_ID));
}

static int sis530_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) return 0;

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)SIS_530_DEV << 16 | SIS_VENDOR_ID));
}

/*============================================================================
 * SIS 460 OPS STRUCTURE
 *============================================================================*/

const chipset_ops_t ops_sis_460 = {
    /* Identity */
    .name = "SiS 85C460",
    .vendor = "SiS",
    .tier = "S",
    .score_x10 = 96,

    /* Detection */
    .probe = sis460_probe,

    /* Cache */
    .cache_get = sis460_cache_get,
    .cache_set = sis460_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (4) */
    .nc_count = 4,
    .nc_granularity = 64,
    .nc_max_kb = 65536,
    .nc_read = sis460_nc_read,
    .nc_write = sis460_nc_write,
    .nc_clear = sis460_nc_clear,

    /* Shadow RAM (not exposed via HAL) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 64,
    .reg_base = 0,
    .reg_read = sis460_reg_read,
    .reg_write = sis460_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0x22,
    .data_port = 0x23
};

/*============================================================================
 * SIS RABBIT (85C310) OPS STRUCTURE
 *============================================================================*/

const chipset_ops_t ops_sis_rabbit = {
    /* Identity */
    .name = "SiS 85C310 Rabbit",
    .vendor = "SiS",
    .tier = "S",
    .score_x10 = 95,

    /* Detection */
    .probe = sis_rabbit_probe,

    /* Cache (same as 460) */
    .cache_get = sis460_cache_get,
    .cache_set = sis460_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (4, same as 460) */
    .nc_count = 4,
    .nc_granularity = 64,
    .nc_max_kb = 65536,
    .nc_read = sis460_nc_read,
    .nc_write = sis460_nc_write,
    .nc_clear = sis460_nc_clear,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 64,
    .reg_base = 0,
    .reg_read = sis460_reg_read,
    .reg_write = sis460_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0x22,
    .data_port = 0x23
};

/*============================================================================
 * SIS 496 OPS STRUCTURE
 *============================================================================*/

const chipset_ops_t ops_sis_496 = {
    /* Identity */
    .name = "SiS 85C496",
    .vendor = "SiS",
    .tier = "S",
    .score_x10 = 93,

    /* Detection */
    .probe = sis496_probe,

    /* Cache */
    .cache_get = sis496_cache_get,
    .cache_set = sis496_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (3 "Exclusive Areas") */
    .nc_count = 3,
    .nc_granularity = 64,
    .nc_max_kb = 8192,
    .nc_read = sis496_nc_read,
    .nc_write = sis496_nc_write,
    .nc_clear = sis496_nc_clear,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = sis496_reg_read,
    .reg_write = sis496_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};

/*============================================================================
 * SIS 5598 OPS STRUCTURE
 *============================================================================*/

const chipset_ops_t ops_sis_5598 = {
    /* Identity */
    .name = "SiS 5598",
    .vendor = "SiS",
    .tier = "A",
    .score_x10 = 86,

    /* Detection */
    .probe = sis5598_probe,

    /* Cache */
    .cache_get = sis5598_cache_get,
    .cache_set = sis5598_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (2 with 16-bit packed encoding) */
    .nc_count = 2,
    .nc_granularity = 64,
    .nc_max_kb = 8192,
    .nc_read = sis5598_nc_read,
    .nc_write = sis5598_nc_write,
    .nc_clear = sis5598_nc_clear,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = sis5598_reg_read,
    .reg_write = sis5598_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};

/*============================================================================
 * SIS 530 OPS STRUCTURE
 *============================================================================*/

const chipset_ops_t ops_sis_530 = {
    /* Identity */
    .name = "SiS 530",
    .vendor = "SiS",
    .tier = "A",
    .score_x10 = 84,

    /* Detection */
    .probe = sis530_probe,

    /* Cache (bit 7 instead of bit 6) */
    .cache_get = sis530_cache_get,
    .cache_set = sis530_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (same as 5598) */
    .nc_count = 2,
    .nc_granularity = 64,
    .nc_max_kb = 8192,
    .nc_read = sis5598_nc_read,
    .nc_write = sis5598_nc_write,
    .nc_clear = sis5598_nc_clear,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = sis5598_reg_read,
    .reg_write = sis5598_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};

/*============================================================================
 * SiS 5591 - Super7 PCI Chipset
 *
 * Nearly identical to 5598, same NC region encoding.
 * PCI ID: 0x1039/0x5591
 *============================================================================*/

#define SIS_5591_DEVICE_ID  0x5591

static int sis5591_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) {
        return 0;
    }

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)SIS_5591_DEVICE_ID << 16 | SIS_VENDOR_ID)) ? 1 : 0;
}

const chipset_ops_t ops_sis_5591 = {
    /* Identity */
    .name = "SiS 5591",
    .vendor = "SiS",
    .tier = "A",
    .score_x10 = 85,

    /* Detection */
    .probe = sis5591_probe,

    /* Cache (same as 5598) */
    .cache_get = sis5598_cache_get,
    .cache_set = sis5598_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (same as 5598) */
    .nc_count = 2,
    .nc_granularity = 64,
    .nc_max_kb = 4096,
    .nc_read = sis5598_nc_read,
    .nc_write = sis5598_nc_write,
    .nc_clear = sis5598_nc_clear,

    /* Shadow RAM */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = sis5598_reg_read,
    .reg_write = sis5598_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};

/*============================================================================
 * SiS 85C411/406 - EISA 386/486 Chipset (Undocumented)
 *
 * Info-only implementation due to lack of documentation.
 *============================================================================*/

static int sis_eisa_probe(void)
{
    /* SiS EISA detection would go here if documented */
    /* For now, this is a placeholder that never matches */
    return 0;
}

static int sis_eisa_cache_get(void)
{
    return CACHE_ENABLED;
}

const chipset_ops_t ops_sis_eisa = {
    /* Identity */
    .name = "SiS 85C411/406",
    .vendor = "SiS",
    .tier = "I",
    .score_x10 = 35,

    /* Detection */
    .probe = sis_eisa_probe,

    /* Cache (info-only) */
    .cache_get = sis_eisa_cache_get,
    .cache_set = hal_stub_unsupported_i,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 0,

    /* NC regions (none documented) */
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

    /* Registers */
    .reg_count = 0,
    .reg_base = 0,
    .reg_read = hal_stub_unsupported_i,
    .reg_write = hal_stub_unsupported_ii,

    /* Metadata */
    .info_only = 1,
    .index_port = 0,
    .data_port = 0
};

/*============================================================================
 * END OF CK_SIS.C
 *============================================================================*/
