test: test.o uthread.o
	gcc test.o uthread.o -o test
test.o: test.c uthread.h
	gcc -Wall -g -pthread test.c -c
uthread.o: uthread.c uthread.h
	gcc -Wall -g -pthread uthread.c -c
clean:
	rm -f *.o test
