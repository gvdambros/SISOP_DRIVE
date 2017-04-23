#ifndef DROPBOXCLIENT_H_INCLUDED
#define DROPBOXCLIENT_H_INCLUDED

int connect_server(char *host, int port);
int sync_client();
void send_file(char *file);
void get_file(char *file);
void close_connection();

#endif // DROPBOXCLIENT_H_INCLUDED
