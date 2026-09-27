// vr_motion_play.cpp -- the motion recorder's playback (vr_motion.hpp): not yet; the recorder's hooks.

#include "vr_motion.hpp"
#include "vr_motion_take.hpp"

#include "vr_engine.hpp"

namespace qvr::motion
{

void initPlayback()
{
}

void playAfterTracking(TrackingState& /* tracking */, FrameState& /* frame */)
{
}

void playServerSample(edict_t*& /* target */)
{
}

void playFrameEnd(std::vector<Event>& /* events */, bool /* tick */, double /* svDt */)
{
}

bool playWantsSamples()
{
    return false;
}

bool playing()
{
    return false;
}

double hostFrameTime(double time)
{
    return time;
}

int serverFrameOverride(double& /* frametime */)
{
    return -1;
}

} // namespace qvr::motion
