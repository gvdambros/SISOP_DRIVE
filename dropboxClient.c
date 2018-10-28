#include "dropboxClient.h"
#include "dropboxUtil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sendfile.h>

#include <utime.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <pthread.h>
#include <linux/inotify.h>
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

#include <time.h>


int connect_server(char *host, int port)
{
    //int socket_client;
    struct sockaddr_in socket_server;
    struct hostent *server;

    server = gethostbyname(host);
    socket_client = socket(AF_INET , SOCK_STREAM , 0);

    if (socket_client == -1)
    {
        printf("Could not create socket\n");
        return -1;
    }

    socket_server.sin_addr = *((struct in_addr *)server->h_addr);
    socket_server.sin_family = AF_INET;
    socket_server.sin_port = htons( port );

    if (connect(socket_client , (struct sockaddr *)&socket_server , sizeof(socket_server)) < 0)
    {
        printf("Could not connect\n");
        return -2;
    }

    int flag = 1;
    setsockopt(socket_client,IPPROTO_TCP,TCP_NODELAY,(char *)&flag,sizeof(flag));

    return SUCCESS;
}

void close_connection()
{
    close(socket_client);
}

void set_dir(char *base_dir, char *name_client)
{

    char *dir, *user; // nome do usuario linux


    user = getLinuxUser();

    dir = malloc(MAXPATH);
    dir = strcpy(dir, base_dir);
    dir = strcat(dir, "/sync_dir_");
    dir = strcat(dir, name_client);
    dir = strcat(dir, "/");

    printf("dir: %s\n", dir);

    if( !dir_exists (dir) )
    {
        mkdir(dir, 0777);
    }

    strcpy(dropboxDir_, dir);
}

int sync_client()
{
    char *pathFile = (char*) malloc(MAXPATH);
    int i;
    char buf[BUFFER_SIZE]; //buffer 1MG
    char* request = (char*) malloc(MAXREQUEST);
    char* fileName = (char*) malloc(MAXFILENAME);

    // send request to sync
    // always send the biggest request possible
    sem_wait(&runningRequest);
    strcpy(request,"sync");
    send(socket_client, request, MAXREQUEST, 0);

    int numberFiles = numberOfFiles_ClientDir(client_info);
    safe_sendINT(socket_client, &numberFiles);
    i = 0;

    fprintf(stderr, "number of file: %d\n", numberFiles);

    while(i < numberFiles)
    {

        int j = getIDoOfFileAtPosition_ClientDir(i++);

        fprintf(stderr, "file: %s %d\n", client_info.file_info[j].name, j);

        // Send name of file
        strcpy(request, client_info.file_info[j].name);
        send(socket_client, request, MAXFILENAME, 0);

        // Send last modified time (time_t)
        time_t lm = client_info.file_info[j].lastModified;
        send(socket_client, &lm, sizeof(time_t), 0);

        //fprintf(stderr,"%s\n",dp->d_name);
        //fprintf(stderr,"%s\n",buff);
    }

    // Receive number of files that are not sync
    safe_recvINT(socket_client, &numberFiles);

    if (numberFiles > 0)
    {
        fprintf(stderr, "Identificadas modificações no servidor\n%d arquivos serão recebidos\n", numberFiles);
    }

    for(i = 0; i < numberFiles; i++)
    {

        user_cmd serverCmd;

        // Receive request from server
        safe_recv(socket_client, request, MAXREQUEST);

        serverCmd = string2userCmd(request);

        if(!strcmp(serverCmd.cmd, "delete"))
        {
            fprintf(stderr, "O arquivo %s vai ser deletado.\n", serverCmd.param);
            sprintf( pathFile, "%s/%s",dropboxDir_,serverCmd.param) ;
            remove(pathFile);
            deleteFile_ClientDir(&client_info, serverCmd.param);
        }
        else if(!strcmp(serverCmd.cmd, "add"))
        {
            fprintf(stderr, "O arquivo %s vai ser adicionado.\n", serverCmd.param);

            // receive file size from server
            int size;
            safe_recvINT(socket_client, &size);

            // receive last modified time (time_t)
            time_t lm;
            safe_recv(socket_client, &lm, sizeof(time_t));

            pathFile = strcpy(pathFile, dropboxDir_);
            pathFile = strcat(pathFile, serverCmd.param);

            FILE *fp = fopen(pathFile, "w");

            //fprintf(stderr, "Recebendo: %s, tamanho: %d bytes\n", fileName, size);

            // while it didn't read all the file, keep reading
            int acc = 0, read = 0;
            while (acc < size)
            {
                int maxRead;
                if( size - acc < BUFFER_SIZE) maxRead = size - acc;
                else maxRead = BUFFER_SIZE;

                // receive at most 1MB of data
                read = recv(socket_client, buf, maxRead, 0);

                // write the received data in the file
                fwrite(buf, sizeof(char), read, fp);
                acc += read;
            }

            fclose(fp);

            struct utimbuf new_times;
            new_times.actime = time(NULL);
            new_times.modtime = lm;    /* set mtime to current time */
            utime(pathFile, &new_times);

            addFile_ClientDir(&client_info, serverCmd.param, size, lm);

        }

    }
    sem_post(&runningRequest);

    return 0;
}

time_t getTimeServer()
{
    time_t lm;
    char* request = (char*) malloc(MAXREQUEST);
    strcpy(request,"time");
    sem_wait(&runningRequest);
    send(socket_client, request, MAXREQUEST, 0);
    safe_recv(socket_client, &lm, sizeof(time_t));
    sem_post(&runningRequest);
    return lm;
}

void get_file(char *file)
{

    char buf[BUFFER_SIZE]; //buffer 1MG

    char* request = (char*) malloc(MAXREQUEST);

    // send request to upload of file
    // always send the biggest request possible
    strcpy(request,"download ");
    strcat(request, file);

    sem_wait(&runningRequest);
    send(socket_client, request, MAXREQUEST, 0);
    // receive file size from server
    int size;
    int sent_bytes = safe_recvINT(socket_client, &size);

    fprintf(stderr, "Tamanho: %d\n", size);

    // receive last modified time (time_t)
    time_t lm;
    safe_recv(socket_client, &lm, sizeof(lm));

    // create the file
    FILE *fp = fopen(file, "w");

    // while it didn't read all the file, keep reading
    int acc = 0;
    while (acc < size)
    {

        int maxRead;
        if( size - acc < BUFFER_SIZE) maxRead = size - acc;
        else maxRead = BUFFER_SIZE;
        // receive at most 1MB of data
        int read = recv(socket_client, buf, maxRead, 0);

        // write the received data in the file
        fwrite(buf, sizeof(char), read, fp);

        acc += read;
    }

    fclose(fp);

    struct utimbuf new_times;
    new_times.actime = time(NULL);
    new_times.modtime = lm;    /* set mtime to current time */
    utime(file, &new_times);

    sem_post(&runningRequest);
    fprintf(stderr, "Download completo\n\n");
    return;
}

void list_files()
{
    DIR *mydir;
    struct dirent *myfile;
    struct stat mystat;

    if ((mydir = opendir(dropboxDir_)) == NULL)
    {
        fprintf(stderr, "Can't open %s\n", dropboxDir_);
        return 0;
    }

    while((myfile = readdir(mydir)) != NULL)
    {
        stat(myfile->d_name, &mystat);
        printf("%d",mystat.st_size);
        printf(" %s\n", myfile->d_name);
    }
    closedir(mydir);
}

void initClient()
{
    char *pathFile = (char*) malloc(MAXPATH);
    char* fileName = (char*) malloc( MAXFILENAME );
    int i=0;

    struct dirent *dp;
    DIR *dfd;

    if ((dfd = opendir(dropboxDir_)) == NULL)
    {
        fprintf(stderr, "Can't open %s\n", dropboxDir_);
        return 0;
    }

    int numberFiles = numberOfFilesInDir(dropboxDir_);

    while(i < numberFiles)
    {
        dp = readdir(dfd);
        if (dp->d_type == DT_REG || dp->d_type == DT_UNKNOWN)
        {
            struct stat stbuf;
            sprintf( pathFile, "%s/%s",dropboxDir_,dp->d_name) ;
            if( stat(pathFile,&stbuf ) == -1 )
            {
                fprintf(stderr, "Unable to stat file: %s\n", pathFile) ;
                return;
            }

            fprintf(stderr, "file: %s %d\n", dp->d_name, stbuf.st_size) ;
            strcpy( client_info.file_info[i].name ,dp->d_name);
            client_info.file_info[i].size = stbuf.st_size;
            client_info.file_info[i].lastModified = stbuf.st_mtime;
            i++;
        }
    }
}

void send_file(char *file)
{
    char *request = (char*) malloc(MAXREQUEST), *filename = malloc(MAXFILENAME);
    int fs, aux, sent_bytes, offset = 0;
    char buffer[BUFFER_SIZE];

    sem_wait(&runningRequest);

    if( strrchr(file, '/') )
    {
        strcpy(filename,strrchr(file, '/') + 1);
    }
    else
    {
        strcpy(filename, file);
    }
    fprintf(stderr, "Nome do arquivo: %s\n", filename);

    if((fs = file_size(file)) < 0)
    {
        fs = 0;
    }

    time_t *lastModified;
    if( (lastModified = file_lastModified(file)) == NULL)
    {
        fs = 0;
        time_t aux = time(NULL);
        lastModified = &aux;
    }

    // send request to upload of file
    // always send the biggest request possible
    strcpy(request,"upload ");
    strcat(request, filename);
    send(socket_client, request, MAXREQUEST, 0);

    // send file size
    safe_sendINT(socket_client, &fs);

    // send last modified
    send(socket_client, lastModified, sizeof(time_t), 0);

    FILE *fp;
    if(fs) fp = fopen(file, "r");

    fprintf(stderr, "Tamanho: %d\n", fs);

    while(offset < fs)
    {
        int bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, fp);
        sent_bytes = send(socket_client, (char*) buffer, bytes_read, 0);
        offset += sent_bytes;
    }

    printf("Envio completo\n\n");

    if(fs) fclose(fp);

    sem_post(&runningRequest);

    return;
}

void delete_file(char *file)
{
    char* request = (char*) malloc(MAXREQUEST);

    // send request to delete file
    // always send the biggest request possible
    strcpy(request,"delete ");
    strcat(request, file);
    sem_wait(&runningRequest);
    send(socket_client, request, MAXREQUEST, 0);
    deleteFile_ClientDir(&client_info, file);
    sem_post(&runningRequest);
    return;
}

int send_id(char *username, char *password)
{
    char* request = (char*) malloc(MAXNAME*2 + 2);
    int response;

    // always send the biggest name possible
    strcpy(request, username);
    strcat(request, " ");
    strcat(request, password);

    printf("LOGIN: %s\n", request);

    send(socket_client, request, MAXNAME*2 + 2, 0);

    safe_recvINT(socket_client, &response);

    return response;
}

void *sync_function()
{
    int guard, watch, numOfChanges, i;
    char buffer[EVENT_BUF_LEN], *pathFile = malloc(MAXPATH);
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);


    if((guard = inotify_init())< 0)
    {
        printf("INOTIFY not initializated\n");
    }
    watch = inotify_add_watch( guard, dropboxDir_, IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE | IN_CREATE | IN_DELETE );

    // Setting non-blocking read for guard
    int flags = fcntl(guard, F_GETFL, 0);
    fcntl(guard, F_SETFL, flags | O_NONBLOCK);

    while(running)
    {

        sleep(10);

        if( (numOfChanges = read( guard, buffer, EVENT_BUF_LEN )) < 0)
        {
            //nothing was read
        }

        i = 0;
        while ( i < numOfChanges )
        {
            struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
            if ( event->len )
            {
                if ( (event->mask & IN_CREATE) || (event->mask & IN_CLOSE_WRITE) || (event->mask & IN_MOVED_TO))
                {
                    // server has to decide if it was a new file or a modification by seeing if the file already exists.
                    sprintf(pathFile, "%s%s",dropboxDir_,event->name) ;
                    fprintf(stderr, "Arquivo modificado na pasta local:  %s\n", event->name);

                    time_t t0 = time(NULL);
                    time_t serverTime = getTimeServer();
                    time_t t1 = time(NULL);
                    time_t clientTime = serverTime + (t1-t0)/2;

                    addFile_ClientDir(&client_info, event->name, file_size(pathFile), clientTime);

                    struct utimbuf new_times;
                    new_times.actime = time(NULL);
                    new_times.modtime = clientTime;    /* set mtime to current time */
                    utime(pathFile, &new_times);

                    send_file(pathFile);

                }
                else if ( (event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM) )
                {
                    // new function.
                    delete_file(event->name);
                }
            }
            i += EVENT_SIZE + event->len;
        }

        sync_client();

        t = time(NULL);
        tm = *localtime(&t);

        //printf("sync em: %d:%d:%d\n", tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
    inotify_rm_watch( guard, watch );
    close( guard );

    printf("Sync Thread ended\n");
    return;

}

int main(int argc, char *argv[])
{

    if(argc <= 5)
    {
        printf("call ./client usuario senha dir endereço porta\n");
        return -1;
    }

    if( connect_server(argv[4], atoi( argv[5] )) < 0)
    {
        fprintf(stderr, "Connection failed\n");
        return -1;
    } else {
        if (send_id(argv[1], argv[2]) < 0){
            fprintf(stderr, "Login failed\n");
        }
    }

    sem_init(&runningRequest, 0, 1); // only one request can be processed at the time
    set_dir(argv[3], argv[1]);
    initFiles_ClientDir(&client_info);
    initClient();
    printFiles_ClientDir(client_info);
    sync_client();

    running = 1;

    pthread_create(&sync_thread, NULL, sync_function, NULL); // can happen that one user request and one sync request try to run together


    char cmd_line[MAXREQUEST] = "";
    user_cmd userCmd;

    do
    {
        gets(cmd_line);
        userCmd = string2userCmd(cmd_line);

        if(!strcmp(userCmd.cmd, "upload"))
        {
            printf("Upload\n");
            send_file(userCmd.param);
        }
        else if(!strcmp(userCmd.cmd, "download"))
        {
            printf("Download\n");
            get_file(userCmd.param);
        }
        else if(!strcmp(userCmd.cmd, "list"))
        {
            printf("List\n");
            list_files();
        }
        else if(!strcmp(userCmd.cmd, "print"))
        {
            printf("Print\n");
            printFiles_ClientDir(client_info);
        }
        else if(!strcmp(userCmd.cmd, "time"))
        {
            printf("Time\n");
            fprintf (stderr, "%lld\n", (long long) getTimeServer());
        }
        else if(!strcmp(userCmd.cmd, "get_sync_dir"))
        {
            printf("Get Sync DIR\n");
        }
        else if(!strcmp(userCmd.cmd, "exit"))
        {
            send(socket_client, cmd_line, MAXREQUEST, 0);
        }

    }
    while(strcmp(userCmd.cmd, "exit"));

    printf("Finishing program...\n");

    // kill sync thread
    running = 0;
    pthread_join(sync_thread, NULL);

    close_connection();

    return 0;
}
