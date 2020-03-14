#ifndef _COMMON_H
#define _COMMON_H
#include <string>
#include <functional>
#include <map>
#include <set>
#include <cmath>
#include "client.h"
namespace ORserver
{
    class ParameterData
    {
    public:
        virtual std::set<client*> get_clients_to_send(std::string value)=0;
        virtual void add_register(client *c)=0;
        virtual void unregister(client *c)=0;
        virtual ~ParameterData(){}
        static ParameterData *construct(std::string name);
    };
    class NumericData : public ParameterData
    {
        float margin;
        struct reg_data
        {
            float value;
            float margin;
        };
        std::map<client*, reg_data> registers;
        public:
        NumericData(float defaultmargin) : margin(defaultmargin)
        {
        }
        void add_register(client *c) override
        {
            registers[c] = {0, margin};
        }
        void unregister(client *c) override
        {
            registers.erase(c);
        }
        std::set<client*> get_clients_to_send(std::string value) override
        {
            std::set<client*> outdated;
            float val = stof(value);
            for(auto it = registers.begin(); it != registers.end(); ++it) {
                if(std::abs(it->second.value-val)>=margin || (val == 0 && it->second.value!=val)) {
                    outdated.insert(it->first);
                    it->second.value = val;
                }
            }
            return outdated;
        }
    };
    class DiscreteData : public ParameterData
    {
        std::map<client*,std::string> registers;
        public:
        DiscreteData() {}
        void add_register(client *c) override
        {
            registers[c] = "";
        }
        void unregister(client *c) override
        {
            registers.erase(c);
        }
        std::set<client*> get_clients_to_send(std::string value) override
        {
            std::set<client*> outdated;
            for(auto it = registers.begin(); it != registers.end(); ++it) {
                if(it->second != value) {
                    outdated.insert(it->first);
                    it->second = value;
                }
            }
            return outdated;
        }
    };
    class Parameter
    {
        public:
        ParameterData *data;
        std::set<client*> clients;
        std::set<client*> registerPetitionSent;
        const std::string name;
        std::function<std::string()> GetValue = nullptr;
        std::function<void(std::string)> SetValue = nullptr;
        Parameter(std::string name) : name(name)
        {
            data = ParameterData::construct(name);
        }
        virtual ~Parameter() 
        {
            delete data;
        }
        virtual void Send();
        bool operator ==(std::string s) const
        {
            return name==s;
        }
        bool operator ==(const Parameter &p) const
        {
            return name==p.name;
        }
        bool operator <(const Parameter &p) const
        {
            return name<p.name;
        }
        void add_register(client *c)
        {
            clients.insert(c);
            data->add_register(c);
        }
        void unregister(client *c)
        {
            data->unregister(c);
            clients.erase(c);
        }
    };
    class ExternalParameter : public Parameter
    {
        std::string value="";
        bool set;
        public:
        ExternalParameter(std::string name);
        void Send() override
        {
            if(set)
                Parameter::Send();
        }
        bool IsSet()
        {
            return set;
        }
    };
    void ParseLine(client *c, std::string line, std::function<Parameter*(std::string)> GetParameter,  std::function<void(client *, Parameter*)> unregister);
    bool ParseParenthesis(std::string line, std::string &before, std::string &inside);
}
extern "C" {
void ParseLine(void *hclient, const char* line, void *getparameter, void *unregister);
int ParseParenthesis(const char *line, char *before, char *inside, int bcount, int icount);
void parameter_send(void *parameter);
void parameter_setvaluefun(void *parameter, void (*setvalue)(const char*));
void parameter_getvaluefun(void *parameter, void (*getvalue)(char*, int));
void parameter_getvalue(void *parameter, char *data, int size);
void parameter_setvalue(void *parameter, const char *data);
}
#endif
