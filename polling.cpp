#include <orts/polling.h>
#ifdef __linux__r
#define USE_EPOLL
#elif defined unix
#define USE_POLL
#endif

#ifdef _WIN32
#include <winsock2.h>
#else
#ifdef USE_EPOLL
#include <sys/epoll.h>
#elif defined USE_POLL
#include <poll.h>
#else
#include <sys/select.h>
#endif
#endif
using namespace std;

#ifdef _WIN32
class winwait : public evwait
{
    public:
    vector<HANDLE> handles;
    winwait() {}
    void add(event ev) override
    {
        handles.push_back(ev.handle);
    }
    void remove(event ev) override
    {
        int index = -1;
        for(int i=0; i<handles.size(); i++) {
            if(handles[i] == ev.handle) {
                index = i;
                break;
            }
        }
        if(index != -1)
            handles.erase(handles.begin() + index);
    }
    int get_events(event ev) override
    {
        if(WaitForSingleObject(ev.handle, 0) == WAIT_OBJECT_0)
            return POLLREAD;
        return 0;
    }
    int poll(int timeout) override
    {
        if(handles.size()==0) {
            this_thread::sleep_for(chrono::milliseconds(timeout));
            return 0;
        }
        unsigned long res = WaitForMultipleObjects(handles.size(), &handles[0], false, timeout);
        if(res == WAIT_TIMEOUT)
            return 0;
        if(res == WAIT_FAILED)
            return -1;
        if(res - WAIT_OBJECT_0 >= handles.size())
            return -1;
        return 1;
    }
};
#endif
class fdwait : public evwait
{
    public:
    static fdwait * create();
};
#ifdef USE_POLL
class poll_poll : public fdwait
{
    vector<pollfd> fds;
    map<int, int> fdindex;
    public:
    void add(event ev) override
    {
        int fd = ev.id;
        pollfd p;
        p.fd = fd;
        p.events = POLLREAD;
        fds.push_back(p);
        fdindex[fd] = fds.size()-1;
    }
    void remove(event ev) override
    {
        int fd = ev.id;
        int orig_index = fdindex[fd];
        fdindex.erase(fd);
        fds.erase(fds.begin() + orig_index);
        for (auto it = fdindex.begin(); it != fdindex.end(); ++it)
        {
            if (it->second > orig_index) --it->second;
        }
    }
    int get_events(event ev) override
    {
        int fd = ev.id;
        auto it = fdindex.find(fd);
        if (it == fdindex.end()) return POLLERROR;
        return fds[it->second].revents;
    }
    int poll(int timeout)
    {
        return ::poll(&fds[0], fds.size(), timeout);
    }
};
#elif defined USE_EPOLL
class epoll_poll : public fdwait
{
    int epollfd;
    map<int,int> eventmap;
    public:
    epoll_poll()
    {
        epollfd = epoll_create1(0);
        if(epollfd == -1)
            perror("epoll_create1");
    }
    void add(event nev) override
    {
        int fd = nev.id;
        epoll_event ev;
        ev.data.fd = fd;
        ev.events = EPOLLIN;
        if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev)==-1)
            perror("epoll_ctl");
    }
    void remove(event ev) override
    {
        int fd = ev.id;
        if(epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr)==-1)
            perror("epoll_ctl");
    }
    int get_events(event ev) override
    {
        int fd = ev.id;
        return eventmap[fd];
    }
    int poll(int timeout) override
    {
        int n = 10;
        epoll_event events[n];
        int nfds = epoll_wait(epollfd, events, n, timeout);
        eventmap.clear();
        for(int i=0; i<nfds; i++) {
            eventmap[events[i].data.fd] = events[i].events;
        }
        return nfds;
    }
};
#else
class select_poll : public fdwait
{
    fd_set read_orig;
    fd_set except_orig;
    fd_set read;
    fd_set except;
    set<int> fds;
    public:
    select_poll()
    {
        FD_ZERO(&read_orig);
        FD_ZERO(&except_orig);
    }
    void add(event ev) override
    {
        int fd = ev.id;
        FD_SET(fd, &read_orig);
        fds.insert(fd);
    }
    void remove(event ev) override
    {
        int fd = ev.id;
        FD_CLR(fd, &read_orig);
        FD_CLR(fd, &except_orig);
        fds.erase(fd);
    }
    int get_events(event ev) override
    {
        int fd = ev.id;
        return (FD_ISSET(fd, &read) * POLLREAD) | (FD_ISSET(fd, &except) * POLLERROR);
    }
    int poll(int timeout) override
    {
        read = read_orig;
        except = except_orig;
        timeval tv;
        tv.tv_sec = timeout/1000;
        tv.tv_usec = (timeout-tv.tv_sec*1000)*1000;
        return select((*(--fds.end())) + 1, &read, nullptr, &except, &tv);
    }
};
#endif

threadwait::threadwait()
{
    fdwaiter = fdwait::create();
#ifdef WIN32
    winwaiter = new winwait();
#endif
}
threadwait::~threadwait()
{
    running = true;
    unique_lock<mutex> lck(mtx);
    cv.notify_all();
    for(auto it = waiters.begin(); it!=waiters.end(); ++it) {
        evwait *w = *it;
        if(w->running) {
            cv.wait(lck);
        }
    }
    delete fdwaiter;
}
void threadwait::add(event ev)
{
    if(ev.type == FD)
        ev.waiter = fdwaiter;
#ifdef WIN32
    else if(ev.type == WINHANDLE)
        ev.waiter = winwaiter;
#endif
    events.insert(ev);
    ev.waiter->add(ev);
    waiters.insert(ev.waiter);
}
void threadwait::remove(event ev)
{
    auto it = events.find(ev);
    if (it == events.end())
        return;
    evwait *w = (*it).waiter;
    w->remove(ev);
    events.erase(it);
}
int threadwait::get_events(event ev)
{
    auto it = events.find(ev);
    if (it == events.end())
        return -1;
    evwait *w = (*it).waiter;
    if(w->ready)
        return w->get_events(ev);
    return 0;
}
int threadwait::poll(int timeout)
{
    ready = false;
    if(waiters.size() == 1) {
        evwait *w = *waiters.begin();
        w->ready = false;
        int res = w->poll(timeout);
        w->ready = true;
        ready = true;
        return res;
    }
    unique_lock<mutex> lck(mtx);
    for(auto it = waiters.begin(); it!=waiters.end(); ++it) {
        evwait *w = *it;
        if(!w->running) {
            w->running = true;
            thread thr([this, w, timeout] {
                w->ready = false;
                int val = w->poll(timeout);
                w->ready = true;
                unique_lock<mutex> lck(mtx);
                if(!running) {
                    cv.wait(lck, [this] {return running;});
                }
                if(val!=0) {
                    if(val==-1)
                        num = -1;
                    else if(num!=-1) 
                        num += val;
                }
                w->running = false;
                lck.unlock();
                cv.notify_all();
            });
            thr.detach();
        }
    }
    num = 0;
    running = true;
    cv.notify_all();
    cv.wait_for(lck, chrono::milliseconds(timeout));
    running = false;
    ready = true;
    return num;
}
fdwait *fdwait::create()
{
#ifdef USE_EPOLL
    return new epoll_poll();
#elif defined USE_POLL
    return new poll_poll();
#else
    return new select_poll();
#endif
}
void *create_wait()
{
        return new threadwait();
}
int get_events(void *evwaiter, void *ev)
{
    return ((evwait*)evwaiter)->get_events(*((event*)ev));
}
int event_poll(void *evwaiter, int timeout)
{
    return ((evwait*)evwaiter)->poll(timeout);
}
void event_add(void *evwaiter, void *ev)
{
    ((evwait*)evwaiter)->add(*((event*)ev));
}
void event_remove(void *evwaiter, void *ev);
