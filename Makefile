CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -DDEBUG -g
LDFLAGS = -Llib -lSDL2
DEPDIR = .d
BUILDDIR = obj
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d

SRC = $(wildcard *.c)
HDR = $(wildcard *.h)

%.o: %.c
$(BUILDDIR)/%.o: %.c $(DEPDIR)/%.d
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

tmpgb: $(SRC:%.c=$(BUILDDIR)/%.o)
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEPFLAGS) -o $@ $^

-include $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRC)))

clean:
	rm tmpgb $(BUILDDIR)/*.o $(DEPDIR)/*.d

.PHONY:
	clean all
