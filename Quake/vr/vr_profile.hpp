// vr_profile.hpp -- a small scope profiler (docs/vr-port/TESTING.md, "Profiling").
//
// vr_profile 1 times the named scopes of every frame, on the CPU (steady_clock) and, for GPU
// scopes, on the GPU (OpenGL timestamp queries, read back a few frames later so the CPU never
// waits for them). Scopes nest: the report is a call tree (the eyes' scopes apart), with each
// scope's average and worst per-frame time over the interval, written every vr_profile_interval
// seconds (or on vr_profile_dump) to <gamedir>/profile/profile_<map>_<date>_<time>.csv, with a
// summary in the console. vr_profile 2 also shows the costliest scopes over the wrist gadget.
// With vr_profile 0 a scope costs a test of one flag.

#pragma once

#include "vr_profile.h"

namespace qvr::profile
{

// Collecting this frame (vr_profile, latched at the frame's start).
extern bool active;

void begin(const char* name, bool gpu);
void end();

void init();    // commands (VR_Init)
void overlay(); // vr_profile 2: queues the costliest scopes as world text (after text3d::clear)

class Scope
{
public:
    Scope(const char* name, bool gpu) : on_{active}
    {
        if(on_)
        {
            begin(name, gpu);
        }
    }

    ~Scope()
    {
        if(on_)
        {
            end();
        }
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    bool on_;
};

} // namespace qvr::profile

#define QVR_PROFILE_JOIN2(a, b) a##b
#define QVR_PROFILE_JOIN(a, b) QVR_PROFILE_JOIN2(a, b)

// Times the rest of the enclosing block (name: a string literal): on the CPU, or on both.
#define QVR_PROFILE(name) const ::qvr::profile::Scope QVR_PROFILE_JOIN(qvrProfileScope, __LINE__){name, false}
#define QVR_GPU_PROFILE(name) const ::qvr::profile::Scope QVR_PROFILE_JOIN(qvrProfileScope, __LINE__){name, true}
