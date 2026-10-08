// vr_tutorial.qc -- the scripting entities of the tutorial (vrtutorial2; Misc/quakevr/maps/vrtutorial2_gen.py), for
// any map: a door that waits for full health (trigger_vr_health_gate), monsters made on demand that report when they
// are dead (func_vr_spawner), target boards that break (func_vr_target), supplies that come back (func_vr_restock), and
// checkpoints a dead player comes back to instead of the level restarting (info_vr_checkpoint). vr_crate's
// "contents" (vr_crates.qc) and func_vr_tip's TRIGGERED flag (vr_tips.qc) are the other two.

// (Their fields: vr_fields.qc, "The tutorial's scripting".)

// ----------------------------------------------------------------------------
// Things made by name

float vr_tut_precaching; // VR_Tut_Precache: made only for its precaches (not placed)

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
door that opens only then, as vrtutorial2's). Below it, "message" is centerprinted (every 2 seconds while he stays in it).
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
    VR_EnemyDispenser_Spawn(sp.weapon);
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
        self.vr_spawn_by = find(world, classname, "player");
    }
    self.vr_spawn_left = self.vr_spawn_left + self.count;
    self.vr_spawn_armed = 1;
    self.think = func_vr_spawner_think;
    self.nextthink = time + 0.05;
};

/*QUAKED func_vr_spawner (1 0.4 0.2) (-16 -16 -24) (16 16 40) ONE_AT_A_TIME ANGRY
Quake VR: makes monsters here when used, with a teleporter's flash: "weapon" which kind (as func_enemy_dispenser's: 0
grunt, 1 ogre, 2 zombie, 7 dog, 8 enforcer...), "count" how many a use makes (default 1), "wait" the seconds between
them (default 0.5; one waits while something stands on the spot), "angle" the way they face. "target" is fired (the
user the activator) once every monster of its latest use is dead: chain waves with trigger_counter and trigger_relay.
ONE_AT_A_TIME: a use while one of its monsters lives makes none ("message" centerprinted to the player who used it).
ANGRY: they go for the player who used it at once.
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
    sound(self, CHAN_AUTO, k < 1 ? "vr/crate_break1.wav" : (k < 2 ? "vr/crate_break2.wav" : "vr/crate_break3.wav"), 1,
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
10), firing its "target" (the activator: what broke it). vrtutorial2's firing range: a trigger_counter opens the way on.
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
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if((e.classname == cls || e.classname == cls2) && e.modelindex &&
           vlen((e.absmin + e.absmax) * 0.5 - xRestock.origin) <= xRestock.distance)
        {
            n++;
        }
    }
    return n;
};

void() VR_Restock_Put =
{
    if(VR_Restock_Near(self) >= self.count)
    {
        return;
    }
    entity e = VR_Tut_Make(self.contents, self.origin, self.contentsflags, self.model, self.angles);
    if(e != world)
    {
        sound(self, CHAN_VOICE, "items/itembk2.wav", 0.6, ATTN_NORM);
        dprint(sprintf("restock: a new %s at %v\n", self.contents, self.origin));
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

void() VR_Checkpoint_Take =
{
    if(self.vr_cp_taken)
    {
        return;
    }
    self.vr_cp_taken = time + 0.001;
    self.nextthink = 0;
    dprint(sprintf("checkpoint %s taken at %g\n", self.targetname, time));
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
    if(coop || deathmatch || VR_Checkpoint_Latest() == world)
    {
        return FALSE;
    }
    float keys = self.items & (IT_KEY1 | IT_KEY2);
    SetNewParms();
    PutClientInServer();
    self.items = self.items | keys;
    dprint(sprintf("checkpoint: back at %v\n", self.origin));
    return TRUE;
};
