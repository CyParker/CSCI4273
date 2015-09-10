#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>

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
    int socket_fd, status;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct sockaddr_storage client_addr;
    int yes = 1;
    
    //TODO:read in config file here

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_sockettype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, SERV_PORT, &hints, &servinfo)) != 0) {
        fprinf(stderr, "getaddrinfo error: %s\n", gai_strerr(status));
        exit(1);
    }

    if (-1 == (socket_fd = socket(servinfo->ai_family, servinfo->ai_sockettype, servinfo->ai_protocol)){
        fprintf(stderr, "failed to aquir socket_fd errorno: %i\n", errno);
        exit(1);
    }

    if ( -1 == setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))){
        fprintf(stderr, "setsockopt failed");
    }

    if (-1 == (bind(socket_fd, servinfo->ai_addr, servinfo->ai_addrlen){
        fprintf(stderr, "failed to aquir socket_fd errorno: %i\n", errno);
        exit(1);
    }
  
    freeaddrinfo(servinfo);
    while(1)
    {
        if (-1 == listen(socket_fd, BACKLOG)) {
            perror("listen");
            exit(1);
        }
        
        printf("Server: waiting for connections...\n");
        
        if(!fork()) {
            sin_size = sizeof(client_addr);
            client_fd = accept(socket_fd, (sruct sockaddr *) &client_addr, &sin_size);
            if (-1 == client_fd {
                perror("accept");
                exit(1);
            }

            if (-1 == send(client_fd, "HELLO WORLD", 10, 0)) {
                perror("send");
            }
            close(client_fd);
            exit(0);

        }      
        
    
    }

    return 0;

}

