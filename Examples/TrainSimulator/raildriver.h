#ifndef _RAILDRIVER_TS_H
#define _RAILDRIVER_TS_H
#ifdef __cplusplus
extern "C"{
#endif
float __declspec(dllimport) GetControllerValue(int controllerId, int getType);
void __declspec(dllimport) SetControllerValue(int controllerId, float value);
void __declspec(dllimport) SetRailSimConnected(int v);
void __declspec(dllimport) SetRailDriverConnected(int v);
char __declspec(dllimport) *GetControllerList();
char __declspec(dllimport) *GetLocoName();
#ifdef __cplusplus
}
#endif
#endif //_RAILDRIVER_TS_H
