// vr_grab.qc -- the grab query's QC stage (design review 5 (c); the engine's half and the whole order: Quake/vr/vr_grab.hpp).
// What an empty hand's grip takes in W_Frame, asked of one query: each search is a provider (VR_*_GrabCandidate: TRUE when
// it would take, the thing in vr_grab_ent, how far in vr_grab_dist, units), tried in the order of precedence, the first
// that would take answers. The kinds are grab::Kind's numbers (smaller takes the grip first); the stages before this one
// (the client's, climbing, the foe grab, the hands' touches) have taken or hidden the grip by now.

float VR_GRAB_NONE = 0;
float VR_GRAB_ZONE_DRAG = 1;     // (the client's)
float VR_GRAB_CHAINSAW_CORD = 2; // (the client's)
float VR_GRAB_LAMP = 3;          // (the client's)
float VR_GRAB_CLIMB = 4;         // (the engine's, before the move)
float VR_GRAB_FOE = 5;           // (the engine's, before the move)
float VR_GRAB_RAGDOLL = 6;       // (the hands' touches: physics::handOn)
float VR_GRAB_PROP = 7;
float VR_GRAB_ROUND = 8;
float VR_GRAB_ARMED_WEAPON = 9;
float VR_GRAB_WEAPON = 10;
float VR_GRAB_BOX = 11;
float VR_GRAB_SECOND_HAND = 12;  // W_Frame: VR_Carry_TrySecondHand
float VR_GRAB_TORCH = 13;        // W_Frame: VR_WallTorch_GrabCandidate
float VR_GRAB_GRENADE_POUCH = 14; // W_Frame: VR_HandGrenade_GrabCandidate
float VR_GRAB_AMMO_POUCH = 15;   // W_Frame: VR_Reload_GrabCandidate
float VR_GRAB_HOLSTER = 16;      // W_Frame: DoHandImpl (a holster's draw, the carried gun's grip)
float VR_GRAB_FORCE_CATCH = 17;  // a force-grabbed thing's own think: VR_Forcegrab_FlightThink

entity vr_grab_ent;  // the answer's thing (world: a pouch's, made as it is taken)
float vr_grab_dist;  // how far it was from the hand (units; 0: not measured)

// The providers (self is the player), side-effect free.
float(float xHand) VR_WallTorch_GrabCandidate;
float(float xHand) VR_HandGrenade_GrabCandidate;
float(float xHand) VR_Reload_GrabCandidate;

// What `self`'s hand `xHand` would take now in W_Frame (VR_GRAB_*; vr_grab_ent, vr_grab_dist), VR_GRAB_NONE for nothing.
// Asked by a kind's site: the providers after `xUpTo` are not asked (they cannot take the grip from it).
float(float xHand, float xUpTo) VR_Grab_Query =
{
    vr_grab_ent = world;
    vr_grab_dist = 0;
    if(xUpTo >= VR_GRAB_TORCH && VR_WallTorch_GrabCandidate(xHand))
    {
        return VR_GRAB_TORCH;
    }
    if(xUpTo >= VR_GRAB_GRENADE_POUCH && VR_HandGrenade_GrabCandidate(xHand))
    {
        return VR_GRAB_GRENADE_POUCH;
    }
    if(xUpTo >= VR_GRAB_AMMO_POUCH && VR_Reload_GrabCandidate(xHand))
    {
        return VR_GRAB_AMMO_POUCH;
    }
    return VR_GRAB_NONE;
};

string(float xKind) VR_Grab_Name =
{
    if(xKind == VR_GRAB_TORCH)
    {
        return "torch";
    }
    if(xKind == VR_GRAB_GRENADE_POUCH)
    {
        return "grenade pouch";
    }
    if(xKind == VR_GRAB_AMMO_POUCH)
    {
        return "ammo pouch";
    }
    return xKind == VR_GRAB_NONE ? "none" : ftos(xKind);
};

// The query's answer for `xKind`'s site (does the query give the grip to it now?), logged with vr_debug_grab.
float(float xHand, float xKind) VR_Grab_Answers =
{
    float kind = VR_Grab_Query(xHand, xKind);
    if(kind != VR_GRAB_NONE && cvar_hget(cvarh_vr_debug_grab))
    {
        bprint(sprintf("grabquery: %s hand, %s (%s, %.1f u)%s\n", xHand == cVR_MainHand ? "main" : "off", VR_Grab_Name(kind),
                      vr_grab_ent == world ? "made" : vr_grab_ent.classname, vr_grab_dist,
                      kind == xKind ? "" : sprintf(", not the %s's", VR_Grab_Name(xKind))));
    }
    return kind == xKind;
};
