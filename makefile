all:
	gcc -g -c fsck.c -Wall
	gcc -g -o myfsck fsck.o -Wall
clean:
	rm -f fsck.o
