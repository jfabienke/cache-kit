/*============================================================================
 * CK_XMS.C - XMS (Extended Memory) client core for CACHEKIT
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-06-17 00:00:00 EST
 *
 * Presence detection, EMB allocate/free, lock/unlock (to get the 32-bit
 * linear address), and local A20 control. See CK_XMS.H for the interface.
 *
 * XMS calling convention: after INT 2Fh AX=4310h returns the driver entry
 * point in ES:BX, each service is a FAR CALL to that entry with the function
 * number in AH. Services return AX=1 on success / AX=0 on failure, and
 * preserve SI, DI, BP and the segment registers (AX/BX/CX/DX are scratch).
 *
 * NOTE: the XMS call ABI here is implemented per spec but has not been
 * runtime-tested on hardware in this environment; validate on 86Box/PCem
 * with HIMEM.SYS loaded (this is the same emulator-verification path used by
 * the rest of the fix plan).
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

unsigned int xms_alloc_kb(unsigned int kb)
{
    unsigned int ok = 0;        /* init: _asm writes are invisible to -ox */
    unsigned int handle = 0;

    if (!g_xms_present)
        return 0;

    _asm {
        mov ah, 09h             /* AH=09h: allocate EMB, DX=KB */
        mov dx, kb
        call dword ptr g_xms_entry
        mov ok, ax              /* AX=1 success, DX=handle */
        mov handle, dx
    }

    return ok ? handle : 0;
}

void xms_free(unsigned int handle)
{
    if (!g_xms_present)
        return;

    _asm {
        mov ah, 0Ah             /* AH=0Ah: free EMB, DX=handle */
        mov dx, handle
        call dword ptr g_xms_entry
    }
}

unsigned long xms_lock(unsigned int handle)
{
    unsigned int ok = 0;
    unsigned int addr_hi = 0;
    unsigned int addr_lo = 0;

    if (!g_xms_present)
        return 0UL;

    _asm {
        mov ah, 0Ch             /* AH=0Ch: lock EMB, DX=handle */
        mov dx, handle
        call dword ptr g_xms_entry
        mov ok, ax              /* AX=1 success, DX:BX = linear address */
        mov addr_hi, dx
        mov addr_lo, bx
    }

    if (!ok)
        return 0UL;
    return ((unsigned long)addr_hi << 16) | (unsigned long)addr_lo;
}

void xms_unlock(unsigned int handle)
{
    if (!g_xms_present)
        return;

    _asm {
        mov ah, 0Dh             /* AH=0Dh: unlock EMB, DX=handle */
        mov dx, handle
        call dword ptr g_xms_entry
    }
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
