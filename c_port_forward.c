#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#define BUF_SIZE 4096

struct struct_rc {
    int server_socket;
    int client_socket;
    int remote_socket;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    struct sockaddr_in remote_addr;
    struct hostent *remote_host;
};
struct struct_para {
    char *re_flag;
    char *re_addr;
    char *re_port;
    char *lo_port;
};
struct struct_rc rc;
struct struct_para para;
struct hostent* l_gethostbyname(char *msg);
struct hostent* l_gethostbyaddr(char *msg);
struct hostent* (*find)(char*);
int Create_LocalServer();
int Accept_Client();
void handle_client();
void handle_tunnel();
int build_tunnel();
int use_tunnel();
int Maxfd();

int main(int argc,char *argv[]) {
    if(argc!=5){
        printf("Usage : %s [0/1] <RemoteIP/Domain> <RemotePort> <LocalPort>\n",argv[0]);
        exit(1);
    }
    para.re_flag=argv[1];
    para.re_addr=argv[2];
    para.re_port=argv[3];
    para.lo_port=argv[4];
    if(strcmp(para.re_flag, "0")==0){
        find = l_gethostbyaddr;
    }
    if(strcmp(para.re_flag, "1")==0){
        find = l_gethostbyname;
    }
    if (Create_LocalServer() == 1) {
        exit(1);
    }
    while(!Accept_Client()){
        handle_client();
    }
    close(rc.server_socket);
    return 0;
}

int Create_LocalServer(){
    rc.server_socket = socket(PF_INET,SOCK_STREAM,0);
    int option = 1;
    if(setsockopt(rc.server_socket,SOL_SOCKET,SO_REUSEADDR,(void*)&option,sizeof(option))<0)
        return 1;
    memset(&rc.server_addr,0,sizeof(rc.server_addr));
    rc.server_addr.sin_family=AF_INET;
    rc.server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    rc.server_addr.sin_port=htons(atoi(para.lo_port));
    if(bind(rc.server_socket,(struct sockaddr*)&rc.server_addr,sizeof(rc.server_addr))<0)
        return 1;
    if(listen(rc.server_socket,1)<0)
        return 1;
    return 0;
}

int Accept_Client(){
    socklen_t adr_sz;
    adr_sz = sizeof(rc.client_addr);
    rc.client_socket = accept(rc.server_socket,(struct sockaddr*)&rc.client_addr,&adr_sz);
    if(rc.client_socket<0)
        return 1;
    return 0;
}

void handle_client(){
    if (fork() == 0) {
        close(rc.server_socket);
        handle_tunnel();
        exit(0);
    }
    close(rc.client_socket);
}

void handle_tunnel() {
    if (build_tunnel() == 0) {
        use_tunnel();
    }
}

int build_tunnel(){
    rc.remote_host =find(para.re_addr);
    if (rc.remote_host == NULL) {
        return 1;
    }
    memset(&rc.remote_addr, 0, sizeof(rc.remote_addr));
    rc.remote_addr.sin_family = AF_INET;
    rc.remote_addr.sin_port = htons(atoi(para.re_port));
    memcpy(&rc.remote_addr.sin_addr.s_addr, rc.remote_host->h_addr, rc.remote_host->h_length);
    rc.remote_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (rc.remote_socket < 0) {
        return 1;
    }
    if (connect(rc.remote_socket, (struct sockaddr *) &rc.remote_addr, sizeof(rc.remote_addr)) < 0) {
        return 1;
    }

    return 0;
}

int use_tunnel() {
    fd_set reads;
    char buffer[BUF_SIZE];
    for (;;) {
        FD_ZERO(&reads);
        FD_SET(rc.client_socket, &reads);
        FD_SET(rc.remote_socket, &reads);
        memset(buffer, 0, sizeof(buffer));
        if (select(Maxfd(), &reads, NULL, NULL, NULL) < 0) {
            break;
        }
        if (FD_ISSET(rc.client_socket, &reads)) {
            int count = recv(rc.client_socket, buffer, sizeof(buffer), MSG_DONTWAIT);
            if (count < 0) {
                close(rc.client_socket);
                close(rc.remote_socket);
                return 1;
            }
            if (count == 0) {
                close(rc.client_socket);
                close(rc.remote_socket);
                return 0;
            }
            send(rc.remote_socket, buffer, count, MSG_DONTWAIT);
        }
        if (FD_ISSET(rc.remote_socket, &reads)) {
            int count = recv(rc.remote_socket, buffer, sizeof(buffer), MSG_DONTWAIT);
            if (count < 0) {
                close(rc.client_socket);
                close(rc.remote_socket);
                return 1;
            }
            if (count == 0) {
                close(rc.client_socket);
                close(rc.remote_socket);
                return 0;
            }
            send(rc.client_socket, buffer, count, MSG_DONTWAIT);
        }
    }
    return 0;
}

int Maxfd(void) {
    unsigned int fd = rc.client_socket;
    if (fd < rc.remote_socket) {
        fd = rc.remote_socket;
    }
    return fd + 1;
}
struct hostent* l_gethostbyname(char *msg){
    return gethostbyname(msg);
}

struct hostent* l_gethostbyaddr(char *msg){
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_addr.s_addr=inet_addr(msg);
    return gethostbyaddr((char *)&addr.sin_addr,4,AF_INET);
}