/*============================================================================
 * CK_IO.C - Low-Level I/O and PCI Configuration Space Access
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-05 18:50:00 EST
 *
 * Provides portable access to:
 * - PCI configuration space (via 0xCF8/0xCFC mechanism)
 * - Legacy index/data port pairs (0x22/0x23, 0x22/0x24, 0xA8/0xA9)
 * - EISA ports (0x0C80/0x0C85)
 *============================================================================*/

#include <conio.h>
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

unsigned long io_read_dword(unsigned int port)
{
    return inpd(port);
}

void io_write_dword(unsigned int port, unsigned long val)
{
    outpd(port, val);
}

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
 * SAFE PORT ACCESS WITH TIMEOUT
 *
 * Some operations may hang on incompatible hardware.
 * These wrappers provide basic protection.
 *============================================================================*/

/* Currently just direct wrappers - could add timeout logic if needed */
unsigned char safe_read_byte(unsigned int port)
{
    return io_read_byte(port);
}

void safe_write_byte(unsigned int port, unsigned char val)
{
    io_write_byte(port, val);
}
