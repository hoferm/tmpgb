CC = cc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -DDEBUG -g -O2
CFLAGS += -Werror \
	-Wstrict-prototypes \
	-Wdeclaration-after-statement \
	-Wno-format-zero-length \
	-Wold-style-definition \
	-Wvla
LDFLAGS = -lSDL2
BUILDDIR = obj

SRC = $(wildcard *.c)

$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

tmpgb: $(SRC:%.c=$(BUILDDIR)/%.o)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(BUILDDIR):
	mkdir $(BUILDDIR)

clean:
	rm tmpgb $(BUILDDIR)/*.o

.PHONY:
	clean all
