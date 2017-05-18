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

int read_and_write(int id_client, char *buffer){
	int n;

    /* read from the socket */
	n = read(id_client, buffer, BUFFER_SIZE);
	if (n < 0)
		printf("ERROR reading from socket\n");
	//printf("Here is the message: %s", buffer);

	/* write in the socket */
	n = write(id_client,"I got your message\n", BUFFER_SIZE);
	if (n < 0)
		printf("ERROR writing to socket\n");

	return n;
}

char* read_user_id(int id_client){
    int n;
    /* read from the socket */
    n = read(id_client, buffer, BUFFER_SIZE);
    if (n < 0)
        printf("ERROR reading from socket\n");
    printf("The userID is: %s", buffer);

    return buffer;
}

CLIENT_LIST* user_verify(char *id_user){

	//Função responsável pela verificação da existência/registro de um cliente
    //INSERIR LOCKS

	CLIENT new_user;
	CLIENT_LIST *existing_user;

    strcpy(new_user.userid, id_user);
	existing_user = searchInClientList(new_user);
	if (existing_user == NULL){

		//Registra e retorna um novo cliente
		printf("Registering Client\n");

		new_user.devices[0] = 0;
		new_user.devices[1] = 0;
		new_user.logged_in = 0;
		if (insertList(new_user) < 0) {
			printf("Client unsuccessfully registered\n");
			return NULL;
		} else {
			printf("Client successfully registered\n");
			return searchInClientList(new_user);
		}
	} else {
		//Retorna um cliente existente
		printf("Client is already registered\n");
		return existing_user;
	}
}

int client_login(CLIENT *client){

    //Verifica se o usuário já está logado e tenta realizar o login. Retorna o id do Device utilizado caso obtenha sucesso.
    //INSERIR LOCKS

    if (client->logged_in == 0){
        client->logged_in = 1;
        client->devices[0] = 1;
        printf("Successfully logged in device 0\n");
        return 0;
    } else if (client->devices[0] == 0){
        client->logged_in = 1;
        client->devices[0] = 1;
        printf("Successfully logged in device 0\n");
        return 0;
    } else if (client->devices[1] == 0){
        client->logged_in = 1;
        client->devices[1] = 1;
        printf("Successfully logged in device 1\n");
        return 1;
    } else {
        printf("Max number of devices already in use\n");
        return -1;
    }
}

int client_logout(CLIENT *client, int device){

    //Realiza o logout do usuário devidamente
    //INSERIR LOCKS

    client->devices[device] = 0;
    if ((client->devices[0] == 0) && (client->devices[1] == 0)){
        client->logged_in = 0;
    }
    printf("Successfully logged out of device %d\n", device);
    return 0;
}

void client_handling(void *arguments){

    //Visa organizar todas as ações relacionadas a lidar com o login de um usuário e realizar suas solicitações

    ARGS *args = (ARGS *) arguments;
    char id_user[MAXNAME];
    int id_client;
    strcpy(id_user, args->arg1);
    id_client = args->arg2;

    CLIENT_LIST *current_client;    //Nodo da lista contendo as infos do usuário
    int n, device = -1;                //Device ativo para esta thread
    current_client = user_verify(id_user); //Verifica a existência do usuário (ou insere) no database
    char buffer[BUFFER_SIZE];

    //printf("Client_handling check: Client registered: %s", current_client->cli.userid); //Print de verificação para ver se o treco tá indo pra lista corretamente

    device = client_login(&current_client->cli); //Tenta realizar o login do usuário
    if (device < 0){
        printf("Login request canceled\n\n");
        close(id_client);   //Fecha o socket
        pthread_exit(0);    //Se não for possível, encerra a thread
    }

    while (running_){

        n = read_and_write(id_client, buffer);
        if (n < 0) //Esse logout não tá rolando no caso de o socket fechar do nada, isso é um PROBLEMA!!!
            client_logout(&current_client->cli, device); //Realiza o logout


        /*
        Fazer aqui: tratamento das requisições do usuário.
        */

        if (strcmp(buffer, "logout\n") == 0) { //Solicitação de Logout
            client_logout(&current_client->cli, device); //Realiza o logout
            close(id_client);
            pthread_exit(0);
        } else {
            printf("I received a request I'm not prepared to handle :(\n");
        }
    }
    printf("Service closed for %s\n", id_user);
    pthread_exit(0);
}

void sync_server(){


}


int main (int argc, char *argv[]) {
    running_ = 1;
    int id_client, id_thread;
    char id_user[MAXNAME];
	pthread_t thread;
	ARGS args;

    if(connect_client() == 0)
        printf("Server Connected!!!\n");

    initializerList(); //Inicializa a lista de clientes

    while(running_){
        id_client = acceptLoop();
        if(id_client >= 0){
        	strcpy(id_user, read_user_id(id_client));
        	strcpy(args.arg1, id_user);
        	args.arg2 = id_client;
            pthread_create(&thread, NULL, client_handling, (void *)&args); //Thread que processa login/requisições
        }
    }

    return 0;
}
