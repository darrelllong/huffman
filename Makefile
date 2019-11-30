CC=cc
CFLAGS=-Wall -Wextra -Wpedantic -Werror -Ofast -std=c11

.PHONY	:
all	: encode decode

encode	: encode.o huffman.o priority.o

decode	: decode.o huffman.o stack.o

infer   :
	make clean; infer-capture -- make; infer-analyze -- make

clean	:
	rm -f encode encode.o decode decode.o huffman.o priority.o stack.o
