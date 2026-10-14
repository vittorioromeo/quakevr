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
int VR_TracyQCEnabled(void);
void VR_TracyQC(const char *name, const char *file, int builtin);
void VR_TracyQCEnd(void);
void VR_TracyDisplay(double ms, int missed);
void VR_TracyPlot(const char *name, double value);
int VR_TracyConnected(void);
void VR_TracyAlloc(const void *ptr, unsigned long long bytes, int pool, int end);
void VR_TracyFree(const void *ptr);
void VR_TracyHunkReset(int mark);
int VR_TracyCheckPool(int pool, unsigned long long count, unsigned long long bytes);
void VR_TracyBoxAlloc(const void *ptr, unsigned long long bytes);
void VR_TracyBoxFree(const void *ptr);
void VR_TracyAllocationWindow(int enter);
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
static inline int VR_TracyQCEnabled(void) {return 0;}
static inline void VR_TracyQC(const char *name, const char *file, int builtin) {(void)name; (void)file; (void)builtin;}
static inline void VR_TracyQCEnd(void) {}
static inline void VR_TracyDisplay(double ms, int missed) {(void)ms; (void)missed;}
static inline void VR_TracyPlot(const char *name, double value) {(void)name; (void)value;}
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
#define VR_TracyQC(name, file, builtin) ((void)0)
#define VR_TracyQCEnd() ((void)0)
#define VR_TracyDisplay(ms, missed) ((void)0)
#define VR_TracyPlot(name, value) ((void)0)
#define VR_TracyAlloc(ptr, bytes, pool, end) ((void)0)
#define VR_TracyFree(ptr) ((void)0)
#define VR_TracyHunkReset(mark) ((void)0)
#define VR_TracyCheckPool(pool, count, bytes) 1
#define VR_TracyBoxAlloc(ptr, bytes) ((void)0)
#define VR_TracyBoxFree(ptr) ((void)0)
#define VR_TracyAllocationWindow(enter) ((void)0)
#endif
#ifdef QVR_TRACY
// clang C cleanup handles all ordinary returns; Host_Error uses VR_TracyAbort.
static inline void VR_TracyCleanup(int *unused) {(void)unused; VR_TracyEnd();}
#define VR_TRACY_SCOPE(name) int vr_tracy_scope __attribute__((cleanup(VR_TracyCleanup))) = (VR_TracyBeginAt(name, __FILE__, __LINE__), 0)
#else
#define VR_TRACY_SCOPE(name) ((void)0)
#endif
#ifdef __cplusplus
}
#ifdef QVR_TRACY
#include <source_location>
#endif
namespace qvr::tracyprof
{
class Scope
{
public:
#ifdef QVR_TRACY
    explicit Scope(const char *name, std::source_location location = std::source_location::current())
    { VR_TracyBeginAt(name, location.file_name(), location.line()); }
#else
    explicit Scope(const char *name) {(void)name;}
#endif
    ~Scope() { VR_TracyEnd(); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
}
#endif
#endif
