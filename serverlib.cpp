#include <orts/client.h>
#include <orts/common.h>
#include <orts/server.h>

using namespace std;
using namespace ORserver;

Register::Register(std::string name) : name(name)
{
    topic = SplitTopic(name);
}

bool Register::operator <(const Register &r) const
{
	return name<r.name;
}

Parameter* Server::GetParameter(string parameter)
{
    Parameter *p = ParameterManager::GetParameter(parameter);
    if (p==nullptr) {
        p = new ExternalParameter(parameter);
        AddParameter(p);
        UpdatePetitions(p);
    }
    return p;
}

void Server::AddRegisterPetition(string petition, client *cl)
{
    Register r(petition);
    auto it = registers.find(r);
    if (it == registers.end())
        it = registers.insert(std::pair<Register,set<client*>>(r,set<client*>())).first;
    
    it->first.clients.insert(cl);
    
    for (client *c : clients) {
        if (it->second.find(c)==it->second.end()) {
            c->WriteLine("register(" + petition + ")");
            it->second.insert(c);
        }
    }
    for (Parameter *p : parent_topic.GetAllMatches(r.topic)) {
        p->add_register(cl);
        p->Send();
    }
}

void Server::UpdateEmptyRegisters()
{
    set<Register> bad;
    for (auto it = registers.begin(); it!=registers.end(); ++it) {
        if (it->first.clients.empty())
            bad.insert(it->first);
    }
    for (Register r : bad) {
        auto it = registers.find(r);
        if (it == registers.end())
            continue;
        bool none=true;
        for (Parameter *p : parent_topic.GetAllMatches(r.topic)) {
            if (!p->clients.empty()) {
                none = false;
            } else {
                RemoveParameter(p);
                for (client *c : it->second) {
                    c->WriteLine("unregister(" + p->name + ")");
                }
                delete p;
            }
        }
        if (none) for (client *c : it->second)
            c->WriteLine("unregister(" + r.name + ")");
        registers.erase(it);
    }
}

void Server::RemoveRegisterPetition(string petition, client *cl)
{
    auto it = registers.find(Register(petition));
    if (it != registers.end()) {
        it->first.clients.erase(cl);
        for (Parameter *p : parent_topic.GetAllMatches(it->first.topic))
            p->unregister(cl);
    }
    UpdateEmptyRegisters();
}

void Server::UpdatePetitions(Parameter *p)
{
    bool found=false;
    for (auto it = registers.begin(); it!=registers.end(); ++it) {
        set<Parameter*> params = parent_topic.GetAllMatches(it->first.topic);
        if (params.find(p) != params.end()) {
            found = true;
            for (client *c : it->first.clients)
                p->add_register(c);
        }
    }
    p->Send();
}

void Server::AddClient(client *c)
{
    if(!c->connected) c->start();
    for(auto it=registers.begin(); it!=registers.end(); ++it) {
        c->WriteLine("register(" + it->first.name + ")");
        it->second.insert(c);
    }
    clients.insert(c);
}

void Server::RemoveClient(client *c)
{
    clients.erase(c);
    for(auto it=registers.begin(); it!=registers.end(); ++it) {
        it->first.clients.erase(c);
        it->second.erase(c);
        for (Parameter *p : parent_topic.GetAllMatches(it->first.topic))
            p->unregister(c);
    }
    UpdateEmptyRegisters();
    set<Parameter*> orphans;
    for(auto it=parameters.begin(); it!=parameters.end(); ++it) {
        (*it)->providers.erase(c);
        if ((*it)->providers.empty())
            orphans.insert(*it);
    }
    for (auto it=orphans.begin(); it!=orphans.end(); ++it) {
        RemoveParameter(*it);
        delete *it;
    }
    delete c;
}

void Server::function_call(client *c, string fun, string param)
{
    if (fun == "register") {
        AddRegisterPetition(param, c);
    } else if (fun == "unregister") {
        RemoveRegisterPetition(param, c);
    } else if (fun == "noretain") {
        string::size_type eq = param.find_first_of('=');
        if (eq == string::npos)
            return;
        string parameter = param.substr(0, eq);
        string value = param.substr(eq + 1);
        Parameter *p = new ExternalParameter(parameter);
        Topic temp(nullptr,"");
        temp.insert(p);
        for (auto it = registers.begin(); it!=registers.end(); ++it) {
            set<Parameter*> params = temp.GetAllMatches(it->first.topic);
            if (params.find(p) != params.end()) {
                for (client *c : it->first.clients)
                    c->WriteLine(parameter+"="+value);
            }
        }
        temp.erase(p);
        delete p;
    } else if (fun == "get") {
        Register r(param);
        for (Parameter *p : parent_topic.GetAllMatches(r.topic)) {
            c->WriteLine(p->name + '=' + p->GetValue());
        }
    }
}

const std::set<client*>& Server::get_clients()
{
    return clients;
}
