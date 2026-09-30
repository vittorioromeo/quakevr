// vr_debris.qc -- rocks and bricks lying about the maps (docs/vr-port/ROUND21.md, "Rocks and bricks"). The engine
// (vr_debris.cpp) finds their places at the foot of walls by what the textures are made of; here they are spawned, at
// the first server frame of a new map (after the map's own entities, so theirs keep their numbers; before the
// baselines, so a piece lying still costs the network next to nothing), and made carried props like the gibs:
// gripped, force grabbed, punched with (a rock in the fist: Box Punch Damage, its Melee Damage x and its mass, vr_melee.qc)
// and thrown (vr_carry.qc: they hurt what they hit by their speed, their Throw Damage x and their mass), Box3D rigid bodies (vr_box3d.cpp) asleep until
// touched. A saved game keeps them as they are (ordinary entities); a new map places its own.

.float vr_debris;       // 1 a rock, 2 a brick
.float vr_debris_sound; // when it last sounded a hit

float vr_debris_done;   // placed in this map (a saved game's are in it already)

// The models, in vr_debris.cpp's order; always precached (a saved game may hold pieces whatever the settings now).
void() VR_Debris_Precache =
{
    vr_debris_done = FALSE;
    precache_model("progs/vr_rock1.mdl");
    precache_model("progs/vr_rock2.mdl");
    precache_model("progs/vr_rock3.mdl");
    precache_model("progs/vr_rock4.mdl");
    precache_model("progs/vr_rock5.mdl");
    precache_model("progs/vr_brick1.mdl");
    precache_model("progs/vr_brick2.mdl");
    precache_model("progs/vr_brick3.mdl");
    precache_model("progs/vr_brick4.mdl");
    precache_sound("vr/rock1.wav"); // a stone's thud (Misc/quakevr/make_sounds.py)
    precache_sound("vr/rock2.wav");
    precache_sound("vr/rock3.wav");
    precache_sound("vr/brick1.wav"); // a brick's clack
    precache_sound("vr/brick2.wav");
    precache_sound("vr/brick3.wav");
};

// A piece's knock (a rock's thud, a brick's clack), one of three at random.
string(entity e) VR_Debris_Sound =
{
    float k = floor(random() * 3);
    if(e.vr_debris == 2)
    {
        return k < 1 ? "vr/brick1.wav" : (k < 2 ? "vr/brick2.wav" : "vr/brick3.wav");
    }
    return k < 1 ? "vr/rock1.wav" : (k < 2 ? "vr/rock2.wav" : "vr/rock3.wav");
};

// Whether `e` is a rock or a brick.
float(entity e) VR_Debris_Is =
{
    return e.vr_debris > 0;
};

// Box3D's hardest hit of a frame (vr_box3d.cpp callShocks: on the level, a prop, a monster; not a player's body or
// hands). Its knock is the engine's now, as every prop's (vr_physsound.cpp: rock1..3 and brick1..3 by how hard and how
// heavy, at most every vr_physsound_interval); this only reports it (developer 1).
void(float xSpeed) VR_Debris_Impact =
{
    if(xSpeed < 1.5 || time < self.vr_debris_sound + 0.1)
    {
        return;
    }
    self.vr_debris_sound = time;
    dprint(sprintf("debris: %s knocks at %.1f m/s\n", self.classname, xSpeed));
};

// Touching what it hits: thrown hard, it hurts it as a thrown box does (forcegrabbable_touch; its base damage is set
// at the throw, its Throw Damage x and mass at the hit: VR_Carry_Release, VR_Thrown_Damage).
void() VR_Debris_Touch =
{
    forcegrabbable_touch();
};

void(entity e, float kind) VR_Debris_Setup =
{
    e.classname = kind == 2 ? "vr_brick" : "vr_rock";
    e.vr_debris = kind;
    e.movetype = MOVETYPE_TOSS;
    e.solid = SOLID_NOT_BUT_TOUCHABLE;
    e.flags = e.flags | FL_FORCEGRABBABLE | FL_EASYHANDTOUCH;
    e.touch = VR_Debris_Touch;
    e.handtouch = SUB_Null; // nothing to take (VR_Carry_Setup keeps it as carry_use)
    e.throwhit = QVR_THROWHIT_HIT;
    e.owner = world;
    SetThrownWeaponDealDamage(e, FALSE);
    SetThrownWeaponDisappearOnHit(e, FALSE);
    VR_Carry_Setup(e); // carried, nudged, thrown; a rigid body (vr_rigid 1)
    e.vr_impact = VR_Debris_Impact;
};

/*QUAKED vr_debris_piece (0.6 0.5 0.4) (-6 -6 0) (6 6 8)
Quake VR: one of the rocks or bricks that lie about the maps, placed by hand (the firing range's prop table): taken,
thrown and swung as they are, and tuned in Held Object Offsets. "model": progs/vr_rock1.mdl to vr_rock5.mdl, or
progs/vr_brick1.mdl (whole), vr_brick2.mdl (chipped), vr_brick3.mdl (a half), vr_brick4.mdl (a broken piece); the
first rock if not given. "angle": its yaw; "skin": which of its skins. Its size is the pieces' (vr_world_scale); it
rests on whatever is below its origin (within 128 units). Placed whatever Rocks and Bricks' options are.
*/
void() vr_debris_piece =
{
    if(self.model == "")
    {
        self.model = "progs/vr_rock1.mdl";
    }
    setmodel(self, self.model); // (precached: VR_Debris_Precache)
    traceline(self.origin + '0 0 1', self.origin - '0 0 128', TRUE, self);
    setorigin(self, trace_endpos);
    float kind = debrisplace(self);
    if(!kind)
    {
        dprint(sprintf("vr_debris_piece at %v: %s is not a rock or a brick\n", self.origin, self.model));
        remove(self);
        return;
    }
    VR_Debris_Setup(self, kind);
};

// Every server frame (StartFrame): at the first of a new map, the pieces.
void() VR_Debris_Frame =
{
    if(vr_debris_done)
    {
        return;
    }
    vr_debris_done = TRUE;
    if(spawnServerFromSaveFile)
    {
        return; // a saved game: its entities replace these
    }
    float n = debrisplan();
    for(float i = 0; i < n; i++)
    {
        entity e = spawn();
        setmodel(e, debrismodel(i));
        float kind = debrisput(e, i); // its skin, size, turn, place and box
        if(!kind)
        {
            remove(e);
            continue;
        }
        VR_Debris_Setup(e, kind);
    }
};
