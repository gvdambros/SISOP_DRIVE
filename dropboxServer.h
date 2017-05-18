#ifndef DROPBOXSERVER_H_INCLUDED
#define DROPBOXSERVER_H_INCLUDED

#include "dropboxUtil.h"

#define BUFFER_SIZE 1024

#define PORT 4000 //temporario

typedef struct file_info{
    char name[MAXNAME];
    char extension[MAXNAME];
    char last_modified[MAXNAME];
    int size;
} FILE_INFO;

typedef struct client{
    int devices[MAXDEVICES];
    char userid[MAXNAME];
    FILE_INFO file_info[MAXFILES];
    int logged_in;
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
CLIENT_LIST* searchInClientList(CLIENT nodo);


//Funções
int connect_client();
int acceptLoop();
int read_and_write(int id_cliente, char *buffer);

// Variáveis
CLIENT_LIST *clientLst_ = NULL; //lista global de clientes
int running_;
int socket_server_;
char buffer[BUFFER_SIZE];//temporario


// ADDED STUFF

#endif // DROPBOXSERVER_H_INCLUDED
