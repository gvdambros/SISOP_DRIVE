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
#include <semaphore.h>
#include <pthread.h>


typedef struct file_info{
    char name[MAXFILENAME];
    time_t lastModified;
    int size;
} FILE_INFO;

typedef struct client{
    int devices[MAXDEVICES];
    char user_id[MAXNAME];
    char user_password[MAXNAME];
    FILE_INFO file_info[MAXFILES];
    int logged_in;
    pthread_mutex_t mutex;
} CLIENT;


typedef struct command{
    char cmd[MAXCMD + 1];
    char param[MAXPATH];
} user_cmd;

int safe_recv(int client_fd, char *buf, int s);
int safe_recvINT(int client_fd, int *buf);
int safe_sendINT(int client_fd, int *buf);

int file_size(char *file);
time_t file_lastModified(char *file);
user_cmd string2userCmd(char *cmd);
int dir_exists(char *dir);
char* getLinuxUser ();
int numberOfFilesInDir(char *dir);


void printFiles_ClientDir(CLIENT client);

int addFile_ClientDir(CLIENT *client, char *file, int size, time_t lastModified);

int nextEmptySpace_ClientDir(CLIENT client);

int findFile_ClientDir(CLIENT client, char *file);

int deleteFile_ClientDir(CLIENT *client, char *file);

int numberOfFiles_ClientDir(CLIENT client);

int isFull_ClientDir(CLIENT client);

void initFiles_ClientDir(CLIENT *client);

#endif // DROPBOXUTIL_H_INCLUDED
