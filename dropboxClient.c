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

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <pthread.h>
#include <linux/inotify.h>
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

#define BUFFER_SIZE 1024*1024

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

    return 0;
}

void close_connection()
{
    close(socket_client);
}

int sync_client()
{
    char dir[100] = "/home/";
    char *user; // nome do usuario linux
    int i;
    char buf[BUFFER_SIZE]; //buffer 1MG
    char* request = (char*) malloc(MAXREQUEST);
    char* fileName = (char*) malloc( MAXFILENAME );
    char* filePath = (char*) malloc(200);

    user = getLinuxUser();
    strcat(dir, user);
    strcat(dir, "/sync_dir_");
    strcat(dir, name_client);

    if( !dir_exists (dir) )
    {
        mkdir(dir, 0777);
    }

    // send request to sync
    // always send the biggest request possible
    strcpy(request,"sync");
    send(socket_client, request, MAXREQUEST, 0);

    struct dirent *dp;
    DIR *dfd;

    if ((dfd = opendir(dir)) == NULL)
    {
        fprintf(stderr, "Can't open %s\n", dir);
        return 0;
    }

    int numberFiles = numberOfFiles(dir);
    int aux = htons(numberFiles);
    send(socket_client, &aux, sizeof(int), 0);

    for(i = 0; i < numberFiles; i++){
        dp = readdir(dfd);
        if (dp->d_type == DT_REG){
            struct stat stbuf ;
            sprintf( filePath, "%s/%s",dir,dp->d_name) ;
            if( stat(filePath,&stbuf ) == -1 )
            {
                fprintf(stderr, "Unable to stat file: %s\n", filePath) ;
                return;
            }

            // Send name of fime
            strcpy(request,dp->d_name);
            send(socket_client, request, MAXREQUEST, 0);

            // Send last modified time (time_t)
            send(socket_client, &(stbuf.st_ctime), sizeof(stbuf.st_ctime), 0);

            fprintf(stderr,"%s\n",dp->d_name) ;
        }

    }

    // receive number of files that are not sync
    safe_recv(socket_client, &numberFiles, sizeof(int));
    numberFiles = ntohs(numberFiles);

    for(i = 0; i < numberFiles; i++){

        // receive file name from server
        safe_recv(socket_client, fileName, MAXFILENAME);

        // receive file size from server
        int size;
        safe_recv(socket_client, &size, sizeof(int));
        size = ntohs(size);

        // create the file
        sprintf(filePath, "%s/%s",dir,fileName) ;
        FILE *fp = fopen(filePath, "w");

        // while it didn't read all the file, keep reading
        int acc = 0, read = 0;
        while (acc < size)
        {
            // receive at most 1MB of data
            read = safe_recv(socket_client, buf, BUFFER_SIZE);

            // write the received data in the file
            fwrite(buf, sizeof(char), read, fp);

            printf("dados: %d\n", read);
            acc += read;
        }
    }
    return 0;
}

void get_file(char *file)
{

    char buf[BUFFER_SIZE]; //buffer 1MG

    char* request = (char*) malloc(MAXREQUEST);

    // send request to upload of file
    // always send the biggest request possible
    strcpy(request,"download ");
    strcat(request, file);
    send(socket_client, request, MAXREQUEST, 0);

    // receive file size from server
    int size;
    int sent_bytes = safe_recv(socket_client, &size, sizeof(int));
    fprintf(stderr, "size antes: %d\n",size);
    size = ntohs(size);
    fprintf(stderr, "size depois: %d\n",size);

    // create the file
    FILE *fp = fopen(file, "w");

    // while it didn't read all the file, keep reading
    int acc = 0;
    while (acc < size)
    {
        // receive at most 1MB of data
        int read = recv(socket_client, buf, BUFFER_SIZE, 0);

        // write the received data in the file
        fwrite(buf, sizeof(char), read, fp);

        printf("dados: %d\n", read);
        acc += read;
    }

    fclose(fp);

    return;
}

void send_file(char *file)
{
    char* request = (char*) malloc(MAXREQUEST);

    sem_wait(&runningRequest);

    // send request to upload of file
    // always send the biggest request possible
    strcpy(request,"upload ");
    strcat(request, file);
    send(socket_client, request, MAXREQUEST, 0);

    // get file size and send it to server
    int fs = file_size(file);
    fs = htons(fs);

    int sent_bytes = send(socket_client, &fs, sizeof(int), 0);

    printf("%d %d \n", sent_bytes, fs);

    // send all the content at once to the server
    FILE *fp = fopen(file, "r");
    char buffer[BUFFER_SIZE];

    int offset = 0;

    while(offset < fs)
    {
        int bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, fp);
        sent_bytes = send(socket_client, (char*) buffer, bytes_read, 0);
        offset += sent_bytes;
    }

    printf("sending done\n");

    fclose(fp);

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
    send(socket_client, request, MAXREQUEST, 0);

    return;
}

void send_id(char *id)
{
    char* request = (char*) malloc(MAXNAME);

    // send id to server
    // always send the biggest name possible
    strcpy(request, id);
    send(socket_client, request, MAXNAME, 0);

    return;
}

void *sync_function()
{
    int guard, watch, numOfChanges, i;
    char buffer[EVENT_BUF_LEN];

    if((guard = inotify_init())< 0)
    {
        printf("INOTIFY not initializated\n");
    }
    watch = inotify_add_watch( guard, "sync_dir_gvdambros", IN_MOVED_FROM | IN_MOVED_TO | IN_MODIFY | IN_CREATE | IN_DELETE );

    // Setting non-blocking read for guard
    int flags = fcntl(guard, F_GETFL, 0);
    fcntl(guard, F_SETFL, flags | O_NONBLOCK);

    while(running)
    {

        sleep(1);

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
                if ( (event->mask & IN_CREATE) || (event->mask & IN_MODIFY) || (event->mask & IN_MOVED_TO))
                {
                    // server has to decide if it was a new file or a modification by seeing if the file already exists.
                    printf("add %s\n", event->name);
                    send_file(event->name);
                }
                else if ( (event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM) )
                {
                    // new function.
                    delete_file(event->name);
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }
    inotify_rm_watch( guard, watch );
    close( guard );

    printf("Sync Thread ended\n");
    return;

}

int main(int argc, char *argv[])
{

    if(argc <= 3)
    {
        printf("call ./dropboxClient fulano endereço porta\n");
        //return -1;
    }

    printf("%s %s\n",argv[2], argv[3]);

    if( connect_server(argv[2], atoi( argv[3] )) < 0)
    {
        printf("connection failed\n");
        //return -1;
    }
    else
    {
        strcpy(name_client, argv[1]);
        fprintf(stderr, "name: %s\n", name_client);
        send_id(argv[1]);
    }

    sync_client();

    running = 1;

    sem_init(&runningRequest, 0, 1); // only one request can be processed at the time
    pthread_create(&sync_thread, NULL, sync_function, NULL); // can happen that one user request and one sync request try to run together

    char cmd_line[MAXCMD + 2*MAXNAME + 2] = "";
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
            get_file(userCmd.param);
            printf("Download\n");

        }
        else if(!strcmp(userCmd.cmd, "list"))
        {
            sync_client();
            printf("List\n");

        }
        else if(!strcmp(userCmd.cmd, "get_sync_dir"))
        {
            printf("Get Sync DIR\n");

        }
        else if(strcmp(userCmd.cmd, "exit"))
        {
            send(socket_client, request, MAXREQUEST, 0);
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
