#include <orts/common.h>
#include <cmath>
#include <cstring>
using namespace std;
namespace ORserver{
    
void Topic::insert(Parameter *p, int depth)
{
    if (p->split_name.size() == depth + 1) {
        parameters.insert(p);
    } else {
        for (Topic *t : subtopics) {
            if (t->topic_name == p->split_name[depth]) {
                t->insert(p, depth+1);
                return;
            }
        }
        Topic *t = new Topic(this, p->split_name[depth]);
        t->insert(p, depth+1);
        subtopics.insert(t);
    }
}
void Topic::erase(Parameter *p, int depth)
{
    if (p->split_name.size() == depth + 1) {
        parameters.erase(p);
    } else {
        for (Topic *t : subtopics) {
            if (t->topic_name == p->split_name[depth]) {
                t->erase(p, depth+1);
                if (t->parameters.empty() && t->subtopics.empty()) {
                    subtopics.erase(t);
                    delete t;
                }
                return;
            }
        }
    }
}
std::set<Parameter*> Topic::GetAllMatches(std::vector<std::string> match)
{
    std::set<Parameter*> params;
    
    if (match[0] == "*") {
        params = parameters;
        for (Topic *t : subtopics) {
            std::set<Parameter*> more = t->GetAllMatches(match);
            params.insert(more.begin(), more.end());
        }
        return params;
    }
    
    if (match.size() == 1) {
        for (Parameter *p : parameters) {
            if (p->split_name.back() == match.back())
                params.insert(p);
        }
    } else {
        for (Topic *t : subtopics) {
            if (match[0] == t->topic_name || match[0] == "+") {
                vector<string> newmatch = match;
                newmatch.erase(newmatch.begin());
                std::set<Parameter*> more = t->GetAllMatches(newmatch);
                params.insert(more.begin(), more.end());
            }
        }
    }
    return params;
}
Parameter *Topic::GetMatch(std::vector<std::string> match)
{
    if (match.size() == 1) {
        for (Parameter *p : parameters) {
            if (p->split_name.back() == match.back())
                return p;
        }
    } else {
        for (Topic *t : subtopics) {
            if (match[0] == t->topic_name) {
                match.erase(match.begin());
                return t->GetMatch(match);
            }
        }
    }
    return nullptr;
}

void ParameterManager::function_call(client *c, string fun, string param)
{
    if (fun == "register") {
        for (Parameter *p : parent_topic.GetAllMatches(SplitTopic(param))) {
            p->add_register(c);
        }
    } else if (fun == "unregister") {
        for (Parameter *p : parent_topic.GetAllMatches(SplitTopic(param))) {
            p->unregister(c);
        }
    }
}
Parameter *ParameterManager::GetParameter(string name)
{
    return parent_topic.GetMatch(SplitTopic(name));
}
void ParameterManager::ParseLine(client *c, string line)
{
    string fun;
    string param;
    string::size_type eq = line.find_first_of('=');
    if (line.find_first_of('(')<eq && ParseParenthesis(line, fun, param)) {
        function_call(c, fun, param);
    }
    else {
        if (eq == string::npos)
            return;
        string parameter = line.substr(0, eq);
        string value = line.substr(eq + 1);
        if (parameter=="connected")
            return;
        Parameter *p = GetParameter(parameter);
        if(p!=nullptr && p->SetValue!=nullptr) {
            p->SetValue(value);
            p->providers.insert(c);
        }
    }
}
bool ParseParenthesis(string line, string &before, string &inside)
{
    string::size_type par1 = line.find_first_of('(');
    string::size_type par2 = line.find_last_of(')');
    if (par1 == string::npos || par2 == string::npos)
        return false;
    before = line.substr(0, par1);
    inside = line.substr(par1 + 1, par2 - par1 - 1);
    return true;  
}
std::vector<std::string> SplitTopic(std::string s)
{
    std::vector<std::string> vec;
    int index=-2;
    do {
        s = s.substr(index+2);
        index = s.find_first_of("::");
        vec.push_back(s.substr(0,index));
    } while(index != std::string::npos);
    return vec;
}
ParameterData *ParameterData::construct(string name)
{
    if (name == "speed" || name == "cruise_speed" || name == "distance" || name == "simulator_time")
        return new NumericData(0.1f);
    else if (name == "controller::throttle" || name == "controller::brakes::dynamic" || name == "controller::brakes::train")
        return new NumericData(0.01f);
    else
        return new DiscreteData();
}
void Parameter::Send()
{
    if(GetValue==nullptr) return;
    string val = GetValue();
    set<client*> cl = data->get_clients_to_send(val);
    for(auto it=cl.begin(); it!=cl.end(); ++it)
    {
        (*it)->WriteLine(name + '=' + val);
    }
}
ExternalParameter::ExternalParameter(string name) : Parameter(name)
{
    set = false;
    SetValue = [this](string val)
    {
        set = true;
        value = val;
        Send();
    };
    GetValue = [this]() {return value;};
}
}
using namespace ORserver;
/*void ParseLine(void *hclient, const char* line, void *(*getparameter)(const char *), void (*unregister)(void *, void *))
{
    ORserver::ParseLine((client*)hclient, 
        line,
        [getparameter](string name) {return (Parameter*)getparameter(name.c_str());},
        unregister);
}
int ParseParenthesis(const char *line, char *before, char *inside, int bcount, int icount)
{
    string bstr, istr;
    bool result = ORserver::ParseParenthesis(line, bstr, istr);
    memcpy(before, bstr.c_str(), min(bcount, (int)bstr.size()));
    memcpy(inside, istr.c_str(), min(icount, (int)istr.size()));
    return result;
}
void parameter_send(void *parameter)
{
    ((Parameter*)parameter)->Send();
}
void parameter_setvaluefun(void *parameter, void (*setvalue)(const char*))
{
    ((Parameter*)parameter)->SetValue = [setvalue](string val) {setvalue(val.c_str());};
}
void parameter_getvaluefun(void *parameter, void (*getvalue)(char*, int))
{
    ((Parameter*)parameter)->GetValue = [getvalue]() {
        char c[50];
        getvalue(c, 50);
        return string(c);
    };
}
void parameter_getvalue(void *parameter, char *data, int size)
{
    string s = ((Parameter*)parameter)->GetValue();
    memcpy(data, s.c_str(), min(size, (int)s.size()));
}
void parameter_setvalue(void *parameter, const char *data)
{
    ((Parameter*)parameter)->SetValue(data);
}*/
