// Included from several files and listed in progs.src: a guard, as FTEQCC read it again despite a
// `#pragma once` (warning F316 for every macro, each time).
#ifndef VR_MACROS_QC
#define VR_MACROS_QC

// QVR_TESTS: the *_test.qc files (about 26k lines: every vr_*_test harness, the test impulses, the QC side of
// the Debug menu's test rows) are compiled in. On by default (dev builds: QC/build.sh, the kit, Visual Studio);
// the release flavour passes -DQVR_NO_TESTS to fteqcc (Misc/release/make_release.ps1, flatpak, CI) and leaves
// them out. Game code that calls into a test file wraps the call in #ifdef QVR_TESTS.
#ifndef QVR_NO_TESTS
#define QVR_TESTS
#endif

#define VRUTIL_POWER_OF_TWO(xExponent) (1 << xExponent)

#define NULLSTR 0
#define NULLENT world

#ifndef NDEBUG

#define ASSERT(x) \
    do { \
        if(!(x)) \
        { \
            bprint("Assertion failure!\n'" __FUNC__ "' at " __LINESTR__ "\nExpression is false: '" #x "'\n"); \
        } \
    } while(FALSE)

#else

#define ASSERT(x)

#endif

#define BPRINT_FVAR(x) \
    bprint(#x ": '"); \
    bprint(ftos(x)); \
    bprint("'\n")

#define BRPINT_VVAR(x) \
    bprint(#x ": '{"); \
    bprint(ftos(x ## _x)); \
    bprint("; "); \
    bprint(ftos(x ## _y)); \
    bprint("; "); \
    bprint(ftos(x ## _z)); \
    bprint("}'\n")

#endif
