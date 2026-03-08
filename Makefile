CC ?= cc
CFLAGS ?= -Wall -Wextra -Wpedantic -Werror -Wshadow -Wparentheses -O3 -DNDEBUG -std=c17 -Isrc
LDFLAGS ?=

.PHONY	:
all	: encode decode

usage.o: src/usage.c src/usage.h
	$(CC) $(CFLAGS) -c -o $@ $<

encode.o: src/encode.c src/code.h src/crc16.h src/endian.h src/header.h src/huffman.h src/queue.h src/sizes.h src/usage.h
	$(CC) $(CFLAGS) -c -o $@ $<

decode.o: src/decode.c src/code.h src/crc16.h src/endian.h src/header.h src/huffman.h src/queue.h src/sizes.h src/stack.h
	$(CC) $(CFLAGS) -c -o $@ $<

huffman.o: src/huffman.c src/huffman.h
	$(CC) $(CFLAGS) -c -o $@ $<

priority.o: src/priority.c src/huffman.h src/queue.h
	$(CC) $(CFLAGS) -c -o $@ $<

stack.o: src/stack.c src/stack.h src/huffman.h
	$(CC) $(CFLAGS) -c -o $@ $<

encode	: usage.o encode.o huffman.o priority.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

decode	: usage.o decode.o huffman.o stack.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

format   :
	clang-format -i -style=file src/*.[ch]

fuzz    :
	python3 tests/fuzz_huffman.py --iterations 2000 --timeout 1.0

infer   :
	make clean; infer-capture -- make; infer-analyze -- make

clean	:
	rm -fr infer-out encode encode.o decode decode.o huffman.o priority.o stack.o usage.o
