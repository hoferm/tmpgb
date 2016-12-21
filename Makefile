CC = gcc
CFLAGS = -Wall -std=c99 -pedantic -MMD -MP
LD =

SRC = $(wildcard *.c)

tmpgb: $(SRC:%.c=%.o)
	$(LD) -o $@ $^

-include $(SRC:%.c=%.d)

clean:
	rm $(OBJ_FILE)

.PHONY:
	clean all
