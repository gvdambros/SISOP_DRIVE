#include "dropboxUtil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <pwd.h>
#include <dirent.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

int file_size(char *file)
{
    struct stat file_stat;

    if (stat(file, &file_stat) < 0)
    {
        printf("Error fstat --> %s\n", file);
        return -1;
    }

    return file_stat.st_size;
}

time_t* file_lastModified(char *file){
    time_t *t = (time_t*) malloc(sizeof(time_t));
    struct stat stbuf;
    if(stat(file, &stbuf) < 0){
        printf("Error fstat --> %s\n", file);
        return 0;
    }
    (*t) = stbuf.st_mtime;
    return t;
}

user_cmd string2userCmd(char *cmd)
{
    printf("%s\n", cmd);
    char *pch;
    user_cmd temp;
    pch = strtok (cmd," ");
    if (pch != NULL)
    {
        printf ("cmd: %s\n",pch);
        strcpy(temp.cmd, pch);
        pch = strtok (NULL, " ");
        if (pch != NULL)
        {
            printf ("param: %s\n",pch);
            strcpy(temp.param, pch);
        }
    }
    return temp;
}

// wait until it receives s bytes
// nova função: SSL_read(SSL *ssl, void *buf, int num)
// antiga: recv(int fd, void *buf, num, 0);
int safe_recv(SSL *ssl, char *buf, int s)
{
    int offset = 0;
    while(offset < s)
    {
        //int recv_bytes = recv(client_fd, &(buf[offset]), s, 0);
        int recv_bytes = SSL_read(ssl, &(buf[offset]), s);
        offset += recv_bytes;
    }
    return offset;
}

int safe_recvINT(SSL *ssl, int *buf){
    safe_recv(ssl, buf, sizeof(int));
    (*buf) = ntohl(*buf);
}


int safe_sendINT(SSL *ssl, int *buf){
    int aux = htonl(*buf);
    //send(client_fd, &aux, sizeof(int), 0);
    SSL_write(ssl, &aux, sizeof(int));
}


int dir_exists(char *dir)
{
    struct stat st = {0};

    if (stat(dir, &st) == -1)
    {
        return 0;
    }
    return 1;
}

char* getLinuxUser ()
{
    register struct passwd *pw;
    register uid_t uid;
    char *user = NULL;
    uid = geteuid ();
    pw = getpwuid (uid);
    if (pw)
    {
        user = malloc(strlen(pw->pw_name + 1));
        strcpy(user, pw->pw_name);
        puts (user);
        return user;
    }
    return -1;
}

int numberOfFilesInDir(char *dir)
{

    int file_count = 0;
    DIR * dirp;
    struct dirent * entry;

    dirp = opendir(dir); /* There should be error handling after this */
    while ((entry = readdir(dirp)) != NULL)
    {
        if (entry->d_type == DT_REG)   /* If the entry is a regular file */
        {
            file_count++;
        }
    }
    closedir(dirp);
    return file_count;
}


void printFiles_ClientDir(CLIENT client)
{
    int i;
    fprintf(stderr, "Arquivos:\n");
    for(i = 0; i < MAXFILES; i ++)
    {
        if(client.file_info[i].size != -1)
        {

            char buff[20];

            struct tm * timeinfo;

            timeinfo = localtime (&(client.file_info[i].lastModified));

            strftime(buff, sizeof(buff), "%b %d %H:%M", timeinfo);
            fprintf(stderr, "id: %d | Arquivo: %s | Tamanho: %d | Última modif.: %s\n", i, client.file_info[i].name, client.file_info[i].size, buff);
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

    fprintf(stderr, "Arquivo adicionado\n");
    strcpy( client->file_info[i].name, file);
    client->file_info[i].size = size;
    client->file_info[i].lastModified = lastModified;

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
        //fprintf(stderr, "File: %s\n", client.file_info[i].name);
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
    fprintf(stderr, "Excluindo");
    if((i = findFile_ClientDir(*client, file)) == -1)
    {
        return -1;
    }
    fprintf(stderr, "%s %s %d", file, client->file_info[i].name, i);
    client->file_info[i].size = -1;
    strcpy(client->file_info[i].name,"");
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

int getIDoOfFileAtPosition_ClientDir(CLIENT client, int p)
{
    int i;
    for(i = 0; i < MAXFILES && p >= 0 ; i++)
    {
        if(client.file_info[i].size != -1)
        {

            p--;
        }
    }
    if(p >= 0) return -1;
    return i-1;
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
        strcpy(client->file_info[i].name, "");
    }
    return;
}

void printCertificate(SSL *ssl){
    //Impressão do certificado SSL
    X509 *cert;
    char *line;
    cert = SSL_get_peer_certificate(ssl);
    if (cert != NULL){
        printf("\nxxx Certificado: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
        line= X509_NAME_oneline(X509_get_subject_name(cert),0,0);
        printf(" Subject: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert),0,0);
        printf(" Issuer: %s\n", line);
        printf("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
    } else printf("Certificate not validated \n");
}
