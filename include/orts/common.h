#ifndef _COMMON_H
#define _COMMON_H
#include <string>
#include <functional>
#include <map>
#include <set>
#include <vector>
#include <cmath>
#include <orts/client.h>
namespace ORserver
{   
    class Parameter;
    std::vector<std::string> SplitTopic(std::string s);
    bool ParseParenthesis(std::string line, std::string &before, std::string &inside);
    
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
    class Topic
    {
        Topic *parent;
        std::set<Parameter*> parameters;
        std::set<Topic*> subtopics;
        std::string topic_name;
        public:
        Topic(Topic *parent, std::string name) : parent(parent), topic_name(name) {}
        void insert(Parameter *p, int depth=0);
        void erase(Parameter *p, int depth=0);
        std::set<Parameter*> GetAllMatches(std::vector<std::string> match);
        Parameter *GetMatch(std::vector<std::string> match);
    };
    class Parameter
    {
        public:
        ParameterData *data;
        std::set<client*> providers;
        std::set<client*> clients;
        std::function<std::string()> GetValue = nullptr;
        std::function<void(std::string)> SetValue = nullptr;
        const std::string name;
        std::vector<std::string> split_name;
        Parameter(std::string name) : name(name)
        {
            split_name = SplitTopic(name);
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
    class ParameterManager
    {
        protected:
        Topic parent_topic;
        virtual void function_call(client *c, std::string fun, std::string param);
        virtual Parameter *GetParameter(std::string name);
        public:
        std::set<Parameter*> parameters;
        ParameterManager() : parent_topic(nullptr,"") {}
        virtual ~ParameterManager() {}
        void AddParameter(Parameter *p)
        {
            parameters.insert(p);
            parent_topic.insert(p);
        }
        void RemoveParameter(Parameter *p)
        {
            parameters.erase(p);
            parent_topic.erase(p);
        }
        void ParseLine(client *c, std::string line);
    };
}
/*extern "C" {
void ParseLine(void *hclient, const char* line, void *getparameter, void *unregister);
int ParseParenthesis(const char *line, char *before, char *inside, int bcount, int icount);
void parameter_send(void *parameter);
void parameter_setvaluefun(void *parameter, void (*setvalue)(const char*));
void parameter_getvaluefun(void *parameter, void (*getvalue)(char*, int));
void parameter_getvalue(void *parameter, char *data, int size);
void parameter_setvalue(void *parameter, const char *data);
}*/
#endif
