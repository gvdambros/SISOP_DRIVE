#ifndef DROPBOXUTIL_H_INCLUDED
#define DROPBOXUTIL_H_INCLUDED

#define MAXREQUEST MAXCMD + MAXFILENAME + 1
#define MAXCMD 15
#define MAXFILES 10
#define MAXNAME 10
#define MAXDEVICES 2
#define MAXPATH 200
#define MAXFILENAME 2*MAXNAME+2
#define BUFFER_SIZE 1024*1024

#include <time.h>


typedef struct command{
    char cmd[MAXCMD + 1];
    char param[MAXPATH];
} user_cmd;

int safe_recv(int client_fd, char *buf, int s);
int safe_recvINT(int client_fd, int *buf);
int safe_sendINT(int client_fd, int *buf);

int file_size(char *file);
time_t* file_lastModifier(char *file);
user_cmd string2userCmd(char *cmd);
int dir_exists(char *dir);
char* getLinuxUser ();
int numberOfFilesInDir(char *dir);

#endif // DROPBOXUTIL_H_INCLUDED
