#ifdef QVR_TESTS
// Test 18 (`vr_agrunt_test 18`; ROUND23.md, "Armed humanoids' melee spacing"; his voice note 2026-10-31 19:59): each
// kind (grunt, enforcer, knight, death knight) with fists, the axe, the sword, the hell knight's long sword, Mjolnir and
// the chainsaw comes at you (standing still, your hands empty) from 160 units: its standoff (VR_AG_MeleeStandoff: 0
// for a short weapon, which closes in as before), where it stands as its blows land (flat, its middle to yours) and
// whether each attack hurt you. Phases, each its own state machine's end: (A) three attacks as it comes; (B) you step
// back 80 units: how long until it attacks again, and that attack; (C) a long blade's holder: you step in to 24 units
// inside its standoff: it holds its ground (no further off as it strikes) and its blow lands. Per trial
// `agtest: PASS|FAIL melee reach: <kind> <weapon>: ...` (every attack hurt you; a long blade stands within 8 of its
// standoff, a short weapon within 62, as before); the run ends on `agtest: test 18 over: <p> passed, <f> failed`.
// Debug > Tests > Armed Grunts: Melee Spacing.

float vr_agmr_trial;
float vr_agmr_phase;    // 0 set up, 1 coming (A), 2 you backed off (B), 3 you stepped in (C)
float vr_agmr_at;       // the phase's start
float vr_agmr_n;        // attacks over in the phase
float vr_agmr_hits;     // ... of them hurting you (over the trial)
float vr_agmr_all;      // attacks over in the trial
float vr_agmr_actat;    // the attack under way's start (its vr_ag_act_at; 0 none)
float vr_agmr_actph;    // ... the phase it began in (one begun before you moved is not the next phase's)
float vr_agmr_dmg0;     // vr_ag_player_dmg at its start
float vr_agmr_strikes;  // its vr_ag_strikes seen
float vr_agmr_dsum;     // phase A: its distances as its blows landed (each attack's first)
float vr_agmr_dn;
float vr_agmr_struck;   // the attack under way struck (its first blow's distance taken)
float vr_agmr_dB;       // phase B: the time to its next attack, its distance as it landed
float vr_agmr_tB;
float vr_agmr_dC0;      // phase C: the distance you stepped in to, its distance as its blow landed
float vr_agmr_dC;
float vr_agmr_s;        // its standoff (0: none)
float vr_agmr_stuck;     // a phase timed out
vector vr_agmr_home;
float vr_agmr_yaw;
float VR_AGMR_WEAPONS = 6;
float VR_AGMR_N = 24;   // (4 kinds x 6 weapons)

float(float w) VR_AGMR_Wid =
{
    return w == 0 ? -1 : w == 1 ? WID_AXE : (w == 2 || w == 3) ? WID_SWORD : w == 4 ? WID_MJOLNIR : WID_CHAINSAW;
};
float(float w) VR_AGMR_Flags = { return w == 3 ? QVR_WPNFLAG_USE_SECONDARY_AMMO : 0; }; // (the hell knight's sword)
string(float w) VR_AGMR_Name =
{
    return w == 0 ? "fists" : w == 1 ? "axe" : w == 2 ? "sword" : w == 3 ? "long sword" : w == 4 ? "mjolnir" : "chainsaw";
};
string(float k) VR_AGMR_Kind = { VR_AM_Row(k); return vr_am_name; };

// (flat, between the middles)
float(entity a, entity b) VR_AGMR_Dist =
{
    vector to = a.origin - b.origin;
    to_z = 0;
    return vlen(to);
};

// An armed monster of kind `k` with weapon `wid` (`flags`), `dist` ahead of you, facing you.
entity(float k, float wid, float flags, float dist) VR_AGMR_Spawn =
{
    entity p = vr_agt_player;
    makevectors([0, vr_agmr_yaw, 0]);
    vector at = p.origin + v_forward * dist;
    entity oldself = self;
    self = spawn();
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    self.weapon = wid;
    self.weaponflags = flags;
    VR_AM_Spawn(k);
    entity g = self;
    self = oldself;
    return g;
};

// You put `d` units from it (flat, along the line between you), still.
void(entity pl, entity g, float d) VR_AGMR_PlaceYou =
{
    vector to = pl.origin - g.origin;
    to_z = 0;
    vector at = g.origin + normalize(to) * d;
    at_z = pl.origin_z;
    setorigin(pl, at);
    pl.velocity = '0 0 0';
};

// The attacks' bookkeeping each frame: one starting, one over (hurt you or not), the first blow's distance.
void(entity pl, entity g) VR_AGMR_Watch =
{
    float d = VR_AGMR_Dist(g, pl);
    if(g.vr_ag_strikes != vr_agmr_strikes)
    {
        vr_agmr_strikes = g.vr_ag_strikes;
        if(vr_agmr_actat && !vr_agmr_struck && vr_agmr_actph == vr_agmr_phase)
        {
            vr_agmr_struck = 1;
            if(vr_agmr_phase == 1) { vr_agmr_dsum = vr_agmr_dsum + d; vr_agmr_dn = vr_agmr_dn + 1; }
            else if(vr_agmr_phase == 2) { vr_agmr_dB = d; }
            else { vr_agmr_dC = d; }
        }
    }
    float on = g.vr_ag_act != 0 && g.vr_ag_act != VR_AGP_SHOVE;
    if(vr_agmr_actat && (!on || g.vr_ag_act_at != vr_agmr_actat) && vr_agmr_actph != vr_agmr_phase)
    {
        vr_agmr_actat = 0; // (begun before you moved: not the phase's)
    }
    if(vr_agmr_actat && (!on || g.vr_ag_act_at != vr_agmr_actat))
    {
        // (its attack over: it hurt you or not)
        float hit = vr_ag_player_dmg > vr_agmr_dmg0;
        vr_agmr_hits = vr_agmr_hits + hit;
        vr_agmr_all = vr_agmr_all + 1;
        vr_agmr_n = vr_agmr_n + 1;
        if(!hit)
        {
            // (its blow's line again, as its weapon's melee traced it: what it met)
            vector dir = normalize(g.handvel);
            traceline(g.handpos - dir * 6, g.handpos + dir * 85, MOVE_HITMODEL_MELEE, g);
            string met = sprintf("the line met %s at %.2f; parries %g", trace_ent.classname, trace_fraction,
                vr_drp_parries);
            VR_AGT_Print(sprintf("melee reach: an attack missed (phase %g, %.0f apart, struck %g; its grip %v off you, "
                "its tip %v, its way %v: %s)", vr_agmr_phase, d, vr_agmr_struck, g.handpos - pl.origin,
                g.muzzlepos - pl.origin, dir, met));
        }
        vr_agmr_actat = 0;
    }
    if(on && !vr_agmr_actat)
    {
        vr_agmr_actat = g.vr_ag_act_at;
        vr_agmr_actph = vr_agmr_phase;
        vr_agmr_dmg0 = vr_ag_player_dmg;
        vr_agmr_struck = 0;
        if(vr_agmr_phase == 2 && vr_agmr_tB < 0)
        {
            vr_agmr_tB = time - vr_agmr_at;
        }
    }
};

void() VR_AGMR_Frame =
{
    entity pl = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agmr_trial == 0 && vr_agmr_phase == 0 && !vr_agmr_yaw)
    {
        vr_agmr_home = pl.origin;
        vr_agmr_yaw = pl.angles_y + 0.001;
    }
    if(vr_agmr_trial >= VR_AGMR_N)
    {
        if(g && g.classname != "") { VR_AGT_Remove(g); }
        VR_AGT_ClearWeapons();
        vr_agmr_trial = 0;
        vr_agmr_yaw = 0;
        cvar_set("vr_agrunt_test", "0");
        return;
    }
    float k = floor(vr_agmr_trial / VR_AGMR_WEAPONS);
    float w = vr_agmr_trial - k * VR_AGMR_WEAPONS;
    if(vr_agmr_phase == 0)
    {
        if(g && g.classname != "" && g.health > 0) { VR_AGT_Remove(g); }
        VR_AGT_ClearWeapons();
        VR_AGPT_EmptyHand(pl);
        pl.angles_y = vr_agmr_yaw;
        pl.v_angle_y = vr_agmr_yaw;
        pl.fixangle = TRUE;
        setorigin(pl, vr_agmr_home);
        pl.velocity = '0 0 0';
        vr_agt_grunt = VR_AGMR_Spawn(k, VR_AGMR_Wid(w), VR_AGMR_Flags(w), 160);
        g = vr_agt_grunt;
        VR_AGT_Hunt(g);
        vr_agmr_s = VR_AG_MeleeStandoff(g);
        VR_AGT_Print(sprintf("melee reach: trial %g: %s with %s: its weapon %.1f long from its grip, its arm %.1f, "
            "standoff %.1f (raw %.1f)", vr_agmr_trial, VR_AGMR_Kind(k), VR_AGMR_Name(w), vr_ag_msp_len,
            vr_ag_msp_arm, vr_agmr_s, vr_ag_msp_raw));
        vr_agmr_phase = 1;
        vr_agmr_at = time;
        vr_agmr_n = 0;
        vr_agmr_hits = 0;
        vr_agmr_all = 0;
        vr_agmr_actat = 0;
        vr_agmr_strikes = g.vr_ag_strikes;
        vr_agmr_dsum = 0;
        vr_agmr_dn = 0;
        vr_agmr_dB = -1;
        vr_agmr_tB = -1;
        vr_agmr_dC0 = -1;
        vr_agmr_dC = -1;
        vr_agmr_stuck = 0;
        return;
    }
    float done = FALSE;
    if(!g || g.classname == "" || g.health <= 0)
    {
        vr_agmr_stuck = 1;
        done = TRUE;
    }
    else
    {
        pl.velocity_x = 0; // (you standing still)
        pl.velocity_y = 0;
        VR_AGMR_Watch(pl, g);
        float t = time - vr_agmr_at;
        if(t > 12)
        {
            VR_AGT_Print(sprintf("melee reach: phase %g timed out (%g attacks, %.0f apart)", vr_agmr_phase, vr_agmr_n,
                VR_AGMR_Dist(g, pl)));
            vr_agmr_stuck = 1;
            done = TRUE;
        }
        else if(vr_agmr_phase == 1 && vr_agmr_n >= 3)
        {
            VR_AGMR_PlaceYou(pl, g, VR_AGMR_Dist(g, pl) + 80);
            vr_agmr_phase = 2;
            vr_agmr_at = time;
            vr_agmr_n = 0;
        }
        else if(vr_agmr_phase == 2 && vr_agmr_n >= 1)
        {
            if(vr_agmr_s > 0)
            {
                vr_agmr_dC0 = max(36, vr_agmr_s - 24);
                VR_AGMR_PlaceYou(pl, g, vr_agmr_dC0);
                vr_agmr_phase = 3;
                vr_agmr_at = time;
                vr_agmr_n = 0;
            }
            else
            {
                done = TRUE;
            }
        }
        else if(vr_agmr_phase == 3 && vr_agmr_n >= 1)
        {
            done = TRUE;
        }
    }
    if(!done)
    {
        return;
    }
    float dA = vr_agmr_dn > 0 ? vr_agmr_dsum / vr_agmr_dn : -1;
    float standOk = vr_agmr_s > 0 ? fabs(dA - vr_agmr_s) <= 8 : dA > 0 && dA <= 62;
    float heldOk = vr_agmr_s <= 0 || (vr_agmr_dC > 0 && vr_agmr_dC <= vr_agmr_dC0 + 6);
    float ok = !vr_agmr_stuck && vr_agmr_all > 0 && vr_agmr_hits == vr_agmr_all && standOk && heldOk;
    string inC = vr_agmr_s > 0 ? sprintf("stepped in to %.0f, it struck from %.1f", vr_agmr_dC0, vr_agmr_dC) :
                 "no standoff";
    string what = sprintf("melee reach: %s %s: standoff %.0f, stood %.1f as its blows landed; ", VR_AGMR_Kind(k),
        VR_AGMR_Name(w), vr_agmr_s, dA);
    VR_AGT_Check(strcat(what, sprintf("backed off: it attacked again after %.2f s from %.1f; %s: %g of %g attacks hurt "
        "you:", vr_agmr_tB, vr_agmr_dB, inC, vr_agmr_hits, vr_agmr_all)), ok);
    if(g && g.classname != "") { VR_AGT_Remove(g); }
    vr_agt_grunt = world;
    vr_agmr_trial = vr_agmr_trial + 1;
    vr_agmr_phase = 0;
};
#endif
