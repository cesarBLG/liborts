#ifndef _POLLING_H
#define _POLLING_H
#include <vector>
#include <map>
#include <set>
#include <thread>
#include <condition_variable>

#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#endif

#define POLLREAD 1
#define POLLWRITE 4
#define POLLERROR 24

enum evtype
{
    FD,
    WINHANDLE,
    EXTERNAL
};
class evwait;
struct event
{
    evtype type;
    union
    {
        long long id;
#ifdef _WIN32
        HANDLE handle;
#endif
    };
    evwait *waiter;
    event(evtype type, long long id) : type(type), id(id) {}
    event(evtype type, long long id, evwait* waiter) : type(type), id(id), waiter(waiter) {}
#ifdef _WIN32
    event(evtype type, HANDLE handle) : type(type), handle(handle) {}
#endif
    bool operator<(const event &ev) const
    {
        if(type==ev.type) return id<ev.id;
        return type<ev.type;
    }
    bool operator==(const event &ev) const
    {
        return type==ev.type && id==ev.id;
    }
};
class evwait
{
    public:
    bool ready=false;
    bool running=false;
    virtual void add(event ev)=0;
    virtual void remove(event ev)=0;
    virtual int get_events(event ev)=0;
    virtual int poll(int timeout)=0;
    virtual ~evwait() {}
};
class fdwait;
#ifdef _WIN32
class winwait;
#endif
class threadwait : public evwait
{
    std::mutex mtx;
    std::condition_variable cv;
    std::set<event> events;
    std::set<evwait*> waiters;
    fdwait *fdwaiter;
#ifdef _WIN32
    winwait *winwaiter;
#endif
    int num;
    public:
    threadwait();
    ~threadwait();
    void add(event ev) override;
    void remove(event ev) override;
    int get_events(event ev) override;
    int poll(int timeout) override;
};
extern "C" {
    void *create_wait();
    int get_events(void *evwaiter, void *ev);
    int event_poll(void *evwaiter, int timeout);
    void event_add(void *evwaiter, void *ev);
    void event_remove(void *evwaiter, void *ev);
}
#endif
