#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 100 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

struct server{
    char name[64];
    char port[64];
    char ip[512];

};

int main(int argc, char *argv[])
{
    int sockfd[4];  
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char* line = NULL;
    char usr[64];
    char passwd[64];
    struct server server1, server2, server3, server4;
    struct server server_array[4] = {server1, server2, server3, server4};
    FILE* CONFIG_FILE;
    size_t len;
    struct timeval timeout;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (argc != 2) {
        fprintf(stderr,"usage: configuration file name\n");
        exit(1);
    }

    if (NULL == (CONFIG_FILE = fopen(argv[1], "r"))) {
            fprintf(stderr, "failed to open config file...");
            exit(0);
    }
    int s_num = 0;
    while (-1 != (len = getline(&line, &len, CONFIG_FILE))) {
         if(line[0] == ' ') {
            free(line);
            line = NULL;
            continue;
        }
        if(strstr(line, "Server")) {
            sscanf(line, "%*s %s %s %s", server_array[s_num].name, server_array[s_num].ip, server_array[s_num].port);
            free(line);
            line = NULL;
            s_num++;
            continue;
        }
        if(strstr(line, "Username:")) {
            sscanf(line, "%*s %s", usr);
            printf("usr is: %s\n", usr);
            free(line); line = NULL;
            continue;
        }

        if(strstr(line, "Password:")) {
            sscanf(line, "%*s %s", passwd);
            printf("password is: %s\n", passwd);
            free(line); line = NULL;
            continue;
        }

    }
    perror("EHRE");
    for(int i = 0; i < 4; i++)
    {
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if ((rv = getaddrinfo("localhost", server_array[i].port, &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return 1;
        }

        // loop through all the results and connect to the first we can
        if ((sockfd[i] = socket(servinfo->ai_family, servinfo->ai_socktype,
                servinfo->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
    
         if (connect(sockfd[i], servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
            close(sockfd[i]);
            perror("client: connect");
            continue;
        }
    
    
        inet_ntop(servinfo->ai_family, get_in_addr((struct sockaddr *)servinfo->ai_addr),
                s, sizeof s);
        printf("client: connecting to %s\n", s);

        freeaddrinfo(servinfo); // all done with this structure
    }

    //read std input for commands
    char *cmdbuf = NULL;
    size_t size;
    for( int i = 0; i <4; i++)
    {
        if(-1 == setsockopt(sockfd[i], SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout))) {
            fprintf(stderr, "setsockopt: failed\n");
        }
    }
    while(-1 != getline(&line, &size, stdin))
    {
        char *pos;
        char cmd[8];
        char file[512];
        int num_bytes = 0;
        if((pos=strchr(line, '\n')) != NULL)
            *pos = '\0';
        strcat(line, " ");
        strcat(line, usr);
        strcat(line, " " );
        strcat(line, passwd);
        printf("CMD: %s\n", line);
        if (strstr(line, "LIST")) {
          char dirlist1[1024] = {'\0'}; 
          char dirlist2[1024] = {'\0'}; 
          char dirlist3[1024] = {'\0'}; 
          char dirlist4[1024] = {'\0'};
          printf("LIST CMD\n");
          if( -1 == (num_bytes = recv(sockfd[0], dirlist1, 1023, 0)))
          {
              dirlist1[0] = '\0';
          } else {
            dirlist1[num_bytes] = '\0';
          }

          printf("DIRLIST1: %s \n", dirlist1);
          send(sockfd[1], line, strlen(line), 0);
          if( -1 == (num_bytes = recv(sockfd[1], dirlist2, 1023, 0)))
          {
              dirlist2[0] = '\0';
          } else {
            dirlist2[num_bytes] = '\0';
          }
          printf("DIRLIST2: %s \n", dirlist2);

          send(sockfd[2], line, strlen(line), 0);
          if( -1 == (num_bytes = recv(sockfd[2], dirlist3, 1023, 0)))
          {
              dirlist3[0] = '\0';
          } else {
            dirlist3[num_bytes] = '\0';
          }

          printf("DIRLIST3: %s \n", dirlist3);
          send(sockfd[3], line, strlen(line), 0);
          if( -1 == (num_bytes = recv(sockfd[3], dirlist4, 1023, 0)))
          {
              dirlist4[0] = '\0';
          } else {
            dirlist4[num_bytes] = '\0';
          }

          printf("DIRLIST4: %s \n", dirlist4);

         if(!(dirlist2[0] == '\0') && !(dirlist4[0] == '\0')) { // two out only incomplete files now
                char fname[64];
                int idx = 1;
                int i = 0;
                int count = 0;
                while(dirlist2[idx] != '\0')
                {
                    while(dirlist2[idx] != ' '){
                        fname[i] = dirlist2[idx];
                        idx++;
                        i++;
                    }
                    if(count == 0){
                        fname[i - 2] = '\0';
                        printf("%s\n", fname);
                        count++;
                    } else {
                        count = 0;
                    }

                    idx = idx + 2;
                    i = 0;
                }
           
          } else if(dirlist1[0] != '\0' && dirlist3[0] != '\0') {

                char fname[64];
                int idx = 1;
                int i = 0;
                int count = 0; 
                printf("FILELIST\n");
                while(dirlist1[idx] != '\0')
                {
                    while(dirlist1[idx] != ' '){
                        fname[i] = dirlist1[idx];
                        idx++;
                        i++;
                    }
                    if(count == 0){
                        fname[i - 2] = '\0';
                        printf("%s\n", fname);
                        count++;
                    } else {
                        count = 0;
                    }
                    idx = idx + 2;
                    i = 0;
                }
          } else if(dirlist1[0] != '\0'){

                char fname[64];
                int idx = 1;
                int i = 0;
                int count = 0; 
                printf("FILELIST\n");
                while(dirlist1[idx] != '\0')
                {
                    while(dirlist1[idx] != ' '){
                        fname[i] = dirlist1[idx];
                        idx++;
                        i++;
                    }
                    if(count == 0){
                        fname[i - 2] = '\0';
                        printf("%s\n", fname);
                        printf("INCOMPLETE ");
                        count++;
                    } else {
                        count = 0;
                    }
                    idx = idx + 2;
                    i = 0;
                }
          } else if(dirlist3[0] != '\0'){

                char fname[64];
                int idx = 1;
                int i = 0;
                int count = 0; 
                printf("FILELIST\n");
                while(dirlist3[idx] != '\0')
                {
                    while(dirlist3[idx] != ' '){
                        fname[i] = dirlist3[idx];
                        idx++;
                        i++;
                    }
                    if(count == 0){
                        fname[i - 2] = '\0';
                        printf("INCOMPLETE ");
                        printf("%s\n", fname);
                        count++;
                    } else {
                        count = 0;
                    }
                    idx = idx + 2;
                    i = 0;
                }
          } else if(dirlist2[0] != '\0'){
          
                char fname[64];
                int idx = 1;
                int i = 0;
                int count = 0; 
                printf("FILELIST\n");
                while(dirlist1[idx] != '\0')
                {
                    while(dirlist2[idx] != ' '){
                        fname[i] = dirlist2[idx];
                        idx++;
                        i++;
                    }
                    if(count == 0){
                        fname[i - 2] = '\0';

                        printf("INCOMPLETE ");
                        printf("%s\n", fname);
                        count++;
                    } else {
                        count = 0;
                    }
                    idx = idx + 2;
                    i = 0;
                }
          } else if(dirlist4[0] != '\0'){
          
                char fname[64];
                int idx = 1;
                int i = 0;
                int count = 0; 
                printf("FILELIST\n");
                while(dirlist4[idx] != '\0')
                {
                    while(dirlist4[idx] != ' '){
                        fname[i] = dirlist4[idx];
                        idx++;
                        i++;
                    }
                    if(count == 0){
                        fname[i - 2] = '\0';
                        printf("INCOMPLETE ");
                        printf("%s\n", fname);
                        count++;
                    } else {
                        count = 0;
                    }
                    idx = idx + 2;
                    i = 0;
                }
          }

                    

        }else if(strstr(line, "GET")) {
            char f1NAME1[64] = {'\0'};
            char f1NAME2[64] = {'\0'};
            char f2NAME1[64] = {'\0'};
            char f2NAME2[64] = {'\0'};
            char f3NAME1[64] = {'\0'};
            char f3NAME2[64] = {'\0'};
            char f4NAME1[64] = {'\0'};
            char f4NAME2[64] = {'\0'};
            char f1file1[256] = {'\0'};
            char f1file2[256] = {'\0'};
            char f2file1[256] = {'\0'};
            char f2file2[256] = {'\0'};
            char f3file1[256] = {'\0'};
            char f3file2[256] = {'\0'};
            char f4file1[256] = {'\0'};
            char f4file2[256] = {'\0'};
            char parts[4][256];
            timeout.tv_sec = 1;
            timeout.tv_usec = 300000;
            for(int i = 0; i < 4; i++) {
        if(-1 == setsockopt(sockfd[i], SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout))) {
            fprintf(stderr, "setsockopt: failed\n");
        }
            }
            send(sockfd[0], line, strlen(line), 0);

            if( -1 == (num_bytes = recv(sockfd[0], f1NAME1, 63, 0)))
            {
                f1NAME1[0] = '\0';
            } else {
                f1NAME1[num_bytes] = '\0';
                printf("TEMP f1NAME: %s \n", f1NAME1);
                if( -1 == (num_bytes = recv(sockfd[0], f1file1, 255, 0)))
                {
                    //error
                }
            }
            
            if( -1 == (num_bytes = recv(sockfd[0], f1NAME2, 63, 0)))
            {
                f1NAME2[0] = '\0';
            } else {
                f1NAME2[num_bytes] = '\0';

                if( -1 == (num_bytes = recv(sockfd[0], f1file2, 255, 0)))
                {
                    //error
                }
            }
            //2
            send(sockfd[1], line, strlen(line), 0);

            if( -1 == (num_bytes = recv(sockfd[1], f2NAME1, 63, 0)))
            {
                f2NAME1[0] = '\0';
            } else {
                f2NAME1[num_bytes] = '\0';

                if( -1 == (num_bytes = recv(sockfd[1], f2file1, 255, 0)))
                {
                    //error
                }
            }
            if( -1 == (num_bytes = recv(sockfd[1], f2NAME2, 63, 0)))
            {
                f2NAME2[0] = '\0';
            } else {
                f2NAME2[num_bytes] = '\0';

                if( -1 == (num_bytes = recv(sockfd[1], f2file2, 255, 0)))
                {
                    //error
                }
            }
            //3
            send(sockfd[2], line, strlen(line), 0);

            if( -1 == (num_bytes = recv(sockfd[2], f3NAME1, 63, 0)))
            {
                f3NAME1[0] = '\0';
            } else {
                f3NAME1[num_bytes] = '\0';

                if( -1 == (num_bytes = recv(sockfd[2], f3file1, 255, 0)))
                {
                    //error
                }
            }
            if( -1 == (num_bytes = recv(sockfd[2], f3NAME2, 63, 0)))
            {
                f3NAME2[0] = '\0';
            } else {
                f3NAME2[num_bytes] = '\0';

                if( -1 == (num_bytes = recv(sockfd[2], f3file2, 255, 0)))
                {
                    //error
                }
            }
            //4
            send(sockfd[3], line, strlen(line), 0);

            if( -1 == (num_bytes = recv(sockfd[3], f4NAME1, 63, 0)))
            {
                f4NAME1[0] = '\0';
            } else {
                f4NAME1[num_bytes] = '\0';

                if( -1 == (num_bytes = recv(sockfd[3], f4file1, 255, 0)))
                {
                    //error
                }
            }
            if( -1 == (num_bytes = recv(sockfd[3], f4NAME2, 63, 0)))
            {
                f4NAME2[0] = '\0';
            } else {
                f4NAME2[num_bytes] = '\0';

                if( -1 == (num_bytes = recv(sockfd[3], f4file2, 255, 0)))
                {
                    //error
                }
            }
        printf("F1.1: %s\n", f1NAME1);
        printf("F1.2: %s\n", f1NAME2);
        printf("F2.1: %s\n", f2NAME1);
        printf("F2.2: %s\n", f2NAME2);
        printf("F3.1: %s\n", f3NAME1);
        printf("F3.2: %s\n", f3NAME2);
        printf("F4.1: %s\n", f4NAME1);
        printf("F4.2: %s\n", f4NAME2);
        char fNAME[64];
        int end = strlen(f1NAME1);
        if(f1NAME1[0] == '\0') { //use 2 and 4
            if(f2NAME1[0] == '\0') { //no files!
                printf("The requested file DNE...\n");
            } 
            end = strlen(f2NAME1);
            int i = f2NAME1[end-1] - '0';
            strcpy(parts[i - 1], f2file1);
            end = strlen(f2NAME2);
            i = f2NAME2[end-1] - '0';
            strcpy(parts[i - 1], f2file2);
            end = strlen(f4NAME1);
            i = f4NAME1[end-1] - '0';
            strcpy(parts[i - 1], f4file1); 
            end = strlen(f4NAME2);
            i = f4NAME2[end-1]; -'0';
            strcpy(parts[i - 1], f4file2);
            size_t idx = 1;
            while(f2NAME1 != '\0')
            {
                fNAME[idx-1] = f2NAME1[i];
                idx++;
            }
            fNAME[strlen(fNAME)-2] = '\0';
            
        } else { //use 1 and 3
            int i = f1NAME1[end-1] - '0';
            printf("i: %d\n", i);
            strcpy(parts[i - 1], f1file1);
            end = strlen(f1NAME2);
            i = f1NAME2[end-1] - '0';
            
            printf("i: %d\n", i);
            strcpy(parts[i - 1], f1file2);
            end = strlen(f3NAME1);
            i = f3NAME1[end-1] - '0';

            printf("i: %d\n", i);
            strcpy(parts[i - 1], f3file1); 
            end = strlen(f3NAME2);
            i = f3NAME2[end-1] - '0';
            printf("i: %d\n", i);
            strcpy(parts[i - 1], f3file2);
            size_t idx = 1;
            while(f1NAME1[idx] != '\0')
            {
                fNAME[idx-1] = f1NAME1[idx];
                idx++;
            }
            printf("HEREASDFASD\n");
            fNAME[idx-3] = '\0';
            printf("fNANE; %s\n", fNAME);
        }

        FILE *fptr = fopen(fNAME, "w+");
        fprintf(fptr, "%s%s%s%s", parts[0], parts[1], parts[2], parts[3]);
        fclose(fptr);
        
        } else if(strstr(line, "PUT")) {
            char fname[64];
            char fnameNum[128];
            char fpart[256];
            sscanf(line, "%*s %s %*s %*s",  fname);
            printf("fame: %s\n", fname);
            FILE * fptr = fopen(fname, "r");
            fseek(fptr,0, SEEK_END);
            int size = ftell(fptr);
            int smf = size % 4;
            int s123 = (size - smf)/4;
            int s4 = s123 + smf;
            strcpy(fnameNum, fname);
            strcat(fnameNum, " ");
            strcat(fnameNum, "1");
            strcat(fnameNum, " ");
            strcat(fnameNum, "2");
            send(sockfd[0], fnameNum, strlen(fnameNum), 0);
            fread(fpart, 1, s123, fptr);
            send(sockfd[0], fpart, strlen(fpart), 0);
            //2
            strcpy(fnameNum, fname);
            strcat(fnameNum, " ");
            strcat(fnameNum, "2");
            strcat(fnameNum, " ");
            strcat(fnameNum, "3");
            send(sockfd[1], fnameNum, strlen(fnameNum), 0);
            fread(fpart, 1, s123, fptr);
            send(sockfd[1], fpart, strlen(fpart), 0);
            //3
            strcpy(fnameNum, fname);
            strcat(fnameNum, " ");
            strcat(fnameNum, "3");
            strcat(fnameNum, " ");
            strcat(fnameNum, "4");
            send(sockfd[2], fnameNum, strlen(fnameNum), 0);
            fread(fpart, 1, s123, fptr);
            send(sockfd[2], fpart, strlen(fpart), 0);
            //4
            strcpy(fnameNum, fname);
            strcat(fnameNum, " ");
            strcat(fnameNum, "4");
            strcat(fnameNum, " ");
            strcat(fnameNum, "1");
            send(sockfd[3], fnameNum, strlen(fnameNum), 0);
            fread(fpart, 1, s4, fptr);
            send(sockfd[3], fpart, strlen(fpart), 0);
        }


        free(line);
        line = NULL;
    }
    for(int i = 0; i < 4; i++){
        close(sockfd[i]);
    }
    return 0;
}
