/*============================================================================
 * CK_VIA.C - VIA Chipset HAL Implementations
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-05 20:20:00 EST
 *
 * Supported Chipsets:
 * - VIA VT82C495 (Venus) - 486 EISA, info-only (special ports 0xA8/0xA9)
 * - VIA VT82C570 (VP1)   - Pentium PCI, WT, 2 NC regions, 64KB min
 * - VIA VT82C597 (VP3)   - Super7 PCI, WB, 4 NC regions, 64KB min
 * - VIA VT82C598 (MVP3)  - Super7 PCI, WB, 4 NC regions, 64KB min
 *
 * VIA Quirks:
 * - VT82C495 uses special ports 0xA8 (index) / 0xA9 (data)
 * - VP1/VP3/MVP3 use standard PCI config space
 * - NC regions at PCI 0x58-0x5F with 64KB granularity
 *============================================================================*/

#include <conio.h>
#include "CK_HAL.H"

/*============================================================================
 * EXTERNAL DECLARATIONS
 *============================================================================*/

/* From CK_IO.C */
extern int pci_bus_present(void);
extern unsigned char pci_read_config_byte(unsigned char bus, unsigned char dev,
                                          unsigned char func, unsigned char reg);
extern void pci_write_config_byte(unsigned char bus, unsigned char dev,
                                  unsigned char func, unsigned char reg,
                                  unsigned char val);
extern unsigned long pci_read_config_dword(unsigned char bus, unsigned char dev,
                                           unsigned char func, unsigned char reg);
extern unsigned char legacy_read_a8_a9(unsigned char reg);
extern void legacy_write_a8_a9(unsigned char reg, unsigned char val);

/* From CK_HAL.C */
extern int generic_wbinvd_flush(void);
extern int generic_port92_a20_get(void);
extern int generic_port92_a20_set(int enable);
extern int hal_stub_unsupported_i(int x);
extern int hal_stub_unsupported_ii(int x, int y);
extern int hal_stub_unsupported_iull(int x, unsigned long y, unsigned long z);
extern int hal_stub_nc_read(int idx, nc_region_t *r);

/*============================================================================
 * VIA PCI IDS
 *============================================================================*/

#define VIA_VENDOR_ID       0x1106

/* North Bridge device IDs */
#define VIA_VP1_DEVICE_ID   0x0570  /* VT82C570 VP1 */
#define VIA_VP3_DEVICE_ID   0x0597  /* VT82C597 Apollo VP3 */
#define VIA_MVP3_DEVICE_ID  0x0598  /* VT82C598 Apollo MVP3 */

/*============================================================================
 * VIA PCI REGISTER DEFINITIONS
 *
 * All VIA PCI chipsets use bus 0, device 0, function 0 for north bridge.
 *
 * VP1 registers:
 *   0x50: CPU/Cache Control
 *         Bit 2: L2 Cache Enable
 *   0x58-0x5B: NC Region 0-1 (2 regions)
 *
 * VP3/MVP3 registers:
 *   0x52: CPU/Cache Control
 *         Bit 0: L2 Cache Enable
 *   0x58-0x5F: NC Region 0-3 (4 regions)
 *
 * NC Region encoding (all VIA PCI chipsets):
 *   Byte 0: Base address in 64KB units (bits 7:0 = A23:A16)
 *   Byte 1: Bits 7:4 = Size code (0=disabled, 1=64K, 2=128K, etc.)
 *           Bits 3:0 = reserved (write 0)
 *
 * Size: 64KB << (size_code - 1)
 * Base: byte0 << 6 = KB
 *============================================================================*/

/* PCI config registers */
#define VIA_VP1_CACHE_REG   0x50    /* VP1 cache control */
#define VIA_VP3_CACHE_REG   0x52    /* VP3/MVP3 cache control */
#define VIA_NC_BASE_REG     0x58    /* NC regions start here */

/* Bit definitions */
#define VIA_VP1_CACHE_BIT   0x04    /* Bit 2 for VP1 */
#define VIA_VP3_CACHE_BIT   0x01    /* Bit 0 for VP3/MVP3 */

/*============================================================================
 * VIA VT82C495 (Venus) - Legacy EISA Chipset
 *
 * Uses special ports 0xA8 (index) / 0xA9 (data)
 * Info-only: no documented NC region or cache control
 *============================================================================*/

#define VIA_VENUS_INDEX     0xA8
#define VIA_VENUS_DATA      0xA9

static int via_vt82c495_probe(void)
{
    unsigned char val1, val2;

    /* Venus uses 0xA8/0xA9 ports */
    /* Read two different registers and check for non-FF response */
    outp(VIA_VENUS_INDEX, 0x00);
    val1 = inp(VIA_VENUS_DATA);

    outp(VIA_VENUS_INDEX, 0x01);
    val2 = inp(VIA_VENUS_DATA);

    /* If both are 0xFF, port is probably not connected */
    if (val1 == 0xFF && val2 == 0xFF) {
        return 0;
    }

    /* Additional check: VIA signature pattern */
    /* VT82C49x typically has identifiable patterns at certain registers */
    /* For safety, we'll accept any responsive 0xA8/0xA9 as potential Venus */
    /* unless a more specific chipset is detected first */

    return 1;
}

static int via_vt82c495_cache_get(void)
{
    /* Info-only - assume enabled */
    return CACHE_ENABLED;
}

static int via_vt82c495_reg_read(int reg)
{
    if (reg < 0 || reg > 0xFF) {
        return -1;
    }
    return legacy_read_a8_a9((unsigned char)reg);
}

const chipset_ops_t ops_via_vt82c495 = {
    /* Identity */
    .name = "VIA VT82C495 (Venus)",
    .vendor = "VIA",
    .tier = "I",
    .score_x10 = 40,

    /* Detection */
    .probe = via_vt82c495_probe,

    /* Cache (info-only) */
    .cache_get = via_vt82c495_cache_get,
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

    /* Shadow RAM (none) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 64,
    .reg_base = 0x00,
    .reg_read = via_vt82c495_reg_read,
    .reg_write = hal_stub_unsupported_ii,

    /* Metadata */
    .info_only = 1,
    .index_port = VIA_VENUS_INDEX,
    .data_port = VIA_VENUS_DATA
};

/*============================================================================
 * VIA PCI CHIPSET SHARED HELPERS
 *============================================================================*/

/*
 * Read VIA PCI north bridge config register
 */
static unsigned char via_pci_read_reg(unsigned char reg)
{
    return pci_read_config_byte(0, 0, 0, reg);
}

/*
 * Write VIA PCI north bridge config register
 */
static void via_pci_write_reg(unsigned char reg, unsigned char val)
{
    pci_write_config_byte(0, 0, 0, reg, val);
}

/*
 * Read VIA PCI NC region
 *
 * NC encoding:
 *   Byte 0: Base in 64KB units
 *   Byte 1: Bits 7:4 = size code (0=off, 1=64K, 2=128K, etc.)
 *
 * Size: 64KB << (code - 1)
 * Base: byte0 << 6 = KB
 */
static int via_pci_nc_read(int idx, nc_region_t *r, int max_regions)
{
    unsigned char base_val, size_val;
    unsigned char size_code;

    if (idx < 0 || idx >= max_regions || !r) {
        return HAL_ERR_PARAM;
    }

    r->base_kb = 0;
    r->size_kb = 0;
    r->active = 0;

    /* Each region uses 2 bytes starting at 0x58 */
    base_val = via_pci_read_reg(VIA_NC_BASE_REG + (idx * 2));
    size_val = via_pci_read_reg(VIA_NC_BASE_REG + (idx * 2) + 1);

    size_code = size_val >> 4;

    if (size_code != 0) {
        r->active = 1;
        /* Base: 64KB units */
        r->base_kb = (unsigned long)base_val << 6;
        /* Size: 64KB << (code - 1) */
        r->size_kb = 64UL << (size_code - 1);
    }

    return HAL_OK;
}

/*
 * Write VIA PCI NC region
 */
static int via_pci_nc_write(int idx, unsigned long base_kb, unsigned long size_kb, int max_regions)
{
    unsigned char base_val, size_val;
    unsigned char size_code;

    if (idx < 0 || idx >= max_regions) {
        return HAL_ERR_PARAM;
    }

    /* Validate alignment (64KB minimum) */
    if (base_kb % 64 != 0) {
        return HAL_ERR_PARAM;
    }

    /* Calculate size code */
    if (size_kb == 0) {
        size_code = 0;  /* Disabled */
    } else if (size_kb <= 64) {
        size_code = 1;
    } else if (size_kb <= 128) {
        size_code = 2;
    } else if (size_kb <= 256) {
        size_code = 3;
    } else if (size_kb <= 512) {
        size_code = 4;
    } else if (size_kb <= 1024) {
        size_code = 5;
    } else if (size_kb <= 2048) {
        size_code = 6;
    } else {
        size_code = 7;  /* 4MB max */
    }

    /* Encode base: 64KB units */
    base_val = (unsigned char)(base_kb >> 6);
    /* Size in upper nibble, lower nibble reserved (write 0) */
    size_val = size_code << 4;

    /* Write registers */
    via_pci_write_reg(VIA_NC_BASE_REG + (idx * 2), base_val);
    via_pci_write_reg(VIA_NC_BASE_REG + (idx * 2) + 1, size_val);

    return HAL_OK;
}

/*
 * Clear VIA PCI NC region
 */
static int via_pci_nc_clear(int idx, int max_regions)
{
    if (idx < 0 || idx >= max_regions) {
        return HAL_ERR_PARAM;
    }

    via_pci_write_reg(VIA_NC_BASE_REG + (idx * 2), 0x00);
    via_pci_write_reg(VIA_NC_BASE_REG + (idx * 2) + 1, 0x00);

    return HAL_OK;
}

/*
 * Read VIA PCI register for F4 dump
 */
static int via_pci_reg_read(int reg)
{
    if (reg < 0 || reg > 0xFF) {
        return -1;
    }
    return via_pci_read_reg((unsigned char)reg);
}

/*
 * Write VIA PCI register
 */
static int via_pci_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 0xFF || val < 0 || val > 0xFF) {
        return HAL_ERR_PARAM;
    }
    via_pci_write_reg((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

/*============================================================================
 * VIA VT82C570 (VP1) - Pentium PCI Chipset
 *
 * Features:
 * - Write-through cache (early Pentium design)
 * - 2 NC regions at PCI 0x58-0x5B
 * - 64KB granularity
 * - Cache control at PCI 0x50, bit 2
 *============================================================================*/

static int via_vp1_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) {
        return 0;
    }

    /* Read vendor/device ID at bus 0, device 0, function 0 */
    id = pci_read_config_dword(0, 0, 0, 0x00);

    /* Check for VIA VP1: vendor 0x1106, device 0x0570 */
    return (id == ((unsigned long)VIA_VP1_DEVICE_ID << 16 | VIA_VENDOR_ID)) ? 1 : 0;
}

static int via_vp1_cache_get(void)
{
    unsigned char val = via_pci_read_reg(VIA_VP1_CACHE_REG);
    return (val & VIA_VP1_CACHE_BIT) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int via_vp1_cache_set(int enable)
{
    unsigned char val = via_pci_read_reg(VIA_VP1_CACHE_REG);

    if (enable) {
        val |= VIA_VP1_CACHE_BIT;
    } else {
        val &= ~VIA_VP1_CACHE_BIT;
    }

    via_pci_write_reg(VIA_VP1_CACHE_REG, val);
    return HAL_OK;
}

static int via_vp1_nc_read(int idx, nc_region_t *r)
{
    return via_pci_nc_read(idx, r, 2);
}

static int via_vp1_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    return via_pci_nc_write(idx, base_kb, size_kb, 2);
}

static int via_vp1_nc_clear(int idx)
{
    return via_pci_nc_clear(idx, 2);
}

const chipset_ops_t ops_via_vp1 = {
    /* Identity */
    .name = "VIA VT82C570 (VP1)",
    .vendor = "VIA",
    .tier = "B",
    .score_x10 = 76,

    /* Detection */
    .probe = via_vp1_probe,

    /* Cache control */
    .cache_get = via_vp1_cache_get,
    .cache_set = via_vp1_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 0,  /* Write-through only */

    /* NC regions */
    .nc_count = 2,
    .nc_granularity = 64,
    .nc_max_kb = 4096,
    .nc_read = via_vp1_nc_read,
    .nc_write = via_vp1_nc_write,
    .nc_clear = via_vp1_nc_clear,

    /* Shadow RAM (info-only) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 256,
    .reg_base = 0x00,
    .reg_read = via_pci_reg_read,
    .reg_write = via_pci_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0,
    .data_port = 0
};

/*============================================================================
 * VIA VT82C597 (Apollo VP3) - Super7 PCI/AGP Chipset
 *
 * Features:
 * - Write-back cache support
 * - 4 NC regions at PCI 0x58-0x5F
 * - 64KB granularity
 * - Cache control at PCI 0x52, bit 0
 * - AGP support
 *============================================================================*/

static int via_vp3_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) {
        return 0;
    }

    /* Read vendor/device ID */
    id = pci_read_config_dword(0, 0, 0, 0x00);

    /* Check for VIA VP3: vendor 0x1106, device 0x0597 */
    return (id == ((unsigned long)VIA_VP3_DEVICE_ID << 16 | VIA_VENDOR_ID)) ? 1 : 0;
}

static int via_vp3_cache_get(void)
{
    unsigned char val = via_pci_read_reg(VIA_VP3_CACHE_REG);
    return (val & VIA_VP3_CACHE_BIT) ? CACHE_ENABLED : CACHE_DISABLED;
}

static int via_vp3_cache_set(int enable)
{
    unsigned char val = via_pci_read_reg(VIA_VP3_CACHE_REG);

    if (enable) {
        val |= VIA_VP3_CACHE_BIT;
    } else {
        val &= ~VIA_VP3_CACHE_BIT;
    }

    via_pci_write_reg(VIA_VP3_CACHE_REG, val);
    return HAL_OK;
}

static int via_vp3_nc_read(int idx, nc_region_t *r)
{
    return via_pci_nc_read(idx, r, 4);
}

static int via_vp3_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    return via_pci_nc_write(idx, base_kb, size_kb, 4);
}

static int via_vp3_nc_clear(int idx)
{
    return via_pci_nc_clear(idx, 4);
}

const chipset_ops_t ops_via_vp3 = {
    /* Identity */
    .name = "VIA Apollo VP3",
    .vendor = "VIA",
    .tier = "A",
    .score_x10 = 87,

    /* Detection */
    .probe = via_vp3_probe,

    /* Cache control */
    .cache_get = via_vp3_cache_get,
    .cache_set = via_vp3_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions */
    .nc_count = 4,
    .nc_granularity = 64,
    .nc_max_kb = 4096,
    .nc_read = via_vp3_nc_read,
    .nc_write = via_vp3_nc_write,
    .nc_clear = via_vp3_nc_clear,

    /* Shadow RAM (info-only) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 256,
    .reg_base = 0x00,
    .reg_read = via_pci_reg_read,
    .reg_write = via_pci_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0,
    .data_port = 0
};

/*============================================================================
 * VIA VT82C598 (Apollo MVP3) - Super7 PCI/AGP Chipset
 *
 * Features:
 * - Write-back cache support
 * - 4 NC regions at PCI 0x58-0x5F
 * - 64KB granularity
 * - Cache control at PCI 0x52, bit 0 (same as VP3)
 * - AGP 2x support
 * - Improved memory performance over VP3
 *============================================================================*/

static int via_mvp3_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) {
        return 0;
    }

    /* Read vendor/device ID */
    id = pci_read_config_dword(0, 0, 0, 0x00);

    /* Check for VIA MVP3: vendor 0x1106, device 0x0598 */
    return (id == ((unsigned long)VIA_MVP3_DEVICE_ID << 16 | VIA_VENDOR_ID)) ? 1 : 0;
}

/* MVP3 uses same cache control as VP3 */
static int via_mvp3_nc_read(int idx, nc_region_t *r)
{
    return via_pci_nc_read(idx, r, 4);
}

static int via_mvp3_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    return via_pci_nc_write(idx, base_kb, size_kb, 4);
}

static int via_mvp3_nc_clear(int idx)
{
    return via_pci_nc_clear(idx, 4);
}

const chipset_ops_t ops_via_mvp3 = {
    /* Identity */
    .name = "VIA Apollo MVP3",
    .vendor = "VIA",
    .tier = "S",
    .score_x10 = 92,

    /* Detection */
    .probe = via_mvp3_probe,

    /* Cache control (same as VP3) */
    .cache_get = via_vp3_cache_get,
    .cache_set = via_vp3_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions */
    .nc_count = 4,
    .nc_granularity = 64,
    .nc_max_kb = 4096,
    .nc_read = via_mvp3_nc_read,
    .nc_write = via_mvp3_nc_write,
    .nc_clear = via_mvp3_nc_clear,

    /* Shadow RAM (info-only) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 gate */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Register access */
    .reg_count = 256,
    .reg_base = 0x00,
    .reg_read = via_pci_reg_read,
    .reg_write = via_pci_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0,
    .data_port = 0
};

/*============================================================================
 * END OF CK_VIA.C
 *============================================================================*/
