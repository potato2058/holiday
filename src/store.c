#include "store.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

const char *kind_name(EntryKind k)
{
    switch (k) {
    case KIND_HOLIDAY:
        return "holiday";
    case KIND_EVENT:
        return "event";
    case KIND_BIRTHDAY:
        return "birthday";
    default:
        return "entry";
    }
}

EntryKind kind_from_name(const char *s)
{
    if (!s) {
        return KIND_EVENT;
    }
    if (strcasecmp(s, "holiday") == 0 || strcasecmp(s, "holidays") == 0) {
        return KIND_HOLIDAY;
    }
    if (strcasecmp(s, "birthday") == 0 || strcasecmp(s, "birthdays") == 0) {
        return KIND_BIRTHDAY;
    }
    return KIND_EVENT;
}

void store_init(Store *s)
{
    memset(s, 0, sizeof(*s));
}

void store_free(Store *s)
{
    free(s->items);
    s->items = NULL;
    s->count = s->cap = 0;
}

const char *store_find_path(char *buf, size_t n, const char *explicit_path)
{
    const char *env;
    if (explicit_path && explicit_path[0]) {
        snprintf(buf, n, "%s", explicit_path);
        return buf;
    }
    env = getenv("HOLIDAY_FILE");
    if (env && env[0]) {
        snprintf(buf, n, "%s", env);
        return buf;
    }
    if (access("dates.json", F_OK) == 0) {
        snprintf(buf, n, "dates.json");
        return buf;
    }
    if (access(".dates.json", F_OK) == 0) {
        snprintf(buf, n, ".dates.json");
        return buf;
    }
    snprintf(buf, n, "dates.json");
    return buf;
}

static int store_grow(Store *s)
{
    int ncap = s->cap ? s->cap * 2 : 64;
    Entry *n = realloc(s->items, (size_t)ncap * sizeof(Entry));
    if (!n) {
        snprintf(s->err, sizeof(s->err), "out of memory");
        return -1;
    }
    s->items = n;
    s->cap = ncap;
    return 0;
}

int store_is_hidden(const Store *s, const char *name)
{
    int i;
    if (!name || !name[0]) {
        return 0;
    }
    for (i = 0; i < s->hidden_count; i++) {
        if (strcasecmp(s->hidden[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

int store_hide(Store *s, const char *name)
{
    if (!name || !name[0] || store_is_hidden(s, name)) {
        return 0;
    }
    if (s->hidden_count >= STORE_HIDDEN_MAX) {
        snprintf(s->err, sizeof(s->err), "too many hidden names");
        return -1;
    }
    snprintf(s->hidden[s->hidden_count], ENTRY_NAME_MAX, "%s", name);
    s->hidden_count++;
    s->dirty = 1;
    return 0;
}

void store_unhide(Store *s, const char *name)
{
    int i;
    for (i = 0; i < s->hidden_count; i++) {
        if (strcasecmp(s->hidden[i], name) == 0) {
            memmove(s->hidden[i], s->hidden[i + 1],
                    (size_t)(s->hidden_count - i - 1) * ENTRY_NAME_MAX);
            s->hidden_count--;
            s->dirty = 1;
            return;
        }
    }
}

void store_claim(Store *s, int index)
{
    if (index < 0 || index >= s->count) {
        return;
    }
    if (s->items[index].builtin) {
        s->items[index].builtin = 0;
        s->dirty = 1;
    }
}

int store_add(Store *s, const Entry *e)
{
    if (s->count >= s->cap && store_grow(s) != 0) {
        return -1;
    }
    store_unhide(s, e->name);
    s->items[s->count++] = *e;
    s->dirty = 1;
    return s->count - 1;
}

int store_delete(Store *s, int index)
{
    if (index < 0 || index >= s->count) {
        return -1;
    }
    memmove(&s->items[index], &s->items[index + 1],
            (size_t)(s->count - index - 1) * sizeof(Entry));
    s->count--;
    s->dirty = 1;
    return 0;
}

int store_update(Store *s, int index, const Entry *e)
{
    if (index < 0 || index >= s->count) {
        return -1;
    }
    s->items[index] = *e;
    s->dirty = 1;
    return 0;
}

int entry_matches(const Entry *e, CalDate d)
{
    return cal_spec_matches(&e->when, d);
}

int store_on_day(const Store *s, CalDate d, int *idxs, int max)
{
    int i, n = 0;
    for (i = 0; i < s->count && n < max; i++) {
        if (entry_matches(&s->items[i], d)) {
            idxs[n++] = i;
        }
    }
    return n;
}

int store_upcoming(const Store *s, CalDate from, int days, int *idxs, CalDate *on,
                   int max)
{
    int off, n = 0, i;
    char seen[1024];
    int cap = s->count < 1024 ? s->count : 1024;
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < cap; i++) {
        if (entry_matches(&s->items[i], from)) {
            seen[i] = 1; /* already happening today */
        }
    }
    for (off = 1; off <= days && n < max; off++) {
        CalDate d = cal_add_days(from, off);
        for (i = 0; i < s->count && n < max; i++) {
            if (i < cap && seen[i]) {
                continue;
            }
            if (entry_matches(&s->items[i], d)) {
                idxs[n] = i;
                on[n] = d;
                n++;
                if (i < cap) {
                    seen[i] = 1;
                }
            }
        }
    }
    return n;
}

static int ascii_icontains(const char *hay, const char *needle)
{
    size_t n = strlen(needle);
    size_t h = strlen(hay);
    size_t i;
    if (n == 0) {
        return 1;
    }
    if (n > h) {
        return 0;
    }
    for (i = 0; i + n <= h; i++) {
        size_t j;
        for (j = 0; j < n; j++) {
            unsigned char a = (unsigned char)hay[i + j];
            unsigned char b = (unsigned char)needle[j];
            if (tolower(a) != tolower(b)) {
                break;
            }
        }
        if (j == n) {
            return 1;
        }
    }
    return 0;
}

int store_search(const Store *s, const char *q, int start, int *out_index,
                 CalDate *out_date, CalDate around)
{
    int i, n = s->count;
    if (n == 0 || !q || !q[0]) {
        return -1;
    }
    if (start < 0) {
        start = 0;
    }
    for (i = 0; i < n; i++) {
        int idx = (start + i) % n;
        const Entry *e = &s->items[idx];
        CalDate d;
        if (!ascii_icontains(e->name, q)) {
            continue;
        }
        if (cal_spec_next(&e->when, around, &d) != 0) {
            d = cal_resolve(&e->when, around.year);
        }
        *out_index = idx;
        *out_date = d;
        return 0;
    }
    return -1;
}

int store_prune_past(Store *s, CalDate today)
{
    int i, n = 0;
    for (i = 0; i < s->count; ) {
        if (s->items[i].builtin) {
            i++;
            continue;
        }
        if (cal_end_passed(&s->items[i].when, today)) {
            store_delete(s, i);
            n++;
            continue;
        }
        i++;
    }
    return n;
}

void entry_format_when(const Entry *e, int year, char *buf, int n)
{
    char spec[48];
    CalDate got;
    cal_format_spec(spec, sizeof(spec), &e->when);
    if (e->when.rule != CAL_RULE_NONE) {
        got = cal_resolve(&e->when, year);
        snprintf(buf, (size_t)n, "from %s %d · this year %s %d",
                 cal_month_abbrev(e->when.month ? e->when.month : got.month),
                 e->when.day ? e->when.day : got.day, cal_month_abbrev(got.month),
                 got.day);
        return;
    }
    if (e->when.around) {
        snprintf(buf, (size_t)n, "around %s %d", cal_month_abbrev(e->when.month),
                 e->when.day);
        return;
    }
    if (e->when.is_range) {
        snprintf(buf, (size_t)n, "from %s %d to %s %d",
                 cal_month_abbrev(e->when.month), e->when.day,
                 cal_month_abbrev(e->when.until_month), e->when.until_day);
        return;
    }
    if (e->when.year > 0) {
        snprintf(buf, (size_t)n, "%s (once)", spec);
        return;
    }
    snprintf(buf, (size_t)n, "every year");
    (void)spec;
}

/* ---------- JSON ---------- */

typedef struct {
    const char *p;
    const char *end;
    char *err;
    size_t errsz;
} Parser;

static void perr(Parser *ps, const char *msg)
{
    if (ps->err && ps->err[0] == '\0') {
        char around[24];
        size_t n = 0;
        const char *q = ps->p;
        while (q < ps->end && n + 1 < sizeof(around)) {
            unsigned char c = (unsigned char)*q++;
            around[n++] = (c < 32 || c > 126) ? '.' : (char)c;
        }
        around[n] = '\0';
        snprintf(ps->err, ps->errsz, "%s near '%s'", msg, around);
    }
}

static void skip_ws(Parser *ps)
{
    for (;;) {
        while (ps->p < ps->end && isspace((unsigned char)*ps->p)) {
            ps->p++;
        }
        if (ps->p + 1 < ps->end && ps->p[0] == '/' && ps->p[1] == '/') {
            ps->p += 2;
            while (ps->p < ps->end && *ps->p != '\n') {
                ps->p++;
            }
            continue;
        }
        if (ps->p + 1 < ps->end && ps->p[0] == '/' && ps->p[1] == '*') {
            ps->p += 2;
            while (ps->p + 1 < ps->end && !(ps->p[0] == '*' && ps->p[1] == '/')) {
                ps->p++;
            }
            if (ps->p + 1 < ps->end) {
                ps->p += 2;
            } else {
                ps->p = ps->end;
            }
            continue;
        }
        break;
    }
}

static int peek(Parser *ps)
{
    skip_ws(ps);
    if (ps->p >= ps->end) {
        return 0;
    }
    return (unsigned char)*ps->p;
}

static int eat(Parser *ps, char c)
{
    skip_ws(ps);
    if (ps->p >= ps->end || *ps->p != c) {
        return 0;
    }
    ps->p++;
    return 1;
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static int parse_string(Parser *ps, char *out, size_t outsz)
{
    size_t n = 0;
    skip_ws(ps);
    if (!eat(ps, '"')) {
        perr(ps, "expected string");
        return -1;
    }
    while (ps->p < ps->end) {
        unsigned char c = (unsigned char)*ps->p++;
        if (c == '"') {
            if (out && outsz) {
                out[n < outsz ? n : outsz - 1] = '\0';
            }
            return 0;
        }
        if (c == '\\') {
            if (ps->p >= ps->end) {
                perr(ps, "unterminated string escape");
                return -1;
            }
            c = (unsigned char)*ps->p++;
            switch (c) {
            case '"':
            case '\\':
            case '/':
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'u': {
                int i, cp = 0;
                for (i = 0; i < 4; i++) {
                    int h;
                    if (ps->p >= ps->end) {
                        perr(ps, "bad unicode escape");
                        return -1;
                    }
                    h = hexval((unsigned char)*ps->p++);
                    if (h < 0) {
                        perr(ps, "bad unicode escape");
                        return -1;
                    }
                    cp = (cp << 4) | h;
                }
                if (cp < 0x80) {
                    c = (unsigned char)cp;
                } else if (cp < 0x800) {
                    if (out && n + 2 < outsz) {
                        out[n++] = (char)(0xC0 | (cp >> 6));
                        out[n++] = (char)(0x80 | (cp & 0x3F));
                    } else {
                        n += 2;
                    }
                    continue;
                } else {
                    if (out && n + 3 < outsz) {
                        out[n++] = (char)(0xE0 | (cp >> 12));
                        out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        out[n++] = (char)(0x80 | (cp & 0x3F));
                    } else {
                        n += 3;
                    }
                    continue;
                }
                break;
            }
            default:
                perr(ps, "bad string escape");
                return -1;
            }
        }
        if (out && n + 1 < outsz) {
            out[n++] = (char)c;
        } else {
            n++;
        }
    }
    perr(ps, "unterminated string");
    return -1;
}

static int skip_value(Parser *ps);

static int skip_object(Parser *ps)
{
    if (!eat(ps, '{')) {
        return -1;
    }
    if (eat(ps, '}')) {
        return 0;
    }
    for (;;) {
        if (parse_string(ps, NULL, 0) != 0) {
            return -1;
        }
        if (!eat(ps, ':')) {
            perr(ps, "expected ':'");
            return -1;
        }
        if (skip_value(ps) != 0) {
            return -1;
        }
        if (eat(ps, ',')) {
            if (peek(ps) == '}') {
                eat(ps, '}');
                return 0;
            }
            continue;
        }
        if (eat(ps, '}')) {
            return 0;
        }
        perr(ps, "expected ',' or '}'");
        return -1;
    }
}

static int skip_array(Parser *ps)
{
    if (!eat(ps, '[')) {
        return -1;
    }
    if (eat(ps, ']')) {
        return 0;
    }
    for (;;) {
        if (skip_value(ps) != 0) {
            return -1;
        }
        if (eat(ps, ',')) {
            if (peek(ps) == ']') {
                eat(ps, ']');
                return 0;
            }
            continue;
        }
        if (eat(ps, ']')) {
            return 0;
        }
        perr(ps, "expected ',' or ']'");
        return -1;
    }
}

static int skip_value(Parser *ps)
{
    int c = peek(ps);
    if (c == '"') {
        return parse_string(ps, NULL, 0);
    }
    if (c == '{') {
        return skip_object(ps);
    }
    if (c == '[') {
        return skip_array(ps);
    }
    if (c == 't' || c == 'f' || c == 'n') {
        const char *w = (c == 't') ? "true" : (c == 'f') ? "false" : "null";
        size_t n = strlen(w);
        skip_ws(ps);
        if ((size_t)(ps->end - ps->p) < n || strncmp(ps->p, w, n) != 0) {
            perr(ps, "bad literal");
            return -1;
        }
        ps->p += n;
        return 0;
    }
    skip_ws(ps);
    if (ps->p < ps->end && (*ps->p == '-' || *ps->p == '+')) {
        ps->p++;
    }
    if (ps->p >= ps->end || !isdigit((unsigned char)*ps->p)) {
        perr(ps, "expected value");
        return -1;
    }
    while (ps->p < ps->end &&
           (isdigit((unsigned char)*ps->p) || *ps->p == '.' || *ps->p == 'e' ||
            *ps->p == 'E' || *ps->p == '+' || *ps->p == '-')) {
        ps->p++;
    }
    return 0;
}

static int parse_bool(Parser *ps, int *out)
{
    skip_ws(ps);
    if ((size_t)(ps->end - ps->p) >= 4 && strncmp(ps->p, "true", 4) == 0) {
        ps->p += 4;
        *out = 1;
        return 0;
    }
    if ((size_t)(ps->end - ps->p) >= 5 && strncmp(ps->p, "false", 5) == 0) {
        ps->p += 5;
        *out = 0;
        return 0;
    }
    perr(ps, "expected boolean");
    return -1;
}

static int parse_entry_object(Parser *ps, Entry *e)
{
    memset(e, 0, sizeof(*e));
    if (!eat(ps, '{')) {
        perr(ps, "expected object");
        return -1;
    }
    if (eat(ps, '}')) {
        return 0;
    }
    for (;;) {
        char key[64];
        if (parse_string(ps, key, sizeof(key)) != 0) {
            return -1;
        }
        if (!eat(ps, ':')) {
            perr(ps, "expected ':'");
            return -1;
        }
        if (strcmp(key, "name") == 0) {
            if (parse_string(ps, e->name, sizeof(e->name)) != 0) {
                return -1;
            }
        } else if (strcmp(key, "date") == 0 || strcmp(key, "rule") == 0) {
            char date[48];
            DateSpec spec;
            if (parse_string(ps, date, sizeof(date)) != 0) {
                return -1;
            }
            if (cal_parse_spec(date, &spec) == 0) {
                if (strcmp(key, "rule") == 0 && spec.rule != CAL_RULE_NONE) {
                    e->when.rule = spec.rule;
                    e->when.rule_n = spec.rule_n;
                    e->when.rule_wday = spec.rule_wday;
                    e->when.rule_month = spec.rule_month;
                    e->when.rule_offset = spec.rule_offset;
                    if (!e->when.month) {
                        e->when = spec;
                    } else {
                        e->when.rule = spec.rule;
                        e->when.rule_n = spec.rule_n;
                        e->when.rule_wday = spec.rule_wday;
                        e->when.rule_month = spec.rule_month;
                        e->when.rule_offset = spec.rule_offset;
                    }
                } else {
                    e->when = spec;
                }
            }
        } else if (strcmp(key, "until") == 0) {
            char date[32];
            int y, m, d;
            if (parse_string(ps, date, sizeof(date)) != 0) {
                return -1;
            }
            if (cal_parse_date(date, &y, &m, &d) == 0) {
                e->when.until_year = y;
                e->when.until_month = m;
                e->when.until_day = d;
                e->when.is_range = 1;
            }
        } else if (strcmp(key, "personal") == 0) {
            if (parse_bool(ps, &e->personal) != 0) {
                return -1;
            }
        } else if (strcmp(key, "around") == 0) {
            int v = 0;
            if (parse_bool(ps, &v) != 0) {
                return -1;
            }
            e->when.around = v;
            if (v && !e->when.around_days) {
                e->when.around_days = CAL_AROUND_DEFAULT;
            }
        } else {
            if (skip_value(ps) != 0) {
                return -1;
            }
        }
        if (eat(ps, ',')) {
            if (peek(ps) == '}') {
                eat(ps, '}');
                return 0;
            }
            continue;
        }
        if (eat(ps, '}')) {
            return 0;
        }
        perr(ps, "expected ',' or '}' in entry");
        return -1;
    }
}

static int parse_entry_array(Parser *ps, Store *s, EntryKind kind)
{
    if (!eat(ps, '[')) {
        perr(ps, "expected array");
        return -1;
    }
    if (eat(ps, ']')) {
        return 0;
    }
    for (;;) {
        Entry e;
        if (parse_entry_object(ps, &e) != 0) {
            return -1;
        }
        e.kind = kind;
        if (e.name[0] && (e.when.month >= 1 || e.when.rule != CAL_RULE_NONE)) {
            if (store_add(s, &e) < 0) {
                return -1;
            }
        }
        if (eat(ps, ',')) {
            if (peek(ps) == ']') {
                eat(ps, ']');
                return 0;
            }
            continue;
        }
        if (eat(ps, ']')) {
            return 0;
        }
        perr(ps, "expected ',' or ']' in array");
        return -1;
    }
}

static int parse_root(Parser *ps, Store *s)
{
    if (!eat(ps, '{')) {
        perr(ps, "expected top-level object");
        return -1;
    }
    if (eat(ps, '}')) {
        return 0;
    }
    for (;;) {
        char key[64];
        if (parse_string(ps, key, sizeof(key)) != 0) {
            return -1;
        }
        if (!eat(ps, ':')) {
            perr(ps, "expected ':'");
            return -1;
        }
        if (strcmp(key, "hidden") == 0) {
            if (!eat(ps, '[')) {
                perr(ps, "expected array");
                return -1;
            }
            if (!eat(ps, ']')) {
                for (;;) {
                    char name[ENTRY_NAME_MAX];
                    if (parse_string(ps, name, sizeof(name)) != 0) {
                        return -1;
                    }
                    store_hide(s, name);
                    if (eat(ps, ',')) {
                        if (peek(ps) == ']') {
                            eat(ps, ']');
                            break;
                        }
                        continue;
                    }
                    if (eat(ps, ']')) {
                        break;
                    }
                    perr(ps, "expected ',' or ']' in hidden");
                    return -1;
                }
            }
        } else if (strcmp(key, "holidays") == 0) {
            if (parse_entry_array(ps, s, KIND_HOLIDAY) != 0) {
                return -1;
            }
        } else if (strcmp(key, "events") == 0) {
            if (parse_entry_array(ps, s, KIND_EVENT) != 0) {
                return -1;
            }
        } else if (strcmp(key, "birthdays") == 0) {
            if (parse_entry_array(ps, s, KIND_BIRTHDAY) != 0) {
                return -1;
            }
        } else {
            if (skip_value(ps) != 0) {
                return -1;
            }
        }
        if (eat(ps, ',')) {
            if (peek(ps) == '}') {
                eat(ps, '}');
                break;
            }
            continue;
        }
        if (eat(ps, '}')) {
            break;
        }
        perr(ps, "expected ',' or '}' at top level");
        return -1;
    }
    skip_ws(ps);
    return 0;
}

static int cmp_entry(const void *a, const void *b)
{
    const Entry *ea = a, *eb = b;
    if (ea->kind != eb->kind) {
        return (int)ea->kind - (int)eb->kind;
    }
    if (ea->when.month != eb->when.month) {
        return ea->when.month - eb->when.month;
    }
    if (ea->when.day != eb->when.day) {
        return ea->when.day - eb->when.day;
    }
    if (ea->when.year != eb->when.year) {
        return ea->when.year - eb->when.year;
    }
    return strcasecmp(ea->name, eb->name);
}

int store_load(Store *s, const char *path)
{
    FILE *f;
    char *buf = NULL;
    long sz;
    Parser ps;
    char resolved[STORE_PATH_MAX];
    int had;

    store_free(s);
    store_init(s);

    if (!path) {
        store_find_path(resolved, sizeof(resolved), NULL);
        path = resolved;
    }
    snprintf(s->path, sizeof(s->path), "%s", path);

    f = fopen(path, "rb");
    if (!f) {
        if (errno == ENOENT) {
            return 0;
        }
        snprintf(s->err, sizeof(s->err), "cannot open file: %s",
                 strerror(errno));
        s->readonly = 1;
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0) {
        snprintf(s->err, sizeof(s->err), "cannot stat file");
        fclose(f);
        s->readonly = 1;
        return -1;
    }
    rewind(f);
    buf = malloc((size_t)sz + 1);
    if (!buf) {
        snprintf(s->err, sizeof(s->err), "out of memory");
        fclose(f);
        s->readonly = 1;
        return -1;
    }
    if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        snprintf(s->err, sizeof(s->err), "failed reading file");
        free(buf);
        fclose(f);
        s->readonly = 1;
        return -1;
    }
    buf[sz] = '\0';
    fclose(f);

    memset(&ps, 0, sizeof(ps));
    ps.p = buf;
    ps.end = buf + sz;
    ps.err = s->err;
    ps.errsz = sizeof(s->err);
    s->err[0] = '\0';

    had = parse_root(&ps, s);
    free(buf);
    if (had != 0) {
        if (!s->err[0]) {
            snprintf(s->err, sizeof(s->err), "invalid JSON");
        }
        s->readonly = 1;
        return -1;
    }
    s->dirty = 0;
    store_add_builtins(s);
    s->dirty = 0;
    if (store_prune_past(s, cal_today()) > 0) {
        s->dirty = 1;
    }
    return 0;
}

static void json_escape(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '"':
            fputs("\\\"", f);
            break;
        case '\\':
            fputs("\\\\", f);
            break;
        case '\n':
            fputs("\\n", f);
            break;
        case '\r':
            fputs("\\r", f);
            break;
        case '\t':
            fputs("\\t", f);
            break;
        default:
            if (c < 0x20) {
                fprintf(f, "\\u%04x", c);
            } else {
                fputc(c, f);
            }
        }
    }
    fputc('"', f);
}

static void write_kind(FILE *f, const Store *s, EntryKind kind, int *first_kind)
{
    int i, first = 1;
    if (!*first_kind) {
        fputs(",\n", f);
    }
    *first_kind = 0;
    fprintf(f, "  \"%s\": [", kind == KIND_HOLIDAY ? "holidays"
                                 : kind == KIND_BIRTHDAY ? "birthdays"
                                                         : "events");
    for (i = 0; i < s->count; i++) {
        const Entry *e = &s->items[i];
        char date[48];
        if (e->kind != kind || e->builtin) {
            continue;
        }
        cal_format_spec(date, sizeof(date), &e->when);
        fputs(first ? "\n    { \"name\": " : ",\n    { \"name\": ", f);
        json_escape(f, e->name);
        fputs(", \"date\": ", f);
        json_escape(f, date);
        if (e->personal) {
            fputs(", \"personal\": true", f);
        }
        fputs(" }", f);
        first = 0;
    }
    fputs(first ? "]" : "\n  ]", f);
}

int store_save(Store *s)
{
    char tmp[STORE_PATH_MAX + 8];
    FILE *f;
    int first = 1;
    Store sorted;
    int i;

    if (s->readonly) {
        if (!s->err[0]) {
            snprintf(s->err, sizeof(s->err), "file is read-only");
        }
        return -1;
    }
    if (!s->path[0]) {
        snprintf(s->err, sizeof(s->err), "no path to save");
        return -1;
    }

    memset(&sorted, 0, sizeof(sorted));
    sorted.items = malloc((size_t)s->count * sizeof(Entry) + 1);
    if (!sorted.items && s->count > 0) {
        snprintf(s->err, sizeof(s->err), "out of memory");
        return -1;
    }
    sorted.count = s->count;
    for (i = 0; i < s->count; i++) {
        sorted.items[i] = s->items[i];
    }
    if (sorted.count > 1) {
        qsort(sorted.items, (size_t)sorted.count, sizeof(Entry), cmp_entry);
    }

    snprintf(tmp, sizeof(tmp), "%s.tmp", s->path);
    f = fopen(tmp, "w");
    if (!f) {
        snprintf(s->err, sizeof(s->err), "cannot write temp file: %s",
                 strerror(errno));
        free(sorted.items);
        return -1;
    }
    fputs("{\n", f);
    if (s->hidden_count > 0) {
        int h;
        fputs("  \"hidden\": [", f);
        for (h = 0; h < s->hidden_count; h++) {
            fputs(h ? ", " : " ", f);
            json_escape(f, s->hidden[h]);
        }
        fputs(" ]", f);
        first = 0;
    }
    write_kind(f, &sorted, KIND_HOLIDAY, &first);
    write_kind(f, &sorted, KIND_EVENT, &first);
    write_kind(f, &sorted, KIND_BIRTHDAY, &first);
    fputs("\n}\n", f);
    if (ferror(f) || fclose(f) != 0) {
        snprintf(s->err, sizeof(s->err), "failed writing temp file");
        unlink(tmp);
        free(sorted.items);
        return -1;
    }
    if (rename(tmp, s->path) != 0) {
        snprintf(s->err, sizeof(s->err), "cannot replace file: %s",
                 strerror(errno));
        unlink(tmp);
        free(sorted.items);
        return -1;
    }
    free(sorted.items);
    s->dirty = 0;
    s->err[0] = '\0';
    return 0;
}

static int store_has_dup(const Store *s, const Entry *e)
{
    int i;
    for (i = 0; i < s->count; i++) {
        const Entry *o = &s->items[i];
        if (o->kind == e->kind && o->when.month == e->when.month &&
            o->when.day == e->when.day && o->when.year == e->when.year &&
            strcasecmp(o->name, e->name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void str_trim(char *s)
{
    char *a = s, *b;
    if (!s) {
        return;
    }
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

static void birthday_strip_suffix(char *name)
{
    size_t n, i;
    static const char *suf[] = {
        "'s birthday", "s birthday", " birthday", " (birthday)", "'s bday", NULL
    };
    str_trim(name);
    n = strlen(name);
    for (i = 0; suf[i]; i++) {
        size_t sl = strlen(suf[i]);
        if (n > sl && strcasecmp(name + n - sl, suf[i]) == 0) {
            name[n - sl] = '\0';
            str_trim(name);
            return;
        }
    }
}

static int add_imported(Store *s, EntryKind kind, const char *name, int y, int m,
                        int d)
{
    Entry e;
    memset(&e, 0, sizeof(e));
    e.kind = kind;
    snprintf(e.name, sizeof(e.name), "%s", name);
    str_trim(e.name);
    if (kind == KIND_BIRTHDAY) {
        birthday_strip_suffix(e.name);
        y = 0;
    }
    if (!e.name[0] || m < 1 || m > 12 || d < 1 || d > 31) {
        return 0;
    }
    e.when.year = y;
    e.when.month = m;
    e.when.day = d;
    e.personal = 1;
    if (store_has_dup(s, &e)) {
        return 0;
    }
    if (store_add(s, &e) < 0) {
        return -1;
    }
    return 1;
}

static int parse_csv_field(const char **pp, char *out, size_t outsz)
{
    const char *p = *pp;
    size_t n = 0;
    if (*p == '"') {
        p++;
        while (*p) {
            if (*p == '"' && p[1] == '"') {
                if (n + 1 < outsz) {
                    out[n++] = '"';
                }
                p += 2;
                continue;
            }
            if (*p == '"') {
                p++;
                break;
            }
            if (n + 1 < outsz) {
                out[n++] = *p;
            }
            p++;
        }
    } else {
        while (*p && *p != ',' && *p != '\n' && *p != '\r') {
            if (n + 1 < outsz) {
                out[n++] = *p;
            }
            p++;
        }
    }
    out[n] = '\0';
    if (*p == ',') {
        p++;
    }
    *pp = p;
    return 0;
}

static int import_csv(Store *s, const char *buf, EntryKind kind)
{
    const char *p = buf;
    int added = 0, line = 0;
    while (*p) {
        char name[ENTRY_NAME_MAX];
        char date[64];
        const char *row = p;
        int y = 0, m = 0, d = 0;
        line++;
        while (*p && *p != '\n') {
            p++;
        }
        if (*p == '\n') {
            p++;
        }
        parse_csv_field(&row, name, sizeof(name));
        parse_csv_field(&row, date, sizeof(date));
        str_trim(name);
        str_trim(date);
        if (!name[0] || !date[0]) {
            continue;
        }
        if (line == 1 && strcasecmp(name, "name") == 0) {
            continue;
        }
        if (cal_parse_date(date, &y, &m, &d) != 0) {
            continue;
        }
        {
            int n = add_imported(s, kind, name, y, m, d);
            if (n < 0) {
                return -1;
            }
            added += n;
        }
    }
    return added;
}

static void ics_unfold_inplace(char *s)
{
    char *r = s, *w = s;
    while (*r) {
        if ((r[0] == '\n' || r[0] == '\r') && (r[1] == ' ' || r[1] == '\t')) {
            r += 2;
            continue;
        }
        if (r[0] == '\r' && r[1] == '\n' && (r[2] == ' ' || r[2] == '\t')) {
            r += 3;
            continue;
        }
        *w++ = *r++;
    }
    *w = '\0';
}

static const char *ics_prop_value(const char *line)
{
    const char *p = strchr(line, ':');
    return p ? p + 1 : "";
}

static int ics_parse_dtstart(const char *val, int *y, int *m, int *d)
{
    char digits[16];
    int n = 0;
    const char *p = val;
    while (*p && n < 8) {
        if (isdigit((unsigned char)*p)) {
            digits[n++] = *p;
        }
        p++;
    }
    digits[n] = '\0';
    if (n < 8) {
        return -1;
    }
    *y = (digits[0] - '0') * 1000 + (digits[1] - '0') * 100 +
         (digits[2] - '0') * 10 + (digits[3] - '0');
    *m = (digits[4] - '0') * 10 + (digits[5] - '0');
    *d = (digits[6] - '0') * 10 + (digits[7] - '0');
    /* Facebook / Google birthday feeds often use a dummy year. */
    if (*y < 1970 || *y == 1604 || *y == 1900 || *y == 1970) {
        *y = 0;
    }
    if (*m < 1 || *m > 12 || *d < 1 || *d > 31) {
        return -1;
    }
    return 0;
}

static int import_ics(Store *s, char *buf, EntryKind kind)
{
    char *line, *next;
    char summary[ENTRY_NAME_MAX];
    int y = 0, m = 0, d = 0;
    int have_sum = 0, have_date = 0, in_event = 0;
    int added = 0;

    ics_unfold_inplace(buf);
    summary[0] = '\0';
    for (line = buf; line && *line; line = next) {
        next = strchr(line, '\n');
        if (next) {
            *next++ = '\0';
        }
        if (line[0] && line[strlen(line) - 1] == '\r') {
            line[strlen(line) - 1] = '\0';
        }
        if (!strncasecmp(line, "BEGIN:VEVENT", 12)) {
            in_event = 1;
            have_sum = have_date = 0;
            summary[0] = '\0';
            y = m = d = 0;
            continue;
        }
        if (!in_event) {
            continue;
        }
        if (!strncasecmp(line, "END:VEVENT", 10)) {
            if (have_sum && have_date) {
                int n = add_imported(s, kind, summary, y, m, d);
                if (n < 0) {
                    return -1;
                }
                added += n;
            }
            in_event = 0;
            continue;
        }
        if (!strncasecmp(line, "SUMMARY", 7) &&
            (line[7] == ';' || line[7] == ':')) {
            snprintf(summary, sizeof(summary), "%s", ics_prop_value(line));
            have_sum = summary[0] != '\0';
        } else if (!strncasecmp(line, "DTSTART", 7) &&
                   (line[7] == ';' || line[7] == ':')) {
            if (ics_parse_dtstart(ics_prop_value(line), &y, &m, &d) == 0) {
                have_date = 1;
            }
        }
    }
    return added;
}

int store_import_file(Store *s, const char *path, EntryKind kind)
{
    FILE *f;
    char *buf;
    long sz;
    int added;
    const char *ext;

    f = fopen(path, "rb");
    if (!f) {
        snprintf(s->err, sizeof(s->err), "cannot open import file: %s",
                 strerror(errno));
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || sz > 20 * 1024 * 1024) {
        snprintf(s->err, sizeof(s->err), "cannot read import file");
        fclose(f);
        return -1;
    }
    rewind(f);
    buf = malloc((size_t)sz + 1);
    if (!buf) {
        snprintf(s->err, sizeof(s->err), "out of memory");
        fclose(f);
        return -1;
    }
    if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        snprintf(s->err, sizeof(s->err), "failed reading import file");
        free(buf);
        fclose(f);
        return -1;
    }
    buf[sz] = '\0';
    fclose(f);

    ext = strrchr(path, '.');
    if ((ext && strcasecmp(ext, ".ics") == 0) || strstr(buf, "BEGIN:VCALENDAR") ||
        strstr(buf, "BEGIN:VEVENT")) {
        added = import_ics(s, buf, kind);
    } else {
        added = import_csv(s, buf, kind);
    }
    free(buf);
    return added;
}
