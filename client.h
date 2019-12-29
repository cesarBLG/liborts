#ifndef _CLIENT_H
#define _CLIENT_H
#include <string>
#include "polling.h"
namespace ORserver
{
    class client
    {
        protected:
        std::string buff;
        public:
        bool started = false;
        bool connected = false;
        virtual void start();
        virtual void WriteLine(std::string line)=0;
        std::string ReadLine();
        evwait *poller;
        client(evwait *p) : poller(p) {}
        virtual void handle()=0;
        virtual ~client()
        {
        }
    };
    class POSIXclient : public client
    {   
        protected:
        int fd;
        protected:
        POSIXclient(evwait *p) : client(p) {}
        public:
        void start() override;
        POSIXclient(int fd, evwait *p) : client(p), fd(fd) {};
        void WriteLine(std::string line) override;
        void handle() override;
        virtual ~POSIXclient();
    };
    class TCPclient : public POSIXclient
    {
        public:
        TCPclient(int fd, evwait *p) : POSIXclient(fd, p) {start();}
        TCPclient(std::string ip, int port, evwait *p);
        ~TCPclient();
    };
#ifndef _WIN32
    class SerialClient : public POSIXclient
    {
        public:
        SerialClient(std::string port, int BaudRate, evwait *p);
    };
#else
    class WindowsClient : public client
    {
        protected:
        HANDLE hComm;
        OVERLAPPED ov={0};
        DWORD evmask;
        WindowsClient(evwait *p) : client(p) 
        {
            ov.hEvent = CreateEvent(0,true,0,0);
        }
        WindowsClient(HANDLE h, evwait *p) : client(p), hComm(h) 
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
#endif
