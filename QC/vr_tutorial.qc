// vr_tutorial.qc -- the scripting entities of the tutorial (vrtutorial; Misc/quakevr/maps/vrtutorial_gen.py), for
// any map: a door that waits for full health (trigger_vr_health_gate), monsters made on demand that report when they
// are dead (func_vr_spawner), target boards that break (func_vr_target), supplies that come back (func_vr_restock),
// checkpoints a dead player comes back to instead of the level restarting (info_vr_checkpoint), the music changed
// (target_vr_music), a message held in the middle of the view (target_vr_message) and a spike or a grenade fired at the
// player for batting and catching practice (trap_vr_shooter). vr_crate's
// "contents" (vr_crates.qc) and func_vr_tip's TRIGGERED flag (vr_tips.qc) are the other two.

// (Their fields: vr_fields.qc, "The tutorial's scripting".)

// ----------------------------------------------------------------------------
// Things made by name

float vr_tut_precaching; // VR_Tut_Precache: made only for its precaches (not placed)

void() weapon_axe;
void() weapon_knight_sword;
void() weapon_dagger;
void() item_armor1;

// The middle of a placed item's floor from its origin: ammo and health boxes have their origin at a corner (id's
// 0 0 0 .. 32 32 56), the others in the middle.
float(string cls) VR_Tut_CornerOrigin =
{
    return cls == "item_shells" || cls == "item_spikes" || cls == "item_rockets" || cls == "item_cells" ||
           cls == "item_health";
};

// A new `cls` (its spawn function) standing at `at` (the middle of its floor), with `xFlags` (its spawnflags), `xModel`
// (a rock's or a brick's) and `xAngles`; placed now (VRPlaceItemNow), as a crate's contents are. world: a class this
// doesn't make, or it removed itself.
entity(string cls, vector at, float xFlags, string xModel, vector xAngles) VR_Tut_Make =
{
    entity oself = self;
    entity e = spawn();
    self = e;
    e.classname = cls;
    e.spawnflags = xFlags;
    e.angles = xAngles;
    if(VR_Tut_CornerOrigin(cls))
    {
        setorigin(e, at - '16 16 0');
    }
    else if(cls == "item_key1" || cls == "item_key2")
    {
        setorigin(e, at + '0 0 24'); // (a key's box: -24 .. 32 about its origin)
    }
    else
    {
        setorigin(e, at);
    }
    float ok = TRUE;
    float item = TRUE;
         if(cls == "item_shells")         { item_shells(); }
    else if(cls == "item_spikes")         { item_spikes(); }
    else if(cls == "item_rockets")        { item_rockets(); }
    else if(cls == "item_cells")          { item_cells(); }
    else if(cls == "item_health")         { item_health(); }
    else if(cls == "item_key1")           { item_key1(); }
    else if(cls == "item_key2")           { item_key2(); }
    else if(cls == "weapon_shotgun")      { weapon_shotgun(); }
    else if(cls == "weapon_supershotgun") { weapon_supershotgun(); }
    else if(cls == "weapon_nailgun")      { weapon_nailgun(); }
    else if(cls == "weapon_supernailgun") { weapon_supernailgun(); }
    else if(cls == "weapon_axe")          { weapon_axe(); }
    else if(cls == "weapon_knight_sword") { weapon_knight_sword(); }
    else if(cls == "weapon_dagger")       { weapon_dagger(); }
    else if(cls == "item_armor1")         { item_armor1(); }
    else if(cls == "vr_debris_piece")     { e.model = xModel; item = FALSE; vr_debris_piece(); }
    else if(cls == "vr_crate")            { item = FALSE; vr_crate(); }
    else                                  { ok = FALSE; }
    if(ok && item && e.nextthink >= 0 && !vr_tut_precaching)
    {
        ok = VRPlaceItemNow();
    }
    self = oself;
    if(!ok)
    {
        if(e.nextthink >= 0)
        {
            remove(e);
        }
        return world;
    }
    if(e.nextthink < 0)
    {
        return world; // (it removed itself: a freed entity's nextthink is -1)
    }
    return e;
};

// What VR_Tut_Make makes `cls` with precached while the map spawns (precaches are allowed only then): one made, its
// spawn function's precaches done, and removed at once.
void(string cls, string xModel) VR_Tut_Precache =
{
    if(cls == NULLSTR || cls == "none" || cls == "vr_debris_piece" || cls == "vr_crate")
    {
        return; // (the rocks, the bricks and the crates are always precached)
    }
    // the blades: their models only (one made would leave its lying weapon, a thrown_weapon, behind at the origin)
    if(cls == "weapon_axe" || cls == "weapon_knight_sword" || cls == "weapon_dagger")
    {
        precache_model(WeaponIdToModel(cls == "weapon_axe" ? WID_AXE : WID_SWORD,
                                       cls == "weapon_dagger" ? VR_ADMelee_Flags(VR_SWORDKIND_DAGGER) : 0));
        return;
    }
    vr_tut_precaching = TRUE;
    entity e = VR_Tut_Make(cls, '0 0 0', 0, xModel, '0 0 0');
    vr_tut_precaching = FALSE;
    if(e != world)
    {
        remove(e);
    }
};

// ----------------------------------------------------------------------------
// trigger_vr_health_gate

void() trigger_vr_health_gate_touch =
{
    if(other.classname != "player" || other.health <= 0)
    {
        return;
    }
    float need = self.health > 0 ? self.health : other.max_health;
    if(other.health >= need)
    {
        activator = other;
        SUB_UseTargets();
        self.touch = SUB_Null;
        self.solid = SOLID_NOT;
        self.think = SUB_Remove;
        self.nextthink = time + 0.1;
        return;
    }
    if(time >= self.attack_finished && self.message != NULLSTR)
    {
        centerprint(other, self.message);
        self.attack_finished = time + 2;
    }
};

/*QUAKED trigger_vr_health_gate (.5 .5 .5) ?
Quake VR: fires its "target" once, when the player touches it at full health (his max_health, or "health" if set: a
door that opens only then, as vrtutorial's). Below it, "message" is centerprinted (every 2 seconds while he stays in it).
*/
void() trigger_vr_health_gate =
{
    InitTrigger();
    self.touch = trigger_vr_health_gate_touch;
};

// ----------------------------------------------------------------------------
// func_vr_spawner

// How many of its monsters are alive.
float(entity xSpawner) VR_Spawner_Alive =
{
    float n = 0;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_spawner == xSpawner && (e.flags & FL_MONSTER) && e.health > 0 && e.deadflag == DEAD_NO)
        {
            n++;
        }
    }
    return n;
};

// Its ANGRY monsters with no enemy go for the player who used it.
void(entity xSpawner) VR_Spawner_Rouse =
{
    entity p = xSpawner.vr_spawn_by;
    if(p.classname != "player" || p.health <= 0 || (p.flags & FL_NOTARGET))
    {
        return;
    }
    for(entity e = nextent(world); e; e = nextent(e))
    {
        // (started: walkmonster_start_go has run, it can be hurt; roused before it, it never would start)
        if(e.vr_spawner == xSpawner && (e.flags & FL_MONSTER) && e.health > 0 && e.enemy == world && e.th_run &&
           e.takedamage != DAMAGE_NO)
        {
            entity oself = self;
            self = e;
            self.enemy = p;
            FoundTarget();
            self = oself;
        }
    }
};

// Its monsters stranded down out of the player's sight (more than 64 units below the spot they came in at, the player
// not seen for VR_SPAWNER_STRANDED seconds) come back to that spot with a teleporter's flash: a dog that leapt or was
// knocked into vrtutorial's arena pit can't find the pit's stairs, sees no one from down there and is seen by no one
// on the floor, and the wave can't end while it lives.
const float VR_SPAWNER_STRANDED = 10;

void(entity xSpawner) VR_Spawner_Unstrand =
{
    entity p = xSpawner.vr_spawn_by;
    vector org = xSpawner.origin;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_spawner != xSpawner || !(e.flags & FL_MONSTER) || e.health <= 0 || e.deadflag != DEAD_NO)
        {
            continue;
        }
        float seen = e.origin_z >= org_z - 64;
        if(!seen && p.classname == "player" && p.health > 0)
        {
            traceline(e.origin + e.view_ofs, p.origin + p.view_ofs, TRUE, e);
            seen = trace_fraction == 1;
        }
        if(seen || !e.vr_spawn_seen)
        {
            e.vr_spawn_seen = time;
            continue;
        }
        if(time - e.vr_spawn_seen < VR_SPAWNER_STRANDED)
        {
            continue;
        }
        // (something standing on the spot: next time)
        float clear = TRUE;
        for(entity o = findradius(org, 40); o; o = o.chain)
        {
            if(o != e && (((o.flags & FL_MONSTER) && o.health > 0) || o.classname == "player"))
            {
                clear = FALSE;
            }
        }
        if(!clear)
        {
            continue;
        }
        dprint(sprintf("spawner %s: its %s stranded out of sight at %v: back at %v\n", xSpawner.targetname, e.classname,
                       e.origin, org));
        spawn_tfog(e.origin);
        setorigin(e, org);
        e.velocity = '0 0 0';
        e.flags = e.flags - (e.flags & FL_ONGROUND);
        spawn_tfog(org);
        e.vr_spawn_seen = time;
    }
};

void() VR_Spawner_Make =
{
    entity sp = self;
    vector org = sp.origin;
    // (another monster still standing where it comes: wait)
    for(entity e = findradius(org, 40); e; e = e.chain)
    {
        if(((e.flags & FL_MONSTER) && e.health > 0) || e.classname == "player")
        {
            return;
        }
    }
    self = spawn();
    self.spawnflags = 0;
    self.angles = sp.angles;
    setorigin(self, org);
    if(sp.weapon == 0 && sp.vr_spawn_arm != 0)
    {
        // an armed grunt holding that weapon (vrtutorial's lesson 13: an axe, to shove off a bridge or disarm)
        self.classname = "monster_vr_grunt";
        self.weapon = sp.vr_spawn_arm;
        monster_vr_grunt();
    }
    else
    {
        VR_EnemyDispenser_Spawn(sp.weapon);
    }
    setorigin(self, org);
    self.vr_spawner = sp;
    self = sp;
    spawn_tfog(org);
    sp.vr_spawn_left--;
    dprint(sprintf("spawner %s: made a %s at %v, %g to come\n", sp.targetname, VR_EnemyDispenser_Model(sp.weapon), org,
                   sp.vr_spawn_left));
};

void() func_vr_spawner_think =
{
    self.nextthink = time + 0.25;
    if(self.vr_spawn_left > 0 && time >= self.attack_finished)
    {
        float before = self.vr_spawn_left;
        VR_Spawner_Make();
        if(self.vr_spawn_left < before)
        {
            self.attack_finished = time + self.wait;
        }
    }
    if(self.spawnflags & 2)
    {
        VR_Spawner_Rouse(self);
    }
    VR_Spawner_Unstrand(self);
    if(self.vr_spawn_left <= 0 && self.vr_spawn_armed && VR_Spawner_Alive(self) == 0)
    {
        self.vr_spawn_armed = 0;
        self.nextthink = 0;
        activator = self.vr_spawn_by;
        dprint(sprintf("spawner %s: all dead, fires %s\n", self.targetname, self.target));
        SUB_UseTargets();
    }
};

void() func_vr_spawner_use =
{
    if((self.spawnflags & 1) && (self.vr_spawn_left > 0 || VR_Spawner_Alive(self) > 0))
    {
        if(activator.classname == "player" && self.message != NULLSTR)
        {
            centerprint(activator, self.message);
        }
        return;
    }
    if(activator.classname == "player")
    {
        self.vr_spawn_by = activator;
    }
    else if(self.vr_spawn_by == world)
    {
        self.vr_spawn_by = VR_Player();
    }
    self.vr_spawn_left = self.vr_spawn_left + self.count;
    self.vr_spawn_armed = 1;
    self.think = func_vr_spawner_think;
    self.nextthink = time + 0.05;
};

/*QUAKED func_vr_spawner (1 0.4 0.2) (-16 -16 -24) (16 16 40) ONE_AT_A_TIME ANGRY
Quake VR: makes monsters here when used, with a teleporter's flash: "weapon" which kind (as func_enemy_dispenser's: 0
grunt, 1 ogre, 2 zombie, 7 dog, 8 enforcer...; with "weapon" 0, "vr_spawn_arm" makes armed grunts holding that weapon,
monster_vr_grunt's codes: 2 an axe, 13 a sword...), "count" how many a use makes (default 1), "wait" the seconds between
them (default 0.5; one waits while something stands on the spot), "angle" the way they face. "target" is fired (the
user the activator) once every monster of its latest use is dead: chain waves with trigger_counter and trigger_relay.
ONE_AT_A_TIME: a use while one of its monsters lives makes none ("message" centerprinted to the player who used it).
ANGRY: they go for the player who used it at once.
One stranded more than 64 units below the spot it came in at, out of the player's sight for 10 seconds (a pit it can't
climb out of), comes back to that spot.
*/
void() func_vr_spawner =
{
    if(self.weapon < 5)
    {
        monster_army_precache();
        monster_ogre_precache();
        monster_zombie_precache();
        monster_shambler_precache();
        monster_wizard_precache();
    }
    else
    {
        string mdl = VR_EnemyDispenser_Model(self.weapon);
        if(mdl != NULLSTR && fileexists(mdl))
        {
            precache_model(mdl); // (its other models and sounds: vr_probe.qc, as a dispenser's)
        }
    }
    if(self.count <= 0)
    {
        self.count = 1;
    }
    if(!self.wait)
    {
        self.wait = 0.5;
    }
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    self.use = func_vr_spawner_use;
};

// ----------------------------------------------------------------------------
// func_vr_target

void() func_vr_target_die =
{
    vector mid = (self.absmin + self.absmax) * 0.5;
    vector dir = '0 0 0';
    if(damage_inflictor != world && damage_inflictor != self)
    {
        dir = normalize(mid - (damage_inflictor.absmin + damage_inflictor.absmax) * 0.5);
    }
    activator = damage_attacker;
    SUB_UseTargets();
    float k = floor(random() * 3);
    sound(self, CHAN_AUTO, k < 1 ? "vr/crate_break1.wav" : (k < 2 ? "vr/crate_break2.wav" : "vr/crate_break3.wav"),
          VR_Crate_Volume(1, cvar_hget(cvarh_vr_crate_break_volume)),
          ATTN_NORM);
    particle2(mid, dir, QVR_PARTICLE_PRESET_WOODDUST, 24);
    self.takedamage = DAMAGE_NO;
    self.th_die = SUB_Null;
    self.solid = SOLID_NOT;
    self.vr_cratekind = 1; // (a small crate's splinters: VR_Crate_Pieces reads its kind, box and skin)
    self.skin = 2;
    VR_Crate_Pieces(self, mid, dir);
    self.vr_cratekind = 0;
    self.model = string_null;
    self.modelindex = 0;
    self.think = SUB_Remove;
    self.nextthink = time + 0.1;
};

/*QUAKED func_vr_target (0.8 0.5 0.2) ?
Quake VR: a target board (a brush) that breaks into splinters when it is shot, struck or blown up ("health", default
10), firing its "target" (the activator: what broke it). vrtutorial's firing range: a trigger_counter opens the way on.
*/
void() func_vr_target =
{
    self.solid = SOLID_BSP;
    self.movetype = MOVETYPE_PUSH;
    setorigin(self, self.origin);
    setmodel(self, self.model);
    if(self.health <= 0)
    {
        self.health = 10;
    }
    self.max_health = self.health;
    self.takedamage = DAMAGE_YES;
    self.th_pain = SUB_NullPain;
    self.th_die = func_vr_target_die;
};

// ----------------------------------------------------------------------------
// trap_vr_shooter

void() OgreGrenadeTouch;
void() OgreGrenadeExplode;
void() monster_vr_grunt;

void() trap_vr_shooter_use =
{
    entity p = activator.classname == "player" ? activator : VR_Player();
    if(p == world)
    {
        return;
    }
    vector at = p.origin + '0 0 16'; // (his chest)
    dprint(sprintf("trap_vr_shooter %s: a %s at %v\n", self.targetname, self.style == 1 ? "grenade" : "spike", at));
    if(self.style == 1)
    {
        // a grenade lobbed to come down at him in a second: the ogre's, a physics object a hand can catch
        sound(self, CHAN_WEAPON, "weapons/grenade.wav", 1, ATTN_NORM);
        entity g = spawn();
        g.owner = self;
        g.movetype = MOVETYPE_BOUNCE;
        g.solid = SOLID_BBOX;
        g.classname = "ogre_grenade";
        float flight = 1;
        g.velocity = (at - self.origin) * (1 / flight) + '0 0 1' * (0.5 * cvar_hget(cvarh_sv_gravity) * flight);
        g.avelocity = '300 300 300';
        g.angles = vectoangles(g.velocity);
        g.touch = OgreGrenadeTouch;
        g.nextthink = time + VR_Grenade_Fuse();
        g.think = OgreGrenadeExplode;
        setmodel(g, "progs/grenade.mdl");
        setsize(g, '0 0 0', '0 0 0');
        setorigin(g, self.origin);
        VR_Grenade_Make(g, OgreGrenadeExplode, g.nextthink - time, FALSE);
    }
    else
    {
        // a spike straight at him (500 units/s, as a spikeshooter's)
        vector dir = normalize(at - self.origin);
        sound(self, CHAN_VOICE, "weapons/spike2.wav", 1, ATTN_NORM);
        entity s = launch_spike(self.origin, dir);
        s.velocity = dir * 500;
    }
};

/*QUAKED trap_vr_shooter (0 .5 .8) (-8 -8 -8) (8 8 8)
Quake VR: when used, fires at the player who used it (single player: the player): "style" 0 a spike straight at him,
1 a grenade lobbed to come down at him a second later (the ogre's: catch it and throw it back). For batting and
catching practice (vrtutorial's lesson 13).
*/
void() trap_vr_shooter =
{
    precache_model("progs/grenade.mdl");
    precache_sound("weapons/grenade.wav");
    precache_sound("weapons/spike2.wav");
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    self.use = trap_vr_shooter_use;
};

// ----------------------------------------------------------------------------
// func_vr_restock

// How many of its contents lie within its distance (a rock or a brick: vr_rock, vr_brick once placed).
float(entity xRestock) VR_Restock_Near =
{
    float n = 0;
    string cls = xRestock.contents;
    string cls2 = cls;
    if(cls == "vr_debris_piece")
    {
        cls = "vr_rock";
        cls2 = "vr_brick";
    }
    // the blades (func_weapon_grabbable's classes: the spawner draws nothing, the weapon lies as a thrown_weapon)
    float wid = 0;
    float wkind = VR_ADMelee_Flags(VR_SWORDKIND_DAGGER); // (the sword's kind bits: a dagger's, a knight's sword's none)
    float wflags = 0;
    if(cls == "weapon_axe") { wid = WID_AXE; }
    else if(cls == "weapon_knight_sword") { wid = WID_SWORD; }
    else if(cls == "weapon_dagger") { wid = WID_SWORD; wflags = wkind; }
    if(wid)
    {
        cls = "thrown_weapon";
        cls2 = cls;
    }
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if((e.classname == cls || e.classname == cls2) && e.modelindex && e.model != NULLSTR && e.solid != SOLID_NOT &&
           (!wid || (e.weapon == wid && (e.weaponflags & wkind) == wflags)) &&
           vlen((e.absmin + e.absmax) * 0.5 - xRestock.origin) <= xRestock.distance)
        {
            n++;
        }
    }
    return n;
};

void() VR_Restock_Put =
{
    float near = VR_Restock_Near(self);
    if(cvar_hget(cvarh_developer) >= 2)
    {
        dprint(sprintf("restock: %g %s near %v (of %g)\n", near, self.contents, self.origin, self.count));
    }
    if(near >= self.count)
    {
        return;
    }
    entity e = VR_Tut_Make(self.contents, self.origin, self.contentsflags, self.model, self.angles);
    if(e != world)
    {
        sound(self, CHAN_VOICE, "items/itembk2.wav", 0.6, ATTN_NORM);
        dprint(sprintf("restock: a new %s at %v (%g were near)\n", self.contents, self.origin, near));
    }
    else
    {
        dprint(sprintf("restock: a new %s at %v could not be made\n", self.contents, self.origin));
    }
};

void() func_vr_restock_think =
{
    VR_Restock_Put();
    self.nextthink = time + self.wait;
};

void() func_vr_restock_use =
{
    VR_Restock_Put();
};

/*QUAKED func_vr_restock (0.3 0.8 1) (-8 -8 -8) (8 8 8)
Quake VR: keeps a supply going (single player). Every "wait" seconds (default 5; -1: only when used), when fewer than
"count" (default 1) of "contents" lie within "distance" (default 96) of it, it puts a new one here: item_shells,
item_spikes, item_rockets, item_cells, item_health, item_key1, item_key2, weapon_shotgun, weapon_supershotgun,
weapon_nailgun, weapon_supernailgun, vr_debris_piece (a rock or a brick: "model", e.g. progs/vr_rock3.mdl) or
vr_crate. "contentsflags": its spawnflags (item_health 1: the small kit). Its origin is the middle of the new one's
floor. Place one of them in the map as well (what it makes is precached by it).
*/
void() func_vr_restock =
{
    if(deathmatch || coop || self.contents == NULLSTR)
    {
        remove(self);
        return;
    }
    precache_sound("items/itembk2.wav");
    VR_Tut_Precache(self.contents, self.model);
    if(self.count <= 0)
    {
        self.count = 1;
    }
    if(self.distance <= 0)
    {
        self.distance = 96;
    }
    if(!self.wait)
    {
        self.wait = 5;
    }
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    self.use = func_vr_restock_use;
    if(self.wait > 0)
    {
        self.think = func_vr_restock_think;
        self.nextthink = time + self.wait + random();
    }
};

// ----------------------------------------------------------------------------
// info_vr_checkpoint

const float VR_CP_SAVE = 1; // info_vr_checkpoint's SAVE: taken, the game is saved; a death after it loads that save
nosave float vr_cp_loading; // a SAVE checkpoint's load asked for (a map's load starts it at 0 again)

// The save a SAVE checkpoint makes ("vrtutorial_checkpoint": one per map, written over each time).
string() VR_Checkpoint_SaveName =
{
    return strcat(mapname, "_checkpoint");
};

void() VR_Checkpoint_Take =
{
    if(self.vr_cp_taken)
    {
        return;
    }
    self.vr_cp_taken = time + 0.001;
    self.nextthink = 0;
    dprint(sprintf("checkpoint %s taken at %g\n", self.targetname, time));
    if((self.spawnflags & VR_CP_SAVE) && !coop && !deathmatch)
    {
        // (the save made on the next frame's commands: the checkpoint taken is in it)
        localcmd(sprintf("save %s\n", VR_Checkpoint_SaveName()));
        entity p = VR_Player();
        if(self.message != NULLSTR && p != world)
        {
            centerprint(p, self.message);
        }
    }
};

void() info_vr_checkpoint_think =
{
    self.nextthink = time + 0.25;
    for(entity e = findradius(self.origin, self.distance); e; e = e.chain)
    {
        if(e.classname == "player" && e.health > 0)
        {
            VR_Checkpoint_Take();
            return;
        }
    }
};

void() info_vr_checkpoint_use =
{
    VR_Checkpoint_Take();
};

/*QUAKED info_vr_checkpoint (0.3 1 0.3) (-16 -16 -24) (16 16 32)
Quake VR: where a player who dies comes back (single player), instead of the level restarting: taken when he comes
within "distance" (default 96) of it, or when it is used. He comes back at the latest one taken, facing its "angle",
with a new life (the level's starting items and health; the keys he holds kept), the level as he left it. No
checkpoint taken: the level restarts, as Quake's.
SAVE: taken, the game is saved ("<map>_checkpoint", written over each time) and "message" centerprinted; while it is the
latest one taken, a death loads that save (the level as it was then: a fight after it starts again) instead.
*/
void() info_vr_checkpoint =
{
    if(self.distance <= 0)
    {
        self.distance = 96;
    }
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    self.use = info_vr_checkpoint_use;
    self.think = info_vr_checkpoint_think;
    self.nextthink = time + 0.5 + random() * 0.25;
};

// The checkpoint taken last (world: none).
entity() VR_Checkpoint_Latest =
{
    entity best = world;
    for(entity e = find(world, classname, "info_vr_checkpoint"); e; e = find(e, classname, "info_vr_checkpoint"))
    {
        if(e.vr_cp_taken && (best == world || e.vr_cp_taken > best.vr_cp_taken))
        {
            best = e;
        }
    }
    return best;
};

// respawn() in single player: back at the latest checkpoint with a new life (TRUE), or nothing done (FALSE: none taken).
float() VR_Checkpoint_Respawn =
{
    entity cp = VR_Checkpoint_Latest();
    if(coop || deathmatch || cp == world)
    {
        return FALSE;
    }
    string save = strcat(VR_Checkpoint_SaveName(), ".sav");
    if((cp.spawnflags & VR_CP_SAVE) && fileexists(save))
    {
        if(!vr_cp_loading) // (asked once: the load comes on the next frame's commands)
        {
            vr_cp_loading = TRUE;
            dprint(sprintf("checkpoint: %s, loading %s\n", cp.targetname, save));
            localcmd(sprintf("load %s\n", VR_Checkpoint_SaveName()));
        }
        return TRUE;
    }
    float keys = self.items & (IT_KEY1 | IT_KEY2);
    SetNewParms();
    PutClientInServer();
    self.items = self.items | keys;
    dprint(sprintf("checkpoint: back at %v\n", self.origin));
    return TRUE;
};

// ----------------------------------------------------------------------------
// target_vr_music

// The CD track target_vr_music switched to (0: the map's own, worldspawn's "sounds"). Saved with the game: the engine
// tells a connecting client this one in place of the map's (VR_ServerMusicTrack), so a game loaded mid-fight plays it.
float vr_music_track;
.string music_folder; // target_vr_music: the campaign folder whose own track "sounds" is ("hipnotic")

void() target_vr_music_use =
{
    float want = self.sounds;
    if(self.music_folder != "" && want)
    {
        // A campaign's own track (the engine's number for it): only when the player's Quake has it; else the music
        // stays as it is (and so does vr_music_track).
        want = campaignmusic(self.music_folder, self.sounds);
        if(!want)
        {
            dprint(sprintf("music: no %s track %g, unchanged at %g\n", self.music_folder, self.sounds, time));
            return;
        }
    }
    vr_music_track = want;
    float track = want ? want : world.sounds;
    WriteByte(MSG_ALL, SVC_CDTRACK);
    WriteByte(MSG_ALL, track);
    WriteByte(MSG_ALL, track);
    dprint(sprintf("music: track %g at %g\n", track, time));
};

/*QUAKED target_vr_music (0.6 0.3 1) (-8 -8 -8) (8 8 8)
Quake VR: when used, the music changes to CD track "sounds" (as worldspawn's "sounds": 2 to 11 are Quake's own), or
back to the map's own track with "sounds" 0. The tracks play from the player's own Quake (its music folders), at his
music volume; none there: silence, as the map's own. A saved game keeps the track. With "music_folder" (a campaign's
folder: "hipnotic", "rogue", "dopa", "mg1", "mg3", "id1") "sounds" is that campaign's own track (2 to 15), whatever
the game; when the player's Quake has no such track the music is left as it is.
*/
void() target_vr_music =
{
    self.use = target_vr_music_use;
};

// ----------------------------------------------------------------------------
// target_vr_message

void() target_vr_message_think =
{
    float sent = FALSE;
    for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
    {
        if(p.flags & FL_CLIENT)
        {
            // (Held by the client for "wait" seconds: in the middle of the view, or a centre print on a flat screen.)
            viewmessage(p, self.message, self.wait);
            sound(p, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NORM);
            sent = TRUE;
        }
    }
    if(sent)
    {
        dprint(sprintf("message: sent at %g, for %g s\n", time, self.wait));
        self.nextthink = 0; // (kept, not removed: the edicts the game spawns later stay where they were)
        return;
    }
    self.nextthink = time + 0.1; // no player yet
};

void() target_vr_message_use =
{
    self.think = target_vr_message_think;
    self.nextthink = time + 0.05;
};

/*QUAKED target_vr_message (0.6 0.3 1) (-8 -8 -8) (8 8 8)
Quake VR: a message ("message") floating in the middle of every player's view (at eye level, as VR Calibration's text;
not in the wrist's hologram) for "wait" seconds (default 8), not a centre print's 2; on a flat screen a centre print held
that long. No new tip shows until it is gone. Shown when used, or with no "targetname" by itself
"delay" seconds (default 1) after the map starts.
*/
void() target_vr_message =
{
    if(self.wait <= 0)
    {
        self.wait = 8;
    }
    self.use = target_vr_message_use;
    if(self.targetname == "")
    {
        self.think = target_vr_message_think;
        self.nextthink = time + (self.delay > 0 ? self.delay : 1);
    }
};

// ----------------------------------------------------------------------------
// Debug > Tests > Tutorial > Go to a Lesson (vr_tutorial_goto): in vrtutorial, the player put at lesson N's
// checkpoint ("cp<N>", taken), when the cvar changes to N (1..15). Checked every frame (StartFrame).

nosave float vr_tut_goto_seen; // the value acted on (a map's start takes the cvar as it is: no jump)
nosave float vr_tut_goto_ready;

void() VR_Tutorial_Frame =
{
    if(mapname != "vrtutorial")
    {
        return;
    }
    float n = cvar_hget(cvarh_vr_tutorial_goto);
    if(!vr_tut_goto_ready)
    {
        vr_tut_goto_ready = TRUE;
        vr_tut_goto_seen = n;
        return;
    }
    if(n == vr_tut_goto_seen)
    {
        return;
    }
    vr_tut_goto_seen = n;
    entity cp = find(world, targetname, sprintf("cp%g", n));
    entity p = VR_Player();
    if(cp == world || p == world)
    {
        return;
    }
    entity oself = self;
    self = cp;
    VR_Checkpoint_Take();
    self = oself;
    setorigin(p, cp.origin);
    p.velocity = '0 0 0';
    p.angles = cp.angles;
    p.fixangle = TRUE;
    p.flags = p.flags - (p.flags & FL_ONGROUND);
    dprint(sprintf("tutorial: to lesson %g at %v\n", n, cp.origin));
};
