all: 
	gcc -c -ansi -pedantic-errors fileShareServer.c -D_GNU_SOURCE
	gcc -o fileShareServer fileShareServer.o -lpthread
	rm fileShareServer.o
clean:
	$(RM) fileShareServer

