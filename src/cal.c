#include "cal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cal_is_leap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int cal_days_in_month(int year, int month)
{
    static const int dim[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && cal_is_leap(year)) {
        return 29;
    }
    return dim[month];
}

int cal_weekday(int year, int month, int day)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = year;
    if (month < 3) {
        y -= 1;
    }
    return (y + y / 4 - y / 100 + y / 400 + t[month - 1] + day) % 7;
}

int cal_valid(CalDate d)
{
    if (d.year < 1 || d.month < 1 || d.month > 12 || d.day < 1) {
        return 0;
    }
    return d.day <= cal_days_in_month(d.year, d.month);
}

CalDate cal_today(void)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    CalDate d = {tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday};
    return d;
}

CalDate cal_add_days(CalDate d, int n)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = d.year - 1900;
    tm.tm_mon = d.month - 1;
    tm.tm_mday = d.day + n;
    tm.tm_isdst = -1;
    mktime(&tm);
    CalDate out = {tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday};
    return out;
}

int cal_cmp(CalDate a, CalDate b)
{
    if (a.year != b.year) {
        return a.year - b.year;
    }
    if (a.month != b.month) {
        return a.month - b.month;
    }
    return a.day - b.day;
}

static const char *month_names[] = {
    "", "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
static const char *month_abbrevs[] = {
    "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char *wday_names[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
static const char *wday_abbrevs[] = {
    "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"
};

const char *cal_month_name(int month)
{
    if (month < 1 || month > 12) {
        return "";
    }
    return month_names[month];
}

const char *cal_month_abbrev(int month)
{
    if (month < 1 || month > 12) {
        return "";
    }
    return month_abbrevs[month];
}

const char *cal_wday_name(int wday)
{
    if (wday < 0 || wday > 6) {
        return "";
    }
    return wday_names[wday];
}

const char *cal_wday_abbrev(int wday)
{
    if (wday < 0 || wday > 6) {
        return "";
    }
    return wday_abbrevs[wday];
}

static int parse_int_span(const char *s, int len, int *out)
{
    int v = 0;
    int i;
    if (len <= 0) {
        return -1;
    }
    for (i = 0; i < len; i++) {
        if (!isdigit((unsigned char)s[i])) {
            return -1;
        }
        v = v * 10 + (s[i] - '0');
    }
    *out = v;
    return 0;
}

int cal_parse_date(const char *s, int *year, int *month, int *day)
{
    int y = 0, m = 0, d = 0;
    size_t n;
    const char *p;

    if (!s) {
        return -1;
    }
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        n--;
    }

    if (n == 10 && s[4] == '-' && s[7] == '-') {
        if (parse_int_span(s, 4, &y) || parse_int_span(s + 5, 2, &m) ||
            parse_int_span(s + 8, 2, &d)) {
            return -1;
        }
        p = s + 10;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p) {
            return -1;
        }
        if (m < 1 || m > 12 || d < 1 || d > 31) {
            return -1;
        }
        if (d > cal_days_in_month(y, m)) {
            return -1;
        }
        *year = y;
        *month = m;
        *day = d;
        return 0;
    }

    if (n == 5 && s[2] == '-') {
        if (parse_int_span(s, 2, &m) || parse_int_span(s + 3, 2, &d)) {
            return -1;
        }
        p = s + 5;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p) {
            return -1;
        }
        if (m < 1 || m > 12 || d < 1 || d > 31) {
            return -1;
        }
        *year = 0;
        *month = m;
        *day = d;
        return 0;
    }

    return -1;
}

void cal_format_date(char *buf, int n, int year, int month, int day)
{
    if (year > 0) {
        snprintf(buf, (size_t)n, "%04d-%02d-%02d", year, month, day);
    } else {
        snprintf(buf, (size_t)n, "%02d-%02d", month, day);
    }
}

void cal_format_pretty(char *buf, int n, CalDate d)
{
    int wd = cal_weekday(d.year, d.month, d.day);
    snprintf(buf, (size_t)n, "%s, %s %d, %d",
             cal_wday_name(wd), cal_month_name(d.month), d.day, d.year);
}

CalDate cal_nth_weekday(int year, int month, int wday, int nth)
{
    int dim = cal_days_in_month(year, month);
    CalDate d = {year, month, 1};
    int first, day;
    if (dim <= 0) {
        return d;
    }
    if (nth < 0) {
        int last = cal_weekday(year, month, dim);
        day = dim - (last - wday + 7) % 7;
        d.day = day;
        return d;
    }
    if (nth < 1) {
        nth = 1;
    }
    first = cal_weekday(year, month, 1);
    day = 1 + (wday - first + 7) % 7 + (nth - 1) * 7;
    if (day > dim) {
        day -= 7;
    }
    d.day = day;
    return d;
}

CalDate cal_easter(int year)
{
    /* Anonymous Gregorian algorithm */
    int a = year % 19;
    int b = year / 100;
    int c = year % 100;
    int d = b / 4;
    int e = b % 4;
    int f = (b + 8) / 25;
    int g = (b - f + 1) / 3;
    int h = (19 * a + b - d - g + 15) % 30;
    int i = c / 4;
    int k = c % 4;
    int l = (32 + 2 * e + 2 * i - h - k) % 7;
    int m = (a + 11 * h + 22 * l) / 451;
    int month = (h + l - 7 * m + 114) / 31;
    int day = ((h + l - 7 * m + 114) % 31) + 1;
    CalDate out = {year, month, day};
    return out;
}

CalDate cal_wday_on_or_after(int year, int month, int day, int wday)
{
    CalDate d = {year, month, day};
    int i;
    if (!cal_valid(d)) {
        d.day = 1;
    }
    for (i = 0; i < 8; i++) {
        if (cal_weekday(d.year, d.month, d.day) == wday) {
            return d;
        }
        d = cal_add_days(d, 1);
    }
    return d;
}

CalDate cal_resolve(const DateSpec *sp, int year)
{
    CalDate d;
    if (sp->rule == CAL_RULE_EASTER) {
        return cal_add_days(cal_easter(year), sp->rule_offset);
    }
    if (sp->rule == CAL_RULE_NTH_WDAY) {
        d = cal_nth_weekday(year, sp->rule_month ? sp->rule_month : sp->month,
                            sp->rule_wday, sp->rule_n);
        if (sp->rule_offset) {
            d = cal_add_days(d, sp->rule_offset);
        }
        return d;
    }
    if (sp->rule == CAL_RULE_WDAY_ON_OR_AFTER) {
        d = cal_wday_on_or_after(year, sp->month, sp->day, sp->rule_wday);
        if (sp->rule_offset) {
            d = cal_add_days(d, sp->rule_offset);
        }
        return d;
    }
    d.year = sp->year > 0 ? sp->year : year;
    d.month = sp->month;
    d.day = sp->day;
    if (d.day > cal_days_in_month(d.year, d.month)) {
        d.day = cal_days_in_month(d.year, d.month);
    }
    return d;
}

static int doy(CalDate d)
{
    int n = d.day, m;
    for (m = 1; m < d.month; m++) {
        n += cal_days_in_month(d.year, m);
    }
    return n;
}

static int abs_i(int x)
{
    return x < 0 ? -x : x;
}

int cal_spec_matches(const DateSpec *sp, CalDate d)
{
    CalDate a, b;
    int rad, dist, dim;

    if (sp->rule != CAL_RULE_NONE) {
        a = cal_resolve(sp, d.year);
        return cal_cmp(a, d) == 0;
    }

    if (sp->around) {
        rad = sp->around_days > 0 ? sp->around_days : CAL_AROUND_DEFAULT;
        a = cal_resolve(sp, d.year);
        dist = abs_i(doy(d) - doy(a));
        dim = cal_is_leap(d.year) ? 366 : 365;
        if (dist > dim / 2) {
            dist = dim - dist;
        }
        return dist <= rad;
    }

    if (sp->is_range && sp->until_month) {
        a.year = sp->year > 0 ? sp->year : d.year;
        a.month = sp->month;
        a.day = sp->day;
        b.year = sp->until_year > 0 ? sp->until_year
                 : (sp->year > 0 ? sp->year : d.year);
        b.month = sp->until_month;
        b.day = sp->until_day;
        if (sp->year > 0 && (d.year < a.year || d.year > b.year)) {
            /* one-off range: only those years */
        }
        if (sp->year == 0) {
            a.year = b.year = d.year;
        }
        if (cal_cmp(a, b) <= 0) {
            return cal_cmp(a, d) <= 0 && cal_cmp(d, b) <= 0;
        }
        /* wraps year, e.g. 12-20~01-05 */
        return cal_cmp(d, a) >= 0 || cal_cmp(d, b) <= 0;
    }

    if (sp->month != d.month || sp->day != d.day) {
        return 0;
    }
    if (sp->year == 0) {
        return d.day <= cal_days_in_month(d.year, d.month);
    }
    return sp->year == d.year;
}

int cal_spec_next(const DateSpec *sp, CalDate from, CalDate *out)
{
    int off;
    /* One-off already past? */
    if (sp->year > 0 && !sp->around && !sp->is_range && sp->rule == CAL_RULE_NONE) {
        CalDate d = {sp->year, sp->month, sp->day};
        if (cal_cmp(d, from) <= 0) {
            return -1;
        }
        *out = d;
        return 0;
    }
    if (sp->year > 0 && sp->is_range) {
        CalDate b = {sp->until_year > 0 ? sp->until_year : sp->year,
                     sp->until_month, sp->until_day};
        if (cal_cmp(b, from) <= 0) {
            return -1;
        }
    }
    for (off = 1; off <= 400; off++) {
        CalDate d = cal_add_days(from, off);
        if (cal_spec_matches(sp, d)) {
            *out = d;
            return 0;
        }
        /* skip rest of a long range: return first hit only — caller unique-s */
    }
    return -1;
}

int cal_end_passed(const DateSpec *sp, CalDate today)
{
    CalDate end;
    if (sp->year <= 0 && sp->rule == CAL_RULE_NONE) {
        return 0; /* yearly */
    }
    if (sp->rule != CAL_RULE_NONE && sp->year <= 0) {
        return 0;
    }
    if (sp->is_range && sp->until_month) {
        end.year = sp->until_year > 0 ? sp->until_year : sp->year;
        end.month = sp->until_month;
        end.day = sp->until_day;
        return cal_cmp(end, today) < 0;
    }
    if (sp->year > 0) {
        end.year = sp->year;
        end.month = sp->month;
        end.day = sp->day;
        return cal_cmp(end, today) < 0;
    }
    return 0;
}

static int month_from_tok(const char *s, int len)
{
    static const char *names[] = {
        "", "january", "february", "march", "april", "may", "june",
        "july", "august", "september", "october", "november", "december"
    };
    static const char *abbr[] = {
        "", "jan", "feb", "mar", "apr", "may", "jun",
        "jul", "aug", "sep", "oct", "nov", "dec"
    };
    char buf[16];
    int i, v;
    if (len <= 0 || len > 12) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        buf[i] = (char)tolower((unsigned char)s[i]);
    }
    buf[len] = '\0';
    if (isdigit((unsigned char)buf[0])) {
        v = atoi(buf);
        return (v >= 1 && v <= 12) ? v : 0;
    }
    for (i = 1; i <= 12; i++) {
        if (strcmp(buf, names[i]) == 0 || strcmp(buf, abbr[i]) == 0) {
            return i;
        }
    }
    return 0;
}

static int wday_from_tok(const char *s, int len)
{
    char buf[16];
    int i;
    if (len <= 0 || len > 12) {
        return -1;
    }
    for (i = 0; i < len; i++) {
        buf[i] = (char)tolower((unsigned char)s[i]);
    }
    buf[len] = '\0';
    if (!strcmp(buf, "su") || !strcmp(buf, "sun") || !strcmp(buf, "sunday")) {
        return 0;
    }
    if (!strcmp(buf, "mo") || !strcmp(buf, "mon") || !strcmp(buf, "monday")) {
        return 1;
    }
    if (!strcmp(buf, "tu") || !strcmp(buf, "tue") || !strcmp(buf, "tues") ||
        !strcmp(buf, "tuesday")) {
        return 2;
    }
    if (!strcmp(buf, "we") || !strcmp(buf, "wed") || !strcmp(buf, "wednesday")) {
        return 3;
    }
    if (!strcmp(buf, "th") || !strcmp(buf, "thu") || !strcmp(buf, "thur") ||
        !strcmp(buf, "thurs") || !strcmp(buf, "thursday")) {
        return 4;
    }
    if (!strcmp(buf, "fr") || !strcmp(buf, "fri") || !strcmp(buf, "friday")) {
        return 5;
    }
    if (!strcmp(buf, "sa") || !strcmp(buf, "sat") || !strcmp(buf, "saturday")) {
        return 6;
    }
    return -1;
}

static void fill_nth_bounds(DateSpec *out)
{
    int n = out->rule_n;
    out->rule_month = out->rule_month ? out->rule_month : out->month;
    out->month = out->rule_month;
    out->year = 0;
    if (n < 0) {
        out->day = 25;
        out->until_month = out->month;
        out->until_day = 31;
        out->is_range = 1;
        return;
    }
    if (n < 1) {
        n = 1;
    }
    if (n > 5) {
        n = 5;
    }
    out->day = (n - 1) * 7 + 1;
    out->until_month = out->month;
    out->until_day = n * 7;
    if (out->until_day > 31) {
        out->until_day = 31;
    }
    out->is_range = 1;
}

static int parse_offset(const char *s, int *off)
{
    if (!s || !*s) {
        *off = 0;
        return 0;
    }
    if (s[0] != '+' && s[0] != '-') {
        return -1;
    }
    *off = atoi(s);
    return 0;
}

static int parse_rule(const char *s, DateSpec *out)
{
    const char *dash, *p;
    int nth = 1, wday, month, off = 0;
    char tmp[64];
    size_t n = strlen(s);

    if (n >= sizeof(tmp)) {
        return -1;
    }
    memcpy(tmp, s, n + 1);
    for (p = tmp; *p; p++) {
        * (char *)p = (char)tolower((unsigned char)*p);
    }
    p = tmp;

    if (!strncmp(p, "easter", 6)) {
        out->rule = CAL_RULE_EASTER;
        if (parse_offset(p + 6, &off) != 0) {
            return -1;
        }
        out->rule_offset = off;
        out->month = 3;
        out->day = 22;
        out->until_month = 4;
        out->until_day = 25;
        out->is_range = 1;
        out->year = 0;
        return 0;
    }

    /* sun>=11-27 or sun>=11-27-7 */
    {
        const char *ge = strstr(p, ">=");
        if (ge) {
            int y = 0, m = 0, d = 0;
            char datebuf[16];
            int dlen;
            wday = wday_from_tok(p, (int)(ge - p));
            if (wday < 0) {
                return -1;
            }
            ge += 2;
            dlen = 0;
            while (ge[dlen] && ge[dlen] != '+' &&
                   !(ge[dlen] == '-' && dlen >= 5)) {
                dlen++;
            }
            if (dlen >= (int)sizeof(datebuf)) {
                return -1;
            }
            memcpy(datebuf, ge, (size_t)dlen);
            datebuf[dlen] = '\0';
            if (cal_parse_date(datebuf, &y, &m, &d) != 0) {
                return -1;
            }
            if (parse_offset(ge + dlen, &off) != 0) {
                return -1;
            }
            out->rule = CAL_RULE_WDAY_ON_OR_AFTER;
            out->rule_wday = wday;
            out->rule_offset = off;
            out->month = m;
            out->day = d;
            out->year = 0;
            return 0;
        }
    }

    if (!strncmp(p, "last-", 5)) {
        nth = -1;
        p += 5;
    } else if (isdigit((unsigned char)*p)) {
        nth = 0;
        while (isdigit((unsigned char)*p)) {
            nth = nth * 10 + (*p - '0');
            p++;
        }
    } else {
        return -1;
    }

    dash = strchr(p, '-');
    if (!dash) {
        return -1;
    }
    wday = wday_from_tok(p, (int)(dash - p));
    {
        const char *mp = dash + 1;
        const char *op = strchr(mp, '+');
        char mtok[16];
        size_t ml;
        if (op) {
            if (parse_offset(op, &off) != 0) {
                return -1;
            }
            ml = (size_t)(op - mp);
        } else {
            ml = strlen(mp);
        }
        if (ml == 0 || ml >= sizeof(mtok)) {
            return -1;
        }
        memcpy(mtok, mp, ml);
        mtok[ml] = '\0';
        month = month_from_tok(mtok, (int)ml);
    }
    if (wday < 0 || month < 1) {
        return -1;
    }
    out->rule = CAL_RULE_NTH_WDAY;
    out->rule_n = nth;
    out->rule_wday = wday;
    out->rule_month = month;
    out->rule_offset = off;
    fill_nth_bounds(out);
    return 0;
}

int cal_parse_spec(const char *s, DateSpec *out)
{
    char left[32], right[32];
    const char *tilde;
    const char *p;
    DateSpec a, b;
    int y, m, d;

    memset(out, 0, sizeof(*out));
    if (!s) {
        return -1;
    }
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    p = s + strlen(s);
    while (p > s && isspace((unsigned char)p[-1])) {
        p--;
    }
    {
        size_t n = (size_t)(p - s);
        char buf[64];
        if (n == 0 || n >= sizeof(buf)) {
            return -1;
        }
        memcpy(buf, s, n);
        buf[n] = '\0';
        s = buf;

        if (s[0] == '~') {
            if (cal_parse_date(s + 1, &y, &m, &d) != 0) {
                return -1;
            }
            out->year = y;
            out->month = m;
            out->day = d;
            out->around = 1;
            out->around_days = CAL_AROUND_DEFAULT;
            return 0;
        }

        tilde = strchr(s, '~');
        if (tilde && tilde != s) {
            size_t ln = (size_t)(tilde - s);
            if (ln >= sizeof(left) || strlen(tilde + 1) >= sizeof(right)) {
                return -1;
            }
            memcpy(left, s, ln);
            left[ln] = '\0';
            snprintf(right, sizeof(right), "%s", tilde + 1);
            memset(&a, 0, sizeof(a));
            memset(&b, 0, sizeof(b));
            if (cal_parse_date(left, &a.year, &a.month, &a.day) != 0 ||
                cal_parse_date(right, &b.year, &b.month, &b.day) != 0) {
                return -1;
            }
            out->year = a.year;
            out->month = a.month;
            out->day = a.day;
            out->until_year = b.year;
            out->until_month = b.month;
            out->until_day = b.day;
            out->is_range = 1;
            return 0;
        }

        if (parse_rule(s, out) == 0) {
            return 0;
        }
        if (cal_parse_date(s, &y, &m, &d) == 0) {
            out->year = y;
            out->month = m;
            out->day = d;
            return 0;
        }
        return -1;
    }
}

void cal_format_spec(char *buf, int n, const DateSpec *sp)
{
    char a[40], b[40];
    static const char *wd[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
    static const char *mo[] = {
        "", "jan", "feb", "mar", "apr", "may", "jun",
        "jul", "aug", "sep", "oct", "nov", "dec"
    };

    if (sp->rule == CAL_RULE_EASTER) {
        if (sp->rule_offset == 0) {
            snprintf(buf, (size_t)n, "easter");
        } else {
            snprintf(buf, (size_t)n, "easter%+d", sp->rule_offset);
        }
        return;
    }
    if (sp->rule == CAL_RULE_NTH_WDAY) {
        int month = sp->rule_month ? sp->rule_month : sp->month;
        if (sp->rule_n < 0) {
            if (sp->rule_offset) {
                snprintf(buf, (size_t)n, "last-%s-%s%+d", wd[sp->rule_wday],
                         mo[month], sp->rule_offset);
            } else {
                snprintf(buf, (size_t)n, "last-%s-%s", wd[sp->rule_wday],
                         mo[month]);
            }
        } else if (sp->rule_offset) {
            snprintf(buf, (size_t)n, "%d%s-%s%+d", sp->rule_n, wd[sp->rule_wday],
                     mo[month], sp->rule_offset);
        } else {
            snprintf(buf, (size_t)n, "%d%s-%s", sp->rule_n, wd[sp->rule_wday],
                     mo[month]);
        }
        return;
    }
    if (sp->rule == CAL_RULE_WDAY_ON_OR_AFTER) {
        if (sp->rule_offset) {
            snprintf(buf, (size_t)n, "%s>=%02d-%02d%+d", wd[sp->rule_wday],
                     sp->month, sp->day, sp->rule_offset);
        } else {
            snprintf(buf, (size_t)n, "%s>=%02d-%02d", wd[sp->rule_wday],
                     sp->month, sp->day);
        }
        return;
    }
    cal_format_date(a, sizeof(a), sp->year, sp->month, sp->day);
    if (sp->around) {
        snprintf(buf, (size_t)n, "~%s", a);
        return;
    }
    if (sp->is_range && sp->until_month) {
        cal_format_date(b, sizeof(b), sp->until_year, sp->until_month,
                        sp->until_day);
        snprintf(buf, (size_t)n, "%s~%s", a, b);
        return;
    }
    snprintf(buf, (size_t)n, "%s", a);
}
