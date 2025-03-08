peer-listen: link-peer-listen
	./peer-listen.out 1

link-peer-listen: compile-peer-listen
	clang -o peer-listen.out peer-listen.o first.o

compile-peer-listen:
	clang -c peer-listen.c
