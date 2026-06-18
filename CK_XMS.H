/*============================================================================
 * CK_XMS.H - XMS (Extended Memory) client for CACHEKIT
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-06-18 00:00:00 EST
 *
 * Minimal XMS client used to obtain the High Memory Area (HMA) as a safe
 * scratch buffer for the cache benchmark/flush tests. The HMA is real memory
 * just above 1MB (FFFF:0010..FFFF:FFFF, ~64KB) reachable with an ordinary
 * real-mode far pointer once A20 is enabled - no protected-mode/unreal-mode
 * excursion required. The benchmark falls back to a conventional buffer when
 * the HMA is unavailable.
 *============================================================================*/

#ifndef CK_XMS_H
#define CK_XMS_H

/*
 * Detect an XMS driver (HIMEM.SYS or equivalent) and capture its entry point.
 * Must be called before any other xms_* function. Returns 1 if present, else 0.
 */
int xms_init(void);

/* 1 if xms_init() found a driver, else 0. */
int xms_available(void);

/*
 * Request the High Memory Area. Returns 1 if granted (then addressable at
 * FFFF:0010 with A20 enabled), 0 if unavailable (no driver, or already owned
 * by DOS=HIGH / another program - caller should fall back to conventional).
 */
int xms_request_hma(void);

/*
 * Local A20 enable/disable (reference-counted by the driver). A20 must be
 * enabled to address the HMA (memory at/above 1MB). Returns 1 on success.
 */
int xms_local_enable_a20(void);
int xms_local_disable_a20(void);

#endif /* CK_XMS_H */
