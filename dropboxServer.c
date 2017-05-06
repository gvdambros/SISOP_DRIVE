#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "dropboxServer.h"


#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

void initializerList(){
    clientLst_ = NULL;
}

int insertList(CLIENT newClient){
    CLIENT_LIST *new_node;
    new_node = (CLIENT_LIST *)malloc(sizeof(CLIENT_LIST));

    if(new_node == NULL){
        printf("Insert Error!");
        return 1;
    }

    new_node->cli = newClient;
    new_node->next = NULL;

    if(clientLst_ == NULL)
        clientLst_ = new_node;
    else{
        new_node->next = clientLst_;
        clientLst_ = new_node;
    }
    return 0;
}

CLIENT_LIST* searchInClientList(CLIENT nodo){
    CLIENT_LIST *ptr;

    if(clientLst_ == NULL)
        return NULL; //Lista vazia

    ptr = clientLst_;
    int a = NULL;
    while(ptr != NULL){
        a = strcmp(ptr->cli.userid, nodo.userid);
        if(a == 0)
            return ptr;
        else
            ptr = ptr->next;
    }
    return NULL;
}

int connect_client(){

	struct sockaddr_in serv_addr;

    if ((socket_server_ = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

    if (bind(socket_server_, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		printf("ERROR on binding");

    listen(socket_server_, 5);

	return 0;
}

int acceptLoop(){
    int new_socket_cliente;
    struct sockaddr_in cli_addr;
    //char buffer[BUFFER_SIZE];
    socklen_t clilen;

    clilen = sizeof(struct sockaddr_in);
    if ((new_socket_cliente = accept(socket_server_, (struct sockaddr *) &cli_addr, &clilen)) == -1)
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

char* read_userId(int id_client){
    int n;
    /* read from the socket */
    n = read(id_client, buffer, BUFFER_SIZE);
    if (n < 0)
        printf("ERROR reading from socket");
    printf("The userID is: %s\n", buffer);

    return buffer;

}


void sync_server(){


}


int main (int argc, char *argv[]) {
    running_ = 1;
    int c, id_client;

    c = connect_client();
    if(c == 0)
        printf("Server Connected!!!\n");

    char userId[MAXNAME];

    while(running_){
        id_client = acceptLoop();
        if(id_client >= 0){
            read_and_write(id_client);
            strcpy(userId, read_userId(id_client));

        }
    }

    return 0;
}
