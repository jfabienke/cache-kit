/*============================================================================
 * CK_ENUM.C - CACHEKIT Bus Enumeration Implementation
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 17:08:46 EST
 *
 * Provides device enumeration for:
 *   - PCI (full 256-bus scan)
 *   - PCIe (capability detection + MCFG)
 *   - MCA (PS/2 Micro Channel POS registers)
 *   - EISA (slot configuration space)
 *   - ISA Plug-and-Play (LFSR isolation protocol)
 *
 * Includes embedded ID databases for device name lookup.
 *============================================================================*/

#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include "CK_ENUM.H"
#include "CK_HAL.H"  /* For pci_read_config_*, pci_present, check_mca, check_eisa */

/*============================================================================
 * BUS-SPECIFIC CONSTANTS
 *============================================================================*/

/* MCA (Micro Channel Architecture) */
#define MCA_MOTHERBOARD_SETUP  0x94    /* System board setup register */
#define MCA_ADAPTER_SETUP      0x96    /* Adapter enable/setup register */
#define MCA_POS_BASE           0x100   /* POS register base (0x100-0x107) */

/* EISA */
#define EISA_ID_OFFSET         0xC80   /* Product ID offset within slot */

/* ISA Plug-and-Play */
#define ISAPNP_ADDRESS         0x279   /* Address port (write-only) */
#define ISAPNP_WRITE_DATA      0xA79   /* Write data port */
/* Read data port is determined dynamically (0x203-0x3F3) */

/* ISA PnP register indices */
#define ISAPNP_SET_RD_PORT     0x00    /* Set read data port */
#define ISAPNP_SERIAL_ISOL     0x01    /* Serial isolation */
#define ISAPNP_CONFIG_CTRL     0x02    /* Configuration control */
#define ISAPNP_WAKE            0x03    /* Wake[CSN] */
#define ISAPNP_RES_DATA        0x04    /* Resource data */
#define ISAPNP_STATUS          0x05    /* Status */
#define ISAPNP_CSN             0x06    /* Card Select Number */
#define ISAPNP_LOGICAL_DEV     0x07    /* Logical device number */

/* ISA PnP resource tags */
#define ISAPNP_TAG_END         0x79    /* End tag */
#define ISAPNP_TAG_IRQ         0x22    /* IRQ descriptor (small) */
#define ISAPNP_TAG_DMA         0x2A    /* DMA descriptor (small) */
#define ISAPNP_TAG_IO          0x47    /* I/O descriptor (small) */
#define ISAPNP_TAG_FIXED_IO    0x4B    /* Fixed I/O (small) */

/* PCIe Extended Configuration */
#define MCFG_SIGNATURE         0x4746434D  /* "MCFG" in little-endian */
#define PCIE_EXT_CAP_START     0x100   /* Extended capabilities start offset */

/*============================================================================
 * MODULE GLOBALS
 *============================================================================*/

/* Device array (shared with main application) */
device_entry_t g_devices[MAX_DEVICES];

/* ISA PnP state */
static unsigned int g_isapnp_read_port = 0x203;  /* Default read data port */
static int g_isapnp_cards_found = 0;             /* Number of PnP cards detected */

/* PCIe MCFG (Memory-mapped Configuration) */
static unsigned long g_mcfg_base = 0;            /* MCFG base address */
static unsigned char g_mcfg_start_bus = 0;       /* First bus in MCFG */
static unsigned char g_mcfg_end_bus = 0;         /* Last bus in MCFG */
static int g_pcie_available = 0;                 /* PCIe MMIO access available */

/* Internal device count tracker */
static int g_device_count = 0;

/*============================================================================
 * PCI ID DATABASE - Embedded lookup table (~600 entries)
 *============================================================================*/

typedef struct {
    unsigned int vendor_id;
    unsigned int device_id;
    const char *name;
} pci_id_entry_t;

static const pci_id_entry_t g_pci_ids[] = {
    /* ===== Intel (0x8086) ===== */
    { 0x8086, 0x0122, "Intel 6300ESB LPC" },
    { 0x8086, 0x0482, "Intel 82375EB PCEB" },
    { 0x8086, 0x0483, "Intel 82424ZX PCMC" },
    { 0x8086, 0x0484, "Intel 82378IB ISA" },
    { 0x8086, 0x04A3, "Intel 82434LX PCMC" },
    { 0x8086, 0x0960, "Intel 82960 i960RP" },
    { 0x8086, 0x1000, "Intel 82542 Gigabit" },
    { 0x8086, 0x1001, "Intel 82543GC Gigabit" },
    { 0x8086, 0x1004, "Intel 82543GC Gigabit" },
    { 0x8086, 0x1008, "Intel 82544EI Gigabit" },
    { 0x8086, 0x100E, "Intel 82540EM Gigabit" },
    { 0x8086, 0x100F, "Intel 82545EM Gigabit" },
    { 0x8086, 0x1010, "Intel 82546EB Gigabit" },
    { 0x8086, 0x1019, "Intel 82547EI Gigabit" },
    { 0x8086, 0x101E, "Intel 82540EP Gigabit" },
    { 0x8086, 0x1026, "Intel 82545GM Gigabit" },
    { 0x8086, 0x1029, "Intel 82559 Ethernet" },
    { 0x8086, 0x1209, "Intel 82559ER Ethernet" },
    { 0x8086, 0x1227, "Intel 82865 Ethernet" },
    { 0x8086, 0x1229, "Intel 82557/8/9 Ethernet" },
    { 0x8086, 0x122D, "Intel 82437FX TSC" },
    { 0x8086, 0x122E, "Intel 82371FB ISA" },
    { 0x8086, 0x1230, "Intel 82371FB IDE" },
    { 0x8086, 0x1234, "Intel 82371MX ISA" },
    { 0x8086, 0x1235, "Intel 82437MX TSC" },
    { 0x8086, 0x1237, "Intel 82440FX PMC" },
    { 0x8086, 0x1250, "Intel 82439HX TXC" },
    { 0x8086, 0x2410, "Intel 82801AA ICH" },
    { 0x8086, 0x2411, "Intel 82801AA ICH IDE" },
    { 0x8086, 0x2412, "Intel 82801AA ICH USB" },
    { 0x8086, 0x2413, "Intel 82801AA ICH SMBus" },
    { 0x8086, 0x2415, "Intel 82801AA AC97 Audio" },
    { 0x8086, 0x2418, "Intel 82801AA ICH LPC" },
    { 0x8086, 0x2420, "Intel 82801AB ICH0" },
    { 0x8086, 0x2440, "Intel 82801BA ICH2" },
    { 0x8086, 0x244E, "Intel 82801BA ICH2 Hub" },
    { 0x8086, 0x2480, "Intel 82801CA ICH3" },
    { 0x8086, 0x24C0, "Intel 82801DB ICH4" },
    { 0x8086, 0x24CC, "Intel 82801DBM ICH4-M" },
    { 0x8086, 0x24D0, "Intel 82801EB ICH5" },
    { 0x8086, 0x2640, "Intel 82801FB ICH6" },
    { 0x8086, 0x27B8, "Intel 82801GB ICH7" },
    { 0x8086, 0x2918, "Intel 82801IB ICH9" },
    { 0x8086, 0x3A18, "Intel 82801JIB ICH10" },
    { 0x8086, 0x7000, "Intel 82371SB PIIX3 ISA" },
    { 0x8086, 0x7010, "Intel 82371SB PIIX3 IDE" },
    { 0x8086, 0x7020, "Intel 82371SB PIIX3 USB" },
    { 0x8086, 0x7100, "Intel 82439TX MTXC" },
    { 0x8086, 0x7110, "Intel 82371AB PIIX4 ISA" },
    { 0x8086, 0x7111, "Intel 82371AB PIIX4 IDE" },
    { 0x8086, 0x7112, "Intel 82371AB PIIX4 USB" },
    { 0x8086, 0x7113, "Intel 82371AB PIIX4 ACPI" },
    { 0x8086, 0x7180, "Intel 82443LX PAC" },
    { 0x8086, 0x7190, "Intel 82443BX PAC" },
    { 0x8086, 0x7192, "Intel 82443BX PAC (AGP)" },
    { 0x8086, 0x71A0, "Intel 82443GX PAC" },
    { 0x8086, 0x71A2, "Intel 82443GX PAC (AGP)" },
    { 0x8086, 0x84C4, "Intel 82454KX PCMC" },
    { 0x8086, 0x84C5, "Intel 82453KX DRC" },
    { 0x8086, 0x84CB, "Intel 82454GX PXB" },

    /* ===== AMD/ATI (0x1002 ATI, 0x1022 AMD) ===== */
    { 0x1002, 0x4158, "ATI Mach32" },
    { 0x1002, 0x4354, "ATI Mach64 CT" },
    { 0x1002, 0x4742, "ATI Rage PRO (AGP)" },
    { 0x1002, 0x4744, "ATI Rage PRO (PCI)" },
    { 0x1002, 0x474D, "ATI Rage XL (AGP)" },
    { 0x1002, 0x474F, "ATI Rage XL" },
    { 0x1002, 0x4752, "ATI Rage XL (PCI)" },
    { 0x1002, 0x4C42, "ATI Rage LT PRO (AGP)" },
    { 0x1002, 0x4C4D, "ATI Rage Mobility" },
    { 0x1002, 0x5041, "ATI Rage 128 PA/PRO" },
    { 0x1002, 0x5144, "ATI Radeon QD" },
    { 0x1002, 0x5157, "ATI Radeon 7200 QW" },
    { 0x1002, 0x5245, "ATI Rage 128 RE/SG" },
    { 0x1022, 0x2000, "AMD 79C970 PCnet-PCI" },
    { 0x1022, 0x2001, "AMD 79C978 PCnet-PCI II" },
    { 0x1022, 0x7400, "AMD 755 ISA Bridge" },
    { 0x1022, 0x7408, "AMD 756 ISA Bridge" },
    { 0x1022, 0x7440, "AMD 768 ISA Bridge" },

    /* ===== NVIDIA (0x10DE) ===== */
    { 0x10DE, 0x0018, "RIVA 128" },
    { 0x10DE, 0x0020, "RIVA TNT" },
    { 0x10DE, 0x0028, "RIVA TNT2" },
    { 0x10DE, 0x0029, "RIVA TNT2 Ultra" },
    { 0x10DE, 0x002C, "Vanta" },
    { 0x10DE, 0x002D, "RIVA TNT2 M64" },
    { 0x10DE, 0x0100, "GeForce 256" },
    { 0x10DE, 0x0110, "GeForce2 MX/MX 400" },
    { 0x10DE, 0x0150, "GeForce2 GTS/Pro" },
    { 0x10DE, 0x0170, "GeForce4 MX 460" },
    { 0x10DE, 0x0171, "GeForce4 MX 440" },
    { 0x10DE, 0x0200, "GeForce3" },
    { 0x10DE, 0x0250, "GeForce4 Ti 4600" },

    /* ===== 3dfx (0x121A) ===== */
    { 0x121A, 0x0001, "3dfx Voodoo Graphics" },
    { 0x121A, 0x0002, "3dfx Voodoo 2" },
    { 0x121A, 0x0003, "3dfx Voodoo Banshee" },
    { 0x121A, 0x0005, "3dfx Voodoo 3" },
    { 0x121A, 0x0009, "3dfx Voodoo 4/5" },

    /* ===== S3 (0x5333) ===== */
    { 0x5333, 0x8811, "S3 Trio64" },
    { 0x5333, 0x883D, "S3 ViRGE/VX" },
    { 0x5333, 0x8901, "S3 Trio64V2/DX" },
    { 0x5333, 0x8A01, "S3 ViRGE/DX" },
    { 0x5333, 0x8A20, "S3 Savage3D" },
    { 0x5333, 0x8A22, "S3 Savage4" },
    { 0x5333, 0x8C00, "S3 ViRGE/MX" },
    { 0x5333, 0x9102, "S3 Savage 2000" },

    /* ===== VIA (0x1106) ===== */
    { 0x1106, 0x0305, "VIA VT8363 Apollo KT133" },
    { 0x1106, 0x0571, "VIA VT82C586 IDE" },
    { 0x1106, 0x0586, "VIA VT82C586 ISA" },
    { 0x1106, 0x0596, "VIA VT82C596 ISA" },
    { 0x1106, 0x0597, "VIA VT82C597 Apollo VP3" },
    { 0x1106, 0x0598, "VIA VT82C598 Apollo MVP3" },
    { 0x1106, 0x0686, "VIA VT82C686 ISA" },
    { 0x1106, 0x0691, "VIA VT82C691 Apollo PRO" },
    { 0x1106, 0x3038, "VIA VT82C586 USB" },
    { 0x1106, 0x3058, "VIA VT82C686 AC97 Audio" },
    { 0x1106, 0x3065, "VIA VT6102 Rhine-II" },
    { 0x1106, 0x3074, "VIA VT8233 ISA" },
    { 0x1106, 0x3104, "VIA VT6202 USB 2.0" },

    /* ===== SiS (0x1039) ===== */
    { 0x1039, 0x0001, "SiS 530 Host" },
    { 0x1039, 0x0008, "SiS 85C503 LPC" },
    { 0x1039, 0x0200, "SiS 5597/5598 VGA" },
    { 0x1039, 0x0496, "SiS 85C496" },
    { 0x1039, 0x0530, "SiS 530 Host" },
    { 0x1039, 0x0630, "SiS 630 Host" },
    { 0x1039, 0x0900, "SiS 900 Ethernet" },
    { 0x1039, 0x5513, "SiS 5513 IDE" },
    { 0x1039, 0x5591, "SiS 5591" },
    { 0x1039, 0x5597, "SiS 5597" },
    { 0x1039, 0x5598, "SiS 5598" },
    { 0x1039, 0x6326, "SiS 6326 VGA" },
    { 0x1039, 0x7001, "SiS 7001 USB" },
    { 0x1039, 0x7012, "SiS 7012 AC97 Audio" },

    /* ===== ALi/Acer Labs (0x10B9) ===== */
    { 0x10B9, 0x1489, "ALi M1489" },
    { 0x10B9, 0x1521, "ALi M1521 Aladdin III" },
    { 0x10B9, 0x1523, "ALi M1523 ISA" },
    { 0x10B9, 0x1531, "ALi M1531 Aladdin IV" },
    { 0x10B9, 0x1533, "ALi M1533 ISA" },
    { 0x10B9, 0x1541, "ALi M1541 Aladdin V" },
    { 0x10B9, 0x1543, "ALi M1543 ISA" },
    { 0x10B9, 0x5229, "ALi M5229 IDE" },
    { 0x10B9, 0x5237, "ALi M5237 USB" },
    { 0x10B9, 0x5451, "ALi M5451 AC97 Audio" },

    /* ===== Matrox (0x102B) ===== */
    { 0x102B, 0x0518, "Matrox Millennium" },
    { 0x102B, 0x051A, "Matrox Mystique" },
    { 0x102B, 0x051B, "Matrox Millennium II" },
    { 0x102B, 0x0520, "Matrox G200 PCI" },
    { 0x102B, 0x0521, "Matrox G200 AGP" },
    { 0x102B, 0x0525, "Matrox G400/G450" },
    { 0x102B, 0x2527, "Matrox G550" },

    /* ===== Cirrus Logic (0x1013) ===== */
    { 0x1013, 0x00A0, "Cirrus GD 5430" },
    { 0x1013, 0x00B8, "Cirrus GD 5446" },
    { 0x1013, 0x00BC, "Cirrus GD 5480" },
    { 0x1013, 0x00D4, "Cirrus GD 5464" },
    { 0x1013, 0x6005, "Cirrus CS 4281" },

    /* ===== Creative/Ensoniq (0x1102, 0x1274) ===== */
    { 0x1102, 0x0002, "Creative EMU10K1 SB Live" },
    { 0x1102, 0x0004, "Creative EMU10K2 Audigy" },
    { 0x1274, 0x1371, "Ensoniq ES1371 AudioPCI" },
    { 0x1274, 0x5000, "Ensoniq ES1370 AudioPCI" },

    /* ===== Realtek (0x10EC) ===== */
    { 0x10EC, 0x8029, "Realtek RTL8029 NE2000" },
    { 0x10EC, 0x8139, "Realtek RTL8139 Ethernet" },
    { 0x10EC, 0x8169, "Realtek RTL8169 Gigabit" },

    /* ===== Broadcom (0x14E4) ===== */
    { 0x14E4, 0x1644, "Broadcom BCM5700 Gigabit" },
    { 0x14E4, 0x1645, "Broadcom BCM5701 Gigabit" },
    { 0x14E4, 0x1677, "Broadcom BCM5751 Gigabit" },

    /* ===== LSI/Symbios (0x1000) ===== */
    { 0x1000, 0x0001, "LSI 53C810 SCSI" },
    { 0x1000, 0x000F, "LSI 53C875 SCSI" },
    { 0x1000, 0x0030, "LSI 53C1030 SCSI" },

    /* ===== Adaptec (0x9004, 0x9005) ===== */
    { 0x9004, 0x7178, "Adaptec AHA-2940" },
    { 0x9004, 0x8178, "Adaptec AHA-2940U" },
    { 0x9005, 0x00C0, "Adaptec AIC-7899A" },

    /* ===== Trident (0x1023) ===== */
    { 0x1023, 0x9440, "Trident TGUI 9440" },
    { 0x1023, 0x9660, "Trident TGUI 9660" },
    { 0x1023, 0x9750, "Trident 3DImage 9750" },
    { 0x1023, 0x9880, "Trident Blade 3D" },

    /* ===== OPTi (0x1045) ===== */
    { 0x1045, 0xC557, "OPTi 82C557" },
    { 0x1045, 0xC558, "OPTi 82C558" },
    { 0x1045, 0xC621, "OPTi 82C621" },
    { 0x1045, 0xC700, "OPTi 82C700" },

    /* ===== UMC (0x1060) ===== */
    { 0x1060, 0x0881, "UMC UM8881F" },
    { 0x1060, 0x0886, "UMC UM8886A" },
    { 0x1060, 0x8881, "UMC UM8881" },

    /* ===== Tseng Labs (0x100C) ===== */
    { 0x100C, 0x3202, "Tseng ET4000/W32p" },
    { 0x100C, 0x3208, "Tseng ET6000" },

    /* Terminator */
    { 0, 0, NULL }
};

/*============================================================================
 * MCA ADAPTER ID DATABASE - IBM PS/2 Micro Channel adapters (~50 entries)
 *============================================================================*/

typedef struct {
    unsigned int adapter_id;
    const char *name;
} mca_id_entry_t;

static const mca_id_entry_t g_mca_ids[] = {
    /* IBM Display Adapters */
    { 0x8EFD, "IBM XGA-2 Display" },
    { 0x8EFC, "IBM XGA Display" },
    { 0x8EFB, "IBM Image Adapter/A" },
    { 0x90EE, "IBM VGA Adapter" },

    /* IBM SCSI Adapters */
    { 0x8EFF, "IBM SCSI Adapter" },
    { 0x8EFE, "IBM SCSI Adapter w/Cache" },
    { 0x8F9A, "IBM Fast SCSI-2 Adapter" },
    { 0x8F9B, "IBM SCSI-2 Fast/Wide" },

    /* IBM Network Adapters */
    { 0x6FC0, "IBM Token Ring (16/4)" },
    { 0x6FC1, "IBM Token Ring (16/4) II" },
    { 0xE001, "IBM Token Ring (16/4)" },
    { 0xE000, "IBM Ethernet Adapter/A" },
    { 0x8EF5, "IBM Auto LANStreamer" },

    /* IBM Memory Adapters */
    { 0xEFFF, "IBM Memory Expansion" },
    { 0xEFFE, "IBM 2-8MB Memory" },
    { 0xEFFD, "IBM 2-14MB Memory" },
    { 0x8FDB, "IBM 32-Bit Memory" },

    /* IBM Disk Controllers */
    { 0xDDFF, "IBM ESDI Controller" },
    { 0xDFFE, "IBM ESDI Controller" },
    { 0xDF9F, "IBM IDE Adapter" },

    /* IBM Communication Adapters */
    { 0x6FC2, "IBM 3270 Connection" },
    { 0xEFE5, "IBM Async (Dual)" },
    { 0xEFE4, "IBM Async (Single)" },
    { 0xE7FF, "IBM 5250 Emulator" },

    /* Third Party - Network */
    { 0x6298, "3Com EtherLink/MC" },
    { 0x6299, "3Com EtherLink/MC 32" },
    { 0x627D, "3Com EtherLink III/MC" },
    { 0x80EC, "SMC Ethernet" },

    /* Third Party - Graphics */
    { 0x5E80, "ATI 8514/Ultra" },
    { 0x5E81, "ATI Graphics Ultra" },
    { 0x4EDD, "Matrox MG-1024" },

    /* Third Party - SCSI */
    { 0x0F1F, "Adaptec AHA-1640" },
    { 0x7012, "Buslogic BT-640A" },
    { 0x8EF2, "NCR 53C700 SCSI" },

    /* Third Party - Sound */
    { 0x5138, "Sound Blaster MCV" },
    { 0x5137, "Sound Blaster Pro MCV" },
    { 0x6BBA, "Pro AudioSpectrum 16" },

    /* Terminator */
    { 0, NULL }
};

/*============================================================================
 * ISA PLUG-AND-PLAY ID DATABASE (~45 entries)
 *============================================================================*/

typedef struct {
    unsigned long pnp_id;       /* EISA-style ID (vendor + product) */
    const char *name;
} isapnp_id_entry_t;

static const isapnp_id_entry_t g_isapnp_ids[] = {
    /* Creative Labs Sound Cards */
    { 0x0100630E, "Creative Sound Blaster 16 PnP" },
    { 0x2100630E, "Creative Sound Blaster AWE32 PnP" },
    { 0x3100630E, "Creative Sound Blaster AWE64 PnP" },
    { 0x4200630E, "Creative Sound Blaster AWE64 Gold" },
    { 0x4B00630E, "Creative Sound Blaster 16 Vibra" },

    /* ESS Technology */
    { 0x68187316, "ESS AudioDrive ES1868" },
    { 0x69187316, "ESS AudioDrive ES1869" },
    { 0x78187316, "ESS AudioDrive ES1878" },
    { 0x88187316, "ESS AudioDrive ES1888" },

    /* Yamaha */
    { 0x01008A06, "Yamaha OPL3-SA" },
    { 0x02008A06, "Yamaha OPL3-SA2" },
    { 0x03008A06, "Yamaha OPL3-SA3" },

    /* Crystal/Cirrus Logic Audio */
    { 0x00100E63, "Crystal CS4232 Audio" },
    { 0x01100E63, "Crystal CS4236 Audio" },

    /* OPTi Audio */
    { 0x01011845, "OPTi 82C930 Audio" },
    { 0x11011845, "OPTi 82C931 Audio" },

    /* Network Cards - 3Com */
    { 0x90506D50, "3Com EtherLink III ISA PnP" },
    { 0x94506D50, "3Com EtherLink III ISA PnP" },

    /* Network Cards - Intel */
    { 0x3040A436, "Intel EtherExpress PRO/10 ISA PnP" },

    /* Network Cards - SMC */
    { 0x8010634D, "SMC EZ Ethernet ISA PnP" },

    /* Network Cards - NE2000 Clones */
    { 0xD008D041, "Realtek RTL8019AS NE2000 PnP" },
    { 0xD108D041, "NE2000 Compatible ISA PnP" },

    /* Serial/Parallel Ports */
    { 0x0105D041, "Serial Port COM1" },
    { 0x0005D041, "Serial Port" },
    { 0x0104D041, "ECP Parallel Port" },
    { 0x0004D041, "Standard Parallel Port" },

    /* Modems */
    { 0x01008C4A, "US Robotics Sportster PnP" },
    { 0x02008C4A, "US Robotics Courier PnP" },
    { 0xC000D041, "PnP Modem" },

    /* Game Ports */
    { 0x0301D041, "Joystick/Game Port" },

    /* SCSI Controllers */
    { 0x0001A003, "Adaptec AHA-1542CP SCSI PnP" },

    /* Terminator */
    { 0, NULL }
};

/*============================================================================
 * LOOKUP FUNCTIONS
 *============================================================================*/

const char *enum_lookup_pci_name(unsigned int vendor, unsigned int device)
{
    int i;
    for (i = 0; g_pci_ids[i].name != NULL; i++) {
        if (g_pci_ids[i].vendor_id == vendor &&
            g_pci_ids[i].device_id == device)
            return g_pci_ids[i].name;
    }
    return NULL;
}

const char *enum_lookup_mca_name(unsigned int adapter_id)
{
    int i;
    for (i = 0; g_mca_ids[i].name != NULL; i++) {
        if (g_mca_ids[i].adapter_id == adapter_id)
            return g_mca_ids[i].name;
    }
    return NULL;
}

const char *enum_lookup_isapnp_name(unsigned long pnp_id)
{
    int i;
    for (i = 0; g_isapnp_ids[i].name != NULL; i++) {
        if (g_isapnp_ids[i].pnp_id == pnp_id)
            return g_isapnp_ids[i].name;
    }
    return NULL;
}

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

const char *enum_bus_name(unsigned char bus_type)
{
    switch (bus_type) {
        case BUS_PCI:    return "PCI";
        case BUS_PCIE:   return "PCIe";
        case BUS_MCA:    return "MCA";
        case BUS_EISA:   return "EISA";
        case BUS_ISAPNP: return "PnP";
        default:         return "???";
    }
}

int enum_count_by_bus(unsigned char bus_type, int total_count)
{
    int i, count = 0;
    for (i = 0; i < total_count; i++) {
        if (bus_type == 0 || g_devices[i].bus_type == bus_type)
            count++;
    }
    return count;
}

int enum_pcie_available(void)
{
    return g_pcie_available;
}

unsigned long enum_mcfg_base(void)
{
    return g_mcfg_base;
}

/* Decode compressed EISA vendor ID (3 chars in 2 bytes) */
static void decode_eisa_vendor(unsigned int compressed, char *vendor_str)
{
    unsigned char b0 = compressed & 0xFF;
    unsigned char b1 = (compressed >> 8) & 0xFF;

    vendor_str[0] = ((b0 >> 2) & 0x1F) + '@';
    vendor_str[1] = (((b0 & 0x03) << 3) | ((b1 >> 5) & 0x07)) + '@';
    vendor_str[2] = (b1 & 0x1F) + '@';
    vendor_str[3] = '\0';
}

/*============================================================================
 * PCI ENUMERATION
 *============================================================================*/

int enum_pci_devices(void)
{
    int count = 0;
    unsigned int bus, dev, func;
    unsigned long id, class_rev;
    unsigned char header;

    if (!pci_present()) return 0;

    for (bus = 0; bus < 256 && count < MAX_DEVICES; bus++) {
        for (dev = 0; dev < 32 && count < MAX_DEVICES; dev++) {
            for (func = 0; func < 8 && count < MAX_DEVICES; func++) {
                id = pci_read_config_dword((unsigned char)bus,
                                           (unsigned char)dev,
                                           (unsigned char)func, 0);
                if ((id & 0xFFFF) == 0xFFFF || id == 0)
                    continue;

                g_devices[count].bus_type = BUS_PCI;
                g_devices[count].bus = (unsigned char)bus;
                g_devices[count].dev = (unsigned char)dev;
                g_devices[count].func = (unsigned char)func;
                g_devices[count].vendor_id = id & 0xFFFF;
                g_devices[count].device_id = (id >> 16) & 0xFFFF;

                class_rev = pci_read_config_dword((unsigned char)bus,
                                                  (unsigned char)dev,
                                                  (unsigned char)func, 0x08);
                g_devices[count].class_code = (class_rev >> 24) & 0xFF;
                g_devices[count].subclass = (class_rev >> 16) & 0xFF;

                g_devices[count].irq = pci_read_config_byte((unsigned char)bus,
                                                            (unsigned char)dev,
                                                            (unsigned char)func, 0x3C);

                g_devices[count].io_base = pci_read_config_dword((unsigned char)bus,
                                                                  (unsigned char)dev,
                                                                  (unsigned char)func, 0x10);
                g_devices[count].mem_base = 0;

                g_devices[count].name = enum_lookup_pci_name(
                    g_devices[count].vendor_id,
                    g_devices[count].device_id);

                count++;

                if (func == 0) {
                    header = pci_read_config_byte((unsigned char)bus,
                                                  (unsigned char)dev, 0, 0x0E);
                    if (!(header & 0x80))
                        break;
                }
            }
        }
    }
    return count;
}

/*============================================================================
 * MCA ENUMERATION
 *============================================================================*/

int enum_mca_devices(void)
{
    int slot, count = 0;
    int base_index = g_device_count;
    unsigned int adapter_id;
    unsigned char pos[8];
    int i;

    if (!check_mca()) return 0;

    for (slot = 0; slot < 8 && (base_index + count) < MAX_DEVICES; slot++) {
        _disable();
        outp(MCA_ADAPTER_SETUP, (slot & 0x07) | 0x08);

        adapter_id = inp(MCA_POS_BASE) | ((unsigned int)inp(MCA_POS_BASE + 1) << 8);

        if (adapter_id == 0xFFFF || adapter_id == 0x0000) {
            outp(MCA_ADAPTER_SETUP, 0);
            _enable();
            continue;
        }

        for (i = 0; i < 8; i++) {
            pos[i] = inp(MCA_POS_BASE + i);
        }

        outp(MCA_ADAPTER_SETUP, 0);
        _enable();

        g_devices[base_index + count].bus_type = BUS_MCA;
        g_devices[base_index + count].bus = (unsigned char)slot;
        g_devices[base_index + count].dev = 0;
        g_devices[base_index + count].func = 0;
        g_devices[base_index + count].vendor_id = adapter_id;
        g_devices[base_index + count].device_id = 0;
        g_devices[base_index + count].class_code = 0;
        g_devices[base_index + count].subclass = 0;
        g_devices[base_index + count].irq = (pos[5] >> 1) & 0x07;
        if (g_devices[base_index + count].irq == 0)
            g_devices[base_index + count].irq = 0xFF;
        g_devices[base_index + count].io_base = 0;
        g_devices[base_index + count].mem_base = 0;
        g_devices[base_index + count].name = enum_lookup_mca_name(adapter_id);

        count++;
    }

    return count;
}

/*============================================================================
 * EISA ENUMERATION
 *============================================================================*/

int enum_eisa_devices(void)
{
    int slot, count = 0;
    int base_index = g_device_count;
    unsigned int port_base;
    unsigned int vendor_compressed;
    unsigned int product_id;

    if (!check_eisa()) return 0;

    for (slot = 1; slot <= 15 && (base_index + count) < MAX_DEVICES; slot++) {
        port_base = EISA_ID_OFFSET + ((unsigned int)slot << 12);

        _disable();
        vendor_compressed = inp(port_base) | ((unsigned int)inp(port_base + 1) << 8);
        product_id = inp(port_base + 2) | ((unsigned int)inp(port_base + 3) << 8);
        _enable();

        if ((vendor_compressed == 0xFFFF && product_id == 0xFFFF) ||
            (vendor_compressed == 0x0000 && product_id == 0x0000))
            continue;

        g_devices[base_index + count].bus_type = BUS_EISA;
        g_devices[base_index + count].bus = (unsigned char)slot;
        g_devices[base_index + count].dev = 0;
        g_devices[base_index + count].func = 0;
        g_devices[base_index + count].vendor_id = vendor_compressed;
        g_devices[base_index + count].device_id = product_id;
        g_devices[base_index + count].class_code = 0;
        g_devices[base_index + count].subclass = 0;
        g_devices[base_index + count].irq = 0xFF;
        g_devices[base_index + count].io_base = 0;
        g_devices[base_index + count].mem_base = 0;
        g_devices[base_index + count].name = NULL;

        count++;
    }

    return count;
}

/*============================================================================
 * ISA PLUG-AND-PLAY ENUMERATION
 *============================================================================*/

static void isapnp_write(unsigned char reg, unsigned char val)
{
    outp(ISAPNP_ADDRESS, reg);
    outp(ISAPNP_WRITE_DATA, val);
}

static unsigned char isapnp_read(unsigned char reg)
{
    outp(ISAPNP_ADDRESS, reg);
    return inp(g_isapnp_read_port);
}

static void isapnp_send_key(void)
{
    int i;
    unsigned char lfsr = 0x6A;

    outp(ISAPNP_ADDRESS, 0x00);
    outp(ISAPNP_ADDRESS, 0x00);

    for (i = 0; i < 32; i++) {
        outp(ISAPNP_ADDRESS, lfsr);
        lfsr = (lfsr >> 1) | (((lfsr ^ (lfsr >> 1)) & 1) << 7);
    }
}

static void isapnp_delay(void)
{
    int i;
    for (i = 0; i < 10; i++) {
        inp(0x80);
    }
}

static int isapnp_find_read_port(void)
{
    unsigned int port;
    unsigned char val1, val2;

    for (port = 0x203; port <= 0x3F3; port += 0x10) {
        g_isapnp_read_port = port;

        isapnp_write(ISAPNP_CONFIG_CTRL, 0x02);
        isapnp_delay();
        isapnp_send_key();

        isapnp_write(ISAPNP_WAKE, 0x00);
        isapnp_delay();

        isapnp_write(ISAPNP_SET_RD_PORT, port >> 2);
        isapnp_delay();

        val1 = isapnp_read(ISAPNP_STATUS);
        val2 = isapnp_read(ISAPNP_STATUS);

        if (val1 != 0xFF && val1 == val2) {
            return 1;
        }
    }

    return 0;
}

static int isapnp_isolate(void)
{
    int csn = 0;
    int bit, byte_idx;
    int fail_count = 0;     /* consecutive checksum failures (EN-M1 guard) */
    unsigned char serial[9];
    unsigned char checksum;
    unsigned char b1, b2;

    isapnp_write(ISAPNP_CONFIG_CTRL, 0x02);
    isapnp_delay();
    isapnp_send_key();

    isapnp_write(ISAPNP_WAKE, 0x00);
    isapnp_delay();

    isapnp_write(ISAPNP_SET_RD_PORT, g_isapnp_read_port >> 2);
    isapnp_delay();

    while (csn < 32 && fail_count < 64) {
        isapnp_write(ISAPNP_SERIAL_ISOL, 0x00);
        isapnp_delay();

        checksum = 0x6A;

        for (byte_idx = 0; byte_idx < 9; byte_idx++) {
            serial[byte_idx] = 0;

            for (bit = 0; bit < 8; bit++) {
                unsigned char data_bit;

                b1 = inp(g_isapnp_read_port);
                isapnp_delay();
                b2 = inp(g_isapnp_read_port);
                isapnp_delay();

                if (b1 == 0x55 && b2 == 0xAA) {
                    data_bit = 1;
                    serial[byte_idx] |= (1 << bit);
                } else if (b1 == 0xFF && b2 == 0xFF) {
                    goto isolation_done;        /* no card driving the bus */
                } else {
                    data_bit = 0;               /* '0' bit (or noise -> 0) */
                }

                /* Advance the checksum LFSR for EVERY serial-ID bit (bytes
                 * 0-7), feeding the ACTUAL data bit. The previous code only
                 * updated it on '1' bits and hardcoded the bit as 1, so the
                 * computed checksum never matched serial[8] for any card with
                 * a '0' in its ID (i.e. essentially all of them) and no CSN
                 * was ever assigned. Byte 8 is the checksum itself (not fed).
                 * LFSR: new_msb = LFSR[0] ^ LFSR[1] ^ data_bit; shift right. */
                if (byte_idx < 8) {
                    unsigned char fb;
                    fb = (unsigned char)((checksum ^ (checksum >> 1) ^ data_bit) & 1);
                    checksum = (unsigned char)((checksum >> 1) | (fb << 7));
                }
            }
        }

        if (serial[8] != checksum) {
            fail_count++;       /* bounded so a noisy bus can't spin forever */
            continue;
        }

        fail_count = 0;         /* reset on a good read */
        csn++;
        isapnp_write(ISAPNP_CSN, csn);
        isapnp_delay();
    }

isolation_done:
    isapnp_write(ISAPNP_CONFIG_CTRL, 0x01);

    g_isapnp_cards_found = csn;
    return csn;
}

static void isapnp_read_resources(int csn, unsigned char *irq, unsigned int *io_base)
{
    unsigned char tag;
    unsigned int len;       /* 16-bit: large-resource length is 2 bytes */
    int found_irq = 0, found_io = 0;
    int timeout = 256;
    int i;

    *irq = 0xFF;
    *io_base = 0;

    isapnp_write(ISAPNP_WAKE, csn);
    isapnp_delay();

    isapnp_write(ISAPNP_LOGICAL_DEV, 0);
    isapnp_delay();

    while (timeout-- > 0) {
        while ((isapnp_read(ISAPNP_STATUS) & 0x01) == 0 && timeout-- > 0) {
            isapnp_delay();
        }

        tag = isapnp_read(ISAPNP_RES_DATA);

        if (tag == ISAPNP_TAG_END || tag == 0x79) {
            break;
        }

        if (tag & 0x80) {
            len = isapnp_read(ISAPNP_RES_DATA);
            len |= isapnp_read(ISAPNP_RES_DATA) << 8;
            while (len-- > 0 && timeout-- > 0) {
                isapnp_read(ISAPNP_RES_DATA);
            }
        } else {
            len = tag & 0x07;

            if ((tag & 0xF8) == ISAPNP_TAG_IRQ && !found_irq) {
                unsigned int irq_mask = isapnp_read(ISAPNP_RES_DATA);
                irq_mask |= isapnp_read(ISAPNP_RES_DATA) << 8;
                for (i = 0; i < 16; i++) {
                    if (irq_mask & (1 << i)) {
                        *irq = i;
                        found_irq = 1;
                        break;
                    }
                }
                if (len > 2) isapnp_read(ISAPNP_RES_DATA);
            } else if ((tag & 0xF8) == ISAPNP_TAG_IO && !found_io) {
                isapnp_read(ISAPNP_RES_DATA);
                *io_base = isapnp_read(ISAPNP_RES_DATA);
                *io_base |= isapnp_read(ISAPNP_RES_DATA) << 8;
                found_io = 1;
                for (i = 3; i < len; i++) {
                    isapnp_read(ISAPNP_RES_DATA);
                }
            } else if ((tag & 0xF8) == ISAPNP_TAG_FIXED_IO && !found_io) {
                *io_base = isapnp_read(ISAPNP_RES_DATA);
                *io_base |= isapnp_read(ISAPNP_RES_DATA) << 8;
                found_io = 1;
                if (len > 2) isapnp_read(ISAPNP_RES_DATA);
            } else {
                while (len-- > 0) {
                    isapnp_read(ISAPNP_RES_DATA);
                }
            }
        }
    }

    isapnp_write(ISAPNP_WAKE, 0);
}

int enum_isapnp_devices(void)
{
    int count = 0;
    int base_index = g_device_count;
    int csn;
    unsigned long pnp_id;
    unsigned char irq;
    unsigned int io_base;

    _disable();
    if (!isapnp_find_read_port()) {
        _enable();
        return 0;
    }

    if (isapnp_isolate() == 0) {
        _enable();
        return 0;
    }
    _enable();

    for (csn = 1; csn <= g_isapnp_cards_found && (base_index + count) < MAX_DEVICES; csn++) {
        _disable();

        isapnp_send_key();
        isapnp_write(ISAPNP_WAKE, csn);
        isapnp_delay();

        outp(ISAPNP_ADDRESS, 0x20);
        pnp_id = inp(g_isapnp_read_port);
        outp(ISAPNP_ADDRESS, 0x21);
        pnp_id |= (unsigned long)inp(g_isapnp_read_port) << 8;
        outp(ISAPNP_ADDRESS, 0x22);
        pnp_id |= (unsigned long)inp(g_isapnp_read_port) << 16;
        outp(ISAPNP_ADDRESS, 0x23);
        pnp_id |= (unsigned long)inp(g_isapnp_read_port) << 24;

        isapnp_read_resources(csn, &irq, &io_base);

        isapnp_write(ISAPNP_CONFIG_CTRL, 0x01);
        _enable();

        if (pnp_id == 0 || pnp_id == 0xFFFFFFFF) {
            continue;
        }

        g_devices[base_index + count].bus_type = BUS_ISAPNP;
        g_devices[base_index + count].bus = csn;
        g_devices[base_index + count].dev = 0;
        g_devices[base_index + count].func = 0;
        g_devices[base_index + count].vendor_id = pnp_id & 0xFFFF;
        g_devices[base_index + count].device_id = (pnp_id >> 16) & 0xFFFF;
        g_devices[base_index + count].class_code = 0;
        g_devices[base_index + count].subclass = 0;
        g_devices[base_index + count].irq = irq;
        g_devices[base_index + count].io_base = io_base;
        g_devices[base_index + count].mem_base = 0;
        g_devices[base_index + count].name = enum_lookup_isapnp_name(pnp_id);

        count++;
    }

    return count;
}

/*============================================================================
 * PCIe EXTENDED CONFIGURATION - PROPER ACPI TABLE ENUMERATION
 *
 * MCFG (Memory Mapped Configuration) table contains the PCIe extended
 * configuration space base address. To find it properly:
 *
 * 1. Locate RSDP (Root System Description Pointer) in BIOS area
 * 2. Get RSDT (Root System Description Table) address from RSDP
 * 3. Walk RSDT entries to find MCFG table
 * 4. Extract base address from MCFG table
 *
 * Note: This only works if RSDT is in the first 1MB (real mode accessible).
 * On systems where RSDT is in extended memory, PCIe detection falls back
 * to capability-based detection only.
 *============================================================================*/

/* ACPI signatures */
#define RSDP_SIG_LO     0x20445352UL    /* "RSD " little-endian */
#define RSDP_SIG_HI     0x20525450UL    /* "PTR " little-endian */
#define RSDT_SIGNATURE  0x54445352UL    /* "RSDT" little-endian */

/*
 * Verify ACPI table checksum (sum of all bytes must be 0)
 */
static int acpi_checksum_valid(unsigned char far *table, unsigned int len)
{
    unsigned char sum = 0;
    unsigned int i;

    for (i = 0; i < len; i++) {
        sum += table[i];
    }
    return (sum == 0);
}

/*
 * Find RSDP in BIOS area (0xE0000-0xFFFFF)
 * Returns far pointer to RSDP, or NULL if not found
 */
static unsigned char far *find_rsdp(void)
{
    unsigned char far *ptr;

    /* Scan the BIOS area (E0000-FFFFF) on 16-byte boundaries via the
       segment-wise helper. The old loop advanced a far pointer's 16-bit
       offset (`ptr += 16`) and compared against an end far pointer; once the
       offset wrapped 0xFFF0->0x0000 within segment 0xE000 the segment never
       advanced, so it scanned the same 64KB window FOREVER when no RSDP was
       present in that segment. io_find_sig walks segment by segment. */
    ptr = io_find_sig(0xE000, 0xFFFF, "RSD PTR ", 8);
    if (ptr != 0 && acpi_checksum_valid(ptr, 20)) {
        return ptr;             /* signature + valid ACPI 1.0 checksum */
    }

    return (unsigned char far *)0;
}

/*
 * Convert physical address to far pointer (only works for addresses < 1MB)
 * Returns NULL if address is not accessible in real mode
 */
static unsigned char far *phys_to_far(unsigned long phys_addr)
{
    unsigned int seg, off;

    /* Can only access first 1MB in real mode */
    if (phys_addr >= 0x100000UL) {
        return (unsigned char far *)0;
    }

    /* Convert to segment:offset (segment = high 16 bits of 20-bit addr >> 4) */
    seg = (unsigned int)(phys_addr >> 4);
    off = (unsigned int)(phys_addr & 0x0F);

    return (unsigned char far *)MK_FP(seg, off);
}

/*
 * Find MCFG table by walking ACPI RSDT
 */
static int find_mcfg_table(void)
{
    unsigned char far *rsdp;
    unsigned char far *rsdt;
    unsigned long rsdt_phys;
    unsigned long rsdt_len;
    unsigned int num_entries;
    unsigned int i;
    unsigned long far *entries;
    unsigned long table_phys;
    unsigned char far *table;
    unsigned long sig;

    /* Step 1: Find RSDP */
    rsdp = find_rsdp();
    if (!rsdp) {
        return 0;  /* No ACPI on this system */
    }

    /* Step 2: Get RSDT physical address (offset 16 in RSDP) */
    rsdt_phys = *(unsigned long far *)(rsdp + 16);

    /* Convert to far pointer (must be < 1MB for real mode access) */
    rsdt = phys_to_far(rsdt_phys);
    if (!rsdt) {
        return 0;  /* RSDT in extended memory, can't access in real mode */
    }

    /* Step 3: Verify RSDT signature */
    sig = *(unsigned long far *)rsdt;
    if (sig != RSDT_SIGNATURE) {
        return 0;  /* Invalid RSDT */
    }

    /* Get RSDT length and verify checksum */
    rsdt_len = *(unsigned long far *)(rsdt + 4);
    if (rsdt_len > 4096 || rsdt_len < 36) {
        return 0;  /* Sanity check: reasonable table size */
    }
    if (!acpi_checksum_valid(rsdt, (unsigned int)rsdt_len)) {
        return 0;  /* Checksum failed */
    }

    /* Step 4: Walk RSDT entries looking for MCFG */
    /* RSDT header is 36 bytes, followed by array of 4-byte physical addresses */
    num_entries = (unsigned int)((rsdt_len - 36) / 4);
    entries = (unsigned long far *)(rsdt + 36);

    for (i = 0; i < num_entries && i < 64; i++) {
        table_phys = entries[i];

        /* Convert table address to far pointer */
        table = phys_to_far(table_phys);
        if (!table) {
            continue;  /* Table in extended memory, skip */
        }

        /* Check for MCFG signature */
        sig = *(unsigned long far *)table;
        if (sig == MCFG_SIGNATURE) {
            /*
             * MCFG table structure (relevant offsets):
             *   0-3:   Signature "MCFG"
             *   4-7:   Length
             *   44-51: Base Address (8 bytes, we use low 4)
             *   54:    Start Bus Number
             *   55:    End Bus Number
             */
            g_mcfg_base = *(unsigned long far *)(table + 44);
            g_mcfg_start_bus = *(table + 54);
            g_mcfg_end_bus = *(table + 55);

            /* Validate base address (must be above 1MB, typically 0xE0000000+) */
            if (g_mcfg_base >= 0x100000UL) {
                g_pcie_available = 1;
                return 1;
            }
        }
    }

    return 0;  /* MCFG not found */
}

static int is_pcie_device(unsigned char bus, unsigned char dev, unsigned char func)
{
    unsigned long cap_ptr_reg;
    unsigned char cap_ptr;
    unsigned long cap_header;
    int timeout = 48;

    cap_ptr_reg = pci_read_config(bus, dev, func, 0x34);
    cap_ptr = cap_ptr_reg & 0xFF;

    while (cap_ptr != 0 && cap_ptr != 0xFF && timeout-- > 0) {
        cap_header = pci_read_config(bus, dev, func, cap_ptr);

        if ((cap_header & 0xFF) == 0x10) {
            return 1;
        }

        cap_ptr = (cap_header >> 8) & 0xFF;
    }

    return 0;
}

void enum_mark_pcie_devices(void)
{
    int i;

    for (i = 0; i < g_device_count; i++) {
        if (g_devices[i].bus_type == BUS_PCI) {
            if (is_pcie_device(g_devices[i].bus, g_devices[i].dev, g_devices[i].func)) {
                g_devices[i].bus_type = BUS_PCIE;
            }
        }
    }
}

/*============================================================================
 * PUBLIC API
 *============================================================================*/

void enum_init(void)
{
    g_device_count = 0;
    g_pcie_available = 0;
    g_mcfg_base = 0;
    g_isapnp_cards_found = 0;
}

int enum_all_devices(void)
{
    g_device_count = 0;

    /* PCI enumeration */
    g_device_count += enum_pci_devices();

    /* MCA enumeration */
    g_device_count += enum_mca_devices();

    /* EISA enumeration */
    g_device_count += enum_eisa_devices();

    /* ISA Plug-and-Play enumeration */
    g_device_count += enum_isapnp_devices();

    /* Find MCFG table for PCIe */
    find_mcfg_table();

    /* Mark PCIe devices */
    enum_mark_pcie_devices();

    return g_device_count;
}
