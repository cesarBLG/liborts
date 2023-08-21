#pragma once
#include <orts/client.h>
#include <orts/polling.h>
namespace ORserver
{
    class clientext : public client
    {
        protected:
        std::string buff;
        public:
        evwait *poller;
        clientext(evwait *p) : poller(p) {}
        virtual void start() override;
        virtual std::string ReadLine() override;
    };
    class POSIXclient : public clientext
    {   
        protected:
        int fd;
        protected:
        POSIXclient(evwait *p) : clientext(p) {}
        public:
        void start() override;
        POSIXclient(int fd, evwait *p) : clientext(p), fd(fd) {};
        void WriteLine(std::string line) override;
        void handle() override;
        ~POSIXclient();
    };
    class TCPclient : public POSIXclient
    {
        public:
        TCPclient(int fd, evwait *p) : POSIXclient(fd, p) {start();}
        TCPclient(std::string ip, int port, evwait *p);
        ~TCPclient();
        static TCPclient* connect_to_server(evwait *p);
    };
#ifndef _WIN32
    class SerialClient : public POSIXclient
    {
        public:
        SerialClient(std::string port, int BaudRate, evwait *p);
    };
#else
    class WindowsClient : public clientext
    {
        protected:
        HANDLE hComm;
        OVERLAPPED ov={0};
        DWORD evmask;
        WindowsClient(evwait *p) : clientext(p) 
        {
            ov.hEvent = CreateEvent(0,true,0,0);
        }
        WindowsClient(HANDLE h, evwait *p) : clientext(p), hComm(h) 
        {
            ov.hEvent = CreateEvent(0,true,0,0);
        }
        ~WindowsClient()
        {
            CloseHandle(hComm);
            CloseHandle(ov.hEvent);
        }
        void start() override;
        void WriteLine(std::string line) override;
        void handle() override;
    };
    class SerialClient : public WindowsClient
    {
        public:
        SerialClient(std::string port, int BaudRate, evwait *p);
    };
#endif
}

extern "C"
{
void client_start(void *hclient);
void client_handle(void *hclient);
int client_connected(void *hclient);
void client_writeline(void *hclient, const char *data);
void client_readline(void *hclient, char *data, int size);
}
