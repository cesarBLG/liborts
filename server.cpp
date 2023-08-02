#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#else
#include <winsock2.h>
#include <windows.h>
#endif
#include <orts/clientext.h>
#include <orts/common.h>
#include <orts/server.h>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <chrono>

using namespace std;

using namespace ORserver;
bool go=true;
void quit(int sig)
{
    go = false;
}
void server_broadcast()
{    
    char msg[] = "Train Simulator Socket";
    while(go)
    {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if(sock==-1)
        {
            perror("socket");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
#ifdef _WIN32
        BOOL br = 1;
        if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&br, sizeof(BOOL))==-1)
#else
        int br = 1;
        if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &br, sizeof(br))==-1)
#endif
        {
            perror("UDP server");
#ifdef _WIN32
            closesocket(sock);
#else
            close(sock);
#endif
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(5091);
        addr.sin_addr.s_addr = INADDR_BROADCAST;
        while(sendto(sock, msg, sizeof(msg), 0,(struct sockaddr *)&(addr), sizeof(struct sockaddr_in))==-1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
#ifdef _WIN32
        closesocket(sock);
#else
        shutdown(sock, SHUT_RDWR);
        close(sock);
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}
class SerialManager
{
    set<string> connectable;
    set<string> connected;
    public:
    SerialManager()
    {
#ifndef RELEASE
#ifdef WIN32
        /*connectable.insert("COM5");
        connectable.insert("COM6");
        connectable.insert("COM7");
        connectable.insert("COM8");
        connectable.insert("COM9");
        connectable.insert("COM10");*/
#else
        /*connectable.insert("/dev/ttyACM0");
        connectable.insert("/dev/ttyACM1");
        connectable.insert("/dev/ttyACM2");
        connectable.insert("/dev/ttyACM3");
        connectable.insert("/dev/ttyUSB0");
        connectable.insert("/dev/ttyUSB1");
        connectable.insert("/dev/ttyUSB2");
        connectable.insert("/dev/ttyUSB3");*/
#endif
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
void run_server(Server &srv, bool is_local)
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
#ifdef _WIN32
    char ra = 1;
#else
    int ra = 1;
#endif
    if(setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &ra, sizeof(ra))==-1) {
        perror("setsockopt");
        return;
    }
    if (::bind(server, (struct sockaddr *)&(serv), sizeof(serv)) == -1) {
        perror("bind");
        return;
    }
    if (listen(server, SOMAXCONN) == -1) {
        perror("listen");
        return;
    }
    evwait *poller = new threadwait();
    poller->add({FD, server});
    thread broadcast;
    if (is_local) srv.AddClient(TCPclient::connect_to_server(poller));
    else broadcast = thread(server_broadcast);
#ifdef WIN32
    /*string port;
    cout<<"Escriba numero de puerto: ";
    cin>>port;
    auto cl = new SerialClient(port, 115200, poller);
    this_thread::sleep_for(chrono::seconds(3));
    srv.AddClient(cl);*/
#endif
    SerialManager serial;
    bool err = false;
    while (go) {
        auto ser = serial.available();
        for(auto it=ser.begin(); it!=ser.end(); ++it)
        {
            srv.AddClient(new SerialClient(*it, 115200, poller));
            cout<<"Added "<<*it<<endl;
        }
        //auto t1 = chrono::system_clock::now();
        int nfds = poller->poll(5000);
        //auto t2 = chrono::system_clock::now();
        //cout<<chrono::duration_cast<chrono::duration<int, milli>>(t2-t1).count()<<endl;
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
            cout<<"Added TCP client"<<endl;
            srv.AddClient(new TCPclient(cl, poller));
        }
        const std::set<client*> &clients = srv.get_clients();
        for (auto it=clients.begin(); it!=clients.end();) {
            (*it)->handle();
            string s = (*it)->ReadLine();
            while (s != "") {
                srv.ParseLine(*it, s);
                s = (*it)->ReadLine();
            }
            if(!(*it)->connected) {
                client *c = *it;
                ++it;
                srv.RemoveClient(c);
                delete c;
            } else {
                ++it;
            }
        }
    }
    printf("Waiting for clients to close...\n");
    const std::set<client*> &clients = srv.get_clients();
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
    if (!is_local) broadcast.join();
}
int main(int argc, char **argv)
{
    cout<<"Servidor de comunicación entre periféricos y simulador."<<endl;
    cout<<"Por favor, no cierre esta ventana mientras quiera utilizar el simulador"<<endl;
    
    bool local_server = false;
    if (argc > 1 && argv[1][0] == 'l')
    {
        local_server = true;
        cout<<"Servidor local"<<endl;
    }
    
#ifndef _WIN32
    signal(SIGINT, quit);
    signal(SIGPIPE, SIG_IGN);
#endif
    Server server;
    run_server(server, local_server);
    return 0;
}
