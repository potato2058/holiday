#include "tui.h"

#include <ctype.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <ncurses.h>

enum {
    CP_TITLE = 1,
    CP_STATUS,
    CP_HEADER,
    CP_TODAY,
    CP_SELECT,
    CP_HOLIDAY,
    CP_BIRTHDAY,
    CP_EVENT,
    CP_MUTED,
    CP_DIALOG,
    CP_ERROR,
    CP_WEEKEND,
    CP_PERSONAL
};

static int tui_started;
static char status_msg[256];
static int status_err;

static void on_signal(int sig)
{
    if (tui_started) {
        endwin();
    }
    _exit(128 + sig);
}

static void set_status(int err, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status_msg, sizeof(status_msg), fmt, ap);
    va_end(ap);
    status_err = err;
}

static void trim(char *s)
{
    char *a = s, *b;
    while (*a && isspace((unsigned char)*a)) {
        a++;
    }
    if (a != s) {
        memmove(s, a, strlen(a) + 1);
    }
    b = s + strlen(s);
    while (b > s && isspace((unsigned char)b[-1])) {
        *--b = '\0';
    }
}

static void print_clip(int y, int x, int w, const char *s)
{
    int n;
    if (w <= 0 || y < 0 || y >= LINES || x >= COLS) {
        return;
    }
    if (x < 0) {
        s += -x;
        w += x;
        x = 0;
        if (w <= 0) {
            return;
        }
    }
    if (x + w > COLS) {
        w = COLS - x;
    }
    n = (int)strlen(s);
    if (n > w) {
        n = w;
    }
    mvaddnstr(y, x, s, n);
    if (n < w) {
        mvhline(y, x + n, ' ', w - n);
    }
}

static void print_fmt(int y, int x, int w, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    print_clip(y, x, w, buf);
}

static void draw_box(int y, int x, int h, int w)
{
    if (h < 2 || w < 2) {
        return;
    }
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + w - 1, ACS_URCORNER);
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
    mvhline(y, x + 1, ACS_HLINE, w - 2);
    mvhline(y + h - 1, x + 1, ACS_HLINE, w - 2);
    mvvline(y + 1, x, ACS_VLINE, h - 2);
    mvvline(y + 1, x + w - 1, ACS_VLINE, h - 2);
}

static attr_t kind_color(EntryKind k)
{
    switch (k) {
    case KIND_HOLIDAY:
        return COLOR_PAIR(CP_HOLIDAY);
    case KIND_BIRTHDAY:
        return COLOR_PAIR(CP_BIRTHDAY);
    default:
        return COLOR_PAIR(CP_EVENT);
    }
}

static attr_t entry_attr(const Entry *e)
{
    return e->personal ? COLOR_PAIR(CP_PERSONAL) : kind_color(e->kind);
}

static int day_mark_color(const Store *s, CalDate d)
{
    int idxs[32];
    int n = store_on_day(s, d, idxs, 32);
    int i, has_b = 0, has_h = 0, has_e = 0, has_p = 0;
    for (i = 0; i < n; i++) {
        if (s->items[idxs[i]].personal) {
            has_p = 1;
        }
        switch (s->items[idxs[i]].kind) {
        case KIND_BIRTHDAY:
            has_b = 1;
            break;
        case KIND_HOLIDAY:
            has_h = 1;
            break;
        default:
            has_e = 1;
            break;
        }
    }
    if (has_p) {
        return COLOR_PAIR(CP_PERSONAL);
    }
    if (has_b) {
        return COLOR_PAIR(CP_BIRTHDAY);
    }
    if (has_h) {
        return COLOR_PAIR(CP_HOLIDAY);
    }
    if (has_e) {
        return COLOR_PAIR(CP_EVENT);
    }
    return 0;
}

static int prompt_line(const char *label, char *out, size_t outsz)
{
    size_t pos = strlen(out);
    curs_set(1);
    for (;;) {
        int ch;
        int lx = (int)strlen(label);
        mvhline(LINES - 1, 0, ' ', COLS);
        attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
        mvhline(LINES - 1, 0, ' ', COLS);
        print_clip(LINES - 1, 1, COLS - 2, "");
        mvaddnstr(LINES - 1, 1, label, COLS - 2);
        if (lx + 1 < COLS) {
            mvaddnstr(LINES - 1, 1 + lx, out, COLS - 2 - lx);
        }
        attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
        move(LINES - 1, 1 + lx + (int)pos);
        if (1 + lx + (int)pos >= COLS) {
            move(LINES - 1, COLS - 1);
        }
        ch = getch();
        if (ch == KEY_RESIZE) {
            continue;
        }
        if (ch == 27) {
            curs_set(0);
            return 0;
        }
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            curs_set(0);
            return 1;
        }
        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (pos > 0) {
                out[--pos] = '\0';
            }
            continue;
        }
        if (ch == KEY_LEFT && pos > 0) {
            pos--;
            continue;
        }
        if (ch == KEY_RIGHT && pos < strlen(out)) {
            pos++;
            continue;
        }
        if (ch >= 32 && ch < 127 && strlen(out) + 1 < outsz) {
            size_t n = strlen(out);
            if (pos < n) {
                memmove(out + pos + 1, out + pos, n - pos + 1);
            } else {
                out[n + 1] = '\0';
            }
            out[pos++] = (char)ch;
        }
    }
}

static int confirm(const char *question)
{
    int ch;
    mvhline(LINES - 1, 0, ' ', COLS);
    attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
    mvhline(LINES - 1, 0, ' ', COLS);
    print_fmt(LINES - 1, 1, COLS - 2, "%s [y/N]", question);
    attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
    ch = getch();
    return ch == 'y' || ch == 'Y';
}

static void shade_win(WINDOW *w)
{
    wbkgd(w, COLOR_PAIR(CP_DIALOG));
}

static int form_dialog(const char *title, Entry *e)
{
    char name[ENTRY_NAME_MAX];
    char date[48];
    EntryKind kind = e->kind;
    int personal = e->personal;
    int field = 1;
    int h = 13, w = 56;
    snprintf(name, sizeof(name), "%s", e->name);
    if (e->when.month || e->when.rule) {
        cal_format_spec(date, sizeof(date), &e->when);
    } else {
        date[0] = '\0';
    }

    curs_set(1);
    for (;;) {
        WINDOW *win;
        int wy, wx, ch, i;
        const char *kinds[] = {"holiday", "event", "birthday"};
        if (LINES < h + 2) {
            h = LINES - 2;
        }
        if (COLS < w + 2) {
            w = COLS - 2;
        }
        if (h < 8 || w < 46) {
            set_status(1, "terminal too small for editor");
            curs_set(0);
            return 0;
        }
        wy = (LINES - h) / 2;
        wx = (COLS - w) / 2;
        win = newwin(h, w, wy, wx);
        keypad(win, TRUE);
        shade_win(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " %s ", title);
        mvwprintw(win, 2, 2, "Type  (left/right)");
        for (i = 0; i < 3; i++) {
            int x = 2 + i * 14;
            if (i == (int)kind) {
                wattron(win, A_REVERSE | kind_color(kind));
            } else if (field == 0) {
                wattron(win, A_BOLD);
            }
            mvwprintw(win, 3, x, " %-10s ", kinds[i]);
            wattrset(win, A_NORMAL);
        }
        mvwprintw(win, 5, 2, "Name");
        if (field == 1) {
            wattron(win, A_REVERSE);
        }
        mvwprintw(win, 6, 2, " %-46s", name);
        wattroff(win, A_REVERSE);
        mvwprintw(win, 7, 2, "When  MM-DD  ~MM-DD  MM-DD~MM-DD  4thu-nov  easter");
        if (field == 2) {
            wattron(win, A_REVERSE);
        }
        mvwprintw(win, 8, 2, " %-50s", date);
        wattroff(win, A_REVERSE);
        mvwprintw(win, 9, 2, "Personal  (left/right)");
        if (field == 3) {
            wattron(win, A_REVERSE);
        }
        if (personal) {
            wattron(win, COLOR_PAIR(CP_PERSONAL) | A_BOLD);
        }
        mvwprintw(win, 10, 2, " %s ", personal ? "* personal" : "  shared  ");
        wattrset(win, A_NORMAL);
        mvwprintw(win, h - 2, 2, "Tab fields  Enter save  Esc cancel");
        if (field == 1) {
            wmove(win, 6, 3 + (int)strlen(name));
        } else if (field == 2) {
            wmove(win, 8, 3 + (int)strlen(date));
        } else if (field == 3) {
            wmove(win, 10, 3);
        } else {
            wmove(win, 3, 2 + (int)kind * 14);
        }
        wrefresh(win);
        ch = wgetch(win);
        delwin(win);

        if (ch == KEY_RESIZE) {
            continue;
        }
        if (ch == 27) {
            curs_set(0);
            return 0;
        }
        if (ch == '\t' || ch == KEY_DOWN) {
            field = (field + 1) % 4;
            continue;
        }
        if (ch == KEY_UP || ch == KEY_BTAB) {
            field = (field + 3) % 4;
            continue;
        }
        if (field == 0) {
            if (ch == KEY_LEFT || ch == 'h') {
                kind = (EntryKind)((kind + KIND_COUNT - 1) % KIND_COUNT);
            } else if (ch == KEY_RIGHT || ch == 'l' || ch == ' ') {
                kind = (EntryKind)((kind + 1) % KIND_COUNT);
            } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
                field = 1;
            }
            continue;
        }
        if (field == 3) {
            if (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ' || ch == 'h' ||
                ch == 'l') {
                personal = !personal;
            }
            continue;
        }
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            DateSpec spec;
            trim(name);
            trim(date);
            if (!name[0]) {
                set_status(1, "name cannot be empty");
                continue;
            }
            if (cal_parse_spec(date, &spec) != 0) {
                set_status(1, "try MM-DD, ~MM-DD, MM-DD~MM-DD, 4thu-nov, easter");
                continue;
            }
            memset(e, 0, sizeof(*e));
            e->kind = kind;
            snprintf(e->name, sizeof(e->name), "%s", name);
            e->when = spec;
            e->personal = personal;
            e->builtin = 0;
            curs_set(0);
            return 1;
        }
        if (field != 1 && field != 2) {
            continue;
        }
        {
            char *buf = (field == 1) ? name : date;
            size_t cap = (field == 1) ? sizeof(name) : sizeof(date);
            size_t n = strlen(buf);
            if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                if (n > 0) {
                    buf[n - 1] = '\0';
                }
            } else if (ch >= 32 && ch < 127 && n + 1 < cap) {
                buf[n] = (char)ch;
                buf[n + 1] = '\0';
            }
        }
    }
}

static int pick_index(Store *s, const int *idxs, int n, const char *title)
{
    int sel = 0;
    if (n <= 0) {
        return -1;
    }
    if (n == 1) {
        return idxs[0];
    }
    for (;;) {
        int h = n + 4, w = 50, wy, wx, i, ch;
        WINDOW *win;
        if (h > LINES - 2) {
            h = LINES - 2;
        }
        if (w > COLS - 2) {
            w = COLS - 2;
        }
        wy = (LINES - h) / 2;
        wx = (COLS - w) / 2;
        win = newwin(h, w, wy, wx);
        keypad(win, TRUE);
        shade_win(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " %s ", title);
        for (i = 0; i < n && i < h - 3; i++) {
            const Entry *e = &s->items[idxs[i]];
            char date[24];
            cal_format_spec(date, sizeof(date), &e->when);
            if (i == sel) {
                wattron(win, A_REVERSE);
            }
            wattron(win, entry_attr(e));
            mvwprintw(win, i + 2, 2, "%s%-8s %-8s %-22s", e->personal ? "*" : " ",
                      kind_name(e->kind), date, e->name);
            wattrset(win, A_NORMAL);
        }
        mvwprintw(win, h - 1, 2, " j/k  Enter  Esc ");
        wrefresh(win);
        ch = wgetch(win);
        delwin(win);
        if (ch == 27 || ch == 'q') {
            return -1;
        }
        if (ch == KEY_UP || ch == 'k') {
            if (sel > 0) {
                sel--;
            }
        } else if (ch == KEY_DOWN || ch == 'j') {
            if (sel + 1 < n) {
                sel++;
            }
        } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            return idxs[sel];
        }
    }
}

static void show_help(void)
{
    static const char *lines[] = {
        "h j k l / arrows   move day",
        "[ ]  or  p n       previous / next month",
        "{ }                previous / next year",
        "t                  jump to today",
        "g                  go to date",
        "/                  search by name",
        "a / Enter          add a personal entry on this day",
        "e                  edit name / date (any entry)",
        "d                  remove from the calendar",
        "m                  mark / unmark as personal",
        "s                  save JSON now",
        "r                  reload JSON from disk",
        "?                  this help",
        "q                  quit",
        "",
        "Delete any day to hide it; it stays gone on reload.",
        "Edit a built-in to change its name or date.",
        "When: MM-DD  ~MM-DD (around)  MM-DD~MM-DD (window)",
        "      4thu-nov  last-mon-may  easter  easter-46",
        NULL
    };
    int n = 0, i, h, w = 56, wy, wx, ch;
    while (lines[n]) {
        n++;
    }
    h = n + 4;
    if (h > LINES - 1) {
        h = LINES - 1;
    }
    if (w > COLS - 2) {
        w = COLS - 2;
    }
    wy = (LINES - h) / 2;
    wx = (COLS - w) / 2;
    for (;;) {
        WINDOW *win = newwin(h, w, wy, wx);
        keypad(win, TRUE);
        shade_win(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " keys ");
        for (i = 0; i < n && i < h - 3; i++) {
            mvwprintw(win, i + 2, 2, "%s", lines[i]);
        }
        mvwprintw(win, h - 1, 2, " any key to close ");
        wrefresh(win);
        ch = wgetch(win);
        delwin(win);
        if (ch != KEY_RESIZE) {
            break;
        }
    }
}

static int try_save(Store *s, const char *ok)
{
    if (s->readonly) {
        set_status(1, "read-only: %s", s->err[0] ? s->err : s->path);
        return -1;
    }
    if (store_save(s) != 0) {
        set_status(1, "%s", s->err);
        return -1;
    }
    set_status(0, "%s (%s)", ok, s->path);
    return 0;
}

static void clamp_day(CalDate *sel)
{
    int dim = cal_days_in_month(sel->year, sel->month);
    if (sel->day > dim) {
        sel->day = dim;
    }
    if (sel->day < 1) {
        sel->day = 1;
    }
}

static void shift_month(CalDate *sel, int delta)
{
    int m = sel->month - 1 + delta;
    int y = sel->year + m / 12;
    m %= 12;
    if (m < 0) {
        m += 12;
        y--;
    }
    sel->year = y;
    sel->month = m + 1;
    clamp_day(sel);
}

static void draw_calendar(const Store *s, CalDate sel, CalDate today, int y,
                          int x, int h, int w)
{
    char header[64];
    int first, dim, d, col, row;
    int i;
    int inner_w = w - 2;
    int grid_x;

    draw_box(y, x, h, w);
    snprintf(header, sizeof(header), "%s %d", cal_month_name(sel.month),
             sel.year);
    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    print_fmt(y, x + 2, inner_w - 2, " %s ", header);
    attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

    grid_x = x + (w - 22) / 2;
    if (grid_x < x + 2) {
        grid_x = x + 2;
    }
    attron(COLOR_PAIR(CP_MUTED));
    for (i = 0; i < 7; i++) {
        int ax = grid_x + i * 3;
        if (i == 0 || i == 6) {
            attroff(COLOR_PAIR(CP_MUTED));
            attron(COLOR_PAIR(CP_WEEKEND));
        }
        mvaddstr(y + 2, ax, cal_wday_abbrev(i));
        if (i == 0 || i == 6) {
            attroff(COLOR_PAIR(CP_WEEKEND));
            attron(COLOR_PAIR(CP_MUTED));
        }
    }
    attroff(COLOR_PAIR(CP_MUTED));

    first = cal_weekday(sel.year, sel.month, 1);
    dim = cal_days_in_month(sel.year, sel.month);
    col = first;
    row = 0;
    for (d = 1; d <= dim; d++) {
        CalDate cur = {sel.year, sel.month, d};
        int ax = grid_x + col * 3;
        int ay = y + 4 + row;
        int attr = 0;
        int mark = day_mark_color(s, cur);
        char cell[8];
        int is_sel = (d == sel.day);
        int is_today = (cal_cmp(cur, today) == 0);
        int is_weekend = (col == 0 || col == 6);

        if (ay >= y + h - 2) {
            break;
        }
        snprintf(cell, sizeof(cell), "%2d", d < 1 ? 1 : (d > 31 ? 31 : d));
        if (is_sel) {
            attr = COLOR_PAIR(CP_SELECT) | A_BOLD;
        } else if (is_today) {
            attr = COLOR_PAIR(CP_TODAY) | A_BOLD;
        } else if (mark) {
            attr = mark | A_BOLD;
        } else if (is_weekend) {
            attr = COLOR_PAIR(CP_WEEKEND);
        }
        attron(attr);
        mvaddstr(ay, ax, cell);
        attroff(attr);
        if (mark && !is_sel) {
            attron(mark);
            mvaddch(ay, ax + 2, ACS_BULLET);
            attroff(mark);
        } else if (mark && is_sel) {
            attron(COLOR_PAIR(CP_SELECT) | A_BOLD);
            mvaddch(ay, ax + 2, ACS_BULLET);
            attroff(COLOR_PAIR(CP_SELECT) | A_BOLD);
        } else {
            mvaddch(ay, ax + 2, ' ');
        }
        col++;
        if (col == 7) {
            col = 0;
            row++;
        }
    }

    {
        int ly = y + h - 2;
        if (ly > y + 2) {
            attron(COLOR_PAIR(CP_HOLIDAY) | A_BOLD);
            mvaddstr(ly, x + 2, "holiday");
            attroff(COLOR_PAIR(CP_HOLIDAY) | A_BOLD);
            attron(COLOR_PAIR(CP_BIRTHDAY) | A_BOLD);
            mvaddstr(ly, x + 11, "birthday");
            attroff(COLOR_PAIR(CP_BIRTHDAY) | A_BOLD);
            attron(COLOR_PAIR(CP_EVENT) | A_BOLD);
            mvaddstr(ly, x + 21, "event");
            attroff(COLOR_PAIR(CP_EVENT) | A_BOLD);
            attron(COLOR_PAIR(CP_PERSONAL) | A_BOLD);
            mvaddstr(ly, x + 28, "*you");
            attroff(COLOR_PAIR(CP_PERSONAL) | A_BOLD);
        }
    }
}

static void draw_details(const Store *s, CalDate sel, CalDate today, int y,
                         int x, int h, int w)
{
    char pretty[64];
    int idxs[64];
    int n, i, row;
    CalDate upc_on[24];
    int upc_idx[24];
    int un, ui;

    draw_box(y, x, h, w);
    cal_format_pretty(pretty, sizeof(pretty), sel);
    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    print_fmt(y, x + 2, w - 4, " %s ", pretty);
    attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

    n = store_on_day(s, sel, idxs, 64);
    row = y + 2;
    if (n == 0) {
        attron(COLOR_PAIR(CP_MUTED));
        print_clip(row, x + 2, w - 4, "nothing on this day");
        attroff(COLOR_PAIR(CP_MUTED));
        row += 2;
    } else {
        for (i = 0; i < n && row < y + h - 8; i++) {
            const Entry *e = &s->items[idxs[i]];
            char when[80];
            entry_format_when(e, sel.year, when, sizeof(when));
            attron(entry_attr(e) | A_BOLD);
            print_fmt(row, x + 2, w - 4, "%s%s", e->personal ? "* " : "", e->name);
            attroff(COLOR_PAIR(CP_PERSONAL) | kind_color(e->kind) | A_BOLD);
            row++;
            attron(COLOR_PAIR(CP_MUTED));
            print_fmt(row, x + 4, w - 6, "%s%s · %s", kind_name(e->kind),
                      e->builtin ? " (built-in)" : "", when);
            attroff(COLOR_PAIR(CP_MUTED));
            row += 2;
        }
    }

    if (row < y + h - 4) {
        attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
        print_clip(row, x + 2, w - 4, "upcoming");
        attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);
        row++;
        un = store_upcoming(s, today, 60, upc_idx, upc_on, 24);
        if (un == 0) {
            attron(COLOR_PAIR(CP_MUTED));
            print_clip(row, x + 2, w - 4, "nothing in the next 60 days");
            attroff(COLOR_PAIR(CP_MUTED));
        }
        for (ui = 0; ui < un && row < y + h - 2; ui++) {
            const Entry *e = &s->items[upc_idx[ui]];
            print_fmt(row, x + 2, w - 4, "%s %2d  ",
                      cal_month_abbrev(upc_on[ui].month), upc_on[ui].day);
            attron(entry_attr(e));
            print_fmt(row, x + 10, w - 12, "%s%s", e->personal ? "* " : "",
                      e->name);
            attroff(COLOR_PAIR(CP_PERSONAL) | kind_color(e->kind));
            row++;
        }
    }
}

static void draw_all(const Store *s, CalDate sel, CalDate today)
{
    int left_w, right_w, box_h, y0;
    char title[256];
    const char *hint =
        " ? help  a add  e edit  d del  m personal  t today  q quit";

    erase();
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvhline(0, 0, ' ', COLS);
    snprintf(title, sizeof(title), " holiday  %s%s%s", s->path,
             s->dirty ? "  [modified]" : "",
             s->readonly ? "  [read-only]" : "");
    print_clip(0, 1, COLS - 2, title);
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

    y0 = 1;
    box_h = LINES - 2 - y0;
    if (box_h < 8) {
        box_h = LINES - 2;
        y0 = 1;
    }

    if (COLS >= 72) {
        left_w = 34;
        if (left_w > COLS / 2) {
            left_w = COLS / 2;
        }
        right_w = COLS - left_w - 1;
        draw_calendar(s, sel, today, y0, 0, box_h, left_w);
        draw_details(s, sel, today, y0, left_w + 1, box_h, right_w);
    } else {
        int cal_h = 14;
        if (cal_h > box_h / 2 + 4) {
            cal_h = box_h / 2 + 4;
        }
        if (cal_h < 10) {
            cal_h = box_h;
        }
        draw_calendar(s, sel, today, y0, 0, cal_h, COLS);
        if (box_h - cal_h >= 8) {
            draw_details(s, sel, today, y0 + cal_h, 0, box_h - cal_h, COLS);
        }
    }

    attron(COLOR_PAIR(status_err ? CP_ERROR : CP_STATUS) | A_BOLD);
    mvhline(LINES - 1, 0, ' ', COLS);
    if (status_msg[0]) {
        print_clip(LINES - 1, 1, COLS - 2, status_msg);
    } else {
        print_clip(LINES - 1, 1, COLS - 2, hint);
    }
    attroff(COLOR_PAIR(status_err ? CP_ERROR : CP_STATUS) | A_BOLD);
}

static int pick_day_entry(Store *s, CalDate sel, const char *title)
{
    int idxs[64];
    int n = store_on_day(s, sel, idxs, 64);
    if (n == 0) {
        set_status(1, "no entries on this day");
        return -1;
    }
    return pick_index(s, idxs, n, title);
}

int tui_run(Store *s)
{
    CalDate today = cal_today();
    CalDate sel = today;
    int running = 1;

    setlocale(LC_ALL, "");
    initscr();
    tui_started = 1;
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    meta(stdscr, TRUE);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(CP_TITLE, COLOR_BLACK, COLOR_CYAN);
        init_pair(CP_STATUS, COLOR_BLACK, COLOR_WHITE);
        init_pair(CP_HEADER, COLOR_CYAN, -1);
        init_pair(CP_TODAY, COLOR_BLACK, COLOR_GREEN);
        init_pair(CP_SELECT, COLOR_BLACK, COLOR_WHITE);
        init_pair(CP_HOLIDAY, COLOR_RED, -1);
        init_pair(CP_BIRTHDAY, COLOR_MAGENTA, -1);
        init_pair(CP_EVENT, COLOR_YELLOW, -1);
        init_pair(CP_MUTED, COLOR_BLUE, -1);
        init_pair(CP_DIALOG, COLOR_WHITE, COLOR_BLUE);
        init_pair(CP_ERROR, COLOR_WHITE, COLOR_RED);
        init_pair(CP_WEEKEND, COLOR_BLUE, -1);
        init_pair(CP_PERSONAL, COLOR_GREEN, -1);
    }

    if (s->readonly && s->err[0]) {
        set_status(1, "%s", s->err);
    } else {
        set_status(0, "loaded %d personal, %d built-in", store_user_count(s),
                   s->count - store_user_count(s));
    }

    while (running) {
        int ch;
        draw_all(s, sel, today);
        refresh();
        ch = getch();
        status_msg[0] = '\0';
        status_err = 0;

        switch (ch) {
        case 'q':
        case 'Q':
            if (s->dirty) {
                if (confirm("unsaved changes — save before quit?")) {
                    if (try_save(s, "saved") != 0) {
                        break;
                    }
                }
            }
            running = 0;
            break;
        case 't':
        case 'T':
            today = cal_today();
            sel = today;
            set_status(0, "today");
            break;
        case KEY_LEFT:
        case 'h':
            sel = cal_add_days(sel, -1);
            break;
        case KEY_RIGHT:
        case 'l':
            sel = cal_add_days(sel, 1);
            break;
        case KEY_UP:
        case 'k':
            sel = cal_add_days(sel, -7);
            break;
        case KEY_DOWN:
        case 'j':
            sel = cal_add_days(sel, 7);
            break;
        case '[':
        case 'p':
        case 'P':
            shift_month(&sel, -1);
            break;
        case ']':
        case 'n':
        case 'N':
            shift_month(&sel, 1);
            break;
        case '{':
            sel.year--;
            clamp_day(&sel);
            break;
        case '}':
            sel.year++;
            clamp_day(&sel);
            break;
        case '?':
        case KEY_F(1):
            show_help();
            break;
        case 'g':
        case 'G': {
            char buf[32] = {0};
            int y, m, d;
            if (!prompt_line("go to (YYYY-MM-DD or MM-DD): ", buf,
                             sizeof(buf))) {
                break;
            }
            trim(buf);
            if (cal_parse_date(buf, &y, &m, &d) != 0) {
                /* YYYY-MM */
                if (strlen(buf) == 7 && buf[4] == '-') {
                    y = atoi(buf);
                    m = atoi(buf + 5);
                    d = 1;
                    if (y < 1 || m < 1 || m > 12) {
                        set_status(1, "could not parse date");
                        break;
                    }
                    sel.year = y;
                    sel.month = m;
                    sel.day = 1;
                    break;
                }
                set_status(1, "could not parse date");
                break;
            }
            sel.month = m;
            sel.day = d;
            sel.year = y > 0 ? y : sel.year;
            if (d > cal_days_in_month(sel.year, sel.month)) {
                set_status(1, "that day does not exist in %d", sel.year);
                clamp_day(&sel);
            }
            break;
        }
        case '/': {
            char q[64] = {0};
            int idx;
            CalDate found;
            if (!prompt_line("search: ", q, sizeof(q))) {
                break;
            }
            trim(q);
            if (store_search(s, q, 0, &idx, &found, sel) != 0) {
                set_status(1, "no match for \"%s\"", q);
                break;
            }
            sel = found;
            set_status(0, "found %s", s->items[idx].name);
            break;
        }
        case 'a':
        case 'A':
        case '\n':
        case '\r':
        case KEY_ENTER: {
            Entry e;
            memset(&e, 0, sizeof(e));
            e.kind = KIND_EVENT;
            e.personal = 1;
            e.when.month = sel.month;
            e.when.day = sel.day;
            if (form_dialog("add entry", &e)) {
                if (store_add(s, &e) < 0) {
                    set_status(1, "%s", s->err);
                } else {
                    try_save(s, "added");
                }
            }
            break;
        }
        case 'e':
        case 'E': {
            int idx = pick_day_entry(s, sel, "edit entry");
            Entry e;
            if (idx < 0) {
                break;
            }
            e = s->items[idx];
            if (form_dialog("edit entry", &e)) {
                if (s->items[idx].builtin &&
                    strcasecmp(s->items[idx].name, e.name) != 0) {
                    store_hide(s, s->items[idx].name);
                }
                store_claim(s, idx);
                e.builtin = 0;
                store_update(s, idx, &e);
                try_save(s, "updated");
            }
            break;
        }
        case 'd':
        case 'D': {
            int idx = pick_day_entry(s, sel, "delete entry");
            char q[256];
            if (idx < 0) {
                break;
            }
            snprintf(q, sizeof(q), "remove \"%s\" from the calendar?",
                     s->items[idx].name);
            if (confirm(q)) {
                store_hide(s, s->items[idx].name);
                store_delete(s, idx);
                try_save(s, "removed");
            }
            break;
        }
        case 'm':
        case 'M': {
            int idx = pick_day_entry(s, sel, "mark personal");
            if (idx < 0) {
                break;
            }
            store_claim(s, idx);
            s->items[idx].personal = !s->items[idx].personal;
            s->dirty = 1;
            try_save(s, s->items[idx].personal ? "marked personal" : "unmarked");
            break;
        }
        case 's':
        case 'S':
            try_save(s, "saved");
            break;
        case 'r':
        case 'R': {
            char path[STORE_PATH_MAX];
            snprintf(path, sizeof(path), "%s", s->path);
            if (s->dirty && !confirm("reload and discard unsaved changes?")) {
                break;
            }
            if (store_load(s, path) != 0) {
                set_status(1, "%s", s->err);
            } else {
                if (s->dirty) {
                    store_save(s);
                }
                set_status(0, "reloaded %d personal, %d built-in",
                           store_user_count(s), s->count - store_user_count(s));
            }
            break;
        }
        case KEY_RESIZE:
            break;
        default:
            break;
        }
    }

    endwin();
    tui_started = 0;
    return 0;
}
