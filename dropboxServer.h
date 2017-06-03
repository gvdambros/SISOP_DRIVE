#ifndef DROPBOXSERVER_H_INCLUDED
#define DROPBOXSERVER_H_INCLUDED

#include "dropboxUtil.h"
#include <pthread.h>

typedef struct file_info{
    char name[MAXFILENAME];
    time_t time_lastModified;
    int size;
} FILE_INFO;

typedef struct client{
    int devices[MAXDEVICES];
    char userid[MAXNAME];
    FILE_INFO file_info[MAXFILES];
    int logged_in;
    pthread_mutex_t mutex;
} CLIENT;

typedef struct arg_struct {
    char arg1[MAXNAME];
    int arg2;
} ARGS;

void sync_server();
void receive_file(char *file);
void get_file(char *file);

// ADDED STUFF
//Structs
typedef struct lst{
    CLIENT cli;
    struct lst *next;
}CLIENT_LIST;

void initializerList();
int insertList(CLIENT newCliente);
void initServer();
CLIENT_LIST* searchInClientList(CLIENT nodo);


//Funções
int connect_client();
int acceptLoop();
int read_and_write(int id_cliente, void* buffer);

// Variáveis
CLIENT_LIST *clientLst_ = NULL; //lista global de clientes
int running_;
int socket_server_;
int port_ = 4000;
pthread_mutex_t server_mutex_;


// ADDED STUFF

#endif // DROPBOXSERVER_H_INCLUDED
