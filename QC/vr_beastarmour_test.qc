#ifdef QVR_TESTS
// vr_beastarmour_test.qc -- the beasts' armour's test (vr_agarmor.qc VR_BeastArmour_*, vr_beastarmour.cpp; ROUND26.md,
// "beastarmour"; Debug > Tests). `vr_agarmor_test 15`: a rottweiler, an ogre, a scrag and a fiend ahead of the player:
// each kind's slots and cuirass group as the data says, a full set given (only its own pieces), every piece keeping its
// size through every frame of its kind (the scrag drawn a quarter larger, its pieces too), the client told and every
// piece drawn, the pieces following the body from one animation frame to another (a limb's piece moves, two pieces on one
// bone keep their distance), a shot at the cuirass and at the helmet meeting that piece, Plate Blocks All stopping a hit
// the bare body takes, a helmet broken (its bit off, its debris made), the skill's roll dressing a spawned beast (Nightmare
// at a chance of 1), and two killed (the corpses keeping their pieces, on the body as it lies, at their size). Ends on
// "beastarmour test: N passed, M failed".
// `vr_agarmor_test 16`: the scene (scenario armour_beasts): the four in full sets (green, yellow, red, yellow), calm (the
// player notarget). `17`-`20`: one of them close ahead in full yellow, three quarters on (the renders).

entity vr_bat_e0, vr_bat_e1, vr_bat_e2, vr_bat_e3;
float vr_bat_busy;   // the test running
float vr_bat_state;
float vr_bat_wait;   // the state's next step at this time (game time: the frames the logic needs)
float vr_bat_until;  // give up
float vr_bat_rule;   // the setting put back after
vector vr_bat_was0, vr_bat_was1, vr_bat_was2, vr_bat_was3; // a limb piece's place in frame A
float vr_bat_gap0, vr_bat_gap1, vr_bat_gap2, vr_bat_gap3;  // two pieces on one bone: their distance in frame A
vector vr_bat_dead0, vr_bat_dead1; // where the killed ones died
entity vr_bat_render; // 17-20's beast
float vr_bat_sweep;     // the frame swept (state 5: every frame of every kind)
float vr_bat_size0[40]; // each beast's (4) pieces' (10) size as drawn at the sweep's first frame (limbpiece 18: the box's diagonal)
float vr_bat_spread[4]; // the most any of its pieces' sizes strayed from that, as a share
float vr_bat_spreadd;   // the same for the corpses through their fall and ragdoll (state 4's wait)

entity(float i) VR_BAT_Ent =
{
    if(i == 0) return vr_bat_e0;
    if(i == 1) return vr_bat_e1;
    if(i == 2) return vr_bat_e2;
    return vr_bat_e3;
};

// Kind `kind` (0 rottweiler, 1 ogre, 2 scrag, 3 fiend) `ahead` units before player `p`, `side` to its right, turned
// `turn` degrees from facing it.
entity(entity p, float kind, float side, float ahead, float turn) VR_BAT_Spawn =
{
    makevectors('0 1 0' * p.angles_y);
    entity oself = self;
    self = spawn();
    setorigin(self, p.origin + v_forward * ahead + v_right * side + (kind == 2 ? '0 0 24' : '0 0 8'));
    self.angles_y = vectoyaw(p.origin - self.origin) + turn;
    if(kind == 0) { self.classname = "monster_dog"; monster_dog(); }
    else if(kind == 1) { self.classname = "monster_ogre"; monster_ogre(); }
    else if(kind == 2) { self.classname = "monster_wizard"; monster_wizard(); }
    else { self.classname = "monster_demon1"; monster_demon1(); }
    entity g = self;
    self = oself;
    g.health = 1000;
    return g;
};

// A hit of `d` at `at` on `g` (the body's zone): the health it took.
float(entity g, vector at, float d) VR_BAT_Hit =
{
    float h0 = g.health;
    VR_Hit_SetZone(g, 0, at);
    T_Damage(g, world, world, d);
    return h0 - g.health;
};

// The slots each kind's data gives (Misc/quakevr/make_beastarmour.py) and its cuirass group.
float(float kind) VR_BAT_Slots = { return kind == 0 ? 975 : kind == 2 ? 831 : 1023; };
float(float kind) VR_BAT_Group = { return kind == 0 ? 257 : 769; };
// Two of its pieces on one bone (the chest), and a limb's piece.
float(float kind) VR_BAT_PairB = { return kind == 1 ? 8 : 9; };
// Each kind's frames (the sweep's: every one, deaths included).
float(float kind) VR_BAT_Frames = { return kind == 0 ? 86 : kind == 1 ? 149 : kind == 2 ? 55 : 69; };

// Beast `i`'s pieces' sizes now against their first (`first`: record them): the most one strayed, as a share.
float(float i, float first) VR_BAT_SizeSpread =
{
    entity e = VR_BAT_Ent(i);
    float k, worst = 0;
    for(k = 0; k < VR_AR_PIECES; k++)
    {
        if(!VR_AgArmor_On(e, k)) continue;
        vector size = limbpiece(e, k, 18);
        float sz = size_y;
        if(first) vr_bat_size0[i * 10 + k] = sz;
        float s0 = vr_bat_size0[i * 10 + k];
        worst = max(worst, s0 > 0 ? fabs(sz - s0) / s0 : 1);
    }
    return worst;
};

// Frames A (standing) and B (another pose: run, swing, attack) of each kind.
float(float kind) VR_BAT_FrameA = { return kind == 0 ? 69 : 0; };
float(float kind) VR_BAT_FrameB = { return kind == 0 ? 48 : kind == 1 ? 33 : kind == 2 ? 29 : 54; };

float(entity e) VR_BAT_Count =
{
    float n = 0;
    float i;
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        n = n + ((e.vr_ar_mask & VR_AgArmor_Bit(i)) != 0);
    }
    return n;
};

// The farthest a limb's piece (2..7) moved between `was` (its places, packed by VR_BAT_LimbAt) and now.
vector(entity e) VR_BAT_LimbAt =
{
    float i;
    for(i = 2; i < 8; i++)
    {
        if(VR_AgArmor_On(e, i)) return limbpiece(e, i, 10) - e.origin;
    }
    return '0 0 0';
};

void(float i, entity e, float kind) VR_BAT_RecordA =
{
    vector at = VR_BAT_LimbAt(e);
    float gap = vlen(limbpiece(e, 0, 10) - limbpiece(e, VR_BAT_PairB(kind), 10));
    if(i == 0) { vr_bat_was0 = at; vr_bat_gap0 = gap; }
    else if(i == 1) { vr_bat_was1 = at; vr_bat_gap1 = gap; }
    else if(i == 2) { vr_bat_was2 = at; vr_bat_gap2 = gap; }
    else { vr_bat_was3 = at; vr_bat_gap3 = gap; }
};

void(float req) VR_BAT_Frame =
{
    entity p = VR_Player();
    entity e;
    float i;
    if(req >= 16)
    {
        // (the scene and the renders: dressed and left calm)
        p.flags = p.flags | FL_NOTARGET;
        if(req == 16)
        {
            VR_AgArmor_Give(VR_BAT_Spawn(p, 0, -84, 150, 0), 1, 1023);
            VR_AgArmor_Give(VR_BAT_Spawn(p, 1, -28, 170, 0), 2, 1023);
            VR_AgArmor_Give(VR_BAT_Spawn(p, 2, 28, 150, 0), 3, 1023);
            VR_AgArmor_Give(VR_BAT_Spawn(p, 3, 84, 170, 0), 2, 1023);
            return;
        }
        if(vr_bat_render)
        {
            remove(vr_bat_render); // (the one before: one at a time, for a row of renders)
        }
        vr_bat_render = VR_BAT_Spawn(p, req - 17, 0, req == 19 ? 90 : req == 18 ? 110 : 130, 35);
        VR_AgArmor_Give(vr_bat_render, 2, 1023);
        return;
    }
    if(req == 15)
    {
        VR_Test_BeginFlags("beastarmour test", VR_TEST_LOUD);
        vr_bat_busy = TRUE;
        vr_bat_rule = cvar_hget(cvarh_vr_agarmor_rule);
        vr_bat_spreadd = 0;
        p.flags = p.flags | FL_NOTARGET;
        vr_bat_e0 = VR_BAT_Spawn(p, 0, -96, 170, 0);
        vr_bat_e1 = VR_BAT_Spawn(p, 1, -32, 190, 0);
        vr_bat_e2 = VR_BAT_Spawn(p, 2, 32, 170, 0);
        vr_bat_e3 = VR_BAT_Spawn(p, 3, 96, 190, 0);
        vr_bat_state = 1;
        vr_bat_wait = time + 0.8; // (their start_go: up to half a second's spread)
        vr_bat_until = time + 20;
        return;
    }
    if(time > vr_bat_until)
    {
        VR_Test_Check(FALSE, sprintf("timed out in state %g", vr_bat_state));
        vr_bat_state = 9;
    }
    if(vr_bat_state == 4)
    {
        // (the corpses' pieces' sizes through their death frames and ragdolls, against the living's)
        for(i = 0; i < 2; i++)
        {
            vr_bat_spreadd = max(vr_bat_spreadd, VR_BAT_SizeSpread(i, FALSE));
        }
    }
    if(time < vr_bat_wait && vr_bat_state != 9)
    {
        return;
    }
    if(vr_bat_state == 1)
    {
        for(i = 0; i < 4; i++)
        {
            e = VR_BAT_Ent(i);
            float sl = ragdollbone(e, '0 0 0', QVR_ARMOR_BEAST);
            float gr = ragdollbone(e, '1 0 0', QVR_ARMOR_BEAST);
            VR_Test_Check(sl == VR_BAT_Slots(i) && gr == VR_BAT_Group(i), sprintf("%s: slots %g (%g), cuirass group %g (%g)",
                e.classname, sl, VR_BAT_Slots(i), gr, VR_BAT_Group(i)));
            VR_AgArmor_Give(e, 2, 1);
            VR_Test_Check(e.vr_ar_mask == VR_BAT_Group(i), sprintf("%s: the cuirass alone gives its group (%g)", e.classname, e.vr_ar_mask));
            VR_AgArmor_Give(e, 2, 1023);
            VR_Test_Check(e.vr_ar_mask == VR_BAT_Slots(i) && e.vr_ar_hp[0] == VR_AgArmor_Full(2, 0),
                sprintf("%s: a full set is its own pieces (%g), the cuirass's pool %g", e.classname, e.vr_ar_mask, e.vr_ar_hp[0]));
            string m = VR_AgArmor_Model(e, 0);
            VR_Test_Check(m != "" && (VR_AgArmor_Model(e, 4) == "") == (i == 0), sprintf("%s: its cuirass's model %s", e.classname, m));
            e.nextthink = time + 60; // (frozen: its frames are the test's)
            e.think = SUB_Null;
            e.frame = 0;
        }
        // The sweep first: every frame of each kind, a frame a tick; the scrag drawn a quarter larger (its pieces with it).
        vr_bat_e2.scale = 1.25;
        vr_bat_sweep = 0;
        vr_bat_state = 5;
        return;
    }
    if(vr_bat_state == 5)
    {
        // Each piece's size the same in every frame (his note 2026-11-27: they grew and shrank as the animations played).
        for(i = 0; i < 4; i++)
        {
            e = VR_BAT_Ent(i);
            float sp = VR_BAT_SizeSpread(i, vr_bat_sweep == 0);
            vr_bat_spread[i] = vr_bat_sweep == 0 ? 0 : max(vr_bat_spread[i], sp);
            e.frame = min(vr_bat_sweep, VR_BAT_Frames(i) - 1);
        }
        vr_bat_sweep = vr_bat_sweep + 1;
        if(vr_bat_sweep <= VR_BAT_Frames(1)) return;
        for(i = 0; i < 4; i++)
        {
            e = VR_BAT_Ent(i);
            vector sc = limbpiece(e, 0, 18);
            VR_Test_Check(vr_bat_spread[i] < 0.01 && fabs(sc_x - (i == 2 ? 1.25 : 1)) < 0.01,
                sprintf("%s: its pieces keep their size through all %g frames (strayed %g; scale %g)", e.classname,
                VR_BAT_Frames(i), vr_bat_spread[i], sc_x));
            e.frame = VR_BAT_FrameA(i);
        }
        vr_bat_e2.scale = 0;
        vr_bat_state = 2;
        vr_bat_wait = time + 0.4; // (the client told, the frame's lerp done)
        return;
    }
    if(vr_bat_state == 2)
    {
        float want = 0;
        for(i = 0; i < 4; i++)
        {
            e = VR_BAT_Ent(i);
            want = want + VR_BAT_Count(e);
            float heard = ragdollbone(e, '1 0 0', QVR_ARMOR_CLIENT);
            VR_Test_Check(heard == e.vr_ar_mask, sprintf("%s: the client heard pieces %g (%g)", e.classname, heard, e.vr_ar_mask));
            VR_BAT_RecordA(i, e, i);
            e.frame = VR_BAT_FrameB(i);
        }
        float drawn = ragdollbone(world, '2 0 0', QVR_ARMOR_BEAST);
        VR_Test_Check(drawn == want, sprintf("every piece drawn: %g of %g", drawn, want));
        vr_bat_state = 3;
        vr_bat_wait = time + 0.4;
        return;
    }
    if(vr_bat_state == 3)
    {
        for(i = 0; i < 4; i++)
        {
            e = VR_BAT_Ent(i);
            vector was = i == 0 ? vr_bat_was0 : i == 1 ? vr_bat_was1 : i == 2 ? vr_bat_was2 : vr_bat_was3;
            float gap0 = i == 0 ? vr_bat_gap0 : i == 1 ? vr_bat_gap1 : i == 2 ? vr_bat_gap2 : vr_bat_gap3;
            float moved = vlen(VR_BAT_LimbAt(e) - was);
            float gap = vlen(limbpiece(e, 0, 10) - limbpiece(e, VR_BAT_PairB(i), 10));
            VR_Test_Check(moved > 0.5 && moved < 40, sprintf("%s: a limb's piece follows frame %g -> %g (moved %g)", e.classname,
                VR_BAT_FrameA(i), VR_BAT_FrameB(i), moved));
            VR_Test_Check(fabs(gap - gap0) < 0.75 && gap0 > 0.5, sprintf("%s: two pieces on its chest keep their distance (%g -> %g)",
                e.classname, gap0, gap));
            // A shot at the cuirass and at the helmet, from before each (out from the body's middle through it).
            float k;
            for(k = 0; k < 2; k++)
            {
                vector at = limbpiece(e, k, 10);
                vector out = normalize(at - (e.absmin + e.absmax) * 0.5);
                float met = ragdollbone(e, at, QVR_ARMOR_SHOT, -out);
                float near = ragdollbone(e, at, QVR_ARMOR_PIECE_AT);
                VR_Test_Check(met == k && near == k, sprintf("%s: a shot at piece %g meets %g, its point %g", e.classname, k, met, near));
            }
            // Plate Blocks All: a hit on the cuirass takes nothing; bare, the same hit hurts.
            cvar_set("vr_agarmor_rule", "1");
            vector pc = limbpiece(e, 0, 10);
            float d = VR_BAT_Hit(e, pc, 10);
            float type = e.vr_ar_type;
            float mask = e.vr_ar_mask;
            e.vr_ar_type = 0;
            float bare = VR_BAT_Hit(e, pc, 10);
            e.vr_ar_type = type;
            e.vr_ar_mask = mask;
            VR_Test_Check(d == 0 && bare > 0, sprintf("%s: 10 on the cuirass takes %g (bare %g)", e.classname, d, bare));
            // The helmet broken: its bit off, its debris made.
            float made = vr_ar_debris_made;
            d = VR_BAT_Hit(e, limbpiece(e, 1, 10), 400);
            VR_Test_Check(!VR_AgArmor_On(e, 1) && vr_ar_debris_made > made && d == 0,
                sprintf("%s: the helmet broke (pieces %g, debris %g, took %g)", e.classname, e.vr_ar_mask, vr_ar_debris_made - made, d));
            e.health = 1000;
        }
        cvar_set("vr_agarmor_rule", ftos(vr_bat_rule));
        // The skill's roll on a spawned beast: Nightmare, a chance of 1, the full set.
        float s0 = cvar_hget(cvarh_vr_agarmor_skill3), s1 = cvar_hget(cvarh_vr_agarmor_skill3_pieces), sk = cvar_hget(cvarh_skill);
        cvar_set("vr_agarmor_skill3", "1");
        cvar_set("vr_agarmor_skill3_pieces", "10");
        cvar_set("skill", "3");
        e = VR_BAT_Spawn(p, 1, 0, 300, 0);
        VR_Test_Check(e.vr_ar_type > 0 && e.vr_ar_mask == 1023, sprintf("the skill's roll dresses a spawned ogre (type %g, pieces %g)",
            e.vr_ar_type, e.vr_ar_mask));
        remove(e);
        cvar_set("vr_agarmor_skill3", ftos(s0));
        cvar_set("vr_agarmor_skill3_pieces", ftos(s1));
        cvar_set("skill", ftos(sk));
        // Two killed: the ogre and the rottweiler (small blows: corpses, not gibs).
        vr_bat_dead0 = vr_bat_e0.origin;
        vr_bat_dead1 = vr_bat_e1.origin;
        for(i = 0; i < 2; i++)
        {
            e = VR_BAT_Ent(i);
            e.health = 2;
            e.think = SUB_Null;
            float type4 = e.vr_ar_type;
            e.vr_ar_type = 0; // (the killing blow past its plates: a corpse wearing them, not a hit they absorb)
            T_Damage(e, world, world, 5);
            e.vr_ar_type = type4;
        }
        vr_bat_state = 4;
        vr_bat_wait = time + 3; // (their death animations and ragdolls)
        return;
    }
    if(vr_bat_state == 4)
    {
        for(i = 0; i < 2; i++)
        {
            e = VR_BAT_Ent(i);
            vector died = i == 0 ? vr_bat_dead0 : vr_bat_dead1;
            vector pc4 = limbpiece(e, 0, 10);
            VR_Test_Check(e.health <= 0 && e.vr_ar_mask != 0 && vlen(pc4 - (e.absmin + e.absmax) * 0.5) < 48,
                sprintf("%s: dead (health %g), keeps its pieces %g, its cuirass on the body (%g units off its middle; it moved %g)",
                e.classname, e.health, e.vr_ar_mask, vlen(pc4 - (e.absmin + e.absmax) * 0.5), vlen(e.origin - died)));
        }
        VR_Test_Check(vr_bat_spreadd < 0.01, sprintf("the corpses' pieces keep their size through the fall and the ragdoll (strayed %g)",
            vr_bat_spreadd));
        float drawn4 = ragdollbone(world, '2 0 0', QVR_ARMOR_BEAST);
        VR_Test_Check(drawn4 > 0, sprintf("the corpses' and the living's pieces drawn: %g", drawn4));
        vr_bat_state = 9;
    }
    if(vr_bat_state == 9)
    {
        cvar_set("vr_agarmor_rule", ftos(vr_bat_rule));
        for(i = 0; i < 4; i++)
        {
            e = VR_BAT_Ent(i);
            if(e) remove(e);
        }
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        vr_bat_busy = FALSE;
        vr_bat_state = 0;
        VR_Test_Verdict();
    }
};
#endif
