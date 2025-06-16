all: bin/server bin/client

bin/server: server.c
	mkdir -p bin
	gcc server.c -o bin/server

bin/client: client.c
	mkdir -p bin
	gcc client.c -o bin/client

clean:
	rm -rf bin
