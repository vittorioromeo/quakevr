// THROWAWAY audit impl (included at the end of vr_findradius.cpp)
#include <windows.h>
#include <stdio.h>
static constexpr int eaMax = 160;
struct EaRec { long long calls = 0, ticks = 0; };
static EaRec eaRec[eaMax];
struct EaQc { int id = -1; const void* fn = nullptr; long long calls = 0, ticks = 0; };
static EaQc eaQc[4096];
static int eaFrame0 = 0;
static long long eaFreq = 0;
extern "C" long long EA_Now(void) { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
extern "C" void EA_Add(int id, long long t0)
{
    const long long dt = EA_Now() - t0;
    eaRec[id].calls++; eaRec[id].ticks += dt;
    if(qcvm && qcvm->xfunction && (ea_names[id][0] == 'p' || strstr(ea_names[id], "builtins")))
    {
        const void* fn = qcvm->xfunction; unsigned h = (unsigned)(((size_t)fn >> 4) * 31 + id) & 4095;
        for(int n = 0; n < 4096; n++, h = (h + 1) & 4095)
        {
            EaQc& q = eaQc[h];
            if(q.id < 0 && q.calls == 0) { q.id = id; q.fn = fn; }
            if(q.id == id && q.fn == fn) { q.calls++; q.ticks += dt; break; }
        }
    }
}
static void ea_reset_f()
{
    for(auto& r : eaRec) r = EaRec();
    for(auto& q : eaQc) q = EaQc();
    eaFrame0 = host_framecount;
}
static void ea_fields_f();
static void ea_stats_f()
{
    QueryPerformanceFrequency((LARGE_INTEGER*)&eaFreq);
    const int fr = host_framecount - eaFrame0 > 0 ? host_framecount - eaFrame0 : 1;
    const int n = (int)(sizeof(ea_names) / sizeof(ea_names[0]));
    Con_Printf("EDICT sizeof(edict_t) %d sizeof(entvars_t) %d entityfields*4 %d edict_size %d header(before v) %d\n",
        (int)sizeof(edict_t), (int)sizeof(entvars_t), sv.qcvm.progs->entityfields * 4, sv.qcvm.edict_size, (int)offsetof(edict_t, v));
    Con_Printf("EDICT off: free %d freechain %d area %d areanode %d num_leafs %d leafnums %d baseline %d(sz %d) alpha %d forcewater %d oldframe %d showbbox %d freetime %d v %d\n",
        (int)offsetof(edict_t, free), (int)offsetof(edict_t, freechain), (int)offsetof(edict_t, area), (int)offsetof(edict_t, areanode),
        (int)offsetof(edict_t, num_leafs), (int)offsetof(edict_t, leafnums), (int)offsetof(edict_t, baseline), (int)sizeof(entity_state_t),
        (int)offsetof(edict_t, alpha), (int)offsetof(edict_t, forcewater), (int)offsetof(edict_t, oldframe), (int)offsetof(edict_t, showbboxframe),
        (int)offsetof(edict_t, freetime), (int)offsetof(edict_t, v));
    Con_Printf("EDICT off: origin %d mins %d maxs %d absmin %d absmax %d solid %d flags %d movetype %d health %d takedamage %d nextthink %d think %d classname %d owner %d velocity %d\n",
        (int)offsetof(edict_t, v.origin), (int)offsetof(edict_t, v.mins), (int)offsetof(edict_t, v.maxs), (int)offsetof(edict_t, v.absmin), (int)offsetof(edict_t, v.absmax),
        (int)offsetof(edict_t, v.solid), (int)offsetof(edict_t, v.flags), (int)offsetof(edict_t, v.movetype), (int)offsetof(edict_t, v.health), (int)offsetof(edict_t, v.takedamage),
        (int)offsetof(edict_t, v.nextthink), (int)offsetof(edict_t, v.think), (int)offsetof(edict_t, v.classname), (int)offsetof(edict_t, v.owner), (int)offsetof(edict_t, v.velocity));
    Con_Printf("EA frames %d edicts %d\n", fr, sv.qcvm.num_edicts);
    for(int i = 0; i < n; i++)
        if(eaRec[i].calls)
            Con_Printf("EA %-34s calls/f %7.2f us/f %8.2f us/call %7.2f\n", ea_names[i], (double)eaRec[i].calls / fr,
                1e6 * eaRec[i].ticks / eaFreq / fr, 1e6 * eaRec[i].ticks / eaFreq / eaRec[i].calls);
    for(auto& q : eaQc)
        if(q.calls)
            Con_Printf("EAQ %-30s <- %-28s calls/f %7.2f us/f %8.2f\n", ea_names[q.id],
                (sv.qcvm.strings + ((dfunction_t*)q.fn)->s_name), (double)q.calls / fr, 1e6 * q.ticks / eaFreq / fr);
    Con_Printf("EA_DONE\n");
}
