#ifndef DROPBOXUTIL_H_INCLUDED
#define DROPBOXUTIL_H_INCLUDED

#define MAXREQUEST MAXCMD+2*MAXNAME+2
#define MAXCMD 15
#define MAXFILES 10
#define MAXNAME 10
#define MAXDEVICES 2

typedef struct command{
    char *cmd[MAXCMD + 1];
    char *param[MAXNAME*2 + 1];
} user_cmd;

int safe_recv(int client_fd, char *buf, int s);
int file_size(char *file);
user_cmd string2userCmd(char *cmd);

#endif // DROPBOXUTIL_H_INCLUDED
