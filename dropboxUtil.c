#include "dropboxUtil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <pwd.h>
#include <dirent.h>

int file_size(char *file)
{
    struct stat file_stat;

    if (stat(file, &file_stat) < 0)
    {
        printf("Error fstat --> %s\n", file);
        return -1;
    }

    return file_stat.st_size;
}

time_t* file_lastModifier(char *file){
    time_t *t = (time_t*) malloc(sizeof(time_t));
    struct stat stbuf;
    if(stat(file, &stbuf) < 0){
        printf("Error fstat --> %s\n", file);
        return 0;
    }
    (*t) = stbuf.st_mtime;
    return t;
}

user_cmd string2userCmd(char *cmd)
{
    printf("%s\n", cmd);
    char *pch;
    user_cmd temp;
    pch = strtok (cmd," ");
    if (pch != NULL)
    {
        printf ("cmd: %s\n",pch);
        strcpy(temp.cmd, pch);
        pch = strtok (NULL, " ");
        if (pch != NULL)
        {
            printf ("param: %s\n",pch);
            strcpy(temp.param, pch);
        }
    }
    return temp;
}

// wait until it receives s bytes
int safe_recv(int client_fd, char *buf, int s)
{
    int offset = 0;
    while(offset < s)
    {
        int recv_bytes = recv(client_fd, &(buf[offset]), s, 0);
        offset += recv_bytes;
    }
    return offset;
}


int dir_exists(char *dir)
{
    struct stat st = {0};

    if (stat(dir, &st) == -1)
    {
        return 0;
    }
    return 1;
}

char* getLinuxUser ()
{
    register struct passwd *pw;
    register uid_t uid;
    char *user = NULL;
    uid = geteuid ();
    pw = getpwuid (uid);
    if (pw)
    {
        user = malloc(strlen(pw->pw_name + 1));
        strcpy(user, pw->pw_name);
        puts (user);
        return user;
    }
    return -1;
}

int numberOfFilesInDir(char *dir)
{

    int file_count = 0;
    DIR * dirp;
    struct dirent * entry;

    dirp = opendir(dir); /* There should be error handling after this */
    while ((entry = readdir(dirp)) != NULL)
    {
        if (entry->d_type == DT_REG)   /* If the entry is a regular file */
        {
            file_count++;
        }
    }
    closedir(dirp);
    return file_count;
}
