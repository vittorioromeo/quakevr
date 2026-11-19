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

// A Quake VR setting's kind in multiplayer: its vr_cvars.inc row's QVR_RULE, QVR_PREF or QVR_OWN (vr_cvars.hpp); Unset
// for a row with none and for every cvar that is not Quake VR's.
enum class Kind
{
    Unset,
    Rule,
    Pref,
    Own,
};
[[nodiscard]] Kind kindOf(const cvar_t* var);
[[nodiscard]] const char* kindName(Kind kind); // "unset", "rule", "pref", "own"

// fn(cvar_t*, Kind) for every vr_cvars.inc setting whose row says its kind, in the table's order.
template <typename F>
void forEachWithKind(F&& fn);

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

namespace detail
{
struct KindRow
{
    cvar_t* var;
    Kind kind;
};
// The vr_cvars.inc rows with a kind (vr_cvars.cpp).
[[nodiscard]] const KindRow* kindRows(int& count);
} // namespace detail

template <typename F>
void forEachWithKind(F&& fn)
{
    int count = 0;
    const detail::KindRow* rows = detail::kindRows(count);
    for(int i = 0; i < count; i++)
    {
        fn(rows[i].var, rows[i].kind);
    }
}

} // namespace qvr::cvars
