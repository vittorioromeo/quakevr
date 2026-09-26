//
//
//
// ----------------------------------------------------------------------------
// Implementation details
// ----------------------------------------------------------------------------

// Records when the grip was last pressed (rising edge) and let go (falling edge).
void(float xCurrBit, float xPrevBit, float* xOutLastGrabTime, float* xOutLastReleaseTime)
VR_HandGrabUtil_Impl_UpdateStateForHand =
{
    if(!xPrevBit && xCurrBit) // Rising edge (grab)
    {
        *xOutLastGrabTime = time;
    }

    if(xPrevBit && !xCurrBit) // Falling edge (release)
    {
        *xOutLastReleaseTime = time;
    }
}

inline float(entity xEntPlayer, float xHand) VR_HandGrabUtil_Impl_GetLastGrabTime =
{
    return VR_IMPL_SELECT(xHand, xEntPlayer.offhand_lastgrabtime, xEntPlayer.mainhand_lastgrabtime);
}

inline float(entity xEntPlayer, float xHand) VR_HandGrabUtil_Impl_GetLastReleaseTime =
{
    return VR_IMPL_SELECT(xHand, xEntPlayer.offhand_lastreleasetime, xEntPlayer.mainhand_lastreleasetime);
}

//
//
//
// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void(entity xEntPlayer) VR_HandGrabUtil_UpdateState =
{
    if(!xEntPlayer.ishuman)
    {
        return;
    }

    VR_HandGrabUtil_Impl_UpdateStateForHand(
        VRGetOffHandGrabbingBit(xEntPlayer),
        VRGetOffHandPrevGrabbingBit(xEntPlayer),
        &xEntPlayer.offhand_lastgrabtime,
        &xEntPlayer.offhand_lastreleasetime
    );

    VR_HandGrabUtil_Impl_UpdateStateForHand(
        VRGetMainHandGrabbingBit(xEntPlayer),
        VRGetMainHandPrevGrabbingBit(xEntPlayer),
        &xEntPlayer.mainhand_lastgrabtime,
        &xEntPlayer.mainhand_lastreleasetime
    );
}

inline float(entity xEntPlayer, float xHand) VR_HandGrabUtil_IsHandGrabbing =
{
    return VRIsEntHandGrabbingBit(xEntPlayer, xHand);
}

inline float(entity xEntPlayer, float xHand) VR_HandGrabUtil_IsHandPrevGrabbing =
{
    return VRIsEntHandPrevGrabbingBit(xEntPlayer, xHand);
}

float(entity xEntPlayer, float xHand) VR_HandGrabUtil_StartedHandGrabbing =
{
    float lastGrabTime = VR_HandGrabUtil_Impl_GetLastGrabTime(xEntPlayer, xHand);
    float lastReleaseTime = VR_HandGrabUtil_Impl_GetLastReleaseTime(xEntPlayer, xHand);

    if(lastReleaseTime + 0.125 > lastGrabTime)
    {
        return FALSE;
    }

    return time < lastGrabTime + 0.125
        && time > lastReleaseTime;
}

inline float(entity xEntPlayer, float xHand) VR_HandGrabUtil_IsHandReloadFlicking =
{
    return VRIsEntHandReloadFlickingBit(xEntPlayer, xHand);
}
