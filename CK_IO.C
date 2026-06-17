/*============================================================================
 * CK_IO.C - Low-Level I/O and PCI Configuration Space Access
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 17:08:46 EST
 *
 * Provides portable access to:
 * - PCI configuration space (via 0xCF8/0xCFC mechanism)
 * - Legacy index/data port pairs (0x22/0x23, 0x22/0x24, 0xA8/0xA9)
 * - EISA ports (0x0C80/0x0C85)
 *============================================================================*/

#include <conio.h>
#include <dos.h>
#include <i86.h>
#include "CK_HAL.H"

/*============================================================================
 * PORT I/O PRIMITIVES
 *
 * These wrap the Watcom inp()/outp() functions for clarity.
 *============================================================================*/

unsigned char io_read_byte(unsigned int port)
{
    return (unsigned char)inp(port);
}

void io_write_byte(unsigned int port, unsigned char val)
{
    outp(port, val);
}

unsigned int io_read_word(unsigned int port)
{
    return inpw(port);
}

void io_write_word(unsigned int port, unsigned int val)
{
    outpw(port, val);
}

/*
 * 32-bit port I/O (386+ required; call sites are gated by CPU/bus checks).
 * The 16-bit conio.h provides no inpd/outpd, so these are implemented via
 * #pragma aux, which gives the optimizer correct register clobber info
 * (a bare _asm block hides the 32-bit EAX clobber from the compiler).
 *
 * IN/OUT support only AL/AX/EAX, so the dword is read into / assembled in EAX.
 * A 32-bit return/parameter occupies a 16-bit register PAIR (high:low):
 *   io_read_dword  returns DX:AX   (DX = high word, AX = low word)
 *   io_write_dword takes  port=DX, val=CX:BX (CX = high word, BX = low word)
 */
unsigned long io_read_dword(unsigned int port);
#pragma aux io_read_dword =     \
    ".386"                      \
    "in     eax, dx"            \
    "mov    edx, eax"           \
    "shr    edx, 16"            \
    parm   [dx]                 \
    value  [dx ax];

void io_write_dword(unsigned int port, unsigned long val);
#pragma aux io_write_dword =    \
    ".386"                      \
    "mov    ax, cx"             \
    "shl    eax, 16"            \
    "mov    ax, bx"             \
    "out    dx, eax"            \
    parm   [dx] [cx bx]         \
    modify [ax];

/*============================================================================
 * INTERRUPT FLAG SAVE / RESTORE
 *
 * ck_irq_save() returns the current FLAGS and clears IF (CLI).
 * ck_irq_restore() restores FLAGS (including the prior IF state).
 * Unlike _disable()/_enable(), this nests correctly: restoring leaves IF
 * exactly as it was, so an already-disabled caller is not re-enabled.
 * All 16-bit ops - safe on 8086 and up.
 *============================================================================*/

unsigned int ck_irq_save(void);
#pragma aux ck_irq_save =       \
    "pushf"                     \
    "pop ax"                    \
    "cli"                       \
    value [ax];

void ck_irq_restore(unsigned int flags);
#pragma aux ck_irq_restore =    \
    "push ax"                   \
    "popf"                      \
    parm [ax];

/*============================================================================
 * PCI CONFIGURATION SPACE ACCESS
 *
 * Uses the standard Type 1 configuration mechanism:
 * - Write address to 0xCF8 (CONFIG_ADDRESS)
 * - Read/write data at 0xCFC (CONFIG_DATA)
 *
 * Address format:
 * Bits 31    : Enable bit (must be 1)
 * Bits 23:16 : Bus number
 * Bits 15:11 : Device number
 * Bits 10:8  : Function number
 * Bits 7:0   : Register offset (must be aligned)
 *============================================================================*/

#define PCI_CONFIG_ADDRESS  0x0CF8
#define PCI_CONFIG_DATA     0x0CFC
#define PCI_ENABLE_BIT      0x80000000UL

static unsigned long pci_make_address(unsigned char bus, unsigned char dev,
                                      unsigned char func, unsigned char reg)
{
    return PCI_ENABLE_BIT |
           ((unsigned long)bus << 16) |
           ((unsigned long)(dev & 0x1F) << 11) |
           ((unsigned long)(func & 0x07) << 8) |
           (reg & 0xFC);  /* Align to dword */
}

/*--- Byte Access ---*/

unsigned char pci_read_config_byte(unsigned char bus, unsigned char dev,
                                   unsigned char func, unsigned char reg)
{
    unsigned long addr = pci_make_address(bus, dev, func, reg);
    io_write_dword(PCI_CONFIG_ADDRESS, addr);
    /* Read appropriate byte from dword */
    return io_read_byte(PCI_CONFIG_DATA + (reg & 3));
}

void pci_write_config_byte(unsigned char bus, unsigned char dev,
                           unsigned char func, unsigned char reg,
                           unsigned char val)
{
    unsigned long addr = pci_make_address(bus, dev, func, reg);
    io_write_dword(PCI_CONFIG_ADDRESS, addr);
    io_write_byte(PCI_CONFIG_DATA + (reg & 3), val);
}

/*--- Word Access ---*/

unsigned int pci_read_config_word(unsigned char bus, unsigned char dev,
                                  unsigned char func, unsigned char reg)
{
    unsigned long addr = pci_make_address(bus, dev, func, reg);
    io_write_dword(PCI_CONFIG_ADDRESS, addr);
    return io_read_word(PCI_CONFIG_DATA + (reg & 2));
}

void pci_write_config_word(unsigned char bus, unsigned char dev,
                           unsigned char func, unsigned char reg,
                           unsigned int val)
{
    unsigned long addr = pci_make_address(bus, dev, func, reg);
    io_write_dword(PCI_CONFIG_ADDRESS, addr);
    io_write_word(PCI_CONFIG_DATA + (reg & 2), val);
}

/*--- Dword Access ---*/

unsigned long pci_read_config_dword(unsigned char bus, unsigned char dev,
                                    unsigned char func, unsigned char reg)
{
    unsigned long addr = pci_make_address(bus, dev, func, reg);
    io_write_dword(PCI_CONFIG_ADDRESS, addr);
    return io_read_dword(PCI_CONFIG_DATA);
}

void pci_write_config_dword(unsigned char bus, unsigned char dev,
                            unsigned char func, unsigned char reg,
                            unsigned long val)
{
    unsigned long addr = pci_make_address(bus, dev, func, reg);
    io_write_dword(PCI_CONFIG_ADDRESS, addr);
    io_write_dword(PCI_CONFIG_DATA, val);
}

/*============================================================================
 * PCI BUS DETECTION
 *
 * Returns 1 if PCI bus is present, 0 otherwise.
 * Tests by reading vendor ID at bus 0, device 0, function 0.
 *============================================================================*/

int pci_bus_present(void)
{
    unsigned long id;

    /* Try to read device 0:0.0 vendor/device ID */
    io_write_dword(PCI_CONFIG_ADDRESS, PCI_ENABLE_BIT);
    id = io_read_dword(PCI_CONFIG_DATA);

    /* 0xFFFFFFFF means no device (or no PCI bus) */
    /* 0x00000000 is also invalid */
    if (id == 0xFFFFFFFFUL || id == 0x00000000UL) {
        return 0;
    }

    return 1;
}

/*
 * Alias for pci_bus_present (for compatibility with code using pci_present)
 */
int pci_present(void)
{
    return pci_bus_present();
}

/*============================================================================
 * BUS TYPE DETECTION
 *
 * Detect EISA and MCA buses for expansion card enumeration.
 *============================================================================*/

/*
 * Check for EISA bus by looking for "EISA" signature in ROM
 */
int check_eisa(void)
{
    char far *sig = (char far *)0xF000FFD9L;
    return (sig[0] == 'E' && sig[1] == 'I' && sig[2] == 'S' && sig[3] == 'A');
}

/*
 * Check for MCA bus via INT 15h AH=C0h BIOS configuration table
 */
int check_mca(void)
{
    union REGS regs;
    struct SREGS sregs;
    unsigned char far *config_table;
    unsigned char feature_byte1;

    regs.h.ah = 0xC0;
    int86x(0x15, &regs, &regs, &sregs);

    /* Check carry flag - function not supported if set */
    if (regs.x.cflag)
        return 0;

    /* Get pointer to configuration table in ROM */
    config_table = (unsigned char far *)MK_FP(sregs.es, regs.x.bx);

    /* Feature byte 1 is at offset 05h */
    feature_byte1 = config_table[5];

    /* Bit 1 = MCA (Micro Channel Architecture) bus present */
    return (feature_byte1 & 0x02) ? 1 : 0;
}

/*
 * Wrapper for 4-parameter pci_read_config (maps to dword read)
 */
unsigned long pci_read_config(unsigned char bus, unsigned char dev,
                              unsigned char func, unsigned char reg)
{
    return pci_read_config_dword(bus, dev, func, reg);
}

/*============================================================================
 * BIOS SIGNATURE SCAN
 *
 * Walk [seg_start, seg_end] one paragraph (16 bytes) at a time, rebuilding a
 * normalized far pointer with MK_FP each step. This is the correct way to
 * scan the BIOS area: advancing a far pointer's offset wraps at 64KB without
 * bumping the segment, so a naive "ptr += 16; while (ptr < end)" loop never
 * crosses a segment boundary (and can spin forever). Signatures of interest
 * (SMBIOS "_SM_", ACPI "RSD PTR ") are 16-byte aligned, so paragraph stepping
 * finds them. Returns far pointer to the first match, or NULL.
 *============================================================================*/

unsigned char far *io_find_sig(unsigned int seg_start, unsigned int seg_end,
                               const char *sig, int siglen)
{
    unsigned int seg;
    int i;
    unsigned char far *p;

    for (seg = seg_start; ; seg++) {
        p = (unsigned char far *)MK_FP(seg, 0);
        for (i = 0; i < siglen; i++) {
            if (p[i] != (unsigned char)sig[i])
                break;
        }
        if (i == siglen)
            return p;               /* full signature matched */
        if (seg >= seg_end)
            break;                  /* done; break before seg++ can wrap */
    }
    return (unsigned char far *)0;
}

/*============================================================================
 * LEGACY INDEX/DATA PORT ACCESS
 *
 * Many 386/486 chipsets use an index/data port pair for configuration:
 * - Standard: 0x22 (index) / 0x23 (data)
 * - OPTi extended: 0x22 (index) / 0x24 (data)
 * - VIA Venus: 0xA8 (index) / 0xA9 (data)
 *============================================================================*/

unsigned char legacy_read_reg(unsigned int index_port, unsigned int data_port,
                              unsigned char reg)
{
    io_write_byte(index_port, reg);
    return io_read_byte(data_port);
}

void legacy_write_reg(unsigned int index_port, unsigned int data_port,
                      unsigned char reg, unsigned char val)
{
    io_write_byte(index_port, reg);
    io_write_byte(data_port, val);
}

/*--- Standard 0x22/0x23 ports ---*/

unsigned char legacy_read_22_23(unsigned char reg)
{
    return legacy_read_reg(0x22, 0x23, reg);
}

void legacy_write_22_23(unsigned char reg, unsigned char val)
{
    legacy_write_reg(0x22, 0x23, reg, val);
}

/*--- OPTi 0x22/0x24 ports ---*/

unsigned char legacy_read_22_24(unsigned char reg)
{
    return legacy_read_reg(0x22, 0x24, reg);
}

void legacy_write_22_24(unsigned char reg, unsigned char val)
{
    legacy_write_reg(0x22, 0x24, reg, val);
}

/*--- VIA Venus 0xA8/0xA9 ports ---*/

unsigned char legacy_read_a8_a9(unsigned char reg)
{
    return legacy_read_reg(0xA8, 0xA9, reg);
}

void legacy_write_a8_a9(unsigned char reg, unsigned char val)
{
    legacy_write_reg(0xA8, 0xA9, reg, val);
}

/*============================================================================
 * EISA PORT ACCESS
 *
 * EISA chipsets use different port ranges.
 * Intel 82350DT uses ports in the 0x0C80-0x0C85 range.
 *============================================================================*/

unsigned char eisa_read_reg(unsigned int base, unsigned char reg)
{
    return io_read_byte(base + reg);
}

void eisa_write_reg(unsigned int base, unsigned char reg, unsigned char val)
{
    io_write_byte(base + reg, val);
}

/*============================================================================
 * PORT VALIDATION
 *
 * Attempt to detect if an index/data port pair is responsive.
 * Reads a register and checks for non-0xFF response.
 *============================================================================*/

int legacy_port_valid(unsigned int index_port, unsigned int data_port)
{
    unsigned char val1, val2;

    /* Read register 0x00 and 0xFF, see if we get different values */
    io_write_byte(index_port, 0x00);
    val1 = io_read_byte(data_port);

    io_write_byte(index_port, 0xFF);
    val2 = io_read_byte(data_port);

    /* If both return 0xFF, port is probably not connected */
    if (val1 == 0xFF && val2 == 0xFF) {
        return 0;
    }

    return 1;
}

/*============================================================================
 * SAFE PORT ACCESS
 *
 * WARNING: True timeout protection is not possible in DOS real mode without
 * hooking timer interrupts (INT 08h/INT 1Ch), which adds significant
 * complexity and potential compatibility issues.
 *
 * KNOWN LIMITATION: If hardware hangs on port access (e.g., accessing
 * non-existent chipset registers on incompatible hardware), only a
 * hardware reset will recover the system. This is a fundamental limitation
 * of x86 port I/O in real mode.
 *
 * MITIGATION STRATEGIES:
 * 1. Use legacy_port_valid() to probe before accessing unknown ports
 * 2. Use safe_port_probe() to check for floating bus conditions
 * 3. Check pci_bus_present() before PCI operations
 * 4. Validate chipset detection before writing to registers
 *
 * These wrappers exist primarily for documentation and future enhancement.
 *============================================================================*/

unsigned char safe_read_byte(unsigned int port)
{
    /* Note: If hardware hangs here, no software recovery is possible in DOS */
    return io_read_byte(port);
}

void safe_write_byte(unsigned int port, unsigned char val)
{
    /* Note: If hardware hangs here, no software recovery is possible in DOS */
    io_write_byte(port, val);
}

/*
 * Probe a port to check for floating bus condition.
 * A floating bus often returns different values on consecutive reads,
 * or returns 0xFF consistently. This can help detect unmapped ports.
 *
 * Returns: 1 if port appears valid (consistent reads), 0 if likely floating
 */
int safe_port_probe(unsigned int port)
{
    unsigned char val1, val2;

    val1 = io_read_byte(port);
    val2 = io_read_byte(port);

    /* Floating bus may return different values on consecutive reads */
    if (val1 != val2) {
        return 0;  /* Inconsistent - likely floating */
    }

    /* 0xFF is common for unmapped ports, but also valid for some registers */
    /* Caller should combine with other checks (e.g., signature validation) */
    return 1;
}
