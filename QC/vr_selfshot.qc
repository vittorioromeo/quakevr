// ----------------------------------------------------------------------------
// VR: shooting yourself (vr_selfshot, vr_selfshot_damage; ROUND24.md, "Shooting yourself").
//
// In a headset, a hand's shots and missiles meet the shooter's own body as it is drawn: the torso, the legs, each
// forearm and hand (the engine's vr_selfshot.cpp: the capsules your wounds are painted by). Not the head: the eyes are
// inside it, and a gun at your face is never seen hitting it. The hand that fires is never met (its gun points away
// from it), nor the other hand while it steadies the gun (two-handed: the shot's line starts past it). A hitscan shot is
// met from its muzzle on (what lies along the gun behind the muzzle is the gun's own: the grip, the hand on its
// fore-end); a missile from where it starts, so a muzzle pressed into your forearm hits it.
//
// The damage takes the normal path (T_Damage: armour, the plate nearest the hit shed, god mode, Quad, teamplay), times
// vr_selfshot_damage instead of vr_damage_self; its wound is painted exactly where it struck (QVR_WOUND_PRECISE), the
// hand struck buzzes. Only the listen server's own player (the engine draws his body): co-op guests and flat-screen play
// are never hit by themselves.
// ----------------------------------------------------------------------------

float QVR_SS_PART_TORSO        = 0;
float QVR_SS_PART_LEGS         = 1;
float QVR_SS_PART_OFF_FOREARM  = 2;
float QVR_SS_PART_OFF_HAND     = 3;
float QVR_SS_PART_MAIN_FOREARM = 4;
float QVR_SS_PART_MAIN_HAND    = 5;

float QVR_WOUND_PRECISE = 128; // added to a wound's kind (vr_wounds.qc): at its point exactly (the engine's Event::precise)

// The self-hit being dealt now: whose, when (its frame), where, going which way, the part, the hand that fired.
entity vr_ss_ent;
float vr_ss_time;
vector vr_ss_at;
vector vr_ss_dir;
float vr_ss_part;
float vr_ss_hand;
float vr_ss_hits;        // self-hits so far (the tests)
float vr_ss_felt;        // hits felt in a hand (haptic) so far
float vr_ss_felt_hand;   // the last one's hand
float vr_ss_last_take;   // the last self-hit's damage taken (health and armour)
// A player's hand's pellets being fired now (FireBullets, around FireBulletsImpl): the hand + 1 (0 none) and its
// muzzle. Other callers of FireBulletsImpl (monsters, the tests' shots from anywhere) never meet the shooter.
float vr_ss_armed;
vector vr_ss_armed_muzzle;

// Whether `pl` can hit himself: the option on, a player in a headset with tracked hands (not flat-screen play).
float(entity pl) VR_SelfShot_On =
{
    return cvar_hget(cvarh_vr_selfshot) && (pl.flags & FL_CLIENT) && VRHandsTracked(pl) && !VR_FlatPlay(pl);
};

// The parts a shot from `xHand` never meets (1 << QVR_SS_PART_*): its own forearm and hand; the other's too while both
// hold the gun.
float(entity pl, float xHand) VR_SelfShot_Skip =
{
    float mainArm = 16 + 32, offArm = 4 + 8;
    float s = xHand == cVR_MainHand ? mainArm : offArm;
    if(VRGet2HAiming(pl))
    {
        s = mainArm | offArm;
    }
    return s;
};

// The self-hit now dealt to `targ` (in this frame's T_Damage, its wound).
float(entity targ) VR_SelfShot_Live =
{
    return targ == vr_ss_ent && vr_ss_time == time && targ != world;
};

void(entity pl, vector xAt, vector xDir, float xPart, float xHand) VR_SelfShot_Note =
{
    vr_ss_ent = pl;
    vr_ss_time = time;
    vr_ss_at = xAt;
    vr_ss_dir = xDir;
    vr_ss_part = xPart;
    vr_ss_hand = xHand;
    vr_ss_hits++;
    if(cvar_hget(cvarh_vr_selfshot_debug))
    {
        dprint(sprintf("selfshot: hand %g hit part %g at %v going %v\n", xHand, xPart, xAt, xDir));
    }
};

// A hitscan shot of `pl`'s hand `xHand` from its muzzle `xFrom` to `xTo`: TRUE if it meets his own body first (where:
// vr_ss_at).
float(entity pl, float xHand, vector xFrom, vector xTo) VR_SelfShot_Segment =
{
    if(!VR_SelfShot_On(pl) || xFrom == xTo)
    {
        if(cvar_hget(cvarh_vr_selfshot_debug) >= 2)
        {
            dprint(sprintf("selfshot: off (option %g, tracked %g, flat %g)\n", cvar_hget(cvarh_vr_selfshot),
                VRHandsTracked(pl), VR_FlatPlay(pl)));
        }
        return FALSE;
    }
    float f = selfhit_segment(pl, xFrom, xTo, 0, VR_SelfShot_Skip(pl, xHand));
    if(cvar_hget(cvarh_vr_selfshot_debug) >= 2)
    {
        dprint(sprintf("selfshot: hand %g segment %v .. %v: %g\n", xHand, xFrom, xTo, f));
    }
    if(f < 0)
    {
        return FALSE;
    }
    VR_SelfShot_Note(pl, xFrom + (xTo - xFrom) * f, normalize(xTo - xFrom), selfhit_info(0), xHand);
    return TRUE;
};

// A pellet's trace from `xSrc` (behind the muzzle, VR_ShotBackPoint) going `xDir`, just done (self the shooter, the
// muzzle `xMuzzle` of hand `xHand`): if it meets his own body before what it struck, the trace globals say so (that
// body struck there). Not through a teleporter (its trace goes on beyond: your image is met as before).
void(float xHand, vector xMuzzle, vector xSrc, vector xDir) VR_SelfShot_Pellet =
{
    if(!(self.flags & FL_CLIENT) || portal_crossings() > 0)
    {
        return;
    }
    float from = (xMuzzle - xSrc) * xDir; // (level with the muzzle on the pellet's line)
    float to = (trace_endpos - xSrc) * xDir;
    if(cvar_hget(cvarh_vr_selfshot_debug) >= 2)
    {
        dprint(sprintf("selfshot: pellet from %g to %g (struck %s)\n", from, to, trace_ent.classname));
    }
    if(to <= from ||!VR_SelfShot_Segment(self, xHand, xSrc + xDir * max(0, from), trace_endpos))
    {
        return;
    }
    trace_ent = self;
    trace_endpos = vr_ss_at;
    trace_fraction = trace_fraction * ((vr_ss_at - xSrc) * xDir) / to;
    trace_plane_normal = -xDir;
    trace_startsolid = trace_allsolid = FALSE;
};

// A hand's missile, just launched (VR_ShotLaunched; self the player): it may meet his own body (the engine's
// VR_SelfShotMove, from where it starts).
void(entity xMissile, float xHand) VR_SelfShot_Launched =
{
    xMissile.vr_ss_skip = VR_SelfShot_On(self) ? 64 + VR_SelfShot_Skip(self, xHand) : 0;
    xMissile.vr_ss_struck = 0;
};

// A missile's touch of its owner after the engine found it struck his own body (VR_OwnerSpared; self the missile).
void(entity xMissile) VR_SelfShot_MissileStruck =
{
    VR_SelfShot_Note(xMissile.owner, xMissile.origin, normalize(xMissile.velocity), selfhit_info(0), -1);
};

// T_Damage, as `take` (and `save`, the armour's) is taken from the player by his own shot: the hand it struck buzzes
// (a forearm or a hand; the body's hits buzz as any other's: the client's pain haptics).
void(entity targ, float take, float save) VR_SelfShot_Felt =
{
    vr_ss_last_take = take + save;
    float hand = -1;
    if(vr_ss_part == QVR_SS_PART_OFF_FOREARM || vr_ss_part == QVR_SS_PART_OFF_HAND)
    {
        hand = cVR_OffHand;
    }
    else if(vr_ss_part == QVR_SS_PART_MAIN_FOREARM || vr_ss_part == QVR_SS_PART_MAIN_HAND)
    {
        hand = cVR_MainHand;
    }
    if(hand < 0)
    {
        return;
    }
    entity oldSelf = self;
    self = targ;
    haptic(hand, 0.0, 0.25, 90, 1.0);
    self = oldSelf;
    vr_ss_felt++;
    vr_ss_felt_hand = hand;
};
