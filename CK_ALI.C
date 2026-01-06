/*============================================================================
 * CK_ALI.C - ALi (Acer Labs) Chipset HAL Implementations
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 09:00:00 EST
 *
 * Supported Chipsets:
 * - ALi M1489 (Aladdin IV) - 486 PCI, WB, 2 NC regions, 64KB min
 * - ALi M1541 (Aladdin V)  - Super7 PCI/AGP, WB, 4 NC regions, 64KB min
 *
 * ALi Register Layout:
 * - Cache control at PCI 0x42, bit 0
 * - NC regions at PCI 0x50-0x57 (Aladdin IV: 2, Aladdin V: 4)
 * - 64KB granularity for NC regions
 *============================================================================*/

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

/* From CK_HAL.C */
extern int generic_wbinvd_flush(void);
extern int generic_port92_a20_get(void);
extern int generic_port92_a20_set(int enable);
extern int hal_stub_unsupported_i(int x);
extern int hal_stub_unsupported_ii(int x, int y);

/*============================================================================
 * ALi PCI IDS
 *============================================================================*/

#define ALI_VENDOR_ID           0x10B9

/* North Bridge device IDs */
#define ALI_M1489_DEVICE_ID     0x1489  /* Aladdin IV */
#define ALI_M1541_DEVICE_ID     0x1541  /* Aladdin V */

/*============================================================================
 * ALi PCI REGISTER DEFINITIONS
 *
 * Both Aladdin IV and V use similar register layouts:
 *
 * 0x42: CPU/Cache Control
 *       Bit 0: L2 Cache Enable (1=enabled)
 *
 * NC Region encoding (64KB granularity):
 *   Aladdin IV: 0x50-0x53 (2 regions, each 2 bytes)
 *   Aladdin V:  0x50-0x57 (4 regions, each 2 bytes)
 *
 *   Byte 0: Base address in 64KB units (bits 7:0 = A23:A16)
 *   Byte 1: Bits 7:4 = Size code (0=disabled, 1=64K, 2=128K, etc.)
 *           Bits 3:0 = reserved
 *
 * Size: 64KB << (size_code - 1)
 * Base: byte0 << 6 = KB
 *============================================================================*/

#define ALI_CACHE_REG           0x42    /* Cache control register */
#define ALI_CACHE_ENABLE        0x01    /* Bit 0: L2 cache enable */
#define ALI_NC_BASE_REG         0x50    /* NC regions start here */

/*============================================================================
 * ALi SHARED HELPER FUNCTIONS
 *============================================================================*/

/*
 * Read ALi PCI north bridge config register
 */
static unsigned char ali_pci_read_reg(unsigned char reg)
{
    return pci_read_config_byte(0, 0, 0, reg);
}

/*
 * Write ALi PCI north bridge config register
 */
static void ali_pci_write_reg(unsigned char reg, unsigned char val)
{
    pci_write_config_byte(0, 0, 0, reg, val);
}

/*
 * Get cache state for ALi PCI chipsets
 */
static int ali_cache_get(void)
{
    unsigned char val = ali_pci_read_reg(ALI_CACHE_REG);
    return (val & ALI_CACHE_ENABLE) ? CACHE_ENABLED : CACHE_DISABLED;
}

/*
 * Set cache state for ALi PCI chipsets
 */
static int ali_cache_set(int enable)
{
    unsigned char val = ali_pci_read_reg(ALI_CACHE_REG);

    if (enable) {
        val |= ALI_CACHE_ENABLE;
    } else {
        val &= ~ALI_CACHE_ENABLE;
    }

    ali_pci_write_reg(ALI_CACHE_REG, val);
    return HAL_OK;
}

/*
 * Read ALi NC region configuration
 *
 * NC encoding (same as VIA PCI chipsets):
 *   Byte 0: Base in 64KB units
 *   Byte 1: Bits 7:4 = size code (0=off, 1=64K, 2=128K, etc.)
 */
static int ali_nc_read(int idx, nc_region_t *r, int max_regions)
{
    unsigned char base_val, size_val;
    unsigned char size_code;

    if (idx < 0 || idx >= max_regions || !r) {
        return HAL_ERR_PARAM;
    }

    r->base_kb = 0;
    r->size_kb = 0;
    r->active = 0;

    /* Each region uses 2 bytes starting at 0x50 */
    base_val = ali_pci_read_reg(ALI_NC_BASE_REG + (idx * 2));
    size_val = ali_pci_read_reg(ALI_NC_BASE_REG + (idx * 2) + 1);

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
 * Write ALi NC region configuration
 */
static int ali_nc_write(int idx, unsigned long base_kb, unsigned long size_kb, int max_regions)
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
    /* Size in upper nibble */
    size_val = size_code << 4;

    /* Write registers */
    ali_pci_write_reg(ALI_NC_BASE_REG + (idx * 2), base_val);
    ali_pci_write_reg(ALI_NC_BASE_REG + (idx * 2) + 1, size_val);

    return HAL_OK;
}

/*
 * Clear ALi NC region
 */
static int ali_nc_clear(int idx, int max_regions)
{
    if (idx < 0 || idx >= max_regions) {
        return HAL_ERR_PARAM;
    }

    ali_pci_write_reg(ALI_NC_BASE_REG + (idx * 2), 0x00);
    ali_pci_write_reg(ALI_NC_BASE_REG + (idx * 2) + 1, 0x00);

    return HAL_OK;
}

/*
 * Read ALi PCI register for F4 dump
 */
static int ali_reg_read(int reg)
{
    if (reg < 0 || reg > 0xFF) {
        return -1;
    }
    return ali_pci_read_reg((unsigned char)reg);
}

/*
 * Write ALi PCI register
 */
static int ali_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 0xFF || val < 0 || val > 0xFF) {
        return HAL_ERR_PARAM;
    }
    ali_pci_write_reg((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

/*============================================================================
 * ALi M1489 (Aladdin IV) - 486 PCI Chipset
 *
 * Features:
 * - Write-back L2 cache support
 * - 2 NC regions at PCI 0x50-0x53
 * - 64KB granularity
 *============================================================================*/

static int ali_m1489_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) {
        return 0;
    }

    /* Read vendor/device ID at bus 0, device 0, function 0 */
    id = pci_read_config_dword(0, 0, 0, 0x00);

    /* Check for ALi M1489: vendor 0x10B9, device 0x1489 */
    return (id == ((unsigned long)ALI_M1489_DEVICE_ID << 16 | ALI_VENDOR_ID)) ? 1 : 0;
}

static int ali_m1489_nc_read(int idx, nc_region_t *r)
{
    return ali_nc_read(idx, r, 2);
}

static int ali_m1489_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    return ali_nc_write(idx, base_kb, size_kb, 2);
}

static int ali_m1489_nc_clear(int idx)
{
    return ali_nc_clear(idx, 2);
}

const chipset_ops_t ops_ali_aladdin4 = {
    /* Identity */
    .name = "ALi M1489 (Aladdin IV)",
    .vendor = "ALi",
    .tier = "A",
    .score_x10 = 82,

    /* Detection */
    .probe = ali_m1489_probe,

    /* Cache control */
    .cache_get = ali_cache_get,
    .cache_set = ali_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions */
    .nc_count = 2,
    .nc_granularity = 64,
    .nc_max_kb = 4096,
    .nc_read = ali_m1489_nc_read,
    .nc_write = ali_m1489_nc_write,
    .nc_clear = ali_m1489_nc_clear,

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
    .reg_read = ali_reg_read,
    .reg_write = ali_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0,
    .data_port = 0
};

/*============================================================================
 * ALi M1541 (Aladdin V) - Super7 PCI/AGP Chipset
 *
 * Features:
 * - Write-back L2 cache support
 * - 4 NC regions at PCI 0x50-0x57
 * - 64KB granularity
 * - AGP support
 *============================================================================*/

static int ali_m1541_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) {
        return 0;
    }

    /* Read vendor/device ID */
    id = pci_read_config_dword(0, 0, 0, 0x00);

    /* Check for ALi M1541: vendor 0x10B9, device 0x1541 */
    return (id == ((unsigned long)ALI_M1541_DEVICE_ID << 16 | ALI_VENDOR_ID)) ? 1 : 0;
}

static int ali_m1541_nc_read(int idx, nc_region_t *r)
{
    return ali_nc_read(idx, r, 4);
}

static int ali_m1541_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    return ali_nc_write(idx, base_kb, size_kb, 4);
}

static int ali_m1541_nc_clear(int idx)
{
    return ali_nc_clear(idx, 4);
}

const chipset_ops_t ops_ali_aladdin5 = {
    /* Identity */
    .name = "ALi M1541 (Aladdin V)",
    .vendor = "ALi",
    .tier = "A",
    .score_x10 = 88,

    /* Detection */
    .probe = ali_m1541_probe,

    /* Cache control */
    .cache_get = ali_cache_get,
    .cache_set = ali_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions - 4 regions on Aladdin V */
    .nc_count = 4,
    .nc_granularity = 64,
    .nc_max_kb = 4096,
    .nc_read = ali_m1541_nc_read,
    .nc_write = ali_m1541_nc_write,
    .nc_clear = ali_m1541_nc_clear,

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
    .reg_read = ali_reg_read,
    .reg_write = ali_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0,
    .data_port = 0
};

/*============================================================================
 * END OF CK_ALI.C
 *============================================================================*/
