// ----------------------------------------------------------------------------
// VR: monsters hurt by slime and lava (vr_enemy_liquid_damage, the Gameplay menu's "Enemies Hurt by Liquids").
//
// In id's QuakeC only the player is hurt by liquids (WaterMove, client.qc): a monster walked or was shoved into
// slime or lava and stood there unharmed (a shove's landing burned it once, VR_Shove_Landed, and hipnotic's
// gremlins die in lava). Now a monster in slime or lava burns as the player does, by how deep it is in it (its
// waterlevel, measured as the engine measures the player's: 1 the feet, 2 the waist, 3 the eyes):
//
//   lava   10 x waterlevel every 0.2 s (50, 100, 150 a second)
//   slime   4 x waterlevel every second (4, 8, 12 a second)
//   water   nothing: no drowning (Quake's monsters hold their breath; a knight wading a deep pool should not die)
//
// It is the liquid's damage (no attacker, as the player's): no quad, no Damage to Enemies multiplier, no hit push,
// no gore spray. The monster's own pain code runs (its pain sounds and flinches, at its own pace), and it dies by
// its own death code (a big monster in deep lava goes past its gib health and bursts, as usual). Its corpse is left
// alone (no gibs bursting out of a pool a while later). Lava sizzles as the monster goes in, and it smokes and throws
// embers while it burns; slime hisses as it goes in.
//
// Immune:
//   fish, eels (FL_SWIM)                 their liquid is their home, whatever it is
//   Chthon, Shub-Niggurath, Armagon,     bosses: scripted deaths, and Chthon and the lava lord live in lava
//   the dragon, the lava lord (Hephaestus)
//   the firing range's dummy             reports hits, never hurt (vr_dummy.qc)
// Special:
//   zombies                              lava burns them up at once (gibs: nothing less kills a zombie);
//                                        slime's small ticks are shrugged off as any small hit (zombie_pain)
// Everyone else burns: shamblers too.
//
// Scanned once a server frame (StartFrame): every monster is looked at each 0.1 s, and one in slime or lava at
// its liquid's pace.
// ----------------------------------------------------------------------------

.float vr_liquid_next; // when the monster is next looked at (in slime or lava: when it next burns)
.float vr_liquid_in;   // the liquid it was in when last looked at (CONTENT_*, 0 none), for the sound going in

float VR_LIQUID_LOOK = 0.1; // seconds between looks at a monster out of slime and lava

float vr_liquid_type; // set by VR_Liquid_Level: the liquid at the feet

// How deep `e` is in liquid, as the engine measures the player's waterlevel: 0 out of it, 1 the feet, 2 the waist,
// 3 the eyes. vr_liquid_type: the liquid (CONTENT_WATER, CONTENT_SLIME, CONTENT_LAVA; else CONTENT_EMPTY).
float(entity e) VR_Liquid_Level =
{
    vector p = e.origin;
    p_z = e.origin_z + e.mins_z + 1;
    float c = pointcontents(p);
    if(c != CONTENT_WATER && c != CONTENT_SLIME && c != CONTENT_LAVA)
    {
        vr_liquid_type = CONTENT_EMPTY;
        return 0;
    }
    vr_liquid_type = c;

    p_z = e.origin_z + (e.mins_z + e.maxs_z) * 0.5;
    c = pointcontents(p);
    if(c > CONTENT_WATER || c < CONTENT_LAVA)
    {
        return 1;
    }

    // The eyes (walkmonster_start_go's view_ofs), else three quarters of the way up.
    if(e.view_ofs_z > 0)
    {
        p_z = e.origin_z + e.view_ofs_z;
    }
    else
    {
        p_z = e.origin_z + e.mins_z + (e.maxs_z - e.mins_z) * 0.75;
    }
    c = pointcontents(p);
    if(c > CONTENT_WATER || c < CONTENT_LAVA)
    {
        return 2;
    }
    return 3;
};

float(entity e) VR_Liquid_Immune =
{
    if(e.flags & FL_SWIM)
    {
        return TRUE;
    }
    if(e.classname == "monster_boss" || e.classname == "monster_oldone" || e.classname == "monster_armagon" ||
       e.classname == "monster_dragon" || e.classname == "monster_lava_man" || e.classname == "vr_dummy")
    {
        return TRUE;
    }
    return FALSE;
};

// Smoke and embers where the burning monster meets the lava's surface.
void(entity e) VR_Liquid_BurnEffect =
{
    vector top = e.origin + '0 0 1' * (e.maxs_z + 8);
    vector at = liquidentry(top, e.origin + '0 0 1' * e.mins_z);
    if(at == top)
    {
        at = e.origin;
    }
    vector r = '1 0 0' * crandom() * e.maxs_x * 0.6 + '0 1 0' * crandom() * e.maxs_y * 0.6;
    particle2(at + r, '0 0 1', QVR_PARTICLE_PRESET_LAVASPIKE, 6); // embers (the lava nails' sparkles)
    particle2(at + r, '0 0 1', QVR_PARTICLE_PRESET_SMOKE, 4);
};

// `hurt`: vr_enemy_liquid_damage (without, only vr_debug_shots' log: a monster in slime or lava unharmed).
void(entity e, float hurt) VR_Liquid_Monster =
{
    e.vr_liquid_next = time + VR_LIQUID_LOOK;

    // Alive (not a corpse, vr_corpse_gib, nor a dying one, whose takedamage is off).
    if(e.health <= 0)
    {
        return;
    }
    if(VR_Liquid_Immune(e))
    {
        return;
    }

    float level = VR_Liquid_Level(e);
    float kind = vr_liquid_type;
    if(kind != CONTENT_SLIME && kind != CONTENT_LAVA)
    {
        e.vr_liquid_in = kind;
        return;
    }

    if(!hurt)
    {
        e.vr_liquid_in = 0;
        e.vr_liquid_next = time + 1;
        dprint(sprintf("liquid: %s in %s (level %g) not burnt (vr_enemy_liquid_damage 0) at %g s, health %g\n",
            e.classname, kind == CONTENT_LAVA ? "lava" : "slime", level, time, e.health));
        return;
    }

    if(e.vr_liquid_in != kind)
    {
        e.vr_liquid_in = kind;
        if(kind == CONTENT_LAVA)
        {
            sound(e, CHAN_BODY, "player/inlava.wav", 1, ATTN_NORM);
        }
        else
        {
            sound(e, CHAN_BODY, "player/slimbrn2.wav", 1, ATTN_NORM);
        }
    }

    float dmg;
    if(kind == CONTENT_LAVA)
    {
        e.vr_liquid_next = time + 0.2;
        dmg = 10 * level;
        if(e.health > 0 && e.classname == "monster_zombie")
        {
            dmg = e.health + 40; // burnt up: a zombie only dies of a hit its size (zombie_pain)
        }
        VR_Liquid_BurnEffect(e);
    }
    else
    {
        e.vr_liquid_next = time + 1;
        dmg = 4 * level;
    }

    T_Damage(e, world, world, dmg);
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("liquid: %s in %s (level %g) burnt for %g at %g s, health %g\n", e.classname,
            kind == CONTENT_LAVA ? "lava" : "slime", level, dmg, time, e.health));
    }
};

// StartFrame: the monsters in slime and lava burn.
void() VR_Liquids_Frame =
{
    float hurt = cvar_hget(cvarh_vr_enemy_liquid_damage);
    if(!hurt && !cvar_hget(cvarh_vr_debug_shots))
    {
        return;
    }

    entity e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e.takedamage && e.vr_liquid_next <= time)
        {
            VR_Liquid_Monster(e, hurt);
        }
        e = nextent(e);
    }
};
