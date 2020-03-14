#ifndef _MODULES_H
#define _MODULES_H
#include <string>
#include <iostream>
#include <string.h>
#include <vector>
#include <set>
class ts_module
{
    static std::map<std::string, ts_module*> loaded_modules;
    public:
    const std::string name;
    ts_module(std::string name) : name(name)
    {
    }
    virtual ~ts_module() {}
    static ts_module * by_name(std::string name);
};
#ifdef _WIN32
#include <windows.h>
class windows_module : public ts_module
{
    PROCESS_INFORMATION pi;
    bool loaded;
    public:
    windows_module(std::string name, std::string command) : ts_module(name) 
    {
        STARTUPINFO si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        
        char commname[command.size()+1];
        strcpy(commname, command.c_str());
        
        if (!CreateProcess(nullptr, commname, nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pi))
            std::cerr<<"Error creating module process: "<<GetLastError()<<std::endl;
        else
            loaded = true;
    }
    ~windows_module()
    {
        if (!loaded)
            return;
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
};
#else
#include <unistd.h>
#include <signal.h>
class unix_module : public ts_module
{
    int pid;
    bool loaded;
    public:
    unix_module(std::string name, std::string path, std::vector<std::string> args) : ts_module(name) 
    {
        pid = fork();
        if (pid == -1) {
            perror("fork");
            return;
        } else if (pid == 0) {
            char * arglst[args.size()];
            for (int i=0; i<args.size(); i++) {
                arglst[i] = (char *)args[i].c_str();
            }
            execv(path.c_str(), arglst);
            exit(0);
        }
        loaded = true;
    }
    ~unix_module()
    {
        if (loaded)
            kill(pid, SIGINT);
    }
};
#endif
std::map<std::string, ts_module*> ts_module::loaded_modules;
ts_module * ts_module::by_name(std::string name)
{
    if (loaded_modules.find(name) != loaded_modules.end())
        return loaded_modules[name];
    ts_module *m;
#ifdef _WIN32
    if (name == "asfa_digital")
        m = new windows_module(name, "cmd /c ASFA.jar");
#else
    if (name == "asfa_digital")
        m = new unix_module(name, "./asfa.sh", {"asfa.sh"});
#endif
    loaded_modules[name] = m;
    return m;
}
#endif
