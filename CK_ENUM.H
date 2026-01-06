/*============================================================================
 * CK_ENUM.H - CACHEKIT Bus Enumeration Interface
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 11:45:00 EST
 *
 * Provides device enumeration for PCI, PCIe, MCA, EISA, and ISA Plug-and-Play.
 *============================================================================*/

#ifndef CK_ENUM_H
#define CK_ENUM_H

/*============================================================================
 * BUS TYPE CONSTANTS
 *============================================================================*/

#define BUS_PCI       1
#define BUS_PCIE      2
#define BUS_MCA       3
#define BUS_EISA      4
#define BUS_ISAPNP    5

#define MAX_DEVICES   64

/*============================================================================
 * DEVICE ENTRY STRUCTURE
 *============================================================================*/

typedef struct {
    unsigned char bus_type;       /* BUS_PCI, BUS_MCA, BUS_EISA, BUS_ISAPNP, BUS_PCIE */
    unsigned char bus;            /* Bus number (PCI) or slot (MCA/EISA) */
    unsigned char dev;            /* Device number */
    unsigned char func;           /* Function number */
    unsigned int vendor_id;       /* Vendor ID */
    unsigned int device_id;       /* Device ID */
    unsigned char class_code;     /* PCI class code */
    unsigned char subclass;       /* PCI subclass */
    unsigned char irq;            /* IRQ assignment */
    unsigned long io_base;        /* Primary I/O base */
    unsigned long mem_base;       /* Primary memory base */
    const char *name;             /* Device name (from embedded DB) */
} device_entry_t;

/*============================================================================
 * GLOBAL DEVICE ARRAY
 *============================================================================*/

extern device_entry_t g_devices[MAX_DEVICES];

/*============================================================================
 * ENUMERATION FUNCTIONS
 *============================================================================*/

/* Initialize enumeration subsystem */
void enum_init(void);

/* Enumerate all devices on all bus types
 * Returns total device count
 * Results stored in g_devices[] */
int enum_all_devices(void);

/* Enumerate specific bus types (returns count of devices found) */
int enum_pci_devices(void);      /* PCI bus scan */
int enum_mca_devices(void);      /* MCA (PS/2 Micro Channel) */
int enum_eisa_devices(void);     /* EISA slot scan */
int enum_isapnp_devices(void);   /* ISA Plug-and-Play isolation */

/* Mark PCI devices that support PCIe extended config */
void enum_mark_pcie_devices(void);

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/* Get bus type name string */
const char *enum_bus_name(unsigned char bus_type);

/* Count devices by bus type (0 = all) */
int enum_count_by_bus(unsigned char bus_type, int total_count);

/* Lookup device name from embedded databases */
const char *enum_lookup_pci_name(unsigned int vendor, unsigned int device);
const char *enum_lookup_mca_name(unsigned int adapter_id);
const char *enum_lookup_isapnp_name(unsigned long pnp_id);

/*============================================================================
 * PCIE/MCFG STATE (read-only access)
 *============================================================================*/

/* Returns 1 if PCIe MCFG table found and MMIO config available */
int enum_pcie_available(void);

/* Get MCFG base address (0 if not available) */
unsigned long enum_mcfg_base(void);

#endif /* CK_ENUM_H */
