/*============================================================================
 * CK_BCFG.C - CACHEKIT Bus Configuration Implementation
 *
 * Part of CACHEKIT v3.1
 * Last Updated: 2026-01-13
 *
 * Provides slot/device configuration for EISA and MCA buses.
 *============================================================================*/

#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include "CK_BCFG.H"
#include "CK_ENUM.H"

/*============================================================================
 * PORT DEFINITIONS
 *============================================================================*/

/* MCA (Micro Channel Architecture) */
#define MCA_MOTHERBOARD_SETUP   0x94    /* System board setup register */
#define MCA_ADAPTER_SETUP       0x96    /* Adapter enable/setup register */
#define MCA_POS_BASE            0x100   /* POS register base (0x100-0x107) */

/* EISA */
#define EISA_ID_BASE            0xC80   /* Board ID offset within slot */
#define EISA_CTRL_BASE          0xC84   /* Control register offset */

/*============================================================================
 * EISA NVM PORT DEFINITIONS - VENDOR-SPECIFIC
 *
 * Different EISA system vendors use different NVM access methods:
 *
 * COMPAQ (Most common EISA systems):
 *   Ports 0x800-0x802 for Extended CMOS NVM
 *   64 bytes per slot, checksum at byte 7
 *
 * HP (Hewlett-Packard):
 *   Ports 0xC00-0xC02 for Extended CMOS
 *   Some models use INT 15h AH=D8h services
 *
 * DEC (Digital Equipment Corporation):
 *   Uses INT 15h AH=D8h EISA CMOS services
 *   AL=00h: Read, AL=01h: Write, AL=02h: Get Info
 *
 * AMI EISA BIOS:
 *   Ports 0x800-0x802 (Compaq-compatible)
 *   May also support INT 15h AH=D8h
 *
 * AWARD/PHOENIX EISA:
 *   Usually Compaq-compatible 0x800-0x802
 *   Some use port 0x22/0x23 with bank select
 *
 * OLIVETTI:
 *   Ports 0xC00-0xC02 (HP-compatible)
 *
 * NCR:
 *   INT 15h AH=D8h services preferred
 *
 *============================================================================*/

/* Compaq/AMI/Award - Standard EISA NVM ports */
#define EISA_NVM_COMPAQ_ADDR_LO     0x800
#define EISA_NVM_COMPAQ_ADDR_HI     0x801
#define EISA_NVM_COMPAQ_DATA        0x802

/* HP/Olivetti - Alternate NVM ports */
#define EISA_NVM_HP_ADDR_LO         0xC00
#define EISA_NVM_HP_ADDR_HI         0xC01
#define EISA_NVM_HP_DATA            0xC02

/* Legacy aliases for backward compatibility */
#define EISA_NVM_ADDR_LO            EISA_NVM_COMPAQ_ADDR_LO
#define EISA_NVM_ADDR_HI            EISA_NVM_COMPAQ_ADDR_HI
#define EISA_NVM_DATA               EISA_NVM_COMPAQ_DATA

/*============================================================================
 * MODULE STATE
 *============================================================================*/

/* Note: EISA_VENDOR_* constants are defined in CK_BCFG.H */

static buscfg_state_t g_buscfg_state;
static unsigned char g_eisa_bios_vendor = EISA_VENDOR_UNKNOWN;
static char g_eisa_bios_vendor_name[32] = "Unknown";
static unsigned char g_eisa_int15_available = 0;  /* INT 15h AH=D8h support */

/*============================================================================
 * EMBEDDED CARD DATABASE - EISA (~130 common adapter cards)
 *
 * Reference: Linux kernel eisa.ids database
 * https://github.com/torvalds/linux/blob/master/drivers/eisa/eisa.ids
 *============================================================================*/

static const card_db_entry_t g_eisa_cards[] = {
    /*========================================================================
     * COMPAQ - SCSI Controllers and Storage
     *========================================================================*/
    { 0x110E, 0x0001, "Compaq 32-bit SCSI-2 Controller", BUS_EISA, 14, 0, 0, 0 },
    { 0x110E, 0x0002, "Compaq Integrated SCSI-2", BUS_EISA, 14, 0, 0, 0 },
    { 0x110E, 0x0046, "Compaq Smart Array", BUS_EISA, 14, 0, 0, 0 },
    { 0x110E, 0x0E11, "Compaq 32-Bit Fast SCSI-2", BUS_EISA, 14, 0, 0, 0 },
    { 0x110E, 0x4000, "Compaq SMART-2/E Array", BUS_EISA, 14, 0, 0, 0 },
    { 0x110E, 0x4002, "Compaq SMART-2SL Array", BUS_EISA, 14, 0, 0, 0 },
    { 0x110E, 0x4010, "Compaq Smart Array 3200", BUS_EISA, 14, 0, 0, 0 },
    { 0x110E, 0x4020, "Compaq Smart Array 3100ES", BUS_EISA, 14, 0, 0, 0 },
    { 0x110E, 0x4030, "Compaq Smart Array 221", BUS_EISA, 14, 0, 0, 0 },

    /* Compaq - Network */
    { 0x110E, 0x004A, "Compaq NetFlex Controller", BUS_EISA, 5, 0, 0x300, 0 },
    { 0x110E, 0x6000, "Compaq NetFlex ENET-TR", BUS_EISA, 5, 0, 0x300, 0 },
    { 0x110E, 0x6001, "Compaq NetFlex-2 ENET", BUS_EISA, 5, 0, 0x300, 0 },
    { 0x110E, 0x6002, "Compaq NetFlex-2 TR", BUS_EISA, 5, 0, 0x300, 0 },
    { 0x110E, 0x6010, "Compaq NetFlex-2 DualPort", BUS_EISA, 5, 0, 0x300, 0 },
    { 0x110E, 0x6020, "Compaq NetFlex-3 Controller", BUS_EISA, 5, 0, 0x300, 0 },
    { 0x110E, 0x6030, "Compaq Netelligent 10/100 TX", BUS_EISA, 5, 0, 0x300, 0 },

    /* Compaq - Graphics */
    { 0x110E, 0x7000, "Compaq QVision 1024/E", BUS_EISA, 0, 0, 0, 0 },
    { 0x110E, 0x7001, "Compaq QVision 1024/E (v2)", BUS_EISA, 0, 0, 0, 0 },
    { 0x110E, 0x7002, "Compaq QVision 1280/E", BUS_EISA, 0, 0, 0, 0 },

    /*========================================================================
     * ADAPTEC - SCSI Host Adapters
     *========================================================================*/
    { 0x0904, 0x0100, "Adaptec AHA-1740 SCSI", BUS_EISA, 11, 5, 0x1C00, 0 },
    { 0x0904, 0x0200, "Adaptec AHA-1740A SCSI", BUS_EISA, 11, 5, 0x1C00, 0 },
    { 0x0904, 0x1505, "Adaptec AVA-1505 SCSI", BUS_EISA, 11, 0, 0x140, 0 },
    { 0x0904, 0x1510, "Adaptec AHA-1510 SCSI", BUS_EISA, 11, 5, 0x340, 0 },
    { 0x0904, 0x1520, "Adaptec AHA-1520 SCSI", BUS_EISA, 11, 5, 0x340, 0 },
    { 0x0904, 0x1522, "Adaptec AHA-1522 SCSI", BUS_EISA, 11, 5, 0x340, 0 },
    { 0x0904, 0x1740, "Adaptec AHA-1740/1742 SCSI", BUS_EISA, 11, 5, 0x1C00, 0 },
    { 0x0904, 0x1744, "Adaptec AHA-1744 SCSI", BUS_EISA, 11, 5, 0x1C00, 0 },
    { 0x0904, 0x2740, "Adaptec AHA-2740 SCSI", BUS_EISA, 11, 5, 0x1C00, 0 },
    { 0x0904, 0x2742, "Adaptec AHA-2742 SCSI", BUS_EISA, 11, 5, 0x1C00, 0 },
    { 0x0904, 0x2840, "Adaptec AHA-2840 VL SCSI", BUS_EISA, 11, 5, 0x330, 0 },

    /*========================================================================
     * BUSLOGIC - SCSI Host Adapters
     *========================================================================*/
    { 0x104B, 0x0140, "BusLogic BT-540CF SCSI", BUS_EISA, 11, 5, 0x330, 0 },
    { 0x104B, 0x0542, "BusLogic BT-542B SCSI", BUS_EISA, 11, 5, 0x330, 0 },
    { 0x104B, 0x0545, "BusLogic BT-545S SCSI", BUS_EISA, 11, 5, 0x330, 0 },
    { 0x104B, 0x0640, "BusLogic BT-640A SCSI", BUS_EISA, 11, 5, 0x330, 0 },
    { 0x104B, 0x0742, "BusLogic BT-742A SCSI", BUS_EISA, 11, 5, 0x330, 0 },
    { 0x104B, 0x0747, "BusLogic BT-747S SCSI", BUS_EISA, 11, 5, 0x330, 0 },
    { 0x104B, 0x0757, "BusLogic BT-757S SCSI", BUS_EISA, 11, 5, 0x330, 0 },
    { 0x104B, 0x0946, "BusLogic BT-946C PCI SCSI", BUS_EISA, 11, 0, 0, 0 },
    { 0x104B, 0x0958, "BusLogic BT-958 PCI SCSI", BUS_EISA, 11, 0, 0, 0 },

    /*========================================================================
     * DPT - SCSI RAID Controllers
     *========================================================================*/
    { 0x1260, 0x2001, "DPT PM2001 SCSI", BUS_EISA, 14, 5, 0x1C00, 0 },
    { 0x1260, 0x2012, "DPT PM2012A SCSI", BUS_EISA, 14, 5, 0x1C00, 0 },
    { 0x1260, 0x2022, "DPT PM2022A SCSI", BUS_EISA, 14, 5, 0x1C00, 0 },
    { 0x1260, 0x2024, "DPT PM2024 SCSI", BUS_EISA, 14, 5, 0x1C00, 0 },
    { 0x1260, 0x2122, "DPT PM2122A Cache", BUS_EISA, 14, 5, 0x1C00, 0 },
    { 0x1260, 0x2124, "DPT PM2124 Cache", BUS_EISA, 14, 5, 0x1C00, 0 },
    { 0x1260, 0x2144, "DPT PM2144UW Cache", BUS_EISA, 14, 5, 0x1C00, 0 },
    { 0x1260, 0x3222, "DPT PM3222 SmartRAID IV", BUS_EISA, 14, 5, 0x1C00, 0 },
    { 0x1260, 0x3224, "DPT PM3224 SmartRAID IV", BUS_EISA, 14, 5, 0x1C00, 0 },

    /*========================================================================
     * NCR / SYMBIOS - SCSI Controllers
     *========================================================================*/
    { 0x254A, 0x5300, "NCR 53C700 SCSI", BUS_EISA, 11, 0, 0, 0 },
    { 0x254A, 0x5310, "NCR 53C710 SCSI", BUS_EISA, 11, 0, 0, 0 },
    { 0x254A, 0x7000, "NCR 53C700-66 SCSI", BUS_EISA, 11, 0, 0, 0 },
    { 0x254A, 0x7100, "NCR 53C710 SCSI", BUS_EISA, 11, 0, 0, 0 },
    { 0x254A, 0x8100, "NCR 53C810 PCI SCSI", BUS_EISA, 11, 0, 0, 0 },
    { 0x254A, 0x8250, "NCR 53C825 Wide SCSI", BUS_EISA, 11, 0, 0, 0 },

    /*========================================================================
     * FUTURE DOMAIN - SCSI Controllers
     *========================================================================*/
    { 0x0FD0, 0x0850, "Future Domain TMC-850 SCSI", BUS_EISA, 11, 0, 0x140, 0 },
    { 0x0FD0, 0x1610, "Future Domain TMC-1610 SCSI", BUS_EISA, 11, 0, 0x140, 0 },
    { 0x0FD0, 0x1650, "Future Domain TMC-1650 SCSI", BUS_EISA, 11, 0, 0x140, 0 },
    { 0x0FD0, 0x1680, "Future Domain TMC-1680 SCSI", BUS_EISA, 11, 0, 0x140, 0 },
    { 0x0FD0, 0x18C0, "Future Domain TMC-18C30 SCSI", BUS_EISA, 11, 0, 0x140, 0 },

    /*========================================================================
     * ULTRASTOR - SCSI Controllers
     *========================================================================*/
    { 0x554C, 0x0001, "UltraStor 14F SCSI", BUS_EISA, 11, 5, 0x340, 0 },
    { 0x554C, 0x0002, "UltraStor 24F SCSI", BUS_EISA, 11, 5, 0x1C00, 0 },
    { 0x554C, 0x0003, "UltraStor 34F SCSI", BUS_EISA, 11, 5, 0x1C00, 0 },
    { 0x554C, 0x0024, "UltraStor 24FA SCSI", BUS_EISA, 11, 5, 0x1C00, 0 },

    /*========================================================================
     * MYLEX - RAID Controllers
     *========================================================================*/
    { 0x2E98, 0x0010, "Mylex DAC960 RAID", BUS_EISA, 15, 0, 0, 0 },
    { 0x2E98, 0x0020, "Mylex DAC960-A RAID", BUS_EISA, 15, 0, 0, 0 },
    { 0x2E98, 0x0030, "Mylex DAC960-P RAID", BUS_EISA, 15, 0, 0, 0 },
    { 0x2E98, 0x0040, "Mylex AcceleRAID 150", BUS_EISA, 15, 0, 0, 0 },

    /*========================================================================
     * ICP VORTEX - RAID Controllers
     *========================================================================*/
    { 0x2120, 0x3000, "ICP GDT3000B SCSI Cache", BUS_EISA, 11, 0, 0, 0 },
    { 0x2120, 0x3010, "ICP GDT3010A SCSI Cache", BUS_EISA, 11, 0, 0, 0 },
    { 0x2120, 0x3020, "ICP GDT3020 Dual SCSI", BUS_EISA, 11, 0, 0, 0 },

    /*========================================================================
     * 3COM - Network Adapters
     *========================================================================*/
    { 0x6D50, 0x0040, "3Com 3C503 EtherLink II", BUS_EISA, 3, 0, 0x300, 0xD8000 },
    { 0x6D50, 0x0070, "3Com 3C507 EtherLink 16", BUS_EISA, 3, 0, 0x300, 0xD0000 },
    { 0x6D50, 0x5090, "3Com 3C509 EtherLink III", BUS_EISA, 10, 0, 0x300, 0 },
    { 0x6D50, 0x5091, "3Com 3C509-TP EtherLink III", BUS_EISA, 10, 0, 0x300, 0 },
    { 0x6D50, 0x5092, "3Com 3C509-Combo", BUS_EISA, 10, 0, 0x300, 0 },
    { 0x6D50, 0x5094, "3Com 3C509B-C EtherLink III", BUS_EISA, 10, 0, 0x300, 0 },
    { 0x6D50, 0x5790, "3Com 3C579 EtherLink III EISA", BUS_EISA, 10, 0, 0, 0 },
    { 0x6D50, 0x5791, "3Com 3C579-TP EtherLink III", BUS_EISA, 10, 0, 0, 0 },
    { 0x6D50, 0x5920, "3Com 3C592 EtherLink III BM", BUS_EISA, 10, 0, 0, 0 },
    { 0x6D50, 0x5970, "3Com 3C597 Fast EtherLink", BUS_EISA, 10, 0, 0, 0 },
    { 0x6D50, 0x5971, "3Com 3C597-TX Fast EtherLink", BUS_EISA, 10, 0, 0, 0 },
    { 0x6D50, 0x3190, "3Com 3C319 TokenLink Velocity", BUS_EISA, 3, 0, 0, 0 },

    /*========================================================================
     * INTEL - Network Adapters
     *========================================================================*/
    { 0x25D4, 0x0510, "Intel EtherExpress 16", BUS_EISA, 10, 0, 0x300, 0xD0000 },
    { 0x25D4, 0x0520, "Intel EtherExpress PRO/10", BUS_EISA, 10, 0, 0x300, 0 },
    { 0x25D4, 0x0530, "Intel EtherExpress PRO/10+", BUS_EISA, 10, 0, 0x300, 0 },
    { 0x25D4, 0x8100, "Intel EtherExpress 32 EISA", BUS_EISA, 10, 0, 0, 0 },
    { 0x25D4, 0x8200, "Intel EtherExpress Flash32", BUS_EISA, 10, 0, 0, 0 },
    { 0x25D4, 0x8300, "Intel EtherExpress PRO/100", BUS_EISA, 10, 0, 0, 0 },
    { 0x25D4, 0x9100, "Intel TokenExpress EISA 16/4", BUS_EISA, 10, 0, 0, 0 },

    /*========================================================================
     * DIGITAL EQUIPMENT (DEC) - Network Adapters
     *========================================================================*/
    { 0x1028, 0x0001, "DEC EtherWORKS LC", BUS_EISA, 5, 0, 0x300, 0xD0000 },
    { 0x1028, 0x0300, "DEC DE200 EtherWORKS Turbo", BUS_EISA, 5, 0, 0x300, 0xD0000 },
    { 0x1028, 0x0310, "DEC DE201 EtherWORKS TP", BUS_EISA, 5, 0, 0x300, 0xD0000 },
    { 0x1028, 0x4200, "DEC DE422 EtherWORKS EISA", BUS_EISA, 5, 0, 0, 0 },
    { 0x1028, 0x4250, "DEC DE425 EtherWORKS EISA", BUS_EISA, 5, 0, 0, 0 },
    { 0x1028, 0x4500, "DEC DE450 EtherWORKS EISA", BUS_EISA, 5, 0, 0, 0 },
    { 0x1028, 0x5000, "DEC DE500 Fast Ethernet", BUS_EISA, 5, 0, 0, 0 },
    { 0x1028, 0x6000, "DEC FDDIcontroller/EISA", BUS_EISA, 5, 0, 0, 0 },
    { 0x1028, 0x6100, "DEC DEFEA FDDI EISA", BUS_EISA, 5, 0, 0, 0 },

    /*========================================================================
     * HEWLETT-PACKARD - Network Adapters
     *========================================================================*/
    { 0x3E07, 0x2000, "HP 27245A EtherTwist EISA", BUS_EISA, 5, 0, 0x300, 0 },
    { 0x3E07, 0x2100, "HP 27246A EtherTwist BNC", BUS_EISA, 5, 0, 0x300, 0 },
    { 0x3E07, 0x2200, "HP 27247A EtherTwist Combo", BUS_EISA, 5, 0, 0x300, 0 },
    { 0x3E07, 0x2248, "HP 27248A EtherTwist EISA/32", BUS_EISA, 5, 0, 0, 0 },
    { 0x3E07, 0x2577, "HP J2577A 10/100VG EISA LAN", BUS_EISA, 5, 0, 0, 0 },

    /*========================================================================
     * SMC - Network Adapters
     *========================================================================*/
    { 0x534D, 0x0001, "SMC WD8003E Ethernet", BUS_EISA, 3, 0, 0x280, 0xD0000 },
    { 0x534D, 0x0002, "SMC WD8013EP/A Ethernet", BUS_EISA, 3, 0, 0x280, 0xD0000 },
    { 0x534D, 0x0110, "SMC 8232 Elite32 EISA", BUS_EISA, 10, 0, 0x300, 0xD0000 },
    { 0x534D, 0x0120, "SMC 8332 Elite32 BNC", BUS_EISA, 10, 0, 0x300, 0xD0000 },
    { 0x534D, 0x0130, "SMC 8432 EtherPower EISA", BUS_EISA, 10, 0, 0x300, 0 },
    { 0x534D, 0x8010, "SMC 8013EP/A", BUS_EISA, 10, 0, 0x280, 0xD0000 },
    { 0x534D, 0x8013, "SMC Elite Ultra", BUS_EISA, 10, 0, 0x280, 0xD0000 },

    /*========================================================================
     * MADGE - Token Ring Adapters
     *========================================================================*/
    { 0x2E2A, 0x0001, "Madge Smart 16/4 AT", BUS_EISA, 2, 0, 0xA20, 0xD8000 },
    { 0x2E2A, 0x0002, "Madge Smart 16/4 EISA", BUS_EISA, 2, 0, 0, 0 },
    { 0x2E2A, 0x0010, "Madge Smart 16/4 Ringnode", BUS_EISA, 2, 0, 0, 0 },
    { 0x2E2A, 0x0020, "Madge Smart EISA Ringnode", BUS_EISA, 2, 0, 0, 0 },

    /*========================================================================
     * ATI - Graphics Adapters
     *========================================================================*/
    { 0x1002, 0x4158, "ATI Mach32 EISA", BUS_EISA, 0, 0, 0, 0 },
    { 0x1002, 0x4354, "ATI Mach32 CT EISA", BUS_EISA, 0, 0, 0, 0 },
    { 0x1002, 0x4758, "ATI Graphics Ultra Pro", BUS_EISA, 0, 0, 0, 0 },

    /*========================================================================
     * MATROX - Graphics Adapters
     *========================================================================*/
    { 0x102B, 0x0100, "Matrox IM-1280 EISA", BUS_EISA, 0, 0, 0, 0 },
    { 0x102B, 0x0500, "Matrox MGA-II EISA", BUS_EISA, 0, 0, 0, 0 },
    { 0x102B, 0x0518, "Matrox Millennium EISA", BUS_EISA, 0, 0, 0, 0 },
    { 0x102B, 0x1000, "Matrox MGA-1024SG EISA", BUS_EISA, 0, 0, 0, 0 },

    /*========================================================================
     * S3 - Graphics Adapters
     *========================================================================*/
    { 0x5333, 0x8811, "S3 86C911 EISA", BUS_EISA, 0, 0, 0, 0 },
    { 0x5333, 0x8928, "S3 86C928 EISA", BUS_EISA, 0, 0, 0, 0 },
    { 0x5333, 0x8864, "S3 Vision864 EISA", BUS_EISA, 0, 0, 0, 0 },
    { 0x5333, 0x8964, "S3 Vision964 EISA", BUS_EISA, 0, 0, 0, 0 },
    { 0x5333, 0x8968, "S3 Vision968 EISA", BUS_EISA, 0, 0, 0, 0 },

    /*========================================================================
     * TSENG LABS - Graphics Adapters
     *========================================================================*/
    { 0x5450, 0x4000, "Tseng Labs ET4000 EISA", BUS_EISA, 0, 0, 0, 0 },
    { 0x5450, 0x4001, "Tseng Labs ET4000/W32", BUS_EISA, 0, 0, 0, 0 },
    { 0x5450, 0x4002, "Tseng Labs ET4000/W32i", BUS_EISA, 0, 0, 0, 0 },

    /*========================================================================
     * CIRRUS LOGIC - Graphics Adapters
     *========================================================================*/
    { 0x1013, 0x0038, "Cirrus Logic GD5434", BUS_EISA, 0, 0, 0, 0 },
    { 0x1013, 0x0040, "Cirrus Logic GD5440", BUS_EISA, 0, 0, 0, 0 },
    { 0x1013, 0x00A0, "Cirrus Logic GD5446", BUS_EISA, 0, 0, 0, 0 },

    /*========================================================================
     * NUMBER NINE - Graphics Adapters
     *========================================================================*/
    { 0x3A79, 0x0001, "Number Nine GXE", BUS_EISA, 0, 0, 0, 0 },
    { 0x3A79, 0x0002, "Number Nine GXE64", BUS_EISA, 0, 0, 0, 0 },
    { 0x3A79, 0x0003, "Number Nine GXE64 Pro", BUS_EISA, 0, 0, 0, 0 },

    /*========================================================================
     * WEITEK - Graphics Adapters
     *========================================================================*/
    { 0x578A, 0x0001, "Weitek Power 9000 EISA", BUS_EISA, 0, 0, 0, 0 },
    { 0x578A, 0x0002, "Weitek Power 9100 EISA", BUS_EISA, 0, 0, 0, 0 },

    /*========================================================================
     * SOUND / MULTIMEDIA
     *========================================================================*/
    { 0x0F24, 0x0001, "Creative Sound Blaster Pro", BUS_EISA, 5, 1, 0x220, 0 },
    { 0x0F24, 0x0016, "Creative Sound Blaster 16", BUS_EISA, 5, 1, 0x220, 0 },
    { 0x0F24, 0x0032, "Creative Sound Blaster AWE32", BUS_EISA, 5, 1, 0x220, 0 },
    { 0x2F5E, 0x0001, "Media Vision Pro AudioSpectrum", BUS_EISA, 11, 7, 0x388, 0 },
    { 0x2F5E, 0x0016, "Media Vision PAS 16", BUS_EISA, 11, 7, 0x388, 0 },
    { 0x2120, 0x0001, "Gravis UltraSound", BUS_EISA, 11, 7, 0x220, 0 },
    { 0x2120, 0x0002, "Gravis UltraSound MAX", BUS_EISA, 11, 7, 0x220, 0 },

    /*========================================================================
     * MODEMS / SERIAL
     *========================================================================*/
    { 0x554E, 0x0001, "US Robotics Sportster", BUS_EISA, 3, 0, 0x3F8, 0 },
    { 0x554E, 0x0002, "US Robotics Courier", BUS_EISA, 3, 0, 0x3F8, 0 },
    { 0x554E, 0x0288, "US Robotics Sportster 28.8", BUS_EISA, 3, 0, 0x3F8, 0 },
    { 0x241C, 0x0001, "Hayes Optima 96", BUS_EISA, 3, 0, 0x3F8, 0 },
    { 0x241C, 0x0002, "Hayes Optima 144", BUS_EISA, 3, 0, 0x3F8, 0 },

    /*========================================================================
     * MULTI-PORT SERIAL Controllers
     *========================================================================*/
    { 0x1137, 0x0001, "DigiBoard PC/Xe EISA", BUS_EISA, 5, 0, 0, 0 },
    { 0x1137, 0x0002, "DigiBoard PC/Xi EISA", BUS_EISA, 5, 0, 0, 0 },
    { 0x1137, 0x0004, "DigiBoard EPC/X EISA", BUS_EISA, 5, 0, 0, 0 },
    { 0x1575, 0x0001, "Equinox SST-16 EISA", BUS_EISA, 11, 0, 0, 0 },
    { 0x1575, 0x0002, "Equinox SST-64 EISA", BUS_EISA, 11, 0, 0, 0 },
    { 0x0D1A, 0x0001, "Comtrol RocketPort EISA", BUS_EISA, 11, 0, 0, 0 },
    { 0x5410, 0x0001, "Specialix SI/XIO Host", BUS_EISA, 11, 0, 0, 0 },

    /*========================================================================
     * Terminator
     *========================================================================*/
    { 0, 0, NULL, 0, 0, 0, 0, 0 }
};

/*============================================================================
 * EMBEDDED CARD DATABASE - MCA (~50 common cards)
 * (Uses adapter_id in vendor_id field, device_id = 0)
 *============================================================================*/

static const card_db_entry_t g_mca_cards[] = {
    /* IBM Display Adapters */
    { 0x8EFD, 0, "IBM XGA-2 Display Adapter/A", BUS_MCA, 0, 0, 0, 0 },
    { 0x8EFC, 0, "IBM XGA Display Adapter/A", BUS_MCA, 0, 0, 0, 0 },
    { 0x8EFB, 0, "IBM Image Adapter/A", BUS_MCA, 0, 0, 0, 0 },
    { 0x90EE, 0, "IBM VGA Adapter", BUS_MCA, 0, 0, 0, 0 },

    /* IBM SCSI Adapters */
    { 0x8EFF, 0, "IBM SCSI Adapter", BUS_MCA, 14, 0, 0x3540, 0 },
    { 0x8EFE, 0, "IBM SCSI Adapter w/Cache", BUS_MCA, 14, 0, 0x3540, 0 },
    { 0x8F9A, 0, "IBM Fast SCSI-2 Adapter/A", BUS_MCA, 14, 0, 0x3540, 0 },
    { 0x8F9B, 0, "IBM SCSI-2 Fast/Wide Adapter/A", BUS_MCA, 14, 0, 0x3540, 0 },
    { 0x8F98, 0, "IBM Raid SCSI Adapter", BUS_MCA, 14, 0, 0x3540, 0 },

    /* IBM Network Adapters */
    { 0x6FC0, 0, "IBM Token Ring 16/4 Adapter/A", BUS_MCA, 2, 0, 0xA20, 0xD4000 },
    { 0x6FC1, 0, "IBM Token Ring 16/4 Adapter II/A", BUS_MCA, 2, 0, 0xA20, 0xD4000 },
    { 0xE001, 0, "IBM Token Ring Adapter/A", BUS_MCA, 2, 0, 0xA20, 0xD4000 },
    { 0xE000, 0, "IBM Ethernet Adapter/A", BUS_MCA, 3, 0, 0x300, 0 },
    { 0x8EF5, 0, "IBM Auto LANStreamer MC 32", BUS_MCA, 10, 0, 0x300, 0 },

    /* IBM Memory Adapters */
    { 0xEFFF, 0, "IBM Memory Expansion Adapter", BUS_MCA, 0, 0, 0, 0 },
    { 0xEFFE, 0, "IBM 2-8MB Memory Adapter/A", BUS_MCA, 0, 0, 0, 0 },
    { 0xEFFD, 0, "IBM 2-14MB Memory Adapter/A", BUS_MCA, 0, 0, 0, 0 },
    { 0x8FDB, 0, "IBM 32-bit Memory Adapter", BUS_MCA, 0, 0, 0, 0 },

    /* IBM Disk Controllers */
    { 0xDDFF, 0, "IBM ESDI Fixed Disk Controller", BUS_MCA, 14, 0, 0x3510, 0 },
    { 0xDFFE, 0, "IBM ESDI Adapter/A", BUS_MCA, 14, 0, 0x3510, 0 },
    { 0xDF9F, 0, "IBM IDE Adapter/A", BUS_MCA, 14, 0, 0x1F0, 0 },

    /* IBM Communication Adapters */
    { 0x6FC2, 0, "IBM 3270 Connection", BUS_MCA, 0, 0, 0, 0 },
    { 0xEFE5, 0, "IBM Dual Async Adapter/A", BUS_MCA, 3, 0, 0x3F8, 0 },
    { 0xEFE4, 0, "IBM Async Adapter/A", BUS_MCA, 4, 0, 0x3F8, 0 },

    /* Third Party - Network */
    { 0x6298, 0, "3Com EtherLink/MC", BUS_MCA, 3, 0, 0x300, 0 },
    { 0x6299, 0, "3Com EtherLink/MC 32", BUS_MCA, 3, 0, 0x300, 0 },
    { 0x627D, 0, "3Com EtherLink III/MC", BUS_MCA, 10, 0, 0x300, 0 },
    { 0x80EC, 0, "SMC Ethernet MC", BUS_MCA, 3, 0, 0x280, 0xD0000 },

    /* Third Party - SCSI */
    { 0x0F1F, 0, "Adaptec AHA-1640 SCSI", BUS_MCA, 11, 0, 0x330, 0 },
    { 0x7012, 0, "BusLogic BT-640A SCSI", BUS_MCA, 11, 5, 0x330, 0 },
    { 0x8EF2, 0, "NCR 53C700 SCSI", BUS_MCA, 11, 0, 0x330, 0 },
    { 0x8FA2, 0, "Future Domain TMC-850 SCSI", BUS_MCA, 11, 0, 0x140, 0 },

    /* Third Party - Graphics */
    { 0x5E80, 0, "ATI 8514/Ultra MC", BUS_MCA, 0, 0, 0, 0 },
    { 0x5E81, 0, "ATI Graphics Ultra MC", BUS_MCA, 0, 0, 0, 0 },
    { 0x4EDD, 0, "Matrox MG-1024 MC", BUS_MCA, 0, 0, 0, 0 },

    /* Third Party - Sound */
    { 0x5138, 0, "Sound Blaster MCV", BUS_MCA, 5, 1, 0x220, 0 },
    { 0x5137, 0, "Sound Blaster Pro MCV", BUS_MCA, 5, 1, 0x220, 0 },
    { 0x6BBA, 0, "Pro AudioSpectrum 16 MC", BUS_MCA, 5, 1, 0x388, 0 },

    /* Third Party - Modems */
    { 0x5946, 0, "Intel SatisFAXtion Modem MC", BUS_MCA, 4, 0, 0x3E8, 0 },
    { 0x50AB, 0, "US Robotics Courier V.32bis MC", BUS_MCA, 4, 0, 0x3E8, 0 },

    /* Terminator */
    { 0, 0, NULL, 0, 0, 0, 0, 0 }
};

/*============================================================================
 * BUS DETECTION
 *============================================================================*/

/* Check for EISA bus (look for EISA signature in BIOS ROM) */
static int check_eisa_bus(void)
{
    char far *sig = (char far *)MK_FP(0xF000, 0xFFD9);
    return (sig[0] == 'E' && sig[1] == 'I' && sig[2] == 'S' && sig[3] == 'A');
}

/* Check for MCA bus via INT 15h AH=C0h */
static int check_mca_bus(void)
{
    union REGS regs;
    struct SREGS sregs;
    unsigned char far *config;
    unsigned char features;

    regs.h.ah = 0xC0;
    int86x(0x15, &regs, &regs, &sregs);

    if (regs.x.cflag) return 0;  /* Function not supported */

    config = (unsigned char far *)MK_FP(sregs.es, regs.x.bx);
    features = config[5];  /* Feature byte 1 */

    return (features & 0x02) ? 1 : 0;  /* Bit 1 = MCA bus present */
}

unsigned char buscfg_detect_bus(void)
{
    if (check_eisa_bus()) return BUS_EISA;
    if (check_mca_bus()) return BUS_MCA;
    return 0;
}

/*============================================================================
 * EISA BIOS VENDOR DETECTION
 *
 * Scans ROM BIOS area (F000:0000-F000:FFFF) for vendor identification strings.
 * Also checks for INT 15h AH=D8h EISA CMOS services support.
 *
 * This is critical for NVM operations because different vendors use different:
 *   - NVM port addresses (0x800 vs 0xC00)
 *   - NVM data formats (slot size, checksum location)
 *   - Access methods (direct I/O vs INT 15h services)
 *============================================================================*/

/* BIOS signature strings to search for */
typedef struct {
    const char *signature;
    unsigned char vendor_type;
    const char *vendor_name;
} bios_signature_t;

static const bios_signature_t g_bios_signatures[] = {
    /* Compaq - most common EISA vendor */
    { "COMPAQ",         EISA_VENDOR_COMPAQ,     "Compaq" },
    { "Compaq",         EISA_VENDOR_COMPAQ,     "Compaq" },

    /* Hewlett-Packard */
    { "HEWLETT-PACKARD", EISA_VENDOR_HP,        "HP" },
    { "Hewlett-Packard", EISA_VENDOR_HP,        "HP" },
    { "(C) Copyright Hewlett", EISA_VENDOR_HP,  "HP" },
    { "HP Vectra",      EISA_VENDOR_HP,         "HP Vectra" },

    /* Digital Equipment Corporation */
    { "DIGITAL",        EISA_VENDOR_DEC,        "DEC" },
    { "Digital Equipment", EISA_VENDOR_DEC,     "DEC" },
    { "DECpc",          EISA_VENDOR_DEC,        "DEC" },

    /* AMI BIOS */
    { "American Megatrends", EISA_VENDOR_AMI,   "AMI" },
    { "AMIBIOS",        EISA_VENDOR_AMI,        "AMI" },
    { "AMI BIOS",       EISA_VENDOR_AMI,        "AMI" },

    /* Award BIOS */
    { "Award Software", EISA_VENDOR_AWARD,      "Award" },
    { "Award Modular",  EISA_VENDOR_AWARD,      "Award" },
    { "AWARD",          EISA_VENDOR_AWARD,      "Award" },

    /* Phoenix BIOS */
    { "Phoenix Technologies", EISA_VENDOR_PHOENIX, "Phoenix" },
    { "Phoenix BIOS",   EISA_VENDOR_PHOENIX,    "Phoenix" },
    { "PhoenixBIOS",    EISA_VENDOR_PHOENIX,    "Phoenix" },

    /* Olivetti */
    { "OLIVETTI",       EISA_VENDOR_OLIVETTI,   "Olivetti" },
    { "Olivetti",       EISA_VENDOR_OLIVETTI,   "Olivetti" },

    /* NCR */
    { "NCR Corporation", EISA_VENDOR_NCR,       "NCR" },
    { "NCR ",           EISA_VENDOR_NCR,        "NCR" },

    /* Dell */
    { "Dell Computer",  EISA_VENDOR_DELL,       "Dell" },
    { "DELL",           EISA_VENDOR_DELL,       "Dell" },

    /* Micronics */
    { "Micronics",      EISA_VENDOR_MICRONICS,  "Micronics" },

    /* Intel */
    { "Intel Corporation", EISA_VENDOR_INTEL,   "Intel" },
    { "Intel Corp",     EISA_VENDOR_INTEL,      "Intel" },

    /* Terminator */
    { NULL, EISA_VENDOR_UNKNOWN, NULL }
};

/* Search for a string in ROM BIOS area */
static int search_bios_string(const char *needle)
{
    char far *bios_start = (char far *)MK_FP(0xF000, 0x0000);
    int needle_len = 0;
    int i, j;
    int match;
    const char *p;

    /* Get needle length */
    for (p = needle; *p; p++) needle_len++;

    /* Search F000:0000 - F000:FFFF */
    for (i = 0; i < 0xFFFF - needle_len; i++) {
        match = 1;
        for (j = 0; j < needle_len && match; j++) {
            if (bios_start[i + j] != needle[j]) {
                match = 0;
            }
        }
        if (match) return 1;
    }

    return 0;
}

/* Check if INT 15h AH=D8h EISA CMOS services are available */
static int check_eisa_int15_services(void)
{
    union REGS regs;
    struct SREGS sregs;

    /* INT 15h AH=D8h AL=02h: Get EISA CMOS Info */
    regs.h.ah = 0xD8;
    regs.h.al = 0x02;
    regs.x.bx = 0;
    regs.x.cx = 0;
    segread(&sregs);

    int86x(0x15, &regs, &regs, &sregs);

    /* If carry clear and AH=00h, services are available */
    if (!regs.x.cflag && regs.h.ah == 0) {
        return 1;
    }

    return 0;
}

/* Detect EISA BIOS vendor */
static void detect_eisa_bios_vendor(void)
{
    int i;

    g_eisa_bios_vendor = EISA_VENDOR_UNKNOWN;
    strcpy(g_eisa_bios_vendor_name, "Unknown");

    /* First, check for INT 15h services - this indicates a standards-compliant BIOS */
    if (check_eisa_int15_services()) {
        /* INT 15h available - still detect vendor for logging, but we can use INT 15h */
    }

    /* Scan BIOS ROM for vendor signatures */
    for (i = 0; g_bios_signatures[i].signature != NULL; i++) {
        if (search_bios_string(g_bios_signatures[i].signature)) {
            g_eisa_bios_vendor = g_bios_signatures[i].vendor_type;
            strncpy(g_eisa_bios_vendor_name, g_bios_signatures[i].vendor_name,
                    sizeof(g_eisa_bios_vendor_name) - 1);
            return;
        }
    }

    /* If no vendor detected but EISA bus present, assume Compaq-compatible */
    if (g_buscfg_state.bus_detected == BUS_EISA) {
        g_eisa_bios_vendor = EISA_VENDOR_COMPAQ;
        strcpy(g_eisa_bios_vendor_name, "Generic (Compaq-compat)");
    }
}

/* Get detected EISA BIOS vendor name (for UI display) */
const char *buscfg_get_eisa_vendor_name(void)
{
    return g_eisa_bios_vendor_name;
}

/* Get detected EISA BIOS vendor type */
unsigned char buscfg_get_eisa_vendor_type(void)
{
    return g_eisa_bios_vendor;
}

/*============================================================================
 * EISA SYSTEM BOARD DETECTION
 *
 * Unlike MCA which uses INT 15h, EISA systems identify themselves via the
 * Slot 0 system board ID at I/O ports 0x0C80-0x0C83. This is mandated by
 * the EISA specification and is bulletproof for identification.
 *
 * The EISA board ID format is:
 *   - Bytes 0-1: Compressed vendor ID (3 chars -> 16 bits)
 *   - Bytes 2-3: Product ID (4 hex digits)
 *
 * Example: CPQ0401 = Compaq Deskpro 486/33L System Board
 *
 * By reading slot 0, we can identify the exact system board model and
 * determine the number of EISA slots, CPU type, and NVM characteristics.
 *
 * Reference: EISA Specification v3.12, Linux eisa.ids database
 *============================================================================*/

/* EISA System Board database entry */
typedef struct {
    unsigned int vendor_id;         /* Compressed 3-char vendor (e.g., "CPQ") */
    unsigned int product_id;        /* 4-hex-digit product ID */
    unsigned char slots;            /* Number of EISA slots */
    unsigned char cpu_class;        /* 0=386, 1=486, 2=Pentium, 3=PentiumPro */
    const char *name;               /* System board name */
} eisa_sysboard_entry_t;

/* Complete EISA system board database */
static const eisa_sysboard_entry_t g_eisa_sysboards[] = {
    /*========================================================================
     * COMPAQ (CPQ) - Most common EISA vendor
     * Compaq invented EISA and dominated the market
     *========================================================================*/
    /* SystemPro Series - Original EISA servers */
    { 0x110E, 0x0101, 8, 0, "Compaq SystemPro" },
    { 0x110E, 0x0109, 8, 0, "Compaq SystemPro (Rev B)" },
    { 0x110E, 0x0111, 8, 1, "Compaq SystemPro/LT" },
    { 0x110E, 0x0119, 8, 1, "Compaq SystemPro/XL" },

    /* Deskpro Series - Desktop EISA workstations */
    { 0x110E, 0x0401, 5, 0, "Compaq Deskpro 386/33L" },
    { 0x110E, 0x0409, 5, 1, "Compaq Deskpro 486/33L" },
    { 0x110E, 0x0501, 4, 1, "Compaq Deskpro/M" },
    { 0x110E, 0x0509, 4, 1, "Compaq Deskpro/M (Audio)" },
    { 0x110E, 0x0521, 5, 1, "Compaq Deskpro XL" },
    { 0x110E, 0x0529, 5, 2, "Compaq Deskpro XL (Pentium)" },

    /* ProSignia Series - Entry servers */
    { 0x110E, 0x0531, 6, 1, "Compaq ProSignia 500" },
    { 0x110E, 0x0541, 4, 1, "Compaq ProSignia 300" },
    { 0x110E, 0x0571, 4, 2, "Compaq ProSignia 200" },

    /* ProLiant Series - Enterprise servers */
    { 0x110E, 0x0551, 8, 2, "Compaq ProLiant 2500" },
    { 0x110E, 0x0552, 8, 2, "Compaq ProLiant 2500 (Rev B)" },
    { 0x110E, 0x0553, 6, 2, "Compaq ProLiant 1600" },
    { 0x110E, 0x0559, 6, 2, "Compaq ProLiant 1500" },
    { 0x110E, 0x0561, 10, 2, "Compaq ProLiant 3000" },
    { 0x110E, 0x0569, 10, 3, "Compaq ProLiant 5000" },
    { 0x110E, 0x0579, 6, 2, "Compaq ProLiant 800" },
    { 0x110E, 0x0589, 6, 2, "Compaq ProLiant 850R" },
    { 0x110E, 0x0631, 6, 2, "Compaq ProLiant 1000" },
    { 0x110E, 0x0639, 8, 2, "Compaq ProLiant 4000" },
    { 0x110E, 0x0641, 10, 2, "Compaq ProLiant 4500" },
    { 0x110E, 0x0651, 10, 3, "Compaq ProLiant 6000" },
    { 0x110E, 0x0661, 10, 3, "Compaq ProLiant 6500" },
    { 0x110E, 0x0671, 10, 3, "Compaq ProLiant 7000" },

    /*========================================================================
     * HEWLETT-PACKARD (HWP) - Major EISA vendor
     *========================================================================*/
    /* Vectra Series - Desktop workstations */
    { 0x3E07, 0x0101, 4, 0, "HP Vectra RS/16" },
    { 0x3E07, 0x0109, 4, 0, "HP Vectra RS/20" },
    { 0x3E07, 0x0111, 4, 0, "HP Vectra RS/25" },
    { 0x3E07, 0x0201, 4, 1, "HP Vectra 486s" },
    { 0x3E07, 0x0209, 5, 1, "HP Vectra 486/25T" },
    { 0x3E07, 0x0211, 5, 1, "HP Vectra 486/33T" },
    { 0x3E07, 0x0301, 5, 2, "HP Vectra XU" },
    { 0x3E07, 0x0309, 5, 2, "HP Vectra VL" },

    /* NetServer Series - Servers */
    { 0x3E07, 0x0401, 6, 1, "HP NetServer LE" },
    { 0x3E07, 0x0409, 6, 1, "HP NetServer LM" },
    { 0x3E07, 0x0411, 8, 2, "HP NetServer LF" },
    { 0x3E07, 0x0419, 8, 2, "HP NetServer LH" },
    { 0x3E07, 0x0421, 6, 2, "HP NetServer LC" },
    { 0x3E07, 0x0429, 8, 2, "HP NetServer LS" },
    { 0x3E07, 0x0431, 8, 3, "HP NetServer LH Pro" },
    { 0x3E07, 0x0439, 10, 3, "HP NetServer LX Pro" },

    /*========================================================================
     * DIGITAL EQUIPMENT (DEC) - EISA workstations and Alpha systems
     *========================================================================*/
    { 0x1028, 0x0101, 6, 0, "DECpc 433" },
    { 0x1028, 0x0201, 6, 1, "DECpc 450" },
    { 0x1028, 0x0301, 6, 1, "DECstation 450" },
    { 0x1028, 0x0401, 6, 2, "DECpc XL 560" },
    { 0x1028, 0x0501, 6, 0, "DEC Alpha Jensen" },  /* DECpc AXP 150 */
    { 0x1028, 0x0509, 6, 0, "DEC 2000 AXP Model 300" },

    /*========================================================================
     * DELL - PowerEdge EISA servers
     *========================================================================*/
    { 0x1028, 0x0601, 6, 1, "Dell PowerEdge 2100" },
    { 0x1028, 0x0609, 6, 1, "Dell PowerEdge 2200" },
    { 0x1028, 0x0611, 8, 2, "Dell PowerEdge 4100" },
    { 0x1028, 0x0619, 8, 2, "Dell PowerEdge 4200" },

    /*========================================================================
     * NCR - System 3000 EISA servers
     *========================================================================*/
    { 0x254A, 0x0101, 6, 0, "NCR System 3230" },
    { 0x254A, 0x0201, 6, 1, "NCR System 3340" },
    { 0x254A, 0x0301, 8, 1, "NCR System 3410" },
    { 0x254A, 0x0401, 8, 2, "NCR System 3430" },
    { 0x254A, 0x0501, 8, 2, "NCR System 3450" },
    { 0x254A, 0x0601, 10, 2, "NCR System 3550" },

    /*========================================================================
     * INTEL - Server boards and reference designs
     *========================================================================*/
    { 0x25D4, 0x0101, 5, 1, "Intel EISA 486 Reference" },
    { 0x25D4, 0x0201, 6, 2, "Intel Alder" },
    { 0x25D4, 0x0301, 8, 2, "Intel Xpress Server" },

    /*========================================================================
     * OLIVETTI - European EISA systems
     *========================================================================*/
    { 0x364F, 0x0101, 5, 0, "Olivetti CP486" },
    { 0x364F, 0x0201, 6, 1, "Olivetti M486" },
    { 0x364F, 0x0301, 6, 2, "Olivetti NetStrada" },

    /*========================================================================
     * MICRONICS - Third-party EISA motherboards
     *========================================================================*/
    { 0x2E90, 0x0101, 5, 0, "Micronics EISA-3" },
    { 0x2E90, 0x0201, 6, 1, "Micronics 486/EISA" },
    { 0x2E90, 0x0301, 6, 1, "Micronics EISA-4" },
    { 0x2E90, 0x0401, 6, 2, "Micronics Gemini" },

    /*========================================================================
     * AST - Premium EISA workstations
     *========================================================================*/
    { 0x0674, 0x0101, 5, 0, "AST Premium 386/33" },
    { 0x0674, 0x0201, 5, 1, "AST Premium 486/33" },
    { 0x0674, 0x0301, 6, 2, "AST Manhattan P" },

    /*========================================================================
     * ALR (Advanced Logic Research) - EISA servers
     *========================================================================*/
    { 0x0309, 0x0101, 6, 1, "ALR Business VEISA" },
    { 0x0309, 0x0201, 8, 1, "ALR PowerPro" },
    { 0x0309, 0x0301, 8, 2, "ALR Revolution" },

    /*========================================================================
     * ACER - AcerPower EISA systems
     *========================================================================*/
    { 0x0025, 0x0101, 4, 1, "Acer AcerPower 4e" },
    { 0x0025, 0x0201, 5, 2, "Acer AcerAltos 7000" },

    /*========================================================================
     * APRICOT - UK manufacturer
     *========================================================================*/
    { 0x0422, 0x0101, 5, 1, "Apricot LS Pro" },
    { 0x0422, 0x0201, 6, 2, "Apricot XEN-S" },

    /*========================================================================
     * NEC - PowerMate EISA systems
     *========================================================================*/
    { 0x254C, 0x0101, 5, 1, "NEC PowerMate 486" },
    { 0x254C, 0x0201, 6, 2, "NEC Express Server" },

    /*========================================================================
     * Terminator
     *========================================================================*/
    { 0, 0, 0, 0, NULL }
};

/* EISA system detection state */
static unsigned int g_eisa_sysboard_vendor = 0;
static unsigned int g_eisa_sysboard_product = 0;
static unsigned char g_eisa_slot_count = 8;  /* Default to 8 if unknown */
static char g_eisa_sysboard_name[48] = "Unknown EISA System";

/* Read EISA slot 0 system board ID */
static void detect_eisa_sysboard(void)
{
    unsigned char b0, b1, b2, b3;
    unsigned int vendor, product;
    char vendor_str[4];
    int i;

    g_eisa_sysboard_vendor = 0;
    g_eisa_sysboard_product = 0;
    g_eisa_slot_count = 8;
    strcpy(g_eisa_sysboard_name, "Unknown EISA System");

    /*
     * Read EISA slot 0 ID at 0x0C80-0x0C83
     * This is the system board (motherboard) ID
     *
     * Format:
     *   0xC80: Vendor ID byte 0 (compressed)
     *   0xC81: Vendor ID byte 1 (compressed)
     *   0xC82: Product ID high byte
     *   0xC83: Product ID low byte
     */
    b0 = inp(0x0C80);
    b1 = inp(0x0C81);
    b2 = inp(0x0C82);
    b3 = inp(0x0C83);

    /* Check for empty slot (all 0xFF) */
    if (b0 == 0xFF && b1 == 0xFF && b2 == 0xFF && b3 == 0xFF) {
        strcpy(g_eisa_sysboard_name, "EISA (No System Board ID)");
        return;
    }

    /* Decode vendor and product IDs */
    vendor = b0 | ((unsigned int)b1 << 8);
    product = b2 | ((unsigned int)b3 << 8);

    g_eisa_sysboard_vendor = vendor;
    g_eisa_sysboard_product = product;

    /* Decode vendor string for display */
    buscfg_decode_eisa_vendor(vendor, vendor_str);

    /* Look up in database */
    for (i = 0; g_eisa_sysboards[i].name != NULL; i++) {
        if (g_eisa_sysboards[i].vendor_id == vendor &&
            g_eisa_sysboards[i].product_id == product) {
            /* Found exact match */
            strncpy(g_eisa_sysboard_name, g_eisa_sysboards[i].name,
                    sizeof(g_eisa_sysboard_name) - 1);
            g_eisa_slot_count = g_eisa_sysboards[i].slots;
            return;
        }
    }

    /* No exact match - try vendor match only for slot count estimate */
    for (i = 0; g_eisa_sysboards[i].name != NULL; i++) {
        if (g_eisa_sysboards[i].vendor_id == vendor) {
            /* Use this vendor's typical slot count */
            g_eisa_slot_count = g_eisa_sysboards[i].slots;
            break;
        }
    }

    /* Format generic name with vendor/product ID */
    sprintf(g_eisa_sysboard_name, "EISA %s%04X", vendor_str, product);
}

/* Get detected EISA system board name (for UI display) */
const char *buscfg_get_eisa_sysboard_name(void)
{
    return g_eisa_sysboard_name;
}

/* Get EISA system board vendor ID (compressed) */
unsigned int buscfg_get_eisa_sysboard_vendor(void)
{
    return g_eisa_sysboard_vendor;
}

/* Get EISA system board product ID */
unsigned int buscfg_get_eisa_sysboard_product(void)
{
    return g_eisa_sysboard_product;
}

/* Get number of EISA slots for this system (from system board database) */
unsigned char buscfg_get_eisa_slot_count(void)
{
    return g_eisa_slot_count;
}

/*============================================================================
 * MCA/PS/2 MODEL DETECTION
 *
 * IBM PS/2 systems (and rare clones) can be precisely identified using the
 * INT 15h AH=C0h System Configuration call. This returns a pointer to a
 * configuration table containing Model and Submodel bytes.
 *
 * This is more reliable than generic probing because:
 *   - IBM standardized this across ALL PS/2 models
 *   - Model bytes are unique and well-documented
 *   - We can determine exact slot count, CPU type, and NVRAM characteristics
 *
 * Reference: IBM PS/2 Hardware Interface Technical Reference
 *============================================================================*/

/* PS/2 Model database entry */
typedef struct {
    unsigned char model;            /* Model byte from INT 15h */
    unsigned char submodel;         /* Submodel byte */
    unsigned char slots;            /* Number of MCA slots */
    unsigned char cpu_class;        /* 0=286, 1=386SX, 2=386DX, 3=486, 4=Pentium */
    unsigned char bus_width;        /* 16 or 32 (bit width of MCA bus) */
    unsigned char bus_speed;        /* Bus speed in MHz (10, 16, 20, 25, 33) */
    const char *name;               /* Model name string */
} ps2_model_entry_t;

/* Complete PS/2 model database
 *
 * Bus Width: 16-bit for 286/386SX systems, 32-bit for 386DX/486/Pentium
 * Bus Speed: Varies by model (10-33 MHz)
 *
 * Reference: IBM PS/2 Hardware Interface Technical Reference
 *            PS/2 Blue Books, MCA Mafia documentation
 */
static const ps2_model_entry_t g_ps2_models[] = {
    /*========================================================================
     * PS/2 Model 25/30 - ISA bus, NOT MCA (included for completeness)
     * These use ISA, not MCA - bus_width=0 indicates ISA
     *========================================================================*/
    { 0xFA, 0x00, 0, 0, 0, 0, "PS/2 Model 25" },
    { 0xFA, 0x01, 0, 0, 0, 0, "PS/2 Model 25-286" },
    { 0xFC, 0x00, 0, 0, 0, 0, "PS/2 Model 30 (8086)" },
    { 0xFC, 0x01, 0, 0, 0, 0, "PS/2 Model 30" },
    { 0xFC, 0x02, 0, 0, 0, 0, "PS/2 Model 30-286" },

    /*========================================================================
     * PS/2 Model 50/50Z - Desktop, 286/386SX, 4 MCA slots
     * 16-bit MCA at 10 MHz (286) or 10 MHz (386SX)
     *========================================================================*/
    { 0xFC, 0x04, 4, 0, 16, 10, "PS/2 Model 50" },
    { 0xFC, 0x05, 4, 0, 16, 10, "PS/2 Model 50 (late)" },
    { 0xFC, 0x09, 4, 1, 16, 10, "PS/2 Model 50Z" },

    /*========================================================================
     * PS/2 Model 55SX/55LS - Desktop, 386SX, 3 MCA slots
     * 16-bit MCA at 10 MHz
     *========================================================================*/
    { 0xFC, 0x0B, 3, 1, 16, 10, "PS/2 Model 55SX" },
    { 0xFC, 0x0C, 3, 1, 16, 10, "PS/2 Model 55LS" },
    { 0xFC, 0x23, 3, 1, 16, 10, "PS/2 Model 55SX (late)" },

    /*========================================================================
     * PS/2 Model 60 - Floor-standing, 286, 8 MCA slots
     * 16-bit MCA at 10 MHz
     *========================================================================*/
    { 0xFC, 0x05, 8, 0, 16, 10, "PS/2 Model 60" },

    /*========================================================================
     * PS/2 Model 65SX - Desktop, 386SX, 8 MCA slots
     * 16-bit MCA at 10 MHz
     *========================================================================*/
    { 0xFC, 0x1C, 8, 1, 16, 10, "PS/2 Model 65SX" },

    /*========================================================================
     * PS/2 Model 70 - Desktop, 386DX, 3 MCA slots
     * 32-bit MCA at 16/20/25 MHz depending on variant
     *========================================================================*/
    { 0xF8, 0x04, 3, 2, 32, 16, "PS/2 Model 70 (386-16)" },
    { 0xF8, 0x09, 3, 2, 32, 20, "PS/2 Model 70 (386-20)" },
    { 0xF8, 0x0D, 3, 2, 32, 25, "PS/2 Model 70 (386-25)" },
    { 0xF8, 0x1B, 3, 3, 32, 25, "PS/2 Model 70 (486)" },

    /*========================================================================
     * PS/2 Model 80 - Floor-standing, 386DX, 8 MCA slots
     * 32-bit MCA at 16/20/25 MHz depending on variant
     *========================================================================*/
    { 0xF8, 0x00, 8, 2, 32, 16, "PS/2 Model 80 (386-16)" },
    { 0xF8, 0x01, 8, 2, 32, 20, "PS/2 Model 80 (386-20)" },
    { 0xF8, 0x80, 8, 2, 32, 16, "PS/2 Model 80-041" },
    { 0xF8, 0x04, 8, 2, 32, 25, "PS/2 Model 80-A21 (386-25)" },
    { 0xF8, 0x09, 8, 2, 32, 25, "PS/2 Model 80-A31" },

    /*========================================================================
     * PS/2 Model 90/95 XP - Desktop/Tower, 486/Pentium, 8 MCA slots
     * 32-bit MCA at 25-33 MHz
     *========================================================================*/
    { 0xF8, 0x0B, 8, 3, 32, 25, "PS/2 Model 90 XP 486" },
    { 0xF8, 0x0C, 8, 3, 32, 25, "PS/2 Model 95 XP 486" },
    { 0xF8, 0x0D, 8, 4, 32, 33, "PS/2 Model 90/95 XP (Pentium)" },
    { 0xF8, 0x0E, 8, 3, 32, 25, "PS/2 Model 90 XP 486 (late)" },
    { 0xF8, 0x0F, 8, 3, 32, 25, "PS/2 Model 95 XP 486 (late)" },
    { 0xF8, 0x1B, 8, 3, 32, 33, "PS/2 Model 95A" },
    { 0xF8, 0x1C, 8, 3, 32, 25, "PS/2 Model 90 XP 486SX" },
    { 0xF8, 0x1E, 8, 3, 32, 33, "PS/2 Model 95 XP (Server)" },
    { 0xF8, 0x23, 8, 3, 32, 33, "PS/2 Model 95A XP" },
    { 0xF8, 0x26, 8, 4, 32, 33, "PS/2 Model 95A (Pentium)" },
    { 0xF8, 0x2C, 8, 3, 32, 33, "PS/2 Server 95A" },

    /*========================================================================
     * PS/2 Model 35/40 - Desktop, 386SX, 3 MCA slots
     * 16-bit MCA at 10 MHz
     *========================================================================*/
    { 0x9A, 0x00, 3, 1, 16, 10, "PS/2 Model 35SX/40SX" },
    { 0x9A, 0x01, 3, 1, 16, 10, "PS/2 Model 35SX" },
    { 0x9A, 0x02, 3, 1, 16, 10, "PS/2 Model 40SX" },

    /*========================================================================
     * PS/2 Model 57 - Desktop, 486SLC, 3 MCA slots
     * 32-bit MCA at 20 MHz (486SLC is 386SX-bus but with 32-bit internal)
     * Note: Model 57 uses 16-bit external bus despite 486SLC CPU
     *========================================================================*/
    { 0xF8, 0x11, 3, 3, 16, 20, "PS/2 Model 57SX" },
    { 0xF8, 0x13, 3, 3, 16, 20, "PS/2 Model 57SLC" },

    /*========================================================================
     * PS/2 Model 76/77 - Desktop, 486, 4 MCA slots
     * 32-bit MCA at 25 MHz
     *========================================================================*/
    { 0xF8, 0x16, 4, 3, 32, 25, "PS/2 Model 76i" },
    { 0xF8, 0x17, 4, 3, 32, 25, "PS/2 Model 77i" },
    { 0xF8, 0x19, 4, 3, 32, 25, "PS/2 Model 76s" },
    { 0xF8, 0x1A, 4, 3, 32, 25, "PS/2 Model 77s" },

    /*========================================================================
     * PS/1 with MCA (some models)
     * 16-bit MCA at 10 MHz
     *========================================================================*/
    { 0xFC, 0x0B, 1, 1, 16, 10, "PS/1 Model 2011" },
    { 0xFC, 0x0F, 1, 1, 16, 10, "PS/1 Model 2121" },

    /*========================================================================
     * PS/2 Servers and Specialty Models
     * 32-bit MCA at 25-33 MHz
     *========================================================================*/
    { 0xF8, 0x14, 8, 3, 32, 25, "PS/2 Server 85" },
    { 0xF8, 0x1D, 8, 3, 32, 33, "PS/2 Server 295" },
    { 0xF8, 0x20, 8, 3, 32, 33, "PS/2 Server 500" },
    { 0xF8, 0x25, 8, 3, 32, 33, "PS/2 Server 720" },

    /*========================================================================
     * Third-party MCA systems (rare)
     * Bus specs vary by manufacturer
     *========================================================================*/
    { 0xF8, 0x50, 8, 2, 32, 16, "NCR 3300 Series" },
    { 0xF8, 0x51, 6, 2, 32, 16, "NCR 3300 (6-slot)" },
    { 0xFC, 0x50, 4, 1, 16, 10, "Reply Model 32" },
    { 0xFC, 0x81, 6, 2, 32, 16, "Tandy 5000 MC" },
    { 0xF8, 0x81, 8, 2, 32, 16, "Apricot Qi" },

    /*========================================================================
     * Terminator
     *========================================================================*/
    { 0, 0, 0, 0, 0, 0, NULL }
};

/* MCA system detection state */
static unsigned char g_mca_model_byte = 0;
static unsigned char g_mca_submodel_byte = 0;
static unsigned char g_mca_bios_revision = 0;
static unsigned char g_mca_slot_count = 8;  /* Default to 8 if unknown */
static unsigned char g_mca_bus_width = 32;  /* Default to 32-bit if unknown */
static unsigned char g_mca_bus_speed = 16;  /* Default to 16 MHz if unknown */
static char g_mca_model_name[48] = "Unknown MCA System";

/* Detect PS/2 model via INT 15h AH=C0h */
static void detect_mca_model(void)
{
    union REGS regs;
    struct SREGS sregs;
    unsigned char far *config;
    int i;

    g_mca_model_byte = 0;
    g_mca_submodel_byte = 0;
    g_mca_bios_revision = 0;
    g_mca_slot_count = 8;
    g_mca_bus_width = 32;
    g_mca_bus_speed = 16;
    strcpy(g_mca_model_name, "Unknown MCA System");

    /* INT 15h AH=C0h: Get System Configuration */
    regs.h.ah = 0xC0;
    segread(&sregs);
    int86x(0x15, &regs, &regs, &sregs);

    if (regs.x.cflag) {
        /* Function not supported - very unlikely on MCA system */
        return;
    }

    /* ES:BX points to System Configuration Table */
    config = (unsigned char far *)MK_FP(sregs.es, regs.x.bx);

    /*
     * System Configuration Table layout:
     * Offset 0-1: Table length (bytes)
     * Offset 2:   Model byte
     * Offset 3:   Submodel byte
     * Offset 4:   BIOS revision level
     * Offset 5:   Feature byte 1 (bit 1 = MCA present)
     * Offset 6:   Feature byte 2
     * Offset 7:   Feature byte 3
     * Offset 8:   Feature byte 4
     * Offset 9:   Feature byte 5
     */

    g_mca_model_byte = config[2];
    g_mca_submodel_byte = config[3];
    g_mca_bios_revision = config[4];

    /* Look up model in database */
    for (i = 0; g_ps2_models[i].name != NULL; i++) {
        if (g_ps2_models[i].model == g_mca_model_byte &&
            g_ps2_models[i].submodel == g_mca_submodel_byte) {
            /* Found exact match */
            strncpy(g_mca_model_name, g_ps2_models[i].name,
                    sizeof(g_mca_model_name) - 1);
            g_mca_slot_count = g_ps2_models[i].slots;
            g_mca_bus_width = g_ps2_models[i].bus_width;
            g_mca_bus_speed = g_ps2_models[i].bus_speed;
            return;
        }
    }

    /* No exact match - try model byte only */
    for (i = 0; g_ps2_models[i].name != NULL; i++) {
        if (g_ps2_models[i].model == g_mca_model_byte) {
            /* Partial match - use as base */
            sprintf(g_mca_model_name, "PS/2 (Model %02Xh/%02Xh)",
                    g_mca_model_byte, g_mca_submodel_byte);
            g_mca_slot_count = g_ps2_models[i].slots;
            g_mca_bus_width = g_ps2_models[i].bus_width;
            g_mca_bus_speed = g_ps2_models[i].bus_speed;
            return;
        }
    }

    /* Completely unknown - format generic string */
    sprintf(g_mca_model_name, "MCA System (%02Xh/%02Xh)",
            g_mca_model_byte, g_mca_submodel_byte);
}

/* Get detected MCA system model name (for UI display) */
const char *buscfg_get_mca_model_name(void)
{
    return g_mca_model_name;
}

/* Get MCA model byte */
unsigned char buscfg_get_mca_model_byte(void)
{
    return g_mca_model_byte;
}

/* Get MCA submodel byte */
unsigned char buscfg_get_mca_submodel_byte(void)
{
    return g_mca_submodel_byte;
}

/* Get number of MCA slots for this system */
unsigned char buscfg_get_mca_slot_count(void)
{
    return g_mca_slot_count;
}

/* Get MCA bus width (16 or 32 bits)
 * 16-bit MCA: Model 50, 50Z, 55SX, 55LS, 60, 65SX, 35/40 (286 or 386SX CPU)
 * 32-bit MCA: Model 70, 80, 90, 95 (386DX, 486, or Pentium CPU)
 */
unsigned char buscfg_get_mca_bus_width(void)
{
    return g_mca_bus_width;
}

/* Get MCA bus speed in MHz (10, 16, 20, 25, or 33)
 * Speed varies by model:
 *   10 MHz - Most 286/386SX models (Model 50, 55, 60, 65, 35/40)
 *   16 MHz - Model 70-E, Model 80-041, NCR/Tandy
 *   20 MHz - Model 70-0xx, Model 80-386-20, Model 57
 *   25 MHz - Model 70-386-25, Model 80-A21/A31, Model 76/77, Model 90 486
 *   33 MHz - Model 90/95 XP Pentium, Server 295/500/720
 */
unsigned char buscfg_get_mca_bus_speed(void)
{
    return g_mca_bus_speed;
}

/* Check if system is a PS/2 (vs third-party MCA) */
int buscfg_is_ibm_ps2(void)
{
    /* IBM PS/2 systems have model bytes F8h, FAh, FCh, or 9Ah */
    switch (g_mca_model_byte) {
        case 0xF8:  /* Model 70, 80, 90, 95, etc. */
        case 0xFA:  /* Model 25 */
        case 0xFC:  /* Model 30, 50, 55, 60, 65 */
        case 0x9A:  /* Model 35, 40 */
            return 1;
        default:
            return 0;
    }
}

/*============================================================================
 * EISA VENDOR ID ENCODING/DECODING
 *============================================================================*/

void buscfg_decode_eisa_vendor(unsigned int compressed, char *vendor_str)
{
    unsigned char b0 = compressed & 0xFF;
    unsigned char b1 = (compressed >> 8) & 0xFF;

    vendor_str[0] = ((b0 >> 2) & 0x1F) + '@';
    vendor_str[1] = (((b0 & 0x03) << 3) | ((b1 >> 5) & 0x07)) + '@';
    vendor_str[2] = (b1 & 0x1F) + '@';
    vendor_str[3] = '\0';
}

unsigned int buscfg_encode_eisa_vendor(const char *vendor_str)
{
    unsigned char c0 = (vendor_str[0] - '@') & 0x1F;
    unsigned char c1 = (vendor_str[1] - '@') & 0x1F;
    unsigned char c2 = (vendor_str[2] - '@') & 0x1F;

    unsigned char b0 = (c0 << 2) | (c1 >> 3);
    unsigned char b1 = ((c1 & 0x07) << 5) | c2;

    return b0 | ((unsigned int)b1 << 8);
}

/*============================================================================
 * CARD DATABASE LOOKUP
 *============================================================================*/

const char *buscfg_lookup_card(unsigned char bus_type,
                                unsigned int vendor_id,
                                unsigned int device_id)
{
    const card_db_entry_t *db;
    int i;

    if (bus_type == BUS_EISA) {
        db = g_eisa_cards;
    } else if (bus_type == BUS_MCA) {
        db = g_mca_cards;
    } else {
        return NULL;
    }

    for (i = 0; db[i].name != NULL; i++) {
        if (bus_type == BUS_EISA) {
            if (db[i].vendor_id == vendor_id && db[i].device_id == device_id) {
                return db[i].name;
            }
        } else {
            /* MCA: Only match adapter_id (stored in vendor_id) */
            if (db[i].vendor_id == vendor_id) {
                return db[i].name;
            }
        }
    }

    return NULL;
}

/*============================================================================
 * SLOT ID FORMATTING
 *============================================================================*/

void buscfg_format_slot_id(const slot_config_t *cfg, char *id_str)
{
    if (cfg->bus_type == BUS_EISA) {
        /* EISA: "CPQ0001" format (vendor + product) */
        sprintf(id_str, "%s%04X", cfg->vendor_str, cfg->device_id);
    } else if (cfg->bus_type == BUS_MCA) {
        /* MCA: "@XXXX" format (adapter ID) */
        sprintf(id_str, "@%04X", cfg->vendor_id);
    } else {
        strcpy(id_str, "????");
    }
}

/*============================================================================
 * EISA SLOT OPERATIONS
 *============================================================================*/

/*
 * EISA Function Configuration Space Layout (per function, 26 bytes):
 * Offset 0x00-0x03: Type/Subtype/Revision
 * Offset 0x04-0x05: Selection byte (choice index for CFG file)
 * Offset 0x06-0x07: Function info (minor revision)
 * Offset 0x08-0x0B: Memory config 0 (24-bit base, 8-bit size/flags)
 * Offset 0x0C-0x0F: Memory config 1
 * Offset 0x10-0x11: IRQ config (2 IRQ assignments)
 * Offset 0x12-0x14: DMA config (2 DMA channels + timing)
 * Offset 0x15-0x19: I/O port ranges (3 port bases)
 *
 * EISA Extended Configuration Space:
 * Port base = slot_base + 0xC80
 * Function data at slot_base + 0xC88 to slot_base + 0xCFF
 */

/* Read EISA function block to extract resources */
static void eisa_read_function_resources(unsigned int slot_base, slot_config_t *cfg)
{
    unsigned int func_base;
    unsigned char func_info;
    unsigned char irq_byte0, irq_byte1;
    unsigned char dma_byte0, dma_byte1;
    unsigned int io_base;
    unsigned long mem_base;
    unsigned char mem_info;
    int func;

    /* EISA cards can have up to 4 functions at offsets 0xC88, 0xCA0, 0xCB8, 0xCD0 */
    for (func = 0; func < 4; func++) {
        func_base = slot_base + 0xC88 + (func * 0x18);

        _disable();

        /* Read function type - 0xFF means no function */
        func_info = inp(func_base);
        if (func_info == 0xFF || func_info == 0x00) {
            _enable();
            continue;  /* Empty function slot */
        }

        /* Read IRQ configuration (offset 0x10-0x11 from func_base) */
        irq_byte0 = inp(func_base + 0x08);  /* IRQ config byte 0 */
        irq_byte1 = inp(func_base + 0x09);  /* IRQ config byte 1 */

        /* IRQ byte format: bits 0-3 = IRQ number, bit 5 = level triggered, bit 6 = shareable */
        if ((irq_byte0 & 0x0F) != 0x0F && cfg->irq_count < MAX_SLOT_IRQS) {
            cfg->irqs[cfg->irq_count].irq = irq_byte0 & 0x0F;
            cfg->irqs[cfg->irq_count].level_triggered = (irq_byte0 & 0x20) ? 1 : 0;
            cfg->irqs[cfg->irq_count].shared = (irq_byte0 & 0x40) ? 1 : 0;
            cfg->irq_count++;
        }
        if ((irq_byte1 & 0x0F) != 0x0F && cfg->irq_count < MAX_SLOT_IRQS) {
            cfg->irqs[cfg->irq_count].irq = irq_byte1 & 0x0F;
            cfg->irqs[cfg->irq_count].level_triggered = (irq_byte1 & 0x20) ? 1 : 0;
            cfg->irqs[cfg->irq_count].shared = (irq_byte1 & 0x40) ? 1 : 0;
            cfg->irq_count++;
        }

        /* Read DMA configuration (offset 0x12-0x13 from func_base) */
        dma_byte0 = inp(func_base + 0x0A);
        dma_byte1 = inp(func_base + 0x0B);

        /* DMA byte format: bits 0-2 = channel, bits 3-4 = transfer size, bits 5-6 = timing */
        if ((dma_byte0 & 0x07) != 0x07 && cfg->dma_count < MAX_SLOT_DMAS) {
            cfg->dmas[cfg->dma_count].channel = dma_byte0 & 0x07;
            cfg->dmas[cfg->dma_count].transfer_size = (dma_byte0 >> 3) & 0x03;
            cfg->dmas[cfg->dma_count].timing = (dma_byte0 >> 5) & 0x03;
            cfg->dma_count++;
        }
        if ((dma_byte1 & 0x07) != 0x07 && cfg->dma_count < MAX_SLOT_DMAS) {
            cfg->dmas[cfg->dma_count].channel = dma_byte1 & 0x07;
            cfg->dmas[cfg->dma_count].transfer_size = (dma_byte1 >> 3) & 0x03;
            cfg->dmas[cfg->dma_count].timing = (dma_byte1 >> 5) & 0x03;
            cfg->dma_count++;
        }

        /* Read I/O port configuration (offset 0x0C-0x0F) */
        /* Each I/O entry: 2 bytes base address, 1 byte count */
        io_base = inp(func_base + 0x0C) | ((unsigned int)inp(func_base + 0x0D) << 8);
        if (io_base != 0x0000 && io_base != 0xFFFF && cfg->ioport_count < MAX_SLOT_IOPORTS) {
            cfg->ioports[cfg->ioport_count].base = io_base;
            cfg->ioports[cfg->ioport_count].size = 16;  /* Default size */
            cfg->ioport_count++;
        }

        io_base = inp(func_base + 0x0E) | ((unsigned int)inp(func_base + 0x0F) << 8);
        if (io_base != 0x0000 && io_base != 0xFFFF && cfg->ioport_count < MAX_SLOT_IOPORTS) {
            cfg->ioports[cfg->ioport_count].base = io_base;
            cfg->ioports[cfg->ioport_count].size = 16;
            cfg->ioport_count++;
        }

        /* Read Memory configuration (offset 0x00-0x07) */
        /* Memory entry: 3 bytes base (bits 8-31), 1 byte size/flags */
        mem_base = ((unsigned long)inp(func_base + 0x01) << 8) |
                   ((unsigned long)inp(func_base + 0x02) << 16) |
                   ((unsigned long)inp(func_base + 0x03) << 24);
        mem_info = inp(func_base + 0x04);

        if (mem_base != 0 && mem_base != 0xFFFFFF00UL && cfg->mem_count < MAX_SLOT_MEMRANGES) {
            cfg->mem_ranges[cfg->mem_count].base = mem_base;
            /* Size encoding: bits 0-3 = size (0=1KB, 1=2KB, ... 15=32MB) */
            cfg->mem_ranges[cfg->mem_count].size = 1024UL << (mem_info & 0x0F);
            cfg->mem_ranges[cfg->mem_count].is_rom = (mem_info & 0x40) ? 1 : 0;
            cfg->mem_ranges[cfg->mem_count].is_shared = (mem_info & 0x80) ? 1 : 0;
            cfg->mem_count++;
        }

        _enable();
    }
}

/* Read EISA slot configuration */
static int eisa_read_slot(unsigned char slot, slot_config_t *cfg)
{
    unsigned int port_base;
    unsigned int vendor_compressed, product_id;
    unsigned char ctrl;
    const char *name;
    const card_db_entry_t *db_entry;
    int i;

    if (slot > 15) return BUSCFG_ERR_SLOT;

    port_base = EISA_ID_BASE + ((unsigned int)slot << 12);

    /* Read board ID */
    _disable();
    vendor_compressed = inp(port_base) | ((unsigned int)inp(port_base + 1) << 8);
    product_id = inp(port_base + 2) | ((unsigned int)inp(port_base + 3) << 8);

    /* Read control register */
    ctrl = inp(port_base + 4);
    _enable();

    /* Check for empty slot */
    if ((vendor_compressed == 0xFFFF && product_id == 0xFFFF) ||
        (vendor_compressed == 0x0000 && product_id == 0x0000)) {
        return BUSCFG_ERR_SLOT;
    }

    memset(cfg, 0, sizeof(slot_config_t));

    cfg->slot = slot;
    cfg->bus_type = BUS_EISA;
    cfg->vendor_id = vendor_compressed;
    cfg->device_id = product_id;
    cfg->enabled = (ctrl & 0x01) ? 1 : 0;

    /* Decode vendor string */
    buscfg_decode_eisa_vendor(vendor_compressed, cfg->vendor_str);

    /* Look up name in database */
    name = buscfg_lookup_card(BUS_EISA, vendor_compressed, product_id);
    if (name) {
        strncpy(cfg->name, name, sizeof(cfg->name) - 1);
    } else {
        sprintf(cfg->name, "%s%04X", cfg->vendor_str, product_id);
    }

    /* Read resources from EISA function configuration space */
    eisa_read_function_resources((unsigned int)slot << 12, cfg);

    /* If no resources found from hardware, try database defaults */
    if (cfg->irq_count == 0 && cfg->dma_count == 0 &&
        cfg->ioport_count == 0 && cfg->mem_count == 0) {
        /* Search database for defaults */
        for (i = 0; g_eisa_cards[i].name != NULL; i++) {
            if (g_eisa_cards[i].vendor_id == vendor_compressed &&
                g_eisa_cards[i].device_id == product_id) {
                db_entry = &g_eisa_cards[i];

                if (db_entry->default_irq != 0) {
                    cfg->irqs[0].irq = db_entry->default_irq;
                    cfg->irqs[0].level_triggered = 1;
                    cfg->irqs[0].shared = 1;
                    cfg->irq_count = 1;
                }
                if (db_entry->default_dma != 0) {
                    cfg->dmas[0].channel = db_entry->default_dma;
                    cfg->dmas[0].transfer_size = 1;
                    cfg->dmas[0].timing = 0;
                    cfg->dma_count = 1;
                }
                if (db_entry->default_io != 0) {
                    cfg->ioports[0].base = db_entry->default_io;
                    cfg->ioports[0].size = 16;
                    cfg->ioport_count = 1;
                }
                if (db_entry->default_mem != 0) {
                    cfg->mem_ranges[0].base = db_entry->default_mem;
                    cfg->mem_ranges[0].size = 0x4000;
                    cfg->mem_ranges[0].is_rom = 0;
                    cfg->mem_ranges[0].is_shared = 0;
                    cfg->mem_count = 1;
                }
                break;
            }
        }
    }

    return BUSCFG_OK;
}

/* Enable/disable EISA slot */
static int eisa_enable_slot(unsigned char slot, unsigned char enable)
{
    unsigned int port_base;
    unsigned char ctrl;

    if (slot > 15) return BUSCFG_ERR_SLOT;

    port_base = EISA_ID_BASE + ((unsigned int)slot << 12);

    _disable();
    ctrl = inp(port_base + 4);

    if (enable) {
        ctrl |= 0x01;
    } else {
        ctrl &= ~0x01;
    }

    outp(port_base + 4, ctrl);
    _enable();

    return BUSCFG_OK;
}

/*============================================================================
 * MCA SLOT OPERATIONS
 *============================================================================*/

/*
 * MCA POS Register Layout:
 * POS 0-1: Adapter ID (read-only, 16-bit little-endian)
 * POS 2: Option Select Data Byte 1
 *        Bit 0: Card Enable (CDEN) - 1=enabled
 *        Bits 1-7: Card-specific options
 * POS 3: Option Select Data Byte 2 (card-specific)
 * POS 4: Option Select Data Byte 3
 *        Bits 0-3: Arbitration level (for bus master cards)
 *        Bits 4-7: Card-specific (often IRQ)
 * POS 5: Option Select Data Byte 4
 *        Often contains I/O address configuration
 * POS 6: Subaddress Extension (LSB)
 * POS 7: Subaddress Extension (MSB)
 *
 * Common MCA resource encodings (varies by adapter):
 * - IRQ is often in POS 4 bits 4-7 or POS 3 bits 0-3
 * - I/O base is often in POS 3 or POS 5
 * - Arbitration level in POS 4 bits 0-3
 * - Memory base often encoded across POS 3-5
 */

/* MCA adapter-specific POS register parsing */
typedef struct {
    unsigned int adapter_id;
    unsigned char irq_pos;          /* POS register for IRQ (2-7) */
    unsigned char irq_shift;        /* Bit shift within register */
    unsigned char irq_mask;         /* Mask for IRQ bits */
    unsigned char irq_table[16];    /* IRQ lookup table */
    unsigned char io_pos;           /* POS register for I/O base */
    unsigned char io_shift;
    unsigned int  io_base_table[8]; /* I/O base lookup table */
    unsigned char arb_pos;          /* POS register for arbitration level */
    unsigned char arb_shift;
    unsigned char arb_mask;
} mca_pos_decode_t;

/* Common MCA adapter POS decodings
 *
 * POS register layouts are adapter-specific. This table covers the most
 * common MCA adapters with known POS encodings from IBM ADF files and
 * third-party documentation (MCA Mafia, ps-2.kev009.com).
 *
 * Structure fields:
 *   adapter_id: 16-bit MCA adapter ID (from POS 0-1)
 *   irq_pos: POS register containing IRQ (2-7)
 *   irq_shift: Bit shift within that register
 *   irq_mask: Mask for IRQ bits after shifting
 *   irq_table: Lookup table mapping POS value to IRQ number
 *   io_pos: POS register containing I/O base
 *   io_shift: Bit shift for I/O base field
 *   io_base_table: Lookup table mapping POS value to I/O base address
 *   arb_pos: POS register containing arbitration level (for bus masters)
 *   arb_shift: Bit shift for arbitration field
 *   arb_mask: Mask for arbitration bits
 */
static const mca_pos_decode_t g_mca_pos_decode[] = {
    /*========================================================================
     * IBM TOKEN RING ADAPTERS
     *========================================================================*/

    /* IBM Token Ring 16/4 Adapter/A (0x6FC0)
     * POS 2: bit 0 = card enable
     * POS 3: bits 1-3 = primary I/O address select
     * POS 4: bits 0-3 = arbitration level, bits 5-6 = IRQ select */
    { 0x6FC0, 4, 5, 0x03, {2, 3, 10, 11, 0,0,0,0,0,0,0,0,0,0,0,0},
              3, 1, {0xA20, 0xA24, 0xA28, 0xA2C, 0xA30, 0xA34, 0xA38, 0xA3C},
              4, 0, 0x0F },

    /* IBM Token Ring 16/4 Adapter II/A (0x6FC1) - Same layout as 6FC0 */
    { 0x6FC1, 4, 5, 0x03, {2, 3, 10, 11, 0,0,0,0,0,0,0,0,0,0,0,0},
              3, 1, {0xA20, 0xA24, 0xA28, 0xA2C, 0xA30, 0xA34, 0xA38, 0xA3C},
              4, 0, 0x0F },

    /* IBM Token Ring Adapter/A (0xE001) - Original 4Mbps adapter
     * POS 3: bits 0-2 = I/O address, bits 3-5 = RAM address
     * POS 4: bits 0-1 = IRQ select */
    { 0xE001, 4, 0, 0x03, {2, 3, 6, 7, 0,0,0,0,0,0,0,0,0,0,0,0},
              3, 0, {0xA20, 0xA24, 0xA28, 0xA2C, 0xA30, 0xA34, 0xA38, 0xA3C},
              0, 0, 0x00 },

    /* IBM Auto LANStreamer MC 32 (0x8EF5)
     * POS 3: bits 4-6 = I/O address select
     * POS 4: bits 0-3 = arbitration, bits 4-6 = IRQ */
    { 0x8EF5, 4, 4, 0x07, {9, 10, 11, 3, 5, 7, 12, 15, 0,0,0,0,0,0,0,0},
              3, 4, {0x300, 0x340, 0x380, 0x3C0, 0xA20, 0xA60, 0xAA0, 0xAE0},
              4, 0, 0x0F },

    /*========================================================================
     * IBM SCSI ADAPTERS
     *========================================================================*/

    /* IBM SCSI Adapter (0x8EFF)
     * POS 3: bits 4-6 = I/O address select
     * POS 4: bits 0-3 = arbitration level, bits 4-5 = IRQ select */
    { 0x8EFF, 4, 4, 0x03, {14, 11, 10, 15, 0,0,0,0,0,0,0,0,0,0,0,0},
              3, 4, {0x3540, 0x3548, 0x3550, 0x3558, 0x3560, 0x3568, 0x3570, 0x3578},
              4, 0, 0x0F },

    /* IBM SCSI Adapter w/Cache (0x8EFE) - Same layout as 0x8EFF */
    { 0x8EFE, 4, 4, 0x03, {14, 11, 10, 15, 0,0,0,0,0,0,0,0,0,0,0,0},
              3, 4, {0x3540, 0x3548, 0x3550, 0x3558, 0x3560, 0x3568, 0x3570, 0x3578},
              4, 0, 0x0F },

    /* IBM Fast SCSI-2 Adapter/A (0x8F9A)
     * POS 3: bits 4-6 = I/O address
     * POS 4: bits 0-3 = arbitration, bits 4-6 = IRQ */
    { 0x8F9A, 4, 4, 0x07, {14, 11, 10, 15, 9, 5, 3, 12, 0,0,0,0,0,0,0,0},
              3, 4, {0x3540, 0x3548, 0x3550, 0x3558, 0x3560, 0x3568, 0x3570, 0x3578},
              4, 0, 0x0F },

    /* IBM SCSI-2 Fast/Wide Adapter/A (0x8F9B) - Same layout as 0x8F9A */
    { 0x8F9B, 4, 4, 0x07, {14, 11, 10, 15, 9, 5, 3, 12, 0,0,0,0,0,0,0,0},
              3, 4, {0x3540, 0x3548, 0x3550, 0x3558, 0x3560, 0x3568, 0x3570, 0x3578},
              4, 0, 0x0F },

    /* IBM Raid SCSI Adapter (0x8F98) */
    { 0x8F98, 4, 4, 0x07, {14, 11, 10, 15, 9, 5, 3, 12, 0,0,0,0,0,0,0,0},
              3, 4, {0x3540, 0x3548, 0x3550, 0x3558, 0x3560, 0x3568, 0x3570, 0x3578},
              4, 0, 0x0F },

    /*========================================================================
     * IBM DISPLAY ADAPTERS (XGA)
     *========================================================================*/

    /* IBM XGA Display Adapter/A (0x8EFC)
     * POS 2: bit 0 = enable, bits 1-2 = instance select
     * POS 4: bits 0-3 = arbitration level
     * Memory and I/O are fixed or auto-detected */
    { 0x8EFC, 0, 0, 0x00, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
              0, 0, {0x2100, 0x2110, 0x2120, 0x2130, 0,0,0,0},
              4, 0, 0x0F },

    /* IBM XGA-2 Display Adapter/A (0x8EFD) - Same as XGA */
    { 0x8EFD, 0, 0, 0x00, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
              0, 0, {0x2100, 0x2110, 0x2120, 0x2130, 0,0,0,0},
              4, 0, 0x0F },

    /* IBM Image Adapter/A (0x8EFB) */
    { 0x8EFB, 0, 0, 0x00, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
              0, 0, {0x2100, 0x2110, 0x2120, 0x2130, 0,0,0,0},
              4, 0, 0x0F },

    /*========================================================================
     * IBM DISK CONTROLLERS
     *========================================================================*/

    /* IBM ESDI Fixed Disk Controller (0xDDFF)
     * POS 3: bits 4-6 = I/O address
     * POS 4: bits 0-3 = arbitration, bit 4 = IRQ (14 or 15) */
    { 0xDDFF, 4, 4, 0x01, {14, 15, 0,0,0,0,0,0,0,0,0,0,0,0,0,0},
              3, 4, {0x3510, 0x3518, 0x3520, 0x3528, 0x3530, 0x3538, 0x3540, 0x3548},
              4, 0, 0x0F },

    /* IBM ESDI Adapter/A (0xDFFE) */
    { 0xDFFE, 4, 4, 0x01, {14, 15, 0,0,0,0,0,0,0,0,0,0,0,0,0,0},
              3, 4, {0x3510, 0x3518, 0x3520, 0x3528, 0x3530, 0x3538, 0x3540, 0x3548},
              4, 0, 0x0F },

    /* IBM IDE Adapter/A (0xDF9F)
     * POS 3: bits 0-2 = I/O address
     * POS 4: bit 0 = IRQ (14 or 15) */
    { 0xDF9F, 4, 0, 0x01, {14, 15, 0,0,0,0,0,0,0,0,0,0,0,0,0,0},
              3, 0, {0x1F0, 0x170, 0x1E8, 0x168, 0x1E0, 0x160, 0,0},
              0, 0, 0x00 },

    /*========================================================================
     * IBM COMMUNICATION ADAPTERS
     *========================================================================*/

    /* IBM Ethernet Adapter/A (0xE000)
     * POS 3: bits 2-4 = I/O address select
     * POS 4: bits 4-6 = IRQ */
    { 0xE000, 4, 4, 0x07, {3, 4, 5, 9, 10, 11, 12, 15, 0,0,0,0,0,0,0,0},
              3, 2, {0x300, 0x310, 0x320, 0x330, 0x340, 0x350, 0x360, 0x370},
              0, 0, 0x00 },

    /* IBM Dual Async Adapter/A (0xEFE5)
     * POS 3: bits 0-2 = Port A I/O, bits 3-5 = Port B I/O
     * POS 4: bits 0-2 = Port A IRQ, bits 3-5 = Port B IRQ */
    { 0xEFE5, 4, 0, 0x07, {3, 4, 5, 7, 9, 10, 11, 12, 0,0,0,0,0,0,0,0},
              3, 0, {0x3F8, 0x2F8, 0x3220, 0x3228, 0x4220, 0x4228, 0x5220, 0x5228},
              0, 0, 0x00 },

    /* IBM Async Adapter/A (0xEFE4)
     * POS 3: bits 0-2 = I/O address
     * POS 4: bits 0-2 = IRQ */
    { 0xEFE4, 4, 0, 0x07, {3, 4, 5, 7, 9, 10, 11, 12, 0,0,0,0,0,0,0,0},
              3, 0, {0x3F8, 0x2F8, 0x3220, 0x3228, 0x4220, 0x4228, 0x5220, 0x5228},
              0, 0, 0x00 },

    /*========================================================================
     * 3COM NETWORK ADAPTERS
     *========================================================================*/

    /* 3Com EtherLink/MC (0x6298)
     * POS 3: bits 5-7 = I/O address
     * POS 4: bits 4-6 = IRQ */
    { 0x6298, 4, 4, 0x07, {3, 5, 7, 9, 10, 11, 12, 15, 0,0,0,0,0,0,0,0},
              3, 5, {0x300, 0x310, 0x320, 0x330, 0x340, 0x350, 0x360, 0x370},
              4, 0, 0x0F },

    /* 3Com EtherLink/MC 32 (0x6299)
     * Similar to 0x6298 but with 32-bit DMA */
    { 0x6299, 4, 4, 0x07, {3, 5, 7, 9, 10, 11, 12, 15, 0,0,0,0,0,0,0,0},
              3, 5, {0x300, 0x310, 0x320, 0x330, 0x340, 0x350, 0x360, 0x370},
              4, 0, 0x0F },

    /* 3Com EtherLink III/MC (0x627D)
     * POS 3: bits 4-6 = I/O address (base 0x200, step 0x10)
     * POS 4: bits 0-3 = IRQ select */
    { 0x627D, 4, 0, 0x0F, {3, 5, 7, 9, 10, 11, 12, 15, 0,0,0,0,0,0,0,0},
              3, 4, {0x200, 0x210, 0x220, 0x230, 0x240, 0x250, 0x260, 0x270},
              0, 0, 0x00 },

    /*========================================================================
     * SMC/WESTERN DIGITAL NETWORK ADAPTERS
     *========================================================================*/

    /* SMC Ethernet MC (0x80EC)
     * POS 3: bits 0-3 = I/O address
     * POS 4: bits 4-6 = IRQ */
    { 0x80EC, 4, 4, 0x07, {3, 4, 5, 7, 9, 10, 11, 15, 0,0,0,0,0,0,0,0},
              3, 0, {0x280, 0x2A0, 0x2C0, 0x2E0, 0x300, 0x320, 0x340, 0x360},
              0, 0, 0x00 },

    /*========================================================================
     * THIRD-PARTY SCSI ADAPTERS
     *========================================================================*/

    /* Adaptec AHA-1640 SCSI (0x0F1F)
     * POS 3: bits 4-6 = I/O address
     * POS 4: bits 4-5 = IRQ select, bits 0-3 = arbitration */
    { 0x0F1F, 4, 4, 0x03, {9, 10, 11, 12, 0,0,0,0,0,0,0,0,0,0,0,0},
              3, 4, {0x330, 0x334, 0x340, 0x344, 0x350, 0x354, 0x360, 0x364},
              4, 0, 0x0F },

    /* BusLogic BT-640A SCSI (0x7012)
     * POS 3: bits 0-2 = I/O address
     * POS 4: bits 0-3 = arbitration, bits 4-6 = IRQ */
    { 0x7012, 4, 4, 0x07, {9, 10, 11, 12, 14, 15, 0, 0, 0,0,0,0,0,0,0,0},
              3, 0, {0x330, 0x334, 0x230, 0x234, 0x130, 0x134, 0x340, 0x344},
              4, 0, 0x0F },

    /* NCR 53C700 SCSI (0x8EF2)
     * POS 4: bits 0-3 = arbitration, bits 4-6 = IRQ */
    { 0x8EF2, 4, 4, 0x07, {3, 5, 7, 9, 10, 11, 12, 14, 0,0,0,0,0,0,0,0},
              3, 4, {0x3F0, 0x3E0, 0x3D0, 0x3C0, 0x3B0, 0x3A0, 0x390, 0x380},
              4, 0, 0x0F },

    /* Future Domain TMC-850 SCSI (0x8FA2)
     * POS 3: bits 0-2 = I/O address
     * POS 4: bits 4-6 = IRQ */
    { 0x8FA2, 4, 4, 0x07, {3, 5, 10, 11, 12, 14, 15, 0, 0,0,0,0,0,0,0,0},
              3, 0, {0x140, 0x150, 0x160, 0x170, 0x180, 0x190, 0x1A0, 0x1B0},
              0, 0, 0x00 },

    /*========================================================================
     * THIRD-PARTY GRAPHICS ADAPTERS
     *========================================================================*/

    /* ATI 8514/Ultra MC (0x5E80)
     * Graphics adapters typically don't use IRQ/DMA
     * POS 2: bit 0 = enable, bit 1 = ROM enable */
    { 0x5E80, 0, 0, 0x00, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
              0, 0, {0x2E8, 0,0,0,0,0,0,0},
              0, 0, 0x00 },

    /* ATI Graphics Ultra MC (0x5E81) */
    { 0x5E81, 0, 0, 0x00, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
              0, 0, {0x2E8, 0,0,0,0,0,0,0},
              0, 0, 0x00 },

    /* Matrox MG-1024 MC (0x4EDD) */
    { 0x4EDD, 0, 0, 0x00, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
              0, 0, {0,0,0,0,0,0,0,0},
              4, 0, 0x0F },

    /*========================================================================
     * SOUND ADAPTERS
     *========================================================================*/

    /* Sound Blaster MCV (0x5138)
     * POS 3: bits 0-2 = I/O address
     * POS 4: bits 4-6 = IRQ, bits 0-1 = DMA channel */
    { 0x5138, 4, 4, 0x07, {5, 7, 9, 10, 11, 12, 14, 15, 0,0,0,0,0,0,0,0},
              3, 0, {0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x2E0, 0x300},
              4, 0, 0x03 },  /* DMA in bits 0-1 */

    /* Sound Blaster Pro MCV (0x5137)
     * Same layout as 0x5138 */
    { 0x5137, 4, 4, 0x07, {5, 7, 9, 10, 11, 12, 14, 15, 0,0,0,0,0,0,0,0},
              3, 0, {0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x2E0, 0x300},
              4, 0, 0x03 },

    /* Pro AudioSpectrum 16 MC (0x6BBA)
     * POS 3: bits 0-2 = I/O address
     * POS 4: bits 4-6 = IRQ, bits 0-1 = DMA */
    { 0x6BBA, 4, 4, 0x07, {5, 7, 9, 10, 11, 12, 14, 15, 0,0,0,0,0,0,0,0},
              3, 0, {0x388, 0x384, 0x380, 0x38C, 0,0,0,0},
              4, 0, 0x03 },

    /*========================================================================
     * MODEM ADAPTERS
     *========================================================================*/

    /* Intel SatisFAXtion Modem MC (0x5946)
     * POS 3: bits 0-2 = I/O address (COM port)
     * POS 4: bits 0-2 = IRQ */
    { 0x5946, 4, 0, 0x07, {3, 4, 5, 7, 9, 10, 11, 12, 0,0,0,0,0,0,0,0},
              3, 0, {0x3F8, 0x2F8, 0x3E8, 0x2E8, 0x3220, 0x3228, 0x4220, 0x4228},
              0, 0, 0x00 },

    /* US Robotics Courier V.32bis MC (0x50AB)
     * POS 3: bits 0-2 = I/O address
     * POS 4: bits 0-2 = IRQ */
    { 0x50AB, 4, 0, 0x07, {3, 4, 5, 7, 9, 10, 11, 12, 0,0,0,0,0,0,0,0},
              3, 0, {0x3F8, 0x2F8, 0x3E8, 0x2E8, 0x3220, 0x3228, 0x4220, 0x4228},
              0, 0, 0x00 },

    /*========================================================================
     * ADDITIONAL IBM ADAPTERS
     *========================================================================*/

    /* IBM 3270 Connection (0x6FC2) - Coax adapter
     * POS 4: bits 0-3 = arbitration, bits 4-6 = IRQ */
    { 0x6FC2, 4, 4, 0x07, {3, 5, 7, 9, 10, 11, 12, 15, 0,0,0,0,0,0,0,0},
              3, 4, {0x380, 0x388, 0x390, 0x398, 0x3A0, 0x3A8, 0x3B0, 0x3B8},
              4, 0, 0x0F },

    /* IBM VGA Adapter (0x90EE) - No IRQ/DMA needed */
    { 0x90EE, 0, 0, 0x00, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
              0, 0, {0x3C0, 0,0,0,0,0,0,0},
              0, 0, 0x00 },

    /*========================================================================
     * TERMINATOR - Must be last entry
     *========================================================================*/
    { 0, 0, 0, 0, {0}, 0, 0, {0}, 0, 0, 0 }
};

/* Parse MCA POS registers to extract resources */
static void mca_parse_pos_resources(slot_config_t *cfg)
{
    const mca_pos_decode_t *decode;
    unsigned char irq_idx, io_idx, arb;
    int i;
    const card_db_entry_t *db_entry;

    /* Look up specific POS decoding for this adapter */
    decode = NULL;
    for (i = 0; g_mca_pos_decode[i].adapter_id != 0; i++) {
        if (g_mca_pos_decode[i].adapter_id == cfg->vendor_id) {
            decode = &g_mca_pos_decode[i];
            break;
        }
    }

    if (decode != NULL) {
        /* Use specific POS decoding */

        /* Extract IRQ */
        if (decode->irq_pos >= 2 && decode->irq_mask != 0) {
            irq_idx = (cfg->pos[decode->irq_pos] >> decode->irq_shift) & decode->irq_mask;
            if (decode->irq_table[irq_idx] != 0) {
                cfg->irqs[0].irq = decode->irq_table[irq_idx];
                cfg->irqs[0].level_triggered = 1;  /* MCA is level-triggered */
                cfg->irqs[0].shared = 1;           /* MCA supports IRQ sharing */
                cfg->irq_count = 1;
            }
        }

        /* Extract I/O base */
        if (decode->io_pos >= 2) {
            io_idx = (cfg->pos[decode->io_pos] >> decode->io_shift) & 0x07;
            if (decode->io_base_table[io_idx] != 0) {
                cfg->ioports[0].base = decode->io_base_table[io_idx];
                cfg->ioports[0].size = 16;
                cfg->ioport_count = 1;
            }
        }

        /* Extract arbitration level (DMA-like for bus masters) */
        if (decode->arb_pos >= 2 && decode->arb_mask != 0) {
            arb = (cfg->pos[decode->arb_pos] >> decode->arb_shift) & decode->arb_mask;
            /* Store arbitration level as DMA channel equivalent */
            cfg->dmas[0].channel = arb;
            cfg->dmas[0].transfer_size = 2;  /* 32-bit bus master */
            cfg->dmas[0].timing = 3;         /* Burst */
            cfg->dma_count = 1;
        }
    } else {
        /* Generic POS parsing for unknown adapters */

        /* Common pattern: IRQ in POS 4 bits 4-7 */
        irq_idx = (cfg->pos[4] >> 4) & 0x0F;
        if (irq_idx > 0 && irq_idx < 16) {
            /* Common IRQ encoding: 0=none, 1-15=IRQ number */
            cfg->irqs[0].irq = irq_idx;
            cfg->irqs[0].level_triggered = 1;
            cfg->irqs[0].shared = 1;
            cfg->irq_count = 1;
        }

        /* Common pattern: Arbitration level in POS 4 bits 0-3 */
        arb = cfg->pos[4] & 0x0F;
        if (arb != 0x0F && arb != 0x00) {
            cfg->dmas[0].channel = arb;
            cfg->dmas[0].transfer_size = 2;
            cfg->dmas[0].timing = 3;
            cfg->dma_count = 1;
        }

        /* If no resources decoded, try database defaults */
        if (cfg->irq_count == 0) {
            for (i = 0; g_mca_cards[i].name != NULL; i++) {
                if (g_mca_cards[i].vendor_id == cfg->vendor_id) {
                    db_entry = &g_mca_cards[i];

                    if (db_entry->default_irq != 0) {
                        cfg->irqs[0].irq = db_entry->default_irq;
                        cfg->irqs[0].level_triggered = 1;
                        cfg->irqs[0].shared = 1;
                        cfg->irq_count = 1;
                    }
                    if (db_entry->default_dma != 0) {
                        cfg->dmas[0].channel = db_entry->default_dma;
                        cfg->dmas[0].transfer_size = 1;
                        cfg->dmas[0].timing = 0;
                        cfg->dma_count = 1;
                    }
                    if (db_entry->default_io != 0) {
                        cfg->ioports[0].base = db_entry->default_io;
                        cfg->ioports[0].size = 16;
                        cfg->ioport_count = 1;
                    }
                    if (db_entry->default_mem != 0) {
                        cfg->mem_ranges[0].base = db_entry->default_mem;
                        cfg->mem_ranges[0].size = 0x4000;
                        cfg->mem_ranges[0].is_rom = 0;
                        cfg->mem_ranges[0].is_shared = 0;
                        cfg->mem_count = 1;
                    }
                    break;
                }
            }
        }
    }
}

/* Read MCA slot configuration */
static int mca_read_slot(unsigned char slot, slot_config_t *cfg)
{
    unsigned int adapter_id;
    int i;
    const char *name;

    if (slot >= 8) return BUSCFG_ERR_SLOT;

    memset(cfg, 0, sizeof(slot_config_t));

    /* Select slot and enable setup mode */
    _disable();
    outp(MCA_ADAPTER_SETUP, (slot & 0x07) | 0x08);

    /* Read adapter ID from POS 0-1 */
    adapter_id = inp(MCA_POS_BASE) | ((unsigned int)inp(MCA_POS_BASE + 1) << 8);

    /* Check for empty slot */
    if (adapter_id == 0xFFFF || adapter_id == 0x0000) {
        outp(MCA_ADAPTER_SETUP, 0);
        _enable();
        return BUSCFG_ERR_SLOT;
    }

    /* Read all POS registers */
    for (i = 0; i < 8; i++) {
        cfg->pos[i] = inp(MCA_POS_BASE + i);
    }

    /* Disable setup mode */
    outp(MCA_ADAPTER_SETUP, 0);
    _enable();

    cfg->slot = slot;
    cfg->bus_type = BUS_MCA;
    cfg->vendor_id = adapter_id;
    cfg->device_id = 0;

    /* Card enable is POS 2, bit 0 */
    cfg->enabled = (cfg->pos[2] & 0x01) ? 1 : 0;

    /* Look up name in database */
    name = buscfg_lookup_card(BUS_MCA, adapter_id, 0);
    if (name) {
        strncpy(cfg->name, name, sizeof(cfg->name) - 1);
    } else {
        sprintf(cfg->name, "MCA Adapter @%04X", adapter_id);
    }

    /* Parse POS registers to extract IRQ/DMA/IO/Memory resources */
    mca_parse_pos_resources(cfg);

    /* Detect MCA adapter capabilities from POS registers
     *
     * Bus Master Detection:
     * - Adapters with arbitration level in POS 4 bits 0-3 are bus masters
     * - Non-bus-master cards typically have 0x00 or 0x0F in these bits
     * - We detect based on whether the card uses arbitration (from decode table)
     *
     * Fairness:
     * - For bus master cards, fairness is typically controlled by a POS bit
     * - Defaults to enabled; adapter-specific bit location
     * - Most common: fairness enable in POS 5 or POS 3
     *
     * Streaming Data:
     * - High-performance adapters (32-bit SCSI, Token Ring, etc.)
     * - Indicated by streaming/burst mode bits in POS registers
     */
    cfg->mca_arb_level = cfg->pos[4] & 0x0F;

    /* Detect bus master capability:
     * - If arbitration level is non-zero and not 0x0F (invalid), likely bus master
     * - Cross-reference with known bus master adapters */
    cfg->mca_bus_master = 0;
    cfg->mca_fairness = 0;
    cfg->mca_streaming = 0;

    /* Check against known bus master adapter IDs */
    switch (adapter_id) {
        /* IBM SCSI adapters - all are bus masters */
        case 0x8EFF:  /* IBM SCSI Adapter */
        case 0x8EFE:  /* IBM SCSI Adapter w/Cache */
        case 0x8F9A:  /* IBM Fast SCSI-2 Adapter/A */
        case 0x8F9B:  /* IBM Fast SCSI-2 Adapter/A w/Cache */
        case 0x8F4F:  /* IBM Raid Controller */
            cfg->mca_bus_master = 1;
            cfg->mca_fairness = (cfg->pos[5] & 0x01) ? 1 : 0;  /* Fairness in POS 5 bit 0 */
            cfg->mca_streaming = 1;  /* SCSI adapters support streaming */
            break;

        /* IBM Token Ring adapters - bus masters */
        case 0x6FC0:  /* IBM Token Ring 16/4 Adapter/A */
        case 0x6FC1:  /* IBM Token Ring 16/4 Adapter II/A */
        case 0x8EF5:  /* IBM Auto LANStreamer MC 32 */
        case 0xE001:  /* IBM Token Ring Adapter/A */
            cfg->mca_bus_master = 1;
            cfg->mca_fairness = 1;  /* Token Ring typically has fairness on */
            cfg->mca_streaming = (adapter_id == 0x8EF5) ? 1 : 0;  /* LANStreamer supports streaming */
            break;

        /* IBM Ethernet adapters - some are bus masters */
        case 0x6042:  /* IBM Ethernet Adapter/A */
            cfg->mca_bus_master = 0;  /* This one is not a bus master */
            break;

        /* XGA display adapters - bus masters */
        case 0x8FD8:  /* IBM XGA Display Adapter */
        case 0x8FD9:  /* IBM XGA-2 Display Adapter */
            cfg->mca_bus_master = 1;
            cfg->mca_fairness = 1;
            cfg->mca_streaming = 1;  /* XGA uses streaming for video */
            break;

        /* 3Com Ethernet - bus masters */
        case 0x6276:  /* 3Com EtherLink/MC */
        case 0x62F6:  /* 3Com EtherLink/MC 32 */
        case 0x62F7:  /* 3Com EtherLink III/MC */
            cfg->mca_bus_master = (adapter_id != 0x6276) ? 1 : 0;  /* MC 32 and III are bus masters */
            cfg->mca_fairness = 1;
            break;

        /* Adaptec SCSI - bus masters */
        case 0x0F1F:  /* Adaptec AHA-1640 */
            cfg->mca_bus_master = 1;
            cfg->mca_fairness = 1;
            cfg->mca_streaming = 1;
            break;

        default:
            /* For unknown adapters, infer from arbitration level:
             * If arb level is 0-14 and not all zeros, likely bus master */
            if (cfg->mca_arb_level > 0 && cfg->mca_arb_level < 15) {
                cfg->mca_bus_master = 1;
                cfg->mca_fairness = 1;  /* Assume fairness enabled by default */
            }
            break;
    }

    return BUSCFG_OK;
}

/* Read raw MCA POS register */
unsigned char buscfg_mca_read_pos(unsigned char slot, unsigned char reg)
{
    unsigned char value;

    if (slot >= 8 || reg >= 8) return 0xFF;

    _disable();
    outp(MCA_ADAPTER_SETUP, (slot & 0x07) | 0x08);
    value = inp(MCA_POS_BASE + reg);
    outp(MCA_ADAPTER_SETUP, 0);
    _enable();

    return value;
}

/* Write raw MCA POS register */
int buscfg_mca_write_pos(unsigned char slot, unsigned char reg, unsigned char value)
{
    if (slot >= 8 || reg >= 8) return BUSCFG_ERR_SLOT;

    /* POS 0 and 1 are read-only (adapter ID) */
    if (reg < 2) return BUSCFG_ERR_WRITE;

    _disable();
    outp(MCA_ADAPTER_SETUP, (slot & 0x07) | 0x08);
    outp(MCA_POS_BASE + reg, value);
    outp(MCA_ADAPTER_SETUP, 0);
    _enable();

    return BUSCFG_OK;
}

/* Enable/disable MCA slot (POS 2, bit 0) */
static int mca_enable_slot(unsigned char slot, unsigned char enable)
{
    unsigned char pos2;

    if (slot >= 8) return BUSCFG_ERR_SLOT;

    pos2 = buscfg_mca_read_pos(slot, 2);

    if (enable) {
        pos2 |= 0x01;
    } else {
        pos2 &= ~0x01;
    }

    return buscfg_mca_write_pos(slot, 2, pos2);
}

/*============================================================================
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/

int buscfg_init(void)
{
    memset(&g_buscfg_state, 0, sizeof(g_buscfg_state));

    g_buscfg_state.bus_detected = buscfg_detect_bus();

    if (g_buscfg_state.bus_detected == 0) {
        return BUSCFG_ERR_NOBUS;
    }

    /* For EISA systems, detect BIOS vendor for NVM protocol selection */
    if (g_buscfg_state.bus_detected == BUS_EISA) {
        /* Check for INT 15h AH=D8h EISA CMOS services first */
        g_eisa_int15_available = check_eisa_int15_services();

        /* Detect BIOS vendor by scanning ROM */
        detect_eisa_bios_vendor();

        /* Detect system board model via slot 0 ID */
        detect_eisa_sysboard();
    }

    /* For MCA systems, detect PS/2 model for slot count and capabilities */
    if (g_buscfg_state.bus_detected == BUS_MCA) {
        detect_mca_model();
    }

    return BUSCFG_OK;
}

buscfg_state_t *buscfg_get_state(void)
{
    return &g_buscfg_state;
}

int buscfg_enum_slots(void)
{
    int count = 0;
    int slot;
    int max_slots;
    int result;

    if (g_buscfg_state.bus_detected == BUS_EISA) {
        /* Use detected slot count from system board identification */
        max_slots = g_eisa_slot_count;
    } else if (g_buscfg_state.bus_detected == BUS_MCA) {
        /* Use detected slot count from PS/2 model identification */
        max_slots = g_mca_slot_count;
    } else {
        return 0;
    }

    for (slot = 0; slot < max_slots; slot++) {
        if (g_buscfg_state.bus_detected == BUS_EISA) {
            result = eisa_read_slot(slot, &g_buscfg_state.slots[count]);
        } else {
            result = mca_read_slot(slot, &g_buscfg_state.slots[count]);
        }

        if (result == BUSCFG_OK) {
            count++;
        }
    }

    g_buscfg_state.slot_count = count;
    return count;
}

int buscfg_read_slot(unsigned char slot, slot_config_t *cfg)
{
    if (g_buscfg_state.bus_detected == BUS_EISA) {
        return eisa_read_slot(slot, cfg);
    } else if (g_buscfg_state.bus_detected == BUS_MCA) {
        return mca_read_slot(slot, cfg);
    }
    return BUSCFG_ERR_NOBUS;
}

int buscfg_slot_enable(unsigned char slot, unsigned char enable)
{
    int result;

    if (g_buscfg_state.bus_detected == BUS_EISA) {
        result = eisa_enable_slot(slot, enable);
    } else if (g_buscfg_state.bus_detected == BUS_MCA) {
        result = mca_enable_slot(slot, enable);
    } else {
        return BUSCFG_ERR_NOBUS;
    }

    if (result == BUSCFG_OK) {
        g_buscfg_state.modified = 1;
    }

    return result;
}

/* Helper: Find slot in state array by slot number */
static slot_config_t *find_slot_config(unsigned char slot)
{
    int i;
    for (i = 0; i < g_buscfg_state.slot_count; i++) {
        if (g_buscfg_state.slots[i].slot == slot) {
            return &g_buscfg_state.slots[i];
        }
    }
    return NULL;
}

int buscfg_set_irq(unsigned char slot, unsigned char irq_idx, unsigned char irq)
{
    slot_config_t *cfg = find_slot_config(slot);

    if (!cfg) return BUSCFG_ERR_SLOT;
    if (irq_idx >= MAX_SLOT_IRQS) return BUSCFG_ERR_SLOT;
    if (irq > 15) return BUSCFG_ERR_WRITE;

    /* Check for conflicts before applying */
    if (buscfg_check_irq_conflict(slot, irq) != CONFLICT_NONE) {
        /* Allow the change but caller should check conflicts */
    }

    /* Update the slot configuration */
    if (irq_idx >= cfg->irq_count) {
        /* Adding new IRQ assignment */
        if (cfg->irq_count >= MAX_SLOT_IRQS) return BUSCFG_ERR_WRITE;
        cfg->irq_count = irq_idx + 1;
    }

    cfg->irqs[irq_idx].irq = irq;
    /* Preserve or set default trigger/share settings based on bus type */
    if (cfg->bus_type == BUS_EISA || cfg->bus_type == BUS_MCA) {
        cfg->irqs[irq_idx].level_triggered = 1;
        cfg->irqs[irq_idx].shared = 1;
    }

    g_buscfg_state.modified = 1;
    return BUSCFG_OK;
}

int buscfg_set_dma(unsigned char slot, unsigned char dma_idx, unsigned char channel)
{
    slot_config_t *cfg = find_slot_config(slot);

    if (!cfg) return BUSCFG_ERR_SLOT;
    if (dma_idx >= MAX_SLOT_DMAS) return BUSCFG_ERR_SLOT;
    if (channel > 7) return BUSCFG_ERR_WRITE;

    /* Check for conflicts */
    if (buscfg_check_dma_conflict(slot, channel) != CONFLICT_NONE) {
        /* Allow the change but caller should check conflicts */
    }

    /* Update the slot configuration */
    if (dma_idx >= cfg->dma_count) {
        if (cfg->dma_count >= MAX_SLOT_DMAS) return BUSCFG_ERR_WRITE;
        cfg->dma_count = dma_idx + 1;
    }

    cfg->dmas[dma_idx].channel = channel;
    /* Set default transfer settings */
    if (cfg->dmas[dma_idx].transfer_size == 0) {
        cfg->dmas[dma_idx].transfer_size = 1;  /* 16-bit */
    }

    g_buscfg_state.modified = 1;
    return BUSCFG_OK;
}

int buscfg_set_ioport(unsigned char slot, unsigned char io_idx, unsigned int base)
{
    slot_config_t *cfg = find_slot_config(slot);
    unsigned int size;

    if (!cfg) return BUSCFG_ERR_SLOT;
    if (io_idx >= MAX_SLOT_IOPORTS) return BUSCFG_ERR_SLOT;

    /* Get existing size or use default */
    size = (io_idx < cfg->ioport_count && cfg->ioports[io_idx].size > 0)
           ? cfg->ioports[io_idx].size : 16;

    /* Check for conflicts */
    if (buscfg_check_io_conflict(slot, base, size) != CONFLICT_NONE) {
        /* Allow the change but caller should check conflicts */
    }

    /* Update the slot configuration */
    if (io_idx >= cfg->ioport_count) {
        if (cfg->ioport_count >= MAX_SLOT_IOPORTS) return BUSCFG_ERR_WRITE;
        cfg->ioport_count = io_idx + 1;
    }

    cfg->ioports[io_idx].base = base;
    if (cfg->ioports[io_idx].size == 0) {
        cfg->ioports[io_idx].size = 16;  /* Default size */
    }

    g_buscfg_state.modified = 1;
    return BUSCFG_OK;
}

int buscfg_set_membase(unsigned char slot, unsigned char mem_idx, unsigned long base)
{
    slot_config_t *cfg = find_slot_config(slot);
    unsigned long size;

    if (!cfg) return BUSCFG_ERR_SLOT;
    if (mem_idx >= MAX_SLOT_MEMRANGES) return BUSCFG_ERR_SLOT;

    /* Get existing size or use default */
    size = (mem_idx < cfg->mem_count && cfg->mem_ranges[mem_idx].size > 0)
           ? cfg->mem_ranges[mem_idx].size : 0x4000;

    /* Check for conflicts */
    if (buscfg_check_mem_conflict(slot, base, size) != CONFLICT_NONE) {
        /* Allow the change but caller should check conflicts */
    }

    /* Update the slot configuration */
    if (mem_idx >= cfg->mem_count) {
        if (cfg->mem_count >= MAX_SLOT_MEMRANGES) return BUSCFG_ERR_WRITE;
        cfg->mem_count = mem_idx + 1;
    }

    cfg->mem_ranges[mem_idx].base = base;
    if (cfg->mem_ranges[mem_idx].size == 0) {
        cfg->mem_ranges[mem_idx].size = 0x4000;  /* Default 16KB */
    }

    g_buscfg_state.modified = 1;
    return BUSCFG_OK;
}

/*============================================================================
 * NVM OPERATIONS - EISA Extended CMOS and MCA NVRAM
 *============================================================================*/

/*============================================================================
 * VENDOR-SPECIFIC EISA NVM ACCESS METHODS
 *
 * COMPAQ Protocol (ports 0x800-0x802):
 *   - NVM size: 4KB (0x000-0xFFF)
 *   - Slot data: 64 bytes per slot at offset slot * 0x40
 *   - Checksum: byte 7 of each slot block (sum to 0x100)
 *   - No write protection or unlock sequence
 *
 * HP Protocol (ports 0xC00-0xC02):
 *   - NVM size: 4KB or 8KB depending on model
 *   - Slot data: 64 bytes per slot at offset slot * 0x40
 *   - Checksum: byte 63 of each slot block (XOR checksum)
 *   - May require BIOS unlock via INT 15h
 *
 * DEC/NCR Protocol (INT 15h AH=D8h):
 *   - Uses BIOS services, not direct port I/O
 *   - AL=00h: Read EISA CMOS, AL=01h: Write EISA CMOS
 *   - CX = byte count, DX = offset, ES:BX = buffer
 *   - Most portable, works across vendor implementations
 *
 * AMI/Award/Phoenix:
 *   - Usually Compaq-compatible (0x800-0x802)
 *   - Some may support INT 15h as well
 *
 *============================================================================*/

/* Note: g_eisa_int15_available declared earlier in MODULE STATE section */

/*----------------------------------------------------------------------------
 * COMPAQ/Generic Direct Port I/O NVM Access (0x800-0x802)
 *----------------------------------------------------------------------------*/

/* Read byte via Compaq protocol */
static unsigned char eisa_nvm_read_compaq(unsigned int addr)
{
    unsigned char value;

    _disable();
    outp(EISA_NVM_COMPAQ_ADDR_LO, addr & 0xFF);
    outp(EISA_NVM_COMPAQ_ADDR_HI, (addr >> 8) & 0xFF);
    value = inp(EISA_NVM_COMPAQ_DATA);
    _enable();

    return value;
}

/* Write byte via Compaq protocol */
static void eisa_nvm_write_compaq(unsigned int addr, unsigned char value)
{
    _disable();
    outp(EISA_NVM_COMPAQ_ADDR_LO, addr & 0xFF);
    outp(EISA_NVM_COMPAQ_ADDR_HI, (addr >> 8) & 0xFF);
    outp(EISA_NVM_COMPAQ_DATA, value);
    _enable();
}

/*----------------------------------------------------------------------------
 * HP/Olivetti Direct Port I/O NVM Access (0xC00-0xC02)
 *----------------------------------------------------------------------------*/

/* Read byte via HP protocol */
static unsigned char eisa_nvm_read_hp(unsigned int addr)
{
    unsigned char value;

    _disable();
    outp(EISA_NVM_HP_ADDR_LO, addr & 0xFF);
    outp(EISA_NVM_HP_ADDR_HI, (addr >> 8) & 0xFF);
    value = inp(EISA_NVM_HP_DATA);
    _enable();

    return value;
}

/* Write byte via HP protocol */
static void eisa_nvm_write_hp(unsigned int addr, unsigned char value)
{
    _disable();
    outp(EISA_NVM_HP_ADDR_LO, addr & 0xFF);
    outp(EISA_NVM_HP_ADDR_HI, (addr >> 8) & 0xFF);
    outp(EISA_NVM_HP_DATA, value);
    _enable();
}

/*----------------------------------------------------------------------------
 * INT 15h AH=D8h BIOS Services NVM Access (DEC/NCR and standards-compliant)
 *----------------------------------------------------------------------------*/

/* Read bytes via INT 15h services */
static int eisa_nvm_read_int15(unsigned int addr, unsigned char *buffer, unsigned int count)
{
    union REGS regs;
    struct SREGS sregs;

    /* INT 15h AH=D8h AL=00h: Read EISA CMOS */
    regs.h.ah = 0xD8;
    regs.h.al = 0x00;           /* Read function */
    regs.x.dx = addr;           /* Offset in NVM */
    regs.x.cx = count;          /* Byte count */
    segread(&sregs);
    sregs.es = FP_SEG(buffer);
    regs.x.bx = FP_OFF(buffer);

    int86x(0x15, &regs, &regs, &sregs);

    if (regs.x.cflag || regs.h.ah != 0) {
        return BUSCFG_ERR_NVM;
    }

    return BUSCFG_OK;
}

/* Write bytes via INT 15h services */
static int eisa_nvm_write_int15(unsigned int addr, const unsigned char *buffer, unsigned int count)
{
    union REGS regs;
    struct SREGS sregs;

    /* INT 15h AH=D8h AL=01h: Write EISA CMOS */
    regs.h.ah = 0xD8;
    regs.h.al = 0x01;           /* Write function */
    regs.x.dx = addr;           /* Offset in NVM */
    regs.x.cx = count;          /* Byte count */
    segread(&sregs);
    sregs.es = FP_SEG(buffer);
    regs.x.bx = FP_OFF(buffer);

    int86x(0x15, &regs, &regs, &sregs);

    if (regs.x.cflag || regs.h.ah != 0) {
        return BUSCFG_ERR_NVM;
    }

    return BUSCFG_OK;
}

/*----------------------------------------------------------------------------
 * Unified NVM Access Functions (dispatch based on detected vendor)
 *----------------------------------------------------------------------------*/

/* Read a single byte from EISA NVM using appropriate method */
static unsigned char eisa_nvm_read(unsigned int addr)
{
    unsigned char value = 0xFF;

    /* Prefer INT 15h if available - most portable */
    if (g_eisa_int15_available) {
        if (eisa_nvm_read_int15(addr, &value, 1) == BUSCFG_OK) {
            return value;
        }
        /* Fall through to direct I/O if INT 15h fails */
    }

    /* Use vendor-specific direct port I/O */
    switch (g_eisa_bios_vendor) {
        case EISA_VENDOR_HP:
        case EISA_VENDOR_OLIVETTI:
            return eisa_nvm_read_hp(addr);

        case EISA_VENDOR_COMPAQ:
        case EISA_VENDOR_AMI:
        case EISA_VENDOR_AWARD:
        case EISA_VENDOR_PHOENIX:
        case EISA_VENDOR_DELL:
        case EISA_VENDOR_MICRONICS:
        case EISA_VENDOR_INTEL:
        default:
            return eisa_nvm_read_compaq(addr);
    }
}

/* Write a single byte to EISA NVM using appropriate method */
static void eisa_nvm_write(unsigned int addr, unsigned char value)
{
    /* Prefer INT 15h if available - most portable */
    if (g_eisa_int15_available) {
        if (eisa_nvm_write_int15(addr, &value, 1) == BUSCFG_OK) {
            return;
        }
        /* Fall through to direct I/O if INT 15h fails */
    }

    /* Use vendor-specific direct port I/O */
    switch (g_eisa_bios_vendor) {
        case EISA_VENDOR_HP:
        case EISA_VENDOR_OLIVETTI:
            eisa_nvm_write_hp(addr, value);
            break;

        case EISA_VENDOR_COMPAQ:
        case EISA_VENDOR_AMI:
        case EISA_VENDOR_AWARD:
        case EISA_VENDOR_PHOENIX:
        case EISA_VENDOR_DELL:
        case EISA_VENDOR_MICRONICS:
        case EISA_VENDOR_INTEL:
        default:
            eisa_nvm_write_compaq(addr, value);
            break;
    }
}

/*----------------------------------------------------------------------------
 * Vendor-Specific Checksum Calculations
 *----------------------------------------------------------------------------*/

/* Compaq checksum: sum of bytes 0-62, byte 63 makes sum = 0x100 */
static unsigned char eisa_nvm_checksum_compaq(unsigned int slot_offset)
{
    unsigned char sum = 0;
    int i;

    for (i = 0; i < 63; i++) {
        sum += eisa_nvm_read(slot_offset + i);
    }

    return (unsigned char)(0x100 - sum);
}

/* HP checksum: XOR of bytes 0-62, stored in byte 63 */
static unsigned char eisa_nvm_checksum_hp(unsigned int slot_offset)
{
    unsigned char xor_sum = 0;
    int i;

    for (i = 0; i < 63; i++) {
        xor_sum ^= eisa_nvm_read(slot_offset + i);
    }

    return xor_sum;
}

/* Calculate checksum using appropriate method for detected vendor */
static unsigned char eisa_nvm_checksum(unsigned int slot_offset)
{
    switch (g_eisa_bios_vendor) {
        case EISA_VENDOR_HP:
        case EISA_VENDOR_OLIVETTI:
            return eisa_nvm_checksum_hp(slot_offset);

        case EISA_VENDOR_COMPAQ:
        case EISA_VENDOR_AMI:
        case EISA_VENDOR_AWARD:
        case EISA_VENDOR_PHOENIX:
        case EISA_VENDOR_DEC:
        case EISA_VENDOR_NCR:
        case EISA_VENDOR_DELL:
        case EISA_VENDOR_MICRONICS:
        case EISA_VENDOR_INTEL:
        default:
            return eisa_nvm_checksum_compaq(slot_offset);
    }
}

/* Verify checksum of a slot */
static int eisa_nvm_verify_checksum(unsigned int slot_offset)
{
    unsigned char stored, calculated;

    switch (g_eisa_bios_vendor) {
        case EISA_VENDOR_HP:
        case EISA_VENDOR_OLIVETTI:
            /* HP: XOR checksum in byte 63 */
            stored = eisa_nvm_read(slot_offset + 63);
            calculated = eisa_nvm_checksum_hp(slot_offset);
            return (stored == calculated) ? BUSCFG_OK : BUSCFG_ERR_NVM;

        default:
            /* Compaq and others: sum checksum in byte 7 */
            stored = eisa_nvm_read(slot_offset + 7);
            calculated = eisa_nvm_checksum_compaq(slot_offset);
            return (stored == calculated) ? BUSCFG_OK : BUSCFG_ERR_NVM;
    }
}

/* Save EISA slot configuration to NVM (vendor-specific format) */
static int eisa_save_slot_nvm(slot_config_t *cfg)
{
    unsigned int slot_offset;
    unsigned int func_offset;
    unsigned int checksum_offset;
    int func;

    slot_offset = (unsigned int)cfg->slot * 0x40;

    /* Write board ID */
    eisa_nvm_write(slot_offset + 0, cfg->vendor_id & 0xFF);
    eisa_nvm_write(slot_offset + 1, (cfg->vendor_id >> 8) & 0xFF);
    eisa_nvm_write(slot_offset + 2, cfg->device_id & 0xFF);
    eisa_nvm_write(slot_offset + 3, (cfg->device_id >> 8) & 0xFF);

    /* Write enable flag in byte 4 */
    eisa_nvm_write(slot_offset + 4, cfg->enabled ? 0x80 : 0x00);

    /* Write function configuration data */
    /* Each function is 14 bytes starting at offset 8 */
    for (func = 0; func < 4; func++) {
        func_offset = slot_offset + 8 + (func * 14);

        if (func == 0) {
            /* First function gets all resources */
            /* IRQ configuration */
            if (cfg->irq_count > 0) {
                unsigned char irq_byte = cfg->irqs[0].irq & 0x0F;
                if (cfg->irqs[0].level_triggered) irq_byte |= 0x20;
                if (cfg->irqs[0].shared) irq_byte |= 0x40;
                eisa_nvm_write(func_offset + 8, irq_byte);
            } else {
                eisa_nvm_write(func_offset + 8, 0xFF);
            }

            if (cfg->irq_count > 1) {
                unsigned char irq_byte = cfg->irqs[1].irq & 0x0F;
                if (cfg->irqs[1].level_triggered) irq_byte |= 0x20;
                if (cfg->irqs[1].shared) irq_byte |= 0x40;
                eisa_nvm_write(func_offset + 9, irq_byte);
            } else {
                eisa_nvm_write(func_offset + 9, 0xFF);
            }

            /* DMA configuration */
            if (cfg->dma_count > 0) {
                unsigned char dma_byte = cfg->dmas[0].channel & 0x07;
                dma_byte |= (cfg->dmas[0].transfer_size & 0x03) << 3;
                dma_byte |= (cfg->dmas[0].timing & 0x03) << 5;
                eisa_nvm_write(func_offset + 10, dma_byte);
            } else {
                eisa_nvm_write(func_offset + 10, 0xFF);
            }

            /* I/O port configuration */
            if (cfg->ioport_count > 0) {
                eisa_nvm_write(func_offset + 12, cfg->ioports[0].base & 0xFF);
                eisa_nvm_write(func_offset + 13, (cfg->ioports[0].base >> 8) & 0xFF);
            }
        }
    }

    /* Write checksum at vendor-specific location */
    switch (g_eisa_bios_vendor) {
        case EISA_VENDOR_HP:
        case EISA_VENDOR_OLIVETTI:
            /* HP/Olivetti: XOR checksum in byte 63 */
            checksum_offset = slot_offset + 63;
            eisa_nvm_write(checksum_offset, eisa_nvm_checksum_hp(slot_offset));
            break;

        default:
            /* Compaq and others: sum checksum in byte 7 */
            checksum_offset = slot_offset + 7;
            eisa_nvm_write(checksum_offset, eisa_nvm_checksum_compaq(slot_offset));
            break;
    }

    return BUSCFG_OK;
}

/*
 * MCA NVRAM:
 * MCA systems store adapter configuration in system NVRAM.
 * The configuration is automatically applied at boot from NVRAM.
 *
 * For PS/2, the Reference Diskette stores the configuration which
 * is then programmed into NVRAM. We can write POS registers directly
 * and the system remembers them across reboots (on most PS/2 models).
 *
 * For our purposes, we write POS registers directly which takes
 * immediate effect. The system board's NVRAM controller handles
 * persistence automatically on PS/2 Model 50/60/70/80.
 */

/* Write MCA slot configuration to hardware (POS registers) */
static int mca_save_slot_config(slot_config_t *cfg)
{
    int i;
    int result;

    /* POS 0-1 are read-only (adapter ID), skip them */
    /* Write POS 2-7 */
    for (i = 2; i < 8; i++) {
        result = buscfg_mca_write_pos(cfg->slot, i, cfg->pos[i]);
        if (result != BUSCFG_OK) {
            return result;
        }
    }

    return BUSCFG_OK;
}

int buscfg_save_nvm(void)
{
    int i;
    int result;
    slot_config_t *cfg;

    if (g_buscfg_state.bus_detected == BUS_EISA) {
        /* Save EISA configuration to Extended CMOS NVM */
        for (i = 0; i < g_buscfg_state.slot_count; i++) {
            cfg = &g_buscfg_state.slots[i];
            result = eisa_save_slot_nvm(cfg);
            if (result != BUSCFG_OK) {
                return result;
            }
        }
    } else if (g_buscfg_state.bus_detected == BUS_MCA) {
        /* Save MCA configuration to POS registers (auto-persisted) */
        for (i = 0; i < g_buscfg_state.slot_count; i++) {
            cfg = &g_buscfg_state.slots[i];
            result = mca_save_slot_config(cfg);
            if (result != BUSCFG_OK) {
                return result;
            }
        }
    } else {
        return BUSCFG_ERR_NOBUS;
    }

    g_buscfg_state.modified = 0;
    return BUSCFG_OK;
}

int buscfg_load_nvm(void)
{
    /* For EISA: Configuration is read from hardware registers which
     * reflect NVM contents (loaded by BIOS at boot).
     * For MCA: POS registers reflect current (persistent) configuration.
     *
     * In both cases, buscfg_enum_slots() already reads the current
     * hardware state, so this function just re-enumerates. */

    return buscfg_enum_slots() > 0 ? BUSCFG_OK : BUSCFG_ERR_READ;
}

/*============================================================================
 * SYSTEM RESERVED RESOURCES
 * These are typically used by motherboard and cannot be assigned to cards
 *============================================================================*/

/* IRQs reserved by system (cannot be used by expansion cards) */
#define RESERVED_IRQS   0x0007  /* IRQ 0 (timer), 1 (keyboard), 2 (cascade) */

/* System IRQs that are often in use but may be available */
#define SYSTEM_IRQS     0x20C0  /* IRQ 6 (floppy), 7 (LPT1), 13 (FPU) */

/* Standard IRQs available for expansion cards */
#define AVAILABLE_IRQS  0xDE38  /* IRQ 3,4,5,9,10,11,12,14,15 */

/* DMA channels reserved by system */
#define RESERVED_DMAS   0x11    /* DMA 0 (refresh), 4 (cascade) */

/* Standard DMA channels available */
#define AVAILABLE_DMAS  0xEE    /* DMA 1,2,3,5,6,7 */

/*============================================================================
 * CONFLICT DETECTION HELPERS
 *============================================================================*/

/* Check if two I/O ranges overlap */
static int io_ranges_overlap(unsigned int base1, unsigned int size1,
                             unsigned int base2, unsigned int size2)
{
    if (size1 == 0 || size2 == 0) return 0;

    /* Range 1 ends before range 2 starts, or range 2 ends before range 1 starts */
    if (base1 + size1 <= base2) return 0;
    if (base2 + size2 <= base1) return 0;

    return 1;
}

/* Check if two memory ranges overlap */
static int mem_ranges_overlap(unsigned long base1, unsigned long size1,
                              unsigned long base2, unsigned long size2)
{
    if (size1 == 0 || size2 == 0) return 0;

    if (base1 + size1 <= base2) return 0;
    if (base2 + size2 <= base1) return 0;

    return 1;
}

/* Add a conflict to the state's conflict list */
static void add_conflict(unsigned char slot_a, unsigned char slot_b,
                         unsigned char type, unsigned char res_idx)
{
    if (g_buscfg_state.conflict_count >= MAX_CONFLICTS) return;

    g_buscfg_state.conflicts[g_buscfg_state.conflict_count].slot_a = slot_a;
    g_buscfg_state.conflicts[g_buscfg_state.conflict_count].slot_b = slot_b;
    g_buscfg_state.conflicts[g_buscfg_state.conflict_count].conflict_type = type;
    g_buscfg_state.conflicts[g_buscfg_state.conflict_count].resource_idx = res_idx;
    g_buscfg_state.conflict_count++;
}

/*============================================================================
 * RESOURCE CONFLICT CHECKING
 *============================================================================*/

unsigned char buscfg_check_irq_conflict(unsigned char slot, unsigned char irq)
{
    int i, j;
    slot_config_t *check;

    /* Check against reserved IRQs */
    if ((1 << irq) & RESERVED_IRQS) {
        return CONFLICT_IRQ;
    }

    /* Check against other slots */
    for (i = 0; i < g_buscfg_state.slot_count; i++) {
        check = &g_buscfg_state.slots[i];

        /* Skip the slot we're checking */
        if (check->slot == slot) continue;

        /* Skip disabled slots */
        if (!check->enabled) continue;

        /* Check each IRQ assigned to this slot */
        for (j = 0; j < check->irq_count; j++) {
            if (check->irqs[j].irq == irq) {
                /* Conflict unless both IRQs are shareable and level-triggered */
                if (check->irqs[j].shared && check->irqs[j].level_triggered) {
                    /* Shareable - not a conflict */
                    continue;
                }
                return CONFLICT_IRQ;
            }
        }
    }

    return CONFLICT_NONE;
}

unsigned char buscfg_check_dma_conflict(unsigned char slot, unsigned char dma)
{
    int i, j;
    slot_config_t *check;

    /* Check against reserved DMAs */
    if ((1 << dma) & RESERVED_DMAS) {
        return CONFLICT_DMA;
    }

    /* Check against other slots */
    for (i = 0; i < g_buscfg_state.slot_count; i++) {
        check = &g_buscfg_state.slots[i];

        if (check->slot == slot) continue;
        if (!check->enabled) continue;

        for (j = 0; j < check->dma_count; j++) {
            if (check->dmas[j].channel == dma) {
                return CONFLICT_DMA;  /* DMA cannot be shared */
            }
        }
    }

    return CONFLICT_NONE;
}

unsigned char buscfg_check_io_conflict(unsigned char slot, unsigned int base, unsigned int size)
{
    int i, j;
    slot_config_t *check;

    if (size == 0) return CONFLICT_NONE;

    /* Check against other slots */
    for (i = 0; i < g_buscfg_state.slot_count; i++) {
        check = &g_buscfg_state.slots[i];

        if (check->slot == slot) continue;
        if (!check->enabled) continue;

        for (j = 0; j < check->ioport_count; j++) {
            if (io_ranges_overlap(base, size,
                                  check->ioports[j].base,
                                  check->ioports[j].size)) {
                return CONFLICT_IOPORT;
            }
        }
    }

    return CONFLICT_NONE;
}

unsigned char buscfg_check_mem_conflict(unsigned char slot, unsigned long base, unsigned long size)
{
    int i, j;
    slot_config_t *check;

    if (size == 0) return CONFLICT_NONE;

    /* Check against other slots */
    for (i = 0; i < g_buscfg_state.slot_count; i++) {
        check = &g_buscfg_state.slots[i];

        if (check->slot == slot) continue;
        if (!check->enabled) continue;

        for (j = 0; j < check->mem_count; j++) {
            /* Shared memory regions can overlap */
            if (check->mem_ranges[j].is_shared) continue;

            if (mem_ranges_overlap(base, size,
                                   check->mem_ranges[j].base,
                                   check->mem_ranges[j].size)) {
                return CONFLICT_MEMORY;
            }
        }
    }

    return CONFLICT_NONE;
}

/*============================================================================
 * FULL CONFLICT CHECK
 *============================================================================*/

int buscfg_check_conflicts(void)
{
    int i, j, k, l;
    slot_config_t *slot_a, *slot_b;

    /* Clear existing conflicts */
    g_buscfg_state.conflict_count = 0;

    /* Check all pairs of slots */
    for (i = 0; i < g_buscfg_state.slot_count; i++) {
        slot_a = &g_buscfg_state.slots[i];
        if (!slot_a->enabled) continue;

        for (j = i + 1; j < g_buscfg_state.slot_count; j++) {
            slot_b = &g_buscfg_state.slots[j];
            if (!slot_b->enabled) continue;

            /* Check IRQ conflicts */
            for (k = 0; k < slot_a->irq_count; k++) {
                for (l = 0; l < slot_b->irq_count; l++) {
                    if (slot_a->irqs[k].irq == slot_b->irqs[l].irq) {
                        /* Only conflict if not both shareable level-triggered */
                        if (!(slot_a->irqs[k].shared &&
                              slot_a->irqs[k].level_triggered &&
                              slot_b->irqs[l].shared &&
                              slot_b->irqs[l].level_triggered)) {
                            add_conflict(slot_a->slot, slot_b->slot, CONFLICT_IRQ, k);
                        }
                    }
                }
            }

            /* Check DMA conflicts (DMA can never be shared) */
            for (k = 0; k < slot_a->dma_count; k++) {
                for (l = 0; l < slot_b->dma_count; l++) {
                    if (slot_a->dmas[k].channel == slot_b->dmas[l].channel) {
                        add_conflict(slot_a->slot, slot_b->slot, CONFLICT_DMA, k);
                    }
                }
            }

            /* Check I/O port conflicts */
            for (k = 0; k < slot_a->ioport_count; k++) {
                for (l = 0; l < slot_b->ioport_count; l++) {
                    if (io_ranges_overlap(slot_a->ioports[k].base,
                                          slot_a->ioports[k].size,
                                          slot_b->ioports[l].base,
                                          slot_b->ioports[l].size)) {
                        add_conflict(slot_a->slot, slot_b->slot, CONFLICT_IOPORT, k);
                    }
                }
            }

            /* Check memory range conflicts */
            for (k = 0; k < slot_a->mem_count; k++) {
                /* Skip shared memory regions */
                if (slot_a->mem_ranges[k].is_shared) continue;

                for (l = 0; l < slot_b->mem_count; l++) {
                    if (slot_b->mem_ranges[l].is_shared) continue;

                    if (mem_ranges_overlap(slot_a->mem_ranges[k].base,
                                           slot_a->mem_ranges[k].size,
                                           slot_b->mem_ranges[l].base,
                                           slot_b->mem_ranges[l].size)) {
                        add_conflict(slot_a->slot, slot_b->slot, CONFLICT_MEMORY, k);
                    }
                }
            }
        }
    }

    return g_buscfg_state.conflict_count;
}

/*============================================================================
 * RESOURCE AVAILABILITY
 *============================================================================*/

unsigned int buscfg_get_available_irqs(unsigned char exclude_slot)
{
    unsigned int available = AVAILABLE_IRQS;
    int i, j;
    slot_config_t *slot;

    /* Remove IRQs used by other slots */
    for (i = 0; i < g_buscfg_state.slot_count; i++) {
        slot = &g_buscfg_state.slots[i];

        if (slot->slot == exclude_slot) continue;
        if (!slot->enabled) continue;

        for (j = 0; j < slot->irq_count; j++) {
            /* Don't remove shareable IRQs from available list */
            if (!slot->irqs[j].shared || !slot->irqs[j].level_triggered) {
                available &= ~(1 << slot->irqs[j].irq);
            }
        }
    }

    return available;
}

unsigned char buscfg_get_available_dmas(unsigned char exclude_slot)
{
    unsigned char available = AVAILABLE_DMAS;
    int i, j;
    slot_config_t *slot;

    /* Remove DMAs used by other slots */
    for (i = 0; i < g_buscfg_state.slot_count; i++) {
        slot = &g_buscfg_state.slots[i];

        if (slot->slot == exclude_slot) continue;
        if (!slot->enabled) continue;

        for (j = 0; j < slot->dma_count; j++) {
            available &= ~(1 << slot->dmas[j].channel);
        }
    }

    return available;
}

/*============================================================================
 * GET DEFAULT RESOURCES FROM CARD DATABASE
 *============================================================================*/

int buscfg_get_defaults(unsigned char slot, slot_config_t *defaults)
{
    const card_db_entry_t *db;
    slot_config_t *cfg;
    int i;

    /* Find the slot in our state */
    cfg = NULL;
    for (i = 0; i < g_buscfg_state.slot_count; i++) {
        if (g_buscfg_state.slots[i].slot == slot) {
            cfg = &g_buscfg_state.slots[i];
            break;
        }
    }

    if (!cfg) return BUSCFG_ERR_SLOT;

    /* Select the right database */
    if (cfg->bus_type == BUS_EISA) {
        db = g_eisa_cards;
    } else if (cfg->bus_type == BUS_MCA) {
        db = g_mca_cards;
    } else {
        return BUSCFG_ERR_NOBUS;
    }

    /* Search for the card */
    for (i = 0; db[i].name != NULL; i++) {
        int match = 0;

        if (cfg->bus_type == BUS_EISA) {
            match = (db[i].vendor_id == cfg->vendor_id &&
                     db[i].device_id == cfg->device_id);
        } else {
            match = (db[i].vendor_id == cfg->vendor_id);
        }

        if (match) {
            /* Copy current config and apply defaults */
            memcpy(defaults, cfg, sizeof(slot_config_t));

            /* Apply default IRQ if defined */
            if (db[i].default_irq != 0) {
                defaults->irq_count = 1;
                defaults->irqs[0].irq = db[i].default_irq;
                defaults->irqs[0].level_triggered = (cfg->bus_type == BUS_EISA) ? 1 : 0;
                defaults->irqs[0].shared = (cfg->bus_type == BUS_EISA) ? 1 : 0;
            }

            /* Apply default DMA if defined */
            if (db[i].default_dma != 0) {
                defaults->dma_count = 1;
                defaults->dmas[0].channel = db[i].default_dma;
                defaults->dmas[0].transfer_size = 1;  /* 16-bit */
                defaults->dmas[0].timing = 0;         /* ISA compatible */
            }

            /* Apply default I/O if defined */
            if (db[i].default_io != 0) {
                defaults->ioport_count = 1;
                defaults->ioports[0].base = db[i].default_io;
                defaults->ioports[0].size = 16;  /* Assume 16 bytes */
            }

            /* Apply default memory if defined */
            if (db[i].default_mem != 0) {
                defaults->mem_count = 1;
                defaults->mem_ranges[0].base = db[i].default_mem;
                defaults->mem_ranges[0].size = 0x4000;  /* Assume 16KB */
                defaults->mem_ranges[0].is_rom = 0;
                defaults->mem_ranges[0].is_shared = 0;
            }

            return BUSCFG_OK;
        }
    }

    return BUSCFG_ERR_NOCFG;
}

/*============================================================================
 * AUTO-CONFIGURATION
 *============================================================================*/

/* Priority order for IRQ allocation (prefer higher IRQs first) */
static const unsigned char irq_priority[] = {
    11, 10, 15, 12, 9, 5, 3, 4, 14, 7, 6
};
#define IRQ_PRIORITY_COUNT  (sizeof(irq_priority) / sizeof(irq_priority[0]))

/* Priority order for DMA allocation */
static const unsigned char dma_priority[] = {
    5, 6, 7, 1, 3
};
#define DMA_PRIORITY_COUNT  (sizeof(dma_priority) / sizeof(dma_priority[0]))

int buscfg_auto_config(void)
{
    int i, j, k;
    slot_config_t *slot;
    slot_config_t defaults;
    unsigned int avail_irqs;
    unsigned char avail_dmas;
    unsigned char found_irq, found_dma;

    /* First pass: Apply defaults from database */
    for (i = 0; i < g_buscfg_state.slot_count; i++) {
        slot = &g_buscfg_state.slots[i];

        if (!slot->enabled) continue;

        /* Try to get defaults from database */
        if (buscfg_get_defaults(slot->slot, &defaults) == BUSCFG_OK) {
            /* Apply defaults only if slot doesn't have resources yet */
            if (slot->irq_count == 0 && defaults.irq_count > 0) {
                memcpy(slot->irqs, defaults.irqs, sizeof(defaults.irqs));
                slot->irq_count = defaults.irq_count;
            }
            if (slot->dma_count == 0 && defaults.dma_count > 0) {
                memcpy(slot->dmas, defaults.dmas, sizeof(defaults.dmas));
                slot->dma_count = defaults.dma_count;
            }
            if (slot->ioport_count == 0 && defaults.ioport_count > 0) {
                memcpy(slot->ioports, defaults.ioports, sizeof(defaults.ioports));
                slot->ioport_count = defaults.ioport_count;
            }
            if (slot->mem_count == 0 && defaults.mem_count > 0) {
                memcpy(slot->mem_ranges, defaults.mem_ranges, sizeof(defaults.mem_ranges));
                slot->mem_count = defaults.mem_count;
            }
        }
    }

    /* Second pass: Resolve conflicts by reassigning resources */
    for (i = 0; i < g_buscfg_state.slot_count; i++) {
        slot = &g_buscfg_state.slots[i];

        if (!slot->enabled) continue;

        /* Check and resolve IRQ conflicts */
        for (j = 0; j < slot->irq_count; j++) {
            if (buscfg_check_irq_conflict(slot->slot, slot->irqs[j].irq) != CONFLICT_NONE) {
                /* Find an available IRQ */
                avail_irqs = buscfg_get_available_irqs(slot->slot);
                found_irq = 0xFF;

                for (k = 0; k < IRQ_PRIORITY_COUNT; k++) {
                    if (avail_irqs & (1 << irq_priority[k])) {
                        found_irq = irq_priority[k];
                        break;
                    }
                }

                if (found_irq != 0xFF) {
                    slot->irqs[j].irq = found_irq;
                    g_buscfg_state.modified = 1;
                }
            }
        }

        /* Check and resolve DMA conflicts */
        for (j = 0; j < slot->dma_count; j++) {
            if (buscfg_check_dma_conflict(slot->slot, slot->dmas[j].channel) != CONFLICT_NONE) {
                /* Find an available DMA channel */
                avail_dmas = buscfg_get_available_dmas(slot->slot);
                found_dma = 0xFF;

                for (k = 0; k < DMA_PRIORITY_COUNT; k++) {
                    if (avail_dmas & (1 << dma_priority[k])) {
                        found_dma = dma_priority[k];
                        break;
                    }
                }

                if (found_dma != 0xFF) {
                    slot->dmas[j].channel = found_dma;
                    g_buscfg_state.modified = 1;
                }
            }
        }

        /* Note: I/O port and memory conflicts are harder to resolve automatically
         * as we need to know valid alternative addresses for each card.
         * For now, we just detect them - manual resolution may be required. */
    }

    g_buscfg_state.auto_configured = 1;

    /* Check for remaining conflicts */
    return buscfg_check_conflicts();
}

/*============================================================================
 * CONFIGURATION VALIDATION
 *============================================================================*/

int buscfg_validate_config(void)
{
    int conflicts = buscfg_check_conflicts();

    if (conflicts > 0) {
        return BUSCFG_ERR_CONFLICT;
    }

    return BUSCFG_OK;
}

/*============================================================================
 * CONFIGURATION REPORT EXPORT
 *============================================================================*/

int buscfg_export_report(const char *filename)
{
    FILE *fp;
    int i, j;
    slot_config_t *slot;
    char id_str[16];

    fp = fopen(filename, "w");
    if (!fp) return BUSCFG_ERR_WRITE;

    /* Header */
    fprintf(fp, "CACHEKIT Bus Configuration Report\n");
    fprintf(fp, "==================================\n\n");

    /* System information */
    if (g_buscfg_state.bus_detected == BUS_EISA) {
        fprintf(fp, "Bus Type: EISA (Extended ISA)\n");
        fprintf(fp, "System Board: %s\n", buscfg_get_eisa_sysboard_name());
        fprintf(fp, "BIOS Vendor: %s\n", buscfg_get_eisa_vendor_name());
        fprintf(fp, "Slot Count: %d\n", buscfg_get_eisa_slot_count());
    } else if (g_buscfg_state.bus_detected == BUS_MCA) {
        fprintf(fp, "Bus Type: MCA (Micro Channel Architecture)\n");
        fprintf(fp, "System Model: %s\n", buscfg_get_mca_model_name());
        fprintf(fp, "Model/Submodel: %02Xh/%02Xh\n",
                buscfg_get_mca_model_byte(), buscfg_get_mca_submodel_byte());
        fprintf(fp, "Bus Width: %d-bit\n", buscfg_get_mca_bus_width());
        fprintf(fp, "Bus Speed: %d MHz\n", buscfg_get_mca_bus_speed());
        fprintf(fp, "Slot Count: %d\n", buscfg_get_mca_slot_count());
        fprintf(fp, "IBM PS/2: %s\n", buscfg_is_ibm_ps2() ? "Yes" : "No (Clone)");
    } else {
        fprintf(fp, "Bus Type: None detected\n");
        fclose(fp);
        return BUSCFG_ERR_NOBUS;
    }

    fprintf(fp, "\nPopulated Slots: %d\n", g_buscfg_state.slot_count);
    fprintf(fp, "Conflicts: %d\n", g_buscfg_state.conflict_count);
    fprintf(fp, "\n");

    /* Slot details */
    fprintf(fp, "SLOT CONFIGURATION\n");
    fprintf(fp, "------------------\n\n");

    for (i = 0; i < g_buscfg_state.slot_count; i++) {
        slot = &g_buscfg_state.slots[i];
        buscfg_format_slot_id(slot, id_str);

        fprintf(fp, "Slot %d: %s\n", slot->slot, slot->name);
        fprintf(fp, "  ID: %s\n", id_str);
        fprintf(fp, "  Status: %s\n", slot->enabled ? "Enabled" : "Disabled");

        /* IRQs */
        if (slot->irq_count > 0) {
            fprintf(fp, "  IRQ:");
            for (j = 0; j < slot->irq_count; j++) {
                fprintf(fp, " %d%s", slot->irqs[j].irq,
                        slot->irqs[j].level_triggered ? "(L)" : "(E)");
            }
            fprintf(fp, "\n");
        }

        /* DMA */
        if (slot->dma_count > 0) {
            fprintf(fp, "  DMA:");
            for (j = 0; j < slot->dma_count; j++) {
                fprintf(fp, " %d", slot->dmas[j].channel);
            }
            fprintf(fp, "\n");
        }

        /* I/O Ports */
        if (slot->ioport_count > 0) {
            fprintf(fp, "  I/O:");
            for (j = 0; j < slot->ioport_count; j++) {
                fprintf(fp, " %04Xh-%04Xh",
                        slot->ioports[j].base,
                        slot->ioports[j].base + slot->ioports[j].size - 1);
            }
            fprintf(fp, "\n");
        }

        /* Memory */
        if (slot->mem_count > 0) {
            fprintf(fp, "  Memory:");
            for (j = 0; j < slot->mem_count; j++) {
                fprintf(fp, " %05lXh-%05lXh%s",
                        slot->mem_ranges[j].base,
                        slot->mem_ranges[j].base + slot->mem_ranges[j].size - 1,
                        slot->mem_ranges[j].is_rom ? "(ROM)" : "");
            }
            fprintf(fp, "\n");
        }

        /* MCA-specific: POS registers and capabilities */
        if (slot->bus_type == BUS_MCA) {
            fprintf(fp, "  POS: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                    slot->pos[0], slot->pos[1], slot->pos[2], slot->pos[3],
                    slot->pos[4], slot->pos[5], slot->pos[6], slot->pos[7]);
            if (slot->mca_bus_master) {
                fprintf(fp, "  Bus Master: Yes (ARB %d, %s%s)\n",
                        slot->mca_arb_level,
                        slot->mca_fairness ? "Fairness" : "Burst",
                        slot->mca_streaming ? ", Streaming" : "");
            }
        }

        fprintf(fp, "\n");
    }

    /* Conflict details */
    if (g_buscfg_state.conflict_count > 0) {
        fprintf(fp, "RESOURCE CONFLICTS\n");
        fprintf(fp, "------------------\n\n");

        for (i = 0; i < g_buscfg_state.conflict_count; i++) {
            resource_conflict_t *c = &g_buscfg_state.conflicts[i];
            fprintf(fp, "Conflict %d: Slots %d and %d - ", i + 1, c->slot_a, c->slot_b);

            if (c->conflict_type & CONFLICT_IRQ) {
                fprintf(fp, "IRQ %d\n", c->value.irq);
            } else if (c->conflict_type & CONFLICT_DMA) {
                fprintf(fp, "DMA %d\n", c->value.dma);
            } else if (c->conflict_type & CONFLICT_IOPORT) {
                fprintf(fp, "I/O %04Xh\n", c->value.io_base);
            } else if (c->conflict_type & CONFLICT_MEMORY) {
                fprintf(fp, "Memory %05lXh\n", c->value.mem_base);
            }
        }
        fprintf(fp, "\n");
    }

    /* Footer */
    fprintf(fp, "---\n");
    fprintf(fp, "Generated by CACHEKIT v3.1\n");

    fclose(fp);
    return BUSCFG_OK;
}

/*============================================================================
 * ISA PLUG AND PLAY SUPPORT
 *
 * ISA PnP Protocol Overview:
 * 1. Send initiation key (32 bytes) to port 0x279
 * 2. Isolate cards using serial identifier read
 * 3. Assign Card Select Numbers (CSN) 1-n
 * 4. Read resource data from each card
 * 5. Configure logical devices with IRQ/DMA/IO/Memory
 * 6. Activate logical devices
 *
 * Reference: Plug and Play ISA Specification v1.0a (May 1994)
 *============================================================================*/

/* ISA PnP Port Addresses */
#define ISAPNP_ADDRESS      0x279   /* Address port (write only) */
#define ISAPNP_WRITE_DATA   0xA79   /* Write data port */
#define ISAPNP_READ_DATA    0x203   /* Read data port (default, can be relocated) */

/* ISA PnP Registers (written to ADDRESS port) */
#define ISAPNP_REG_SET_RD_DATA  0x00    /* Set Read Data port address */
#define ISAPNP_REG_SERIAL_ISO   0x01    /* Serial Isolation register */
#define ISAPNP_REG_CFG_CTRL     0x02    /* Config Control register */
#define ISAPNP_REG_WAKE         0x03    /* Wake[CSN] command */
#define ISAPNP_REG_RES_DATA     0x04    /* Resource Data register */
#define ISAPNP_REG_STATUS       0x05    /* Status register */
#define ISAPNP_REG_CSN          0x06    /* Card Select Number */
#define ISAPNP_REG_LOGDEV       0x07    /* Logical Device Number */
#define ISAPNP_REG_ACTIVATE     0x30    /* Activate register */
#define ISAPNP_REG_IO_RANGE_CHK 0x31    /* I/O Range Check register */

/* Logical Device Configuration Registers */
#define ISAPNP_REG_MEM_BASE_HI  0x40    /* Memory base bits 23-16 */
#define ISAPNP_REG_MEM_BASE_LO  0x41    /* Memory base bits 15-8 */
#define ISAPNP_REG_IO_BASE_HI   0x60    /* I/O base bits 15-8 */
#define ISAPNP_REG_IO_BASE_LO   0x61    /* I/O base bits 7-0 */
#define ISAPNP_REG_IRQ_NO       0x70    /* IRQ number */
#define ISAPNP_REG_IRQ_TYPE     0x71    /* IRQ type (edge/level) */
#define ISAPNP_REG_DMA          0x74    /* DMA channel */

/* Config Control register bits */
#define ISAPNP_CC_RESET         0x01    /* Reset all CSNs to 0 */
#define ISAPNP_CC_WAIT_KEY      0x02    /* Return to Wait for Key state */
#define ISAPNP_CC_RESET_CSN     0x04    /* Reset CSN to 0 */

/* ISA PnP Initiation Key (32 bytes)
 * This magic sequence puts all PnP cards into configuration mode */
static const unsigned char isapnp_initiation_key[32] = {
    0x6A, 0xB5, 0xDA, 0xED, 0xF6, 0xFB, 0x7D, 0xBE,
    0xDF, 0x6F, 0x37, 0x1B, 0x0D, 0x86, 0xC3, 0x61,
    0xB0, 0x58, 0x2C, 0x16, 0x8B, 0x45, 0xA2, 0xD1,
    0xE8, 0x74, 0x3A, 0x9D, 0xCE, 0xE7, 0x73, 0x39
};

/* ISA PnP State */
static unsigned char g_isapnp_card_count = 0;
static unsigned char g_isapnp_device_count = 0;
static unsigned int  g_isapnp_read_port = ISAPNP_READ_DATA;

/* ISA PnP Card Database - Common PnP cards */
typedef struct {
    unsigned long vendor_id;    /* 32-bit EISA-style vendor ID */
    unsigned int product_id;    /* 16-bit product ID */
    const char *name;
} isapnp_card_entry_t;

static const isapnp_card_entry_t g_isapnp_cards[] = {
    /*========================================================================
     * CREATIVE LABS (CTL) - Sound Cards
     *========================================================================*/
    { 0x0000630E, 0x0001, "Creative SB16 PnP" },
    { 0x0000630E, 0x0021, "Creative SB16 PnP (IDE)" },
    { 0x0000630E, 0x0022, "Creative SB16 PnP (IDE)" },
    { 0x0000630E, 0x0023, "Creative SB16 PnP" },
    { 0x0000630E, 0x0024, "Creative SB AWE64 PnP" },
    { 0x0000630E, 0x0025, "Creative SB16 PnP" },
    { 0x0000630E, 0x0026, "Creative SB16 PnP" },
    { 0x0000630E, 0x0027, "Creative SB16 PnP" },
    { 0x0000630E, 0x0028, "Creative SB16 PnP" },
    { 0x0000630E, 0x0029, "Creative SB AWE32 PnP" },
    { 0x0000630E, 0x002A, "Creative SB AWE64 PnP" },
    { 0x0000630E, 0x002B, "Creative SB AWE64 Gold" },
    { 0x0000630E, 0x002C, "Creative SB16 PnP (CT4170)" },
    { 0x0000630E, 0x0031, "Creative SB16 Device" },
    { 0x0000630E, 0x0041, "Creative SB16 WaveSynth" },
    { 0x0000630E, 0x0042, "Creative SB AWE64 WaveSynth" },
    { 0x0000630E, 0x0043, "Creative SB16 WaveSynth" },
    { 0x0000630E, 0x0044, "Creative SB AWE64 WaveSynth" },
    { 0x0000630E, 0x0045, "Creative SB AWE64 Gold WaveSynth" },
    { 0x0000630E, 0x0046, "Creative SB16 WaveSynth" },
    { 0x0000630E, 0x0047, "Creative SB AWE64 WaveSynth" },
    { 0x0000630E, 0x0051, "Creative SB16 Vibra PnP" },
    { 0x0000630E, 0x0061, "Creative SB32 PnP" },
    { 0x0000630E, 0x0070, "Creative SB Vibra16C PnP" },
    { 0x0000630E, 0x0071, "Creative SB Vibra16CL PnP" },
    { 0x0000630E, 0x0072, "Creative SB Vibra16X PnP" },
    { 0x0000630E, 0x7001, "Creative Phone Blaster PnP" },
    { 0x0000630E, 0x7002, "Creative Modem Blaster PnP" },

    /*========================================================================
     * ESS TECHNOLOGY (ESS) - Sound Cards
     *========================================================================*/
    { 0x00004535, 0x0100, "ESS ES688 AudioDrive" },
    { 0x00004535, 0x0101, "ESS ES688 AudioDrive" },
    { 0x00004535, 0x0102, "ESS ES1688 AudioDrive" },
    { 0x00004535, 0x0104, "ESS ES1788 AudioDrive" },
    { 0x00004535, 0x0106, "ESS ES1888 AudioDrive" },
    { 0x00004535, 0x0108, "ESS ES888 AudioDrive" },
    { 0x00004535, 0x0114, "ESS ES1788 AudioDrive" },
    { 0x00004535, 0x0116, "ESS ES1888 AudioDrive" },
    { 0x00004535, 0x0968, "ESS ES1688 AudioDrive" },
    { 0x00004535, 0x1868, "ESS ES1868 AudioDrive" },
    { 0x00004535, 0x1869, "ESS ES1869 AudioDrive" },
    { 0x00004535, 0x1878, "ESS ES1878 AudioDrive" },
    { 0x00004535, 0x1879, "ESS ES1879 AudioDrive" },
    { 0x00004535, 0x1887, "ESS ES1887 AudioDrive" },
    { 0x00004535, 0x1888, "ESS ES1888 AudioDrive" },
    { 0x00004535, 0x8898, "ESS ES1898 AudioDrive" },

    /*========================================================================
     * AZTECH (AZT) - Sound Cards
     *========================================================================*/
    { 0x00005441, 0x1008, "Aztech PRO16V (AZT2320)" },
    { 0x00005441, 0x1605, "Aztech Sound Galaxy Nova 16" },
    { 0x00005441, 0x1608, "Aztech Sound Galaxy Pro 16" },
    { 0x00005441, 0x2001, "Aztech AZT3000 MPU401" },
    { 0x00005441, 0x2316, "Aztech Sound Galaxy Washington 16" },
    { 0x00005441, 0x2320, "Aztech Sound Galaxy 16 PnP" },
    { 0x00005441, 0x3000, "Aztech PB Sound III 336" },
    { 0x00005441, 0x3001, "Aztech AZT3000 Game Port" },
    { 0x00005441, 0x4001, "Aztech AZT3000 Modem" },

    /*========================================================================
     * YAMAHA (YMH) - Sound Cards
     *========================================================================*/
    { 0x0000A865, 0x0001, "Yamaha OPL3-SA" },
    { 0x0000A865, 0x0020, "Yamaha OPL3-SA2" },
    { 0x0000A865, 0x0021, "Yamaha OPL3-SA2 (2nd)" },
    { 0x0000A865, 0x0030, "Yamaha OPL3-SA3" },
    { 0x0000A865, 0x0031, "Yamaha OPL3-SA3 MPU401" },
    { 0x0000A865, 0x0032, "Yamaha OPL3-SA3 Game Port" },

    /*========================================================================
     * CRYSTAL/CIRRUS (CSC) - Sound Cards
     *========================================================================*/
    { 0x00004352, 0x0000, "Crystal CS4235 Control" },
    { 0x00004352, 0x0001, "Crystal CS4236 Control" },
    { 0x00004352, 0x0003, "Crystal CS4236B Control" },
    { 0x00004352, 0x0010, "Crystal CS4235 Audio" },
    { 0x00004352, 0x000F, "Crystal CS4236 Audio" },
    { 0x00004352, 0x0100, "Crystal CS4610 GamePort" },
    { 0x00004352, 0x0101, "Crystal CS4327 Audio" },
    { 0x00004352, 0x4231, "Crystal CS4231 Audio" },
    { 0x00004352, 0x4232, "Crystal CS4232 Audio" },
    { 0x00004352, 0x4236, "Crystal CS4236 Audio" },
    { 0x00004352, 0x4237, "Crystal CS4237 Audio" },

    /*========================================================================
     * OPTi (OPT) - Sound Cards
     *========================================================================*/
    { 0x00004F50, 0x0924, "OPTi 82C924 Audio" },
    { 0x00004F50, 0x0925, "OPTi 82C925 Audio" },
    { 0x00004F50, 0x0930, "OPTi 82C930 Audio" },
    { 0x00004F50, 0x0931, "OPTi 82C931 Audio" },
    { 0x00004F50, 0x1931, "OPTi 82C931 MPU401" },

    /*========================================================================
     * GRAVIS (GRV) - Sound Cards
     *========================================================================*/
    { 0x00001048, 0x0000, "Gravis UltraSound PnP" },
    { 0x00001048, 0x0001, "Gravis UltraSound PnP Synth" },
    { 0x00001048, 0x0002, "Gravis UltraSound PnP MIDI" },
    { 0x00001048, 0x0003, "Gravis UltraSound PnP Joystick" },
    { 0x00001048, 0x0100, "Gravis UltraSound PnP Pro" },
    { 0x00001048, 0x0110, "Gravis UltraSound MAX" },

    /*========================================================================
     * ANALOG DEVICES (ADV) - Sound Cards
     *========================================================================*/
    { 0x00004144, 0x1816, "AD1816A Audio" },
    { 0x00004144, 0x0001, "AD1816A MPU401" },
    { 0x00004144, 0x550A, "InterWave STB with TEA6330T" },

    /*========================================================================
     * C-MEDIA (CMI) - Sound Cards
     *========================================================================*/
    { 0x0000434D, 0x8330, "C-Media CMI8330 Audio" },
    { 0x0000434D, 0x0001, "C-Media CMI8330 SB16 Compat" },
    { 0x0000434D, 0x0002, "C-Media CMI8330 MPU401" },
    { 0x0000434D, 0x8338, "C-Media CMI8338 Audio" },

    /*========================================================================
     * ENSONIQ (ENS) - Sound Cards
     *========================================================================*/
    { 0x00004E51, 0x1300, "Ensoniq Soundscape PnP" },
    { 0x00004E51, 0x1310, "Ensoniq Soundscape Elite PnP" },
    { 0x00004E51, 0x2000, "Ensoniq AudioPCI S5016" },

    /*========================================================================
     * MEDIA VISION (MED) - Sound Cards
     *========================================================================*/
    { 0x00004D45, 0x0001, "Media Vision Pro Audio Spectrum" },
    { 0x00004D45, 0x0010, "Media Vision Jazz16" },

    /*========================================================================
     * TERRATEC (TER) - Sound Cards
     *========================================================================*/
    { 0x00005445, 0x1688, "TerraTec EWS88MT" },
    { 0x00005445, 0x1689, "TerraTec EWS88D" },

    /*========================================================================
     * TURTLE BEACH (TBS) - Sound Cards
     *========================================================================*/
    { 0x00005442, 0x1600, "Turtle Beach Tropez" },
    { 0x00005442, 0x1601, "Turtle Beach Tropez Plus" },
    { 0x00005442, 0x1610, "Turtle Beach Maui" },
    { 0x00005442, 0x1620, "Turtle Beach Rio" },
    { 0x00005442, 0x1630, "Turtle Beach Fiji" },
    { 0x00005442, 0x1640, "Turtle Beach Pinnacle" },
    { 0x00005442, 0x1650, "Turtle Beach Malibu" },

    /*========================================================================
     * 3COM (TCM) - Network Cards
     *========================================================================*/
    { 0x00006D50, 0x5090, "3Com EtherLink III 3C509B-TP" },
    { 0x00006D50, 0x5091, "3Com EtherLink III 3C509B-Combo" },
    { 0x00006D50, 0x5094, "3Com EtherLink III 3C509B-Coax" },
    { 0x00006D50, 0x5095, "3Com EtherLink III 3C509B PnP" },
    { 0x00006D50, 0x5098, "3Com EtherLink III ISA" },
    { 0x00006D50, 0x9050, "3Com Fast EtherLink 3C515" },

    /*========================================================================
     * INTEL (INT) - Network Cards
     *========================================================================*/
    { 0x0000494E, 0x0105, "Intel EtherExpress PRO/10" },
    { 0x0000494E, 0x1000, "Intel EtherExpress PRO/100" },
    { 0x0000494E, 0x1001, "Intel EtherExpress PRO/100+" },

    /*========================================================================
     * REALTEK (RTL) - Network Cards
     *========================================================================*/
    { 0x00004A8C, 0x8019, "Realtek RTL8019AS NE2000" },
    { 0x00004A8C, 0x8029, "Realtek RTL8029 NE2000" },

    /*========================================================================
     * SMC (SMC) - Network Cards
     *========================================================================*/
    { 0x00004D53, 0x8416, "SMC EtherEZ 8416 Ultra" },
    { 0x00004D53, 0x1660, "SMC Ultra16 Elite" },
    { 0x00004D53, 0x8013, "SMC EtherCard Elite16" },

    /*========================================================================
     * DEC (DEC) - Network Cards
     *========================================================================*/
    { 0x00004445, 0x4250, "DEC EtherWORKS Turbo PnP" },

    /*========================================================================
     * ACCTON (ACC) - Network Cards
     *========================================================================*/
    { 0x00004143, 0x0200, "Accton EN1660 PnP" },
    { 0x00004143, 0x0210, "Accton EN2209 PnP" },

    /*========================================================================
     * MODEMS - Various Vendors
     *========================================================================*/
    /* US Robotics */
    { 0x00005553, 0x0011, "USR Sportster 14.4 PnP" },
    { 0x00005553, 0x0013, "USR Sportster 28.8 PnP" },
    { 0x00005553, 0x0014, "USR Sportster 33.6 PnP" },
    { 0x00005553, 0x0016, "USR Courier V.Everything" },
    { 0x00005553, 0x0440, "USR Sportster Flash" },
    { 0x00005553, 0x0450, "USR Sportster Vi" },
    { 0x00005553, 0x1440, "USR Sportster 56K" },

    /* Hayes */
    { 0x00004855, 0x0001, "Hayes Optima 14.4 PnP" },
    { 0x00004855, 0x0011, "Hayes Optima 28.8 PnP" },
    { 0x00004855, 0x0144, "Hayes Accura 144 PnP" },
    { 0x00004855, 0x0288, "Hayes Accura 288 PnP" },
    { 0x00004855, 0x0336, "Hayes Accura 336 PnP" },

    /* Practical Peripherals */
    { 0x00005050, 0x1414, "Practical PM14400FX PnP" },
    { 0x00005050, 0x2828, "Practical PM28800 PnP" },
    { 0x00005050, 0x3636, "Practical PM33600 PnP" },

    /* Zoom */
    { 0x00005A4F, 0x1414, "Zoom 14.4 PnP" },
    { 0x00005A4F, 0x2828, "Zoom 28.8 PnP" },
    { 0x00005A4F, 0x3636, "Zoom 33.6 PnP" },
    { 0x00005A4F, 0x5600, "Zoom 56K PnP" },

    /* Motorola */
    { 0x00004D4F, 0x1500, "Motorola ModemSURFR 56K" },
    { 0x00004D4F, 0x2888, "Motorola Montana 28.8" },

    /* Rockwell/Conexant */
    { 0x00005243, 0x1400, "Rockwell 14.4 PnP" },
    { 0x00005243, 0x2800, "Rockwell 28.8 PnP" },
    { 0x00005243, 0x3300, "Rockwell 33.6 PnP" },
    { 0x00005243, 0x5600, "Rockwell 56K PnP" },

    /* Generic Modems */
    { 0x00005047, 0x1400, "Generic 14.4 Modem PnP" },
    { 0x00005047, 0x2800, "Generic 28.8 Modem PnP" },
    { 0x00005047, 0x3300, "Generic 33.6 Modem PnP" },

    /*========================================================================
     * SERIAL/PARALLEL/IDE - System Devices
     *========================================================================*/
    /* National Semiconductor Serial */
    { 0x00004E53, 0x0100, "NSC 16550 UART" },
    { 0x00004E53, 0x0101, "NSC 16650 UART" },
    { 0x00004E53, 0x0102, "NSC 16750 UART" },

    /* Standard System Devices */
    { 0x00004E50, 0x0303, "PnP System COM Port" },
    { 0x00004E50, 0x0400, "PnP System LPT Port" },
    { 0x00004E50, 0x0401, "PnP ECP Printer Port" },
    { 0x00004E50, 0x0501, "PnP Joystick" },

    /*========================================================================
     * SCSI CONTROLLERS
     *========================================================================*/
    /* Adaptec */
    { 0x00004144, 0x1510, "Adaptec AHA-1510 PnP" },
    { 0x00004144, 0x1520, "Adaptec AHA-1520 PnP" },
    { 0x00004144, 0x1542, "Adaptec AHA-1542CP PnP" },

    /* Future Domain */
    { 0x00004644, 0x1600, "Future Domain TMC-1600 PnP" },
    { 0x00004644, 0x1610, "Future Domain TMC-1610 PnP" },
    { 0x00004644, 0x1800, "Future Domain TMC-1800 PnP" },

    /*========================================================================
     * CONTROLLERS - IDE/Multi-I/O
     *========================================================================*/
    { 0x00005543, 0x0091, "UMC 8672 IDE" },
    { 0x00005543, 0x0092, "UMC 8672A IDE" },

    { 0, 0, NULL }  /* Terminator */
};

/*----------------------------------------------------------------------------
 * Low-level ISA PnP I/O
 *----------------------------------------------------------------------------*/

/* Write to ISA PnP ADDRESS port */
static void isapnp_write_address(unsigned char reg)
{
    outp(ISAPNP_ADDRESS, reg);
}

/* Write to ISA PnP WRITE_DATA port */
static void isapnp_write_data(unsigned char data)
{
    outp(ISAPNP_WRITE_DATA, data);
}

/* Read from ISA PnP READ_DATA port */
static unsigned char isapnp_read_data(void)
{
    return inp(g_isapnp_read_port);
}

/* Write a register value */
static void isapnp_write_reg(unsigned char reg, unsigned char data)
{
    isapnp_write_address(reg);
    isapnp_write_data(data);
}

/* Read a register value */
static unsigned char isapnp_read_reg(unsigned char reg)
{
    isapnp_write_address(reg);
    return isapnp_read_data();
}

/*----------------------------------------------------------------------------
 * ISA PnP Protocol Functions
 *----------------------------------------------------------------------------*/

/* Send the initiation key to enter configuration mode */
static void isapnp_send_key(void)
{
    int i;

    /* Send two zeros first to reset state machine */
    isapnp_write_address(0x00);
    isapnp_write_address(0x00);

    /* Send the 32-byte initiation key */
    for (i = 0; i < 32; i++) {
        isapnp_write_address(isapnp_initiation_key[i]);
    }
}

/* Set the Read Data port address */
static void isapnp_set_read_port(unsigned int port)
{
    g_isapnp_read_port = port;
    isapnp_write_reg(ISAPNP_REG_SET_RD_DATA, (port >> 2) & 0xFF);
}

/* Wake a card by CSN (0 = all cards in isolation state) */
static void isapnp_wake(unsigned char csn)
{
    isapnp_write_reg(ISAPNP_REG_WAKE, csn);
}

/* Select a logical device on the current card */
static void isapnp_select_logdev(unsigned char logdev)
{
    isapnp_write_reg(ISAPNP_REG_LOGDEV, logdev);
}

/* Return all cards to Wait for Key state */
static void isapnp_return_to_wait(void)
{
    isapnp_write_reg(ISAPNP_REG_CFG_CTRL, ISAPNP_CC_WAIT_KEY);
}

/* Reset all CSNs to 0 */
static void isapnp_reset_csn(void)
{
    isapnp_write_reg(ISAPNP_REG_CFG_CTRL, ISAPNP_CC_RESET);
}

/*----------------------------------------------------------------------------
 * Card Isolation Protocol (assigns CSNs)
 *----------------------------------------------------------------------------*/

/* Read one bit during isolation */
static unsigned char isapnp_read_isolation_bit(void)
{
    unsigned char b1, b2;

    isapnp_write_address(ISAPNP_REG_SERIAL_ISO);
    /* Wait ~1ms between reads */
    {
        volatile int i;
        for (i = 0; i < 1000; i++);
    }
    b1 = isapnp_read_data();
    {
        volatile int i;
        for (i = 0; i < 1000; i++);
    }
    b2 = isapnp_read_data();

    if (b1 == 0x55 && b2 == 0xAA) {
        return 1;
    } else if (b1 == 0xFF && b2 == 0xFF) {
        return 0xFF;  /* No card responding */
    } else {
        return 0;
    }
}

/* Read 8 bits during isolation */
static unsigned char isapnp_read_isolation_byte(void)
{
    unsigned char byte = 0;
    int i;

    for (i = 0; i < 8; i++) {
        unsigned char bit = isapnp_read_isolation_bit();
        if (bit == 0xFF) return 0xFF;  /* No card */
        byte |= (bit << i);
    }
    return byte;
}

/* Isolate cards and assign CSNs
 * Returns number of cards found */
static int isapnp_isolate(void)
{
    int cards = 0;
    unsigned char csn = 1;
    unsigned char vendor[4];
    unsigned char serial[4];
    unsigned char checksum;
    int i;

    /* Reset all CSNs first */
    isapnp_reset_csn();

    /* Wait a bit */
    {
        volatile int j;
        for (j = 0; j < 5000; j++);
    }

    /* Enter isolation state */
    isapnp_wake(0);

    /* Set read port */
    isapnp_set_read_port(g_isapnp_read_port);

    while (csn <= ISAPNP_MAX_CARDS) {
        /* Read 72 bits: 32-bit vendor ID + 32-bit serial + 8-bit checksum */
        for (i = 0; i < 4; i++) {
            vendor[i] = isapnp_read_isolation_byte();
            if (vendor[i] == 0xFF) break;
        }
        if (i < 4) break;  /* No more cards */

        for (i = 0; i < 4; i++) {
            serial[i] = isapnp_read_isolation_byte();
        }
        checksum = isapnp_read_isolation_byte();

        /* Assign CSN to this card */
        isapnp_write_reg(ISAPNP_REG_CSN, csn);

        cards++;
        csn++;

        /* Wake next card for isolation */
        isapnp_wake(0);
    }

    g_isapnp_card_count = cards;
    return cards;
}

/*----------------------------------------------------------------------------
 * Resource Data Parsing
 *----------------------------------------------------------------------------*/

/* Wait for resource data to be ready */
static int isapnp_wait_status(void)
{
    int timeout = 1000;
    while (timeout--) {
        if (isapnp_read_reg(ISAPNP_REG_STATUS) & 0x01) {
            return 1;  /* Data ready */
        }
    }
    return 0;  /* Timeout */
}

/* Read resource data byte */
static unsigned char isapnp_read_resource_byte(void)
{
    if (!isapnp_wait_status()) return 0xFF;
    return isapnp_read_reg(ISAPNP_REG_RES_DATA);
}

/* Parse resource descriptors for a card */
static void isapnp_parse_resources(unsigned char csn, slot_config_t *cfg)
{
    unsigned char tag;
    unsigned int len;       /* 16-bit: large-resource length is 2 bytes */
    int done = 0;
    int logdev = 0;

    /* Select the card */
    isapnp_wake(csn);

    /* Read resource data stream */
    while (!done && logdev < ISAPNP_MAX_LOGDEV) {
        tag = isapnp_read_resource_byte();
        if (tag == 0xFF) break;

        if (tag & 0x80) {
            /* Large resource descriptor */
            len = isapnp_read_resource_byte();
            len |= isapnp_read_resource_byte() << 8;

            switch (tag) {
                case 0x81:  /* Memory range descriptor */
                    if (cfg->mem_count < MAX_SLOT_MEMRANGES) {
                        unsigned char info = isapnp_read_resource_byte();
                        unsigned long base;
                        base = isapnp_read_resource_byte();
                        base |= (unsigned long)isapnp_read_resource_byte() << 8;
                        base <<= 8;  /* Memory base is bits 23-8 */
                        cfg->mem_ranges[cfg->mem_count].base = base;
                        cfg->mem_ranges[cfg->mem_count].is_rom = (info & 0x40) ? 1 : 0;
                        cfg->mem_count++;
                    }
                    break;

                case 0x82:  /* ANSI identifier string */
                    /* Skip vendor name string */
                    while (len--) isapnp_read_resource_byte();
                    break;

                default:
                    /* Skip unknown large descriptors */
                    while (len--) isapnp_read_resource_byte();
                    break;
            }
        } else {
            /* Small resource descriptor */
            len = tag & 0x07;
            tag = (tag >> 3) & 0x0F;

            switch (tag) {
                case 0x01:  /* PnP version */
                    isapnp_read_resource_byte();  /* Version */
                    isapnp_read_resource_byte();  /* Vendor version */
                    break;

                case 0x02:  /* Logical device ID */
                    {
                        unsigned long id = 0;
                        id = isapnp_read_resource_byte();
                        id |= (unsigned long)isapnp_read_resource_byte() << 8;
                        id |= (unsigned long)isapnp_read_resource_byte() << 16;
                        id |= (unsigned long)isapnp_read_resource_byte() << 24;
                        cfg->isapnp_vendor = id;
                    }
                    break;

                case 0x04:  /* IRQ format */
                    if (cfg->irq_count < MAX_SLOT_IRQS) {
                        unsigned int mask;
                        int irq;
                        mask = isapnp_read_resource_byte();
                        mask |= isapnp_read_resource_byte() << 8;
                        /* Find first available IRQ from mask */
                        for (irq = 0; irq < 16; irq++) {
                            if (mask & (1 << irq)) {
                                cfg->irqs[cfg->irq_count].irq = irq;
                                cfg->irqs[cfg->irq_count].level_triggered = 0;
                                cfg->irq_count++;
                                break;
                            }
                        }
                        if (len > 2) isapnp_read_resource_byte();  /* IRQ info */
                    }
                    break;

                case 0x05:  /* DMA format */
                    if (cfg->dma_count < MAX_SLOT_DMAS) {
                        unsigned char mask;
                        int dma;
                        mask = isapnp_read_resource_byte();
                        for (dma = 0; dma < 8; dma++) {
                            if (mask & (1 << dma)) {
                                cfg->dmas[cfg->dma_count].channel = dma;
                                cfg->dma_count++;
                                break;
                            }
                        }
                        if (len > 1) isapnp_read_resource_byte();  /* DMA info */
                    }
                    break;

                case 0x08:  /* I/O port descriptor */
                    if (cfg->ioport_count < MAX_SLOT_IOPORTS) {
                        unsigned char info;
                        unsigned int min, max, size;
                        info = isapnp_read_resource_byte();
                        min = isapnp_read_resource_byte();
                        min |= isapnp_read_resource_byte() << 8;
                        max = isapnp_read_resource_byte();
                        max |= isapnp_read_resource_byte() << 8;
                        isapnp_read_resource_byte();  /* Alignment */
                        size = isapnp_read_resource_byte();
                        cfg->ioports[cfg->ioport_count].base = min;
                        cfg->ioports[cfg->ioport_count].size = size;
                        cfg->ioport_count++;
                        (void)info;  /* Suppress unused warning */
                    }
                    break;

                case 0x09:  /* Fixed I/O port */
                    if (cfg->ioport_count < MAX_SLOT_IOPORTS) {
                        unsigned int base;
                        unsigned int size;
                        base = isapnp_read_resource_byte();
                        base |= (isapnp_read_resource_byte() & 0x03) << 8;
                        size = isapnp_read_resource_byte();
                        cfg->ioports[cfg->ioport_count].base = base;
                        cfg->ioports[cfg->ioport_count].size = size;
                        cfg->ioport_count++;
                    }
                    break;

                case 0x0F:  /* End tag */
                    done = 1;
                    break;

                default:
                    /* Skip unknown small descriptors */
                    while (len--) isapnp_read_resource_byte();
                    break;
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * Public API Functions
 *----------------------------------------------------------------------------*/

/* Check if ISA PnP cards are present (quick test) */
int isapnp_detect(void)
{
    /* Try to send initiation key and check for response */
    _disable();
    isapnp_send_key();
    isapnp_wake(0);
    isapnp_set_read_port(ISAPNP_READ_DATA);

    /* Try to read isolation bit */
    {
        unsigned char b = isapnp_read_isolation_bit();
        isapnp_return_to_wait();
        _enable();

        /* If we got 0x55/0xAA or 0x00/0x00, cards are present */
        return (b != 0xFF) ? 1 : 0;
    }
}

/* Initialize ISA PnP subsystem */
int isapnp_init(void)
{
    int cards;

    g_isapnp_card_count = 0;
    g_isapnp_device_count = 0;
    g_isapnp_read_port = ISAPNP_READ_DATA;

    _disable();

    /* Send initiation key (twice for reliability) */
    isapnp_send_key();
    isapnp_send_key();

    /* Isolate cards and assign CSNs */
    cards = isapnp_isolate();

    /* Return cards to Wait for Key state */
    isapnp_return_to_wait();

    _enable();

    return cards;
}

/* Enumerate ISA PnP devices */
int isapnp_enum_devices(void)
{
    int total_devices = 0;
    unsigned char csn;

    if (g_isapnp_card_count == 0) return 0;

    _disable();
    isapnp_send_key();

    for (csn = 1; csn <= g_isapnp_card_count; csn++) {
        slot_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));

        isapnp_wake(csn);
        /* Count logical devices by reading device IDs */
        /* For now, assume 1 logical device per card */
        total_devices++;
    }

    isapnp_return_to_wait();
    _enable();

    g_isapnp_device_count = total_devices;
    return total_devices;
}

/* Get card count */
unsigned char isapnp_get_card_count(void)
{
    return g_isapnp_card_count;
}

/* Get device count */
unsigned char isapnp_get_device_count(void)
{
    return g_isapnp_device_count;
}

/* Read ISA PnP card information */
int isapnp_read_card(unsigned char csn, slot_config_t *cfg)
{
    const char *name;
    int i;

    if (csn < 1 || csn > g_isapnp_card_count) return BUSCFG_ERR_SLOT;

    memset(cfg, 0, sizeof(slot_config_t));
    cfg->slot = csn;
    cfg->bus_type = BUS_ISAPNP;
    cfg->isapnp_csn = csn;
    cfg->enabled = 1;

    _disable();
    isapnp_send_key();

    /* Read card identifier */
    isapnp_wake(csn);

    /* Read vendor/product ID from card */
    cfg->vendor_id = isapnp_read_reg(0x00);
    cfg->vendor_id |= (unsigned int)isapnp_read_reg(0x01) << 8;
    cfg->device_id = isapnp_read_reg(0x02);
    cfg->device_id |= (unsigned int)isapnp_read_reg(0x03) << 8;

    /* Parse resource descriptors */
    isapnp_parse_resources(csn, cfg);

    isapnp_return_to_wait();
    _enable();

    /* Look up name in database. The card reports a 32-bit logical-device ID
     * (low 16 bits = compressed vendor, high 16 bits = product). The table
     * stores the vendor in vendor_id (low word, high word 0) and the product
     * separately in product_id, so we must match BOTH halves. The old code
     * compared the table's vendor-only value against the full 32-bit ID, which
     * never matched, leaving the entire database unreachable. */
    name = NULL;
    for (i = 0; g_isapnp_cards[i].name != NULL; i++) {
        if (g_isapnp_cards[i].vendor_id == (cfg->isapnp_vendor & 0xFFFFUL) &&
            g_isapnp_cards[i].product_id ==
                (unsigned int)((cfg->isapnp_vendor >> 16) & 0xFFFFUL)) {
            name = g_isapnp_cards[i].name;
            break;
        }
    }

    if (name) {
        strncpy(cfg->name, name, sizeof(cfg->name) - 1);
    } else {
        char vendor_str[4];
        isapnp_decode_vendor(cfg->vendor_id, vendor_str);
        sprintf(cfg->name, "ISA PnP %s%04X", vendor_str, cfg->device_id);
    }

    return BUSCFG_OK;
}

/* Activate/deactivate a logical device */
int isapnp_activate(unsigned char csn, unsigned char logdev, unsigned char activate)
{
    if (csn < 1 || csn > g_isapnp_card_count) return BUSCFG_ERR_SLOT;

    _disable();
    isapnp_send_key();
    isapnp_wake(csn);
    isapnp_select_logdev(logdev);
    isapnp_write_reg(ISAPNP_REG_ACTIVATE, activate ? 0x01 : 0x00);
    isapnp_return_to_wait();
    _enable();

    return BUSCFG_OK;
}

/* Set I/O port for a logical device */
int isapnp_set_io(unsigned char csn, unsigned char logdev, unsigned char idx, unsigned int base)
{
    unsigned char reg_hi = ISAPNP_REG_IO_BASE_HI + (idx * 2);
    unsigned char reg_lo = ISAPNP_REG_IO_BASE_LO + (idx * 2);

    if (csn < 1 || csn > g_isapnp_card_count) return BUSCFG_ERR_SLOT;
    if (idx >= MAX_SLOT_IOPORTS) return BUSCFG_ERR_SLOT;

    _disable();
    isapnp_send_key();
    isapnp_wake(csn);
    isapnp_select_logdev(logdev);
    isapnp_write_reg(reg_hi, (base >> 8) & 0xFF);
    isapnp_write_reg(reg_lo, base & 0xFF);
    isapnp_return_to_wait();
    _enable();

    return BUSCFG_OK;
}

/* Set IRQ for a logical device */
int isapnp_set_irq(unsigned char csn, unsigned char logdev, unsigned char idx, unsigned char irq)
{
    unsigned char reg = ISAPNP_REG_IRQ_NO + (idx * 2);

    if (csn < 1 || csn > g_isapnp_card_count) return BUSCFG_ERR_SLOT;
    if (idx >= MAX_SLOT_IRQS) return BUSCFG_ERR_SLOT;

    _disable();
    isapnp_send_key();
    isapnp_wake(csn);
    isapnp_select_logdev(logdev);
    isapnp_write_reg(reg, irq);
    isapnp_return_to_wait();
    _enable();

    return BUSCFG_OK;
}

/* Set DMA for a logical device */
int isapnp_set_dma(unsigned char csn, unsigned char logdev, unsigned char idx, unsigned char dma)
{
    unsigned char reg = ISAPNP_REG_DMA + idx;

    if (csn < 1 || csn > g_isapnp_card_count) return BUSCFG_ERR_SLOT;
    if (idx >= MAX_SLOT_DMAS) return BUSCFG_ERR_SLOT;

    _disable();
    isapnp_send_key();
    isapnp_wake(csn);
    isapnp_select_logdev(logdev);
    isapnp_write_reg(reg, dma);
    isapnp_return_to_wait();
    _enable();

    return BUSCFG_OK;
}

/* Set memory base for a logical device */
int isapnp_set_mem(unsigned char csn, unsigned char logdev, unsigned char idx, unsigned long base)
{
    unsigned char reg_hi = ISAPNP_REG_MEM_BASE_HI + (idx * 8);
    unsigned char reg_lo = ISAPNP_REG_MEM_BASE_LO + (idx * 8);

    if (csn < 1 || csn > g_isapnp_card_count) return BUSCFG_ERR_SLOT;
    if (idx >= MAX_SLOT_MEMRANGES) return BUSCFG_ERR_SLOT;

    _disable();
    isapnp_send_key();
    isapnp_wake(csn);
    isapnp_select_logdev(logdev);
    isapnp_write_reg(reg_hi, (base >> 16) & 0xFF);
    isapnp_write_reg(reg_lo, (base >> 8) & 0xFF);
    isapnp_return_to_wait();
    _enable();

    return BUSCFG_OK;
}

/* Format ISA PnP ID as string */
void isapnp_format_id(unsigned long vendor_id, unsigned int product_id, char *id_str)
{
    char vendor_str[4];
    isapnp_decode_vendor(vendor_id, vendor_str);
    sprintf(id_str, "%s%04X", vendor_str, product_id);
}

/* Decode vendor ID to 3-character string (same as EISA) */
void isapnp_decode_vendor(unsigned long vendor_id, char *vendor_str)
{
    /* ISA PnP uses the same compressed vendor format as EISA */
    unsigned char b0 = vendor_id & 0xFF;
    unsigned char b1 = (vendor_id >> 8) & 0xFF;

    vendor_str[0] = ((b0 >> 2) & 0x1F) + '@';
    vendor_str[1] = (((b0 & 0x03) << 3) | ((b1 >> 5) & 0x07)) + '@';
    vendor_str[2] = (b1 & 0x1F) + '@';
    vendor_str[3] = '\0';
}
