/*
 * NCCONFIG.C - Non-Cacheable Region Configuration Utility
 *
 * Part of the Abacus FPGA Project
 * For Open Watcom C (16-bit real mode DOS)
 *
 * Configures NC regions for bounce buffer allocation on 386/486 chipsets.
 * Supports Range, Boundary, and Steering NC strategies.
 *
 * Last Updated: 2026-01-04 15:50:00 EST
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <i86.h>
#include "CK_PARSE.H"   /* parse_uint_range / parse_mb_to_kb */

/*============================================================================
 * CHIPSET TYPE DEFINITIONS (same as CHIPSET.C)
 *============================================================================*/

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
#define CHIPSET_UNKNOWN      0

#define NC_NONE       0
#define NC_RANGE      1
#define NC_BOUNDARY   2
#define NC_STEERING   3
#define NC_SHADOW     4
#define NC_SEGMENTS   5

/*============================================================================
 * CHIPSET INFO STRUCTURE
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
    unsigned int  granularity;
} chipset_info_t;

/*============================================================================
 * I/O ACCESS FUNCTIONS
 *============================================================================*/

static unsigned char safe_read(unsigned idx_port, unsigned data_port,
                               unsigned char reg)
{
    unsigned char val;
    _disable();
    outp(idx_port, reg);
    inp(0x80);
    val = inp(data_port);
    _enable();
    return val;
}

static void safe_write(unsigned idx_port, unsigned data_port,
                       unsigned char reg, unsigned char val)
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

static int check_eisa(void)
{
    char far *eisa_sig = (char far *)0xF000FFD9L;
    return (eisa_sig[0] == 'E' && eisa_sig[1] == 'I' &&
            eisa_sig[2] == 'S' && eisa_sig[3] == 'A');
}

/*============================================================================
 * CHIPSET DETECTION (simplified from CHIPSET.C)
 *============================================================================*/

static chipset_info_t detect_chipset(void)
{
    chipset_info_t info;
    unsigned char id, id2;

    memset(&info, 0, sizeof(info));
    info.index_port = 0x22;
    info.data_port = 0x23;
    info.name = "Unknown";
    info.granularity = 64;

    if (check_eisa()) {
        info.type = CHIPSET_EISA_82350;
        info.index_port = 0x0C80;
        info.data_port = 0x0C85;
        info.name = "Intel 82350 (EISA)";
        info.nc_strategy = NC_RANGE;
        info.nc_regions = 1;
        return info;
    }

    if (verify_port_valid(0x22, 0x24)) {
        id = safe_read(0x22, 0x24, 0x20);
        info.data_port = 0x24;

        switch (id & 0xE0) {
            case 0x40:
                info.type = CHIPSET_OPTI391;
                info.name = "OPTi 82C391 (SYSC)";
                info.is_writeback = 1;
                info.nc_strategy = NC_RANGE;
                info.nc_regions = 2;
                info.granularity = 8;
                return info;
            case 0x20:
                info.type = CHIPSET_OPTI381;
                info.name = "OPTi 82C381 (Symphony)";
                info.nc_strategy = NC_RANGE;
                info.nc_regions = 2;
                info.granularity = 512;
                return info;
            case 0x80:
                info.type = CHIPSET_ETEQ_BENGAL;
                info.name = "Eteq 82C495WB (Bengal)";
                info.is_writeback = 1;
                info.nc_strategy = NC_RANGE;
                info.nc_regions = 2;
                info.granularity = 8;
                return info;
        }
    }

    info.data_port = 0x23;
    if (!verify_port_valid(0x22, 0x23)) {
        info.type = CHIPSET_UNKNOWN;
        return info;
    }

    id = safe_read(0x22, 0x23, 0x00);

    switch (id) {
        case 0x31:
            id2 = safe_read(0x22, 0x23, 0x11);
            if ((id2 & 0x80) || (id2 & 0x30)) {
                info.type = CHIPSET_SIS_RABBIT;
                info.name = "SiS 85C310 (Rabbit)";
                info.is_writeback = 1;
            } else {
                info.type = CHIPSET_VLSI;
                info.name = "VLSI VL82C311 (SCAMP)";
            }
            info.nc_strategy = NC_RANGE;
            info.nc_regions = (info.type == CHIPSET_SIS_RABBIT) ? 4 : 1;
            return info;

        case 0x40:
        case 0x41:
            info.type = CHIPSET_SIS_460;
            info.name = "SiS 85C460 (486)";
            info.is_writeback = 1;
            info.nc_strategy = NC_RANGE;
            info.nc_regions = 4;
            return info;

        case 0x11:
            info.type = CHIPSET_CT_SCAT;
            info.name = "C&T 82C235 (SCAT)";
            info.nc_strategy = NC_BOUNDARY;
            info.nc_regions = 1;
            return info;

        case 0x20:
        case 0x21:
            info.type = CHIPSET_CT_PEAK;
            info.name = "C&T 82C301 (PEAK/386)";
            info.nc_strategy = NC_BOUNDARY;
            info.nc_regions = 1;
            return info;

        case 0x12:
            info.type = CHIPSET_ALI_FINIS;
            info.name = "ALi M1209 (FINIS)";
            info.nc_strategy = NC_BOUNDARY;
            info.nc_regions = 1;
            return info;

        case 0x05:
            info.type = CHIPSET_FARADAY;
            info.name = "Faraday FE3600";
            info.nc_strategy = NC_RANGE;
            info.nc_regions = 3;
            info.granularity = 128;
            return info;

        case 0x93:
            info.type = CHIPSET_MIC9391;
            info.name = "MIC MIC9391";
            info.is_writeback = 1;
            info.nc_strategy = NC_RANGE;
            info.nc_regions = 1;
            return info;
    }

    /* Additional detection attempts */
    id = safe_read(0x22, 0x23, 0x10);
    if ((id ^ 0xAD) == 0x00) {
        info.type = CHIPSET_UMC491;
        info.name = "UMC UM82C491";
        info.is_writeback = 1;
        info.nc_strategy = NC_RANGE;
        info.nc_regions = 1;
        info.granularity = 8;
        return info;
    }

    id = safe_read(0x22, 0x23, 0x0B);
    if (id == 0x46) {
        info.type = CHIPSET_FOREX;
        info.name = "Forex FRX-386DX";
        info.nc_strategy = NC_BOUNDARY;
        info.nc_regions = 1;
        info.granularity = 1024;
        return info;
    }

    id = safe_read(0x22, 0x23, 0x17);
    if ((id >> 4) == 0x01) {
        info.type = CHIPSET_HEADLAND;
        info.name = "Headland HT12 (G2)";
        info.nc_strategy = NC_STEERING;
        info.nc_regions = 4;
        return info;
    }

    info.type = CHIPSET_UNKNOWN;
    return info;
}

/*============================================================================
 * NC REGION SIZE CODES
 *============================================================================*/

/* SiS Rabbit/460 NC Size Codes (Index 14h-17h, bits 3:0) */
static const struct { unsigned int kb; unsigned char code; } sis_sizes[] = {
    {   64, 0x01 },
    {  128, 0x02 },
    {  256, 0x03 },
    {  512, 0x04 },
    { 1024, 0x05 },
    { 2048, 0x06 },
    { 4096, 0x07 },
    {    0, 0x00 }  /* Disabled */
};

/* OPTi 391/381 NC Size Codes */
static const struct { unsigned int kb; unsigned char code; } opti_sizes[] = {
    {    8, 0x00 },
    {   16, 0x01 },
    {   32, 0x02 },
    {   64, 0x03 },
    {  128, 0x04 },
    {  256, 0x05 },
    {  512, 0x06 },
    { 1024, 0x07 },
    {    0, 0xFF }  /* End marker */
};

/*============================================================================
 * NC CONFIGURATION FUNCTIONS
 *============================================================================*/

/*
 * Configure NC region for SiS Rabbit/460
 * Registers: Index 14h-17h (4 regions)
 * Format: Bits 7:4 = Base (MB), Bits 3:0 = Size code
 */
static int config_nc_sis(chipset_info_t *info, int region,
                         unsigned int base_mb, unsigned int size_kb, int dry_run)
{
    unsigned char reg, val, size_code;
    int i;

    if (region < 0 || region >= 4) {
        printf("Error: SiS supports regions 0-3\n");
        return -1;
    }

    /* Find size code */
    size_code = 0;
    for (i = 0; sis_sizes[i].kb != 0; i++) {
        if (sis_sizes[i].kb == size_kb) {
            size_code = sis_sizes[i].code;
            break;
        }
    }
    if (size_code == 0 && size_kb != 0) {
        printf("Error: Invalid size %uKB. Valid: 64, 128, 256, 512, 1024, 2048, 4096\n",
               size_kb);
        return -1;
    }

    if (base_mb > 15) {
        printf("Error: Base address must be 0-15 MB\n");
        return -1;
    }

    reg = 0x14 + region;
    val = ((unsigned char)base_mb << 4) | size_code;

    printf("  Index %02Xh <- 0x%02X (Base=%uMB, Size=%uKB)\n",
           reg, val, base_mb, size_kb);

    if (!dry_run) {
        safe_write(info->index_port, info->data_port, reg, val);
    }

    return 0;
}

/*
 * Configure NC region for OPTi 391/381
 * Index 52h/53h (region 0), Index 54h/55h (region 1)
 * Format: Low reg = Base bits 15:8, High reg = Size + Base bits 23:16
 */
static int config_nc_opti(chipset_info_t *info, int region,
                          unsigned int base_mb, unsigned int size_kb, int dry_run)
{
    unsigned char reg_lo, reg_hi, val_lo, val_hi, size_code;
    unsigned long base_addr;
    int i;

    if (region < 0 || region >= 2) {
        printf("Error: OPTi supports regions 0-1\n");
        return -1;
    }

    /* Find size code */
    size_code = 0xFF;
    for (i = 0; opti_sizes[i].kb != 0; i++) {
        if (opti_sizes[i].kb == size_kb) {
            size_code = opti_sizes[i].code;
            break;
        }
    }
    if (size_code == 0xFF) {
        printf("Error: Invalid size %uKB. Valid: 8, 16, 32, 64, 128, 256, 512, 1024\n",
               size_kb);
        return -1;
    }

    reg_lo = (region == 0) ? 0x52 : 0x54;
    reg_hi = (region == 0) ? 0x53 : 0x55;

    base_addr = (unsigned long)base_mb * 1024UL;  /* KB */
    val_lo = (unsigned char)(base_addr >> 8);     /* Bits 15:8 */
    val_hi = (size_code << 4) | (unsigned char)(base_addr >> 16);  /* Size + bits 23:16 */

    printf("  Index %02Xh <- 0x%02X (Base low)\n", reg_lo, val_lo);
    printf("  Index %02Xh <- 0x%02X (Size=%uKB, Base high)\n", reg_hi, val_hi, size_kb);

    if (!dry_run) {
        safe_write(info->index_port, info->data_port, reg_lo, val_lo);
        safe_write(info->index_port, info->data_port, reg_hi, val_hi);
    }

    return 0;
}

/*
 * Configure NC boundary for C&T SCAT/PEAK, ALi FINIS
 * Index 1Ch (SCAT), Index 1Ah (PEAK)
 * Format: Boundary in 64KB units (everything above = NC)
 */
static int config_nc_boundary(chipset_info_t *info, unsigned long boundary_kb,
                              int dry_run)
{
    unsigned char reg, val;
    unsigned long units;

    /* boundary_kb is unsigned long: the caller computes base_mb * 1024 in
     * unsigned long, so a base >= 64 MB no longer wraps a 16-bit int to a
     * near-zero boundary (which would have marked nearly all DRAM NC). */
    if ((boundary_kb % 64) != 0) {
        printf("Error: Boundary must be 64KB aligned\n");
        return -1;
    }

    units = boundary_kb / 64;       /* default: 64KB units */

    switch (info->type) {
        case CHIPSET_CT_SCAT:
            reg = 0x1C;
            break;
        case CHIPSET_CT_PEAK:
            reg = 0x1A;
            break;
        case CHIPSET_ALI_FINIS:
            reg = 0x1A;
            break;
        case CHIPSET_FOREX:
            /* Forex uses 1MB boundary units */
            if ((boundary_kb % 1024) != 0) {
                printf("Error: Forex boundary must be 1MB aligned\n");
                return -1;
            }
            units = boundary_kb / 1024;
            reg = 0x15;
            break;
        default:
            printf("Error: Chipset does not support boundary NC\n");
            return -1;
    }

    /* The boundary register is a single byte. */
    if (units > 255UL) {
        printf("Error: Boundary %luKB exceeds this chipset's range\n",
               boundary_kb);
        return -1;
    }
    val = (unsigned char)units;

    printf("  Index %02Xh <- 0x%02X (Boundary=%luKB, everything above = NC)\n",
           reg, val, boundary_kb);

    if (!dry_run) {
        safe_write(info->index_port, info->data_port, reg, val);
    }

    return 0;
}

/*
 * Configure NC for UMC 491
 * Index 56h = Base (8KB units), Index 57h = Size (8KB units)
 */
static int config_nc_umc(chipset_info_t *info, unsigned int base_mb,
                         unsigned int size_kb, int dry_run)
{
    unsigned char base_val, size_val;
    unsigned long base_kb;

    if ((size_kb % 8) != 0) {
        printf("Error: UMC size must be 8KB aligned\n");
        return -1;
    }

    base_kb = (unsigned long)base_mb * 1024UL;
    if ((base_kb % 8) != 0) {
        printf("Error: UMC base must be 8KB aligned\n");
        return -1;
    }

    base_val = (unsigned char)(base_kb / 8);
    size_val = (unsigned char)(size_kb / 8);

    printf("  Index 56h <- 0x%02X (Base=%uKB)\n", base_val, (unsigned)base_kb);
    printf("  Index 57h <- 0x%02X (Size=%uKB)\n", size_val, size_kb);

    if (!dry_run) {
        safe_write(info->index_port, info->data_port, 0x56, base_val);
        safe_write(info->index_port, info->data_port, 0x57, size_val);
    }

    return 0;
}

/*
 * Configure NC for VLSI SCAMP
 * Index 0Dh = Base, Index 0Eh = Size
 */
static int config_nc_vlsi(chipset_info_t *info, unsigned int base_mb,
                          unsigned int size_kb, int dry_run)
{
    unsigned char base_val, size_val;

    /* VLSI uses 64KB units */
    if ((size_kb % 64) != 0) {
        printf("Error: VLSI size must be 64KB aligned\n");
        return -1;
    }

    base_val = (unsigned char)(base_mb * 16);  /* 64KB units */
    size_val = (unsigned char)(size_kb / 64);

    printf("  Index 0Dh <- 0x%02X (Base=%uMB)\n", base_val, base_mb);
    printf("  Index 0Eh <- 0x%02X (Size=%uKB)\n", size_val, size_kb);

    if (!dry_run) {
        safe_write(info->index_port, info->data_port, 0x0D, base_val);
        safe_write(info->index_port, info->data_port, 0x0E, size_val);
    }

    return 0;
}

/*============================================================================
 * READ CURRENT NC CONFIGURATION
 *============================================================================*/

static void show_nc_config(chipset_info_t *info)
{
    unsigned char val;
    int i;

    printf("\nCurrent NC Configuration:\n");

    switch (info->type) {
        case CHIPSET_SIS_RABBIT:
        case CHIPSET_SIS_460:
            for (i = 0; i < 4; i++) {
                val = safe_read(info->index_port, info->data_port, 0x14 + i);
                if ((val & 0x0F) == 0) {
                    printf("  Region %d: <disabled>\n", i);
                } else {
                    unsigned int base = (val >> 4) & 0x0F;
                    unsigned int size = 64 << ((val & 0x0F) - 1);
                    printf("  Region %d: %uMB - %uMB (%uKB) [Index %02Xh = 0x%02X]\n",
                           i, base, base + (size / 1024), size, 0x14 + i, val);
                }
            }
            break;

        case CHIPSET_OPTI391:
        case CHIPSET_OPTI381:
        case CHIPSET_ETEQ_BENGAL:
            for (i = 0; i < 2; i++) {
                unsigned char lo = safe_read(info->index_port, info->data_port,
                                             (i == 0) ? 0x52 : 0x54);
                unsigned char hi = safe_read(info->index_port, info->data_port,
                                             (i == 0) ? 0x53 : 0x55);
                unsigned int size_code = (hi >> 4) & 0x07;
                if (size_code == 0x07 && (hi & 0x80)) {
                    printf("  Region %d: <disabled>\n", i);
                } else {
                    unsigned long base = ((unsigned long)(hi & 0x0F) << 16) |
                                         ((unsigned long)lo << 8);
                    unsigned int size = 8 << size_code;
                    printf("  Region %d: %luKB (%uKB) [Index %02Xh/%02Xh]\n",
                           i, base, size, (i == 0) ? 0x52 : 0x54, (i == 0) ? 0x53 : 0x55);
                }
            }
            break;

        case CHIPSET_CT_SCAT:
            val = safe_read(info->index_port, info->data_port, 0x1C);
            printf("  Boundary: %uKB (everything above = NC) [Index 1Ch = 0x%02X]\n",
                   (unsigned)val * 64, val);
            break;

        case CHIPSET_CT_PEAK:
        case CHIPSET_ALI_FINIS:
            val = safe_read(info->index_port, info->data_port, 0x1A);
            printf("  Boundary: %uKB (everything above = NC) [Index 1Ah = 0x%02X]\n",
                   (unsigned)val * 64, val);
            break;

        case CHIPSET_UMC491:
            {
                unsigned char base = safe_read(info->index_port, info->data_port, 0x56);
                unsigned char size = safe_read(info->index_port, info->data_port, 0x57);
                printf("  Region 0: %uKB (%uKB) [Index 56h/57h]\n",
                       (unsigned)base * 8, (unsigned)size * 8);
            }
            break;

        case CHIPSET_VLSI:
            {
                unsigned char base = safe_read(info->index_port, info->data_port, 0x0D);
                unsigned char size = safe_read(info->index_port, info->data_port, 0x0E);
                printf("  Region 0: %uKB (%uKB) [Index 0Dh/0Eh]\n",
                       (unsigned)base * 4, (unsigned)size * 64);
            }
            break;

        case CHIPSET_HEADLAND:
            printf("  Steering-based: Check DRAM bank configuration\n");
            for (i = 0; i < 4; i++) {
                val = safe_read(info->index_port, info->data_port, 0x40 + i);
                printf("  Bank %d: %s [Index %02Xh = 0x%02X]\n",
                       i, (val & 0x80) ? "Enabled" : "Disabled (NC)", 0x40 + i, val);
            }
            break;

        default:
            printf("  NC configuration not supported for this chipset\n");
            break;
    }
}

/*
 * Clear NC region(s)
 */
static int clear_nc_region(chipset_info_t *info, int region, int dry_run)
{
    int i;

    printf("Clearing NC region(s)...\n");

    switch (info->type) {
        case CHIPSET_SIS_RABBIT:
        case CHIPSET_SIS_460:
            if (region < 0) {
                /* Clear all */
                for (i = 0; i < 4; i++) {
                    printf("  Index %02Xh <- 0x00\n", 0x14 + i);
                    if (!dry_run) {
                        safe_write(info->index_port, info->data_port, 0x14 + i, 0x00);
                    }
                }
            } else if (region < 4) {
                printf("  Index %02Xh <- 0x00\n", 0x14 + region);
                if (!dry_run) {
                    safe_write(info->index_port, info->data_port, 0x14 + region, 0x00);
                }
            }
            break;

        case CHIPSET_OPTI391:
        case CHIPSET_OPTI381:
        case CHIPSET_ETEQ_BENGAL:
            if (region < 0) {
                for (i = 0; i < 2; i++) {
                    unsigned char lo_reg = (i == 0) ? 0x52 : 0x54;
                    unsigned char hi_reg = (i == 0) ? 0x53 : 0x55;
                    printf("  Index %02Xh <- 0x00, Index %02Xh <- 0xF0 (disabled)\n",
                           lo_reg, hi_reg);
                    if (!dry_run) {
                        safe_write(info->index_port, info->data_port, lo_reg, 0x00);
                        safe_write(info->index_port, info->data_port, hi_reg, 0xF0);
                    }
                }
            }
            break;

        case CHIPSET_UMC491:
            printf("  Index 56h <- 0x00, Index 57h <- 0x00\n");
            if (!dry_run) {
                safe_write(info->index_port, info->data_port, 0x56, 0x00);
                safe_write(info->index_port, info->data_port, 0x57, 0x00);
            }
            break;

        default:
            printf("  Clear not supported for this chipset\n");
            return -1;
    }

    return 0;
}

/*============================================================================
 * MAIN CONFIGURATION ENTRY POINT
 *============================================================================*/

static int configure_nc(chipset_info_t *info, int region,
                        unsigned int base_mb, unsigned int size_kb, int dry_run)
{
    printf("%s NC Region...\n", dry_run ? "Testing" : "Setting");

    switch (info->type) {
        case CHIPSET_SIS_RABBIT:
        case CHIPSET_SIS_460:
            return config_nc_sis(info, region, base_mb, size_kb, dry_run);

        case CHIPSET_OPTI391:
        case CHIPSET_OPTI381:
        case CHIPSET_ETEQ_BENGAL:
            return config_nc_opti(info, region, base_mb, size_kb, dry_run);

        case CHIPSET_CT_SCAT:
        case CHIPSET_CT_PEAK:
        case CHIPSET_ALI_FINIS:
        case CHIPSET_FOREX:
            /* For boundary chipsets, base_mb * 1024 = boundary in KB.
               Compute in unsigned long so a base >= 64 MB cannot wrap. */
            return config_nc_boundary(info, (unsigned long)base_mb * 1024UL,
                                      dry_run);

        case CHIPSET_UMC491:
            return config_nc_umc(info, base_mb, size_kb, dry_run);

        case CHIPSET_VLSI:
            return config_nc_vlsi(info, base_mb, size_kb, dry_run);

        case CHIPSET_HEADLAND:
            printf("Error: Headland uses steering - disable DRAM banks manually\n");
            return -1;

        default:
            printf("Error: NC configuration not supported for this chipset\n");
            return -1;
    }
}

/*============================================================================
 * MAIN PROGRAM
 *============================================================================*/

static void print_usage(void)
{
    printf("NCCONFIG v1.0 - Non-Cacheable Region Configuration\n");
    printf("Part of the Abacus FPGA Project\n\n");
    printf("Usage:\n");
    printf("  NCCONFIG /S                        Show current NC config\n");
    printf("  NCCONFIG /SET <base_mb> <size_kb>  Set NC region\n");
    printf("  NCCONFIG /SET:<n> <base> <size>    Set specific region n\n");
    printf("  NCCONFIG /CLEAR [region]           Clear NC region(s)\n");
    printf("  NCCONFIG /TEST <base_mb> <size_kb> Dry-run (no writes)\n");
    printf("  NCCONFIG /?                        This help\n\n");
    printf("  base_mb   Base address in MB (e.g., 14 = 14MB)\n");
    printf("  size_kb   Size in KB (must match chipset granularity)\n");
}

static const char *nc_strategy_name(unsigned char strategy)
{
    switch (strategy) {
        case NC_RANGE:    return "Range-based";
        case NC_BOUNDARY: return "Boundary";
        case NC_STEERING: return "Steering";
        case NC_SHADOW:   return "Shadow RAM only";
        case NC_SEGMENTS: return "Segment-based";
        default:          return "None/Unknown";
    }
}

int main(int argc, char *argv[])
{
    chipset_info_t info;
    int show_config = 0;
    int set_config = 0;
    int test_mode = 0;
    int clear_mode = 0;
    int region = 0;
    unsigned int base_mb = 0;
    unsigned int size_kb = 0;
    unsigned long pv;       /* scratch for validated parsing */
    int i;

    /* Parse command line. All numeric arguments are validated (parse_uint_range
       rejects non-numeric/out-of-range input and reports an error) instead of
       the old atoi(), which silently turned bad input into 0. Bad args exit 2. */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '/' && argv[i][0] != '-') {
            fprintf(stderr, "Error: unexpected argument '%s'\n", argv[i]);
            return 2;
        }

        if (stricmp(&argv[i][1], "S") == 0) {
            show_config = 1;
        } else if (strnicmp(&argv[i][1], "SET", 3) == 0) {
            set_config = 1;
            /* Optional :n region specifier (0..3) */
            if (argv[i][4] == ':') {
                if (!parse_uint_range(&argv[i][5], 0UL, 3UL, &pv)) {
                    fprintf(stderr, "Error: invalid region in '%s' (use :0..:3)\n",
                            argv[i]);
                    return 2;
                }
                region = (int)pv;
            }
            /* Require <base_mb> <size_kb> */
            if (i + 2 >= argc) {
                fprintf(stderr, "Error: /SET requires <base_mb> <size_kb>\n");
                return 2;
            }
            if (!parse_uint_range(argv[++i], 0UL, 4095UL, &pv)) {
                fprintf(stderr, "Error: invalid base_mb '%s' (0..4095)\n", argv[i]);
                return 2;
            }
            base_mb = (unsigned int)pv;
            if (!parse_uint_range(argv[++i], 0UL, 65535UL, &pv)) {
                fprintf(stderr, "Error: invalid size_kb '%s' (0..65535)\n", argv[i]);
                return 2;
            }
            size_kb = (unsigned int)pv;
        } else if (strnicmp(&argv[i][1], "TEST", 4) == 0) {
            test_mode = 1;
            set_config = 1;
            if (i + 2 >= argc) {
                fprintf(stderr, "Error: /TEST requires <base_mb> <size_kb>\n");
                return 2;
            }
            if (!parse_uint_range(argv[++i], 0UL, 4095UL, &pv)) {
                fprintf(stderr, "Error: invalid base_mb '%s' (0..4095)\n", argv[i]);
                return 2;
            }
            base_mb = (unsigned int)pv;
            if (!parse_uint_range(argv[++i], 0UL, 65535UL, &pv)) {
                fprintf(stderr, "Error: invalid size_kb '%s' (0..65535)\n", argv[i]);
                return 2;
            }
            size_kb = (unsigned int)pv;
        } else if (strnicmp(&argv[i][1], "CLEAR", 5) == 0) {
            clear_mode = 1;
            /* Optional region number; default = clear all */
            if (i + 1 < argc && argv[i+1][0] != '/' && argv[i+1][0] != '-') {
                if (!parse_uint_range(argv[++i], 0UL, 3UL, &pv)) {
                    fprintf(stderr, "Error: invalid region '%s' (0..3)\n", argv[i]);
                    return 2;
                }
                region = (int)pv;
            } else {
                region = -1;  /* Clear all */
            }
        } else if (argv[i][1] == '?' || argv[i][1] == 'h' || argv[i][1] == 'H') {
            print_usage();
            return 0;
        } else {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            return 2;
        }
    }

    if (!show_config && !set_config && !clear_mode) {
        show_config = 1;  /* Default to showing config */
    }

    printf("NCCONFIG v1.0 - Non-Cacheable Region Configuration\n");
    printf("===================================================\n\n");

    /* Detect chipset */
    info = detect_chipset();

    if (info.type == CHIPSET_UNKNOWN) {
        printf("Error: No recognized chipset detected\n");
        return 1;
    }

    printf("Chipset: %s\n", info.name);
    printf("Strategy: %s", nc_strategy_name(info.nc_strategy));
    if (info.nc_regions > 0) {
        printf(" (%u region%s available)", info.nc_regions,
               info.nc_regions > 1 ? "s" : "");
    }
    printf("\n");
    printf("Granularity: %uKB minimum\n", info.granularity);

    if (info.nc_strategy == NC_BOUNDARY) {
        printf("\n*** WARNING: Boundary-based chipset ***\n");
        printf("Cannot create NC \"holes\" - buffer must be at TOP of RAM!\n");
    }

    /* Execute requested operation */
    if (show_config) {
        show_nc_config(&info);
    }

    if (clear_mode) {
        printf("\n");
        clear_nc_region(&info, region, 0);
        printf("Done.\n");
    }

    if (set_config && base_mb > 0) {
        printf("\n");
        if (configure_nc(&info, region, base_mb, size_kb, test_mode) == 0) {
            printf("Done.\n");
        }
    }

    return 0;
}
