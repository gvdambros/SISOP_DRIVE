#ifndef DROPBOXSERVER_H_INCLUDED
#define DROPBOXSERVER_H_INCLUDED

#include "dropboxUtil.h"

struct file_info{
    char name[MAXNAME];
    char extension[MAXNAME];
    char last_modified[MAXNAME];
    int size;
}

struct client{
    int devices[MAXDEVICES];
    char userid[MAXNAME];
    struct file_info[MAXFILES];
    int logged_in;
}

int sync_server();
void receive_file(char *file);
void get_file(char *file);

#endif // DROPBOXSERVER_H_INCLUDED