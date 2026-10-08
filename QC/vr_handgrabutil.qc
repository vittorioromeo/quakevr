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

// Holding enemies (engine vr_foegrab.cpp; vr_foegrab.qc):
.float vr_foegrab_hands; // on a player, set by the engine: the hands holding an enemy (1 the off hand, 2 the main)
.float vr_foegrab_hold;  // on a monster, set by the engine: how firmly hands hold it now (0..1; 0 not held)
.float vr_foegrab_letgo; // on a monster, set here: every hand holding it lets go (the engine, at the frame's end)

// Whether `xHand` of `p` holds an enemy (it deals no melee blows).
float(entity p, float xHand) VR_FoeGrab_HandHolds =
{
    return (p.vr_foegrab_hands & (xHand == cVR_MainHand ? 2 : 1)) != 0;
};
