#ifndef HOLIDAY_CAL_H
#define HOLIDAY_CAL_H

#include <time.h>

typedef struct {
    int year;  /* full year, e.g. 2026 */
    int month; /* 1-12 */
    int day;   /* 1-31 */
} CalDate;

int cal_is_leap(int year);
int cal_days_in_month(int year, int month);
/* 0 = Sunday ... 6 = Saturday */
int cal_weekday(int year, int month, int day);
int cal_valid(CalDate d);
CalDate cal_today(void);
CalDate cal_add_days(CalDate d, int n);
int cal_cmp(CalDate a, CalDate b);

const char *cal_month_name(int month);
const char *cal_month_abbrev(int month);
const char *cal_wday_name(int wday);
const char *cal_wday_abbrev(int wday);

/* Accepts MM-DD (year out = 0) or YYYY-MM-DD. Returns 0 on success. */
int cal_parse_date(const char *s, int *year, int *month, int *day);
void cal_format_date(char *buf, int n, int year, int month, int day);
void cal_format_pretty(char *buf, int n, CalDate d);

#define CAL_AROUND_DEFAULT 3

typedef enum {
    CAL_RULE_NONE = 0,
    CAL_RULE_NTH_WDAY,           /* nth weekday of month; nth=-1 last */
    CAL_RULE_EASTER,             /* Western Easter Sunday + offset */
    CAL_RULE_WDAY_ON_OR_AFTER    /* first wday on/after month-day + offset */
} CalRule;

typedef struct {
    int year, month, day;                 /* exact, earliest, or center */
    int until_year, until_month, until_day;
    int around;                           /* ~date */
    int around_days;
    int is_range;                         /* date~date */
    CalRule rule;
    int rule_n;                           /* nth (or unused) */
    int rule_wday;                        /* 0=Sun .. 6=Sat */
    int rule_month;
    int rule_offset;                      /* extra days after the anchor */
} DateSpec;

int cal_parse_spec(const char *s, DateSpec *out);
void cal_format_spec(char *buf, int n, const DateSpec *sp);

/* nth: 1-5, or -1 for last. wday 0=Sun. */
CalDate cal_nth_weekday(int year, int month, int wday, int nth);
CalDate cal_easter(int year); /* Western / Gregorian */
CalDate cal_wday_on_or_after(int year, int month, int day, int wday);
CalDate cal_resolve(const DateSpec *sp, int year);
int cal_spec_matches(const DateSpec *sp, CalDate d);
/* First matching day strictly after `from`. Returns 0 if none (one-off past). */
int cal_spec_next(const DateSpec *sp, CalDate from, CalDate *out);
int cal_end_passed(const DateSpec *sp, CalDate today);

#endif
