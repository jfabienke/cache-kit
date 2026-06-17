/*============================================================================
 * CK_XMS.H - XMS (Extended Memory) client for CACHEKIT
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-06-17 00:00:00 EST
 *
 * Minimal XMS 2.0+ client used to obtain a real >1MB buffer for the cache
 * benchmark/flush tests. This is the client CORE: presence detection, EMB
 * allocation, lock (to obtain the 32-bit linear address), and A20 control.
 *
 * Direct CPU access to the locked block from real mode (the benchmark needs
 * to TOUCH the memory to exercise the cache - XMS block-move would only copy)
 * is provided separately by the unreal-mode access path used by the F5/F3
 * benchmark code (386+; see CK-C1). The HMA path is the <=64KB fallback.
 *============================================================================*/

#ifndef CK_XMS_H
#define CK_XMS_H

/*
 * Detect an XMS driver (HIMEM.SYS or equivalent) and capture its entry point.
 * Must be called before any other xms_* function. Returns 1 if present, 0 if
 * not. Safe to call when no driver is installed (returns 0).
 */
int xms_init(void);

/* 1 if xms_init() found a driver, else 0. */
int xms_available(void);

/*
 * Allocate an Extended Memory Block of `kb` kilobytes. Returns a nonzero XMS
 * handle on success, or 0 on failure (no driver, out of memory, etc.).
 */
unsigned int xms_alloc_kb(unsigned int kb);

/* Free a previously allocated EMB handle. */
void xms_free(unsigned int handle);

/*
 * Lock an EMB so it will not move, and return its 32-bit linear (physical)
 * base address. Returns 0 on failure. Pair with xms_unlock(). The linear
 * address is what the unreal-mode access path uses to reach the block.
 */
unsigned long xms_lock(unsigned int handle);

/* Unlock a previously locked EMB handle. */
void xms_unlock(unsigned int handle);

/*
 * Local A20 enable/disable (reference-counted by the driver). A20 must be
 * enabled to address memory at/above 1MB. Returns 1 on success, 0 on failure.
 */
int xms_local_enable_a20(void);
int xms_local_disable_a20(void);

/*============================================================================
 * UNREAL MODE ("flat real") EXTENDED-MEMORY ACCESS  (386+ ONLY)
 *
 * unreal_enter() does a brief protected-mode excursion to load FS with a
 * 4GB-limit, base-0 descriptor, then returns to real mode. FS keeps that
 * cached descriptor (the 16-bit C code never touches FS), so the primitives
 * below address ANY 32-bit linear address directly via fs:[reg]. This is the
 * only way to touch memory above ~1MB with the CPU from real mode (ordinary
 * far pointers and 16-bit string ops cannot).
 *
 * Requirements: 386+ (gate with g_cpu_tier) and A20 enabled
 * (xms_local_enable_a20). All linear addresses are physical addresses.
 *
 * !!! NOT VALIDATED ON HARDWARE in this environment - compile-checked only.
 * !!! Verify on 86Box/PCem before trusting on a real machine. A bug in the
 * !!! protected-mode excursion can triple-fault / reboot.
 *============================================================================*/

int  unreal_enter(void);    /* set up unreal mode (386+); 1 on success */
int  unreal_active(void);   /* 1 if unreal_enter() has succeeded */

/* Read `bytes` of memory from linear `base` with a 16-byte stride (cache
   eviction / read-bandwidth walk). */
void unreal_read_walk(unsigned long base, unsigned long bytes);

/* Fill [base, base+bytes) with the 32-bit `pattern` (bytes rounded to dwords). */
void unreal_fill(unsigned long base, unsigned long bytes, unsigned long pattern);

/* Copy `bytes` from linear `src` to linear `dst` (byte granularity). */
void unreal_copy(unsigned long dst, unsigned long src, unsigned long bytes);

/* 32-bit read/write at a single linear address. */
void          unreal_write32(unsigned long lin, unsigned long val);
unsigned long unreal_read32(unsigned long lin);

#endif /* CK_XMS_H */
