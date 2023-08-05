#include <orts/ip_discovery.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#endif
std::string ORserver::discover_server_ip()
{
    int sock = socket(AF_INET,SOCK_DGRAM,0);
#ifdef _WIN32
    BOOL br = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&br, sizeof(BOOL))==-1)
#else
    int br = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &br, sizeof(br))==-1)
#endif
    {
        perror("setsockopt");
        return "";
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5091);     
    addr.sin_addr.s_addr = INADDR_ANY;
    for(int i=0; ; i++)
    {
        if(::bind(sock,(struct sockaddr*)&(addr), sizeof(struct sockaddr_in))==-1) {
#ifndef _WIN32
            if (errno == EADDRINUSE && i<5) {
                sleep(1);
                continue;
            }
#endif
            perror("bind");
            return "";
        }
        break;
    }
    char buff[25];
    struct sockaddr_in from;
#ifdef _WIN32
    int size = sizeof(struct sockaddr_in);
#else
    socklen_t size = sizeof(struct sockaddr_in);
#endif
    
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    fd_set fds;
    FD_ZERO(&fds) ;
    FD_SET(sock, &fds);
    if (select(sock+1, &fds, nullptr, nullptr, &tv) == 1) {
        if (recvfrom(sock,buff,25,0,(struct sockaddr*)&from,&size) != -1) {
            std::string ip = inet_ntoa(from.sin_addr);
#ifdef _WIN32
            shutdown(sock, SD_BOTH);
            closesocket(sock);
#else
            if (shutdown(sock, SHUT_RDWR) == -1) {
                perror("shutdown");
            }
            if (close(sock) == -1)
                perror("close");
#endif
            return ip;
        }
        perror("recvfrom");
    }
#ifdef _WIN32
    shutdown(sock, SD_BOTH);
    closesocket(sock);
#else
    if (shutdown(sock, SHUT_RDWR) == -1) {
        perror("shutdown");
    }
    if (close(sock) == -1) {
        perror("close");
    }
#endif
    return "";
} 
