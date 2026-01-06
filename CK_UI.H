/*============================================================================
 * CK_UI.H - CACHEKIT User Interface Components
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 12:30:00 EST
 *
 * Common UI components: dialogs, menus, status bars, etc.
 * Uses CK_VIDEO for low-level drawing.
 *============================================================================*/

#ifndef CK_UI_H
#define CK_UI_H

#include "CK_VIDEO.H"

/*============================================================================
 * SCREEN IDENTIFIERS
 *============================================================================*/

typedef enum {
    SCREEN_INFO = 0,        /* F1 - System/Chipset Information */
    SCREEN_NC_CONFIG,       /* F2 - NC Region Configuration */
    SCREEN_CACHE_TEST,      /* F3 - Cache Flush Testing */
    SCREEN_REGISTERS,       /* F4 - Register Dump */
    SCREEN_BENCHMARK,       /* F5 - Memory Benchmarks */
    SCREEN_PROFILES,        /* F6 - Configuration Profiles */
    SCREEN_INVENTORY,       /* F7 - Expansion Card Inventory */
    SCREEN_COUNT
} screen_t;

/*============================================================================
 * DIALOG RESULT CODES
 *============================================================================*/

#define DLG_OK      1
#define DLG_CANCEL  0
#define DLG_YES     1
#define DLG_NO      0

/*============================================================================
 * MENU ITEM STRUCTURE
 *============================================================================*/

#define MENU_MAX_ITEMS  16
#define MENU_ITEM_LEN   32

typedef struct {
    char label[MENU_ITEM_LEN];  /* Display text */
    int  value;                  /* Return value when selected */
    int  enabled;                /* 1 = selectable, 0 = grayed */
} menu_item_t;

typedef struct {
    const char *title;           /* Menu title (NULL for no title) */
    menu_item_t items[MENU_MAX_ITEMS];
    int count;                   /* Number of items */
    int selected;                /* Currently highlighted item */
    int x, y, w;                 /* Position and width */
} menu_t;

/*============================================================================
 * TITLE BAR AND STATUS BAR
 *============================================================================*/

/* Draw application title bar with tab indicators
 * version     - Version string to display
 * current     - Currently active screen (for tab highlighting)
 */
void ui_draw_title_bar(const char *version, screen_t current);

/* Draw status bar at bottom of screen */
void ui_draw_status_bar(const char *msg);

/* Draw status bar with formatted message */
void ui_draw_status_printf(const char *fmt, ...);

/*============================================================================
 * COMMON UI ELEMENTS
 *============================================================================*/

/* Draw horizontal separator line (with T-junctions at box edges) */
void ui_draw_separator(int y);

/* Draw a labeled section header */
void ui_draw_section(int x, int y, const char *title);

/* Draw a label: value pair */
void ui_draw_field(int x, int y, const char *label, const char *value);

/* Draw a checkbox with label */
void ui_draw_checkbox(int x, int y, const char *label, int checked);

/* Draw a radio button with label */
void ui_draw_radio(int x, int y, const char *label, int selected);

/* Draw progress bar with percentage label */
void ui_draw_progress(int x, int y, int width, int percent);

/*============================================================================
 * DIALOG BOXES
 *============================================================================*/

/* Show a message box (OK only)
 * title    - Dialog title
 * message  - Message to display (can contain \n)
 * Returns when user presses Enter or Escape
 */
void ui_message_box(const char *title, const char *message);

/* Show a confirmation dialog (Yes/No)
 * title    - Dialog title
 * message  - Question to ask
 * Returns DLG_YES or DLG_NO
 */
int ui_confirm_box(const char *title, const char *message);

/* Show an error dialog (red styling)
 * title    - Dialog title (NULL for "Error")
 * message  - Error message
 */
void ui_error_box(const char *title, const char *message);

/* Show input dialog for text entry
 * title    - Dialog title
 * prompt   - Prompt text
 * buffer   - Buffer for input (pre-filled for edit)
 * maxlen   - Maximum input length
 * Returns DLG_OK if Enter pressed, DLG_CANCEL if Escape
 */
int ui_input_box(const char *title, const char *prompt, char *buffer, int maxlen);

/* Show input dialog for number entry
 * title    - Dialog title
 * prompt   - Prompt text
 * value    - Pointer to value (pre-filled, receives result)
 * min, max - Valid range
 * Returns DLG_OK or DLG_CANCEL
 */
int ui_number_box(const char *title, const char *prompt, unsigned long *value,
                  unsigned long min, unsigned long max);

/* Show input dialog for hex number entry
 * Same as ui_number_box but displays/accepts hex
 */
int ui_hex_box(const char *title, const char *prompt, unsigned long *value,
               unsigned long min, unsigned long max);

/*============================================================================
 * MENUS
 *============================================================================*/

/* Initialize a menu structure */
void ui_menu_init(menu_t *menu, const char *title, int x, int y, int w);

/* Add item to menu */
void ui_menu_add(menu_t *menu, const char *label, int value, int enabled);

/* Show menu and get selection (blocking)
 * Returns selected item's value, or -1 if cancelled
 */
int ui_menu_show(menu_t *menu);

/*============================================================================
 * LIST VIEW
 *============================================================================*/

/* Draw a scrollable list with highlight
 * items      - Array of strings
 * count      - Number of items
 * selected   - Currently highlighted index
 * top        - First visible item index
 * visible    - Number of visible rows
 * x, y, w    - Position and width
 */
void ui_draw_list(const char **items, int count, int selected, int top,
                  int visible, int x, int y, int w);

/*============================================================================
 * TABLE VIEW
 *============================================================================*/

#define TABLE_MAX_COLS 8

typedef struct {
    const char *header;
    int width;
    int align;  /* 0=left, 1=right, 2=center */
} table_col_t;

/* Draw table header row */
void ui_draw_table_header(int x, int y, table_col_t *cols, int ncols);

/* Draw table row */
void ui_draw_table_row(int x, int y, table_col_t *cols, int ncols,
                       const char **values, unsigned char attr);

/*============================================================================
 * SCREEN FRAME
 *============================================================================*/

/* Draw standard screen frame (box outline, title bar, status bar) */
void ui_draw_frame(const char *version, screen_t current, const char *status);

/* Clear screen content area (between title and status bars) */
void ui_clear_content(void);

#endif /* CK_UI_H */
