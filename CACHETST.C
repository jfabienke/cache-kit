/*
 * CACHETST.C - Cache Flush Verification Utility
 *
 * Part of the Abacus FPGA Project
 * For Open Watcom C (16-bit real mode DOS)
 *
 * Tests cache flush sequences and verifies Write-Back vs Write-Through
 * behavior. Detects cache coherency issues critical for DMA operations.
 *
 * Last Updated: 2026-01-04 15:55:00 EST
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <i86.h>
#include <dos.h>

/*============================================================================
 * CHIPSET TYPE DEFINITIONS
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

/*============================================================================
 * CHIPSET INFO STRUCTURE
 *============================================================================*/

typedef struct {
    unsigned char type;
    unsigned char is_writeback;
    unsigned int  index_port;
    unsigned int  data_port;
    unsigned int  cache_size_kb;
    const char   *name;
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
 * CPU DETECTION
 *============================================================================*/

/*
 * Detect if CPU is 486 or better (has WBINVD instruction)
 * Returns: 0 = 386 or earlier, 1 = 486+
 */
static int is_486_or_better(void)
{
    unsigned int cpu_type = 3;  /* Default to 386 */

    /*
     * Try to toggle the AC flag (bit 18) in EFLAGS
     * 386 doesn't have this flag, 486+ does
     */
    _asm {
        pushfd
        pop eax
        mov ecx, eax
        xor eax, 40000h      ; Toggle AC flag (bit 18)
        push eax
        popfd
        pushfd
        pop eax
        xor eax, ecx
        shr eax, 18
        and eax, 1
        mov cpu_type, ax
        push ecx
        popfd                 ; Restore original flags
    }

    return cpu_type;
}

/*============================================================================
 * CHIPSET DETECTION (simplified)
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

        case CHIPSET_HEADLAND:
            reg = safe_read(0x22, 0x23, 0x18);
            switch (reg & 0x03) {
                case 0: return 64;
                case 1: return 128;
                case 2: return 256;
                case 3: return 512;
            }
            break;
    }

    return 256;  /* Default */
}

static chipset_info_t detect_chipset(void)
{
    chipset_info_t info;
    unsigned char id, id2;

    memset(&info, 0, sizeof(info));
    info.index_port = 0x22;
    info.data_port = 0x23;
    info.name = "Unknown";
    info.cache_size_kb = 256;

    if (check_eisa()) {
        info.type = CHIPSET_EISA_82350;
        info.name = "Intel 82350 (EISA)";
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
                info.cache_size_kb = detect_cache_size(&info);
                return info;
            case 0x20:
                info.type = CHIPSET_OPTI381;
                info.name = "OPTi 82C381 (Symphony)";
                info.cache_size_kb = detect_cache_size(&info);
                return info;
            case 0x80:
                info.type = CHIPSET_ETEQ_BENGAL;
                info.name = "Eteq 82C495WB (Bengal)";
                info.is_writeback = 1;
                info.cache_size_kb = detect_cache_size(&info);
                return info;
        }
    }

    info.data_port = 0x23;
    if (!verify_port_valid(0x22, 0x23)) {
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
            info.cache_size_kb = detect_cache_size(&info);
            return info;

        case 0x40:
        case 0x41:
            info.type = CHIPSET_SIS_460;
            info.name = "SiS 85C460 (486)";
            info.is_writeback = 1;
            info.cache_size_kb = detect_cache_size(&info);
            return info;

        case 0x93:
            info.type = CHIPSET_MIC9391;
            info.name = "MIC MIC9391";
            info.is_writeback = 1;
            info.cache_size_kb = 256;
            return info;
    }

    /* Additional checks */
    id = safe_read(0x22, 0x23, 0x10);
    if ((id ^ 0xAD) == 0x00) {
        info.type = CHIPSET_UMC491;
        info.name = "UMC UM82C491";
        info.is_writeback = 1;
        info.cache_size_kb = 256;
        return info;
    }

    id = safe_read(0x22, 0x23, 0x17);
    if ((id >> 4) == 0x01) {
        info.type = CHIPSET_HEADLAND;
        info.name = "Headland HT12 (G2)";
        info.cache_size_kb = detect_cache_size(&info);
        return info;
    }

    return info;
}

/*============================================================================
 * CACHE CONTROL FUNCTIONS
 *============================================================================*/

/*
 * Disable external cache (L2)
 * Returns previous cache enable state
 */
static unsigned char disable_cache(chipset_info_t *info)
{
    unsigned char prev, val;

    switch (info->type) {
        case CHIPSET_OPTI391:
        case CHIPSET_OPTI381:
        case CHIPSET_ETEQ_BENGAL:
            /* Index 20h, bit 0 = cache enable */
            prev = safe_read(info->index_port, info->data_port, 0x20);
            val = prev & ~0x01;
            safe_write(info->index_port, info->data_port, 0x20, val);
            return prev;

        case CHIPSET_SIS_RABBIT:
        case CHIPSET_SIS_460:
            /* Index 11h, bit 7 = cache enable */
            prev = safe_read(info->index_port, info->data_port, 0x11);
            val = prev & ~0x80;
            safe_write(info->index_port, info->data_port, 0x11, val);
            return prev;

        case CHIPSET_HEADLAND:
            /* Index 1Ah, bit 0 = cache enable */
            prev = safe_read(info->index_port, info->data_port, 0x1A);
            val = prev & ~0x01;
            safe_write(info->index_port, info->data_port, 0x1A, val);
            return prev;

        case CHIPSET_UMC491:
            /* Index 04h, bit 0 = cache enable */
            prev = safe_read(info->index_port, info->data_port, 0x04);
            val = prev & ~0x01;
            safe_write(info->index_port, info->data_port, 0x04, val);
            return prev;

        default:
            return 0;
    }
}

/*
 * Enable external cache (L2)
 */
static void enable_cache(chipset_info_t *info, unsigned char prev_state)
{
    switch (info->type) {
        case CHIPSET_OPTI391:
        case CHIPSET_OPTI381:
        case CHIPSET_ETEQ_BENGAL:
            safe_write(info->index_port, info->data_port, 0x20, prev_state);
            break;

        case CHIPSET_SIS_RABBIT:
        case CHIPSET_SIS_460:
            safe_write(info->index_port, info->data_port, 0x11, prev_state);
            break;

        case CHIPSET_HEADLAND:
            safe_write(info->index_port, info->data_port, 0x1A, prev_state);
            break;

        case CHIPSET_UMC491:
            safe_write(info->index_port, info->data_port, 0x04, prev_state);
            break;
    }
}

/*
 * Flush cache using read loop (386/486 without WBINVD)
 * Reads through a memory region twice the size of cache
 */
static void flush_cache_read_loop(unsigned int cache_kb)
{
    unsigned char far *ptr;
    unsigned long i;
    unsigned long bytes = (unsigned long)cache_kb * 1024UL * 2UL;
    volatile unsigned char dummy;

    /*
     * Read from low memory (below 640K) to flush cache lines.
     * We use the area starting at 0x10000 (64KB) which should be
     * safely above the interrupt vectors and BIOS data area.
     */
    ptr = (unsigned char far *)0x00010000L;

    _disable();

    for (i = 0; i < bytes; i += 16) {
        dummy = ptr[i];
        dummy = ptr[i + 4];
        dummy = ptr[i + 8];
        dummy = ptr[i + 12];
    }

    _enable();

    (void)dummy;  /* Suppress unused warning */
}

/*
 * Flush cache using WBINVD instruction (486+)
 */
static void flush_cache_wbinvd(void)
{
    _asm {
        wbinvd
    }
}

/*
 * Trigger hardware flush on MIC 9391
 * Index 40h, bit 1 = flush trigger
 */
static void flush_cache_mic_hw(chipset_info_t *info)
{
    unsigned char val;

    val = safe_read(info->index_port, info->data_port, 0x40);
    val |= 0x02;  /* Set bit 1 */
    safe_write(info->index_port, info->data_port, 0x40, val);

    /* Small delay */
    inp(0x80);
    inp(0x80);
    inp(0x80);

    val &= ~0x02;  /* Clear bit 1 */
    safe_write(info->index_port, info->data_port, 0x40, val);
}

/*
 * Perform cache flush using best available method
 */
static void flush_cache(chipset_info_t *info, int is_486)
{
    printf("  Flushing cache (%uKB)...\n", info->cache_size_kb);

    /* Special case: MIC 9391 has hardware flush trigger */
    if (info->type == CHIPSET_MIC9391) {
        printf("    Using MIC9391 hardware flush trigger\n");
        flush_cache_mic_hw(info);
        return;
    }

    /* 486+ can use WBINVD */
    if (is_486) {
        printf("    Using WBINVD instruction (486+)\n");
        flush_cache_wbinvd();
        return;
    }

    /* Fall back to read loop */
    printf("    Using read loop (%luKB read)\n",
           (unsigned long)info->cache_size_kb * 2UL);
    flush_cache_read_loop(info->cache_size_kb);
}

/*============================================================================
 * TIMING FUNCTIONS (8254 PIT)
 *============================================================================*/

/*
 * Read 8254 PIT counter 0 for timing
 */
static unsigned int read_pit_count(void)
{
    unsigned char lo, hi;

    _disable();
    outp(0x43, 0x00);  /* Latch counter 0 */
    lo = inp(0x40);
    hi = inp(0x40);
    _enable();

    return ((unsigned int)hi << 8) | lo;
}

/*
 * Measure flush loop execution time in microseconds
 * PIT runs at 1.193182 MHz, so each tick = 0.838 us
 */
static unsigned long measure_flush_time(chipset_info_t *info, int is_486)
{
    unsigned int start, end;
    unsigned long ticks;
    unsigned long us;

    start = read_pit_count();
    flush_cache(info, is_486);
    end = read_pit_count();

    /* Handle counter wraparound */
    if (end > start) {
        ticks = start + (65536UL - end);
    } else {
        ticks = start - end;
    }

    /* Convert to microseconds (tick * 0.838 us) */
    us = (ticks * 838UL) / 1000UL;

    return us;
}

/*============================================================================
 * WRITE-BACK VERIFICATION TEST
 *============================================================================*/

/*
 * Test that write-back cache flushes dirty data correctly
 *
 * This test:
 * 1. Writes a pattern to a test location
 * 2. Forces the cache line dirty (write, then read different address)
 * 3. Flushes the cache
 * 4. Verifies memory contains the correct pattern
 */
static int test_writeback_flush(chipset_info_t *info, int is_486)
{
    volatile unsigned int far *test_ptr;
    unsigned int test_val = 0xAA55;
    unsigned int read_val;
    unsigned char prev_cache;

    printf("\nTesting Write-Back flush...\n");

    if (!info->is_writeback) {
        printf("  Chipset is Write-Through - skipping dirty data test\n");
        return 1;
    }

    /*
     * Use a location in conventional memory for test
     * 0x90000 = 576KB, should be safe if system has 640KB
     */
    test_ptr = (volatile unsigned int far *)0x00090000L;

    printf("  1. Writing 0x%04X to test location (0x90000)\n", test_val);
    *test_ptr = test_val;

    printf("  2. Forcing cache line dirty\n");
    /* Read from a different address to ensure our write is in cache */
    {
        volatile unsigned int far *other = (volatile unsigned int far *)0x00080000L;
        read_val = *other;
        (void)read_val;
    }

    printf("  3. Executing cache flush\n");

    /* For WB: flush BEFORE disable */
    flush_cache(info, is_486);

    printf("  4. Verifying memory\n");

    /* Disable cache to read directly from RAM */
    prev_cache = disable_cache(info);

    read_val = *test_ptr;

    /* Re-enable cache */
    enable_cache(info, prev_cache);

    if (read_val == test_val) {
        printf("\n  Result: PASS - Memory contains 0x%04X\n", read_val);
        return 1;
    } else {
        printf("\n  Result: FAIL - Expected 0x%04X, got 0x%04X\n",
               test_val, read_val);
        printf("\n  *** WARNING: Data corruption detected! ***\n");
        printf("  Cache flush may not be working correctly.\n");
        return 0;
    }
}

/*============================================================================
 * FLUSH SEQUENCE TEST
 *============================================================================*/

static void test_flush_sequence(chipset_info_t *info, int is_486)
{
    unsigned char prev_cache;

    printf("\nTesting flush sequence order...\n");

    if (info->is_writeback) {
        printf("  Cache Type: Write-Back\n");
        printf("  Correct Order: FLUSH first, then DISABLE\n\n");

        printf("  Step 1: Flush cache (while enabled)\n");
        flush_cache(info, is_486);

        printf("  Step 2: Disable cache\n");
        prev_cache = disable_cache(info);

        printf("  Step 3: [Configure NC regions would go here]\n");

        printf("  Step 4: Re-enable cache\n");
        enable_cache(info, prev_cache);

        printf("\n  Sequence completed correctly.\n");
    } else {
        printf("  Cache Type: Write-Through\n");
        printf("  Order: DISABLE first, then FLUSH (order less critical)\n\n");

        printf("  Step 1: Disable cache\n");
        prev_cache = disable_cache(info);

        printf("  Step 2: Flush cache (while disabled)\n");
        flush_cache(info, is_486);

        printf("  Step 3: [Configure NC regions would go here]\n");

        printf("  Step 4: Re-enable cache\n");
        enable_cache(info, prev_cache);

        printf("\n  Sequence completed.\n");
    }
}

/*============================================================================
 * MAIN PROGRAM
 *============================================================================*/

static void print_usage(void)
{
    printf("CACHETST v1.0 - Cache Flush Verification Utility\n");
    printf("Part of the Abacus FPGA Project\n\n");
    printf("Usage: CACHETST [/FLUSH] [/VERIFY] [/TIMING]\n\n");
    printf("  /FLUSH   Perform cache flush and report\n");
    printf("  /VERIFY  Test write-back flush (WB chipsets only)\n");
    printf("  /TIMING  Measure flush execution time\n");
    printf("  /?       This help message\n\n");
    printf("With no options, performs all tests.\n");
}

int main(int argc, char *argv[])
{
    chipset_info_t info;
    int do_flush = 0;
    int do_verify = 0;
    int do_timing = 0;
    int is_486;
    int i;
    unsigned long flush_time;

    /* Parse command line */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '/' || argv[i][0] == '-') {
            if (stricmp(&argv[i][1], "FLUSH") == 0) {
                do_flush = 1;
            } else if (stricmp(&argv[i][1], "VERIFY") == 0) {
                do_verify = 1;
            } else if (stricmp(&argv[i][1], "TIMING") == 0) {
                do_timing = 1;
            } else if (argv[i][1] == '?' || argv[i][1] == 'h' || argv[i][1] == 'H') {
                print_usage();
                return 0;
            }
        }
    }

    /* Default: all tests */
    if (!do_flush && !do_verify && !do_timing) {
        do_flush = 1;
        do_verify = 1;
        do_timing = 1;
    }

    printf("CACHETST v1.0 - Cache Flush Verification Utility\n");
    printf("=================================================\n\n");

    /* Detect CPU type */
    is_486 = is_486_or_better();
    printf("CPU: %s\n", is_486 ? "486 or better (WBINVD available)" :
                                  "386 (using read loop flush)");

    /* Detect chipset */
    info = detect_chipset();

    if (info.type == CHIPSET_UNKNOWN) {
        printf("Chipset: Unknown (using defaults)\n");
        info.cache_size_kb = 256;
    } else {
        printf("Chipset: %s\n", info.name);
    }

    printf("Cache Type: %s\n", info.is_writeback ? "Write-Back" : "Write-Through");
    printf("Cache Size: %uKB (detected)\n", info.cache_size_kb);

    if (info.is_writeback) {
        printf("\n*** WRITE-BACK CACHE ***\n");
        printf("Flush Order: FLUSH first, then DISABLE\n");
        printf("Failure to flush before disable = DATA CORRUPTION!\n");
    }

    /* Flush test */
    if (do_flush) {
        printf("\n--- Flush Sequence Test ---\n");
        test_flush_sequence(&info, is_486);
    }

    /* Verify test */
    if (do_verify) {
        printf("\n--- Write-Back Verification Test ---\n");
        test_writeback_flush(&info, is_486);
    }

    /* Timing test */
    if (do_timing) {
        printf("\n--- Flush Timing Test ---\n");
        printf("Measuring flush execution time...\n");
        flush_time = measure_flush_time(&info, is_486);
        printf("\nTiming: Flush completed in %lu microseconds\n", flush_time);

        if (flush_time < 100) {
            printf("  (Very fast - likely using WBINVD or hardware trigger)\n");
        } else if (flush_time < 1000) {
            printf("  (Normal for hardware-assisted flush)\n");
        } else {
            printf("  (Using read loop - expected for 386 systems)\n");
        }
    }

    printf("\n--- All Tests Complete ---\n");

    return 0;
}
