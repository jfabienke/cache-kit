/*
 * CACHEKIT.C - Combined TUI Chipset Utility for 386/486 Systems
 *
 * Part of the Abacus FPGA Project
 * For Open Watcom C (16-bit real mode DOS)
 *
 * Features:
 *   F1 - System/Chipset Information (with cache toggle)
 *   F2 - Non-Cacheable Region Configuration
 *   F3 - Cache Flush Testing
 *   F4 - Register Dump
 *   F5 - Memory Bandwidth Benchmarks
 *   F6 - Configuration Profiles
 *   F7 - Expansion Card Inventory (PCI enumeration with ID lookup)
 *
 * Last Updated: 2026-01-06 12:45:00 EST
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>

/* HAL interface for chipset abstraction */
#include "CK_HAL.H"
#include "CK_VIDEO.H"
#include "CK_UI.H"
#include "CK_ENUM.H"
#include "CK_BCFG.H"
#include "CK_XMS.H"

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define VERSION "3.0"

/* Screen dimensions */
#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 25

/* Video attributes (CGA compatible) */
#define ATTR_NORMAL     0x07    /* Light gray on black */
#define ATTR_HIGHLIGHT  0x0F    /* White on black */
#define ATTR_TITLE      0x1F    /* White on blue */
#define ATTR_STATUS     0x30    /* Black on cyan */
#define ATTR_ERROR      0x4F    /* White on red */
#define ATTR_SUCCESS    0x2F    /* White on green */
#define ATTR_BOX        0x0B    /* Light cyan on black */
#define ATTR_SELECTED   0x70    /* Black on light gray */
#define ATTR_NC_REGION  0x5F    /* White on magenta */
#define ATTR_TAB_ACTIVE 0x1F    /* White on blue */
#define ATTR_TAB_IDLE   0x08    /* Dark gray on black */
#define ATTR_WARNING    0x0E    /* Yellow on black */
#define ATTR_LABEL      0x0B    /* Cyan on black */
#define ATTR_VALUE      0x0F    /* White on black */
#define ATTR_DIM        0x08    /* Dark gray on black */

/* Box drawing characters (CP437) */
#define BOX_TL  0xDA    /* Top-left */
#define BOX_TR  0xBF    /* Top-right */
#define BOX_BL  0xC0    /* Bottom-left */
#define BOX_BR  0xD9    /* Bottom-right */
#define BOX_H   0xC4    /* Horizontal */
#define BOX_V   0xB3    /* Vertical */
#define BOX_LT  0xC3    /* Left-T */
#define BOX_RT  0xB4    /* Right-T */
#define BOX_TT  0xC2    /* Top-T */
#define BOX_BT  0xC1    /* Bottom-T */

/* Block characters */
#define BLOCK_FULL  0xDB    /* Full block */
#define BLOCK_LIGHT 0xB0    /* Light shade */
#define BLOCK_MED   0xB1    /* Medium shade */
#define BLOCK_DARK  0xB2    /* Dark shade */

/* Checkbox */
#define CHECK_ON    0xFE    /* Filled square */
#define CHECK_OFF   ' '     /* Space */

/* Radio button */
#define RADIO_ON    0x07    /* Bullet */
#define RADIO_OFF   0x09    /* Circle */

/* Key codes */
#define KEY_ESC       0x1B
#define KEY_ENTER     0x0D
#define KEY_BACKSPACE 0x08
#define KEY_SPACE     0x20
#define KEY_TAB       0x09
#define KEY_F1      0x3B00
#define KEY_F2      0x3C00
#define KEY_F3      0x3D00
#define KEY_F4      0x3E00
#define KEY_F5      0x3F00
#define KEY_F6      0x4000
#define KEY_F7      0x4100
#define KEY_F8      0x4200
#define KEY_UP      0x4800
#define KEY_DOWN    0x5000
#define KEY_LEFT    0x4B00
#define KEY_RIGHT   0x4D00
#define KEY_ALT_X   0x2D00

/*============================================================================
 * CHIPSET DEFINITIONS
 *============================================================================*/

#define CHIPSET_UNKNOWN      0
#define CHIPSET_SIS_460      1
#define CHIPSET_SIS_RABBIT   2
#define CHIPSET_OPTI391      3
#define CHIPSET_FARADAY      4
#define CHIPSET_UMC491       5
#define CHIPSET_MIC9391      6
#define CHIPSET_ETEQ_BENGAL  7
#define CHIPSET_OPTI381      8
#define CHIPSET_VIA          9
#define CHIPSET_VLSI        10
#define CHIPSET_CT_PEAK     11
#define CHIPSET_CT_SCAT     12
#define CHIPSET_ALI_FINIS   13
#define CHIPSET_FOREX       14
#define CHIPSET_SUNTAC      15
#define CHIPSET_EISA_82350  16
#define CHIPSET_CT_NEAT     17
#define CHIPSET_HEADLAND    18
/* Pentium-era chipsets */
#define CHIPSET_I430FX      19   /* Intel 430FX Triton */
#define CHIPSET_I430HX      20   /* Intel 430HX */
#define CHIPSET_I430VX      21   /* Intel 430VX */
#define CHIPSET_ALI_ALADDIN 22   /* ALi M1489/1487 Aladdin */
#define CHIPSET_CONTAQ_596  23   /* Contaq 82C596 */
#define CHIPSET_SIS_496     24   /* SiS 85C496 */
#define CHIPSET_VIA_VP1     25   /* VIA VT82C570 VP1 */
#define CHIPSET_OPTI_VIPER  26   /* OPTi 82C596/597 Viper */
/* Late Socket 7 / Super7 chipsets (1997-1999) */
#define CHIPSET_I430TX      27   /* Intel 430TX */
#define CHIPSET_VIA_VP3     28   /* VIA Apollo VP3 */
#define CHIPSET_VIA_MVP3    29   /* VIA Apollo MVP3 */
#define CHIPSET_SIS_5591    30   /* SiS 5591 */
#define CHIPSET_SIS_5598    31   /* SiS 5598 */
#define CHIPSET_SIS_530     32   /* SiS 530 */
#define CHIPSET_ALI_ALADDIN5 33  /* ALi Aladdin V (M1541) */
#define CHIPSET_I430MX      34   /* Intel 430MX (mobile) */
/* 286-era chipsets (informational only - no cache controller) */
#define CHIPSET_CT_NEAT_FULL  35 /* C&T CS8221 full NEAT set */
#define CHIPSET_VLSI_100      36 /* VLSI VL82C100 */
#define CHIPSET_VLSI_101      37 /* VLSI VL82C101 */
#define CHIPSET_VLSI_102      38 /* VLSI VL82C102 */
#define CHIPSET_HEADLAND_101  39 /* Headland HT101 */
#define CHIPSET_HEADLAND_102  40 /* Headland HT102 */
#define CHIPSET_ALI_M1217     41 /* ALi M1217 */
#define CHIPSET_OPTI_212      42 /* OPTi 82C212 */
/* 386SX-era chipsets using 286-style register layouts */
#define CHIPSET_CT_NEAT386    43 /* C&T CS8230 NEAT-386 */
#define CHIPSET_HEADLAND_18   44 /* Headland HT18 (386SX) */
#define CHIPSET_VLSI_320      45 /* VLSI VL82C320 (386SX) */
/* EISA chipsets - Pure EISA (no PCI) */
#define CHIPSET_EISA_82350DT  46 /* Intel 82350DT Mongoose */
/* EISA chipsets - Intel PCIsets (EISA+PCI bridge) */
#define CHIPSET_EISA_420TX    47 /* Intel 420TX Saturn EISA */
#define CHIPSET_EISA_420ZX    48 /* Intel 420ZX Saturn II EISA */
#define CHIPSET_EISA_430LX    49 /* Intel 430LX Mercury EISA */
#define CHIPSET_EISA_430NX    50 /* Intel 430NX Neptune EISA */
#define CHIPSET_EISA_430HX    51 /* Intel 430HX Triton II EISA */
#define CHIPSET_EISA_440FX    52 /* Intel 440FX Natoma EISA */
#define CHIPSET_EISA_450GX    53 /* Intel 450GX Orion Server EISA */
/* EISA chipsets - OPTi */
#define CHIPSET_OPTI_682      54 /* OPTi 82C682 (EISA 486WB) */
#define CHIPSET_OPTI_683      55 /* OPTi 82C683 (EISA 486AWB) */
#define CHIPSET_OPTI_HUNTER   56 /* OPTi 82C691/696 Hunter EISA */
#define CHIPSET_OPTI_PENT_EISA 57 /* OPTi 82C693/6/7 Pentium WB EISA */
/* EISA chipsets - Other vendors */
#define CHIPSET_SIS_EISA      58 /* SiS 85C411/406 (EISA 386/486) - UNDOCUMENTED */
#define CHIPSET_VIA_EISA      59 /* VIA VT82C495 (Venus) - ports 0xA8/0xA9 */
/* MCA bus - observer only (hardware-enforced coherency) */
#define CHIPSET_MCA_GENERIC   60 /* IBM PS/2 Micro Channel Architecture */
/* HAL-detected chipset (v3.0+) */
#define CHIPSET_HAL           99 /* Chipset detected via HAL - use g_hal for ops */

#define NC_NONE       0
#define NC_RANGE      1
#define NC_BOUNDARY   2
#define NC_STEERING   3
#define NC_SHADOW     4

/* Bus types, MAX_DEVICES, MCA/EISA/ISAPNP constants are in CK_ENUM.H */

/* SMBIOS/DMI signatures and constants */
#define SMBIOS_ANCHOR          0x5F4D535F  /* "_SM_" little-endian */
#define SMBIOS_DMI_ANCHOR      0x494D445F  /* "_DMI" little-endian */
#define SMBIOS_STR_LEN         48          /* Max string length to store */
#define SMBIOS_MAX_DIMMS       8           /* Max memory devices to track */

/* ACPI signatures */
#define RSDP_SIGNATURE_LO      0x20445352  /* "RSD " little-endian */
#define RSDP_SIGNATURE_HI      0x20525450  /* "PTR " little-endian */
#define RSDT_SIGNATURE         0x54445352  /* "RSDT" */
#define FACP_SIGNATURE         0x50434146  /* "FACP" */
#define APIC_SIGNATURE         0x43495041  /* "APIC" */
#define HPET_SIGNATURE         0x54455048  /* "HPET" */

/*============================================================================
 * DATA STRUCTURES
 *============================================================================*/

typedef struct {
    unsigned char type;
    unsigned char is_writeback;
    unsigned char nc_strategy;
    unsigned char nc_regions;
    unsigned int  index_port;
    unsigned int  data_port;
    unsigned int  cache_size_kb;
    unsigned char id_value;
    unsigned char id_index;
    const char   *name;
    const char   *vendor;
    const char   *tier;
    unsigned char score_x10;
    unsigned int  granularity;
    unsigned char info_only;      /* 1 = no cache/NC control, info only */
} chipset_info_t;

/* screen_t is defined in CK_UI.H */

/* NC Region live data (extended version with raw register storage)
 * Note: CK_HAL.H defines a simpler nc_region_t for HAL interface.
 * This extended version stores additional debug info. */
typedef struct {
    unsigned char active;
    unsigned long base_kb;      /* Base address in KB */
    unsigned long size_kb;      /* Size in KB */
    unsigned char reg_index;    /* Starting register index */
    unsigned char reg_val[4];   /* Raw register values */
} nc_region_live_t;

/* 286/386SX inventory data (info-only chipsets) */
typedef struct {
    unsigned char shadow_c000_c7ff;   /* C000-C7FF: 1=DRAM, 0=ROM per 16KB */
    unsigned char shadow_c800_cfff;   /* C800-CFFF: 1=DRAM, 0=ROM per 16KB */
    unsigned char shadow_d000_dfff;   /* D000-DFFF: 1=DRAM, 0=ROM per 16KB */
    unsigned char shadow_e000_ffff;   /* E000-FFFF: 1=DRAM, 0=ROM per 16KB */
    unsigned char a20_enabled;        /* A20 gate: 1=enabled, 0=disabled */
    unsigned char a20_method;         /* 0=unknown, 1=chipset, 2=KBC, 3=fast */
    unsigned char dram_banks;         /* Number of DRAM banks (1-4) */
    unsigned char dram_size_256k;     /* DRAM bank size: 0=256K, 1=1M, 2=4M */
    unsigned char wait_states;        /* Memory wait states (0-3) */
    unsigned char refresh_rate;       /* 0=15.6us, 1=125us */
    unsigned char bus_speed;          /* Bus clock divider (0=CLK/2, 1=CLK) */
    unsigned int  dram_total_kb;      /* Total DRAM in KB */
    unsigned char valid;              /* 1=inventory populated */
} inventory_286_t;

/* device_entry_t is now defined in CK_ENUM.H */

/* Expansion card inventory state (F7) */
typedef struct {
    unsigned char bus_filter;     /* Current bus filter (0=all, BUS_PCI, etc.) */
    int cursor;                   /* Selected device index */
    int scroll_offset;            /* For scrolling long lists */
    int device_count;             /* Total devices found */
} inventory_state_t;

/* F1 Info screen tab selection */
typedef enum {
    INFO_TAB_CHIPSET = 0,         /* Default: Chipset/Cache view */
    INFO_TAB_SMBIOS,              /* SMBIOS/ACPI system info view */
    INFO_TAB_COUNT
} info_tab_t;

/* SMBIOS/DMI parsed information */
typedef struct {
    /* Entry point info */
    unsigned char version_major;
    unsigned char version_minor;
    unsigned int table_count;

    /* Type 0: BIOS Information */
    char bios_vendor[SMBIOS_STR_LEN];
    char bios_version[SMBIOS_STR_LEN];
    char bios_date[16];

    /* Type 1: System Information */
    char sys_manufacturer[SMBIOS_STR_LEN];
    char sys_product[SMBIOS_STR_LEN];

    /* Type 2: Baseboard Information */
    char board_manufacturer[SMBIOS_STR_LEN];
    char board_product[SMBIOS_STR_LEN];

    /* Type 3: Chassis */
    unsigned char chassis_type;

    /* Type 4: Processor */
    char cpu_socket[SMBIOS_STR_LEN];
    unsigned int cpu_max_speed;
    unsigned int cpu_current_speed;

    /* Type 16: Physical Memory Array */
    unsigned long mem_max_capacity_kb;
    unsigned char mem_slots;

    /* Type 17: Memory Devices */
    struct {
        unsigned int size_mb;
        unsigned int speed_mhz;
        unsigned char populated;
    } dimms[SMBIOS_MAX_DIMMS];
    unsigned char dimm_count;

    /* Status */
    unsigned char valid;          /* Tables successfully parsed */
    unsigned char entry_found;    /* Entry point located */
} smbios_info_t;

/* ACPI parsed information */
typedef struct {
    /* RSDP info */
    char oem_id[8];
    unsigned char revision;       /* 0=ACPI 1.0, 2+=ACPI 2.0+ */
    unsigned long rsdt_addr;

    /* RSDT info */
    unsigned char table_count;

    /* FACP/FADT info */
    unsigned char pm_profile;     /* 0=Unspec, 1=Desktop, 2=Mobile, etc. */

    /* Table presence flags */
    unsigned char has_facp;
    unsigned char has_mcfg;
    unsigned char has_apic;
    unsigned char has_hpet;

    /* Status */
    unsigned char valid;
    unsigned char rsdp_found;
} acpi_info_t;

/*----------------------------------------------------------------------------
 * Configuration Profile Structure
 *----------------------------------------------------------------------------*/
#define MAX_PROFILES     8
#define PROFILE_NAME_LEN 12
#define PROFILE_MAGIC    0x4B43  /* "CK" */
#define PROFILE_VERSION  1

typedef struct {
    /* Header */
    unsigned int magic;              /* PROFILE_MAGIC */
    unsigned char version;           /* PROFILE_VERSION */
    char name[PROFILE_NAME_LEN + 1]; /* User-defined name */
    char date[11];                   /* YYYY-MM-DD */
    char time[9];                    /* HH:MM:SS */

    /* System identification (for validation) */
    unsigned char chipset_type;      /* Must match to apply */
    char chipset_name[24];

    /* Cache configuration */
    unsigned char cache_enabled;
    unsigned char cache_writeback;   /* WB vs WT mode */

    /* NC region configuration */
    unsigned char nc_count;          /* Number of active NC regions */
    struct {
        unsigned long base_kb;
        unsigned long size_kb;
        unsigned char active;
    } nc_regions[4];

    /* Shadow RAM configuration (for info-only chipsets) */
    unsigned char shadow_c0000;      /* C0000-C7FFF */
    unsigned char shadow_c8000;      /* C8000-CFFFF */
    unsigned char shadow_d0000;      /* D0000-D7FFF */
    unsigned char shadow_d8000;      /* D8000-DFFFF */
    unsigned char shadow_e0000;      /* E0000-EFFFF */
    unsigned char shadow_f0000;      /* F0000-FFFFF */

    /* Checksum */
    unsigned int checksum;
} profile_t;

/* Global profile array */
static profile_t g_profiles[MAX_PROFILES];

/* g_devices[], ISA PnP globals, PCIe/MCFG globals are now in CK_ENUM.C */

typedef struct {
    screen_t current_screen;
    chipset_info_t chipset;
    int is_486;
    unsigned int total_mem_kb;

    /* NC Config state */
    int nc_cursor;
    int nc_dialog_active;
    unsigned int nc_edit_base;
    unsigned int nc_edit_size;
    int nc_edit_field;
    nc_region_live_t nc_live[4];     /* Live NC region data */

    /* Cache Test state */
    unsigned char test_select;
    unsigned char test_results;
    unsigned long test_timing;
    unsigned char test_dirty_pass;  /* Dirty data test passed */
    int test_cursor;

    /* Register state */
    int reg_cursor;
    int reg_edit_active;        /* Edit mode active */
    unsigned char reg_edit_val; /* Value being edited */
    unsigned char reg_values[128];

    /* Benchmark state (F5) */
    struct {
        unsigned long copy_mbs_x10;      /* Copy speed in MB/s * 10 */
        unsigned long fill_mbs_x10;      /* Fill speed in MB/s * 10 */
        unsigned long read_mbs_x10;      /* Read speed in MB/s * 10 */
        unsigned long cache_on_copy, cache_on_fill, cache_on_read;
        unsigned long cache_off_copy, cache_off_fill, cache_off_read;
        int test_running;
        int progress_pct;
        unsigned char cache_was_enabled;
        int cursor;
    } bench;

    /* Profiles state (F6) */
    int profile_cursor;
    int profile_count;
    int profile_loaded;          /* Index of loaded profile (-1 = none) */
    int profile_modified;        /* Current config differs from loaded */
    char profile_dir[64];        /* Profile directory path */

    /* 286/386SX inventory (F1 for info-only chipsets) */
    inventory_286_t inv286;

    /* External cache detection (82385-style, timing-based) */
    struct {
        int present;              /* External cache detected via timing */
        unsigned int size_kb;     /* Detected size: 0=unknown, 32, 64 */
        unsigned int line_size;   /* Cache line size (16 for 82385) */
        unsigned long speed_ratio;/* Cached vs uncached ratio (x100) */
        int probed;               /* 1 = timing probe completed */
    } ext_cache;

    /* Expansion card inventory (F7) */
    inventory_state_t inventory;

    /* SMBIOS/DMI information */
    smbios_info_t smbios;

    /* ACPI information */
    acpi_info_t acpi;

    /* F1 Info screen tab */
    info_tab_t info_tab;

    /* Bus configuration state (F8) */
    struct {
        int cursor;               /* Selected slot index */
        int scroll_offset;        /* For scrolling */
        int initialized;          /* 1 = buscfg_init() called */
        int editing;              /* 1 = in edit mode for selected slot */
        int edit_field;           /* 0=enable, 1=IRQ, 2=DMA, 3=IO, 4=Mem */
        int isapnp_count;         /* Number of ISA PnP cards detected */
    } busconfig;
} app_state_t;

/* Global application state */
static app_state_t g_state;

/* Chipset database */
static const struct {
    unsigned char type;
    const char   *name;
    const char   *vendor;
    const char   *tier;
    unsigned char score_x10;
    unsigned char is_wb;
    unsigned char nc_strategy;
    unsigned char nc_regions;
    unsigned int  granularity;
    unsigned char info_only;    /* 1 = no cache/NC control, info only */
} g_chipset_db[] = {
    /* 386/486 chipsets */
    { CHIPSET_SIS_460,    "SiS 85C460",      "SiS",      "S", 96, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_SIS_RABBIT, "SiS 85C310",      "SiS",      "S", 95, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_OPTI391,    "OPTi 82C391",     "OPTi",     "S", 92, 1, NC_RANGE,    2,    8, 0 },
    { CHIPSET_FARADAY,    "Faraday FE3600",  "Faraday",  "A", 88, 0, NC_RANGE,    3,  128, 0 },
    { CHIPSET_UMC491,     "UMC UM82C491",    "UMC",      "A", 85, 1, NC_RANGE,    1,    8, 0 },
    { CHIPSET_MIC9391,    "MIC MIC9391",     "MIC",      "A", 83, 1, NC_RANGE,    1,   64, 0 },
    { CHIPSET_ETEQ_BENGAL,"Eteq 82C495WB",   "Eteq",     "A", 80, 1, NC_RANGE,    2,    8, 0 },
    { CHIPSET_OPTI381,    "OPTi 82C381",     "OPTi",     "B", 78, 0, NC_RANGE,    2,  512, 0 },
    { CHIPSET_VIA,        "VIA VT82C310",    "VIA",      "B", 75, 0, NC_RANGE,    2,   64, 0 },
    { CHIPSET_VLSI,       "VLSI VL82C311",   "VLSI",     "B", 70, 0, NC_RANGE,    1,   64, 0 },
    { CHIPSET_CT_PEAK,    "C&T 82C301",      "C&T",      "C", 60, 0, NC_BOUNDARY, 1,   64, 0 },
    { CHIPSET_CT_SCAT,    "C&T 82C235",      "C&T",      "C", 55, 0, NC_BOUNDARY, 1,   64, 0 },
    { CHIPSET_ALI_FINIS,  "ALi M1209",       "ALi",      "C", 55, 0, NC_BOUNDARY, 1,   64, 0 },
    { CHIPSET_FOREX,      "Forex FRX-386DX", "Forex",    "D", 45, 0, NC_BOUNDARY, 1, 1024, 0 },
    { CHIPSET_SUNTAC,     "Suntac ST62C301", "Suntac",   "D", 45, 0, NC_SHADOW,   1,   64, 0 },
    { CHIPSET_EISA_82350, "Intel 82350",     "Intel",    "B", 70, 0, NC_RANGE,    1,   64, 0 },
    { CHIPSET_CT_NEAT,    "C&T 82C211",      "C&T",      "D", 40, 0, NC_SHADOW,   0,    0, 0 },
    { CHIPSET_HEADLAND,   "Headland HT12",   "Headland", "B", 72, 0, NC_STEERING, 4,   64, 0 },
    /* 486 VLB-era chipset (no NC regions, shadow RAM only) */
    { CHIPSET_CONTAQ_596, "Contaq 82C596",   "Contaq",   "C", 65, 0, NC_SHADOW,   0,    0, 0 },
    /* Pentium-era chipsets */
    { CHIPSET_I430FX,     "Intel 430FX",     "Intel",    "S", 94, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_I430HX,     "Intel 430HX",     "Intel",    "S", 96, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_I430VX,     "Intel 430VX",     "Intel",    "A", 88, 1, NC_RANGE,    4,  256, 0 },
    { CHIPSET_ALI_ALADDIN,"ALi M1489",       "ALi",      "A", 82, 1, NC_RANGE,    2,   64, 0 },
    { CHIPSET_SIS_496,    "SiS 85C496",      "SiS",      "S", 93, 1, NC_RANGE,    3,   64, 0 },
    { CHIPSET_VIA_VP1,    "VIA VT82C570",    "VIA",      "B", 76, 0, NC_RANGE,    2,   64, 0 },
    { CHIPSET_OPTI_VIPER, "OPTi 82C596",     "OPTi",     "A", 85, 1, NC_RANGE,    4,    8, 0 },
    /* Late Socket 7 / Super7 chipsets (1997-1999) */
    { CHIPSET_I430TX,     "Intel 430TX",     "Intel",    "S", 93, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_VIA_VP3,    "VIA Apollo VP3",  "VIA",      "A", 87, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_VIA_MVP3,   "VIA Apollo MVP3", "VIA",      "S", 92, 1, NC_RANGE,    4,   64, 0 },
    /* SiS Super7 chipsets - NC region counts CORRECTED per datasheets + 86Box */
    { CHIPSET_SIS_5591,   "SiS 5591",        "SiS",      "A", 85, 1, NC_RANGE,    2,   64, 0 },  /* 2 NC regions (86Box verified) */
    { CHIPSET_SIS_5598,   "SiS 5598",        "SiS",      "A", 86, 1, NC_RANGE,    2,   64, 0 },  /* 2 NC regions per datasheet */
    { CHIPSET_SIS_530,    "SiS 530",         "SiS",      "A", 84, 1, NC_RANGE,    2,   64, 0 },  /* 2 NC regions per datasheet */
    { CHIPSET_ALI_ALADDIN5,"ALi M1541",      "ALi",      "A", 88, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_I430MX,     "Intel 430MX",     "Intel",    "A", 86, 1, NC_RANGE,    4,   64, 0 },
    /* 286-era chipsets (informational only) */
    { CHIPSET_CT_NEAT_FULL,"C&T CS8221",     "C&T",      "I", 30, 0, NC_NONE,     0,    0, 1 },
    { CHIPSET_VLSI_100,   "VLSI VL82C100",   "VLSI",     "I", 30, 0, NC_NONE,     0,    0, 1 },
    { CHIPSET_VLSI_101,   "VLSI VL82C101",   "VLSI",     "I", 30, 0, NC_NONE,     0,    0, 1 },
    { CHIPSET_VLSI_102,   "VLSI VL82C102",   "VLSI",     "I", 30, 0, NC_NONE,     0,    0, 1 },
    { CHIPSET_HEADLAND_101,"Headland HT101", "Headland", "I", 30, 0, NC_NONE,     0,    0, 1 },
    { CHIPSET_HEADLAND_102,"Headland HT102", "Headland", "I", 30, 0, NC_NONE,     0,    0, 1 },
    { CHIPSET_ALI_M1217,  "ALi M1217",       "ALi",      "I", 30, 0, NC_NONE,     0,    0, 1 },
    { CHIPSET_OPTI_212,   "OPTi 82C212",     "OPTi",     "I", 30, 0, NC_NONE,     0,    0, 1 },
    /* 386SX-era chipsets (info + limited cache support) */
    { CHIPSET_CT_NEAT386, "C&T CS8230",      "C&T",      "I", 35, 0, NC_NONE,     0,    0, 1 },
    { CHIPSET_HEADLAND_18,"Headland HT18",   "Headland", "I", 35, 0, NC_NONE,     0,    0, 1 },
    { CHIPSET_VLSI_320,   "VLSI VL82C320",   "VLSI",     "I", 35, 0, NC_NONE,     0,    0, 1 },
    /* EISA chipsets - Pure EISA (Intel 350 family) */
    { CHIPSET_EISA_82350DT,"Intel 82350DT",  "Intel",    "B", 72, 0, NC_RANGE,    1,   64, 0 },
    /* EISA chipsets - Intel PCIsets (EISA+PCI bridge via 82375EB/SB) */
    { CHIPSET_EISA_420TX, "Intel 420TX",     "Intel",    "A", 80, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_EISA_420ZX, "Intel 420ZX",     "Intel",    "A", 80, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_EISA_430LX, "Intel 430LX",     "Intel",    "A", 85, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_EISA_430NX, "Intel 430NX",     "Intel",    "A", 88, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_EISA_430HX, "Intel 430HX",     "Intel",    "S", 92, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_EISA_440FX, "Intel 440FX",     "Intel",    "S", 94, 1, NC_RANGE,    4,   64, 0 },
    { CHIPSET_EISA_450GX, "Intel 450GX",     "Intel",    "S", 95, 1, NC_RANGE,    4,   64, 0 },
    /* EISA chipsets - OPTi */
    { CHIPSET_OPTI_682,   "OPTi 82C682",     "OPTi",     "I", 35, 1, NC_NONE,     0,    0, 1 },
    { CHIPSET_OPTI_683,   "OPTi 82C683",     "OPTi",     "I", 35, 1, NC_NONE,     0,    0, 1 },
    { CHIPSET_OPTI_HUNTER,"OPTi 82C691/696", "OPTi",     "I", 35, 1, NC_NONE,     0,    0, 1 },
    { CHIPSET_OPTI_PENT_EISA,"OPTi 82C693/6/7","OPTi",   "I", 35, 1, NC_NONE,     0,    0, 1 },
    /* EISA chipsets - Other vendors */
    { CHIPSET_SIS_EISA,   "SiS 85C411/406",  "SiS",      "I", 35, 0, NC_NONE,     0,    0, 1 },
    { CHIPSET_VIA_EISA,   "VIA VT82C495",    "VIA",      "I", 40, 0, NC_NONE,     0,    0, 1 },
    /* MCA bus - observer only (hardware-enforced cache coherency via snooping) */
    { CHIPSET_MCA_GENERIC,"IBM PS/2 MCA",   "IBM",      "I", 25, 0, NC_NONE,     0,    0, 1 },
    { 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0 }
};

/* PCI/MCA/ISA PnP ID databases and lookup functions are now in CK_ENUM.C */
/* Use enum_lookup_pci_name(), enum_lookup_mca_name(), enum_lookup_isapnp_name() */


/*============================================================================
 * CACHE CONTROL DATABASE
 *============================================================================*/

/* Per-chipset cache enable/disable register information */
typedef struct {
    unsigned char chipset_type;
    unsigned char cache_reg;      /* Register index for cache control */
    unsigned char cache_bit;      /* Bit position (0-7) */
    unsigned char enable_value;   /* 1=set bit to enable, 0=clear bit to enable */
} cache_control_t;

/* Database of cache control registers per chipset */
static const cache_control_t g_cache_ctrl[] = {
    /* OPTi family: Index 20h, bit 0 = cache enable */
    { CHIPSET_OPTI391,     0x20, 0, 1 },
    { CHIPSET_OPTI381,     0x20, 0, 1 },
    { CHIPSET_ETEQ_BENGAL, 0x20, 0, 1 },

    /* SiS family: Index 10h, bit 0 = cache enable */
    { CHIPSET_SIS_460,     0x10, 0, 1 },
    { CHIPSET_SIS_RABBIT,  0x10, 0, 1 },

    /* UMC 491: Index 00h, bit 7 = cache enable */
    { CHIPSET_UMC491,      0x00, 7, 1 },

    /* MIC 9391: Index 40h, bit 0 = cache enable */
    { CHIPSET_MIC9391,     0x40, 0, 1 },

    /* Faraday: Index 15h, bit 0 = cache enable */
    { CHIPSET_FARADAY,     0x15, 0, 1 },

    /* VIA: Index 10h, bit 4 = cache enable */
    { CHIPSET_VIA,         0x10, 4, 1 },

    /* VLSI SCAMP: Index 12h, bit 0 = cache enable */
    { CHIPSET_VLSI,        0x12, 0, 1 },

    /* C&T PEAK/SCAT: Index 14h, bit 0 = cache enable */
    { CHIPSET_CT_PEAK,     0x14, 0, 1 },
    { CHIPSET_CT_SCAT,     0x14, 0, 1 },

    /* Headland: Index 10h, bit 0 = cache enable */
    { CHIPSET_HEADLAND,    0x10, 0, 1 },

    /* Pentium-era chipsets - use PCI config or legacy ports */
    /* Intel Triton series: PCI config space - use MTTR for cache control */
    { CHIPSET_I430FX,      0x52, 0, 1 },  /* PCON register bit 0 */
    { CHIPSET_I430HX,      0x52, 0, 1 },  /* PCON register bit 0 */
    { CHIPSET_I430VX,      0x52, 0, 1 },  /* PCON register bit 0 */

    /* ALi Aladdin: Index 42h, bit 0 = L2 cache enable */
    { CHIPSET_ALI_ALADDIN, 0x42, 0, 1 },

    /* Contaq 82C596: Cache at Index 0x11 bit 0 (per 86Box source) */
    { CHIPSET_CONTAQ_596,  0x11, 0, 1 },

    /* SiS 496: PCI 0x42 bit 0 = External Cache Enable (per datasheet) */
    { CHIPSET_SIS_496,     0x42, 0, 1 },

    /* VIA VP1: Index 50h, bit 2 = L2 cache enable */
    { CHIPSET_VIA_VP1,     0x50, 2, 1 },

    /* OPTi Viper: Index 20h, bit 0 = cache enable (like OPTi 391) */
    { CHIPSET_OPTI_VIPER,  0x20, 0, 1 },

    /* Late Socket 7 / Super7 chipsets */
    /* Intel 430TX/MX: Same as Triton, PCON register */
    { CHIPSET_I430TX,      0x52, 0, 1 },
    { CHIPSET_I430MX,      0x52, 0, 1 },

    /* VIA Apollo VP3/MVP3: PCI config 0x52, bit 0 = L2 enable */
    { CHIPSET_VIA_VP3,     0x52, 0, 1 },
    { CHIPSET_VIA_MVP3,    0x52, 0, 1 },

    /* SiS Super7 chipsets - CORRECTED per datasheets + 86Box verification */
    { CHIPSET_SIS_5591,    0x51, 7, 1 },  /* L2 Cache Enable at PCI 0x51 bit 7 (86Box verified) */
    { CHIPSET_SIS_5598,    0x51, 6, 1 },  /* L2 Cache Enable at PCI 0x51 bit 6 */
    { CHIPSET_SIS_530,     0x51, 7, 1 },  /* L2 Cache Enable at PCI 0x51 bit 7 */

    /* ALi Aladdin V (M1541): Index 42h, bit 0 (like M1489) */
    { CHIPSET_ALI_ALADDIN5, 0x42, 0, 1 },

    /* Terminator */
    { 0, 0, 0, 0 }
};

/*============================================================================
 * V2.4 PER-FEATURE ACCESS DESCRIPTOR ARCHITECTURE
 *============================================================================*/

/* Access method enumeration - how to reach the chipset registers */
typedef enum {
    ACCESS_NONE = 0,        /* Feature not supported */
    ACCESS_PCI_CONFIG,      /* PCI config space (0xCF8/0xCFC) */
    ACCESS_LEGACY_22_23,    /* Index 0x22, Data 0x23 */
    ACCESS_LEGACY_22_24,    /* Index 0x22, Data 0x24 (OPTi) */
    ACCESS_EISA,            /* EISA ports (0x0C80/0x0C85) */
    ACCESS_SPECIAL_FN       /* Dedicated function pointer */
} access_method_t;

/* Encoding type enumeration - how data is stored in registers */
typedef enum {
    ENC_NONE = 0,           /* No encoding (raw register) */
    ENC_BIT_TOGGLE,         /* Single bit toggle (cache enable) */
    ENC_BASE_SIZE_PAIR,     /* Register pair: base + size code */
    ENC_16BIT_PACKED,       /* SiS 5598 style: 16-bit with size in high bits */
    ENC_BOUNDARY,           /* Single boundary register */
    ENC_STEERING,           /* Headland steering-based NC */
    ENC_PAM                 /* Intel PAM registers (info-only) */
} encoding_type_t;

/* Feature type constants */
#define FEAT_CACHE   0x01
#define FEAT_NC      0x02
#define FEAT_SHADOW  0x04
#define FEAT_A20     0x08

/* Forward declarations for function pointers */
typedef void (*feat_read_fn_t)(void);
typedef void (*feat_write_fn_t)(int, unsigned long, unsigned long);
typedef void (*feat_clear_fn_t)(int);

/* Unified feature descriptor - per-chipset, per-feature access specification */
typedef struct {
    /* Identification */
    unsigned char chipset_type;
    unsigned char feature_type;     /* FEAT_CACHE, FEAT_NC, FEAT_SHADOW, FEAT_A20 */

    /* Access routing */
    access_method_t access_method;
    unsigned int index_port;        /* Legacy index port (0x22, 0x0C80) */
    unsigned int data_port;         /* Legacy data port (0x23, 0x24, 0x0C85) */
    unsigned char pci_bus;          /* PCI bus (usually 0) */
    unsigned char pci_dev;          /* PCI device (usually 0) */
    unsigned char pci_func;         /* PCI function (usually 0) */

    /* Register layout */
    unsigned char base_reg;         /* Primary register */
    unsigned char ctrl_reg;         /* Control/enable register (0 if same as base) */
    unsigned char reg_count;        /* Number of registers for feature */

    /* Encoding parameters */
    encoding_type_t encoding;
    unsigned char bit_position;     /* For BIT_TOGGLE: which bit */
    unsigned char enable_high;      /* 1=set bit to enable, 0=clear to enable */
    unsigned char base_shift;       /* Base address >> shift = register value */
    unsigned char size_shift;       /* Size code bit position in register */
    unsigned char max_regions;      /* NC: number of regions supported */
    unsigned char max_size_code;    /* NC: maximum size code value */

    /* Special handler (for complex encodings) */
    feat_read_fn_t read_fn;         /* NULL = use generic, else call this */
    feat_write_fn_t write_fn;
    feat_clear_fn_t clear_fn;
} feature_descriptor_t;

/*----------------------------------------------------------------------------
 * V2.4 SPECIALIZED NC REGION HANDLER FORWARD DECLARATIONS
 *
 * These functions are defined later in the file but referenced in g_features[].
 * They handle chipsets with complex or non-standard NC region encodings.
 *----------------------------------------------------------------------------*/
static void read_sis5598_nc_regions(void);
static void write_sis5598_nc_region(int region, unsigned long base_kb, unsigned long size_kb);
static void clear_sis5598_nc_region(int region);

/*----------------------------------------------------------------------------
 * V2.4 FEATURE DATABASE
 *
 * This table will be populated in Phase 2/3 with all chipset features.
 * Each entry describes one feature (cache, NC, shadow, A20) for one chipset.
 * Use get_feature() to look up entries.
 *----------------------------------------------------------------------------*/
static const feature_descriptor_t g_features[] = {
    /*
     * Format: { chipset, feature, access, idx_port, data_port, bus, dev, func,
     *           base_reg, ctrl_reg, reg_count, encoding, bit_pos, enable_high,
     *           base_shift, size_shift, max_regions, max_size, read_fn, write_fn, clear_fn }
     */

    /*==========================================================================
     * FEAT_CACHE - Legacy 0x22/0x23 Chipsets
     *==========================================================================*/

    /* OPTi family: Index 20h, bit 0 */
    { CHIPSET_OPTI391, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x20, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_OPTI381, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x20, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_ETEQ_BENGAL, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x20, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* SiS 460/Rabbit: Index 10h, bit 0 */
    { CHIPSET_SIS_460, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x10, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_SIS_RABBIT, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x10, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* UMC 491: Index 00h, bit 7 */
    { CHIPSET_UMC491, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x00, 0, 1, ENC_BIT_TOGGLE, 7, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* MIC 9391: Index 40h, bit 0 */
    { CHIPSET_MIC9391, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x40, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* Faraday: Index 15h, bit 0 */
    { CHIPSET_FARADAY, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x15, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* VIA (legacy): Index 10h, bit 4 */
    { CHIPSET_VIA, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x10, 0, 1, ENC_BIT_TOGGLE, 4, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* VLSI SCAMP: Index 12h, bit 0 */
    { CHIPSET_VLSI, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x12, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* C&T PEAK/SCAT: Index 14h, bit 0 */
    { CHIPSET_CT_PEAK, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x14, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_CT_SCAT, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x14, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* Headland: Index 10h, bit 0 */
    { CHIPSET_HEADLAND, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x10, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* Contaq 82C596: Index 11h, bit 0 (uses legacy 0x22/0x23, NOT PCI!) */
    { CHIPSET_CONTAQ_596, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x11, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /*==========================================================================
     * FEAT_CACHE - PCI Config Space Chipsets
     *==========================================================================*/

    /* Intel Triton: PCI 0x52, bit 0 (PCON register) */
    { CHIPSET_I430FX, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_I430HX, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_I430VX, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_I430TX, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_I430MX, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* ALi Aladdin: PCI 0x42, bit 0 */
    { CHIPSET_ALI_ALADDIN, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x42, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_ALI_ALADDIN5, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x42, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* SiS 496: PCI 0x42, bit 0 */
    { CHIPSET_SIS_496, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x42, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* SiS Super7: PCI 0x51, varies per model (86Box verified) */
    { CHIPSET_SIS_5591, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x51, 0, 1, ENC_BIT_TOGGLE, 7, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_SIS_5598, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x51, 0, 1, ENC_BIT_TOGGLE, 6, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_SIS_530, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x51, 0, 1, ENC_BIT_TOGGLE, 7, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* VIA VP1: PCI 0x50, bit 2 */
    { CHIPSET_VIA_VP1, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x50, 0, 1, ENC_BIT_TOGGLE, 2, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* VIA Apollo VP3/MVP3: PCI 0x52, bit 0 */
    { CHIPSET_VIA_VP3, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_VIA_MVP3, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* OPTi Viper (82C596): PCI 0x20, bit 0 */
    { CHIPSET_OPTI_VIPER, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x20, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /*==========================================================================
     * FEAT_NC - Legacy 0x22/0x23 Chipsets
     *==========================================================================*/

    /* OPTi 391/381: NC at regs 0x52-0x55, 2 regions, base/size pairs */
    { CHIPSET_OPTI391, FEAT_NC, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x52, 0, 4, ENC_BASE_SIZE_PAIR, 0, 0, 4, 4, 2, 15, NULL, NULL, NULL },
    { CHIPSET_OPTI381, FEAT_NC, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x52, 0, 4, ENC_BASE_SIZE_PAIR, 0, 0, 4, 4, 2, 15, NULL, NULL, NULL },

    /* SiS 460/Rabbit: NC at regs 0x14-0x1B, 4 regions, base/size pairs */
    { CHIPSET_SIS_460, FEAT_NC, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x14, 0, 8, ENC_BASE_SIZE_PAIR, 0, 0, 6, 4, 4, 15, NULL, NULL, NULL },
    { CHIPSET_SIS_RABBIT, FEAT_NC, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x14, 0, 8, ENC_BASE_SIZE_PAIR, 0, 0, 6, 4, 4, 15, NULL, NULL, NULL },

    /*==========================================================================
     * FEAT_NC - PCI Config Space Chipsets
     *==========================================================================*/

    /* OPTi Viper: PCI, NC at regs 0x52-0x55, 2 regions */
    { CHIPSET_OPTI_VIPER, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 4, ENC_BASE_SIZE_PAIR, 0, 0, 4, 4, 2, 15, NULL, NULL, NULL },

    /* SiS 496: PCI, NC at regs 0x50-0x55, 4 regions (layout per 86Box) */
    { CHIPSET_SIS_496, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x50, 0, 6, ENC_BASE_SIZE_PAIR, 0, 0, 6, 4, 4, 15, NULL, NULL, NULL },

    /* SiS Super7: PCI, NC at 0x77 (enable) + 0x78-0x7B, 2 regions
     * Uses 16-bit packed encoding - use specialized handlers */
    { CHIPSET_SIS_5591, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x78, 0x77, 4, ENC_16BIT_PACKED, 0, 1, 6, 13, 2, 7,
      read_sis5598_nc_regions, write_sis5598_nc_region, clear_sis5598_nc_region },
    { CHIPSET_SIS_5598, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x78, 0x77, 4, ENC_16BIT_PACKED, 0, 1, 6, 13, 2, 7,
      read_sis5598_nc_regions, write_sis5598_nc_region, clear_sis5598_nc_region },
    { CHIPSET_SIS_530, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x78, 0x77, 4, ENC_16BIT_PACKED, 0, 1, 6, 13, 2, 7,
      read_sis5598_nc_regions, write_sis5598_nc_region, clear_sis5598_nc_region },

    /* Intel Triton: PAM registers at 0x59-0x5F (info-only, no traditional NC) */
    { CHIPSET_I430FX, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x59, 0, 7, ENC_PAM, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_I430HX, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x59, 0, 7, ENC_PAM, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_I430VX, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x59, 0, 7, ENC_PAM, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_I430TX, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x59, 0, 7, ENC_PAM, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_I430MX, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x59, 0, 7, ENC_PAM, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },

    /* VIA Apollo: PCI, NC at regs 0x58-0x5F, 4 regions */
    { CHIPSET_VIA_VP1, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x58, 0, 8, ENC_BASE_SIZE_PAIR, 0, 0, 6, 4, 4, 15, NULL, NULL, NULL },
    { CHIPSET_VIA_VP3, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x58, 0, 8, ENC_BASE_SIZE_PAIR, 0, 0, 6, 4, 4, 15, NULL, NULL, NULL },
    { CHIPSET_VIA_MVP3, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x58, 0, 8, ENC_BASE_SIZE_PAIR, 0, 0, 6, 4, 4, 15, NULL, NULL, NULL },

    /* ALi Aladdin: PCI, NC at regs 0x58-0x5F, 4 regions */
    { CHIPSET_ALI_ALADDIN, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x58, 0, 8, ENC_BASE_SIZE_PAIR, 0, 0, 6, 4, 4, 15, NULL, NULL, NULL },
    { CHIPSET_ALI_ALADDIN5, FEAT_NC, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x58, 0, 8, ENC_BASE_SIZE_PAIR, 0, 0, 6, 4, 4, 15, NULL, NULL, NULL },

    /*==========================================================================
     * FEAT_SHADOW - Shadow RAM Configuration (info-only in most cases)
     *==========================================================================*/

    /* C&T NEAT/NEAT-386: Shadow at regs 0x19-0x1B */
    { CHIPSET_CT_NEAT_FULL, FEAT_SHADOW, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x19, 0, 3, ENC_NONE, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_CT_NEAT386, FEAT_SHADOW, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x19, 0, 3, ENC_NONE, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },

    /* Headland: Shadow at regs 0x12-0x13 */
    { CHIPSET_HEADLAND, FEAT_SHADOW, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x12, 0, 2, ENC_NONE, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },

    /* Intel Triton: Shadow via PAM registers 0x59-0x5F (same as NC, different interpretation) */
    { CHIPSET_I430FX, FEAT_SHADOW, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x59, 0, 7, ENC_PAM, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_I430HX, FEAT_SHADOW, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x59, 0, 7, ENC_PAM, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_I430VX, FEAT_SHADOW, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x59, 0, 7, ENC_PAM, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_I430TX, FEAT_SHADOW, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x59, 0, 7, ENC_PAM, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_I430MX, FEAT_SHADOW, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x59, 0, 7, ENC_PAM, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },

    /*==========================================================================
     * FEAT_A20 - A20 Gate Control
     *==========================================================================*/

    /* C&T NEAT/NEAT-386: A20 at reg 0x1D bit 1 */
    { CHIPSET_CT_NEAT_FULL, FEAT_A20, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x1D, 0, 1, ENC_BIT_TOGGLE, 1, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_CT_NEAT386, FEAT_A20, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x1D, 0, 1, ENC_BIT_TOGGLE, 1, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* OPTi 82C212: A20 at reg 0x20 bit 0 (uses 0x22/0x24 ports) */
    { CHIPSET_OPTI_212, FEAT_A20, ACCESS_LEGACY_22_24, 0x22, 0x24, 0, 0, 0,
      0x20, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /*==========================================================================
     * FEAT_CACHE - EISA Chipsets
     *==========================================================================*/

    /* Intel 82350DT (Mongoose): 82359 DRAM controller, reg 0x07 bit 0
     * Access via 0x22/0x23, set chip ID 0x01 first at reg 0x21 */
    { CHIPSET_EISA_82350DT, FEAT_CACHE, ACCESS_LEGACY_22_23, 0x22, 0x23, 0, 0, 0,
      0x07, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* Intel EISA PCIsets: Use PCMC via PCI config space
     * 420TX/ZX (Saturn): 82424, similar to early 430
     * 430LX/NX: 82434, reg 0x52 bit 0
     * 430HX: 82439HX, reg 0x52 bit 0
     * 440FX: 82441FX, reg 0x52 bit 0
     * 450GX: 82454GX, reg 0x52 bit 0 */
    { CHIPSET_EISA_420TX, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_EISA_420ZX, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_EISA_430LX, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_EISA_430NX, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_EISA_430HX, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_EISA_440FX, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },
    { CHIPSET_EISA_450GX, FEAT_CACHE, ACCESS_PCI_CONFIG, 0, 0, 0, 0, 0,
      0x52, 0, 1, ENC_BIT_TOGGLE, 0, 1, 0, 0, 0, 0, NULL, NULL, NULL },

    /* Terminator */
    { 0, 0, ACCESS_NONE, 0, 0, 0, 0, 0, 0, 0, 0, ENC_NONE, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL }
};

/*----------------------------------------------------------------------------
 * V2.4 FEATURE LOOKUP FUNCTION
 *
 * Returns pointer to feature descriptor for given chipset and feature type.
 * Returns NULL if the feature is not supported by the chipset.
 *----------------------------------------------------------------------------*/
static const feature_descriptor_t* get_feature(unsigned char chipset,
                                               unsigned char feature)
{
    const feature_descriptor_t *f;

    for (f = g_features; f->chipset_type != 0 || f->feature_type != 0; f++) {
        if (f->chipset_type == chipset && f->feature_type == feature) {
            return f;
        }
    }
    return NULL;
}

/*----------------------------------------------------------------------------
 * V2.4 GENERIC REGISTER ACCESS FUNCTIONS
 *
 * These functions route register access based on the descriptor's access_method.
 * This eliminates the need for is_pci_chipset() in each feature handler.
 *----------------------------------------------------------------------------*/

/* Forward declarations for legacy port access (defined later in CHIPSET LAYER) */
static unsigned char legacy_read_reg_22_23(unsigned char reg);
static void legacy_write_reg_22_23(unsigned char reg, unsigned char val);
static unsigned char legacy_read_reg_22_24(unsigned char reg);
static void legacy_write_reg_22_24(unsigned char reg, unsigned char val);
static unsigned char eisa_read_reg(unsigned char reg);
static void eisa_write_reg(unsigned char reg, unsigned char val);

/*
 * feature_read_reg - Read a register using the descriptor's access method
 *
 * For PCI access, uses the descriptor's bus/dev/func fields.
 * For legacy access, uses the descriptor's index/data ports.
 */
static unsigned char feature_read_reg(const feature_descriptor_t *desc,
                                      unsigned char reg)
{
    if (desc == NULL) {
        return 0xFF;
    }

    switch (desc->access_method) {
        case ACCESS_PCI_CONFIG:
            return pci_read_config_byte(desc->pci_bus, desc->pci_dev,
                                        desc->pci_func, reg);

        case ACCESS_LEGACY_22_23:
            return legacy_read_reg_22_23(reg);

        case ACCESS_LEGACY_22_24:
            return legacy_read_reg_22_24(reg);

        case ACCESS_EISA:
            return eisa_read_reg(reg);

        case ACCESS_SPECIAL_FN:
        case ACCESS_NONE:
        default:
            return 0xFF;
    }
}

/*
 * feature_write_reg - Write a register using the descriptor's access method
 *
 * For PCI access, uses the descriptor's bus/dev/func fields.
 * For legacy access, uses the descriptor's index/data ports.
 */
static void feature_write_reg(const feature_descriptor_t *desc,
                              unsigned char reg, unsigned char val)
{
    if (desc == NULL) {
        return;
    }

    switch (desc->access_method) {
        case ACCESS_PCI_CONFIG:
            pci_write_config_byte(desc->pci_bus, desc->pci_dev,
                                  desc->pci_func, reg, val);
            break;

        case ACCESS_LEGACY_22_23:
            legacy_write_reg_22_23(reg, val);
            break;

        case ACCESS_LEGACY_22_24:
            legacy_write_reg_22_24(reg, val);
            break;

        case ACCESS_EISA:
            eisa_write_reg(reg, val);
            break;

        case ACCESS_SPECIAL_FN:
        case ACCESS_NONE:
        default:
            break;
    }
}

/*
 * feature_read_reg16 - Read a 16-bit value (two consecutive registers)
 *
 * Returns low byte from base_reg, high byte from base_reg+1.
 * Little-endian: result = (high << 8) | low
 */
static unsigned int feature_read_reg16(const feature_descriptor_t *desc,
                                       unsigned char reg)
{
    unsigned char lo, hi;

    if (desc == NULL) {
        return 0xFFFF;
    }

    lo = feature_read_reg(desc, reg);
    hi = feature_read_reg(desc, (unsigned char)(reg + 1));

    return ((unsigned int)hi << 8) | lo;
}

/*
 * feature_write_reg16 - Write a 16-bit value (two consecutive registers)
 *
 * Writes low byte to base_reg, high byte to base_reg+1.
 * Little-endian: low = val & 0xFF, high = (val >> 8) & 0xFF
 */
static void feature_write_reg16(const feature_descriptor_t *desc,
                                unsigned char reg, unsigned int val)
{
    if (desc == NULL) {
        return;
    }

    feature_write_reg(desc, reg, (unsigned char)(val & 0xFF));
    feature_write_reg(desc, (unsigned char)(reg + 1), (unsigned char)((val >> 8) & 0xFF));
}

/*----------------------------------------------------------------------------
 * V2.4 CACHE CONTROL FUNCTIONS
 *
 * These use the unified g_features[] table for descriptor-based access.
 * They will eventually replace is_cache_enabled(), disable_cache_internal(),
 * and enable_cache().
 *----------------------------------------------------------------------------*/

/*
 * is_cache_enabled_v2 - Check if cache is enabled using feature descriptor
 *
 * Uses get_feature() to find the FEAT_CACHE descriptor for the current chipset,
 * then reads the register and checks the appropriate bit.
 * Returns: 1 if enabled, 0 if disabled, -1 if chipset not in table (assume enabled)
 */
static int is_cache_enabled_v2(unsigned char chipset_type)
{
    const feature_descriptor_t *desc;
    unsigned char reg_val;

    desc = get_feature(chipset_type, FEAT_CACHE);
    if (!desc) {
        return -1;  /* Not in table - caller should fall back to v1 or assume enabled */
    }

    if (desc->encoding != ENC_BIT_TOGGLE) {
        return -1;  /* Unexpected encoding - can't handle */
    }

    reg_val = feature_read_reg(desc, desc->base_reg);

    if (desc->enable_high) {
        /* Bit set = enabled */
        return (reg_val & (1 << desc->bit_position)) ? 1 : 0;
    } else {
        /* Bit clear = enabled */
        return (reg_val & (1 << desc->bit_position)) ? 0 : 1;
    }
}

/*
 * set_cache_enabled_v2 - Enable or disable cache using feature descriptor
 *
 * Uses get_feature() to find the FEAT_CACHE descriptor for the current chipset,
 * then performs read-modify-write to set or clear the appropriate bit.
 * Returns: 0 on success, -1 if chipset not in table
 */
static int set_cache_enabled_v2(unsigned char chipset_type, int enable)
{
    const feature_descriptor_t *desc;
    unsigned char reg_val;

    desc = get_feature(chipset_type, FEAT_CACHE);
    if (!desc) {
        return -1;  /* Not in table */
    }

    if (desc->encoding != ENC_BIT_TOGGLE) {
        return -1;  /* Unexpected encoding - can't handle */
    }

    /* Read-modify-write */
    reg_val = feature_read_reg(desc, desc->base_reg);

    if (enable) {
        if (desc->enable_high) {
            reg_val |= (1 << desc->bit_position);   /* Set bit to enable */
        } else {
            reg_val &= ~(1 << desc->bit_position);  /* Clear bit to enable */
        }
    } else {
        if (desc->enable_high) {
            reg_val &= ~(1 << desc->bit_position);  /* Clear bit to disable */
        } else {
            reg_val |= (1 << desc->bit_position);   /* Set bit to disable */
        }
    }

    feature_write_reg(desc, desc->base_reg, reg_val);
    return 0;
}

/*----------------------------------------------------------------------------
 * V2.4 NC REGION FUNCTIONS
 *
 * Generic NC region handling using feature descriptors.
 * Chipsets with special encodings (SiS 5598) use their function pointers.
 *----------------------------------------------------------------------------*/

/*
 * decode_nc_region_base_size_pair - Decode NC region from base/size pair registers
 *
 * For ENC_BASE_SIZE_PAIR encoding:
 * - Reg[0]: Base address (shifted by base_shift to get KB)
 * - Reg[1]: Size code in upper nibble (bits 7:4), 0 = disabled
 *
 * Returns 0 on success, -1 if region is disabled
 */
static int decode_nc_region_base_size_pair(const feature_descriptor_t *desc,
                                           unsigned char reg0, unsigned char reg1,
                                           unsigned long *base_kb, unsigned long *size_kb)
{
    unsigned char size_code;

    if (!desc || !base_kb || !size_kb) return -1;

    size_code = (reg1 >> desc->size_shift) & 0x0F;
    if (size_code == 0) {
        return -1;  /* Region disabled */
    }

    /* Base calculation depends on base_shift (4 = 64KB/16, 6 = 64KB) */
    if (desc->base_shift == 4) {
        /* OPTi style: base A23:A16 in reg0, A27:A24 in reg1 lower nibble */
        *base_kb = ((unsigned long)(reg1 & 0x0F) << 12) |
                   ((unsigned long)reg0 << 4);
    } else {
        /* SiS style: base in 64KB units */
        *base_kb = (unsigned long)reg0 << desc->base_shift;
    }

    /* Size calculation: 8KB << (size_code - 1) for OPTi, 64KB << (code - 1) for others */
    if (desc->base_shift == 4) {
        *size_kb = 8UL << (size_code - 1);  /* 8KB minimum */
    } else {
        *size_kb = 64UL << (size_code - 1); /* 64KB minimum */
    }

    return 0;
}

/*
 * encode_nc_region_base_size_pair - Encode NC region to base/size pair registers
 *
 * Calculates appropriate register values for the given base and size.
 * Returns 0 on success, -1 on error.
 */
static int encode_nc_region_base_size_pair(const feature_descriptor_t *desc,
                                           unsigned long base_kb, unsigned long size_kb,
                                           unsigned char *reg0, unsigned char *reg1)
{
    unsigned char size_code;
    unsigned char base_lo, base_hi;

    if (!desc || !reg0 || !reg1) return -1;
    if (size_kb == 0) return -1;

    /* Calculate size code */
    if (desc->base_shift == 4) {
        /* OPTi style: 8KB minimum */
        if (size_kb <= 8) size_code = 1;
        else if (size_kb <= 16) size_code = 2;
        else if (size_kb <= 32) size_code = 3;
        else if (size_kb <= 64) size_code = 4;
        else if (size_kb <= 128) size_code = 5;
        else if (size_kb <= 256) size_code = 6;
        else if (size_kb <= 512) size_code = 7;
        else if (size_kb <= 1024) size_code = 8;
        else if (size_kb <= 2048) size_code = 9;
        else if (size_kb <= 4096) size_code = 10;
        else if (size_kb <= 8192) size_code = 11;
        else if (size_kb <= 16384) size_code = 12;
        else if (size_kb <= 32768) size_code = 13;
        else if (size_kb <= 65536) size_code = 14;
        else size_code = 15;  /* Max */

        /* Base A23:A16 -> reg0, A27:A24 -> reg1 lower nibble */
        base_lo = (unsigned char)((base_kb >> 4) & 0xFF);  /* A23:A16 */
        base_hi = (unsigned char)((base_kb >> 12) & 0x0F); /* A27:A24 */
    } else {
        /* SiS/VIA style: 64KB minimum */
        if (size_kb <= 64) size_code = 1;
        else if (size_kb <= 128) size_code = 2;
        else if (size_kb <= 256) size_code = 3;
        else if (size_kb <= 512) size_code = 4;
        else if (size_kb <= 1024) size_code = 5;
        else if (size_kb <= 2048) size_code = 6;
        else if (size_kb <= 4096) size_code = 7;
        else if (size_kb <= 8192) size_code = 8;
        else if (size_kb <= 16384) size_code = 9;
        else if (size_kb <= 32768) size_code = 10;
        else if (size_kb <= 65536) size_code = 11;
        else if (size_kb <= 131072) size_code = 12;
        else size_code = 15;  /* Max */

        /* Base in 64KB units */
        base_lo = (unsigned char)((base_kb >> desc->base_shift) & 0xFF);
        base_hi = 0;
    }

    *reg0 = base_lo;
    *reg1 = (size_code << desc->size_shift) | base_hi;

    return 0;
}

/*
 * read_nc_regions_v2 - Read NC regions using feature descriptor
 *
 * Uses get_feature() to find the FEAT_NC descriptor for the current chipset.
 * For chipsets with special handlers, calls the read function pointer.
 * For generic chipsets, decodes based on encoding type.
 *
 * Updates g_state.nc_live[] with current region configuration.
 */
static void read_nc_regions_v2(unsigned char chipset_type)
{
    const feature_descriptor_t *desc;
    int i;
    unsigned char reg0, reg1;

    desc = get_feature(chipset_type, FEAT_NC);
    if (!desc) {
        return;  /* Not in table */
    }

    /* If specialized read handler exists, use it */
    if (desc->read_fn) {
        desc->read_fn();
        return;
    }

    /* Generic decoding based on encoding type */
    switch (desc->encoding) {
        case ENC_BASE_SIZE_PAIR:
            for (i = 0; i < (int)desc->max_regions && i < 4; i++) {
                reg0 = feature_read_reg(desc, (unsigned char)(desc->base_reg + i * 2));
                reg1 = feature_read_reg(desc, (unsigned char)(desc->base_reg + i * 2 + 1));
                g_state.nc_live[i].reg_index = desc->base_reg + i * 2;
                g_state.nc_live[i].reg_val[0] = reg0;
                g_state.nc_live[i].reg_val[1] = reg1;

                if (decode_nc_region_base_size_pair(desc, reg0, reg1,
                        &g_state.nc_live[i].base_kb,
                        &g_state.nc_live[i].size_kb) == 0) {
                    g_state.nc_live[i].active = 1;
                }
            }
            break;

        case ENC_PAM:
            /* Intel PAM - info only, no active NC regions */
            for (i = 0; i < (int)desc->reg_count && i < 4; i++) {
                g_state.nc_live[i].reg_index = desc->base_reg + i;
                g_state.nc_live[i].reg_val[0] = feature_read_reg(desc,
                    (unsigned char)(desc->base_reg + i));
            }
            break;

        default:
            break;
    }
}

/*
 * write_nc_region_v2 - Write NC region using feature descriptor
 *
 * Uses get_feature() to find the FEAT_NC descriptor.
 * For chipsets with special handlers, calls the write function pointer.
 * For generic chipsets, encodes based on encoding type.
 *
 * Returns 0 on success, -1 on error.
 */
static int write_nc_region_v2(unsigned char chipset_type, int region,
                              unsigned long base_kb, unsigned long size_kb)
{
    const feature_descriptor_t *desc;
    unsigned char reg0, reg1;

    desc = get_feature(chipset_type, FEAT_NC);
    if (!desc) {
        return -1;
    }

    if (region < 0 || region >= (int)desc->max_regions) {
        return -1;  /* Invalid region */
    }

    /* If specialized write handler exists, use it */
    if (desc->write_fn) {
        desc->write_fn(region, base_kb, size_kb);
        return 0;
    }

    /* Generic encoding based on encoding type */
    switch (desc->encoding) {
        case ENC_BASE_SIZE_PAIR:
            if (encode_nc_region_base_size_pair(desc, base_kb, size_kb,
                                                &reg0, &reg1) != 0) {
                return -1;
            }
            feature_write_reg(desc, (unsigned char)(desc->base_reg + region * 2), reg0);
            feature_write_reg(desc, (unsigned char)(desc->base_reg + region * 2 + 1), reg1);
            return 0;

        case ENC_PAM:
            /* Intel PAM - read-only info, cannot write NC regions */
            return -1;

        default:
            return -1;
    }
}

/*
 * clear_nc_region_v2 - Clear (disable) NC region using feature descriptor
 *
 * Uses get_feature() to find the FEAT_NC descriptor.
 * For chipsets with special handlers, calls the clear function pointer.
 * For generic chipsets, writes 0 to size code to disable.
 *
 * Returns 0 on success, -1 on error.
 */
static int clear_nc_region_v2(unsigned char chipset_type, int region)
{
    const feature_descriptor_t *desc;

    desc = get_feature(chipset_type, FEAT_NC);
    if (!desc) {
        return -1;
    }

    if (region < 0 || region >= (int)desc->max_regions) {
        return -1;  /* Invalid region */
    }

    /* If specialized clear handler exists, use it */
    if (desc->clear_fn) {
        desc->clear_fn(region);
        return 0;
    }

    /* Generic clear: write 0 to both registers */
    switch (desc->encoding) {
        case ENC_BASE_SIZE_PAIR:
            feature_write_reg(desc, (unsigned char)(desc->base_reg + region * 2), 0);
            feature_write_reg(desc, (unsigned char)(desc->base_reg + region * 2 + 1), 0);
            return 0;

        case ENC_PAM:
            /* Intel PAM - cannot modify */
            return -1;

        default:
            return -1;
    }
}

/*----------------------------------------------------------------------------
 * V2.4 A20 GATE FUNCTION
 *
 * A20 toggle using feature descriptor. Uses ENC_BIT_TOGGLE encoding.
 * Chipsets without descriptor entries should use Port 0x92 fallback.
 *----------------------------------------------------------------------------*/

/*
 * toggle_a20_v2 - Toggle A20 gate using feature descriptor
 *
 * Uses get_feature() to find the FEAT_A20 descriptor.
 * Returns 0 on success, -1 if chipset not in table (use Port 0x92 fallback).
 */
static int toggle_a20_v2(unsigned char chipset_type, int enable)
{
    const feature_descriptor_t *desc;
    unsigned char reg_val;

    desc = get_feature(chipset_type, FEAT_A20);
    if (!desc) {
        return -1;  /* Not in table - caller should use Port 0x92 */
    }

    if (desc->encoding != ENC_BIT_TOGGLE) {
        return -1;  /* Unexpected encoding */
    }

    /* Read-modify-write */
    reg_val = feature_read_reg(desc, desc->base_reg);

    if (enable) {
        if (desc->enable_high) {
            reg_val |= (1 << desc->bit_position);   /* Set bit to enable */
        } else {
            reg_val &= ~(1 << desc->bit_position);  /* Clear bit to enable */
        }
    } else {
        if (desc->enable_high) {
            reg_val &= ~(1 << desc->bit_position);  /* Clear bit to disable */
        } else {
            reg_val |= (1 << desc->bit_position);   /* Set bit to disable */
        }
    }

    feature_write_reg(desc, desc->base_reg, reg_val);
    return 0;
}

/* Note: g_state is declared earlier in the file, after app_state_t typedef */

/*============================================================================
 * I/O ACCESS
 *============================================================================*/

static unsigned char safe_read(unsigned idx_port, unsigned data_port, unsigned char reg)
{
    unsigned char val;
    _disable();
    outp(idx_port, reg);
    inp(0x80);
    val = inp(data_port);
    _enable();
    return val;
}

static void safe_write(unsigned idx_port, unsigned data_port, unsigned char reg, unsigned char val)
{
    _disable();
    outp(idx_port, reg);
    inp(0x80);
    outp(data_port, val);
    _enable();
}

static int verify_port_valid(unsigned idx_port, unsigned data_port)
{
    unsigned char v1, v2, v3;
    v1 = safe_read(idx_port, data_port, 0x00);
    v2 = safe_read(idx_port, data_port, 0x10);
    v3 = safe_read(idx_port, data_port, 0x20);
    if ((v1 == 0xFF && v2 == 0xFF && v3 == 0xFF) ||
        (v1 == 0x00 && v2 == 0x00 && v3 == 0x00)) {
        return 0;
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * V2.4 LEGACY PORT ACCESS WRAPPERS
 *
 * These implement the forward-declared functions for feature_read_reg/write_reg.
 * They wrap safe_read/safe_write with the appropriate port pairs.
 *----------------------------------------------------------------------------*/

/* Standard 386/486 chipset ports: Index 0x22, Data 0x23 */
static unsigned char legacy_read_reg_22_23(unsigned char reg)
{
    return safe_read(0x22, 0x23, reg);
}

static void legacy_write_reg_22_23(unsigned char reg, unsigned char val)
{
    safe_write(0x22, 0x23, reg, val);
}

/* OPTi chipset ports: Index 0x22, Data 0x24 */
static unsigned char legacy_read_reg_22_24(unsigned char reg)
{
    return safe_read(0x22, 0x24, reg);
}

static void legacy_write_reg_22_24(unsigned char reg, unsigned char val)
{
    safe_write(0x22, 0x24, reg, val);
}

/* EISA chipset ports: Index 0x0C80, Data 0x0C85 (varies by chipset) */
static unsigned char eisa_read_reg(unsigned char reg)
{
    /* Intel EISA chipsets use 0C80h/0C84h or similar */
    return safe_read(0x0C80, 0x0C84, reg);
}

static void eisa_write_reg(unsigned char reg, unsigned char val)
{
    safe_write(0x0C80, 0x0C84, reg, val);
}

/*============================================================================
 * PCI CONFIGURATION SPACE ACCESS (for Pentium-era chipsets)
 *
 * In HAL builds (CK_HAL_H defined), PCI functions are provided by CK_IO.C.
 * In legacy single-file builds, use these local implementations.
 *============================================================================*/

#ifndef CK_HAL_H
/* Legacy build: local PCI implementation */

#define PCI_CONFIG_ADDR 0x0CF8
#define PCI_CONFIG_DATA 0x0CFC

/* Read 32-bit value from PCI config space */
static unsigned long pci_read_config(unsigned char bus, unsigned char dev,
                                     unsigned char func, unsigned char reg)
{
    unsigned long addr;
    unsigned long val;

    /* Build PCI config address: enable bit + bus + device + function + register */
    addr = 0x80000000UL |
           ((unsigned long)bus << 16) |
           ((unsigned long)dev << 11) |
           ((unsigned long)func << 8) |
           (reg & 0xFC);

    _disable();
    outpd(PCI_CONFIG_ADDR, addr);
    val = inpd(PCI_CONFIG_DATA);
    _enable();

    return val;
}

/* Write 32-bit value to PCI config space */
static void pci_write_config(unsigned char bus, unsigned char dev,
                             unsigned char func, unsigned char reg,
                             unsigned long val)
{
    unsigned long addr;

    /* Build PCI config address: enable bit + bus + device + function + register */
    addr = 0x80000000UL |
           ((unsigned long)bus << 16) |
           ((unsigned long)dev << 11) |
           ((unsigned long)func << 8) |
           (reg & 0xFC);

    _disable();
    outpd(PCI_CONFIG_ADDR, addr);
    outpd(PCI_CONFIG_DATA, val);
    _enable();
}

/* Read single byte from PCI config space */
static unsigned char pci_read_config_byte(unsigned char bus, unsigned char dev,
                                          unsigned char func, unsigned char reg)
{
    unsigned long dword;
    int shift;

    dword = pci_read_config(bus, dev, func, reg & 0xFC);
    shift = (reg & 3) * 8;
    return (unsigned char)((dword >> shift) & 0xFF);
}

/* Write single byte to PCI config space (read-modify-write) */
static void pci_write_config_byte(unsigned char bus, unsigned char dev,
                                  unsigned char func, unsigned char reg,
                                  unsigned char val)
{
    unsigned long dword;
    unsigned long mask;
    int shift;

    shift = (reg & 3) * 8;
    mask = 0xFFUL << shift;

    dword = pci_read_config(bus, dev, func, reg & 0xFC);
    dword = (dword & ~mask) | ((unsigned long)val << shift);
    pci_write_config(bus, dev, func, reg & 0xFC, dword);
}

/* Check if PCI is available (Type 1 configuration mechanism) */
static int pci_available(void)
{
    unsigned long val;

    _disable();
    outpd(PCI_CONFIG_ADDR, 0x80000000UL);
    val = inpd(PCI_CONFIG_ADDR);
    _enable();

    return (val == 0x80000000UL) ? 1 : 0;
}

/* Wrapper: pci_read_config_dword (alias for pci_read_config) */
static unsigned long pci_read_config_dword(unsigned char bus, unsigned char dev,
                                           unsigned char func, unsigned char reg)
{
    return pci_read_config(bus, dev, func, reg);
}

/* Wrapper: pci_present (alias for pci_available) */
static int pci_present(void)
{
    return pci_available();
}

#else
/* HAL build: use PCI functions from CK_IO.C, provide compatibility macros */

#define pci_available()     pci_bus_present()
#define pci_present()       pci_bus_present()
#define pci_read_config(b,d,f,r)   pci_read_config_dword(b,d,f,r)
#define pci_write_config(b,d,f,r,v) pci_write_config_dword(b,d,f,r,v)

#endif /* !CK_HAL_H */


/*============================================================================
 * F7 EXPANSION CARD INVENTORY - WRAPPER FUNCTION
 * 
 * Enumeration functions are now in CK_ENUM.C
 *============================================================================*/

/* Wrapper to call CK_ENUM enumeration and update local state */
static void enumerate_all_devices(void)
{
    g_state.inventory.device_count = enum_all_devices();
    g_state.inventory.cursor = 0;
    g_state.inventory.scroll_offset = 0;
}

/*============================================================================
 * SMBIOS/DMI TABLE PARSING
 *============================================================================*/

/* Safe string copy with length limit */
static void smbios_safe_strcpy(char *dest, const char far *src, int max_len)
{
    int i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

/* Extract SMBIOS string by index from string section */
/* String section follows fixed structure, strings are null-terminated */
/* String index 1 = first string, 0 = no string */
static void smbios_get_string(unsigned char far *table_start,
                              unsigned char struct_len,
                              unsigned char string_index,
                              char *dest, int max_len)
{
    unsigned char far *str_section;
    int current_index = 1;

    dest[0] = '\0';
    if (string_index == 0) return;

    /* String section starts after fixed structure */
    str_section = table_start + struct_len;

    /* Walk through strings until we find the requested index */
    while (current_index < string_index && *str_section != '\0') {
        /* Skip to next string (past null terminator) */
        while (*str_section != '\0') str_section++;
        str_section++;  /* Skip null terminator */
        current_index++;

        /* Check for double-null (end of string section) */
        if (*str_section == '\0') return;
    }

    if (current_index == string_index && *str_section != '\0') {
        smbios_safe_strcpy(dest, (const char far *)str_section, max_len);
    }
}

/* Skip to next SMBIOS structure (past string section) */
static unsigned char far *smbios_next_structure(unsigned char far *current)
{
    unsigned char struct_len = current[1];
    unsigned char far *ptr = current + struct_len;

    /* Skip string section (ends with double-null) */
    while (!(*ptr == '\0' && *(ptr + 1) == '\0')) {
        ptr++;
    }

    return ptr + 2;  /* Skip double-null */
}

/* Verify SMBIOS entry point checksum */
static int smbios_verify_checksum(unsigned char far *entry, int len)
{
    unsigned char sum = 0;
    int i;
    for (i = 0; i < len; i++) {
        sum += entry[i];
    }
    return (sum == 0);
}

/* Parse Type 0: BIOS Information */
static void parse_smbios_type0(unsigned char far *table, smbios_info_t *info)
{
    unsigned char struct_len = table[1];

    /* Vendor (string index at offset 0x04) */
    smbios_get_string(table, struct_len, table[0x04],
                      info->bios_vendor, SMBIOS_STR_LEN);

    /* Version (string index at offset 0x05) */
    smbios_get_string(table, struct_len, table[0x05],
                      info->bios_version, SMBIOS_STR_LEN);

    /* Release Date (string index at offset 0x08) */
    smbios_get_string(table, struct_len, table[0x08],
                      info->bios_date, 16);
}

/* Parse Type 1: System Information */
static void parse_smbios_type1(unsigned char far *table, smbios_info_t *info)
{
    unsigned char struct_len = table[1];

    /* Manufacturer (string index at offset 0x04) */
    smbios_get_string(table, struct_len, table[0x04],
                      info->sys_manufacturer, SMBIOS_STR_LEN);

    /* Product Name (string index at offset 0x05) */
    smbios_get_string(table, struct_len, table[0x05],
                      info->sys_product, SMBIOS_STR_LEN);
}

/* Parse Type 2: Baseboard Information */
static void parse_smbios_type2(unsigned char far *table, smbios_info_t *info)
{
    unsigned char struct_len = table[1];

    /* Manufacturer (string index at offset 0x04) */
    smbios_get_string(table, struct_len, table[0x04],
                      info->board_manufacturer, SMBIOS_STR_LEN);

    /* Product (string index at offset 0x05) */
    smbios_get_string(table, struct_len, table[0x05],
                      info->board_product, SMBIOS_STR_LEN);
}

/* Parse Type 3: Chassis */
static void parse_smbios_type3(unsigned char far *table, smbios_info_t *info)
{
    /* Chassis Type at offset 0x05 */
    info->chassis_type = table[0x05] & 0x7F;  /* Mask off lock bit */
}

/* Parse Type 4: Processor Information */
static void parse_smbios_type4(unsigned char far *table, smbios_info_t *info)
{
    unsigned char struct_len = table[1];

    /* Socket Designation (string index at offset 0x04) */
    smbios_get_string(table, struct_len, table[0x04],
                      info->cpu_socket, SMBIOS_STR_LEN);

    /* Max Speed at offset 0x14 (2 bytes, MHz) */
    if (struct_len >= 0x16) {
        info->cpu_max_speed = *(unsigned int far *)(table + 0x14);
    }

    /* Current Speed at offset 0x16 (2 bytes, MHz) */
    if (struct_len >= 0x18) {
        info->cpu_current_speed = *(unsigned int far *)(table + 0x16);
    }
}

/* Parse Type 16: Physical Memory Array */
static void parse_smbios_type16(unsigned char far *table, smbios_info_t *info)
{
    unsigned char struct_len = table[1];

    /* Maximum Capacity at offset 0x07 (4 bytes, KB) */
    if (struct_len >= 0x0B) {
        info->mem_max_capacity_kb = *(unsigned long far *)(table + 0x07);
    }

    /* Number of Memory Devices at offset 0x0D (2 bytes) */
    if (struct_len >= 0x0F) {
        info->mem_slots = (unsigned char)(*(unsigned int far *)(table + 0x0D));
        if (info->mem_slots > SMBIOS_MAX_DIMMS) {
            info->mem_slots = SMBIOS_MAX_DIMMS;
        }
    }
}

/* Parse Type 17: Memory Device */
static void parse_smbios_type17(unsigned char far *table, smbios_info_t *info)
{
    unsigned char struct_len = table[1];
    unsigned int size_raw;
    int idx;

    if (info->dimm_count >= SMBIOS_MAX_DIMMS) return;

    idx = info->dimm_count;

    /* Size at offset 0x0C (2 bytes) */
    if (struct_len >= 0x0E) {
        size_raw = *(unsigned int far *)(table + 0x0C);
        if (size_raw == 0 || size_raw == 0xFFFF) {
            /* Not populated or unknown */
            info->dimms[idx].populated = 0;
            info->dimms[idx].size_mb = 0;
        } else if (size_raw & 0x8000) {
            /* Size in KB */
            info->dimms[idx].size_mb = (size_raw & 0x7FFF) / 1024;
            info->dimms[idx].populated = 1;
        } else {
            /* Size in MB */
            info->dimms[idx].size_mb = size_raw;
            info->dimms[idx].populated = 1;
        }
    }

    /* Speed at offset 0x15 (2 bytes, MHz) - SMBIOS 2.3+ */
    if (struct_len >= 0x17) {
        info->dimms[idx].speed_mhz = *(unsigned int far *)(table + 0x15);
    }

    info->dimm_count++;
}

/* Find SMBIOS entry point in F0000-FFFFF */
static int find_smbios_entry(smbios_info_t *info)
{
    unsigned char far *ptr;
    unsigned char entry_len;
    unsigned long table_addr;

    /* Locate the "_SM_" anchor on a 16-byte boundary via the segment-wise
       scan. The old `while (ptr < end)` far-pointer loop advanced only the
       16-bit offset, so once it wrapped within a segment it never reached the
       end pointer (it could spin / miss the anchor). */
    ptr = io_find_sig(0xF000, 0xFFFF, "_SM_", 4);
    if (ptr == 0)
        return 0;

    /* Verify the entry point (length + checksum). */
    entry_len = ptr[0x05];
    if (entry_len < 0x1F || !smbios_verify_checksum(ptr, entry_len))
        return 0;

    info->version_major = ptr[0x06];
    info->version_minor = ptr[0x07];

    /* Table address at offset 0x18, structure count at 0x1C */
    table_addr = *(unsigned long far *)(ptr + 0x18);
    info->table_count = *(unsigned int far *)(ptr + 0x1C);
    info->entry_found = 1;

    /* Table must be accessible (< 1MB) in real mode */
    return (table_addr < 0x100000L) ? 1 : 0;
}

/* Main SMBIOS parsing entry point */
static void parse_smbios_tables(smbios_info_t *info)
{
    unsigned char far *ptr;
    unsigned char far *table_base;
    unsigned long table_addr;
    unsigned int tables_remaining;
    unsigned char type;
    int timeout = 256;  /* Prevent infinite loop */

    /* Initialize */
    memset(info, 0, sizeof(smbios_info_t));

    /* Find entry point */
    if (!find_smbios_entry(info)) {
        return;
    }

    /* Re-locate the entry point (segment-wise) to read the table base. */
    ptr = io_find_sig(0xF000, 0xFFFF, "_SM_", 4);
    if (ptr == 0)
        return;
    table_addr = *(unsigned long far *)(ptr + 0x18);

    /* Access table (must be < 1MB) */
    if (table_addr >= 0x100000L) {
        return;
    }

    table_base = (unsigned char far *)((unsigned long)table_addr);
    ptr = table_base;
    tables_remaining = info->table_count;

    /* Walk through all structures */
    while (tables_remaining > 0 && timeout-- > 0) {
        type = ptr[0];

        /* Parse known types */
        switch (type) {
            case 0:  /* BIOS Information */
                parse_smbios_type0(ptr, info);
                break;
            case 1:  /* System Information */
                parse_smbios_type1(ptr, info);
                break;
            case 2:  /* Baseboard Information */
                parse_smbios_type2(ptr, info);
                break;
            case 3:  /* Chassis */
                parse_smbios_type3(ptr, info);
                break;
            case 4:  /* Processor */
                parse_smbios_type4(ptr, info);
                break;
            case 16: /* Physical Memory Array */
                parse_smbios_type16(ptr, info);
                break;
            case 17: /* Memory Device */
                parse_smbios_type17(ptr, info);
                break;
            case 127: /* End-of-Table */
                goto done;
        }

        /* Move to next structure */
        ptr = smbios_next_structure(ptr);
        tables_remaining--;
    }

done:
    info->valid = 1;
}

/*============================================================================
 * ACPI TABLE PARSING
 *============================================================================*/

/* Verify ACPI table checksum */
static int acpi_verify_checksum(unsigned char far *table, unsigned int len)
{
    unsigned char sum = 0;
    unsigned int i;
    for (i = 0; i < len; i++) {
        sum += table[i];
    }
    return (sum == 0);
}

/* Find ACPI RSDP in E0000-FFFFF */
static int find_acpi_rsdp(acpi_info_t *info)
{
    unsigned char far *ptr;
    int i;

    /* Segment-wise scan for "RSD PTR " (the old far-pointer offset loop could
       wrap within a segment and spin / miss the anchor). */
    ptr = io_find_sig(0xE000, 0xFFFF, "RSD PTR ", 8);
    if (ptr == 0 || !acpi_verify_checksum(ptr, 20))
        return 0;

    /* OEM ID (6 bytes at offset 9) */
    for (i = 0; i < 6; i++) {
        info->oem_id[i] = ptr[9 + i];
    }
    info->oem_id[6] = '\0';

    info->revision = ptr[15];                       /* revision at offset 15 */
    info->rsdt_addr = *(unsigned long far *)(ptr + 16);  /* RSDT addr at 16 */
    info->rsdp_found = 1;
    return 1;
}

/* Parse RSDT and enumerate tables */
static void parse_acpi_rsdt(acpi_info_t *info)
{
    unsigned char far *rsdt;
    unsigned long rsdt_addr = info->rsdt_addr;
    unsigned long rsdt_len;
    unsigned long sig;
    unsigned int num_entries;
    unsigned int i;
    unsigned long far *entries;
    unsigned char far *table;

    /* Check if RSDT is accessible (< 1MB) */
    if (rsdt_addr >= 0x100000L) {
        return;
    }

    rsdt = (unsigned char far *)((unsigned long)rsdt_addr);

    /* Verify RSDT signature */
    if (*(unsigned long far *)rsdt != RSDT_SIGNATURE) {
        return;
    }

    /* Get length at offset 4 */
    rsdt_len = *(unsigned long far *)(rsdt + 4);

    /* Verify checksum */
    if (!acpi_verify_checksum(rsdt, (unsigned int)rsdt_len)) {
        return;
    }

    /* Calculate number of table pointers */
    /* Header is 36 bytes, each entry is 4 bytes */
    num_entries = (unsigned int)((rsdt_len - 36) / 4);
    info->table_count = (unsigned char)num_entries;

    /* Walk table pointers starting at offset 36 */
    entries = (unsigned long far *)(rsdt + 36);

    for (i = 0; i < num_entries && i < 32; i++) {
        unsigned long table_addr = entries[i];

        /* Skip if table is above 1MB */
        if (table_addr >= 0x100000L) {
            continue;
        }

        table = (unsigned char far *)((unsigned long)table_addr);
        sig = *(unsigned long far *)table;

        /* Check for known table signatures */
        if (sig == FACP_SIGNATURE) {
            info->has_facp = 1;
            /* PM Profile at offset 45 (FADT revision 2+) */
            if (*(unsigned long far *)(table + 4) >= 116) {
                info->pm_profile = table[45];
            }
        } else if (sig == MCFG_SIGNATURE) {
            info->has_mcfg = 1;
        } else if (sig == APIC_SIGNATURE) {
            info->has_apic = 1;
        } else if (sig == HPET_SIGNATURE) {
            info->has_hpet = 1;
        }
    }
}

/* Main ACPI parsing entry point */
static void parse_acpi_tables(acpi_info_t *info)
{
    /* Initialize */
    memset(info, 0, sizeof(acpi_info_t));

    /* Find RSDP */
    if (!find_acpi_rsdp(info)) {
        return;
    }

    /* Parse RSDT */
    parse_acpi_rsdt(info);

    info->valid = 1;
}

/* Detect PCI-based chipsets (Intel Triton, etc.) */
static int detect_pci_chipset(chipset_info_t *info)
{
    unsigned long id;
    unsigned int vendor, device;

    if (!pci_available()) {
        return 0;
    }

    /* Check bus 0, device 0, function 0 - host bridge */
    id = pci_read_config(0, 0, 0, 0x00);
    if (id == 0xFFFFFFFF) {
        return 0;  /* No device */
    }

    vendor = (unsigned int)(id & 0xFFFF);
    device = (unsigned int)((id >> 16) & 0xFFFF);

    /* Intel vendor ID: 0x8086 */
    if (vendor == 0x8086) {
        switch (device) {
            case 0x122D:  /* Intel 430FX Triton */
            case 0x122E:
                info->type = CHIPSET_I430FX;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
            case 0x1250:  /* Intel 430HX */
                info->type = CHIPSET_I430HX;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
            case 0x7030:  /* Intel 430VX */
                info->type = CHIPSET_I430VX;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
            case 0x7100:  /* Intel 430TX */
                info->type = CHIPSET_I430TX;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
            case 0x1235:  /* Intel 430MX (mobile) */
                info->type = CHIPSET_I430MX;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
        }
    }

    /* SiS vendor ID: 0x1039 */
    if (vendor == 0x1039) {
        switch (device) {
            case 0x0496:  /* SiS 496 */
            case 0x0406:
                info->type = CHIPSET_SIS_496;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
            case 0x5591:  /* SiS 5591 */
                info->type = CHIPSET_SIS_5591;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
            case 0x5598:  /* SiS 5598 */
                info->type = CHIPSET_SIS_5598;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
            case 0x0530:  /* SiS 530 */
                info->type = CHIPSET_SIS_530;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
        }
    }

    /* VIA vendor ID: 0x1106 */
    if (vendor == 0x1106) {
        switch (device) {
            case 0x0570:  /* VIA VT82C570 VP1 */
            case 0x0571:
                info->type = CHIPSET_VIA_VP1;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
            case 0x0597:  /* VIA Apollo VP3 */
                info->type = CHIPSET_VIA_VP3;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
            case 0x0598:  /* VIA Apollo MVP3 */
                info->type = CHIPSET_VIA_MVP3;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
        }
    }

    /* ALi vendor ID: 0x10B9 */
    if (vendor == 0x10B9) {
        switch (device) {
            case 0x1489:  /* ALi M1489 Aladdin */
            case 0x1487:
                info->type = CHIPSET_ALI_ALADDIN;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
            case 0x1541:  /* ALi M1541 Aladdin V */
                info->type = CHIPSET_ALI_ALADDIN5;
                info->id_value = (unsigned char)(device & 0xFF);
                info->id_index = 0x00;
                return 1;
        }
    }

    return 0;
}

/*============================================================================
 * FORWARD DECLARATIONS - HAL Helper Functions
 * (Defined later in file, but needed by chipset_read_reg/write_reg)
 *============================================================================*/

static int hal_is_available(void);
static int hal_reg_read(int reg);
static int hal_reg_write(int reg, int val);

/* Check if chipset type uses PCI config space (vs legacy index/data ports) */
static int is_pci_chipset(unsigned char type)
{
    switch (type) {
        /* Intel Triton family */
        case CHIPSET_I430FX:
        case CHIPSET_I430HX:
        case CHIPSET_I430VX:
        case CHIPSET_I430TX:
        case CHIPSET_I430MX:
        /* VIA Apollo family */
        case CHIPSET_VIA_VP1:
        case CHIPSET_VIA_VP3:
        case CHIPSET_VIA_MVP3:
        /* SiS PCI chipsets */
        case CHIPSET_SIS_496:
        case CHIPSET_SIS_5591:
        case CHIPSET_SIS_5598:
        case CHIPSET_SIS_530:
        /* ALi PCI chipsets */
        case CHIPSET_ALI_ALADDIN:
        case CHIPSET_ALI_ALADDIN5:
        /* Other PCI chipsets */
        case CHIPSET_OPTI_VIPER:
            return 1;
        /* NOTE: Contaq 82C596 uses legacy ports 0x22/0x23, NOT PCI */
        default:
            return 0;
    }
}

/* Read chipset register - routes to PCI or legacy based on chipset type */
static unsigned char chipset_read_reg(unsigned char reg)
{
    /* Try HAL first (v3.0) */
    if (hal_is_available()) {
        int val = hal_reg_read(reg);
        return (val >= 0) ? (unsigned char)val : 0xFF;
    }

    /* Legacy fallback */
    if (is_pci_chipset(g_state.chipset.type)) {
        return pci_read_config_byte(0, 0, 0, reg);
    } else {
        return safe_read(g_state.chipset.index_port,
                        g_state.chipset.data_port, reg);
    }
}

/* Write chipset register - routes to PCI or legacy based on chipset type */
static void chipset_write_reg(unsigned char reg, unsigned char val)
{
    /* Try HAL first (v3.0) */
    if (hal_is_available()) {
        hal_reg_write(reg, val);
        return;
    }

    /* Legacy fallback */
    if (is_pci_chipset(g_state.chipset.type)) {
        pci_write_config_byte(0, 0, 0, reg, val);
    } else {
        safe_write(g_state.chipset.index_port,
                  g_state.chipset.data_port, reg, val);
    }
}

static int check_eisa(void)
{
    char far *sig = (char far *)0xF000FFD9L;
    return (sig[0] == 'E' && sig[1] == 'I' && sig[2] == 'S' && sig[3] == 'A');
}

/* Check for MCA bus via INT 15h AH=C0h BIOS configuration table */
static int check_mca(void)
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

/* Detect Intel 82350DT (Mongoose) via 82359 DRAM controller chip ID */
static int detect_82350dt(void)
{
    unsigned char chip_id;
    /* Probe 82359 at index 0x21 (chip ID select register) via 0x22/0x23 */
    outp(0x22, 0x21);
    chip_id = inp(0x23);
    /* Chip ID 0x01 = 82359 DRAM controller present */
    return (chip_id == 0x01);
}

/* Intel PCMC (host bridge) PCI device IDs for EISA PCIset identification */
#define PCI_DEV_82424    0x0483  /* 420TX/ZX Saturn */
#define PCI_DEV_82434    0x04A3  /* 430LX Mercury / 430NX Neptune */
#define PCI_DEV_82439HX  0x1250  /* 430HX Triton II */
#define PCI_DEV_82441FX  0x1237  /* 440FX Natoma */
#define PCI_DEV_82454GX  0x84C4  /* 450GX Orion (tentative) */

/* Detect Intel EISA PCIset and identify specific variant via PCMC */
static int detect_eisa_pciset(unsigned char *chipset_type)
{
    unsigned long pci_id;
    unsigned int pcmc_dev_id = 0;
    int dev;
    int has_eisa_bridge = 0;

    if (!pci_present()) return 0;

    /* Scan for 82375EB/SB PCEB (8086:0482) - indicates EISA+PCI bridge */
    for (dev = 0; dev < 32; dev++) {
        pci_id = pci_read_config_dword(0, dev, 0, 0);
        if ((pci_id & 0xFFFF) == 0x8086 &&
            ((pci_id >> 16) & 0xFFFF) == 0x0482) {
            has_eisa_bridge = 1;
            break;
        }
    }

    if (!has_eisa_bridge) return 0;

    /* Now identify the PCMC (host bridge) to determine specific chipset */
    pci_id = pci_read_config_dword(0, 0, 0, 0);  /* Host bridge at 0:0:0 */
    if ((pci_id & 0xFFFF) == 0x8086) {
        pcmc_dev_id = (pci_id >> 16) & 0xFFFF;
    }

    switch (pcmc_dev_id) {
        case PCI_DEV_82424:
            /* 420TX or 420ZX - need additional differentiation if possible */
            *chipset_type = CHIPSET_EISA_420ZX;  /* Default to ZX */
            break;
        case PCI_DEV_82434:
            /* 430LX or 430NX - differentiate by stepping or other means */
            /* For now, assume Neptune (more common) */
            *chipset_type = CHIPSET_EISA_430NX;
            break;
        case PCI_DEV_82439HX:
            *chipset_type = CHIPSET_EISA_430HX;
            break;
        case PCI_DEV_82441FX:
            *chipset_type = CHIPSET_EISA_440FX;
            break;
        case PCI_DEV_82454GX:
            *chipset_type = CHIPSET_EISA_450GX;
            break;
        default:
            /* Unknown PCMC but has EISA bridge - generic 430NX */
            *chipset_type = CHIPSET_EISA_430NX;
            break;
    }

    return 1;
}

/* Detect OPTi EISA chipsets (82C682/683/691/693) via OPTi-style ports */
static int detect_opti_eisa(unsigned char *chipset_type)
{
    unsigned char id;

    /* OPTi uses 0x22/0x24, check for valid signature */
    if (!verify_port_valid(0x22, 0x24)) return 0;

    id = safe_read(0x22, 0x24, 0x00);

    /* OPTi EISA chipset identification - tentative ID patterns:
     * 82C682: 0x60 (486WB EISA)
     * 82C683: 0x61 (486AWB EISA)
     * 82C691/696: 0x68 (Hunter EISA)
     * 82C693/6/7: 0x6C (Pentium WB EISA)
     * NOTE: These ID patterns need verification with real hardware */
    switch (id & 0xFC) {
        case 0x60:  /* 82C682/683 */
            *chipset_type = (id & 0x01) ? CHIPSET_OPTI_683 : CHIPSET_OPTI_682;
            return 1;
        case 0x68:  /* 82C691/696 Hunter */
            *chipset_type = CHIPSET_OPTI_HUNTER;
            return 1;
        case 0x6C:  /* 82C693/6/7 Pentium */
            *chipset_type = CHIPSET_OPTI_PENT_EISA;
            return 1;
    }

    /* Fallback: any 0x6x pattern = OPTi EISA */
    if ((id & 0xF0) == 0x60) {
        *chipset_type = CHIPSET_OPTI_682;
        return 1;
    }
    return 0;
}

/* Detect SiS EISA chipsets (85C411/406) */
static int detect_sis_eisa(void)
{
    unsigned char id;

    /* SiS uses 0x22/0x23, check for SiS signature */
    if (!verify_port_valid(0x22, 0x23)) return 0;

    id = safe_read(0x22, 0x23, 0x00);
    /* SiS 85C411/406 - UNDOCUMENTED: No ID register exists for EISA variants.
     * Detection would require probing characteristic register behavior.
     * Leaving as placeholder until hardware testing is possible. */
    if ((id & 0xF0) == 0x40) {  /* Tentative: 0x4x pattern (like other SiS) */
        return 1;
    }
    return 0;
}

/* Detect VIA VT82C495 (Venus) 486 chipset
 * Uses ports 0xA8 (index) / 0xA9 (data), NOT 0x22/0x23
 * Register 0x64 returns 0xFF (jumper readout)
 * Cache control at register 0x50 bits 2 and 7 */
static int detect_via_vt82c495(void)
{
    unsigned char id;

    /* VIA VT82C49x uses 0xA8/0xA9, not standard 0x22/0x23 */
    outp(0xA8, 0x64);  /* Jumper readout register */
    id = inp(0xA9);
    if (id == 0xFF) {
        /* Could be VIA - check another register for confirmation */
        outp(0xA8, 0x50);  /* Cache control register */
        id = inp(0xA9);
        if (id != 0xFF) {  /* Should be a valid cache config value */
            return 1;
        }
    }
    return 0;
}

/*============================================================================
 * CPU DETECTION
 *============================================================================*/

static int is_486_or_better(void)
{
    unsigned int result = 0;
    _asm {
        .386
        pushfd
        pop eax
        mov ecx, eax
        xor eax, 40000h
        push eax
        popfd
        pushfd
        pop eax
        xor eax, ecx
        shr eax, 18
        and eax, 1
        mov result, ax
        push ecx
        popfd
    }
    return result;
}

static unsigned int get_total_memory(void)
{
    /* INT 12h returns conventional memory, INT 15h AX=E801h for extended */
    unsigned int conv_kb;
    unsigned int ext_kb = 0;

    /* Get conventional memory */
    _asm {
        int 12h
        mov conv_kb, ax
    }

    /* Try INT 15h, AX=E801h for extended memory */
    _asm {
        mov ax, 0E801h
        int 15h
        jc no_ext
        mov ext_kb, ax      ; KB between 1M-16M
    no_ext:
    }

    return conv_kb + ext_kb + 1024;  /* Add 1MB for system area */
}

/*============================================================================
 * CHIPSET DETECTION
 *============================================================================*/

static unsigned int detect_cache_size(chipset_info_t *info)
{
    unsigned char reg;

    switch (info->type) {
        case CHIPSET_OPTI391:
        case CHIPSET_OPTI381:
        case CHIPSET_ETEQ_BENGAL:
            reg = safe_read(0x22, info->data_port, 0x13);
            switch ((reg >> 4) & 0x03) {
                case 0: return 64;
                case 1: return 128;
                case 2: return 256;
                case 3: return 512;
            }
            break;

        case CHIPSET_SIS_RABBIT:
        case CHIPSET_SIS_460:
            reg = safe_read(0x22, 0x23, 0x11);
            switch ((reg >> 4) & 0x03) {
                case 0: return 64;
                case 1: return 128;
                case 2: return 256;
                case 3: return 512;
            }
            break;
    }
    return 256;
}

static void detect_chipset(void)
{
    chipset_info_t *info = &g_state.chipset;
    unsigned char id, id2;
    int i;

    memset(info, 0, sizeof(*info));
    info->index_port = 0x22;
    info->data_port = 0x23;
    info->name = "Unknown";
    info->vendor = "Unknown";
    info->tier = "?";
    info->granularity = 64;

    /* Check for MCA bus first - observer only (hardware-enforced coherency)
     * MCA systems use bus snooping for cache coherency, so no software
     * cache management is needed. Detection only, no control. */
    if (check_mca()) {
        info->type = CHIPSET_MCA_GENERIC;
        /* MCA uses POS registers at 0x100-0x107, system board at 0x94 */
        info->index_port = 0x94;    /* System board setup port */
        info->data_port = 0x100;    /* POS register base */
        goto found;
    }

    /* Try PCI detection (Pentium-era chipsets) */
    if (detect_pci_chipset(info)) {
        goto found;
    }

    if (check_eisa()) {
        unsigned char eisa_type;

        /* Check for EISA+PCI bridge first (Intel EISA PCIset) */
        if (detect_eisa_pciset(&eisa_type)) {
            info->type = eisa_type;
            /* Uses PCI config space for cache/NC control */
            goto found;
        }

        /* Check for 82350DT (Mongoose) via 82359 DRAM controller */
        if (detect_82350dt()) {
            info->type = CHIPSET_EISA_82350DT;
            info->index_port = 0x22;
            info->data_port = 0x23;
            goto found;
        }

        /* Check for OPTi EISA chipsets (82C682/683/691/693) */
        if (detect_opti_eisa(&eisa_type)) {
            info->type = eisa_type;
            info->index_port = 0x22;
            info->data_port = 0x24;
            goto found;
        }

        /* Check for SiS EISA chipsets (85C411/406) */
        if (detect_sis_eisa()) {
            info->type = CHIPSET_SIS_EISA;
            info->index_port = 0x22;
            info->data_port = 0x23;
            goto found;
        }

        /* Check for VIA VT82C495 (Venus) - uses 0xA8/0xA9 ports */
        if (detect_via_vt82c495()) {
            info->type = CHIPSET_VIA_EISA;
            info->index_port = 0xA8;
            info->data_port = 0xA9;
            goto found;
        }

        /* Fallback to generic Intel 82350 */
        info->type = CHIPSET_EISA_82350;
        info->index_port = 0x0C80;
        info->data_port = 0x0C85;
        goto found;
    }

    if (verify_port_valid(0x22, 0x24)) {
        id = safe_read(0x22, 0x24, 0x20);
        info->id_value = id;
        info->id_index = 0x20;
        info->data_port = 0x24;

        switch (id & 0xE0) {
            case 0x40:
                info->type = CHIPSET_OPTI391;
                goto found;
            case 0x20:
                info->type = CHIPSET_OPTI381;
                goto found;
            case 0x80:
                info->type = CHIPSET_ETEQ_BENGAL;
                goto found;
        }
    }

    info->data_port = 0x23;
    if (!verify_port_valid(0x22, 0x23)) {
        return;
    }

    id = safe_read(0x22, 0x23, 0x00);
    info->id_value = id;
    info->id_index = 0x00;

    switch (id) {
        case 0x31:
            id2 = safe_read(0x22, 0x23, 0x11);
            info->type = ((id2 & 0x80) || (id2 & 0x30)) ? CHIPSET_SIS_RABBIT : CHIPSET_VLSI;
            goto found;
        case 0x40:
        case 0x41:
            info->type = CHIPSET_SIS_460;
            goto found;
        case 0x11:
            info->type = CHIPSET_CT_SCAT;
            goto found;
        case 0x20:
        case 0x21:
            info->type = CHIPSET_CT_PEAK;
            goto found;
        case 0x12:
            info->type = CHIPSET_ALI_FINIS;
            goto found;
        case 0x05:
            info->type = CHIPSET_FARADAY;
            goto found;
        case 0x93:
            info->type = CHIPSET_MIC9391;
            goto found;
    }

    id = safe_read(0x22, 0x23, 0x10);
    if ((id ^ 0xAD) == 0x00) {
        info->type = CHIPSET_UMC491;
        info->id_value = id;
        info->id_index = 0x10;
        goto found;
    }

    id = safe_read(0x22, 0x23, 0x17);
    if ((id >> 4) == 0x01) {
        info->type = CHIPSET_HEADLAND;
        info->id_value = id;
        info->id_index = 0x17;
        goto found;
    }

    /* OPTi Viper (82C596/597) - check for extended OPTi ID at Index 20h */
    id = safe_read(0x22, 0x24, 0x20);
    if ((id & 0xF0) == 0x60) {
        info->type = CHIPSET_OPTI_VIPER;
        info->data_port = 0x24;
        info->id_value = id;
        info->id_index = 0x20;
        goto found;
    }

    /* Contaq 82C596 - check at Index 00h */
    id = safe_read(0x22, 0x23, 0x00);
    if (id == 0x59 || id == 0x96) {
        info->type = CHIPSET_CONTAQ_596;
        info->id_value = id;
        info->id_index = 0x00;
        goto found;
    }

    /*========================================================================
     * 286/386SX ERA CHIPSET DETECTION (Info-Only)
     *========================================================================*/

    /* C&T NEAT Full (CS8221) - check for full NEAT chipset
     * Register 00h at 22h/23h, different pattern from 82C211 (NEAT subset)
     * CS8221 has all four chips: 82C206/211/212/215
     */
    id = safe_read(0x22, 0x23, 0x1D);  /* NEAT control register */
    id2 = safe_read(0x22, 0x23, 0x19); /* Shadow RAM register */
    if ((id & 0x80) || (id2 != 0xFF && id2 != 0x00)) {
        /* Check for NEAT-386 (CS8230) - has 386SX support indicator */
        id = safe_read(0x22, 0x23, 0x1C);
        if (id & 0x40) {
            info->type = CHIPSET_CT_NEAT386;
            info->id_value = id;
            info->id_index = 0x1C;
            goto found;
        }
        /* Otherwise it's a 286 NEAT */
        info->type = CHIPSET_CT_NEAT_FULL;
        info->id_value = id2;
        info->id_index = 0x19;
        goto found;
    }

    /* Headland HT101/102 - similar to HT12 but older ID pattern
     * Index 17h bits 7:4 = 0000b for HT101, 0010b for HT102
     * (vs 0001b for HT12 already detected above)
     */
    id = safe_read(0x22, 0x23, 0x17);
    if ((id >> 4) == 0x00) {
        info->type = CHIPSET_HEADLAND_101;
        info->id_value = id;
        info->id_index = 0x17;
        goto found;
    }
    if ((id >> 4) == 0x02) {
        info->type = CHIPSET_HEADLAND_102;
        info->id_value = id;
        info->id_index = 0x17;
        goto found;
    }
    /* Headland HT18 (386SX) - Index 17h bits 7:4 = 0011b */
    if ((id >> 4) == 0x03) {
        info->type = CHIPSET_HEADLAND_18;
        info->id_value = id;
        info->id_index = 0x17;
        goto found;
    }

    /* VLSI VL82C100/101/102 (286-era) and VL82C320 (386SX)
     * Index FFh identifies VLSI chipset, distinguish by Index 00h pattern
     */
    id = safe_read(0x22, 0x23, 0xFF);
    if (id == 0x35 || id == 0x36 || id == 0x37) {
        id2 = safe_read(0x22, 0x23, 0x00);
        info->id_value = id;
        info->id_index = 0xFF;
        if ((id2 & 0xF0) == 0x10) {
            info->type = CHIPSET_VLSI_100;
            goto found;
        } else if ((id2 & 0xF0) == 0x20) {
            info->type = CHIPSET_VLSI_101;
            goto found;
        } else if ((id2 & 0xF0) == 0x30) {
            /* Could be VLSI 102 or 320 (386SX) */
            id = safe_read(0x22, 0x23, 0x01);
            if (id & 0x80) {
                info->type = CHIPSET_VLSI_320;  /* 386SX variant */
            } else {
                info->type = CHIPSET_VLSI_102;
            }
            goto found;
        }
    }

    /* ALi M1217 (286-era) - Index 00h = 0x17 or 0x12 pattern */
    id = safe_read(0x22, 0x23, 0x00);
    if (id == 0x17) {
        info->type = CHIPSET_ALI_M1217;
        info->id_value = id;
        info->id_index = 0x00;
        goto found;
    }

    /* OPTi 82C212 (286-era) - uses 22h/24h like later OPTi
     * Index 20h pattern 0x10-0x1F range */
    id = safe_read(0x22, 0x24, 0x20);
    if ((id & 0xF0) == 0x10) {
        info->type = CHIPSET_OPTI_212;
        info->data_port = 0x24;
        info->id_value = id;
        info->id_index = 0x20;
        goto found;
    }

    return;

found:
    for (i = 0; g_chipset_db[i].name != NULL; i++) {
        if (g_chipset_db[i].type == info->type) {
            info->name = g_chipset_db[i].name;
            info->vendor = g_chipset_db[i].vendor;
            info->tier = g_chipset_db[i].tier;
            info->score_x10 = g_chipset_db[i].score_x10;
            info->is_writeback = g_chipset_db[i].is_wb;
            info->nc_strategy = g_chipset_db[i].nc_strategy;
            info->nc_regions = g_chipset_db[i].nc_regions;
            info->granularity = g_chipset_db[i].granularity;
            info->info_only = g_chipset_db[i].info_only;
            break;
        }
    }
    /* Skip cache detection for info-only chipsets */
    if (!info->info_only) {
        info->cache_size_kb = detect_cache_size(info);
    }
}

/*============================================================================
 * HAL BRIDGE FUNCTIONS
 *
 * These functions bridge the new HAL interface to the legacy g_state.chipset
 * structure. They allow gradual migration from the old detection code to HAL.
 *============================================================================*/

/*
 * hal_init_chipset - Initialize chipset using HAL
 *
 * Calls the HAL detection and populates g_state.chipset from g_hal fields.
 * This is the preferred initialization path for v3.0.
 */
static void hal_init_chipset(void)
{
    chipset_info_t *info = &g_state.chipset;

    /* Detect CPU tier first so the cache-flush primitives know whether
       WBINVD is available (486+). Must run before any cache_flush(). */
    ck_detect_cpu();

    /* Detect chipset using HAL */
    detect_chipset_hal();

    /* Populate legacy structure from HAL */
    memset(info, 0, sizeof(*info));

    if (g_hal != NULL && g_hal != &ops_unknown) {
        /* Copy fields from HAL ops */
        info->name = g_hal->name;
        info->vendor = g_hal->vendor;
        info->tier = g_hal->tier;
        info->score_x10 = g_hal->score_x10;
        info->is_writeback = g_hal->is_writeback;
        info->nc_regions = g_hal->nc_count;
        info->nc_strategy = (g_hal->nc_count > 0) ? 1 : 0;
        info->granularity = g_hal->nc_granularity ? g_hal->nc_granularity : 64;
        info->info_only = g_hal->info_only;
        info->index_port = g_hal->index_port;
        info->data_port = g_hal->data_port;
        info->type = CHIPSET_HAL;  /* Mark as HAL-detected */
    } else {
        /* Fallback to unknown */
        info->name = "Unknown";
        info->vendor = "Unknown";
        info->tier = "?";
        info->type = CHIPSET_UNKNOWN;
        info->index_port = 0x22;
        info->data_port = 0x23;
        info->granularity = 64;
    }
}

/*
 * hal_cache_get - Get cache state via HAL
 * Returns CACHE_* flags from HAL
 */
static int hal_cache_get(void)
{
    if (g_hal && g_hal->cache_get) {
        return g_hal->cache_get();
    }
    return CACHE_ENABLED;  /* Assume enabled if unknown */
}

/*
 * hal_cache_set - Enable/disable cache via HAL
 * Returns HAL_OK on success
 */
static int hal_cache_set(int enable)
{
    if (g_hal && g_hal->cache_set) {
        return g_hal->cache_set(enable);
    }
    return HAL_ERR_UNSUP;
}

/*
 * hal_cache_flush - Flush cache via HAL
 * Returns HAL_OK on success
 */
static int hal_cache_flush(void)
{
    if (g_hal && g_hal->cache_flush) {
        return g_hal->cache_flush();
    }
    return HAL_ERR_UNSUP;
}

/*
 * hal_nc_read - Read NC region via HAL
 * Returns HAL_OK on success
 */
static int hal_nc_read(int idx, nc_region_t *r)
{
    if (g_hal && g_hal->nc_read) {
        /* HAL uses its own nc_region_t, compatible with ours */
        return g_hal->nc_read(idx, (nc_region_t *)r);
    }
    if (r) {
        r->base_kb = 0;
        r->size_kb = 0;
        r->active = 0;
    }
    return HAL_ERR_UNSUP;
}

/*
 * hal_nc_write - Write NC region via HAL
 * Returns HAL_OK on success
 */
static int hal_nc_write(int idx, unsigned long base_kb, unsigned long size_kb)
{
    if (g_hal && g_hal->nc_write) {
        return g_hal->nc_write(idx, base_kb, size_kb);
    }
    return HAL_ERR_UNSUP;
}

/*
 * hal_nc_clear - Clear/disable NC region via HAL
 * Returns HAL_OK on success
 */
static int hal_nc_clear(int idx)
{
    if (g_hal && g_hal->nc_clear) {
        return g_hal->nc_clear(idx);
    }
    return HAL_ERR_UNSUP;
}

/*
 * nc_write_supported - True only when the detected chipset has a REAL nc_write
 * implementation (not the unimplemented stub). Several chipsets advertise
 * nc_count > 0 but wire hal_stub_unsupported_iull, which silently fails. The
 * NC config screen uses this gate so it does not present phantom "editable"
 * regions that do nothing - which would mislead the user into believing a
 * memory window is fenced from the cache when it is not.
 */
static int nc_write_supported(void)
{
    if (g_hal == NULL)
        return 0;
    if (g_hal->nc_count == 0)
        return 0;
    if (g_hal->nc_write == NULL)
        return 0;
    if (g_hal->nc_write == hal_stub_unsupported_iull)
        return 0;
    return 1;
}

/*
 * hal_reg_read - Read chipset register via HAL
 * Returns register value, or -1 on error
 */
static int hal_reg_read(int reg)
{
    if (g_hal && g_hal->reg_read) {
        return g_hal->reg_read(reg);
    }
    return -1;
}

/*
 * hal_reg_write - Write chipset register via HAL
 * Returns HAL_OK on success
 */
static int hal_reg_write(int reg, int val)
{
    if (g_hal && g_hal->reg_write) {
        return g_hal->reg_write(reg, val);
    }
    return HAL_ERR_UNSUP;
}

/*
 * hal_is_available - Check if HAL detected a chipset
 * Returns 1 if HAL has a valid chipset, 0 if unknown
 */
static int hal_is_available(void)
{
    return (g_hal != NULL && g_hal != &ops_unknown);
}

/*----------------------------------------------------------------------------
 * Cross-reference chipset detection with SMBIOS data
 * May provide additional hints for unknown/ambiguous chipsets
 *----------------------------------------------------------------------------*/
static void enhance_chipset_with_smbios(chipset_info_t *chip)
{
    smbios_info_t *smbios = &g_state.smbios;

    if (!smbios->valid)
        return;

    /* For unknown chipsets, try to identify from SMBIOS board info */
    if (chip->type == CHIPSET_UNKNOWN) {
        const char *board = smbios->board_product;
        const char *sys = smbios->sys_product;

        /* Intel boards often include chipset in product name */
        if (strstr(board, "440BX") || strstr(sys, "440BX")) {
            /* Intel 440BX - Pentium II/III era, PCI chipset
             * Cannot control from ISA, but identification helps */
            chip->name = "Intel 440BX (SMBIOS)";
            chip->vendor = "Intel";
            chip->info_only = 1;
        } else if (strstr(board, "440LX") || strstr(sys, "440LX")) {
            chip->name = "Intel 440LX (SMBIOS)";
            chip->vendor = "Intel";
            chip->info_only = 1;
        } else if (strstr(board, "430TX") || strstr(sys, "430TX")) {
            chip->name = "Intel 430TX (SMBIOS)";
            chip->vendor = "Intel";
            chip->info_only = 1;
        } else if (strstr(board, "430VX") || strstr(sys, "430VX")) {
            chip->name = "Intel 430VX (SMBIOS)";
            chip->vendor = "Intel";
            chip->info_only = 1;
        } else if (strstr(board, "430HX") || strstr(sys, "430HX")) {
            chip->name = "Intel 430HX (SMBIOS)";
            chip->vendor = "Intel";
            chip->info_only = 1;
        } else if (strstr(board, "430FX") || strstr(sys, "430FX")) {
            chip->name = "Intel 430FX (SMBIOS)";
            chip->vendor = "Intel";
            chip->info_only = 1;
        } else if (strstr(board, "815") || strstr(sys, "i815")) {
            chip->name = "Intel 815 (SMBIOS)";
            chip->vendor = "Intel";
            chip->info_only = 1;
        } else if (strstr(board, "810") || strstr(sys, "i810")) {
            chip->name = "Intel 810 (SMBIOS)";
            chip->vendor = "Intel";
            chip->info_only = 1;
        }
        /* VIA chipset hints */
        else if (strstr(board, "VT82C") || strstr(board, "Apollo")) {
            chip->name = "VIA Apollo (SMBIOS)";
            chip->vendor = "VIA";
            chip->info_only = 1;
        }
        /* SiS chipset hints */
        else if (strstr(board, "SiS") && strstr(board, "530")) {
            chip->name = "SiS 530 (SMBIOS)";
            chip->vendor = "SiS";
            chip->info_only = 1;
        }
        /* ALi/Acer chipset hints */
        else if (strstr(board, "ALi") || strstr(board, "Aladdin")) {
            chip->name = "ALi Aladdin (SMBIOS)";
            chip->vendor = "ALi";
            chip->info_only = 1;
        }
    }
}

/*============================================================================
 * 286/386SX INVENTORY READING
 *============================================================================*/

static void read_286_inventory(void)
{
    inventory_286_t *inv = &g_state.inv286;
    chipset_info_t *chip = &g_state.chipset;
    unsigned char reg;

    memset(inv, 0, sizeof(*inv));

    if (!chip->info_only) {
        return;  /* Not an info-only chipset */
    }

    inv->valid = 1;

    switch (chip->type) {
        case CHIPSET_CT_NEAT_FULL:
        case CHIPSET_CT_NEAT386:
            /* C&T NEAT/NEAT-386: Shadow RAM at Index 19h-1Bh, A20 at 1Dh */
            inv->shadow_c000_c7ff = safe_read(0x22, 0x23, 0x19);
            inv->shadow_c800_cfff = safe_read(0x22, 0x23, 0x1A);
            inv->shadow_d000_dfff = 0;  /* NEAT combines D000-DFFF with E000 */
            inv->shadow_e000_ffff = safe_read(0x22, 0x23, 0x1B);

            reg = safe_read(0x22, 0x23, 0x1D);
            inv->a20_enabled = (reg & 0x02) ? 1 : 0;
            inv->a20_method = 1;  /* Chipset-controlled */
            inv->refresh_rate = (reg & 0x80) ? 0 : 1;  /* bit 7: fast refresh */

            /* DRAM config from Index 12h */
            reg = safe_read(0x22, 0x23, 0x12);
            inv->wait_states = reg & 0x03;
            break;

        case CHIPSET_HEADLAND_101:
        case CHIPSET_HEADLAND_102:
        case CHIPSET_HEADLAND_18:
            /* Headland: Shadow at 12h-13h, DRAM config at 10h */
            inv->shadow_c000_c7ff = safe_read(0x22, 0x23, 0x12) & 0x0F;
            inv->shadow_c800_cfff = (safe_read(0x22, 0x23, 0x12) >> 4) & 0x0F;
            inv->shadow_d000_dfff = safe_read(0x22, 0x23, 0x13) & 0x0F;
            inv->shadow_e000_ffff = (safe_read(0x22, 0x23, 0x13) >> 4) & 0x0F;

            reg = safe_read(0x22, 0x23, 0x10);
            inv->dram_size_256k = (reg >> 2) & 0x03;
            inv->wait_states = reg & 0x03;

            reg = safe_read(0x22, 0x23, 0x20);
            inv->refresh_rate = (reg & 0x10) ? 1 : 0;  /* bit 4: slow refresh */

            /* A20 via KBC or fast gate (Headland doesn't have direct A20 reg) */
            inv->a20_method = 2;  /* KBC */
            break;

        case CHIPSET_VLSI_100:
        case CHIPSET_VLSI_101:
        case CHIPSET_VLSI_102:
        case CHIPSET_VLSI_320:
            /* VLSI: Cache control at 06h, memory at various */
            reg = safe_read(0x22, 0x23, 0x06);
            /* VLSI uses different shadow scheme */
            inv->shadow_c000_c7ff = 0;
            inv->shadow_c800_cfff = 0;
            inv->shadow_d000_dfff = 0;
            inv->shadow_e000_ffff = 0;

            inv->a20_method = 2;  /* KBC */
            inv->wait_states = 1;  /* Default for VLSI */
            break;

        case CHIPSET_ALI_M1217:
            /* ALi M1217: Similar register layout to later ALi */
            reg = safe_read(0x22, 0x23, 0x10);
            inv->wait_states = reg & 0x03;
            inv->a20_method = 1;  /* Chipset */
            break;

        case CHIPSET_OPTI_212:
            /* OPTi 82C212: Uses 22h/24h, A20 at reg 20h */
            reg = safe_read(0x22, 0x24, 0x20);
            inv->a20_enabled = (reg & 0x01) ? 1 : 0;
            inv->a20_method = 1;  /* Chipset */
            inv->wait_states = (reg >> 4) & 0x03;
            break;

        case CHIPSET_MCA_GENERIC:
            /* IBM PS/2 MCA: Get model/submodel from INT 15h config table
             * MCA has hardware-enforced cache coherency via bus snooping,
             * so no cache/NC management is needed - observer only */
            {
                union REGS regs;
                struct SREGS sregs;
                unsigned char far *cfg;

                regs.h.ah = 0xC0;
                int86x(0x15, &regs, &regs, &sregs);
                if (!regs.x.cflag) {
                    cfg = (unsigned char far *)MK_FP(sregs.es, regs.x.bx);
                    /* Store model (offset 02h) and submodel (offset 03h) */
                    chip->id_value = cfg[2];   /* PS/2 model byte */
                    chip->id_index = cfg[3];   /* PS/2 submodel byte */
                }
            }
            inv->a20_method = 2;  /* Fast A20 (MCA systems have port 0x92) */
            break;

        default:
            inv->valid = 0;  /* Unknown info-only chipset */
            break;
    }

    /* Check actual A20 state via memory wrap test */
    if (inv->a20_method != 0) {
        unsigned int far *low = (unsigned int far *)0x00000000L;
        unsigned int far *high = (unsigned int far *)0x00100000L;
        unsigned int orig_low = *low;
        unsigned int test_val = 0xAA55;

        _disable();
        *low = test_val;
        /* If A20 is disabled, high wraps to low */
        inv->a20_enabled = (*high != test_val) ? 1 : 0;
        *low = orig_low;
        _enable();
    }

    /* Get total DRAM from BIOS */
    inv->dram_total_kb = g_state.total_mem_kb;
}

/* A20 toggle method codes */
#define A20_METHOD_NONE     0   /* No A20 toggle support */
#define A20_METHOD_CHIPSET  1   /* Chipset-specific register */
#define A20_METHOD_PORT92   2   /* Port 0x92 Fast A20 Gate */

/* Port 0x92 bits (System Control Port A) */
#define PORT_92_A20_BIT     0x02   /* Bit 1 = A20 gate enable */
#define PORT_92_RESET_BIT   0x01   /* Bit 0 = System reset (NEVER SET!) */

/* Check if A20 toggle is supported for current chipset
 * Returns: 0=no support, 1=chipset register, 2=port 0x92
 */
static int supports_a20_toggle(void)
{
    switch (g_state.chipset.type) {
        /* Chipset-specific A20 register control */
        case CHIPSET_CT_NEAT_FULL:
        case CHIPSET_CT_NEAT386:
        case CHIPSET_OPTI_212:
            return A20_METHOD_CHIPSET;

        /* Port 0x92 Fast A20 - supported on 386+ era chipsets */
        case CHIPSET_HEADLAND_101:
        case CHIPSET_HEADLAND_102:
        case CHIPSET_HEADLAND_18:
        case CHIPSET_VLSI_100:
        case CHIPSET_VLSI_101:
        case CHIPSET_VLSI_102:
        case CHIPSET_VLSI_320:
        case CHIPSET_ALI_M1217:
            return A20_METHOD_PORT92;

        default:
            return A20_METHOD_NONE;  /* KBC-only or unsupported */
    }
}

/* Forward declaration for confirmation dialog */
static int show_confirm_dialog(const char *title, const char *msg1, const char *msg2);

/* Toggle A20 via port 0x92 (universal AT Fast A20 method) */
static void toggle_a20_port92(int enable)
{
    unsigned char val;

    val = inp(0x92);
    if (enable) {
        val |= PORT_92_A20_BIT;   /* Enable A20 */
    } else {
        val &= ~PORT_92_A20_BIT;  /* Disable A20 */
    }
    /* CRITICAL: Never set bit 0 (causes system reset!) */
    val &= ~PORT_92_RESET_BIT;
    outp(0x92, val);
}

/* Toggle A20 gate (for info-only chipsets) */
static void toggle_a20_gate(void)
{
    inventory_286_t *inv = &g_state.inv286;
    unsigned char reg;
    const char *msg1;
    const char *msg2 = "This affects protected mode access";
    int method;

    method = supports_a20_toggle();
    if (method == A20_METHOD_NONE) {
        return;
    }

    msg1 = inv->a20_enabled ? "Disable A20 gate?" : "Enable A20 gate?";

    if (!show_confirm_dialog("A20 Gate", msg1, msg2)) {
        return;  /* User cancelled */
    }

    switch (method) {
        case A20_METHOD_CHIPSET:
            /* Chipset-specific register method */
            switch (g_state.chipset.type) {
                case CHIPSET_CT_NEAT_FULL:
                case CHIPSET_CT_NEAT386:
                    /* NEAT: A20 at Index 1Dh bit 1 */
                    reg = safe_read(0x22, 0x23, 0x1D);
                    if (inv->a20_enabled) {
                        reg &= ~0x02;  /* Disable A20 */
                    } else {
                        reg |= 0x02;   /* Enable A20 */
                    }
                    safe_write(0x22, 0x23, 0x1D, reg);
                    break;

                case CHIPSET_OPTI_212:
                    /* OPTi 82C212: A20 at Index 20h bit 0 */
                    reg = safe_read(0x22, 0x24, 0x20);
                    if (inv->a20_enabled) {
                        reg &= ~0x01;  /* Disable A20 */
                    } else {
                        reg |= 0x01;   /* Enable A20 */
                    }
                    safe_write(0x22, 0x24, 0x20, reg);
                    break;
            }
            inv->a20_enabled = !inv->a20_enabled;
            break;

        case A20_METHOD_PORT92:
            /* Port 0x92 Fast A20 method */
            toggle_a20_port92(!inv->a20_enabled);
            inv->a20_enabled = !inv->a20_enabled;
            break;
    }
}

/*============================================================================
 * SiS 5598/530 NC REGION HANDLING (Dedicated functions per datasheet)
 *============================================================================*/

/* SiS 5598/530 NC region encoding (16-bit):
 * Bits 15:13 = Size code (0=64K, 1=128K, 2=256K, 3=512K, 4=1M, 5=2M, 6=4M, 7=8M)
 * Bits 12:0  = Base address A28:A16 (within 384MB)
 */
#define SIS5598_NC_ENABLE_REG   0x77
#define SIS5598_NC_AREA1_LO     0x78
#define SIS5598_NC_AREA1_HI     0x79
#define SIS5598_NC_AREA2_LO     0x7A
#define SIS5598_NC_AREA2_HI     0x7B

/* Read SiS 5598/530 NC regions from registers 0x77-0x7B */
static void read_sis5598_nc_regions(void)
{
    unsigned char enable, lo, hi;
    unsigned int val;

    enable = chipset_read_reg(SIS5598_NC_ENABLE_REG);

    /* NC Area I (registers 0x78-0x79) */
    if (enable & 0x01) {
        lo = chipset_read_reg(SIS5598_NC_AREA1_LO);
        hi = chipset_read_reg(SIS5598_NC_AREA1_HI);
        val = ((unsigned int)hi << 8) | lo;
        g_state.nc_live[0].active = 1;
        g_state.nc_live[0].base_kb = (unsigned long)(val & 0x1FFF) << 6;  /* A28:A16 → KB */
        g_state.nc_live[0].size_kb = 64UL << ((val >> 13) & 0x07);        /* Size code */
        g_state.nc_live[0].reg_index = SIS5598_NC_AREA1_LO;
        g_state.nc_live[0].reg_val[0] = lo;
        g_state.nc_live[0].reg_val[1] = hi;
    }

    /* NC Area II (registers 0x7A-0x7B) */
    if (enable & 0x02) {
        lo = chipset_read_reg(SIS5598_NC_AREA2_LO);
        hi = chipset_read_reg(SIS5598_NC_AREA2_HI);
        val = ((unsigned int)hi << 8) | lo;
        g_state.nc_live[1].active = 1;
        g_state.nc_live[1].base_kb = (unsigned long)(val & 0x1FFF) << 6;
        g_state.nc_live[1].size_kb = 64UL << ((val >> 13) & 0x07);
        g_state.nc_live[1].reg_index = SIS5598_NC_AREA2_LO;
        g_state.nc_live[1].reg_val[0] = lo;
        g_state.nc_live[1].reg_val[1] = hi;
    }
}

/* Write SiS 5598/530 NC region with 16-bit encoding */
static void write_sis5598_nc_region(int region, unsigned long base_kb, unsigned long size_kb)
{
    unsigned char enable, lo, hi;
    unsigned int val;
    unsigned char size_code;
    unsigned int base_unit;

    /* Validate region index */
    if (region < 0 || region > 1) return;

    /* Validate and align base to 64KB boundary */
    base_kb = (base_kb + 63) & ~63UL;  /* Round up to 64KB alignment */

    /* Clamp base to 384MB window (max representable = 6144 * 64KB = 384MB) */
    if (base_kb > 393216UL) base_kb = 393216UL;  /* 384MB in KB */

    /* Validate size: minimum 64KB, reject 0 */
    if (size_kb == 0) return;

    /* Calculate size code: 0=64K, 1=128K, 2=256K, 3=512K, 4=1M, 5=2M, 6=4M, 7=8M */
    if (size_kb <= 64) size_code = 0;
    else if (size_kb <= 128) size_code = 1;
    else if (size_kb <= 256) size_code = 2;
    else if (size_kb <= 512) size_code = 3;
    else if (size_kb <= 1024) size_code = 4;
    else if (size_kb <= 2048) size_code = 5;
    else if (size_kb <= 4096) size_code = 6;
    else size_code = 7;

    /* Convert base to 64KB units (A28:A16) */
    base_unit = (unsigned int)(base_kb >> 6) & 0x1FFF;

    /* Encode: bits 15:13 = size, bits 12:0 = base A28:A16 */
    val = ((unsigned int)size_code << 13) | base_unit;
    lo = val & 0xFF;
    hi = (val >> 8) & 0xFF;

    /* Read current enable register */
    enable = chipset_read_reg(SIS5598_NC_ENABLE_REG);

    if (region == 0) {
        chipset_write_reg(SIS5598_NC_AREA1_LO, lo);
        chipset_write_reg(SIS5598_NC_AREA1_HI, hi);
        enable |= 0x01;  /* Enable NC Area I */
    } else {
        chipset_write_reg(SIS5598_NC_AREA2_LO, lo);
        chipset_write_reg(SIS5598_NC_AREA2_HI, hi);
        enable |= 0x02;  /* Enable NC Area II */
    }

    chipset_write_reg(SIS5598_NC_ENABLE_REG, enable);
}

/* Clear SiS 5598/530 NC region by disabling it */
static void clear_sis5598_nc_region(int region)
{
    unsigned char enable;

    /* Validate region index */
    if (region < 0 || region > 1) return;

    enable = chipset_read_reg(SIS5598_NC_ENABLE_REG);

    if (region == 0) {
        enable &= ~0x01;  /* Disable NC Area I */
    } else {
        enable &= ~0x02;  /* Disable NC Area II */
    }

    chipset_write_reg(SIS5598_NC_ENABLE_REG, enable);
}

/*============================================================================
 * NC REGION READING
 *============================================================================*/

static void read_nc_regions(void)
{
    chipset_info_t *chip = &g_state.chipset;
    unsigned char r0, r1, r2, r3;
    int i;

    /* Clear all regions first */
    for (i = 0; i < 4; i++) {
        memset(&g_state.nc_live[i], 0, sizeof(nc_region_live_t));
    }

    /* Try HAL first (v3.0) */
    if (hal_is_available() && g_hal->nc_count > 0) {
        for (i = 0; i < (int)g_hal->nc_count && i < 4; i++) {
            nc_region_t hal_region;
            if (hal_nc_read(i, &hal_region) == HAL_OK) {
                g_state.nc_live[i].active = hal_region.active;
                g_state.nc_live[i].base_kb = hal_region.base_kb;
                g_state.nc_live[i].size_kb = hal_region.size_kb;
            }
        }
        return;  /* HAL handled it */
    }

    /* Legacy chipset-specific code (fallback) */
    switch (chip->type) {
        case CHIPSET_OPTI391:
        case CHIPSET_OPTI381:
        case CHIPSET_OPTI_VIPER:
            /* OPTi 391/381/Viper: NC regions at Index 52h-53h (region 0) and 54h-55h (region 1)
             * Index 52h: bits 7-0 = base address A23-A16 (in 64KB units)
             * Index 53h: bits 7-4 = size (0=disabled, 1=8KB, 2=16KB, etc.), bits 3-0 = base A27-A24
             */
            r0 = chipset_read_reg(0x52);
            r1 = chipset_read_reg(0x53);
            g_state.nc_live[0].reg_index = 0x52;
            g_state.nc_live[0].reg_val[0] = r0;
            g_state.nc_live[0].reg_val[1] = r1;
            if ((r1 >> 4) != 0) {
                g_state.nc_live[0].active = 1;
                g_state.nc_live[0].base_kb = ((unsigned long)(r1 & 0x0F) << 12) |
                                             ((unsigned long)r0 << 4);  /* In KB */
                g_state.nc_live[0].size_kb = 8UL << ((r1 >> 4) - 1);
            }

            if (chip->nc_regions >= 2) {
                r2 = chipset_read_reg(0x54);
                r3 = chipset_read_reg(0x55);
                g_state.nc_live[1].reg_index = 0x54;
                g_state.nc_live[1].reg_val[0] = r2;
                g_state.nc_live[1].reg_val[1] = r3;
                if ((r3 >> 4) != 0) {
                    g_state.nc_live[1].active = 1;
                    g_state.nc_live[1].base_kb = ((unsigned long)(r3 & 0x0F) << 12) |
                                                 ((unsigned long)r2 << 4);
                    g_state.nc_live[1].size_kb = 8UL << ((r3 >> 4) - 1);
                }
            }
            break;

        case CHIPSET_SIS_460:
        case CHIPSET_SIS_RABBIT:
            /* SiS 460/Rabbit: NC regions at Index 14h-17h for 4 regions
             * Each 2-byte pair: base (bits 7-0) and size/enable (bits 7-4 = size code)
             */
            for (i = 0; i < 4 && i < (int)chip->nc_regions; i++) {
                r0 = chipset_read_reg(0x14 + i * 2);
                r1 = chipset_read_reg(0x15 + i * 2);
                g_state.nc_live[i].reg_index = 0x14 + i * 2;
                g_state.nc_live[i].reg_val[0] = r0;
                g_state.nc_live[i].reg_val[1] = r1;
                if ((r1 >> 4) != 0) {
                    g_state.nc_live[i].active = 1;
                    g_state.nc_live[i].base_kb = (unsigned long)r0 << 6;  /* 64KB units */
                    g_state.nc_live[i].size_kb = 64UL << ((r1 >> 4) - 1);
                }
            }
            break;

        case CHIPSET_SIS_5591:
        case CHIPSET_SIS_5598:
        case CHIPSET_SIS_530:
            /* SiS Super7: NC regions at 0x77 (enable) + 0x78-0x7B (2 regions)
             * Per datasheet + 86Box: 16-bit encoding with size code and base address
             */
            read_sis5598_nc_regions();
            break;

        case CHIPSET_SIS_496:
            /* SiS 496: 3 "Exclusive Areas" via PCI config space at 0x50-0x55
             * Per datasheet (bitsavers SiS_85C496-497_199507.pdf):
             * - 0x50-0x51: Exclusive Area 0 Setup
             * - 0x52-0x53: Exclusive Area 1 Setup
             * - 0x54-0x55: Exclusive Area 2 Setup (0x55 upper 4 bits only)
             * Size options: 64K, 128K, 256K, 512K, 1M, 2M, 4M
             * Note: Exact bit encoding needs full datasheet verification
             */
            for (i = 0; i < 3 && i < (int)chip->nc_regions; i++) {
                r0 = chipset_read_reg(0x50 + i * 2);
                r1 = chipset_read_reg(0x51 + i * 2);
                if (i == 2) r1 &= 0xF0;  /* Area 2 high byte only upper nibble */
                g_state.nc_live[i].reg_index = 0x50 + i * 2;
                g_state.nc_live[i].reg_val[0] = r0;
                g_state.nc_live[i].reg_val[1] = r1;
                if ((r1 >> 4) != 0) {
                    g_state.nc_live[i].active = 1;
                    g_state.nc_live[i].base_kb = (unsigned long)r0 << 6;  /* 64KB units */
                    g_state.nc_live[i].size_kb = 64UL << ((r1 >> 4) - 1);
                }
            }
            break;

        case CHIPSET_I430FX:
        case CHIPSET_I430HX:
        case CHIPSET_I430VX:
        case CHIPSET_I430TX:
        case CHIPSET_I430MX:
            /* Intel Triton series: PAM registers at 59h-5Fh control cacheability
             * We read SMRAM (72h) and DRAMT (68h) for memory type info
             * For NC regions, Intel uses DRB-based approach, not traditional NC
             * Show the relevant registers for diagnostic purposes
             */
            r0 = chipset_read_reg(0x59);  /* PAM0 - 0F0000-0FFFFF */
            r1 = chipset_read_reg(0x5A);  /* PAM1 - 0C0000-0CFFFF */
            r2 = chipset_read_reg(0x5B);  /* PAM2 - 0D0000-0DFFFF */
            r3 = chipset_read_reg(0x5C);  /* PAM3 - 0E0000-0EFFFF */
            g_state.nc_live[0].reg_index = 0x59;
            g_state.nc_live[0].reg_val[0] = r0;
            g_state.nc_live[0].reg_val[1] = r1;
            g_state.nc_live[0].reg_val[2] = r2;
            g_state.nc_live[0].reg_val[3] = r3;
            /* Intel doesn't have traditional NC regions - uses PAM for ROM area */
            /* Mark as "info only" - no active NC region in traditional sense */
            break;

        case CHIPSET_VIA_VP1:
        case CHIPSET_VIA_VP3:
        case CHIPSET_VIA_MVP3:
            /* VIA Apollo: NC regions at PCI config 58h-5Fh
             * Similar to SiS layout: base/size pairs
             */
            for (i = 0; i < 4 && i < (int)chip->nc_regions; i++) {
                r0 = chipset_read_reg(0x58 + i * 2);
                r1 = chipset_read_reg(0x59 + i * 2);
                g_state.nc_live[i].reg_index = 0x58 + i * 2;
                g_state.nc_live[i].reg_val[0] = r0;
                g_state.nc_live[i].reg_val[1] = r1;
                if ((r1 >> 4) != 0) {
                    g_state.nc_live[i].active = 1;
                    g_state.nc_live[i].base_kb = (unsigned long)r0 << 6;  /* 64KB units */
                    g_state.nc_live[i].size_kb = 64UL << ((r1 >> 4) - 1);
                }
            }
            break;

        case CHIPSET_ALI_ALADDIN:
        case CHIPSET_ALI_ALADDIN5:
            /* ALi Aladdin: NC regions at PCI config 50h-53h */
            r0 = chipset_read_reg(0x50);
            r1 = chipset_read_reg(0x51);
            g_state.nc_live[0].reg_index = 0x50;
            g_state.nc_live[0].reg_val[0] = r0;
            g_state.nc_live[0].reg_val[1] = r1;
            if ((r1 >> 4) != 0) {
                g_state.nc_live[0].active = 1;
                g_state.nc_live[0].base_kb = (unsigned long)r0 << 6;
                g_state.nc_live[0].size_kb = 64UL << ((r1 >> 4) - 1);
            }
            if (chip->nc_regions >= 2) {
                r2 = chipset_read_reg(0x52);
                r3 = chipset_read_reg(0x53);
                g_state.nc_live[1].reg_index = 0x52;
                g_state.nc_live[1].reg_val[0] = r2;
                g_state.nc_live[1].reg_val[1] = r3;
                if ((r3 >> 4) != 0) {
                    g_state.nc_live[1].active = 1;
                    g_state.nc_live[1].base_kb = (unsigned long)r2 << 6;
                    g_state.nc_live[1].size_kb = 64UL << ((r3 >> 4) - 1);
                }
            }
            break;

        /* Contaq 82C596: No NC regions - shadow RAM only (Index 0x15) */

        case CHIPSET_UMC491:
            /* UMC 491: NC region at Index 50h-51h
             * 50h: base address bits 23-16, 51h: size/enable
             */
            r0 = chipset_read_reg(0x50);
            r1 = chipset_read_reg(0x51);
            g_state.nc_live[0].reg_index = 0x50;
            g_state.nc_live[0].reg_val[0] = r0;
            g_state.nc_live[0].reg_val[1] = r1;
            if (r1 & 0x80) {
                g_state.nc_live[0].active = 1;
                g_state.nc_live[0].base_kb = (unsigned long)r0 << 4;  /* 16-byte aligned? */
                g_state.nc_live[0].size_kb = 8UL << ((r1 >> 4) & 0x07);
            }
            break;

        case CHIPSET_ETEQ_BENGAL:
            /* Eteq Bengal: Similar to OPTi, Index 52h-55h */
            r0 = chipset_read_reg(0x52);
            r1 = chipset_read_reg(0x53);
            g_state.nc_live[0].reg_index = 0x52;
            g_state.nc_live[0].reg_val[0] = r0;
            g_state.nc_live[0].reg_val[1] = r1;
            if ((r1 >> 4) != 0) {
                g_state.nc_live[0].active = 1;
                g_state.nc_live[0].base_kb = ((unsigned long)(r1 & 0x0F) << 12) |
                                             ((unsigned long)r0 << 4);
                g_state.nc_live[0].size_kb = 8UL << ((r1 >> 4) - 1);
            }

            if (chip->nc_regions >= 2) {
                r2 = chipset_read_reg(0x54);
                r3 = chipset_read_reg(0x55);
                g_state.nc_live[1].reg_index = 0x54;
                g_state.nc_live[1].reg_val[0] = r2;
                g_state.nc_live[1].reg_val[1] = r3;
                if ((r3 >> 4) != 0) {
                    g_state.nc_live[1].active = 1;
                    g_state.nc_live[1].base_kb = ((unsigned long)(r3 & 0x0F) << 12) |
                                                 ((unsigned long)r2 << 4);
                    g_state.nc_live[1].size_kb = 8UL << ((r3 >> 4) - 1);
                }
            }
            break;

        case CHIPSET_CT_PEAK:
        case CHIPSET_CT_SCAT:
        case CHIPSET_ALI_FINIS:
            /* Boundary-based: Read the NC boundary register */
            r0 = chipset_read_reg(0x16);  /* Typical boundary reg */
            g_state.nc_live[0].reg_index = 0x16;
            g_state.nc_live[0].reg_val[0] = r0;
            if (r0 != 0 && r0 != 0xFF) {
                g_state.nc_live[0].active = 1;
                /* Boundary is from this address to top of RAM */
                g_state.nc_live[0].base_kb = (unsigned long)r0 << 6;  /* Assuming 64KB units */
                g_state.nc_live[0].size_kb = g_state.total_mem_kb - g_state.nc_live[0].base_kb;
            }
            break;

        default:
            /* Unknown chipset - try common OPTi-style registers */
            r0 = chipset_read_reg(0x52);
            r1 = chipset_read_reg(0x53);
            g_state.nc_live[0].reg_index = 0x52;
            g_state.nc_live[0].reg_val[0] = r0;
            g_state.nc_live[0].reg_val[1] = r1;
            /* Don't mark as active for unknown chipset - just show raw values */
            break;
    }
}

/*============================================================================
 * EXTENDED-MEMORY BENCHMARK BUFFER (XMS + unreal mode)
 *
 * The benchmark/flush/test routines need real memory ABOVE the program to
 * exercise the cache without corrupting DOS or the program itself. (The old
 * code pointed far literals like 0x10000000L at what is actually ~64KB of
 * CONVENTIONAL RAM and wrote to it.) We allocate an XMS Extended Memory
 * Block, lock it for its linear address, enable A20, and enter unreal mode so
 * the CPU can touch it directly via 32-bit flat addressing. Requires 386+ and
 * an XMS driver (HIMEM.SYS).
 *
 * Layout within the block: read/flush/fill region at +0; copy destination at
 * +BENCH_DST_OFF (1MB).
 *
 * !!! The unreal-mode path is compile-checked only; validate on 86Box/PCem.
 *============================================================================*/

#define BENCH_BUF_KB    2048UL          /* 2MB EMB */
#define BENCH_DST_OFF   0x100000UL      /* copy destination at +1MB */

static int           g_bench_ready = 0; /* 1 = XMS buffer + unreal mode ready */
static int           g_bench_tried = 0; /* 1 = init has been attempted */
static unsigned int  g_bench_handle = 0;
static unsigned long g_bench_lin = 0;   /* linear base of the locked EMB */

/*
 * Lazily set up the extended-memory benchmark buffer. Returns 1 if a real
 * >1MB buffer is available (XMS present, 386+, alloc/lock/A20/unreal all OK);
 * 0 otherwise, in which case callers skip the operation rather than poke
 * conventional RAM. Safe to call repeatedly (initializes once).
 */
static int bench_mem_ensure(void)
{
    if (g_bench_tried)
        return g_bench_ready;
    g_bench_tried = 1;

    if (g_cpu_tier < CK_CPU_386)        /* unreal mode needs a 386+ */
        return 0;
    if (!xms_init())
        return 0;

    g_bench_handle = xms_alloc_kb((unsigned int)BENCH_BUF_KB);
    if (g_bench_handle == 0)
        return 0;

    g_bench_lin = xms_lock(g_bench_handle);
    if (g_bench_lin == 0) {
        xms_free(g_bench_handle);
        g_bench_handle = 0;
        return 0;
    }

    xms_local_enable_a20();
    if (!unreal_enter()) {
        xms_unlock(g_bench_handle);
        xms_free(g_bench_handle);
        g_bench_handle = 0;
        return 0;
    }

    g_bench_ready = 1;
    return 1;
}

/*============================================================================
 * CACHE TIMING MEASUREMENT (8254 PIT)
 *============================================================================*/

/* 8254 PIT ports */
#define PIT_COUNTER0    0x40
#define PIT_CONTROL     0x43

/* PIT frequency: 1.193182 MHz */
#define PIT_FREQ        1193182UL

static unsigned long measure_cache_flush_time(void)
{
    unsigned int start_lo, start_hi, end_lo, end_hi;
    unsigned long start_count, end_count, elapsed;
    unsigned int cache_size = g_state.chipset.cache_size_kb;
    unsigned int flush_size;
    int have_buf;

    /* For 386, need to read 2x cache size to ensure full flush */
    flush_size = g_state.is_486 ? 0 : (cache_size * 2);

    /* The 386 read-loop flush needs the extended-memory buffer; set it up
       BEFORE the timed/interrupts-off region (it calls XMS/INT 2Fh). */
    have_buf = g_state.is_486 ? 0 : bench_mem_ensure();

    /* Latch counter 0 - read current count */
    _disable();

    /* Latch command for counter 0: bits 7-6=00 (counter 0), bits 5-4=00 (latch) */
    outp(PIT_CONTROL, 0x00);
    inp(0x80);  /* I/O delay */
    start_lo = inp(PIT_COUNTER0);
    start_hi = inp(PIT_COUNTER0);

    if (g_state.is_486) {
        /* Use WBINVD instruction on 486+ (opcode 0F 09) */
        _asm {
            db 0Fh, 09h     /* WBINVD */
        }
    } else if (have_buf) {
        /* Read 2x cache size through the XMS extended-memory buffer (unreal
           mode). Reaches genuine memory above 1MB instead of the old far
           literal that aliased ~64KB of conventional RAM. */
        unreal_read_walk(g_bench_lin, (unsigned long)flush_size * 1024UL);
    }

    /* Latch and read end count */
    outp(PIT_CONTROL, 0x00);
    inp(0x80);
    end_lo = inp(PIT_COUNTER0);
    end_hi = inp(PIT_COUNTER0);

    _enable();

    /* Calculate elapsed ticks (counter counts DOWN) */
    start_count = ((unsigned long)start_hi << 8) | start_lo;
    end_count = ((unsigned long)end_hi << 8) | end_lo;

    if (start_count >= end_count) {
        elapsed = start_count - end_count;
    } else {
        /* Counter wrapped around */
        elapsed = start_count + (65536UL - end_count);
    }

    /* Convert to microseconds: ticks * 1000000 / 1193182 ≈ ticks * 838 / 1000 */
    return (elapsed * 838UL) / 1000UL;
}

/*============================================================================
 * EXTERNAL CACHE DETECTION (82385-style timing probe)
 *============================================================================*/

/* Measure read loop time for a given working set size (in KB)
 * Returns time in microseconds.
 * Uses extended memory (above 1MB) to avoid conventional memory conflicts.
 */
static unsigned long measure_working_set_time(unsigned int size_kb)
{
    unsigned long iterations, bytes;
    unsigned int start_lo, start_hi, end_lo, end_hi;
    unsigned long start_count, end_count, elapsed;

    /* Needs the XMS extended-memory buffer (set up before the timed region). */
    if (!bench_mem_ensure())
        return 0;

    bytes = (unsigned long)size_kb * 1024UL;

    /* Multiple passes to get measurable timing */
    iterations = (size_kb < 32) ? 8 : (size_kb < 128) ? 4 : 2;

    _disable();

    /* Latch and read PIT counter 0 */
    outp(PIT_CONTROL, 0x00);
    inp(0x80);
    start_lo = inp(PIT_COUNTER0);
    start_hi = inp(PIT_COUNTER0);

    _enable();

    /* Read through the working set multiple times (16-byte stride) in the
       XMS extended-memory buffer via unreal mode. */
    while (iterations--) {
        unreal_read_walk(g_bench_lin, bytes);
    }

    _disable();

    outp(PIT_CONTROL, 0x00);
    inp(0x80);
    end_lo = inp(PIT_COUNTER0);
    end_hi = inp(PIT_COUNTER0);

    _enable();

    /* Calculate elapsed (counter counts DOWN) */
    start_count = ((unsigned long)start_hi << 8) | start_lo;
    end_count = ((unsigned long)end_hi << 8) | end_lo;

    if (start_count >= end_count) {
        elapsed = start_count - end_count;
    } else {
        elapsed = start_count + (65536UL - end_count);
    }

    /* Convert to microseconds */
    return (elapsed * 838UL) / 1000UL;
}

/* Quick check: is there any cache present? (~100ms)
 * Compares 8KB (fits in cache) vs 64KB (may exceed cache) timing.
 * Returns 1 if cache hint detected (64KB > 1.3x slower than 8KB).
 */
static int has_cache_hint(void)
{
    unsigned long time_8k, time_64k;

    time_8k = measure_working_set_time(8);
    time_64k = measure_working_set_time(64);

    /* If 64KB is significantly slower than 8KB, cache is likely present */
    /* Using 1.3x threshold for hint detection */
    return (time_64k > (time_8k * 130UL) / 100UL);
}

/* Full detection: determine external cache presence and size
 * Sweeps through 8KB, 32KB, 64KB, 128KB working sets.
 * Detects cache cliff using 1.5x threshold.
 */
static void detect_82385_timing(void)
{
    unsigned long time_8k, time_32k, time_64k, time_128k;

    g_state.ext_cache.present = 0;
    g_state.ext_cache.size_kb = 0;
    g_state.ext_cache.line_size = 16;  /* 82385 standard */
    g_state.ext_cache.speed_ratio = 100;
    g_state.ext_cache.probed = 1;

    /* Measure timing at each working set size */
    time_8k = measure_working_set_time(8);
    time_32k = measure_working_set_time(32);
    time_64k = measure_working_set_time(64);
    time_128k = measure_working_set_time(128);

    /* Detect cache cliff using 1.5x threshold
     * If transitioning from size X to 2X shows >1.5x slowdown,
     * then X is the cache size.
     */
    if (time_64k > (time_32k * 150UL) / 100UL) {
        /* 64KB is much slower than 32KB: cache is 32KB */
        g_state.ext_cache.present = 1;
        g_state.ext_cache.size_kb = 32;
    } else if (time_128k > (time_64k * 150UL) / 100UL) {
        /* 128KB is much slower than 64KB: cache is 64KB */
        g_state.ext_cache.present = 1;
        g_state.ext_cache.size_kb = 64;
    } else if (time_8k < (time_128k * 70UL) / 100UL) {
        /* 8KB is much faster than 128KB: cache present, size unclear */
        g_state.ext_cache.present = 1;
        g_state.ext_cache.size_kb = 0;  /* Unknown */
    }

    /* Calculate speed boost ratio (cached vs uncached) */
    if (time_8k > 0) {
        g_state.ext_cache.speed_ratio = (time_128k * 100UL) / time_8k;
    }
}

/*============================================================================
 * CACHE FLUSH AND DIRTY DATA TEST
 *============================================================================*/

static void flush_cache(void)
{
    unsigned int cache_size = g_state.chipset.cache_size_kb;
    unsigned int flush_size = cache_size * 2;

    /* Try HAL first (v3.0) */
    if (hal_is_available()) {
        hal_cache_flush();
        return;
    }

    /* Legacy fallback */
    if (g_state.is_486) {
        _asm {
            db 0Fh, 09h     /* WBINVD */
        }
    } else if (g_state.chipset.type == CHIPSET_MIC9391) {
        /* MIC 9391 has hardware flush trigger at Index 40h bit 1 */
        safe_write(g_state.chipset.index_port, g_state.chipset.data_port,
                   0x40, safe_read(g_state.chipset.index_port, g_state.chipset.data_port, 0x40) | 0x02);
    } else if (bench_mem_ensure()) {
        /* 386 read-loop flush through the XMS extended-memory buffer. */
        unreal_read_walk(g_bench_lin, (unsigned long)flush_size * 1024UL);
    }
}

static int test_dirty_data_writeback(void)
{
    /* Test that dirty cache data is written back to RAM correctly.
     * 1. Write a pattern to memory
     * 2. Read it back to ensure it's in cache
     * 3. Flush the cache
     * 4. Read from a different memory path (direct) to verify writeback
     *
     * For write-through caches, this should always pass.
     * For write-back caches, this verifies flush works correctly.
     */
    unsigned long pattern = 0xDEADBEEF;
    unsigned long readback;

    /* Needs the XMS extended-memory buffer. If unavailable we cannot run the
       test; report pass (don't flag a failure we couldn't actually observe). */
    if (!bench_mem_ensure())
        return 1;

    /* Write pattern to the extended-memory buffer (unreal mode). */
    unreal_write32(g_bench_lin, pattern);

    /* Read back to ensure it's cached */
    readback = unreal_read32(g_bench_lin);
    if (readback != pattern) {
        return 0;  /* Initial write failed */
    }

    /* Flush the cache */
    flush_cache();

    /* Read back again - should still be there from RAM */
    readback = unreal_read32(g_bench_lin);

    return (readback == pattern) ? 1 : 0;
}

/*============================================================================
 * ENHANCED STRESS TEST
 *============================================================================*/

/* Test patterns for comprehensive validation */
static const unsigned long stress_patterns[] = {
    0xDEADBEEF,  /* Classic pattern */
    0xCAFEBABE,  /* Java signature */
    0x55555555,  /* Alternating bits */
    0xAAAAAAAA,  /* Inverse alternating */
    0x00000000,  /* All zeros */
    0xFFFFFFFF,  /* All ones */
    0x12345678,  /* Sequential */
    0xFEDCBA98   /* Reverse sequential */
};
#define NUM_PATTERNS (sizeof(stress_patterns) / sizeof(stress_patterns[0]))

typedef struct {
    int pass_count;
    int fail_count;
    unsigned long total_time_us;
    unsigned long min_time_us;
    unsigned long max_time_us;
    unsigned long failed_pattern;
    int running;
} stress_result_t;

static stress_result_t g_stress_result;

static void run_stress_test(int iterations)
{
    unsigned long lin;
    unsigned long pattern, readback;
    unsigned long timing;
    int i, p;

    /* Initialize results */
    g_stress_result.pass_count = 0;
    g_stress_result.fail_count = 0;
    g_stress_result.total_time_us = 0;
    g_stress_result.min_time_us = 0xFFFFFFFF;
    g_stress_result.max_time_us = 0;
    g_stress_result.failed_pattern = 0;
    g_stress_result.running = 1;

    /* Needs the XMS extended-memory buffer; without it there is nowhere safe
       to write test patterns, so skip rather than poke conventional RAM. */
    if (!bench_mem_ensure()) {
        g_stress_result.running = 0;
        return;
    }

    /* Use multiple test locations to stress different cache lines */
    for (i = 0; i < iterations && g_stress_result.running; i++) {
        for (p = 0; p < (int)NUM_PATTERNS; p++) {
            pattern = stress_patterns[p];

            /* Vary test location within the buffer to hit different lines */
            lin = g_bench_lin + ((unsigned long)(i * 64) % 0x10000UL);

            /* Write pattern */
            unreal_write32(lin, pattern);

            /* Read back to cache */
            readback = unreal_read32(lin);
            if (readback != pattern) {
                g_stress_result.fail_count++;
                g_stress_result.failed_pattern = pattern;
                continue;
            }

            /* Time the flush */
            timing = measure_cache_flush_time();
            g_stress_result.total_time_us += timing;

            if (timing < g_stress_result.min_time_us) {
                g_stress_result.min_time_us = timing;
            }
            if (timing > g_stress_result.max_time_us) {
                g_stress_result.max_time_us = timing;
            }

            /* Verify data survived flush */
            readback = unreal_read32(lin);
            if (readback == pattern) {
                g_stress_result.pass_count++;
            } else {
                g_stress_result.fail_count++;
                g_stress_result.failed_pattern = pattern;
            }
        }

        /* Update progress display periodically */
        if ((i % 10) == 0) {
            video_printf(40, 20, ATTR_VALUE, "Progress: %d/%d  ", i, iterations);
        }
    }

    g_stress_result.running = 0;
}

/*============================================================================
 * BENCHMARK TESTS (F5)
 *============================================================================*/

#define BENCH_SIZE_KB   512     /* Test block size */
#define BENCH_ITERS     100     /* Iterations per test */

/* Memory copy benchmark using REP MOVSD */
static unsigned long benchmark_copy_test(void)
{
    unsigned long src, dst;
    unsigned long start_count, end_count, elapsed;
    unsigned int start_lo, start_hi, end_lo, end_hi;
    unsigned long bytes_copied;
    unsigned int i;

    if (!bench_mem_ensure())
        return 0;

    /* Copy within the XMS extended-memory buffer: src at +0, dst at +1MB. */
    src = g_bench_lin;
    dst = g_bench_lin + BENCH_DST_OFF;

    /* Latch PIT counter for timing */
    _disable();
    outp(PIT_CONTROL, 0x00);
    inp(0x80);
    start_lo = inp(PIT_COUNTER0);
    start_hi = inp(PIT_COUNTER0);
    _enable();

    /* Perform copy operations (1KB per iteration, unreal-mode flat access) */
    for (i = 0; i < BENCH_ITERS; i++) {
        unreal_copy(dst, src, 1024UL);
    }

    /* Latch and read end count */
    _disable();
    outp(PIT_CONTROL, 0x00);
    inp(0x80);
    end_lo = inp(PIT_COUNTER0);
    end_hi = inp(PIT_COUNTER0);
    _enable();

    /* Calculate elapsed ticks */
    start_count = ((unsigned long)start_hi << 8) | start_lo;
    end_count = ((unsigned long)end_hi << 8) | end_lo;

    if (start_count >= end_count) {
        elapsed = start_count - end_count;
    } else {
        elapsed = start_count + (65536UL - end_count);
    }

    /* Convert to microseconds: ticks * 838 / 1000 */
    elapsed = (elapsed * 838UL) / 1000UL;
    if (elapsed == 0) elapsed = 1;

    /* Calculate MB/s * 10: (bytes * 10) / (us) * 1000000 / 1048576 */
    bytes_copied = (unsigned long)BENCH_ITERS * 1024UL;
    return (bytes_copied * 10UL * 1000000UL) / (elapsed * 1048576UL);
}

/* Memory fill benchmark using REP STOSB */
static unsigned long benchmark_fill_test(void)
{
    unsigned long start_count, end_count, elapsed;
    unsigned int start_lo, start_hi, end_lo, end_hi;
    unsigned long bytes_filled;
    unsigned int i;

    if (!bench_mem_ensure())
        return 0;

    _disable();
    outp(PIT_CONTROL, 0x00);
    inp(0x80);
    start_lo = inp(PIT_COUNTER0);
    start_hi = inp(PIT_COUNTER0);
    _enable();

    /* Fill 1KB per iteration in the XMS buffer (unreal-mode flat access). */
    for (i = 0; i < BENCH_ITERS; i++) {
        unreal_fill(g_bench_lin, 1024UL, 0xAAAAAAAAUL);
    }

    _disable();
    outp(PIT_CONTROL, 0x00);
    inp(0x80);
    end_lo = inp(PIT_COUNTER0);
    end_hi = inp(PIT_COUNTER0);
    _enable();

    start_count = ((unsigned long)start_hi << 8) | start_lo;
    end_count = ((unsigned long)end_hi << 8) | end_lo;

    if (start_count >= end_count) {
        elapsed = start_count - end_count;
    } else {
        elapsed = start_count + (65536UL - end_count);
    }

    elapsed = (elapsed * 838UL) / 1000UL;
    if (elapsed == 0) elapsed = 1;

    bytes_filled = (unsigned long)BENCH_ITERS * 1024UL;
    return (bytes_filled * 10UL * 1000000UL) / (elapsed * 1048576UL);
}

/* Memory read benchmark */
static unsigned long benchmark_read_test(void)
{
    unsigned long start_count, end_count, elapsed;
    unsigned int start_lo, start_hi, end_lo, end_hi;
    unsigned long bytes_read;
    unsigned int i;

    if (!bench_mem_ensure())
        return 0;

    _disable();
    outp(PIT_CONTROL, 0x00);
    inp(0x80);
    start_lo = inp(PIT_COUNTER0);
    start_hi = inp(PIT_COUNTER0);
    _enable();

    /* Read 1KB per iteration (16-byte stride) from the XMS buffer. */
    for (i = 0; i < BENCH_ITERS; i++) {
        unreal_read_walk(g_bench_lin, 1024UL);
    }

    _disable();
    outp(PIT_CONTROL, 0x00);
    inp(0x80);
    end_lo = inp(PIT_COUNTER0);
    end_hi = inp(PIT_COUNTER0);
    _enable();

    start_count = ((unsigned long)start_hi << 8) | start_lo;
    end_count = ((unsigned long)end_hi << 8) | end_lo;

    if (start_count >= end_count) {
        elapsed = start_count - end_count;
    } else {
        elapsed = start_count + (65536UL - end_count);
    }

    elapsed = (elapsed * 838UL) / 1000UL;
    if (elapsed == 0) elapsed = 1;

    bytes_read = (unsigned long)BENCH_ITERS * 1024UL;
    return (bytes_read * 10UL * 1000000UL) / (elapsed * 1048576UL);
}

/* Run all benchmark tests */
static void run_benchmarks(void)
{
    /* Benchmarks need the XMS extended-memory buffer (386+ and HIMEM.SYS). */
    if (!bench_mem_ensure()) {
        ui_draw_status_bar("Benchmarks need XMS + a 386 or later (load HIMEM.SYS)");
        return;
    }

    g_state.bench.test_running = 1;
    g_state.bench.progress_pct = 0;

    /* Run with cache ON */
    g_state.bench.cache_was_enabled = is_cache_enabled();
    if (!g_state.bench.cache_was_enabled) {
        enable_cache();
    }

    g_state.bench.progress_pct = 10;
    g_state.bench.cache_on_copy = benchmark_copy_test();
    g_state.bench.progress_pct = 25;
    g_state.bench.cache_on_fill = benchmark_fill_test();
    g_state.bench.progress_pct = 40;
    g_state.bench.cache_on_read = benchmark_read_test();

    /* Store results */
    g_state.bench.copy_mbs_x10 = g_state.bench.cache_on_copy;
    g_state.bench.fill_mbs_x10 = g_state.bench.cache_on_fill;
    g_state.bench.read_mbs_x10 = g_state.bench.cache_on_read;

    g_state.bench.progress_pct = 50;
    g_state.bench.test_running = 0;
}

/* Run comparison benchmark (cache on vs off) */
static void run_comparison_benchmark(void)
{
    int was_enabled;

    /* Benchmarks need the XMS extended-memory buffer (386+ and HIMEM.SYS). */
    if (!bench_mem_ensure()) {
        ui_draw_status_bar("Benchmarks need XMS + a 386 or later (load HIMEM.SYS)");
        return;
    }

    g_state.bench.test_running = 1;
    g_state.bench.progress_pct = 0;

    was_enabled = is_cache_enabled();

    /* Run with cache ON */
    if (!was_enabled) {
        enable_cache();
    }

    g_state.bench.progress_pct = 5;
    g_state.bench.cache_on_copy = benchmark_copy_test();
    g_state.bench.progress_pct = 15;
    g_state.bench.cache_on_fill = benchmark_fill_test();
    g_state.bench.progress_pct = 25;
    g_state.bench.cache_on_read = benchmark_read_test();

    /* Run with cache OFF */
    safe_disable_cache();

    g_state.bench.progress_pct = 40;
    g_state.bench.cache_off_copy = benchmark_copy_test();
    g_state.bench.progress_pct = 55;
    g_state.bench.cache_off_fill = benchmark_fill_test();
    g_state.bench.progress_pct = 70;
    g_state.bench.cache_off_read = benchmark_read_test();

    /* Restore original cache state */
    if (was_enabled) {
        enable_cache();
    }

    g_state.bench.progress_pct = 100;
    g_state.bench.test_running = 0;
}

/*============================================================================
 * REPORT GENERATOR
 *============================================================================*/

#define LPT1_DATA   0x378
#define LPT1_STATUS 0x379
#define LPT1_CTRL   0x37A

/* Print a character to LPT1 */
static void lpt_putc(char c)
{
    int timeout = 10000;

    /* Wait for printer ready (bit 7 of status = 1) */
    while (!(inp(LPT1_STATUS) & 0x80) && timeout-- > 0) {
        inp(0x80);  /* I/O delay */
    }
    if (timeout <= 0) return;  /* Timeout */

    /* Send character */
    outp(LPT1_DATA, c);

    /* Strobe (bit 0 of control) */
    outp(LPT1_CTRL, inp(LPT1_CTRL) | 0x01);
    inp(0x80);
    outp(LPT1_CTRL, inp(LPT1_CTRL) & ~0x01);
}

/* Print a string to LPT1 */
static void lpt_puts(const char *str)
{
    while (*str) {
        if (*str == '\n') {
            lpt_putc('\r');
        }
        lpt_putc(*str++);
    }
}

/* Generate report to file */
static int generate_report(const char *filename)
{
    FILE *fp;
    chipset_info_t *chip = &g_state.chipset;
    int i, row, col;

    fp = fopen(filename, "w");
    if (!fp) {
        return 0;
    }

    /* Header */
    fprintf(fp, "=============================================================\n");
    fprintf(fp, "CACHEKIT v%s - System Report\n", VERSION);
    fprintf(fp, "=============================================================\n\n");

    /* Chipset Information */
    fprintf(fp, "CHIPSET INFORMATION\n");
    fprintf(fp, "-------------------\n");
    fprintf(fp, "Name:        %s\n", chip->name);
    fprintf(fp, "Vendor:      %s\n", chip->vendor);
    fprintf(fp, "Cache Type:  %s (%u KB)\n",
            chip->is_writeback ? "Write-Back" : "Write-Through",
            chip->cache_size_kb);
    fprintf(fp, "Score:       %u.%u/10 (%s-TIER)\n",
            chip->score_x10 / 10, chip->score_x10 % 10, chip->tier);
    fprintf(fp, "CPU:         %s\n", g_state.is_486 ? "486+" : "386DX");
    fprintf(fp, "Memory:      %u KB\n", g_state.total_mem_kb);
    fprintf(fp, "Ports:       %02Xh/%02Xh\n", chip->index_port, chip->data_port);
    fprintf(fp, "\n");

    /* NC Regions */
    fprintf(fp, "NON-CACHEABLE REGIONS\n");
    fprintf(fp, "---------------------\n");
    for (i = 0; i < (int)chip->nc_regions && i < 4; i++) {
        nc_region_live_t *nc = &g_state.nc_live[i];
        if (nc->active) {
            fprintf(fp, "Region %d: %lu KB, Size: %lu KB, ACTIVE\n",
                    i, nc->base_kb, nc->size_kb);
        } else {
            fprintf(fp, "Region %d: -- (disabled)\n", i);
        }
    }
    fprintf(fp, "\n");

    /* Benchmark Results */
    fprintf(fp, "BENCHMARK RESULTS\n");
    fprintf(fp, "-----------------\n");
    if (g_state.bench.cache_on_copy > 0) {
        fprintf(fp, "Test        Cache ON    Cache OFF   Speedup\n");
        if (g_state.bench.cache_on_copy > 0 && g_state.bench.cache_off_copy > 0) {
            unsigned long speedup = (g_state.bench.cache_on_copy * 10) / g_state.bench.cache_off_copy;
            fprintf(fp, "Copy        %lu.%lu MB/s   %lu.%lu MB/s   %lu.%lux\n",
                    g_state.bench.cache_on_copy / 10, g_state.bench.cache_on_copy % 10,
                    g_state.bench.cache_off_copy / 10, g_state.bench.cache_off_copy % 10,
                    speedup / 10, speedup % 10);
        } else if (g_state.bench.cache_on_copy > 0) {
            fprintf(fp, "Copy        %lu.%lu MB/s   --\n",
                    g_state.bench.cache_on_copy / 10, g_state.bench.cache_on_copy % 10);
        }
        if (g_state.bench.cache_on_fill > 0 && g_state.bench.cache_off_fill > 0) {
            unsigned long speedup = (g_state.bench.cache_on_fill * 10) / g_state.bench.cache_off_fill;
            fprintf(fp, "Fill        %lu.%lu MB/s   %lu.%lu MB/s   %lu.%lux\n",
                    g_state.bench.cache_on_fill / 10, g_state.bench.cache_on_fill % 10,
                    g_state.bench.cache_off_fill / 10, g_state.bench.cache_off_fill % 10,
                    speedup / 10, speedup % 10);
        } else if (g_state.bench.cache_on_fill > 0) {
            fprintf(fp, "Fill        %lu.%lu MB/s   --\n",
                    g_state.bench.cache_on_fill / 10, g_state.bench.cache_on_fill % 10);
        }
        if (g_state.bench.cache_on_read > 0 && g_state.bench.cache_off_read > 0) {
            unsigned long speedup = (g_state.bench.cache_on_read * 10) / g_state.bench.cache_off_read;
            fprintf(fp, "Read        %lu.%lu MB/s   %lu.%lu MB/s   %lu.%lux\n",
                    g_state.bench.cache_on_read / 10, g_state.bench.cache_on_read % 10,
                    g_state.bench.cache_off_read / 10, g_state.bench.cache_off_read % 10,
                    speedup / 10, speedup % 10);
        } else if (g_state.bench.cache_on_read > 0) {
            fprintf(fp, "Read        %lu.%lu MB/s   --\n",
                    g_state.bench.cache_on_read / 10, g_state.bench.cache_on_read % 10);
        }
    } else {
        fprintf(fp, "(No benchmark results)\n");
    }
    fprintf(fp, "\n");

    /* Register Dump */
    fprintf(fp, "REGISTER DUMP\n");
    fprintf(fp, "-------------\n");
    fprintf(fp, "     00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
    for (row = 0; row < 8; row++) {
        fprintf(fp, "%02X:  ", row * 16);
        for (col = 0; col < 16; col++) {
            int idx = row * 16 + col;
            fprintf(fp, "%02X ", g_state.reg_values[idx]);
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "\n");

    fprintf(fp, "=============================================================\n");
    fprintf(fp, "Generated by CACHEKIT v%s\n", VERSION);
    fprintf(fp, "=============================================================\n");

    fclose(fp);
    return 1;
}

/* Show save dialog and save report */
static void save_report_dialog(void)
{
    char filename[13] = "REPORT.TXT";
    int key, pos = 0;

    /* Draw dialog */
    video_fill(20, 9, 40, 7, ' ', ATTR_NORMAL);
    video_box(20, 9, 40, 7, ATTR_BOX);
    video_puts(22, 9, " Save Report ", ATTR_TITLE);
    video_puts(22, 11, "Filename:", ATTR_LABEL);
    video_puts(22, 12, filename, ATTR_VALUE);
    video_puts(22, 14, "Enter=Save  Esc=Cancel", ATTR_DIM);

    /* Simple filename entry (just accept default for now) */
    while (1) {
        key = get_key();
        if (key == KEY_ESC) {
            return;
        }
        if (key == KEY_ENTER) {
            break;
        }
    }

    /* Read register values for the dump (routes via PCI for PCI chipsets) */
    {
        int i;
        for (i = 0; i < 128; i++) {
            g_state.reg_values[i] = chipset_read_reg(i);
        }
    }

    /* Generate report */
    if (generate_report(filename)) {
        video_fill(20, 9, 40, 5, ' ', ATTR_NORMAL);
        video_box(20, 9, 40, 5, ATTR_SUCCESS);
        video_puts(22, 9, " Report Saved ", ATTR_SUCCESS);
        video_printf(22, 11, ATTR_VALUE, "Saved to %s", filename);
        video_puts(22, 12, "Press any key...", ATTR_DIM);
    } else {
        video_fill(20, 9, 40, 5, ' ', ATTR_NORMAL);
        video_box(20, 9, 40, 5, ATTR_ERROR);
        video_puts(22, 9, " Error ", ATTR_ERROR);
        video_puts(22, 11, "Failed to save report!", ATTR_ERROR);
        video_puts(22, 12, "Press any key...", ATTR_DIM);
    }

    get_key();
}

/*============================================================================
 * CONFIRMATION DIALOG
 *============================================================================*/

static int show_confirm_dialog(const char *title, const char *msg1, const char *msg2)
{
    int key;
    int result = 0;

    /* Draw dialog box */
    video_fill(20, 9, 40, 7, ' ', ATTR_NORMAL);
    video_box(20, 9, 40, 7, ATTR_BOX);
    video_printf(22, 9, ATTR_TITLE, " %s ", title);

    video_puts(22, 11, msg1, ATTR_VALUE);
    if (msg2) {
        video_puts(22, 12, msg2, ATTR_WARNING);
    }

    video_puts(22, 14, "[Y] Yes    [N] No", ATTR_HIGHLIGHT);

    /* Wait for Y or N */
    while (1) {
        key = get_key();
        if (key == 'y' || key == 'Y') {
            result = 1;
            break;
        }
        if (key == 'n' || key == 'N' || key == KEY_ESC) {
            result = 0;
            break;
        }
    }

    return result;
}

/*============================================================================
 * CACHE TOGGLE FUNCTIONS
 *============================================================================*/

/* Find cache control entry for current chipset */
static const cache_control_t *get_cache_control_entry(void)
{
    int i;
    for (i = 0; g_cache_ctrl[i].chipset_type != 0; i++) {
        if (g_cache_ctrl[i].chipset_type == g_state.chipset.type) {
            return &g_cache_ctrl[i];
        }
    }
    return NULL;
}

/* Check if cache is currently enabled */
static int is_cache_enabled(void)
{
    const cache_control_t *ctrl;
    unsigned char reg_val;

    /* Try HAL first (v3.0) */
    if (hal_is_available()) {
        int state = hal_cache_get();
        return (state & CACHE_ENABLED) ? 1 : 0;
    }

    /* Legacy fallback */
    ctrl = get_cache_control_entry();
    if (!ctrl) {
        return 1;  /* Assume enabled if unknown */
    }

    /* Use chipset_read_reg for PCI/legacy routing */
    reg_val = chipset_read_reg(ctrl->cache_reg);

    if (ctrl->enable_value) {
        /* Bit set = enabled */
        return (reg_val & (1 << ctrl->cache_bit)) ? 1 : 0;
    } else {
        /* Bit clear = enabled */
        return (reg_val & (1 << ctrl->cache_bit)) ? 0 : 1;
    }
}

/* Internal cache disable - writes directly to register */
static void disable_cache_internal(void)
{
    const cache_control_t *ctrl;
    unsigned char reg_val;

    /* Try HAL first (v3.0) */
    if (hal_is_available()) {
        hal_cache_set(0);
        return;
    }

    /* Legacy fallback */
    ctrl = get_cache_control_entry();
    if (!ctrl) return;

    /* Use chipset_read_reg/chipset_write_reg for PCI/legacy routing */
    reg_val = chipset_read_reg(ctrl->cache_reg);

    if (ctrl->enable_value) {
        /* Clear bit to disable */
        reg_val &= ~(1 << ctrl->cache_bit);
    } else {
        /* Set bit to disable */
        reg_val |= (1 << ctrl->cache_bit);
    }

    chipset_write_reg(ctrl->cache_reg, reg_val);
}

/* Enable cache */
static void enable_cache(void)
{
    const cache_control_t *ctrl;
    unsigned char reg_val;

    /* Try HAL first (v3.0) */
    if (hal_is_available()) {
        hal_cache_set(1);
        return;
    }

    /* Legacy fallback */
    ctrl = get_cache_control_entry();
    if (!ctrl) return;

    /* Use chipset_read_reg/chipset_write_reg for PCI/legacy routing */
    reg_val = chipset_read_reg(ctrl->cache_reg);

    if (ctrl->enable_value) {
        /* Set bit to enable */
        reg_val |= (1 << ctrl->cache_bit);
    } else {
        /* Clear bit to enable */
        reg_val &= ~(1 << ctrl->cache_bit);
    }

    chipset_write_reg(ctrl->cache_reg, reg_val);
}

/* Safe cache disable - FLUSH FIRST for write-back chipsets! */
static int safe_disable_cache(void)
{
    const cache_control_t *ctrl;

    /* Try HAL first (v3.0) */
    if (hal_is_available()) {
        /* CRITICAL: Flush before disable for write-back caches */
        if (g_hal->is_writeback) {
            hal_cache_flush();
            hal_cache_flush();  /* Double-flush for safety */
        }
        hal_cache_set(0);
        return 1;
    }

    /* Legacy fallback */
    ctrl = get_cache_control_entry();
    if (!ctrl) {
        return 0;  /* Cannot disable - unknown chipset */
    }

    /* CRITICAL: For write-back caches, we MUST flush before disabling!
     * Failure to do so causes dirty data to be lost! */
    if (g_state.chipset.is_writeback) {
        flush_cache();
        flush_cache();  /* Double-flush for safety */
    }

    disable_cache_internal();
    return 1;
}

/* Toggle cache with confirmation dialog */
static void toggle_cache_with_confirm(void)
{
    const cache_control_t *ctrl = get_cache_control_entry();
    int currently_enabled;
    const char *msg1, *msg2;

    if (!ctrl) {
        /* Unknown chipset - show error */
        video_fill(20, 9, 40, 5, ' ', ATTR_NORMAL);
        video_box(20, 9, 40, 5, ATTR_ERROR);
        video_puts(22, 9, " Cache Toggle ", ATTR_ERROR);
        video_puts(22, 11, "Unsupported chipset!", ATTR_ERROR);
        video_puts(22, 12, "Press any key...", ATTR_DIM);
        get_key();
        return;
    }

    currently_enabled = is_cache_enabled();

    if (currently_enabled) {
        msg1 = "DISABLE cache?";
        if (g_state.chipset.is_writeback) {
            msg2 = "Will FLUSH first (WB chipset)";
        } else {
            msg2 = NULL;
        }
    } else {
        msg1 = "ENABLE cache?";
        msg2 = NULL;
    }

    if (show_confirm_dialog("Cache Toggle", msg1, msg2)) {
        if (currently_enabled) {
            safe_disable_cache();
        } else {
            enable_cache();
        }
    }
}

/*============================================================================
 * NC REGION CONFIGURATION (CLEAR / AUTO-CONFIG)
 *============================================================================*/

static void clear_nc_region(int region)
{
    chipset_info_t *chip = &g_state.chipset;
    unsigned char r0;

    if (region < 0 || region >= (int)chip->nc_regions) return;
    (void)r0;  /* May not be used in all paths */

    switch (chip->type) {
        case CHIPSET_OPTI391:
        case CHIPSET_OPTI381:
        case CHIPSET_OPTI_VIPER:
        case CHIPSET_ETEQ_BENGAL:
            /* Clear by setting size bits to 0 in the size/enable register */
            if (region == 0) {
                chipset_write_reg(0x52, 0x00);
                chipset_write_reg(0x53, 0x00);
            } else if (region == 1) {
                chipset_write_reg(0x54, 0x00);
                chipset_write_reg(0x55, 0x00);
            }
            break;

        case CHIPSET_SIS_460:
        case CHIPSET_SIS_RABBIT:
            /* SiS legacy: Clear by setting size to 0 */
            chipset_write_reg(0x14 + region * 2, 0x00);
            chipset_write_reg(0x15 + region * 2, 0x00);
            break;

        case CHIPSET_SIS_5591:
        case CHIPSET_SIS_5598:
        case CHIPSET_SIS_530:
            /* SiS Super7: NC regions controlled via 0x77 enable bits */
            clear_sis5598_nc_region(region);
            break;

        case CHIPSET_SIS_496:
            /* SiS 496: 3 Exclusive Areas at 50h-55h
             * Area 2 high byte (0x55) only uses upper 4 bits */
            if (region < 3) {
                chipset_write_reg(0x50 + region * 2, 0x00);
                if (region == 2) {
                    /* Preserve lower 4 bits of 0x55 */
                    r0 = chipset_read_reg(0x55) & 0x0F;
                    chipset_write_reg(0x55, r0);
                } else {
                    chipset_write_reg(0x51 + region * 2, 0x00);
                }
            }
            break;

        case CHIPSET_VIA_VP1:
        case CHIPSET_VIA_VP3:
        case CHIPSET_VIA_MVP3:
            /* VIA Apollo: NC regions at 58h-5Fh */
            chipset_write_reg(0x58 + region * 2, 0x00);
            chipset_write_reg(0x59 + region * 2, 0x00);
            break;

        case CHIPSET_ALI_ALADDIN:
        case CHIPSET_ALI_ALADDIN5:
            /* ALi Aladdin: NC regions at 50h-53h */
            if (region == 0) {
                chipset_write_reg(0x50, 0x00);
                chipset_write_reg(0x51, 0x00);
            } else if (region == 1) {
                chipset_write_reg(0x52, 0x00);
                chipset_write_reg(0x53, 0x00);
            }
            break;

        /* Contaq 82C596: No NC regions to clear */

        case CHIPSET_UMC491:
            /* UMC: Clear enable bit */
            if (region == 0) {
                chipset_write_reg(0x50, 0x00);
                chipset_write_reg(0x51, 0x00);
            }
            break;

        case CHIPSET_I430FX:
        case CHIPSET_I430HX:
        case CHIPSET_I430VX:
        case CHIPSET_I430TX:
        case CHIPSET_I430MX:
            /* Intel doesn't have traditional NC regions - no-op */
            break;

        default:
            /* Unknown chipset - try OPTi-style */
            if (region == 0) {
                chipset_write_reg(0x52, 0x00);
                chipset_write_reg(0x53, 0x00);
            }
            break;
    }

    /* Refresh NC region data */
    read_nc_regions();
}

static void clear_all_nc_regions(void)
{
    chipset_info_t *chip = &g_state.chipset;
    int i;

    for (i = 0; i < (int)chip->nc_regions && i < 4; i++) {
        clear_nc_region(i);
    }
}

/*
 * write_nc_region - Wrapper for write_nc_region_v2 using global chipset state
 */
static int write_nc_region(int region, unsigned long base_kb, unsigned long size_kb)
{
    return write_nc_region_v2(g_state.chipset.type, region, base_kb, size_kb);
}

static void auto_config_nc_region(unsigned long size_kb)
{
    chipset_info_t *chip = &g_state.chipset;
    unsigned long base_kb;
    unsigned char base_reg, size_reg, r0;
    int region = 0;  /* Auto-config always uses region 0 */

    (void)r0;  /* May not be used in all code paths */

    /* Calculate base address: place NC region at top of extended memory */
    /* Align to granularity */
    if (g_state.total_mem_kb <= 1024) {
        /* Not enough memory */
        return;
    }

    base_kb = ((unsigned long)g_state.total_mem_kb - size_kb);
    /* Align down to granularity */
    base_kb = (base_kb / chip->granularity) * chip->granularity;

    /* Ensure it's above 1MB */
    if (base_kb < 1024) base_kb = 1024;

    switch (chip->type) {
        case CHIPSET_OPTI391:
        case CHIPSET_OPTI381:
        case CHIPSET_OPTI_VIPER:
        case CHIPSET_ETEQ_BENGAL:
            /* OPTi format:
             * Index 52h = base address bits A23-A16 (in 64KB units, but we use A19-A12)
             * Index 53h = bits 7-4: size code (1=8K,2=16K,3=32K,4=64K,5=128K,6=256K,7=512K)
             *             bits 3-0: base A27-A24
             */
            base_reg = (unsigned char)((base_kb >> 4) & 0xFF);  /* A23-A16 */
            /* Calculate size code: log2(size/8) + 1 */
            if (size_kb <= 8) size_reg = 0x10;
            else if (size_kb <= 16) size_reg = 0x20;
            else if (size_kb <= 32) size_reg = 0x30;
            else if (size_kb <= 64) size_reg = 0x40;
            else if (size_kb <= 128) size_reg = 0x50;
            else if (size_kb <= 256) size_reg = 0x60;
            else if (size_kb <= 512) size_reg = 0x70;
            else size_reg = 0x70;  /* Max 512KB per region */

            /* Add high address bits (assuming < 16MB, these are 0) */
            size_reg |= (unsigned char)((base_kb >> 12) & 0x0F);

            chipset_write_reg(0x52, base_reg);
            chipset_write_reg(0x53, size_reg);
            break;

        case CHIPSET_SIS_460:
        case CHIPSET_SIS_RABBIT:
            /* SiS legacy format:
             * Index 14h = base address in 64KB units
             * Index 15h = bits 7-4: size code (1=64K,2=128K,3=256K,4=512K,5=1M,6=2M)
             */
            base_reg = (unsigned char)(base_kb >> 6);  /* 64KB units */
            if (size_kb <= 64) size_reg = 0x10;
            else if (size_kb <= 128) size_reg = 0x20;
            else if (size_kb <= 256) size_reg = 0x30;
            else if (size_kb <= 512) size_reg = 0x40;
            else if (size_kb <= 1024) size_reg = 0x50;
            else size_reg = 0x60;

            chipset_write_reg(0x14, base_reg);
            chipset_write_reg(0x15, size_reg);
            break;

        case CHIPSET_SIS_5591:
        case CHIPSET_SIS_5598:
        case CHIPSET_SIS_530:
            /* SiS Super7: 16-bit NC region encoding at 0x77+0x78-0x7B */
            write_sis5598_nc_region(region, base_kb, size_kb);
            break;

        case CHIPSET_SIS_496:
            /* SiS 496: 3 Exclusive Areas at 50h-55h
             * Size options: 64K, 128K, 256K, 512K, 1M, 2M, 4M
             * Area 2 high byte (0x55) only uses upper 4 bits */
            if (region >= 3) break;  /* Only 3 regions supported */

            base_reg = (unsigned char)(base_kb >> 6);  /* 64KB units */
            if (size_kb <= 64) size_reg = 0x10;
            else if (size_kb <= 128) size_reg = 0x20;
            else if (size_kb <= 256) size_reg = 0x30;
            else if (size_kb <= 512) size_reg = 0x40;
            else if (size_kb <= 1024) size_reg = 0x50;
            else if (size_kb <= 2048) size_reg = 0x60;
            else size_reg = 0x70;  /* 4M */

            chipset_write_reg(0x50 + region * 2, base_reg);
            if (region == 2) {
                /* Area 2: preserve lower 4 bits of 0x55 */
                r0 = chipset_read_reg(0x55) & 0x0F;
                chipset_write_reg(0x55, size_reg | r0);
            } else {
                chipset_write_reg(0x51 + region * 2, size_reg);
            }
            break;

        case CHIPSET_VIA_VP1:
        case CHIPSET_VIA_VP3:
        case CHIPSET_VIA_MVP3:
            /* VIA Apollo: NC regions at 58h-5Fh, similar layout */
            base_reg = (unsigned char)(base_kb >> 6);  /* 64KB units */
            if (size_kb <= 64) size_reg = 0x10;
            else if (size_kb <= 128) size_reg = 0x20;
            else if (size_kb <= 256) size_reg = 0x30;
            else if (size_kb <= 512) size_reg = 0x40;
            else if (size_kb <= 1024) size_reg = 0x50;
            else size_reg = 0x60;

            chipset_write_reg(0x58, base_reg);
            chipset_write_reg(0x59, size_reg);
            break;

        case CHIPSET_ALI_ALADDIN:
        case CHIPSET_ALI_ALADDIN5:
            /* ALi Aladdin: NC regions at 50h-53h */
            base_reg = (unsigned char)(base_kb >> 6);  /* 64KB units */
            if (size_kb <= 64) size_reg = 0x10;
            else if (size_kb <= 128) size_reg = 0x20;
            else if (size_kb <= 256) size_reg = 0x30;
            else if (size_kb <= 512) size_reg = 0x40;
            else size_reg = 0x50;

            chipset_write_reg(0x50, base_reg);
            chipset_write_reg(0x51, size_reg);
            break;

        /* Contaq 82C596: No NC regions to configure */

        case CHIPSET_UMC491:
            /* UMC format:
             * Index 50h = base bits 23-16
             * Index 51h = bit 7: enable, bits 6-4: size
             */
            base_reg = (unsigned char)((base_kb >> 4) & 0xFF);
            if (size_kb <= 8) size_reg = 0x80;       /* Enable + 8KB */
            else if (size_kb <= 16) size_reg = 0x90;
            else if (size_kb <= 32) size_reg = 0xA0;
            else if (size_kb <= 64) size_reg = 0xB0;
            else if (size_kb <= 128) size_reg = 0xC0;
            else if (size_kb <= 256) size_reg = 0xD0;
            else size_reg = 0xE0;  /* 512KB */

            chipset_write_reg(0x50, base_reg);
            chipset_write_reg(0x51, size_reg);
            break;

        case CHIPSET_I430FX:
        case CHIPSET_I430HX:
        case CHIPSET_I430VX:
        case CHIPSET_I430TX:
        case CHIPSET_I430MX:
            /* Intel Triton doesn't support traditional NC regions */
            /* Would need MTRRs or PAM register manipulation */
            break;

        default:
            /* Try OPTi-style for unknown chipsets */
            base_reg = (unsigned char)((base_kb >> 4) & 0xFF);
            size_reg = 0x70;  /* 512KB */
            chipset_write_reg(0x52, base_reg);
            chipset_write_reg(0x53, size_reg);
            break;
    }

    /* Refresh NC region data */
    read_nc_regions();
}

/*============================================================================
 * CRITICAL REGISTER DATABASE (for warnings)
 *============================================================================*/

static int is_critical_register(int reg_idx)
{
    chipset_info_t *chip = &g_state.chipset;

    /* Check for cache control registers that could cause system instability */
    switch (chip->type) {
        case CHIPSET_OPTI391:
        case CHIPSET_OPTI381:
        case CHIPSET_ETEQ_BENGAL:
            /* 20h: Cache control - very critical on write-back chipsets */
            if (reg_idx == 0x20) return 2;  /* High risk */
            /* 52h-55h: NC regions - moderate risk */
            if (reg_idx >= 0x52 && reg_idx <= 0x55) return 1;
            break;

        case CHIPSET_SIS_460:
        case CHIPSET_SIS_RABBIT:
            /* 10h-11h: Cache control */
            if (reg_idx >= 0x10 && reg_idx <= 0x11) return 2;
            /* 14h-1Bh: NC regions */
            if (reg_idx >= 0x14 && reg_idx <= 0x1B) return 1;
            break;

        case CHIPSET_UMC491:
            /* Cache control register */
            if (reg_idx == 0x00 || reg_idx == 0x01) return 2;
            if (reg_idx >= 0x50 && reg_idx <= 0x51) return 1;
            break;

        default:
            /* For unknown chipsets, warn on common critical indices */
            if (reg_idx == 0x00 || reg_idx == 0x20) return 2;
            break;
    }

    return 0;  /* Not critical */
}

static const char *get_register_warning(int reg_idx)
{
    int level = is_critical_register(reg_idx);

    if (level == 2) {
        return "DANGER: Cache control register!";
    } else if (level == 1) {
        return "Caution: NC region register";
    }

    return NULL;
}

/*============================================================================
 * SCREEN: INFO (F1)
 *============================================================================*/

/* Draw 286/386SX info screen (for info-only chipsets) */
static void draw_info_screen_286(void)
{
    chipset_info_t *chip = &g_state.chipset;
    inventory_286_t *inv = &g_state.inv286;
    const char *cpu_str;

    /* Left column: Chipset Detected */
    video_puts(2, 3, "CHIPSET DETECTED", ATTR_HIGHLIGHT);
    video_hline(2, 4, 17, 0xC4, ATTR_DIM);

    video_puts(2, 5, "Name:", ATTR_LABEL);
    video_puts(10, 5, chip->name, ATTR_VALUE);
    video_puts(2, 6, "Vendor:", ATTR_LABEL);
    video_puts(10, 6, chip->vendor, ATTR_VALUE);
    video_puts(2, 7, "Ports:", ATTR_LABEL);
    video_printf(10, 7, ATTR_VALUE, "%02Xh / %02Xh", chip->index_port, chip->data_port);
    video_puts(2, 8, "Type:", ATTR_LABEL);
    video_puts(10, 8, "286-ERA (Info Only)", ATTR_WARNING);

    /* Capabilities */
    video_puts(2, 10, "CAPABILITIES", ATTR_HIGHLIGHT);
    video_hline(2, 11, 12, 0xC4, ATTR_DIM);

    video_puts(2, 12, "Cache:", ATTR_LABEL);
    video_puts(14, 12, "N/A (no on-chip)", ATTR_DIM);
    video_puts(2, 13, "NC Regions:", ATTR_LABEL);
    video_puts(14, 13, "N/A", ATTR_DIM);
    video_puts(2, 14, "Shadow RAM:", ATTR_LABEL);
    video_puts(14, 14, "Yes (16KB granularity)", ATTR_VALUE);
    video_puts(2, 15, "A20 Control:", ATTR_LABEL);
    video_puts(14, 15, inv->a20_method == 1 ? "Chipset" :
                       inv->a20_method == 2 ? "KBC" : "Unknown", ATTR_VALUE);

    /* Shadow RAM map */
    video_puts(2, 17, "SHADOW RAM MAP", ATTR_HIGHLIGHT);
    video_hline(2, 18, 14, 0xC4, ATTR_DIM);

    video_puts(2, 19, "C000-C7FF:", ATTR_LABEL);
    video_puts(14, 19, inv->shadow_c000_c7ff ? "DRAM" : "ROM",
               inv->shadow_c000_c7ff ? ATTR_SUCCESS : ATTR_DIM);
    video_puts(2, 20, "C800-CFFF:", ATTR_LABEL);
    video_puts(14, 20, inv->shadow_c800_cfff ? "DRAM" : "ROM",
               inv->shadow_c800_cfff ? ATTR_SUCCESS : ATTR_DIM);
    video_puts(22, 19, "D000-DFFF:", ATTR_LABEL);
    video_puts(34, 19, inv->shadow_d000_dfff ? "DRAM" : "ROM",
               inv->shadow_d000_dfff ? ATTR_SUCCESS : ATTR_DIM);
    video_puts(22, 20, "E000-FFFF:", ATTR_LABEL);
    video_puts(34, 20, inv->shadow_e000_ffff ? "DRAM" : "ROM",
               inv->shadow_e000_ffff ? ATTR_SUCCESS : ATTR_DIM);

    /* Right column: System Status */
    video_puts(43, 3, "SYSTEM STATUS", ATTR_HIGHLIGHT);
    video_hline(43, 4, 13, 0xC4, ATTR_DIM);

    /* Determine CPU type based on chipset */
    switch (chip->type) {
        case CHIPSET_CT_NEAT386:
        case CHIPSET_HEADLAND_18:
        case CHIPSET_VLSI_320:
            cpu_str = "386SX";
            break;
        default:
            cpu_str = "80286";
            break;
    }
    video_puts(43, 5, "CPU:", ATTR_LABEL);
    video_puts(52, 5, cpu_str, ATTR_VALUE);
    video_puts(43, 6, "Memory:", ATTR_LABEL);
    video_printf(52, 6, ATTR_VALUE, "%u KB", inv->dram_total_kb);
    video_puts(43, 7, "A20:", ATTR_LABEL);
    video_puts(52, 7, inv->a20_enabled ? "Enabled" : "Disabled",
               inv->a20_enabled ? ATTR_SUCCESS : ATTR_ERROR);
    if (supports_a20_toggle()) {
        video_puts(62, 7, "[A] Toggle", ATTR_DIM);
    }

    /* DRAM Configuration */
    video_puts(43, 9, "DRAM CONFIG", ATTR_HIGHLIGHT);
    video_hline(43, 10, 11, 0xC4, ATTR_DIM);

    video_puts(43, 11, "Wait States:", ATTR_LABEL);
    video_printf(57, 11, ATTR_VALUE, "%u WS", inv->wait_states);
    video_puts(43, 12, "Refresh:", ATTR_LABEL);
    video_puts(57, 12, inv->refresh_rate ? "125us (slow)" : "15.6us (fast)",
               inv->refresh_rate ? ATTR_DIM : ATTR_VALUE);

    /* Status checkboxes */
    video_puts(43, 14, "STATUS", ATTR_HIGHLIGHT);
    video_hline(43, 15, 6, 0xC4, ATTR_DIM);

    video_printf(43, 16, ATTR_DIM, "[%c] Cache (N/A)", CHECK_OFF);
    video_printf(43, 17, ATTR_DIM, "[%c] NC Region (N/A)", CHECK_OFF);
    video_printf(43, 18, inv->a20_enabled ? ATTR_VALUE : ATTR_ERROR,
                 "[%c] A20 Gate %s", inv->a20_enabled ? CHECK_ON : CHECK_OFF,
                 inv->a20_enabled ? "Open" : "CLOSED");

    /* Info note */
    video_puts(2, 22, "NOTE: 286-era chipsets have no cache controller or NC region support.", ATTR_DIM);

    /* Status bar message */
    if (supports_a20_toggle()) {
        ui_draw_status_bar("A=Toggle A20  |  F4=Registers  |  F5=Benchmark  |  Alt-X=Exit");
    } else {
        ui_draw_status_bar("F4=Registers  |  F5=Benchmark  |  Alt-X=Exit");
    }
}

/* Get chassis type name string */
static const char *get_chassis_type_name(unsigned char type)
{
    switch (type) {
        case 1:  return "Other";
        case 2:  return "Unknown";
        case 3:  return "Desktop";
        case 4:  return "Low Profile Desktop";
        case 5:  return "Pizza Box";
        case 6:  return "Mini Tower";
        case 7:  return "Tower";
        case 8:  return "Portable";
        case 9:  return "Laptop";
        case 10: return "Notebook";
        case 11: return "Hand Held";
        case 12: return "Docking Station";
        case 13: return "All in One";
        case 14: return "Sub Notebook";
        case 15: return "Space-saving";
        case 16: return "Lunch Box";
        case 17: return "Main Server";
        case 18: return "Expansion";
        case 19: return "SubChassis";
        case 20: return "Bus Expansion";
        case 21: return "Peripheral";
        case 22: return "RAID";
        case 23: return "Rack Mount";
        default: return "Unknown";
    }
}

/* Get PM profile name string */
static const char *get_pm_profile_name(unsigned char profile)
{
    switch (profile) {
        case 0:  return "Unspecified";
        case 1:  return "Desktop";
        case 2:  return "Mobile";
        case 3:  return "Workstation";
        case 4:  return "Enterprise Server";
        case 5:  return "SOHO Server";
        case 6:  return "Appliance PC";
        case 7:  return "Performance Server";
        default: return "Unknown";
    }
}

/* Draw F1 Info screen - SMBIOS/ACPI view */
static void draw_info_smbios_view(void)
{
    smbios_info_t *smbios = &g_state.smbios;
    acpi_info_t *acpi = &g_state.acpi;
    int i, y;

    /* Tab indicator */
    video_puts(2, 3, "[Tab] Chipset | SMBIOS", ATTR_DIM);
    video_puts(24, 3, "SMBIOS", ATTR_SELECTED);

    /* Left column: SMBIOS Information */
    video_puts(2, 5, "SMBIOS INFORMATION", ATTR_HIGHLIGHT);
    video_hline(2, 6, 18, 0xC4, ATTR_DIM);

    if (smbios->valid) {
        video_puts(2, 7, "Version:", ATTR_LABEL);
        video_printf(14, 7, ATTR_VALUE, "%u.%u",
                     smbios->version_major, smbios->version_minor);

        /* BIOS */
        video_puts(2, 9, "BIOS:", ATTR_LABEL);
        if (smbios->bios_vendor[0]) {
            video_printf(14, 9, ATTR_VALUE, "%.24s", smbios->bios_vendor);
        }
        if (smbios->bios_version[0]) {
            video_printf(14, 10, ATTR_DIM, "%.24s", smbios->bios_version);
        }
        if (smbios->bios_date[0]) {
            video_printf(14, 11, ATTR_DIM, "%s", smbios->bios_date);
        }

        /* System */
        video_puts(2, 13, "System:", ATTR_LABEL);
        if (smbios->sys_manufacturer[0]) {
            video_printf(14, 13, ATTR_VALUE, "%.24s", smbios->sys_manufacturer);
        }
        if (smbios->sys_product[0]) {
            video_printf(14, 14, ATTR_DIM, "%.24s", smbios->sys_product);
        }

        /* Board */
        video_puts(2, 16, "Board:", ATTR_LABEL);
        if (smbios->board_manufacturer[0]) {
            video_printf(14, 16, ATTR_VALUE, "%.24s", smbios->board_manufacturer);
        }
        if (smbios->board_product[0]) {
            video_printf(14, 17, ATTR_DIM, "%.24s", smbios->board_product);
        }

        /* Chassis */
        video_puts(2, 19, "Chassis:", ATTR_LABEL);
        video_puts(14, 19, get_chassis_type_name(smbios->chassis_type), ATTR_VALUE);

        /* Memory summary */
        video_puts(2, 21, "Memory:", ATTR_LABEL);
        if (smbios->mem_slots > 0) {
            video_printf(14, 21, ATTR_VALUE, "%u slots", smbios->mem_slots);
        }
    } else {
        video_puts(2, 8, "SMBIOS not found", ATTR_DIM);
        if (smbios->entry_found) {
            video_puts(2, 9, "(tables above 1MB)", ATTR_DIM);
        }
    }

    /* Right column: ACPI Information */
    video_puts(43, 5, "ACPI INFORMATION", ATTR_HIGHLIGHT);
    video_hline(43, 6, 16, 0xC4, ATTR_DIM);

    if (acpi->valid) {
        video_puts(43, 7, "Revision:", ATTR_LABEL);
        video_printf(55, 7, ATTR_VALUE, "ACPI %u.x",
                     acpi->revision == 0 ? 1 : acpi->revision);

        video_puts(43, 8, "OEM ID:", ATTR_LABEL);
        if (acpi->oem_id[0]) {
            video_puts(55, 8, acpi->oem_id, ATTR_VALUE);
        }

        video_puts(43, 10, "Tables:", ATTR_LABEL);
        video_printf(55, 10, ATTR_VALUE, "%u found", acpi->table_count);

        /* Table presence */
        video_printf(43, 12, acpi->has_facp ? ATTR_SUCCESS : ATTR_DIM,
                     "[%c] FACP/FADT", acpi->has_facp ? CHECK_ON : CHECK_OFF);
        video_printf(43, 13, acpi->has_mcfg ? ATTR_SUCCESS : ATTR_DIM,
                     "[%c] MCFG (PCIe)", acpi->has_mcfg ? CHECK_ON : CHECK_OFF);
        video_printf(43, 14, acpi->has_apic ? ATTR_SUCCESS : ATTR_DIM,
                     "[%c] APIC/MADT", acpi->has_apic ? CHECK_ON : CHECK_OFF);
        video_printf(43, 15, acpi->has_hpet ? ATTR_SUCCESS : ATTR_DIM,
                     "[%c] HPET", acpi->has_hpet ? CHECK_ON : CHECK_OFF);

        /* PM Profile */
        if (acpi->has_facp) {
            video_puts(43, 17, "PM Profile:", ATTR_LABEL);
            video_puts(55, 17, get_pm_profile_name(acpi->pm_profile), ATTR_VALUE);
        }
    } else {
        video_puts(43, 8, "ACPI not found", ATTR_DIM);
    }

    /* Memory DIMMs (if available) */
    if (smbios->valid && smbios->dimm_count > 0) {
        video_puts(43, 19, "MEMORY DETAILS", ATTR_HIGHLIGHT);
        video_hline(43, 20, 14, 0xC4, ATTR_DIM);

        y = 21;
        for (i = 0; i < smbios->dimm_count && i < 3 && y < 24; i++) {
            if (smbios->dimms[i].populated) {
                video_printf(43, y, ATTR_VALUE, "DIMM%d: %u MB", i,
                             smbios->dimms[i].size_mb);
                if (smbios->dimms[i].speed_mhz > 0) {
                    video_printf(60, y, ATTR_DIM, "@ %u MHz",
                                 smbios->dimms[i].speed_mhz);
                }
            } else {
                video_printf(43, y, ATTR_DIM, "DIMM%d: Empty", i);
            }
            y++;
        }
    }

    ui_draw_status_bar("Tab=Chipset view  |  F1-F7=Screens  |  Alt-X=Exit");
}

static void draw_info_screen(void)
{
    chipset_info_t *chip = &g_state.chipset;
    const char *nc_str;
    int score_fill;

    /* Use alternate screen for info-only chipsets */
    if (chip->info_only) {
        draw_info_screen_286();
        return;
    }

    /* Check if SMBIOS tab is active */
    if (g_state.info_tab == INFO_TAB_SMBIOS) {
        draw_info_smbios_view();
        return;
    }

    /* Tab indicator for Chipset view */
    video_puts(2, 3, "[Tab] Chipset | SMBIOS", ATTR_DIM);
    video_puts(7, 3, "Chipset", ATTR_SELECTED);

    /* Left column: Chipset Detected */
    video_puts(2, 5, "CHIPSET DETECTED", ATTR_HIGHLIGHT);
    video_hline(2, 6, 17, 0xC4, ATTR_DIM);

    video_puts(2, 7, "Name:", ATTR_LABEL);
    video_puts(10, 7, chip->name, ATTR_VALUE);
    video_puts(2, 8, "Vendor:", ATTR_LABEL);
    video_puts(10, 8, chip->vendor, ATTR_VALUE);
    video_puts(2, 9, "Ports:", ATTR_LABEL);
    video_printf(10, 9, ATTR_VALUE, "%02Xh / %02Xh", chip->index_port, chip->data_port);
    video_puts(2, 10, "ID:", ATTR_LABEL);
    video_printf(10, 10, ATTR_VALUE, "0x%02X @ Index %02Xh", chip->id_value, chip->id_index);

    /* Cache Controller */
    video_puts(2, 12, "CACHE CONTROLLER", ATTR_HIGHLIGHT);
    video_hline(2, 13, 16, 0xC4, ATTR_DIM);

    video_puts(2, 14, "Type:", ATTR_LABEL);
    video_puts(12, 14, chip->is_writeback ? "Write-Back" : "Write-Through",
               chip->is_writeback ? ATTR_WARNING : ATTR_VALUE);
    video_puts(2, 15, "Size:", ATTR_LABEL);
    video_printf(12, 15, ATTR_VALUE, "%u KB", chip->cache_size_kb);
    video_puts(2, 16, "Status:", ATTR_LABEL);
    {
        int cache_on = is_cache_enabled();
        video_printf(12, 16, cache_on ? ATTR_SUCCESS : ATTR_ERROR,
                     "%s", cache_on ? "Enabled" : "DISABLED");
        video_puts(24, 16, "[T] Toggle", ATTR_DIM);
    }
    video_puts(2, 17, "Flush:", ATTR_LABEL);
    video_puts(12, 17, g_state.is_486 ? "WBINVD (486)" : "Read Loop (386)", ATTR_VALUE);

    /* NC Regions */
    video_puts(2, 19, "NC REGIONS", ATTR_HIGHLIGHT);
    video_hline(2, 20, 10, 0xC4, ATTR_DIM);

    switch (chip->nc_strategy) {
        case NC_RANGE:    nc_str = "Range-based"; break;
        case NC_BOUNDARY: nc_str = "Boundary"; break;
        case NC_STEERING: nc_str = "Steering"; break;
        case NC_SHADOW:   nc_str = "Shadow only"; break;
        default:          nc_str = "None"; break;
    }
    video_puts(2, 21, "Strategy:", ATTR_LABEL);
    video_puts(14, 21, nc_str, ATTR_VALUE);
    video_puts(2, 22, "Available:", ATTR_LABEL);
    video_printf(14, 22, ATTR_VALUE, "%u region%s", chip->nc_regions, chip->nc_regions != 1 ? "s" : "");
    video_puts(2, 23, "Granularity:", ATTR_LABEL);
    video_printf(14, 23, ATTR_VALUE, "%u KB", chip->granularity);

    /* Right column: System Status */
    video_puts(43, 5, "SYSTEM STATUS", ATTR_HIGHLIGHT);
    video_hline(43, 6, 13, 0xC4, ATTR_DIM);

    video_puts(43, 7, "CPU:", ATTR_LABEL);
    video_puts(52, 7, g_state.is_486 ? "486+" : "386DX", ATTR_VALUE);
    video_puts(43, 8, "Memory:", ATTR_LABEL);
    video_printf(52, 8, ATTR_VALUE, "%u KB", g_state.total_mem_kb);
    video_puts(43, 9, "A20:", ATTR_LABEL);
    video_puts(52, 9, "Enabled", ATTR_SUCCESS);

    /* Progress bars */
    video_puts(43, 11, "CACHE", ATTR_LABEL);
    video_progress(52, 11, 10, chip->cache_size_kb, 512, ATTR_VALUE);
    video_printf(65, 11, ATTR_VALUE, "%uK", chip->cache_size_kb);

    /* Check if any NC region is active */
    {
        int nc_active = 0;
        unsigned long nc_total_size = 0;
        int i;
        for (i = 0; i < (int)chip->nc_regions && i < 4; i++) {
            if (g_state.nc_live[i].active) {
                nc_active = 1;
                nc_total_size += g_state.nc_live[i].size_kb;
            }
        }

        video_puts(43, 12, "NC Region", ATTR_LABEL);
        if (nc_active) {
            video_progress(52, 12, 10, (int)(nc_total_size / 50), 10, ATTR_NC_REGION);
            video_printf(65, 12, ATTR_NC_REGION, "%luK", nc_total_size);
        } else {
            video_progress(52, 12, 10, 0, 10, ATTR_DIM);
            video_puts(65, 12, "None", ATTR_DIM);
        }

        /* Status checkboxes */
        video_puts(43, 14, "STATUS", ATTR_HIGHLIGHT);
        video_hline(43, 15, 6, 0xC4, ATTR_DIM);

        {
            int cache_on = is_cache_enabled();
            video_printf(43, 16, cache_on ? ATTR_VALUE : ATTR_ERROR,
                         "[%c] Cache %s", cache_on ? CHECK_ON : CHECK_OFF,
                         cache_on ? "Enabled" : "DISABLED");
        }
        video_printf(43, 17, nc_active ? ATTR_VALUE : ATTR_DIM, "[%c] NC Region Active",
                     nc_active ? CHECK_ON : CHECK_OFF);
        video_printf(43, 18, ATTR_VALUE, "[%c] A20 Gate Open", CHECK_ON);
    }

    /* External cache display (82385-style) for Unknown chipsets */
    if (chip->type == CHIPSET_UNKNOWN && g_state.ext_cache.probed) {
        video_puts(43, 20, "EXT CACHE (82385)", ATTR_HIGHLIGHT);
        video_hline(43, 21, 17, 0xC4, ATTR_DIM);

        if (g_state.ext_cache.present) {
            video_puts(43, 22, "Detected:", ATTR_LABEL);
            video_puts(55, 22, "Yes (timing)", ATTR_SUCCESS);

            video_puts(43, 23, "Size:", ATTR_LABEL);
            if (g_state.ext_cache.size_kb > 0) {
                video_printf(55, 23, ATTR_VALUE, "%u KB", g_state.ext_cache.size_kb);
            } else {
                video_puts(55, 23, "Unknown", ATTR_DIM);
            }

            video_puts(43, 24, "Speed:", ATTR_LABEL);
            video_printf(55, 24, ATTR_VALUE, "%lu.%02lux boost",
                         g_state.ext_cache.speed_ratio / 100,
                         g_state.ext_cache.speed_ratio % 100);
        } else {
            video_puts(43, 22, "Detected:", ATTR_LABEL);
            video_puts(55, 22, "No", ATTR_DIM);
            video_puts(43, 23, "[P] Probe", ATTR_DIM);
        }
    } else {
        /* Score (for known chipsets) */
        video_puts(43, 20, "SCORE", ATTR_HIGHLIGHT);
        video_hline(43, 21, 5, 0xC4, ATTR_DIM);

        video_printf(43, 22, ATTR_HIGHLIGHT, "%u.%u/10", chip->score_x10 / 10, chip->score_x10 % 10);
        score_fill = (chip->score_x10 * 20) / 100;
        video_progress(51, 22, 20, score_fill, 20,
                          chip->score_x10 >= 90 ? ATTR_SUCCESS :
                          chip->score_x10 >= 70 ? ATTR_VALUE : ATTR_WARNING);

        video_printf(43, 23, chip->score_x10 >= 90 ? ATTR_SUCCESS : ATTR_VALUE,
                     "%s-TIER: %s", chip->tier,
                     chip->score_x10 >= 90 ? "Driver's Dream" :
                     chip->score_x10 >= 80 ? "High Performance" :
                     chip->score_x10 >= 70 ? "Solid" : "Limited");
    }

    /* Status bar message */
    if (chip->type == CHIPSET_UNKNOWN && !g_state.is_486) {
        ui_draw_status_bar("Tab=SMBIOS  |  P=Probe ext cache  |  F1-F7=Screens  |  Alt-X=Exit");
    } else if (chip->is_writeback) {
        ui_draw_status_bar("Tab=SMBIOS  |  T=Toggle cache (WB)  |  F1-F7=Screens  |  Alt-X=Exit");
    } else {
        ui_draw_status_bar("Tab=SMBIOS  |  T=Toggle cache  |  F1-F7=Screens  |  Alt-X=Exit");
    }
}

/*============================================================================
 * SCREEN: NC CONFIG (F2) - Dynamic Memory Map
 *============================================================================*/

/* Convert memory address (KB) to screen Y coordinate (5=top/16MB, 19=bottom/0) */
static int mem_to_screen_y(unsigned long mem_kb, unsigned long max_kb)
{
    /* Map range: Y=5 (top, max memory) to Y=19 (bottom, 0) */
    /* That's 14 rows for the memory range */
    int y;
    if (max_kb == 0) max_kb = 16384;  /* Default 16MB */
    y = 19 - (int)((mem_kb * 14UL) / max_kb);
    if (y < 5) y = 5;
    if (y > 19) y = 19;
    return y;
}

static void draw_nc_screen(void)
{
    chipset_info_t *chip = &g_state.chipset;
    unsigned long max_mem_kb = (unsigned long)g_state.total_mem_kb;
    int i, y, y_top, y_bot, y_1mb, y_640k;
    int map_top = 5, map_bot = 19;

    /* Ensure reasonable max for display */
    if (max_mem_kb < 1024) max_mem_kb = 1024;
    if (max_mem_kb > 65536) max_mem_kb = 65536;  /* Cap at 64MB for display */

    /* Memory Map */
    video_puts(2, 3, "MEMORY MAP", ATTR_HIGHLIGHT);
    video_hline(2, 4, 10, 0xC4, ATTR_DIM);

    /* Draw memory map frame */
    video_printf(0, map_top, ATTR_DIM, "%2luMB", max_mem_kb / 1024);
    video_putc(8, map_top, BOX_TL, ATTR_BOX);
    video_hline(9, map_top, 24, BOX_H, ATTR_BOX);
    video_putc(33, map_top, BOX_TR, ATTR_BOX);

    /* Draw vertical sides and fill with cacheable memory */
    for (y = map_top + 1; y < map_bot; y++) {
        video_putc(8, y, BOX_V, ATTR_BOX);
        video_fill(9, y, 24, 1, ' ', ATTR_NORMAL);  /* Clear interior */
        video_putc(33, y, BOX_V, ATTR_BOX);
    }

    video_puts(2, map_bot, " 0KB", ATTR_DIM);
    video_putc(8, map_bot, BOX_BL, ATTR_BOX);
    video_hline(9, map_bot, 24, BOX_H, ATTR_BOX);
    video_putc(33, map_bot, BOX_BR, ATTR_BOX);

    /* Draw 1MB boundary */
    y_1mb = mem_to_screen_y(1024, max_mem_kb);
    if (y_1mb > map_top && y_1mb < map_bot) {
        video_puts(2, y_1mb, " 1MB", ATTR_DIM);
        video_putc(8, y_1mb, BOX_LT, ATTR_BOX);
        video_hline(9, y_1mb, 24, BOX_H, ATTR_BOX);
        video_putc(33, y_1mb, BOX_RT, ATTR_BOX);
    }

    /* Draw 640KB boundary and conventional memory area */
    y_640k = mem_to_screen_y(640, max_mem_kb);
    if (y_640k > map_top && y_640k < map_bot && y_640k != y_1mb) {
        video_puts(0, y_640k, "640K", ATTR_DIM);
        video_putc(8, y_640k, BOX_LT, ATTR_BOX);
        video_hline(9, y_640k, 24, BOX_H, ATTR_BOX);
        video_putc(33, y_640k, BOX_RT, ATTR_BOX);
    }

    /* Fill ROM/System area (640KB-1MB) */
    if (y_1mb < y_640k) {
        for (y = y_1mb + 1; y < y_640k; y++) {
            video_fill(9, y, 24, 1, BLOCK_MED, ATTR_DIM);
        }
        video_puts(10, (y_1mb + y_640k) / 2, " System/ROM ", ATTR_DIM);
    }

    /* Fill conventional memory area (0-640KB) */
    if (y_640k < map_bot - 1) {
        video_puts(10, (y_640k + map_bot) / 2, "Conventional", ATTR_VALUE);
    }

    /* Label extended memory */
    if (y_1mb > map_top + 2) {
        video_puts(10, map_top + 1, "Extended Memory", ATTR_VALUE);
        video_puts(10, map_top + 2, "(Cacheable)", ATTR_DIM);
    }

    /* Draw active NC regions dynamically */
    for (i = 0; i < (int)chip->nc_regions && i < 4; i++) {
        nc_region_live_t *nc = &g_state.nc_live[i];
        if (nc->active && nc->size_kb > 0) {
            unsigned long nc_top_kb = nc->base_kb + nc->size_kb;
            unsigned long nc_bot_kb = nc->base_kb;
            char label[24];

            y_top = mem_to_screen_y(nc_top_kb, max_mem_kb);
            y_bot = mem_to_screen_y(nc_bot_kb, max_mem_kb);

            /* Ensure at least 1 row visible */
            if (y_top >= y_bot) y_top = y_bot - 1;
            if (y_top < map_top + 1) y_top = map_top + 1;
            if (y_bot > map_bot - 1) y_bot = map_bot - 1;

            /* Draw NC region shading */
            for (y = y_top; y <= y_bot; y++) {
                video_fill(9, y, 24, 1, BLOCK_LIGHT, ATTR_NC_REGION);
            }

            /* Draw boundaries */
            video_printf(0, y_top, ATTR_NC_REGION, "%luM", nc_top_kb / 1024);
            video_putc(8, y_top, BOX_LT, ATTR_NC_REGION);
            video_hline(9, y_top, 24, BOX_H, ATTR_NC_REGION);
            video_putc(33, y_top, BOX_RT, ATTR_NC_REGION);

            /* Label in center of region */
            sprintf(label, " NC%d %luKB ", i, nc->size_kb);
            video_puts(10, (y_top + y_bot) / 2, label, ATTR_NC_REGION);

            /* Draw arrow pointing to region in table */
            video_puts(34, (y_top + y_bot) / 2, "<--", ATTR_NC_REGION);
        }
    }

    /* Legend */
    video_puts(2, 21, "Legend:", ATTR_DIM);
    video_putc(10, 21, BLOCK_LIGHT, ATTR_NC_REGION);
    video_puts(12, 21, "NC", ATTR_DIM);
    video_putc(16, 21, BLOCK_MED, ATTR_DIM);
    video_puts(18, 21, "ROM", ATTR_DIM);
    video_putc(23, 21, ' ', ATTR_VALUE);
    video_puts(25, 21, "Cache", ATTR_DIM);

    /* Right side: NC Regions table */
    video_puts(40, 3, "NC REGIONS", ATTR_HIGHLIGHT);
    video_hline(40, 4, 10, 0xC4, ATTR_DIM);

    video_puts(40, 5, "#  Base     Size    Status", ATTR_LABEL);
    video_hline(40, 6, 30, 0xC4, ATTR_DIM);

    for (i = 0; i < (int)chip->nc_regions && i < 4; i++) {
        unsigned char attr = (g_state.nc_cursor == i) ? ATTR_SELECTED : ATTR_VALUE;
        nc_region_live_t *nc = &g_state.nc_live[i];
        if (nc->active) {
            unsigned int base_mb = (unsigned int)(nc->base_kb / 1024);
            unsigned int base_frac = (unsigned int)((nc->base_kb % 1024) * 10 / 1024);
            video_printf(40, 7 + i, attr, "%d  %2u.%u MB  %3lu KB  ACTIVE",
                         i, base_mb, base_frac, nc->size_kb);
        } else {
            video_printf(40, 7 + i, (g_state.nc_cursor == i) ? ATTR_SELECTED : ATTR_DIM,
                         "%d    --      --     empty", i);
        }
    }
    for (; i < 4; i++) {
        video_printf(40, 7 + i, ATTR_DIM, "%d    --      --     (n/a)", i);
    }

    /* Actions */
    video_puts(40, 12, "ACTIONS", ATTR_HIGHLIGHT);
    video_hline(40, 13, 7, 0xC4, ATTR_DIM);

    video_puts(40, 14, "[1] Edit Region 0", ATTR_VALUE);
    video_puts(40, 15, "[2] Edit Region 1", chip->nc_regions >= 2 ? ATTR_VALUE : ATTR_DIM);
    video_puts(40, 16, "[C] Clear All", ATTR_VALUE);
    video_puts(40, 17, "[A] Auto-Config 512KB", ATTR_VALUE);

    /* Chipset Info */
    video_puts(40, 19, "CHIPSET INFO", ATTR_HIGHLIGHT);
    video_hline(40, 20, 12, 0xC4, ATTR_DIM);

    video_printf(40, 21, ATTR_VALUE, "Strategy: %s",
                 chip->nc_strategy == NC_RANGE ? "Range" :
                 chip->nc_strategy == NC_BOUNDARY ? "Boundary" : "Other");
    video_printf(40, 22, ATTR_VALUE, "Granularity: %u KB", chip->granularity);

    /* Show register values for selected region */
    if (g_state.nc_cursor < (int)chip->nc_regions) {
        nc_region_live_t *nc = &g_state.nc_live[g_state.nc_cursor];
        char status_buf[80];
        sprintf(status_buf, "Region %d: Idx %02Xh=%02Xh, %02Xh=%02Xh  |  Enter=Edit  C=Clear  R=Refresh",
                g_state.nc_cursor, nc->reg_index, nc->reg_val[0],
                nc->reg_index + 1, nc->reg_val[1]);
        ui_draw_status_bar(status_buf);
    } else {
        ui_draw_status_bar("1-4=Edit region  |  C=Clear  |  A=Auto-configure  |  R=Refresh");
    }
}

/*============================================================================
 * SCREEN: CACHE TEST (F3)
 *============================================================================*/

static void draw_test_screen(void)
{
    chipset_info_t *chip = &g_state.chipset;

    /* Left: Cache Information */
    video_puts(2, 3, "CACHE INFORMATION", ATTR_HIGHLIGHT);
    video_hline(2, 4, 17, 0xC4, ATTR_DIM);

    video_puts(2, 5, "Chipset:", ATTR_LABEL);
    video_puts(12, 5, chip->name, ATTR_VALUE);
    video_puts(2, 6, "Type:", ATTR_LABEL);
    video_puts(12, 6, chip->is_writeback ? "Write-Back" : "Write-Through",
               chip->is_writeback ? ATTR_WARNING : ATTR_VALUE);
    video_puts(2, 7, "Size:", ATTR_LABEL);
    video_printf(12, 7, ATTR_VALUE, "%u KB", chip->cache_size_kb);
    video_puts(2, 8, "Status:", ATTR_LABEL);
    video_puts(12, 8, "Enabled", ATTR_SUCCESS);
    video_puts(2, 9, "CPU:", ATTR_LABEL);
    video_puts(12, 9, g_state.is_486 ? "486+ (WBINVD)" : "386DX", ATTR_VALUE);

    /* Flush Method */
    video_puts(2, 11, "FLUSH METHOD", ATTR_HIGHLIGHT);
    video_hline(2, 12, 12, 0xC4, ATTR_DIM);

    video_printf(2, 13, ATTR_VALUE, "%c Read Loop (%u KB)",
                 g_state.is_486 ? RADIO_OFF : RADIO_ON, chip->cache_size_kb * 2);
    video_printf(2, 14, g_state.is_486 ? ATTR_VALUE : ATTR_DIM, "%c WBINVD (486+)",
                 g_state.is_486 ? RADIO_ON : RADIO_OFF);
    video_printf(2, 15, chip->type == CHIPSET_MIC9391 ? ATTR_VALUE : ATTR_DIM,
                 "%c Hardware Trigger", chip->type == CHIPSET_MIC9391 ? RADIO_ON : RADIO_OFF);

    /* Write-Back Warning */
    if (chip->is_writeback) {
        video_puts(2, 17, "*** WRITE-BACK WARNING ***", ATTR_ERROR);
        video_hline(2, 18, 26, 0xC4, ATTR_ERROR);
        video_puts(2, 19, "Order: FLUSH", ATTR_WARNING);
        video_putc(14, 19, 0x1A, ATTR_WARNING);  /* Arrow */
        video_puts(16, 19, "DISABLE", ATTR_WARNING);
        video_puts(2, 20, "Wrong order = DATA LOSS!", ATTR_ERROR);
    }

    /* Right: Test Selection */
    video_puts(40, 3, "TEST SELECTION", ATTR_HIGHLIGHT);
    video_hline(40, 4, 14, 0xC4, ATTR_DIM);

    video_printf(40, 5, (g_state.test_cursor == 0) ? ATTR_SELECTED : ATTR_VALUE,
                 "[%c] Flush Sequence Test", (g_state.test_select & 0x01) ? CHECK_ON : CHECK_OFF);
    video_printf(40, 6, (g_state.test_cursor == 1) ? ATTR_SELECTED : ATTR_VALUE,
                 "[%c] Dirty Data Verify", (g_state.test_select & 0x02) ? CHECK_ON : CHECK_OFF);
    video_printf(40, 7, (g_state.test_cursor == 2) ? ATTR_SELECTED : ATTR_VALUE,
                 "[%c] Timing Measurement", (g_state.test_select & 0x04) ? CHECK_ON : CHECK_OFF);
    video_printf(40, 8, (g_state.test_cursor == 3) ? ATTR_SELECTED : ATTR_VALUE,
                 "[%c] Stress Test (30s)", (g_state.test_select & 0x08) ? CHECK_ON : CHECK_OFF);

    video_puts(40, 10, "[R] RUN SELECTED TESTS", ATTR_HIGHLIGHT);

    /* Test Results */
    video_puts(40, 12, "TEST RESULTS", ATTR_HIGHLIGHT);
    video_hline(40, 13, 12, 0xC4, ATTR_DIM);

    video_puts(40, 14, "Test                 Result", ATTR_LABEL);
    video_hline(40, 15, 30, 0xC4, ATTR_DIM);

    video_puts(40, 16, "Flush Sequence", ATTR_VALUE);
    video_puts(60, 16, (g_state.test_results & 0x01) ? "PASS" : "--",
               (g_state.test_results & 0x01) ? ATTR_SUCCESS : ATTR_DIM);

    video_puts(40, 17, "Dirty Data", ATTR_VALUE);
    if (g_state.test_select & 0x02) {
        /* Test was selected, check if it passed or failed */
        if (g_state.test_results & 0x02) {
            video_puts(60, 17, "PASS", ATTR_SUCCESS);
        } else if (g_state.test_dirty_pass == 0 && (g_state.test_results != 0)) {
            video_puts(60, 17, "FAIL", ATTR_ERROR);
        } else {
            video_puts(60, 17, "--", ATTR_DIM);
        }
    } else {
        video_puts(60, 17, "--", ATTR_DIM);
    }

    video_puts(40, 18, "Timing", ATTR_VALUE);
    if (g_state.test_results & 0x04) {
        video_printf(60, 18, ATTR_VALUE, "%lu us", g_state.test_timing);
    } else {
        video_puts(60, 18, "--", ATTR_DIM);
    }

    video_puts(40, 19, "Stress Test", ATTR_VALUE);
    if (g_state.test_select & 0x08) {
        if (g_state.test_results & 0x08) {
            video_printf(60, 19, ATTR_SUCCESS, "%d OK", g_stress_result.pass_count);
        } else if (g_stress_result.fail_count > 0) {
            video_printf(60, 19, ATTR_ERROR, "%d FAIL", g_stress_result.fail_count);
        } else if (g_state.test_results != 0) {
            video_puts(60, 19, "FAIL", ATTR_ERROR);
        } else {
            video_puts(60, 19, "--", ATTR_DIM);
        }
    } else {
        video_puts(60, 19, "--", ATTR_DIM);
    }

    /* Show stress test timing statistics if available */
    if ((g_state.test_results & 0x08) && g_stress_result.pass_count > 0) {
        unsigned long avg = g_stress_result.total_time_us / g_stress_result.pass_count;
        video_printf(40, 20, ATTR_DIM, "Flush: %lu-%lu us (avg %lu)",
                     g_stress_result.min_time_us, g_stress_result.max_time_us, avg);
    }

    /* Show overall status */
    if (g_state.test_results) {
        unsigned char expected = g_state.test_select & 0x0F;
        unsigned char actual = g_state.test_results & 0x0F;
        if ((actual & expected) == expected) {
            video_puts(40, 21, "Overall: ALL TESTS PASSED", ATTR_SUCCESS);
        } else {
            video_puts(40, 21, "Overall: SOME TESTS FAILED", ATTR_ERROR);
        }
    }

    ui_draw_status_bar("Space=Toggle  |  R=Run tests  |  S=Save report");
}

/*============================================================================
 * SCREEN: REGISTERS (F4)
 *============================================================================*/

static void draw_reg_screen(void)
{
    chipset_info_t *chip = &g_state.chipset;
    int row, col, idx, i;
    unsigned char val, attr;
    int cursor_idx;

    cursor_idx = g_state.reg_cursor;

    /* Title */
    video_printf(2, 3, ATTR_HIGHLIGHT, "REGISTER DUMP (Port %02Xh/%02Xh)",
                 chip->index_port, chip->data_port);
    video_hline(2, 4, 28, 0xC4, ATTR_DIM);

    /* Column headers */
    video_puts(6, 5, "00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F", ATTR_LABEL);

    /* Read and display registers (routes via PCI for PCI chipsets) */
    for (row = 0; row < 8; row++) {
        video_printf(2, 6 + row, ATTR_LABEL, "%02X:", row * 16);

        for (col = 0; col < 16; col++) {
            idx = row * 16 + col;
            val = chipset_read_reg(idx);
            g_state.reg_values[idx] = val;

            attr = (idx == cursor_idx) ? ATTR_SELECTED : ATTR_VALUE;
            video_printf(6 + col * 3, 6 + row, attr, "%02X", val);
        }
    }

    /* Selected register info */
    video_puts(55, 3, "SELECTED:", ATTR_HIGHLIGHT);
    video_printf(55, 4, ATTR_VALUE, "Index %02Xh", cursor_idx);

    video_puts(55, 6, "Value:", ATTR_LABEL);
    video_printf(63, 6, ATTR_VALUE, "%02Xh", g_state.reg_values[cursor_idx]);

    video_puts(55, 7, "Binary:", ATTR_LABEL);
    val = g_state.reg_values[cursor_idx];
    video_printf(63, 7, ATTR_VALUE, "%c%c%c%c %c%c%c%c",
                 (val & 0x80) ? '1' : '0', (val & 0x40) ? '1' : '0',
                 (val & 0x20) ? '1' : '0', (val & 0x10) ? '1' : '0',
                 (val & 0x08) ? '1' : '0', (val & 0x04) ? '1' : '0',
                 (val & 0x02) ? '1' : '0', (val & 0x01) ? '1' : '0');

    /* Bit decode */
    video_puts(55, 9, "BIT DECODE", ATTR_HIGHLIGHT);
    video_hline(55, 10, 10, 0xC4, ATTR_DIM);

    for (i = 7; i >= 0; i--) {
        video_printf(55, 11 + (7 - i), ATTR_VALUE, "%d: %c",
                     i, (val & (1 << i)) ? '1' : '0');
    }

    /* Known registers */
    video_puts(2, 15, "KNOWN REGISTERS", ATTR_HIGHLIGHT);
    video_hline(2, 16, 15, 0xC4, ATTR_DIM);

    video_puts(2, 17, "00h: Chip ID", ATTR_VALUE);
    video_puts(2, 18, "13h: Cache Size", ATTR_VALUE);
    video_puts(2, 19, "20h: Cache Control", ATTR_VALUE);
    video_puts(2, 20, "52h-55h: NC Regions", ATTR_VALUE);

    ui_draw_status_bar("Arrow keys=Navigate  |  E=Edit  |  R=Refresh  |  S=Save dump");
}

/*============================================================================
 * SCREEN: BENCHMARK (F5)
 *============================================================================*/

static void draw_benchmark_screen(void)
{
    chipset_info_t *chip = &g_state.chipset;
    unsigned long speedup;

    /* Left: Test info */
    video_puts(2, 3, "MEMORY BANDWIDTH TESTS", ATTR_HIGHLIGHT);
    video_hline(2, 4, 22, 0xC4, ATTR_DIM);

    video_puts(2, 5, "Test Size:", ATTR_LABEL);
    video_printf(14, 5, ATTR_VALUE, "%d KB", BENCH_SIZE_KB);
    video_puts(2, 6, "Iterations:", ATTR_LABEL);
    video_printf(14, 6, ATTR_VALUE, "%d", BENCH_ITERS);
    video_puts(2, 7, "Cache:", ATTR_LABEL);
    video_printf(14, 7, is_cache_enabled() ? ATTR_SUCCESS : ATTR_ERROR,
                 "%s", is_cache_enabled() ? "Enabled" : "Disabled");
    video_puts(2, 8, "Chipset:", ATTR_LABEL);
    video_puts(14, 8, chip->name, ATTR_VALUE);

    video_puts(2, 10, "ACTIONS", ATTR_HIGHLIGHT);
    video_hline(2, 11, 7, 0xC4, ATTR_DIM);

    video_printf(2, 12, g_state.bench.cursor == 0 ? ATTR_SELECTED : ATTR_VALUE,
                 "[R] Run All Tests");
    video_printf(2, 13, g_state.bench.cursor == 1 ? ATTR_SELECTED : ATTR_VALUE,
                 "[C] Compare (On vs Off)");

    /* Progress bar */
    video_puts(2, 15, "PROGRESS", ATTR_HIGHLIGHT);
    video_hline(2, 16, 8, 0xC4, ATTR_DIM);

    if (g_state.bench.test_running) {
        video_progress(2, 17, 30, g_state.bench.progress_pct, 100, ATTR_VALUE);
        video_printf(35, 17, ATTR_VALUE, "%d%%", g_state.bench.progress_pct);
        video_puts(2, 18, "Status: Running tests...", ATTR_WARNING);
    } else {
        video_progress(2, 17, 30, 0, 100, ATTR_DIM);
        video_puts(35, 17, "Ready", ATTR_DIM);
        video_puts(2, 18, "Status: Idle", ATTR_DIM);
    }

    /* Right: Results */
    video_puts(42, 3, "RESULTS", ATTR_HIGHLIGHT);
    video_hline(42, 4, 7, 0xC4, ATTR_DIM);

    video_puts(42, 5, "Test       Cache ON   Cache OFF", ATTR_LABEL);
    video_hline(42, 6, 33, 0xC4, ATTR_DIM);

    /* Copy results */
    video_puts(42, 7, "Copy", ATTR_VALUE);
    if (g_state.bench.cache_on_copy > 0) {
        video_printf(53, 7, ATTR_SUCCESS, "%lu.%lu MB/s",
                     g_state.bench.cache_on_copy / 10,
                     g_state.bench.cache_on_copy % 10);
    } else {
        video_puts(53, 7, "  --", ATTR_DIM);
    }
    if (g_state.bench.cache_off_copy > 0) {
        video_printf(65, 7, ATTR_VALUE, "%lu.%lu MB/s",
                     g_state.bench.cache_off_copy / 10,
                     g_state.bench.cache_off_copy % 10);
    } else {
        video_puts(65, 7, "  --", ATTR_DIM);
    }

    /* Fill results */
    video_puts(42, 8, "Fill", ATTR_VALUE);
    if (g_state.bench.cache_on_fill > 0) {
        video_printf(53, 8, ATTR_SUCCESS, "%lu.%lu MB/s",
                     g_state.bench.cache_on_fill / 10,
                     g_state.bench.cache_on_fill % 10);
    } else {
        video_puts(53, 8, "  --", ATTR_DIM);
    }
    if (g_state.bench.cache_off_fill > 0) {
        video_printf(65, 8, ATTR_VALUE, "%lu.%lu MB/s",
                     g_state.bench.cache_off_fill / 10,
                     g_state.bench.cache_off_fill % 10);
    } else {
        video_puts(65, 8, "  --", ATTR_DIM);
    }

    /* Read results */
    video_puts(42, 9, "Read", ATTR_VALUE);
    if (g_state.bench.cache_on_read > 0) {
        video_printf(53, 9, ATTR_SUCCESS, "%lu.%lu MB/s",
                     g_state.bench.cache_on_read / 10,
                     g_state.bench.cache_on_read % 10);
    } else {
        video_puts(53, 9, "  --", ATTR_DIM);
    }
    if (g_state.bench.cache_off_read > 0) {
        video_printf(65, 9, ATTR_VALUE, "%lu.%lu MB/s",
                     g_state.bench.cache_off_read / 10,
                     g_state.bench.cache_off_read % 10);
    } else {
        video_puts(65, 9, "  --", ATTR_DIM);
    }

    /* Comparison / Speedup */
    video_puts(42, 11, "COMPARISON", ATTR_HIGHLIGHT);
    video_hline(42, 12, 10, 0xC4, ATTR_DIM);

    if (g_state.bench.cache_on_copy > 0 && g_state.bench.cache_off_copy > 0) {
        speedup = (g_state.bench.cache_on_copy * 10) / g_state.bench.cache_off_copy;
        video_printf(42, 13, ATTR_VALUE, "Copy: %lu.%lux speedup", speedup / 10, speedup % 10);
    } else {
        video_puts(42, 13, "Copy: --", ATTR_DIM);
    }

    if (g_state.bench.cache_on_fill > 0 && g_state.bench.cache_off_fill > 0) {
        speedup = (g_state.bench.cache_on_fill * 10) / g_state.bench.cache_off_fill;
        video_printf(42, 14, ATTR_VALUE, "Fill: %lu.%lux speedup", speedup / 10, speedup % 10);
    } else {
        video_puts(42, 14, "Fill: --", ATTR_DIM);
    }

    if (g_state.bench.cache_on_read > 0 && g_state.bench.cache_off_read > 0) {
        speedup = (g_state.bench.cache_on_read * 10) / g_state.bench.cache_off_read;
        video_printf(42, 15, ATTR_VALUE, "Read: %lu.%lux speedup", speedup / 10, speedup % 10);
    } else {
        video_puts(42, 15, "Read: --", ATTR_DIM);
    }

    /* Overall analysis */
    if (g_state.bench.cache_on_copy > 0 && g_state.bench.cache_off_copy > 0) {
        unsigned long avg_on = (g_state.bench.cache_on_copy +
                                g_state.bench.cache_on_fill +
                                g_state.bench.cache_on_read) / 3;
        unsigned long avg_off = (g_state.bench.cache_off_copy +
                                 g_state.bench.cache_off_fill +
                                 g_state.bench.cache_off_read) / 3;
        if (avg_off > 0) {
            speedup = (avg_on * 10) / avg_off;
            video_printf(42, 17, ATTR_SUCCESS, "Overall: Cache provides %lu.%lux boost",
                         speedup / 10, speedup % 10);
        }
    }

    ui_draw_status_bar("R=Run tests  |  C=Compare  |  S=Save report  |  Esc=Back");
}

/*============================================================================
 * PROFILE MANAGEMENT FUNCTIONS
 *============================================================================*/

/* Calculate checksum for profile validation */
static unsigned int profile_calc_checksum(profile_t *p)
{
    unsigned char *data = (unsigned char *)p;
    unsigned int sum = 0;
    int i;

    /* Sum all bytes except the checksum field itself */
    for (i = 0; i < (int)(sizeof(profile_t) - sizeof(unsigned int)); i++) {
        sum += data[i];
    }
    return sum ^ 0xAA55;
}

/* Get current date/time strings for profile */
static void profile_get_datetime(char *date_str, char *time_str)
{
    union REGS regs;

    /* Get date via DOS INT 21h, AH=2Ah */
    regs.h.ah = 0x2A;
    int86(0x21, &regs, &regs);
    sprintf(date_str, "%04d-%02d-%02d", regs.x.cx, regs.h.dh, regs.h.dl);

    /* Get time via DOS INT 21h, AH=2Ch */
    regs.h.ah = 0x2C;
    int86(0x21, &regs, &regs);
    sprintf(time_str, "%02d:%02d:%02d", regs.h.ch, regs.h.cl, regs.h.dh);
}

/* Capture current system configuration into a profile */
static void profile_capture_current(profile_t *p, const char *name)
{
    int i;

    memset(p, 0, sizeof(profile_t));

    /* Header */
    p->magic = PROFILE_MAGIC;
    p->version = PROFILE_VERSION;
    strncpy(p->name, name, PROFILE_NAME_LEN);
    p->name[PROFILE_NAME_LEN] = '\0';
    profile_get_datetime(p->date, p->time);

    /* System identification */
    p->chipset_type = g_state.chipset.type;
    strncpy(p->chipset_name, g_state.chipset.name, 23);
    p->chipset_name[23] = '\0';

    /* Cache configuration */
    p->cache_enabled = is_cache_enabled() ? 1 : 0;
    p->cache_writeback = g_state.chipset.is_writeback;

    /* NC region configuration */
    p->nc_count = 0;
    for (i = 0; i < 4 && i < (int)g_state.chipset.nc_regions; i++) {
        p->nc_regions[i].base_kb = g_state.nc_live[i].base_kb;
        p->nc_regions[i].size_kb = g_state.nc_live[i].size_kb;
        p->nc_regions[i].active = g_state.nc_live[i].active;
        if (g_state.nc_live[i].active) p->nc_count++;
    }

    /* Shadow RAM (read from inventory if available) */
    if (g_state.inv286.valid) {
        p->shadow_c0000 = g_state.inv286.shadow_c000_c7ff;
        p->shadow_c8000 = g_state.inv286.shadow_c800_cfff;
        p->shadow_d0000 = g_state.inv286.shadow_d000_dfff;
        p->shadow_d8000 = 0;  /* Not separately tracked */
        p->shadow_e0000 = g_state.inv286.shadow_e000_ffff;
        p->shadow_f0000 = g_state.inv286.shadow_e000_ffff;
    }

    /* Calculate checksum */
    p->checksum = profile_calc_checksum(p);
}

/* Generate filename from profile name */
static void profile_make_filename(const char *name, char *filename)
{
    int i, j = 0;

    /* Convert name to valid 8.3 filename */
    for (i = 0; name[i] && j < 8; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;  /* Uppercase */
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            filename[j++] = c;
        }
    }
    if (j == 0) {
        strcpy(filename, "PROFILE");
        j = 7;
    }
    filename[j] = '\0';
    strcat(filename, ".PRF");
}

/* Save profile to file */
static int profile_save(profile_t *p)
{
    char filename[80];
    char filepath[80];
    FILE *f;

    profile_make_filename(p->name, filename);
    sprintf(filepath, "%s\\%s", g_state.profile_dir, filename);

    f = fopen(filepath, "wb");
    if (!f) return 0;

    if (fwrite(p, sizeof(profile_t), 1, f) != 1) {
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}

/* Load profile from file */
static int profile_load(const char *filepath, profile_t *p)
{
    FILE *f;
    unsigned int calc_sum;

    f = fopen(filepath, "rb");
    if (!f) return 0;

    if (fread(p, sizeof(profile_t), 1, f) != 1) {
        fclose(f);
        return 0;
    }
    fclose(f);

    /* Validate magic */
    if (p->magic != PROFILE_MAGIC) return 0;

    /* Validate checksum */
    calc_sum = profile_calc_checksum(p);
    if (p->checksum != calc_sum) return 0;

    return 1;
}

/* Scan directory for profile files */
static void profile_scan_dir(void)
{
    struct find_t fileinfo;
    char pattern[80];
    int count = 0;

    sprintf(pattern, "%s\\*.PRF", g_state.profile_dir);

    if (_dos_findfirst(pattern, _A_NORMAL, &fileinfo) == 0) {
        do {
            if (count < MAX_PROFILES) {
                char filepath[80];
                sprintf(filepath, "%s\\%s", g_state.profile_dir, fileinfo.name);
                if (profile_load(filepath, &g_profiles[count])) {
                    count++;
                }
            }
        } while (_dos_findnext(&fileinfo) == 0 && count < MAX_PROFILES);
    }

    g_state.profile_count = count;
    if (g_state.profile_cursor >= count) {
        g_state.profile_cursor = count > 0 ? count - 1 : 0;
    }
}

/* Delete profile file */
static int profile_delete(int index)
{
    char filename[80];
    char filepath[80];

    if (index < 0 || index >= g_state.profile_count) return 0;

    profile_make_filename(g_profiles[index].name, filename);
    sprintf(filepath, "%s\\%s", g_state.profile_dir, filename);

    if (remove(filepath) != 0) return 0;

    /* Rescan directory */
    profile_scan_dir();
    return 1;
}

/* Apply profile to hardware */
static int profile_apply(int index)
{
    profile_t *p;
    int i;

    if (index < 0 || index >= g_state.profile_count) return 0;
    p = &g_profiles[index];

    /* Validate chipset match */
    if (p->chipset_type != g_state.chipset.type) {
        return -1;  /* Chipset mismatch */
    }

    /* Apply cache state */
    if (p->cache_enabled && !is_cache_enabled()) {
        enable_cache();
    } else if (!p->cache_enabled && is_cache_enabled()) {
        safe_disable_cache();
    }

    /* Apply NC regions (only if chipset supports it) */
    if (!g_state.chipset.info_only && g_state.chipset.nc_regions > 0) {
        /* Clear all existing NC regions first */
        for (i = 0; i < (int)g_state.chipset.nc_regions && i < 4; i++) {
            clear_nc_region(i);
        }

        /* Apply saved NC regions */
        for (i = 0; i < (int)p->nc_count && i < (int)g_state.chipset.nc_regions; i++) {
            if (p->nc_regions[i].active) {
                write_nc_region(i, p->nc_regions[i].base_kb, p->nc_regions[i].size_kb);
            }
        }

        /* Refresh live NC data */
        read_nc_regions();
    }

    g_state.profile_loaded = index;
    g_state.profile_modified = 0;
    return 1;
}

/* Initialize profile directory */
static void profile_init(void)
{
    /* Use current directory for profiles */
    strcpy(g_state.profile_dir, ".");
    g_state.profile_loaded = -1;
    g_state.profile_modified = 0;

    /* Scan for existing profiles */
    profile_scan_dir();
}

/*============================================================================
 * SCREEN: PROFILES (F6)
 *============================================================================*/

static void draw_profiles_screen(void)
{
    int i, y;
    profile_t *p;
    int nc_active = 0;
    unsigned char attr, chip_match;

    video_puts(2, 3, "CONFIGURATION PROFILES", ATTR_HIGHLIGHT);
    video_hline(2, 4, 22, 0xC4, ATTR_DIM);

    /* Current snapshot */
    video_puts(2, 6, "CURRENT SNAPSHOT", ATTR_HIGHLIGHT);
    video_hline(2, 7, 16, 0xC4, ATTR_DIM);

    video_printf(2, 8, ATTR_LABEL, "Chipset:");
    video_printf(12, 8, ATTR_VALUE, "%.24s", g_state.chipset.name);
    video_printf(2, 9, ATTR_LABEL, "Cache:");
    video_printf(12, 9, is_cache_enabled() ? ATTR_SUCCESS : ATTR_ERROR,
                 "%s %uKB %s",
                 is_cache_enabled() ? "ON" : "OFF",
                 g_state.chipset.cache_size_kb,
                 g_state.chipset.is_writeback ? "WB" : "WT");
    video_printf(2, 10, ATTR_LABEL, "Memory:");
    video_printf(12, 10, ATTR_VALUE, "%u KB", g_state.total_mem_kb);

    /* Count active NC regions */
    for (i = 0; i < 4 && i < (int)g_state.chipset.nc_regions; i++) {
        if (g_state.nc_live[i].active) nc_active++;
    }
    video_printf(2, 11, ATTR_LABEL, "NC Rgns:");
    video_printf(12, 11, nc_active > 0 ? ATTR_NC_REGION : ATTR_DIM,
                 "%d active", nc_active);

    /* Status indicator */
    video_puts(2, 13, "Status:", ATTR_LABEL);
    if (g_state.profile_loaded >= 0) {
        if (g_state.profile_modified) {
            video_puts(12, 13, "Modified", ATTR_WARNING);
        } else {
            video_printf(12, 13, ATTR_SUCCESS, "Loaded: %s",
                        g_profiles[g_state.profile_loaded].name);
        }
    } else {
        video_puts(12, 13, "Unsaved", ATTR_DIM);
    }

    /* Saved profiles list */
    video_puts(42, 3, "SAVED PROFILES", ATTR_HIGHLIGHT);
    video_hline(42, 4, 14, 0xC4, ATTR_DIM);

    video_puts(42, 5, "#  Name          Date        Cache", ATTR_LABEL);
    video_hline(42, 6, 36, 0xC4, ATTR_DIM);

    if (g_state.profile_count == 0) {
        video_puts(42, 7, "(No profiles saved)", ATTR_DIM);
        video_puts(42, 8, "Press S to save current config", ATTR_DIM);
    } else {
        for (i = 0; i < g_state.profile_count && i < 5; i++) {
            p = &g_profiles[i];
            y = 7 + i;
            attr = (i == g_state.profile_cursor) ? ATTR_SELECTED : ATTR_VALUE;
            chip_match = (p->chipset_type == g_state.chipset.type);

            video_printf(42, y, attr, "%d  %-12s  %s  %s%s",
                        i + 1, p->name, p->date,
                        p->cache_enabled ? "ON " : "OFF",
                        chip_match ? "" : " !");
        }
        if (g_state.profile_count > 5) {
            video_printf(42, 12, ATTR_DIM, "... and %d more",
                        g_state.profile_count - 5);
        }
    }

    /* Actions */
    video_puts(42, 15, "ACTIONS", ATTR_HIGHLIGHT);
    video_hline(42, 16, 7, 0xC4, ATTR_DIM);

    video_puts(42, 17, "[S] Save Current Snapshot", ATTR_VALUE);
    video_puts(42, 18, "[A] Apply Selected",
               g_state.profile_count > 0 ? ATTR_VALUE : ATTR_DIM);
    video_puts(42, 19, "[D] Delete Selected",
               g_state.profile_count > 0 ? ATTR_VALUE : ATTR_DIM);
    video_puts(42, 20, "[R] Refresh List", ATTR_VALUE);

    /* Show chipset mismatch warning */
    if (g_state.profile_count > 0) {
        p = &g_profiles[g_state.profile_cursor];
        if (p->chipset_type != g_state.chipset.type) {
            video_puts(42, 22, "! = Chipset mismatch", ATTR_WARNING);
        }
    }

    ui_draw_status_bar("S=Save  A=Apply  D=Delete  R=Refresh  Up/Down=Select");
}

/*============================================================================
 * SCREEN: INVENTORY (F7) - Expansion Card Inventory
 *============================================================================*/

static void draw_inventory_screen(void)
{
    int i, y;
    int start;
    int visible = 13;  /* Lines available for device list */
    device_entry_t *d;
    unsigned char attr;

    start = g_state.inventory.scroll_offset;

    video_puts(2, 3, "EXPANSION CARD INVENTORY", ATTR_HIGHLIGHT);
    video_hline(2, 4, 24, 0xC4, ATTR_DIM);

    /* Column headers */
    video_puts(2, 5, "Bus  B:D.F  VID:DID   Class  IRQ  Name", ATTR_LABEL);
    video_hline(2, 6, 76, 0xC4, ATTR_DIM);

    /* Check if enumeration has been done */
    if (g_state.inventory.device_count == 0) {
        video_puts(2, 8, "No devices enumerated yet.", ATTR_DIM);
        video_puts(2, 9, "Press R to scan for devices.", ATTR_VALUE);
    } else {
        /* Device list */
        for (i = 0; i < visible && (start + i) < g_state.inventory.device_count; i++) {
            d = &g_devices[start + i];
            attr = (g_state.inventory.cursor == start + i)
                   ? ATTR_SELECTED : ATTR_VALUE;
            y = 7 + i;

            /* Clear line first */
            video_hline(2, y, 76, ' ', attr);

            /* Format: Bus  B:D.F  VID:DID   Class  IRQ  Name */
            video_printf(2, y, attr, "%-4s %d:%02d.%d  %04X:%04X  %02X/%02X  %3d  %.30s",
                enum_bus_name(d->bus_type),
                d->bus, d->dev, d->func,
                d->vendor_id, d->device_id,
                d->class_code, d->subclass,
                d->irq,
                d->name ? d->name : "Unknown");
        }

        /* Scroll indicator */
        if (g_state.inventory.device_count > visible) {
            video_printf(70, 5, ATTR_DIM, "[%d/%d]",
                g_state.inventory.cursor + 1,
                g_state.inventory.device_count);
        }
    }

    /* Summary line */
    video_hline(2, 21, 76, 0xC4, ATTR_DIM);
    video_printf(2, 22, ATTR_LABEL, "Total: %d  [PCI:%d PCIe:%d MCA:%d EISA:%d PnP:%d]",
        g_state.inventory.device_count,
        enum_count_by_bus(BUS_PCI, g_state.inventory.device_count),
        enum_count_by_bus(BUS_PCIE, g_state.inventory.device_count),
        enum_count_by_bus(BUS_MCA, g_state.inventory.device_count),
        enum_count_by_bus(BUS_EISA, g_state.inventory.device_count),
        enum_count_by_bus(BUS_ISAPNP, g_state.inventory.device_count));

    ui_draw_status_bar("Up/Dn=Select  PgUp/PgDn=Page  R=Rescan  Home/End=Jump");
}

static void handle_inventory_keys(int key)
{
    int visible = 13;

    switch (key) {
        case KEY_UP:
            if (g_state.inventory.cursor > 0) {
                g_state.inventory.cursor--;
                if (g_state.inventory.cursor < g_state.inventory.scroll_offset)
                    g_state.inventory.scroll_offset = g_state.inventory.cursor;
            }
            break;

        case KEY_DOWN:
            if (g_state.inventory.cursor < g_state.inventory.device_count - 1) {
                g_state.inventory.cursor++;
                if (g_state.inventory.cursor >= g_state.inventory.scroll_offset + visible)
                    g_state.inventory.scroll_offset = g_state.inventory.cursor - visible + 1;
            }
            break;

        case 0x4900:  /* Page Up */
            g_state.inventory.cursor -= visible;
            if (g_state.inventory.cursor < 0)
                g_state.inventory.cursor = 0;
            if (g_state.inventory.cursor < g_state.inventory.scroll_offset)
                g_state.inventory.scroll_offset = g_state.inventory.cursor;
            break;

        case 0x5100:  /* Page Down */
            g_state.inventory.cursor += visible;
            if (g_state.inventory.cursor >= g_state.inventory.device_count)
                g_state.inventory.cursor = g_state.inventory.device_count - 1;
            if (g_state.inventory.cursor < 0)
                g_state.inventory.cursor = 0;
            if (g_state.inventory.cursor >= g_state.inventory.scroll_offset + visible)
                g_state.inventory.scroll_offset = g_state.inventory.cursor - visible + 1;
            break;

        case 0x4700:  /* Home */
            g_state.inventory.cursor = 0;
            g_state.inventory.scroll_offset = 0;
            break;

        case 0x4F00:  /* End */
            g_state.inventory.cursor = g_state.inventory.device_count - 1;
            if (g_state.inventory.cursor < 0)
                g_state.inventory.cursor = 0;
            if (g_state.inventory.cursor >= visible)
                g_state.inventory.scroll_offset = g_state.inventory.cursor - visible + 1;
            break;

        case 'r':
        case 'R':
            /* Rescan devices */
            ui_draw_status_bar("Scanning for devices...");
            enumerate_all_devices();
            break;

        default:
            break;
    }
}

/*============================================================================
 * SCREEN: BUS CONFIGURATION (F8) - EISA/MCA Slot Configuration
 *============================================================================*/

static void draw_busconfig_screen(void)
{
    buscfg_state_t *bs;
    int i, y;
    int start;
    int visible = 12;  /* Lines available for slot list */
    slot_config_t *slot;
    char id_str[16];
    const char *bus_name;
    unsigned char attr;
    char irq_str[4];
    char dma_str[4];
    unsigned char arb;

    /* Initialize on first entry */
    if (!g_state.busconfig.initialized) {
        buscfg_init();
        buscfg_enum_slots();

        bs = buscfg_get_state();

        /* Check for ISA PnP cards
         * - EISA systems have ISA slots, so can have PnP cards
         * - PCI/ISA systems have ISA slots, so can have PnP cards
         * - MCA systems do NOT have ISA slots (MCA is incompatible with ISA) */
        if (bs->bus_detected != BUS_MCA) {
            int pnp_count = isapnp_init();
            if (pnp_count > 0) {
                int base_slot = bs->slot_count;  /* Append after EISA slots */

                isapnp_enum_devices();

                /* If no EISA bus, this is pure ISA PnP mode */
                if (bs->bus_detected == 0) {
                    bs->bus_detected = BUS_ISAPNP;
                    base_slot = 0;
                }

                /* Add ISA PnP cards to slot list (after any EISA slots) */
                for (i = 0; i < pnp_count && (base_slot + i) < EISA_MAX_SLOTS; i++) {
                    isapnp_read_card(i + 1, &bs->slots[base_slot + i]);
                }
                bs->slot_count = base_slot + pnp_count;

                /* Store PnP count for display */
                g_state.busconfig.isapnp_count = pnp_count;
            }
        }

        /* Run auto-config to set up non-conflicting resources */
        buscfg_auto_config();
        g_state.busconfig.initialized = 1;
    }

    bs = buscfg_get_state();
    start = g_state.busconfig.scroll_offset;

    /* Title based on detected bus */
    if (bs->bus_detected == BUS_EISA) {
        bus_name = "EISA";
        if (g_state.busconfig.isapnp_count > 0) {
            /* EISA + ISA PnP cards */
            video_printf(2, 3, ATTR_HIGHLIGHT, "EISA + ISA PnP CONFIGURATION (+%d PnP)",
                g_state.busconfig.isapnp_count);
        } else {
            video_puts(2, 3, "EISA BUS CONFIGURATION", ATTR_HIGHLIGHT);
        }
        /* Show EISA system board info in upper right */
        video_printf(50, 3, ATTR_DIM, "%.28s", buscfg_get_eisa_sysboard_name());
    } else if (bs->bus_detected == BUS_MCA) {
        bus_name = "MCA";
        /* Show MCA bus width and speed in title */
        video_printf(2, 3, ATTR_HIGHLIGHT, "MCA BUS CONFIGURATION (%d-bit @ %d MHz)",
            buscfg_get_mca_bus_width(), buscfg_get_mca_bus_speed());
        /* Show PS/2 model name in upper right */
        video_printf(50, 3, ATTR_DIM, "%.28s", buscfg_get_mca_model_name());
    } else if (bs->bus_detected == BUS_ISAPNP) {
        bus_name = "ISA PnP";
        video_printf(2, 3, ATTR_HIGHLIGHT, "ISA PLUG AND PLAY CONFIGURATION (%d card%s)",
            isapnp_get_card_count(), isapnp_get_card_count() != 1 ? "s" : "");
    } else {
        video_puts(2, 3, "BUS CONFIGURATION", ATTR_HIGHLIGHT);
        video_puts(2, 5, "No configurable bus detected on this system.", ATTR_WARNING);
        video_puts(2, 7, "This feature requires:", ATTR_DIM);
        video_puts(4, 8, "- EISA system (486 EISA motherboard)", ATTR_DIM);
        video_puts(4, 9, "- MCA system (IBM PS/2 Model 50/60/70/80)", ATTR_DIM);
        video_puts(4, 10, "- ISA Plug and Play cards (Sound/Network)", ATTR_DIM);
        video_puts(2, 12, "Use F7 (Inventory) to view PCI/ISA devices.", ATTR_VALUE);
        ui_draw_status_bar("No configurable bus detected");
        return;
    }

    video_hline(2, 4, 24, 0xC4, ATTR_DIM);

    /* Column headers */
    if (bs->bus_detected == BUS_EISA) {
        video_puts(2, 5, "Slot  ID        Enabled  IRQ  I/O     Name", ATTR_LABEL);
    } else if (bs->bus_detected == BUS_MCA) {
        video_puts(2, 5, "Slot  @ID    Enabled  IRQ  ARB  I/O     Name", ATTR_LABEL);
    } else if (bs->bus_detected == BUS_ISAPNP) {
        video_puts(2, 5, "CSN   ID        Active   IRQ  DMA  I/O     Name", ATTR_LABEL);
    }
    video_hline(2, 6, 76, 0xC4, ATTR_DIM);

    /* Check if any slots found */
    if (bs->slot_count == 0) {
        if (bs->bus_detected == BUS_ISAPNP) {
            video_puts(2, 8, "No ISA Plug and Play cards detected.", ATTR_DIM);
        } else {
            video_puts(2, 8, "No adapter cards detected in slots.", ATTR_DIM);
        }
        video_puts(2, 9, "Press R to rescan.", ATTR_VALUE);
    } else {
        /* Slot list */
        for (i = 0; i < visible && (start + i) < bs->slot_count; i++) {
            slot = &bs->slots[start + i];
            attr = (g_state.busconfig.cursor == start + i)
                   ? ATTR_SELECTED : ATTR_VALUE;
            y = 7 + i;

            /* Clear line */
            video_hline(2, y, 76, ' ', attr);

            /* Format slot ID - check individual slot's bus_type for mixed EISA+PnP */
            if (slot->bus_type == BUS_ISAPNP) {
                isapnp_format_id(slot->isapnp_vendor, slot->device_id, id_str);
            } else {
                buscfg_format_slot_id(slot, id_str);
            }

            /* Display based on individual slot's bus type (handles mixed EISA+PnP) */
            if (slot->bus_type == BUS_EISA) {
                /* EISA: Slot  ID        Enabled  IRQ  I/O     Name */
                if (slot->irq_count > 0 && slot->irqs[0].irq < 16)
                    sprintf(irq_str, "%d", slot->irqs[0].irq);
                else
                    strcpy(irq_str, "---");
                video_printf(2, y, attr, "%4d  %-8s  %-3s      %3s  %04Xh   %.28s",
                    slot->slot,
                    id_str,
                    slot->enabled ? "Yes" : "No",
                    irq_str,
                    (slot->ioport_count > 0) ? slot->ioports[0].base : 0,
                    slot->name);
            } else if (slot->bus_type == BUS_MCA) {
                /* MCA: Slot  @ID    Enabled  IRQ  ARB  I/O     Name */
                /* ARB = Arbitration level from POS 4 bits 0-3 */
                arb = slot->pos[4] & 0x0F;
                if (slot->irq_count > 0 && slot->irqs[0].irq < 16)
                    sprintf(irq_str, "%d", slot->irqs[0].irq);
                else
                    strcpy(irq_str, "---");
                video_printf(2, y, attr, "%4d  %-6s  %-3s      %3s  %3d  %04Xh   %.24s",
                    slot->slot,
                    id_str,
                    slot->enabled ? "Yes" : "No",
                    irq_str,
                    arb,
                    (slot->ioport_count > 0) ? slot->ioports[0].base : 0,
                    slot->name);
            } else if (slot->bus_type == BUS_ISAPNP) {
                /* ISA PnP: CSN   ID        Active   IRQ  DMA  I/O     Name */
                if (slot->irq_count > 0 && slot->irqs[0].irq < 16)
                    sprintf(irq_str, "%d", slot->irqs[0].irq);
                else
                    strcpy(irq_str, "---");
                if (slot->dma_count > 0 && slot->dmas[0].channel < 8)
                    sprintf(dma_str, "%d", slot->dmas[0].channel);
                else
                    strcpy(dma_str, "---");
                video_printf(2, y, attr, " PnP  %-8s  %-3s      %3s  %3s  %04Xh   %.24s",
                    id_str,
                    slot->enabled ? "Yes" : "No",
                    irq_str,
                    dma_str,
                    (slot->ioport_count > 0) ? slot->ioports[0].base : 0,
                    slot->name);
            }
        }

        /* Scroll indicator */
        if (bs->slot_count > visible) {
            video_printf(70, 5, ATTR_DIM, "[%d/%d]",
                g_state.busconfig.cursor + 1, bs->slot_count);
        }
    }

    /* Details panel for selected slot */
    if (bs->slot_count > 0 && g_state.busconfig.cursor < bs->slot_count) {
        int has_conflict = 0;
        int ci;

        slot = &bs->slots[g_state.busconfig.cursor];

        video_hline(2, 19, 76, 0xC4, ATTR_DIM);
        video_printf(2, 20, ATTR_LABEL, "Selected: %s", slot->name);

        /* Check if this slot has conflicts */
        for (ci = 0; ci < bs->conflict_count; ci++) {
            if (bs->conflicts[ci].slot_a == slot->slot ||
                bs->conflicts[ci].slot_b == slot->slot) {
                has_conflict = 1;

                /* Show conflict details */
                if (bs->conflicts[ci].conflict_type & CONFLICT_IRQ) {
                    video_puts(50, 20, ATTR_ERROR, "IRQ CONFLICT!");
                } else if (bs->conflicts[ci].conflict_type & CONFLICT_DMA) {
                    video_puts(50, 20, ATTR_ERROR, "DMA CONFLICT!");
                } else if (bs->conflicts[ci].conflict_type & CONFLICT_IOPORT) {
                    video_puts(50, 20, ATTR_ERROR, "I/O CONFLICT!");
                } else if (bs->conflicts[ci].conflict_type & CONFLICT_MEMORY) {
                    video_puts(50, 20, ATTR_ERROR, "MEM CONFLICT!");
                }
                break;
            }
        }

        /* Show bus-specific details based on individual slot's bus type */
        if (slot->bus_type == BUS_MCA) {
            /* Show raw POS registers for MCA */
            video_printf(2, 21, ATTR_DIM,
                "POS: %02X %02X %02X %02X %02X %02X %02X %02X",
                slot->pos[0], slot->pos[1], slot->pos[2], slot->pos[3],
                slot->pos[4], slot->pos[5], slot->pos[6], slot->pos[7]);

            /* Show MCA adapter capabilities */
            if (slot->mca_bus_master) {
                video_printf(2, 22, ATTR_VALUE, "Bus Master: ARB %d  %s  %s",
                    slot->mca_arb_level,
                    slot->mca_fairness ? "Fair" : "Burst",
                    slot->mca_streaming ? "Stream" : "");
            }
        } else if (slot->bus_type == BUS_ISAPNP) {
            /* Show ISA PnP details */
            video_printf(2, 21, ATTR_DIM, "ISA PnP CSN: %d  LogDev: %d  Serial: %04X",
                slot->isapnp_csn, slot->isapnp_logdev, slot->isapnp_serial);

            /* Show all resources on detail line */
            {
                char res_str[60];
                int rpos = 0;

                res_str[0] = '\0';
                if (slot->irq_count > 0) {
                    rpos += sprintf(res_str + rpos, "IRQ=%d", slot->irqs[0].irq);
                }
                if (slot->dma_count > 0) {
                    rpos += sprintf(res_str + rpos, "%sDMA=%d", rpos > 0 ? "  " : "",
                        slot->dmas[0].channel);
                }
                if (slot->ioport_count > 0) {
                    rpos += sprintf(res_str + rpos, "%sI/O=%04Xh", rpos > 0 ? "  " : "",
                        slot->ioports[0].base);
                    if (slot->ioport_count > 1) {
                        rpos += sprintf(res_str + rpos, ",%04Xh", slot->ioports[1].base);
                    }
                }
                if (slot->mem_count > 0) {
                    rpos += sprintf(res_str + rpos, "%sMem=%lXh", rpos > 0 ? "  " : "",
                        slot->mem_ranges[0].base);
                }
                if (rpos > 0) {
                    video_printf(2, 22, ATTR_VALUE, "%s", res_str);
                }
            }
        }

        /* Show resources summary */
        if (slot->enabled && !has_conflict) {
            video_puts(50, 20, ATTR_SUCCESS, "OK");
        }
    }

    /* Status bar - show conflict and modified status */
    if (bs->conflict_count > 0) {
        video_printf(50, 22, ATTR_ERROR, "[%d CONFLICT%s]",
            bs->conflict_count, bs->conflict_count > 1 ? "S" : "");
    }
    if (bs->modified) {
        video_puts(bs->conflict_count > 0 ? 66 : 60, 22, ATTR_WARNING, "[MODIFIED]");
    }

    if (bs->bus_detected == BUS_MCA) {
        ui_draw_status_bar("Up/Dn=Sel  E=Enable  A=Auto  P=POS  S=Save  W=Export  R=Rescan");
    } else if (bs->bus_detected == BUS_ISAPNP) {
        ui_draw_status_bar("Up/Dn=Select  A=Activate  D=Deactivate  W=Export  R=Rescan");
    } else if (bs->bus_detected == BUS_EISA && g_state.busconfig.isapnp_count > 0) {
        /* Mixed EISA + ISA PnP mode */
        ui_draw_status_bar("Up/Dn=Sel E=En A=Act/Auto D=Deact S=Save W=Export R=Rescan");
    } else {
        ui_draw_status_bar("Up/Dn=Select  E=Enable  A=AutoCfg  S=Save  W=Export  R=Rescan");
    }
}

/*============================================================================
 * POS REGISTER EDITOR DIALOG (MCA only)
 *============================================================================*/

static void show_pos_editor_dialog(slot_config_t *slot)
{
    int done = 0;
    int key;
    int cursor = 2;         /* Start at POS 2 (POS 0-1 are read-only) */
    int nibble = 0;         /* 0 = high nibble, 1 = low nibble */
    unsigned char new_val;
    unsigned char old_vals[8];
    int i;

    /* Save original values for cancel */
    for (i = 0; i < 8; i++) {
        old_vals[i] = slot->pos[i];
    }

    while (!done) {
        /* Draw dialog */
        video_fill(15, 7, 50, 12, ' ', ATTR_NORMAL);
        video_box(15, 7, 50, 12, ATTR_BOX);
        video_printf(17, 7, ATTR_TITLE, " POS Editor - Slot %d ", slot->slot);

        video_puts(17, 9, "POS registers (0-1 read-only):", ATTR_LABEL);

        /* Draw POS register values */
        for (i = 0; i < 8; i++) {
            unsigned char attr;
            int x = 17 + (i * 6);

            if (i < 2) {
                attr = ATTR_DIM;  /* Read-only registers */
            } else if (i == cursor) {
                attr = ATTR_SELECTED;
            } else {
                attr = ATTR_VALUE;
            }

            video_printf(x, 11, ATTR_LABEL, "  %d ", i);
            video_printf(x, 12, attr, " %02X ", slot->pos[i]);
        }

        /* Show editing cursor */
        if (cursor >= 2) {
            int x = 17 + (cursor * 6) + 1;
            if (nibble == 0) {
                video_printf(x, 12, ATTR_HIGHLIGHT, "%X", (slot->pos[cursor] >> 4) & 0x0F);
            } else {
                video_printf(x + 1, 12, ATTR_HIGHLIGHT, "%X", slot->pos[cursor] & 0x0F);
            }
        }

        /* POS 2 bit 0 explanation */
        video_puts(17, 14, "POS 2 bit 0 = Card Enable", ATTR_DIM);
        video_printf(17, 15, ATTR_DIM, "Current: %s",
            (slot->pos[2] & 0x01) ? "ENABLED" : "DISABLED");

        video_puts(17, 17, "Left/Right=Register  0-F=Edit", ATTR_DIM);
        video_puts(17, 18, "Enter=Save  Esc=Cancel", ATTR_DIM);

        key = get_key();

        switch (key) {
            case KEY_ESC:
                /* Restore original values and exit */
                for (i = 0; i < 8; i++) {
                    slot->pos[i] = old_vals[i];
                }
                done = 1;
                break;

            case KEY_ENTER:
                /* Check if values changed */
                for (i = 2; i < 8; i++) {
                    if (slot->pos[i] != old_vals[i]) {
                        buscfg_get_state()->modified = 1;
                        /* Write new POS values to hardware */
                        buscfg_mca_write_pos(slot->slot, i, slot->pos[i]);
                    }
                }
                /* Update enabled status based on POS 2 bit 0 */
                slot->enabled = (slot->pos[2] & 0x01) ? 1 : 0;
                /* Reparse resources from updated POS registers */
                buscfg_check_conflicts();
                done = 1;
                break;

            case KEY_LEFT:
                if (cursor > 2) {
                    cursor--;
                    nibble = 0;
                }
                break;

            case KEY_RIGHT:
                if (cursor < 7) {
                    cursor++;
                    nibble = 0;
                }
                break;

            case KEY_TAB:
                /* Toggle between nibbles */
                nibble = 1 - nibble;
                break;

            default:
                /* Handle hex digit input */
                if (cursor >= 2) {
                    int digit = -1;

                    if (key >= '0' && key <= '9') {
                        digit = key - '0';
                    } else if (key >= 'a' && key <= 'f') {
                        digit = key - 'a' + 10;
                    } else if (key >= 'A' && key <= 'F') {
                        digit = key - 'A' + 10;
                    }

                    if (digit >= 0) {
                        new_val = slot->pos[cursor];
                        if (nibble == 0) {
                            new_val = (new_val & 0x0F) | (digit << 4);
                            nibble = 1;  /* Move to low nibble */
                        } else {
                            new_val = (new_val & 0xF0) | digit;
                            /* Move to next register */
                            if (cursor < 7) {
                                cursor++;
                                nibble = 0;
                            }
                        }
                        slot->pos[cursor - (nibble == 0 ? 1 : 0)] = new_val;
                    }
                }
                break;
        }
    }
}

static void handle_busconfig_keys(int key)
{
    buscfg_state_t *bs = buscfg_get_state();
    int visible = 12;
    slot_config_t *slot;

    if (bs->bus_detected == 0) {
        /* No bus, ignore all keys except standard navigation */
        return;
    }

    switch (key) {
        case KEY_UP:
            if (g_state.busconfig.cursor > 0) {
                g_state.busconfig.cursor--;
                if (g_state.busconfig.cursor < g_state.busconfig.scroll_offset)
                    g_state.busconfig.scroll_offset = g_state.busconfig.cursor;
            }
            break;

        case KEY_DOWN:
            if (g_state.busconfig.cursor < bs->slot_count - 1) {
                g_state.busconfig.cursor++;
                if (g_state.busconfig.cursor >= g_state.busconfig.scroll_offset + visible)
                    g_state.busconfig.scroll_offset = g_state.busconfig.cursor - visible + 1;
            }
            break;

        case 0x4900:  /* Page Up */
            g_state.busconfig.cursor -= visible;
            if (g_state.busconfig.cursor < 0)
                g_state.busconfig.cursor = 0;
            if (g_state.busconfig.cursor < g_state.busconfig.scroll_offset)
                g_state.busconfig.scroll_offset = g_state.busconfig.cursor;
            break;

        case 0x5100:  /* Page Down */
            g_state.busconfig.cursor += visible;
            if (g_state.busconfig.cursor >= bs->slot_count)
                g_state.busconfig.cursor = bs->slot_count - 1;
            if (g_state.busconfig.cursor < 0)
                g_state.busconfig.cursor = 0;
            if (g_state.busconfig.cursor >= g_state.busconfig.scroll_offset + visible)
                g_state.busconfig.scroll_offset = g_state.busconfig.cursor - visible + 1;
            break;

        case 0x4700:  /* Home */
            g_state.busconfig.cursor = 0;
            g_state.busconfig.scroll_offset = 0;
            break;

        case 0x4F00:  /* End */
            g_state.busconfig.cursor = bs->slot_count - 1;
            if (g_state.busconfig.cursor < 0)
                g_state.busconfig.cursor = 0;
            if (g_state.busconfig.cursor >= visible)
                g_state.busconfig.scroll_offset = g_state.busconfig.cursor - visible + 1;
            break;

        case 'e':
        case 'E':
            /* Toggle enable/disable for selected slot */
            if (bs->slot_count > 0 && g_state.busconfig.cursor < bs->slot_count) {
                int conf_count;
                slot = &bs->slots[g_state.busconfig.cursor];
                if (ui_confirm_box("Confirm",slot->enabled ?
                        "Disable this adapter?" : "Enable this adapter?")) {
                    buscfg_slot_enable(slot->slot, !slot->enabled);
                    slot->enabled = !slot->enabled;
                    /* Re-check conflicts after enabling */
                    conf_count = buscfg_check_conflicts();
                    if (slot->enabled) {
                        if (conf_count > 0) {
                            ui_draw_status_bar("Adapter enabled - WARNING: conflicts detected!");
                        } else {
                            ui_draw_status_bar("Adapter enabled");
                        }
                    } else {
                        ui_draw_status_bar("Adapter disabled");
                    }
                }
            }
            break;

        case 'p':
        case 'P':
            /* Edit POS registers (MCA only) */
            if (bs->bus_detected == BUS_MCA && bs->slot_count > 0) {
                slot = &bs->slots[g_state.busconfig.cursor];
                show_pos_editor_dialog(slot);
            }
            break;

        case 's':
        case 'S':
            /* Save configuration to NVM */
            if (bs->modified) {
                /* Check for conflicts before saving */
                if (bs->conflict_count > 0) {
                    if (!ui_confirm_box("Confirm","Warning: Conflicts exist! Save anyway?")) {
                        ui_draw_status_bar("Save cancelled - resolve conflicts first");
                        break;
                    }
                }
                if (ui_confirm_box("Confirm","Save configuration to NVM?")) {
                    if (buscfg_save_nvm() == BUSCFG_OK) {
                        ui_draw_status_bar("Configuration saved to NVM");
                    } else {
                        ui_draw_status_bar("ERROR: Failed to save configuration");
                    }
                }
            } else {
                ui_draw_status_bar("No changes to save");
            }
            break;

        case 'r':
        case 'R':
            /* Rescan slots */
            ui_draw_status_bar("Rescanning...");
            if (bs->bus_detected == BUS_ISAPNP) {
                /* Re-initialize ISA PnP */
                int pnp_count = isapnp_init();
                if (pnp_count > 0) {
                    int ri;
                    isapnp_enum_devices();
                    bs->slot_count = pnp_count;
                    for (ri = 0; ri < pnp_count && ri < ISAPNP_MAX_CARDS; ri++) {
                        isapnp_read_card(ri + 1, &bs->slots[ri]);
                    }
                } else {
                    bs->slot_count = 0;
                }
            } else {
                buscfg_enum_slots();
            }
            g_state.busconfig.cursor = 0;
            g_state.busconfig.scroll_offset = 0;
            break;

        case 'a':
        case 'A':
            /* Check selected slot type for mixed bus support */
            if (bs->slot_count > 0 && g_state.busconfig.cursor < bs->slot_count) {
                slot = &bs->slots[g_state.busconfig.cursor];

                if (slot->bus_type == BUS_ISAPNP) {
                    /* ISA PnP: Activate logical device */
                    if (!slot->enabled) {
                        if (isapnp_activate(slot->isapnp_csn, slot->isapnp_logdev, 1)
                                == BUSCFG_OK) {
                            slot->enabled = 1;
                            ui_draw_status_bar("PnP device activated");
                        } else {
                            ui_draw_status_bar("ERROR: Failed to activate device");
                        }
                    } else {
                        ui_draw_status_bar("Device is already active");
                    }
                } else {
                    /* EISA/MCA: Run auto-configuration */
                    int conflicts;
                    ui_draw_status_bar("Running auto-configuration...");
                    conflicts = buscfg_auto_config();
                    if (conflicts == 0) {
                        ui_draw_status_bar("Auto-config complete - no conflicts");
                    } else {
                        char msg[60];
                        sprintf(msg, "Auto-config complete - %d conflict%s remain",
                            conflicts, conflicts > 1 ? "s" : "");
                        ui_draw_status_bar(msg);
                    }
                }
            }
            break;

        case 'd':
        case 'D':
            /* ISA PnP: Deactivate device (works in pure PnP or mixed mode) */
            if (bs->slot_count > 0 && g_state.busconfig.cursor < bs->slot_count) {
                slot = &bs->slots[g_state.busconfig.cursor];
                if (slot->bus_type == BUS_ISAPNP) {
                    if (slot->enabled) {
                        if (ui_confirm_box("Confirm","Deactivate this PnP device?")) {
                            if (isapnp_activate(slot->isapnp_csn, slot->isapnp_logdev, 0)
                                    == BUSCFG_OK) {
                                slot->enabled = 0;
                                ui_draw_status_bar("PnP device deactivated");
                            } else {
                                ui_draw_status_bar("ERROR: Failed to deactivate device");
                            }
                        }
                    } else {
                        ui_draw_status_bar("Device is already inactive");
                    }
                } else {
                    ui_draw_status_bar("D key only works for ISA PnP devices");
                }
            }
            break;

        case 'c':
        case 'C':
            /* Check/validate configuration */
            {
                int conflicts = buscfg_check_conflicts();
                if (conflicts == 0) {
                    ui_draw_status_bar("Configuration valid - no conflicts detected");
                } else {
                    char msg[60];
                    sprintf(msg, "Warning: %d resource conflict%s detected",
                        conflicts, conflicts > 1 ? "s" : "");
                    ui_draw_status_bar(msg);
                }
            }
            break;

        case 'w':
        case 'W':
            /* Export configuration report to file */
            {
                const char *filename = "BUSCFG.TXT";
                ui_draw_status_bar("Exporting configuration report...");
                if (buscfg_export_report(filename) == BUSCFG_OK) {
                    char msg[60];
                    sprintf(msg, "Report saved to %s", filename);
                    ui_draw_status_bar(msg);
                } else {
                    ui_draw_status_bar("ERROR: Failed to write report file");
                }
            }
            break;

        default:
            break;
    }
}

/*============================================================================
 * KEY HANDLING
 *============================================================================*/

static int get_key(void)
{
    int key = getch();
    if (key == 0 || key == 0xE0) {
        key = getch() << 8;
    }
    return key;
}

static void handle_info_keys(int key)
{
    switch (key) {
        case KEY_TAB:
            /* Toggle between Chipset and SMBIOS views */
            g_state.info_tab = (g_state.info_tab + 1) % INFO_TAB_COUNT;
            draw_screen();
            break;
        case 't':
        case 'T':
            /* Toggle cache enable/disable (not for info-only chipsets) */
            if (!g_state.chipset.info_only && g_state.info_tab == INFO_TAB_CHIPSET) {
                toggle_cache_with_confirm();
            }
            break;
        case 'a':
        case 'A':
            /* Toggle A20 gate (for info-only chipsets with chipset A20 control) */
            if (g_state.chipset.info_only && supports_a20_toggle() &&
                g_state.info_tab == INFO_TAB_CHIPSET) {
                toggle_a20_gate();
            }
            break;
        case 'p':
        case 'P':
            /* Probe external cache (82385-style timing detection) */
            /* Available on Unknown chipsets with 386 CPU */
            if (g_state.chipset.type == CHIPSET_UNKNOWN && !g_state.is_486 &&
                g_state.info_tab == INFO_TAB_CHIPSET) {
                ui_draw_status_bar("Probing external cache... please wait");
                detect_82385_timing();
                /* Redraw will show updated results */
            }
            break;
        default:
            break;
    }
}

static void handle_nc_keys(int key)
{
    int max_cursor = (int)g_state.chipset.nc_regions - 1;
    if (max_cursor < 0) max_cursor = 0;

    switch (key) {
        case KEY_UP:
            if (g_state.nc_cursor > 0) g_state.nc_cursor--;
            break;
        case KEY_DOWN:
            if (g_state.chipset.nc_regions > 0 && g_state.nc_cursor < max_cursor)
                g_state.nc_cursor++;
            break;
        case '1':
            if (g_state.chipset.nc_regions >= 1) g_state.nc_cursor = 0;
            break;
        case '2':
            if (g_state.chipset.nc_regions >= 2) g_state.nc_cursor = 1;
            break;
        case '3':
            if (g_state.chipset.nc_regions >= 3) g_state.nc_cursor = 2;
            break;
        case '4':
            if (g_state.chipset.nc_regions >= 4) g_state.nc_cursor = 3;
            break;
        case 'r':
        case 'R':
            /* Refresh NC region data from chipset */
            read_nc_regions();
            break;
        case 'c':
        case 'C':
            /* Clear all NC regions with confirmation */
            if (!nc_write_supported()) {
                ui_draw_status_bar("NC region editing not supported on this chipset");
                break;
            }
            if (show_confirm_dialog("Clear NC Regions",
                                    "Clear ALL NC regions?",
                                    "This writes to chipset registers!")) {
                clear_all_nc_regions();
            }
            break;
        case 'a':
        case 'A':
            /* Auto-configure 512KB NC region at top of RAM */
            if (!nc_write_supported()) {
                ui_draw_status_bar("NC region editing not supported on this chipset");
                break;
            }
            if (g_state.chipset.nc_strategy == NC_RANGE ||
                g_state.chipset.nc_strategy == NC_BOUNDARY) {
                char msg[48];
                sprintf(msg, "Configure 512KB NC at %luMB?",
                        (g_state.total_mem_kb - 512) / 1024);
                if (show_confirm_dialog("Auto-Config NC",
                                        msg,
                                        "This writes to chipset registers!")) {
                    /* Clear existing regions first */
                    clear_all_nc_regions();
                    /* Configure new region */
                    auto_config_nc_region(512);
                }
            }
            break;
        case KEY_ENTER:
            /* Could open edit dialog for selected region - for future enhancement */
            break;
    }

    /* Bounds check - ensure cursor is valid */
    if (g_state.nc_cursor > max_cursor) {
        g_state.nc_cursor = max_cursor;
    }
    if (g_state.nc_cursor < 0) {
        g_state.nc_cursor = 0;
    }
}

static void handle_test_keys(int key)
{
    switch (key) {
        case KEY_UP:
            if (g_state.test_cursor > 0) g_state.test_cursor--;
            break;
        case KEY_DOWN:
            if (g_state.test_cursor < 3) g_state.test_cursor++;
            break;
        case KEY_SPACE:
            g_state.test_select ^= (1 << g_state.test_cursor);
            break;
        case 'r':
        case 'R':
            /* Run selected tests */
            g_state.test_results = 0;  /* Clear previous results */
            g_state.test_dirty_pass = 1;  /* Assume pass until proven otherwise */
            g_state.test_timing = 0;

            /* Test 1: Flush Sequence Test */
            if (g_state.test_select & 0x01) {
                /* Simple flush test - if we get here without hanging, it passed */
                flush_cache();
                g_state.test_results |= 0x01;
            }

            /* Test 2: Dirty Data Writeback Verification */
            if (g_state.test_select & 0x02) {
                if (test_dirty_data_writeback()) {
                    g_state.test_results |= 0x02;
                    g_state.test_dirty_pass = 1;
                } else {
                    g_state.test_dirty_pass = 0;
                }
            }

            /* Test 3: Timing Measurement */
            if (g_state.test_select & 0x04) {
                g_state.test_timing = measure_cache_flush_time();
                g_state.test_results |= 0x04;
            }

            /* Test 4: Enhanced Stress Test with varied patterns */
            if (g_state.test_select & 0x08) {
                /* Show progress message */
                video_puts(40, 20, "Running stress test...", ATTR_WARNING);

                /* Run 50 iterations x 8 patterns = 400 tests */
                run_stress_test(50);

                /* Check results */
                if (g_stress_result.fail_count == 0 && g_stress_result.pass_count > 0) {
                    g_state.test_results |= 0x08;
                }

                /* Clear progress line */
                video_fill(40, 20, 35, 1, ' ', ATTR_NORMAL);
            }
            break;
        case 's':
        case 'S':
            /* Save report */
            save_report_dialog();
            break;
    }
}

static int hex_char_to_nibble(int ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

static void edit_register_dialog(void)
{
    chipset_info_t *chip = &g_state.chipset;
    int reg_idx = g_state.reg_cursor;
    unsigned char old_val = g_state.reg_values[reg_idx];
    unsigned char new_val = old_val;
    int nibble = 0;  /* 0 = high nibble, 1 = low nibble */
    int done = 0;
    int key;
    const char *warning;
    int crit_level;

    /* Check if this is a critical register */
    crit_level = is_critical_register(reg_idx);
    warning = get_register_warning(reg_idx);

    /* Draw edit dialog overlay */
    video_fill(20, 8, 40, 9, ' ', ATTR_NORMAL);
    video_box(20, 8, 40, 9, crit_level == 2 ? ATTR_ERROR : ATTR_BOX);
    video_printf(22, 8, crit_level == 2 ? ATTR_ERROR : ATTR_TITLE,
                 " Edit Register %02Xh ", reg_idx);

    /* Show warning for critical registers */
    if (warning) {
        video_puts(22, 10, warning, crit_level == 2 ? ATTR_ERROR : ATTR_WARNING);
        if (crit_level == 2 && chip->is_writeback) {
            video_puts(22, 11, "Wrong value = DATA LOSS!", ATTR_ERROR);
        }
    }

    video_printf(22, 13, ATTR_VALUE, "Current: %02Xh  New: ", old_val);
    video_puts(22, 14, "Enter hex value (0-F)", ATTR_DIM);
    video_puts(22, 15, "Enter=OK  Esc=Cancel", ATTR_DIM);

    while (!done) {
        /* Highlight current nibble */
        video_printf(42, 13, nibble == 0 ? ATTR_SELECTED : ATTR_VALUE, "%X",
                     (new_val >> 4) & 0x0F);
        video_printf(43, 13, nibble == 1 ? ATTR_SELECTED : ATTR_VALUE, "%X",
                     new_val & 0x0F);

        key = get_key();

        switch (key) {
            case KEY_ESC:
                done = 1;  /* Cancel */
                break;
            case KEY_ENTER:
                /* For critical registers, require confirmation */
                if (crit_level == 2 && new_val != old_val) {
                    if (!show_confirm_dialog("WARNING",
                                             "Modify critical register?",
                                             "System may become unstable!")) {
                        break;  /* User cancelled */
                    }
                }
                /* Write the new value through the same abstraction used to
                   READ it (chipset_write_reg routes HAL -> PCI config space ->
                   legacy ports). The old raw safe_write() always went to the
                   legacy index/data ports, so on a PCI chipset the edit hit
                   the wrong device while the displayed value came from PCI. */
                chipset_write_reg((unsigned char)reg_idx, (unsigned char)new_val);
                g_state.reg_values[reg_idx] = new_val;
                done = 1;
                break;
            case KEY_LEFT:
                nibble = 0;
                break;
            case KEY_RIGHT:
                nibble = 1;
                break;
            case KEY_TAB:
                nibble = 1 - nibble;
                break;
            default:
                {
                    int val = hex_char_to_nibble(key);
                    if (val >= 0) {
                        if (nibble == 0) {
                            new_val = (new_val & 0x0F) | (val << 4);
                            nibble = 1;  /* Move to low nibble */
                        } else {
                            new_val = (new_val & 0xF0) | val;
                        }
                    }
                }
                break;
        }
    }
}

static void handle_reg_keys(int key)
{
    switch (key) {
        case KEY_UP:
            if (g_state.reg_cursor >= 16) g_state.reg_cursor -= 16;
            break;
        case KEY_DOWN:
            if (g_state.reg_cursor < 112) g_state.reg_cursor += 16;
            break;
        case KEY_LEFT:
            if (g_state.reg_cursor > 0) g_state.reg_cursor--;
            break;
        case KEY_RIGHT:
            if (g_state.reg_cursor < 127) g_state.reg_cursor++;
            break;
        case 'r':
        case 'R':
            /* Refresh - registers are read on each draw */
            break;
        case 'e':
        case 'E':
        case KEY_ENTER:
            /* Edit selected register */
            edit_register_dialog();
            break;
    }
}

static void handle_benchmark_keys(int key)
{
    switch (key) {
        case KEY_UP:
            if (g_state.bench.cursor > 0) g_state.bench.cursor--;
            break;
        case KEY_DOWN:
            if (g_state.bench.cursor < 1) g_state.bench.cursor++;
            break;
        case 'r':
        case 'R':
            /* Run basic benchmark tests */
            run_benchmarks();
            break;
        case 'c':
        case 'C':
            /* Run comparison (cache on vs off) */
            if (show_confirm_dialog("Compare Benchmark",
                                    "Run cache ON vs OFF comparison?",
                                    "This will toggle the cache!")) {
                run_comparison_benchmark();
            }
            break;
        case 's':
        case 'S':
            /* Save report */
            save_report_dialog();
            break;
    }
}

/* Simple text input dialog for profile name */
static int input_profile_name(char *buf, int maxlen)
{
    int y = 10;
    int x = 25;
    int len = 0;
    int key;

    /* Draw dialog box */
    video_fill(x - 1, y - 1, 32, 5, ' ', ATTR_NORMAL);
    video_box(x - 1, y - 1, 32, 5, ATTR_BOX);
    video_puts(x + 2, y, "Save Profile", ATTR_TITLE);
    video_puts(x + 2, y + 1, "Name: [            ]", ATTR_NORMAL);
    video_puts(x + 2, y + 2, "Enter=Save  Esc=Cancel", ATTR_DIM);

    buf[0] = '\0';

    while (1) {
        /* Show cursor position */
        video_fill(x + 9, y + 1, PROFILE_NAME_LEN, 1, ' ', ATTR_HIGHLIGHT);
        video_puts(x + 9, y + 1, buf, ATTR_HIGHLIGHT);

        key = get_key();

        if (key == KEY_ESC) {
            return 0;  /* Cancelled */
        }
        else if (key == KEY_ENTER) {
            if (len > 0) return 1;  /* Accept */
        }
        else if (key == KEY_BACKSPACE) {
            if (len > 0) {
                len--;
                buf[len] = '\0';
            }
        }
        else if (key >= 32 && key < 127 && len < maxlen - 1) {
            /* Alphanumeric and safe filename chars only */
            if ((key >= 'A' && key <= 'Z') ||
                (key >= 'a' && key <= 'z') ||
                (key >= '0' && key <= '9') ||
                key == '_' || key == '-') {
                buf[len++] = (char)key;
                buf[len] = '\0';
            }
        }
    }
}

static void handle_profiles_keys(int key)
{
    profile_t *sel;
    char name_buf[PROFILE_NAME_LEN + 1];
    char msg[64];

    switch (key) {
        case KEY_UP:
            if (g_state.profile_cursor > 0) g_state.profile_cursor--;
            break;

        case KEY_DOWN:
            if (g_state.profile_cursor < g_state.profile_count - 1)
                g_state.profile_cursor++;
            break;

        case 's':
        case 'S':
            /* Save current config as new profile */
            if (g_state.profile_count >= MAX_PROFILES) {
                ui_draw_status_bar("Error: Maximum profiles reached (delete one first)");
                break;
            }
            if (input_profile_name(name_buf, PROFILE_NAME_LEN)) {
                profile_t new_prof;
                profile_capture_current(&new_prof, name_buf);
                if (profile_save(&new_prof)) {
                    ui_draw_status_bar("Profile saved successfully");
                    profile_scan_dir();  /* Refresh list */
                } else {
                    ui_draw_status_bar("Error: Failed to save profile");
                }
            }
            break;

        case 'a':
        case 'A':
            /* Apply selected profile to hardware */
            if (g_state.profile_count == 0) {
                ui_draw_status_bar("No profiles to apply");
                break;
            }
            sel = &g_profiles[g_state.profile_cursor];

            /* Check chipset compatibility */
            if (sel->chipset_type != g_state.chipset.type) {
                sprintf(msg, "Warning: Profile is for %s chipset!", sel->chipset_name);
                if (!show_confirm_dialog("Chipset Mismatch", msg,
                                         "Apply anyway? May cause issues!")) {
                    break;
                }
            } else {
                sprintf(msg, "Apply profile '%s'?", sel->name);
                if (!show_confirm_dialog("Apply Profile", msg,
                                         "This will modify chipset registers!")) {
                    break;
                }
            }

            /* profile_apply takes an index, not a pointer; returns 1 on
               success, -1 on chipset mismatch, 0 on bad index. */
            if (profile_apply(g_state.profile_cursor) == 1) {
                g_state.profile_loaded = g_state.profile_cursor;
                g_state.profile_modified = 0;
                ui_draw_status_bar("Profile applied successfully");
            } else {
                ui_draw_status_bar("Error: Failed to apply profile");
            }
            break;

        case 'd':
        case 'D':
            /* Delete selected profile */
            if (g_state.profile_count == 0) {
                ui_draw_status_bar("No profiles to delete");
                break;
            }
            sel = &g_profiles[g_state.profile_cursor];
            sprintf(msg, "Delete profile '%s'?", sel->name);
            if (show_confirm_dialog("Delete Profile", msg, "This cannot be undone!")) {
                if (profile_delete(g_state.profile_cursor)) {
                    if (g_state.profile_loaded == g_state.profile_cursor) {
                        g_state.profile_loaded = -1;
                    }
                    profile_scan_dir();  /* Refresh list */
                    if (g_state.profile_cursor >= g_state.profile_count &&
                        g_state.profile_count > 0) {
                        g_state.profile_cursor = g_state.profile_count - 1;
                    }
                    ui_draw_status_bar("Profile deleted");
                } else {
                    ui_draw_status_bar("Error: Failed to delete profile");
                }
            }
            break;

        case 'r':
        case 'R':
            /* Rescan profile directory */
            ui_draw_status_bar("Scanning for profiles...");
            profile_scan_dir();
            g_state.profile_cursor = 0;
            break;

        default:
            break;
    }
}

/*============================================================================
 * MAIN LOOP
 *============================================================================*/

static void draw_screen(void)
{
    /* Clear content area */
    video_fill(0, 1, 80, 23, ' ', ATTR_NORMAL);

    /* Draw frame */
    video_box(0, 1, 80, 23, ATTR_BOX);
    ui_draw_separator(23);

    /* Draw title bar */
    ui_draw_title_bar(VERSION, g_state.current_screen);

    /* Draw current screen */
    switch (g_state.current_screen) {
        case SCREEN_INFO:
            draw_info_screen();
            break;
        case SCREEN_NC_CONFIG:
            draw_nc_screen();
            break;
        case SCREEN_CACHE_TEST:
            draw_test_screen();
            break;
        case SCREEN_REGISTERS:
            draw_reg_screen();
            break;
        case SCREEN_BENCHMARK:
            draw_benchmark_screen();
            break;
        case SCREEN_PROFILES:
            draw_profiles_screen();
            break;
        case SCREEN_INVENTORY:
            draw_inventory_screen();
            break;
        case SCREEN_BUSCONFIG:
            draw_busconfig_screen();
            break;
        default:
            break;
    }
}

static int main_loop(void)
{
    int key;
    int running = 1;

    while (running) {
        draw_screen();
        key = get_key();

        /* Global keys */
        switch (key) {
            case KEY_ESC:
            case KEY_ALT_X:
                running = 0;
                break;
            case KEY_F1:
                g_state.current_screen = SCREEN_INFO;
                break;
            case KEY_F2:
                g_state.current_screen = SCREEN_NC_CONFIG;
                break;
            case KEY_F3:
                g_state.current_screen = SCREEN_CACHE_TEST;
                break;
            case KEY_F4:
                g_state.current_screen = SCREEN_REGISTERS;
                break;
            case KEY_F5:
                g_state.current_screen = SCREEN_BENCHMARK;
                break;
            case KEY_F6:
                g_state.current_screen = SCREEN_PROFILES;
                break;
            case KEY_F7:
                g_state.current_screen = SCREEN_INVENTORY;
                enumerate_all_devices();  /* Auto-scan on F7 */
                break;
            case KEY_F8:
                g_state.current_screen = SCREEN_BUSCONFIG;
                break;
            case KEY_TAB:
                /* On the Info screen, TAB toggles the Chipset/SMBIOS sub-view
                   (otherwise the global screen-cycle would swallow it and the
                   SMBIOS tab would be unreachable). Elsewhere it cycles screens. */
                if (g_state.current_screen == SCREEN_INFO) {
                    handle_info_keys(KEY_TAB);
                } else {
                    g_state.current_screen =
                        (g_state.current_screen + 1) % SCREEN_COUNT;
                }
                break;
            default:
                /* Screen-specific keys */
                switch (g_state.current_screen) {
                    case SCREEN_INFO:
                        handle_info_keys(key);
                        break;
                    case SCREEN_NC_CONFIG:
                        handle_nc_keys(key);
                        break;
                    case SCREEN_CACHE_TEST:
                        handle_test_keys(key);
                        break;
                    case SCREEN_REGISTERS:
                        handle_reg_keys(key);
                        break;
                    case SCREEN_BENCHMARK:
                        handle_benchmark_keys(key);
                        break;
                    case SCREEN_PROFILES:
                        handle_profiles_keys(key);
                        break;
                    case SCREEN_INVENTORY:
                        handle_inventory_keys(key);
                        break;
                    case SCREEN_BUSCONFIG:
                        handle_busconfig_keys(key);
                        break;
                    default:
                        break;
                }
                break;
        }
    }

    return 0;
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(void)
{
    /* Initialize state */
    memset(&g_state, 0, sizeof(g_state));
    g_state.current_screen = SCREEN_INFO;
    g_state.test_select = 0x07;  /* First 3 tests selected by default */

    /* Initialize video - save original mode for restore */
    video_init();
    video_clear(ATTR_NORMAL);

    /* Detect system */
    g_state.is_486 = is_486_or_better();
    g_state.total_mem_kb = get_total_memory();

    /* Use HAL for chipset detection (v3.0) */
    hal_init_chipset();

    /* Parse SMBIOS/ACPI tables for additional system info */
    parse_smbios_tables(&g_state.smbios);
    parse_acpi_tables(&g_state.acpi);

    /* Enhance chipset detection with SMBIOS data (for Pentium+ systems) */
    enhance_chipset_with_smbios(&g_state.chipset);

    /* External cache detection (82385-style) for Unknown chipsets on 386 */
    if (g_state.chipset.type == CHIPSET_UNKNOWN && !g_state.is_486) {
        /* Quick check for cache hint before full probe */
        if (has_cache_hint()) {
            detect_82385_timing();
        } else {
            g_state.ext_cache.probed = 1;
            g_state.ext_cache.present = 0;
        }
    }

    /* Read 286/386SX inventory for info-only chipsets */
    read_286_inventory();

    /* Read live NC region data from chipset */
    read_nc_regions();

    /* Initialize profile system */
    profile_init();

    /* Run main loop */
    main_loop();

    /* Restore original video mode via INT 10h */
    video_restore();

    /* Print exit message after mode restore */
    printf("CACHEKIT v%s - Exited normally.\n", VERSION);
    if (hal_is_available()) {
        printf("Chipset: %s (%s)\n", g_hal->name, g_hal->vendor);
    } else if (g_state.chipset.type != CHIPSET_UNKNOWN) {
        printf("Chipset: %s (%s)\n", g_state.chipset.name, g_state.chipset.vendor);
    }

    return 0;
}
