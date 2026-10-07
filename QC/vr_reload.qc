//
//
//
// ----------------------------------------------------------------------------
// Immersive reloading (vr_reload_mode 3; docs/vr-port/RELOAD_PLAN.md)
// ----------------------------------------------------------------------------
//
// The guns that load by hand (VR_Reload_ManualFor: the shotgun so far) take their rounds from the ammo pouch on the front
// of the belt: a hand gripping at it (the engine's hotspot QVR_HS_AMMO_POUCH) takes a round for the gun in the OTHER hand
// (a shell; with vr_reload_shell_pairs, two taped together), its ammo leaving the reserve at once; held within
// the gun's load point's radius of its loading port (the engine's .loadportpos, .offloadportpos: as drawn) it goes into
// the gun's magazine (its record: it stays with the gun). Let go of at the pouch (or at a holster, or the trigger pressed,
// as any carried pickup) it goes back into the reserve; let go of anywhere else it drops, a physics prop that can be taken
// again by hand or force grab, until the cap of rounds lying about (vr_reload_loose_max) or its time
// (vr_reload_loose_time) takes it: then it is lost. One in a hand at a level's end is refunded.
// Everything is the server's (each player's own pouch and rounds); the client only reports where the pouch and the port
// are. Holster, button and flick reloads skip the hand-loaded guns in this mode (VRTryReloadWeapon); the others keep the
// hip holsters' reload.
//
// The super shotgun (phase 2b, vr_reload_ssg_break): its fired shells stay in its barrels (QVR_WPNFLAG_SSG_SPENT); broken
// open (by the flick, or by both hands prying it: the front hand pushing the barrels down against the back hand, which
// the engine sends as the flick: vr_flick.cpp), it
// throws them out, and any live ones as rounds to take again, and stays open (QVR_WPNFLAG_SSG_OPEN; drawn open, it
// can't fire: a dry click). Open, its chambers take the pouch's taped pair (always a pair for it) at its load point (the
// breech face, as drawn open); a pair with one chamber free loads one and keeps the other in the hand (as the shotgun's).
// It closes by the flick again (vr_reload_ssg_close_flick), the front hand lifting the barrels back (the reverse pry:
// vr_reload_ssg_close_pry), or by itself once both chambers are loaded (vr_reload_ssg_close_auto).
//
// Magazines (phase 2: the nailgun, the super nailgun, the thunderbolt; VR_Reload_UsesMag): the gun's magazine is its
// record's (.wi_clip: what the attached magazine holds); none attached is the weapon flag QVR_WPNFLAG_NOMAG (it travels with
// the gun as its flags do; the clients draw the attached one: vr_view.cpp setupMagazines). The pouch gives a magazine for
// the other hand's gun holding min(its size, the reserve); one whose top meets the gun's well (the two radii)
// seats if the gun has none; if it has one, only a bump seats it (the hands meeting at vr_reload_bump_speed: the old one
// knocked out, flying). A seated one comes out by the gun's controller's B/Y (vr_reload_eject_button), by a hit anywhere on
// it, or by the other hand gripping it (the gun's second two-handed grip, beside its own) and pulling it off hard, snapping
// the wrist or moving the hands apart (vr_reload_pull_*: a gentle pull only holds it, as a foregrip). The engine sends the
// attached magazine's shape as drawn (.magbox*: a box, held or carried), which the grips and hits test. However the gun
// is held (by its handle in either hand, or carried anywhere on it), its magazine works the same. Out, it keeps its count: a round as the shells are (taken again, force grabbed, refunded at
// the pouch, lost to the caps).

.float vr_ammo_aid;      // a round: the ammo it is (AID_*)
.float vr_ammo_count;    // and how many (a taped pair: 2)
.float vr_ammo_wid;      // a magazine: the gun it fits (WID_*; 0 a shell)
.float vr_ammo_fresh;    // a magazine just pulled out of its gun (or dropped, knocked out; the super shotgun's live shells
                         // thrown out): it goes in again only once it has left the well
.float vr_ammo_pass;     // a loose round passing through the guns of a player to their load points (VR_Reload_LooseFrame;
                         // the engine's vr_box3d.cpp reachSkips): the hands (1 off, 2 main) of
.entity vr_ammo_passer;  // that player
.float vr_ammo_passing;  // a player's hands whose guns some loose round passes through (1 off, 2 main)
.float vr_mag_pull;      // a player's hands holding the attached magazine of the other hand's gun (1 off, 2 main)
.float vr_reload_sec;    // and its hands' B/Y last frame (1 off, 2 main)
.float vr_mag_pull_apart0; // how far apart the hands were as the off hand took hold of the magazine (the pull)
.float vr_mag_pull_apart1; // the main hand's
.float vr_mag_seated0;     // when the off hand's gun last had a magazine seated (no hit knocks it out just after)
.float vr_mag_seated1;     // the main hand's gun's
// What a player's pouch gives (VR_Reload_PlayerFrame): its ammo (AID_*; 0 none yet: shells) and, for a magazine, the gun
// it is for (WID_*; 0 shells): the last gun held whose ammo the pouch has (any shotgun: shells; a nailgun, a super nailgun,
// a thunderbolt: its magazine), kept while the hands hold nothing of the kind (empty, a melee weapon). And for the
// clients (STAT_QVR_POUCHKIND, STAT_QVR_POUCHCOUNT): what it shows (1 shells, 2 nailgun magazines, 3 super nailgun
// magazines, 4 cells; 0 none) and how many of that ammo are left.
.float vr_pouch_aid;
.float vr_pouch_wid;
.float vr_pouch_kind;
.float vr_pouch_count;
.float vr_ammo_born;     // when it was last in a hand or flying (its time lying about runs from here)
.float vr_ammo_smoke;    // a spent thunderbolt cell: when it stops smoking (vr_reload_battery_smoke_time; 0 not smoking)
.float vr_ammo_puffs;    // and the wisps it has given off, since (for the log)
.float vr_ammo_smoke_from;
.float vr_ammo_tink;     // its last knock's sound
.float vr_ammo_full;     // the full gun's tap given (until it leaves the port)
.float vr_reload_at;     // a player's hands at the ammo pouch last frame (1 off, 2 main)
.float vr_reload_tried;  // and those whose grip press there was used
.float vr_ssg_flick;     // a player's hands' flick reload bits last frame (1 off, 2 main: the super shotgun's flick)
.float vr_ssg_toggled;   // when a super shotgun of the player's was last broken open or closed
.float vr_ssg_pry;       // and the other hand's (the pry or the lift, sent so by the engine), as each gun hand saw them
.float vr_ssg_sec;       // and its B/Y
.float vr_ssg_held;      // both hands on the hand's super shotgun (the two-handed hold, until the other hand lets go)
.float vr_ssg_loaded;    // when a shell last went into a super shotgun of the player's (its hand drawing back is no hit)

string VR_RELOAD_SHELL = "progs/vr_shell_live.mdl";
string VR_RELOAD_PAIR = "progs/vr_shell_pair.mdl";
string VR_RELOAD_CLASS = "vr_ammo_shell";
string VR_RELOAD_MAGCLASS = "vr_ammo_mag";

void() VR_Reload_Precache =
{
    precache_model(VR_RELOAD_SHELL);
    precache_model(VR_RELOAD_PAIR);
    precache_model("progs/vr_mag_nail.mdl"); // the magazines (make_mags.py)
    precache_model("progs/vr_mag_snail.mdl");
    precache_model("progs/vr_mag_light.mdl");
    precache_sound("vr/reload_mag_in.wav");  // a magazine seated
    precache_sound("vr/reload_mag_out.wav"); // and released
    precache_sound("vr/reload_pouch.wav");    // a round taken from the pouch, or put back
    precache_sound("vr/reload_shell_in.wav");   // a shell pushed into the port: three takes (recorded,
    precache_sound("vr/reload_shell_in_2.wav"); // make_reload_shell_sounds.py), one picked at random per insert
    precache_sound("vr/reload_shell_in_3.wav");
    precache_sound("vr/reload_empty.wav");    // the pouch found empty
    precache_sound("vr/reload_blocked.wav");  // a shell held to a full shotgun: the gate won't open
    precache_sound("vr/reload_full.wav");     // the last shell in: the tube full
    precache_sound("vr/reload_ssg_open.wav");  // the super shotgun broken open (make_reload_ssg_sounds.py),
    precache_sound("vr/reload_ssg_eject.wav"); // its shells thrown out (make_sounds.py),
    precache_sound("vr/reload_ssg_close.wav"); // and snapped shut
    precache_sound("gunclick.wav");            // fired open: the dry click
    precache_sound("vr/shell_tink1.wav");     // a round knocking about (the spent shells' tinks)
    precache_sound("vr/shell_tink2.wav");
    precache_sound("vr/shell_tink3.wav");
};

// ----------------------------------------------------------------------------
// The mode

// Whether immersive reloading is on (and reloading at all: Weapon Mode Immersive).
float() VR_Reload_Immersive =
{
    return cvar_hget(cvarh_vr_reload_mode) == QVR_RELOADMODE_IMMERSIVE && getWeaponMode() == QVR_WEAPONMODE_IMMERSIVE;
};

// Whether `xWeapon` takes magazines (immersive reloading's phase 2).
float(float xWeapon) VR_Reload_UsesMag =
{
    return xWeapon == WID_NAILGUN || xWeapon == WID_SUPER_NAILGUN || xWeapon == WID_LIGHTNING;
};

// Whether `xWeapon` loads by hand (immersive reloading): no holster, button or flick reload for it.
float(float xWeapon) VR_Reload_ManualFor =
{
    return VR_Reload_Immersive() && (xWeapon == WID_SHOTGUN || VR_Reload_UsesMag(xWeapon) ||
        (xWeapon == WID_SUPER_SHOTGUN && cvar_hget(cvarh_vr_reload_ssg_break)));
};

// Whether the gun in `xHand` of `p` is a super shotgun broken open (phase 2b: it can't fire; it loads by hand).
float(entity p, float xHand) VR_Reload_SsgIsOpen =
{
    return VRGetEntWeapon(p, xHand) == WID_SUPER_SHOTGUN && VR_Reload_ManualFor(WID_SUPER_SHOTGUN) &&
           (VRGetEntWeaponFlags(p, xHand) & QVR_WPNFLAG_SSG_OPEN);
};

// How many spent shells the super shotgun with flags `xFlags` has in its barrels.
float(float xFlags) VR_Reload_SsgSpentOf =
{
    return floor((xFlags & QVR_WPNFLAG_SSG_SPENT) / QVR_WPNFLAG_SSG_SPENT_ONE);
};

// The super shotgun in `xHand` of self fired `xCount` barrels (W_FireSuperShotgun, W_FireShotgun): with immersive
// reloading their shells stay in, spent (thrown out as it is broken open).
void(float xHand, float xCount) VR_Reload_SsgFired =
{
    if(!VR_Reload_ManualFor(WID_SUPER_SHOTGUN) || VRGetEntWeapon(self, xHand) != WID_SUPER_SHOTGUN)
    {
        return;
    }
    float f = VRGetEntWeaponFlags(self, xHand);
    float spent = min(2, VR_Reload_SsgSpentOf(f) + xCount);
    VRSetEntWeaponFlags(self, xHand, (f - (f & QVR_WPNFLAG_SSG_SPENT)) | (spent * QVR_WPNFLAG_SSG_SPENT_ONE));
};

// The magazine's model for guns `xWeapon`.
string(float xWeapon) VR_Reload_MagModel =
{
    if(xWeapon == WID_SUPER_NAILGUN)
    {
        return "progs/vr_mag_snail.mdl";
    }
    if(xWeapon == WID_LIGHTNING)
    {
        return "progs/vr_mag_light.mdl";
    }
    return "progs/vr_mag_nail.mdl";
};

// Whether the gun `p` holds in `xHand` (by its handle, or carried anywhere on it: QVR_WPNFLAG_FOREGRIP_CARRIED; the author:
// a gun is a gun however it is held) takes magazines (immersive) and has one in.
float(entity p, float xHand) VR_Reload_MagIn =
{
    float w = VRGetEntWeapon(p, xHand);
    float f = VRGetEntWeaponFlags(p, xHand);
    return p.ishuman && VR_Reload_Immersive() && VR_Reload_UsesMag(w) && !(f & QVR_WPNFLAG_NOMAG);
};

// The ammo whose rounds the pouch gives for the gun `p` holds in `xHand` (by its handle or carried), if it loads by hand;
// else -1.
float(entity p, float xHand) VR_Reload_AmmoFor =
{
    float w = VRGetEntWeapon(p, xHand);
    if(!p.ishuman || !VR_Reload_ManualFor(w))
    {
        return -1;
    }
    return WeaponIdToAmmoId(w, VRGetEntWeaponFlags(p, xHand));
};

// The magazine attached to the gun in `xHand` of `p`, as the client draws it (the engine's .magbox*: a box, its middle and
// half-axes, the first from the middle up to its feed end; held or carried; none from a bot or before the first move).
float(entity p, float xHand) VR_Reload_HasMagBox =
{
    return (xHand == cVR_MainHand ? p.magboxx : p.offmagboxx) != '0 0 0';
};

vector(entity p, float xHand) VR_Reload_MagBoxPos =
{
    return xHand == cVR_MainHand ? p.magboxpos : p.offmagboxpos;
};

// Its first half-axis: from its middle up to its feed end (its length's half).
vector(entity p, float xHand) VR_Reload_MagBoxUp =
{
    return xHand == cVR_MainHand ? p.magboxx : p.offmagboxx;
};

// How far `xPoint` is from that magazine (0 inside it); without its box, from the gun's well (its top).
float(entity p, float xHand, vector xPoint) VR_Reload_MagDist =
{
    if(!VR_Reload_HasMagBox(p, xHand))
    {
        return vlen(xPoint - (xHand == cVR_MainHand ? p.loadportpos : p.offloadportpos));
    }
    vector c = VR_Reload_MagBoxPos(p, xHand);
    vector d = xPoint - c;
    vector nearest = c;
    for(float i = 0; i < 3; i++)
    {
        vector a = i == 0 ? (xHand == cVR_MainHand ? p.magboxx : p.offmagboxx)
                 : i == 1 ? (xHand == cVR_MainHand ? p.magboxy : p.offmagboxy)
                          : (xHand == cVR_MainHand ? p.magboxz : p.offmagboxz);
        float len2 = a * a;
        if(len2 > 0)
        {
            nearest = nearest + a * max(-1, min(1, (d * a) / len2));
        }
    }
    return vlen(xPoint - nearest);
};

// How near the line from `xFrom` to `xTo` comes to that magazine (sampled every unit or so).
float(entity p, float xHand, vector xFrom, vector xTo) VR_Reload_MagLineDist =
{
    float n = max(1, min(24, ceil(vlen(xTo - xFrom))));
    float best = 999999;
    for(float i = 0; i <= n; i++)
    {
        best = min(best, VR_Reload_MagDist(p, xHand, xFrom + (xTo - xFrom) * (i / n)));
    }
    return best;
};

// ----------------------------------------------------------------------------
// Feedback (Weapons > Reloading: Haptics, Sounds)

void(entity p, float xHand, float xDuration, float xFrequency, float xAmplitude) VR_Reload_Haptic =
{
    float k = cvar_hget(cvarh_vr_reload_haptics);
    if(!p.ishuman || k <= 0)
    {
        return;
    }
    entity oldSelf = self;
    self = p;
    haptic(xHand, 0.0, xDuration, xFrequency, min(1, xAmplitude * k));
    self = oldSelf;
};

void(entity e, float xChannel, string xSample, float xVolume) VR_Reload_Sound =
{
    float v = xVolume * cvar_hget(cvarh_vr_reload_volume);
    if(v > 0)
    {
        VR_SoundVaried(e, xChannel, xSample, min(1, v), ATTN_NORM);
    }
};

// The shell pushed into the port (VR_Reload_Load): one of the three takes at random, so a tube filled shell by shell
// does not repeat itself.
string() VR_Reload_ShellInSound =
{
    float k = floor(random() * 3);
    return k < 1 ? "vr/reload_shell_in.wav" : (k < 2 ? "vr/reload_shell_in_2.wav" : "vr/reload_shell_in_3.wav");
};

void(string xWhat) VR_Reload_Log =
{
    if(cvar_hget(cvarh_vr_reload_debug))
    {
        dprint(sprintf("reload: %s\n", xWhat));
    }
};

// The reserve changed: the ammo counters (the hands' screens, the HUD).
void(entity p) VR_Reload_AmmoChanged =
{
    entity oldSelf = self;
    self = p;
    W_SetCurrentAmmo();
    self = oldSelf;
};

// ----------------------------------------------------------------------------
// The rounds

float(entity e) VR_Reload_IsRound =
{
    return e.classname == VR_RELOAD_CLASS || e.classname == VR_RELOAD_MAGCLASS;
};

// Every round lying about or held (shells, then magazines): the next after `xFrom` (world: the first), world at the end.
entity(entity xFrom) VR_Reload_NextRound =
{
    if(xFrom == world || xFrom.classname == VR_RELOAD_CLASS)
    {
        entity e = find(xFrom, classname, VR_RELOAD_CLASS);
        if(e)
        {
            return e;
        }
        return find(world, classname, VR_RELOAD_MAGCLASS);
    }
    return find(xFrom, classname, VR_RELOAD_MAGCLASS);
};

// "a shell", "2 shells", "a magazine of 24": for the log.
string(entity r) VR_Reload_Desc =
{
    if(r.vr_ammo_wid)
    {
        return sprintf("a magazine of %g", r.vr_ammo_count);
    }
    return r.vr_ammo_count >= 2 ? "2 shells" : "a shell";
};

// Whether `xHand` of `p` is at the ammo pouch (immersive reloading).
float(entity p, float xHand) VR_Reload_AtPouch =
{
    return VR_Reload_Immersive() &&
           (xHand == cVR_MainHand ? p.mainhand_hotspot : p.offhand_hotspot) == QVR_HS_AMMO_POUCH;
};

// A round's look: a single shell, the taped pair, or its gun's magazine.
void(entity r) VR_Reload_SetModel =
{
    if(r.vr_ammo_wid)
    {
        setmodel(r, VR_Reload_MagModel(r.vr_ammo_wid));
        float k = propvalue(r, "size"); // (its prop slot's Size, as drawn: the model's own bounds were twice the drawn magazine)
        if(k <= 0)
        {
            k = 1;
        }
        setsize(r, modelbounds(r, 0) * k, modelbounds(r, 1) * k);
        return;
    }
    setmodel(r, r.vr_ammo_count >= 2 ? VR_RELOAD_PAIR : VR_RELOAD_SHELL);
    vector lo = modelbounds(r, 0);
    vector hi = modelbounds(r, 1);
    setsize(r, lo * 1.25, hi * 1.25); // (its prop slot's Size: as the spent shells are drawn)
};

// Back into the reserve of `p` (a hand let go of it at the pouch, or took it as a pickup): TRUE if it fit (and it is
// gone), FALSE if the reserve is full (it stays). A magazine gives its rounds back (an empty one is put away).
float(entity p, entity r, float xHand) VR_Reload_Refund =
{
    if(VR_AmmoUtil_GetAmmoCountByAmmoId(p, r.vr_ammo_aid) + r.vr_ammo_count > VR_MaxAmmo(p, r.vr_ammo_aid))
    {
        VR_Reload_Haptic(p, xHand, 0.12, 30, 0.8);
        VR_Reload_Log("the pouch is full: the round stays out");
        return FALSE;
    }
    VR_AmmoUtil_AddAmmoCountByAmmoId(p, r.vr_ammo_aid, r.vr_ammo_count);
    VR_Reload_AmmoChanged(p);
    if(r.carry_player != world)
    {
        VR_Carry_SetHeld(r.carry_player, xHand, world);
        r.carry_player = world;
    }
    VR_Reload_Sound(p, VRGetHandChannel(xHand), "vr/reload_pouch.wav", 0.6);
    VR_Reload_Haptic(p, xHand, 0.08, 90, 0.6);
    VR_Reload_Log(sprintf("%s back in the pouch (hand %g): %g left", VR_Reload_Desc(r), xHand,
        VR_AmmoUtil_GetAmmoCountByAmmoId(p, r.vr_ammo_aid)));
    remove(r);
    return TRUE;
};

// Its pickup (carry_use: a hand let go of it at a holster, the trigger pressed while holding it, a touch without
// vr_carry): back into the reserve, if it fits. self is the round, other the player.
void() VR_Reload_RoundUse =
{
    if(other.classname != "player" || !other.ishuman)
    {
        return;
    }
    VR_Reload_Refund(other, self, self.handtouch_hand);
};

// Box3D's hardest hit of a frame: the spent shells' tink (a magazine's knocks are the engine's metal), at most every
// 0.12 s.
void(float xSpeed) VR_Reload_Impact =
{
    if(self.vr_ammo_wid || xSpeed < 0.6 || time < self.vr_ammo_tink + 0.12)
    {
        return;
    }
    self.vr_ammo_tink = time;
    float k = floor(random() * 3);
    sound(self, CHAN_AUTO, k < 1 ? "vr/shell_tink1.wav" : (k < 2 ? "vr/shell_tink2.wav" : "vr/shell_tink3.wav"),
        min(1, (0.25 + xSpeed * 0.15) * cvar_hget(cvarh_vr_reload_volume)), ATTN_STATIC);
};

// Gone for good (faded, or made room for another): its ammo lost.
void(entity r, string xWhy) VR_Reload_Lose =
{
    VR_Reload_Log(sprintf("%s lost (%s)", VR_Reload_Desc(r), xWhy));
    remove(r);
};

vector(entity r) VR_Reload_MagRef;

// A thunderbolt cell (`p`'s gun in `xHand`, `r` the cell) making or breaking contact (the author's note
// vrfiringrange_2026-10-07_22-14-04: it is a battery): vr_reload_battery_sparks sparks at the gun's well (its seat), or at
// the cell's top without the client's well.
void(entity p, float xHand, entity r, string xHow) VR_Reload_CellSparks =
{
    float n = cvar_hget(cvarh_vr_reload_battery_sparks);
    if(n <= 0 || r.vr_ammo_wid != WID_LIGHTNING)
    {
        return;
    }
    vector at = xHand == cVR_MainHand ? p.loadportpos : p.offloadportpos;
    if(vlen(at - VRGetEntHandPos(p, xHand)) > 64)
    {
        at = VR_Reload_MagRef(r);
    }
    particle2(at, '0 0 0', QVR_PARTICLE_PRESET_CONTACTSPARKS, n);
    VR_Reload_Log(sprintf("the cell's contact sparks (%s): %g", xHow, n));
};

// A spent thunderbolt cell (taken out empty) smokes for vr_reload_battery_smoke_time (the author's note
// vrfiringrange_2026-10-07_22-14-33), held or lying about: a wisp off its top every tenth of a second, thinning out.
void(entity r) VR_Reload_CellSpent =
{
    float t = cvar_hget(cvarh_vr_reload_battery_smoke_time);
    if(r.vr_ammo_wid != WID_LIGHTNING || r.vr_ammo_count > 0 || t <= 0)
    {
        return;
    }
    r.vr_ammo_smoke = time + t;
    r.vr_ammo_puffs = 0;
    r.vr_ammo_smoke_from = time;
    r.nextthink = time + 0.05;
    VR_Reload_Log(sprintf("a spent cell smoking for %g s", t));
};

void(entity r) VR_Reload_CellSmoke =
{
    if(!r.vr_ammo_smoke)
    {
        return;
    }
    float t = max(0.1, cvar_hget(cvarh_vr_reload_battery_smoke_time));
    if(time >= r.vr_ammo_smoke)
    {
        VR_Reload_Log(sprintf("the spent cell stopped smoking after %.1f s: %g wisps%s", time - r.vr_ammo_smoke_from,
            r.vr_ammo_puffs, r.carry_player != world ? " (held)" : ""));
        r.vr_ammo_smoke = 0;
        return;
    }
    if(random() < 0.35 + 0.65 * (r.vr_ammo_smoke - time) / t)
    {
        particle2(VR_Reload_MagRef(r), '0 0 0', QVR_PARTICLE_PRESET_BATTERYSMOKE, 1);
        r.vr_ammo_puffs = r.vr_ammo_puffs + 1;
    }
};

// Held or flying its time waits; lying about past vr_reload_loose_time it fades away over two seconds. A spent cell
// smokes meanwhile (VR_Reload_CellSmoke: thinks every tenth of a second while it does).
void() VR_Reload_RoundThink =
{
    VR_Reload_CellSmoke(self);
    float life = cvar_hget(cvarh_vr_reload_loose_time);
    if(self.carry_player != world || (!(self.flags & FL_ONGROUND) && vlen(self.velocity) > 1))
    {
        self.vr_ammo_born = time;
    }
    if(self.carry_player != world || life <= 0 || time < self.vr_ammo_born + life)
    {
        self.alpha = 0;
        self.nextthink = time + (self.vr_ammo_smoke ? 0.1 : 0.25);
        return;
    }
    if(!self.alpha)
    {
        self.alpha = 1;
    }
    self.alpha = self.alpha - 0.05;
    if(self.alpha <= 0.05)
    {
        VR_Reload_Lose(self, "faded away");
        return;
    }
    self.nextthink = time + 0.1;
};

// Room for one more under vr_reload_loose_max: the rounds (shells, magazines) lying about longest go first (never one in
// a hand).
void() VR_Reload_MakeRoom =
{
    float most = max(1, cvar_hget(cvarh_vr_reload_loose_max));
    float n = 0;
    entity e = VR_Reload_NextRound(world);
    while(e)
    {
        if(e.carry_player == world)
        {
            n++;
        }
        e = VR_Reload_NextRound(e);
    }
    while(n >= most)
    {
        entity oldest = world;
        e = VR_Reload_NextRound(world);
        while(e)
        {
            if(e.carry_player == world && (oldest == world || e.vr_ammo_born < oldest.vr_ammo_born))
            {
                oldest = e;
            }
            e = VR_Reload_NextRound(e);
        }
        if(oldest == world)
        {
            return;
        }
        VR_Reload_Lose(oldest, "too many lying about");
        n--;
    }
};

// A new round of `xCount` of ammo `xAid` at `xOrg` (`xWid`: a magazine for that gun; 0 a shell): a carried thing (taken,
// thrown, force grabbed; a rigid body loose).
entity(float xWid, float xAid, float xCount, vector xOrg, vector xAngles) VR_Reload_MakeRound =
{
    VR_Reload_MakeRoom();
    entity r = spawn();
    r.classname = xWid ? VR_RELOAD_MAGCLASS : VR_RELOAD_CLASS;
    r.vr_ammo_wid = xWid;
    r.vr_ammo_aid = xAid;
    r.vr_ammo_count = xCount;
    VR_Reload_SetModel(r);
    r.skin = (xWid && (xAid == AID_LAVA_NAILS || xAid == AID_PLASMA)) ? 1 : 0; // (the lava nails' and plasma's look)
    setorigin(r, xOrg);
    r.angles = xAngles;
    r.movetype = MOVETYPE_TOSS;
    r.solid = SOLID_NOT_BUT_TOUCHABLE;
    r.flags = r.flags | FL_FORCEGRABBABLE | FL_EASYHANDTOUCH;
    r.touch = SUB_Null;
    r.handtouch = VR_Reload_RoundUse; // (VR_Carry_Setup keeps it as carry_use: its pickup)
    r.throwhit = QVR_THROWHIT_NEVER_HIT; // (a round thrown hurts nothing)
    r.owner = world;
    SetThrownWeaponDealDamage(r, FALSE);
    SetThrownWeaponDisappearOnHit(r, FALSE);
    VR_Carry_Setup(r);
    r.vr_impact = VR_Reload_Impact;
    r.vr_ammo_born = time;
    r.think = VR_Reload_RoundThink;
    r.nextthink = time + 0.25;
    return r;
};

// ----------------------------------------------------------------------------
// Magazines in and out

// The gun in `xHand` of `p` (a magazine gun, one in) lets its magazine go: a round of what it held, where its well is
// (the gun's angles), flying at `xVel` (units/s); the gun empty, no magazine (QVR_WPNFLAG_NOMAG). `xInto`: a hand of `p`
// that takes it at once (the pull), -1 none. The magazine.
entity(entity p, float xHand, vector xVel, float xInto, string xHow) VR_Reload_Eject =
{
    float w = VRGetEntWeapon(p, xHand);
    float f = VRGetEntWeaponFlags(p, xHand);
    float n = VRGetEntWeaponClip(p, xHand);
    vector well = xHand == cVR_MainHand ? p.loadportpos : p.offloadportpos;
    if(VR_Reload_HasMagBox(p, xHand))
    {
        well = VR_Reload_MagBoxPos(p, xHand); // (where it was: its middle, the round's origin)
    }
    if(vlen(well - VRGetEntHandPos(p, xHand)) > 64)
    {
        well = VRGetEntHandPos(p, xHand); // (no port reported: a bot's, a test's)
    }
    entity r = VR_Reload_MakeRound(w, WeaponIdToAmmoId(w, f), n, well, VRGetEntHandRot(p, xHand));
    VRSetEntWeaponClip(p, xHand, 0);
    VRSetEntWeaponFlags(p, xHand, f | QVR_WPNFLAG_NOMAG);
    VR_Reload_AmmoChanged(p);
    VR_Reload_Sound(p, VRGetGunChannel(xHand), "vr/reload_mag_out.wav", 1);
    VR_Reload_Haptic(p, xHand, 0.06, 120, 0.7);
    if(xInto >= 0)
    {
        vr_carry_quiet = TRUE;
        VR_Carry_Start(p, xInto, r);
        vr_carry_quiet = FALSE;
        r.vr_ammo_fresh = TRUE;
        VR_Reload_Haptic(p, xInto, 0.1, 90, 0.8);
    }
    else
    {
        r.velocity = xVel;
        r.vr_ammo_fresh = TRUE; // (not back in by contact until it has left the well: VR_Reload_LooseFrame)
        r.avelocity = '0 0 1' * (crandom() * 200) + '1 0 0' * (crandom() * 300);
        r.flags = r.flags - (r.flags & FL_ONGROUND);
    }
    VR_Reload_Log(sprintf("%s out of the gun (hand %g, %s)", VR_Reload_Desc(r), xHand, xHow));
    VR_Reload_CellSparks(p, xHand, r, "taken out");
    VR_Reload_CellSpent(r);
    return r;
};

// The magazine `r` seats in the gun in `xGunHand` of self (which has none): the gun's magazine its rounds, the magazine
// gone (from the other hand, `xHeld`; else a loose one: VR_Reload_LooseFrame).
void(float xGunHand, entity r, float xHeld) VR_Reload_SeatMagInto =
{
    float gunHand = xGunHand;
    float xHand = VRGetOtherHand(gunHand);
    VRSetEntWeaponClip(self, gunHand, r.vr_ammo_count);
    VRSetEntWeaponFlags(self, gunHand, VRGetEntWeaponFlags(self, gunHand) - (VRGetEntWeaponFlags(self, gunHand) &
        QVR_WPNFLAG_NOMAG));
    if(xHeld)
    {
        VR_Carry_SetHeld(self, xHand, world);
        r.carry_player = world;
    }
    if(gunHand == cVR_MainHand)
    {
        self.vr_mag_seated1 = time;
    }
    else
    {
        self.vr_mag_seated0 = time;
    }
    VR_Reload_AmmoChanged(self);
    VR_Reload_Sound(self, VRGetGunChannel(gunHand), "vr/reload_mag_in.wav", 1);
    if(xHeld)
    {
        VR_Reload_Haptic(self, xHand, 0.06, 150, 0.9);
    }
    VR_Reload_Haptic(self, gunHand, 0.1, 60, 0.9);
    VR_Reload_Log(sprintf("%s seated (hand %g%s): the gun holds %g", VR_Reload_Desc(r), gunHand,
        xHeld ? "" : ", a loose one by contact", r.vr_ammo_count));
    VR_Reload_CellSparks(self, gunHand, r, "seated");
    remove(r);
};

// The magazine `r` held in `xHand` of self seats in the gun in the other hand (which has none).
void(float xHand, entity r) VR_Reload_SeatMag =
{
    VR_Reload_SeatMagInto(VRGetOtherHand(xHand), r, TRUE);
};

// The radius of the load point of the gun `xWeapon` (Weapons > Reloading: vr_reload_port_<gun>_radius).
float(float xWeapon) VR_Reload_PortRadius =
{
    if(xWeapon == WID_SUPER_SHOTGUN)
    {
        return cvar_hget(cvarh_vr_reload_port_sshot_radius);
    }
    if(xWeapon == WID_NAILGUN)
    {
        return cvar_hget(cvarh_vr_reload_port_nail_radius);
    }
    if(xWeapon == WID_SUPER_NAILGUN)
    {
        return cvar_hget(cvarh_vr_reload_port_snail_radius);
    }
    if(xWeapon == WID_LIGHTNING)
    {
        return cvar_hget(cvarh_vr_reload_port_light_radius);
    }
    return cvar_hget(cvarh_vr_reload_port_shot_radius);
};

// A magazine's own radius round its reference point (vr_reload_mag_<kind>_radius; 0: a shell).
float(float xWid) VR_Reload_MagRadius =
{
    if(xWid == WID_NAILGUN)
    {
        return cvar_hget(cvarh_vr_reload_mag_nail_radius);
    }
    if(xWid == WID_SUPER_NAILGUN)
    {
        return cvar_hget(cvarh_vr_reload_mag_snail_radius);
    }
    if(xWid == WID_LIGHTNING)
    {
        return cvar_hget(cvarh_vr_reload_mag_light_radius);
    }
    return 0;
};

// A magazine's reference point in the world: its top, its feed end (make_mags.py: up its +z from its middle, 3.3 the
// nailgun's, 4.7 the super nailgun's, 3.6 the cell's), moved by vr_reload_mag_<kind>_x/y/z, as it is turned and sized
// (its prop slot's Size). The engine draws the same (vr_view.cpp heldRoundRef).
vector(entity r) VR_Reload_MagRef =
{
    float top = 3.3;
    vector off = '0 0 0';
    if(r.vr_ammo_wid == WID_SUPER_NAILGUN)
    {
        top = 4.7;
        off = cvar_hget(cvarh_vr_reload_mag_snail_x) * '1 0 0' + cvar_hget(cvarh_vr_reload_mag_snail_y) * '0 1 0' +
              cvar_hget(cvarh_vr_reload_mag_snail_z) * '0 0 1';
    }
    else if(r.vr_ammo_wid == WID_LIGHTNING)
    {
        top = 3.6;
        off = cvar_hget(cvarh_vr_reload_mag_light_x) * '1 0 0' + cvar_hget(cvarh_vr_reload_mag_light_y) * '0 1 0' +
              cvar_hget(cvarh_vr_reload_mag_light_z) * '0 0 1';
    }
    else
    {
        off = cvar_hget(cvarh_vr_reload_mag_nail_x) * '1 0 0' + cvar_hget(cvarh_vr_reload_mag_nail_y) * '0 1 0' +
              cvar_hget(cvarh_vr_reload_mag_nail_z) * '0 0 1';
    }
    float k = propvalue(r, "size");
    if(k <= 0)
    {
        k = 1;
    }
    makevectors('-1 0 0' * r.angles_x + '0 1 0' * r.angles_y + '0 0 1' * r.angles_z); // (a model's pitch is the other way)
    return r.origin + (v_forward * off_x - v_right * off_y + v_up * (top + off_z)) * k;
};

// Whether a magazine `r` fits the gun `p` holds in `xHand` by its handle: that gun's, of its ammo.
float(entity p, float xHand, entity r) VR_Reload_MagFits =
{
    return r.vr_ammo_wid && VRGetEntWeapon(p, xHand) == r.vr_ammo_wid &&
           VR_Reload_AmmoFor(p, xHand) == r.vr_ammo_aid;
};

// The magazine `r` in `xHand` of self, its top at the well of the gun in the other hand: seated if the gun has none. A
// gun with one in takes nothing (a dull tap): knock that one out first (VR_Reload_HitFrame: the bump), then seat this one
// (the author: never in one touch). TRUE if it left the hand.
float(float xHand, entity r) VR_Reload_MagAtWell =
{
    float gunHand = VRGetOtherHand(xHand);
    if(!VR_Reload_MagIn(self, gunHand))
    {
        VR_Reload_SeatMag(xHand, r);
        return TRUE;
    }
    if(!r.vr_ammo_full)
    {
        r.vr_ammo_full = TRUE;
        VR_Reload_Haptic(self, xHand, 0.05, 40, 0.5); // (a magazine in already: knock it out)
    }
    return FALSE;
};

// ----------------------------------------------------------------------------
// The pouch

// The pouch's round for the gun `xWeapon` (its flags `xFlags`): its ammo, and `vr_reload_kind_wid` the gun a magazine is
// for (0: shells). -1: none (a rocket launcher's, a melee weapon's: the pouch gives nothing for it yet).
float vr_reload_kind_wid;
float(float xWeapon, float xFlags) VR_Reload_KindOf =
{
    vr_reload_kind_wid = 0;
    if(xWeapon == WID_FIST || xWeapon == WID_GRAPPLE)
    {
        return -1;
    }
    float aid = WeaponIdToAmmoId(xWeapon, xFlags);
    if(aid == AID_SHELLS)
    {
        return aid; // (the shotgun, the super shotgun, the grunts' shotgun: shells, by hand into the shotgun)
    }
    if(VR_Reload_UsesMag(xWeapon))
    {
        vr_reload_kind_wid = xWeapon;
        return aid;
    }
    return -1;
};

// Once a player frame (W_Frame, before the hands'): the pouch's last kind from the guns held (the main hand's first),
// and what the clients draw.
void() VR_Reload_PlayerFrame =
{
    for(float h = 0; h < 2; h++)
    {
        float hand = h ? cVR_OffHand : cVR_MainHand;
        float aid = VR_Reload_KindOf(VRGetEntWeapon(self, hand), VRGetEntWeaponFlags(self, hand));
        if(aid >= 0 && self.ishuman && VR_Reload_Immersive())
        {
            self.vr_pouch_aid = aid;
            self.vr_pouch_wid = vr_reload_kind_wid;
            break;
        }
    }
    if(!self.vr_pouch_aid)
    {
        self.vr_pouch_aid = AID_SHELLS;
        self.vr_pouch_wid = 0;
    }
    float w = self.vr_pouch_wid;
    self.vr_pouch_kind = w == WID_NAILGUN ? 2 : (w == WID_SUPER_NAILGUN ? 3 : (w == WID_LIGHTNING ? 4 : 1));
    if(self.vr_pouch_aid == AID_LAVA_NAILS || self.vr_pouch_aid == AID_PLASMA)
    {
        self.vr_pouch_kind = self.vr_pouch_kind + 8; // (the clients draw them glowing: the pouch's skin 1)
    }
    self.vr_pouch_count = VR_AmmoUtil_GetAmmoCountByAmmoId(self, self.vr_pouch_aid);
};

// What `xHand` of self takes from the pouch: the round for the gun in the other hand if the pouch has its ammo, else the
// pouch's last kind (both hands empty, a melee weapon there). Its ammo; `vr_reload_kind_wid` the gun a magazine is for.
float(float xHand) VR_Reload_PouchKind =
{
    float other = VRGetOtherHand(xHand);
    float aid = VR_Reload_KindOf(VRGetEntWeapon(self, other), VRGetEntWeaponFlags(self, other));
    if(aid >= 0) // (held by its handle or carried anywhere on it)
    {
        return aid;
    }
    vr_reload_kind_wid = self.vr_pouch_wid;
    return self.vr_pouch_aid ? self.vr_pouch_aid : AID_SHELLS;
};

// `xHand` of self (empty, at the pouch, its grip just pressed) takes a round by its ammo (VR_Reload_PouchKind: the gun in
// the other hand's, else the last kind): a shell (a taped pair with vr_reload_shell_pairs and two or more left), or a
// magazine holding min(its size, the reserve), out of the reserve. None left: the empty pouch's dull pats.
void(float xHand) VR_Reload_Take =
{
    float aid = VR_Reload_PouchKind(xHand);
    float w = vr_reload_kind_wid;
    if(aid < 0 || !VR_Reload_Immersive())
    {
        return;
    }
    float have = VR_AmmoUtil_GetAmmoCountByAmmoId(self, aid);
    if(have < 1)
    {
        VR_Reload_Haptic(self, xHand, 0.12, 30, 0.8);
        VR_Reload_Sound(self, VRGetHandChannel(xHand), "vr/reload_empty.wav", 0.8);
        VR_Reload_Log("the pouch is empty");
        return;
    }
    // (The super shotgun in the other hand: always a taped pair, its two chambers loaded at once.)
    float ssg = VRGetEntWeapon(self, VRGetOtherHand(xHand)) == WID_SUPER_SHOTGUN && VR_Reload_ManualFor(WID_SUPER_SHOTGUN);
    float n = ((cvar_hget(cvarh_vr_reload_shell_pairs) || ssg) && have >= 2) ? 2 : 1;
    if(ssg)
    {
        // (One chamber free, the other loaded, or a spent shell in it: a single shell; a pair only for two.)
        float gunHand = VRGetOtherHand(xHand);
        float gf = VRGetEntWeaponFlags(self, gunHand);
        float free = 2 - VRGetEntWeaponClip(self, gunHand) - ((gf & QVR_WPNFLAG_SSG_OPEN) ? 0 : VR_Reload_SsgSpentOf(gf));
        if(free == 1)
        {
            n = 1;
        }
    }
    if(w)
    {
        n = min(WeaponIdToBaseClipSize(w), have);
    }
    entity r = VR_Reload_MakeRound(w, aid, n, VRGetEntHandPos(self, xHand), VRGetEntHandRot(self, xHand));
    VR_AmmoUtil_DelAmmoCountByAmmoId(self, aid, n);
    VR_Reload_AmmoChanged(self);
    vr_carry_quiet = TRUE; // (the pouch's own sound)
    VR_Carry_Start(self, xHand, r);
    vr_carry_quiet = FALSE;
    VR_Reload_Sound(self, VRGetHandChannel(xHand), "vr/reload_pouch.wav", 0.9);
    VR_Reload_Haptic(self, xHand, 0.07, 120, 0.7);
    VR_Reload_Log(sprintf("%s taken from the pouch by hand %g: %g left", w ? VR_Reload_Desc(r) : (n >= 2 ?
        "a taped pair" : "a shell"), xHand, VR_AmmoUtil_GetAmmoCountByAmmoId(self, aid)));
};

// `xHand` of self lets go of `r` (VR_Carry_HandFrame, before its own let-go): a round at the pouch goes back (TRUE);
// else FALSE (let go of as any carried thing).
float(float xHand, entity r) VR_Reload_LetGo =
{
    if(!VR_Reload_IsRound(r) || !VR_Reload_AtPouch(self, xHand))
    {
        return FALSE;
    }
    VR_CollectFx_Note(r);
    if(!VR_Reload_Refund(self, r, xHand))
    {
        return FALSE;
    }
    VR_CollectFx_Send(xHand, r, QVR_HS_AMMO_POUCH); // (drawn going in: vr_collect_fx)
    return TRUE;
};

// Whether `e` is an ammo box (a map's, the mission packs', the horde's; a backpack is not).
float(entity e) VR_Reload_IsAmmoBox =
{
    string c = e.classname;
    return c == "item_shells" || c == "item_spikes" || c == "item_rockets" || c == "item_cells" ||
           c == "item_lava_spikes" || c == "item_multi_rockets" || c == "item_plasma" ||
           e.carry_use == ammo_touch || e.carry_use == MGH_horde_ammo_touch;
};

// `xHand` of self lets go of the carried `box` (VR_Carry_HandFrame): an ammo box at the ammo pouch goes into it, as at a
// holster (the pack), whatever vr_carry_take says (the pouch is the place for ammo); full up, it drops there. TRUE if it
// was an ammo box at the pouch.
float(float xHand, entity box) VR_Reload_PouchTakesBox =
{
    if(!VR_Reload_AtPouch(self, xHand) || !VR_Reload_IsAmmoBox(box))
    {
        return FALSE;
    }
    VR_Carry_Take(xHand, box);
    if(box.model == string_null)
    {
        VR_Reload_Sound(self, VRGetHandChannel(xHand), "vr/reload_pouch.wav", 0.8);
        VR_Reload_Haptic(self, xHand, 0.08, 90, 0.7);
        VR_Reload_Log(sprintf("an ammo box into the pouch (hand %g)", xHand));
        return TRUE;
    }
    VR_Carry_SetHeld(self, xHand, world);
    VR_Carry_Loose(box);
    box.velocity = '0 0 0';
    return TRUE;
};

// The magazine of the gun in `xGunHand` of self (held or carried) knocked out by a hit (VR_Reload_MagFrame): what the
// other hand brings (the fist, a prop it holds, a magazine, the gun it holds: its line to the muzzle) within
// vr_reload_hit_reach of the magazine anywhere on it (its box, as drawn: VR_Reload_MagDist) at vr_reload_bump_speed or
// more (the hands' relative speed); not by that hand holding the magazine (the two-handed grip) or gripping it now (its
// grip closing on it: HS_MAGAZINE), nor in the half second after one seated. Out it flies along the blow, with the gun's
// own speed. A magazine that knocked it out must leave the well before it seats (the bump reload's second step).
void(float xGunHand) VR_Reload_HitFrame =
{
    float other = VRGetOtherHand(xGunHand);
    // Not just after a seat: the hand that pushed it in follows through, fast, by the well.
    float seated = xGunHand == cVR_MainHand ? self.vr_mag_seated1 : self.vr_mag_seated0;
    if(!VR_Reload_MagIn(self, xGunHand) || (self.vr_mag_pull & (other == cVR_MainHand ? 2 : 1)) ||
        time < seated + 0.5)
    {
        return;
    }
    if(VRIsHandEmpty(self, other) && VR_HandGrabUtil_StartedHandGrabbing(self, other) &&
       (other == cVR_MainHand ? self.mainhand_hotspot : self.offhand_hotspot) == QVR_HS_MAGAZINE)
    {
        return; // (taking hold of it, not hitting it: VR_Reload_MagFrame)
    }
    vector at = VRGetEntHandPos(self, other);
    float d = VR_Reload_MagDist(self, xGunHand, at);
    entity held = VR_Carry_Held(self, other);
    if(held != world)
    {
        d = min(d, VR_Reload_MagDist(self, xGunHand, held.origin));
        if(held.vr_ammo_wid)
        {
            d = min(d, VR_Reload_MagDist(self, xGunHand, VR_Reload_MagRef(held)));
        }
    }
    if(VRGetEntWeapon(self, other) != WID_FIST)
    {
        // The other gun: its line from the hand to the muzzle.
        d = min(d, VR_Reload_MagLineDist(self, xGunHand, at, other == cVR_MainHand ? self.muzzlepos : self.offmuzzlepos));
    }
    if(d > cvar_hget(cvarh_vr_reload_hit_reach))
    {
        return;
    }
    vector rel = VRGetEntHandVel(self, other) - VRGetEntHandVel(self, xGunHand);
    if(cvar_hget(cvarh_vr_reload_debug) >= 2)
    {
        dprint(sprintf("reload: a hit on the magazine at %.2f m/s, %.1f units from it (it pops out at %g)\n",
            vlen(rel), d, cvar_hget(cvarh_vr_reload_bump_speed)));
    }
    if(vlen(rel) < cvar_hget(cvarh_vr_reload_bump_speed))
    {
        return;
    }
    VR_Reload_Eject(self, xGunHand, VRGetEntHandVel(self, xGunHand) * VRMetersToUnits() + normalize(rel) *
        (vlen(rel) * 0.7 * VRMetersToUnits()) + '0 0 40', -1, sprintf("knocked out by a hit at %.2f m/s, %.1f units from it, %.1f from its top",
        vlen(rel), d, vlen(at - (xGunHand == cVR_MainHand ? self.loadportpos : self.offloadportpos))));
    if(held != world && held.vr_ammo_wid)
    {
        held.vr_ammo_fresh = TRUE; // (the bump: it seats once brought back to the well)
    }
    VR_Reload_Haptic(self, other, 0.06, 90, 0.8);
};

// The wrist snap (VR_Reload_MagFrame): how fast the hand holding a magazine turns against the gun's hand (degrees per
// second), the turn between its pose in the gun hand's frame now and as it was at least vr_reload_snap_window seconds ago
// (then that one is kept: a window, the same at any frame rate, a tracking jitter of a frame averaged out). The rate of
// the last window until the next. `xStart`: from now (the grip just taken), 0.
// (The hands' .handavel was the throw's estimate: while the grip is held "as if let go now", the peak of the last
// moments; the gun's hand, not gripping, kept its last release's spin: the old snap test read either.)
.vector vr_mag_snap_f0;  // the off hand's forward and up, in the gun hand's axes (x forward, y right, z up), at
.vector vr_mag_snap_u0;
.float vr_mag_snap_t0;   // this time
.float vr_mag_snap_rate0;
.vector vr_mag_snap_f1;  // the main hand's
.vector vr_mag_snap_u1;
.float vr_mag_snap_t1;
.float vr_mag_snap_rate1;
float vr_reload_snap_window = 0.04;
float(float xHand, float xGunHand, float xStart) VR_Reload_SnapRate =
{
    makevectors(VRGetEntHandRot(self, xGunHand));
    vector gf = v_forward;
    vector gr = v_right;
    vector gu = v_up;
    makevectors(VRGetEntHandRot(self, xHand));
    vector f = (v_forward * gf) * '1 0 0' + (v_forward * gr) * '0 1 0' + (v_forward * gu) * '0 0 1';
    vector u = (v_up * gf) * '1 0 0' + (v_up * gr) * '0 1 0' + (v_up * gu) * '0 0 1';
    float main = xHand == cVR_MainHand;
    float t0 = main ? self.vr_mag_snap_t1 : self.vr_mag_snap_t0;
    if(xStart || t0 > time || time - t0 > 1)
    {
        t0 = time;
    }
    else if(time - t0 >= vr_reload_snap_window)
    {
        vector f0 = main ? self.vr_mag_snap_f1 : self.vr_mag_snap_f0;
        vector u0 = main ? self.vr_mag_snap_u1 : self.vr_mag_snap_u0;
        // The turn's angle from the trace of the relative rotation (f.f0 + u.u0 + r.r0, r = f x u: Binet-Cauchy).
        float c = ((f * f0) + (u * u0) + (f * f0) * (u * u0) - (f * u0) * (u * f0) - 1) * 0.5;
        c = max(-1, min(1, c));
        vector turn = vectoangles(c * '1 0 0' + sqrt(max(0, 1 - c * c)) * '0 1 0');
        float angle = turn_y;
        float rate = angle / (time - t0);
        if(main)
        {
            self.vr_mag_snap_rate1 = rate;
        }
        else
        {
            self.vr_mag_snap_rate0 = rate;
        }
        t0 = time;
    }
    else
    {
        return main ? self.vr_mag_snap_rate1 : self.vr_mag_snap_rate0; // (inside the window: its last)
    }
    if(main)
    {
        self.vr_mag_snap_f1 = f;
        self.vr_mag_snap_u1 = u;
        self.vr_mag_snap_t1 = t0;
        if(xStart)
        {
            self.vr_mag_snap_rate1 = 0;
        }
        return self.vr_mag_snap_rate1;
    }
    self.vr_mag_snap_f0 = f;
    self.vr_mag_snap_u0 = u;
    self.vr_mag_snap_t0 = t0;
    if(xStart)
    {
        self.vr_mag_snap_rate0 = 0;
    }
    return self.vr_mag_snap_rate0;
};

// Every player frame, for `xHand` of self (VR_Reload_HandFrame): the magazines' ejects. The gun in this hand (held or
// carried): its B/Y pressed lets its magazine drop (vr_reload_eject_button); a hit knocks it out (VR_Reload_HitFrame).
// This hand empty, its grip closing on the other hand's gun's magazine (the engine's hotspot HS_MAGAZINE: within
// vr_reload_pull_reach of the magazine's box, nearer it than the gun's own two-handed grip; the engine's two-handed grip
// takes it there too, the gun aimed from it): it holds it (vr_mag_pull; nothing comes out), and it comes out into the hand
// only pulled off hard (the hands' relative speed along the magazine's way out of its well, vr_reload_pull_speed m/s),
// with the wrist snapping against the gun's hand (vr_reload_pull_snap degrees/s: VR_Reload_SnapRate), or with the hands
// gone vr_reload_pull_apart units further apart than when it took hold: any one of the three.
void(float xHand) VR_Reload_MagFrame =
{
    float bit = xHand == cVR_MainHand ? 2 : 1;
    float secBit = xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_SECONDARY : QVR_VRBITS0_OFFHAND_SECONDARY;
    float sec = VRHasBit(self.vrbits0, secBit);
    float secWas = (self.vr_reload_sec & bit) != 0;
    self.vr_reload_sec = self.vr_reload_sec - (self.vr_reload_sec & bit);
    if(sec)
    {
        self.vr_reload_sec = self.vr_reload_sec | bit;
    }
    if(self.health <= 0 || !self.ishuman)
    {
        self.vr_mag_pull = 0;
        return;
    }
    if(sec && !secWas && cvar_hget(cvarh_vr_reload_eject_button) && VR_Reload_MagIn(self, xHand))
    {
        // Out under the gun, falling with the hand's own speed and a little push down.
        makevectors(VRGetEntHandRot(self, xHand));
        VR_Reload_Eject(self, xHand, VRGetEntHandVel(self, xHand) * VRMetersToUnits() - v_up * 40, -1, "the button");
    }
    VR_Reload_HitFrame(xHand);

    float gunHand = VRGetOtherHand(xHand);
    float grabbing = VR_HandGrabUtil_IsHandGrabbing(self, xHand);
    if(!grabbing || !VR_Reload_MagIn(self, gunHand))
    {
        self.vr_mag_pull = self.vr_mag_pull - (self.vr_mag_pull & bit);
        return;
    }
    float apart = vlen(VRGetEntHandPos(self, xHand) - VRGetEntHandPos(self, gunHand));
    if(!(self.vr_mag_pull & bit))
    {
        // On it: the engine's hotspot (the hand within Pull Reach of the magazine's box, nearer it than the gun's other
        // grips or a carried gun's handle; or holding it two-handed); without the client's box (a bot), by its well.
        float on = VR_Reload_HasMagBox(self, gunHand)
                       ? (xHand == cVR_MainHand ? self.mainhand_hotspot : self.offhand_hotspot) == QVR_HS_MAGAZINE
                       : VR_Reload_MagDist(self, gunHand, VRGetEntHandPos(self, xHand)) <= cvar_hget(cvarh_vr_reload_pull_reach);
        if(VR_HandGrabUtil_StartedHandGrabbing(self, xHand) && VRIsHandEmpty(self, xHand) &&
           !VR_Forcegrab_IsHandBusy(self, xHand) && on)
        {
            self.vr_mag_pull = self.vr_mag_pull | bit;
            if(xHand == cVR_MainHand)
            {
                self.vr_mag_pull_apart1 = apart;
            }
            else
            {
                self.vr_mag_pull_apart0 = apart;
            }
            VR_Reload_SnapRate(xHand, gunHand, TRUE);
            VR_Reload_Haptic(self, xHand, 0.04, 140, 0.4);
            VR_Reload_Log(sprintf("hand %g holds the magazine of the gun in hand %g", xHand, gunHand));
        }
        return;
    }
    if(!VRIsHandEmpty(self, xHand))
    {
        self.vr_mag_pull = self.vr_mag_pull - (self.vr_mag_pull & bit);
        return;
    }
    // The pull: the hands' relative speed (the controllers', m/s) along the magazine's way out of its well (its box's
    // length, from its feed end down; without the box, away from the well).
    vector rel = VRGetEntHandVel(self, xHand) - VRGetEntHandVel(self, gunHand);
    vector out = VR_Reload_HasMagBox(self, gunHand)
                     ? normalize(-VR_Reload_MagBoxUp(self, gunHand))
                     : normalize(VRGetEntHandPos(self, xHand) - (gunHand == cVR_MainHand ? self.loadportpos : self.offloadportpos));
    float speed = rel * out;
    float snap = VR_Reload_SnapRate(xHand, gunHand, FALSE);
    float further = apart - (xHand == cVR_MainHand ? self.vr_mag_pull_apart1 : self.vr_mag_pull_apart0);
    if(cvar_hget(cvarh_vr_reload_debug) >= 2)
    {
        dprint(sprintf("reload: holding the magazine: pulled at %.2f m/s, the wrist at %.0f deg/s, %.1f units further apart\n",
            speed, snap, further));
    }
    string why = "";
    if(speed >= cvar_hget(cvarh_vr_reload_pull_speed))
    {
        why = sprintf("pulled off at %.2f m/s", speed);
    }
    else if(snap >= cvar_hget(cvarh_vr_reload_pull_snap))
    {
        why = sprintf("snapped off at %.0f deg/s", snap);
    }
    else if(further >= cvar_hget(cvarh_vr_reload_pull_apart))
    {
        why = sprintf("the hands %.1f units further apart", further);
    }
    if(why != "")
    {
        self.vr_mag_pull = self.vr_mag_pull - (self.vr_mag_pull & bit);
        VR_Reload_Eject(self, gunHand, '0 0 0', xHand, why);
    }
};

// ----------------------------------------------------------------------------
// The super shotgun broken open (phase 2b)

// The super shotgun in `xHand` of `p` breaks open (`xHow`: what did it, for the log): its spent shells thrown out (the
// clients' casings, vr_shells.cpp), its live ones too, as rounds lying about to take again; open, empty, until closed.
void(entity p, float xHand, string xHow) VR_Reload_SsgBreak =
{
    float f = VRGetEntWeaponFlags(p, xHand);
    float spent = VR_Reload_SsgSpentOf(f);
    float live = VRGetEntWeaponClip(p, xHand);
    VRSetEntWeaponFlags(p, xHand, (f - (f & QVR_WPNFLAG_SSG_SPENT)) | QVR_WPNFLAG_SSG_OPEN);
    VRSetEntWeaponClip(p, xHand, 0);
    VR_Reload_AmmoChanged(p);
    p.vr_ssg_toggled = time;
    VR_Reload_Sound(p, VRGetGunChannel(xHand), "vr/reload_ssg_open.wav", 1);
    VR_Reload_Haptic(p, xHand, 0.08, 60, 0.9);
    if(spent + live > 0)
    {
        VR_Reload_Sound(p, CHAN_AUTO, "vr/reload_ssg_eject.wav", 0.9);
        VR_Reload_Haptic(p, xHand, 0.05, 140, 0.6);
    }
    if(spent > 0)
    {
        entity oself = self;
        self = p;
        ejectcasings(xHand, QVR_CASING_SHOTGUNSHELL, spent, 0.08, 0);
        self = oself;
    }
    // The live ones: out of the chambers (the breech face, as drawn), back over the hand and up.
    vector at = xHand == cVR_MainHand ? p.loadportpos : p.offloadportpos;
    if(vlen(at - VRGetEntHandPos(p, xHand)) > 64)
    {
        at = VRGetEntHandPos(p, xHand); // (no port reported: a bot's, a test's)
    }
    makevectors(VRGetEntHandRot(p, xHand));
    for(float i = 0; i < live; i++)
    {
        float side = i ? -1 : 1;
        entity r = VR_Reload_MakeRound(0, AID_SHELLS, 1, at + v_right * side * 1.2, VRGetEntHandRot(p, xHand));
        r.vr_ammo_fresh = TRUE; // (not back in by contact until it has left the breech)
        r.velocity = VRGetEntHandVel(p, xHand) * VRMetersToUnits() - v_forward * 70 + v_up * 90 + v_right * side * 25;
        r.avelocity = '0 1 0' * (crandom() * 400) + '1 0 0' * (crandom() * 300);
        r.flags = r.flags - (r.flags & FL_ONGROUND);
    }
    VR_Reload_Log(sprintf("the super shotgun (hand %g) broken open by %s: %g spent and %g live thrown out", xHand, xHow,
        spent, live));
};

// The super shotgun in `xHand` of `p` snaps shut (`xHow`, for the log): it fires again (what its chambers hold).
void(entity p, float xHand, string xHow) VR_Reload_SsgClose =
{
    float f = VRGetEntWeaponFlags(p, xHand);
    VRSetEntWeaponFlags(p, xHand, f - (f & QVR_WPNFLAG_SSG_OPEN));
    p.vr_ssg_toggled = time;
    VR_Reload_Sound(p, VRGetGunChannel(xHand), "vr/reload_ssg_close.wav", 1);
    VR_Reload_Haptic(p, xHand, 0.1, 55, 1);
    if(VRIsHandEmpty(p, VRGetOtherHand(xHand)))
    {
        VR_Reload_Haptic(p, VRGetOtherHand(xHand), 0.06, 80, 0.6);
    }
    VR_Reload_Log(sprintf("the super shotgun (hand %g) closed by %s: %g loaded", xHand, xHow, VRGetEntWeaponClip(p,
        xHand)));
};

// Whether something the other hand brings hits the front half of the barrels of the super shotgun in `xGunHand` of
// self (its front 40%): `xSide` -1 from above (the gun's own down; it breaks open: vr_reload_ssg_hit_open_speed, _angle), 1 from below
// (its own up; open, it snaps shut: vr_reload_ssg_hit_close_*), within vr_reload_ssg_hit_reach of them (open: as they
// are turned down by vr_reload_ssg_open_angle from the breech), at the hands' relative speed: a fist, a held prop's
// middle, the other gun's line from its hand to its muzzle. Not while both hands hold it (the pry and the lift are
// theirs), nor a round the hand holds (brought to the breech to load). What it was in vr_ssg_hitdesc.
string vr_ssg_hitdesc;
float(float xGunHand, float xSide) VR_Reload_SsgHit =
{
    float other = VRGetOtherHand(xGunHand);
    if(self.vr_ssg_held & (xGunHand == cVR_MainHand ? 2 : 1))
    {
        return FALSE;
    }
    if(time < self.vr_ssg_loaded + 0.6)
    {
        return FALSE; // (the hand that just loaded it, drawing back)
    }
    vector b = xGunHand == cVR_MainHand ? self.loadportpos : self.offloadportpos;
    vector m = xGunHand == cVR_MainHand ? self.muzzlepos : self.offmuzzlepos;
    if(vlen(b - VRGetEntHandPos(self, xGunHand)) > 64 || vlen(m - b) < 1)
    {
        return FALSE; // (no port or muzzle reported: a bot's, a test's)
    }
    makevectors(VRGetEntHandRot(self, xGunHand));
    vector up = v_up;
    vector dir = normalize(m - b);
    if(xSide > 0)
    {
        float a = cvar_hget(cvarh_vr_reload_ssg_open_angle) / 57.2958;
        dir = normalize(v_forward * cos(a) - v_up * sin(a));
    }
    float len = vlen(m - b);
    vector p0 = b + dir * (len * 0.6);
    vector p1 = b + dir * len;
    // The nearest of what the other hand brings to that half (the other gun's line in five points).
    float d = 100000;
    float side = 0; // (where the nearest was: above the barrels (> 0) or under them, by the gun's up)
    entity held = VR_Carry_Held(self, other);
    if(held != world && VR_Reload_IsRound(held))
    {
        return FALSE; // (a shell or a pair brought to its breech to load it: not a hit)
    }
    vector q = '0 0 0';
    float t;
    float k;
    for(k = 0; k < 5; k++)
    {
        if(k == 0)
        {
            q = held != world ? held.origin : VRGetEntHandPos(self, other);
        }
        else if(VRGetEntWeapon(self, other) != WID_FIST)
        {
            vector om = other == cVR_MainHand ? self.muzzlepos : self.offmuzzlepos;
            q = VRGetEntHandPos(self, other) + (om - VRGetEntHandPos(self, other)) * (k / 4);
        }
        else
        {
            break;
        }
        t = max(0, min(1, ((q - p0) * (p1 - p0)) / max(0.001, (p1 - p0) * (p1 - p0))));
        if(vlen(p0 + (p1 - p0) * t - q) < d)
        {
            d = vlen(p0 + (p1 - p0) * t - q);
            // (Where it came from, 50 ms before: on the barrels now, it hit them from that side.)
            side = (q - (VRGetEntHandVel(self, other) - VRGetEntHandVel(self, xGunHand)) * (VRMetersToUnits() * 0.05) -
                    (p0 + (p1 - p0) * t)) * up;
        }
    }
    float reach = cvar_hget(cvarh_vr_reload_ssg_hit_reach);
    if(d > reach)
    {
        return FALSE;
    }
    vector rel = VRGetEntHandVel(self, other) - VRGetEntHandVel(self, xGunHand);
    vector want = xSide < 0 ? -up : up;
    float speed = vlen(rel);
    float along = rel * want;
    float off = atan2(vlen(rel - want * along), along) * 57.2958;
    float need = cvar_hget(xSide < 0 ? cvarh_vr_reload_ssg_hit_open_speed : cvarh_vr_reload_ssg_hit_close_speed);
    float most = cvar_hget(xSide < 0 ? cvarh_vr_reload_ssg_hit_open_angle : cvarh_vr_reload_ssg_hit_close_angle);
    if(cvar_hget(cvarh_vr_reload_debug) >= 2)
    {
        dprint(sprintf("reload: a hit on the super shotgun's barrels at %.2f m/s, %.0f deg off %s, %.1f units off them\n",
            speed, off, xSide < 0 ? "down" : "up", d));
    }
    if(speed < need || off > most || side * xSide > 0)
    {
        return FALSE; // (too slow, from the side, or from the other side: going away)
    }
    vr_ssg_hitdesc = sprintf("a hit from %s at %.2f m/s (%.0f deg off)", xSide < 0 ? "above" : "below", speed, off);
    return TRUE;
};

// Every player frame, for the super shotgun in `xHand` of self (VR_Reload_HandFrame): it opens by the flick (its hand's
// flick bit rising: vr_reload_ssg_open_flick), the pry (the other hand's flick bit rising: the engine's vr_flick.cpp
// sends it so, with both hands on it: vr_reload_ssg_pry), a hit from above (vr_reload_ssg_open_hit) or its B/Y
// (vr_reload_ssg_open_button); open, it closes by the flick (vr_reload_ssg_close_flick), the barrels lifted (the other
// hand's bit again: vr_reload_ssg_close_pry), a hit from below (vr_reload_ssg_close_hit) or by itself once both chambers
// are loaded (vr_reload_ssg_close_auto). Each its own switch.
void(float xHand) VR_Reload_SsgFrame =
{
    float bit = xHand == cVR_MainHand ? 2 : 1;
    float other = VRGetOtherHand(xHand);
    float flick = VR_HandGrabUtil_IsHandReloadFlicking(self, xHand);
    float pry = VR_HandGrabUtil_IsHandReloadFlicking(self, other);
    float sec = VRHasBit(self.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_SECONDARY : QVR_VRBITS0_OFFHAND_SECONDARY);
    float flickEdge = flick && !(self.vr_ssg_flick & bit);
    float pryEdge = pry && !(self.vr_ssg_pry & bit);
    float secEdge = sec && !(self.vr_ssg_sec & bit);
    self.vr_ssg_flick = (self.vr_ssg_flick - (self.vr_ssg_flick & bit)) | (flick ? bit : 0);
    self.vr_ssg_pry = (self.vr_ssg_pry - (self.vr_ssg_pry & bit)) | (pry ? bit : 0);
    self.vr_ssg_sec = (self.vr_ssg_sec - (self.vr_ssg_sec & bit)) | (sec ? bit : 0);
    float f = VRGetEntWeaponFlags(self, xHand);
    if(VRGetEntWeapon(self, xHand) != WID_SUPER_SHOTGUN || !VR_Reload_ManualFor(WID_SUPER_SHOTGUN) || self.health <= 0 ||
       !self.ishuman || (f & QVR_WPNFLAG_FOREGRIP_CARRIED))
    {
        self.vr_ssg_held = self.vr_ssg_held - (self.vr_ssg_held & bit);
        return;
    }
    // Both hands on it (the two-handed hold, until the other hand lets go): its hits are the pry's and the lift's.
    float holding = VRIsHandEmpty(self, other) && VR_HandGrabUtil_IsHandGrabbing(self, other);
    if(holding && VRGet2HAiming(self))
    {
        self.vr_ssg_held = self.vr_ssg_held | bit;
    }
    else if(!holding)
    {
        self.vr_ssg_held = self.vr_ssg_held - (self.vr_ssg_held & bit);
    }
    if(time < self.vr_ssg_toggled + 0.35)
    {
        return;
    }
    float flicks = VRIsWeaponReloadFlickingEnabledFor(self, xHand);
    if(!(f & QVR_WPNFLAG_SSG_OPEN))
    {
        // Not while it fires (the author's note, vrfiringrange_2026-10-07_22-07-20): its firing animation runs to its end
        // (the gun back at rest, frame 0) before anything breaks it open; a way tried meanwhile does nothing.
        if(VRGetWeaponFrame(xHand) != 0)
        {
            if(flickEdge || pryEdge || secEdge)
            {
                VR_Reload_Log(sprintf("the super shotgun (hand %g) is still firing (frame %g): not opened", xHand,
                    VRGetWeaponFrame(xHand)));
            }
            return;
        }
        if(flickEdge && flicks && cvar_hget(cvarh_vr_reload_ssg_open_flick))
        {
            VR_Reload_SsgBreak(self, xHand, "a flick");
        }
        else if(pryEdge && cvar_hget(cvarh_vr_reload_ssg_pry))
        {
            VR_Reload_SsgBreak(self, xHand, "the pry");
        }
        else if(secEdge && cvar_hget(cvarh_vr_reload_ssg_open_button))
        {
            VR_Reload_SsgBreak(self, xHand, "the button");
        }
        else if(cvar_hget(cvarh_vr_reload_ssg_open_hit) && VR_Reload_SsgHit(xHand, -1))
        {
            VR_Reload_SsgBreak(self, xHand, vr_ssg_hitdesc);
        }
        return;
    }
    if(flickEdge && flicks && cvar_hget(cvarh_vr_reload_ssg_close_flick))
    {
        VR_Reload_SsgClose(self, xHand, "a flick");
    }
    else if(pryEdge && cvar_hget(cvarh_vr_reload_ssg_close_pry))
    {
        VR_Reload_SsgClose(self, xHand, "the barrels lifted");
    }
    else if(cvar_hget(cvarh_vr_reload_ssg_close_hit) && VR_Reload_SsgHit(xHand, 1))
    {
        VR_Reload_SsgClose(self, xHand, vr_ssg_hitdesc);
    }
    else if(cvar_hget(cvarh_vr_reload_ssg_close_auto) && VRGetEntWeaponClip(self, xHand) >= 2 &&
            time >= self.vr_ssg_toggled + 0.3)
    {
        VR_Reload_SsgClose(self, xHand, "itself (both loaded)");
    }
};

// Every player frame, for `xHand` of self (W_Frame, beside the grenade pouch's): at the ammo pouch, a tap as the hand
// arrives (with a round to take for the other hand's gun, or one to put back), and a grip pressed there by an empty
// hand takes one (VR_Reload_Take).
void(float xHand) VR_Reload_LooseFrame;

void(float xHand) VR_Reload_HandFrame =
{
    float bit = xHand == cVR_MainHand ? 2 : 1;
    float at = VR_Reload_AtPouch(self, xHand) && self.health > 0;
    float was = (self.vr_reload_at & bit) != 0;
    self.vr_reload_at = self.vr_reload_at - (self.vr_reload_at & bit);
    if(at)
    {
        self.vr_reload_at = self.vr_reload_at | bit;
    }
    if(!VR_HandGrabUtil_IsHandGrabbing(self, xHand))
    {
        self.vr_reload_tried = self.vr_reload_tried - (self.vr_reload_tried & bit);
    }
    VR_Reload_MagFrame(xHand);
    VR_Reload_SsgFrame(xHand);
    VR_Reload_LooseFrame(xHand);
    if(!at || !self.ishuman || (!deathmatch && time < readytime))
    {
        return;
    }
    float climbing = VRHasBit(self.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_CLIMBING :
                                                                    QVR_VRBITS0_OFFHAND_CLIMBING);
    float emptyHand = VRIsHandEmpty(self, xHand) &&
                      !VR_Melee_HoldsTorch(self, xHand) && !VR_Forcegrab_IsHandBusy(self, xHand) && !climbing;
    float aid = VR_Reload_Immersive() ? VR_Reload_PouchKind(xHand) : -1;
    if(!was)
    {
        entity held = VR_Carry_Held(self, xHand);
        if(emptyHand && aid >= 0 && VR_AmmoUtil_GetAmmoCountByAmmoId(self, aid) >= 1)
        {
            VR_Reload_Haptic(self, xHand, 0.04, 160, 0.35); // (as a holster with a gun in it)
        }
        else if(held != world && VR_Reload_IsRound(held))
        {
            VR_Reload_Haptic(self, xHand, 0.04, 160, 0.35); // (to put it back)
        }
    }
    if(emptyHand && aid >= 0 && VR_HandGrabUtil_StartedHandGrabbing(self, xHand) && !(self.vr_reload_tried & bit))
    {
        self.vr_reload_tried = self.vr_reload_tried | bit;
        VR_Reload_Take(xHand);
    }
};

// ----------------------------------------------------------------------------
// Loading

// The round `r` goes into the gun in `xGunHand` of self (from the other hand, `xHeld`; else a loose one:
// VR_Reload_LooseFrame): as many of its rounds as fit (a pair with one place left: one, a single shell left in the hand
// or lying). TRUE if it is used up (gone). A full gun takes nothing: a dull tap (once, until it leaves the port).
float(float xGunHand, entity r, float xHeld) VR_Reload_LoadInto =
{
    float gunHand = xGunHand;
    float xHand = VRGetOtherHand(gunHand);
    float w = VRGetEntWeapon(self, gunHand);
    float clip = VRGetEntWeaponClip(self, gunHand);
    float room = WeaponIdToBaseClipSize(w) - clip;
    if(w == WID_SUPER_SHOTGUN && !VR_Reload_SsgIsOpen(self, gunHand))
    {
        if(!r.vr_ammo_full)
        {
            // (Shut: its chambers can't be reached. Once as it comes within the load point's range, until it leaves it.)
            r.vr_ammo_full = TRUE;
            VR_Reload_Haptic(self, xHeld ? xHand : gunHand, 0.06, 40, 0.6);
            VR_Reload_Sound(self, VRGetGunChannel(gunHand), "vr/reload_blocked.wav", 1);
            VR_Reload_Log("the super shotgun is shut: break it open first");
        }
        return FALSE;
    }
    if(room <= 0)
    {
        if(!r.vr_ammo_full)
        {
            // (Once as it comes within the port's range, until it leaves it: VR_Reload_HeldFrame.)
            r.vr_ammo_full = TRUE;
            VR_Reload_Haptic(self, xHeld ? xHand : gunHand, 0.06, 40, 0.6);
            VR_Reload_Sound(self, VRGetGunChannel(gunHand), "vr/reload_blocked.wav", 1);
            VR_Reload_Log("the gun is full: the gate won't open");
        }
        return FALSE;
    }
    float n = min(r.vr_ammo_count, room);
    VRSetEntWeaponClip(self, gunHand, clip + n);
    r.vr_ammo_count = r.vr_ammo_count - n;
    VR_Reload_AmmoChanged(self);
    VR_Reload_Sound(self, VRGetGunChannel(gunHand), VR_Reload_ShellInSound(), 1);
    if(w == WID_SUPER_SHOTGUN)
    {
        self.vr_ssg_loaded = time;
    }
    if(xHeld)
    {
        VR_Reload_Haptic(self, xHand, 0.05, 160, 0.8); // the shell pushed home
    }
    VR_Reload_Haptic(self, gunHand, 0.08, 70, 0.7);  // the gun taking it
    VR_Reload_Log(sprintf("%g into the gun (hand %g%s): its magazine %g of %g, %g in the reserve", n, gunHand,
        xHeld ? "" : ", a loose round by contact", clip + n, WeaponIdToBaseClipSize(w),
        VR_AmmoUtil_GetAmmoCountByAmmoId(self, r.vr_ammo_aid)));
    if(clip + n >= WeaponIdToBaseClipSize(w))
    {
        // The last one in: the tube full (a heavier knock, a second buzz).
        VR_Reload_Sound(self, CHAN_AUTO, "vr/reload_full.wav", 1);
        VR_Reload_Haptic(self, gunHand, 0.14, 50, 0.9);
        VR_Reload_Log("the gun is full now");
    }
    // Drawn sliding into the gun on the player's client (vr_reload_insert_time; vr_collectfx.cpp's "into the gun"): the
    // round let go of at once (loaded already), as it was; of a pair with one place, a single shell where it is.
    if(r.vr_ammo_count <= 0)
    {
        collectfx(xHand, r, QVR_CFX_INTO_GUN, r.modelindex, r.origin, r.angles);
        if(xHeld)
        {
            VR_Carry_SetHeld(self, xHand, world);
            r.carry_player = world;
        }
        remove(r);
        return TRUE;
    }
    VR_Reload_SetModel(r); // (a pair's other shell, still in the fingers, or lying)
    collectfx(xHand, world, QVR_CFX_INTO_GUN, r.modelindex, r.origin, r.angles);
    return FALSE;
};

// The round `r` in `xHand` of self goes into the gun in the other hand (VR_Reload_LoadInto).
float(float xHand, entity r) VR_Reload_Load =
{
    return VR_Reload_LoadInto(VRGetOtherHand(xHand), r, TRUE);
};

// While `xHand` of self holds the round `r` (VR_Carry_HandFrame, gripping): at the loading port of the gun in the other
// hand (a shell's middle within the gun's radius; a magazine's top within the gun's radius plus its own), it goes in. TRUE
// if it is gone from the hand.
float(float xHand, entity r) VR_Reload_HeldFrame =
{
    if(!VR_Reload_IsRound(r))
    {
        return FALSE;
    }
    float gunHand = VRGetOtherHand(xHand);
    if(VR_Reload_AmmoFor(self, gunHand) != r.vr_ammo_aid)
    {
        r.vr_ammo_full = FALSE;
        return FALSE;
    }
    if(r.vr_ammo_wid && !VR_Reload_MagFits(self, gunHand, r))
    {
        r.vr_ammo_full = FALSE;
        return FALSE;
    }
    vector port = gunHand == cVR_MainHand ? self.loadportpos : self.offloadportpos;
    float d = vlen((r.vr_ammo_wid ? VR_Reload_MagRef(r) : r.origin) - port);
    float leniency = VR_Reload_PortRadius(VRGetEntWeapon(self, gunHand)) + VR_Reload_MagRadius(r.vr_ammo_wid);
    if(cvar_hget(cvarh_vr_reload_debug) >= 2)
    {
        dprint(sprintf("reload: a held round %.1f units from the port (leniency %g): at %v, the port %v\n", d,
            leniency, r.vr_ammo_wid ? VR_Reload_MagRef(r) : r.origin, port));
    }
    if(d > leniency + 4)
    {
        r.vr_ammo_fresh = FALSE; // (well away: a magazine just out, or that knocked one out, may seat when brought back)
    }
    if(d > leniency)
    {
        r.vr_ammo_full = FALSE;
        return FALSE;
    }
    if(r.vr_ammo_fresh)
    {
        return FALSE; // (just pulled out: not back in until it has left the well)
    }
    if(r.vr_ammo_wid)
    {
        return VR_Reload_MagAtWell(xHand, r);
    }
    return VR_Reload_Load(xHand, r);
};

// ----------------------------------------------------------------------------
// Loose rounds by contact (the author's notes 2026-10-07): a shell or magazine lying about, dropped or thrown goes into
// the gun it meets at its load point, lying the way it goes in: shells dropped into the shotgun's port or the open
// super shotgun's barrels, a magazine dropped feed end first into its well, or the gun brought down onto one lying on a
// table. The gun's collision is its drawn hull (vr_box3d.cpp syncReach), which covers its port and well (the shotgun's
// 1.2 units in, the thunderbolt's 1.9: vr_physics_shapes): a round that fits passes through it near the load point (the
// engine's reachSkips, by .vr_ammo_pass), so that it reaches it.

// The way the round `r` lies: a shell's axis (its open end: +x), a magazine's (its feed end: +z).
vector(entity r) VR_Reload_RoundAxis =
{
    makevectors('-1 0 0' * r.angles_x + '0 1 0' * r.angles_y + '0 0 1' * r.angles_z); // (a model's pitch is the other way)
    return r.vr_ammo_wid ? v_up : v_forward;
};

// Whether the gun in `xGunHand` of self takes the loose round `r` now: it fits (its ammo; a magazine its gun's), there is
// room (a shell: the gun not full, the super shotgun open; a magazine: none in), and `r` lies the way it goes in (within
// vr_reload_contact_angle degrees of the engine's .loadportaxis: a shell's open end along the tube or the barrels, a
// magazine's feed end up the well; never sideways).
float(float xGunHand, entity r) VR_Reload_LooseFits =
{
    if(VR_Reload_AmmoFor(self, xGunHand) != r.vr_ammo_aid)
    {
        return FALSE;
    }
    float w = VRGetEntWeapon(self, xGunHand);
    if(r.vr_ammo_wid)
    {
        if(!VR_Reload_MagFits(self, xGunHand, r) || VR_Reload_MagIn(self, xGunHand))
        {
            return FALSE;
        }
    }
    else if(VR_Reload_UsesMag(w) || VRGetEntWeaponClip(self, xGunHand) >= WeaponIdToBaseClipSize(w) ||
            (w == WID_SUPER_SHOTGUN && !VR_Reload_SsgIsOpen(self, xGunHand)))
    {
        return FALSE;
    }
    vector way = xGunHand == cVR_MainHand ? self.loadportaxis : self.offloadportaxis;
    if(way == '0 0 0')
    {
        return FALSE; // (no axis from the engine: a bot's)
    }
    float most = max(0, min(89, cvar_hget(cvarh_vr_reload_contact_angle)));
    return VR_Reload_RoundAxis(r) * normalize(way) >= cos(most * 0.0174533);
};

// How near the reference point of the loose round `r` (a shell's middle, a magazine's top) came to `xPort` this frame:
// along its way since the last (a falling round moves a few units a frame).
float(entity r, vector xPort) VR_Reload_LooseDist =
{
    vector b = r.vr_ammo_wid ? VR_Reload_MagRef(r) : r.origin;
    vector ab = r.velocity * frametime;
    vector a = b - ab;
    float t = 0;
    if(ab * ab > 0.0001)
    {
        t = max(0, min(1, ((xPort - a) * ab) / (ab * ab)));
    }
    return vlen(a + ab * t - xPort);
};

// Each player frame, for each hand (VR_Reload_HandFrame): the loose rounds at the load point of the hand's gun go in,
// if they fit and lie right (VR_Reload_LooseFits): within its radius (a magazine's own radius added) and
// vr_reload_contact_leniency cm more, on the side its opening faces (the engine's .loadportface); within that leniency
// more again they pass through the gun's collision (.vr_ammo_pass), which covers the port, so that they reach it. One
// just out of a gun (a magazine dropped or knocked out, the super shotgun's live shells thrown out: .vr_ammo_fresh)
// only once it has left.
void(float xHand) VR_Reload_LooseFrame =
{
    float bit = xHand == cVR_MainHand ? 2 : 1;
    float on = cvar_hget(cvarh_vr_reload_contact) && self.ishuman && self.health > 0 &&
               VR_Reload_AmmoFor(self, xHand) >= 0;
    if(!on && !(self.vr_ammo_passing & bit))
    {
        return;
    }
    self.vr_ammo_passing = self.vr_ammo_passing - (self.vr_ammo_passing & bit);
    vector port = xHand == cVR_MainHand ? self.loadportpos : self.offloadportpos;
    vector face = xHand == cVR_MainHand ? self.loadportface : self.offloadportface;
    float radius = VR_Reload_PortRadius(VRGetEntWeapon(self, xHand));
    float lenient = max(0, cvar_hget(cvarh_vr_reload_contact_leniency)) * VRMetersToUnits() / 100;
    entity r = VR_Reload_NextRound(world);
    while(r)
    {
        entity next = VR_Reload_NextRound(r);
        float pass = FALSE;
        float gone = FALSE;
        if(on && r.carry_player == world && r.fg_state != 1)
        {
            float within = radius + VR_Reload_MagRadius(r.vr_ammo_wid) + lenient;
            float d = VR_Reload_LooseDist(r, port);
            // (From the opening's side only, at most half the radius past it: not through the gun from its other side.)
            float side = ((r.vr_ammo_wid ? VR_Reload_MagRef(r) : r.origin) - port) * face;
            if(d > within + lenient + 4)
            {
                r.vr_ammo_fresh = FALSE;
            }
            if(d <= within + lenient && side >= -0.5 * radius && !r.vr_ammo_fresh && VR_Reload_LooseFits(xHand, r))
            {
                pass = TRUE;
                if(cvar_hget(cvarh_vr_reload_debug) >= 2)
                {
                    dprint(sprintf("reload: %s lying loose %.1f units from the port of hand %g (in within %.1f, through the gun "
                        "within %.1f)\n", VR_Reload_Desc(r), d, xHand, within, within + lenient));
                }
                if(d <= within)
                {
                    if(r.vr_ammo_wid)
                    {
                        VR_Reload_SeatMagInto(xHand, r, FALSE);
                        gone = TRUE;
                    }
                    else
                    {
                        gone = VR_Reload_LoadInto(xHand, r, FALSE);
                    }
                }
            }
        }
        if(!gone)
        {
            if(pass)
            {
                if(r.vr_ammo_passer != self)
                {
                    r.vr_ammo_pass = 0;
                }
                r.vr_ammo_passer = self;
                r.vr_ammo_pass = r.vr_ammo_pass | bit;
                self.vr_ammo_passing = self.vr_ammo_passing | bit;
            }
            else if(r.vr_ammo_passer == self)
            {
                r.vr_ammo_pass = r.vr_ammo_pass - (r.vr_ammo_pass & bit);
            }
        }
        r = next;
    }
};

// A level's end (SetChangeParms; self is the player): a round in a hand goes back into the reserve, so that it goes on
// with the player (one lying about stays behind with the level).
void() VR_Reload_LevelEnd =
{
    for(float h = 0; h < 2; h++)
    {
        float hand = h ? cVR_MainHand : cVR_OffHand;
        entity r = VR_Carry_Held(self, hand);
        if(r != world && VR_Reload_IsRound(r))
        {
            VR_Reload_Refund(self, r, hand);
        }
    }
};
