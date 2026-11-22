// ----------------------------------------------------------------------------
// VR: player ragdolls (vr_death_view; ROUND21.md, "Player ragdolls"; the engine's vr_deathview.cpp). A player dying
// whole (not gibbed: his health not under -40) leaves a body of his own where he stood: progs/player.mdl in the pose
// he was in, moving as he moved, the killing blow's push times vr_death_ragdoll_push (at most 400 units/s), a corpse as a monster's
// (VR_Corpse_Arm: it lies still at once, so the engine makes it a ragdoll at once, vr_box3d.cpp; shots and blows gib
// it, VR_Corpse_Parts). Its death animation plays for the players whose engine draws no ragdoll (a remote client).
// The player himself stays where he was (no death hop: the view where his eyes were) and is not sent to the others
// while he is dead (the engine's sv_main.c: they see the body). His death view (Off, Third Person, Immersive) is the
// client's. Respawning, his body stays as a body (no copy to the body queue); the oldest ragdolls give way to new ones
// (VR_Knockdown_MakeRoom) but a dead player's own body only once he is up again.

float VR_DEATHDOLL_MAXSPEED = 400; // units/s: the fastest a body is thrown when made (a blast throws it too: vr_box3d.cpp)

// The avatar's own ragdoll (avatardoll, MP_PLAN.md "The avatar's own ragdoll"): the body is his avatar's build as a .mdl
// (Misc/quakevr/make_vrdoll.py: its authored rig quakevr/rigs/vrdoll*.rig2.txt), sized to his height and world scale
// (avatarbody: his calibration as the server has it, single player your settings, a bot the default build), so the
// ragdoll's parts are his body's own; each client draws his avatar (build, look, hands) on its bones (vr_mpavatar.cpp
// addDolls). id's ranger (progs/player.mdl) where there is none: vr_mp_body 0, his body mode none, a worldspawn that did not
// precache them (VR_DeathDoll_Precache), a data folder without them.
float vr_deathdoll_models; // TRUE: this map precached every build's model

void() VR_DeathDoll_Precache =
{
    if(fileexists("progs/vrdoll_lean.mdl") && fileexists("progs/vrdoll.mdl") && fileexists("progs/vrdoll_brawny.mdl") &&
       fileexists("progs/vrdoll_fem.mdl"))
    {
        precache_model("progs/vrdoll_lean.mdl");
        precache_model("progs/vrdoll.mdl");
        precache_model("progs/vrdoll_brawny.mdl");
        precache_model("progs/vrdoll_fem.mdl");
        vr_deathdoll_models = TRUE;
    }
};

// Body b made as p's avatar's own (TRUE), or not (FALSE: id's ranger). Its feet where his were (the model's at -24 scaled).
float(entity b, entity p) VR_DeathDoll_Avatar =
{
    if(!vr_deathdoll_models)
    {
        return FALSE;
    }
    vector ab = avatarbody(p);
    if(ab_x < 0 || ab_y <= 0)
    {
        return FALSE;
    }
    if(ab_x < 1)      { setmodel(b, "progs/vrdoll_lean.mdl"); }
    else if(ab_x < 2) { setmodel(b, "progs/vrdoll.mdl"); }
    else if(ab_x < 3) { setmodel(b, "progs/vrdoll_brawny.mdl"); }
    else              { setmodel(b, "progs/vrdoll_fem.mdl"); }
    setsize(b, VEC_HULL_MIN, VEC_HULL_MAX);
    b.model_scale = '1 1 1' * (ab_y - 1); // (an offset from 1: the engine's ragdoll and drawing scale)
    b.frame = 0;
    if(ab_y > 1)
    {
        setorigin(b, b.origin + '0 0 24' * (ab_y - 1)); // (the feet, at -24 * size, on the floor)
    }
    return TRUE;
};

// Whether `p` has a ragdoll body lying there now.
float(entity p) VR_DeathDoll_Has =
{
    return p.vr_deathdoll_body != world && p.vr_deathdoll_body.vr_deathdoll == p;
};

// Whether body `b` is a dead player's own body (VR_Knockdown_MakeRoom keeps it).
float(entity b) VR_DeathDoll_InUse =
{
    return b.vr_deathdoll != world && b.vr_deathdoll.health <= 0 && b.vr_deathdoll.vr_deathdoll_body == b;
};

// The body's death animation (seen where it is drawn animated), a frame each tenth of a second.
void() VR_DeathDoll_Anim =
{
    self.frame = self.vr_deathdoll_next;
    if(self.vr_deathdoll_next >= self.vr_deathdoll_last)
    {
        self.think = SUB_Null;
        return;
    }
    self.vr_deathdoll_next = self.vr_deathdoll_next + 1;
    self.nextthink = time + 0.1;
};

// From PlayerDie (before the death hop): `p`'s body made, if vr_death_view is on and he is not gibbed. TRUE if made.
float(entity p) VR_DeathDoll_Make =
{
    if(cvar_hget(cvarh_vr_death_view) < 1 || p.health < -40)
    {
        return FALSE;
    }
    entity b = spawn();
    b.classname = "player_body";
    setmodel(b, "progs/player.mdl");
    setsize(b, VEC_HULL_MIN, VEC_HULL_MAX);
    setorigin(b, p.origin);
    b.angles = '0 0 0';
    b.angles_y = p.angles_y;
    b.frame = p.frame;
    if(p.modelindex != b.modelindex || b.frame > 142)
    {
        b.frame = 12; // (the eyes', the hammer's: standing)
    }
    b.skin = p.skin;
    b.colormap = p.colormap;
    b.movetype = MOVETYPE_TOSS;
    b.flags = FL_MONSTER; // (a dead monster's body to the engine: its ragdoll, its corpse)
    b.health = -1;
    // Its motion: his, the killing blow's push (in it already: T_Damage) times vr_death_ragdoll_push, at most
    // VR_DEATHDOLL_MAXSPEED (Quake's push is the damage times 8: a rocket's would throw it across the room).
    b.velocity = p.velocity;
    if(p.vr_deathdoll_pushtime == time)
    {
        b.velocity = b.velocity + p.vr_deathdoll_push * (cvar_hget(cvarh_vr_death_ragdoll_push) - 1);
    }
    if(vlen(b.velocity) > VR_DEATHDOLL_MAXSPEED)
    {
        b.velocity = normalize(b.velocity) * VR_DEATHDOLL_MAXSPEED;
    }
    b.vr_deathdoll = p;
    b.vr_corpse_since = time;
    // (his plates as he died, the body's own from now on: sent for it as his are, never his live ones again -- his
    // notes 2026-11-22, the body gained the armour he picked up after his respawn)
    b.vr_pp_type = p.vr_pp_type;
    b.vr_pp_mask = p.vr_pp_mask;

    // Its death animation (the one his own death code would play: the axe's with the axe).
    float first = 41;
    float last = 49;
    if(p.weapon != WID_AXE)
    {
        float i = floor(random() * 5);
        if(i < 1)      { first = 50; last = 60; }
        else if(i < 2) { first = 61; last = 69; }
        else if(i < 3) { first = 70; last = 84; }
        else if(i < 4) { first = 85; last = 93; }
        else           { first = 94; last = 102; }
    }
    b.vr_deathdoll_next = first;
    b.vr_deathdoll_last = last;
    b.think = VR_DeathDoll_Anim;
    b.nextthink = time + 0.1;
    if(VR_DeathDoll_Avatar(b, p))
    {
        b.think = SUB_Null; // (one frame, no death animation: a client drawing no ragdoll draws it standing)
        b.nextthink = 0;
    }

    VR_Corpse_Arm(b); // (a corpse lying still: the engine's ragdoll at once, gibbed by enough damage)
    b.solid = SOLID_NOT_BUT_TOUCHABLE;
    setorigin(b, b.origin);

    p.vr_deathdoll_body = b;
    dprint(sprintf("deathdoll: %s's body made, moving %v\n", p.netname, b.velocity));
    return TRUE;
};
