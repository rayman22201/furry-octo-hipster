all: tracker client

tracker: tracker.o
	gcc -pthread tracker.o -o ./bin/tracker

client: client.o
	gcc -pthread client.o -o ./bin/client

tracker.o: ./tracker/tracker.c
	gcc -c ./tracker/tracker.c

client.o: ./client/client.c
	gcc -c -std=c99 ./client/client.c

clean:
	rm -rf ./bin/* ./*.o
