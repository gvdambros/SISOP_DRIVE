#ifndef DROPBOXCLIENT_H_INCLUDED
#define DROPBOXCLIENT_H_INCLUDED

#include "dropboxUtil.h"
#include <semaphore.h>

int sync_client();
int send_file(char *file);
int get_file(char *file);
void close_connection();
time_t gettime();
int delete_file(char *file);

// ADDED STUFF

pthread_t sync_thread;
sem_t runningRequest;
int running;
int socket_client;
char name_client[MAXNAME];
char dropboxDir_[200];

// ADDED STUFF

#endif // DROPBOXCLIENT_H_INCLUDED
