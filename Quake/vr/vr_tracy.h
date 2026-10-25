// Optional external profiler. Literal names must remain valid for the process lifetime.
#ifndef QVR_VR_TRACY_H
#define QVR_VR_TRACY_H
#ifdef __cplusplus
extern "C" {
#endif
#ifdef QVR_TRACY
void VR_TracyInit(void);
void VR_TracyBegin(const char *name, int gpu);
void VR_TracyEnd(void);
void VR_TracyFrame(void);
void VR_TracyAbort(void);
void VR_TracyGLShutdown(void);
void VR_TracyThreadName(const char *name);
void VR_TracyText(const char *text); // copied annotation of the innermost zone; dynamic strings allowed
void VR_TracyBeginAt(const char *name, const char *file, unsigned line);
struct vr_tracy_loc {const char *name; const char *function; const char *file; unsigned line; unsigned color;}; // layout of ___tracy_source_location_data
void VR_TracyBeginLoc(const struct vr_tracy_loc *loc); // static location, no per-call allocation
void VR_TracyQCProgs(void); // the running qcvm loaded progs: build its per-function source locations
void VR_TracyQC(const void *function); // a dfunction_t of the running qcvm (function or builtin) is entered
void VR_TracyQCEnd(void);
void VR_TracyDisplay(double ms, int missed);
void VR_TracyPlot(const char *name, double value);
void VR_TracyMessage(const char *text, unsigned length); // copied; on the timeline (vr_xr_stall.cpp's held frames)
int VR_TracyConnected(void);
void VR_TracyAlloc(const void *ptr, unsigned long long bytes, int pool, int end);
void VR_TracyFree(const void *ptr);
void VR_TracyHunkReset(int mark);
int VR_TracyCheckPool(int pool, unsigned long long count, unsigned long long bytes);
void VR_TracyBoxAlloc(const void *ptr, unsigned long long bytes);
void VR_TracyBoxFree(const void *ptr);
void VR_TracyAllocationWindow(int enter);
extern int vr_tracy_physics_zones; // vr_tracy_physics_detail, latched at each frame: SV_Physics's per-edict zones
extern int vr_tracy_render_zones; // vr_tracy_render_detail, latched at each frame: per-entity render steps (r_alias.c)
// vr_tracy_gpu_detail with the GPU context live and connected, latched at each frame: the draws split into finer GPU
// zones where one call drew several passes (the world and the brush entities, the monsters and the player's models)
extern int vr_tracy_gpu_zones;
// The innermost open zone's name on this thread (null when none or not connected): a job's poster (vr_jobs.cpp).
const char *VR_TracyCurrentName(void);
// A job's execute zone named by where it was posted (vr_jobs.cpp): "<posting zone> @ <file>:<line>".
void VR_TracyJobText(const char *from, const char *file, unsigned line);
#else
static inline void VR_TracyInit(void) {}
static inline void VR_TracyBegin(const char *name, int gpu) {(void)name; (void)gpu;}
static inline void VR_TracyEnd(void) {}
static inline void VR_TracyFrame(void) {}
static inline void VR_TracyAbort(void) {}
static inline void VR_TracyGLShutdown(void) {}
static inline void VR_TracyThreadName(const char *name) {(void)name;}
static inline void VR_TracyText(const char *text) {(void)text;}
static inline void VR_TracyBeginAt(const char *name, const char *file, unsigned line) {(void)name; (void)file; (void)line;}
static inline void VR_TracyQCProgs(void) {}
static inline void VR_TracyQC(const void *function) {(void)function;}
static inline void VR_TracyQCEnd(void) {}
static inline void VR_TracyDisplay(double ms, int missed) {(void)ms; (void)missed;}
static inline void VR_TracyPlot(const char *name, double value) {(void)name; (void)value;}
static inline void VR_TracyMessage(const char *text, unsigned length) {(void)text; (void)length;}
static inline int VR_TracyConnected(void) {return 0;}
static inline void VR_TracyAlloc(const void *ptr, unsigned long long bytes, int pool, int end) {(void)ptr; (void)bytes; (void)pool; (void)end;}
static inline void VR_TracyFree(const void *ptr) {(void)ptr;}
static inline void VR_TracyHunkReset(int mark) {(void)mark;}
static inline int VR_TracyCheckPool(int pool, unsigned long long count, unsigned long long bytes) {(void)pool; (void)count; (void)bytes; return 1;}
static inline void VR_TracyBoxAlloc(const void *ptr, unsigned long long bytes) {(void)ptr; (void)bytes;}
static inline void VR_TracyBoxFree(const void *ptr) {(void)ptr;}
static inline void VR_TracyAllocationWindow(int enter) {(void)enter;}
#endif
#ifndef QVR_TRACY
// Disabled hooks must not evaluate dynamic names, resource expressions or conversions, even in Debug.
// Retain the inline declarations for type compatibility; direct calls disappear at preprocessing time.
#define VR_TracyBegin(name, gpu) ((void)0)
#define VR_TracyBeginAt(name, file, line) ((void)0)
#define VR_TracyThreadName(name) ((void)0)
#define VR_TracyText(text) ((void)0)
#define VR_TracyQCProgs() ((void)0)
#define VR_TracyQC(function) ((void)0)
#define VR_TracyQCEnd() ((void)0)
#define VR_TracyDisplay(ms, missed) ((void)0)
#define VR_TracyPlot(name, value) ((void)0)
#define VR_TracyMessage(text, length) ((void)0)
#define VR_TracyAlloc(ptr, bytes, pool, end) ((void)0)
#define VR_TracyFree(ptr) ((void)0)
#define VR_TracyHunkReset(mark) ((void)0)
#define VR_TracyCheckPool(pool, count, bytes) 1
#define VR_TracyBoxAlloc(ptr, bytes) ((void)0)
#define VR_TracyBoxFree(ptr) ((void)0)
#define VR_TracyAllocationWindow(enter) ((void)0)
#define vr_tracy_render_zones 0
#define vr_tracy_gpu_zones 0
#endif
#ifdef QVR_TRACY
// clang C cleanup handles all ordinary returns; Host_Error uses VR_TracyAbort.
static inline void VR_TracyCleanup(int *unused) {(void)unused; VR_TracyEnd();}
#define VR_TRACY_SCOPE(name) static const struct vr_tracy_loc vr_tracy_loc_ = {name, "QVR", __FILE__, __LINE__, 0}; \
    int vr_tracy_scope __attribute__((cleanup(VR_TracyCleanup))) = (VR_TracyBeginLoc(&vr_tracy_loc_), 0)
// A detail-gated zone around one statement: a static location, opened only while flag (a latched int) is set.
#define VR_TRACY_DETAIL(flag, name, call) do { if (flag) { VR_TRACY_SCOPE (name); call; } else { call; } } while (0)
#else
#define VR_TRACY_SCOPE(name) ((void)0)
#define VR_TRACY_DETAIL(flag, name, call) do { call; } while (0)
#endif
#ifdef __cplusplus
}
namespace qvr::tracyprof
{
class Scope
{
public:
#ifdef QVR_TRACY
    explicit Scope(const vr_tracy_loc *loc) { VR_TracyBeginLoc(loc); }
#else
    explicit Scope(const char *name) {(void)name;}
#endif
    ~Scope() { VR_TracyEnd(); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
}
#define QVR_TRACY_CAT2(a, b) a##b
#define QVR_TRACY_CAT(a, b) QVR_TRACY_CAT2(a, b)
#ifdef QVR_TRACY
#define QVR_TRACY_SCOPE(var, name) \
    static constexpr vr_tracy_loc QVR_TRACY_CAT(var, _loc){name, "QVR", __FILE__, __LINE__, 0}; \
    qvr::tracyprof::Scope var{&QVR_TRACY_CAT(var, _loc)}
// One statement in a zone of its own (profiling builds; others: the statement alone).
#define QVR_TRACY_CALL(name, call) do { QVR_TRACY_SCOPE(qvrTracyCall, name); call; } while(0)
#else
#define QVR_TRACY_SCOPE(var, name) qvr::tracyprof::Scope var{name}
#define QVR_TRACY_CALL(name, call) do { call; } while(0)
#endif
#endif
#endif
