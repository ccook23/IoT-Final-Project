CC=gcc
CFLAGS=-Wall -g

requestor: requestor.c
	$(CC) $(CFLAGS) -o requestor requestor.c

clean:
	rm -f requestor *.o
