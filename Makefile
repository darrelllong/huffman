CC=cc
CFLAGS=-Wall -Wextra -Wpedantic -Werror -Wshadow -Wparentheses -Oz -std=c17
LDFLAGS=

.PHONY	:
all	: encode decode

encode	: usage.o encode.o huffman.o priority.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

decode	: usage.o decode.o huffman.o stack.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

format   :
	clang-format -i -style=file *.[ch]

fuzz    :
	python3 fuzz_huffman.py --iterations 2000 --timeout 1.0

infer   :
	make clean; infer-capture -- make; infer-analyze -- make

clean	:
	rm -fr infer-out encode encode.o decode decode.o huffman.o priority.o stack.o usage.o
