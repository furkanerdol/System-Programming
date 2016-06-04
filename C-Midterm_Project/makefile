all: 
	gcc -c -ansi -pedantic-errors IntegralGen.c -D_POSIX_C_SOURCE
	gcc -o IntegralGen IntegralGen.o -lm
	rm IntegralGen.o
	
	gcc -c -ansi -pedantic-errors Client.c -D_POSIX_C_SOURCE
	gcc -o Client Client.o
	rm Client.o
clean:
	$(RM) IntegralGen
	$(RM) Client
