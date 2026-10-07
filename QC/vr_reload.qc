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
// vr_reload_port_leniency of that gun's loading port (the engine's .loadportpos, .offloadportpos: as drawn) it goes into
// the gun's magazine (its record: it stays with the gun). Let go of at the pouch (or at a holster, or the trigger pressed,
// as any carried pickup) it goes back into the reserve; let go of anywhere else it drops, a physics prop that can be taken
// again by hand or force grab, until the cap of rounds lying about (vr_reload_loose_max) or its time
// (vr_reload_loose_time) takes it: then it is lost. One in a hand at a level's end is refunded.
// Everything is the server's (each player's own pouch and rounds); the client only reports where the pouch and the port
// are. Holster, button and flick reloads skip the hand-loaded guns in this mode (VRTryReloadWeapon); the others keep the
// hip holsters' reload.
//
// Magazines (phase 2: the nailgun, the super nailgun, the thunderbolt; VR_Reload_UsesMag): the gun's magazine is its
// record's (.wi_clip: what the attached magazine holds); none attached is the weapon flag QVR_WPNFLAG_NOMAG (it travels with
// the gun as its flags do; the clients draw the attached one: vr_view.cpp setupMagazines). The pouch gives a magazine for
// the other hand's gun holding min(its size, the reserve); one held at the gun's magazine well (vr_reload_mag_leniency)
// seats if the gun has none; if it has one, only a bump seats it (the hands meeting at vr_reload_bump_speed: the old one
// knocked out, flying). A seated one comes out by the gun's controller's B/Y (vr_reload_eject_button), or by the other hand
// gripping it and pulling it off hard with a wrist snap (vr_reload_pull_speed and vr_reload_pull_snap: a gentle pull only
// holds it, as a foregrip). Out, it keeps its count: a round as the shells are (taken again, force grabbed, refunded at
// the pouch, lost to the caps).

.float vr_ammo_aid;      // a round: the ammo it is (AID_*)
.float vr_ammo_count;    // and how many (a taped pair: 2)
.float vr_ammo_wid;      // a magazine: the gun it fits (WID_*; 0 a shell)
.float vr_ammo_fresh;    // a magazine just pulled out of its gun: it seats again only once it has left the well
.float vr_mag_pull;      // a player's hands holding the attached magazine of the other hand's gun (1 off, 2 main)
.float vr_reload_sec;    // and its hands' B/Y last frame (1 off, 2 main)
.float vr_ammo_born;     // when it was last in a hand or flying (its time lying about runs from here)
.float vr_ammo_tink;     // its last knock's sound
.float vr_ammo_full;     // the full gun's tap given (until it leaves the port)
.float vr_reload_at;     // a player's hands at the ammo pouch last frame (1 off, 2 main)
.float vr_reload_tried;  // and those whose grip press there was used

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
    precache_sound("vr/reload_shell_in.wav"); // a shell pushed into the port
    precache_sound("vr/reload_empty.wav");    // the pouch found empty
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
    return VR_Reload_Immersive() && (xWeapon == WID_SHOTGUN || VR_Reload_UsesMag(xWeapon));
};

// The hand's angular velocity (radians per second, world axes).
vector(entity e, float xHand) VR_Reload_HandAVel =
{
    return xHand == cVR_OffHand ? e.offhandavel : e.handavel;
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

// Whether the gun `p` holds in `xHand` by its handle takes magazines (immersive) and has one in.
float(entity p, float xHand) VR_Reload_MagIn =
{
    float w = VRGetEntWeapon(p, xHand);
    float f = VRGetEntWeaponFlags(p, xHand);
    return p.ishuman && VR_Reload_Immersive() && VR_Reload_UsesMag(w) && !(f & QVR_WPNFLAG_FOREGRIP_CARRIED) &&
           !(f & QVR_WPNFLAG_NOMAG);
};

// The ammo whose rounds the pouch gives for the gun `p` holds in `xHand` by its handle, if it loads by hand; else -1.
float(entity p, float xHand) VR_Reload_AmmoFor =
{
    float w = VRGetEntWeapon(p, xHand);
    if(!p.ishuman || !VR_Reload_ManualFor(w) || (VRGetEntWeaponFlags(p, xHand) & QVR_WPNFLAG_FOREGRIP_CARRIED))
    {
        return -1;
    }
    return WeaponIdToAmmoId(w, VRGetEntWeaponFlags(p, xHand));
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
        setsize(r, modelbounds(r, 0), modelbounds(r, 1));
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

// Held or flying its time waits; lying about past vr_reload_loose_time it fades away over two seconds.
void() VR_Reload_RoundThink =
{
    float life = cvar_hget(cvarh_vr_reload_loose_time);
    if(self.carry_player != world || (!(self.flags & FL_ONGROUND) && vlen(self.velocity) > 1))
    {
        self.vr_ammo_born = time;
    }
    if(self.carry_player != world || life <= 0 || time < self.vr_ammo_born + life)
    {
        self.alpha = 0;
        self.nextthink = time + 0.25;
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
        r.avelocity = '0 0 1' * (crandom() * 200) + '1 0 0' * (crandom() * 300);
        r.flags = r.flags - (r.flags & FL_ONGROUND);
    }
    VR_Reload_Log(sprintf("%s out of the gun (hand %g, %s)", VR_Reload_Desc(r), xHand, xHow));
    return r;
};

// The magazine `r` held in `xHand` of self seats in the gun in the other hand (which has none): the gun's magazine its
// rounds, the magazine gone from the hand.
void(float xHand, entity r) VR_Reload_SeatMag =
{
    float gunHand = VRGetOtherHand(xHand);
    VRSetEntWeaponClip(self, gunHand, r.vr_ammo_count);
    VRSetEntWeaponFlags(self, gunHand, VRGetEntWeaponFlags(self, gunHand) - (VRGetEntWeaponFlags(self, gunHand) &
        QVR_WPNFLAG_NOMAG));
    VR_Carry_SetHeld(self, xHand, world);
    r.carry_player = world;
    VR_Reload_AmmoChanged(self);
    VR_Reload_Sound(self, VRGetGunChannel(gunHand), "vr/reload_mag_in.wav", 1);
    VR_Reload_Haptic(self, xHand, 0.06, 150, 0.9);
    VR_Reload_Haptic(self, gunHand, 0.1, 60, 0.9);
    VR_Reload_Log(sprintf("%s seated (hand %g): the gun holds %g", VR_Reload_Desc(r), gunHand, r.vr_ammo_count));
    remove(r);
};

// Whether a magazine `r` fits the gun `p` holds in `xHand` by its handle: that gun's, of its ammo.
float(entity p, float xHand, entity r) VR_Reload_MagFits =
{
    return r.vr_ammo_wid && VRGetEntWeapon(p, xHand) == r.vr_ammo_wid &&
           VR_Reload_AmmoFor(p, xHand) == r.vr_ammo_aid;
};

// The magazine `r` in `xHand` of self, at the well of the gun in the other hand: seated if the gun has none; if it has
// one, a bump (the hands meeting at vr_reload_bump_speed or more) knocks that one out along the bump and seats this one.
// TRUE if it left the hand.
float(float xHand, entity r) VR_Reload_MagAtWell =
{
    float gunHand = VRGetOtherHand(xHand);
    if(!VR_Reload_MagIn(self, gunHand))
    {
        VR_Reload_SeatMag(xHand, r);
        return TRUE;
    }
    vector rel = VRGetEntHandVel(self, xHand) - VRGetEntHandVel(self, gunHand);
    if(cvar_hget(cvarh_vr_reload_debug) >= 2)
    {
        dprint(sprintf("reload: a magazine at a full well, the hands meeting at %.2f m/s (a bump: %g)\n", vlen(rel),
            cvar_hget(cvarh_vr_reload_bump_speed)));
    }
    if(vlen(rel) < cvar_hget(cvarh_vr_reload_bump_speed))
    {
        if(!r.vr_ammo_full)
        {
            r.vr_ammo_full = TRUE;
            VR_Reload_Haptic(self, xHand, 0.05, 40, 0.5); // (a magazine in already: bump it out)
        }
        return FALSE;
    }
    VR_Reload_Eject(self, gunHand, VRGetEntHandVel(self, gunHand) * VRMetersToUnits() + normalize(rel) *
        (vlen(rel) * 0.6 * VRMetersToUnits()), -1, "knocked out by a bump");
    VR_Reload_SeatMag(xHand, r);
    return TRUE;
};

// ----------------------------------------------------------------------------
// The pouch

// `xHand` of self (empty, at the pouch, its grip just pressed) takes a round for the gun in the other hand: a shell (a
// taped pair with vr_reload_shell_pairs and two or more left), or its magazine holding min(its size, the reserve), out of
// the reserve. None left: the empty pouch's dull pats.
void(float xHand) VR_Reload_Take =
{
    float aid = VR_Reload_AmmoFor(self, VRGetOtherHand(xHand));
    if(aid < 0)
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
    float w = VRGetEntWeapon(self, VRGetOtherHand(xHand));
    float n = (cvar_hget(cvarh_vr_reload_shell_pairs) && have >= 2) ? 2 : 1;
    if(VR_Reload_UsesMag(w))
    {
        n = min(WeaponIdToBaseClipSize(w), have);
    }
    else
    {
        w = 0;
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
    return VR_Reload_Refund(self, r, xHand);
};

// Every player frame, for `xHand` of self (VR_Reload_HandFrame): the magazines' ejects. The gun in this hand: its B/Y
// pressed lets its magazine drop (vr_reload_eject_button). This hand empty, its grip pressed within vr_reload_pull_reach of
// the other hand's gun's magazine: it holds it (vr_mag_pull; as a foregrip: nothing comes out), and pulled off it hard,
// away from the gun at vr_reload_pull_speed (m/s, the hands' relative speed) with the wrist snapping at
// vr_reload_pull_snap (degrees/s) or more, the magazine comes out into it.
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

    float gunHand = VRGetOtherHand(xHand);
    float grabbing = VR_HandGrabUtil_IsHandGrabbing(self, xHand);
    if(!grabbing || !VR_Reload_MagIn(self, gunHand))
    {
        self.vr_mag_pull = self.vr_mag_pull - (self.vr_mag_pull & bit);
        return;
    }
    vector well = gunHand == cVR_MainHand ? self.loadportpos : self.offloadportpos;
    if(!(self.vr_mag_pull & bit))
    {
        if(VR_HandGrabUtil_StartedHandGrabbing(self, xHand) && VRIsHandEmpty(self, xHand) &&
           !VR_Forcegrab_IsHandBusy(self, xHand) &&
           vlen(VRGetEntHandPos(self, xHand) - well) <= cvar_hget(cvarh_vr_reload_pull_reach))
        {
            self.vr_mag_pull = self.vr_mag_pull | bit;
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
    vector rel = VRGetEntHandVel(self, xHand) - VRGetEntHandVel(self, gunHand);
    vector away = normalize(VRGetEntHandPos(self, xHand) - VRGetEntHandPos(self, gunHand));
    float speed = rel * away; // (pulled off, away from the gun's hand)
    float snap = vlen(VR_Reload_HandAVel(self, xHand) - VR_Reload_HandAVel(self, gunHand)) * 57.2958;
    if(cvar_hget(cvarh_vr_reload_debug) >= 2)
    {
        dprint(sprintf("reload: pulling the magazine at %.2f m/s, the wrist at %.0f deg/s\n", speed, snap));
    }
    if(speed >= cvar_hget(cvarh_vr_reload_pull_speed) && snap >= cvar_hget(cvarh_vr_reload_pull_snap))
    {
        self.vr_mag_pull = self.vr_mag_pull - (self.vr_mag_pull & bit);
        VR_Reload_Eject(self, gunHand, '0 0 0', xHand, sprintf("pulled off at %.2f m/s, %.0f deg/s", speed, snap));
    }
};

// Every player frame, for `xHand` of self (W_Frame, beside the grenade pouch's): at the ammo pouch, a tap as the hand
// arrives (with a round to take for the other hand's gun, or one to put back), and a grip pressed there by an empty
// hand takes one (VR_Reload_Take).
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
    if(!at || !self.ishuman || (!deathmatch && time < readytime))
    {
        return;
    }
    float climbing = VRHasBit(self.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_CLIMBING :
                                                                    QVR_VRBITS0_OFFHAND_CLIMBING);
    float emptyHand = VRIsHandEmpty(self, xHand) &&
                      !VR_Melee_HoldsTorch(self, xHand) && !VR_Forcegrab_IsHandBusy(self, xHand) && !climbing;
    float aid = VR_Reload_AmmoFor(self, VRGetOtherHand(xHand));
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

// The round `r` in `xHand` of self goes into the gun in the other hand: as many of its rounds as fit (a pair with one
// place left: one, a single shell left in the hand). TRUE if it is used up (gone from the hand). A full gun takes
// nothing: a dull tap (once, until it leaves the port).
float(float xHand, entity r) VR_Reload_Load =
{
    float gunHand = VRGetOtherHand(xHand);
    float w = VRGetEntWeapon(self, gunHand);
    float clip = VRGetEntWeaponClip(self, gunHand);
    float room = WeaponIdToBaseClipSize(w) - clip;
    if(room <= 0)
    {
        if(!r.vr_ammo_full)
        {
            r.vr_ammo_full = TRUE;
            VR_Reload_Haptic(self, xHand, 0.06, 40, 0.6);
            VR_Reload_Log("the gun is full");
        }
        return FALSE;
    }
    float n = min(r.vr_ammo_count, room);
    VRSetEntWeaponClip(self, gunHand, clip + n);
    r.vr_ammo_count = r.vr_ammo_count - n;
    VR_Reload_AmmoChanged(self);
    VR_Reload_Sound(self, VRGetGunChannel(gunHand), "vr/reload_shell_in.wav", 1);
    VR_Reload_Haptic(self, xHand, 0.05, 160, 0.8);   // the shell pushed home
    VR_Reload_Haptic(self, gunHand, 0.08, 70, 0.7);  // the gun taking it
    VR_Reload_Log(sprintf("%g into the gun (hand %g): its magazine %g of %g, %g in the reserve", n, gunHand, clip + n,
        WeaponIdToBaseClipSize(w), VR_AmmoUtil_GetAmmoCountByAmmoId(self, r.vr_ammo_aid)));
    if(r.vr_ammo_count <= 0)
    {
        VR_Carry_SetHeld(self, xHand, world);
        r.carry_player = world;
        remove(r);
        return TRUE;
    }
    VR_Reload_SetModel(r); // (a pair's other shell, still in the fingers)
    return FALSE;
};

// While `xHand` of self holds the round `r` (VR_Carry_HandFrame, gripping): at the loading port of the gun in the other
// hand (within vr_reload_port_leniency of it), it goes in. TRUE if it is gone from the hand.
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
    float d = vlen(r.origin - port);
    float leniency = cvar_hget(r.vr_ammo_wid ? cvarh_vr_reload_mag_leniency : cvarh_vr_reload_port_leniency);
    if(cvar_hget(cvarh_vr_reload_debug) >= 2)
    {
        dprint(sprintf("reload: a held round %.1f units from the port (leniency %g)\n", d, leniency));
    }
    if(d > leniency)
    {
        r.vr_ammo_full = FALSE;
        r.vr_ammo_fresh = FALSE;
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
