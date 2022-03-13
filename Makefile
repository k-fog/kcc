CFLAGS=-std=c11 -g -static
CC=cc

kcc: kcc.c

test: kcc
	./test.sh

clean:
	rm -f kcc *.o *~ tmp*

.PHONY: test clean
