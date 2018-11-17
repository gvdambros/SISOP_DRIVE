## all

all: server client

## make client

client: dropboxClient.o dropboxUtil.o logUtil.o
	gcc dropboxClient.o dropboxUtil.o logUtil.o -o client -pthread
dropboxClient.o: dropboxClient.c
	gcc -c dropboxClient.c

## make server

server: dropboxServer.o dropboxUtil.o logUtil.o
	gcc dropboxServer.o dropboxUtil.o logUtil.o -o server -pthread
dropboxServer.o: dropboxServer.c
	gcc -c dropboxServer.c

## make Utils

dropboxUtil.o: dropboxUtil.c
	gcc -c dropboxUtil.c
logUtil.o: logUtil.c
	gcc -c logUtil.c

## make clean
clean:
	rm *~ *.o client server
