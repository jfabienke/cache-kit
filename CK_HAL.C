/*============================================================================
 * CK_HAL.C - CACHEKIT Hardware Abstraction Layer Core
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 11:30:00 EST
 *
 * Contains:
 * - Chipset detection registry and loop
 * - Unknown chipset fallback ops
 * - Generic helper implementations (WBINVD, Port 0x92 A20, stubs)
 *============================================================================*/

#include <conio.h>
#include <string.h>
#include "CK_HAL.H"

/*============================================================================
 * EXTERNAL DECLARATIONS FOR I/O FUNCTIONS (from CK_IO.C)
 *============================================================================*/

extern int pci_bus_present(void);
extern unsigned char io_read_byte(unsigned int port);
extern void io_write_byte(unsigned int port, unsigned char val);

/*============================================================================
 * GLOBAL HAL POINTER
 *============================================================================*/

const chipset_ops_t *g_hal = NULL;  /* Set by detect_chipset_hal() */

/*============================================================================
 * GENERIC HELPER IMPLEMENTATIONS
 *============================================================================*/

/*
 * Cache flush using WBINVD instruction (486+)
 * Writes back all modified cache lines, then invalidates.
 */
int generic_wbinvd_flush(void)
{
    /* Watcom inline assembly for WBINVD */
    #pragma aux do_wbinvd = \
        ".486" \
        "wbinvd" \
        modify exact [];
    extern void do_wbinvd(void);

    do_wbinvd();
    return HAL_OK;
}

/*
 * Cache flush using INVD instruction (386, no writeback)
 * WARNING: This invalidates WITHOUT writing back - data loss possible!
 * Only use on write-through caches or when cache is known clean.
 */
int generic_invd_flush(void)
{
    #pragma aux do_invd = \
        ".386" \
        "invd" \
        modify exact [];
    extern void do_invd(void);

    do_invd();
    return HAL_OK;
}

/*
 * A20 gate control via Port 0x92 (Fast A20)
 * Available on most AT-compatible systems from 386 onward.
 *
 * Port 0x92 bits:
 *   Bit 0: System reset (1 = reset, DANGEROUS!)
 *   Bit 1: A20 gate (1 = enabled)
 */
#define PORT_92         0x92
#define PORT_92_RESET   0x01
#define PORT_92_A20     0x02

int generic_port92_a20_get(void)
{
    unsigned char val = io_read_byte(PORT_92);
    return (val & PORT_92_A20) ? 1 : 0;
}

int generic_port92_a20_set(int enable)
{
    unsigned char val = io_read_byte(PORT_92);

    /* CRITICAL: Always clear reset bit to avoid system reset! */
    val &= ~PORT_92_RESET;

    if (enable) {
        val |= PORT_92_A20;
    } else {
        val &= ~PORT_92_A20;
    }

    io_write_byte(PORT_92, val);
    return HAL_OK;
}

/*============================================================================
 * STUB FUNCTIONS FOR UNSUPPORTED OPERATIONS
 *
 * These return HAL_ERR_UNSUP and can be used in ops structs for
 * features that a particular chipset doesn't support.
 *============================================================================*/

int hal_stub_unsupported(void)
{
    return HAL_ERR_UNSUP;
}

int hal_stub_unsupported_i(int x)
{
    (void)x;
    return HAL_ERR_UNSUP;
}

int hal_stub_unsupported_ii(int x, int y)
{
    (void)x;
    (void)y;
    return HAL_ERR_UNSUP;
}

int hal_stub_unsupported_iul(int x, unsigned long y)
{
    (void)x;
    (void)y;
    return HAL_ERR_UNSUP;
}

int hal_stub_unsupported_iull(int x, unsigned long y, unsigned long z)
{
    (void)x;
    (void)y;
    (void)z;
    return HAL_ERR_UNSUP;
}

int hal_stub_nc_read(int idx, nc_region_t *r)
{
    (void)idx;
    if (r) {
        r->base_kb = 0;
        r->size_kb = 0;
        r->active = 0;
    }
    return HAL_ERR_UNSUP;
}

/*============================================================================
 * UNKNOWN CHIPSET FALLBACK OPS
 *
 * Used when no chipset is detected. Provides minimal functionality
 * via Port 0x92 A20 and WBINVD cache flush.
 *============================================================================*/

static int unknown_probe(void)
{
    /* Never matches during detection - only used as fallback */
    return 0;
}

static int unknown_cache_get(void)
{
    /* Assume cache is enabled if we can't detect */
    return CACHE_ENABLED;
}

static int unknown_reg_read(int reg)
{
    (void)reg;
    return -1;  /* Invalid - no known registers */
}

const chipset_ops_t ops_unknown = {
    /* Identity */
    .name = "Unknown Chipset",
    .vendor = "Unknown",
    .tier = "?",
    .score_x10 = 0,

    /* Detection */
    .probe = unknown_probe,

    /* Cache (assume enabled, use WBINVD for flush) */
    .cache_get = unknown_cache_get,
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

    /* A20 (use Port 0x92 as fallback) */
    .a20_get = generic_port92_a20_get,
    .a20_set = generic_port92_a20_set,

    /* Registers (none) */
    .reg_count = 0,
    .reg_base = 0,
    .reg_read = unknown_reg_read,
    .reg_write = hal_stub_unsupported_ii,

    /* Metadata */
    .info_only = 1,
    .index_port = 0,
    .data_port = 0
};

/*============================================================================
 * CHIPSET DETECTION REGISTRY
 *
 * Array of all known chipset ops, in probe priority order:
 * 1. PCI chipsets first (fast PCI ID check)
 * 2. EISA chipsets
 * 3. Legacy ISA chipsets last (slower port probing)
 *
 * Detection stops at first successful probe.
 *============================================================================*/

static const chipset_ops_t * const g_chipset_registry[] = {
    /*--- MCA Bus (IBM PS/2) - probe early, distinct bus type ---*/
    &ops_mca_generic,

    /*--- PCI Chipsets (probe first - fast PCI ID check) ---*/

    /* Intel Triton family (CK_INTEL.C) */
    &ops_intel_430fx,
    &ops_intel_430hx,
    &ops_intel_430vx,
    &ops_intel_430tx,
    &ops_intel_430mx,

    /* Intel EISA PCIsets (CK_INTEL.C) */
    &ops_intel_440fx,
    &ops_intel_450gx,
    &ops_intel_420zx,
    &ops_intel_430nx,

    /* SiS PCI chipsets (CK_SIS.C) */
    &ops_sis_496,
    &ops_sis_5598,
    &ops_sis_530,
    &ops_sis_5591,

    /* VIA PCI chipsets (CK_VIA.C) */
    &ops_via_vp1,
    &ops_via_vp3,
    &ops_via_mvp3,

    /* OPTi PCI/VLB chipsets (CK_OPTI.C) */
    &ops_opti_viper,

    /* ALi PCI chipsets (CK_ALI.C) */
    &ops_ali_aladdin4,
    &ops_ali_aladdin5,

    /*--- EISA Chipsets ---*/
    &ops_intel_82350dt,   /* Intel 82350DT Mongoose (CK_INTEL.C) */
    &ops_sis_eisa,        /* SiS 85C411/406 (CK_SIS.C) */

    /* OPTi EISA chipsets - probe specific IDs before Viper pattern */
    &ops_opti_682,
    &ops_opti_683,
    &ops_opti_hunter,
    &ops_opti_pent_eisa,

    /*--- Legacy ISA Chipsets (probe last - slower port probing) ---*/

    /* SiS legacy chipsets (CK_SIS.C) */
    &ops_sis_460,
    &ops_sis_rabbit,

    /* OPTi legacy chipsets (CK_OPTI.C) */
    &ops_opti_391,
    &ops_opti_381,
    &ops_opti_212,

    /* VIA legacy chipsets (CK_VIA.C, CK_LEGAC.C) */
    &ops_via_vt82c495,
    &ops_via_vt82c310,

    /* Other 486-era chipsets (CK_OTHER.C) */
    &ops_umc_491,
    &ops_eteq_bengal,
    &ops_faraday,
    &ops_mic_9391,
    &ops_contaq_596,
    &ops_forex,
    &ops_suntac,

    /* 286/386-era legacy chipsets (CK_LEGAC.C) - probe last */
    &ops_ct_peak,       /* C&T PEAK - check specific ID first */
    &ops_ct_scat,       /* C&T SCAT - check specific ID first */
    &ops_ct_neat386,    /* Check 386 variant before 286 */
    &ops_ct_neat,
    &ops_ali_finis,     /* ALi M1209 - specific ID */
    &ops_ali_m1217,     /* ALi M1217 - specific ID */
    &ops_headland_ht12,
    &ops_headland_ht18,
    &ops_headland_ht101,
    &ops_headland_ht102,
    &ops_vlsi_vl82c311, /* Check specific variant first */
    &ops_vlsi_vl82c320, /* Check 386 variant before 286 */
    &ops_vlsi_vl82c100,

    /* Sentinel - must be last */
    NULL
};

/*============================================================================
 * CHIPSET DETECTION FUNCTION
 *
 * Iterates through the registry, calling each chipset's probe function.
 * Sets g_hal to the first chipset that returns 1 from probe().
 * Falls back to ops_unknown if no chipset is detected.
 *============================================================================*/

void detect_chipset_hal(void)
{
    const chipset_ops_t * const *p;

    /* Start with unknown fallback */
    g_hal = &ops_unknown;

    /* Try each chipset in priority order */
    for (p = g_chipset_registry; *p != NULL; p++) {
        if ((*p)->probe()) {
            g_hal = *p;
            return;
        }
    }

    /* No chipset detected - g_hal remains pointing to ops_unknown */
}

/*============================================================================
 * CONVENIENCE MACROS FOR CHIPSET IMPLEMENTATION
 *
 * These macros help reduce boilerplate when implementing chipset ops.
 *============================================================================*/

/* Create a simple bit-toggle cache control pair */
#define DEFINE_CACHE_BIT_OPS(prefix, read_fn, write_fn, reg, bit, enable_high) \
    static int prefix##_cache_get(void) { \
        unsigned char val = read_fn(reg); \
        int enabled = enable_high ? (val & (1 << bit)) : !(val & (1 << bit)); \
        return enabled ? CACHE_ENABLED : CACHE_DISABLED; \
    } \
    static int prefix##_cache_set(int enable) { \
        unsigned char val = read_fn(reg); \
        if (enable_high) { \
            if (enable) val |= (1 << bit); else val &= ~(1 << bit); \
        } else { \
            if (enable) val &= ~(1 << bit); else val |= (1 << bit); \
        } \
        write_fn(reg, val); \
        return HAL_OK; \
    }
