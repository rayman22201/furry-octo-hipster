tracker: tracker.o
	gcc -pthread -o ./bin/tracker ./tracker/tracker.c

tracker.o: ./tracker/tracker.c
	gcc -c ./tracker/tracker.c
	
clean:
	rm -f *.o
