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

#define BACKLOG 10
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
    FILE *CONFIG_FILE;
    size_t len;
    char * line = NULL;

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    //TODO:read in config file here

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
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
                
               if (((strncmp(command,"GET",3)==0))&&(strncmp(version,"HTTP/1.0",8)==0)&&(strncmp(path,"http://",7)==0)) {
                    struct addrinfo hh, *r;
                    char * temp = NULL;
                    int sockdest;
                    char recvbuff[65535] = {"\0"};
                    
                    temp = strtok(path, "//");
                    temp = strtok(NULL, "/");
                    memset(&hh, 0, sizeof(hh));
                    hh.ai_family = AF_UNSPEC;
                    hh.ai_socktype = SOCK_STREAM;
                    getaddrinfo(temp, "80", &hints, &r);
                    printf("\nHost is: %s\n", temp);
                    sockdest = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
                    connect(sockdest, r->ai_addr, r->ai_addrlen);
                    if(-1 == send(sockdest, header_buffer, strlen(header_buffer), 0)) {
                        perror("Failed to send to dest");
                    }
                    if((num_bytes = recv(sockdest, recvbuff, 65534, 0)) == -1){
                        perror("failed to recv from dest");
                    }
                    recvbuff[num_bytes] = '\0';
                    if(-1 == send(client_fd, recvbuff, strlen(recvbuff), 0)){
                        perror("failed to send back to client");
                    }
                    close(sockdest);


                } else {
                    //not GET or 1.0 or http://
                    sprintf(header_buffer, "%s 501 Not Implemented: %s", version, fullPath);
                    send(client_fd, header_buffer, strlen(header_buffer), 0);
                }
                close(client_fd);
                exit(0);

            }
        }

        close(client_fd);
    
    }

    return 0;

}

