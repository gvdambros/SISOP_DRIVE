#ifndef DROPBOXSERVER_H_INCLUDED
#define DROPBOXSERVER_H_INCLUDED

#include "dropboxUtil.h"
#include <pthread.h>

typedef struct user_info {
    char username[MAXNAME];
    char password[MAXNAME];
    int socket;
} USER_INFO;

void sync_server(CLIENT client, int socket);
void receive_file(char *file, CLIENT *client, int socket);
void send_file(char *file, CLIENT client, int socket);

// ADDED STUFF
//Structs
typedef struct lst{
    CLIENT cli;
    struct lst *next;
}CLIENT_LIST;

//Funções gerenciamento da lista de usuarios
void initList();
int insertList(CLIENT newCliente);

//Funções gerenciamento do server/conexoes
int initServer();
int connectClient();
int acceptLoop();
int clientLogin(CLIENT *client);
int clientLogout(CLIENT *client, int device);
void clientHandling(void* arg);

//Funções gerenciamento do diretorio no server
void initFiles_ClientDir(CLIENT *client);
void printFiles_ClientDir(CLIENT client);
int addFile_ClientDir(CLIENT *client, char *file, int size, time_t lastModified);
int nextEmptySpace_ClientDir(CLIENT client);
int findFile_ClientDir(CLIENT client, char *file);
int deleteFile_ClientDir(CLIENT *client, char *file);
int numberOfFiles_ClientDir(CLIENT client);
int isFull_ClientDir(CLIENT client);

// Variáveis
CLIENT_LIST *clientLst_ = NULL; //lista global de clientes
int running_;
int socket_server_;
int port_ = 4000;
pthread_mutex_t server_mutex_;


// ADDED STUFF

#endif // DROPBOXSERVER_H_INCLUDED
