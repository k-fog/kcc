CC=gcc
CFLAGS=-std=c11 -g -static -Wall
SRCS=codegen.c lexer.c main.c parser.c preprocessor.c type.c
OBJS=$(SRCS:.c=.o)

kcc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

kcc2: kcc
	./kcc codegen.c > codegen2.s
	./kcc lexer.c > lexer2.s
	./kcc main.c > main2.s
	./kcc parser.c > parser2.s
	./kcc preprocessor.c > preprocessor2.s
	./kcc type.c > type2.s
	$(CC) -o $@ codegen2.s lexer2.s main2.s parser2.s preprocessor2.s type2.s

kcc3: kcc2
	./kcc2 codegen.c > codegen3.s
	./kcc2 lexer.c > lexer3.s
	./kcc2 main.c > main3.s
	./kcc2 parser.c > parser3.s
	./kcc2 preprocessor.c > preprocessor3.s
	./kcc2 type.c > type3.s
	$(CC) -o $@ codegen3.s lexer3.s main3.s parser3.s preprocessor3.s type3.s
	@echo "===== check diff ====="
	diff codegen2.s codegen3.s
	diff lexer2.s lexer3.s
	diff main2.s main3.s
	diff parser2.s parser3.s
	diff preprocessor2.s preprocessor3.s
	diff type2.s type3.s
	@echo "===== checked ====="

debug: CFLAGS += -DDEBUG
debug: clean $(OBJS)
	$(CC) $(CFLAGS) -o kcc $(OBJS) $(LDFLAGS)

$(OBJS): kcc.h

test: kcc
	./test.sh kcc

test2: kcc2
	./test.sh kcc2

test3: kcc3
	./test.sh kcc3

clean:
	rm -f kcc kcc2 kcc3 *.o *~ tmp* *.s

.PHONY: debug test test2 test3 clean
