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
#else
static inline void VR_TracyInit(void) {}
static inline void VR_TracyBegin(const char *name, int gpu) {(void)name; (void)gpu;}
static inline void VR_TracyEnd(void) {}
static inline void VR_TracyFrame(void) {}
static inline void VR_TracyAbort(void) {}
static inline void VR_TracyGLShutdown(void) {}
static inline void VR_TracyThreadName(const char *name) {(void)name;}
static inline void VR_TracyText(const char *text) {(void)text;}
#endif
#ifdef __cplusplus
}
namespace qvr::tracyprof
{
class Scope
{
public:
    explicit Scope(const char *name) { VR_TracyBegin(name, 0); }
    ~Scope() { VR_TracyEnd(); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
}
#endif
#endif
