#ifndef _CLIENT_H
#define _CLIENT_H
#include <string>
namespace ORserver
{
    class client
    {
    public:
        bool started = false;
        bool connected = false;
        virtual void start() = 0;
        virtual void WriteLine(std::string line) = 0;
        virtual std::string ReadLine() = 0;
        virtual void handle() = 0;
        virtual ~client() = default;
    };
} 
#endif
