#include <windows.h>
void (*SetRailSimConnected)(int connected);
void (*SetRailDriverConnected)(int connected);
const char* (*GetLocoName)();
const char* (*GetControllerList)();
float (*GetControllerValue)(int id, int flag);
void (*SetControllerValue)(int id, float value);
void load_dll()
{
#ifdef TS_DLL_STUB
    SetRailDriverConnected = [](int val){};
    SetRailSimConnected = [](int val){};
    GetLocoName = [](){return "Test stub";};
    GetControllerList = [](){return "VirtualThrottle::DynamicBrake::SpeedometerKPH";};
    SetControllerValue = [](int id, float val){};
    GetControllerValue = [](int id, int flag){if(flag==2)return 1.0f; return 0.0f;};
#else
    HINSTANCE dll = LoadLibrary("RailDriver64.dll");
    SetRailSimConnected = (void(_stdcall *)(int)) GetProcAddress(dll, "SetRailSimConnected");
    SetRailDriverConnected = (void(_stdcall *)(int)) GetProcAddress(dll, "SetRailDriverConnected");
    GetLocoName = (const char* (_stdcall *)(void)) GetProcAddress(dll, "GetLocoName");
    GetControllerList = (const char* (_stdcall *)(void)) GetProcAddress(dll, "GetControllerList");
    GetControllerValue = (float(_stdcall *)(int, int)) GetProcAddress(dll, "GetControllerValue");
    SetControllerValue = (void(_stdcall *)(int, float)) GetProcAddress(dll, "SetControllerValue");
#endif
}
