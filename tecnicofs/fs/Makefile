CFLAGS = -g -Wall -std=c11

op: operations.o state.o
	gcc $(CFLAGS) operations.o state.o -o op

operations.o: operations.c operations.h state.h config.h
	gcc $(CFLAGS) -c operations.c -o operations.o

state.o: state.c state.h operations.h config.h
	gcc $(CFLAGS) -c state.c -o state.o

clean:
	rm -f *.o op

clean2:
	rm -f *.o

run:
	./test

run1:
	./op < tree1.txt

run2:
	./op < tree2.txt