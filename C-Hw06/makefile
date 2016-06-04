all: 
	gcc -c -ansi -pedantic-errors grepfromDirIPC.c -D_GNU_SOURCE
	gcc -o grepfromDirIPC grepfromDirIPC.o -lpthread
	rm grepfromDirIPC.o
clean:
	$(RM) grepfromDirIPC

