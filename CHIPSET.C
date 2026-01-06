/*
 * CHIPSET.C - 386/486 Chipset Detection Utility
 *
 * Part of the Abacus FPGA Project
 * For Open Watcom C (16-bit real mode DOS)
 *
 * Detects 17+ chipsets with cache type, NC strategy, and port addresses.
 * Based on 386-chipset-reference.md safe detection algorithm.
 *
 * Last Updated: 2026-01-04 15:45:00 EST
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <i86.h>

/*============================================================================
 * CHIPSET TYPE DEFINITIONS
 *============================================================================*/

/* S-TIER (Score 9.0+): Driver's Dream */
#define CHIPSET_SIS_460      1   /* 23h, WB, 4x Range, 64KB - 486 GOLD STANDARD */
#define CHIPSET_SIS_RABBIT   2   /* 23h, WB, 4x Range, 64KB - 386 GOLD STANDARD */
#define CHIPSET_OPTI391      3   /* 24h, WB, 2x Range, 8KB */

/* A-TIER (Score 8.0-8.9): High Performance */
#define CHIPSET_FARADAY      4   /* 23h, WT, 3x Range, 128KB */
#define CHIPSET_UMC491       5   /* 23h, WB*, 1x Range, 8KB */
#define CHIPSET_MIC9391      6   /* 23h, WB, 1x Range + HW Flush! */
#define CHIPSET_ETEQ_BENGAL  7   /* 24h, WB, 2x Range (OPTi clone) */

/* B-TIER (Score 7.0-7.9): Solid but Limited */
#define CHIPSET_OPTI381      8   /* 24h, WT, 2x Range, 512KB */
#define CHIPSET_VIA          9   /* 23h, WT, 2x Range, 64KB */
#define CHIPSET_VLSI        10   /* 23h, WT, 1x Range, 64KB */

/* C-TIER (Score 5.0-6.9): Boundary Problem */
#define CHIPSET_CT_PEAK     11   /* 23h, WT, Boundary, 64KB */
#define CHIPSET_CT_SCAT     12   /* 23h, WT, Boundary, 64KB */
#define CHIPSET_ALI_FINIS   13   /* 23h, WT, Boundary, 64KB */

/* D-TIER (Score <5.0): Primitive */
#define CHIPSET_FOREX       14   /* 23h, WT, Boundary, 1MB */
#define CHIPSET_SUNTAC      15   /* 23h, WT, Segments, 64KB */

/* SPECIAL */
#define CHIPSET_EISA_82350  16   /* EISA bus (memory signature) */
#define CHIPSET_CT_NEAT     17   /* 23h, WT, Shadow Only */
#define CHIPSET_HEADLAND    18   /* 23h, WT, Steering */

#define CHIPSET_UNKNOWN      0

/* NC Strategy Types */
#define NC_NONE       0
#define NC_RANGE      1   /* Programmable start/size (BEST) */
#define NC_BOUNDARY   2   /* Everything above boundary = NC */
#define NC_STEERING   3   /* Disable DRAM banks */
#define NC_SHADOW     4   /* 640K-1M only */
#define NC_SEGMENTS   5   /* Fixed segment exclusions */

/*============================================================================
 * CHIPSET INFO STRUCTURE
 *============================================================================*/

typedef struct {
    unsigned char type;         /* CHIPSET_xxx define */
    unsigned char is_writeback; /* 1 = WB, 0 = WT */
    unsigned char nc_strategy;  /* NC_xxx define */
    unsigned char nc_regions;   /* Number of NC regions available */
    unsigned int  index_port;   /* Usually 0x22 */
    unsigned int  data_port;    /* 0x23 or 0x24 */
    unsigned int  cache_size_kb;/* Detected cache size */
    unsigned char id_value;     /* Raw ID value read */
    unsigned char id_index;     /* Index register where ID was found */
    const char   *name;         /* Human-readable name */
    const char   *tier;         /* S, A, B, C, D tier */
    unsigned char score_x10;    /* Score * 10 (e.g., 92 = 9.2) */
    unsigned int  granularity;  /* NC granularity in KB */
} chipset_info_t;

/*============================================================================
 * CHIPSET DATABASE
 *============================================================================*/

static const struct {
    unsigned char type;
    const char   *name;
    const char   *tier;
    unsigned char score_x10;
    unsigned char is_wb;
    unsigned char nc_strategy;
    unsigned char nc_regions;
    unsigned int  granularity;
} chipset_db[] = {
    { CHIPSET_SIS_460,    "SiS 85C460 (486)",        "S", 96, 1, NC_RANGE,    4,  64 },
    { CHIPSET_SIS_RABBIT, "SiS 85C310 (Rabbit)",     "S", 95, 1, NC_RANGE,    4,  64 },
    { CHIPSET_OPTI391,    "OPTi 82C391 (SYSC)",      "S", 92, 1, NC_RANGE,    2,   8 },
    { CHIPSET_FARADAY,    "Faraday FE3600",          "A", 88, 0, NC_RANGE,    3, 128 },
    { CHIPSET_UMC491,     "UMC UM82C491",            "A", 85, 1, NC_RANGE,    1,   8 },
    { CHIPSET_MIC9391,    "MIC MIC9391",             "A", 83, 1, NC_RANGE,    1,  64 },
    { CHIPSET_ETEQ_BENGAL,"Eteq 82C495WB (Bengal)",  "A", 80, 1, NC_RANGE,    2,   8 },
    { CHIPSET_OPTI381,    "OPTi 82C381 (Symphony)",  "B", 78, 0, NC_RANGE,    2, 512 },
    { CHIPSET_VIA,        "VIA VT82C310 (FlexSet)",  "B", 75, 0, NC_RANGE,    2,  64 },
    { CHIPSET_VLSI,       "VLSI VL82C311 (SCAMP)",   "B", 70, 0, NC_RANGE,    1,  64 },
    { CHIPSET_CT_PEAK,    "C&T 82C301 (PEAK/386)",   "C", 60, 0, NC_BOUNDARY, 1,  64 },
    { CHIPSET_CT_SCAT,    "C&T 82C235 (SCAT)",       "C", 55, 0, NC_BOUNDARY, 1,  64 },
    { CHIPSET_ALI_FINIS,  "ALi M1209 (FINIS)",       "C", 55, 0, NC_BOUNDARY, 1,  64 },
    { CHIPSET_FOREX,      "Forex FRX-386DX",         "D", 45, 0, NC_BOUNDARY, 1,1024 },
    { CHIPSET_SUNTAC,     "Suntac ST62C301",         "D", 45, 0, NC_SEGMENTS, 1,  64 },
    { CHIPSET_EISA_82350, "Intel 82350 (EISA)",      "B", 70, 0, NC_RANGE,    1,  64 },
    { CHIPSET_CT_NEAT,    "C&T 82C211 (NEAT)",       "D", 40, 0, NC_SHADOW,   0,   0 },
    { CHIPSET_HEADLAND,   "Headland HT12 (G2)",      "B", 72, 0, NC_STEERING, 4,  64 },
    { 0, NULL, NULL, 0, 0, 0, 0, 0 }
};

/*============================================================================
 * I/O ACCESS FUNCTIONS
 *============================================================================*/

/*
 * Safe indexed register read with I/O delay
 * Disables interrupts during the critical section
 */
static unsigned char safe_read(unsigned idx_port, unsigned data_port,
                               unsigned char reg)
{
    unsigned char val;

    _disable();
    outp(idx_port, reg);
    /* I/O delay via dummy port read */
    inp(0x80);
    val = inp(data_port);
    _enable();

    return val;
}

/*
 * Verify that a port pair contains a real indexed register set
 * Returns 0 if port appears to be floating bus (all 0xFF or 0x00)
 */
static int verify_port_valid(unsigned idx_port, unsigned data_port)
{
    unsigned char v1, v2, v3;

    v1 = safe_read(idx_port, data_port, 0x00);
    v2 = safe_read(idx_port, data_port, 0x10);
    v3 = safe_read(idx_port, data_port, 0x20);

    /* If all reads return 0xFF or 0x00, it's probably empty bus */
    if ((v1 == 0xFF && v2 == 0xFF && v3 == 0xFF) ||
        (v1 == 0x00 && v2 == 0x00 && v3 == 0x00)) {
        return 0;  /* Not a valid chipset */
    }
    return 1;  /* Port pair is valid */
}

/*
 * Check for EISA signature at F000:FFD9
 * Memory-based detection - 100% safe, no port I/O
 */
static int check_eisa(void)
{
    char far *eisa_sig = (char far *)0xF000FFD9L;

    if (eisa_sig[0] == 'E' && eisa_sig[1] == 'I' &&
        eisa_sig[2] == 'S' && eisa_sig[3] == 'A') {
        return 1;
    }
    return 0;
}

/*============================================================================
 * CACHE SIZE DETECTION
 *============================================================================*/

/*
 * Detect cache size for known chipsets
 * Returns size in KB, or 0 if unknown
 */
static unsigned int detect_cache_size(chipset_info_t *info)
{
    unsigned char reg;

    switch (info->type) {
        case CHIPSET_OPTI391:
        case CHIPSET_OPTI381:
        case CHIPSET_ETEQ_BENGAL:
            /* OPTi: Index 13h bits 5:4 = cache size */
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
            /* SiS: Index 11h bits 5:4 = cache size */
            reg = safe_read(0x22, 0x23, 0x11);
            switch ((reg >> 4) & 0x03) {
                case 0: return 64;
                case 1: return 128;
                case 2: return 256;
                case 3: return 512;
            }
            break;

        case CHIPSET_HEADLAND:
            /* Headland: Index 18h bits 1:0 */
            reg = safe_read(0x22, 0x23, 0x18);
            switch (reg & 0x03) {
                case 0: return 64;
                case 1: return 128;
                case 2: return 256;
                case 3: return 512;
            }
            break;

        case CHIPSET_UMC491:
            /* UMC: Fixed at 256KB typically */
            return 256;

        default:
            /* Assume 256KB for unknown */
            return 256;
    }

    return 256;  /* Default */
}

/*============================================================================
 * MAIN DETECTION ALGORITHM
 *============================================================================*/

/*
 * Safe chipset detection following industry-standard probing order:
 * 1. EISA signature (memory-based, 100% safe)
 * 2. Port 24h probe (OPTi/Eteq family - ~40% of market)
 * 3. Port 23h probe (SiS/C&T/UMC/VLSI/ALi/etc.)
 */
static chipset_info_t detect_chipset(void)
{
    chipset_info_t info;
    unsigned char id, id2;
    int i;

    memset(&info, 0, sizeof(info));
    info.index_port = 0x22;
    info.data_port = 0x23;
    info.name = "Unknown";
    info.tier = "?";

    /*-----------------------------------------------------------------------
     * Step 1: EISA Check (100% Safe - memory read only)
     *-----------------------------------------------------------------------*/
    if (check_eisa()) {
        info.type = CHIPSET_EISA_82350;
        info.index_port = 0x0C80;
        info.data_port = 0x0C85;
        goto found;
    }

    /*-----------------------------------------------------------------------
     * Step 2: OPTi Family (Port 24h) - ~40% of market
     *-----------------------------------------------------------------------*/
    if (verify_port_valid(0x22, 0x24)) {
        id = safe_read(0x22, 0x24, 0x20);
        info.id_value = id;
        info.id_index = 0x20;
        info.data_port = 0x24;

        switch (id & 0xE0) {  /* Mask bits 7:5 */
            case 0x40:  /* 010b = OPTi 82C391 (Write-Back!) */
                info.type = CHIPSET_OPTI391;
                goto found;

            case 0x20:  /* 001b = OPTi 82C381 (Write-Through) */
                info.type = CHIPSET_OPTI381;
                goto found;

            case 0x80:  /* 100b = OPTi 82C495 / Eteq Bengal (WB) */
                info.type = CHIPSET_ETEQ_BENGAL;
                goto found;
        }
    }

    /*-----------------------------------------------------------------------
     * Step 3: Standard Port 23h Chipsets
     *-----------------------------------------------------------------------*/
    info.data_port = 0x23;

    if (!verify_port_valid(0x22, 0x23)) {
        info.type = CHIPSET_UNKNOWN;
        return info;
    }

    /* Check Index 00h */
    id = safe_read(0x22, 0x23, 0x00);
    info.id_value = id;
    info.id_index = 0x00;

    switch (id) {
        case 0x31:  /* SiS 310 Rabbit OR VLSI - need to distinguish */
            /* Read Index 11h - if cache bits present, it's SiS */
            id2 = safe_read(0x22, 0x23, 0x11);
            if ((id2 & 0x80) || (id2 & 0x30)) {  /* Cache enable/size bits */
                info.type = CHIPSET_SIS_RABBIT;
            } else {
                info.type = CHIPSET_VLSI;
            }
            goto found;

        case 0x40:  /* SiS 460 (486) - early revision */
        case 0x41:  /* SiS 460 (486) - standard */
            info.type = CHIPSET_SIS_460;
            goto found;

        case 0x11:  /* C&T SCAT */
            info.type = CHIPSET_CT_SCAT;
            goto found;

        case 0x20:  /* C&T PEAK 301 */
        case 0x21:  /* C&T PEAK 301A */
            info.type = CHIPSET_CT_PEAK;
            goto found;

        case 0x12:  /* ALi M1209 FINIS */
            info.type = CHIPSET_ALI_FINIS;
            goto found;

        case 0x05:  /* Faraday FE3600 */
            info.type = CHIPSET_FARADAY;
            goto found;

        case 0x93:  /* MIC MIC9391 */
            info.type = CHIPSET_MIC9391;
            goto found;
    }

    /* Try UMC detection (XOR with 0xAD) */
    id = safe_read(0x22, 0x23, 0x10);
    if ((id ^ 0xAD) == 0x00) {
        info.type = CHIPSET_UMC491;
        info.id_value = id;
        info.id_index = 0x10;
        goto found;
    }

    /* Try Forex (Index 0Bh = 0x46) */
    id = safe_read(0x22, 0x23, 0x0B);
    if (id == 0x46) {
        info.type = CHIPSET_FOREX;
        info.id_value = id;
        info.id_index = 0x0B;
        goto found;
    }

    /* Try Headland (Index 17h, bits 7:4 = 0001b) */
    id = safe_read(0x22, 0x23, 0x17);
    if ((id >> 4) == 0x01) {
        info.type = CHIPSET_HEADLAND;
        info.id_value = id;
        info.id_index = 0x17;
        goto found;
    }

    /* Try VIA (Index 00h = 0x01 or 0x02) */
    id = safe_read(0x22, 0x23, 0x00);
    if (id == 0x01 || id == 0x02) {
        info.type = CHIPSET_VIA;
        info.id_value = id;
        info.id_index = 0x00;
        goto found;
    }

    /* Try C&T NEAT (Index 00h specific patterns) */
    /* NEAT typically returns specific values */
    if (id >= 0x80 && id <= 0x8F) {
        info.type = CHIPSET_CT_NEAT;
        info.id_value = id;
        info.id_index = 0x00;
        goto found;
    }

    /* Try Suntac (check for specific ID pattern) */
    id = safe_read(0x22, 0x23, 0x00);
    if (id == 0x62 || id == 0x63) {
        info.type = CHIPSET_SUNTAC;
        info.id_value = id;
        info.id_index = 0x00;
        goto found;
    }

    /* Unknown chipset */
    info.type = CHIPSET_UNKNOWN;
    return info;

found:
    /* Look up chipset in database */
    for (i = 0; chipset_db[i].name != NULL; i++) {
        if (chipset_db[i].type == info.type) {
            info.name = chipset_db[i].name;
            info.tier = chipset_db[i].tier;
            info.score_x10 = chipset_db[i].score_x10;
            info.is_writeback = chipset_db[i].is_wb;
            info.nc_strategy = chipset_db[i].nc_strategy;
            info.nc_regions = chipset_db[i].nc_regions;
            info.granularity = chipset_db[i].granularity;
            break;
        }
    }

    /* Detect cache size */
    info.cache_size_kb = detect_cache_size(&info);

    return info;
}

/*============================================================================
 * VERBOSE REGISTER DUMP
 *============================================================================*/

static void dump_registers(chipset_info_t *info, int raw_hex)
{
    int i;
    unsigned char val;

    printf("\nRegister Dump (Port %02Xh/%02Xh):\n",
           info->index_port, info->data_port);

    if (raw_hex) {
        printf("     ");
        for (i = 0; i < 16; i++) {
            printf(" %02X", i);
        }
        printf("\n");
    }

    for (i = 0; i < 64; i++) {
        if ((i % 16) == 0) {
            if (raw_hex) {
                printf("%02X:  ", i);
            } else {
                printf("\nIndex %02Xh-%02Xh: ", i, i + 15);
            }
        }

        val = safe_read(info->index_port, info->data_port, i);

        if (raw_hex) {
            printf(" %02X", val);
            if ((i % 16) == 15) printf("\n");
        } else {
            printf("%02X ", val);
        }
    }

    if (!raw_hex) printf("\n");
}

/*============================================================================
 * NC STRATEGY DESCRIPTION
 *============================================================================*/

static const char *nc_strategy_name(unsigned char strategy)
{
    switch (strategy) {
        case NC_RANGE:    return "Range-based (programmable)";
        case NC_BOUNDARY: return "Boundary (top-down)";
        case NC_STEERING: return "Steering (disable DRAM)";
        case NC_SHADOW:   return "Shadow RAM only (640K-1M)";
        case NC_SEGMENTS: return "Segment-based";
        default:          return "None/Unknown";
    }
}

/*============================================================================
 * MAIN PROGRAM
 *============================================================================*/

static void print_usage(void)
{
    printf("CHIPSET v1.0 - 386/486 Chipset Detection Utility\n");
    printf("Part of the Abacus FPGA Project\n\n");
    printf("Usage: CHIPSET [/V] [/RAW]\n\n");
    printf("  /V    Verbose output (show probed registers)\n");
    printf("  /RAW  Raw hex dump of chipset registers\n");
    printf("  /?    This help message\n");
}

int main(int argc, char *argv[])
{
    chipset_info_t info;
    int verbose = 0;
    int raw_hex = 0;
    int i;

    /* Parse command line */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '/' || argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'v':
                case 'V':
                    verbose = 1;
                    break;
                case 'r':
                case 'R':
                    raw_hex = 1;
                    verbose = 1;
                    break;
                case '?':
                case 'h':
                case 'H':
                    print_usage();
                    return 0;
                default:
                    printf("Unknown option: %s\n", argv[i]);
                    print_usage();
                    return 1;
            }
        }
    }

    printf("CHIPSET v1.0 - 386/486 Chipset Detection Utility\n");
    printf("================================================\n\n");

    /* Detect chipset */
    info = detect_chipset();

    if (info.type == CHIPSET_UNKNOWN) {
        printf("Detected: UNKNOWN CHIPSET\n");
        printf("  Ports:    22h/23h and 22h/24h probed\n");
        printf("  Status:   No recognized chipset signature found\n\n");
        printf("This system may use:\n");
        printf("  - A chipset not in our database\n");
        printf("  - A proprietary/custom chipset\n");
        printf("  - No external cache controller\n");

        if (verbose) {
            info.index_port = 0x22;
            info.data_port = 0x23;
            dump_registers(&info, raw_hex);
        }

        return 1;
    }

    /* Display results */
    printf("Detected: %s\n", info.name);
    printf("  Ports:    %02Xh/%02Xh (Index/Data)\n",
           info.index_port, info.data_port);
    printf("  Cache:    %s (%uKB detected)\n",
           info.is_writeback ? "Write-Back" : "Write-Through",
           info.cache_size_kb);
    printf("  NC:       %s", nc_strategy_name(info.nc_strategy));
    if (info.nc_regions > 0) {
        printf(" (%u region%s, %uKB granularity)",
               info.nc_regions,
               info.nc_regions > 1 ? "s" : "",
               info.granularity);
    }
    printf("\n");
    printf("  Score:    %u.%u/10 (%s-Tier)\n",
           info.score_x10 / 10, info.score_x10 % 10, info.tier);
    printf("\n");
    printf("Chipset ID: 0x%02X at Index %02Xh\n",
           info.id_value, info.id_index);

    /* Write-Back warning */
    if (info.is_writeback) {
        printf("\n");
        printf("*** WRITE-BACK CACHE ***\n");
        printf("Flush order: FLUSH first, then DISABLE\n");
        printf("Failure to flush before disable = DATA CORRUPTION!\n");
    }

    /* Boundary NC warning */
    if (info.nc_strategy == NC_BOUNDARY) {
        printf("\n");
        printf("*** BOUNDARY NC LIMITATION ***\n");
        printf("Cannot create NC \"holes\" - buffer must be at TOP of RAM!\n");
    }

    /* Verbose output */
    if (verbose) {
        dump_registers(&info, raw_hex);
    }

    return 0;
}
