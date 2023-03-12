#include <orts/client.h>
#include <iostream>
#include <errno.h>
#include <cstring>
#include <chrono>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#endif
using namespace std;
#ifdef WIN32
int read(int fd, char *data, unsigned long length)
{
    return recv(fd, data, length, 0);
}
int write(int fd, const char *data, unsigned long length)
{
    return send(fd, data, length, 0);
}
#endif
namespace ORserver
{
    void client::start()
    {
        started = true;
        WriteLine("connected=true");
        connected = true;
    }
    void POSIXclient::start()
    {
        poller->add({FD, fd});
        /*if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
            perror("set non blocking");
            return;
        }*/
        client::start();
    }
    void POSIXclient::WriteLine(string line)
    {
        line = line+"\r\n";
        int res = write(fd, line.c_str(), line.length());
        if(res==-1) {
            perror("write");
            connected = false;
        }
    }
    void POSIXclient::handle()
    {
        int revents = poller->get_events({FD, fd});
        if (revents>0) {
            if(revents & POLLERROR) {
                connected = false;
                return;
            }
            char data[1025];
            int c = read(fd, data, 1024);
            if (c == 0 || c== -1) {
                if (c == -1) {
                    if (errno==EAGAIN)
                        return;
                    else
                        perror("read");
                }
                connected = false;
            } else {
                data[c] = 0;
                buff += data;
            }
            if (c >= 2047)
                handle();
        }
    }
    POSIXclient::~POSIXclient()
    {
        if(fd == -1)
            return;
        poller->remove({FD, fd});
#ifndef _WIN32
        if (close(fd) == -1)
            perror("close");
#endif
    }
    TCPclient::TCPclient(string ip, int port, evwait *p) : POSIXclient(p)
    {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            perror("socket client creation");
            return;
        }
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(5090);
        /*if (ip == "127.0.0.1" || ip == "localhost")
            addr.sin_addr.s_addr = INADDR_LOOPBACK;
        else
            */addr.sin_addr.s_addr = inet_addr(ip.c_str());
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            perror("tcp connect");
            return;
        }
        start();
    }
    TCPclient::~TCPclient()
    {
#ifdef _WIN32
        shutdown(fd, SD_BOTH);
        closesocket(fd);
#else
        if (shutdown(fd, SHUT_RDWR) == -1) {
            perror("shutdown");
        }
#endif
    }
    TCPclient* TCPclient::connect_to_server(evwait *p)
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
            return new TCPclient("127.0.0.1", 5090, p);
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
                return new TCPclient("127.0.0.1", 5090, p);
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
                string ip = inet_ntoa(from.sin_addr);
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
                return new TCPclient(ip, 5090, p);
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
        return new TCPclient("127.0.0.1", 5090, p);
    }
#ifndef _WIN32
    SerialClient::SerialClient(string port, int BaudRate, evwait *p) : POSIXclient(p)
    {
        fd = open(port.c_str(), O_RDWR);
        if(fd == -1) {
            perror("serial port open");
            connected = false;
            return;
        }
        struct termios tty;
        memset(&tty, 0, sizeof(tty));
        tcgetattr(fd, &tty);
        cfsetospeed(&tty, B115200);
        cfsetispeed(&tty, B115200);
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;
        tty.c_cflag &= ~CRTSCTS;
        tty.c_cc[VMIN] = 1;
        tty.c_cc[VTIME] = 5;
        tty.c_cflag |= CREAD | CLOCAL;
        cfmakeraw(&tty);
        tcflush(fd, TCIFLUSH);
        tcsetattr(fd, TCSANOW, &tty);
        thread thr([this]{
            this_thread::sleep_for(chrono::seconds(2));
            start();
        });
        thr.detach();
    }
#else
    void WindowsClient::start()
    {
        poller->add({WINHANDLE, ov.hEvent});
        client::start();
        WaitCommEvent(hComm, &evmask, &ov);
    }
    void WindowsClient::WriteLine(string line)
    {
        line = line+'\n';
        unsigned long nwritten;
        OVERLAPPED ovWrite = {0};
        ovWrite.hEvent = CreateEvent(0,true,0,0);
        int res = WriteFile(hComm, line.c_str(), line.length(), &nwritten, &ovWrite);
        if(res==-1) {
            if(GetLastError() == ERROR_IO_PENDING) {
                //WaitForSingleObject();
            }
            else {
                perror("write");
                connected = false;
            }
        }
        CloseHandle(ovWrite.hEvent);
    }
    void WindowsClient::handle()
    {
        if (!connected) return;
        int revents = poller->get_events({WINHANDLE, ov.hEvent});
        if (revents>0) {
            if(revents & POLLERROR) {
                connected = false;
                return;
            }
            COMSTAT status;
            ClearCommError(hComm, nullptr, &status);
            int toread = status.cbInQue;
            if(toread==0) {
                ResetEvent(ov.hEvent);
                WaitCommEvent(hComm, &evmask, &ov);
                return;
            }
            char data[51];
            if(toread>50)
                toread = 50;
            OVERLAPPED ovRead = {0};
            ovRead.hEvent = CreateEvent(0,true,0,0);
            DWORD c;
            int res = ReadFile(hComm, data, toread, &c, &ovRead);
            if (res== 0) {
                if (GetLastError() == ERROR_IO_PENDING) {
                    WaitForSingleObject(ovRead.hEvent, INFINITE);
                    GetOverlappedResult(hComm, &ovRead, &c, true);
                    data[c] = 0;
                    buff += data;
                } else {
                    perror("read");
                    connected = false;
                }
            } else {
                data[c] = 0;
                buff += data;
            }
            CloseHandle(ovRead.hEvent);
            ResetEvent(ov.hEvent);
            WaitCommEvent(hComm, &evmask, &ov);
        }
    }
    SerialClient::SerialClient(std::string port, int BaudRate, evwait *p) : WindowsClient(p)
    {
        connected = false;
        port = "\\\\.\\"+port;
        hComm = CreateFile(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if(hComm == INVALID_HANDLE_VALUE) {
            printf("serial port: invalid handle");
            return;
        }
        SetCommMask(hComm, EV_RXCHAR);
        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        GetCommState(hComm, &dcbSerialParams);
        dcbSerialParams.BaudRate = CBR_115200;
        SetCommState(hComm, &dcbSerialParams);
        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = 5000;
        timeouts.ReadTotalTimeoutMultiplier = 5000;
        timeouts.ReadTotalTimeoutConstant = 5000;
        timeouts.WriteTotalTimeoutMultiplier = 5000;
        timeouts.WriteTotalTimeoutConstant = 5000;
        SetCommTimeouts(hComm, &timeouts);
        EscapeCommFunction(hComm,SETDTR);
        thread thr([this]{
            this_thread::sleep_for(chrono::seconds(2));
            start();
        });
        thr.detach();
    }
#endif
    string client::ReadLine()
    {
        string::size_type ind = buff.find_first_of("\r\n");
        if(ind != string::npos) {
            string line = buff.substr(0, ind);
            buff = buff.substr(ind+1);
            if (line.size()==0 && buff.size()!=0) return ReadLine();
            return line;
        }
        return "";
    }
}
using namespace ORserver;
void client_start(void *hclient)
{
    ((client*)hclient)->start();
}
void client_handle(void *hclient)
{
    ((client*)hclient)->handle();
}
int client_connected(void *hclient)
{
    return ((client*)hclient)->connected;
}
void client_writeline(void *hclient, const char *data)
{
    ((client*)hclient)->WriteLine(data);
}
void client_readline(void *hclient, char *data, int size)
{
    string s = ((client*)hclient)->ReadLine();
    memcpy(data, s.c_str(), min(size, (int)s.size()));
}
