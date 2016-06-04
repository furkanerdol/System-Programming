all: 
	gcc -c -ansi -pedantic-errors grepfromDirSemaphore.c -D_GNU_SOURCE
	gcc -o grepfromDirSemaphore grepfromDirSemaphore.o -lpthread
	rm grepfromDirSemaphore.o
clean:
	$(RM) grepfromDirTh

