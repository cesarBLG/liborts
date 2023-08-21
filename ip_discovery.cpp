#include <orts/ip_discovery.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#endif
#include <iostream>
std::string ORserver::discover_server_ip()
{
    std::string ip;
    int sock = socket(AF_INET,SOCK_DGRAM,0);
    if (sock < 0) return ip;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5091);
    addr.sin_addr.s_addr = INADDR_ANY;
#ifdef _WIN32
    char br = 1;
#else
    int br = 1;
#endif
    if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &br, sizeof(br))==-1)
    {
        perror("setsockopt");
        goto cleanup;
    }
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &br, sizeof(br));
    if(::bind(sock,(struct sockaddr*)&(addr), sizeof(struct sockaddr_in))==-1)
    {
        perror("bind");
        goto cleanup;
    }
    addr.sin_addr.s_addr = INADDR_BROADCAST;
    for (int i=0; i<3; i++)
    {
        char msg[] = "Train Simulator Client";
        if (sendto(sock, msg, sizeof(msg), 0,(struct sockaddr *)&(addr), sizeof(struct sockaddr_in))==-1) continue;
        char buff[25];
        struct sockaddr_in from;
#ifdef _WIN32
        int size = sizeof(struct sockaddr_in);
#else
        socklen_t size = sizeof(struct sockaddr_in);
#endif
        for (int j=0; j<3; j++)
        {
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            fd_set fds;
            FD_ZERO(&fds) ;
            FD_SET(sock, &fds);
            if (select(sock+1, &fds, nullptr, nullptr, &tv) == 1)
            {
                if (recvfrom(sock,buff,25,0,(struct sockaddr*)&from,&size) == -1) perror("recvfrom");
                else if (std::string(buff) == "Train Simulator Server")
                {
                    ip = inet_ntoa(from.sin_addr);
                    goto cleanup;
                }
                else continue;
            }
        }
    }
cleanup:
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    return ip;
} 
