make:
	gcc -o dialog dialog.c

run:
	./dialog 5

valgrind:
	valgrind ./dialog