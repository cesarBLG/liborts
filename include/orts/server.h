#pragma once

#include <orts/client.h>
#include <orts/common.h>

namespace ORserver
{
    class Register
    {
        public:
        const std::string name;
        std::vector<std::string> topic;
        mutable std::set<client*> clients; //TODO: convert to map with update delta for each client
        Register(std::string name);
        bool operator <(const Register &r) const;
    };
    class Server : public ParameterManager
    {
        std::set<client*> clients;
        std::map<Register, std::set<client*>> registers;
        public:
        Parameter *GetParameter(std::string parameter) override;
        void AddClient(client *c);
        void RemoveClient(client *c);
        void AddRegisterPetition(std::string petition, client *cl);
        void UpdateEmptyRegisters();
        void RemoveRegisterPetition(std::string petition, client *cl);
        void UpdatePetitions(Parameter *p);
        const std::set<client*>& get_clients();
        void function_call(client *c, std::string fun, std::string param) override;
    };
}
