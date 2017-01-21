CC = gcc
CFLAGS = -Wall -std=c99 -pedantic -DDEBUG
LDFLAGS =
DEPDIR = .d
BUILDDIR = obj
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d

SRC = $(wildcard *.c)

%.o: %.c
$(BUILDDIR)/%.o: %.c $(DEPDIR)/%.d
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

tmpgb: $(SRC:%.c=$(BUILDDIR)/%.o)
	$(CC) $(CFLAGS) $(DEPFLAGS) -o $@ $^

-include $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRC)))

clean:
	rm tmpgb $(BUILDDIR)/*.o $(DEPDIR)/*.d

.PHONY:
	clean all
