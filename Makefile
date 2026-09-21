CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700
LDLIBS += $(shell pkg-config --libs ncursesw 2>/dev/null || echo -lncursesw)

SRC = src/main.c src/cal.c src/store.c src/tui.c src/builtin.c
OBJ = $(SRC:.c=.o)
BIN = holiday

.PHONY: all clean test

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

src/main.o: src/main.c src/cal.h src/store.h src/tui.h
src/cal.o: src/cal.c src/cal.h
src/store.o: src/store.c src/store.h src/cal.h
src/tui.o: src/tui.c src/tui.h src/store.h src/cal.h
src/builtin.o: src/builtin.c src/store.h src/cal.h

clean:
	rm -f $(OBJ) $(BIN)

test: $(BIN)
	./$(BIN) --self-test
	./$(BIN) --today
	./$(BIN) --cal 2026-09
	./$(BIN) --list
