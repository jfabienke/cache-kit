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
 * CPU TIER DETECTION
 *
 * Detection ladder ordered so each step's opcodes are legal on the CPU that
 * reaches it:
 *   1. 8086 vs 286 vs 386+  - 16-bit FLAGS bits 12-15 test (safe on 8086/286)
 *   2. 386 vs 486           - EFLAGS AC bit (bit 18); only after step 1 = 386+
 *   3. 486 vs Pentium       - EFLAGS ID bit (bit 21); only after step 2 = 486+
 * The 32-bit (.386) blocks are guarded by early-return so they are never
 * EXECUTED on a 286, even though their bytes exist in the image.
 *============================================================================*/

unsigned char g_cpu_tier = CK_CPU_8086;     /* set by ck_detect_cpu() */

/*
 * CPU-probe primitives implemented as #pragma aux so the result returns in a
 * register (no _asm-writes-to-local that the optimizer can't see, and no
 * 32-bit register names in the parm/value lists, which -0 mode rejects).
 * The .386 directive only affects the assembled body; only the 16-bit halves
 * (ax/cx) appear in value/modify. cpu_ac_settable/cpu_id_settable execute
 * 32-bit opcodes, so they are CALLED only after 386+ is confirmed.
 */

/* FLAGS bits 12-15 readback after trying to CLEAR them.
   Returns 0xF000 only on 8086/8088 (bits stuck high). Restores FLAGS. */
static unsigned int cpu_flags_clear_test(void);
#pragma aux cpu_flags_clear_test = \
    "pushf"  "pop ax"  "mov cx, ax" \
    "and ax, 0FFFh"  "push ax"  "popf" \
    "pushf"  "pop ax"  "and ax, 0F000h" \
    "push cx"  "popf" \
    value [ax] modify [cx];

/* FLAGS bits 12-15 readback after trying to SET them.
   Returns 0x0000 only on 80286 (bits stuck low). Restores FLAGS. */
static unsigned int cpu_flags_set_test(void);
#pragma aux cpu_flags_set_test = \
    "pushf"  "pop ax"  "mov cx, ax" \
    "or ax, 0F000h"  "push ax"  "popf" \
    "pushf"  "pop ax"  "and ax, 0F000h" \
    "push cx"  "popf" \
    value [ax] modify [cx];

/* 386 vs 486: returns 1 if the AC flag (EFLAGS bit 18) is settable. (386+ only) */
static unsigned int cpu_ac_settable(void);
#pragma aux cpu_ac_settable = \
    ".386" \
    "pushfd"  "pop eax"  "mov ecx, eax" \
    "xor eax, 40000h"  "push eax"  "popfd" \
    "pushfd"  "pop eax"  "xor eax, ecx" \
    "shr eax, 18"  "and eax, 1" \
    "push ecx"  "popfd" \
    value [ax] modify [cx];

/* 486 vs Pentium: returns 1 if the ID flag (EFLAGS bit 21 -> CPUID) is settable. */
static unsigned int cpu_id_settable(void);
#pragma aux cpu_id_settable = \
    ".386" \
    "pushfd"  "pop eax"  "mov ecx, eax" \
    "xor eax, 200000h"  "push eax"  "popfd" \
    "pushfd"  "pop eax"  "xor eax, ecx" \
    "shr eax, 21"  "and eax, 1" \
    "push ecx"  "popfd" \
    value [ax] modify [cx];

void ck_detect_cpu(void)
{
    /* Step 1: 8086 vs 286 vs 386+ (16-bit FLAGS bits 12-15; safe everywhere). */
    if (cpu_flags_clear_test() == 0xF000) {  /* bits stuck high -> 8086/8088 */
        g_cpu_tier = CK_CPU_8086;
        return;
    }
    if (cpu_flags_set_test() != 0xF000) {    /* bits stuck low -> 80286 */
        g_cpu_tier = CK_CPU_286;
        return;
    }

    /* 386 or later: 32-bit opcodes are now safe to execute. */
    g_cpu_tier = CK_CPU_386;

    /* Step 2: 386 vs 486 (AC flag). */
    if (!cpu_ac_settable())
        return;                              /* AC not settable -> 80386 */
    g_cpu_tier = CK_CPU_486;

    /* Step 3: 486 vs Pentium (ID flag / CPUID). */
    if (cpu_id_settable())
        g_cpu_tier = CK_CPU_PENTIUM;
}

/*============================================================================
 * GENERIC HELPER IMPLEMENTATIONS
 *============================================================================*/

/*
 * Tier-aware cache flush. WBINVD (0F 09) and INVD (0F 08) are BOTH 486+
 * instructions; the 386 has no on-chip cache and no cache-management opcode.
 * So we issue WBINVD only on 486+, and no-op on 386 and earlier (external/L2
 * cache flushing on those parts is handled by chipset-specific ops). This
 * avoids the invalid-opcode (#UD) fault that unconditional WBINVD/INVD causes
 * on a 386. ck_detect_cpu() must have run first.
 */
int generic_safe_flush(void)
{
    if (g_cpu_tier >= CK_CPU_486) {
        _asm {
            db 0Fh, 09h     /* WBINVD - Write Back and Invalidate Data Cache */
        }
    }
    /* 386 and earlier: no CPU cache instruction exists - safe no-op. */
    return HAL_OK;
}

/*
 * Cache flush using WBINVD instruction (486+).
 * Now routed through generic_safe_flush() so it is CPU-tier safe; retained
 * as a named entry point for the many ops structs that reference it.
 */
int generic_wbinvd_flush(void)
{
    return generic_safe_flush();
}

/*
 * Cache flush formerly using bare INVD (which is 486+, not 386, and discards
 * without writeback). Now routed through generic_safe_flush(): WBINVD on 486+,
 * no-op below. This is strictly safer (no data loss, no #UD).
 */
int generic_invd_flush(void)
{
    return generic_safe_flush();
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

    /* Cache (assume enabled, use CPU-tier-safe flush) */
    .cache_get = unknown_cache_get,
    .cache_set = hal_stub_unsupported_i,
    .cache_flush = generic_safe_flush,
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
