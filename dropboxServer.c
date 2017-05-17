#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "dropboxServer.h"
#include <pthread.h>

#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

void initializerList(){
    clientLst_ = NULL;
}

int insertList(CLIENT newClient){
    CLIENT_LIST *new_node;
    new_node = (CLIENT_LIST *)malloc(sizeof(CLIENT_LIST));

    if(new_node == NULL){
        printf("ERROR on insertList\n");
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
            return ptr; //Retorna um CLIENT_LIST
        else
            ptr = ptr->next;
    }
    return NULL;
}

int connect_client(){

	struct sockaddr_in serv_addr;

    if ((socket_server_ = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        printf("ERROR opening socket\n");
        return -1;
    }

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

    if (bind(socket_server_, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		printf("ERROR on binding\n");
        return -1;
    }

    listen(socket_server_, 5);

	return 0;
}

int acceptLoop(){
	int new_socket_client;
	struct sockaddr_in cli_addr;
	socklen_t clilen;

	clilen = sizeof(struct sockaddr_in);
	if ((new_socket_client = accept(socket_server_, (struct sockaddr *) &cli_addr, &clilen)) == -1)
		printf("ERROR on accept\n");

	bzero(buffer, BUFFER_SIZE);

	return new_socket_client;
}

int read_and_write(int id_client){
	int n;

    /* read from the socket */
	n = read(id_client, buffer, BUFFER_SIZE);
	if (n < 0)
		printf("ERROR reading from socket\n");
	printf("Here is the message: %s\n", buffer);

	/* write in the socket */
	n = write(id_client,"I got your message\n", BUFFER_SIZE);
	if (n < 0)
		printf("ERROR writing to socket\n");

	return n;
}

char* read_userId(int id_client){
    int n;
    /* read from the socket */
    n = read(id_client, buffer, BUFFER_SIZE);
    if (n < 0)
        printf("ERROR reading from socket\n");
    printf("The userID is: %s\n", buffer);

    return buffer;
}

CLIENT_LIST* verify_client(char *id_client){

	//Função responsável pela verificação da existência/registro de um cliente

	CLIENT new_client;
	CLIENT_LIST *existing_client;

    strcpy(new_client.userid, id_client);
	existing_client = searchInClientList(new_client);
	if (existing_client == NULL){

		//Registra e retorna um novo cliente
		printf("Registering Client\n");

		//Quais atributos inicializar?
		//new_client.devices[MAXDEVICES] = [0,0];

		new_client.logged_in = 1;

		if (insertList(new_client) < 0) {
			printf("Client unsuccessfully registered\n");
			return NULL;
		} else {
			printf("Client successfully registered\n");
			return searchInClientList(new_client);
		}
	} else {

		//Retorna um cliente existente
		printf("Client is already registered\n");
		return existing_client;
	}
}

void client_handling(char *id_client){

    //Visa organizar todas as ações relacionadas a lidar com o login de um usuário e realizar suas solicitações

    CLIENT_LIST *current_client;
    current_client = verify_client(id_client);

    printf("Client_handling check: Client registered: %s", current_client->cli.userid); //Print de verificação para ver se o treco tá indo pra lista corretamente

    pthread_exit(0);
}

void sync_server(){


}


int main (int argc, char *argv[]) {
    running_ = 1;
    int id_client, id_thread;
    char userId[MAXNAME];
	pthread_t thread;

    if(connect_client() == 0)
        printf("Server Connected!!!\n");

    initializerList(); //Inicializa a lista de clientes

    while(running_){
        id_client = acceptLoop();
        if(id_client >= 0){
        	strcpy(userId, read_userId(id_client));

            /*
            Acho que aqui criamos uma thread para cada login de usuário (máx de 2 logins/threads por usuário)
            Então precisamos ajustar as rotinas pra:
                *verificar se existe o usuário, retornar o nodo dele: função verify_client(char id_client);
                *verificar se está logado no server e quantas vezes está logado (max: 2)
                *manter a thread ativa recebendo requisições desse usuário (algum loop)

                *podemos organizar isso tudo em client_handling(char id_client)
            */

            pthread_create(&thread, NULL, client_handling, userId);
        }
    }

    return 0;
}
