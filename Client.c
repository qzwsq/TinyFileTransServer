#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#define MAX_LINE 500

void err_sys(const char * err_msg){
    perror(err_msg);
    exit(1);
}

int main(int argc, char** argv){
    int sockfd, n;
    char recvline[MAX_LINE];
    struct sockaddr_in server_addr;
    char sendline[MAX_LINE];
    if(argc!=2)
        err_sys("usage a.out ${Port}");
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0))<0)
        err_sys("socket error");
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[argc-1]));

    if(inet_pton(AF_INET, argv[1], &server_addr.sin_addr)<0)
        err_sys("IP address format error");
    
    printf("wait for connect\n");
    if(connect(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr))<0)
        err_sys("connect failed");
    printf("input filename to receive\n");
    scanf("%s", sendline);
    send(sockfd, sendline, strlen(sendline), 0);
    printf("send data success data:%s  length:%ld\n",sendline, strlen(sendline));
    char path[60] = "recv/";
    memset(recvline, '\0', sizeof(recvline));
    FILE *fp = fopen(strcat(path, sendline), "wb");
    while ((n=read(sockfd, recvline, MAX_LINE-4))>0)
    {
        if(recvline[0] == 0){
            perror("file not exists");
            fclose(fp);
            printf("remove file\n");
            while(remove(strcat(path, sendline))!=0){
                sleep(10);
            }
            exit(1);
        }
        fwrite(recvline, sizeof(char), strlen(recvline), fp);
        printf("%s", recvline);
        memset(recvline, '\0', sizeof(recvline));
    }
    fclose(fp);
    if(n<0) err_sys("read error");
    exit(0);
}