/*============================================================================
 * CK_UI.C - CACHEKIT User Interface Components
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 12:30:00 EST
 *
 * Common UI components: dialogs, menus, status bars, etc.
 *============================================================================*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>    /* strtoul - without this it implicitly returns int,
                          truncating the parsed number/hex value */
#include <string.h>
#include <ctype.h>
#include "CK_UI.H"
#include "CK_VIDEO.H"

/*============================================================================
 * INTERNAL HELPERS
 *============================================================================*/

/* Calculate dialog width for message (handles newlines) */
static int calc_msg_width(const char *msg, int *lines)
{
    int width = 0;
    int line_width = 0;
    int line_count = 1;
    const char *p = msg;

    while (*p) {
        if (*p == '\n') {
            if (line_width > width) width = line_width;
            line_width = 0;
            line_count++;
        } else {
            line_width++;
        }
        p++;
    }
    if (line_width > width) width = line_width;

    if (lines) *lines = line_count;
    return width;
}

/* Draw multi-line message in dialog */
static void draw_msg_lines(int x, int y, const char *msg, unsigned char attr)
{
    char line[80];
    int i = 0;
    int row = y;

    while (*msg) {
        if (*msg == '\n') {
            line[i] = '\0';
            video_puts(x, row++, line, attr);
            i = 0;
        } else if (i < 78) {
            line[i++] = *msg;
        }
        msg++;
    }
    line[i] = '\0';
    if (i > 0) {
        video_puts(x, row, line, attr);
    }
}

/*============================================================================
 * TITLE BAR AND STATUS BAR
 *============================================================================*/

void ui_draw_title_bar(const char *version, screen_t current)
{
    static const char *tabs[] = {
        "F1 Info", "F2 NC", "F3 Test", "F4 Reg",
        "F5 Bench", "F6 Prof", "F7 Cards", "F8 Bus"
    };
    char title[32];
    int i, x;

    /* Clear title bar */
    video_hline(0, 0, SCREEN_WIDTH, ' ', ATTR_TITLE);

    /* Draw version */
    sprintf(title, "CACHEKIT v%s", version);
    video_puts(1, 0, title, ATTR_TITLE);

    /* Draw tabs */
    x = 17;
    for (i = 0; i < SCREEN_COUNT && i < 8; i++) {
        unsigned char attr = (current == i) ? ATTR_TAB_ACTIVE : ATTR_TAB_IDLE;
        if (current == i) {
            video_putc(x, 0, '[', ATTR_TITLE);
            video_puts(x + 1, 0, tabs[i], attr);
            video_putc(x + 1 + strlen(tabs[i]), 0, ']', ATTR_TITLE);
            x += strlen(tabs[i]) + 3;
        } else {
            video_puts(x, 0, tabs[i], attr);
            x += strlen(tabs[i]) + 1;
        }
    }

    /* Exit hint */
    video_puts(72, 0, "Alt-X", ATTR_TITLE);
}

void ui_draw_status_bar(const char *msg)
{
    video_hline(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, ' ', ATTR_STATUS);
    if (msg) {
        video_puts(1, SCREEN_HEIGHT - 1, msg, ATTR_STATUS);
    }
}

void ui_draw_status_printf(const char *fmt, ...)
{
    char buf[SCREEN_WIDTH];
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    ui_draw_status_bar(buf);
}

/*============================================================================
 * COMMON UI ELEMENTS
 *============================================================================*/

void ui_draw_separator(int y)
{
    video_putc(0, y, BOX_LT, ATTR_BOX);
    video_hline(1, y, SCREEN_WIDTH - 2, BOX_H, ATTR_BOX);
    video_putc(SCREEN_WIDTH - 1, y, BOX_RT, ATTR_BOX);
}

void ui_draw_section(int x, int y, const char *title)
{
    video_puts(x, y, title, ATTR_HIGHLIGHT);
    video_hline(x, y + 1, strlen(title), BOX_H, ATTR_DIM);
}

void ui_draw_field(int x, int y, const char *label, const char *value)
{
    video_puts(x, y, label, ATTR_LABEL);
    video_puts(x + strlen(label) + 1, y, value, ATTR_VALUE);
}

void ui_draw_checkbox(int x, int y, const char *label, int checked)
{
    video_putc(x, y, '[', ATTR_NORMAL);
    video_putc(x + 1, y, checked ? CHECK_ON : CHECK_OFF, checked ? ATTR_SUCCESS : ATTR_DIM);
    video_putc(x + 2, y, ']', ATTR_NORMAL);
    video_puts(x + 4, y, label, ATTR_NORMAL);
}

void ui_draw_radio(int x, int y, const char *label, int selected)
{
    video_putc(x, y, '(', ATTR_NORMAL);
    video_putc(x + 1, y, selected ? RADIO_ON : ' ', selected ? ATTR_SUCCESS : ATTR_DIM);
    video_putc(x + 2, y, ')', ATTR_NORMAL);
    video_puts(x + 4, y, label, ATTR_NORMAL);
}

void ui_draw_progress(int x, int y, int width, int percent)
{
    char pct_str[8];
    int fill;

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    fill = (width * percent) / 100;

    video_putc(x, y, '[', ATTR_NORMAL);
    video_hline(x + 1, y, fill, BLOCK_FULL, ATTR_SUCCESS);
    video_hline(x + 1 + fill, y, width - fill, BLOCK_LIGHT, ATTR_DIM);
    video_putc(x + width + 1, y, ']', ATTR_NORMAL);

    sprintf(pct_str, "%3d%%", percent);
    video_puts(x + width + 3, y, pct_str, ATTR_VALUE);
}

/*============================================================================
 * DIALOG BOXES
 *============================================================================*/

void ui_message_box(const char *title, const char *message)
{
    int msg_lines;
    int msg_width;
    int title_len;
    int dlg_width;
    int dlg_height;
    int x, y;
    int key;

    msg_width = calc_msg_width(message, &msg_lines);
    title_len = title ? strlen(title) : 0;
    dlg_width = (msg_width > title_len ? msg_width : title_len) + 6;
    dlg_height = msg_lines + 6;
    x = (SCREEN_WIDTH - dlg_width) / 2;
    y = (SCREEN_HEIGHT - dlg_height) / 2;

    if (dlg_width > 76) dlg_width = 76;
    if (dlg_height > 20) dlg_height = 20;

    /* Draw dialog box */
    video_fill(x, y, dlg_width, dlg_height, ' ', ATTR_NORMAL);
    video_box2(x, y, dlg_width, dlg_height, ATTR_BOX);

    /* Title */
    if (title) {
        video_puts(x + (dlg_width - title_len) / 2, y, title, ATTR_HIGHLIGHT);
    }

    /* Message */
    draw_msg_lines(x + 3, y + 2, message, ATTR_NORMAL);

    /* OK button */
    video_puts(x + (dlg_width - 6) / 2, y + dlg_height - 2, "[ OK ]", ATTR_SELECTED);

    /* Wait for key */
    while (1) {
        key = video_getkey();
        if (key == KEY_ENTER || key == KEY_ESC || key == KEY_SPACE) {
            break;
        }
    }
}

int ui_confirm_box(const char *title, const char *message)
{
    int msg_lines;
    int msg_width;
    int title_len;
    int dlg_width;
    int dlg_height;
    int x, y;
    int selected = 0;  /* 0 = Yes, 1 = No */
    int btn_y, yes_x, no_x;
    int key;

    msg_width = calc_msg_width(message, &msg_lines);
    title_len = title ? strlen(title) : 0;
    dlg_width = (msg_width > title_len ? msg_width : title_len) + 6;
    dlg_height = msg_lines + 6;
    x = (SCREEN_WIDTH - dlg_width) / 2;
    y = (SCREEN_HEIGHT - dlg_height) / 2;

    if (dlg_width < 24) dlg_width = 24;
    if (dlg_width > 76) dlg_width = 76;
    if (dlg_height > 20) dlg_height = 20;

    /* Draw dialog box */
    video_fill(x, y, dlg_width, dlg_height, ' ', ATTR_NORMAL);
    video_box2(x, y, dlg_width, dlg_height, ATTR_BOX);

    /* Title */
    if (title) {
        video_puts(x + (dlg_width - title_len) / 2, y, title, ATTR_HIGHLIGHT);
    }

    /* Message */
    draw_msg_lines(x + 3, y + 2, message, ATTR_NORMAL);

    /* Buttons */
    btn_y = y + dlg_height - 2;
    yes_x = x + dlg_width / 2 - 10;
    no_x = x + dlg_width / 2 + 3;
    while (1) {
        video_puts(yes_x, btn_y, "[ Yes ]", selected == 0 ? ATTR_SELECTED : ATTR_NORMAL);
        video_puts(no_x, btn_y, "[ No ]", selected == 1 ? ATTR_SELECTED : ATTR_NORMAL);

        key = video_getkey();
        switch (key) {
            case KEY_LEFT:
            case KEY_TAB:
                selected = 0;
                break;
            case KEY_RIGHT:
                selected = 1;
                break;
            case 'y':
            case 'Y':
                return DLG_YES;
            case 'n':
            case 'N':
            case KEY_ESC:
                return DLG_NO;
            case KEY_ENTER:
            case KEY_SPACE:
                return selected == 0 ? DLG_YES : DLG_NO;
        }
    }
}

void ui_error_box(const char *title, const char *message)
{
    int msg_lines;
    int msg_width;
    int title_len;
    int dlg_width;
    int dlg_height;
    int x, y;
    int key;

    msg_width = calc_msg_width(message, &msg_lines);
    title_len = title ? strlen(title) : 5;  /* "Error" */
    dlg_width = (msg_width > title_len ? msg_width : title_len) + 6;
    dlg_height = msg_lines + 6;
    x = (SCREEN_WIDTH - dlg_width) / 2;
    y = (SCREEN_HEIGHT - dlg_height) / 2;

    if (dlg_width > 76) dlg_width = 76;
    if (dlg_height > 20) dlg_height = 20;

    /* Draw dialog with error styling */
    video_fill(x, y, dlg_width, dlg_height, ' ', ATTR_ERROR);
    video_box2(x, y, dlg_width, dlg_height, ATTR_ERROR);

    /* Title */
    video_puts(x + (dlg_width - title_len) / 2, y,
               title ? title : "Error", ATTR_ERROR | 0x08);  /* Bright */

    /* Message */
    draw_msg_lines(x + 3, y + 2, message, ATTR_ERROR);

    /* OK button */
    video_puts(x + (dlg_width - 6) / 2, y + dlg_height - 2, "[ OK ]", ATTR_SELECTED);

    /* Wait for key */
    while (1) {
        key = video_getkey();
        if (key == KEY_ENTER || key == KEY_ESC || key == KEY_SPACE) {
            break;
        }
    }
}

int ui_input_box(const char *title, const char *prompt, char *buffer, int maxlen)
{
    int prompt_len;
    int title_len;
    int dlg_width;
    int dlg_height = 7;
    int x, y, input_x, input_y;
    int pos;
    int result = DLG_CANCEL;
    int key;

    prompt_len = strlen(prompt);
    title_len = title ? strlen(title) : 0;
    dlg_width = prompt_len + maxlen + 8;
    pos = strlen(buffer);

    if (dlg_width < title_len + 4) dlg_width = title_len + 4;
    if (dlg_width > 76) dlg_width = 76;

    x = (SCREEN_WIDTH - dlg_width) / 2;
    y = (SCREEN_HEIGHT - dlg_height) / 2;
    input_x = x + 3 + prompt_len + 1;
    input_y = y + 3;

    /* Draw dialog */
    video_fill(x, y, dlg_width, dlg_height, ' ', ATTR_NORMAL);
    video_box2(x, y, dlg_width, dlg_height, ATTR_BOX);

    /* Title */
    if (title) {
        video_puts(x + (dlg_width - title_len) / 2, y, title, ATTR_HIGHLIGHT);
    }

    /* Prompt */
    video_puts(x + 3, input_y, prompt, ATTR_LABEL);

    /* Input field background */
    video_hline(input_x, input_y, maxlen, '_', ATTR_DIM);

    /* Show cursor */
    video_cursor_show();
    video_cursor_move(input_x + pos, input_y);

    /* Input loop */
    while (1) {
        /* Draw current input */
        video_hline(input_x, input_y, maxlen, ' ', ATTR_SELECTED);
        video_putsn(input_x, input_y, buffer, maxlen, ATTR_SELECTED);
        video_cursor_move(input_x + pos, input_y);

        key = video_getkey();

        if (key == KEY_ENTER) {
            result = DLG_OK;
            break;
        } else if (key == KEY_ESC) {
            result = DLG_CANCEL;
            break;
        } else if (key == KEY_BACKSPACE && pos > 0) {
            pos--;
            buffer[pos] = '\0';
        } else if (key >= 32 && key < 127 && pos < maxlen) {
            buffer[pos++] = (char)key;
            buffer[pos] = '\0';
        } else if (key == KEY_LEFT && pos > 0) {
            pos--;
        } else if (key == KEY_RIGHT && pos < (int)strlen(buffer)) {
            pos++;
        }
    }

    video_cursor_hide();
    return result;
}

int ui_number_box(const char *title, const char *prompt, unsigned long *value,
                  unsigned long min, unsigned long max)
{
    char buf[16];
    int result;

    sprintf(buf, "%lu", *value);
    result = ui_input_box(title, prompt, buf, 12);

    if (result == DLG_OK) {
        unsigned long v = strtoul(buf, NULL, 10);
        if (v >= min && v <= max) {
            *value = v;
        } else {
            ui_error_box("Invalid Value", "Value out of range");
            return DLG_CANCEL;
        }
    }
    return result;
}

int ui_hex_box(const char *title, const char *prompt, unsigned long *value,
               unsigned long min, unsigned long max)
{
    char buf[16];
    int result;

    sprintf(buf, "%lX", *value);
    result = ui_input_box(title, prompt, buf, 10);

    if (result == DLG_OK) {
        unsigned long v = strtoul(buf, NULL, 16);
        if (v >= min && v <= max) {
            *value = v;
        } else {
            ui_error_box("Invalid Value", "Value out of range");
            return DLG_CANCEL;
        }
    }
    return result;
}

/*============================================================================
 * MENUS
 *============================================================================*/

void ui_menu_init(menu_t *menu, const char *title, int x, int y, int w)
{
    menu->title = title;
    menu->count = 0;
    menu->selected = 0;
    menu->x = x;
    menu->y = y;
    menu->w = w;
}

void ui_menu_add(menu_t *menu, const char *label, int value, int enabled)
{
    if (menu->count < MENU_MAX_ITEMS) {
        strncpy(menu->items[menu->count].label, label, MENU_ITEM_LEN - 1);
        menu->items[menu->count].label[MENU_ITEM_LEN - 1] = '\0';
        menu->items[menu->count].value = value;
        menu->items[menu->count].enabled = enabled;
        menu->count++;
    }
}

int ui_menu_show(menu_t *menu)
{
    int i;
    int h;
    int x;
    int y;
    int w;
    int key;
    unsigned char attr;

    /* Guard against an empty menu: the navigation code does
       `% menu->count`, which is a divide-by-zero (INT 0 -> crash) when
       count == 0. Return the "cancelled" sentinel instead. */
    if (menu == NULL || menu->count <= 0)
        return -1;

    h = menu->count + 2;
    x = menu->x;
    y = menu->y;
    w = menu->w;

    if (menu->title) h++;

    /* Draw menu box */
    video_fill(x, y, w, h, ' ', ATTR_NORMAL);
    video_box(x, y, w, h, ATTR_BOX);

    /* Title */
    if (menu->title) {
        video_puts(x + 2, y + 1, menu->title, ATTR_HIGHLIGHT);
        y++;
    }

    /* Menu loop */
    while (1) {
        /* Draw items */
        for (i = 0; i < menu->count; i++) {
            if (!menu->items[i].enabled) {
                attr = ATTR_DIM;
            } else if (i == menu->selected) {
                attr = ATTR_SELECTED;
            } else {
                attr = ATTR_NORMAL;
            }
            video_hline(x + 1, y + 1 + i, w - 2, ' ', attr);
            video_puts(x + 2, y + 1 + i, menu->items[i].label, attr);
        }

        key = video_getkey();
        switch (key) {
            case KEY_UP:
                {
                    /* Stop after a full cycle so an all-disabled menu can't
                       loop forever (the old `count > 1` guard never broke). */
                    int start = menu->selected;
                    do {
                        menu->selected =
                            (menu->selected + menu->count - 1) % menu->count;
                    } while (!menu->items[menu->selected].enabled &&
                             menu->selected != start);
                }
                break;

            case KEY_DOWN:
                {
                    int start = menu->selected;
                    do {
                        menu->selected = (menu->selected + 1) % menu->count;
                    } while (!menu->items[menu->selected].enabled &&
                             menu->selected != start);
                }
                break;

            case KEY_ENTER:
            case KEY_SPACE:
                if (menu->items[menu->selected].enabled) {
                    return menu->items[menu->selected].value;
                }
                break;

            case KEY_ESC:
                return -1;

            default:
                /* Check for hotkey (first char of enabled items) */
                for (i = 0; i < menu->count; i++) {
                    if (menu->items[i].enabled &&
                        toupper(key) == toupper(menu->items[i].label[0])) {
                        return menu->items[i].value;
                    }
                }
                break;
        }
    }
}

/*============================================================================
 * LIST VIEW
 *============================================================================*/

void ui_draw_list(const char **items, int count, int selected, int top,
                  int visible, int x, int y, int w)
{
    int i;

    for (i = 0; i < visible && (top + i) < count; i++) {
        int idx = top + i;
        unsigned char attr = (idx == selected) ? ATTR_SELECTED : ATTR_NORMAL;

        video_hline(x, y + i, w, ' ', attr);
        video_putsn(x + 1, y + i, items[idx], w - 2, attr);
    }

    /* Clear remaining rows */
    for (; i < visible; i++) {
        video_hline(x, y + i, w, ' ', ATTR_NORMAL);
    }

    /* Scroll indicators */
    if (top > 0) {
        video_putc(x + w - 1, y, ARROW_UP, ATTR_DIM);
    }
    if (top + visible < count) {
        video_putc(x + w - 1, y + visible - 1, ARROW_DOWN, ATTR_DIM);
    }
}

/*============================================================================
 * TABLE VIEW
 *============================================================================*/

void ui_draw_table_header(int x, int y, table_col_t *cols, int ncols)
{
    int i, cx = x;

    for (i = 0; i < ncols && i < TABLE_MAX_COLS; i++) {
        video_putsn(cx, y, cols[i].header, cols[i].width, ATTR_HIGHLIGHT);
        cx += cols[i].width + 1;
    }
    video_hline(x, y + 1, cx - x - 1, BOX_H, ATTR_DIM);
}

void ui_draw_table_row(int x, int y, table_col_t *cols, int ncols,
                       const char **values, unsigned char attr)
{
    int i, cx = x;

    if (cols == NULL || values == NULL)
        return;

    for (i = 0; i < ncols && i < TABLE_MAX_COLS; i++) {
        /* Guard against a NULL cell: strlen(NULL) would fault. */
        const char *val = values[i] ? values[i] : "";
        int len = strlen(val);
        int pad = cols[i].width - len;
        int left_pad = 0, right_pad = 0;

        if (pad > 0) {
            switch (cols[i].align) {
                case 1: left_pad = pad; break;  /* Right align */
                case 2: left_pad = pad / 2; right_pad = pad - left_pad; break;  /* Center */
                default: right_pad = pad; break;  /* Left align */
            }
        }

        video_hline(cx, y, left_pad, ' ', attr);
        video_putsn(cx + left_pad, y, val, cols[i].width - left_pad, attr);
        video_hline(cx + left_pad + len, y, right_pad, ' ', attr);

        cx += cols[i].width + 1;
    }
}

/*============================================================================
 * SCREEN FRAME
 *============================================================================*/

void ui_draw_frame(const char *version, screen_t current, const char *status)
{
    /* Clear screen */
    video_clear(ATTR_NORMAL);

    /* Draw outer box */
    video_box(0, 1, SCREEN_WIDTH, SCREEN_HEIGHT - 2, ATTR_BOX);

    /* Title bar and status bar */
    ui_draw_title_bar(version, current);
    ui_draw_status_bar(status);
}

void ui_clear_content(void)
{
    /* Clear area between title (row 0) and status (row 24), inside box */
    video_fill(1, 2, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 4, ' ', ATTR_NORMAL);
}
