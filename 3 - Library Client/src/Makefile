# Stroe Teodora, 321CA

CC=gcc
CFLAGS=-I.

run: client
	./client

client: client.c requests.c helpers.c buffer.c parson.c
	$(CC) -o client client.c requests.c helpers.c buffer.c parson.c -Wall

clean:
	rm -f *.o client
