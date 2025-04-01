all: link-peer-listen

link-peer-listen: compile-peer-listen compile-host compile-first
	clang -o peer-listen.out peer-listen.o host.o first.o

compile-peer-listen:
	clang -c peer-listen.c

compile-host:
	clang -c host.c

compile-first:
	clang -c first.c

run: all
	./peer-listen.out host 1
