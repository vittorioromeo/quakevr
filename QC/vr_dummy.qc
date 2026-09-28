// vr_dummy.qc -- the firing range's training dummy (vr_dummy, placed by quakevr/maps/vrfiringrange.ent):
// a grunt that stands still, cannot be hurt and reports every hit, for trying the melee (swings,
// punches, headbutts, bashes), throws and guns and getting a feel for their damage:
// - in the attacker's console (a player's; a monster's with `developer` on): how much damage (after quad and the damage multipliers), what kind of
//   blow (weapon fire and which weapon, a melee swing with what and how fast, a headbutt, a bash or
//   shove, a thrown weapon, box, gib or head, an explosion, a deflected projectile) and where it
//   struck (head, limb, legs or body, with the multiplier, where positional damage applies; for a
//   melee blow only where it landed: melee damage does not depend on it), with the run of hits so far;
// - as a number rising from where it struck, for every player (floattext): yellow on the head, orange
//   for melee, cyan for thrown things, red for explosions, white otherwise.
// It flinches (its pain frames, a lean away from the blow) but is not knocked away, and is no
// monster of the level (not counted, never killed). Attacks that their inflictor does not tell
// apart are tagged by their code (VR_SetHitKind, combat.qc).
//
// With vr_dummy_attacks on (the firing range's "dummy attacks" button: vr_dummy_attack_toggle, below) it
// strikes back, for practising the parry: see "Attacks" below. Off (the default, and at every map load),
// it does nothing more than above: the motion takes are replayed against it (docs/vr-port/MOTIONS.md).

.vector vr_dummy_home;       // where it stands
.vector vr_dummy_angles;     // and its facing
.vector vr_dummy_lean;       // leaning away from the last blow (pitch, roll), dying out
.float vr_dummy_painframe;   // the pain animation's next frame (0: standing)
.float vr_dummy_streak_time; // the last hit of the current run (hits less than a second apart)
.float vr_dummy_streak_hits;
.float vr_dummy_streak_damage;

// The soldier model's frames.
float VR_DUMMY_STAND_FIRST = 0;  // stand1 .. stand8
float VR_DUMMY_PAIN_FIRST  = 40; // pain1 .. pain6
float VR_DUMMY_PAIN_LAST   = 45;

float VR_DUMMY_HEALTH = 1000;

vector vr_dummy_colour; // the floating number's, chosen along with the kind of hit

.float vr_dummy_turn; // yaw turned from its facing, towards whom it strikes at (attacks on; else 0)

// Which way it faces: its own facing, turned towards whom it strikes at.
vector(entity e) VR_Dummy_Facing =
{
    return e.vr_dummy_angles + '0 1 0' * e.vr_dummy_turn;
};

// ----------------------------------------------------------------------------
// Naming what hit it

// A projectile (or anything thrown or fired) by its classname.
string(entity e) VR_Dummy_ProjectileName =
{
    string c = e.classname;
    if(c == "missile")           { return "rocket"; }
    if(c == "MultiRocket")       { return "multi-rocket"; }
    if(c == "grenade")           { return "grenade"; }
    if(c == "MiniGrenade" || c == "MultiGrenade") { return "multi-grenade"; }
    if(c == "proximity_grenade") { return "proximity mine"; }
    if(c == "spike")             { return e.touch == superspike_touch ? "super nail" : "nail"; }
    if(c == "lava_spike")        { return "lava nail"; }
    if(c == "plasma")            { return "plasma bolt"; }
    if(c == "hiplaser")          { return "laser"; }
    if(c == "hook")              { return "grappling hook"; }
    if(c == "thrown_weapon")     { return sprintf("thrown %s", e.netname); }
    if(c == "")                  { return "something"; }
    return c;
};

string(float xHand) VR_Dummy_HandName =
{
    return xHand == cVR_OffHand ? "off hand" : "main hand";
};

// How fast `e` flies, in metres per second.
float(entity e) VR_Dummy_Speed =
{
    return vlen(e.velocity) / VRMetersToUnits();
};

// What kind of blow it is, in words; sets vr_dummy_colour.
string(entity inflictor, entity attacker, float isVRMelee) VR_Dummy_Kind =
{
    vector orange = '1 0.6 0.15';
    vector cyan = '0.4 0.9 1';
    vr_dummy_colour = '1 1 1';

    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        vr_dummy_colour = '1 0.35 0.25';
        if(inflictor == attacker)
        {
            // A discharge (the lightning gun under water), an exploding box, ...
            return sprintf("explosion: %s", (attacker.flags & FL_CLIENT) ? "discharge" : attacker.classname);
        }
        return sprintf("explosion: %s", VR_Dummy_ProjectileName(inflictor));
    }

    if(vr_hitkind == QVR_HITKIND_HEADBUTT)
    {
        vr_dummy_colour = orange;
        float lunge = vlen(attacker.headbutt_max_vec);
        float threshold = max(0.3, cvar_hget(cvarh_vr_headbutt_speed));
        return sprintf("headbutt, %.1f m/s (x%.2f)", lunge, min(3, lunge / threshold));
    }

    if(vr_hitkind == QVR_HITKIND_BASH)
    {
        vr_dummy_colour = orange;
        string bash = sprintf("bash with the %s, %s",
                       WeaponIdToWeaponName(VRGetEntWeapon(attacker, vr_hithand >= 0 ? vr_hithand : cVR_MainHand)),
                       vr_bash_both ? "two hands" : "one hand");
        bash = vr_bash_counter ? sprintf("counter %s (x%.2f)", bash, vr_bash_counter) : bash;
        return vr_bash_exhaust_dmg > 0 ? sprintf("%s, exhausted (damage x%.2f, knockback x%.2f)", bash, vr_bash_exhaust_dmg,
            vr_bash_exhaust_push) : bash;
    }

    if(vr_hitkind == QVR_HITKIND_SHOVE)
    {
        vr_dummy_colour = orange;
        string shove = vr_hithand >= 0 ? sprintf("shove with the %s", VR_Dummy_HandName(vr_hithand)) : "shove with both hands";
        shove = vr_bash_counter ? sprintf("counter %s (x%.2f)", shove, vr_bash_counter) : shove;
        return vr_bash_exhaust_dmg > 0 ? sprintf("%s, exhausted (damage x%.2f, knockback x%.2f)", shove, vr_bash_exhaust_dmg,
            vr_bash_exhaust_push) : shove;
    }

    if(vr_hitkind == QVR_HITKIND_MELEE)
    {
        vr_dummy_colour = orange;
        float wid = VRGetEntWeapon(attacker, vr_hithand);
        entity oldSelf = self;
        self = attacker;
        entity b = VR_Blow(vr_hithand);
        self = oldSelf;
        // What struck (the weapon, the kind of blow, how it moved, the part), with which hand, how fast the hand went
        // (a pommel strike: the pommel) and the strength (vr_melee.qc).
        string what = VR_Melee_KindName(b.mh_kind, b.mh_thing);
        if(b.mh_way != "")
        {
            what = sprintf("%s (%s)", what, b.mh_way);
        }
        if(wid == WID_FIST)
        {
            if(VR_Carry_IsHolding(attacker, vr_hithand))
            {
                what = sprintf("%s with a box in hand", what);
            }
        }
        else
        {
            what = sprintf("%s, %s", WeaponIdToWeaponName(wid), what);
        }
        if(b.mh_part != "")
        {
            what = sprintf("%s with %s", what, b.mh_part);
        }
        what = sprintf("melee: %s, %s, %.1f m/s (x%.2f)", what, VR_Dummy_HandName(vr_hithand), b.mh_speed, b.mh_strength);
        if(b.mh_counter > 0)
        {
            what = sprintf("%s, counter x%.2f", what, b.mh_counter); // after a parry (vr_counter)
        }
        if(b.mh_exhaust > 0)
        {
            what = sprintf("%s, exhausted x%.2f", what, b.mh_exhaust); // short of stamina (vr_strike_stamina)
        }
        return what;
    }

    if(vr_hitkind == QVR_HITKIND_SHOT)
    {
        string weapon = vr_hitweapon;
        if(weapon == "")
        {
            weapon = vr_hithand >= 0 ? WeaponIdToWeaponName(VRGetEntWeapon(attacker, vr_hithand)) : "hitscan";
        }
        return sprintf("weapon fire: %s", weapon);
    }

    if(isVRMelee)
    {
        vr_dummy_colour = orange;
        return "melee"; // a flat-screen player's swing
    }

    if(inflictor == attacker || inflictor == world)
    {
        if(attacker.flags & FL_CLIENT)
        {
            return "hit";
        }
        return sprintf("%s's attack", attacker.classname);
    }

    if(inflictor.classname == "thrown_weapon")
    {
        vr_dummy_colour = cyan;
        return sprintf("thrown weapon: %s, %.1f m/s", inflictor.netname, VR_Dummy_Speed(inflictor));
    }

    if(inflictor.vr_gib)
    {
        vr_dummy_colour = cyan;
        // A head is the monster itself (ThrowHead), still named after it.
        string gib = substring(inflictor.classname, 0, 8) == "monster_" ? "head" : "gib";
        return sprintf("thrown %s, %.1f m/s", gib, VR_Dummy_Speed(inflictor));
    }

    if(inflictor.carry_use)
    {
        vr_dummy_colour = cyan;
        return sprintf("thrown box (%s), %.1f m/s", inflictor.classname, VR_Dummy_Speed(inflictor));
    }

    if(inflictor.vr_deflect_time > 0 && (attacker.flags & FL_CLIENT))
    {
        vr_dummy_colour = '0.5 1 0.5';
        return sprintf("deflected %s", inflictor.classname);
    }

    if(attacker.flags & FL_CLIENT)
    {
        return sprintf("weapon fire: %s", VR_Dummy_ProjectileName(inflictor));
    }
    return sprintf("%s of %s", VR_Dummy_ProjectileName(inflictor), attacker.classname);
};

string(float xRegion) VR_Dummy_RegionName =
{
    if(xRegion == 1) { return "head"; }
    if(xRegion == 2) { return "limb"; }
    if(xRegion == 3) { return "legs"; }
    return "body";
};

// Where it struck, in words (starting with ", "; "" when unknown); a headshot turns the number yellow.
string(entity targ, float isVRMelee) VR_Dummy_Part =
{
    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        return "";
    }

    // A shotgun blast: its pellets.
    if(vr_hitpellets > 0)
    {
        string s = sprintf(", %d pellet%s", vr_hitpellets, vr_hitpellets == 1 ? "" : "s");
        if(vr_hitpellets_head > 0)
        {
            s = sprintf("%s, %d head (x%.2f)", s, vr_hitpellets_head, PositionalDamageFactor(1));
            vr_dummy_colour = '1 0.85 0.2';
        }
        if(vr_hitpellets_limb > 0)
        {
            s = sprintf("%s, %d limb (x%.2f)", s, vr_hitpellets_limb, PositionalDamageFactor(2));
        }
        if(vr_hitpellets_leg > 0)
        {
            s = sprintf("%s, %d legs (x%.2f)", s, vr_hitpellets_leg, PositionalDamageFactor(3));
        }
        return s;
    }

    // A melee blow: only where it landed.
    if(isVRMelee || vr_hitkind == QVR_HITKIND_BASH || vr_hitkind == QVR_HITKIND_SHOVE)
    {
        if(vr_hitpos != '0 0 0' && vr_hitdir != '0 0 0')
        {
            float region = PositionalRegion(targ, vr_hitpos, vr_hitdir);
            if(region >= 0)
            {
                return sprintf(", at the %s", VR_Dummy_RegionName(region));
            }
        }
        return "";
    }

    // A shot with positional damage.
    if(vr_hitpart_target == targ && vr_hitpart_time == time && vr_hitpart >= 0)
    {
        if(vr_hitpart == 1)
        {
            vr_dummy_colour = '1 0.85 0.2';
        }
        return sprintf(", %s (x%.2f)", VR_Dummy_RegionName(vr_hitpart), PositionalDamageFactor(vr_hitpart));
    }

    return "";
};

// Where it struck: the shot's or blow's point if known, else the inflictor's, else the side facing
// the attacker; on its box.
vector(entity targ, entity inflictor, entity attacker) VR_Dummy_HitPoint =
{
    vector p;
    if(vr_hitkind != QVR_HITKIND_NONE && vr_hitpos != '0 0 0')
    {
        p = vr_hitpos;
    }
    else if(vr_hitpart_target == targ && vr_hitpart_time == time)
    {
        p = vr_hitpart_pos;
    }
    else if(inflictor != attacker && inflictor != world)
    {
        p = inflictor.origin;
    }
    else
    {
        p = attacker.origin + attacker.view_ofs;
    }

    p_x = min(targ.absmax_x, max(targ.absmin_x, p_x));
    p_y = min(targ.absmax_y, max(targ.absmin_y, p_y));
    p_z = min(targ.absmax_z, max(targ.absmin_z, p_z));
    return p;
};

// ----------------------------------------------------------------------------
// The report (from T_DamageImpl, instead of the damage)

void(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) VR_Dummy_Hit =
{
    // The callers may still need these.
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;

    string kind = VR_Dummy_Kind(inflictor, attacker, isVRMelee);
    string part = VR_Dummy_Part(targ, isVRMelee);
    vector at = VR_Dummy_HitPoint(targ, inflictor, attacker);
    float shown = rint(damage * 10) / 10;

    // The run of hits so far (less than a second apart).
    if(time - targ.vr_dummy_streak_time > 1)
    {
        targ.vr_dummy_streak_hits = 0;
        targ.vr_dummy_streak_damage = 0;
    }
    targ.vr_dummy_streak_time = time;
    targ.vr_dummy_streak_hits = targ.vr_dummy_streak_hits + 1;
    targ.vr_dummy_streak_damage = targ.vr_dummy_streak_damage + damage;
    string streak = "";
    if(targ.vr_dummy_streak_hits > 1)
    {
        streak = sprintf(" [%d hits: %g]", targ.vr_dummy_streak_hits, rint(targ.vr_dummy_streak_damage * 10) / 10);
    }

    string line = sprintf("Dummy: %g damage - %s%s%s\n", shown, kind, part, streak);
    // Monsters' blows (a grunt missing the player) only with `developer` on.
    if(attacker.flags & FL_CLIENT)
    {
        sprint(attacker, line);
    }
    else
    {
        dprint(line);
    }

    // The number, a little apart from the last one.
    at_x = at_x + crandom() * 3;
    at_y = at_y + crandom() * 3;
    floattext(at, ftos(shown), vr_dummy_colour, 0.45 + min(0.5, damage / 160));

    // Flinch: lean away from the blow, and the pain frames.
    vector dir = vr_hitdir;
    if(dir == '0 0 0')
    {
        dir = targ.origin - (inflictor != world ? (inflictor.absmin + inflictor.absmax) * 0.5 : attacker.origin);
    }
    dir_z = 0;
    dir = normalize(dir);
    makevectors(VR_Dummy_Facing(targ));
    float lean = min(15, 3 + damage * 0.25);
    targ.vr_dummy_lean_x = targ.vr_dummy_lean_x - (dir * v_forward) * lean;
    targ.vr_dummy_lean_z = targ.vr_dummy_lean_z + (dir * v_right) * lean;
    targ.vr_dummy_lean_x = min(20, max(-20, targ.vr_dummy_lean_x));
    targ.vr_dummy_lean_z = min(20, max(-20, targ.vr_dummy_lean_z));

    entity oldSelf = self;
    self = targ;
    self.th_pain(attacker, damage);
    self = oldSelf;

    // Nothing hurts it.
    targ.health = VR_DUMMY_HEALTH;
    vr_hitpart_target = world;

    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
};

// ----------------------------------------------------------------------------
// Attacks (parry practice)
//
// With vr_dummy_attacks on, it strikes at the nearest living player within vr_dummy_attack_reach (units, origin to
// origin, as a knight's reach is measured), a blow every vr_dummy_attack_period seconds, give or take
// vr_dummy_attack_jitter at random. Each blow has a wind-up of vr_dummy_attack_windup seconds first, the tell: a
// sound (vr/dummy_windup.wav), a glow, the rifle raised as it turns to face you and rears back. Then it lunges.
// The blow is a monster's melee blow: T_Damage from the dummy itself, as a knight's (ai_melee), so the parry
// (VR_Parry, combat.qc: the dummy counts as a monster that strikes in melee while its blow is dealt), parry
// stamina, the counter's window, the knockback and armour all work as in a fight, for vr_dummy_attack_damage. A
// parried blow throws it back. Stepping out of reach during the wind-up makes it miss; while nobody is within
// reach it waits, turned back to its own facing. Your hits are reported as before and don't interrupt it.
//
// Motion takes (docs/vr-port/MOTIONS.md): a take recorded with attacks on says so in its header ("dummy attacks:
// on") and has a "strike" event for each wind-up, blow and miss (sub windup, blow, miss; a blow's damage). A
// replay (vr_motion_play, vr_motion_eval) turns vr_dummy_attacks off, and the dummy strikes when the take says
// instead (VR_Dummy_Replay, called by the engine for the take's strike events): the same blows at the same moments.

.float vr_dummy_atk_phase;     // 0 idle, 1 winding up, 2 the blow's follow-through
.float vr_dummy_atk_start;     // when the phase began
.float vr_dummy_atk_at;        // winding up: when the blow comes (0: when a replay says); follow-through: when it ends
.float vr_dummy_atk_next;      // idle: when the next wind-up may begin
.float vr_dummy_atk_live;      // attacks were on at its last think
.entity vr_dummy_atk_victim;   // whom it winds up against
.float vr_dummy_replay_what;   // a replay's strike event for its next think: 1 wind-up, 2 blow, 3 miss
.float vr_dummy_replay_damage;
.vector vr_dummy_pose;         // the attack's lean (pitch): rearing back, lunging

float VR_DUMMY_SHOOT_FIRST = 81;  // shoot1 .. shoot9: the rifle raised (the wind-up), brought down (the blow)
float VR_DUMMY_FOLLOW = 0.4;      // s of the blow's follow-through
float VR_DUMMY_TURN_SPEED = 300;  // degrees a second it turns to face whom it strikes at
float VR_DUMMY_SETTLE = 0.5;      // s someone must be within reach before a wind-up begins

// The nearest living player within `xReach` of `d` (not one with notarget: as monsters do, it leaves them be).
entity(entity d, float xReach) VR_Dummy_Victim =
{
    entity best = world;
    float bestDist = xReach;
    entity p = find(world, classname, "player");
    while(p)
    {
        float dist = vlen(p.origin - d.origin);
        if(p.health > 0 && p.takedamage && !(p.flags & FL_NOTARGET) && dist <= bestDist)
        {
            best = p;
            bestDist = dist;
        }
        p = find(p, classname, "player");
    }
    return best;
};

// (The parry and the push of an unparried blow reach 150 units: beyond, it would be no melee blow.)
float() VR_Dummy_Reach =
{
    return min(150, max(16, cvar_hget(cvarh_vr_dummy_attack_reach)));
};

float() VR_Dummy_Windup =
{
    return min(2, max(0.1, cvar_hget(cvarh_vr_dummy_attack_windup)));
};

// When the wind-up after a blow at `xBlow` may begin: the period (give or take the jitter), less the wind-up.
float(float xBlow) VR_Dummy_NextWindup =
{
    float jitter = max(0, cvar_hget(cvarh_vr_dummy_attack_jitter));
    float period = cvar_hget(cvarh_vr_dummy_attack_period) + crandom() * jitter;
    return xBlow + max(VR_DUMMY_FOLLOW + 0.1, period - VR_Dummy_Windup());
};

// The wind-up begins (self: the dummy): the tell. A replay's has no time of its own for the blow.
void(entity xVictim, float xReplay) VR_Dummy_WindUp =
{
    self.vr_dummy_atk_phase = 1;
    self.vr_dummy_atk_start = time;
    self.vr_dummy_atk_at = xReplay ? 0 : time + VR_Dummy_Windup();
    self.vr_dummy_atk_victim = xVictim;
    self.effects = self.effects | EF_DIMLIGHT;
    sound(self, CHAN_WEAPON, "vr/dummy_windup.wav", 1, ATTN_NORM);
    VR_Motion_Event("strike", "windup", -1, 0, self.origin, self, "");
    dprint(sprintf("dummy: winds up%s (%.2f)\n", xReplay ? " (replay)" : sprintf(", the blow in %.2f s", VR_Dummy_Windup()), time));
};

// The blow (self: the dummy): at `xVictim` for `xDamage`, or a miss (world).
void(entity xVictim, float xDamage) VR_Dummy_Strike =
{
    self.vr_dummy_atk_phase = 2;
    self.vr_dummy_atk_start = time;
    self.vr_dummy_atk_at = time + VR_DUMMY_FOLLOW;
    self.effects = self.effects - (self.effects & EF_DIMLIGHT);
    sound(self, CHAN_WEAPON, "knight/sword1.wav", 1, ATTN_NORM);
    if(xVictim == world)
    {
        VR_Motion_Event("strike", "miss", -1, 0, self.origin, self, "");
        dprint(sprintf("dummy: misses: nobody within reach (%.2f)\n", time));
        return;
    }

    // As a monster's blow (ai_melee): the parry, its stamina and the counter's window, the knockback, armour.
    self.vr_dummy_striking = TRUE;
    self.deathtype = "was beaten by the training dummy";
    T_Damage(xVictim, self, self, xDamage);
    self.vr_dummy_striking = FALSE;
    self.deathtype = NULLSTR;
    float parried = vr_parried;
    string how = "";
    if(parried == 1)
    {
        how = "parried";
    }
    else if(parried == 2)
    {
        how = "parried with the arms";
    }
    VR_Motion_Event("strike", "blow", -1, xDamage, self.origin, self, how);
    dprint(sprintf("dummy: strikes %s for %g, %.0f units away%s (%.2f)\n", xVictim.netname, xDamage,
        vlen(xVictim.origin - self.origin), how != "" ? sprintf(": %s", how) : "", time));
    if(parried)
    {
        // Thrown back by the parry (a lean, as a hit's).
        self.vr_dummy_lean_x = min(20, self.vr_dummy_lean_x + 14);
    }
};

// A replay's strike event (see above), at the dummy's think: as the take had it.
void() VR_Dummy_ReplayEvent =
{
    float what = self.vr_dummy_replay_what;
    self.vr_dummy_replay_what = 0;
    entity p = VR_Dummy_Victim(self, 100000);
    if(what == 1)
    {
        VR_Dummy_WindUp(p, TRUE);
    }
    else if(what == 2 && p != world)
    {
        self.vr_dummy_atk_victim = p;
        VR_Dummy_Strike(p, self.vr_dummy_replay_damage);
    }
    else
    {
        VR_Dummy_Strike(world, 0);
    }
};

// The engine, for a take's strike event (vr_motion_play; self: the take's target, xWhat: 1 wind-up, 2 blow, 3 miss):
// done at the dummy's think in the coming server frame, after the player's, as when it was recorded.
void(float xWhat, float xDamage) VR_Dummy_Replay =
{
    if(self.classname != "vr_dummy")
    {
        return;
    }
    if(self.vr_dummy_replay_what)
    {
        VR_Dummy_ReplayEvent(); // (two before a think: the first now)
    }
    self.vr_dummy_replay_what = xWhat;
    self.vr_dummy_replay_damage = xDamage;
    self.nextthink = time;
};

// Once a think (self: the dummy), while attacks are on or something of them is left.
void() VR_Dummy_Attack =
{
    if(self.vr_dummy_replay_what)
    {
        VR_Dummy_ReplayEvent();
    }

    float live = cvar_hget(cvarh_vr_dummy_attacks) != 0;
    if(!live)
    {
        if(self.vr_dummy_atk_live)
        {
            // Turned off: a wind-up of its own stops (a replay's goes on).
            self.vr_dummy_atk_live = FALSE;
            if(self.vr_dummy_atk_phase == 1 && self.vr_dummy_atk_at)
            {
                self.vr_dummy_atk_phase = 0;
                self.effects = self.effects - (self.effects & EF_DIMLIGHT);
            }
        }
    }
    else if(!self.vr_dummy_atk_live)
    {
        self.vr_dummy_atk_live = TRUE;
        self.vr_dummy_atk_next = time + 1; // the first wind-up a second after it's turned on
    }

    float reach = VR_Dummy_Reach();
    entity near = world;
    if(live)
    {
        near = VR_Dummy_Victim(self, reach);
    }

    if(self.vr_dummy_atk_phase == 1 && self.vr_dummy_atk_at)
    {
        if(time >= self.vr_dummy_atk_at)
        {
            // Whom it wound up against, if still within reach: else a miss.
            entity v = self.vr_dummy_atk_victim;
            if(v.health <= 0 || !v.takedamage || vlen(v.origin - self.origin) > reach)
            {
                v = world;
            }
            VR_Dummy_Strike(v, max(0, cvar_hget(cvarh_vr_dummy_attack_damage)));
            self.vr_dummy_atk_next = VR_Dummy_NextWindup(time);
        }
        else
        {
            self.nextthink = min(self.nextthink, self.vr_dummy_atk_at); // the blow on time
        }
    }
    else if(self.vr_dummy_atk_phase == 2)
    {
        if(time >= self.vr_dummy_atk_at)
        {
            self.vr_dummy_atk_phase = 0;
        }
    }
    else if(self.vr_dummy_atk_phase == 0 && live)
    {
        if(near == world)
        {
            self.vr_dummy_atk_next = max(self.vr_dummy_atk_next, time + VR_DUMMY_SETTLE);
        }
        else if(time >= self.vr_dummy_atk_next)
        {
            VR_Dummy_WindUp(near, FALSE);
        }
    }

    // Turning to face whom it strikes at (during an attack, or someone within reach), else back.
    float turn = 0;
    entity face = near;
    if(self.vr_dummy_atk_phase)
    {
        face = self.vr_dummy_atk_victim;
    }
    if(face != world)
    {
        turn = anglemod(vectoyaw(face.origin - self.origin) - self.vr_dummy_angles_y + 180) - 180;
    }
    if(turn != self.vr_dummy_turn)
    {
        float step = VR_DUMMY_TURN_SPEED * 0.05;
        float d = anglemod(turn - self.vr_dummy_turn + 180) - 180;
        if(fabs(d) <= step)
        {
            self.vr_dummy_turn = turn;
        }
        else
        {
            self.vr_dummy_turn = self.vr_dummy_turn + (d > 0 ? step : -step);
        }
    }

    // The pose: rearing back through the wind-up, lunging at the blow, back up in the follow-through.
    float u;
    if(self.vr_dummy_atk_phase == 1)
    {
        u = min(1, (time - self.vr_dummy_atk_start) / VR_Dummy_Windup());
        self.vr_dummy_pose_x = 12 * u;
    }
    else if(self.vr_dummy_atk_phase == 2)
    {
        u = min(1, (time - self.vr_dummy_atk_start) / VR_DUMMY_FOLLOW);
        if(u < 0.25)
        {
            self.vr_dummy_pose_x = -22 * (u / 0.25);
        }
        else
        {
            self.vr_dummy_pose_x = -22 * (1 - (u - 0.25) / 0.75);
        }
    }
    else
    {
        self.vr_dummy_pose_x = 0;
    }
};

// Its frame during an attack: the rifle raised through the wind-up (shoot1-4), brought down in the blow (shoot5-9).
float() VR_Dummy_AttackFrame =
{
    float u;
    if(self.vr_dummy_atk_phase == 1)
    {
        u = min(1, (time - self.vr_dummy_atk_start) / VR_Dummy_Windup());
        return VR_DUMMY_SHOOT_FIRST + min(3, floor(u * 4));
    }
    u = min(1, (time - self.vr_dummy_atk_start) / VR_DUMMY_FOLLOW);
    return VR_DUMMY_SHOOT_FIRST + 4 + min(4, floor(u * 5));
};

// ----------------------------------------------------------------------------
// The dummy itself

void(entity attacker, float damage) vr_dummy_pain =
{
    self.vr_dummy_painframe = VR_DUMMY_PAIN_FIRST;
    if(self.pain_finished < time)
    {
        sound(self, CHAN_VOICE, random() < 0.5 ? "soldier/pain1.wav" : "soldier/pain2.wav", 1, ATTN_NORM);
        self.pain_finished = time + 0.6;
    }
};

// Never killed (VR_Dummy_Hit takes no health); should anything still kill it, it gets up.
void() vr_dummy_die =
{
    self.health = VR_DUMMY_HEALTH;
    self.takedamage = DAMAGE_AIM;
};

void() vr_dummy_think =
{
    self.nextthink = time + 0.05;

    // Its attacks (off: nothing).
    if(self.vr_dummy_atk_phase || self.vr_dummy_atk_live || self.vr_dummy_turn || self.vr_dummy_replay_what ||
       cvar_hget(cvarh_vr_dummy_attacks))
    {
        VR_Dummy_Attack();
    }

    // An attack's frames, else its pain frames, else standing (at 10 frames a second).
    if(self.vr_dummy_atk_phase)
    {
        self.frame = VR_Dummy_AttackFrame();
        self.vr_dummy_painframe = 0; // (a hit's lean still shows)
    }
    else if(self.vr_dummy_painframe)
    {
        self.frame = self.vr_dummy_painframe;
        self.vr_dummy_painframe = self.vr_dummy_painframe + 1;
        if(self.vr_dummy_painframe > VR_DUMMY_PAIN_LAST)
        {
            self.vr_dummy_painframe = 0;
        }
    }
    else
    {
        float f = floor(time * 10);
        self.frame = VR_DUMMY_STAND_FIRST + f - floor(f / 8) * 8;
    }

    // Straightens up.
    self.vr_dummy_lean = self.vr_dummy_lean * 0.8;
    if(vlen(self.vr_dummy_lean) < 0.3)
    {
        self.vr_dummy_lean = '0 0 0';
    }
    self.angles = VR_Dummy_Facing(self) + self.vr_dummy_lean + self.vr_dummy_pose;

    // Pushes (vr_hit_push, vr_melee_push, a bash) move nothing: it stays on its spot.
    self.velocity = '0 0 0';
    if(self.origin != self.vr_dummy_home)
    {
        setorigin(self, self.vr_dummy_home);
    }
};

// Once the world is in: stand on the floor, and put up its sign.
void() vr_dummy_start =
{
    self.origin_z = self.origin_z + 1;
    droptofloor();
    self.vr_dummy_home = self.origin;

    float wth = worldtext_hmake();
    worldtext_hsetpos(wth, self.origin + '0 0 44');
    worldtext_hsetangles(wth, self.vr_dummy_angles + '0 180 0');
    worldtext_hsethalign(wth, 1);
    worldtext_hsetscale(wth, 0.3);
    worldtext_hsettext(wth, "Training dummy\n(damage in the console)");

    self.think = vr_dummy_think;
    self.nextthink = time + 0.05;
};

/*QUAKED vr_dummy (1 0.5 0) (-12 -12 -24) (12 12 27)
Quake VR: a training dummy (a grunt) that cannot be hurt and reports every hit: the damage, the kind
of blow and where it struck, in the attacker's console and as numbers rising from it. Not counted as
a monster.
*/
void() vr_dummy =
{
    monster_army_precache();
    precache_sound("vr/dummy_windup.wav"); // its attacks (vr_dummy_attacks)
    precache_sound("knight/sword1.wav");

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_NONE;
    setmodel(self, "progs/soldier.mdl");
    setsize(self, '-12 -12 -24', '12 12 27');

    self.health = VR_DUMMY_HEALTH;
    self.max_health = VR_DUMMY_HEALTH;
    self.takedamage = DAMAGE_AIM;
    self.flags = self.flags | FL_MONSTER; // a target like a monster (bashes, throw aim); not counted
    self.netname = "training dummy";
    self.th_pain = vr_dummy_pain;
    self.th_die = vr_dummy_die;
    self.vr_dummy_angles = self.angles;
    self.frame = VR_DUMMY_STAND_FIRST;

    self.think = vr_dummy_start;
    self.nextthink = time + 0.1 + random() * 0.1;
};

// ----------------------------------------------------------------------------
// The firing range's "dummy attacks" button

.float vr_dummy_label;       // the label's worldtext handle
.float vr_dummy_label_shown; // what it shows: 0 off, 1 on (-1: nothing yet)

// The label, and the button lit while attacks are on (vr_dummy_attacks: the console's too).
void() vr_dummy_attack_toggle_show =
{
    float on = cvar_hget(cvarh_vr_dummy_attacks) != 0;
    if(on != self.vr_dummy_label_shown)
    {
        self.vr_dummy_label_shown = on;
        worldtext_hsettext(self.vr_dummy_label, on ? "dummy\nattacks\non" : "dummy\nattacks\noff");
    }
    // (Its pressed textures also as it comes back out: button_return turns them off.)
    entity b = self.enemy;
    if(b != world && (b.state == STATE_BOTTOM || b.state == STATE_DOWN) && b.frame != on)
    {
        b.frame = on;
    }
};

void() vr_dummy_attack_toggle_think =
{
    if(self.enemy == world)
    {
        self.enemy = find(world, target, self.targetname);
    }
    vr_dummy_attack_toggle_show();
    // Every frame while the button moves (after its think: this entity comes after it), else ten times a second.
    self.nextthink = time + ((self.enemy != world && self.enemy.state != STATE_BOTTOM) ? 0.01 : 0.1);
};

void() vr_dummy_attack_toggle_use =
{
    float on = cvar_hget(cvarh_vr_dummy_attacks) == 0;
    cvar_set("vr_dummy_attacks", on ? "1" : "0");
    vr_dummy_attack_toggle_show();
    if(!(activator.flags & FL_CLIENT))
    {
        return;
    }
    if(!on)
    {
        centerprint(activator, "Dummy attacks OFF");
        return;
    }
    centerprint(activator, "Dummy attacks ON");
    sprint(activator, sprintf("Dummy attacks on: a blow of %g every %g s (give or take %g) within %g units, after a %g s wind-up (Gameplay > Parry, Bash and Headbutt)\n",
        cvar_hget(cvarh_vr_dummy_attack_damage), cvar_hget(cvarh_vr_dummy_attack_period),
        cvar_hget(cvarh_vr_dummy_attack_jitter), VR_Dummy_Reach(), VR_Dummy_Windup()));
};

/*QUAKED vr_dummy_attack_toggle (1 0.5 0) (-8 -8 -8) (8 8 8)
Quake VR: the switch of the training dummies' attacks (vr_dummy_attacks: parry practice). Used by the button whose
target it is, it turns them on or off, and it shows which on a label at its origin ("angle": the way the label
faces, as a func_worldtext_banner's; "worldtext_scale", 0.75 if not given). The button stays lit while they're on.
Not saved: off at every map load.
*/
void() vr_dummy_attack_toggle =
{
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.use = vr_dummy_attack_toggle_use;
    self.vr_dummy_label = worldtext_hmake();
    self.vr_dummy_label_shown = -1;
    worldtext_hsetpos(self.vr_dummy_label, self.origin);
    worldtext_hsetangles(self.vr_dummy_label, self.angles + '0 180 0');
    worldtext_hsethalign(self.vr_dummy_label, 1);
    worldtext_hsetscale(self.vr_dummy_label, self.worldtext_scale ? self.worldtext_scale : 0.75);
    vr_dummy_attack_toggle_show();
    self.think = vr_dummy_attack_toggle_think;
    self.nextthink = time + 0.1;
};
