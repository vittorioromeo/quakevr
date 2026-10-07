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
// It flinches (its pain frames, a lean away from the blow) but is not knocked away, and is no monster of the level (not
// counted; never killed, unless vr_dummy_gib). It bleeds as a grunt does (vr_dummy_gore, on: the gore's sprays and mist,
// wounds on its model, small gibs, blood thrown onto you and what you hold, burns; NOTES.md
// vrfiringrange_2026-10-03_02-07-05). Its hits take its health (vr_dummy_health: a grunt's 30 by default), shown on a
// board over its head (vr_dummy_healthbar), which fills up again vr_dummy_regen seconds after its last hit. With
// vr_dummy_gib on (the default), the hit that takes the last of it kills it as a grunt (VR_Dummy_Die; NOTES.md
// vrfiringrange, 2026-10-06 12:28): it dies, is beheaded, has its head popped or is gibbed exactly as a grunt would be,
// its body lies as a grunt's, and it stands again vr_dummy_gib_respawn seconds later; off, it stays at 0 ("would kill").
// Attacks that their inflictor does not tell apart are tagged by their code (VR_SetHitKind, combat.qc).
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
.float vr_dummy_hp;          // its health now (vr_dummy_health): what its hits take away, filled up again vr_dummy_regen s
.float vr_dummy_hpmax;       // after the last; its full health as last set,
.float vr_dummy_own_health;  // and its enemy's own (its spawn function's: a grunt's 30; vr_dummy_health -1)
.float vr_dummy_hit_time;    // its last hit
.float vr_dummy_board;       // its board over its head (the health; 1 + the worldtext handle, 0 none yet): made once, kept
                             // by its stand-in while it is dead
.float vr_dummy_board_hp;    // what the board shows: the health (-1 nothing yet, -2 dead, -3 its sign: vr_dummy_healthbar 0)
.float vr_dummy_board_max;   // of that full health,
.float vr_dummy_board_yaw;   // the way it faces,
.float vr_dummy_board_z;     // and its height

// Its enemy (vr_dummy_type: the monster it stands for, made by that monster's own spawn function; vr_dummy_types.qc).
.float vr_dummy_kind;        // 1 + its enemy while it stands (VR_IsDummy); 0 otherwise (its body once killed, all else)
.float vr_dummy_enemy;       // its enemy (VR_Dummy_Type's number), its stand-in's too while it is dead
.void() vr_dummy_th_die;     // its enemy's death and pain (its own while it stands: VR_Dummy_Die gives them back)
.void(entity attacker, float damage) vr_dummy_th_pain;
.float vr_dummy_movetype;    // its enemy's movetype (it stands still: MOVETYPE_NONE)
.float vr_dummy_floor;       // the floor it stands on (its box's bottom: its enemies' boxes differ)
float VR_DUMMY_TYPES = 52;   // the enemies (VR_Dummy_Type: 0 grunt .. 18 electric eel; Dawn of the Machine's 19 rocket ogre, 20 demo dog,
                             // 21 ranged knight; from 30: 30 orb, 31 slime, 32 lava man, 33 super shambler; from 50: 50 Shub,
                             // 51 Shub's eye; the numbers between: none yet, a grunt)

// An enemy's row (VR_Dummy_Type, vr_dummy_types.qc): its classname, its name, its pack (0 Quake, 1 Scourge of Armagon,
// 2 Dissolution of Eternity, 3 Dawn of the Machine), the first frame and the count of its stand loop, its pain animation and an attack (the
// wind-up and the blow of vr_dummy_attacks), its pain sounds.
string vr_dt_class;
string vr_dt_name;
float vr_dt_pack;
float vr_dt_stand, vr_dt_stands;
float vr_dt_pain, vr_dt_pains;
float vr_dt_atk, vr_dt_atks;
string vr_dt_snd1, vr_dt_snd2;

float VR_DUMMY_HEALTH = 1000;     // its .health while it stands (VR_Dummy_Hit takes its hits from vr_dummy_hp)
float VR_DUMMY_REFILL = 0.4;      // s its health takes to fill up again, from nothing (vr_dummy_regen)
float VR_DUMMY_BAR_CELLS = 12;    // its board's bar

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
    if(c == "vr_burn")           { return "fire"; }
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

    if(inflictor.vr_grenade && inflictor.vr_gren_set && !inflictor.vr_deflect_time && (attacker.flags & FL_CLIENT))
    {
        vr_dummy_colour = '0.5 1 0.5';
        return sprintf("caught and thrown %s", inflictor.classname); // (vr_grenade.qc; before a thrown box's)
    }

    if(inflictor.vr_gib)
    {
        vr_dummy_colour = cyan;
        // A head is the monster itself (ThrowHead), still named after it.
        string gib = substring(inflictor.classname, 0, 8) == "monster_" ? "head" : "gib";
        return sprintf("thrown %s, %.1f m/s", gib, VR_Dummy_Speed(inflictor));
    }

    if(inflictor.carry_use && !inflictor.vr_grenade)
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

    // A melee blow: only where it landed (and its zone's multiplier, with vr_melee_positional: VR_Melee_Positional
    // records it as a shot's PositionalDamage does).
    if(isVRMelee || vr_hitkind == QVR_HITKIND_BASH || vr_hitkind == QVR_HITKIND_SHOVE)
    {
        if(vr_hitpart_target == targ && vr_hitpart_time == time && vr_hitpart >= 0)
        {
            if(vr_hitpart == 1)
            {
                vr_dummy_colour = '1 0.85 0.2';
            }
            return sprintf(", %s (x%.2f)", VR_Dummy_RegionName(vr_hitpart), PositionalDamageFactor(vr_hitpart));
        }
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
// Its health (vr_dummy_health, vr_dummy_regen) and the board over its head (vr_dummy_healthbar)

// Its full health: vr_dummy_health, or its enemy's own (below 1: -1, the default).
float(entity d) VR_Dummy_FullHealth =
{
    float h = cvar_hget(cvarh_vr_dummy_health);
    return h < 1 ? d.vr_dummy_own_health : rint(h); // (the menu: its leftmost step, Its Own, -1 or 0)
};

// Its full health as set now: changed (the menu), it is full again.
void(entity d) VR_Dummy_Health =
{
    float full = VR_Dummy_FullHealth(d);
    if(full != d.vr_dummy_hpmax)
    {
        d.vr_dummy_hpmax = full;
        d.vr_dummy_hp = full;
    }
};

// What it is called on its board: its enemy's name.
string(entity d) VR_Dummy_Name =
{
    VR_Dummy_Type(d.vr_dummy_enemy);
    return vr_dt_name;
};

// Whether `e` is a training dummy standing (not its body once killed, nor its stand-in).
float(entity e) VR_IsDummy =
{
    return e.vr_dummy_kind > 0;
};

nosave float vr_dummy_warned; // 1 + the enemy whose pack is missing, last said

// The enemy it should be (vr_dummy_type), a grunt for one not here (its mission pack's files: VR_Dummy_Available).
float() VR_Dummy_Wanted =
{
    float t = rint(cvar_hget(cvarh_vr_dummy_type));
    if(t < 0 || t >= VR_DUMMY_TYPES)
    {
        t = 0;
    }
    if(!VR_Dummy_Available(t))
    {
        if(vr_dummy_warned != t + 1)
        {
            vr_dummy_warned = t + 1;
            VR_Dummy_Type(t);
            bprint(sprintf("Training dummy: the %s needs %s; a grunt instead\n", vr_dt_name,
                vr_dt_pack == 1 ? "Scourge of Armagon" : (vr_dt_pack == 3 ? "Dawn of the Machine" : "Dissolution of Eternity")));
        }
        t = 0;
    }
    return t;
};

// `xFrac` of the bar filled: solid cells, then dim ones.
string(float xFrac) VR_Dummy_Bar =
{
    float n = xFrac > 0 ? max(1, rint(xFrac * VR_DUMMY_BAR_CELLS)) : 0;
    string s = "";
    for(float i = 0; i < VR_DUMMY_BAR_CELLS; i++)
    {
        float c = 139;
        if(i < n)
        {
            c = 11;
        }
        s = strcat(s, sprintf("%c", c));
    }
    return s;
};

// Its board (made the first time), over its head and turned to face the nearest player; `xDead` while it is dead (`d` its
// stand-in). Sent only as it changes.
void(entity d, float xDead) VR_Dummy_ShowBoard =
{
    if(!d.vr_dummy_board)
    {
        d.vr_dummy_board = 1 + worldtext_hmake();
        worldtext_hsethalign(d.vr_dummy_board - 1, 1);
        worldtext_hsetscale(d.vr_dummy_board - 1, 0.3);
        d.vr_dummy_board_hp = -1;
        d.vr_dummy_board_z = -100000;
        d.vr_dummy_board_yaw = -1000;
    }
    float wth = d.vr_dummy_board - 1;

    float shown = ceil(d.vr_dummy_hp);
    if(xDead)
    {
        shown = -2;
    }
    else if(!cvar_hget(cvarh_vr_dummy_healthbar))
    {
        shown = -3;
    }
    if(shown != d.vr_dummy_board_hp || (shown >= 0 && d.vr_dummy_hpmax != d.vr_dummy_board_max))
    {
        d.vr_dummy_board_hp = shown;
        d.vr_dummy_board_max = d.vr_dummy_hpmax;
        string text;
        if(shown == -2)
        {
            text = sprintf("%s\nkilled\n(stands again)", VR_Dummy_Name(d));
        }
        else if(shown == -3)
        {
            text = sprintf("Training dummy: %s\n(damage in the console)", VR_Dummy_Name(d));
        }
        else
        {
            text = sprintf("%s\n%s\n%g / %g", VR_Dummy_Name(d), VR_Dummy_Bar(d.vr_dummy_hp / d.vr_dummy_hpmax), shown,
                d.vr_dummy_hpmax);
        }
        worldtext_hsettext(wth, text);
    }

    // Over its head (a grunt's: 44 units over its origin; where it stood while it is dead).
    float z = d.vr_dummy_home_z + d.maxs_z + 17; // (a stand-in has its box)
    if(z != d.vr_dummy_board_z)
    {
        d.vr_dummy_board_z = z;
        vector at = d.vr_dummy_home;
        at_z = z;
        worldtext_hsetpos(wth, at);
    }

    // Facing the nearest player (its own facing without one), turned a few degrees at a time.
    float yaw = d.vr_dummy_angles_y;
    float best = 100000;
    for(entity p = find(world, classname, "player"); p != world; p = find(p, classname, "player"))
    {
        vector to = p.origin - d.vr_dummy_home;
        to_z = 0;
        if(vlen(to) < best && vlen(to) > 1)
        {
            best = vlen(to);
            yaw = vectoyaw(to);
        }
    }
    float off = anglemod(yaw - d.vr_dummy_board_yaw);
    if(off > 4 && off < 356)
    {
        d.vr_dummy_board_yaw = yaw;
        worldtext_hsetangles(wth, '0 1 0' * (yaw + 180));
    }
};

// Every think while it stands: its health fills up again vr_dummy_regen s after its last hit; the board.
void(entity d) VR_Dummy_HealthThink =
{
    VR_Dummy_Health(d);
    float regen = cvar_hget(cvarh_vr_dummy_regen);
    if(d.vr_dummy_hp < d.vr_dummy_hpmax && regen > 0 && time - d.vr_dummy_hit_time >= regen)
    {
        d.vr_dummy_hp = min(d.vr_dummy_hpmax, d.vr_dummy_hp + d.vr_dummy_hpmax * 0.05 / VR_DUMMY_REFILL);
    }
    VR_Dummy_ShowBoard(d, FALSE);
};

// ----------------------------------------------------------------------------
// The report (from T_DamageImpl, instead of the damage)

void(entity d, float xHealth) VR_Dummy_Die;

// TRUE: the blow kills it (vr_dummy_gib): it is now the grunt it stands for (VR_Dummy_Die), and the caller deals the blow
// to it as to a grunt.
float(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) VR_Dummy_Hit =
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

    // Its health (vr_dummy_health; VR_Dummy_Health) takes the blow whole, as T_Damage takes it. vr_dummy_gib: killed as
    // its enemy is (alive before the hit, 0 or below after it); off, it stays at 0 until it fills up again.
    // Its .health set lower (the decapitation tests' health 1: vr_decap_test) is its health for this blow.
    VR_Dummy_Health(targ);
    float gruntLeft = targ.vr_dummy_hp;
    if(targ.health > 0 && targ.health < VR_DUMMY_HEALTH)
    {
        gruntLeft = min(gruntLeft, targ.health);
    }
    float lethal = gruntLeft > 0 && gruntLeft - ceil(damage) <= 0;
    float killed = cvar_hget(cvarh_vr_dummy_gib) && lethal;
    targ.vr_dummy_hp = max(0, gruntLeft - ceil(damage));
    targ.vr_dummy_hit_time = time;

    // A blade's blow at its head or neck (or one that would behead): whether it would behead a grunt (VR_Decap_Judge).
    string decap = "";
    if(isVRMelee && vr_hitkind == QVR_HITKIND_MELEE && (attacker.flags & FL_CLIENT))
    {
        entity oldSelf2 = self;
        self = attacker;
        float judged = VR_Decap_Judge(targ, vr_hithand);
        self = oldSelf2;
        if(judged > 0 || (judged == 0 && vr_decap_judge_zone))
        {
            decap = sprintf(" - %s (%s)", judged ? "would behead" : "would not behead", vr_decap_judge_why);
        }
    }
    string hpText = sprintf(" - health %g/%g%s", targ.vr_dummy_hp, targ.vr_dummy_hpmax,
        killed ? " - killed" : (lethal ? " - would kill" : ""));
    string line = sprintf("Dummy: %g damage - %s%s%s%s%s\n", shown, kind, part, streak, hpText, decap);
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

    if(killed)
    {
        VR_Dummy_Die(targ, gruntLeft);
        v_forward = oldForward;
        v_right = oldRight;
        v_up = oldUp;
        return TRUE;
    }

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
    return FALSE;
};

// ----------------------------------------------------------------------------
// Debug: Show Damage Numbers (vr_debug_damage_numbers; vrfiringrange_2026-10-01_11-41-53)
//
// Every hit on anything that takes damage, in normal play, as the dummy reports its own: the number floating where it
// struck (never over a player: it would be in your face) and the dummy's line in the console, to the player who dealt
// it or took it (else with `developer`). From T_DamageDeal: `xTake` as taken, after armour (`xSaved` what armour took);
// a gib's, a crate piece's or a corpse's hit as dealt (`xSaved` -1: their own damage follows).

void(entity targ, entity inflictor, entity attacker, float xTake, float xSaved, float isVRMelee) VR_DamageNumber =
{
    if(!cvar_hget(cvarh_vr_debug_damage_numbers) || VR_IsDummy(targ))
    {
        return;
    }

    // The callers may still need these.
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;

    string kind = VR_Dummy_Kind(inflictor, attacker, isVRMelee);
    string part = VR_Dummy_Part(targ, isVRMelee);
    vector at = VR_Dummy_HitPoint(targ, inflictor, attacker);
    float shown = rint(xTake * 10) / 10;

    string what = targ.classname;
    string state = "";
    if(targ.flags & FL_CLIENT)
    {
        what = targ == attacker ? "yourself" : "you";
    }
    if(xSaved < 0)
    {
        if(targ.vr_corpse == 2 || targ.vr_corpse_dying)
        {
            what = sprintf("corpse of %s", targ.classname);
        }
        else if(targ.vr_cratekind == 3)
        {
            what = "crate piece";
        }
        else if(targ.vr_gib)
        {
            what = substring(targ.classname, 0, 8) == "monster_" ? "head" : "gib";
        }
    }
    else
    {
        state = sprintf(" (%g left%s)", rint((targ.health - xTake) * 10) / 10,
            xSaved > 0 ? sprintf(", armour took %g", xSaved) : "");
    }

    string line = sprintf("Damage: %g to %s%s - %s%s\n", shown, what, state, kind, part);
    if(attacker.flags & FL_CLIENT)
    {
        sprint(attacker, line);
    }
    if((targ.flags & FL_CLIENT) && targ != attacker)
    {
        sprint(targ, line);
    }
    if(!(attacker.flags & FL_CLIENT) && !(targ.flags & FL_CLIENT))
    {
        dprint(line); // monsters' blows on each other, a trap's
    }

    if(!(targ.flags & FL_CLIENT))
    {
        // The number, a little apart from the last one.
        at_x = at_x + crandom() * 3;
        at_y = at_y + crandom() * 3;
        floattext(at, ftos(shown), vr_dummy_colour, 0.45 + min(0.5, xTake / 160));
    }

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
    VR_SoundVaried(self, CHAN_WEAPON, "knight/sword1.wav", 1, ATTN_NORM);
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
    if(!VR_IsDummy(self))
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

// Its frame during an attack (its enemy's: VR_Dummy_Type loaded): the first 4/9 of its attack through the wind-up (a
// grunt's rifle raised, shoot1-4), the rest in the blow (brought down, shoot5-9).
float() VR_Dummy_AttackFrame =
{
    float wind = max(1, floor(vr_dt_atks * 4 / 9));
    float blow = max(1, vr_dt_atks - wind);
    float u;
    if(self.vr_dummy_atk_phase == 1)
    {
        u = min(1, (time - self.vr_dummy_atk_start) / VR_Dummy_Windup());
        return vr_dt_atk + min(wind - 1, floor(u * wind));
    }
    u = min(1, (time - self.vr_dummy_atk_start) / VR_DUMMY_FOLLOW);
    return vr_dt_atk + min(vr_dt_atks - 1, wind + min(blow - 1, floor(u * blow)));
};

// ----------------------------------------------------------------------------
// The dummy itself

// Its enemy's pain animation (vr_dummy_painframe: its next frame, from 1) and one of its pain sounds.
void(entity attacker, float damage) vr_dummy_pain =
{
    VR_Dummy_Type(self.vr_dummy_enemy);
    self.vr_dummy_painframe = vr_dt_pains > 0;
    if(self.pain_finished < time)
    {
        string snd = (random() < 0.5 || vr_dt_snd2 == "") ? vr_dt_snd1 : vr_dt_snd2;
        if(snd != "")
        {
            sound(self, CHAN_VOICE, snd, 1, ATTN_NORM);
        }
        self.pain_finished = time + 0.6;
    }
};

// Never killed (VR_Dummy_Hit takes no health; a blow that would kill a grunt makes it one first: VR_Dummy_Die); should
// anything still kill it, it gets up.
void() vr_dummy_die =
{
    self.health = VR_DUMMY_HEALTH;
    self.takedamage = DAMAGE_AIM;
};

float(entity d, float xType, float xForce) VR_Dummy_Retype;

void() vr_dummy_think =
{
    self.nextthink = time + 0.05;

    // Another enemy chosen (vr_dummy_type): it stands as that one now (not in the middle of a blow; not while someone
    // stands where the new one would).
    float wanted = VR_Dummy_Wanted();
    if(wanted != self.vr_dummy_enemy && !self.vr_dummy_atk_phase && VR_Dummy_Retype(self, wanted, FALSE))
    {
        return; // (self is gone)
    }

    VR_Dummy_HealthThink(self);
    VR_Dummy_Type(self.vr_dummy_enemy); // (its frames)

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
        self.frame = vr_dt_pain + self.vr_dummy_painframe - 1;
        self.vr_dummy_painframe = self.vr_dummy_painframe + 1;
        if(self.vr_dummy_painframe > vr_dt_pains)
        {
            self.vr_dummy_painframe = 0;
        }
    }
    else
    {
        float f = floor(time * 10);
        self.frame = vr_dt_stand + f - floor(f / vr_dt_stands) * vr_dt_stands;
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

// Standing as its enemy `xType` (VR_Dummy_Type), as placed, as it stands again after it was killed (vr_dummy_gib) and as
// another enemy is chosen (vr_dummy_type): `self` a new entity (the map's, its stand-in). The enemy's own spawn function
// makes it (VR_Dummy_Make): its model, box, health, death and everything else of its own, its classname with them (the
// hit zones, the head it loses, its gore, its hull against you, its ragdoll: all by its classname). Then what would move
// it or let it fight is taken away (its AI never starts): it stands still, takes no harm and reports.
void(float xType) VR_Dummy_Setup =
{
    if(!VR_Dummy_Make(xType))
    {
        return; // (cannot be: VR_Dummy_Wanted's are available)
    }
    self.vr_dummy_enemy = xType;
    self.vr_dummy_kind = xType + 1;
    self.vr_dummy_th_die = self.th_die;
    self.vr_dummy_th_pain = self.th_pain;
    self.vr_dummy_movetype = self.movetype;
    self.vr_dummy_own_health = self.health;

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_NONE;
    self.health = VR_DUMMY_HEALTH;
    self.max_health = VR_DUMMY_HEALTH;
    self.vr_dummy_hpmax = VR_Dummy_FullHealth(self);
    self.vr_dummy_hp = self.vr_dummy_hpmax;
    self.takedamage = DAMAGE_AIM;
    self.flags = self.flags | FL_MONSTER; // a target like a monster (bashes, throw aim); not counted
    self.view_ofs = '0 0 25';
    self.netname = "training dummy";
    self.th_pain = vr_dummy_pain;
    self.th_die = vr_dummy_die;
    self.frame = vr_dt_stand;
    self.think = vr_dummy_think;
    self.nextthink = time + 0.05;
};

// On its floor (its enemy's box's bottom there), facing as placed.
void() VR_Dummy_Place =
{
    self.vr_dummy_home = self.origin;
    self.vr_dummy_home_z = self.vr_dummy_floor - self.mins_z;
    self.angles = self.vr_dummy_angles;
    setorigin(self, self.vr_dummy_home);
};

// What its stand-in keeps of it: where it stands and its board.
void(entity n, entity d) VR_Dummy_Keep =
{
    n.vr_dummy_floor = d.vr_dummy_floor;
    n.vr_dummy_home = d.vr_dummy_home;
    n.vr_dummy_angles = d.vr_dummy_angles;
    n.vr_dummy_enemy = d.vr_dummy_enemy;
    n.vr_dummy_board = d.vr_dummy_board;
    n.vr_dummy_board_hp = d.vr_dummy_board_hp;
    n.vr_dummy_board_max = d.vr_dummy_board_max;
    n.vr_dummy_board_yaw = d.vr_dummy_board_yaw;
    n.vr_dummy_board_z = d.vr_dummy_board_z;
    d.vr_dummy_board = 0;
    setorigin(n, d.vr_dummy_home);
};

// Whether someone (a player, a monster) stands where an enemy of box `xMins`..`xMaxs` would stand at `xAt` (`xSelf`
// ignored): it waits for them to step away.
float(vector xAt, vector xMins, vector xMaxs, entity xSelf) VR_Dummy_Blocked =
{
    tracebox(xAt, xMins, xMaxs, xAt, FALSE, xSelf);
    return trace_ent != world && (trace_ent.flags & (FL_CLIENT | FL_MONSTER)) && trace_ent.health > 0; // (not a body)
};

// Another enemy chosen (vr_dummy_type; or a motion take's: VR_Dummy_RetypeAll): a new entity stands as that one where
// `d` stood, and `d` goes (a monster's spawn function expects a new entity: nothing of the last enemy's stays). FALSE:
// someone stands where the new one would (unless `xForce`: a take is placed against it next), it stays as it is.
float(entity d, float xType, float xForce) VR_Dummy_Retype =
{
    entity n = spawn();
    VR_Dummy_Keep(n, d);
    n.vr_dummy_board_hp = -1; // (its name changes)
    d.solid = SOLID_NOT; // (out of the new one's way)
    setorigin(d, d.origin);
    entity oldSelf = self;
    self = n;
    VR_Dummy_Setup(xType);
    VR_Dummy_Place();
    if(!xForce && VR_Dummy_Blocked(self.vr_dummy_home, self.mins, self.maxs, self))
    {
        // (Taken back: the new one goes, the old one stays.)
        d.vr_dummy_board = self.vr_dummy_board;
        d.solid = SOLID_SLIDEBOX;
        setorigin(d, d.origin);
        remove(self);
        self = oldSelf;
        return FALSE;
    }
    remove(d);
    dprint(sprintf("dummy: stands as %s (%s)\n", vr_dt_name, vr_dt_class));
    self = oldSelf;
    return TRUE;
};

// The engine, as a motion take starts (vr_motion_play.cpp, after its settings): every training dummy as the take's
// vr_dummy_type has it, now, before the take is placed against it.
void() VR_Dummy_RetypeAll =
{
    float wanted = VR_Dummy_Wanted();
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(VR_IsDummy(e) && e.vr_dummy_enemy != wanted && !e.vr_dummy_atk_phase)
        {
            VR_Dummy_Retype(e, wanted, TRUE);
        }
    }
};

// vr_dummy_gib: a killed dummy's stand-in (no model) stands again where it stood, once no one is in its way; as the
// enemy chosen now.
void() VR_Dummy_Return =
{
    if(VR_Dummy_Blocked(self.vr_dummy_home, self.mins, self.maxs, self))
    {
        self.nextthink = time + 0.25;
        return;
    }
    self.vr_dummy_board_hp = -1;
    VR_Dummy_Setup(VR_Dummy_Wanted());
    VR_Dummy_Place();
    spawn_tfog(self.origin);
};

// vr_dummy_gib: killed as its enemy is (VR_Dummy_Hit: its health gone). It becomes the monster it stands for, `xHealth`
// left before the blow, and the caller deals the blow to it as to one (T_DamageImpl): Killed and its enemy's own death
// (army_die, ...), so it is beheaded or has its head popped (vr_decap.qc: the blow was armed on it as on its enemy), dies,
// lies as a corpse and a ragdoll, or is gibbed, all its enemy's, a grunt's gun falling as a grunt's. What stays the
// dummy's: it is not counted (Killed: vr_dummy_dead), drops no backpack (DropBackpack, SoldierDrop), its health is its own
// (vr_dummy_health, filled up again vr_dummy_regen s after a hit), and a stand-in stands again where it stood
// vr_dummy_gib_respawn seconds later.
void(entity d, float xHealth) VR_Dummy_Die =
{
    entity n = spawn();
    n.classname = "vr_dummy_gibbed";
    VR_Dummy_Keep(n, d); // (its board, showing it killed)
    setsize(n, d.mins, d.maxs); // (not solid: the room it needs to stand again)
    VR_Dummy_ShowBoard(n, TRUE);
    n.think = VR_Dummy_Return;
    n.nextthink = time + max(0.1, cvar_hget(cvarh_vr_dummy_gib_respawn));
    dprint(sprintf("dummy: killed as its enemy, %s (%g health before the blow); stands again in %.1f s\n", VR_Dummy_Name(d),
        xHealth, n.nextthink - time));

    d.vr_dummy_kind = 0;
    d.vr_dummy_dead = TRUE;
    d.health = xHealth;
    d.max_health = d.vr_dummy_own_health;
    d.th_die = d.vr_dummy_th_die;
    d.th_pain = d.vr_dummy_th_pain;
    d.movetype = d.vr_dummy_movetype;
    d.angles = VR_Dummy_Facing(d);
    d.velocity = '0 0 0';
    d.think = SUB_Null; // (its death code's frames take over: never vr_dummy_think again)
    d.nextthink = 0;
    d.vr_dummy_painframe = 0;
    d.vr_dummy_lean = '0 0 0';
    d.vr_dummy_atk_phase = 0;
    d.vr_dummy_atk_live = 0;
    d.vr_dummy_turn = 0;
    d.vr_dummy_replay_what = 0;
    d.effects = d.effects - (d.effects & EF_DIMLIGHT); // (a wind-up's glow)
};

// Killed (combat.qc) by what passed by its report (a zombie it stands as, beheaded whatever its health: vr_decap.qc):
// with vr_dummy_gib, it dies as its enemy, by the death set on it since if any (a beheaded zombie's); TRUE, Killed goes
// on. Off, it stands, unhurt (FALSE).
float(entity d) VR_Dummy_Killed =
{
    if(!cvar_hget(cvarh_vr_dummy_gib))
    {
        d.health = VR_DUMMY_HEALTH;
        return FALSE;
    }
    if(d.th_die != vr_dummy_die)
    {
        d.vr_dummy_th_die = d.th_die;
    }
    VR_Dummy_Die(d, d.health);
    return TRUE;
};

// Once the world is in: stand on the floor, and put up its board.
void() vr_dummy_start =
{
    self.origin_z = self.origin_z + 1;
    droptofloor();
    self.vr_dummy_floor = self.origin_z + self.mins_z;
    VR_Dummy_Place();
    VR_Dummy_ShowBoard(self, FALSE);

    self.think = vr_dummy_think;
    self.nextthink = time + 0.05;
};

/*QUAKED vr_dummy (1 0.5 0) (-12 -12 -24) (12 12 27)
Quake VR: a training dummy that cannot be hurt and reports every hit: the damage, the kind of blow and where it struck,
in the attacker's console and as numbers rising from it. It stands as the enemy chosen in the menu (vr_dummy_type: a
grunt by default), with its health over its head. Not counted as a monster.
*/
void() vr_dummy =
{
    monster_army_precache();
    precache_sound("vr/dummy_windup.wav"); // its attacks (vr_dummy_attacks)
    precache_sound("knight/sword1.wav");

    setmodel(self, "progs/soldier.mdl"); // (a grunt's: here for fgdgen.py, the editor's model)
    self.vr_dummy_angles = self.angles;
    VR_Dummy_Setup(VR_Dummy_Wanted());

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

// ----------------------------------------------------------------------------
// Tests: `vr_dummy_test <n>` (Debug > Gore Tests > Training Dummy), on the training dummy nearest you; `dummytest:` lines.
// 1 what it is: its enemy, model, box, health, and the zone a shot at its head, body and legs strikes (PositionalRegion);
// 2 a 10-damage hit from you (no kind: a plain blow); 3 a hit of just its health left (Dummy Dies: it dies); 4 its health
// now (the refill: vr_dummy_regen).

entity(entity p) VR_Dummy_Nearest =
{
    entity best = world;
    float bestDist = 100000;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(VR_IsDummy(e) && vlen(e.origin - p.origin) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - p.origin);
        }
    }
    return best;
};

// The zone a shot from 64 units ahead of it, level, strikes at height `xUp` over its origin (-1: none).
float(entity d, float xUp) VR_Dummy_TestZone =
{
    makevectors('0 1 0' * d.angles_y);
    vector at = d.origin + '0 0 1' * xUp;
    if(PositionalHead(d) && xUp > d.maxs_z * 0.5)
    {
        at = d.origin + v_forward * xHeadForward + '0 0 1' * xHeadUp; // (its head sphere's middle)
    }
    vector from = at + v_forward * 64;
    traceline(from, at - v_forward * 64, FALSE, self); // (self: you, between)
    if(trace_ent != d)
    {
        return -2; // (missed it)
    }
    return PositionalRegion(d, trace_endpos, at - from);
};

void(float xTest) VR_Dummy_Test =
{
    entity d = VR_Dummy_Nearest(self);
    if(d == world)
    {
        bprint("dummytest: no training dummy\n");
        return;
    }
    VR_Dummy_Health(d);
    if(xTest == 1)
    {
        bprint(sprintf("dummytest: %s (%s) model %s box %v .. %v\n", VR_Dummy_Name(d), d.classname, d.model, d.mins,
            d.maxs));
        bprint(sprintf("dummytest: own health %g, health %g/%g\n", d.vr_dummy_own_health, d.vr_dummy_hp, d.vr_dummy_hpmax));
        bprint(sprintf("dummytest: zones head %g body %g legs %g (1 head, 0 body, 2 limbs, 3 legs, -1 none, -2 missed)\n",
            VR_Dummy_TestZone(d, d.maxs_z), VR_Dummy_TestZone(d, d.maxs_z * 0.3), VR_Dummy_TestZone(d, d.mins_z * 0.5)));
        return;
    }
    if(xTest == 2 || xTest == 3)
    {
        float hurt = xTest == 2 ? 10 : max(1, d.vr_dummy_hp);
        T_Damage(d, self, self, hurt);
        bprint(sprintf("dummytest: hit for %g: health %g/%g, %s\n", hurt, d.vr_dummy_hp, d.vr_dummy_hpmax,
            VR_IsDummy(d) ? "standing" : sprintf("killed (%s, health %g)", d.classname, d.health)));
        return;
    }
    bprint(sprintf("dummytest: health %g/%g (%.1f s since its last hit)\n", d.vr_dummy_hp, d.vr_dummy_hpmax,
        time - d.vr_dummy_hit_time));
};

// Every server frame (StartFrame): a test asked for (vr_dummy_test).
void() VR_Dummy_TestFrame =
{
    float ask = cvar("vr_dummy_test");
    if(!ask)
    {
        return;
    }
    cvar_set("vr_dummy_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    VR_Dummy_Test(ask);
    self = oself;
};
