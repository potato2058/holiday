#ifndef HOLIDAY_STORE_H
#define HOLIDAY_STORE_H

#include "cal.h"

#define ENTRY_NAME_MAX 192
#define STORE_PATH_MAX 1024
#define STORE_HIDDEN_MAX 128

typedef enum {
    KIND_HOLIDAY = 0,
    KIND_EVENT,
    KIND_BIRTHDAY,
    KIND_COUNT
} EntryKind;

typedef struct {
    EntryKind kind;
    char name[ENTRY_NAME_MAX];
    DateSpec when;
    int personal;
    int builtin; /* computed; not written to JSON */
} Entry;

typedef struct {
    Entry *items;
    int count;
    int cap;
    char path[STORE_PATH_MAX];
    char hidden[STORE_HIDDEN_MAX][ENTRY_NAME_MAX];
    int hidden_count;
    int dirty;
    int readonly;
    char err[256];
} Store;

const char *kind_name(EntryKind k);
EntryKind kind_from_name(const char *s);

void store_init(Store *s);
void store_free(Store *s);

/* Resolve which JSON file to use. explicit_path may be NULL. */
const char *store_find_path(char *buf, size_t n, const char *explicit_path);

/* path NULL = store_find_path. Missing file => empty store, path filled. */
int store_load(Store *s, const char *path);
int store_save(Store *s);

int store_add(Store *s, const Entry *e);
int store_delete(Store *s, int index);
int store_update(Store *s, int index, const Entry *e);
int store_is_hidden(const Store *s, const char *name);
int store_hide(Store *s, const char *name);
void store_unhide(Store *s, const char *name);
void store_claim(Store *s, int index); /* take a built-in so edits persist */

int entry_matches(const Entry *e, CalDate d);
int store_on_day(const Store *s, CalDate d, int *idxs, int max);
int store_upcoming(const Store *s, CalDate from, int days, int *idxs, CalDate *on,
                   int max);
int store_search(const Store *s, const char *q, int start, int *out_index,
                 CalDate *out_date, CalDate around);

/* Drop one-off entries whose date (or range end) is before today.
 * Yearly / rule / approx entries are kept. Returns how many were removed. */
int store_prune_past(Store *s, CalDate today);
void store_add_builtins(Store *s);
int store_user_count(const Store *s);

void entry_format_when(const Entry *e, int year, char *buf, int n);

/* Local CSV or ICS (no network). Birthdays are stored as yearly MM-DD.
 * Returns number of new entries, or -1 on error (s->err set). */
int store_import_file(Store *s, const char *path, EntryKind kind);

#endif
