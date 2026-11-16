// vr_playerdisarm_grab.qc -- taking a weapon out of another player's hand (MP B10; his voice note 2026-11-15 03:27): close an
// empty hand on the weapon he holds, on its drawn model (the melee's line of it, its hotspots), as the armed humanoids'
// grab (not too fast: vr_agrunt_grab_speed), then pull it away from him hard enough (vr_player_disarm_pull) while the
// grip holds: it is yours, in that hand. No tug of war, no ragdoll: a pull too gentle lets the hand slip off it
// (vr_player_disarm_slip). Every player the same way (a listen host's own, a remote one: lag compensated, where the
// taker saw the other's hands). On with vr_player_disarm_pvp (players disarm each other) and vr_player_disarm_grab;
// the weapon leaves his hand by the one way out of a player's hand, vr_playerdisarm.qc VR_Player_Disarm, as a shot's.

.entity vr_pdg_of;   // the player whose weapon this player's hand holds (world: none)
.float vr_pdg_hand;  // ... the hand holding it (this player's)
.float vr_pdg_vhand; // ... his hand holding the weapon
.float vr_pdg_since; // ... the grab's time
.float vr_pdg_peak;  // ... the fastest pull so far (m/s)
float vr_pdg_takes;  // weapons taken from players this map (the tests' count)
float vr_pdg_slips;  // grabs that slipped off (too gentle, let go)

void(string s) VR_PDG_Log = { dprint(sprintf("pdisarm: %s\n", s)); };

// How far point `at` is from the weapon in player `v`'s hand `vh` as drawn (its melee line from butt to tip, its
// hotspots); 1000000 none (an empty hand, a fist).
float(entity v, float vh, vector at) VR_PDG_Gap =
{
    entity was = self;
    self = v; // (VR_Melee_Line's helpers read self)
    float ok = VR_Parry_WeaponLine(v, vh);
    self = was;
    if(!ok)
    {
        return 1000000;
    }
    float d = VR_SegmentDistance(at, at, vr_parry_butt, vr_parry_end);
    for(float i = 0; i < 4; i++)
    {
        vector hs = weaponhotspot(v, vh, i);
        if(hs != '0 0 0')
        {
            d = min(d, vlen(at - hs));
        }
    }
    return d;
};

void(entity pl, string why) VR_PDG_End =
{
    if(why != "")
    {
        VR_PDG_Log(sprintf("%s lets go of %s's weapon (%s; fastest pull %.2f m/s, %g needed)", pl.netname,
            pl.vr_pdg_of.netname, why, pl.vr_pdg_peak, cvar_hget(cvarh_vr_player_disarm_pull)));
        vr_pdg_slips = vr_pdg_slips + 1;
    }
    pl.vr_pdg_of = world;
};

// `pl`'s hand `h` grip just pressed, empty: on another player's weapon? Lag compensated (a remote taker: the others'
// hands where he saw them). TRUE: holding it now.
float(entity pl, float h) VR_PDG_Try =
{
    float m2u = VRMetersToUnits();
    float reach = cvar_hget(cvarh_vr_player_disarm_reach) * 0.01 * m2u;
    float lc = lagcompbegin(pl);
    vector at = VRGetEntHandPos(pl, h);
    entity best = world;
    float bestHand = 0;
    float bestGap = reach;
    entity v = VR_Player();
    while(v)
    {
        if(v != pl && (v.flags & FL_CLIENT) && v.health > 0 && vlen(v.origin - pl.origin) < 200)
        {
            for(float vh = 0; vh < 2; vh++)
            {
                float g = VR_PDG_Gap(v, vh, at);
                if(g <= bestGap)
                {
                    best = v;
                    bestHand = vh;
                    bestGap = g;
                }
            }
        }
        v = VR_NextPlayer(v);
    }
    vector rel = '0 0 0';
    if(best)
    {
        rel = VRGetEntHandVelU(pl, h) + pl.velocity - (VRGetEntHandVelU(best, bestHand) + best.velocity);
    }
    if(lc)
    {
        lagcompend();
    }
    if(!best)
    {
        return FALSE;
    }
    float most = cvar_hget(cvarh_vr_agrunt_grab_speed); // (the armed humanoids' rule: a deliberate grab, not a swipe)
    float sp = vlen(rel) * (vr_hand_time_scale > 0 ? vr_hand_time_scale : 1) / m2u;
    if(most > 0 && sp > most)
    {
        VR_PDG_Log(sprintf("%s's hand %g through %s's weapon at %.2f m/s: too fast to take hold", pl.netname, h,
            best.netname, sp));
        return FALSE;
    }
    pl.vr_pdg_of = best;
    pl.vr_pdg_hand = h;
    pl.vr_pdg_vhand = bestHand;
    pl.vr_pdg_since = time;
    pl.vr_pdg_peak = 0;
    VR_PDG_Log(sprintf("%s's hand %g takes hold of %s's weapon (hand %g), %.1f units off it, at %.2f m/s", pl.netname, h,
        best.netname, bestHand, bestGap, sp));
    return TRUE;
};

// A hold on another's weapon, each frame: pulled away from him hard enough, it is yours; let go, slipped off, or his no
// longer: over.
void(entity pl) VR_PDG_Hold =
{
    entity v = pl.vr_pdg_of;
    float h = pl.vr_pdg_hand;
    float vh = pl.vr_pdg_vhand;
    if(pl.health <= 0 || !VRIsHandEmpty(pl, h) || VR_HandGrabUtil_Impl_GetLastReleaseTime(pl, h) > pl.vr_pdg_since)
    {
        VR_PDG_End(pl, "let go");
        return;
    }
    if(v.health <= 0 || !(v.flags & FL_CLIENT) || VRIsHandWeaponless(v, vh))
    {
        VR_PDG_End(pl, ""); // (dead, gone, or it left his hand some other way)
        return;
    }
    float m2u = VRMetersToUnits();
    float lc = lagcompbegin(pl);
    vector at = VRGetEntHandPos(pl, h);
    float gap = VR_PDG_Gap(v, vh, at);
    vector rel = VRGetEntHandVelU(pl, h) + pl.velocity - (VRGetEntHandVelU(v, vh) + v.velocity);
    vector chest = v.origin + v.view_ofs * 0.6;
    if(lc)
    {
        lagcompend();
    }
    // (away from his body: the pull towards you, or aside; pushing it into him is no pull)
    float pull = (rel * normalize(at - chest)) * (vr_hand_time_scale > 0 ? vr_hand_time_scale : 1) / m2u;
    pl.vr_pdg_peak = max(pl.vr_pdg_peak, pull);
    if(time > pl.vr_pdg_since && pull >= cvar_hget(cvarh_vr_player_disarm_pull))
    {
        pl.vr_pdg_of = world;
        vr_pdg_takes = vr_pdg_takes + 1;
        string name = WeaponNameOf(VRGetEntWeapon(v, vh), VRGetEntWeaponFlags(v, vh));
        string why = sprintf("pulled at %.2f m/s", pull);
        VR_Player_Disarm(v, vh, pl, h, '0 0 0', 0, why);
        VR_PDG_Log(sprintf("%s's %s (hand %g) taken by %s (hand %g): %s; %s", v.netname, name, vh, pl.netname, h, why,
            VRIsHandWeaponless(pl, h) ? "it falls" : "in his hand"));
        return;
    }
    if(gap > cvar_hget(cvarh_vr_player_disarm_slip) * 0.01 * m2u)
    {
        VR_PDG_End(pl, sprintf("slipped off, %.1f units from it", gap));
    }
};

// Each player's frame (PlayerPostThink).
void(entity pl) VR_PDG_Frame =
{
    if(!cvar_hget(cvarh_vr_player_disarm_pvp) || !cvar_hget(cvarh_vr_player_disarm_grab) || !(pl.flags & FL_CLIENT))
    {
        pl.vr_pdg_of = world;
        return;
    }
    if(pl.vr_pdg_of)
    {
        VR_PDG_Hold(pl);
        return;
    }
    if(pl.health <= 0)
    {
        return;
    }
    for(float h = 0; h < 2; h++)
    {
        if(VR_Hand_Free(pl, h, VR_HANDFREE_LOCKED) && VR_HandGrabUtil_StartedHandGrabbing(pl, h) && VR_PDG_Try(pl, h))
        {
            return;
        }
    }
};
