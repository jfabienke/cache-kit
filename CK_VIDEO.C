/*============================================================================
 * CK_VIDEO.C - CACHEKIT Video Layer Implementation
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 12:00:00 EST
 *
 * Direct VGA text-mode video output via segment B800h.
 * Uses BIOS INT 10h for mode save/restore and cursor control.
 *============================================================================*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include "CK_VIDEO.H"

/*============================================================================
 * MODULE STATE
 *============================================================================*/

static unsigned int far *g_video_mem = (unsigned int far *)0xB8000000L;
static unsigned char g_orig_mode = 0x03;    /* Default to mode 3 */
static unsigned char g_initialized = 0;

/*============================================================================
 * VIDEO INITIALIZATION AND CLEANUP
 *============================================================================*/

void video_init(void)
{
    if (g_initialized) return;

    /* Get current video mode via INT 10h AH=0Fh */
    _asm {
        mov ah, 0Fh
        int 10h
        mov g_orig_mode, al
    }

    /* Set video memory pointer (color text mode B800:0000) */
    g_video_mem = (unsigned int far *)0xB8000000L;
    g_initialized = 1;

    /* Hide cursor for cleaner UI */
    video_cursor_hide();
}

void video_restore(void)
{
    unsigned char mode;

    if (!g_initialized) return;

    /* Show cursor before restoring mode */
    video_cursor_show();

    /* Restore original video mode via INT 10h AH=00h */
    mode = g_orig_mode;
    _asm {
        mov ah, 00h
        mov al, mode
        int 10h
    }

    g_initialized = 0;
}

/*============================================================================
 * BASIC OUTPUT FUNCTIONS
 *============================================================================*/

void video_putc(int x, int y, char ch, unsigned char attr)
{
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        g_video_mem[y * SCREEN_WIDTH + x] = ((unsigned int)attr << 8) | (unsigned char)ch;
    }
}

void video_puts(int x, int y, const char *str, unsigned char attr)
{
    if (!str) return;
    while (*str && x < SCREEN_WIDTH) {
        video_putc(x++, y, *str++, attr);
    }
}

void video_putsn(int x, int y, const char *str, int maxlen, unsigned char attr)
{
    int i = 0;
    if (!str) return;
    while (*str && x < SCREEN_WIDTH && i < maxlen) {
        video_putc(x++, y, *str++, attr);
        i++;
    }
}

void video_printf(int x, int y, unsigned char attr, const char *fmt, ...)
{
    char buf[SCREEN_WIDTH + 1];
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    buf[SCREEN_WIDTH] = '\0';  /* Safety truncation */
    video_puts(x, y, buf, attr);
}

/*============================================================================
 * FILL AND LINE FUNCTIONS
 *============================================================================*/

void video_clear(unsigned char attr)
{
    int i;
    unsigned int cell = ((unsigned int)attr << 8) | ' ';

    for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        g_video_mem[i] = cell;
    }
}

void video_hline(int x, int y, int len, char ch, unsigned char attr)
{
    while (len-- > 0 && x < SCREEN_WIDTH) {
        video_putc(x++, y, ch, attr);
    }
}

void video_vline(int x, int y, int len, char ch, unsigned char attr)
{
    while (len-- > 0 && y < SCREEN_HEIGHT) {
        video_putc(x, y++, ch, attr);
    }
}

void video_fill(int x, int y, int w, int h, char ch, unsigned char attr)
{
    int i, j;
    for (j = 0; j < h && (y + j) < SCREEN_HEIGHT; j++) {
        for (i = 0; i < w && (x + i) < SCREEN_WIDTH; i++) {
            video_putc(x + i, y + j, ch, attr);
        }
    }
}

/*============================================================================
 * BOX DRAWING
 *============================================================================*/

void video_box(int x, int y, int w, int h, unsigned char attr)
{
    int i;

    if (w < 2 || h < 2) return;

    /* Corners */
    video_putc(x, y, BOX_TL, attr);
    video_putc(x + w - 1, y, BOX_TR, attr);
    video_putc(x, y + h - 1, BOX_BL, attr);
    video_putc(x + w - 1, y + h - 1, BOX_BR, attr);

    /* Horizontal lines */
    for (i = 1; i < w - 1; i++) {
        video_putc(x + i, y, BOX_H, attr);
        video_putc(x + i, y + h - 1, BOX_H, attr);
    }

    /* Vertical lines */
    for (i = 1; i < h - 1; i++) {
        video_putc(x, y + i, BOX_V, attr);
        video_putc(x + w - 1, y + i, BOX_V, attr);
    }
}

void video_box2(int x, int y, int w, int h, unsigned char attr)
{
    int i;

    if (w < 2 || h < 2) return;

    /* Double-line corners */
    video_putc(x, y, BOX2_TL, attr);
    video_putc(x + w - 1, y, BOX2_TR, attr);
    video_putc(x, y + h - 1, BOX2_BL, attr);
    video_putc(x + w - 1, y + h - 1, BOX2_BR, attr);

    /* Double horizontal lines */
    for (i = 1; i < w - 1; i++) {
        video_putc(x + i, y, BOX2_H, attr);
        video_putc(x + i, y + h - 1, BOX2_H, attr);
    }

    /* Double vertical lines */
    for (i = 1; i < h - 1; i++) {
        video_putc(x, y + i, BOX2_V, attr);
        video_putc(x + w - 1, y + i, BOX2_V, attr);
    }
}

/*============================================================================
 * CURSOR CONTROL
 *============================================================================*/

void video_cursor_hide(void)
{
    /* INT 10h AH=01h: Set cursor shape
     * CH bits 5-6 = 01 means cursor off (invisible)
     * CL = end scan line (doesn't matter when hidden)
     */
    _asm {
        mov ah, 01h
        mov ch, 20h     /* Bit 5 set = cursor off */
        mov cl, 00h
        int 10h
    }
}

void video_cursor_show(void)
{
    /* INT 10h AH=01h: Set cursor shape
     * Standard underline cursor: start=6, end=7 (for 8-line cell)
     */
    _asm {
        mov ah, 01h
        mov ch, 06h     /* Start scan line */
        mov cl, 07h     /* End scan line */
        int 10h
    }
}

void video_cursor_move(int x, int y)
{
    unsigned char col = (unsigned char)x;
    unsigned char row = (unsigned char)y;

    /* INT 10h AH=02h: Set cursor position
     * BH = page number (0)
     * DH = row, DL = column
     */
    _asm {
        mov ah, 02h
        mov bh, 00h
        mov dh, row
        mov dl, col
        int 10h
    }
}

/*============================================================================
 * KEYBOARD INPUT
 *============================================================================*/

int video_getkey(void)
{
    int key;

    /* INT 16h AH=00h: Read keyboard character (blocking) */
    _asm {
        mov ah, 00h
        int 16h
        mov key, ax
    }

    /* If AL is 0, it's an extended key - return scan code shifted */
    if ((key & 0xFF) == 0) {
        return key;  /* Already in 0xSS00 format */
    }

    /* Regular ASCII key - return just the ASCII value */
    return key & 0xFF;
}

int video_kbhit(void)
{
    int result;

    /* INT 16h AH=01h: Check keyboard buffer (non-blocking)
     * ZF=0 if key available, ZF=1 if not
     */
    _asm {
        mov ah, 01h
        int 16h
        jz no_key
        mov result, 1
        jmp done
    no_key:
        mov result, 0
    done:
    }

    return result;
}

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/*
 * Draw a progress bar: [########....] style
 * x, y     - position
 * width    - inner width (excluding brackets)
 * filled   - current value
 * total    - maximum value
 * attr     - color attribute
 */
void video_progress(int x, int y, int width, int filled, int total, unsigned char attr)
{
    int i;
    int fill_count;

    if (total <= 0) total = 1;
    fill_count = (width * filled) / total;
    if (fill_count > width) fill_count = width;

    video_putc(x, y, '[', attr);
    for (i = 0; i < width; i++) {
        video_putc(x + 1 + i, y, (i < fill_count) ? BLOCK_FULL : BLOCK_LIGHT, attr);
    }
    video_putc(x + width + 1, y, ']', attr);
}

/*
 * Center a string on a line
 * y        - row
 * str      - text to center
 * attr     - color attribute
 */
void video_center(int y, const char *str, unsigned char attr)
{
    int len = strlen(str);
    int x = (SCREEN_WIDTH - len) / 2;
    if (x < 0) x = 0;
    video_puts(x, y, str, attr);
}

/*
 * Draw a horizontal separator line with T-junctions at edges
 * Assumes a box outline exists at columns 0 and 79
 */
void video_separator(int y, unsigned char attr)
{
    video_putc(0, y, BOX_LT, attr);
    video_hline(1, y, SCREEN_WIDTH - 2, BOX_H, attr);
    video_putc(SCREEN_WIDTH - 1, y, BOX_RT, attr);
}
