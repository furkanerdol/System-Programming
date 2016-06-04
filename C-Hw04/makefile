all: 
	gcc -c -ansi -pedantic-errors grepfromDirTh.c -D_POSIX_C_SOURCE
	gcc -o grepfromDirTh grepfromDirTh.o -lpthread
	rm grepfromDirTh.o
clean:
	$(RM) grepfromDirTh

