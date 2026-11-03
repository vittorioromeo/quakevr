// vr_deadanim_test 20 / 21 / 22 (vr_deadleap_test.qc; Misc/quakevr/deadleap_test.sh): a leaping monster (20 a fiend, 21 a
// dog, 22 a spawn) dies and stays dead: each scene makes one (ragdolls on), kills it in its own way, on the ground or in
// mid-leap, watches it 5 s and fails if its body runs its AI again (its think one of its AI's, its leap's touch back on),
// or moves on after it has had time to fall (more than VR_DL_FAR, 1.5 s on: a leap is hundreds of units). Scenes 12-18
// poke its lying corpse. Its lines: VR_Test_Check's, the last `deadleap: N passed, M failed`.
#ifdef QVR_TESTS
entity dl_m;
entity dl_prey;       // (scene 9: what its leap strikes: its death kills the fiend inside the fiend's own touch)
float dl_ask;         // 20-22 (0: none running)
float dl_scene;
float dl_stage;       // 0 spawned, 1 leaping (waits to be in the air), 2 dead, watched
float dl_t0;
float dl_ai;          // frames its think was one of its AI's
float dl_touch;       // frames its touch was back on
float dl_far;         // how far it moved after 1.5 s
float dl_poked;
vector dl_settled;
float VR_DL_LAST = 18;
float VR_DL_FAR = 64;
float VR_DL_WATCH = 5;

string(float xScene) VR_DL_Name =
{
    if(xScene == 1) { return "shot on the ground"; }
    if(xScene == 2) { return "shot in mid-leap"; }
    if(xScene == 3) { return "a blow on the ground"; }
    if(xScene == 4) { return "a blow in mid-leap"; }
    if(xScene == 5) { return "neck snapped standing"; }
    if(xScene == 6) { return "neck snapped in mid-leap"; }
    if(xScene == 7) { return "knocked down, shot"; }
    if(xScene == 8) { return "staggered, shot"; }
    if(xScene == 9) { return "dies inside its own leap's touch"; }
    if(xScene == 10) { return "killed by a thrown thing"; }
    if(xScene == 11) { return "shot in mid-leap by a thrown thing"; }
    if(xScene == 12) { return "corpse shot"; }
    if(xScene == 13) { return "corpse struck"; }
    if(xScene == 14) { return "corpse shoved"; }
    if(xScene == 15) { return "corpse taken hold of"; }
    if(xScene == 16) { return "corpse provoked"; }
    if(xScene == 17) { return "corpse knocked down"; }
    return "corpse's use fired";
};

// (Scene 9: self the thing its leap strikes, dying of it: its blast kills its striker, as a barrel's does.)
void() VR_DL_PreyDie =
{
    self.takedamage = DAMAGE_NO;
    dl_m.health = 1;
    T_RadiusDamage(self, self, 160, world);
};

void(entity xBy, float xBlow) VR_DL_Kill =
{
    entity oself = self;
    self = xBy;
    if(xBlow)
    {
        hit_kind = QVR_HITKIND_MELEE;
        hit_hand = 0;
        T_Damage_VRMelee(dl_m, xBy, xBy, dl_m.health + 5);
        hit_kind = QVR_HITKIND_NONE;
    }
    else
    {
        T_Damage(dl_m, xBy, xBy, dl_m.health + 5);
    }
    self = oself;
};

void() VR_DL_Next =
{
    if(dl_m)
    {
        remove(dl_m);
        dl_m = world;
    }
    dl_scene = dl_scene + 1;
    if(dl_scene > VR_DL_LAST)
    {
        VR_Test_Verdict();
        dl_ask = 0;
        return;
    }
    entity p = VR_Player();
    makevectors('0 1 0' * p.angles_y);
    entity oself = self;
    self = spawn();
    setorigin(self, p.origin + v_forward * 220 + '0 0 8');
    self.angles_y = p.angles_y + 180;
    if(dl_ask == 21)
    {
        self.classname = "monster_dog";
        monster_dog();
    }
    else if(dl_ask == 22)
    {
        self.classname = "monster_tarbaby";
        monster_tarbaby();
    }
    else
    {
        self.classname = "monster_demon1";
        monster_demon1();
    }
    dl_m = self;
    self = oself;
    dl_stage = 0;
    dl_t0 = time;
    dl_ai = 0;
    dl_touch = 0;
    dl_far = 0;
    dl_poked = 0;
    dl_settled = '0 0 0';
};

// The kill, its scene's (the monster is up, or in the air).
void() VR_DL_DoKill =
{
    entity p = VR_Player();
    entity oself = self;
    vector dir = dl_m.origin - p.origin;
    dir_z = 0;
    dir = normalize(dir);
    float s = dl_scene;
    if(s == 3 || s == 4)
    {
        VR_DL_Kill(p, TRUE);
    }
    else if(s == 5 || s == 6)
    {
        self = p;
        VR_FoeGrab_NeckSnap(dl_m, 1, 0);
        self = oself;
    }
    else if(s == 7)
    {
        VR_Knockdown_Start(dl_m);
        VR_DL_Kill(p, FALSE);
    }
    else if(s == 8)
    {
        if(ragdollcanstagger(dl_m))
        {
            VR_Knockdown_Stagger(dl_m, dir, 0.4, 1);
        }
        VR_DL_Kill(p, FALSE);
    }
    else if(s == 9)
    {
        dl_prey = spawn();
        dl_prey.classname = "vr_dl_prey";
        dl_prey.takedamage = DAMAGE_YES;
        dl_prey.health = 1;
        dl_prey.th_die = VR_DL_PreyDie;
        dl_prey.solid = SOLID_NOT;
        setorigin(dl_prey, dl_m.origin + '16 0 0');
        dl_m.velocity = '600 0 -50';
        // (Down at the floor as it strikes it: its touch goes on as if it landed.)
        tracebox(dl_m.origin, dl_m.mins, dl_m.maxs, dl_m.origin - '0 0 200', TRUE, dl_m);
        setorigin(dl_m, trace_endpos + '0 0 1');
        self = dl_m;
        other = dl_prey;
        if(self.touch != SUB_Null)
        {
            self.touch();
        }
        self = oself;
        remove(dl_prey);
        dl_prey = world;
        if(dl_m.health > 0)
        {
            VR_DL_Kill(p, FALSE); // (a leap whose touch deals no blow, the velocity gone: killed all the same)
        }
    }
    else if(s == 10 || s == 11)
    {
        dl_prey = spawn();
        dl_prey.classname = "vr_dl_thrown";
        dl_prey.solid = SOLID_NOT;
        VR_DL_Kill(dl_prey, FALSE);
        remove(dl_prey);
        dl_prey = world;
    }
    else
    {
        VR_DL_Kill(p, FALSE);
    }
};

// The corpse's poke, its scene's.
void() VR_DL_Poke =
{
    entity p = VR_Player();
    entity oself = self;
    vector dir = dl_m.origin - p.origin;
    dir_z = 0;
    dir = normalize(dir);
    float s = dl_scene;
    self = p;
    if(s == 12)
    {
        T_Damage(dl_m, p, p, 1);
    }
    else if(s == 13)
    {
        hit_kind = QVR_HITKIND_MELEE;
        hit_hand = 0;
        T_Damage_VRMelee(dl_m, p, p, 1);
        hit_kind = QVR_HITKIND_NONE;
    }
    else if(s == 14)
    {
        VR_Bash_Hit(dl_m, dl_m.origin, dir, QVR_HITKIND_SHOVE, 0, 1, 1);
    }
    else if(s == 15)
    {
        dl_m.enemy = world; // (killed by nothing: as a fall's or a slime's)
        VR_FoeGrab_Taken(dl_m, 0, 0);
    }
    else if(s == 16)
    {
        VR_Provoke(dl_m, p);
    }
    else if(s == 17)
    {
        VR_Knockdown_Try(dl_m, dir, QVR_HITKIND_SHOVE, 0, 1);
    }
    else if(dl_m.use)
    {
        activator = p;
        self = dl_m;
        self.use();
    }
    self = oself;
};

void() VR_DeadLeap_Frame =
{
    if(!dl_ask)
    {
        return;
    }
    if(!dl_m)
    {
        VR_DL_Next();
        return;
    }
    if(dl_stage == 0)
    {
        if((time < dl_t0 + 0.8) || !dl_m.takedamage)
        {
            return;
        }
        entity p = VR_Player();
        dl_m.enemy = p;
        float s = dl_scene;
        if(s == 2 || s == 4 || s == 6 || s == 9 || s == 11)
        {
            entity oself = self;
            self = dl_m;
            self.th_missile();
            self = oself;
            dl_stage = 1;
            dl_t0 = time;
            return;
        }
        VR_DL_DoKill();
        dl_stage = 2;
        dl_t0 = time;
        return;
    }
    if(dl_stage == 1)
    {
        if(!(dl_m.flags & FL_ONGROUND) && (dl_m.velocity_z != 0))
        {
            {
                VR_DL_DoKill();
                dl_stage = 2;
                dl_t0 = time;
            }
        }
        else if(time > dl_t0 + 3)
        {
            bprint(sprintf("deadleap: %g %s: never left the ground: killed where it stands\n", dl_scene, VR_DL_Name(dl_scene)));
            VR_DL_DoKill();
            dl_stage = 2;
            dl_t0 = time;
        }
        return;
    }
    if(!dl_m.modelindex || dl_m.vr_gib)
    {
        // (Gibbed: no body to rise.)
        VR_Test_Check(TRUE, sprintf("%g %s: gibbed", dl_scene, VR_DL_Name(dl_scene)));
        dl_m = world;
        return;
    }
    if((dl_scene >= 12) && !dl_poked && (time > dl_t0 + 2))
    {
        dl_poked = TRUE;
        VR_DL_Poke();
    }
    if(VR_DeadAnim_AiThink(dl_m) && (dl_m.nextthink > 0))
    {
        if(!dl_ai)
        {
            bprint(sprintf("deadleap: %g: runs its AI (which %g) %.2f s after its death, frame %g\n", dl_scene,
                VR_DeadAnim_AiThink(dl_m), time - dl_t0, dl_m.frame));
        }
        dl_ai = dl_ai + 1;
    }
    if(dl_m.touch != SUB_Null)
    {
        dl_touch = dl_touch + 1;
    }
    if((time > dl_t0 + 1.5) && !dl_settled)
    {
        dl_settled = dl_m.origin;
    }
    if(dl_settled)
    {
        dl_far = max(dl_far, vlen(dl_m.origin - dl_settled));
    }
    if(time < dl_t0 + VR_DL_WATCH)
    {
        return;
    }
    float pass = ((dl_ai == 0) && (dl_touch == 0) && (dl_far <= VR_DL_FAR) && (dl_m.health <= 0));
    string state = sprintf("health %g, frame %g, ragdoll %g, corpse %g, solid %g", dl_m.health, dl_m.frame, isragdoll(dl_m),
        dl_m.vr_corpse, dl_m.solid);
    VR_Test_Check(pass, sprintf("%g %s (%s; AI frames %g, touch frames %g, moved %.0f after settling)", dl_scene,
        VR_DL_Name(dl_scene), state, dl_ai, dl_touch, dl_far));
    VR_DL_Next();
};

void(float xAsk) VR_DeadLeap_Start =
{
    entity p = VR_Player();
    p.flags = p.flags | FL_NOTARGET | FL_GODMODE;
    dl_ask = xAsk;
    dl_scene = 0;
    VR_Test_BeginFlags("deadleap", VR_TEST_LOUD);
    dl_m = world;
};
#else
void() VR_DeadLeap_Frame = {};
void(float xAsk) VR_DeadLeap_Start = {};
#endif
