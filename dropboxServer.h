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
} file_info;

struct client{
    int devices[MAXDEVICES];
    char userid[MAXNAME];
    file_info file_info[MAXFILES];
    int logged_in;
};

void sync_server();
void receive_file(char *file);
void get_file(char *file);

// ADDED STUFF
//Funções
int connect_client();
int acceptLoop();
int read_and_write(int id_cliente);
// Variáveis
int running;
int socket_server;
char buffer[BUFFER_SIZE];//temporario
// ADDED STUFF

#endif // DROPBOXSERVER_H_INCLUDED
