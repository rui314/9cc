CFLAGS=-Wall -std=c11 -g
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

9cc: $(OBJS)
	cc -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): 9cc.h

test: 9cc test/test.c
	./9cc -test

	@gcc -E -P test/test.c > tmp-test.tmp
	@./9cc tmp-test.tmp > tmp-test.s
	@echo 'int global_arr[1] = {5};' | gcc -xc -c -o tmp-test2.o -
	@gcc -static -o tmp-test tmp-test.s tmp-test2.o
	@./tmp-test

slow-tests: 9cc test
	./test/test.sh

clean:
	rm -f 9cc *.o *~ tmp* a.out test/*~ test/exec/*.{gccbin,bin,expected,s,output,diff}

.PHONY: test slow-tests clean
