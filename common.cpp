#include "common.h"
#include <cmath>
#include <cstring>
using namespace std;
namespace ORserver{
void ParseLine(client *c, string line, function<Parameter*(string)> GetParameter, function<void(client*, Parameter*)> unregister)
{
    string fun;
    string parameter;
    if(ParseParenthesis(line, fun, parameter)) {
        if(fun=="get"||fun=="register"||fun=="unregister") {
            Parameter *p = GetParameter(parameter);
            if(p==nullptr) return;
            if(fun=="unregister") {
                unregister(c, p);
            } else if(fun=="get") {
                if(p->GetValue==nullptr) return;
                string val = p->GetValue();
                if (val!="") 
                    c->WriteLine(parameter + "=" + val);
            } else {
                p->add_register(c);
                if(p->GetValue!=nullptr) {
                    string val = p->GetValue();
                    if (val!="") 
                        c->WriteLine(parameter + "=" + val);
                }
                
            }
        }
    }
    else {
        string::size_type eq = line.find_first_of('=');
        if (eq == string::npos)
            return;
        string parameter = line.substr(0, eq);
        string value = line.substr(eq + 1);
        if (parameter=="connected")
            return;
        Parameter *p = GetParameter(parameter);
        if(p->SetValue!=nullptr)
            p->SetValue(value);
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
ParameterData *ParameterData::construct(string name)
{
    if (name == "speed" || name == "cruise_speed" || name == "distance")
        return new NumericData(0.1f);
    else if (name == "throttle" || name == "dynamic_brake" || name == "train_brake")
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
void ParseLine(void *hclient, const char* line, void *(*getparameter)(const char *), void (*unregister)(void *, void *))
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
}
