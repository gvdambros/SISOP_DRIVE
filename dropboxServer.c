#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "dropboxServer.h"
#include <pthread.h>

#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

void initializerList()
{
    clientLst_ = NULL;
}

int insertList(CLIENT newClient)
{
    CLIENT_LIST *new_node;
    new_node = (CLIENT_LIST *)malloc(sizeof(CLIENT_LIST));

    if(new_node == NULL)
    {
        printf("ERROR on insertList\n");
        return 1;
    }

    new_node->cli = newClient;
    new_node->next = NULL;

    if(clientLst_ == NULL)
        clientLst_ = new_node;
    else
    {
        new_node->next = clientLst_;
        clientLst_ = new_node;
    }
    return 0;
}

CLIENT_LIST* searchInClientList(CLIENT nodo)
{
    CLIENT_LIST *ptr;

    if(clientLst_ == NULL)
        return NULL; //Lista vazia

    ptr = clientLst_;
    int a = NULL;
    while(ptr != NULL)
    {
        a = strcmp(ptr->cli.userid, nodo.userid);
        if(a == 0)
            return ptr; //Retorna um CLIENT_LIST
        else
            ptr = ptr->next;
    }
    return NULL;
}

int connect_client()
{

    struct sockaddr_in serv_addr;

    if ((socket_server_ = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("ERROR opening socket\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(serv_addr.sin_zero), 8);

    if (bind(socket_server_, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("ERROR on binding\n");
        return -1;
    }

    listen(socket_server_, 5);

    return 0;
}

int acceptLoop()
{
    int new_socket_client;
    struct sockaddr_in cli_addr;
    socklen_t clilen;

    clilen = sizeof(struct sockaddr_in);
    if ((new_socket_client = accept(socket_server_, (struct sockaddr *) &cli_addr, &clilen)) == -1)
        printf("ERROR on accept\n");

    bzero(buffer, BUFFER_SIZE);

    return new_socket_client;
}

char* read_user_id(int id_client)
{
    int n;
    /* read from the socket */
    n = read(id_client, buffer, BUFFER_SIZE);
    if (n < 0)
        printf("ERROR reading from socket\n");
    printf("The userID is: %s\n", buffer);

    return buffer;
}

CLIENT_LIST* user_verify(char *id_user)
{

    //Função responsável pela verificação da existência/registro de um cliente
    //INSERIR LOCKS

    CLIENT new_user;
    CLIENT_LIST *existing_user;

    strcpy(new_user.userid, id_user);
    existing_user = searchInClientList(new_user);
    if (existing_user == NULL)
    {

        //Registra e retorna um novo cliente
        printf("Registering Client\n");

        new_user.devices[0] = 0;
        new_user.devices[1] = 0;
        new_user.logged_in = 0;
        if (insertList(new_user) < 0)
        {
            printf("Client unsuccessfully registered\n");
            return NULL;
        }
        else
        {
            printf("Client successfully registered\n");
            return searchInClientList(new_user);
        }
    }
    else
    {
        //Retorna um cliente existente
        printf("Client is already registered\n");
        return existing_user;
    }
}

int client_login(CLIENT *client)
{

    //Verifica se o usuário já está logado e tenta realizar o login. Retorna o id do Device utilizado caso obtenha sucesso.
    //INSERIR LOCKS

    if (client->logged_in == 0)
    {
        client->logged_in = 1;
        client->devices[0] = 1;
        printf("Successfully logged in device 0\n");
        return 0;
    }
    else if (client->devices[0] == 0)
    {
        client->logged_in = 1;
        client->devices[0] = 1;
        printf("Successfully logged in device 0\n");
        return 0;
    }
    else if (client->devices[1] == 0)
    {
        client->logged_in = 1;
        client->devices[1] = 1;
        printf("Successfully logged in device 1\n");
        return 1;
    }
    else
    {
        printf("Max number of devices already in use\n");
        return -1;
    }
}

int client_logout(CLIENT *client, int device)
{

    //Realiza o logout do usuário devidamente
    //INSERIR LOCKS

    client->devices[device] = 0;
    if ((client->devices[0] == 0) && (client->devices[1] == 0))
    {
        client->logged_in = 0;
    }
    printf("Successfully logged out of device %d\n", device);
    return 0;
}

void client_handling(void *arguments)
{

    //Visa organizar todas as ações relacionadas a lidar com o login de um usuário e realizar suas solicitações

    ARGS *args = (ARGS *) arguments;
    char id_user[MAXNAME];
    int id_client;
    strcpy(id_user, args->arg1);
    id_client = args->arg2;

    CLIENT_LIST *current_client;    //Nodo da lista contendo as infos do usuário
    int n, device = -1, n_files = 0;                //Device ativo para esta thread
    current_client = user_verify(id_user); //Verifica a existência do usuário (ou insere) no database
    char buffer[BUFFER_SIZE], filename[MAXREQUEST], request[MAXNAME];
    time_t filetime;

    //printf("Client_handling check: Client registered: %s", current_client->cli.userid); //Print de verificação para ver se o treco tá indo pra lista corretamente


    // id_client = socket

    device = client_login(&current_client->cli); //Tenta realizar o login do usuário
    if (device < 0)
    {
        printf("Login request canceled\n\n");
        close(id_client);   //Fecha o socket
        pthread_exit(0);    //Se não for possível, encerra a thread
    }

    while (running_)
    {

        safe_recv(id_client, request, MAXREQUEST);
        fprintf(stderr, "%s\n", request);

        if (n < 0) //Esse logout não tá rolando no caso de o socket fechar do nada, isso é um PROBLEMA!!!
            client_logout(&current_client->cli, device); //Realiza o logout

        /*
        Fazer aqui: tratamento das requisições do usuário.
        */

        user_cmd commandLine = string2userCmd(request);

        if (strcmp(commandLine.cmd, "sync") == 0)   //Solicitação de Sincronização (sync_client())
        {

            printf("Received: sync_client\n");

        }
        else if (strcmp(commandLine.cmd, "download") == 0)    //Solicitação de Logout
        {
            fprintf(stderr, "download file...\n");
            //int fs = file_size(commandLine.param);
            int fs = file_size("a.txt");

            fs = htons(fs);

            send(id_client, &fs, sizeof(int), 0);
            // send all the content at once to the server

            // FILE *fp = fopen(commandLine.param, "r");
            FILE *fp = fopen("a.txt", "r");

            int sent_bytes, offset = 0;

            while(offset < fs)
            {
                int bytes_send = fread(buffer, sizeof(char), BUFFER_SIZE, fp);
                sent_bytes = send(id_client, (char*) buffer, bytes_send, 0);
                offset += sent_bytes;
            }

            fclose(fp);
            fprintf(stderr, "download done\n");
        }
        else if (strcmp(commandLine.cmd, "upload") == 0)
        {
            fprintf(stderr, "upload file...\n");
            //int fs = file_size(commandLine.param);

            int fs;

            safe_recv(id_client, &fs, sizeof(int));

            fprintf(stderr, "sa %d\n",fs);
            fs = htons(fs);
            fprintf(stderr, "sd %d\n",fs);

            // FILE *fp = fopen(commandLine.param, "r");
            FILE *fp = fopen("a.txt", "w");

            int offset = 0;

            int acc = 0;
            while (acc < fs)
            {
                // receive at most 1MB of data
                int read = recv(id_client, buffer, BUFFER_SIZE, 0);

                // write the received data in the file
                fwrite(buffer, sizeof(char), read, fp);

                acc += read;
            }

            fclose(fp);
            fprintf(stderr, "upload done\n");
        }
        else if (strcmp(buffer, "exit") == 0)   //Solicitação de Logout
        {
            client_logout(&current_client->cli, device); //Realiza o logout
            close(id_client);
            pthread_exit(0);
        }
        else
        {
            printf("I received a request I'm not prepared to handle :(\n");
        }
    }
    printf("Service closed for %s\n", id_user);
    pthread_exit(0);
}

void sync_server()
{


}


int main (int argc, char *argv[])
{
    running_ = 1;
    int id_client, id_thread;
    char id_user[MAXNAME];
    pthread_t thread;
    ARGS args;
    fprintf(stderr, "%d\n", argc);
    if(argc > 1) port_ = atoi(argv[1]);

    if(connect_client() == 0)
        printf("Server Connected!!!\n");

    initializerList(); //Inicializa a lista de clientes

    while(running_)
    {
        id_client = acceptLoop();
        if(id_client >= 0)
        {
            safe_recv(id_client, id_user, MAXNAME);
            strcpy(args.arg1, id_user);
            args.arg2 = id_client;
            pthread_create(&thread, NULL, client_handling, (void *)&args); //Thread que processa login/requisições
        }
    }

    return 0;
}
