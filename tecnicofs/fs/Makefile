CFLAGS = -g -Wall -std=c11

<<<<<<< HEAD
operation: operations.o state.o
	gcc $(CFLAGS) operations.o state.o -o operation

operations.o: operations.c operations.h config.h
	gcc $(CFLAGS) -c operations.c -o operations.o

state.o: state.c state.h config.h
	gcc $(CFLAGS) -c state.c -o state.o

clean:
	rm -f *.o test

clean2:
	rm -f *.o

run:
	./operations

run1:
	./operations < txtin.txt > txtout.txt
