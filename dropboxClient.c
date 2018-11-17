#include "dropboxClient.h"
#include "dropboxUtil.h"
#include "logUtil.h"


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

#include <time.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

CLIENT _clientInfo;

static int connectToServer(char *host, int port)
{
    logBeginningFunction("connectToServer");

    struct sockaddr_in socket_server;
    struct hostent *server;

    server = gethostbyname(host);
    socket_client = socket(AF_INET , SOCK_STREAM , 0);

    if (socket_client == -1)
    {
        logExceptionInFunction("connectToServer", "could not create socket");
        return ERROR;
    }

    socket_server.sin_addr = *((struct in_addr *)server->h_addr);
    socket_server.sin_family = AF_INET;
    socket_server.sin_port = htons( port );

    if (connect(socket_client , (struct sockaddr *)&socket_server , sizeof(socket_server)) < 0)
    {
        logExceptionInFunction("connectToServer", "could not connect");
        return ERROR;
    }

    int flag = 1;
    setsockopt(socket_client,IPPROTO_TCP,TCP_NODELAY,(char *)&flag,sizeof(flag));

    logEndingFunction("connectToServer");
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

    if( !dir_exists (dir) )
    {
        mkdir(dir, 0777);
    }

    strcpy(dropboxDir_, dir);
}

int sync_client()
{
    logBeginningFunction("sync_client");

    char *pathFile = (char*) malloc(MAXPATH);
    int i, numberOfFilesChanged;
    char buf[BUFFER_SIZE]; //buffer 1MG
    char* request = (char*) malloc(MAXREQUEST);
    char* fileName = (char*) malloc(MAXFILENAME);
    char message[1024];

    // send request to sync
    // always send the biggest request possible
    sem_wait(&runningRequest);
    strcpy(request,"sync");
    send(socket_client, request, MAXREQUEST, 0);

    int numberFiles = numberOfFiles_ClientDir(_clientInfo);
    safe_sendINT(socket_client, &numberFiles);
    i = 0;

    snprintf(message, sizeof(message), "Number of files in client directory: %d", numberFiles);
    logMiddleFunction(message);

    while(i < numberFiles)
    {

        int j = getIDoOfFileAtPosition_ClientDir(i++);

        snprintf(message, sizeof(message), "\tFile %d: %s", j, _clientInfo.file_info[j].name);
        logMiddleFunction(message);

        // Send name of file
        strcpy(request, _clientInfo.file_info[j].name);
        send(socket_client, request, MAXFILENAME, 0);

        // Send last modified time (time_t)
        time_t lm = _clientInfo.file_info[j].lastModified;
        send(socket_client, &lm, sizeof(time_t), 0);

    }

    // Receive number of files that are not sync
    safe_recvINT(socket_client, &numberOfFilesChanged);

    if (numberOfFilesChanged > 0)
    {
        snprintf(message, sizeof(message), "Changes were indetified and %d files will be synced", numberOfFilesChanged);
        logMiddleFunction(message);
    }

    for(i = 0; i < numberOfFilesChanged; i++)
    {

        user_cmd serverCmd;

        // Receive request from server
        safe_recv(socket_client, request, MAXREQUEST);

        serverCmd = string2userCmd(request);

        if(!strcmp(serverCmd.cmd, "delete"))
        {
            snprintf(message, sizeof(message), "The file %s will be deleted", serverCmd.param);
            logMiddleFunction(message);
            
            sprintf(pathFile, "%s/%s",dropboxDir_,serverCmd.param) ;
            remove(pathFile);
            deleteFile_ClientDir(&_clientInfo, serverCmd.param);
        }
        else if(!strcmp(serverCmd.cmd, "add"))
        {
            snprintf(message, sizeof(message), "The file %s will be added", serverCmd.param);
            logMiddleFunction(message);

            // receive file size from server
            int size;
            safe_recvINT(socket_client, &size);

            // receive last modified time (time_t)
            time_t lm;
            safe_recv(socket_client, &lm, sizeof(time_t));

            pathFile = strcpy(pathFile, dropboxDir_);
            pathFile = strcat(pathFile, serverCmd.param);

            FILE *fp = fopen(pathFile, "w");

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

            addFile_ClientDir(&_clientInfo, serverCmd.param, size, lm);

        }

    }
    sem_post(&runningRequest);

    logEndingFunction("sync_client");
    return SUCCESS;
}
 
time_t get_time()
{
    logBeginningFunction("get_time");

    time_t lm;
    char* request = (char*) malloc(MAXREQUEST);
    strcpy(request,"time");
    sem_wait(&runningRequest);
    send(socket_client, request, MAXREQUEST, 0);
    safe_recv(socket_client, &lm, sizeof(time_t));
    sem_post(&runningRequest);

    logEndingFunction("get_time");
    return lm;
}

int get_file(char *file)
{
    
    logBeginningFunction("get_file");

    char message[1024];
    snprintf(message, sizeof(message), "File %s will be downloaded\n", file);
    logMiddleFunction(message);

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

    // receive last modified time (time_t)
    time_t lm;
    safe_recv(socket_client, &lm, sizeof(lm));

    // create the file
    FILE *fp = fopen(file, "w");

    if(!fp){
        sem_post(&runningRequest); 
        logExceptionInFunction("get_file", "could not create file");
        return ERROR;
    }

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

    logEndingFunction("get_file");
    return SUCCESS;
}

int list_files()
{

    logBeginningFunction("list_files");

    DIR *mydir;
    struct dirent *myfile;
    struct stat mystat;
    char message[1024];

    if ((mydir = opendir(dropboxDir_)) == NULL)
    {
        logExceptionInFunction("list_files", "Can't open directory");
        return ERROR;
    }

    while((myfile = readdir(mydir)) != NULL)
    {
        stat(myfile->d_name, &mystat);
        snprintf(message, sizeof(message), "%d %s", mystat.st_size, myfile->d_name);
        logMiddleFunction(message);
    }
    closedir(mydir);

    logEndingFunction("list_files");
    return SUCCESS;
}

static int initClient()
{
    logBeginningFunction("initClient");

    char *pathFile = (char*) malloc(MAXPATH);
    char* fileName = (char*) malloc( MAXFILENAME );
    int i=0;
    char message[1024];

    struct dirent *dp;
    DIR *dfd;

    if ((dfd = opendir(dropboxDir_)) == NULL)
    {
        logExceptionInFunction("initClient", "can't open directory");
        return ERROR;
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
                snprintf(message, sizeof(message), "Unable to stat file: %s", pathFile);
                logExceptionInFunction("initClient", message);
                return ERROR;
            }

            strcpy(_clientInfo.file_info[i].name ,dp->d_name);
            _clientInfo.file_info[i].size = stbuf.st_size;
            _clientInfo.file_info[i].lastModified = stbuf.st_mtime;
            i++;
        }
    }

    logEndingFunction("initClient");
    return SUCCESS;
}

int send_file(char *file)
{

    logBeginningFunction("send_file");

    char *request = (char*) malloc(MAXREQUEST), *filename = malloc(MAXFILENAME);
    int fs, aux, sent_bytes, offset = 0;
    char buffer[BUFFER_SIZE], message[1024];

    sem_wait(&runningRequest);

    if( strrchr(file, '/') )
    {
        strcpy(filename,strrchr(file, '/') + 1);
    }
    else
    {
        strcpy(filename, file);
    }

    snprintf(message, sizeof(message), "The file %s will be sended", filename);
    logMiddleFunction(message);


    fs = file_size(file);

    if(fs <= 0){
        sem_post(&runningRequest);
        logExceptionInFunction("send_file", "Invalid file");
        return ERROR;
    }
    
    time_t lastModified;
    if( (lastModified = file_lastModified(file)) == 0)
    {
        sem_post(&runningRequest);
        logExceptionInFunction("send_file", "Invalid file");
        return ERROR;
    }

    // send request to upload of file
    // always send the biggest request possible
    strcpy(request,"upload ");
    strcat(request, filename);
    send(socket_client, request, MAXREQUEST, 0);

    // send file size
    safe_sendINT(socket_client, &fs);

    // send last modified
    send(socket_client, &lastModified, sizeof(time_t), 0);

    FILE *fp = fopen(file, "r");

    while(offset < fs)
    {
        int bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, fp);
        sent_bytes = send(socket_client, (char*) buffer, bytes_read, 0);
        offset += sent_bytes;
    }

    fclose(fp);

    sem_post(&runningRequest);

    logEndingFunction("send_file");
    return SUCCESS;
}

int delete_file(char *file)
{
    logBeginningFunction("delete_file");

    char* request = (char*) malloc(MAXREQUEST);

    // send request to delete file
    // always send the biggest request possible
    strcpy(request,"delete ");
    strcat(request, file);
    sem_wait(&runningRequest);
    send(socket_client, request, MAXREQUEST, 0);
    deleteFile_ClientDir(&_clientInfo, file);
    sem_post(&runningRequest);

    logEndingFunction("delete_file");
    return SUCCESS;
}

int send_id(char *username, char *password)
{
    logBeginningFunction("send_id");

    char* request = (char*) malloc(MAXNAME*2 + 2);
    int response;

    strcpy(request, username);
    strcat(request, " ");
    strcat(request, password);

    send(socket_client, request, MAXNAME*2 + 2, 0);

    safe_recvINT(socket_client, &response);

    logEndingFunction("send_id");
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
        fprintf(stderr, "INOTIFY not initializated\n");
    }
    watch = inotify_add_watch( guard, dropboxDir_, IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE | IN_CREATE | IN_DELETE );

    // Setting non-blocking read for guard
    int flags = fcntl(guard, F_GETFL, 0);
    fcntl(guard, F_SETFL, flags | O_NONBLOCK);

    while(running)
    {

        sleep(10);

        numOfChanges = read( guard, buffer, EVENT_BUF_LEN );
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
                    fprintf(stderr, "Changes in file %s were detected\n", event->name);

                    time_t t0 = time(NULL);
                    time_t serverTime = get_time();
                    time_t t1 = time(NULL);
                    time_t clientTime = serverTime + (t1-t0)/2;

                    addFile_ClientDir(&_clientInfo, event->name, file_size(pathFile), clientTime);

                    struct utimbuf new_times;
                    new_times.actime = time(NULL);
                    new_times.modtime = clientTime;    /* set mtime to current time */
                    utime(pathFile, &new_times);

                    send_file(pathFile);

                }
                else if ( (event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM) )
                {
                    delete_file(event->name);
                }
            }
            i += EVENT_SIZE + event->len;
        }

        sync_client();

        t = time(NULL);
        tm = *localtime(&t);

    }
    inotify_rm_watch( guard, watch );
    close( guard );

    fprintf(stderr, "Sync Thread ended\n");
    return NULL;

}

int main(int argc, char *argv[])
{

    if(argc <= 5)
    {
        fprintf(stderr, "call ./client usuario senha dir endereço porta\n");
        return ERROR;
    }

    if (connectToServer(argv[4], atoi( argv[5] )) != SUCCESS)
    {
        fprintf(stderr, "Connection failed\n");
        return ERROR;
    } else {
        if (send_id(argv[1], argv[2]) != SUCCESS){
            fprintf(stderr, "Login failed\n");
            return ERROR;
        }
    }

    sem_init(&runningRequest, 0, 1); // only one request can be processed at the time
    set_dir(argv[3], argv[1]);
    initFiles_ClientDir(&_clientInfo);
    initClient();

    printFiles_ClientDir(_clientInfo);
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
            send_file(userCmd.param);
        }
        else if(!strcmp(userCmd.cmd, "download"))
        {
            get_file(userCmd.param);
        }
        else if(!strcmp(userCmd.cmd, "list"))
        {
            list_files();
        }
        else if(!strcmp(userCmd.cmd, "print"))
        {
            printFiles_ClientDir(_clientInfo);
        }
        else if(!strcmp(userCmd.cmd, "time"))
        {
            fprintf (stderr, "%lld\n", (long long) get_time());
        }
        else if(!strcmp(userCmd.cmd, "exit"))
        {
            send(socket_client, cmd_line, MAXREQUEST, 0);
        }

    }
    while(strcmp(userCmd.cmd, "exit"));

    fprintf(stderr, "Finishing program...\n");

    // kill sync thread
    running = 0;
    pthread_join(sync_thread, NULL);

    close_connection();

    return 0;
}