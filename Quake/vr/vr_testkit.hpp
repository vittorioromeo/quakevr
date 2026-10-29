#pragma once

// vr_testkit.hpp -- the C++ self-tests' pass/fail counting and their one verdict line. run.sh's -QuitOn and
// run_baseline.py match "<name>: N passed, M failed", so the format lives here and nowhere else.

#include "vr_engine.hpp"

namespace qvr::testkit
{

// The verdict line: "<name>: N passed, M failed".
inline void verdict(const char* name, const int passed, const int failed)
{
    Con_Printf("%s: %d passed, %d failed\n", name, passed, failed);
}

// Counts checks; a failed one prints "<name>: FAIL <what>" (name: the test's prefix) when `what` is given.
struct Tally
{
    int passed{0};
    int failed{0};

    bool check(const bool ok, const char* name = nullptr, const char* what = nullptr)
    {
        (ok ? passed : failed)++;
        if(!ok && name && what)
        {
            Con_Printf("%s: FAIL %s\n", name, what);
        }
        return ok;
    }
    void reset() { *this = Tally{}; }
    void verdict(const char* name) const { testkit::verdict(name, passed, failed); }
};

} // namespace qvr::testkit
