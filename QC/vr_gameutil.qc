//
//
//
// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

float() VR_GameUtil_InTutorialMap =
{
    return (mapname == "vrtutorial");
}

float() VR_GameUtil_InStartMap =
{
    return (mapname == "start");
}

float() VR_GameUtil_InHoneyMap =
{
    return (mapname == "saint") ||
           (mapname == "honey") ||
           (mapname == "h_hub1") ||
           (mapname == "h_hub2") ||
           (mapname == "h_end") ||
           (mapname == "credits");
}

float() VR_GameUtil_EnableHoneyExtensions =
{
    // TODO VR: (P2) also add cvar override

    if(VR_GameUtil_InStartMap() || VR_GameUtil_InHoneyMap())
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}
