//
//
//
// ----------------------------------------------------------------------------
// Implementation details
// ----------------------------------------------------------------------------

void(entity xEntPlayer,
     float xHand,
     float xCurrBit,
     float xPrevBit,
     float xLastGrabTime,
     float* xOutLastGrabTime,
     float xLastReleaseTime,
     float* xOutLastReleaseTime,
     float xCurrReloadFlickingBit,
     float xPrevReloadFlickingBit)
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
        xEntPlayer,
        cVR_OffHand,
        VRGetOffHandGrabbingBit(xEntPlayer),
        VRGetOffHandPrevGrabbingBit(xEntPlayer),
        xEntPlayer.offhand_lastgrabtime,
        &xEntPlayer.offhand_lastgrabtime,
        xEntPlayer.offhand_lastreleasetime,
        &xEntPlayer.offhand_lastreleasetime,
        VRGetOffHandReloadFlickingBit(xEntPlayer),
        VRGetOffHandPrevReloadFlickingBit(xEntPlayer)
    );

    VR_HandGrabUtil_Impl_UpdateStateForHand(
        xEntPlayer,
        cVR_MainHand,
        VRGetMainHandGrabbingBit(xEntPlayer),
        VRGetMainHandPrevGrabbingBit(xEntPlayer),
        xEntPlayer.mainhand_lastgrabtime,
        &xEntPlayer.mainhand_lastgrabtime,
        xEntPlayer.mainhand_lastreleasetime,
        &xEntPlayer.mainhand_lastreleasetime,
        VRGetMainHandReloadFlickingBit(xEntPlayer),
        VRGetMainHandPrevReloadFlickingBit(xEntPlayer)
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

float(entity xEntPlayer, float xHand) VR_HandGrabUtil_StoppedHandGrabbing =
{
    return VRIsEntHandPrevGrabbingBit(xEntPlayer, xHand) &&
           !VRIsEntHandGrabbingBit(xEntPlayer, xHand);
}

float(entity xEntPlayer, float xHand) VR_HandGrabUtil_IsHandReloadFlicking =
{
    return VRIsEntHandReloadFlickingBit(xEntPlayer, xHand);
}

float(entity xEntPlayer, float xHand) VR_HandGrabUtil_IsHandPrevReloadFlicking =
{
    return VRIsEntHandPrevReloadFlickingBit(xEntPlayer, xHand);
}
