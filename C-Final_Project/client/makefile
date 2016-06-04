all: 
	gcc -c -ansi -pedantic-errors client.c -D_GNU_SOURCE
	gcc -o client2 client.o -lpthread
	rm client.o
clean:
	$(RM) client

