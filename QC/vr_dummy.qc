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
        return sprintf(vr_bash_counter ? "parry-bash with the %s" : "bash with the %s",
                       WeaponIdToWeaponName(VRGetEntWeapon(attacker, vr_hithand)));
    }

    if(vr_hitkind == QVR_HITKIND_SHOVE)
    {
        vr_dummy_colour = orange;
        return vr_hithand >= 0 ? sprintf("shove with the %s", VR_Dummy_HandName(vr_hithand)) : "shove with both hands";
    }

    if(vr_hitkind == QVR_HITKIND_MELEE)
    {
        vr_dummy_colour = orange;
        float wid = VRGetEntWeapon(attacker, vr_hithand);
        entity oldSelf = self;
        self = attacker;
        entity b = VR_Blow(vr_hithand);
        self = oldSelf;
        // What struck, how (the blow's kind), how fast and far the wrist went (and a swung weapon's
        // head), how suddenly (the wrist's mean acceleration to its fastest), and the strength.
        string what = VR_Blow_KindName(b.blow_kind);
        string speed = sprintf("wrist %.1f m/s", b.blow_peak);
        if(wid == WID_FIST)
        {
            if(VR_Carry_IsHolding(attacker, vr_hithand))
            {
                what = sprintf("%s with a box in hand", what);
            }
        }
        else if(wid == WID_AXE || wid == WID_SWORD || wid == WID_MJOLNIR)
        {
            what = sprintf("%s, %s", WeaponIdToWeaponName(wid), what);
            speed = sprintf("wrist %.1f m/s, head %.1f m/s", b.blow_peak, b.blow_tippeak);
        }
        else
        {
            what = sprintf("%s as a club, %s", WeaponIdToWeaponName(wid), what);
        }
        if(b.blow_hit_stroke == b.blow_t0 && b.blow_hit_name != "")
        {
            what = sprintf("%s with %s", what, b.blow_hit_name); // the striking point (vr_juice.qc)
        }
        return sprintf("melee: %s, %s, %s over %.0f cm, %.0f m/s2 (x%.2f)", what, VR_Dummy_HandName(vr_hithand),
            speed, b.blow_dist * 100, b.blow_accel, b.blow_strength);
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
    makevectors(targ.vr_dummy_angles);
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

    // Its pain frames, else standing (at 10 frames a second).
    if(self.vr_dummy_painframe)
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
    self.angles = self.vr_dummy_angles + self.vr_dummy_lean;

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
