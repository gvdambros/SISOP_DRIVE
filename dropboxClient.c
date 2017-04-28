#include "dropboxClient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>


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

    return socket_client;
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
        if (pch != NULL){
            printf ("param: %s\n",pch);
            strcpy(temp.param, pch);
        }
    }
    return temp;
}

void clean_stdin()
{
    int c;
    do {
        c = getchar();
    } while (c != '\n' && c != EOF);
}


/*
    Discutir isso com quem fará o server, há necessidade de um protocolo.

    Como mandar <nome usuário> para server? Ideia: seria a primeira informação a mandar após conexão inicia. Precisará de uma funão adicional para fazer isso.
    Como mandar <nome do arquivo> para server? Ideia: mandar <nome do arquivo>, tamanho e depois arquivo em si.

    Outra ideia para ambas as questões: quando usuário for mandar arquivo, ele manda <nome usuário>, <nome do arquico>, tamanho e arquivo em si, mas como tratar a variável devices?
*/

int main(int argc, char *argv[])
{
    if(argc < 3)
    {
        printf("call ./dropboxClient fulano endereço porta\n");
        //return -1;
    }

    //connect_server(argv[1], argv[2]);

    char cmd_line[MAXCMD + 2*MAXNAME + 2] = "";
    user_cmd userCmd;

    do
    {
        fgets(cmd_line, sizeof(cmd_line), stdin);
        userCmd = string2userCmd(cmd_line);

        if(!strcmp(userCmd.cmd, "upload"))
        {
            printf("Upload\n");

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
        else {
            printf("Invalid command\n");
        }
        clean_stdin();
    }
    while(strcmp(userCmd.cmd, "exit"));

}
