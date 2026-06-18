/*============================================================================
 * CK_XMS.C - XMS (Extended Memory) client for CACHEKIT
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-06-18 00:00:00 EST
 *
 * Presence detection, local A20 control, and an HMA (High Memory Area)
 * request. The benchmark uses the HMA - genuine memory just above 1MB that an
 * ordinary real-mode far pointer at FFFF:0010 can reach once A20 is on - as a
 * safe scratch buffer, falling back to a conventional buffer when the HMA is
 * unavailable.
 *
 * (The earlier EMB-allocate + unreal-mode path was removed: unreal mode's
 * protected-mode excursion is delicate / hardware-risky, and is unnecessary
 * for the small working sets the benchmark actually touches.)
 *
 * XMS calling convention: after INT 2Fh AX=4310h returns the driver entry
 * point in ES:BX, each service is a FAR CALL to that entry with the function
 * number in AH. Services return AX=1 on success / AX=0 on failure.
 *
 * NOTE: the XMS call ABI here is implemented per spec; validate on 86Box/PCem
 * with HIMEM.SYS loaded.
 *============================================================================*/

#include <dos.h>
#include <i86.h>
#include "CK_XMS.H"

/* Driver state (captured by xms_init). */
static int g_xms_present = 0;
static void (__far *g_xms_entry)(void) = 0;     /* far entry point, call w/ AH */

int xms_init(void)
{
    union REGS r;
    struct SREGS sr;

    g_xms_present = 0;
    g_xms_entry = 0;

    /* INT 2Fh AX=4300h : XMS installation check. AL == 0x80 => present. */
    r.x.ax = 0x4300;
    int86(0x2F, &r, &r);
    if (r.h.al != 0x80)
        return 0;

    /* INT 2Fh AX=4310h : get driver entry point in ES:BX. */
    r.x.ax = 0x4310;
    int86x(0x2F, &r, &r, &sr);
    g_xms_entry = (void (__far *)(void))MK_FP(sr.es, r.x.bx);

    g_xms_present = 1;
    return 1;
}

int xms_available(void)
{
    return g_xms_present;
}

/*
 * Request the High Memory Area (XMS AH=01h, DX=0xFFFF = request all of it).
 * Returns 1 if granted - the HMA is then addressable at FFFF:0010 with A20
 * enabled - or 0 if unavailable (no driver, or the HMA is already owned by
 * DOS=HIGH / another program). Requesting it (rather than just poking
 * FFFF:0010) is the documented protocol that avoids clobbering whoever owns it.
 */
int xms_request_hma(void)
{
    unsigned int ok = 0;        /* init: _asm writes are invisible to -ox */

    if (!g_xms_present)
        return 0;

    _asm {
        mov ah, 01h             /* AH=01h: request HMA */
        mov dx, 0FFFFh          /* DX=0xFFFF: request the entire HMA */
        call dword ptr g_xms_entry
        mov ok, ax              /* AX=1 granted */
    }

    return ok ? 1 : 0;
}

int xms_local_enable_a20(void)
{
    unsigned int ok = 0;

    if (!g_xms_present)
        return 0;

    _asm {
        mov ah, 05h             /* AH=05h: local enable A20 */
        call dword ptr g_xms_entry
        mov ok, ax
    }

    return ok ? 1 : 0;
}

int xms_local_disable_a20(void)
{
    unsigned int ok = 0;

    if (!g_xms_present)
        return 0;

    _asm {
        mov ah, 06h             /* AH=06h: local disable A20 */
        call dword ptr g_xms_entry
        mov ok, ax
    }

    return ok ? 1 : 0;
}
