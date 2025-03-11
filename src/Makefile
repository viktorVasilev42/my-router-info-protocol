all: link-peer-listen

link-peer-listen: compile-peer-listen compile-first
	clang -o peer-listen.out peer-listen.o first.o

compile-peer-listen:
	clang -c peer-listen.c

compile-first:
	clang -c first.c

run: all
	./peer-listen.out 1
