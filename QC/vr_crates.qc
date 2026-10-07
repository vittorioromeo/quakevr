// vr_crates.qc -- wooden crates (docs/vr-port/ROUND21.md, "Wooden crates"). The engine (vr_crates.cpp) finds their
// places at map load, by walls and in corners; here they are spawned, at the first server frame of a new map (after the
// map's own entities, so theirs keep their numbers; before the rocks and bricks, which then keep clear of them; before
// the baselines), and made physics props as the explosive boxes are: solid Box3D bodies (.vr_rigid 2), pushed, tipped,
// stacked, stood on, carried by hand (heavy: Held Object Offsets, 25 kg and 40), never force grabbed.
//
// A crate is broken by shots, blows, explosions, things thrown at it and hard hits (VR_Crate_Impact): health by its size
// (vr_crate_health). It breaks with a crack of wood and a cloud of dust into light pieces (boards, a splinter, a batten:
// make_crates.py), rigid bodies that don't block you (SOLID_NOT_BUT_TOUCHABLE: pushed aside by your body, carried,
// thrown, force grabbed), and each piece bursts into wood dust when hit (VR_CratePiece_Burst). At most vr_crate_piece_max
// lie about (the oldest go to dust first); each fades away after vr_crate_piece_time. It may hold a small box of ammo for
// a weapon you have (vr_crate_ammo) or a small health box (vr_crate_health_box). Burning (vr_burning.qc): a crate burnt
// through breaks into charred pieces (skin VR_PIECE_SKIN_CHARRED) that don't burn; a smashed crate's pieces burn as
// crates do, then burn away (VR_CratePiece_BurnAway).
//
// A crowbar may lie on a crate's top (vr_crate_crowbar, at most vr_crate_crowbar_max a map; the top of a stack): the
// engine rolls which and where with the layout (deterministic), VR_Crowbar_OnCrate (vr_crowbar.qc) lays it there.
//
// Sight (vr_crate_sight): a monster doesn't see you behind a crate or an explosive box lying about (visible(), ai.qc:
// VR_Crate_HidesFrom); carrying one, you are seen through it.

float vr_crates_done;          // placed in this map (a saved game's are in it already)
float vr_crate_piecesoundtime; // a piece's dust sounds once a frame (an explosion bursts many)
float VR_THROWN_BOX_TIME = 2;  // s: how long a thrown solid prop (a crate, an explosive box) may hurt what it meets

void(entity xCrate, float i) VR_Crowbar_OnCrate; // (vr_crowbar.qc)

// Burning (vr_burning.qc): a crate burnt through breaks into charred pieces, one broken while burning into burning ones;
// a piece set on fire by a hit isn't burst by it; a lit torch's swing sets a piece on fire instead of bursting it.
.float vr_charred;       // a piece of a crate burnt through (or half burnt itself): its charred skin; it burns no more
.float vr_burnt_through; // a crate breaking because it has burnt its time (its pieces charred)
float VR_PIECE_SKIN_CHARRED = 3; // the pieces' charred skin (make_crates.py)
void(entity xCrate, entity xPiece) VR_Burn_NewPiece;
float(entity xPiece) VR_Burn_JustLit;
float(entity xPlayer, float xHand, entity xPiece) VR_Burn_PieceStruck;

// The models (make_crates.py) and sounds (make_crate_sounds.py; the physics sounds' wood knocks); always precached (a
// saved game may hold crates whatever the settings now), and what a crate may hold (the small ammo and health boxes).
void() VR_Crates_Precache =
{
    vr_crates_done = FALSE;
    precache_model("progs/vr_crate1.mdl");
    precache_model("progs/vr_crate2.mdl");
    precache_model("progs/vr_barrel.mdl");
    precache_model("progs/vr_plank1.mdl");
    precache_model("progs/vr_plank2.mdl");
    precache_model("progs/vr_plank3.mdl");
    precache_model("progs/vr_plank4.mdl");
    precache_sound("vr/crate_break1.wav");
    precache_sound("vr/crate_break2.wav");
    precache_sound("vr/crate_break3.wav");
    precache_sound("vr/crate_dust1.wav");
    precache_sound("vr/crate_dust2.wav");
    precache_sound("vr/phys/wood_m1.wav");
    precache_sound("vr/phys/wood_m2.wav");
    precache_sound("vr/phys/wood_m3.wav");
    precache_sound("vr/phys/wood_m4.wav");
    precache_sound("vr/phys/metal_h1.wav"); // (an explosive box struck: VR_Metal_Sound)
    precache_sound("vr/phys/metal_h2.wav");
    precache_sound("vr/phys/metal_h3.wav");
    precache_sound("vr/phys/metal_h4.wav");
    precache_model("maps/b_shell0.bsp");
    precache_model("maps/b_nail0.bsp");
    precache_model("maps/b_rock0.bsp");
    precache_model("maps/b_batt0.bsp");
    precache_model("maps/b_bh10.bsp");
    precache_sound("weapons/lock4.wav");
    precache_sound("items/r_item1.wav");
};

// A crate (1 small, 2 large), not one of its pieces.
float(entity e) VR_Crate_Is =
{
    return e.vr_cratekind == 1 || e.vr_cratekind == 2;
};

// A piece of a crate (not one gone to dust).
float(entity e) VR_CratePiece_Is =
{
    return e.vr_cratekind == 3;
};

void() barrel_explode; // (misc.qc)

// Metal: an explosive box (misc_explobox, misc_explobox2; blowing up: explo_box). Shot or struck, it throws sparks, not
// splinters or blood, and rings (VR_Metal_Hit); its knocks and scrapes are metal too (the engine's vr_physsound.cpp).
float(entity e) VR_Prop_Metal =
{
    return e.th_die == barrel_explode || e.classname == "explo_box" || e.classname == "misc_explobox" ||
           e.classname == "misc_explobox2";
};

// Wood: a crate, a piece of one (not an explosive box: metal, VR_Prop_Metal). Shot or struck, it throws splinters, not
// blood. Also one the blow has just broken (VR_Crate_Break, VR_CratePiece_Burst clear vr_cratekind): blood often comes
// after the damage.
float(entity e) VR_Crate_Wooden =
{
    if(VR_Prop_Metal(e))
    {
        return FALSE;
    }
    return e.vr_cratekind > 0 || e.vr_blocksight > 0 || e.classname == "vr_crate_broken" || e.classname == "vr_crate_piece" ||
           e.classname == "vr_crate_piece_gone";
};

// Hit material, separate from prop classification: secret doors rename themselves to door. Shootable triggers
// clear their model in InitTrigger, so recognize their class even when activation disables damage.
// Keep crates' existing material ahead of the brush fallback.
float(entity e) VR_Hit_Metal =
{
    if(e == world) { return FALSE; }
    if(VR_Prop_Metal(e)) { return TRUE; }
    if(VR_Crate_Wooden(e)) { return FALSE; }
    return e.solid == SOLID_BSP || substring(e.model, 0, 1) == "*" ||
           e.classname == "trigger_multiple" || e.classname == "trigger_once" || e.classname == "trigger_secret";
};

// What a hit on metal sounds like (VR_Metal_Hit, VR_Melee_HitSoundOn): a shot's tink or ricochet, a blade's clang (the
// axe's on a wall), a blunt blow's or a thrown thing's heavy metal knock (the physics sounds' metal_h, a 40 kg box's).
float VR_METAL_SHOT = 0;
float VR_METAL_BLADE = 1;
float VR_METAL_BLUNT = 2;
float vr_metal_soundtime; // a shot's ring once a frame (a shotgun's pellets, a nailgun's stream)
string(float xHow) VR_Metal_Sound =
{
    float r = random();
    if(xHow == VR_METAL_SHOT)
    {
        return r < 0.7 ? "weapons/tink1.wav" : (r < 0.8 ? "weapons/ric1.wav" : (r < 0.9 ? "weapons/ric2.wav" : "weapons/ric3.wav"));
    }
    if(xHow == VR_METAL_BLADE)
    {
        return "player/axhit2.wav";
    }
    return r < 0.25 ? "vr/phys/metal_h1.wav" : (r < 0.5 ? "vr/phys/metal_h2.wav" : (r < 0.75 ? "vr/phys/metal_h3.wav" : "vr/phys/metal_h4.wav"));
};

// Sparks where a shot or a blow hits metal (`damage`: how much; `dir` their way), and, a shot's (VR_METAL_SHOT), its
// ring at what it hit (`targ`); a blow's sound is the blow's own (VR_Melee_HitSoundOn). `developer` logs both.
void(entity targ, vector org, vector dir, float damage, float xHow) VR_Metal_Hit =
{
    particle2(org, dir, QVR_PARTICLE_PRESET_SPARKS, max(3, min(10, damage * 0.5)));
    string s = "";
    if(xHow == VR_METAL_SHOT && time > vr_metal_soundtime)
    {
        vr_metal_soundtime = time;
        s = VR_Metal_Sound(xHow);
        sound(targ, CHAN_AUTO, s, 0.8, ATTN_NORM);
    }
    dprint(sprintf("hit: %s is metal: sparks at %v%s%s\n", targ.classname, org, s != "" ? ", sound " : "", s));
};

// VR_Melee_HitSound (vr_melee.qc) for a blow on `xTarg`: on metal (an explosive box), a blade's (`xBlade`) clang or a
// blunt blow's (a pommel, a gun's butt, a fist, a club) metal knock; else the blow's own `xSound`.
string(entity xTarg, string xSound, float xBlade) VR_Melee_HitSoundOn =
{
    if(VR_Hit_Metal(xTarg))
    {
        float blade = xBlade && !vr_melee_blunt;
        string s = VR_Metal_Sound(blade ? VR_METAL_BLADE : VR_METAL_BLUNT);
        dprint(sprintf("melee sound: %s on metal (%s)\n", s, xTarg.classname));
        return s;
    }
    return VR_Melee_HitSound(xSound);
};

// One of four recordings, at random (the physics sounds' sets: vr/phys/<material>_<weight>1..4).
string(string a, string b, string c, string d) VR_Pick4 =
{
    float r = random();
    return r < 0.25 ? a : (r < 0.5 ? b : (r < 0.75 ? c : d));
};

// Armour under a blow (a knight's mail, a death knight's plate, an enforcer's suit; their corpses keep it).
float(entity e) VR_Hit_Armoured =
{
    string c = e.classname;
    return c == "monster_knight" || c == "monster_hell_knight" || c == "monster_enforcer" || c == "monster_ranged_knight";
};

// The second layer of a blunt blow (a fist, a gun's butt, a headbutt, a gib, a prop or a weapon struck or thrown into
// something; the thud itself is fisthit.wav or a pommel's knock): of what the blow landed on, so that the thud sounds
// of it and is never twice the same, played from `xFrom` on `xChan` (a hand's own channel: at the hand): a body (a
// monster, a player, a corpse, a gib) a slap of flesh (vr/phys/flesh_m1..4), an armoured one (VR_Hit_Armoured) a light
// metal knock (metal_l1..4) over a duller slap, wood (a crate, its pieces) a wooden knock (wood_m1..4), a wall or the
// floor the hand's own knock (soft_m1..4). Metal (an explosive box, a door, a lift) has its own knock instead of the
// thud already (VR_Melee_HitSoundOn): none. Each a little higher or lower (VR_SoundVaried); vr_snd_hit_layer loud (0
// none). `developer` prints which.
void(entity xFrom, float xChan, entity xTarg) VR_Blunt_HitLayer =
{
    float vol = min(1, cvar_hget(cvarh_vr_snd_hit_layer));
    if(vol <= 0 || (xTarg != world && VR_Hit_Metal(xTarg)))
    {
        return;
    }
    string s;
    string s2 = "";
    if(xTarg == world || (!xTarg.takedamage && !xTarg.vr_gib))
    {
        s = VR_Pick4("vr/phys/soft_m1.wav", "vr/phys/soft_m2.wav", "vr/phys/soft_m3.wav", "vr/phys/soft_m4.wav");
        vol = vol * 0.6;
    }
    else if(VR_Crate_Wooden(xTarg))
    {
        s = VR_Pick4("vr/phys/wood_m1.wav", "vr/phys/wood_m2.wav", "vr/phys/wood_m3.wav", "vr/phys/wood_m4.wav");
        vol = vol * 0.8;
    }
    else
    {
        s = VR_Pick4("vr/phys/flesh_m1.wav", "vr/phys/flesh_m2.wav", "vr/phys/flesh_m3.wav", "vr/phys/flesh_m4.wav");
        vol = vol * 0.8;
        if(VR_Hit_Armoured(xTarg))
        {
            s2 = VR_Pick4("vr/phys/metal_l1.wav", "vr/phys/metal_l2.wav", "vr/phys/metal_l3.wav", "vr/phys/metal_l4.wav");
        }
    }
    if(s2 != "")
    {
        VR_SoundVaried(xFrom, xChan, s2, vol, ATTN_NORM);
        vol = vol * 0.5;
    }
    VR_SoundVaried(xFrom, xChan, s, vol, ATTN_NORM);
    dprint(sprintf("blunt hit layer: %s%s%s on %s\n", s, s2 != "" ? " + " : "", s2, xTarg.classname));
};

// Splinters and a puff of dust where a shot or a blow hits wood (`damage`: how much).
void(vector org, vector dir, float damage) VR_Crate_Splinters =
{
    particle2(org, dir, QVR_PARTICLE_PRESET_WOODDUST, max(2, min(8, damage * 0.5)));
};

void(vector org, vector vel, float damage) SpawnBlood;
void(vector org, float damage) SpawnBloodSplash;

// Where a blow or a shot hits `targ`: blood (SpawnBlood, `vel` its spray), or splinters if it is wood. Every melee path
// goes through these (fists, held weapons and things, the crowbar, a torch, a bash, a headbutt, the chainsaw, the
// grapple, the lightning gun), so a crate never bleeds (NOTES.md e1m1_2026-10-01_12-10-47). It may come after T_Damage:
// a crate the blow broke is still wood (VR_Crate_Wooden).
// A grenade shots set off (vr_grenade.qc): sparks, not blood.
float(entity g) VR_GrenShot_Is;
void(vector xAt) VR_GrenShot_Sparks;

void(entity targ, vector org, vector vel, float damage) VR_HitBlood =
{
    if(VR_WallTorch_IsTorch(targ)) { return; }
    if(targ.vr_shootnade)
    {
        VR_GrenShot_Sparks(org);
        return;
    }
    if(VR_Hit_Metal(targ))
    {
        VR_Metal_Hit(targ, org, vel * 0.2, damage, VR_METAL_BLUNT);
        return;
    }
    if(VR_Crate_Wooden(targ))
    {
        dprint(sprintf("hit: %s is wood: splinters at %v\n", targ.classname, org));
        VR_Crate_Splinters(org, vel * 0.2, damage);
        return;
    }
    SpawnBlood(org, vel, damage);
};

// The same, sprayed every way (SpawnBloodSplash).
void(entity targ, vector org, float damage) VR_HitBloodSplash =
{
    if(VR_WallTorch_IsTorch(targ)) { return; }
    if(targ.vr_shootnade)
    {
        VR_GrenShot_Sparks(org);
        return;
    }
    if(VR_Hit_Metal(targ))
    {
        VR_Metal_Hit(targ, org, [crandom() * 60, crandom() * 60, random() * 40 + 30], damage, VR_METAL_BLUNT);
        return;
    }
    if(VR_Crate_Wooden(targ))
    {
        dprint(sprintf("hit: %s is wood: splinters at %v\n", targ.classname, org));
        VR_Crate_Splinters(org, [crandom() * 48, crandom() * 48, random() * 24 + 20], damage);
        return;
    }
    SpawnBloodSplash(org, damage);
};

// ----------------------------------------------------------------------------
// Sight

// Whether a crate or an explosive box lying about hides `xTarget` from `xLooker`, whose eyes are at `xEye` (visible(),
// after the level let the look through): the line to its head meets one's shape (sightblocked: Box3D's, as it lies).
// Not while it carries one (in either hand): what it holds up is seen, and so is it.
float(entity xLooker, entity xTarget, vector xEye) VR_Crate_HidesFrom =
{
    if(!cvar_hget(cvarh_vr_crate_sight))
    {
        return FALSE;
    }
    vector at = xTarget.origin + xTarget.view_ofs;
    if(xTarget.classname == "player")
    {
        if(xTarget.mainhand_held.vr_blocksight || xTarget.offhand_held.vr_blocksight)
        {
            return FALSE;
        }
        at = VRGetHeadPos(xTarget);
    }
    entity blocker = sightblocked(xEye, at, xLooker, xTarget);
    if(blocker == world)
    {
        if(cvar_hget(cvarh_vr_debug_crates) >= 2 && xTarget.classname == "player")
        {
            dprint(sprintf("crates: %s sees %s (nothing in the way)\n", xLooker.classname, xTarget.netname));
        }
        return FALSE;
    }
    if(cvar_hget(cvarh_vr_debug_crates) && xTarget.classname == "player")
    {
        dprint(sprintf("crates: %s can't see %s: %s %v in the way\n", xLooker.classname, xTarget.netname, blocker.classname,
            blocker.origin));
    }
    return TRUE;
};

// ----------------------------------------------------------------------------
// Hits

// A solid prop's hit (Box3D's, .vr_impact: a crate's, an explosive box's) on what it met (`other`), at `xSpeed` m/s: thrown
// (VR_Carry_Release: a heavy prop is a missile at a lower speed) into something that takes damage, it hits it as any
// thrown thing does (VR_Thrown_Damage), once; flung into a monster (VR_Prop_Flung), likewise. Landing and sliding on the
// floor or against a wall its throw goes on; come down below the throw's least speed, or longer than VR_THROWN_BOX_TIME
// since it left the hand, the throw is over.
void(float xSpeed) VR_SolidProp_Impact =
{
    // Not thrown, but flung into a monster or player as fast as a throw: it hurts as a thrown one. Its own share of that
    // speed: not what it hit's (a player running into it; after the step its own velocity is the bounce's).
    float approach = xSpeed * VRMetersToUnits();
    VR_Prop_Flung(approach, approach - vlen(other.velocity), FALSE);
    entity thrower = VR_Throw_Thrower(self); // (its owner; past vr_throw_self_grace the player who threw it)
    if(self.throwhit == QVR_THROWHIT_NEVER_HIT && thrower.classname == "player")
    {
        float speed = max(vlen(self.velocity), xSpeed * VRMetersToUnits());
        if(time - self.carry_thrown > VR_THROWN_BOX_TIME || speed < VR_Thrown_HitSpeed(self))
        {
            self.throwhit = QVR_THROWHIT_HIT;
            self.owner = world; // the throw is over: no longer the thrower's (his traces pass his own missiles)
        }
        else if(other != world && other.takedamage && other != self.owner)
        {
            self.throwhit = QVR_THROWHIT_HIT;
            float dmg = VR_Thrown_Damage(self, speed);
            dprint(sprintf("%s: thrown into %s at %.1f u/s (%.1f m/s against it): %.1f\n", self.classname, other.classname,
                speed, xSpeed, dmg));
            // (An explosive box thrown, or one struck: a metal knock.)
            float metal = VR_Prop_Metal(self) || VR_Prop_Metal(other);
            VR_SoundVaried(self, CHAN_AUTO, metal ? VR_Metal_Sound(VR_METAL_BLUNT) : "fisthit.wav", 1, ATTN_NORM);
            if(!metal)
            {
                VR_Blunt_HitLayer(self, CHAN_AUTO, other);
            }
            if(VR_Crate_Wooden(other))
            {
                VR_Crate_Splinters(self.origin, self.velocity, dmg);
            }
            else
            {
                spawn_touchblood(dmg);
            }
            float pop = VR_Decap_ThrownArm(other, self, thrower, dmg); // VR: a heavy blunt thing at the head pops it if it kills (vr_decap.qc)
            T_Damage(other, self, thrower, dmg);
            if(pop)
            {
                VR_Decap_After(other);
            }
            self.vr_fling_hit = time; // (not hit again as a flung prop as it bounces off: VR_FLING_REHIT)
            self.owner = world; // (as above)
            self.vr_throw_self = 0;
        }
    }
    // The throw over, it is nobody's: a trace from its owner passes through what he owns, so a crate thrown and
    // never given back ignored his shots and blows, though a thing he threw broke it (NOTES.md e1m1_2026-10-01_02-48-29,
    // e1m2_2026-10-01_02-52-58).
    if(self.throwhit != QVR_THROWHIT_NEVER_HIT && self.carry_player == world && self.owner.classname == "player")
    {
        self.owner = world;
    }
};

// ----------------------------------------------------------------------------
// Overhead slams (NOTES.md vrfiringrange_2026-10-07_23-08-07; ROUND21.md, "Heavy props thrown and slammed"): a crate or
// a barrel held in both hands over the head (the hands' middle vr_carry_slam_height cm over the eyes or more), then
// within vr_carry_slam_window s swung down (the hands at vr_carry_slam_speed m/s or more, mostly downwards) onto
// something (a monster, a prop, a wall, the floor: its box swept a frame and a little ahead) breaks at once, what it met
// hurt as by a heavy blow (vr_carry_slam_damage). Let go of from there (thrown down, or dropped), its first landing at
// vr_crate_slam_throw_speed m/s or more takes vr_crate_slam_throw_damage of its full health: two such throws break it.

// Each frame `xBox` is held in both hands (VR_Carry_TwoHandFrame; self the player).
void(entity xBox) VR_Crate_SlamFrame =
{
    if(!cvar_hget(cvarh_vr_carry_slam) || !VR_Crate_Is(xBox) || xBox.takedamage == DAMAGE_NO || xBox.health <= 0)
    {
        return;
    }
    float m2u = VRMetersToUnits();
    vector mid = (VRGetHandPos(cVR_MainHand) + VRGetHandPos(cVR_OffHand)) * 0.5;
    vector head = VRGetHeadPos(self);
    if(mid_z >= head_z + cvar_hget(cvarh_vr_carry_slam_height) * 0.01 * m2u)
    {
        if(xBox.vr_slam_armed < time)
        {
            dprint(sprintf("slam: %s held over the head\n", xBox.model));
        }
        xBox.vr_slam_armed = time + max(0, cvar_hget(cvarh_vr_carry_slam_window));
        return; // (still up: not swung down yet)
    }
    if(time > xBox.vr_slam_armed)
    {
        return;
    }
    vector vel = (VRGetHandVel(cVR_MainHand) + VRGetHandVel(cVR_OffHand)) * 0.5; // m/s
    float speed = vlen(vel);
    float need = max(0.1, cvar_hget(cvarh_vr_carry_slam_speed));
    if(speed < need || vel_z > -0.3 * speed)
    {
        return;
    }
    tracebox(xBox.origin, xBox.mins, xBox.maxs, xBox.origin + vel * m2u * (frametime + 0.03), FALSE, self);
    entity struck = trace_ent;
    if((trace_fraction >= 1 && !trace_startsolid) || struck == xBox)
    {
        return;
    }
    float dmg = 0;
    if(struck != world && struck.takedamage && struck.health > 0)
    {
        // A heavy blow: a punch's with it in hand (vr_dmg_fist, Held Weapon: Punch x, its Melee Damage x and weight's
        // curve) times its speed over the least (at most twice) and vr_carry_slam_damage.
        dmg = cvar_hget(cvarh_vr_dmg_fist) * VR_Carry_PunchMult(self, cVR_MainHand) * VR_Weight_MeleeMult(self, cVR_MainHand) *
              cvar_hget(cvarh_vr_melee_dmg_multiplier) * min(speed / need, 2) * max(0, cvar("vr_carry_slam_damage"));
    }
    dprint(sprintf("slam: %s swung down at %.1f m/s onto %s: %.1f damage to it, broken\n", xBox.model, speed,
        struck == world ? "the level" : struck.classname, dmg));
    VR_SoundVaried(xBox, CHAN_AUTO, VR_Prop_Metal(struck) ? VR_Metal_Sound(VR_METAL_BLUNT) : "fisthit.wav", 1, ATTN_NORM);
    entity crate = xBox;
    if(dmg > 0)
    {
        if(struck.flags & FL_MONSTER)
        {
            VR_Blunt_HitLayer(crate, CHAN_AUTO, struck);
            spawn_touchblood(dmg);
        }
        T_Damage(struck, crate, self, dmg);
    }
    if(crate.takedamage != DAMAGE_NO && crate.health > 0)
    {
        T_Damage(crate, struck, self, crate.health + 1);
    }
};

// `xBox` let go of by `xHands` hands (VR_Carry_Release; self the player): from over the head (within the slam's window),
// its first hard landing hurts it (VR_Crate_Impact).
void(entity xBox, float xHands) VR_Crate_SlamReleased =
{
    xBox.vr_slam_thrown = 0;
    if(xHands == 2 && VR_Crate_Is(xBox) && time <= xBox.vr_slam_armed && cvar_hget(cvarh_vr_carry_slam))
    {
        xBox.vr_slam_thrown = time;
        dprint(sprintf("slam: %s let go of from over the head\n", xBox.model));
    }
    xBox.vr_slam_armed = 0;
};

// A crate's hardest hit of a frame (Box3D's): what it met thrown or flung (VR_SolidProp_Impact), and itself: from 70% of
// Breaks On Impact (vr_crate_impact) it takes its health times (speed / that)^2, so at it, it breaks (dropped from about
// 85 units; the upper crate of a stack falling off the lower lands at about 10 m/s and doesn't).
void(float xSpeed) VR_Crate_Impact =
{
    VR_SolidProp_Impact(xSpeed);
    // Let go of from over the head (VR_Crate_SlamReleased): its first hard landing (within 3 s) hurts it.
    if(self.vr_slam_thrown && self.carry_player == world && self.takedamage != DAMAGE_NO && self.health > 0)
    {
        if(time > self.vr_slam_thrown + 3)
        {
            self.vr_slam_thrown = 0;
        }
        else if(xSpeed >= cvar("vr_crate_slam_throw_speed"))
        {
            self.vr_slam_thrown = 0;
            float hurt = self.max_health * max(0, cvar("vr_crate_slam_throw_damage"));
            dprint(sprintf("slam: %s thrown down from over the head hit %s at %.1f m/s: %.1f of its %.1f\n", self.model,
                other == world ? "the level" : other.classname, xSpeed, hurt, self.health));
            T_Damage(self, world, world, hurt);
            if(self.takedamage == DAMAGE_NO || self.health <= 0)
            {
                return;
            }
        }
    }
    float threshold = cvar("vr_crate_impact");
    if(threshold <= 0 || xSpeed < threshold * 0.7 || self.takedamage == DAMAGE_NO || self.health <= 0)
    {
        return;
    }
    float r = xSpeed / threshold;
    dprint(sprintf("crate at %v: hit %s at %.1f m/s: %.1f\n", self.origin, other.classname, xSpeed, self.max_health * r * r));
    T_Damage(self, world, world, self.max_health * r * r);
};

// Hurt, not broken: a knock of wood, at most every 0.1 s.
void(entity attacker, float damage) VR_Crate_Pain =
{
    dprint(sprintf("crate at %v: hit by %s for %.1f, %.1f left\n", self.origin, attacker.classname, damage, self.health));
    if(time < self.vr_crate_hittime + 0.1)
    {
        return;
    }
    self.vr_crate_hittime = time;
    float k = floor(random() * 4);
    string s = k < 1 ? "vr/phys/wood_m1.wav" : (k < 2 ? "vr/phys/wood_m2.wav" : (k < 3 ? "vr/phys/wood_m3.wav" : "vr/phys/wood_m4.wav"));
    sound(self, CHAN_AUTO, s, min(1, 0.4 + damage * 0.04), ATTN_NORM);
};

// ----------------------------------------------------------------------------
// Monsters' shots (FireBulletsImpl)

float VR_SHOT_OVER_HEAD = 5; // units: the top of a player's head over the headset's middle (vr_hull.cpp hitBox's)

// A crate `xHeld` held by a player in the way of a monster's bullet from `xA` to `xB` before what it hit (trace_*): the
// crate is hit there, met by its shape as the hand holds it (vr_crate_shield; 2: an explosive box too).
void(entity xHeld, vector xA, vector xB, float xShield) VR_Crate_ShieldShot =
{
    if(xHeld == world || xHeld.carry_player == world || !xHeld.takedamage || !xHeld.vr_blocksight ||
       !(VR_Crate_Is(xHeld) || xShield >= 2))
    {
        return;
    }
    float t = heldshape(xHeld, xA, xB);
    if(t >= trace_fraction)
    {
        return;
    }
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("shot: %s held by %s takes it (%.0f units before %s)\n", xHeld.classname,
            xHeld.carry_player.netname, vlen(xB - xA) * (trace_fraction - t), trace_ent.classname));
    }
    trace_ent = xHeld;
    trace_fraction = t;
    trace_endpos = xA + (xB - xA) * t;
    trace_plane_normal = normalize(xA - xB);
    trace_allsolid = FALSE;
    trace_startsolid = FALSE;
    trace_inopen = TRUE;
    trace_inwater = FALSE;
};

// After the traceline of a monster's bullet from `xA` to `xB` (trace_* its result; NOTES.md e1m1_2026-10-01_02-50-03,
// "if I'm holding a crate in front of my body ... enemy bullets should hit the crate before me"): a crate a player holds
// up in its way is hit first (vr_crate_shield): held, it is not solid, and they went through it. (Crouched behind a
// crate, those flying over his head miss him: his hit box reaches to his head, vr_hull_hit_head, vr_hull.cpp hitBox.)
void(vector xA, vector xB) VR_Crate_MonsterShot =
{
    float shield = cvar_hget(cvarh_vr_crate_shield);
    if(shield <= 0)
    {
        return;
    }
    for(entity p = find(world, classname, "player"); p != world; p = find(p, classname, "player"))
    {
        VR_Crate_ShieldShot(p.mainhand_held, xA, xB, shield);
        if(p.offhand_held != p.mainhand_held)
        {
            VR_Crate_ShieldShot(p.offhand_held, xA, xB, shield);
        }
    }
};

// A monster's bullets at a player in VR (EnemyFireBullets, vr_hull_hit_head): aimed no higher than 70% of his real
// height (his head's), as Quake's aim at his box's middle is for a player standing: crouched, the shots come lower
// (into the crate he crouches behind), not all over his head.
vector(vector xSrc, vector xDir, entity xEnemy) VR_Crate_ShotAim =
{
    if(!cvar_hget(cvarh_vr_hull_hit_head) || xEnemy.classname != "player" || xEnemy.headpos == '0 0 0')
    {
        return xDir;
    }
    vector flat = xDir;
    flat_z = 0;
    float along = vlen(flat);
    vector to = xEnemy.origin - xSrc;
    to_z = 0;
    float far = vlen(to);
    if(along < 0.01 || far < 1)
    {
        return xDir;
    }
    float most = xEnemy.absmin_z + 0.7 * (xEnemy.headpos_z + VR_SHOT_OVER_HEAD - xEnemy.absmin_z);
    float passes = xSrc_z + xDir_z * far / along; // the height it passes him at
    if(passes <= most)
    {
        return xDir;
    }
    return normalize(flat * (far / along) + '0 0 1' * (most - xSrc_z));
};

// ----------------------------------------------------------------------------
// Pieces

// Gone (burst, burnt away): nothing left of it, removed the next frame.
void(entity xPiece) VR_CratePiece_Gone =
{
    VR_Carry_Forget(xPiece);
    // Removed the next frame, not now: what struck it may still use it this frame.
    xPiece.takedamage = DAMAGE_NO;
    xPiece.th_die = SUB_Null;
    xPiece.vr_cratekind = 0;
    xPiece.touch = SUB_Null;
    xPiece.handtouch = SUB_Null;
    xPiece.vr_wpntouch = SUB_Null;
    xPiece.flags = xPiece.flags - (xPiece.flags & FL_FORCEGRABBABLE);
    xPiece.movetype = MOVETYPE_NONE;
    xPiece.solid = SOLID_NOT;
    xPiece.velocity = '0 0 0';
    xPiece.avelocity = '0 0 0';
    xPiece.vr_spin = '0 0 0';
    xPiece.vr_rigid = 0;
    xPiece.model = string_null;
    xPiece.modelindex = 0;
    setorigin(xPiece, xPiece.origin);
    xPiece.think = SUB_Remove;
    xPiece.nextthink = time + 0.1;
};

// Gone: bursts into a cloud of wood dust, with a crumble (once a frame).
void(entity xPiece, vector xDir) VR_CratePiece_Burst =
{
    vector org = (xPiece.absmin + xPiece.absmax) * 0.5;
    dprint(sprintf("crate: a piece bursts at %v\n", org));
    particle2(org, xDir, QVR_PARTICLE_PRESET_WOODDUST, 10);
    if(vr_crate_piecesoundtime != time)
    {
        vr_crate_piecesoundtime = time;
        sound(xPiece, CHAN_AUTO, random() < 0.5 ? "vr/crate_dust1.wav" : "vr/crate_dust2.wav", 0.7, ATTN_NORM);
    }
    VR_CratePiece_Gone(xPiece);
};

// Burnt away (it has burnt its time: vr_burn_piece_time): a last puff of embers and smoke, a soft crumble.
void(entity xPiece) VR_CratePiece_BurnAway =
{
    vector org = (xPiece.absmin + xPiece.absmax) * 0.5;
    dprint(sprintf("crate: a piece burns away at %v\n", org));
    particle2(org, '0 0 50', QVR_PARTICLE_PRESET_LAVASPIKE, 14);
    particle2(org, '0 0 25', QVR_PARTICLE_PRESET_SMOKE, 4);
    if(vr_crate_piecesoundtime != time)
    {
        vr_crate_piecesoundtime = time;
        sound(xPiece, CHAN_AUTO, random() < 0.5 ? "vr/crate_dust1.wav" : "vr/crate_dust2.wav", 0.35, ATTN_NORM);
    }
    VR_CratePiece_Gone(xPiece);
};

// Its damage, from T_Damage (instead of Quake's): any hit bursts it. Held, it takes none.
float(entity targ, entity inflictor, entity attacker, float damage) VR_CratePiece_Damage =
{
    if(targ.carry_player != world || damage <= 0 || !targ.takedamage)
    {
        return FALSE;
    }
    if(VR_Burn_JustLit(targ))
    {
        return TRUE; // set on fire by this very hit (a lava nail, a thrown torch): it burns, it doesn't burst
    }
    vector dir = '0 0 0';
    if(inflictor != world && inflictor != targ)
    {
        dir = normalize((targ.absmin + targ.absmax) * 0.5 - (inflictor.absmin + inflictor.absmax) * 0.5);
    }
    VR_CratePiece_Burst(targ, dir);
    return TRUE;
};

void() VR_CratePiece_Die =
{
    VR_CratePiece_Burst(self, '0 0 0');
};

// Struck by a hand or a weapon at a swing's speed (VR_Carry_Nudge; vr_melee_speed): it bursts. Not by the hand that just
// let go of it (a throw's follow-through).
void(entity xPlayer, float xHand, entity xPiece) VR_CratePiece_Struck =
{
    float speed = vlen(VRGetEntHandVel(xPlayer, xHand));
    if(speed < max(0.1, cvar_hget(cvarh_vr_melee_speed)) || !xPiece.takedamage ||
       (xPiece.carry_letgo == xPlayer && time < xPiece.carry_letgo_time + 0.4))
    {
        return;
    }
    if(VR_Burn_PieceStruck(xPlayer, xHand, xPiece))
    {
        return; // a lit torch's swing: set on fire
    }
    T_Damage(xPiece, xPlayer, xPlayer, 5);
};

// Armed a moment after it is made (not burst by the rest of the blow that broke its crate); then, past its time (Pieces
// Last), it fades away over two seconds (not while in a hand: taken, it is whole again).
void() VR_CratePiece_Think =
{
    if(!self.takedamage && self.vr_cratekind == 3)
    {
        self.takedamage = DAMAGE_YES; // (not DAMAGE_AIM: no autoaim at it)
    }
    float life = cvar("vr_crate_piece_time");
    if(self.carry_player != world || life <= 0 || time < self.vr_crate_born + life)
    {
        self.alpha = 0;
        self.nextthink = time + 1;
        return;
    }
    if(!self.alpha)
    {
        self.alpha = 1;
    }
    self.alpha = self.alpha - 0.05;
    if(self.alpha <= 0.05)
    {
        VR_Carry_Forget(self);
        remove(self);
        return;
    }
    self.nextthink = time + 0.1;
};

void() VR_CratePiece_Touch =
{
    forcegrabbable_touch();
};

void(entity p) VR_CratePiece_Setup =
{
    p.classname = "vr_crate_piece";
    p.vr_cratekind = 3;
    p.movetype = MOVETYPE_TOSS;
    p.solid = SOLID_NOT_BUT_TOUCHABLE;
    p.flags = p.flags | FL_FORCEGRABBABLE | FL_EASYHANDTOUCH;
    p.touch = VR_CratePiece_Touch;
    p.handtouch = SUB_Null; // nothing to take (VR_Carry_Setup keeps it as carry_use)
    p.throwhit = QVR_THROWHIT_HIT;
    p.owner = world;
    SetThrownWeaponDealDamage(p, FALSE);
    SetThrownWeaponDisappearOnHit(p, FALSE);
    VR_Carry_Setup(p); // carried, nudged, thrown; a rigid body (vr_rigid 1)
    p.health = 1;
    p.takedamage = DAMAGE_NO; // armed in a moment (VR_CratePiece_Think)
    p.th_die = VR_CratePiece_Die;
    p.vr_crate_born = time;
    p.think = VR_CratePiece_Think;
    p.nextthink = time + 0.15;
};

// Room for `xMore` pieces under Most Pieces (vr_crate_piece_max): the oldest lying about burst first (not one in a hand).
void(float xMore) VR_CratePiece_MakeRoom =
{
    float most = max(0, cvar("vr_crate_piece_max"));
    float count = 0;
    entity e = find(world, classname, "vr_crate_piece");
    while(e)
    {
        count++;
        e = find(e, classname, "vr_crate_piece");
    }
    while(count + xMore > most && count > 0)
    {
        entity oldest = world;
        e = find(world, classname, "vr_crate_piece");
        while(e)
        {
            if(e.vr_cratekind == 3 && e.carry_player == world && (oldest == world || e.vr_crate_born < oldest.vr_crate_born))
            {
                oldest = e;
            }
            e = find(e, classname, "vr_crate_piece");
        }
        if(oldest == world)
        {
            return;
        }
        VR_CratePiece_Burst(oldest, '0 0 0');
        oldest.classname = "vr_crate_piece_gone"; // (counted no more)
        count--;
    }
};

// The pieces of `crate`, from all over its box, flung out from its middle `mid` and along the blow `dir`.
void(entity crate, vector mid, vector dir) VR_Crate_Pieces =
{
    float n = floor(max(0, cvar("vr_crate_pieces")) * (crate.vr_cratekind == 2 ? 1.5 : 1) + 0.5);
    n = min(n, max(0, cvar("vr_crate_piece_max")));
    if(n <= 0)
    {
        return;
    }
    VR_CratePiece_MakeRoom(n);
    vector half = (crate.absmax - crate.absmin) * 0.35;
    for(float i = 0; i < n; i++)
    {
        entity p = spawn();
        float k = random();
        setmodel(p, k < 0.25 ? "progs/vr_plank1.mdl" : (k < 0.55 ? "progs/vr_plank2.mdl" :
                   (k < 0.8 ? "progs/vr_plank3.mdl" : "progs/vr_plank4.mdl")));
        p.skin = crate.skin;
        vector at = mid;
        at_x = at_x + crandom() * half_x;
        at_y = at_y + crandom() * half_y;
        at_z = at_z + random() * half_z; // (the upper half: none starts in the floor)
        setorigin(p, at);
        p.angles = '0 0 0' + '1 0 0' * (random() * 360) + '0 1 0' * (random() * 360) + '0 0 1' * (random() * 360);
        vector away = normalize(at - mid + '0 0 1' * crandom());
        p.velocity = away * (60 + random() * 120) + '0 0 1' * (40 + random() * 100) + dir * 120;
        p.vr_spin = '1 0 0' * crandom() * 9 + '0 1 0' * crandom() * 9 + '0 0 1' * crandom() * 9;
        VR_CratePiece_Setup(p);
        VR_Burn_NewPiece(crate, p); // burnt through: charred; broken while burning: on fire
    }
};

// ----------------------------------------------------------------------------
// What a crate holds

// An item made where the crate was (`at`: the middle of its floor), by its spawn function (a brush item's origin is its
// corner: its middle there), placed at once (VRPlaceItemNow: an object at its final size from the first frame), and
// popped out (vr_crate_item_pop, m/s): up, and away from the crate's middle (`mid`) or along the blow (`dir`).
void(vector at, float kind, vector mid, vector dir) VR_Crate_Item =
{
    entity oself = self;
    self = spawn();
    setorigin(self, at - '16 16 0');
    if(kind == 0)      { self.classname = "item_shells"; item_shells(); }
    else if(kind == 1) { self.classname = "item_spikes"; item_spikes(); }
    else if(kind == 2) { self.classname = "item_rockets"; item_rockets(); }
    else if(kind == 3) { self.classname = "item_cells"; item_cells(); }
    else               { self.classname = "item_health"; self.spawnflags = 1; item_health(); } // H_ROTTEN: the small one (15)
    dprint(sprintf("crate: holds %s\n", self.classname));
    if(VRPlaceItemNow())
    {
        float pop = max(0, cvar("vr_crate_item_pop")) * VRMetersToUnits();
        if(pop > 0 && self.movetype != MOVETYPE_NONE)
        {
            vector away = at - mid;
            away_z = 0;
            away = normalize(away) + dir * 0.5 + ('1 0 0' * crandom() + '0 1 0' * crandom()) * 0.3;
            away_z = 0;
            away = normalize(away);
            self.velocity = away * pop * (0.35 + random() * 0.2) + '0 0 1' * pop * (0.85 + random() * 0.3);
            self.flags &= ~FL_ONGROUND;
        }
        dprint(sprintf("crate: %s at %v, box %v .. %v, scale %v, velocity %v\n", self.classname, self.origin, self.mins,
                       self.maxs, self.model_scale + '1 1 1', self.velocity));
    }
    self = oself;
};

// A small box of ammo for a weapon the player may use (the shotgun's shells always; nails, rockets, cells for a nailgun,
// a grenade or rocket launcher, the lightning gun he has: VR_WeaponUtil_EntIsEligibleForWeapon), at vr_crate_ammo; a
// small health box at vr_crate_health_box.
void(entity crate, vector mid, vector dir) VR_Crate_Contents =
{
    vector at = mid;
    at_z = crate.absmin_z + 1;
    entity player = find(world, classname, "player");
    if(player != world && random() < cvar("vr_crate_ammo"))
    {
        float nails = (VR_WeaponUtil_EntIsEligibleForWeapon(player, WID_NAILGUN) ||
                       VR_WeaponUtil_EntIsEligibleForWeapon(player, WID_SUPER_NAILGUN));
        float rockets = (VR_WeaponUtil_EntIsEligibleForWeapon(player, WID_GRENADE_LAUNCHER) ||
                         VR_WeaponUtil_EntIsEligibleForWeapon(player, WID_ROCKET_LAUNCHER));
        float cells = VR_WeaponUtil_EntIsEligibleForWeapon(player, WID_LIGHTNING);
        // One of those at random: shells (0) and each other kind he may use.
        float pick = floor(random() * (1 + nails + rockets + cells) * 0.999);
        float kind = 0;
        if(pick > 0 && nails)
        {
            kind = 1;
            pick--;
        }
        if(pick > 0 && rockets)
        {
            kind = 2;
            pick--;
        }
        if(pick > 0 && cells)
        {
            kind = 3;
        }
        VR_Crate_Item(at - '6 0 0', kind, mid, dir);
    }
    if(random() < cvar("vr_crate_health_box"))
    {
        VR_Crate_Item(at + '6 0 0', 4, mid, dir);
    }
};

// ----------------------------------------------------------------------------
// Breaking

// Broken (its health gone: Killed): a crack of wood and a cloud of dust, its pieces flung out along the blow (from what
// broke it: damage_inflictor), what it held left where it stood; gone.
void() VR_Crate_Break =
{
    entity crate = self;
    vector mid = (crate.absmin + crate.absmax) * 0.5;
    vector dir = '0 0 0';
    if(damage_inflictor != world && damage_inflictor != crate)
    {
        dir = normalize(mid - (damage_inflictor.absmin + damage_inflictor.absmax) * 0.5);
    }
    dprint(sprintf("crate: breaks at %v (by %s)\n", mid, damage_inflictor.classname));
    VR_Carry_Forget(crate); // (broken in a hand)
    float k = floor(random() * 3);
    sound(crate, CHAN_AUTO, k < 1 ? "vr/crate_break1.wav" : (k < 2 ? "vr/crate_break2.wav" : "vr/crate_break3.wav"), 1, ATTN_NORM);
    particle2(mid, dir, QVR_PARTICLE_PRESET_WOODDUST, crate.vr_cratekind == 2 ? 45 : 30);

    // Gone first (not solid, no body): the pieces and what it held come out where it was.
    crate.takedamage = DAMAGE_NO;
    crate.th_die = SUB_Null;
    crate.vr_blocksight = 0;
    crate.vr_rigid = 0;
    crate.movetype = MOVETYPE_NONE;
    crate.solid = SOLID_NOT;
    crate.velocity = '0 0 0';
    crate.avelocity = '0 0 0';
    crate.vr_spin = '0 0 0';
    crate.model = string_null;
    crate.modelindex = 0;
    setorigin(crate, crate.origin);

    VR_Crate_Pieces(crate, mid, dir);
    VR_Crate_Contents(crate, mid, dir);

    crate.vr_cratekind = 0;
    crate.classname = "vr_crate_broken";
    crate.think = SUB_Remove;
    crate.nextthink = time + 0.1;
};

// ----------------------------------------------------------------------------
// Making them

// A crate's health: Health (vr_crate_health) for a small one, 1.6 times it for a large one (their faces' areas), times
// its Size's square (Held Object Offsets). 0: it can't be broken.
float(entity e) VR_Crate_FullHealth =
{
    float size = propvalue(e, "size");
    if(size <= 0)
    {
        size = 1;
    }
    return max(0, cvar("vr_crate_health")) * (e.vr_cratekind == 2 ? 1.6 : 1) * size * size;
};

// `e` (its model set, turned and placed: crateput, crateplace) made a crate of `kind` (1 small, 2 large).
void(entity e, float kind) VR_Crate_Setup =
{
    e.classname = "vr_crate";
    e.vr_cratekind = kind;
    e.vr_blocksight = 1;
    e.handtouch = SUB_Null; // nothing to take (VR_Carry_Setup keeps it as carry_use)
    VR_Carry_Setup(e);
    e.movetype = MOVETYPE_TOSS;
    e.solid = SOLID_BBOX;
    e.vr_rigid = 2;
    e.carry_solid = SOLID_BBOX;
    e.carry_rigid = 2;
    e.vr_impact = VR_Crate_Impact;
    e.velocity = '0 0 0';
    float h = VR_Crate_FullHealth(e);
    e.health = max(1, h);
    e.max_health = e.health;
    e.takedamage = h > 0 ? DAMAGE_AIM : DAMAGE_NO;
    e.th_die = VR_Crate_Break;
    e.th_pain = VR_Crate_Pain;
    setorigin(e, e.origin); // (linked as solid)
};

/*QUAKED vr_crate (0.6 0.45 0.25) (-16 -16 0) (16 16 32) LARGE
Quake VR: a wooden crate, placed by hand: a heavy physics prop (pushed, tipped, stacked, stood on, carried by hand) that
breaks into light pieces when shot, struck, blown up or hit hard, sometimes holding a small box of ammo or health
(Crates' settings), and hides the player from monsters behind it. Small: 32 units a side; LARGE: 40 x 40 x 48. "angle":
its yaw; "skin": 0 pine, 1 brown, 2 weathered. It rests on whatever is below its origin (within 128 units). Placed whatever
Crates' options are.
*/
void() vr_crate =
{
    float kind = (self.spawnflags & 1) ? 2 : 1;
    setmodel(self, kind == 2 ? "progs/vr_crate2.mdl" : "progs/vr_crate1.mdl"); // (precached: VR_Crates_Precache)
    if(!crateplace(self))
    {
        remove(self);
        return;
    }
    VR_Crate_Setup(self, kind);
};

/*QUAKED vr_barrel (0.6 0.45 0.25) (-11 -11 -16) (11 11 16) LYING
Quake VR: a wooden barrel, placed by hand (vrstart's, by the pier and the pavilion): a crate in all but its shape (a
heavy physics prop: pushed, tipped, rolled, stood on, carried by hand; broken into boards by shots, blows, explosions
and hard hits, sometimes holding a small box of ammo or health; it hides the player from monsters behind it). 22 units
across, 32 high; its origin is its middle. "angle": its yaw; "skin": 0 pine, 1 brown, 2 weathered; LYING: on its side
(along its yaw's left). It rests on whatever is below its origin (within 128 units). Placed whatever Crates' options are.
*/
void() vr_barrel =
{
    setmodel(self, "progs/vr_barrel.mdl"); // (precached: VR_Crates_Precache)
    vector half = '11.5 11.5 16';
    if(self.spawnflags & 1)
    {
        self.angles_z = 90;
        half = '16 16 11.5'; // (any yaw: the lying barrel's box round it, the physics' shape is the model's)
    }
    traceline(self.origin + '0 0 1', self.origin - '0 0 128', TRUE, self); // (nomonsters)
    if(trace_fraction < 1 && !trace_startsolid)
    {
        self.origin_z = trace_endpos_z + half_z + 0.02;
    }
    setsize(self, -1 * half, half);
    setorigin(self, self.origin);
    self.flags = self.flags | FL_ONGROUND;
    self.groundentity = world;
    VR_Crate_Setup(self, 1); // (a small crate's health, pieces and sounds)
};

// A test crate (vr_test_spawn 107, 108, 109: Debug > Tests > Thing): `kind` at `at`, turned by `yaw`; `stacked`: a small
// one on it, turned 20 degrees and off its middle by 3 units.
entity(vector at, float kind, float yaw, float stacked) VR_Crate_TestSpawn =
{
    entity e = spawn();
    setmodel(e, kind == 2 ? "progs/vr_crate2.mdl" : "progs/vr_crate1.mdl");
    e.angles = '0 1 0' * yaw;
    setorigin(e, at);
    crateplace(e);
    VR_Crate_Setup(e, kind);
    dprint(sprintf("test spawn: crate at %v, box %v .. %v, health %.0f\n", e.origin, e.mins, e.maxs, e.health));
    if(stacked)
    {
        entity top = spawn();
        setmodel(top, "progs/vr_crate1.mdl");
        top.angles = '0 1 0' * (yaw + 20);
        setorigin(top, at + '3 0 0');
        crateplace(top);
        // Its bottom on the lower one's top (their boxes: absmin and absmax are a unit wider each way, which left a gap
        // of 2 units the asleep top never fell across: NOTES.md vrfiringrange_2026-10-01_11-44-23).
        setorigin(top, [top.origin_x, top.origin_y, e.origin_z + e.maxs_z - top.mins_z + 0.05]);
        VR_Crate_Setup(top, 1);
        dprint(sprintf("test spawn: a crate on it at %v\n", top.origin));
    }
    return e;
};

// Every server frame (StartFrame): at the first of a new map, the crates.
void() VR_Crates_Frame =
{
    if(vr_crates_done)
    {
        return;
    }
    vr_crates_done = TRUE;
    if(spawnServerFromSaveFile)
    {
        return; // a saved game: its entities replace these
    }
    float n = crateplan();
    for(float i = 0; i < n; i++)
    {
        entity e = spawn();
        setmodel(e, cratemodel(i));
        float kind = crateput(e, i); // its skin, turn, place and box
        if(!kind)
        {
            remove(e);
            continue;
        }
        VR_Crate_Setup(e, kind);
        VR_Crowbar_OnCrate(e, i); // (a stack's top comes after the crate under it)
    }
};
