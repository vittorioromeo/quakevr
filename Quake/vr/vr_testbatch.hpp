// vr_testbatch.hpp -- many test cases in one game (vr_test_snapshot, vr_test_reset) and waits for a condition instead of
// a fixed count of frames (vr_waituntil, vr_waitprint). See vr_testbatch.cpp.
#pragma once

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

struct cvar_s;

namespace qvr::testbatch
{
void registerCommands(); // vr_test_snapshot, vr_test_reset, vr_waituntil, vr_waitprint

// The tests' one cvar snapshot (design review 7): cvars' values kept to be put back. vr_test_snapshot takes every cvar
// (takeAll: vr_test_reset also sets the cvars made since to their defaults); a scenario takes each setting it changes
// (take: the first value kept) and puts them back when it ends (vr_scenario.cpp).
class CvarSnapshot
{
public:
    struct Saved
    {
        cvar_s* var;
        za::String value;
    };

    void clear();
    void take(cvar_s* var); // its value now (once: a cvar already taken keeps its first value)
    void takeAll();         // every cvar's (the cvars made later: their defaults on restore)
    void restore() const;   // each taken cvar back (a settable one that changed); after takeAll the newer ones too

    [[nodiscard]] bool empty() const { return saved_.empty(); }
    [[nodiscard]] int size() const { return static_cast<int>(saved_.size()); }
    [[nodiscard]] const za::Vector<Saved>& saved() const { return saved_; }

private:
    za::Vector<Saved> saved_;
    ankerl::unordered_dense::set<const cvar_s*> known_;
    bool all_{false};
};
} // namespace qvr::testbatch

extern "C" void VR_TestConsolePrint(const char* text); // Con_Printf (the main thread's): vr_waitprint's text, if waiting
