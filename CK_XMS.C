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

/*============================================================================
 * UNREAL MODE ("flat real") - see CK_XMS.H for the contract and caveats.
 *
 * !!! NOT hardware-validated here; compile-checked only. Verify on 86Box.
 *============================================================================*/

static unsigned char g_unreal_ready = 0;

/* GDT: [0] null, [1] flat data descriptor (base 0, limit 4GB, 4KB gran).
 * Descriptor [1] bytes: limit15:0=FFFF, base15:0=0000, base23:16=00,
 * access=92h (present|ring0|data|writable), flags|limit19:16=CFh, base31:24=00. */
static unsigned short g_gdt[8] = {
    0x0000, 0x0000, 0x0000, 0x0000,     /* null descriptor */
    0xFFFF, 0x0000, 0x9200, 0x00CF      /* flat 4GB data, selector 0x08 */
};

/* GDTR image: word limit + dword linear base of g_gdt. */
static unsigned char g_gdtr[6];

int unreal_enter(void)
{
    /* Compute the linear base of g_gdt (DS<<4 + offset) into the GDTR, then
       briefly enter protected mode to load FS with the flat descriptor. FS
       retains the cached base/limit after returning to real mode. */
    _asm {
        .386p
        xor  eax, eax
        mov  ax, ds
        shl  eax, 4
        mov  bx, offset g_gdt
        movzx ebx, bx
        add  eax, ebx                   /* eax = linear address of g_gdt */
        mov  dword ptr g_gdtr+2, eax
        mov  word ptr g_gdtr, 15        /* limit = 2 descriptors * 8 - 1 */

        cli
        lgdt fword ptr g_gdtr
        mov  eax, cr0
        or   al, 1
        mov  cr0, eax                   /* enter protected mode */
        mov  bx, 8
        mov  fs, bx                     /* FS <- flat descriptor (base0,4GB) */
        mov  eax, cr0
        and  al, 0FEh
        mov  cr0, eax                   /* back to real mode; FS cache stays */
        sti
    }

    g_unreal_ready = 1;
    return 1;
}

int unreal_active(void)
{
    return g_unreal_ready;
}

/* Read-bandwidth / cache-eviction walk: touch [base, base+bytes) by 16 bytes.
   Uses scratch regs (eax/ebx/edx) only; fs base is 0 so the offset == linear. */
void unreal_read_walk(unsigned long base, unsigned long bytes)
{
    _asm {
        .386p
        mov  ebx, base
        mov  edx, base
        add  edx, bytes                 /* edx = end (exclusive) */
    rw_loop:
        cmp  ebx, edx
        jae  rw_done
        mov  al, fs:[ebx]
        add  ebx, 16
        jmp  rw_loop
    rw_done:
    }
}

void unreal_fill(unsigned long base, unsigned long bytes, unsigned long pattern)
{
    _asm {
        .386p
        mov  ebx, base
        mov  edx, base
        add  edx, bytes
        mov  eax, pattern
    f_loop:
        cmp  ebx, edx
        jae  f_done
        mov  fs:[ebx], eax
        add  ebx, 4
        jmp  f_loop
    f_done:
    }
}

void unreal_copy(unsigned long dst, unsigned long src, unsigned long bytes)
{
    _asm {
        .386p
        mov  ebx, src
        mov  ecx, dst
        mov  edx, src
        add  edx, bytes                 /* edx = end of src */
    c_loop:
        cmp  ebx, edx
        jae  c_done
        mov  al, fs:[ebx]
        mov  fs:[ecx], al
        add  ebx, 1
        add  ecx, 1
        jmp  c_loop
    c_done:
    }
}

void unreal_write32(unsigned long lin, unsigned long val)
{
    _asm {
        .386p
        mov  ebx, lin
        mov  eax, val
        mov  fs:[ebx], eax
    }
}

unsigned long unreal_read32(unsigned long lin)
{
    unsigned int lo = 0;
    unsigned int hi = 0;

    _asm {
        .386p
        mov  ebx, lin
        mov  eax, fs:[ebx]
        mov  lo, ax
        shr  eax, 16
        mov  hi, ax
    }

    return ((unsigned long)hi << 16) | (unsigned long)lo;
}
