#include "../../client.h"
#include "../../common.h"
#include "interface.h"
#include <iostream>
#include <set>
#include <vector>
#include <sstream>
#include <string>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <fstream>
#include "liborts_setup.h"
using namespace std;
using namespace ORserver;
TCPclient *tcp_client;

class tswait : public evwait
{
    long id;
    public:
    condition_variable cv;
    mutex mtx;
    bool data;
    bool error;
    tswait(long id) : id(id)
    {
    }
    void add(event ev) override
    {
        return;
    }
    void remove(event ev) override
    {
        return;
    }
    int get_events(event ev) override
    {
        if(ev.type == EXTERNAL && ev.id == id) return POLLREAD;
        return 0;
    }
    int poll(int timeout) override
    {
        unique_lock<mutex> lck(mtx);
        cv.wait_for(lck, chrono::milliseconds(timeout));
        return 1;
    }
};

struct Controller
{
    int index;
    string name;
    float min;
    float max;
    string own_name;
    function<string()> get;
    function<void(string)> set;
    bool operator<(const Controller c) const
    {
        return index<c.index;
    }
};
struct ControllerMap
{
    string name;
    string own_name;
    bool get;
    bool set;
    bool scale;
    bool autoscale;
    double scaleval[4];
};
vector<Controller> controllers;
#define CONTROLLER_GET 0
#define CONTROLLER_MIN 1
#define CONTROLLER_MAX 2
Controller GetControllerByOwnName(string name)
{
    for(auto it=controllers.begin(); it!=controllers.end(); ++it) {
        if(it->own_name == name)
            return *it;
    }
    Controller c;
    c.index = -1;
    return c;
}
set<Parameter*> parameters; 
Parameter *GetParameter(string name)
{
    for(auto it=parameters.begin(); it!=parameters.end(); ++it) {
        if(**it==name) return *it;
    }
    Controller c = GetControllerByOwnName(name);
    if(c.index == -1)
        return nullptr;
    Parameter *p = new Parameter(name);
    if(c.get != nullptr) {
        p->GetValue = [c] {return c.get();};
    }
    if(c.set != nullptr) {
        p->SetValue = [c](string val) {c.set(val);};
    }
    p->add_register(tcp_client);
    parameters.insert(p);
    return p;
}
inline float map_values(float val, float min0, float max0, float min1, float max1)
{
    return (val-min0)*(max1-min1)/(max0-min0)+min1;
}
int main()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    load_dll();
    cout<<"Waiting for connection..."<<endl;
    Sleep(1000);
    bool connected = false;
    while(!connected)
    {
        SetRailSimConnected(1);
        SetRailDriverConnected(1);
        Sleep(1000);
        connected = (GetLocoName()!=0 && *GetLocoName()!=0);
        Sleep(1000);
    }
    cout<<"Connected"<<endl;
    string contlist = GetControllerList();
    size_t pos = contlist.find(':');
    for(int i=0; ; i++) {
        Controller c;
        c.index = i;
        c.name = contlist.substr(0, pos);
        c.min = GetControllerValue(i, CONTROLLER_MIN);
        c.max = GetControllerValue(i, CONTROLLER_MAX);
        controllers.push_back(c);
        if(pos==string::npos)
            break;
        contlist = contlist.substr(pos+2);
        pos = contlist.find(':');
        i++;
    }
    vector<ControllerMap> cmap;
    ifstream maps("maps.txt");
    string line;
    while(getline(maps, line)) {
        stringstream s(line);
        ControllerMap m;
        s>>m.own_name;
        s>>m.name;
        string d;
        s>>d;
        m.get = d.find("get") != string::npos;
        m.set = d.find("set") != string::npos;
        s>>d;
        m.scale = d.find("noscale")==string::npos;
        m.autoscale = d.find("autoscale")!=string::npos;
        if(m.scale)
        {
            s>>d;
            m.scaleval[0] = stoi(d);
            s>>d;
            m.scaleval[1] = stoi(d);
            if(!m.autoscale)
            {
                s>>d;
                m.scaleval[2] = stoi(d);
                s>>d;
                m.scaleval[3] = stoi(d);
            }
        }
        cmap.push_back(m);
    }
    threadwait *poller = new threadwait();
    poller->add({EXTERNAL, 1, new tswait(1)});
    tcp_client = new TCPclient("127.0.0.1", 5090, poller);
    for(int i=0; i<controllers.size(); i++) {
        for(int j=0; j<cmap.size(); j++) {
            Controller &c = controllers[i];
            ControllerMap &m = cmap[j];
            if(c.name == m.name) {
                tcp_client->WriteLine("register("+m.own_name+")");
                c.own_name = m.own_name;
                if(m.set)
                    c.set = [c, m](string val) {
                        float valf = stof(val);
                        if(m.scale) {
                            if(m.autoscale)
                                valf = map_values(valf, m.scaleval[0], m.scaleval[1], c.min, c.max);
                            else
                                valf = map_values(valf, m.scaleval[0], m.scaleval[1], m.scaleval[2], m.scaleval[3]);
                        }
                        SetControllerValue(c.index, valf);
                    };
                if(m.get)
                    c.get = [c, m]() {
                        float val = GetControllerValue(c.index, CONTROLLER_GET);
                        if(m.scale) {
                            if(m.autoscale)
                                val = map_values(val, c.min, c.max, m.scaleval[0], m.scaleval[1]);
                            else
                                val = map_values(val, m.scaleval[2], m.scaleval[3], m.scaleval[0], m.scaleval[1]);
                        }
                        return to_string(val);
                    };
            }
        }
    }
    while(tcp_client->connected && connected) {
        int nfds = poller->poll(100);
        if (nfds == 0)
            continue;
        if (nfds == -1) {
            perror("poll");
            break;
        }
        tcp_client->handle();
        string s = tcp_client->ReadLine();
        while(s!="") {
            ParseLine(tcp_client, s, GetParameter, [](client *c, Parameter *p) {parameters.erase(p);});
            s = tcp_client->ReadLine();
        }
        for_each(parameters.begin(), parameters.end(), [](Parameter* p){p->Send();});
    }
    delete tcp_client;
    delete poller;
    return 0;
}
