// THROWAWAY edict-walk audit instrumentation (branch agent/edictaudit only)
#ifndef VR_EA_H
#define VR_EA_H
#ifdef __cplusplus
extern "C" {
#endif
long long EA_Now(void);
void EA_Add(int id, long long t0);
#ifdef __cplusplus
}
struct EaScope { int id; long long t0; EaScope(int i):id(i),t0(EA_Now()){} ~EaScope(){ EA_Add(id,t0);} };
#define EA_OPEN(id) { EaScope ea_scope_(id);
#define EA_CLOSE(id) }
#else
#define EA_OPEN(id) { long long ea_t0_ = EA_Now();
#define EA_CLOSE(id) EA_Add(id, ea_t0_); }
#endif
#endif
