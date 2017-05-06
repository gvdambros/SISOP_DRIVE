#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "dropboxServer.h"


#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

int connect_client(){

	struct sockaddr_in serv_addr;

	if ((socket_server = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(socket_server, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		printf("ERROR on binding");

	listen(socket_server, 5);

	return 0;
}

int acceptLoop(){
    int new_socket_cliente;
    struct sockaddr_in cli_addr;
    //char buffer[BUFFER_SIZE];
    socklen_t clilen;

    clilen = sizeof(struct sockaddr_in);
	if ((new_socket_cliente = accept(socket_server, (struct sockaddr *) &cli_addr, &clilen)) == -1)
		printf("ERROR on accept");

	bzero(buffer, BUFFER_SIZE);

	return new_socket_cliente;
}

int read_and_write(int id_cliente){
    int n;

    /* read from the socket */
	n = read(id_cliente, buffer, BUFFER_SIZE);
	if (n < 0)
		printf("ERROR reading from socket");
	printf("Here is the message: %s\n", buffer);

	/* write in the socket */
	n = write(id_cliente,"I got your message", BUFFER_SIZE);
	if (n < 0)
		printf("ERROR writing to socket");

    return n;
}


void sync_server(){


}


int main (int argc, char *argv[]) {
    running = 1;
    int c, id_client;

    c = connect_client();

    while(running){
        id_client = acceptLoop();
        if(id_client >= 0){
            read_and_write(id_client);

        }
    }

    return 0;
}
