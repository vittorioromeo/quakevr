//
//
//
// ----------------------------------------------------------------------------
// The heavy revolver (WID_REVOLVER; docs/vr-port/ROUND22.md, "The heavy revolver")
// ----------------------------------------------------------------------------
//
// Quake VR's six-shot revolver for 12-gauge shells (the art: Misc/quakevr/models_src/revolver, its README: the parts,
// their pivots, the sight line). One-handed, DOUBLE ACTION: a new pull of the trigger cocks the hammer and turns the
// cylinder a chamber (60 degrees: the next chamber under the hammer, on the bore's line), then the hammer falls
// (vr_revolver_cock_time later) on that chamber: a live shell fires as a slug (vr_dmg_revolver, up to
// vr_revolver_dmg_random more; a grunt dies to one in the head (x1.5) or two in the body; vr_revolver_heavy_mult into a
// big body: an ogre takes 4-5), a spent shell or an empty chamber only clicks. At most a shot each vr_revolver_cooldown s.
// Heavy recoil (slot 38's), a big flash, smoke.
//
// Its six chambers are tracked one by one (the weapon's record, vr_weaponinst.qc: they go with the gun to the holsters,
// the floor, a save): each empty, a live shell or a spent one; its clip is how many are live (the HUD, the ammo screen).
// Drawn in its parts in the hands (the engine's vr_view.cpp setupRevolverParts, from .vr_rv_hud and .vr_rv_cyl: the
// hammer's frames, the crane swung out or home, the cylinder turned, a blue slug shell in each loaded chamber, spent ones
// darker).
//
// Immersive reloading (vr_reload_mode 3, or its own vr_reload_mode_revolver; VR_Revolver_ReloadFrame):
//   - its cylinder swings out (to the left in the right hand) by a flick of the wrist that way (vr_revolver_open_flick,
//     on; its force vr_revolver_open_flick_speed and its wrist flick vr_revolver_open_flick_angle) or B/Y (its eject
//     button: vr_revolver_open_button);
//   - out, its shells drop, spent and live (the spent ones as casings, the live ones as shells to pick up again), when
//     the muzzle is tilted up (vr_revolver_eject_tilt), B/Y is pressed, or the other hand pushes the ejector rod's head
//     back (vr_revolver_eject_rod);
//   - a shell from the pouch (one at a time: VR_Reload_Take) held to the cylinder's back face goes into the empty chamber
//     nearest it (VR_Revolver_LoadInto, from vr_reload.qc's VR_Reload_LoadInto);
//   - the other hand swiping across it spins it (vr_revolver_spin): it slows and stops on a chamber, which changes which
//     chamber comes next;
//   - it swings home by a flick the other way (always; vr_revolver_close_flick_speed, _angle), the other hand pushing it
//     up from below (vr_revolver_close_push) or B/Y with vr_revolver_close_button.
// Out, it doesn't fire. A revolver that leaves the hand swung out swings home (drawn shut anywhere else).
// The other reloading modes load it as any gun (its chambers follow its clip: VR_Revolver_Sync).
//
// Weapon settings slot 38 (vr_weapons.inc: its grip, sights, holsters, recoil, flash). weapon_revolver lies ready to be
// picked up. Armed grunts carry it rarely (vr_agrunt_w_revolver; vr_armedrepl.qc's rare tier), firing it through
// W_FireRevolver.

float RV_CHAMBERS = 6;
float RV_SCALE = 0.45;       // its in-hand Scale (slot 38's): its parts' sizes in world units
float RV_CYL_LEN = 4.4;      // the cylinder (model units: the README): its length,
float RV_CYL_RADIUS = 2.25;  // its radius,
float RV_ROD_TIP = 5.42;     // the ejector rod's head from its back face (v_revolver_cyl.mdl's front)
float RV_SWING = -95;        // the crane's swing out, degrees about the bore (+y towards +z)

// Its record's (vr_weaponinst.qc): what goes with the weapon.
.float wi_rv;      // its chambers: base 4, chamber i's digit (4^i) 0 empty, 1 a live shell, 2 a spent one
.float wi_rv_at;   // the chamber under the hammer, on the bore's line (on top), 0..5
.float wi_rv_open; // its cylinder swung out
.float wi_rv_spin; // swung out and spun: degrees a second (its model's own sense: +y towards +z), 0 still
.float wi_rv_ofs;  // and how far past chamber wi_rv_at's place it is turned, -30..30 degrees

// Each hand's state (an entity of its own a hand: .vr_rv_h, .vr_rv_h2; classname "rv_hand", never sent).
.entity vr_rv_h;
.entity vr_rv_h2;
.float rvh_rearm;   // the trigger pulled since its last pull: no other until it is let go
.float rvh_fire_at; // the cocked hammer falls then (0: not cocked)
.float rvh_shot_at; // its last fall (a shot or a click): the trigger held after it, frame 2
.float rvh_smoke;   // when the wisp of smoke after its last shot rises (0 none)
.float rvh_fo;      // the wrist's roll towards the cylinder's side in one go (degrees),
.float rvh_fop;     // and its fastest (degrees a second)
.float rvh_fc;      // the other way
.float rvh_fcp;
.float rvh_fot;     // the seconds since each flick began (the time window)
.float rvh_fct;
.float rvh_toggled; // when it last swung out or home
.float rvh_rod;     // until when its ejector rod is drawn pushed
.float rvh_spun;    // its last swipe
.float rvh_held_at; // the last time the other hand held anything (a shell) or a shell went in: no spin a moment after
.float rvh_sec;     // B/Y held last frame
.float rvh_tilt;    // the muzzle tilted up (its eject done; again once it is lowered)
.entity rvh_inst;   // the record the hand held last frame
.vector rvh_up;     // the hand's up last frame, and its side towards the cylinder (the flicks' roll)
.vector rvh_side;
.float rvh_upok;

// For the engine (STAT_QVR_REVOLVER, _REVOLVER2, _REVOLVERCYL, _REVOLVERCYL2: vr_view.cpp setupRevolverParts): the
// chambers (12 bits) + 4096 swung out + 8192 the rod pushed + 16384 x the chamber under the hammer + 131072 (a revolver
// in that hand); its cylinder's turn (degrees: -60 x that chamber + its spin's).
.float vr_rv_hud;
.float vr_rv_hud2;
.float vr_rv_cyl;
.float vr_rv_cyl2;

float vr_rv_shots;       // shots fired (tests)
float vr_rv_dry;         // clicks on empty or spent chambers (tests)
float vr_rv_ej_spent;    // spent and live shells dropped (tests)
float vr_rv_ej_live;
float vr_rv_loaded;      // shells loaded by hand, and the last one's chamber (tests)
float vr_rv_lastch;
float vr_rv_refusals;    // swipes refused right after loading (tests), the last one's time
float vr_rv_spinrefused;
float vr_rv_opened;      // times swung out and home (tests)
float vr_rv_closed;
string vr_rv_lasthow;    // what last swung it out, home or dropped its shells (tests)
float vr_rv_lastdmg;     // the last slug's damage dealt, its part and what it hit (tests)
float vr_rv_lastpart;
entity vr_rv_lasthit;
#ifdef QVR_TESTS
void() VR_Revolver_TestFrame; // (vr_revolver_test.qc)
#endif

void() VR_Revolver_Precache =
{
    precache_model("progs/v_revolver.mdl");
    precache_sound("weapons/shotgn2.wav");
    precache_sound("weapons/guncock.wav");
    precache_sound("gunclick.wav");
    precache_sound("vr/reload_ssg_open.wav");
    precache_sound("vr/reload_ssg_close.wav");
    precache_sound("vr/reload_ssg_eject.wav");
    precache_sound("vr/gadget_click_on.wav");
};

float() VR_Revolver_Chambers = { return RV_CHAMBERS; };

// The weapon cycle's next weapon id from `xWeapon` (+1 or -1): the heavy revolver between the shotgun and the super
// shotgun (the shells' slot: weapons.qc W_ChangeWeapon's impulse 2), the nail pistol before the nailgun.
float(float xWeapon, float xDir) VR_Revolver_CycleStep =
{
    if(xDir > 0)
    {
        if(xWeapon == WID_SHOTGUN)  { return WID_REVOLVER; }
        if(xWeapon == WID_REVOLVER) { return WID_SUPER_SHOTGUN; }
    }
    else
    {
        if(xWeapon == WID_SUPER_SHOTGUN) { return WID_REVOLVER; }
        if(xWeapon == WID_REVOLVER)      { return WID_SHOTGUN; }
    }
    return VR_NailPistol_CycleStep(xWeapon, xDir);
};

// ----------------------------------------------------------------------------
// Its chambers

float(float i) VR_RV_Wrap = { return i - floor(i / RV_CHAMBERS) * RV_CHAMBERS; };

float(float i) VR_RV_Pow4 =
{
    if(i <= 0) { return 1; }
    if(i == 1) { return 4; }
    if(i == 2) { return 16; }
    if(i == 3) { return 64; }
    if(i == 4) { return 256; }
    return 1024;
};

// Chamber `i` of the revolver of record `r`: 0 empty, 1 live, 2 spent.
float(entity r, float i) VR_Revolver_Chamber =
{
    float d = floor(r.wi_rv / VR_RV_Pow4(VR_RV_Wrap(i)));
    return d - floor(d / 4) * 4;
};

void(entity r, float i, float xState) VR_Revolver_SetChamber =
{
    i = VR_RV_Wrap(i);
    r.wi_rv = r.wi_rv + (xState - VR_Revolver_Chamber(r, i)) * VR_RV_Pow4(i);
};

// How many of its chambers hold `xState`.
float(entity r, float xState) VR_Revolver_Count =
{
    float n = 0;
    for(float i = 0; i < RV_CHAMBERS; i++)
    {
        if(VR_Revolver_Chamber(r, i) == xState) { n++; }
    }
    return n;
};

// Its chambers as text, from the one under the hammer on (tests, logs): "L" live, "S" spent, "-" empty.
string(entity r) VR_Revolver_Desc =
{
    string s = "";
    for(float k = 0; k < RV_CHAMBERS; k++)
    {
        float c = VR_Revolver_Chamber(r, r.wi_rv_at + k);
        s = strcat(s, c == 1 ? "L" : (c == 2 ? "S" : "-"));
    }
    return s;
};

// Its live chambers made `xLive` (its clip, or with reloading off your shells, up to six: whatever else changed them, a
// reload at the holster, a level's start, a grunt's shots): the next to come under the hammer filled first (empty ones
// before spent ones), the last to come emptied first.
void(entity r, float xLive) VR_Revolver_Sync =
{
    xLive = max(0, min(RV_CHAMBERS, xLive));
    float live = VR_Revolver_Count(r, 1);
    float k;
    float i;
    for(float pass = 0; pass < 2 && live < xLive; pass++)
    {
        for(k = 1; k <= RV_CHAMBERS && live < xLive; k++)
        {
            i = VR_RV_Wrap(r.wi_rv_at + k);
            if(VR_Revolver_Chamber(r, i) == pass * 2)
            {
                VR_Revolver_SetChamber(r, i, 1);
                live++;
            }
        }
    }
    for(k = RV_CHAMBERS; k >= 1 && live > xLive; k--)
    {
        i = VR_RV_Wrap(r.wi_rv_at + k);
        if(VR_Revolver_Chamber(r, i) == 1)
        {
            VR_Revolver_SetChamber(r, i, 0);
            live--;
        }
    }
};

// Its cylinder's turn: degrees about the bore (its model's +y towards +z) from chamber 0 on top.
float(entity r) VR_Revolver_CylAngle = { return -60 * r.wi_rv_at + r.wi_rv_ofs; };

// The pull's turn: the next chamber under the hammer (a spin stops).
void(entity r) VR_Revolver_Advance =
{
    r.wi_rv_at = VR_RV_Wrap(r.wi_rv_at + 1);
    r.wi_rv_ofs = 0;
    r.wi_rv_spin = 0;
};

// The state of `xHand` of `p` (made the first time).
entity(entity p, float xHand) VR_Revolver_Hand =
{
    entity h = xHand == cVR_OffHand ? p.vr_rv_h2 : p.vr_rv_h;
    if(!h)
    {
        h = spawn();
        h.classname = "rv_hand";
        h.owner = p;
        if(xHand == cVR_OffHand) { p.vr_rv_h2 = h; } else { p.vr_rv_h = h; }
    }
    return h;
};

vector(vector a, vector b) VR_RV_Cross =
{
    vector c;
    c_x = a_y * b_z - a_z * b_y;
    c_y = a_z * b_x - a_x * b_z;
    c_z = a_x * b_y - a_y * b_x;
    return c;
};

float(float a) VR_RV_AngleNorm =
{
    a = a - floor(a / 360) * 360;
    return a > 180 ? a - 360 : a;
};

// The swung-out cylinder in `xHand` of self as drawn (the engine's load point there: its back face's middle, its axis
// forward): its back face's middle, its axis, its up and its model's +y (vr_rv_left: mirrored in the off hand) square to
// the axis. FALSE if the engine reported none (a bot's, a test's before its first move).
vector vr_rv_port, vr_rv_axis, vr_rv_up, vr_rv_left;
float(float xHand) VR_Revolver_Frame =
{
    vr_rv_port = xHand == cVR_MainHand ? self.loadportpos : self.offloadportpos;
    vector ax = xHand == cVR_MainHand ? self.loadportaxis : self.offloadportaxis;
    makevectors(VRGetEntHandRot(self, xHand));
    vr_rv_axis = ax == '0 0 0' ? v_forward : normalize(ax);
    vr_rv_up = normalize(v_up - vr_rv_axis * (v_up * vr_rv_axis));
    vr_rv_left = VR_RV_Cross(vr_rv_up, vr_rv_axis);
    if(xHand == cVR_OffHand)
    {
        vr_rv_left = -vr_rv_left; // (the model's +y: the gun mirrored in the off hand)
    }
    return ax != '0 0 0' && vlen(vr_rv_port - VRGetEntHandPos(self, xHand)) < 64;
};

// The empty chamber of the revolver of record `r` in `xGunHand` of self nearest the point `xAt` (a shell's middle at
// its back face), by the angle about its axis; -1 if none is empty.
float(float xGunHand, entity r, vector xAt) VR_Revolver_ChamberFor =
{
    float best = -1;
    float bestOff = 1000;
    float i;
    vector d = '0 0 0';
    if(VR_Revolver_Frame(xGunHand))
    {
        d = xAt - vr_rv_port;
        d = d - vr_rv_axis * (d * vr_rv_axis);
    }
    if(vlen(d) < 0.1)
    {
        // (No frame, or on its axis: the next empty one to come under the hammer.)
        for(i = 1; i <= RV_CHAMBERS; i++)
        {
            if(VR_Revolver_Chamber(r, r.wi_rv_at + i) == 0)
            {
                return VR_RV_Wrap(r.wi_rv_at + i);
            }
        }
        return -1;
    }
    float a = atan2(d * vr_rv_up, d * vr_rv_left) * 57.29578;
    float turn = VR_Revolver_CylAngle(r) + (r.wi_rv_open ? RV_SWING : 0);
    for(i = 0; i < RV_CHAMBERS; i++)
    {
        if(VR_Revolver_Chamber(r, i) != 0)
        {
            continue;
        }
        float off = fabs(VR_RV_AngleNorm(90 + 60 * i + turn - a));
        if(off < bestOff)
        {
            bestOff = off;
            best = i;
        }
    }
    return best;
};

// ----------------------------------------------------------------------------
// Firing

void(entity p, float xChannel, string xSample, float xVolume, float xPitch) VR_RV_Sound =
{
    float v = xVolume * cvar_hget(cvarh_vr_reload_volume);
    if(v > 0)
    {
        sound_pitch(p, xChannel, xSample, min(1, v), ATTN_NORM, xPitch + crandom() * 2);
    }
};

// A slug of self's about to be fired (FireBulletsImpl's one pellet): its damage (vr_dmg_revolver and up to
// vr_revolver_dmg_random more, a player's; a monster's 15), into a big body more (a player's: vr_revolver_heavy_*), its
// head pops (vr_decap.qc: QVR_DECAP_REVOLVER). VR_Revolver_SlugEnd after.
void() VR_Revolver_SlugBegin =
{
    float base = VR_WpnDmg(self, cvarh_vr_dmg_revolver, 15);
    vr_pellet_damage = base * (1 + random() * max(0, min(1, cvar_hget(cvarh_vr_revolver_dmg_random))));
    if(self.flags & FL_CLIENT)
    {
        vr_pellet_heavy_mult = max(0, cvar_hget(cvarh_vr_revolver_heavy_mult));
        vr_pellet_heavy_health = cvar_hget(cvarh_vr_revolver_heavy_health);
        vr_decap_weapon = QVR_DECAP_REVOLVER; // (head pops: vr_decap.qc)
    }
    vr_pellet_last = 0;
    vr_pellet_last_ent = world;
};

void() VR_Revolver_SlugEnd =
{
    vr_decap_weapon = 0;
    vr_pellet_damage = 0;
    vr_pellet_heavy_mult = 0;
    vr_pellet_heavy_health = 0;
};

// A slug of self's (a player's) from `xFrom` along `xDir`, `xRange` units at most, as the revolver fires one (the
// tests' shots at a monster's head or body, from close by).
void(vector xFrom, vector xDir, float xRange) VR_Revolver_SlugFrom =
{
    VR_DamageCredit_FromHand(self, self, cVR_MainHand);
    VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', xDir);
    vr_tracer_hand = cVR_MainHand;
    vr_tracer_from = xFrom;
    VR_Revolver_SlugBegin();
    FireBulletsImpl(1, xDir, '0 0 0', xFrom, xRange, FALSE);
    vr_rv_lastdmg = vr_pellet_last;
    vr_rv_lastpart = vr_pellet_last_part;
    vr_rv_lasthit = vr_pellet_last_ent;
    VR_Revolver_SlugEnd();
    VR_ClearHitKind();
};

// Its shot from `xHand` of self (a player, or an armed grunt: vr_armedgrunt.qc): a slug of its shells (its clip; without
// reloading, your shells), a player's from the chamber under the hammer (spent then); its blast, kick, flash and smoke.
void(float xHand) W_FireRevolver =
{
    if(getCurrentAmmoOrClip(xHand) < 1)
    {
        return;
    }
    float client = (self.flags & FL_CLIENT) != 0;
    float vol = max(0, min(4, cvar_hget(cvarh_vr_revolver_volume)));
    // Its blast: the super shotgun's, deeper (a big bore from a short barrel), and the action's clack under it.
    sound_pitch(self, VRGetGunChannel(xHand), "weapons/shotgn2.wav", vol, ATTN_NORM, 86 + crandom() * 3);
    sound_pitch(self, CHAN_AUTO, "weapons/guncock.wav", vol * 0.6, ATTN_NORM, 72 + crandom() * 3);
    delAmmoOrClipAndUpdateCounter(xHand, AID_SHELLS, 1);
    entity rec = VRGetEntWeaponInst(self, xHand);
    if(client && rec)
    {
        VR_Revolver_SetChamber(rec, rec.wi_rv_at, 2);
    }

    VR_Revolver_SlugBegin();
    float k = max(0, cvar_hget(cvarh_vr_revolver_spread)) * 0.0174533; // (radians: small, its tangent)
    FireBullets(xHand, 1, VRGetWeaponFireRotFwd(xHand), '1 1 0' * k);
    VR_Revolver_SlugEnd();

    self.effects = self.effects | EF_MUZZLEFLASH;
    self.punchangle_x = -4;
    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp; slot 38's Recoil, Flash)
    VRGunHaptic(xHand, 0.16, 50, 1);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 2);

    if(client)
    {
        entity h = VR_Revolver_Hand(self, xHand);
        h.rvh_smoke = time + 0.25;
        vr_rv_shots++;
        vr_rv_lastdmg = vr_pellet_last;
        vr_rv_lastpart = vr_pellet_last_part;
        vr_rv_lasthit = vr_pellet_last_ent;
    }
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("revolver: hand %g fired from %v (%g left, chambers %s): %.1f on %s, part %g\n", xHand,
            VRGetMuzzlePos(xHand), getCurrentAmmoOrClip(xHand), rec ? VR_Revolver_Desc(rec) : "?", vr_pellet_last,
            vr_pellet_last_ent ? vr_pellet_last_ent.classname : "nothing", vr_pellet_last_part));
    }
};

// The cocked hammer in `xHand` of self falls on the chamber under it: a live shell fires, else a click.
void(float xHand) VR_Revolver_Fall =
{
    entity h = VR_Revolver_Hand(self, xHand);
    entity rec = VRGetEntWeaponInst(self, xHand);
    h.rvh_fire_at = 0;
    h.rvh_shot_at = time;
    if(rec && VR_Revolver_Chamber(rec, rec.wi_rv_at) == 1 && getCurrentAmmoOrClip(xHand) >= 1)
    {
        self.show_hostile = time + 1;
        VR_Stealth_Shot(self, WID_REVOLVER); // QVR: heard by its loudness (vr_stealth.qc)
        RuneApplyBlackNoise(self);
        SuperDamageSound();
        W_FireRevolver(xHand);
        return;
    }
    // (An empty chamber, or a spent shell: the hammer's click, the cylinder turned all the same.)
    sound(self, VRGetHandChannel(xHand), "gunclick.wav", 1, ATTN_NORM);
    VRGunHaptic(xHand, 0.03, 180, 0.4);
    vr_rv_dry++;
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("revolver: click, hand %g (chamber %g: %s)\n", xHand, rec ? rec.wi_rv_at : -1,
            rec ? VR_Revolver_Desc(rec) : "?"));
    }
};

// W_AttackImpl's: the trigger pulled (time past its cooldown). Double action, a shot a pull: a new pull cocks the hammer
// and turns the cylinder a chamber, the hammer falls vr_revolver_cock_time later (VR_Revolver_HandFrame); then nothing
// until the trigger is let go. Swung out, it doesn't fire.
void(float xHand, float xAttackFinished, float* xOutAttackFinished) VR_Revolver_Attack =
{
    entity h = VR_Revolver_Hand(self, xHand);
    if(h.rvh_rearm)
    {
        return;
    }
    h.rvh_rearm = TRUE;
    entity rec = VRGetEntWeaponInst(self, xHand);
    if(!rec || h.rvh_fire_at > 0)
    {
        return;
    }
    if(rec.wi_rv_open)
    {
        VR_Reload_Log("the revolver's cylinder is out: it doesn't fire");
        return;
    }
    float cool = max(0.05, cvar_hget(cvarh_vr_revolver_cooldown));
    *xOutAttackFinished = time + RuneApplyHell(cool, self);
    VR_Revolver_Advance(rec);
    VRSetWeaponFrame(xHand, 1); // (the hammer cocked)
    float cock = max(0, cvar_hget(cvarh_vr_revolver_cock_time));
    h.rvh_fire_at = time + cock;
    if(cvar_hget(cvarh_vr_debug_shots) >= 2)
    {
        dprint(sprintf("revolver: the trigger, hand %g: cocked, chamber %g under the hammer (%s)\n", xHand, rec.wi_rv_at,
            VR_Revolver_Desc(rec)));
    }
    if(cock <= 0)
    {
        VR_Revolver_Fall(xHand);
    }
};

// ----------------------------------------------------------------------------
// Loading by hand (immersive reloading)

// Its cylinder in `xHand` of self swings out (`xHow`: what did it, for the log).
void(float xHand, entity r, string xHow) VR_Revolver_Open =
{
    entity h = VR_Revolver_Hand(self, xHand);
    r.wi_rv_open = 1;
    r.wi_rv_spin = 0;
    r.wi_rv_ofs = 0;
    h.rvh_toggled = time;
    h.rvh_tilt = 0;
    VR_RV_Sound(self, VRGetGunChannel(xHand), "vr/reload_ssg_open.wav", 0.9, 135);
    VR_Reload_Haptic(self, xHand, 0.06, 90, 0.8);
    vr_rv_opened++;
    vr_rv_lasthow = xHow;
    VR_Reload_Log(sprintf("the revolver (hand %g) swung out by %s: %s", xHand, xHow, VR_Revolver_Desc(r)));
};

// And home: it fires again (what its chambers hold).
void(float xHand, entity r, string xHow) VR_Revolver_Close =
{
    entity h = VR_Revolver_Hand(self, xHand);
    r.wi_rv_open = 0;
    r.wi_rv_spin = 0;
    r.wi_rv_ofs = 0;
    h.rvh_toggled = time;
    VR_RV_Sound(self, VRGetGunChannel(xHand), "vr/reload_ssg_close.wav", 1, 125);
    VR_Reload_Haptic(self, xHand, 0.08, 60, 1);
    vr_rv_closed++;
    vr_rv_lasthow = xHow;
    VR_Reload_Log(sprintf("the revolver (hand %g) swung home by %s: %s, %g live", xHand, xHow, VR_Revolver_Desc(r),
        VR_Revolver_Count(r, 1)));
};

// Its shells drop out of the swung-out cylinder in `xHand` of self, spent and live: the spent ones as casings (the
// clients', vr_shells.cpp, out of the cylinder as drawn), the live ones as shells lying about to take again.
void(float xHand, entity r, string xHow) VR_Revolver_Eject =
{
    entity h = VR_Revolver_Hand(self, xHand);
    float spent = VR_Revolver_Count(r, 2);
    float live = VR_Revolver_Count(r, 1);
    r.wi_rv = 0;
    VRSetEntWeaponClip(self, xHand, 0);
    VR_Reload_AmmoChanged(self);
    h.rvh_rod = time + 0.35;
    VR_RV_Sound(self, CHAN_AUTO, "vr/reload_ssg_eject.wav", 0.9, 115);
    VR_Reload_Haptic(self, xHand, 0.05, 140, 0.6);
    if(spent > 0)
    {
        ejectcasings(xHand, QVR_CASING_SLUG, spent, 0.02, 0); // (blue, as the chambers' spent slugs)
    }
    VR_Revolver_Frame(xHand);
    vector at = vr_rv_port;
    if(vlen(at - VRGetEntHandPos(self, xHand)) > 64)
    {
        at = VRGetEntHandPos(self, xHand); // (no load point reported: a bot's, a test's)
    }
    for(float i = 0; i < live; i++)
    {
        float a = (90 + 60 * i) * 0.0174533;
        vector from = at + (vr_rv_left * cos(a) + vr_rv_up * sin(a)) * 0.9 - vr_rv_axis * 1.5;
        entity s = VR_Reload_MakeRound(0, AID_SHELLS, 1, from, vectoangles(vr_rv_axis));
        s.skin = 1; // (the blue slug, as in its chambers: vr_shell_live.mdl's skin 1)
        VR_Reload_MarkFresh(s, self); // (not back in by contact until it has left the cylinder)
        s.velocity = VRGetEntHandVel(self, xHand) * VRMetersToUnits() - vr_rv_axis * 40 + '0 0 1' * (20 + random() * 20) +
                     (vr_rv_left * crandom() + vr_rv_up * crandom()) * 15;
        s.avelocity = '0 1 0' * (crandom() * 300) + '1 0 0' * (crandom() * 300);
        s.flags = s.flags - (s.flags & FL_ONGROUND);
    }
    vr_rv_ej_spent += spent;
    vr_rv_ej_live += live;
    vr_rv_lasthow = xHow;
    VR_Reload_Log(sprintf("the revolver (hand %g): its shells dropped by %s: %g spent, %g live", xHand, xHow, spent, live));
};

// A round `xRound` goes into the revolver in `xGunHand` of self (vr_reload.qc VR_Reload_LoadInto; from the other hand,
// `xHeld`, else a loose one): a shell into the empty chamber nearest it, if the cylinder is out. TRUE if it is used up.
float(float xGunHand, entity xRound, float xHeld) VR_Revolver_LoadInto =
{
    float xHand = VRGetOtherHand(xGunHand);
    entity g = VRGetEntWeaponInst(self, xGunHand);
    if(!g)
    {
        return FALSE;
    }
    float i = g.wi_rv_open ? VR_Revolver_ChamberFor(xGunHand, g, VR_Reload_RoundRef(xRound)) : -1;
    if(i < 0)
    {
        if(!xRound.vr_ammo_full)
        {
            // (Once as it comes within the load point's range, until it leaves it: shut, or every chamber full.)
            xRound.vr_ammo_full = TRUE;
            VR_Reload_Haptic(self, xHeld ? xHand : xGunHand, 0.06, 40, 0.6);
            VR_Reload_Sound(self, VRGetGunChannel(xGunHand), "vr/reload_blocked.wav", 1);
            VR_Reload_Log(g.wi_rv_open ? "the revolver's chambers are full" : "the revolver is shut: swing its cylinder out first");
        }
        return FALSE;
    }
    VR_Revolver_SetChamber(g, i, 1);
    entity rh = VR_Revolver_Hand(self, xGunHand);
    if(rh) { rh.rvh_held_at = time; } // (no spin from the swipe away after it: vr_revolver_spin_hold)
    float clip = VRGetEntWeaponClip(self, xGunHand);
    VRSetEntWeaponClip(self, xGunHand, clip + 1);
    xRound.vr_ammo_count = xRound.vr_ammo_count - 1;
    VR_Reload_AmmoChanged(self);
    VR_Reload_ShellsIn(self, VRGetGunChannel(xGunHand), 1);
    if(xHeld)
    {
        VR_Reload_Haptic(self, xHand, 0.05, 160, 0.8); // the shell pushed home
    }
    VR_Reload_Haptic(self, xGunHand, 0.06, 80, 0.6);
    vr_rv_loaded++;
    vr_rv_lastch = i;
    VR_Reload_Log(sprintf("a shell into the revolver's chamber %g (hand %g%s): %s, %g in the reserve", i, xGunHand,
        xHeld ? "" : ", a loose one by contact", VR_Revolver_Desc(g), VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_SHELLS)));
    if(VR_Revolver_Count(g, 0) == 0)
    {
        VR_Reload_Sound(self, CHAN_AUTO, "vr/reload_full.wav", 0.8);
        VR_Reload_Log("the revolver's chambers are full now");
    }
    if(xRound.vr_ammo_count <= 0)
    {
        collectfx(xHand, xRound, QVR_CFX_INTO_GUN, xRound.modelindex, xRound.origin, xRound.angles);
        if(xHeld)
        {
            VR_Carry_SetHeld(self, xHand, world);
            xRound.carry_player = world;
        }
        remove(xRound);
        return TRUE;
    }
    VR_Reload_SetModel(xRound); // (a pair's other shell, still in the fingers)
    collectfx(xHand, world, QVR_CFX_INTO_GUN, xRound.modelindex, xRound.origin, xRound.angles);
    return FALSE;
};

// The wrist's roll for the flicks (`xRate` degrees a second towards the cylinder's side, over `xDt` s; VR_Revolver_Flick):
// +1 a flick that way (it swings out: vr_revolver_open_flick_speed at its fastest, vr_revolver_open_flick_angle in one
// go), -1 the other way (home: vr_revolver_close_flick_*), else 0. A roll under vr_revolver_flick_sustain (0.3) of the speed ends
// a flick; its angle must come within vr_revolver_*_flick_time seconds of its start (older: it restarts).
float(entity h, float xRate, float xDt) VR_Revolver_FlickRate =
{
    float needO = max(1, cvar_hget(cvarh_vr_revolver_open_flick_speed));
    float needC = max(1, cvar_hget(cvarh_vr_revolver_close_flick_speed));
    float sus = min(1, max(0, cvar_hget(cvarh_vr_revolver_flick_sustain)));
    float winO = cvar_hget(cvarh_vr_revolver_open_flick_time);
    float winC = cvar_hget(cvarh_vr_revolver_close_flick_time);
    if(xRate > needO * sus)
    {
        h.rvh_fot = h.rvh_fot + xDt;
        if(winO > 0 && h.rvh_fot > winO)
        {
            h.rvh_fo = h.rvh_fop = 0;
            h.rvh_fot = xDt;
        }
        h.rvh_fo = h.rvh_fo + xRate * xDt;
        h.rvh_fop = max(h.rvh_fop, xRate);
    }
    else
    {
        h.rvh_fo = h.rvh_fop = h.rvh_fot = 0;
    }
    if(-xRate > needC * sus)
    {
        h.rvh_fct = h.rvh_fct + xDt;
        if(winC > 0 && h.rvh_fct > winC)
        {
            h.rvh_fc = h.rvh_fcp = 0;
            h.rvh_fct = xDt;
        }
        h.rvh_fc = h.rvh_fc - xRate * xDt;
        h.rvh_fcp = max(h.rvh_fcp, -xRate);
    }
    else
    {
        h.rvh_fc = h.rvh_fcp = h.rvh_fct = 0;
    }
    if(h.rvh_fo >= max(0, cvar_hget(cvarh_vr_revolver_open_flick_angle)) && h.rvh_fop >= needO)
    {
        vr_rv_lasthow = sprintf("a flick (%.0f deg at %.0f deg/s)", h.rvh_fo, h.rvh_fop);
        h.rvh_fo = h.rvh_fop = h.rvh_fot = 0;
        return 1;
    }
    if(h.rvh_fc >= max(0, cvar_hget(cvarh_vr_revolver_close_flick_angle)) && h.rvh_fcp >= needC)
    {
        vr_rv_lasthow = sprintf("a flick (%.0f deg at %.0f deg/s)", h.rvh_fc, h.rvh_fcp);
        h.rvh_fc = h.rvh_fcp = h.rvh_fct = 0;
        return -1;
    }
    return 0;
};

// The hand `xHand` of self's wrist roll about its aim now (degrees a second, towards the cylinder's side: its left in the
// right hand, its right in the left: the gun mirrored) and the flick it makes (VR_Revolver_FlickRate).
float vr_rv_rollrate;
float vr_rv_rollmax; // (the fastest roll seen, signed: the tests reset it)
float(entity h, float xHand) VR_Revolver_Flick =
{
    // (From the hand's turn since the last frame: how far its up swung about its aim towards its left, or its right in the
    // off hand: the cylinder's side. The hand as tracked, as its gun is drawn.)
    makevectors(VRGetEntHandRot(self, xHand));
    vector up = v_up;
    vector side = xHand == cVR_OffHand ? v_right : -v_right;
    float roll = 0;
    if(h.rvh_upok && frametime > 0)
    {
        roll = atan2(up * h.rvh_side, up * h.rvh_up) * 57.29578 / frametime;
    }
    h.rvh_up = up;
    h.rvh_side = side;
    h.rvh_upok = TRUE;
    vr_rv_rollrate = roll;
    if(fabs(vr_rv_rollrate) > fabs(vr_rv_rollmax))
    {
        vr_rv_rollmax = vr_rv_rollrate;
    }
    if(cvar_hget(cvarh_vr_reload_debug) >= 2 && fabs(vr_rv_rollrate) > 60)
    {
        dprint(sprintf("revolver: hand %g's wrist rolls %.0f deg/s towards the cylinder's side\n", xHand, vr_rv_rollrate));
    }
    return VR_Revolver_FlickRate(h, vr_rv_rollrate, frametime);
};

// The other hand of `xHand` of self at the swung-out cylinder of record `r`: pushing the ejector rod's head back drops its
// shells; pushing the cylinder up from below swings it home; swiping across it spins it. Empty hands only.
void(float xHand, entity h, entity r) VR_Revolver_OtherHand =
{
    float other = VRGetOtherHand(xHand);
    if(!VRIsHandEmpty(self, other))
    {
        h.rvh_held_at = time; // (a shell in it: its swipe away after loading spins nothing, vr_revolver_spin_hold)
        return;
    }
    if(!VR_Revolver_Frame(xHand))
    {
        return;
    }
    vector hp = VRGetEntHandPos(self, other);
    vector rel = VRGetEntHandVel(self, other) - VRGetEntHandVel(self, xHand); // (m/s)
    // The ejector rod's head, in front of the cylinder on its axis.
    vector tip = vr_rv_port + vr_rv_axis * (RV_ROD_TIP * RV_SCALE);
    if(cvar_hget(cvarh_vr_revolver_eject_rod) && VR_Revolver_Count(r, 0) < RV_CHAMBERS &&
       vlen(hp - tip) <= cvar_hget(cvarh_vr_revolver_eject_rod_reach) &&
       rel * -vr_rv_axis >= cvar_hget(cvarh_vr_revolver_eject_rod_speed))
    {
        VR_Revolver_Eject(xHand, r, sprintf("the ejector rod pushed (%.2f m/s)", rel * -vr_rv_axis));
        return;
    }
    float half = RV_CYL_LEN * 0.5 * RV_SCALE;
    vector mid = vr_rv_port + vr_rv_axis * half;
    vector d = hp - mid;
    float along = d * vr_rv_axis;
    vector radial = d - vr_rv_axis * along;
    float rr = vlen(radial);
    float radius = RV_CYL_RADIUS * RV_SCALE;
    if(fabs(along) > half + 1 || rr > radius + max(0, cvar_hget(cvarh_vr_revolver_hand_reach)))
    {
        return;
    }
    // Home: pushed along the crane's way home (its arc's tangent there: the model's (y, z) -0.137, 0.99), from that side.
    vector home = normalize(vr_rv_left * -0.137 + vr_rv_up * 0.99);
    if(cvar_hget(cvarh_vr_revolver_close_push) && rel * home >= cvar_hget(cvarh_vr_revolver_close_push_speed) && d * home < 0)
    {
        VR_Revolver_Close(xHand, r, sprintf("a push from below (%.2f m/s)", rel * home));
        return;
    }
    if(!cvar_hget(cvarh_vr_revolver_spin) || rr < 0.1 || time < h.rvh_spun + 0.2)
    {
        return;
    }
    vector t = VR_RV_Cross(vr_rv_axis, radial * (1 / rr)); // (a turn's way there, right-handed about the axis in the world)
    float vt = rel * t;
    if(fabs(vt) < cvar_hget(cvarh_vr_revolver_spin_speed))
    {
        return;
    }
    // Not while loading (ROUND22.md, "Revolver spins by accident while loading"): the other hand held a shell (or
    // anything) or a shell went in a moment ago.
    if(time < h.rvh_held_at + max(0, cvar_hget(cvarh_vr_revolver_spin_hold)))
    {
        if(time > vr_rv_spinrefused + 0.5)
        {
            VR_Reload_Log(sprintf("the revolver's cylinder: a swipe (%.2f m/s) right after loading, not spun", vt));
        }
        vr_rv_spinrefused = time;
        vr_rv_refusals++;
        return;
    }
    h.rvh_spun = time;
    float sense = xHand == cVR_OffHand ? -1 : 1; // (the model's own sense: mirrored in the off hand)
    float w = vt * VRMetersToUnits() / radius * 57.29578 * max(0, cvar_hget(cvarh_vr_revolver_spin_mult)) * sense;
    float most = max(60, cvar_hget(cvarh_vr_revolver_spin_max));
    r.wi_rv_spin = max(-most, min(most, r.wi_rv_spin + w));
    VR_RV_Sound(self, CHAN_AUTO, "vr/gadget_click_on.wav", 0.5, 70);
    VR_Reload_Haptic(self, other, 0.04, 200, 0.4);
    VR_Reload_Log(sprintf("the revolver's cylinder spun (a swipe at %.2f m/s): %.0f deg/s", vt, r.wi_rv_spin));
};

// Swung out and spun: it turns, slows (vr_revolver_spin_friction) and stops on a chamber, which comes under the hammer.
void(entity r) VR_Revolver_SpinStep =
{
    if(!r.wi_rv_open)
    {
        r.wi_rv_spin = 0;
        r.wi_rv_ofs = 0;
        return;
    }
    if(r.wi_rv_spin == 0 && r.wi_rv_ofs == 0)
    {
        return;
    }
    float w = r.wi_rv_spin;
    float slow = max(0, cvar_hget(cvarh_vr_revolver_spin_friction)) * frametime;
    w = w > 0 ? max(0, w - slow) : min(0, w + slow);
    if(fabs(w) < 90)
    {
        // (Slow: it settles on the nearest chamber.)
        w = 0;
        float step = 240 * frametime;
        if(fabs(r.wi_rv_ofs) <= step)
        {
            r.wi_rv_ofs = 0;
            sound(self, CHAN_AUTO, "gunclick.wav", 0.4, ATTN_NORM);
        }
        else
        {
            r.wi_rv_ofs = r.wi_rv_ofs - step * (r.wi_rv_ofs > 0 ? 1 : -1);
        }
    }
    r.wi_rv_ofs = r.wi_rv_ofs + w * frametime;
    while(r.wi_rv_ofs > 30)
    {
        r.wi_rv_ofs = r.wi_rv_ofs - 60;
        r.wi_rv_at = VR_RV_Wrap(r.wi_rv_at - 1);
    }
    while(r.wi_rv_ofs < -30)
    {
        r.wi_rv_ofs = r.wi_rv_ofs + 60;
        r.wi_rv_at = VR_RV_Wrap(r.wi_rv_at + 1);
    }
    r.wi_rv_spin = w;
};

// Every player frame, for the revolver of record `r` in `xHand` of self loading by hand (VR_Revolver_HandFrame).
void(float xHand, entity h, entity r) VR_Revolver_ReloadFrame =
{
    float sec = VRHasBit(self.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_SECONDARY : QVR_VRBITS0_OFFHAND_SECONDARY);
    float secEdge = sec && !h.rvh_sec;
    h.rvh_sec = sec;
    float flick = VR_Revolver_Flick(h, xHand);
    if(VRGet2HAiming(self))
    {
        flick = 0; // (both hands on it: the wrist's roll is the aim's)
    }
    if(time < h.rvh_toggled + 0.3 || h.rvh_fire_at > 0)
    {
        return; // (just swung out or home; firing)
    }
    if(!r.wi_rv_open)
    {
        if(secEdge && cvar_hget(cvarh_vr_revolver_open_button))
        {
            VR_Revolver_Open(xHand, r, "the eject button");
        }
        else if(flick > 0 && cvar_hget(cvarh_vr_revolver_open_flick))
        {
            VR_Revolver_Open(xHand, r, vr_rv_lasthow);
        }
        return;
    }
    if(secEdge)
    {
        if(cvar_hget(cvarh_vr_revolver_close_button))
        {
            VR_Revolver_Close(xHand, r, "the button");
        }
        else
        {
            VR_Revolver_Eject(xHand, r, "the eject button");
        }
        return;
    }
    if(flick < 0)
    {
        VR_Revolver_Close(xHand, r, vr_rv_lasthow);
        return;
    }
    // The muzzle tilted up: its shells drop (once; again once it is lowered).
    float tilt = cvar_hget(cvarh_vr_revolver_eject_tilt);
    makevectors(VRGetEntHandRot(self, xHand));
    if(tilt > 0 && v_forward_z >= sin(min(89, tilt) * 0.0174533))
    {
        if(!h.rvh_tilt)
        {
            h.rvh_tilt = 1;
            if(VR_Revolver_Count(r, 0) < RV_CHAMBERS)
            {
                VR_Revolver_Eject(xHand, r, sprintf("the muzzle tilted up (%.0f deg)", atan2(v_forward_z, sqrt(max(0, 1 - v_forward_z * v_forward_z))) * 57.29578));
            }
        }
    }
    else if(v_forward_z < sin(max(0, tilt - 10) * 0.0174533))
    {
        h.rvh_tilt = 0;
    }
    VR_Revolver_OtherHand(xHand, h, r);
};

// Every player frame, for `xHand` of self: a revolver's trigger re-armed, its chambers following its clip, its cocked
// hammer falling, its loading by hand, its spin, its hammer's frame, its smoke, what the engine draws.
void(float xHand) VR_Revolver_HandFrame =
{
    float w = VRGetWeapon(xHand);
    entity h = xHand == cVR_OffHand ? self.vr_rv_h2 : self.vr_rv_h;
    entity r = w == WID_REVOLVER ? VRGetEntWeaponInst(self, xHand) : world;
    if(h && h.rvh_inst && h.rvh_inst != r && h.rvh_inst.wi_rv_open)
    {
        // (Gone from the hand swung out, to a holster, the floor, a grunt: it swings home, drawn shut there.)
        h.rvh_inst.wi_rv_open = 0;
        h.rvh_inst.wi_rv_spin = 0;
        h.rvh_inst.wi_rv_ofs = 0;
    }
    if(h)
    {
        h.rvh_inst = r;
    }
    if(w != WID_REVOLVER || !r)
    {
        if(h)
        {
            h.rvh_rearm = FALSE;
            h.rvh_fire_at = 0;
            h.rvh_smoke = 0;
        }
        if(xHand == cVR_OffHand) { self.vr_rv_hud2 = 0; } else { self.vr_rv_hud = 0; }
        return;
    }
    h = VR_Revolver_Hand(self, xHand);
    h.rvh_inst = r;
    if(!VRGetFireButtonPressed(xHand))
    {
        h.rvh_rearm = FALSE;
    }
    float reloads = VRIsWeaponReloadingEnabledFor(self, xHand);
    VR_Revolver_Sync(r, reloads ? VRGetEntWeaponClip(self, xHand) : getCurrentAmmoCount(xHand));
    if(h.rvh_fire_at > 0 && time >= h.rvh_fire_at)
    {
        VR_Revolver_Fall(xHand);
    }
    if(self.ishuman && self.health > 0 && VR_Reload_ManualInHand(self, xHand))
    {
        VR_Revolver_ReloadFrame(xHand, h, r);
    }
    else if(r.wi_rv_open)
    {
        r.wi_rv_open = 0; // (no longer loaded by hand: home)
    }
    VR_Revolver_SpinStep(r);
    // The hammer and the trigger: cocked (1) until it falls, then the trigger held back (2), let go (0).
    float fr = 0;
    if(h.rvh_fire_at > 0)
    {
        fr = 1;
    }
    else if(VRGetFireButtonPressed(xHand) && h.rvh_shot_at > 0 && time >= h.rvh_shot_at)
    {
        fr = 2;
    }
    VRSetWeaponFrame(xHand, fr);
    if(h.rvh_smoke > 0 && time >= h.rvh_smoke)
    {
        if(cvar_hget(cvarh_vr_revolver_smoke) > 0)
        {
            particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_SMOKE, rint(3 * cvar_hget(cvarh_vr_revolver_smoke)));
        }
        h.rvh_smoke = 0;
    }
    float hud = r.wi_rv + (r.wi_rv_open ? 4096 : 0) + (time < h.rvh_rod ? 8192 : 0) + 16384 * r.wi_rv_at + 131072;
    if(xHand == cVR_OffHand)
    {
        self.vr_rv_hud2 = hud;
        self.vr_rv_cyl2 = VR_Revolver_CylAngle(r);
    }
    else
    {
        self.vr_rv_hud = hud;
        self.vr_rv_cyl = VR_Revolver_CylAngle(r);
    }
};

// Every player frame (client.qc PlayerPreThink, self the player).
void() VR_Revolver_PlayerFrame =
{
    VR_Revolver_HandFrame(cVR_MainHand);
    VR_Revolver_HandFrame(cVR_OffHand);
};

/*QUAKED weapon_revolver (0 .5 .8) (-16 -16 0) (16 16 32)
Quake VR's heavy revolver: one-handed, double action, six chambers of your shells fired as slugs. Picked up as the other
weapons are (with 12 shells); its own model lies there (no classic pickup model).
*/
void() weapon_revolver =
{
    impl_weapon_item("progs/v_revolver.mdl", WID_REVOLVER);
};
