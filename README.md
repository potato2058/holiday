# holiday

A small calendar TUI in C. You should almost never need to touch the JSON file.

```
make
./holiday
```

Needs ncursesw (`ncurses-devel` on Fedora, `libncurses-dev` on Debian).

## What you actually edit

Add/edit/delete in the app (`a` `e` `d`). Built-in days are the Christian
year (Christmas, Easter, saints' days, …). **`e` and `d` work on those too** —
remove a feast and it stays gone (`hidden` in `dates.json`). Rename or change
its date with `e`; that override is saved as one of your entries.

One-off dates (`YYYY-MM-DD`) are **removed automatically** the next time the
app starts after that day has passed.

`m` marks an entry as **personal** (green `*`). Imports and anything you add
are personal by default. Built-ins cannot be marked; add your own copy if you
want a personal overlay.

## Date forms (type these in the add dialog)

| what you type | meaning |
|---------------|---------|
| `03-14` | every year on that day |
| `2026-09-18` | once; dropped after it passes |
| `~11-25` | around that day (±3) |
| `11-22~11-28` | earliest..latest each year |
| `4thu-nov` | 4th Thursday of November (Thanksgiving) |
| `last-mon-may` | last Monday of May (Memorial Day) |
| `1mon-sep` | first Monday of September (Labor Day) |
| `easter` | Western Easter Sunday |
| `easter-46` | days relative to Easter (Ash Wednesday) |
| `sun>=11-27` | first Sunday on or after that date (Advent) |

Thanksgiving is `4thu-nov`: the JSON/human form still records the earliest
possible day (Nov 22) through Nov 28, and the calendar lights up the real
Thursday for the year you are looking at.

## Keys

| key | action |
|-----|--------|
| `h j k l` / arrows | move day |
| `[ ]` or `n` | next month (`p` previous — `p` is month, `m` is personal) |
| `{ }` | year |
| `t` | today |
| `g` | go to a date |
| `/` | search |
| `a` / Enter | add (personal by default) |
| `e` | edit name or date (including built-ins) |
| `d` | remove from the calendar (including built-ins) |
| `m` | mark / unmark personal |
| `s` | save |
| `r` | reload |
| `?` | help |
| `q` | quit |

## JSON (optional)

Default path: `dates.json`, then `.dates.json`. Override with `-f` or
`HOLIDAY_FILE`.

```json
{
  "holidays": [],
  "events": [
    { "name": "Dentist", "date": "2026-09-18", "personal": true }
  ],
  "birthdays": [
    { "name": "John Doe", "date": "01-05", "personal": true }
  ]
}
```

`//` comments are allowed when reading. Saves write only your entries.

## CLI

```
./holiday
./holiday --today
./holiday --list
./holiday --cal 2026-11
./holiday --import friends.ics
```

## Facebook birthdays

No API. Export what you can already see on
[facebook.com/events/birthdays](https://www.facebook.com/events/birthdays/)
to `.ics`/`.csv`, then `./holiday --import that-file`.
