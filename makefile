## make client

client: dropboxClient.o dropboxUtil.o
	gcc dropboxClient.o dropboxUtil.o -o client -pthread
dropboxClient.o: dropboxClient.c
	gcc -c dropboxClient.c 
dropboxUtil.o: dropboxUtil.c
	gcc -c dropboxUtil.c

## make server

server: dropboxServer.o dropboxUtil.o
	gcc dropboxServer.o dropboxUtil.o -o server -pthread
dropboxServer.o: dropboxServer.c
	gcc -c dropboxServer.c 
dropboxUtil.o: dropboxUtil.c
	gcc -c dropboxUtil.c

##

all: server client

## make clean
clean:
	rm *~ *.o client server
