all: tracker client

tracker: tracker.o
	gcc -pthread -o ./bin/tracker.o ./tracker/tracker.c

tracker.o: ./tracker/tracker.c
	gcc -c ./tracker/tracker.c

client: ./client/client.c
	gcc ./client/client.c -lpthread -o ./bin/client.o -std=c99
	
clean:
	rm -rf ./bin/*.o
