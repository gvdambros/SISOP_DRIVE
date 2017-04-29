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
