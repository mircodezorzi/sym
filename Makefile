CC := clang
CFLAGS := -std=c99 -pedantic -Wno-everything #-Wall

HDRS :=
SRCS := sym.c

OBJS := $(SRCS:.c=.o)
EXEC := sym
DESTDIR := /usr/local
PREFIX :=
TERMINAL := st
all: $(EXEC)

$(EXEC): $(OBJS) $(HDRS) Makefile
	$(CC) -o $@ $(OBJS) $(CFLAGS)

install: $(EXEC)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $< $(DESTDIR)$(PREFIX)bin/$(EXEC)

run: $(EXEC)
	./$(EXEC)

.PHONY: clean

clean:
	rm -f $(EXEC) $(OBJS)

