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
#include <dirent.h>

#define BACKLOG 10
char DOC_ROOT[512] = {"\0"};
char PORT[64] = {"\0"};

bool auth(char *usr, char *passwd, FILE *AUTH_FILE )
{
    
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
    char usr[512];
    char passwd[64];
    char path[512];
    int yes = 1;
    int num_bytes;
    char s[INET6_ADDRSTRLEN];
    FILE *CONFIG_FILE;
    size_t len;
    char * line = NULL;
    DIR *dir;
    struct dirent *ent;
    char dirlist[1024] = {'\0'};
    
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    if(3 != argc) {
        fprintf(stderr, "not enough arguments\n");
        exit(0);
    }
    getcwd(DOC_ROOT, 511);
    strncat(DOC_ROOT, argv[1], 511);
    strncpy(PORT, argv[2], 63);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
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
           // if ( -1 == setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout))) {
           //     fprintf(stderr, "setsockopt:2 failed");
           // }
            printf("Server: waiting for command\n");
            if (-1 == (num_bytes = recv(client_fd, header_buffer, 511, 0))) {

                perror("read header");
                close(client_fd);
                exit(0);
            }
           printf("Server: got command\n"); 
            while(1){

                header_buffer[num_bytes] = '\0';
                //parse initial header
                char cmd[8]= {header_buffer[0], header_buffer[1], header_buffer[2], header_buffer[3], header_buffer[4], '\0'};
                printf("CMD: %s", cmd);
                if (strstr(cmd, "LIST")) {
                    char LFP[1024] = {'\0'};
                    dirlist[0] = '\0';
                    sscanf(header_buffer, "%s %s %s", command, usr, passwd);
                    //check usr passwd
                    //get full path, if not exist create
                    strcpy(LFP, DOC_ROOT);
                    strcat(LFP, "//");
                    strcat(LFP, usr);
                    if (NULL != (dir = opendir(LFP))) {
                        while(NULL != (ent = readdir(dir))) {
                            if((!strcmp(ent->d_name, "..")) || (!strcmp(ent->d_name, ".")))
                                continue;
                            strcat(dirlist ,ent->d_name);
                            strcat(dirlist, " ");
                        }
                        closedir(dir);
                    } else {
                        char sysString[512] = "mkdir -p ";
                        strcat(sysString, LFP);
                        system(sysString);
                        if (NULL != (dir = opendir(LFP))) {
                            while(NULL != (ent = readdir(dir))) {
                                strcat(dirlist ,ent->d_name);
                                strcat(dirlist, " ");
                            }
                            closedir(dir);
                        }
                    }
                    //send dirlis
                    send(client_fd, dirlist, strlen(dirlist), 0);

                } else if(strstr(cmd, "GET")) {
                    char GFP[1024] = {'\0'};
                    sscanf(header_buffer, "%s %s %s %s", command, path, usr, passwd);
                    printf("COMMAND: %s\n", command);
                    printf("path: %s\n", path);
                    //auth
                    strcpy(GFP, DOC_ROOT);
                    strcat(GFP, "/");
                    strcat(GFP, usr);
                    if (NULL != (dir = opendir(GFP))) {
                        size_t count = 0;
                        while(NULL != (ent = readdir(dir))) {
                            printf("%s \n", ent->d_name);
                            if(!strstr(ent->d_name, path))
                                continue;
                            //sendfilename
                            char fullname[1024] = {'\0'};
                            strcpy(fullname, GFP);
                            strcat(fullname, "/");
                            strcat(fullname, ent->d_name);
                            printf("GET SENDING NAME: %s\n", fullname);
                            send(client_fd, ent->d_name, strlen(ent->d_name), 0);
                            sleep(1);
                            //sendfile
                            send_fd = open(fullname, O_RDONLY);
                            perror("send_fd\n");
                            if(-1 != send_fd) {

                                fstat(send_fd, &stat_buf);
                       
                                off_t offset = 0;
                                int rc = sendfile(client_fd, send_fd, &offset, stat_buf.st_size);
                               sleep(1);
                                if( -1 == rc) {
                                    fprintf(stderr, "failed to sendfile\n");
                                }            

                                if (rc != stat_buf.st_size) {
                                    fprintf(stderr, "didnt send the entire buffer\n");
                                }

                                } else {
                                    fprintf(stderr, "Failed to open file\n");
                                    //send file DNE
                                    sprintf(header_buffer, "Not Found: %s", ent->d_name);
    
                                    send(client_fd, header_buffer, strlen(header_buffer), 0); 
                                }

                            if(++count == 2)
                                break;
                        }
                        closedir(dir);
                    } else {
                        //senderror
                    }
                } else if(strstr(cmd, "PUT")) {
                    char file1[1024];
                    char file2[1024];
                    char name1[64];
                    char name2[64];
                    char num1[16];
                    char num2[16];
                    char recv_buffer[1024];
                    int bytes;
                    char PFP[1024] = {'\0'};
                    char P1[1024] = {'\0'};
                    char P2[1024] = {'\0'};
                    sscanf(header_buffer, "%s %s %s %s", command, path, usr, passwd);
                    strcpy(PFP, DOC_ROOT);
                    strcat(PFP, usr);
                    //get files (2)
                    //getname and numbers
                    if (-1 == (num_bytes = recv(client_fd, recv_buffer, 1023, 0))) {
                        fprintf(stderr, "timeout or failed to recv\n");
                        close(client_fd);
                        exit(0);
                    }
                    sscanf(recv_buffer, "%s %s %s", name1, num1, num2);
                    strcpy(name2, name1);
                    strcat(name1, num1);
                    strcat(name2, num2);
                    printf("NAME: %s\n", recv_buffer);
                    //recv 1                    
                    if (-1 == (num_bytes = recv(client_fd, file1, 1024, 0))) {
                        fprintf(stderr, "timeout or failed to recv\n");
                        close(client_fd);
                        exit(0);
                    }
                    strcpy(P1, PFP);
                    strcat(P1, name1);
                    FILE *fileptr = fopen(P1, "w");
                    fwrite(file1, sizeof(char), num_bytes, fileptr);
                    fclose(fileptr);
                    //recv 2
                    if (-1 == (num_bytes = recv(client_fd, file2, 1024, 0))) {
                        fprintf(stderr, "timeout or failed to recv\n");
                        close(client_fd);
                        exit(0);
                    }
                    fileptr = fopen(P2, "w");
                    fwrite(file2, sizeof(char), num_bytes, fileptr);
                    fclose(fileptr);


                }
                
                if (-1 == (num_bytes = recv(client_fd, header_buffer, 1024, 0))) {
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

