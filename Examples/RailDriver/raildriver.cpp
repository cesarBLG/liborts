#ifdef _WIN32
#include "PieHid32.h"
#else
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/signal.h>
#endif
#include <stdio.h>
#include <string>
#include <algorithm>
#include <iostream>
#include "../../client.h"
#include "../../common.h"
using namespace std;
using namespace ORserver;
float FullReversed = 209;
float Neutral = 122;
float FullForward = 72;
float FullThrottle = 61;
float ThrottleIdle = 125;
float DynamicBrake = 210;
float DynamicBrakeSetup = 163;
float AutoBrakeRelease = 190;
float FullAutoBrake = 92;
float EmergencyBrake = 79;
float IndependentBrakeRelease = 182;
float BailOffEngagedRelease = 187;
float IndependentBrakeFull = 61;
float BailOffEngagedFull = 201;
float BailOffDisengagedRelease = 140;
float BailOffDisengagedFull = 150;
float Rotary1Position1 = 98;
float Rotary1Position2 = 139;
float Rotary1Position3 = 175;
float Rotary2Position1 = 79;
float Rotary2Position2 = 132;
float Rotary2Position3 = 161;
unsigned char LEDDigits[] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };

struct BotonesASFA
{
    unsigned int AnPar:1;
    unsigned int AnPre:1;
    unsigned int PrePar:1;
    unsigned int Modo:1;
    unsigned int Rearme:1;
    unsigned int Rebase:1;
    unsigned int AumVel:1;
    unsigned int Alarma:1;
    unsigned int Ocultacion:1;
    unsigned int LVI:1;
    unsigned int PN:1;
};
struct RDState
{
    float DirectionPercent;
    float ThrottlePercent;
    float DynamicBrakePercent;
    float TrainBrakePercent;
    float EngineBrakePercent;
    int BailOff;
    int Emergency;
    int Wipers;
    int Lights;
    int Gears;
    int Horn;
    int Bell;
    int Modocond;
    union 
    {
        BotonesASFA ASFA;
        unsigned long long bot;
    };
} State;
float Percentage(float x, float x0, float x100);
float Percentage(float x, float xminus100, float x0, float xplus100);
float clamped(float v)
{
    return max(min(v/100, (float)1), (float)0);
}
string getFunction(string param, string val)
{
    return param+'='+val+'\n';
}
float speed=0;
POSIXclient *s_client;
set<Parameter*> parameters;
Parameter *GetParameter(string name)
{
    for(auto it=parameters.begin(); it!=parameters.end(); ++it) {
        if(**it==name) return *it;
    }
    Parameter *p = new Parameter(name);
    if(name=="direction") {
        p->GetValue = [] {return to_string(State.DirectionPercent > 90 ? 1 : (State.DirectionPercent < -90 ? -1 : 0));};
    } else if(name=="throttle") {
        p->GetValue = [] {return State.Modocond==2 ? "0" : to_string(clamped(State.ThrottlePercent));};
    } else if(name=="cruise_speed") {
        p->GetValue = [] {return State.Modocond==2 ? to_string(clamped(State.ThrottlePercent)*120) : "0";};
    } else if(name=="train_brake") {
        p->GetValue = [] {return to_string(clamped(State.DynamicBrakePercent));};
    /*} else if(name=="train_brake") {
        p->GetValue = [] {return to_string(clamped(State.TrainBrakePercent));};*/
    } else if(name=="horn") {
        p->GetValue = [] {return to_string(State.Horn);};
    } else if(name=="bell") {
        p->GetValue = [] {return to_string(State.Bell);};
    } else if(name=="headlight") {
        p->GetValue = [] {return to_string(State.Lights);};
    } else if(name=="asfa_pulsador_anpar") {
        p->GetValue = [] {return to_string(State.ASFA.AnPar);};
    } else if(name=="asfa_pulsador_anpre") {
        p->GetValue = [] {return to_string(State.ASFA.AnPre);};
    } else if(name=="asfa_pulsador_prepar") {
        p->GetValue = [] {return to_string(State.ASFA.PrePar);};
    } else if(name=="asfa_pulsador_modo") {
        p->GetValue = [] {return to_string(State.ASFA.Modo);};
    } else if(name=="asfa_pulsador_rearme") {
        p->GetValue = [] {return to_string(State.ASFA.Rearme);};
    } else if(name=="asfa_pulsador_rebase") {
        p->GetValue = [] {return to_string(State.ASFA.Rebase);};
    } else if(name=="asfa_pulsador_aumento") {
        p->GetValue = [] {return to_string(State.ASFA.AumVel);};
    } else if(name=="asfa_pulsador_alarma") {
        p->GetValue = [] {return to_string(State.ASFA.Alarma);};
    } else if(name=="asfa_pulsador_ocultacion") {
        p->GetValue = [] {return to_string(State.ASFA.Ocultacion);};
    } else if(name=="asfa_pulsador_lvi") {
        p->GetValue = [] {return to_string(State.ASFA.LVI);};
    } else if(name=="asfa_pulsador_pn") {
        p->GetValue = [] {return to_string(State.ASFA.PN);};
    } else if(name=="speed") {
        p->SetValue = [](string val) {speed = stof(val);};
    } else {
        delete p;
        return nullptr;
    }
    p->add_register(s_client);
    parameters.insert(p);
    return p;
}
void parse_rd(unsigned char *rdata)
{
    State.DirectionPercent = Percentage(rdata[0], FullReversed, Neutral, FullForward);

    State.ThrottlePercent = Percentage(rdata[1], ThrottleIdle, FullThrottle);

    State.DynamicBrakePercent = Percentage(rdata[1], ThrottleIdle, DynamicBrakeSetup, DynamicBrake);
    State.TrainBrakePercent = Percentage(rdata[2], AutoBrakeRelease, FullAutoBrake);
    State.EngineBrakePercent = Percentage(rdata[3], IndependentBrakeRelease, IndependentBrakeFull);
    float a = .01f * State.EngineBrakePercent;
    float calOff = (1 - a) * BailOffDisengagedRelease + a * BailOffDisengagedFull;
    float calOn = (1 - a) * BailOffEngagedRelease + a * BailOffEngagedFull;
    State.BailOff = Percentage(rdata[4], calOff, calOn) > 50;
    if (State.TrainBrakePercent >= 100)
        State.Emergency = Percentage(rdata[2], FullAutoBrake, EmergencyBrake) > 50;
    State.Wipers = (int)(.01 * Percentage(rdata[5], Rotary1Position1, Rotary1Position2, Rotary1Position3) + 2.5);
    State.Lights = (int)(.01 * Percentage(rdata[6], Rotary2Position1, Rotary2Position2, Rotary2Position3) + 2.5);
    State.Horn = (rdata[12]&4) != 0;
    State.Bell = (rdata[12]&8) != 0;
    State.Modocond = State.Wipers;
    unsigned long long l = rdata[7] + 128*(rdata[9]>>1);
    State.bot = l;
    //printf("%d\n", (unsigned int)rdata[1]);
}
#ifdef _WIN32
unsigned long __stdcall datacallback(unsigned char *pData, unsigned long deviceID, unsigned long error);
unsigned long __stdcall errorcallback(unsigned long deviceID, unsigned long error);
class rdwait : public evwait
{
    long rdhandle;
    bool rdata = false;
    bool rerror = false;
    public:
    static condition_variable cv;
    static mutex mtx;
    static bool data;
    static bool error;
    rdwait(long rdhandle) : rdhandle(rdhandle)
    {
        data = error = false;
        SetDataCallback(rdhandle, datacallback);
        SetErrorCallback(rdhandle, errorcallback);
    }
    //~rdwait();
    void add(event ev) override
    {
        return;
    }
    void remove(event ev) override
    {
        return;
    }
    int get_events(event ev) override
    {
        if(ev.id == rdhandle) return rdata*POLLREAD + rerror*POLLERROR;
        return 0;
    }
    int poll(int timeout) override
    {
        if(data||error) return 1;
        unique_lock<mutex> lck(mtx);
        cv.wait_for(lck, chrono::milliseconds(timeout));
        rdata |= data;
        rerror |= error;
        error=data=false;
        if(rerror)
            return -1;
        if(rdata)
            return 1;
        return 0;
    }
};
bool rdwait::data = 0;
bool rdwait::error = 0;
condition_variable rdwait::cv;
mutex rdwait::mtx;
unsigned long __stdcall datacallback(unsigned char *pData, unsigned long deviceID, unsigned long error)
{
    rdwait::data = true;
    rdwait::cv.notify_all();
    return true;
}
unsigned long __stdcall errorcallback(unsigned long deviceID, unsigned long error)
{
    rdwait::error = true;
    rdwait::cv.notify_all();
    return true;
}
int read(long handle, unsigned char *buff, int count)
{
    ReadData(handle, buff);
}
int write(long handle, unsigned char *buff, int count)
{
    WriteData(handle, buff);
}
#endif
void quit(int sig)
{
    s_client->connected = false;
}
int prevc=-1;
int main()
{
    threadwait *poller = new threadwait();
#ifdef __linux__
    signal(SIGINT, quit);
    int rd = open("/dev/raildriver", O_RDWR);
    if(rd==-1) {
        perror("/dev/raildriver");
        return -1;
    }
    poller->add({FD, rd});
#elif defined _WIN32
    long count;
    long rd;
    TEnumHIDInfo info[128];
    if(EnumeratePIE(0x5F3, info, count)!=0)
        return -1;
    for (long i = 0; i < count; i++) {
        int pid;
        pid=info[i].PID; //get the device pid
		int hidusagepage=info[i].UP; //hid usage page
		int version=info[i].Version;
		int writelen=GetWriteLength(info[i].Handle);
        long hnd;
		if ((hidusagepage == 0xC && writelen==36)) {
            hnd = info[i].Handle; //handle required for piehid32.dll calls
			if(SetupInterfaceEx(hnd)!=0) {
                return -1;
            } else if(pid == 0xD2) {
                rd = hnd;
                break;
            }
        }
    }
    poller->add({EXTERNAL, rd, new rdwait(rd)});
#endif
    s_client = TCPclient::connect_to_server(poller);
    s_client->WriteLine("register(speed)");
    while(s_client->connected) {
        int nfds = poller->poll(100);
        if (nfds == 0)
            continue;
        if (nfds == -1) {
            perror("poll");
            break;
        }
#ifdef _WIN32
        int revents = poller->get_events({EXTERNAL, rd});
#else
        int revents = poller->get_events({FD, rd});
#endif
        if(revents>0) {
            unsigned char rdata[14];
            int res = read(rd, rdata, 14);
            if (res == -1) {
                perror("raildriver read");
                s_client->connected = false;
            } else if (res == 14) {
                parse_rd(rdata);
            }
        }
        s_client->handle();
        string s = s_client->ReadLine();
        while(s!="") {
            ParseLine(s_client, s, GetParameter, [](client *c, Parameter *p) {parameters.erase(p);});
            s = s_client->ReadLine();
        }
        for_each(parameters.begin(), parameters.end(), [](Parameter* p){p->Send();});
        
        int c;
        if(State.Wipers == 2)
            c = (int)(clamped(State.ThrottlePercent)*120);
        else
            c = (int)speed;
        if (c!=prevc) {
            unsigned char led[] = {0, 134, LEDDigits[c%10], LEDDigits[(c/10)%10], LEDDigits[c/100], 0, 0, 0, 0};
            write(rd, led, 9);
            prevc = c;
        }
    }
    delete poller;
#ifndef _WIN32
    close(rd);
#endif
    delete s_client;
    return 0;
}
float Percentage(float x, float x0, float x100)
{
    float p = 100 * (x - x0) / (x100 - x0);
    if (p < 5)
        return 0;
    if (p > 95)
        return 100;
    return p;
}
float Percentage(float x, float xminus100, float x0, float xplus100)
{
    float p = 100 * (x - x0) / (xplus100 - x0);
    if (p < 0)
        p = 100 * (x - x0) / (x0 - xminus100);
    if (p < -95)
        return -100;
    if (p > 95)
        return 100;
    return p;
}
