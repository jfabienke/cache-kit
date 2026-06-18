/*============================================================================
 * CK_INTEL.C - Intel Chipset HAL Implementations
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 10:30:00 EST
 *
 * Supported chipsets:
 *   - Intel 430FX (Triton)        - PCI, Pentium, 1995
 *   - Intel 430HX (Triton II)     - PCI, Pentium Pro, 1996
 *   - Intel 430VX                 - PCI, Pentium, 1996
 *   - Intel 430TX                 - PCI, Pentium MMX, 1997
 *   - Intel 430MX (Mobile)        - PCI, Mobile Pentium, 1996
 *   - Intel 82350DT (Mongoose)    - EISA, 486, 1992
 *   - Intel 420TX/ZX (Saturn)     - EISA/PCI, 486/Pentium, 1992
 *   - Intel 430LX/NX (Mercury/Neptune) - EISA/PCI, Pentium, 1993
 *   - Intel 440FX (Natoma)        - PCI, Pentium Pro/II, 1996
 *   - Intel 450GX (Orion)         - EISA/PCI, Pentium Pro Server, 1995
 *
 * All Triton-era chipsets use:
 *   - PCI config space for register access
 *   - PCON register (0x52) bit 0 for cache enable
 *   - PAM registers (0x59-0x5F) for shadow RAM control
 *   - No traditional NC regions (PAM controls cacheability per-segment)
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
 * INTEL PCI VENDOR AND DEVICE IDS
 *============================================================================*/

#define INTEL_VENDOR_ID     0x8086

/* Host bridge (PCMC) device IDs - Triton family */
#define INTEL_430FX_DEV     0x122D  /* 82437FX TSC */
#define INTEL_430HX_DEV     0x1250  /* 82439HX TXC */
#define INTEL_430VX_DEV     0x7030  /* 82437VX TVX */
#define INTEL_430TX_DEV     0x7100  /* 82439TX MTXC */
#define INTEL_430MX_DEV     0x1235  /* 82437MX MTSC */

/* Host bridge (PCMC) device IDs - Early EISA PCIsets */
#define INTEL_82424_DEV     0x0483  /* 82424ZX - 420TX/ZX Saturn */
#define INTEL_82434_DEV     0x04A3  /* 82434LX/NX - 430LX/NX */
#define INTEL_82441FX_DEV   0x1237  /* 82441FX PMC - 440FX Natoma */
#define INTEL_82454GX_DEV   0x84C4  /* 82454GX PXB - 450GX Orion */

/*============================================================================
 * INTEL TRITON REGISTER DEFINITIONS
 *
 * All Triton-era chipsets share similar register layouts for cache and
 * shadow RAM control. The host bridge is always at PCI 0:0:0.
 *============================================================================*/

/* PCON - Processor Configuration Register (offset 0x52) */
#define TRITON_PCON_REG         0x52
#define TRITON_PCON_CACHE_EN    0x01    /* Bit 0: L2 cache enable */

/* PAM - Programmable Attribute Map Registers (offsets 0x59-0x5F) */
#define TRITON_PAM0_REG         0x59    /* PAM0: 0F0000-0FFFFF (BIOS) */
#define TRITON_PAM1_REG         0x5A    /* PAM1: 0C0000-0C3FFF, 0C4000-0C7FFF */
#define TRITON_PAM2_REG         0x5B    /* PAM2: 0C8000-0CBFFF, 0CC000-0CFFFF */
#define TRITON_PAM3_REG         0x5C    /* PAM3: 0D0000-0D3FFF, 0D4000-0D7FFF */
#define TRITON_PAM4_REG         0x5D    /* PAM4: 0D8000-0DBFFF, 0DC000-0DFFFF */
#define TRITON_PAM5_REG         0x5E    /* PAM5: 0E0000-0E3FFF, 0E4000-0E7FFF */
#define TRITON_PAM6_REG         0x5F    /* PAM6: 0E8000-0EBFFF, 0EC000-0EFFFF */

/* PAM attribute values (per nibble: low=lower 16KB, high=upper 16KB) */
#define PAM_DISABLED    0x0     /* DRAM disabled, accesses go to PCI */
#define PAM_READ_ONLY   0x1     /* Read from DRAM, write to PCI (shadow) */
#define PAM_WRITE_ONLY  0x2     /* Read from PCI, write to DRAM */
#define PAM_READ_WRITE  0x3     /* Read/write DRAM (shadow complete) */

/*============================================================================
 * INTEL 82350DT (MONGOOSE) REGISTER DEFINITIONS
 *
 * Uses 82359 DRAM controller chip, accessed via legacy 0x22/0x23 ports.
 *============================================================================*/

#define MONGOOSE_ID_REG         0x21    /* Chip ID register (reads 0x01) */
#define MONGOOSE_CACHE_REG      0x07    /* Cache control register */
#define MONGOOSE_CACHE_EN       0x01    /* Bit 0: Cache enable */

/*============================================================================
 * TRITON COMMON FUNCTIONS
 *============================================================================*/

/*
 * Read the PCON register and return cache state flags.
 */
static int triton_cache_get(void)
{
    unsigned char pcon = pci_read_config_byte(0, 0, 0, TRITON_PCON_REG);
    int flags = 0;

    if (pcon & TRITON_PCON_CACHE_EN) {
        flags = CACHE_ENABLED | CACHE_WRITEBACK;
    }

    return flags;
}

/*
 * Enable or disable L2 cache via PCON register.
 */
static int triton_cache_set(int enable)
{
    unsigned char pcon = pci_read_config_byte(0, 0, 0, TRITON_PCON_REG);

    if (enable) {
        pcon |= TRITON_PCON_CACHE_EN;
    } else {
        pcon &= ~TRITON_PCON_CACHE_EN;
    }

    pci_write_config_byte(0, 0, 0, TRITON_PCON_REG, pcon);
    return HAL_OK;
}

/*
 * Read a PAM register and decode shadow state for a region.
 * region: 0-6 corresponding to PAM0-PAM6
 * nibble: 0=low nibble, 1=high nibble (for PAM1-PAM6 which cover two 16KB areas)
 */
static int triton_shadow_get_pam(int pam_reg, int nibble)
{
    unsigned char pam = pci_read_config_byte(0, 0, 0, pam_reg);
    unsigned char attr;

    if (nibble) {
        attr = (pam >> 4) & 0x0F;
    } else {
        attr = pam & 0x0F;
    }

    switch (attr) {
        case PAM_READ_ONLY:
            return SHADOW_RO;
        case PAM_READ_WRITE:
            return SHADOW_RW;
        default:
            return SHADOW_DISABLED;
    }
}

/*
 * Get shadow state for standard regions.
 * region: 0=C000, 1=C800, 2=D000, 3=D800, 4=E000, 5=F000
 */
static int triton_shadow_get(int region)
{
    switch (region) {
        case 0: return triton_shadow_get_pam(TRITON_PAM1_REG, 0);  /* C0000-C7FFF */
        case 1: return triton_shadow_get_pam(TRITON_PAM2_REG, 0);  /* C8000-CFFFF */
        case 2: return triton_shadow_get_pam(TRITON_PAM3_REG, 0);  /* D0000-D7FFF */
        case 3: return triton_shadow_get_pam(TRITON_PAM4_REG, 0);  /* D8000-DFFFF */
        case 4: return triton_shadow_get_pam(TRITON_PAM5_REG, 0);  /* E0000-E7FFF */
        case 5: return triton_shadow_get_pam(TRITON_PAM6_REG, 0);  /* E8000-EFFFF */
        case 6: return triton_shadow_get_pam(TRITON_PAM0_REG, 0);  /* F0000-FFFFF */
        default: return HAL_ERR_PARAM;
    }
}

/*
 * Set shadow state for a region.
 * Note: This is a simplified implementation that sets both nibbles.
 */
static int triton_shadow_set(int region, int mode)
{
    unsigned char attr;
    unsigned char pam_reg;
    unsigned char pam;

    /* Convert mode to PAM attribute */
    switch (mode) {
        case SHADOW_DISABLED: attr = PAM_DISABLED; break;
        case SHADOW_RO: attr = PAM_READ_ONLY; break;
        case SHADOW_RW: attr = PAM_READ_WRITE; break;
        default: return HAL_ERR_PARAM;
    }

    /* Map region to PAM register. Region 5 (PAM6, E8000-EFFFF) was previously
       orphaned - the old map jumped from PAM5 straight to PAM0, so the upper
       half of the E segment could not be shadowed/cached at all (IS-H2). */
    switch (region) {
        case 0: pam_reg = TRITON_PAM1_REG; break;  /* C0000-C7FFF */
        case 1: pam_reg = TRITON_PAM2_REG; break;  /* C8000-CFFFF */
        case 2: pam_reg = TRITON_PAM3_REG; break;  /* D0000-D7FFF */
        case 3: pam_reg = TRITON_PAM4_REG; break;  /* D8000-DFFFF */
        case 4: pam_reg = TRITON_PAM5_REG; break;  /* E0000-E7FFF */
        case 5: pam_reg = TRITON_PAM6_REG; break;  /* E8000-EFFFF */
        case 6: pam_reg = TRITON_PAM0_REG; break;  /* F0000-FFFFF */
        default: return HAL_ERR_PARAM;
    }

    /* Set both nibbles to same attribute (covers full 32KB) */
    pam = (attr << 4) | attr;
    pci_write_config_byte(0, 0, 0, pam_reg, pam);

    return HAL_OK;
}

/*
 * Intel Triton doesn't have traditional NC regions.
 * The PAM registers control cacheability per ROM segment.
 */
static int triton_nc_read(int idx, nc_region_t *r)
{
    (void)idx;
    if (r) {
        r->base_kb = 0;
        r->size_kb = 0;
        r->active = 0;
    }
    return HAL_ERR_UNSUP;
}

/*
 * Raw register read for F4 screen.
 * Triton registers 0x00-0xFF in PCI config space.
 */
static int triton_reg_read(int reg)
{
    if (reg < 0 || reg > 255) return -1;
    return pci_read_config_byte(0, 0, 0, (unsigned char)reg);
}

static int triton_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 255) return HAL_ERR_PARAM;
    pci_write_config_byte(0, 0, 0, (unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

/*============================================================================
 * INTEL 430FX (TRITON) IMPLEMENTATION
 *============================================================================*/

static int i430fx_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) return 0;

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)INTEL_430FX_DEV << 16 | INTEL_VENDOR_ID));
}

const chipset_ops_t ops_intel_430fx = {
    /* Identity */
    .name = "Intel 430FX Triton",
    .vendor = "Intel",
    .tier = "S",
    .score_x10 = 94,

    /* Detection */
    .probe = i430fx_probe,

    /* Cache */
    .cache_get = triton_cache_get,
    .cache_set = triton_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (not supported - uses PAM) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = triton_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM (6 regions via PAM) */
    .shadow_regions = 7,    /* C/D/E (incl. E8000 via PAM6) + F segments */
    .shadow_get = triton_shadow_get,
    .shadow_set = triton_shadow_set,

    /* A20 (use standard Port 0x92) */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = triton_reg_read,
    .reg_write = triton_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};

/*============================================================================
 * INTEL 430HX (TRITON II) IMPLEMENTATION
 *============================================================================*/

static int i430hx_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) return 0;

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)INTEL_430HX_DEV << 16 | INTEL_VENDOR_ID));
}

const chipset_ops_t ops_intel_430hx = {
    /* Identity */
    .name = "Intel 430HX Triton II",
    .vendor = "Intel",
    .tier = "S",
    .score_x10 = 96,

    /* Detection */
    .probe = i430hx_probe,

    /* Cache */
    .cache_get = triton_cache_get,
    .cache_set = triton_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (not supported) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = triton_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 7,    /* C/D/E (incl. E8000 via PAM6) + F segments */
    .shadow_get = triton_shadow_get,
    .shadow_set = triton_shadow_set,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = triton_reg_read,
    .reg_write = triton_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};

/*============================================================================
 * INTEL 430VX IMPLEMENTATION
 *============================================================================*/

static int i430vx_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) return 0;

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)INTEL_430VX_DEV << 16 | INTEL_VENDOR_ID));
}

const chipset_ops_t ops_intel_430vx = {
    /* Identity */
    .name = "Intel 430VX",
    .vendor = "Intel",
    .tier = "A",
    .score_x10 = 88,

    /* Detection */
    .probe = i430vx_probe,

    /* Cache */
    .cache_get = triton_cache_get,
    .cache_set = triton_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (not supported) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = triton_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 7,    /* C/D/E (incl. E8000 via PAM6) + F segments */
    .shadow_get = triton_shadow_get,
    .shadow_set = triton_shadow_set,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = triton_reg_read,
    .reg_write = triton_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};

/*============================================================================
 * INTEL 430TX IMPLEMENTATION
 *============================================================================*/

static int i430tx_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) return 0;

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)INTEL_430TX_DEV << 16 | INTEL_VENDOR_ID));
}

const chipset_ops_t ops_intel_430tx = {
    /* Identity */
    .name = "Intel 430TX",
    .vendor = "Intel",
    .tier = "S",
    .score_x10 = 93,

    /* Detection */
    .probe = i430tx_probe,

    /* Cache */
    .cache_get = triton_cache_get,
    .cache_set = triton_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (not supported) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = triton_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 7,    /* C/D/E (incl. E8000 via PAM6) + F segments */
    .shadow_get = triton_shadow_get,
    .shadow_set = triton_shadow_set,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = triton_reg_read,
    .reg_write = triton_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};

/*============================================================================
 * INTEL 430MX (MOBILE) IMPLEMENTATION
 *============================================================================*/

static int i430mx_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) return 0;

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)INTEL_430MX_DEV << 16 | INTEL_VENDOR_ID));
}

const chipset_ops_t ops_intel_430mx = {
    /* Identity */
    .name = "Intel 430MX Mobile",
    .vendor = "Intel",
    .tier = "A",
    .score_x10 = 86,

    /* Detection */
    .probe = i430mx_probe,

    /* Cache */
    .cache_get = triton_cache_get,
    .cache_set = triton_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (not supported) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = triton_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 7,    /* C/D/E (incl. E8000 via PAM6) + F segments */
    .shadow_get = triton_shadow_get,
    .shadow_set = triton_shadow_set,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = triton_reg_read,
    .reg_write = triton_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};

/*============================================================================
 * INTEL 82350DT (MONGOOSE) IMPLEMENTATION
 *
 * EISA-era 486 chipset with 82359 DRAM controller.
 * Uses legacy index/data ports (0x22/0x23) for register access.
 * Info-only: Cache control register is read-only in most BIOSes.
 *============================================================================*/

static int i82350dt_probe(void)
{
    unsigned char chip_id;

    /* Probe 82359 at index 0x21 (chip ID register) */
    chip_id = legacy_read_22_23(MONGOOSE_ID_REG);

    /* Chip ID 0x01 = 82359 DRAM controller present */
    return (chip_id == 0x01);
}

static int i82350dt_cache_get(void)
{
    unsigned char reg = legacy_read_22_23(MONGOOSE_CACHE_REG);
    return (reg & MONGOOSE_CACHE_EN) ? CACHE_ENABLED : CACHE_DISABLED;
}

/* 82350DT is typically info-only (BIOS controls cache) */
static int i82350dt_cache_set(int enable)
{
    (void)enable;
    return HAL_ERR_UNSUP;
}

static int i82350dt_reg_read(int reg)
{
    if (reg < 0 || reg > 255) return -1;
    return legacy_read_22_23((unsigned char)reg);
}

static int i82350dt_reg_write(int reg, int val)
{
    if (reg < 0 || reg > 255) return HAL_ERR_PARAM;
    legacy_write_22_23((unsigned char)reg, (unsigned char)val);
    return HAL_OK;
}

const chipset_ops_t ops_intel_82350dt = {
    /* Identity */
    .name = "Intel 82350DT Mongoose",
    .vendor = "Intel",
    .tier = "B",
    .score_x10 = 72,

    /* Detection */
    .probe = i82350dt_probe,

    /* Cache (info-only) */
    .cache_get = i82350dt_cache_get,
    .cache_set = i82350dt_cache_set,
    .cache_flush = generic_invd_flush,      /* 486 era, use INVD not WBINVD */
    .is_writeback = 0,

    /* NC regions (not supported) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = hal_stub_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM (not supported) */
    .shadow_regions = 0,
    .shadow_get = hal_stub_unsupported_i,
    .shadow_set = hal_stub_unsupported_ii,

    /* A20 (use standard Port 0x92) */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 64,        /* 82359 has fewer registers */
    .reg_base = 0,
    .reg_read = i82350dt_reg_read,
    .reg_write = i82350dt_reg_write,

    /* Metadata */
    .info_only = 1,
    .index_port = 0x22,
    .data_port = 0x23
};

/*============================================================================
 * INTEL 420TX/420ZX (SATURN) IMPLEMENTATION
 *
 * First-generation Intel EISA PCIset (1992-1993).
 * Uses 82424ZX Cache/DRAM Controller with 82423TX Data Path Unit.
 * Cache control similar to Triton at register 0x52.
 *============================================================================*/

static int i420zx_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) return 0;

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)INTEL_82424_DEV << 16 | INTEL_VENDOR_ID));
}

const chipset_ops_t ops_intel_420zx = {
    /* Identity */
    .name = "Intel 420TX/ZX Saturn",
    .vendor = "Intel",
    .tier = "A",
    .score_x10 = 80,

    /* Detection */
    .probe = i420zx_probe,

    /* Cache */
    .cache_get = triton_cache_get,
    .cache_set = triton_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (not supported - uses PAM) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = triton_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 7,    /* C/D/E (incl. E8000 via PAM6) + F segments */
    .shadow_get = triton_shadow_get,
    .shadow_set = triton_shadow_set,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = triton_reg_read,
    .reg_write = triton_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};

/*============================================================================
 * INTEL 430LX/430NX (MERCURY/NEPTUNE) IMPLEMENTATION
 *
 * Second-generation Intel EISA PCIset (1993-1994).
 * Uses 82434LX/NX PCMC. Both share device ID 0x04A3.
 * 430LX = Mercury (write-through), 430NX = Neptune (write-back).
 * Cache control at register 0x52 bit 0.
 *============================================================================*/

static int i430nx_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) return 0;

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)INTEL_82434_DEV << 16 | INTEL_VENDOR_ID));
}

const chipset_ops_t ops_intel_430nx = {
    /* Identity */
    .name = "Intel 430LX/NX Mercury/Neptune",
    .vendor = "Intel",
    .tier = "A",
    .score_x10 = 85,

    /* Detection */
    .probe = i430nx_probe,

    /* Cache */
    .cache_get = triton_cache_get,
    .cache_set = triton_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (not supported - uses PAM) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = triton_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 7,    /* C/D/E (incl. E8000 via PAM6) + F segments */
    .shadow_get = triton_shadow_get,
    .shadow_set = triton_shadow_set,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = triton_reg_read,
    .reg_write = triton_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};

/*============================================================================
 * INTEL 440FX (NATOMA) IMPLEMENTATION
 *
 * Pentium Pro/II EISA PCIset (1996).
 * Uses 82441FX PMC + 82442FX DBA (Data Bus Accelerator).
 * Cache control at register 0x52 bit 0.
 *============================================================================*/

static int i440fx_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) return 0;

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)INTEL_82441FX_DEV << 16 | INTEL_VENDOR_ID));
}

const chipset_ops_t ops_intel_440fx = {
    /* Identity */
    .name = "Intel 440FX Natoma",
    .vendor = "Intel",
    .tier = "S",
    .score_x10 = 94,

    /* Detection */
    .probe = i440fx_probe,

    /* Cache */
    .cache_get = triton_cache_get,
    .cache_set = triton_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (not supported - uses PAM) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = triton_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 7,    /* C/D/E (incl. E8000 via PAM6) + F segments */
    .shadow_get = triton_shadow_get,
    .shadow_set = triton_shadow_set,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = triton_reg_read,
    .reg_write = triton_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};

/*============================================================================
 * INTEL 450GX (ORION) IMPLEMENTATION
 *
 * Server-class EISA PCIset (1995).
 * Four-chip solution: 82454GX PXB + 82452GX + 82453GX + 82451GX.
 * Cache control at register 0x52 bit 0.
 *============================================================================*/

static int i450gx_probe(void)
{
    unsigned long id;

    if (!pci_bus_present()) return 0;

    id = pci_read_config_dword(0, 0, 0, 0x00);
    return (id == ((unsigned long)INTEL_82454GX_DEV << 16 | INTEL_VENDOR_ID));
}

const chipset_ops_t ops_intel_450gx = {
    /* Identity */
    .name = "Intel 450GX Orion",
    .vendor = "Intel",
    .tier = "S",
    .score_x10 = 95,

    /* Detection */
    .probe = i450gx_probe,

    /* Cache */
    .cache_get = triton_cache_get,
    .cache_set = triton_cache_set,
    .cache_flush = generic_wbinvd_flush,
    .is_writeback = 1,

    /* NC regions (not supported - uses PAM) */
    .nc_count = 0,
    .nc_granularity = 0,
    .nc_max_kb = 0,
    .nc_read = triton_nc_read,
    .nc_write = hal_stub_unsupported_iull,
    .nc_clear = hal_stub_unsupported_i,

    /* Shadow RAM */
    .shadow_regions = 7,    /* C/D/E (incl. E8000 via PAM6) + F segments */
    .shadow_get = triton_shadow_get,
    .shadow_set = triton_shadow_set,

    /* A20 */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers */
    .reg_count = 256,
    .reg_base = 0,
    .reg_read = triton_reg_read,
    .reg_write = triton_reg_write,

    /* Metadata */
    .info_only = 0,
    .index_port = 0xCF8,
    .data_port = 0xCFC
};
