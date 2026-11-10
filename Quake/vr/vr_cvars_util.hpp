// vr_cvars_util.hpp -- cvar helpers that need no cvar by name: every cvar in turn, a cvar set for a scope, cvars kept
// and put back (vr_cvars.cpp). The cvars by name: vr_cvars.hpp.

#pragma once

extern "C" {
#include "q_stdinc.h"
#include "cvar.h"
}

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

namespace qvr::cvars
{

// fn(cvar_t*) for every cvar the engine has, in its list's order (by name).
template <typename F>
void forEach(F&& fn)
{
    for(cvar_t* var = Cvar_FindVarAfter("", 0); var; var = var->next)
    {
        fn(var);
    }
}

// A cvar set for a scope (RAII): `value` now, its string before put back when the Override goes (a test's setting).
class Override
{
public:
    Override(cvar_t& var, float value);
    Override(cvar_t& var, const char* value);
    ~Override();
    Override(const Override&) = delete;
    Override& operator=(const Override&) = delete;

private:
    cvar_t& var;
    za::String was;
};

// Cvars' values kept before a run changes them (a playback, a scenario) and put back by restore(): the latest kept
// first, so a cvar kept twice ends at its first kept value. Not on destruction (a file's own one lives to the exit).
class Snapshot
{
public:
    void keep(cvar_t& var);
    void restore();
    [[nodiscard]] bool empty() const { return saved.empty(); }
    [[nodiscard]] int size() const { return static_cast<int>(saved.size()); }

private:
    struct Saved
    {
        cvar_t* var;
        za::String value;
    };
    za::Vector<Saved> saved;
};

} // namespace qvr::cvars
