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

#endif /* CK_XMS_H */
