#ifndef TS_INTERFACE_H
#define TS_INTERFACE_H
extern void (*SetRailSimConnected)(int connected);
extern void (*SetRailDriverConnected)(int connected);
extern char* (*GetLocoName)();
extern char* (*GetControllerList)();
extern float (*GetControllerValue)(int id, int flag);
extern void (*SetControllerValue)(int id, float value);
extern bool (*GetRailSimLocoChanged)();
extern void load_dll();
#endif
