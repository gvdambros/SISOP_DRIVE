#include "dropboxServer.h"
#include "logUtil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
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
        return ERROR;
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
    return SUCCESS;
}

CLIENT* searchInClientList(CLIENT nodo)
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

int server_turnUp()
{

    struct sockaddr_in serv_addr;

    if ((socket_server_ = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Error opening socket\n");
        return ERROR;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(serv_addr.sin_zero), 8);

    if (bind(socket_server_, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Error on binding\n");
        return ERROR;
    }

    listen(socket_server_, 5);

    return SUCCESS;
}

int acceptLoop()
{
    int new_socket_client;
    struct sockaddr_in cli_addr;
    socklen_t clilen;

    clilen = sizeof(struct sockaddr_in);
    if ((new_socket_client = accept(socket_server_, (struct sockaddr *) &cli_addr, &clilen)) < 0){
        fprintf(stderr, "Error on accept\n");
    }
       
    return new_socket_client;
}

int createUserConfigFile(CLIENT new_user, char *dir){
    char *config = malloc(MAXPATH);
    strcpy(config, dir);
    config = strcat(config, ".config");
    FILE *f = fopen(config, "w");
    if (f == NULL)
    {
        free(config);
        fprintf(stderr, "Error creating config file!\n");
        return ERROR;
    }

    fclose(f);
    free(config);
    return SUCCESS;
}

int createUserDir(CLIENT new_user){

    char *dir = malloc(MAXPATH);
    char *user = getLinuxUser();

    dir = strcpy(dir, "/home/");
    dir = strcat(dir, user);
    dir = strcat(dir, "/server_dir_");
    dir = strcat(dir, new_user.user_id);
    dir = strcat(dir, "/");

    if( !dir_exists (dir) )
    {
        mkdir(dir, 0777);
    }

    if( createUserConfigFile(new_user, dir) != SUCCESS ){
        free(dir);
        return ERROR;
    }

    free(dir);
    return SUCCESS;
}

int readConfigFile(char *dir, char **user_id, char **user_password){
    char *config = malloc(MAXPATH);
    strcpy(config, dir);
    config = strcat(config, ".config");
    FILE *f = fopen(config, "r");

    if (f == NULL)
    {
        free(config);
        fprintf(stderr, "Error reading config file!\n");
        return ERROR;
    }

    fscanf(f, "%s", *user_id);
    fscanf(f, "%s", *user_password);

    fclose(f);
    free(config);
    return SUCCESS;
}

CLIENT* addUser(char *user_id, char *user_password)
{
    CLIENT new_user;

    strcpy(new_user.user_id, user_id);
    strcpy(new_user.user_password, user_password);

    pthread_mutex_lock(&server_mutex_);

        //Registra e retorna um novo cliente
        new_user.devices[0] = 0;
        new_user.devices[1] = 0;
        new_user.logged_in = 0;
        initFiles_ClientDir(&new_user);

        pthread_mutex_init(&(new_user.mutex), NULL);

        if (insertList(new_user) != 0)
        {
            pthread_mutex_unlock(&server_mutex_);
            return NULL;
        } else {
            pthread_mutex_unlock(&server_mutex_);
            return searchInClientList(new_user);
        }
}



CLIENT* searchUser(char *user_id, char *user_password)
{

    //Função responsável pela verificação da existência/registro de um cliente
    //INSERIR LOCKS

    CLIENT new_user;
    CLIENT *existing_user;

    strcpy(new_user.user_id, user_id);
    strcpy(new_user.user_password, user_password);

    pthread_mutex_lock(&server_mutex_);
    existing_user = searchInClientList(new_user);
    if (existing_user != NULL)
    {
        pthread_mutex_unlock(&server_mutex_);
        return existing_user;
    } else {
        pthread_mutex_unlock(&server_mutex_);
        return NULL;
    }
}

int clientValidate(CLIENT *client, char *user_id, char *user_password)
{

    if(strcmp(client->user_id, user_id) != 0) {
        fprintf(stderr, "Invalid username\n");
        return -1;
    }

    if(strcmp(client->user_password, user_password) != 0) {
        fprintf(stderr, "Invalid password\n");
        return -1;
    }

    //Verifica se o usuário já está logado e tenta realizar o login. Retorna o id do Device utilizado caso obtenha sucesso.
    //INSERIR LOCKS

    if (client->logged_in == 0) {
        client->logged_in = 1;
        client->devices[0] = 1;
        return 0;
    } else if (client->devices[0] == 0) {
        client->logged_in = 1;
        client->devices[0] = 1;
        return 0;
    }
    else if (client->devices[1] == 0) {
        client->logged_in = 1;
        client->devices[1] = 1;
        return 1;
    } else {
        return -1;
    }
}

int clientLogin(char *user_id, char *user_password, int user_socket, CLIENT **current_client){
    *current_client = searchUser(user_id, user_password);    //Nodo da lista contendo as infos do usuário

    if(*current_client == NULL){
        fprintf(stderr, "New user will be created.\n");
        *current_client = addUser(user_id, user_password);
        if(*current_client == NULL){
            fprintf(stderr, "Error while creating user.\n");
            return -1;
        }
        if( createUserDir(**current_client) != 0){
            fprintf(stderr, "Error while creating user directory.\n");
            return -1;
        }
    }

    int response, device = -1;                //Device ativo para esta thread

    device = clientValidate(*current_client, user_id, user_password); //Tenta realizar o login do usuário

    (device >= 0) ? (response = SUCCESS) : (response = ERROR);

    safe_sendINT(user_socket, &response);

    return device;
}

int clientLogout(CLIENT *client, int device)
{
    client->devices[device] = 0;
    if ((client->devices[0] == 0) && (client->devices[1] == 0))
    {
        client->logged_in = 0;
    }
    return SUCCESS;
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

    CLIENT *current_client;

    int device = clientLogin(user_id, user_password, socket_user, &current_client);
    
    if (device < 0)
    {
        fprintf(stderr, "User login canceled\n");
        close(socket_user);   //Fecha o socket
        pthread_exit(0);    //Se não for possível, encerra a thread
    }           

    char request[MAXNAME], *user = getLinuxUser(), *dir = malloc(MAXPATH*sizeof(char)), *pathFile = malloc(MAXPATH*sizeof(char));

    dir = strcpy(dir, "/home/");
    dir = strcat(dir, user);
    dir = strcat(dir, "/server_dir_");
    dir = strcat(dir, user_id);
    dir = strcat(dir, "/");

    while (running_)
    {
        safe_recv(socket_user, request, MAXREQUEST);
        user_cmd commandLine = string2userCmd(request);

        if (strcmp(commandLine.cmd, "sync") == 0)   //Solicitação de Sincronização (sync_client())
        {
            sync_server(*current_client, socket_user);
        }
        else if (strcmp(commandLine.cmd, "download") == 0)
        {
            strcpy(pathFile, dir);
            strcat(pathFile, commandLine.param);
            send_file(pathFile, *current_client, socket_user);
        }
        else if (strcmp(commandLine.cmd, "upload") == 0)
        {
            strcpy(pathFile, dir);
            strcat(pathFile, commandLine.param);
            receive_file(pathFile, current_client, socket_user);
        }
        else if (strcmp(commandLine.cmd, "delete") == 0)
        {
            fprintf(stderr, "delete file %s...\n", commandLine.param);

            pthread_mutex_lock(&(current_client->mutex));

            printFiles_ClientDir(*current_client);

            deleteFile_ClientDir(current_client, commandLine.param);

            printFiles_ClientDir(*current_client);
            pathFile = strcpy(pathFile, dir);
            pathFile = strcat(pathFile, commandLine.param);
            remove(pathFile);
            pthread_mutex_unlock(&(current_client->mutex));

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
            fprintf(stderr, "Logout\n");
            clientLogout(current_client, device); //Realiza o logout
            close(socket_user);
            pthread_exit(SUCCESS);
        }
        else
        {
            fprintf(stderr, "I received a request I'm not prepared to handle :(\n");
        }
    }
    pthread_exit(SUCCESS);
}


void sync_server(CLIENT client, int socket)
{
    logBeginningFunction("sync_server");
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

    for(i = 0; i < MAXFILES; i++)
    {
        if(client.file_info[i].size > 0)
        {
            bitMap[i] = 2;
        }
    }

    safe_recvINT(socket, &numberFilesClient);

    fprintf(stderr, "The user has %d files\n", numberFilesClient);

    // Check to see which files are sync
    // bitMap keeps track of them
    time_t lastModified;
    for(i = 0; i < numberFilesClient; i++)
    {
        safe_recv(socket, filename, MAXFILENAME);
        safe_recv(socket, &lastModified, sizeof(time_t));

        fprintf(stderr, " - %s ", filename);

        // find file in the client dir
        if( (j = findFile_ClientDir(client, filename)) < 0)
        {
            // if this file is not in server, another client deleted it.
            strcpy(deletedFiles[numberOfDeletedFiles++].name, filename);
        }
        // if the file in the user computer is sync
        else if(!difftime(lastModified, client.file_info[j].lastModified ))
        {
            fprintf(stderr, " (sinc)\n", filename);
            bitMap[j] = 1;
            count++;
        }
        else fprintf(stderr, "\n");

    }

    // send number of files that are not sync
    count = numberOfDeletedFiles + numberFilesServer - count;
    safe_sendINT(socket, &count);

    // send files that are not sync
    for(i = 0; i < MAXFILES; i++)
    {
        //fprintf(stderr, "%d\n", bitMap[i]);
        if(bitMap[i] == 2)
        {
            strcpy(pathFile, dir);
            strcat(pathFile, client.file_info[i].name);

            int fs = client.file_info[i].size;

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

    logEndingFunction("sync_server");
}

void send_file(char *file, CLIENT client, int socket)
{

    logBeginningFunction("send_file");

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
    logEndingFunction("send_file");
}

void receive_file(char *file, CLIENT *client, int socket)
{
    char buffer[BUFFER_SIZE], *filename = malloc(MAXFILENAME);
    int fs;
    
    logBeginningFunction("receive_file");

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

    logEndingFunction("receive_file");

}

int initServer()
{

    logBeginningFunction("initServer");

    char *user, *pathFile, *dir_prefix, c;
    pathFile = malloc(MAXPATH);
    DIR *serverdir;
    struct dirent *mydir;
    user = getLinuxUser();
    pathFile = strcpy(pathFile, "/home/");
    pathFile = strcat(pathFile, user);
    pathFile = strcat(pathFile, "/");
    dir_prefix = malloc(MAXPATH);
    dir_prefix = strcpy(dir_prefix, "server_dir_");

    pthread_mutex_init(&(server_mutex_), NULL);

    if ((serverdir = opendir(pathFile)) == NULL)
    {
        logExceptionInFunction("initServer", "Can't open server directory");
        return ERROR;
    }
    char *dir;
    char *clientname;
    char *clientpassword;
    struct dirent *myfile;
    DIR *clientdir;
    char *pathclient;
    clientname = malloc(MAXNAME);
    clientpassword = malloc(MAXNAME);
    dir = malloc(MAXPATH);
    
    fprintf(stderr, "Updating user list\n");

    while((mydir = readdir(serverdir)) != NULL)
    {
        int n = 11, i = 0, fileindex=0;
        memset(clientname, '\0', MAXPATH);
        if(strncmp(dir_prefix, mydir->d_name, 11) == 0)
        {
            strcpy(dir, pathFile);
            dir = strcat(dir, mydir->d_name);
            dir = strcat(dir, "/");

            readConfigFile(dir, &clientname, &clientpassword);

            CLIENT *clt = addUser(clientname, clientpassword);

            if ((clientdir = opendir(dir)) == NULL)
            {
                logExceptionInFunction("initServer", "Can't open user directory");
                return ERROR;
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
                        filepath = strcpy(filepath, dir);
                        filepath = strcat(filepath,myfile->d_name);
                        strcpy(fileinf.name, myfile->d_name);
                        fileinf.lastModified = file_lastModified(filepath);
                        fileinf.size = file_size(filepath);
                        clt->file_info[fileindex] = fileinf;
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
    
    logEndingFunction("initServer");
    return SUCCESS;
}

int main (int argc, char *argv[])
{
    int socket_user, id_thread;
    char login[MAXNAME*2 + 2];
    pthread_t thread;
    USER_INFO user_info;
    if(argc > 1) port_ = atoi(argv[1]);

    server_turnUp();

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
