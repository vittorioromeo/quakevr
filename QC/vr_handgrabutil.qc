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

float(entity xEntPlayer, float xHand) VR_HandGrabUtil_Impl_GetLastGrabTime =
{
    if(xHand == cVR_OffHand)  { return xEntPlayer.offhand_lastgrabtime; }
    if(xHand == cVR_MainHand) { return xEntPlayer.mainhand_lastgrabtime; }

    ASSERT(FALSE);
    return 0;
}

float(entity xEntPlayer, float xHand) VR_HandGrabUtil_Impl_GetLastReleaseTime =
{
    if(xHand == cVR_OffHand)  { return xEntPlayer.offhand_lastreleasetime; }
    if(xHand == cVR_MainHand) { return xEntPlayer.mainhand_lastreleasetime; }

    ASSERT(FALSE);
    return 0;
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

float(entity xEntPlayer, float xHand) VR_HandGrabUtil_IsHandGrabbing =
{
    return VRIsEntHandGrabbingBit(xEntPlayer, xHand);
}

float(entity xEntPlayer, float xHand) VR_HandGrabUtil_IsHandPrevGrabbing =
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

float(entity xEntPlayer, float xHand) VR_HandGrabUtil_IsHandReloadFlicking =
{
    return VRIsEntHandReloadFlickingBit(xEntPlayer, xHand);
}
