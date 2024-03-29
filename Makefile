CC=cc
CFLAGS=-Wall -Wextra -Wpedantic -Werror -Wshadow -Wparentheses -Oz -std=c17

.PHONY	:
all	: encode decode

encode	: usage.o encode.o huffman.o priority.o

decode	: usage.o decode.o huffman.o stack.o

format   :
	clang-format -i -style=file *.[ch]

infer   :
	make clean; infer-capture -- make; infer-analyze -- make

clean	:
	rm -fr infer-out encode encode.o decode decode.o huffman.o priority.o stack.o
