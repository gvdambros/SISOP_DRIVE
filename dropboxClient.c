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

    return 0;
}

void close_connection()
{
    close(socket_client);
}

void set_dir() {

    char *dir, *user; // nome do usuario linux

    user = getLinuxUser();

    dir = malloc(200);
    dir = strcpy(dir, "/home/");
    dir = strcat(dir, user);
    dir = strcat(dir, "/Dropbox/sync_dir_");
    dir = strcat(dir, name_client);
    dir = strcat(dir, "/");

    if( !dir_exists (dir) )
    {
        mkdir(dir, 0777);
    }

    strcpy(dropboxDir_, dir);
}

int sync_client()
{
    char *pathFile = (char*) malloc(200);;
    int i;
    char buf[BUFFER_SIZE]; //buffer 1MG
    char* request = (char*) malloc(MAXREQUEST);
    char* fileName = (char*) malloc( MAXFILENAME );

    // send request to sync
    // always send the biggest request possible
    strcpy(request,"sync");
    send(socket_client, request, MAXREQUEST, 0);

    struct dirent *dp;
    DIR *dfd;

    fprintf(stderr, "dir: %s\n", dropboxDir_);
    if ((dfd = opendir(dropboxDir_)) == NULL)
    {
        fprintf(stderr, "Can't open %s\n", dropboxDir_);
        return 0;
    }

    int numberFiles = numberOfFilesInDir(dropboxDir_);
    safe_sendINT(socket_client, &numberFiles);

    i = 0;
    while(i < numberFiles){
        dp = readdir(dfd);
        if (dp->d_type == DT_REG || dp->d_type == DT_UNKNOWN){
            i++;
            struct stat stbuf;
            sprintf( pathFile, "%s/%s",dropboxDir_,dp->d_name) ;
            if( stat(pathFile,&stbuf ) == -1 )
            {
                fprintf(stderr, "Unable to stat file: %s\n", pathFile) ;
                return;
            }

            // Send name of file
            strcpy(request,dp->d_name);
            send(socket_client, request, MAXFILENAME, 0);


            // Send last modified time (time_t)
            send(socket_client, &(stbuf.st_mtime), sizeof(stbuf.st_mtime), 0);

            char buff[20];
            struct tm * timeinfo;
            timeinfo = localtime (&stbuf.st_mtime);
            strftime(buff, sizeof(buff), "%b %d %H:%M", timeinfo);

            fprintf(stderr,"%s\n",dp->d_name) ;
            fprintf(stderr,"%s\n",buff) ;
        }

    }

    // receive number of files that are not sync
    safe_recvINT(socket_client, &numberFiles);

    fprintf(stderr, "%d vao ser recebidos\n",numberFiles );


    for(i = 0; i < numberFiles; i++){

        // receive file name from server
        safe_recv(socket_client, fileName, MAXFILENAME);

        // receive file size from server
        int size;
        safe_recvINT(socket_client, &size);

        // receive last modified time (time_t)
        time_t lm;
        safe_recv(socket_client, &lm, sizeof(time_t));

        pathFile = strcpy(pathFile, dropboxDir_);
        pathFile = strcat(pathFile, fileName);

        FILE *fp = fopen(pathFile, "w");

        fprintf(stderr, "recebendo %s %d bytes\n", fileName, size);

        // while it didn't read all the file, keep reading
        int acc = 0, read = 0;
        while (acc < size)
        {
            // receive at most 1MB of data
            read = recv(socket_client, buf, size, 0);

            // write the received data in the file
            fwrite(buf, sizeof(char), read, fp);

            printf("dados: %d\n", read);
            acc += read;
        }

        fclose(fp);

        struct utimbuf new_times;
        new_times.actime = time(NULL);
        new_times.modtime = lm;    /* set mtime to current time */
        utime(pathFile, &new_times);
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
    int sent_bytes = safe_recvINT(socket_client, &size);

    fprintf(stderr, "size: %d\n", size);

    // receive last modified time (time_t)
    time_t lm;
    safe_recv(socket_client, &lm, sizeof(lm));

    // create the file
    FILE *fp = fopen(file, "w");

    // while it didn't read all the file, keep reading
    int acc = 0;
    while (acc < size)
    {
        // receive at most 1MB of data
        int read = recv(socket_client, buf, size, 0);

        // write the received data in the file
        fwrite(buf, sizeof(char), read, fp);

        printf("dados: %d\n", read);
        acc += read;
    }

    fclose(fp);

    struct utimbuf new_times;
    new_times.actime = time(NULL);
    new_times.modtime = lm;    /* set mtime to current time */
    utime(file, &new_times);

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

void send_file(char *file)
{
    char *request = (char*) malloc(MAXREQUEST), *filename = malloc(MAXFILENAME);
    int fs, aux, sent_bytes, offset = 0;
    char buffer[BUFFER_SIZE];

    sem_wait(&runningRequest);

    if( strrchr(file, '/') ){
        strcpy(filename,strrchr(file, '/') + 1);
    }
    else{
        strcpy(filename, file);
    }
    fprintf(stderr, "filename: %s\n", filename);

    if((fs = file_size(file)) < 0){
        fs = 0;
    }

    time_t *lastModified;
    if( (lastModified = file_lastModifier(file)) == NULL){
        fs = 0;
        time_t aux = time(NULL);
        lastModified = &aux;
    }

    // send request to upload of file
    // always send the biggest request possible
    strcpy(request,"upload ");
    strcat(request, filename);
    send(socket_client, request, MAXREQUEST, 0);
    fprintf(stderr, "request: %s\n", request);

    // send file size
    safe_sendINT(socket_client, &fs);

    // send last modified
    send(socket_client, lastModified, sizeof(time_t), 0);

    FILE *fp;
    if(fs) fp = fopen(file, "r");

    fprintf(stderr, "size: %d\n", fs);

    while(offset < fs)
    {
        int bytes_read = fread(buffer, sizeof(char), fs, fp);
        sent_bytes = send(socket_client, (char*) buffer, bytes_read, 0);
        offset += sent_bytes;
    }

    printf("sending done\n");

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
    char buffer[EVENT_BUF_LEN], *pathFile = malloc(200);


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
                    sprintf( pathFile, "%s%s",dropboxDir_,event->name) ;
                    fprintf(stderr, "add %s\n", pathFile);
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
        return -1;
    }
    else
    {
        strcpy(name_client, argv[1]);
        fprintf(stderr, "name: %s\n", name_client);
        send_id(argv[1]);
    }

    set_dir();
    sync_client();

    running = 1;

    sem_init(&runningRequest, 0, 1); // only one request can be processed at the time
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
            get_file(userCmd.param);
            printf("Download\n");

        }
        else if(!strcmp(userCmd.cmd, "list"))
        {
            list_files();
            printf("List\n");

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
