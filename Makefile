CC := clang
CFLAGS := -std=c99 -pedantic -Ofast -Wall

HDRS :=
SRCS := sym.c

OBJS := $(SRCS:.c=.o)
EXEC := sym
all: $(EXEC)

$(EXEC): $(OBJS) $(HDRS) Makefile
	$(CC) -o $@ $(OBJS) $(CFLAGS)

clean:
	rm -f $(EXEC) $(OBJS)

.PHONY: all clean
