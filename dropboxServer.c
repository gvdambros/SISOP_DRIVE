#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include "dropboxServer.h"
#include <pthread.h>
#include <pthread.h>
#include <time.h>
#include <utime.h>

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

    return new_socket_client;
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
        initFiles_ClientDir(&new_user);
        pthread_mutex_init(&(new_user.mutex), NULL);

        pthread_mutex_lock(&server_mutex_);
        if (insertList(new_user) < 0)
        {
            printf("Client unsuccessfully registered\n");
            pthread_mutex_unlock(&server_mutex_);
            return NULL;
        }
        else
        {
            printf("Client successfully registered\n");
            pthread_mutex_unlock(&server_mutex_);
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

void client_handling(void* arg)
{
    USER_INFO user_info = *(USER_INFO*)arg;

    //Visa organizar todas as ações relacionadas a lidar com o login de um usuário e realizar suas solicitações
    char id_user[MAXNAME];
    int socket_user = user_info.socket;
    strcpy(id_user, user_info.username);

    CLIENT_LIST* current_client = user_verify(id_user);    //Nodo da lista contendo as infos do usuário
    int n, device = -1, n_files = 0;                //Device ativo para esta thread
    char filename[MAXREQUEST], request[MAXNAME], *dir, *pathFile;
    time_t filetime;
    char *user;

    device = client_login(&current_client->cli); //Tenta realizar o login do usuário
    if (device < 0)
    {
        printf("Login request canceled\n\n");
        close(socket_user);   //Fecha o socket
        pthread_exit(0);    //Se não for possível, encerra a thread
    }

    dir = malloc(MAXPATH);
    pathFile = malloc(MAXPATH);
    user = getLinuxUser();

    dir = strcpy(dir, "/home/");
    dir = strcat(dir, user);
    dir = strcat(dir, "/server_dir_");
    dir = strcat(dir, id_user);
    dir = strcat(dir, "/");

    if( !dir_exists (dir) )
    {
        fprintf(stderr, "Criou diretorio em %s\n", dir);
        mkdir(dir, 0777);
    }

    while (running_)
    {
        safe_recv(socket_user, request, MAXREQUEST);
        fprintf(stderr, "request: %s\n",request);
        user_cmd commandLine = string2userCmd(request);

        if (strcmp(commandLine.cmd, "sync") == 0)   //Solicitação de Sincronização (sync_client())
        {
            sync_server(current_client->cli, socket_user);
        }
        else if (strcmp(commandLine.cmd, "download") == 0)
        {
            strcpy(pathFile, dir);
            strcat(pathFile, commandLine.param);
            send_file(pathFile, current_client->cli, socket_user);
        }
        else if (strcmp(commandLine.cmd, "upload") == 0)
        {
            strcpy(pathFile, dir);
            strcat(pathFile, commandLine.param);
            receive_file(pathFile, &(current_client->cli), socket_user);
        }
        else if (strcmp(commandLine.cmd, "delete") == 0)
        {
            fprintf(stderr, "delete file %s...\n", commandLine.param);

            pthread_mutex_lock(&(current_client->cli.mutex));

            printFiles_ClientDir(current_client->cli);

            deleteFile_ClientDir(&(current_client->cli), commandLine.param);

            printFiles_ClientDir(current_client->cli);
            pathFile = strcpy(pathFile, dir);
            pathFile = strcat(pathFile, commandLine.param);
            remove(pathFile);
            pthread_mutex_unlock(&(current_client->cli.mutex));

            fprintf(stderr, "delete done\n\n");
        }
        else if (strcmp(commandLine.cmd, "exit") == 0)
        {
            fprintf(stderr, "exiting...\n\n");
            client_logout(&current_client->cli, device); //Realiza o logout
            close(socket_user);
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

void printFiles_ClientDir(CLIENT client)
{
    int i;
    fprintf(stderr, "files of %s\n", client.userid);
    for(i = 0; i < MAXFILES; i ++)
    {
        if(client.file_info[i].size != -1)
        {
            char buff[20];
            struct tm * timeinfo;
            timeinfo = localtime (&(client.file_info[i].time_lastModified));
            strftime(buff, sizeof(buff), "%b %d %H:%M", timeinfo);
            fprintf(stderr, "id: %d file: %s size: %d last: %s\n", i, client.file_info[i].name, client.file_info[i].size, buff);
        }
    }
    return;
}

int addFile_ClientDir(CLIENT *client, char *file, int size, time_t lastModified)
{
    int i;
    if( (i = findFile_ClientDir(*client, file)) < 0)
    {
        // If file doesn't exist
        if((i = nextEmptySpace_ClientDir(*client)) < 0) return -1;

    }

    fprintf(stderr, "FILE ADDED\n");
    strcpy( client->file_info[i].name, file);
    client->file_info[i].size = size;
    client->file_info[i].time_lastModified = lastModified;

    return 0;
}

int nextEmptySpace_ClientDir(CLIENT client)
{
    int i = 0;
    while(i < MAXFILES && client.file_info[i].size != -1) i++;
    if(i == MAXFILES)
    {
        // if dir is full
        return -1;
    }
    return i;

}

int findFile_ClientDir(CLIENT client, char *file)
{
    int i = 0;
    while(i < MAXFILES && strcmp(client.file_info[i].name, file))
    {
        fprintf(stderr, "File: %s\n", client.file_info[i].name);
        i++;
    }
    if(i >= MAXFILES)
    {
        return -1;
    }
    return i;
}

int deleteFile_ClientDir(CLIENT *client, char *file)
{
    int i = 0;
        fprintf(stderr, "DELEEEEEEETE");
    while(i < MAXFILES && strcmp(client->file_info[i].name, file)) {
        fprintf(stderr, "%s %s ", file, client->file_info[i].name);
        i++;
    }
    if(i == MAXFILES)
    {
        return -1;
    }
    fprintf(stderr, "%s %s %d", file, client->file_info[i].name, i);
    client->file_info[i].size = -1;
    return 0;
}

int numberOfFiles_ClientDir(CLIENT client)
{
    int i = 0, count = 0;
    for(i = 0; i < MAXFILES; i ++)
    {
        if(client.file_info[i].size != -1)
        {
            count++;
        }
    }
    return count;
}

int isFull_ClientDir(CLIENT client)
{
    int i;
    for(i = 0; i < MAXFILES; i ++)
    {
        if(client.file_info[i].size == -1)
        {
            return 0;
        }
    }
    return 1;
}

void initFiles_ClientDir(CLIENT *client)
{
    int i;
    for(i = 0; i < MAXFILES; i++)
    {
        client->file_info[i].size = -1;
        client->file_info[i].name[0] = '\0';
    }
    return;
}

void sync_server(CLIENT client, int socket)
{
    fprintf(stderr, "sync file... %d \n" ,numberOfFiles_ClientDir(client));
    int numberFilesClient, numberFilesServer = numberOfFiles_ClientDir(client), i, j, count = 0;
    int *bitMap = calloc(MAXFILES, sizeof(int));
    char buffer[BUFFER_SIZE], *pathFile = malloc(MAXPATH), *user = malloc(MAXNAME), *dir = malloc(MAXPATH), *filename = malloc(MAXFILENAME);

    fprintf(stderr, "sync file...\n");
    user = getLinuxUser();

    fprintf(stderr, "sync file...\n");
    dir = strcpy(dir, "/home/");
    dir = strcat(dir, user);
    dir = strcat(dir, "/server_dir_");
    dir = strcat(dir, client.userid);
    dir = strcat(dir, "/");
    // bitMap[i] == 0 -> espaço vazio
    // bitMap[i] == 1 -> arquivo synx
    // bitMap[i] == 2 -> arquivo não existe no usuario ou não sync

    fprintf(stderr, "sync file...\n");
    for(i = 0; i < MAXFILES; i++)
    {
        if(client.file_info[i].size > 0)
        {
            bitMap[i] = 2;
        }
    }
    fprintf(stderr, "sync file...\n");

    safe_recvINT(socket, &numberFilesClient);

    fprintf(stderr, "O usuario tem %d arquivos\n", numberFilesClient);

    // Check to see which files are sync
    // bitMap keeps track of them
    time_t lastModified;
    for(i = 0; i < numberFilesClient; i++)
    {
        safe_recv(socket, filename, MAXFILENAME);
        safe_recv(socket, &lastModified, sizeof(time_t));

        fprintf(stderr, "arquivo %s\n", filename);

        // find file in the client dir
        if( (j = findFile_ClientDir(client, filename)) < 0)
        {
            continue;
        }

        // if the file in the user computer is sync
        if(!difftime(lastModified, client.file_info[j].time_lastModified ))
        {
            fprintf(stderr, "arquivo %s ta sync\n", filename);
            bitMap[j] = 1;
            count++;
        }

    }

    // send number of files that are not sync
    count = numberFilesServer - count;
    safe_sendINT(socket, &count);

    fprintf(stderr, "%d vao ser enviados dos %d arquivo no server\n", count, numberFilesServer);
    // send files that are not sync
    for(i = 0; i < MAXFILES; i++)
    {
        //fprintf(stderr, "%d\n", bitMap[i]);
        if(bitMap[i] == 2)
        {
            strcpy(pathFile, dir);
            strcat(pathFile, client.file_info[i].name);

            int fs = client.file_info[i].size;

            fprintf(stderr, "enviando %s %d bytes\n", client.file_info[i].name, fs);

            send(socket, client.file_info[i].name, MAXFILENAME, 0);
            safe_sendINT(socket, &fs);
            send(socket, &(client.file_info[i].time_lastModified), sizeof(time_t), 0);

            FILE *fp = fopen(pathFile, "r");

            int sent_bytes, offset = 0;

            while(offset < fs)
            {
                int bytes_send = fread(buffer, sizeof(char), BUFFER_SIZE, fp);
                sent_bytes = send(socket, (char*) buffer, bytes_send, 0);
                offset += sent_bytes;
            }

            fclose(fp);

        }
    }

    fprintf(stderr, "sync done\n\n");
}
void send_file(char *file, CLIENT client, int socket)
{
    char buffer[BUFFER_SIZE], *filename = malloc(MAXFILENAME);

    int fs;
    fprintf(stderr, "download file...\n");

    if( strrchr(file, '/') )
    {
        strcpy(filename,strrchr(file, '/') + 1);
    }
    else
    {
        strcpy(filename, file);
    }

    int i;

    if( (i = findFile_ClientDir(client, filename)) < 0)
    {
        // file doens't exist in dir
        fs = 0;
    }
    else
    {
        fs = client.file_info[i].size;
    }

    fprintf(stderr, "a %d\n", fs);

    safe_sendINT(socket, &fs);
    send(socket, &(client.file_info[i].time_lastModified), sizeof(time_t), 0);


    FILE *fp;
    if(fs) fp = fopen(file, "r");

    int sent_bytes, offset = 0;

    while(offset < fs)
    {
        int bytes_send = fread(buffer, sizeof(char), BUFFER_SIZE, fp);
        sent_bytes = send(socket, (char*) buffer, bytes_send, 0);
        offset += sent_bytes;
    }

    if(fs) fclose(fp);
    fprintf(stderr, "download done\n\n");
}
void receive_file(char *file, CLIENT *client, int socket)
{
    char buffer[BUFFER_SIZE], *filename = malloc(MAXFILENAME);
    int fs;

    if( strrchr(file, '/') )
    {
        strcpy(filename,strrchr(file, '/') + 1);
    }
    else
    {
        strcpy(filename, file);
    }
    safe_recvINT(socket, &fs);

    // Send last modified time (time_t)
    time_t lm;
    safe_recv(socket, &lm, sizeof(time_t));

    if(fs)
    {
        FILE *fp = fopen(file, "w");

        fprintf(stderr, "file size: %d\n", fs);

        int acc = 0, read, maxRead;
        while (acc < fs)
        {

            // receive at most 1MB of data
            read = recv(socket, buffer, BUFFER_SIZE, 0);

            // write the received data in the file
            fwrite(buffer, sizeof(char), read, fp);

            acc += read;
        }

        // if the dir was full before, delete the received file
        // else add to the struct

        pthread_mutex_lock(&(client->mutex));
        int full = isFull_ClientDir(*client);
        if(full)
        {
            remove(file);
        }
        else
        {
            addFile_ClientDir(client, filename, fs, lm);
        }
        pthread_mutex_unlock(&(client->mutex));

        fclose(fp);

        struct utimbuf new_times;
        new_times.actime = time(NULL);
        new_times.modtime = lm;    /* set mtime to current time */
        utime(file, &new_times);
    }
    fprintf(stderr, "upload done\n\n");

}


void initServer()
{
    char *user, *pathFile, *dir_prefix, c;
    pathFile = malloc(MAXPATH);
    DIR *serverdir;
    struct dirent *mydir;
    user = getLinuxUser();
    pathFile = strcpy(pathFile, "/home/");
    pathFile = strcat(pathFile, user);
    dir_prefix = malloc(MAXPATH);
    dir_prefix = strcpy(dir_prefix, "server_dir_");

    pthread_mutex_init(&(server_mutex_), NULL);

    if ((serverdir = opendir(pathFile)) == NULL)
    {
        fprintf(stderr, "Can't open %s\n", pathFile);
        return -1;
    }
    char *name;
    char *clientname;
    struct dirent *myfile;
    DIR *clientdir;
    char *pathclient;
    clientname = malloc(MAXPATH);
    name = malloc(MAXPATH);
    pathclient = malloc(MAXPATH);
    while((mydir = readdir(serverdir)) != NULL)
    {
        int n = 11, i = 0, fileindex=0;
        memset(clientname, '\0', MAXPATH);
        if(strncmp(dir_prefix, mydir->d_name, 11) == 0)
        {

            name = strcpy(name, mydir->d_name);
            do
            {
                clientname[i] = name[n];
                n++;
                i++;
            }
            while(name[n]!='\0');

            CLIENT_LIST *clt = user_verify(clientname);
            memset(pathclient, '\0', MAXPATH);
            pathclient = strcpy(pathclient, pathFile);
            pathclient = strcat(pathclient, "/");
            pathclient = strcat(pathclient, dir_prefix);
            pathclient = strcat(pathclient, clientname);
            pathclient = strcat(pathclient, "/");
            if ((clientdir = opendir(pathclient)) == NULL)
            {
                fprintf(stderr, "Can'et open %s\n", pathclient);
                return 0;
            }
            while((myfile = readdir(clientdir)) != NULL)
            {
                if (myfile->d_type == DT_REG)
                {
                    if(myfile->d_name[strlen(myfile->d_name)-1] != '~')
                    {
                        FILE_INFO fileinf;
                        char *filepath;
                        filepath = malloc(MAXPATH);
                        filepath = strcpy(filepath, pathclient);
                        filepath = strcat(filepath,myfile->d_name);
                        strcpy(fileinf.name,myfile->d_name);
                        fileinf.time_lastModified = *file_lastModifier(filepath);
                        fileinf.size = file_size(filepath);
                        clt->cli.file_info[fileindex] = fileinf;
                        fileindex++;
                    }
                }
            }
            closedir(clientdir);
            fileindex=0;
        }
    }
    closedir(serverdir);
}

int main (int argc, char *argv[])
{
    running_ = 1;
    int socket_user, id_thread;
    char id_user[MAXNAME];
    pthread_t thread;
    USER_INFO user_info;
    fprintf(stderr, "%d\n", argc);
    if(argc > 1) port_ = atoi(argv[1]);

    if(connect_client() == 0)
        printf("Server Connected!!!\n");

    initializerList(); //Inicializa a lista de clientes
    initServer();
    while(running_)
    {
        socket_user = acceptLoop();
        if(socket_user >= 0)
        {
            safe_recv(socket_user, id_user, MAXNAME);
            strcpy(user_info.username, id_user);
            user_info.socket = socket_user;
            pthread_create(&thread, NULL, client_handling, (void *)&user_info); //Thread que processa login/requisições
        }
    }

    return 0;
}
