/*============================================================================
 * CK_VIDEO.H - CACHEKIT Video Layer Interface
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 12:00:00 EST
 *
 * Direct VGA text-mode (mode 3) video output via segment B800h.
 * All functions work with 80x25 character cells.
 *============================================================================*/

#ifndef CK_VIDEO_H
#define CK_VIDEO_H

/*============================================================================
 * SCREEN DIMENSIONS
 *============================================================================*/

#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 25

/*============================================================================
 * VIDEO ATTRIBUTES (CGA/EGA/VGA compatible)
 *
 * Format: 0xBF where B=background (0-7), F=foreground (0-F)
 * Add 0x80 for blinking (if enabled)
 *============================================================================*/

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

/*============================================================================
 * BOX DRAWING CHARACTERS (Code Page 437)
 *============================================================================*/

#define BOX_TL  0xDA    /* Top-left corner */
#define BOX_TR  0xBF    /* Top-right corner */
#define BOX_BL  0xC0    /* Bottom-left corner */
#define BOX_BR  0xD9    /* Bottom-right corner */
#define BOX_H   0xC4    /* Horizontal line */
#define BOX_V   0xB3    /* Vertical line */
#define BOX_LT  0xC3    /* Left-T junction */
#define BOX_RT  0xB4    /* Right-T junction */
#define BOX_TT  0xC2    /* Top-T junction */
#define BOX_BT  0xC1    /* Bottom-T junction */
#define BOX_X   0xC5    /* Cross junction */

/* Double-line box (for dialogs) */
#define BOX2_TL 0xC9
#define BOX2_TR 0xBB
#define BOX2_BL 0xC8
#define BOX2_BR 0xBC
#define BOX2_H  0xCD
#define BOX2_V  0xBA

/*============================================================================
 * BLOCK CHARACTERS (Code Page 437)
 *============================================================================*/

#define BLOCK_FULL  0xDB    /* Full block */
#define BLOCK_LIGHT 0xB0    /* Light shade (25%) */
#define BLOCK_MED   0xB1    /* Medium shade (50%) */
#define BLOCK_DARK  0xB2    /* Dark shade (75%) */
#define BLOCK_UPPER 0xDF    /* Upper half block */
#define BLOCK_LOWER 0xDC    /* Lower half block */

/*============================================================================
 * UI ELEMENT CHARACTERS
 *============================================================================*/

#define CHECK_ON    0xFE    /* Filled square (checkbox checked) */
#define CHECK_OFF   ' '     /* Space (checkbox unchecked) */
#define RADIO_ON    0x07    /* Bullet (radio selected) */
#define RADIO_OFF   0x09    /* Circle (radio unselected) */
#define ARROW_RIGHT 0x10    /* Right arrow */
#define ARROW_LEFT  0x11    /* Left arrow */
#define ARROW_UP    0x1E    /* Up arrow */
#define ARROW_DOWN  0x1F    /* Down arrow */

/*============================================================================
 * KEY CODES
 *
 * Standard keys return their ASCII value.
 * Extended keys (F-keys, arrows) return 0x00 followed by scan code,
 * which we combine as 0xSS00 for convenience.
 *============================================================================*/

#define KEY_ESC       0x1B
#define KEY_ENTER     0x0D
#define KEY_BACKSPACE 0x08
#define KEY_SPACE     0x20
#define KEY_TAB       0x09

/* Extended keys (scan code << 8) */
#define KEY_F1      0x3B00
#define KEY_F2      0x3C00
#define KEY_F3      0x3D00
#define KEY_F4      0x3E00
#define KEY_F5      0x3F00
#define KEY_F6      0x4000
#define KEY_F7      0x4100
#define KEY_F8      0x4200
#define KEY_F9      0x4300
#define KEY_F10     0x4400

#define KEY_UP      0x4800
#define KEY_DOWN    0x5000
#define KEY_LEFT    0x4B00
#define KEY_RIGHT   0x4D00
#define KEY_HOME    0x4700
#define KEY_END     0x4F00
#define KEY_PGUP    0x4900
#define KEY_PGDN    0x5100
#define KEY_INS     0x5200
#define KEY_DEL     0x5300

#define KEY_ALT_X   0x2D00

/*============================================================================
 * VIDEO INITIALIZATION AND CLEANUP
 *============================================================================*/

/* Initialize video layer (saves current mode, sets up B800 access) */
void video_init(void);

/* Restore original video mode on exit */
void video_restore(void);

/*============================================================================
 * BASIC OUTPUT FUNCTIONS
 *============================================================================*/

/* Put single character at (x,y) with attribute */
void video_putc(int x, int y, char ch, unsigned char attr);

/* Put null-terminated string at (x,y) with attribute */
void video_puts(int x, int y, const char *str, unsigned char attr);

/* Put string with maximum length (no null required) */
void video_putsn(int x, int y, const char *str, int maxlen, unsigned char attr);

/* Printf-style output at (x,y) with attribute */
void video_printf(int x, int y, unsigned char attr, const char *fmt, ...);

/*============================================================================
 * FILL AND LINE FUNCTIONS
 *============================================================================*/

/* Clear entire screen with attribute (fills with spaces) */
void video_clear(unsigned char attr);

/* Draw horizontal line of repeated character */
void video_hline(int x, int y, int len, char ch, unsigned char attr);

/* Draw vertical line of repeated character */
void video_vline(int x, int y, int len, char ch, unsigned char attr);

/* Fill rectangular area with character */
void video_fill(int x, int y, int w, int h, char ch, unsigned char attr);

/*============================================================================
 * BOX DRAWING
 *============================================================================*/

/* Draw single-line box outline */
void video_box(int x, int y, int w, int h, unsigned char attr);

/* Draw double-line box outline (for dialogs) */
void video_box2(int x, int y, int w, int h, unsigned char attr);

/*============================================================================
 * CURSOR CONTROL
 *============================================================================*/

/* Hide text cursor */
void video_cursor_hide(void);

/* Show text cursor */
void video_cursor_show(void);

/* Move cursor to position */
void video_cursor_move(int x, int y);

/*============================================================================
 * KEYBOARD INPUT
 *============================================================================*/

/* Get key press (blocking). Returns ASCII or extended code (0xSS00) */
int video_getkey(void);

/* Check if key is waiting (non-blocking). Returns 1 if key available */
int video_kbhit(void);

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/* Draw progress bar: [########....] style
 * width is inner width excluding brackets */
void video_progress(int x, int y, int width, int filled, int total, unsigned char attr);

/* Center string on line */
void video_center(int y, const char *str, unsigned char attr);

/* Draw horizontal separator with T-junctions (assumes 80-col box at edges) */
void video_separator(int y, unsigned char attr);

#endif /* CK_VIDEO_H */
