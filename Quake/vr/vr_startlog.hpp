#pragma once

// vr_startlog.hpp -- the start-up trace (vr_startlog.cpp): vr_startup.cpp's timing hooks feed it.

namespace qvr::startlog
{

void start(double processStart);                // VR_TimeStart: the trace begins (the watchdog thread too)
void init();                                    // VR_TimeInit: the commands
void mark(const char* stage, double now);       // VR_TimeMark: a stage ended (ends the step of its name, if open)
void loadBegin(const char* what, double now);   // VR_TimeLoadBegin: a map's load
void loadFrame(int frames, int signon);         // VR_TimeFrameEnd: a load's frame before its signon
void loadEnd(double now, bool failed, const char* work); // the load's first frame drawn, or the load failed
void startupEnd(double now, const char* work);  // the start-up's first frame drawn; `work`: its VR_TimeAdd sums, a line
void frameEnd();                                // the end of a frame: steps left open (an error's longjmp) closed
void glStep(const char* text);                  // VR_GLStep: the step's progress (the heartbeat's "last GL step")
void quit();                                    // the quit: the watchdog stops

} // namespace qvr::startlog

// The whole-load panel's stages (vr_shaderload.cpp, flatloadbar), from the same hooks whether or not the trace runs: a
// load begins (its command's start), a step begins or ends, a stage ends (VR_TimeMark), the load ends (its first frame
// drawn, or it failed). Main thread only.
namespace qvr::loadbar
{

void begin(double start);
void stepBegin(const char* step);
void stepEnd();
void mark(const char* stage);
void end();

} // namespace qvr::loadbar
