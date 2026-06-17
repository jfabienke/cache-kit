/*============================================================================
 * CK_PARSE.H - Validated argument parsing for CACHEKIT CLI tools
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-06-17 00:00:00 EST
 *
 * Header-only (static) helpers, because the CLI tools (CHIPSET, NCCONFIG,
 * CACHETST) are built as single-file `wcl` targets and do not link the HAL
 * objects. Each tool just #includes this header.
 *
 * Provides:
 *   parse_uint_range() - parse an unsigned decimal with full validation
 *   parse_mb_to_kb()   - parse MB and scale to KB, overflow/range-checked
 *   cpu_is_386_plus()  - 286-safe "is this a 386 or later?" gate
 *
 * All C89, 8086-safe. Replaces atoi() (which gives no error signal) and the
 * ad-hoc base_mb*1024 math (which overflowed a 16-bit int).
 *============================================================================*/

#ifndef CK_PARSE_H
#define CK_PARSE_H

/* These are optional-use static helpers: a given CLI tool may include this
 * header but call only some of them. Suppress W202 ("defined but not
 * referenced") so an unused helper does not break the strict (-w4) build. */
#pragma warning 202 9

/*
 * Parse an unsigned decimal integer from `s` into *out, with full validation.
 * Accepts optional leading whitespace and an optional '+'. Rejects: NULL,
 * empty/no-digit, a leading '-', any trailing non-digit, arithmetic overflow,
 * and results outside [lo, hi]. Returns 1 on success (and writes *out), else 0.
 * *out is written only on success.
 */
static int parse_uint_range(const char *s, unsigned long lo, unsigned long hi,
                            unsigned long *out)
{
    unsigned long acc = 0UL;
    int digits = 0;

    if (s == 0)
        return 0;

    while (*s == ' ' || *s == '\t')     /* optional leading whitespace */
        s++;
    if (*s == '+')                      /* optional unary plus */
        s++;
    if (*s == '-')                      /* these fields are unsigned */
        return 0;

    while (*s >= '0' && *s <= '9') {
        unsigned int d = (unsigned int)(*s - '0');
        /* reject before overflowing: acc*10 + d must fit in unsigned long */
        if (acc > (0xFFFFFFFFUL - d) / 10UL)
            return 0;
        acc = acc * 10UL + d;
        digits++;
        s++;
    }

    if (digits == 0)                    /* no digits parsed */
        return 0;
    if (*s != '\0')                     /* trailing garbage */
        return 0;
    if (acc < lo || acc > hi)           /* out of allowed range */
        return 0;

    if (out)
        *out = acc;
    return 1;
}

/*
 * Parse a value in megabytes and return it scaled to kilobytes in *kb_out.
 * The MB value is capped so that mb*1024 cannot overflow an unsigned long,
 * and the resulting KB value is rejected if it exceeds max_kb. The multiply
 * is done in unsigned long (not 16-bit int), fixing the wrap that turned a
 * base of >=64 MB into a near-zero boundary. Returns 1 on success, else 0.
 */
static int parse_mb_to_kb(const char *s, unsigned long max_kb,
                          unsigned long *kb_out)
{
    unsigned long mb;
    unsigned long kb;

    if (!parse_uint_range(s, 0UL, 0xFFFFFFFFUL / 1024UL, &mb))
        return 0;

    kb = mb * 1024UL;
    if (kb > max_kb)
        return 0;

    if (kb_out)
        *kb_out = kb;
    return 1;
}

/*
 * 286-safe CPU gate. Uses only the 16-bit FLAGS bits 12-15 behavior, which is
 * valid on 8086/286: 8086 keeps those bits set, 286 keeps them clear (real
 * mode), 386+ allows them to be modified. Returns 1 for 386 or later, 0 for
 * 8086/286. Call this BEFORE executing any 386-only opcode (pushfd, 32-bit
 * registers, WBINVD) so the tool degrades gracefully on a 286 instead of
 * faulting on an invalid opcode.
 */
static unsigned int ckp_flags_clear_test(void);
#pragma aux ckp_flags_clear_test = \
    "pushf"  "pop ax"  "mov cx, ax" \
    "and ax, 0FFFh"  "push ax"  "popf" \
    "pushf"  "pop ax"  "and ax, 0F000h" \
    "push cx"  "popf" \
    value [ax] modify [cx];

static unsigned int ckp_flags_set_test(void);
#pragma aux ckp_flags_set_test = \
    "pushf"  "pop ax"  "mov cx, ax" \
    "or ax, 0F000h"  "push ax"  "popf" \
    "pushf"  "pop ax"  "and ax, 0F000h" \
    "push cx"  "popf" \
    value [ax] modify [cx];

static int cpu_is_386_plus(void)
{
    if (ckp_flags_clear_test() == 0xF000)   /* bits stuck high -> 8086/8088 */
        return 0;
    if (ckp_flags_set_test() != 0xF000)     /* bits stuck low  -> 80286 */
        return 0;
    return 1;                               /* 386 or later */
}

#endif /* CK_PARSE_H */
