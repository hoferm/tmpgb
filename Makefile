CC = gcc
CFLAGS = -Wall -std=c99 -pedantic -MD -MP

SRC = $(wildcard *.c)

all: $(SRC:%.c=%.o)
	$(LD) -o $@ $^

-include $(SRC:%.c=%.d)

clean:
	rm $(OBJ_FILE)

.PHONY:
	clean all
