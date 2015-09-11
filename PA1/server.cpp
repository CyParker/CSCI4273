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

void sigchld_handler(int s) 
{
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}
int main(int argc, char **argv)
{
    int socket_fd, status, new_fd, client_fd, send_fd;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct sockaddr_storage client_addr;
    struct sigaction sa;
    socklen_t sin_size;
    char buffer[512];
    char command[64];
    char path[512];
    char version[64];
    int yes = 1;
    int num_bytes, file_size;
    struct stat stat_buf;
    char fullPath[64] = {'.'};
    
    //TODO:read in config file here

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
        fprintf(stderr, "setsockopt failed");
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
            if (-1 == (num_bytes = recv(client_fd, buffer, 511, 0))) {
                perror("read header");
                close(client_fd);
                exit(0);
            }

            buffer[num_bytes] = '\0';
            //parse initial header
            sscanf(buffer, "%s %s %s %*s", command, path, version);
            printf("The Header is: %s\n", buffer);
            printf("Command: %s, PATH: %s, Version: %s\n", command, path, version);
            strcat(fullPath, path);

            //fd getfile
            send_fd = open(fullPath, O_RDONLY);
            if(-1 == send_fd) {
                fprintf(stderr, "Failed to open file\n");
            }

            fstat(send_fd, &stat_buf);
            sprintf(buffer, "HTTP/1.1 200 OK\n Content-Type: text/html\n Content-Length: %i\n", stat_buf.st_size);
            send(client_fd, buffer, strlen(buffer), 0);
            off_t offset = 0;
            int rc = sendfile(client_fd, send_fd, &offset, stat_buf.st_size);
            if( -1 == rc) {
                fprintf(stderr, "failed to sendfile\n");
            }

            if (rc != stat_buf.st_size) {
            
                fprintf(stderr, "didnt send the entire buffer\n");
            }

            
            //if (-1 == send()) {
            //    perror("send");
            //}
            close(client_fd);
            exit(0);

        }

        close(client_fd);
    
    }

    return 0;

}

