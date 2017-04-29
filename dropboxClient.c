#include "dropboxClient.h"
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

#define BUFSIZ 10

int connect_server(char *host, int port)
{
    //int socket_client;
    struct sockaddr_in socket_server;

    socket_client = socket(AF_INET , SOCK_STREAM , 0);

    if (socket_client == -1)
    {
        printf("Could not create socket");
        return -1;
    }

    socket_server.sin_addr.s_addr = inet_addr(host);
    socket_server.sin_family = AF_INET;
    socket_server.sin_port = htons( port );

    if (connect(socket_client , (struct sockaddr *)&socket_server , sizeof(socket_server)) < 0)
    {
        printf("Could not connect");
        return -2;
    }

    int flag = 1;
    setsockopt(socket_client,IPPROTO_TCP,TCP_NODELAY,(char *)&flag,sizeof(flag));

    return 0;
}

void close_connection()
{
    close(socket_client);
}

user_cmd string2userCmd(char *cmd)
{
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

void send_file(char *file)
{

    int fs = file_size(file);

    FILE *fp = fopen(file, "r");

    char buffer[fs + 1];

    fread(buffer, fs, 1, fp);

    // envia o tamanho do arquivo
    int sent_bytes = send(socket_client, &fs, sizeof(int), 0);

    printf("%d %d \n", sent_bytes, fs);

    // enquanto não enviar todo o arquivo envia o proximo pacote. Também da para enviar o arquivo inteiro de uma vez, mas não sei se não pode dar problema.
    int offset = 0;
    while (((sent_bytes = send(socket_client, &(buffer[offset]), BUFSIZ, 0)) > 0) && (fs > offset))
    {
        fprintf(stdout, "1. Server sent %d bytes from file's data, offset is now : %d\n", sent_bytes, offset);
        offset += sent_bytes;
        fprintf(stdout, "2. Server sent %d bytes from file's data, offset is now : %d\n", sent_bytes, offset);
    }
}

/*
    Discutir isso com quem fará o server, há necessidade de um protocolo.

    ex.: user abre conexão e manda ID
        server da ok pro ID
        user manda o nome do arquivo
        server da ok pro nome do arquivo
        user manda o tamanho do arquivo
        server da ok pro tamanho do arquivo
        user manda o arquivo
        server da ok pro arquivo (talvez não precise, pois, se user fecha conexão, server ainda recebe o que esta na fila de dados)

    Como mandar <nome usuário> para server? Ideia: seria a primeira informação a mandar após conexão inicia. Precisará de uma funão adicional para fazer isso.
    Como mandar <nome do arquivo> para server? Ideia: mandar <nome do arquivo>, tamanho e depois arquivo em si.

    Outra ideia para ambas as questões: quando usuário for mandar arquivo, ele manda <nome usuário>, <nome do arquico>, tamanho e arquivo em si, mas como tratar a variável devices?
*/

int main(int argc, char *argv[])
{
    int flag = 1;

    if(argc <= 3)
    {
        printf("call ./dropboxClient fulano endereço porta\n");
        //return -1;
    }

    printf("%s %s\n",argv[2], argv[3]);

    if( connect_server(argv[2], atoi( argv[3] )) < 0){
        printf("connection failed\n");
        //return -1;
    }

    char cmd_line[MAXCMD + 2*MAXNAME + 2] = "";
    user_cmd userCmd;

    do
    {
        gets(cmd_line);
        userCmd = string2userCmd(cmd_line);

        if(!strcmp(userCmd.cmd, "upload"))
        {
            printf("Upload\n");
            send_file("a.txt");
        }
        else if(!strcmp(userCmd.cmd, "download"))
        {
            printf("Download\n");

        }
        else if(!strcmp(userCmd.cmd, "list"))
        {
            printf("List\n");

        }
        else if(!strcmp(userCmd.cmd, "get_sync_dir"))
        {
            printf("Get Sync DIR\n");

        }
        else if(strcmp(userCmd.cmd, "exit"))
        {
            printf("Invalid command\n");
        }

    }
    while(strcmp(userCmd.cmd, "exit"));

    close_connection();
}
