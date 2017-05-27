#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
        initFilesOfClient(&new_user);
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

    CLIENT_LIST *current_client = user_verify(id_user);    //Nodo da lista contendo as infos do usuário
    int n, device = -1, n_files = 0;                //Device ativo para esta thread
    char buffer[BUFFER_SIZE], filename[MAXREQUEST], request[MAXNAME], *dir, *pathFile;
    time_t filetime;
    char *user;

    device = client_login(&current_client->cli); //Tenta realizar o login do usuário
    if (device < 0)
    {
        printf("Login request canceled\n\n");
        close(id_client);   //Fecha o socket
        pthread_exit(0);    //Se não for possível, encerra a thread
    }

    dir = malloc(200);
    pathFile = malloc(200);
    user = getLinuxUser();

    dir = strcpy(dir, "/home/");
    dir = strcat(dir, user);
    dir = strcat(dir, "/Documents/server_dir_");
    dir = strcat(dir, id_user);
    dir = strcat(dir, "/");

    if( !dir_exists (dir) )
    {
        fprintf(stderr, "Criou diretorio em %s\n", dir);
        mkdir(dir, 0777);
    }


    while (running_)
    {
        safe_recv(id_client, request, MAXREQUEST);
        fprintf(stderr, "%s\n", request);

        /*
        Fazer aqui: tratamento das requisições do usuário.
        */
        fprintf(stderr, "request: %s\n",request);
        user_cmd commandLine = string2userCmd(request);

        if (strcmp(commandLine.cmd, "sync") == 0)   //Solicitação de Sincronização (sync_client())
        {
            fprintf(stderr, "sync file...\n");

            /*
            int numberFilesClient, numberFilesServer = numberOfFiles(dir), i, j = 0, count = 0;
            int bitMap[numberFilesServer];

            safe_recv(id_client, &numberFilesClient, sizeof(int));
            numberFilesClient = ntohs(numberFilesClient);

            // Check to see which files are sync
            // bitMap keeps track of them
            timer_t lastModified;
            for(i = 0; i < numberFilesClient; i++){
               safe_recv(id_client, &filename, MAXNAME);
               safe_recv(id_client, &lastModified, sizeof(lastModified));

               while(j < MAXFILES && !strcmp(filename, current_client->cli.file_info[j].name));

               if(strcmp(filename, current_client->cli.file_info[j].name)){

                    if(!difftime(lastModified, current_client->cli.file_info[j].last_modified )){
                        bitMap[j] = 1;
                        count++;
                    }
               }

            }

            count = numberFilesServer - count;
            send(id_client, &count, sizeof(int), 0);

            // send files that are not sync
            for(i = 0; i < numberFilesServer; i++){
                if(!bitMap[i]){

                }
            }
            */

            fprintf(stderr, "sync file...\n");

        }
        else if (strcmp(commandLine.cmd, "download") == 0) {
            fprintf(stderr, "download file...\n");

            strcpy(pathFile, dir);
            strcat(pathFile, commandLine.param);
            fprintf(stderr, "dir: %s path: %s\n", dir, pathFile);

            //int fs = file_size(commandLine.param);
            int fs = file_size(pathFile);
            int aux = htons(fs);

            send(id_client, &aux, sizeof(int), 0);
            // send all the content at once to the server

            struct stat stbuf;
            stat(pathFile, &stbuf );
            send(id_client, &(stbuf.st_mtime), sizeof(stbuf.st_mtime), 0);

            FILE *fp = fopen(pathFile, "r");
            //FILE *fp = fopen("a.txt", "r");

            int sent_bytes, offset = 0;

            while(offset < fs)
            {
                int bytes_send = fread(buffer, sizeof(char), BUFFER_SIZE, fp);
                sent_bytes = send(id_client, (char*) buffer, bytes_send, 0);
                offset += sent_bytes;
            }

            fclose(fp);
            fprintf(stderr, "download done\n");
        } else if (strcmp(commandLine.cmd, "upload") == 0) {
            fprintf(stderr, "upload file...\n");

            int fs;
            safe_recv(id_client, &fs, sizeof(int));
            fs = htons(fs);

            // Send last modified time (time_t)
            time_t lm;
            safe_recv(id_client, &lm, sizeof(time_t));

            pathFile = strcpy(pathFile, dir);
            pathFile = strcat(pathFile, commandLine.param);

            FILE *fp = fopen(pathFile, "w");
            //FILE *fp = fopen("a.txt", "w");

            int full = dirOfClientIsFull(current_client->cli);

            fprintf(stderr, "file size: %d\n", fs);

            int acc = 0;
            while (acc < fs)
            {
                // receive at most 1MB of data
                int read = recv(id_client, buffer, BUFFER_SIZE, 0);

                // write the received data in the file
                if(!full) fwrite(buffer, sizeof(char), read, fp);

                acc += read;
            }

            if(!full) addFileToClient(&(current_client->cli), commandLine.param, fs, lm);

            printFilesOfClient(current_client->cli);

            fclose(fp);
            fprintf(stderr, "upload done\n");
        } else if (strcmp(commandLine.cmd, "delete") == 0)
        {
            fprintf(stderr, "delete file...\n");

            pathFile = strcpy(pathFile, dir);
            pathFile = strcat(pathFile, commandLine.param);
            remove(pathFile);

            deleteFileToClient(&(current_client->cli), commandLine.param);
            printFilesOfClient(current_client->cli);

            fprintf(stderr, "delete done\n");
        }
        else if (strcmp(commandLine.cmd, "exit") == 0)
        {
            fprintf(stderr, "exiting...\n");
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

void printFilesOfClient(CLIENT client)
{
    int i;
    fprintf(stderr, "files of %s\n", client.userid);
    for(i = 0; i < MAXFILES; i ++)
    {
        if(client.file_info[i].size != -1)
        {
            fprintf(stderr, "file: %s size: %d\n", client.file_info[i].name, client.file_info[i].size);
        }
    }
    return;
}

int addFileToClient(CLIENT *client, char *file, int size, time_t lastModified){
    int i = 0;
    while(i < MAXFILES && client->file_info[i].size != -1) i++;
    if(i == MAXFILES){
        return -1;
    }

    strcpy( client->file_info[i].name, file);
    client->file_info[i].size = size;
    client->file_info[i].time_lastModified = lastModified;

    return 0;
}

int deleteFileToClient(CLIENT *client, char *file){
    int i = 0;
    while(i < MAXFILES && !strcmp(client->file_info[i].name, file))  i++;
    if(i == MAXFILES){
        return -1;
    }
    client->file_info[i].size = -1;
    return 0;
}

int numberOfFilesOfClient(CLIENT client){
    int i = 0, count = 0;
    for(i = 0; i < MAXFILES; i ++){
        if(client.file_info[i].size != -1){
            count++;
        }
    }
    return count;
}

int dirOfClientIsFull(CLIENT client){
    int i = 0;
    for(i = 0; i < MAXFILES; i ++){
        if(client.file_info[i].size != -1){
            return 0;
        }
    }
    return 1;
}

void initFilesOfClient(CLIENT *client){
    int i;
    for(i = 0; i < MAXFILES; i++){
        client->file_info[i].size = -1;
    }
    return;
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
