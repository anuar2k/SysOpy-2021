CC       := gcc
CFLAGS   := -g2 -Wall
SRC      := $(wildcard *.c)
OUT_BINS := $(SRC:%.c=%)

.PHONY: all test clean

all: $(OUT_BINS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

test:
	./runner 5 2 A B C
	
clean:
	$(RM) $(OUT_BINS)
