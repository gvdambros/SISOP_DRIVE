#include "dropboxClient.h"
#include "dropboxUtil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

// #include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <pthread.h>
#include <linux/inotify.h>
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

#define BUFFER_SIZE 1024*1024

int connect_server(char *host, int port)
{
    //int socket_client;
    struct sockaddr_in socket_server;

    socket_client = socket(AF_INET , SOCK_STREAM , 0);

    if (socket_client == -1)
    {
        printf("Could not create socket\n");
        return -1;
    }

    socket_server.sin_addr.s_addr = inet_addr(host);
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
    uint16_t aux;
    int sent_bytes = recv(socket_client, &aux, sizeof(int), 0);
    int size = ntohs(size);

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
    int aux = htons(fs);
    int sent_bytes = send(socket_client, &aux, sizeof(int), 0);

    printf("%d %d \n", sent_bytes, fs);

    // send all the content at once to the server
    FILE *fp = fopen(file, "r");
    char buffer[fs + 1];
    fread(buffer, fs, 1, fp);
    sent_bytes = send(socket_client, &(buffer), fs, 0);

    sem_post(&runningRequest);

    return;
}

void delete_file(char *file){
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

    while(1)
    {

        sleep(1);

        if( (numOfChanges = read( guard, buffer, EVENT_BUF_LEN )) < 0)
        {
            printf("READ error\n");
        }

        i = 0;
        while ( i < numOfChanges )
        {
            struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
            if ( event->len )
            {
                if ( (event->mask & IN_CREATE) || (event->mask & IN_MODIFY) )
                {
                    // server has to decide if it was a new file or a modification by seeing if the file already exists.
                    send_file(event->name);
                }
                else if ( event->mask & IN_DELETE )
                {
                    // new function.
                    delete_file(event->name);
                }
                else if( event->mask & IN_MOVED_FROM)
                {   // file rename is a IN_MOVED_FROM followed by a IN_MOVED_TO event
                    // maybe there is a better way to do it
                    printf( "File %s moved from .\n", event->name );
                }
                else if( event->mask & IN_MOVED_TO)
                {
                    printf( "File %s moved to.\n", event->name );
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
    else{
        send_id(argv[1]);
    }

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
            send_file("a.txt");
        }
        else if(!strcmp(userCmd.cmd, "download"))
        {
            printf("Download\n");

        }
        else if(!strcmp(userCmd.cmd, "up"))
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

    return 0;
}
