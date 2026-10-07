// vr_highlights.qc -- the game's cool moments for the highlight log (vr_highlights; the engine's vr_highlights.cpp,
// docs/vr-port/TRAILER.md): each reported with the builtin highlight(kind, score, subject, detail, since), only while
// a log runs (highlighting()). The engine stamps them in real time from the log's sync mark, makes multi-kills of the
// kills ("kill", "gib") close together, and logs bullet time itself.
//
// Kinds (the score: the engine's own for 0): kill (1 and up), gib (3 and up), corpsegib, parry, shoveparry (an enemy's
// shove parried), counter, shotgrenade (a grenade set off by your shot or blow), chain (one set off by a blast, or a box
// blown up by another's), explosion (a blast that kills two or more), barrel (an explosive box blown up), axestick (a
// thrown axe stuck in a monster), grapplepull (a monster hooked and reeled in), swing (swinging on the grappling hook).

float vr_hl_kills;       // kills logged so far (a blast's are its count: VR_Highlight_Blast)
float vr_hl_blast_depth; // blasts within blasts (a box blown up by a blast blows up within it)
string vr_hl_what;       // VR_Highlight_Kill's detail

// What dealt the blow (before Killed: the hit's globals are the blow's).
string(entity inflictor, entity attacker) VR_Highlight_Weapon =
{
    if(attacker.classname == "explo_box" || attacker.classname == "misc_explobox")
    {
        return "barrel";
    }
    if(inflictor != attacker && inflictor != world)
    {
        if(inflictor.classname == "thrown_weapon")
        {
            return sprintf("thrown %s", inflictor.netname);
        }
        return VR_Dummy_ProjectileName(inflictor);
    }
    float hand = vr_hithand >= 0 ? vr_hithand : cVR_MainHand;
    if(vr_hitkind == QVR_HITKIND_HEADBUTT) { return "headbutt"; }
    if(vr_hitkind == QVR_HITKIND_SHOVE)    { return "shove"; }
    if(vr_hitkind == QVR_HITKIND_BASH)     { return sprintf("%s bash", WeaponIdToWeaponName(VRGetEntWeapon(attacker, hand))); }
    if(vr_hitkind == QVR_HITKIND_SPLASH)   { return "discharge"; }
    if(vr_hitkind == QVR_HITKIND_SHOT && vr_hitweapon != "") { return vr_hitweapon; }
    if(vr_hitkind == QVR_HITKIND_BLEED)    { return "bleeding"; }
    return WeaponIdToWeaponName(VRGetEntWeapon(attacker, hand));
};

// T_Damage, a blow about to kill `targ`: its score as a moment (0: not one), its detail in vr_hl_what. The kill is
// logged after Killed (VR_Highlight_Killed), which knows whether it gibbed.
float(entity targ, entity inflictor, entity attacker, float isVRMelee) VR_Highlight_Kill =
{
    vr_hl_what = "";
    if(!highlighting() || !(targ.flags & FL_MONSTER) || targ.health > 0)
    {
        return 0;
    }
    float byBox = (attacker.classname == "explo_box" || attacker.classname == "misc_explobox");
    if(!(attacker.flags & FL_CLIENT) && !byBox)
    {
        return 0;
    }

    float score = 1;
    string tags = "";
    float headshot = ((vr_hitpart_target == targ && vr_hitpart_time == time && vr_hitpart == 1) ||
                      (vr_hitpellets > 0 && vr_hitpellets_head > 0));
    if(headshot)
    {
        score = score + 1.5;
        tags = " headshot";
    }
    float melee = (isVRMelee || vr_hitkind == QVR_HITKIND_MELEE || vr_hitkind == QVR_HITKIND_BASH ||
                   vr_hitkind == QVR_HITKIND_SHOVE || vr_hitkind == QVR_HITKIND_HEADBUTT);
    if(vr_hitkind == QVR_HITKIND_HEADBUTT)
    {
        score = score + 2;
    }
    else if(melee)
    {
        score = score + 1;
        tags = sprintf("%s melee", tags);
    }
    if(inflictor.classname == "thrown_weapon")
    {
        score = score + 3;
    }
    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        tags = sprintf("%s blast", tags);
    }
    if(byBox)
    {
        score = score + 1;
    }
    // The big ones.
    string c = targ.classname;
    if(c == "monster_shambler" || c == "monster_super_shambler")
    {
        score = score + 2;
    }
    else if(c == "monster_ogre" || c == "monster_demon1" || c == "monster_shalrath" || c == "monster_hell_knight" ||
            c == "monster_ranged_knight")
    {
        score = score + 1;
    }
    vr_hl_what = sprintf("%s%s", VR_Highlight_Weapon(inflictor, attacker), tags);
    return score;
};

// T_Damage, after Killed: the kill `VR_Highlight_Kill` scored (`xScore` 0: none), as a gib when the death code gibbed it.
void(entity targ, float xScore, string xWhat) VR_Highlight_Killed =
{
    if(xScore <= 0)
    {
        return;
    }
    vr_hl_kills = vr_hl_kills + 1;
    if(targ.vr_gib)
    {
        highlight("gib", xScore + 2, targ, xWhat, 0);
    }
    else
    {
        highlight("kill", xScore, targ, xWhat, 0);
    }
};

// VR_Corpse_Gib: a corpse blown apart.
void(entity c, entity inflictor, entity attacker) VR_Highlight_CorpseGib =
{
    if(highlighting() && (attacker.flags & FL_CLIENT))
    {
        highlight("corpsegib", 0, c, VR_Highlight_Weapon(inflictor, attacker), 0);
    }
};

// VR_Parry: `p` parried `attacker`'s blow with `hand` (-1: crossed arms); an enemy's shove parried is its own kind.
void(entity p, entity attacker, float hand) VR_Highlight_Parry =
{
    if(!highlighting() || !(p.flags & FL_CLIENT))
    {
        return;
    }
    string how = "crossed arms";
    if(hand >= 0)
    {
        how = sprintf("%s, %s", WeaponIdToWeaponName(VRGetEntWeapon(p, hand)), VR_Dummy_HandName(hand));
    }
    highlight(attacker.vr_eshove_striking ? "shoveparry" : "parry", 0, attacker, how, 0);
};

// VR_Counter_Use (self: the player): the counter `xWhat` landing on `xOn`.
void(string xWhat, entity xOn) VR_Highlight_Counter =
{
    if(self.flags & FL_CLIENT)
    {
        highlight("counter", 0, xOn, xWhat, 0);
    }
};

// VR_GrenShot_Damage: the grenade `g` set off by `attacker` (`xHow`: what did it); a blast's: a chain.
void(entity g, entity attacker, string xHow) VR_Highlight_GrenShot =
{
    if(!highlighting() || !(attacker.flags & FL_CLIENT))
    {
        return;
    }
    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        highlight("chain", 0, g, xHow, 0);
        return;
    }
    // One shot in flight: harder, and the better moment.
    highlight("shotgrenade", vlen(g.velocity) >= VR_GREN_FLYING ? 6 : 4, g, xHow, 0);
};

// T_RadiusDamage, after its damage: the blast's moment, `xKills` the kills it made (with those of the blasts it set off).
void(entity inflictor, entity attacker, float xKills) VR_Highlight_Blast =
{
    if(!highlighting())
    {
        return;
    }
    float box = (inflictor.classname == "explo_box");
    if(!(attacker.flags & FL_CLIENT) && !box && xKills < 2)
    {
        return;
    }
    string what = box ? "barrel" : VR_Dummy_ProjectileName(inflictor);
    if(inflictor == attacker && !box)
    {
        what = "discharge";
    }
    if(xKills >= 2)
    {
        highlight("explosion", 2 + 1.5 * xKills, inflictor, sprintf("%s, %g kills", what, xKills), 0);
    }
    else if(vr_hl_blast_depth > 0)
    {
        highlight("chain", 0, inflictor, what, 0); // (a box another blast blew up)
    }
    else if(box)
    {
        highlight("barrel", xKills > 0 ? 3 : 0, inflictor, what, 0);
    }
};

// VR_AxeStick_Stuck (self: the axe, other: what it is in): a thrown axe stuck in a monster.
void(float xSpeed) VR_Highlight_AxeStick =
{
    if(highlighting() && (self.vr_stick_by.flags & FL_CLIENT) && VR_AxeStick_InMonster(self))
    {
        highlight("axestick", 0, other, sprintf("%s at %.0f u/s", self.netname, xSpeed), 0);
    }
};

// Swinging on the grappling hook: off the floor, the hook in something that stays, faster than this (u/s), for at least
// VR_HL_SWING_TIME game seconds: one moment, logged as the swing ends (from its start, as long as it lasted).
float VR_HL_SWING_SPEED = 250;
float VR_HL_SWING_TIME = 0.6;
.float vr_hl_swing_since; // on a player: swinging since then (0: not swinging)

// VR_Grapple_PlayerFrame (self: the player), after its hooks: `xHooked`, a hook holds it from something that stays.
void(float xHooked) VR_Highlight_SwingFrame =
{
    float swinging = (xHooked && !(self.flags & FL_ONGROUND) && vlen(self.velocity) > VR_HL_SWING_SPEED);
    if(swinging)
    {
        if(!self.vr_hl_swing_since)
        {
            self.vr_hl_swing_since = time;
        }
        return;
    }
    if(self.vr_hl_swing_since && time - self.vr_hl_swing_since >= VR_HL_SWING_TIME)
    {
        float lasted = time - self.vr_hl_swing_since;
        highlight("swing", 2 + min(3, lasted), world, sprintf("%.1f s", lasted), self.vr_hl_swing_since);
    }
    self.vr_hl_swing_since = 0;
};
