CC=gcc
CFLAGS=-std=c11 -g -static -Wall
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

kcc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

debug: CFLAGS += -DDEBUG
debug: clean $(OBJS)
	$(CC) $(CFLAGS) -o kcc $(OBJS) $(LDFLAGS)

$(OBJS): kcc.h

test: kcc
	./test.sh

clean:
	rm -f kcc *.o *~ tmp*

.PHONY: debug test clean
