#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/sendfile.h>

#define SERV_PORT "8097" 
#define BACKLOG 10

int getFullFilePath(char *path, char *fullFilePath, char *type)
{
    size_t len = strlen(path);
    getcwd(fullFilePath, 64);
    fprintf(stderr, "path[len-1]: %c, path[len]: %c\n", path[len-1], path[len]);
    if ('/' == path[len-1]) {
        strcat(fullFilePath, "/index.html");
    }
    else {
        strcat(fullFilePath, path);
    }

    if(NULL != strstr(fullFilePath, ".html")) {
        strcpy(type, "text/html\0");
    }
    else if (NULL != strstr(fullFilePath, ".htm")) {
        strcpy(type, "text/html\0");
    }
    else if (NULL != strstr(fullFilePath, ".png")) { 
        strcpy(type, "image/png\0");
    }
    else if (NULL != strstr(fullFilePath, ".gif")) { 
        strcpy(type, "image/gif\0");
    }
    else if (NULL != strstr(fullFilePath, ".jpg")) { 
        strcpy(type, "image/jpg\0");
    }
    else if (NULL != strstr(fullFilePath, ".css")) { 
        strcpy(type, "text/css\0");
    }
    else if (NULL != strstr(fullFilePath, ".js")) { 
        strcpy(type, "text/javascript\0");
    }
    else if (NULL != strstr(fullFilePath, "x-icon")) { 
        strcpy(type, "image/x-icon\0");
    }
    else {
        //not supported send 503?
        return -1;
    }

    return 0;


}

void sigchld_handler(int s) 
{
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
 void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
       return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
int main(int argc, char **argv)
{
    int socket_fd, status, client_fd, send_fd;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct sockaddr_storage client_addr;
    struct sigaction sa;
    struct stat stat_buf;
    struct timeval timeout;
    socklen_t sin_size;
    char header_buffer[512];
    char command[512];
    char path[512];
    char version[64];
    char fullPath[512];
    char type[64];
    int yes = 1;
    int num_bytes;
    char s[INET6_ADDRSTRLEN];
    

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    //TODO:read in config file here
    getcwd(fullPath, 64);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, SERV_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    if (-1 == (socket_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol))){
        fprintf(stderr, "failed to aquir socket_fd errorno: %i\n", errno);
        exit(1);
    }

    if ( -1 == setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))){
        fprintf(stderr, "setsockopt:1 failed");
    }

    if (-1 == (bind(socket_fd, servinfo->ai_addr, servinfo->ai_addrlen))){
        fprintf(stderr, "failed to aquir socket_fd errorno: %i\n", errno);
        exit(1);
    }
  
    freeaddrinfo(servinfo);
    
    if (-1 == listen(socket_fd, BACKLOG)) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    size_t fork_num = 0;
    while(1)
    {
                
        printf("Server: waiting for connections...\n");
        
        sin_size = sizeof(client_fd);
        client_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &sin_size);
        if (-1 == client_fd) {
            perror("accept");
            exit(1);
        }

        if(!fork()) {
            close(socket_fd);
            //Read initial header
           // fprintf(stderr, "\n\n\n\n\n\n\n\n FORKNUM: %i \n\n\n\n\n\n\n", fork_num);
            if ( -1 == setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout))) {
                fprintf(stderr, "setsockopt:2 failed");
            }

            if (-1 == (num_bytes = recv(client_fd, header_buffer, 511, 0))) {

                perror("read header");
                close(client_fd);
                exit(0);
            }
            
            while(1){

                header_buffer[num_bytes] = '\0';
                //parse initial header
                sscanf(header_buffer, "%s %s %s %*s", command, path, version);
                printf("The Header is: %s\n", header_buffer);
                printf("Command: %s, PATH: %s, Version: %s\n", command, path, version);
                
               if (!getFullFilePath(path, fullPath, type)) {
                    //503 unsupported type
               
                    fprintf(stderr,"Full File Path: %s\n", fullPath);
                    fprintf(stderr, "Type: %s\n", type);


                    send_fd = open(fullPath, O_RDONLY);
                    if(-1 != send_fd) {

                        fstat(send_fd, &stat_buf);
                        if (strstr(version, "HTTP/1.1")) {
                             sprintf(header_buffer, "%s 200 OK\r\nContent-Type: %s\r\nContent-Length: %li\r\nConnection: keep-alive\r\n\r\n",version,  type, stat_buf.st_size);
                        } else if (strstr(version, "HTTP/1.0"))  {
                             sprintf(header_buffer, "%s 200 OK\r\nContent-Type: %s\r\nContent-Length: %li\r\n\r\n",version,  type, stat_buf.st_size);
                        } else {
                            //POORLY FORMATED HEADER
                            sprintf(header_buffer, "%s 400 Invalid HTTP-Version: %s", version, version);
                        }
                        send(client_fd, header_buffer, strlen(header_buffer), 0);
                        
                        off_t offset = 0;
                        int rc = sendfile(client_fd, send_fd, &offset, stat_buf.st_size);
                        if( -1 == rc) {
                            fprintf(stderr, "failed to sendfile\n");
                        }            

                        if (rc != stat_buf.st_size) {
                           fprintf(stderr, "didnt send the entire buffer\n");
                        }

                    } else {
                        fprintf(stderr, "Failed to open file\n");
                        //send file DNE
                        sprintf(header_buffer, "%s 404 Not Found: %s", version, fullPath);

                        send(client_fd, header_buffer, strlen(header_buffer), 0); 
                    }

                } else {
                    //send file UNSUPPORTED
                    sprintf(header_buffer, "%s 501 Not Implemented: %s", version, fullPath);
                    send(client_fd, header_buffer, strlen(header_buffer), 0);
                }

                fprintf(stderr, "\n\nRESPONSE\n\n%s\n\n",header_buffer);
                
                if (NULL == strstr(version, "HTTP/1.1")) {  
                    close(send_fd);
                    close(client_fd);
                    exit(0);
                }

                close(send_fd);

                if (-1 == (num_bytes = recv(client_fd, header_buffer, 511, 0))) {
                    fprintf(stderr, "timeout or failed to recv\n");
                    close(client_fd);
                    exit(0);
                }

            }
        }

        close(client_fd);
    
    }

    return 0;

}

