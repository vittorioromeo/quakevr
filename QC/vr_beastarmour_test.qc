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
// armourhits (ROUND26.md): the parts that break on their own (the ogre's shins by side, the scrag's back half and tail):
// worn with their slots, each its own pool (its slot's share split), a shot at it meeting it, broken alone (its slot and
// the others kept), its debris its own pieces. scragtail: each of the scrag's four tail segments its own part (bits
// 11-14, one piece each), a lower one broken before the ones above, and a cut taking the tail dropping every segment
// still on.
// `vr_agarmor_test 16`: the scene (scenario armour_beasts): the four in full sets (green, yellow, red, yellow), calm (the
// player notarget), at their own health (armourhits: the test's 1000 made them take forever). `17`-`20`: one of them
// close ahead in full yellow (temp1 1-3: that tier), three quarters on (the renders).

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
float vr_bat_spawn_health; // VR_BAT_Spawn's beast's own health (before the test's 1000)

entity(float i) VR_BAT_Ent =
{
    if(i == 0) return vr_bat_e0;
    if(i == 1) return vr_bat_e1;
    if(i == 2) return vr_bat_e2;
    return vr_bat_e3;
};

void() monster_shambler;
// Kind `kind` (0 rottweiler, 1 ogre, 2 scrag, 3 fiend, 4 a shambler: no armour kind) `ahead` units before player `p`,
// `side` to its right, turned `turn` degrees from facing it.
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
    else if(kind == 4) { self.classname = "monster_shambler"; monster_shambler(); } // (no armour: the drawn-shape check's)
    else { self.classname = "monster_demon1"; monster_demon1(); }
    entity g = self;
    self = oself;
    vr_bat_spawn_health = g.health;
    g.health = 1000;
    return g;
};

// The scene's and the renders': at its own health.
entity(entity p, float kind, float side, float ahead, float turn) VR_BAT_SpawnOwn =
{
    entity g = VR_BAT_Spawn(p, kind, side, ahead, turn);
    g.health = vr_bat_spawn_health;
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

// The slots each kind's data gives (Misc/quakevr/make_beastarmour.py), with its parts breaking on their own (1024, 2048:
// the ogre's shins; the scrag's back half 1024, his tail's segments 2048-16384: "scragtail"), and its cuirass group (the
// scrag's with his back half and tail).
float(float kind) VR_BAT_Slots = { return kind == 0 ? 975 : kind == 1 ? 4095 : kind == 2 ? 32575 : 1023; };
float(float kind) VR_BAT_Group = { return kind == 0 ? 257 : 769; };
float(float kind) VR_BAT_CuirassWorn = { return kind == 2 ? 32513 : VR_BAT_Group(kind); };
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
// (B: the limbs moved, the ogre's and the scrag's arms not before their chests: their breastplates shot from the front)
float(float kind) VR_BAT_FrameB = { return kind == 0 ? 48 : kind == 1 ? 28 : kind == 2 ? 31 : 54; };

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

// armourhits: beast `e` (kind 1 the ogre, 2 the scrag) in a fresh full yellow set: each part that breaks on its own (bits
// 10, 11) its own pool (its slot's share split with it), a shot at it meeting it, a hit there breaking it alone (its slot
// and the other part kept) and dropping its own pieces as debris (the tail's four).
// A shot at part bit `b` of `e` (from outside its piece: out from the body's middle, else from either side, behind,
// below) and a hit of 400 there: the part the shot met (vr_bat_took: the health the hit took, vr_bat_debris the debris made).
float vr_bat_took, vr_bat_debris;
float(entity e, float b) VR_BAT_BreakPart =
{
    vector at = limbpiece(e, b, 10);
    vector out = normalize(at - (e.absmin + e.absmax) * 0.5);
    makevectors('0 1 0' * e.angles_y);
    float met = ragdollbone(e, at, QVR_ARMOR_SHOT, -out);
    float k;
    for(k = 0; k < 4 && met != b; k++)
    {
        out = k == 0 ? v_right : k == 1 ? -v_right : k == 2 ? -v_forward : '0 0 -1';
        met = ragdollbone(e, at, QVR_ARMOR_SHOT, -out);
    }
    float made = vr_ar_debris_made;
    VR_AgArmor_Shot(e, at, -out);
    vr_bat_took = VR_BAT_Hit(e, at, 400);
    vr_bat_debris = vr_ar_debris_made - made;
    e.health = 1000;
    return met;
};

void(entity e, float kind) VR_BAT_PartsAlone =
{
    float b, j;
    float slots = ragdollbone(e, '0 0 0', QVR_ARMOR_BEAST);
    for(b = VR_AR_PIECES; b < VR_AR_SLOTS; b++)
    {
        if(!(slots & VR_AgArmor_Bit(b)))
        {
            continue;
        }
        VR_AgArmor_Give(e, 2, 1023);
        float owner = ragdollbone(e, [4, b, 0], QVR_ARMOR_BEAST);
        float pieces = ragdollbone(e, [3, b, 0], QVR_ARMOR_BEAST);
        // (its slot's pool split among the slot and its parts: their pools summed about the slot's whole, each part's
        // rounding at most one)
        float split = VR_AgArmor_FullOf(e, owner);
        for(j = VR_AR_PIECES; j < VR_AR_SLOTS; j++)
        {
            float partOf = (slots & VR_AgArmor_Bit(j)) && (ragdollbone(e, [4, j, 0], QVR_ARMOR_BEAST) == owner);
            split = split + (partOf ? VR_AgArmor_FullOf(e, j) : 0);
        }
        float whole = rint(VR_AgArmor_Full(2, owner) * VR_AgArmor_PoolScale(e));
        VR_Test_Check(owner >= 0 && VR_AgArmor_On(e, b) && e.vr_ar_hp[b] == VR_AgArmor_FullOf(e, b) && fabs(split - whole) <= 2 && pieces == 1,
            sprintf("%s: part %g (with slot %g, %g pieces: 1) worn, its pool %g; its slot split %g of %g", e.classname, b, owner,
            pieces, e.vr_ar_hp[b], split, whole));
        float mask0 = e.vr_ar_mask;
        float met = VR_BAT_BreakPart(e, b);
        float others = mask0 - VR_AgArmor_Bit(b);
        VR_Test_Check(met == b && e.vr_ar_mask == others && vr_bat_took == 0 && vr_bat_debris == pieces,
            sprintf("%s: part %g alone broke (a shot met %g; pieces left %g, took %g, debris %g of %g)", e.classname, b, met,
            e.vr_ar_mask, vr_bat_took, vr_bat_debris, pieces));
    }
    if(kind == 2)
    {
        // scragtail: a lower segment first (tail3, bit 13: the data's lines tail1-4 are bits 11-14), the ones above it and
        // below kept; then the top one (tail1), tail2 and tail4 still on.
        VR_AgArmor_Give(e, 2, 1023);
        float tail = 2048 + 4096 + 8192 + 16384;
        float m13 = VR_BAT_BreakPart(e, 13);
        float d13 = vr_bat_debris;
        float left13 = e.vr_ar_mask & tail;
        float m11 = VR_BAT_BreakPart(e, 11);
        VR_Test_Check(m13 == 13 && d13 == 1 && left13 == tail - 8192 && m11 == 11 && vr_bat_debris == 1 &&
            (e.vr_ar_mask & tail) == 4096 + 16384 && VR_AgArmor_On(e, 8) && VR_AgArmor_On(e, 10),
            sprintf("scrag: tail3 broke first (met %g, debris %g, tail left %g), then tail1 (met %g, debris %g, tail left %g)",
            m13, d13, left13, m11, vr_bat_debris, e.vr_ar_mask & tail));
    }
    VR_AgArmor_Give(e, 2, 1023);
};

// scragtail: a cut taking the scrag's tail (the joint whose cut takes the most of its segments' bones) drops every
// segment still on as debris, one each (one broken before stays broken, no debris).
entity vr_bat_cut; // (spawned with the four, killed with the ogre and the rottweiler: cut as a corpse, its ragdoll's)
void() VR_BAT_TailCutPrepare =
{
    entity e = vr_bat_cut;
    VR_AgArmor_Give(e, 2, 1023);
    e.health = 2;
    e.think = SUB_Null;
    float type4 = e.vr_ar_type;
    e.vr_ar_type = 0; // (the killing blow past its plates)
    T_Damage(e, world, world, 5);
    e.vr_ar_type = type4;
};
void(entity p) VR_BAT_TailCut =
{
    entity e = vr_bat_cut;
    vr_bat_cut = world;
    float b, j;
    float limbs = ragdolllimbs(e, 0);
    float joint = -1, most = 0;
    for(j = 0; j < 24; j++)
    {
        if(!(limbs & VR_AgArmor_Bit(j)))
        {
            continue;
        }
        float takes = ragdolllimbs(e, 4, j);
        float n = 0;
        for(b = 11; b < 15; b++)
        {
            n = n + ((ragdollbone(e, [b, 0, 0], QVR_ARMOR_PIECE_BONES) & takes) != 0);
        }
        joint = n > most ? j : joint;
        most = max(most, n);
    }
    // (the lowest segment broken first, its pool gone: the cut drops the rest)
    VR_AgArmor_Break(e, 14);
    float on = 0, cutBits = 0;
    float takes2 = joint >= 0 ? ragdolllimbs(e, 4, joint) : 0;
    for(b = 11; b < 15; b++)
    {
        float hit = VR_AgArmor_On(e, b) && (ragdollbone(e, [b, 0, 0], QVR_ARMOR_PIECE_BONES) & takes2);
        on = on + hit;
        cutBits = cutBits + (hit ? VR_AgArmor_Bit(b) : 0);
    }
    float mask0 = e.vr_ar_mask;
    float made = vr_ar_debris_made;
    float cut = joint >= 0 && VR_Limb_Cut(e, p, normalize(e.origin - p.origin) * 200, QVR_DECAP_SWING, joint, FALSE);
    VR_Test_Check(cut && most == 4 && on == 3 && vr_ar_debris_made - made == on && e.vr_ar_mask == mask0 - cutBits,
        sprintf("scrag: the cut at joint %g (cut %g; takes %g of the 4 segments' bones) dropped %g segments (%g still on, tail4 broken before); pieces %g -> %g",
        joint, cut, most, vr_ar_debris_made - made, on, mask0, e.vr_ar_mask));
    remove(e);
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
            VR_AgArmor_Give(VR_BAT_SpawnOwn(p, 0, -84, 150, 0), 1, 1023);
            VR_AgArmor_Give(VR_BAT_SpawnOwn(p, 1, -28, 170, 0), 2, 1023);
            VR_AgArmor_Give(VR_BAT_SpawnOwn(p, 2, 28, 150, 0), 3, 1023);
            VR_AgArmor_Give(VR_BAT_SpawnOwn(p, 3, 84, 170, 0), 2, 1023);
            return;
        }
        if(vr_bat_render)
        {
            remove(vr_bat_render); // (the one before: one at a time, for a row of renders)
        }
        vr_bat_render = VR_BAT_SpawnOwn(p, req - 17, 0, req == 19 ? 100 : req == 18 ? 150 : 130, 35);
        float tier = cvar_hget(cvarh_temp1); // (temp1 1-3: that tier, else yellow)
        VR_AgArmor_Give(vr_bat_render, (tier >= 1 && tier <= 3) ? tier : 2, 1023);
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
        vr_bat_cut = VR_BAT_Spawn(p, 2, 0, 100, 0);// (scragtail's tail cut: bare until then, settled with the others)
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
            VR_Test_Check(e.vr_ar_mask == VR_BAT_CuirassWorn(i), sprintf("%s: the cuirass alone gives its group (%g)", e.classname, e.vr_ar_mask));
            VR_AgArmor_Give(e, 2, 1023);
            VR_Test_Check(e.vr_ar_mask == VR_BAT_Slots(i) && e.vr_ar_hp[0] == VR_AgArmor_FullOf(e, 0),
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
        // (and the slots' parts: the ogre's shins with his thighs' pieces, the scrag's tail and his plate 8's back half with his
        // belt's front half: ROUND26.md, "scragsplit")
        float parts = VR_AgArmor_On(vr_bat_e1, 10) + VR_AgArmor_On(vr_bat_e1, 11) + VR_AgArmor_On(vr_bat_e2, 10) + VR_AgArmor_On(vr_bat_e2, 11) +
            VR_AgArmor_On(vr_bat_e2, 12) + VR_AgArmor_On(vr_bat_e2, 13) + VR_AgArmor_On(vr_bat_e2, 14); // (scragtail: a bit each segment)
        float ents = ragdollbone(world, '2 1 0', QVR_ARMOR_BEAST);
        VR_Test_Check((ents == want + parts) && (parts == 7), sprintf("every part drawn: %g entities, %g pieces and %g parts", ents, want,
            parts));
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
                float met, turn;
                if(k == 0 && (i == 1 || i == 2))
                {
                    // (the ogre's and the scrag's breastplates wrap the chest on its middle: their box's middle is near
                    // the body's, so the shot comes from before them, the way the body faces; beastfit: the plates lie on
                    // the skin, square to the shoulders (the ogre's stance turns them ~33 degrees) with a seam down the
                    // middle, so the first of a fan of shots within 40 degrees of his facing that meets a plate)
                    met = -1;
                    for(turn = 0; turn <= 40 && met != k; turn = turn > 0 ? -turn : 10 - turn)
                    {
                        makevectors('0 1 0' * (e.angles_y + turn));
                        out = v_forward;
                        met = ragdollbone(e, at, QVR_ARMOR_SHOT, -out);
                    }
                }
                else
                {
                    met = ragdollbone(e, at, QVR_ARMOR_SHOT, -out);
                }
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
            if(i == 1 || i == 2)
            {
                VR_BAT_PartsAlone(e, i);
            }
        }
        cvar_set("vr_agarmor_rule", ftos(vr_bat_rule));
        // The skill's roll on a spawned beast: Nightmare, a chance of 1, the full set.
        float s0 = cvar_hget(cvarh_vr_agarmor_skill3), s1 = cvar_hget(cvarh_vr_agarmor_skill3_pieces), sk = cvar_hget(cvarh_skill);
        cvar_set("vr_agarmor_skill3", "1");
        cvar_set("vr_agarmor_skill3_pieces", "10");
        cvar_set("skill", "3");
        e = VR_BAT_Spawn(p, 1, 0, 300, 0);
        VR_Test_Check(e.vr_ar_type > 0 && e.vr_ar_mask == 4095, sprintf("the skill's roll dresses a spawned ogre (type %g, pieces %g)",
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
        VR_BAT_TailCutPrepare();
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
        VR_BAT_TailCut(p);
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
        if(vr_bat_cut) remove(vr_bat_cut); // (timed out before its cut)
        vr_bat_cut = world;
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        vr_bat_busy = FALSE;
        vr_bat_state = 0;
        VR_Test_Verdict();
    }
};

// `vr_agarmor_test 21` (doglerp; ROUND26.md): the drawn body moves smoothly and the armour with it, in slow motion and at
// full speed. One beast at a time, dressed in a full set, ahead of the player: a rottweiler's leap, its run, its roll over
// (painb) after a shove, a scrag flying, an ogre running, an ogre's chainsaw swing, a shambler running (bare: none worn).
// Each is watched frame by frame (vr_drawnlerp_watch, vr_hitmodel_diag.cpp): the drawn body against the hit model the
// armour is placed from: no frame drawn still while it moves, no jump, the two in step; and the armour's points against
// the shape the shader drew (the spline, the rebuild: within 0.05 units every frame; ROUND26 "armourrebuild"). First at
// a quarter speed (vr_timescale 0.25), then at 1. Ends on "doglerp test: N passed, M failed".
float vr_dlt_busy;
float vr_dlt_case;   // 0 leap, 1 run, 2 roll, 3 scrag, 4 ogre, 5 ogre's chainsaw, 6 shambler; + 7 at full speed
float vr_dlt_state;  // 1 spawned, 2 moving (watched), 3 the watch ended: its result next frame
float vr_dlt_wait;   // the state's next step at this time (game time)
float vr_dlt_until;  // give up on the case
float vr_dlt_flew;   // the leap and the roll: off the ground once
float vr_dlt_ticks;  // frames since the watch ended (its result: the engine's, a frame later)
float vr_dlt_scale0, vr_dlt_ramp0, vr_dlt_kd0;
entity vr_dlt_e;
float VR_DLT_CASES = 7;
void() dog_leap1;
void() dog_run1;
void() dog_painb1;
void() wiz_run1;
void() ogre_run1;
void() ogre_swing1;
void() sham_run1;

string(float c) VR_DLT_Name =
{
    c = c - floor(c / VR_DLT_CASES) * VR_DLT_CASES;
    return c == 0 ? "rottweiler leap" : c == 1 ? "rottweiler run" : c == 2 ? "rottweiler roll (shoved)" : c == 3 ? "scrag flying" : c == 4 ? "ogre running" : c == 5 ? "ogre chainsaw" : "shambler running";
};

// Case `c`'s beast spawned (its armour on), left to settle; the next state starts its move.
void(entity p, float c) VR_DLT_Spawn =
{
    float k = c - floor(c / VR_DLT_CASES) * VR_DLT_CASES;
    float kind = k <= 2 ? 0 : k == 3 ? 2 : k == 6 ? 4 : 1;
    vr_dlt_e = VR_BAT_Spawn(p, kind, 0, k == 1 ? 420 : k >= 4 ? 360 : 220, 0);
    if(kind != 4)
    {
        VR_AgArmor_Give(vr_dlt_e, 2, 1023);
    }
    cvar_set("vr_timescale", c < VR_DLT_CASES ? "0.25" : "1");
    vr_dlt_flew = FALSE;
    vr_dlt_state = 1;
    vr_dlt_wait = time + 0.8; // (its start_go: up to half a second's spread)
    vr_dlt_until = time + 6;
};

// Its move begun (self the beast) and watched.
void(entity p, float c) VR_DLT_Move =
{
    float k = c - floor(c / VR_DLT_CASES) * VR_DLT_CASES;
    entity oself = self;
    self = vr_dlt_e;
    self.enemy = self.goalentity = p;
    self.attack_finished = time + 100; // (no attacks: a muzzle flash's frames are drawn unlerped on purpose, cl_main.c)
    if(k == 0)
    {
        dog_leap1();
    }
    else if(k == 1)
    {
        dog_run1();
    }
    else if(k == 2)
    {
        makevectors('0 1 0' * self.angles_y);
        VR_Push(self, -v_forward, 300, 120);
        dog_painb1();
    }
    else if(k == 3)
    {
        wiz_run1();
    }
    else if(k == 4)
    {
        ogre_run1();
    }
    else if(k == 5)
    {
        ogre_swing1(); // (its chainsaw's frames: the player out of its reach)
    }
    else
    {
        sham_run1();
    }
    self = oself;
    localcmd(sprintf("vr_drawnlerp_watch %s\n", vr_dlt_e.classname));
    vr_dlt_state = 2;
    vr_dlt_wait = time + (k == 2 ? 1.6 : k == 5 ? 1.4 : 0.8); // (the run, the flight: 8 frames; the roll: its 16; the swing its 14)
};

void(float req) VR_DLT_Frame =
{
    entity p = VR_Player();
    if(req == 21)
    {
        VR_Test_BeginFlags("doglerp test", VR_TEST_LOUD);
        vr_dlt_busy = TRUE;
        vr_dlt_scale0 = cvar_hget(cvarh_vr_timescale);
        vr_dlt_ramp0 = cvar_hget(cvarh_vr_timescale_ramp);
        vr_dlt_kd0 = cvar_hget(cvarh_vr_knockdown_chance);
        cvar_set("vr_timescale_ramp", "0");
        cvar_set("vr_knockdown_chance", "0"); // (the roll is its pain frames, not a knockdown's ragdoll)
        p.flags = p.flags | FL_NOTARGET | FL_GODMODE;
        vr_dlt_case = 0;
        VR_DLT_Spawn(p, 0);
        return;
    }
    if(time > vr_dlt_until && vr_dlt_state < 3)
    {
        VR_Test_Check(FALSE, sprintf("%s: timed out in state %g", VR_DLT_Name(vr_dlt_case), vr_dlt_state));
        localcmd("vr_drawnlerp_watch 0 timedout\n");
        vr_dlt_state = 3;
        vr_dlt_ticks = -1000; // (no result read)
    }
    if(vr_dlt_state == 1)
    {
        if(time >= vr_dlt_wait)
        {
            VR_DLT_Move(p, vr_dlt_case);
        }
        return;
    }
    if(vr_dlt_state == 2)
    {
        float k = vr_dlt_case - floor(vr_dlt_case / VR_DLT_CASES) * VR_DLT_CASES;
        if(!(vr_dlt_e.flags & FL_ONGROUND))
        {
            vr_dlt_flew = TRUE;
        }
        // The leap: watched until it is down again; the roll: through its frames; the rest a while.
        float done = k == 0 ? (vr_dlt_flew && (vr_dlt_e.flags & FL_ONGROUND) && time >= vr_dlt_wait - 0.6) : time >= vr_dlt_wait;
        if(!done)
        {
            return;
        }
        if(k == 0 || k == 2)
        {
            VR_Test_Check(vr_dlt_flew, sprintf("%s: off the ground in it", VR_DLT_Name(vr_dlt_case)));
        }
        localcmd(sprintf("vr_drawnlerp_watch 0 \"%s at %s\"\n", VR_DLT_Name(vr_dlt_case), vr_dlt_case < VR_DLT_CASES ? "0.25x" : "1x"));
        vr_dlt_state = 3;
        vr_dlt_ticks = 0;
        return;
    }
    if(vr_dlt_state == 3)
    {
        vr_dlt_ticks = vr_dlt_ticks + 1;
        if(vr_dlt_ticks >= 0 && vr_dlt_ticks < 3)
        {
            return; // (the watch's end runs at the next frame's commands)
        }
        if(vr_dlt_ticks >= 0)
        {
            float r = cvar_hget(cvarh_vr_drawnlerp_result);
            VR_Test_Check(r == 1, sprintf("%s at %s: drawn smoothly, the armour in step (%s)", VR_DLT_Name(vr_dlt_case),
                vr_dlt_case < VR_DLT_CASES ? "0.25x" : "1x", r == 0 ? "not smooth" : r < 0 ? "too few frames" : "smooth"));
        }
        if(vr_dlt_e) remove(vr_dlt_e);
        vr_dlt_e = world;
        vr_dlt_case = vr_dlt_case + 1;
        if(vr_dlt_case < 2 * VR_DLT_CASES)
        {
            VR_DLT_Spawn(p, vr_dlt_case);
            return;
        }
        cvar_set("vr_timescale", ftos(vr_dlt_scale0));
        cvar_set("vr_timescale_ramp", ftos(vr_dlt_ramp0));
        cvar_set("vr_knockdown_chance", ftos(vr_dlt_kd0));
        p.flags = p.flags - (p.flags & (FL_NOTARGET | FL_GODMODE));
        vr_dlt_busy = FALSE;
        vr_dlt_state = 0;
        VR_Test_Verdict();
    }
};
// `vr_agarmor_test 23` (armourrebuild; ROUND26.md): the cost of the beasts' armour on the drawn body. Ten ogres in full
// armour ahead, running at the player (god mode, their attacks put off), settled 72 frames, then measured 288:
// vr_anim_smooth_info at both ends (the first resets its counts) prints the drawn points' calls and time ("animsmooth:
// drawn points ..."). Ends on "armourcost: N passed, M failed" (all ten alive through it).
float vr_arc_busy;
float vr_arc_state;  // 1 settling, 2 measured, 3 the info asked for: the verdict a few frames later
float vr_arc_frames; // frames counted in the state
float VR_ARC_SETTLE = 72;
float VR_ARC_FRAMES = 288;

float() VR_ARC_Alive =
{
    float n = 0;
    entity e = find(world, classname, "monster_ogre");
    while(e)
    {
        n = n + (e.health > 0 ? 1 : 0);
        e = find(e, classname, "monster_ogre");
    }
    return n;
};

void(float req) VR_ARC_Frame =
{
    entity p = VR_Player();
    if(req == 23)
    {
        VR_Test_BeginFlags("armourcost", VR_TEST_LOUD);
        vr_arc_busy = TRUE;
        p.flags = p.flags | FL_NOTARGET | FL_GODMODE;
        float i = 0;
        while(i < 10)
        {
            float row = floor(i / 5);
            entity g = VR_BAT_Spawn(p, 1, (i - row * 5 - 2) * 72, 320 + row * 110, 0);
            VR_AgArmor_Give(g, 2, VR_BAT_Slots(1));
            entity oself = self;
            self = g;
            self.enemy = self.goalentity = p;
            self.attack_finished = time + 100;
            ogre_run1();
            self = oself;
            i = i + 1;
        }
        vr_arc_state = 1;
        vr_arc_frames = 0;
        return;
    }
    vr_arc_frames = vr_arc_frames + 1;
    if(vr_arc_state == 1 && vr_arc_frames >= VR_ARC_SETTLE)
    {
        localcmd("vr_anim_smooth_info\n"); // (its counts reset)
        vr_arc_state = 2;
        vr_arc_frames = 0;
        return;
    }
    if(vr_arc_state == 2 && vr_arc_frames >= VR_ARC_FRAMES)
    {
        localcmd("vr_anim_smooth_info\n");
        VR_Test_Check(VR_ARC_Alive() == 10, sprintf("ten armoured ogres alive through %g frames (%g)", VR_ARC_FRAMES, VR_ARC_Alive()));
        vr_arc_state = 3;
        vr_arc_frames = 0;
        return;
    }
    if(vr_arc_state == 3 && vr_arc_frames >= 3) // (the info runs at the next frame's commands)
    {
        entity e = find(world, classname, "monster_ogre");
        while(e)
        {
            entity nx = find(e, classname, "monster_ogre");
            remove(e);
            e = nx;
        }
        p.flags = p.flags - (p.flags & (FL_NOTARGET | FL_GODMODE));
        vr_arc_busy = FALSE;
        vr_arc_state = 0;
        VR_Test_Verdict();
    }
};
// `vr_agarmor_test 22` (armourhits; ROUND26.md): time to kill. Each of a rottweiler, an ogre, a scrag, a fiend, an armed
// grunt and an armed knight stands still ahead (its own health), bare, then in each skill's armour (Normal, Hard,
// Nightmare: the skill's best type and its count of pieces, the cuirass, the helmet, then the limb plates in order), and
// is shot from 128 units until it has taken its own health (one body a target, kept alive past it): the shotgun (6
// pellets, a shot each 0.5 s), the super shotgun (14, 0.7 s), the nailgun (a nail each 0.1 s), aimed over the middle of
// its body (the same points each run), Plate Blocks All as set (the default).
// Each run twice; the shots and seconds printed, and each beast's armoured / bare ratio checked against the humanoids'
// (the mean of the grunt's and the knight's) for the same weapon and skill: within VR_TTK_LO..VR_TTK_HI of it, the three
// weapons together. Ends on "armour ttk test: N passed, M failed".
float vr_ttk_busy;
float vr_ttk_run;     // target x 12 + state x 3 + weapon
float vr_ttk_trial;
float vr_ttk_step;    // 1 spawned (its start awaited), 2 shooting
float vr_ttk_shots;
float vr_ttk_aim;     // the aim's sequence (each run from 0)
float vr_ttk_until;   // give up on the run
float vr_ttk_nail_until;
float vr_ttk_frame0;
float vr_ttk_own;     // its own health
float vr_ttk_god;
vector vr_ttk_org, vr_ttk_ang;
entity vr_ttk_e, vr_ttk_nail;
float vr_ttk_res[72];  // shots to kill, summed over the trials
float VR_TTK_TARGETS = 6;
float VR_TTK_TRIALS = 2;
float VR_TTK_MAXSHOTS = 400;
float VR_TTK_LO = 0.15; // a beast's armoured / bare ratio over the humanoids', at least (its armour does something) ...
// ... and at most (never much tougher than they are; the three weapons' summed shots). 1.5, not 1.1: with two trials and
// the random aim and pellet spread the ratio is noise-bound: the scrag measured 0.91-1.16 of the humanoids' over two runs
// of the same build (scragtail), 1.17-1.43 in ogreplates' runs with his armour unchanged; the humanoids' mean itself varies
// 2.05-2.59 (ROUND26.md, "scragtail"). A doubled pool still fails it.
float VR_TTK_HI = 1.5;

string(float t) VR_TTK_Name =
{
    return t == 0 ? "rottweiler" : t == 1 ? "ogre" : t == 2 ? "scrag" : t == 3 ? "fiend" : t == 4 ? "grunt" : "knight";
};
string(float w) VR_TTK_Weapon = { return w == 0 ? "shotgun" : w == 1 ? "super shotgun" : "nailgun"; };
float(float w) VR_TTK_Refire = { return w == 0 ? 0.5 : w == 1 ? 0.7 : 0.1; };
string(float s) VR_TTK_State = { return s == 0 ? "bare" : s == 1 ? "Normal" : s == 2 ? "Hard" : "Nightmare"; };

// Skill state `s`'s armour: its best type, its pieces (the cuirass, the helmet, then the limb plates 2.. in order).
float(float s) VR_TTK_Type = { return s <= 0 ? 0 : max(1, min(3, floor(VR_AgArmor_SkillValue(s, 1)))); };
float(float s) VR_TTK_Mask =
{
    float n = max(1, min(VR_AR_PIECES, floor(VR_AgArmor_SkillValue(s, 2))));
    return VR_AgArmor_Bit(n) - 1;
};

entity(entity p, float t) VR_TTK_Spawn =
{
    entity g;
    if(t < 4)
    {
        g = VR_BAT_SpawnOwn(p, t, 0, 128, 0);
    }
    else
    {
        makevectors('0 1 0' * p.angles_y);
        entity oself = self;
        self = spawn();
        setorigin(self, p.origin + v_forward * 128);
        self.angles_y = vectoyaw(p.origin - self.origin);
        self.weapon = -1; // (its fists)
        VR_AM_Spawn(t == 4 ? 0 : 2);
        g = self;
        self = oself;
    }
    VR_AgArmor_Give(g, 0, 0); // (its own roll's off)
    return g;
};

// Still where it stood (its pain and its hunt undone each frame: the same body every shot).
void(entity e) VR_TTK_Hold =
{
    e.think = SUB_Null;
    e.nextthink = time + 60;
    e.enemy = world;
    e.velocity = '0 0 0';
    e.avelocity = '0 0 0';
    e.angles = vr_ttk_ang;
    e.frame = vr_ttk_frame0;
    setorigin(e, vr_ttk_org);
};

void(entity p) VR_TTK_Fire =
{
    entity e = vr_ttk_e;
    float w = vr_ttk_run - floor(vr_ttk_run / 3) * 3;
    vector mid = (e.absmin + e.absmax) * 0.5;
    vector to = p.origin - mid;
    to_z = 0;
    to = normalize(to);
    vector src = mid + to * 128;
    float a1 = 0.5 + vr_ttk_aim * 0.7548776662;
    float a2 = 0.5 + vr_ttk_aim * 0.5698402910;
    a1 = a1 - floor(a1);
    a2 = a2 - floor(a2);
    vr_ttk_aim = vr_ttk_aim + 1;
    makevectors(vectoangles(-to));
    vector aim = mid + v_right * (a1 - 0.5) * 0.6 * e.size_x + '0 0 1' * (a2 - 0.5) * 0.6 * e.size_z;
    vector dir = normalize(aim - src);
    entity oself = self;
    self = p;
    if(w == 2)
    {
        vr_ttk_nail = launch_spike(aim - dir * 48, dir); // (on the same line, near: a frame or two in flight)
        vr_ttk_nail_until = time + 0.5;
    }
    else
    {
        VR_Hit_Begin(QVR_HITKIND_SHOT, 0, '0 0 0', dir);
        vr_pellet_damage = cvar_hget(w == 0 ? cvarh_vr_dmg_shotgun : cvarh_vr_dmg_super_shotgun);
        FireBulletsImpl(w == 0 ? 6 : 14, dir, w == 0 ? '0.01 0.01 0' : '0.035 0.025 0', src, 2048, FALSE);
        vr_pellet_damage = 0;
        VR_Hit_End();
    }
    self = oself;
    vr_ttk_shots = vr_ttk_shots + 1;
};

// The table and the checks.
void() VR_TTK_Report =
{
    float t, s, w;
    for(t = 0; t < VR_TTK_TARGETS; t++)
    {
        for(w = 0; w < 3; w++)
        {
            float b = vr_ttk_res[t * 12 + w] / VR_TTK_TRIALS;
            string line = sprintf("ttk: %s, %s: bare %g shots (%g s)", VR_TTK_Name(t), VR_TTK_Weapon(w), b, b * VR_TTK_Refire(w));
            for(s = 1; s < 4; s++)
            {
                float a = vr_ttk_res[t * 12 + s * 3 + w] / VR_TTK_TRIALS;
                line = strcat(line, sprintf("; %s %g (%g s, %gx)", VR_TTK_State(s), a, a * VR_TTK_Refire(w), b > 0 ? rint(a / b * 100) / 100 : 0));
            }
            VR_Test_Print(strcat(line, "\n"));
        }
    }
    // Each beast and skill: the three weapons' shots armoured over bare, against the humanoids' mean.
    for(s = 1; s < 4; s++)
    {
        float hr = 0;
        for(t = 4; t < 6; t++)
        {
            float hb = 0, ha = 0;
            for(w = 0; w < 3; w++)
            {
                hb = hb + vr_ttk_res[t * 12 + w];
                ha = ha + vr_ttk_res[t * 12 + s * 3 + w];
            }
            hr = hr + ha / max(1, hb) / 2;
        }
        for(t = 0; t < 4; t++)
        {
            float bb = 0, ba = 0;
            for(w = 0; w < 3; w++)
            {
                bb = bb + vr_ttk_res[t * 12 + w];
                ba = ba + vr_ttk_res[t * 12 + s * 3 + w];
            }
            float br = ba / max(1, bb);
            VR_Test_Check(br >= hr * VR_TTK_LO && br <= hr * VR_TTK_HI, sprintf("%s on %s: armoured / bare %g, the humanoids' %g (%g of it)",
                VR_TTK_Name(t), VR_TTK_State(s), rint(br * 100) / 100, rint(hr * 100) / 100, rint(br / max(0.01, hr) * 100) / 100));
        }
    }
};

void(float req) VR_TTK_Frame =
{
    entity p = VR_Player();
    if(req == 22)
    {
        VR_Test_BeginFlags("armour ttk test", VR_TEST_LOUD);
        vr_ttk_busy = TRUE;
        vr_ttk_god = p.flags & (FL_GODMODE | FL_NOTARGET);
        p.flags = p.flags | FL_GODMODE | FL_NOTARGET;
        float i;
        for(i = 0; i < 72; i++)
        {
            vr_ttk_res[i] = 0;
        }
        vr_ttk_run = 0;
        vr_ttk_trial = 0;
        vr_ttk_step = 0;
        vr_ttk_until = time + 1; // (the level settled: the player standing where he spawned)
        return;
    }
    if(vr_ttk_step == 0)
    {
        if(time < vr_ttk_until)
        {
            return;
        }
        vr_ttk_e = VR_TTK_Spawn(p, floor(vr_ttk_run / 12));
        vr_ttk_step = 1;
        vr_ttk_nail_until = time + 0.6; // (its start_go: up to half a second's spread)
        vr_ttk_until = time + 2;
        return;
    }
    if(vr_ttk_step == 1)
    {
        if((vr_ttk_e.takedamage == DAMAGE_NO || time < vr_ttk_nail_until) && time < vr_ttk_until)
        {
            return; // (walkmonster_start_go not run yet)
        }
        vr_ttk_own = vr_ttk_e.health;
        vr_ttk_org = vr_ttk_e.origin;
        vr_ttk_ang = vr_ttk_e.angles;
        vr_ttk_frame0 = floor(vr_ttk_run / 12) == 0 ? 69 : 0; // (VR_BAT_FrameA: standing)
        vr_ttk_step = 3;
    }
    entity e = vr_ttk_e;
    if(vr_ttk_step == 3)
    {
        // A run on the same body: dressed for its state, a health far past its own (it never dies: no corpse, no new
        // spawn), its own health's worth of damage taken counted as the kill.
        float st = floor(vr_ttk_run / 3) - floor(vr_ttk_run / 12) * 4;
        VR_AgArmor_Give(e, VR_TTK_Type(st), st > 0 ? VR_TTK_Mask(st) : 0);
        e.health = vr_ttk_own + 100000;
        VR_TTK_Hold(e);
        vr_ttk_shots = 0;
        vr_ttk_aim = 0;
        vr_ttk_nail = world;
        vr_ttk_step = 2;
        vr_ttk_until = time + 120;
        return;
    }
    // vr_ttk_step 2: a shot a frame (a nail: once it is gone), until its own health's worth is taken.
    VR_TTK_Hold(e); // (every frame: a hit's flinch never carries it out of the next nail's way)
    if(vr_ttk_nail != world && vr_ttk_nail.modelindex != 0 && vr_ttk_nail.classname == "spike" && time < vr_ttk_nail_until)
    {
        return;
    }
    vr_ttk_nail = world;
    float dead = vr_ttk_own + 100000 - e.health >= vr_ttk_own || e.modelindex == 0;
    if(!dead && vr_ttk_shots < VR_TTK_MAXSHOTS && time < vr_ttk_until)
    {
        VR_TTK_Fire(p);
        return;
    }
    if(!dead)
    {
        VR_Test_Check(FALSE, sprintf("%s, %s, %s: not killed after %g shots (took %g of %g; timed out)", VR_TTK_Name(floor(vr_ttk_run / 12)),
            VR_TTK_State(floor(vr_ttk_run / 3) - floor(vr_ttk_run / 12) * 4), VR_TTK_Weapon(vr_ttk_run - floor(vr_ttk_run / 3) * 3),
            vr_ttk_shots, vr_ttk_own + 100000 - e.health, vr_ttk_own));
    }
    vr_ttk_res[vr_ttk_run] = vr_ttk_res[vr_ttk_run] + vr_ttk_shots;
    vr_ttk_run = vr_ttk_run + 1;
    vr_ttk_step = 3;
    if(vr_ttk_run - floor(vr_ttk_run / 12) * 12 != 0)
    {
        return; // (this body's next run)
    }
    vr_ttk_trial = vr_ttk_trial + 1;
    if(vr_ttk_trial < VR_TTK_TRIALS)
    {
        vr_ttk_run = vr_ttk_run - 12; // (its runs again)
        return;
    }
    vr_ttk_trial = 0;
    if(e.modelindex != 0)
    {
        remove(e);
    }
    vr_ttk_e = world;
    vr_ttk_step = 0;
    if(vr_ttk_run < VR_TTK_TARGETS * 12)
    {
        return;
    }
    VR_TTK_Report();
    p.flags = p.flags - (p.flags & (FL_GODMODE | FL_NOTARGET)) + vr_ttk_god;
    vr_ttk_busy = FALSE;
    VR_Test_Verdict();
};
#endif
