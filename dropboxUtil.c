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

int file_size(char *file){
    int fd;
    struct stat file_stat;
    fd = open(file, O_RDONLY);

    if (fd == -1)
    {
        printf("Error opening file --> %s\n", file);
        return -1;
    }

    if (fstat(fd, &file_stat) < 0)
    {
        printf("Error fstat --> %s\n", file);
        return -2;
    }

    close(fd);

    return file_stat.st_size;
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
int safe_recv(int client_fd, char *buf, int s){
	int offset = 0;
	while(offset < s){
		int recv_bytes = recv(client_fd, &(buf[offset]), s, 0);
		offset += recv_bytes;
	}
	return offset;
}
