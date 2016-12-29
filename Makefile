CC = gcc
CFLAGS = -Wall -std=c99 -pedantic -MMD -MP -DDEBUG
LD =

SRC = $(wildcard *.c)

tmpgb: $(SRC:%.c=%.o)
	$(CC) $(CFLAGS) -o $@ $^

-include $(SRC:%.c=%.d)

clean:
	rm tmpgb

.PHONY:
	clean all
