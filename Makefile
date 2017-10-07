CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -DDEBUG -g -O2
LDFLAGS = -lSDL2
BUILDDIR = obj

SRC = $(wildcard *.c)

$(BUILDDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tmpgb: $(SRC:%.c=$(BUILDDIR)/%.o)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm tmpgb $(BUILDDIR)/*.o

.PHONY:
	clean all
