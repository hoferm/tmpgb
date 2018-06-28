CC = cc
RM = rm -f
CFLAGS = -Wall -Wextra -std=c99 -pedantic -g
CFLAGS += -Werror \
	-Wstrict-prototypes \
	-Wdeclaration-after-statement \
	-Wno-format-zero-length \
	-Wold-style-definition \
	-Wvla
LDFLAGS = -lSDL2
BUILDDIR = obj

QUIET_CC = @echo '   ' CC $@;
QUIET_LINK = @echo '   ' LINK $@;

SRC = $(wildcard *.c)

$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(QUIET_CC)$(CC) $(CFLAGS) -c $< -o $@

tmpgb: $(SRC:%.c=$(BUILDDIR)/%.o)
	$(QUIET_LINK)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(BUILDDIR):
	mkdir $@

clean:
	$(RM) tmpgb $(BUILDDIR)/*.o

.PHONY: clean all
