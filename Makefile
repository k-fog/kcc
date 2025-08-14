CC=gcc
CFLAGS=-std=c11 -g -static -Wall
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

kcc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

kcc2: kcc
	./kcc codegen.c > codegen2.s
	$(CC) -S -o - lexer.c > lexer2.s
	$(CC) -S -o - main.c > main2.s
	$(CC) -S -o - parser.c > parser2.s
	./kcc preprocessor.c > preprocessor2.s
	./kcc type.c > type2.s
	$(CC) -o $@ codegen2.s lexer2.s main2.s parser2.s preprocessor2.s type2.s

debug: CFLAGS += -DDEBUG
debug: clean $(OBJS)
	$(CC) $(CFLAGS) -o kcc $(OBJS) $(LDFLAGS)

$(OBJS): kcc.h

test: kcc2
	./test.sh

clean:
	rm -f kcc kcc2 *.o *~ tmp* *.s

.PHONY: debug test clean
