#include "store.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

static int has_name(const Store *s, const char *name)
{
    int i;
    for (i = 0; i < s->count; i++) {
        if (strcasecmp(s->items[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void add(Store *s, EntryKind k, const char *name, const char *spec)
{
    Entry e;
    if (has_name(s, name) || store_is_hidden(s, name)) {
        return;
    }
    memset(&e, 0, sizeof(e));
    e.kind = k;
    snprintf(e.name, sizeof(e.name), "%s", name);
    if (cal_parse_spec(spec, &e.when) != 0) {
        return;
    }
    e.builtin = 1;
    e.personal = 0;
    store_add(s, &e);
}

void store_add_builtins(Store *s)
{
    /* Christian year only. Delete any of these in the app to hide them. */
    add(s, KIND_HOLIDAY, "Solemnity of Mary, Mother of God", "01-01");
    add(s, KIND_HOLIDAY, "New Year's Day", "01-01");
    add(s, KIND_HOLIDAY, "Twelfth Night", "01-05");
    add(s, KIND_HOLIDAY, "Epiphany", "01-06");
    add(s, KIND_HOLIDAY, "Baptism of the Lord", "sun>=01-07");
    add(s, KIND_HOLIDAY, "Orthodox Christmas", "01-07");
    add(s, KIND_HOLIDAY, "Conversion of St. Paul", "01-25");
    add(s, KIND_HOLIDAY, "Candlemas / Presentation of the Lord", "02-02");
    add(s, KIND_HOLIDAY, "St. Valentine's Day", "02-14");
    add(s, KIND_HOLIDAY, "Chair of St. Peter", "02-22");
    add(s, KIND_HOLIDAY, "St. David's Day", "03-01");
    add(s, KIND_HOLIDAY, "St. Patrick's Day", "03-17");
    add(s, KIND_HOLIDAY, "St. Joseph's Day", "03-19");
    add(s, KIND_HOLIDAY, "Annunciation", "03-25");
    add(s, KIND_HOLIDAY, "St. George's Day", "04-23");
    add(s, KIND_HOLIDAY, "St. Mark", "04-25");
    add(s, KIND_HOLIDAY, "SS. Philip and James", "05-03");
    add(s, KIND_HOLIDAY, "Visitation of Mary", "05-31");
    add(s, KIND_HOLIDAY, "Nativity of John the Baptist", "06-24");
    add(s, KIND_HOLIDAY, "Saints Peter and Paul", "06-29");
    add(s, KIND_HOLIDAY, "Transfiguration", "08-06");
    add(s, KIND_HOLIDAY, "St. Lawrence", "08-10");
    add(s, KIND_HOLIDAY, "Assumption of Mary", "08-15");
    add(s, KIND_HOLIDAY, "Beheading of John the Baptist", "08-29");
    add(s, KIND_HOLIDAY, "Nativity of Mary", "09-08");
    add(s, KIND_HOLIDAY, "Holy Cross Day", "09-14");
    add(s, KIND_HOLIDAY, "St. Matthew", "09-21");
    add(s, KIND_HOLIDAY, "Michaelmas", "09-29");
    add(s, KIND_HOLIDAY, "St. Francis of Assisi", "10-04");
    add(s, KIND_HOLIDAY, "St. Luke", "10-18");
    add(s, KIND_HOLIDAY, "SS. Simon and Jude", "10-28");
    add(s, KIND_HOLIDAY, "All Hallows' Eve", "10-31");
    add(s, KIND_HOLIDAY, "Reformation Day", "10-31");
    add(s, KIND_HOLIDAY, "All Saints' Day", "11-01");
    add(s, KIND_HOLIDAY, "All Souls' Day", "11-02");
    add(s, KIND_HOLIDAY, "Martinmas", "11-11");
    add(s, KIND_HOLIDAY, "Presentation of Mary", "11-21");
    add(s, KIND_HOLIDAY, "St. Andrew's Day", "11-30");
    add(s, KIND_HOLIDAY, "Immaculate Conception", "12-08");
    add(s, KIND_HOLIDAY, "Our Lady of Guadalupe", "12-12");
    add(s, KIND_HOLIDAY, "St. Lucy's Day", "12-13");
    add(s, KIND_HOLIDAY, "Christmas Eve", "12-24");
    add(s, KIND_HOLIDAY, "Christmas Day", "12-25");
    add(s, KIND_HOLIDAY, "Saint Stephen's Day", "12-26");
    add(s, KIND_HOLIDAY, "St. John the Evangelist", "12-27");
    add(s, KIND_HOLIDAY, "Holy Innocents", "12-28");
    add(s, KIND_HOLIDAY, "New Year's Eve", "12-31");

    add(s, KIND_HOLIDAY, "Shrove Tuesday", "easter-47");
    add(s, KIND_HOLIDAY, "Ash Wednesday", "easter-46");
    add(s, KIND_HOLIDAY, "First Sunday of Lent", "easter-42");
    add(s, KIND_HOLIDAY, "Laetare Sunday", "easter-21");
    add(s, KIND_HOLIDAY, "Palm Sunday", "easter-7");
    add(s, KIND_HOLIDAY, "Maundy Thursday", "easter-3");
    add(s, KIND_HOLIDAY, "Good Friday", "easter-2");
    add(s, KIND_HOLIDAY, "Holy Saturday", "easter-1");
    add(s, KIND_HOLIDAY, "Easter Sunday", "easter");
    add(s, KIND_HOLIDAY, "Easter Monday", "easter+1");
    add(s, KIND_HOLIDAY, "Divine Mercy Sunday", "easter+7");
    add(s, KIND_HOLIDAY, "Ascension of the Lord", "easter+39");
    add(s, KIND_HOLIDAY, "Pentecost", "easter+49");
    add(s, KIND_HOLIDAY, "Trinity Sunday", "easter+56");
    add(s, KIND_HOLIDAY, "Corpus Christi", "easter+60");
    add(s, KIND_HOLIDAY, "Sacred Heart of Jesus", "easter+68");
    add(s, KIND_HOLIDAY, "Christ the King", "sun>=11-27-7");
    add(s, KIND_HOLIDAY, "First Sunday of Advent", "sun>=11-27");
    add(s, KIND_HOLIDAY, "Gaudete Sunday", "sun>=11-27+14");
}

int store_user_count(const Store *s)
{
    int i, n = 0;
    for (i = 0; i < s->count; i++) {
        if (!s->items[i].builtin) {
            n++;
        }
    }
    return n;
}
