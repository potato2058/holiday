#include "cal.h"
#include "store.h"
#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

static void usage(FILE *out)
{
    fprintf(out,
            "holiday — calendar TUI backed by a JSON file\n"
            "\n"
            "usage: holiday [options]\n"
            "  -f, --file PATH   JSON file (default: dates.json, then .dates.json)\n"
            "      --today       print today and the next 7 days, then exit\n"
            "      --list        list every stored entry\n"
            "      --cal [YYYY-MM]  print a month (default: current)\n"
            "      --import FILE import birthdays from a local .csv or .ics\n"
            "      --self-test   run date/JSON checks\n"
            "  -h, --help        this help\n"
            "\n"
            "With no options, opens the interactive calendar.\n"
            "HOLIDAY_FILE overrides the default path.\n");
}

static int cmd_today(const Store *s)
{
    time_t now = time(NULL);
    struct tm tm;
    char stamp[64];
    CalDate today = cal_today();
    int idxs[64];
    int n, i, found = 0;
    CalDate on[32];
    int uidx[32];
    int un, ui;

    localtime_r(&now, &tm);
    strftime(stamp, sizeof(stamp), "Date: %Y-%b-%d %a %H:%M:%S %z", &tm);
    puts(stamp);

    n = store_on_day(s, today, idxs, 64);
    for (i = 0; i < n; i++) {
        const Entry *e = &s->items[idxs[i]];
        printf("%s: %s%s\n",
               e->kind == KIND_HOLIDAY   ? "Holiday"
               : e->kind == KIND_BIRTHDAY ? "Birthday"
                                          : "Event",
               e->name, e->personal ? " *" : "");
        found = 1;
    }
    if (!found) {
        puts("No holidays, events, or birthdays today.");
    }
    puts("---------------------------------------------------------");
    puts("Upcoming in the next 7 days:");
    un = store_upcoming(s, today, 7, uidx, on, 32);
    if (un == 0) {
        puts("No holidays, events, or birthdays in the next 7 days.");
    }
    for (ui = 0; ui < un; ui++) {
        const Entry *e = &s->items[uidx[ui]];
        char mmdd[8];
        cal_format_date(mmdd, sizeof(mmdd), 0, on[ui].month, on[ui].day);
        printf("%s %02d (%s): %s - %s%s\n", cal_month_abbrev(on[ui].month),
               on[ui].day, mmdd,
               e->kind == KIND_HOLIDAY   ? "Holiday"
               : e->kind == KIND_BIRTHDAY ? "Birthday"
                                          : "Event",
               e->name, e->personal ? " *" : "");
    }
    return 0;
}

static int cmd_list(const Store *s)
{
    int k, i;
    printf("# %s (%d entries)\n", s->path, s->count);
    for (k = 0; k < KIND_COUNT; k++) {
        int any = 0;
        printf("\n[%s]\n", k == KIND_HOLIDAY   ? "holidays"
                           : k == KIND_BIRTHDAY ? "birthdays"
                                                : "events");
        for (i = 0; i < s->count; i++) {
            const Entry *e = &s->items[i];
            char date[48];
            if ((int)e->kind != k) {
                continue;
            }
            cal_format_spec(date, sizeof(date), &e->when);
            printf("  %s  %s%s%s\n", date, e->name,
                   e->personal ? " *" : "", e->builtin ? "  [built-in]" : "");
            any = 1;
        }
        if (!any) {
            puts("  (none)");
        }
    }
    return 0;
}

static int cmd_cal(const Store *s, const char *spec)
{
    CalDate d = cal_today();
    int first, dim, col, day, i;
    int idxs[8];

    if (spec && spec[0]) {
        int y = 0, m = 0, dummy = 0;
        size_t n = strlen(spec);
        if (n == 7 && spec[4] == '-') {
            y = atoi(spec);
            m = atoi(spec + 5);
            if (y < 1 || m < 1 || m > 12) {
                fprintf(stderr, "holiday: bad month %s (want YYYY-MM)\n", spec);
                return 1;
            }
            d.year = y;
            d.month = m;
            d.day = 1;
        } else if (cal_parse_date(spec, &y, &m, &dummy) == 0 && y > 0) {
            d.year = y;
            d.month = m;
            d.day = 1;
        } else {
            fprintf(stderr, "holiday: bad month %s (want YYYY-MM)\n", spec);
            return 1;
        }
    }

    printf("   %s %d\n", cal_month_name(d.month), d.year);
    printf("Su Mo Tu We Th Fr Sa\n");
    first = cal_weekday(d.year, d.month, 1);
    dim = cal_days_in_month(d.year, d.month);
    for (i = 0; i < first; i++) {
        fputs("   ", stdout);
    }
    col = first;
    for (day = 1; day <= dim; day++) {
        CalDate cur = {d.year, d.month, day};
        int n = store_on_day(s, cur, idxs, 8);
        if (n) {
            printf("%2d*", day);
        } else {
            printf("%2d ", day);
        }
        col++;
        if (col == 7) {
            putchar('\n');
            col = 0;
        }
    }
    if (col) {
        putchar('\n');
    }
    return 0;
}

static int fail(const char *msg)
{
    fprintf(stderr, "self-test: FAIL %s\n", msg);
    return 1;
}

static int cmd_self_test(const char *path)
{
    Store s;
    int idxs[16];
    int i, found = 0;
    char tmp[] = "/tmp/holiday-self-test-XXXXXX";
    int fd;
    CalDate d;
    int rc = 0;

    if (cal_weekday(2026, 9, 12) != 6) {
        return fail("2026-09-12 should be Saturday");
    }
    if (cal_weekday(2026, 1, 1) != 4) {
        return fail("2026-01-01 should be Thursday");
    }
    if (!cal_is_leap(2024) || cal_is_leap(2026) || !cal_is_leap(2000) ||
        cal_is_leap(1900)) {
        return fail("leap year logic");
    }
    if (cal_days_in_month(2024, 2) != 29 || cal_days_in_month(2026, 2) != 28) {
        return fail("february length");
    }
    d = cal_add_days((CalDate){2026, 12, 31}, 1);
    if (d.year != 2027 || d.month != 1 || d.day != 1) {
        return fail("add_days year wrap");
    }
    {
        int y, m, day;
        if (cal_parse_date("01-05", &y, &m, &day) != 0 || y != 0 || m != 1 ||
            day != 5) {
            return fail("parse MM-DD");
        }
        if (cal_parse_date("2026-09-12", &y, &m, &day) != 0 || y != 2026 ||
            m != 9 || day != 12) {
            return fail("parse YYYY-MM-DD");
        }
        if (cal_parse_date("00-00", &y, &m, &day) == 0) {
            return fail("00-00 should be invalid");
        }
        if (cal_parse_date("2026-02-29", &y, &m, &day) == 0) {
            return fail("2026-02-29 should be invalid");
        }
    }

    {
        char cpath[] = "/tmp/holiday-comment-XXXXXX";
        int cfd = mkstemp(cpath);
        const char *jsonc =
            "{\n"
            "  // comment\n"
            "  \"holidays\": [\n"
            "    { \"name\": \"X-Day\", \"date\": \"09-12\", },\n"
            "  ],\n"
            "  \"events\": [],\n"
            "  \"birthdays\": []\n"
            "}\n";
        Store cs;
        CalDate today = {2026, 9, 12};
        int cidx[4];
        if (cfd < 0) {
            return fail("mkstemp comments");
        }
        if (write(cfd, jsonc, strlen(jsonc)) < 0) {
            close(cfd);
            unlink(cpath);
            return fail("write comments json");
        }
        close(cfd);
        store_init(&cs);
        if (store_load(&cs, cpath) != 0) {
            fprintf(stderr, "self-test: FAIL comments: %s\n", cs.err);
            store_free(&cs);
            unlink(cpath);
            return 1;
        }
        if (store_on_day(&cs, today, cidx, 4) != 1 ||
            strcmp(cs.items[cidx[0]].name, "X-Day") != 0) {
            store_free(&cs);
            unlink(cpath);
            return fail("comment/trailing-comma JSON");
        }
        store_free(&cs);
        unlink(cpath);
    }

    store_init(&s);
    if (store_load(&s, path) != 0) {
        fprintf(stderr, "self-test: FAIL load %s: %s\n", path, s.err);
        store_free(&s);
        return 1;
    }
    d = (CalDate){2026, 1, 1};
    found = store_on_day(&s, d, idxs, 16);
    for (i = 0; i < found; i++) {
        if (strcasecmp(s.items[idxs[i]].name, "New Year's Day") == 0) {
            break;
        }
    }
    if (found == 0 || i == found) {
        store_free(&s);
        return fail("New Year's Day missing from JSON");
    }
    d = (CalDate){2026, 4, 5};
    found = store_on_day(&s, d, idxs, 16);
    for (i = 0; i < found; i++) {
        if (strcasecmp(s.items[idxs[i]].name, "Easter Sunday") == 0) {
            break;
        }
    }
    if (found == 0 || i == found) {
        store_free(&s);
        return fail("Easter Sunday 2026");
    }
    {
        int before = s.count;
        int ci;
        for (ci = 0; ci < s.count; ci++) {
            if (strcasecmp(s.items[ci].name, "Christmas Day") == 0) {
                break;
            }
        }
        if (ci == s.count) {
            store_free(&s);
            return fail("Christmas Day builtin missing");
        }
        store_hide(&s, "Christmas Day");
        store_delete(&s, ci);
        if (!store_is_hidden(&s, "Christmas Day")) {
            store_free(&s);
            return fail("hide Christmas");
        }
        if (s.count != before - 1) {
            store_free(&s);
            return fail("delete Christmas");
        }
    }
    {
        DateSpec sp;
        if (cal_parse_spec("~09-22", &sp) != 0 ||
            !cal_spec_matches(&sp, (CalDate){2026, 9, 22}) ||
            !cal_spec_matches(&sp, (CalDate){2026, 9, 21}) ||
            cal_spec_matches(&sp, (CalDate){2026, 9, 18})) {
            store_free(&s);
            return fail("~MM-DD around window");
        }
        if (cal_parse_spec("11-22~11-28", &sp) != 0 ||
            !cal_spec_matches(&sp, (CalDate){2026, 11, 22}) ||
            !cal_spec_matches(&sp, (CalDate){2026, 11, 26}) ||
            cal_spec_matches(&sp, (CalDate){2026, 11, 21})) {
            store_free(&s);
            return fail("MM-DD~MM-DD range");
        }
        if (cal_parse_spec("4thu-nov", &sp) != 0) {
            store_free(&s);
            return fail("parse 4thu-nov");
        }
        {
            CalDate got = cal_resolve(&sp, 2026);
            if (got.month != 11 || got.day != 26) {
                store_free(&s);
                return fail("4thu-nov 2026");
            }
        }
    }
    {
        Entry past;
        int before = s.count;
        memset(&past, 0, sizeof(past));
        snprintf(past.name, sizeof(past.name), "Expired One-Off");
        past.kind = KIND_EVENT;
        past.when.year = 2020;
        past.when.month = 1;
        past.when.day = 1;
        store_add(&s, &past);
        if (store_prune_past(&s, (CalDate){2026, 9, 13}) < 1 ||
            s.count != before) {
            store_free(&s);
            return fail("prune past one-off");
        }
    }

    fd = mkstemp(tmp);
    if (fd < 0) {
        store_free(&s);
        return fail("mkstemp");
    }
    close(fd);
    snprintf(s.path, sizeof(s.path), "%s", tmp);
    s.readonly = 0;
    s.dirty = 1;
    if (store_save(&s) != 0) {
        fprintf(stderr, "self-test: FAIL save: %s\n", s.err);
        unlink(tmp);
        store_free(&s);
        return 1;
    }
    store_free(&s);
    store_init(&s);
    if (store_load(&s, tmp) != 0) {
        fprintf(stderr, "self-test: FAIL reload: %s\n", s.err);
        unlink(tmp);
        store_free(&s);
        return 1;
    }
    d = (CalDate){2026, 1, 1};
    found = store_on_day(&s, d, idxs, 16);
    if (found == 0) {
        rc = fail("round-trip lost New Year's Day");
    }
    d = (CalDate){2026, 12, 25};
    found = store_on_day(&s, d, idxs, 16);
    for (i = 0; i < found; i++) {
        if (strcasecmp(s.items[idxs[i]].name, "Christmas Day") == 0) {
            rc = fail("hidden Christmas Day came back");
            break;
        }
    }
    store_free(&s);
    unlink(tmp);

    {
        char ipath[] = "/tmp/holiday-import-XXXXXX";
        int ifd = mkstemp(ipath);
        const char *ics =
            "BEGIN:VCALENDAR\n"
            "BEGIN:VEVENT\n"
            "SUMMARY:Ada Lovelace's Birthday\n"
            "DTSTART;VALUE=DATE:18151210\n"
            "END:VEVENT\n"
            "END:VCALENDAR\n";
        Store is;
        int iidx[4];
        CalDate bday = {2026, 12, 10};
        if (ifd < 0) {
            return fail("mkstemp import");
        }
        if (write(ifd, ics, strlen(ics)) < 0) {
            close(ifd);
            unlink(ipath);
            return fail("write import ics");
        }
        close(ifd);
        store_init(&is);
        if (store_import_file(&is, ipath, KIND_BIRTHDAY) != 1) {
            fprintf(stderr, "self-test: FAIL import: %s\n", is.err);
            store_free(&is);
            unlink(ipath);
            return 1;
        }
        if (store_on_day(&is, bday, iidx, 4) != 1 ||
            strcmp(is.items[iidx[0]].name, "Ada Lovelace") != 0 ||
            is.items[iidx[0]].when.year != 0) {
            store_free(&is);
            unlink(ipath);
            return fail("ics birthday import");
        }
        store_free(&is);
        unlink(ipath);
    }

    if (rc == 0) {
        puts("self-test: ok");
    }
    return rc;
}

int main(int argc, char **argv)
{
    const char *file = NULL;
    int mode = 0; /* 0 tui, 1 today, 2 list, 3 cal, 4 self-test, 6 import */
    const char *cal_spec = NULL;
    const char *import_path = NULL;
    Store store;
    int i, rc;
    char path[STORE_PATH_MAX];

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(stdout);
            return 0;
        }
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--file") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "holiday: %s needs a path\n", argv[i]);
                return 2;
            }
            file = argv[++i];
        } else if (strncmp(argv[i], "--file=", 7) == 0) {
            file = argv[i] + 7;
        } else if (strcmp(argv[i], "--today") == 0) {
            mode = 1;
        } else if (strcmp(argv[i], "--list") == 0) {
            mode = 2;
        } else if (strcmp(argv[i], "--cal") == 0) {
            mode = 3;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                cal_spec = argv[++i];
            }
        } else if (strcmp(argv[i], "--self-test") == 0) {
            mode = 4;
        } else if (strcmp(argv[i], "--import") == 0) {
            mode = 6;
            if (i + 1 >= argc) {
                fprintf(stderr, "holiday: --import needs a .csv or .ics path\n");
                return 2;
            }
            import_path = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "holiday: unknown option %s\n", argv[i]);
            usage(stderr);
            return 2;
        } else {
            fprintf(stderr, "holiday: unexpected argument %s\n", argv[i]);
            usage(stderr);
            return 2;
        }
    }

    store_init(&store);
    store_find_path(path, sizeof(path), file);

    if (mode == 4) {
        rc = cmd_self_test(path);
        store_free(&store);
        return rc;
    }

    if (store_load(&store, path) != 0) {
        fprintf(stderr, "holiday: %s\n", store.err);
        if (mode == 0) {
            /* still open TUI read-only so the user can see the error */
        } else {
            store_free(&store);
            return 1;
        }
    } else if (store.dirty && !store.readonly) {
        store_save(&store);
    }

    if (mode == 1) {
        rc = cmd_today(&store);
    } else if (mode == 2) {
        rc = cmd_list(&store);
    } else if (mode == 3) {
        rc = cmd_cal(&store, cal_spec);
    } else if (mode == 6) {
        int n = store_import_file(&store, import_path, KIND_BIRTHDAY);
        if (n < 0) {
            fprintf(stderr, "holiday: %s\n", store.err);
            rc = 1;
        } else if (store_save(&store) != 0) {
            fprintf(stderr, "holiday: %s\n", store.err);
            rc = 1;
        } else {
            printf("imported %d birthday(s) into %s\n", n, store.path);
            rc = 0;
        }
    } else if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        rc = cmd_today(&store);
    } else {
        rc = tui_run(&store);
    }

    store_free(&store);
    return rc;
}
