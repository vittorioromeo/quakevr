// vr_testbatch.hpp -- many test cases in one game (vr_test_snapshot, vr_test_reset) and waits for a condition instead of
// a fixed count of frames (vr_waituntil, vr_waitprint). See vr_testbatch.cpp.
#pragma once

namespace qvr::testbatch
{
void registerCommands(); // vr_test_snapshot, vr_test_reset, vr_waituntil, vr_waitprint
}

extern "C" void VR_TestConsolePrint(const char* text); // Con_Printf (the main thread's): vr_waitprint's text, if waiting
