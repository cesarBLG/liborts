#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#else
#include <winsock2.h>
#include <windows.h>
#endif
#include "client.h"
#include "common.h"
#include <vector>
#include <set>
#include <string>

using namespace std;
namespace ORserver
{
    class Server
    {
        set<Parameter*> parameters;
        set<client*> clients;
        public:
        Server();
        Parameter *GetParameter(string parameter);
        void unregister(client *c, Parameter *p);
        void AddClient(client *c);
        void RemoveClient(client *c);
    };
}
using namespace ORserver;
bool go=true;
void quit(int sig)
{
    go = false;
}
void server_broadcast()
{
    /*int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int br = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &br, sizeof(br))==-1)
    {
        perror("UDP server");
        close(sock);
        return;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5091);
    addr.sin_addr.s_addr = INADDR_BROADCAST;
    
    char msg[] = "Train Simulator Socket";
    while(go)
    {
        if(sendto(sock, msg, sizeof(msg), 0,(struct sockaddr *)&(addr), sizeof(struct sockaddr_in))==-1)
        {
            perror("sendto");
        }
        sleep(1);
    }
    close(sock);*/
}
class SerialManager
{
    set<string> connectable;
    set<string> connected;
    public:
    SerialManager()
    {
#ifdef WIN32
        connectable.insert("COM5");
        connectable.insert("COM6");
        connectable.insert("COM7");
#else
        connectable.insert("/dev/ttyACM0");
        connectable.insert("/dev/ttyACM1");
        connectable.insert("/dev/ttyACM2");
        connectable.insert("/dev/ttyACM3");
        connectable.insert("/dev/ttyUSB0");
        connectable.insert("/dev/ttyUSB1");
        connectable.insert("/dev/ttyUSB2");
        connectable.insert("/dev/ttyUSB3");
#endif
    }
    set<string> available()
    {
        set<string> avail;
        for(auto it=connectable.begin(); it!=connectable.end(); ++it) {
            bool exists;
#ifdef WIN32
            char buff[500];
            exists = QueryDosDevice(it->c_str(), buff, 500) != 0;
#else
            exists = access(it->c_str(), 0) == 0; 
#endif
            if(exists)
            {
                if(connected.find(*it)==connected.end()) {
                    connected.insert(*it);
                    avail.insert(*it);
                }
            }
            else connected.erase(*it);
        }
        return avail;
    }
};
int main()
{
#ifndef _WIN32
    signal(SIGINT, quit);
    signal(SIGPIPE, SIG_IGN);
    int pid = fork();
    if(pid == 0) {
        //int fd = open("raildriver.log", O_WRONLY | O_CREAT);
        //dup2(fd, 1);
        return execl("./raildriver", "raildriver", (char *)nullptr);
    }
#endif
    Server *s = new Server();
    delete s;
    return 0;
}
Server::Server()
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    int server = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(5090);
    serv.sin_addr.s_addr = INADDR_ANY;
    if (server == -1) {
        perror("socket");
        return;
    }
    if (bind(server, (struct sockaddr *)&(serv), sizeof(serv)) == -1) {
        perror("bind");
        return;
    }
    if (listen(server, 10) == -1) {
        perror("listen");
        return;
    }
    evwait *poller = new threadwait();
    poller->add({FD, server});
    thread broadcast(server_broadcast);
#ifdef WIN32
    string port;
    cout<<"Escriba numero de puerto: ";
    cin>>port;
    AddClient(new SerialClient(port, 115200, poller));
#else
    SerialManager serial;
#endif
    bool err = false;
    while (go) {
        //auto t1 = chrono::system_clock::now();
        int nfds = poller->poll(5000);
        //auto t2 = chrono::system_clock::now();
        //cout<<chrono::duration_cast<chrono::duration<int, milli>>(t2-t1).count()<<endl;
        auto ser = serial.available();
        for(auto it=ser.begin(); it!=ser.end(); ++it)
        {
            AddClient(new SerialClient(*it, 115200, poller));
            cout<<"Added "<<*it<<endl;
        }
        if (nfds == 0) {
            continue;
        }
        if (nfds == -1) {
            perror("poll");
            if(err)
                break;
            else
                err = true;
        }
        err = false;
        if(!go)
            break;
        int revents = poller->get_events({FD, server});
        if(revents>0) {
            struct sockaddr_in addr;
            int c = sizeof(struct sockaddr_in);
            int cl = accept(server, (struct sockaddr *)&addr, 
#ifdef _WIN32
            (int *)
#else
            (socklen_t *)
#endif
            &c);
            if(cl == -1) {
                perror("accept");
                continue;
            }
            AddClient(new TCPclient(cl, poller));
        }
        for (auto it=clients.begin(); it!=clients.end();) {
            (*it)->handle();
            string s = (*it)->ReadLine();
            while (s != "") {
                ParseLine(*it, s, [this](string val) {return GetParameter(val);},  [this](client *c, Parameter *p) {unregister(c, p);});
                s = (*it)->ReadLine();
            }
            if(!(*it)->connected) {
                client *c = *it;
                ++it;
                RemoveClient(c);
            } else {
                ++it;
            }
        }
    }
    printf("Waiting for clients to close...\n");
    for (auto it=clients.begin(); it!=clients.end(); ++it) {
        delete *it;
    }
    delete poller;
    printf("Waiting for server to close...\n");
#ifdef _WIN32
    closesocket(server);
#else
    close(server);
#endif
    broadcast.join();
}
Parameter* Server::GetParameter(string parameter)
{
    Parameter *p=nullptr;
    for (auto it=parameters.begin(); it!=parameters.end(); ++it) {
        if (*(*it)==parameter) {
            p = *it;
            break;
        }
    }
    if (p==nullptr) {
        p = new ExternalParameter(parameter);
        parameters.insert(p);
        for (auto it=clients.begin(); it!=clients.end(); ++it) {
            (*it)->WriteLine("register(" + p->name + ")");
        }
        p->registerPetitionSent.insert(clients.begin(), clients.end());
    }
    return p;
}
void Server::unregister(client *c, Parameter *p)
{
    p->unregister(c);
    if (p->clients.size()==0) {
        for (auto it=p->registerPetitionSent.begin(); it!=p->registerPetitionSent.end(); ++it) {
            (*it)->WriteLine("unregister("+p->name+")");
        }
        parameters.erase(p);
        delete p;
    }
}
void Server::AddClient(client *c)
{
    if(!c->connected) c->start();
    for(auto it=parameters.begin(); it!=parameters.end(); ++it) {
        c->WriteLine("register(" + (*it)->name + ")");
        (*it)->registerPetitionSent.insert(c);
    }
    clients.insert(c);
}
void Server::RemoveClient(client *c)
{
    clients.erase(c);
    for (auto it=parameters.begin(); it!=parameters.end();) {
        Parameter *p = *it;
        ++it;
        p->registerPetitionSent.erase(c);
        unregister(c, p);
    }
    delete c;
}
