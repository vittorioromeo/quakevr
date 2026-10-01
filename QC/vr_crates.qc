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
// a weapon you have (vr_crate_ammo) or a small health box (vr_crate_health_box).
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

// The models (make_crates.py) and sounds (make_crate_sounds.py; the physics sounds' wood knocks); always precached (a
// saved game may hold crates whatever the settings now), and what a crate may hold (the small ammo and health boxes).
void() VR_Crates_Precache =
{
    vr_crates_done = FALSE;
    precache_model("progs/vr_crate1.mdl");
    precache_model("progs/vr_crate2.mdl");
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

// Wood: a crate, a piece of one, an explosive box. Shot or struck, it throws splinters, not blood. Also one the blow has
// just broken (VR_Crate_Break, VR_CratePiece_Burst clear vr_cratekind): blood often comes after the damage.
float(entity e) VR_Crate_Wooden =
{
    return e.vr_cratekind > 0 || e.vr_blocksight > 0 || e.classname == "vr_crate_broken" || e.classname == "vr_crate_piece" ||
           e.classname == "vr_crate_piece_gone";
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
void(entity targ, vector org, vector vel, float damage) VR_HitBlood =
{
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
    VR_Prop_Flung(approach, approach - vlen(other.velocity));
    if(self.throwhit == QVR_THROWHIT_NEVER_HIT && self.owner.classname == "player")
    {
        float lenient = VR_Thrown_Lenient(self);
        float speed = max(vlen(self.velocity), xSpeed * VRMetersToUnits());
        if(time - self.carry_thrown > VR_THROWN_BOX_TIME || speed < cvar_hget(cvarh_vr_throw_hit_min_speed) * lenient)
        {
            self.throwhit = QVR_THROWHIT_HIT;
            self.owner = world; // the throw is over: no longer the thrower's (his traces pass his own missiles)
        }
        else if(other != world && other.takedamage && other != self.owner)
        {
            self.throwhit = QVR_THROWHIT_HIT;
            float dmg = VR_Thrown_Damage(self, speed, lenient);
            dprint(sprintf("%s: thrown into %s at %.1f u/s (%.1f m/s against it): %.1f\n", self.classname, other.classname,
                speed, xSpeed, dmg));
            sound(self, CHAN_AUTO, "fisthit.wav", 1, ATTN_NORM);
            if(VR_Crate_Wooden(other))
            {
                VR_Crate_Splinters(self.origin, self.velocity, dmg);
            }
            else
            {
                spawn_touchblood(dmg);
            }
            T_Damage(other, self, self.owner, dmg);
            self.owner = world; // (as above)
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

// A crate's hardest hit of a frame (Box3D's): what it met thrown or flung (VR_SolidProp_Impact), and itself: from 70% of
// Breaks On Impact (vr_crate_impact) it takes its health times (speed / that)^2, so at it, it breaks (dropped from about
// 85 units; the upper crate of a stack falling off the lower lands at about 10 m/s and doesn't).
void(float xSpeed) VR_Crate_Impact =
{
    VR_SolidProp_Impact(xSpeed);
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

// Its damage, from T_Damage (instead of Quake's): any hit bursts it. Held, it takes none.
float(entity targ, entity inflictor, entity attacker, float damage) VR_CratePiece_Damage =
{
    if(targ.carry_player != world || damage <= 0 || !targ.takedamage)
    {
        return FALSE;
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
