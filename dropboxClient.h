#ifndef DROPBOXCLIENT_H_INCLUDED
#define DROPBOXCLIENT_H_INCLUDED

#include "dropboxUtil.h"
#include <semaphore.h>

#define SUCCESS 0

int connect_server(char *host, int port);
int sync_client();
void send_file(char *file);
void get_file(char *file);
void close_connection();
time_t getTimeServer();

// ADDED STUFF

pthread_t sync_thread;
sem_t	runningRequest;
int running;
int socket_client; // Maybe it shouldn't be here
char name_client[MAXNAME];

CLIENT client_info;
void delete_file(char *file);

char dropboxDir_[200];

// ADDED STUFF


#endif // DROPBOXCLIENT_H_INCLUDED
