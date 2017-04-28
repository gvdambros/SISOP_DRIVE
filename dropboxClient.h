#ifndef DROPBOXCLIENT_H_INCLUDED
#define DROPBOXCLIENT_H_INCLUDED

#include "dropboxUtil.h"

#define MAXCMD 15

int connect_server(char *host, int port);
int sync_client();
void send_file(char *file);
void get_file(char *file);
void close_connection();

int socket_client; // Maybe it shouldn't be here

typedef struct command{
    char *cmd[MAXCMD + 1];
    char *param[MAXNAME*2 + 1];
} user_cmd;


#endif // DROPBOXCLIENT_H_INCLUDED
