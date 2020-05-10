//
//
//
// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

float() VR_GameUtil_InVanillaMap =
{
    return (mapname == "orig_start") ||
           (mapname == "e1m1") ||
           (mapname == "e1m2") ||
           (mapname == "e1m3") ||
           (mapname == "e1m4") ||
           (mapname == "e1m5") ||
           (mapname == "e1m6") ||
           (mapname == "e1m7") ||
           (mapname == "e1m8") ||
           (mapname == "e2m1") ||
           (mapname == "e2m2") ||
           (mapname == "e2m3") ||
           (mapname == "e2m4") ||
           (mapname == "e2m5") ||
           (mapname == "e2m6") ||
           (mapname == "e2m7") ||
           (mapname == "e3m1") ||
           (mapname == "e3m2") ||
           (mapname == "e3m3") ||
           (mapname == "e3m4") ||
           (mapname == "e3m5") ||
           (mapname == "e3m6") ||
           (mapname == "e3m7") ||
           (mapname == "e4m1") ||
           (mapname == "e4m2") ||
           (mapname == "e4m3") ||
           (mapname == "e4m4") ||
           (mapname == "e4m5") ||
           (mapname == "e4m6") ||
           (mapname == "e4m7") ||
           (mapname == "e4m8") ||
           (mapname == "end");
}

float() VR_GameUtil_InHipnoticMap =
{
    return (mapname == "hip1m1") ||
           (mapname == "hip1m2") ||
           (mapname == "hip1m3") ||
           (mapname == "hip1m4") ||
           (mapname == "hip1m5") ||
           (mapname == "hip2m1") ||
           (mapname == "hip2m2") ||
           (mapname == "hip2m3") ||
           (mapname == "hip2m4") ||
           (mapname == "hip2m5") ||
           (mapname == "hip2m6") ||
           (mapname == "hip3m1") ||
           (mapname == "hip3m2") ||
           (mapname == "hip3m3") ||
           (mapname == "hip3m4") ||
           (mapname == "hipdm1") ||
           (mapname == "hipend");
}

float() VR_GameUtil_InRogueMap =
{
return (mapname == "r1m1") ||
       (mapname == "r1m2") ||
       (mapname == "r1m3") ||
       (mapname == "r1m4") ||
       (mapname == "r1m5") ||
       (mapname == "r1m6") ||
       (mapname == "r1m7") ||
       (mapname == "r2m1") ||
       (mapname == "r2m2") ||
       (mapname == "r2m3") ||
       (mapname == "r2m4") ||
       (mapname == "r2m5") ||
       (mapname == "r2m6") ||
       (mapname == "r2m7") ||
       (mapname == "r2m8") ||
       (mapname == "ctf1");
}

float() VR_GameUtil_InDopaMap =
{
    return (mapname == "e5m1") ||
           (mapname == "e5m2") ||
           (mapname == "e5m3") ||
           (mapname == "e5m4") ||
           (mapname == "e5m5") ||
           (mapname == "e5m6") ||
           (mapname == "e5m7") ||
           (mapname == "e5m8") ||
           (mapname == "e5end") ||
           (mapname == "e5dm");
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

float() VR_GameUtil_InUnknownMap =
{
    return !VR_GameUtil_InVanillaMap() &&
           !VR_GameUtil_InHipnoticMap() &&
           !VR_GameUtil_InRogueMap() &&
           !VR_GameUtil_InDopaMap() &&
           !VR_GameUtil_InHoneyMap();
}
