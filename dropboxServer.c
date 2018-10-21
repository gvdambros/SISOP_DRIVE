#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void initList()
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

USER_INFO* searchInClientList(CLIENT nodo)
{
    CLIENT_LIST *ptr;

    if(clientLst_ == NULL)
        return NULL; //Lista vazia

    ptr = clientLst_;
    int a;
    while(ptr != NULL)
    {
        a = strcmp(ptr->cli.user_id, nodo.user_id);
        if(a == 0)
            return &(ptr->cli); //Retorna um CLIENT_LIST
        else
            ptr = ptr->next;
    }
    return NULL;
}

int connectClient()
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


USER_INFO* verifyUser(char *user_id, char *user_password)
{

    //Função responsável pela verificação da existência/registro de um cliente
    //INSERIR LOCKS

    CLIENT new_user;
    CLIENT_LIST *existing_user;

    strcpy(new_user.user_id, user_id);
    strcpy(new_user.user_password, user_password);

    pthread_mutex_lock(&server_mutex_);
    existing_user = searchInClientList(new_user);
    if (existing_user == NULL)
    {

        //Registra e retorna um novo cliente
        printf("Registrando usuário: %s\n", user_id);
        new_user.devices[0] = 0;
        new_user.devices[1] = 0;
        new_user.logged_in = 0;
        initFiles_ClientDir(&new_user);
        pthread_mutex_init(&(new_user.mutex), NULL);

        if (insertList(new_user) != 0)
        {
            printf("Falha no registro do usuário\n");
            pthread_mutex_unlock(&server_mutex_);
            return NULL;
        }
        else
        {
            printf("Usuário registrado com sucesso\n");
            pthread_mutex_unlock(&server_mutex_);
            return searchInClientList(new_user);
        }
    } else {
        //Retorna um cliente existente
        printf("Usuário já registrado\n");
        pthread_mutex_unlock(&server_mutex_);
        return existing_user;
    }
}

int clientLogin(CLIENT *client)
{

    //Verifica se o usuário já está logado e tenta realizar o login. Retorna o id do Device utilizado caso obtenha sucesso.
    //INSERIR LOCKS

    if (client->logged_in == 0)
    {
        client->logged_in = 1;
        client->devices[0] = 1;
        printf("Login efetuado no dispositivo 0\n");
        return 0;
    }
    else if (client->devices[0] == 0)
    {
        client->logged_in = 1;
        client->devices[0] = 1;
        printf("Login efetuado no dispositivo 0\n");
        return 0;
    }
    else if (client->devices[1] == 0)
    {
        client->logged_in = 1;
        client->devices[1] = 1;
        printf("Login efetuado no dispositivo 1\n");
        return 1;
    }
    else
    {
        printf("Limite de dispositivos ativos atingido! (2)\n");
        return -1;
    }
}

int clientLogout(CLIENT *client, int device)
{

    //Realiza o logout do usuário devidamente
    //INSERIR LOCKS

    client->devices[device] = 0;
    if ((client->devices[0] == 0) && (client->devices[1] == 0))
    {
        client->logged_in = 0;
    }
    printf("Logout efetuado no dispositivo %d\n", device);
    return 0;
}

void client_handling(void* arg)
{
    USER_INFO user_info = *(USER_INFO*)arg;

    //Visa organizar todas as ações relacionadas a lidar com o login de um usuário e realizar suas solicitações
    char user_id[MAXNAME];
    char user_password[MAXNAME];

    int socket_user = user_info.socket;
    strcpy(user_id, user_info.username);
    strcpy(user_password, user_info.password);

    CLIENT_LIST* current_client = verifyUser(user_id, user_password);    //Nodo da lista contendo as infos do usuário
    int n, device = -1, n_files = 0;                //Device ativo para esta thread
    char filename[MAXREQUEST], request[MAXNAME], *dir, *pathFile;
    time_t filetime;
    char *user;

    device = clientLogin(&current_client->cli); //Tenta realizar o login do usuário
    if (device < 0)
    {
        printf("Solicitação de login cancelada\n\n");
        close(socket_user);   //Fecha o socket
        pthread_exit(0);    //Se não for possível, encerra a thread
    }

    dir = malloc(MAXPATH);
    pathFile = malloc(MAXPATH);
    user = getLinuxUser();

    dir = strcpy(dir, "/home/");
    dir = strcat(dir, user);
    dir = strcat(dir, "/server_dir_");
    dir = strcat(dir, user_id);
    dir = strcat(dir, "/");

    if( !dir_exists (dir) )
    {
        fprintf(stderr, "Criou diretorio em %s\n", dir);
        mkdir(dir, 0777);
    }

    while (running_)
    {
        safe_recv(socket_user, request, MAXREQUEST);
        fprintf(stderr, "------------------------------------\nSolicitacao: %s\n",request);
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
        else if(strcmp(commandLine.cmd, "time") == 0){
            time_t rawtime;
            struct tm * timeinfo;
            time ( &rawtime );
            timeinfo = localtime ( &rawtime );
            send(socket_user, &rawtime, sizeof(time_t), 0);
        }
        else if (strcmp(commandLine.cmd, "exit") == 0)
        {
            fprintf(stderr, "Saindo...\n\n");
            clientLogout(&current_client->cli, device); //Realiza o logout
            close(socket_user);
            pthread_exit(0);
        }
        else
        {
            printf("I received a request I'm not prepared to handle :(\n");
        }
    }
    printf("Service closed for %s\n", user_id);
    pthread_exit(0);
}


void sync_server(CLIENT client, int socket)
{
    fprintf(stderr, "Sync iniciado: ");
    int numberFilesClient, numberFilesServer = numberOfFiles_ClientDir(client), i, j, count = 0, numberOfDeletedFiles = 0;
    int *bitMap = calloc(MAXFILES, sizeof(int));
    char buffer[BUFFER_SIZE], *pathFile = malloc(MAXPATH), *user = malloc(MAXNAME), *dir = malloc(MAXPATH), *filename = malloc(MAXFILENAME);
    char* request = (char*) malloc(MAXREQUEST);

    FILE_INFO deletedFiles[MAXFILES];

    user = getLinuxUser();

    dir = strcpy(dir, "/home/");
    dir = strcat(dir, user);
    dir = strcat(dir, "/server_dir_");
    dir = strcat(dir, client.user_id);
    dir = strcat(dir, "/");
    // bitMap[i] == 0 -> espaço vazio
    // bitMap[i] == 1 -> arquivo sync
    // bitMap[i] == 2 -> arquivo não existe no usuario ou não esta sync


    for(i = 0; i < MAXFILES; i++)
    {
        if(client.file_info[i].size > 0)
        {
            bitMap[i] = 2;
        }
    }

    safe_recvINT(socket, &numberFilesClient);

    fprintf(stderr, "O usuário tem %d arquivos:\n", numberFilesClient);

    // Check to see which files are sync
    // bitMap keeps track of them
    time_t lastModified;
    for(i = 0; i < numberFilesClient; i++)
    {
        safe_recv(socket, filename, MAXFILENAME);
        safe_recv(socket, &lastModified, sizeof(time_t));

        fprintf(stderr, " - arquivo: %s ", filename);

        // find file in the client dir
        if( (j = findFile_ClientDir(client, filename)) < 0)
        {
            // if this file is not in server, another client deleted it.
            strcpy(deletedFiles[numberOfDeletedFiles++].name, filename);
        }
        // if the file in the user computer is sync
        else if(!difftime(lastModified, client.file_info[j].lastModified ))
        {
            fprintf(stderr, " (sincronizado)\n", filename);
            bitMap[j] = 1;
            count++;
        }
        else fprintf(stderr, "\n");

    }

    // send number of files that are not sync
    count = numberOfDeletedFiles + numberFilesServer - count;
    safe_sendINT(socket, &count);

    fprintf(stderr, "%d vão ser enviados dos %d arquivos no servidor\n", count, numberFilesServer);
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

            strcpy(request,"add ");

            strcat(request, client.file_info[i].name);
            send(socket, request, MAXREQUEST, 0);

            safe_sendINT(socket, &fs);
            send(socket, &(client.file_info[i].lastModified), sizeof(time_t), 0);

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

    for(i = 0; i < numberOfDeletedFiles; i++)
    {
        strcpy(request,"delete ");
        strcat(request, deletedFiles[i].name);
        send(socket, request, MAXREQUEST, 0);
    }

    fprintf(stderr, "Sync completo\n------------------------------------\n\n");
}
void send_file(char *file, CLIENT client, int socket)
{
    char buffer[BUFFER_SIZE], *filename = malloc(MAXFILENAME);

    int fs;
    fprintf(stderr, "Download iniciado\n");

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

    fprintf(stderr, "Tamanho: %d\n", fs);

    safe_sendINT(socket, &fs);
    send(socket, &(client.file_info[i].lastModified), sizeof(time_t), 0);


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
    fprintf(stderr, "Download completo\n------------------------------------\n\n");
}

void receive_file(char *file, CLIENT *client, int socket)
{
    char buffer[BUFFER_SIZE], *filename = malloc(MAXFILENAME);
    int fs;

    fprintf(stderr, "Upload iniciado\n");

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

        fprintf(stderr, "Tamanho: %d\n", fs);

        int acc = 0, read, maxRead;
        while (acc < fs)
        {
            int maxRead;
            if( fs - acc < BUFFER_SIZE) maxRead = fs - acc;
            else maxRead = BUFFER_SIZE;

            // receive at most 1MB of data
            read = recv(socket, buffer, maxRead, 0);

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
    fprintf(stderr, "Upload completo\n------------------------------------\n\n");

}


int initServer()
{
    char *user, *pathFile, *dir_prefix, c;
    pathFile = malloc(MAXPATH);
    DIR *serverdir;
    struct dirent *mydir;
    printf("Máquina: ");
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

    printf("Atualizando lista de usuários\n");

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

            CLIENT_LIST *clt = verifyUser(clientname, "");
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
                        fileinf.lastModified = *file_lastModified(filepath);
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
    running_ = 1;
    printf("Pronto\n\n");
    return 0;
}

int main (int argc, char *argv[])
{
    int socket_user, id_thread;
    char login[MAXNAME*2 + 2];
    pthread_t thread;
    USER_INFO user_info;
    fprintf(stderr, "%d\n", argc);
    if(argc > 1) port_ = atoi(argv[1]);

    if(connectClient() == 0)
        printf("Servidor conectado!\n");

    initList(); //Inicializa a lista de clientes
    initServer();
    while(running_)
    {
        socket_user = acceptLoop();
        if(socket_user >= 0)
        {
            safe_recv(socket_user, login, MAXNAME*2 + 2);
            strcpy(user_info.username, strtok(login, " "));
            strcpy(user_info.password, strtok(NULL, " "));
            user_info.socket = socket_user;

            pthread_create(&thread, NULL, client_handling, (void *)&user_info); //Thread que processa login/requisições
        }
    }

    return 0;
}
