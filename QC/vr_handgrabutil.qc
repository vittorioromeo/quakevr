//
//
//
// ----------------------------------------------------------------------------
// Implementation details
// ----------------------------------------------------------------------------

void(entity xEntPlayer, float xHand, float xCurrBit, float xPrevBit,
     float xLastGrabTime, float* xOutLastGrabTime)
VR_HandGrabUtil_Impl_UpdateStateForHand =
{
    if(!xPrevBit && xCurrBit) // Rising edge
    {
        *xOutLastGrabTime = time;
    }
}

float(entity xEntPlayer, float xHand) VR_HandGrabUtil_Impl_GetLastGrabTime =
{
    if(xHand == cVR_OffHand)  { return xEntPlayer.offhand_lastgrabtime; }
    if(xHand == cVR_MainHand) { return xEntPlayer.mainhand_lastgrabtime; }

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
        &xEntPlayer.offhand_lastgrabtime
    );

    VR_HandGrabUtil_Impl_UpdateStateForHand(
        xEntPlayer,
        cVR_MainHand,
        VRGetMainHandGrabbingBit(xEntPlayer),
        VRGetMainHandPrevGrabbingBit(xEntPlayer),
        xEntPlayer.mainhand_lastgrabtime,
        &xEntPlayer.mainhand_lastgrabtime
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
    return time < VR_HandGrabUtil_Impl_GetLastGrabTime(xEntPlayer, xHand) + 0.125;
}

float(entity xEntPlayer, float xHand) VR_HandGrabUtil_StoppedHandGrabbing =
{
    return VRIsEntHandPrevGrabbingBit(xEntPlayer, xHand) &&
           !VRIsEntHandGrabbingBit(xEntPlayer, xHand);
}
