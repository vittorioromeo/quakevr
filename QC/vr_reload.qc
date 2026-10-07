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

.float vr_ammo_aid;      // a round: the ammo it is (AID_*)
.float vr_ammo_count;    // and how many (a taped pair: 2)
.float vr_ammo_born;     // when it was last in a hand or flying (its time lying about runs from here)
.float vr_ammo_tink;     // its last knock's sound
.float vr_ammo_full;     // the full gun's tap given (until it leaves the port)
.float vr_reload_at;     // a player's hands at the ammo pouch last frame (1 off, 2 main)
.float vr_reload_tried;  // and those whose grip press there was used

string VR_RELOAD_SHELL = "progs/vr_shell_live.mdl";
string VR_RELOAD_PAIR = "progs/vr_shell_pair.mdl";
string VR_RELOAD_CLASS = "vr_ammo_shell";

void() VR_Reload_Precache =
{
    precache_model(VR_RELOAD_SHELL);
    precache_model(VR_RELOAD_PAIR);
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

// Whether `xWeapon` loads by hand (immersive reloading): no holster, button or flick reload for it.
float(float xWeapon) VR_Reload_ManualFor =
{
    return VR_Reload_Immersive() && xWeapon == WID_SHOTGUN;
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
    return e.classname == VR_RELOAD_CLASS;
};

// Whether `xHand` of `p` is at the ammo pouch (immersive reloading).
float(entity p, float xHand) VR_Reload_AtPouch =
{
    return VR_Reload_Immersive() &&
           (xHand == cVR_MainHand ? p.mainhand_hotspot : p.offhand_hotspot) == QVR_HS_AMMO_POUCH;
};

// A round's look: a single shell, or the taped pair.
void(entity r) VR_Reload_SetModel =
{
    setmodel(r, r.vr_ammo_count >= 2 ? VR_RELOAD_PAIR : VR_RELOAD_SHELL);
    vector lo = modelbounds(r, 0);
    vector hi = modelbounds(r, 1);
    setsize(r, lo * 1.25, hi * 1.25); // (its prop slot's Size: as the spent shells are drawn)
};

// Back into the reserve of `p` (a hand let go of it at the pouch, or took it as a pickup): TRUE if it fit (and it is
// gone), FALSE if the reserve is full (it stays).
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
    VR_Reload_Log(sprintf("%g %s back in the pouch (hand %g): %g left", r.vr_ammo_count,
        r.vr_ammo_count >= 2 ? "shells" : "shell", xHand, VR_AmmoUtil_GetAmmoCountByAmmoId(p, r.vr_ammo_aid)));
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

// Box3D's hardest hit of a frame: the spent shells' tink, at most every 0.12 s.
void(float xSpeed) VR_Reload_Impact =
{
    if(xSpeed < 0.6 || time < self.vr_ammo_tink + 0.12)
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
    VR_Reload_Log(sprintf("%g %s lost (%s)", r.vr_ammo_count, r.vr_ammo_count >= 2 ? "shells" : "shell", xWhy));
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

// Room for one more under vr_reload_loose_max: the rounds lying about longest go first (never one in a hand).
void() VR_Reload_MakeRoom =
{
    float most = max(1, cvar_hget(cvarh_vr_reload_loose_max));
    float n = 0;
    entity e = find(world, classname, VR_RELOAD_CLASS);
    while(e)
    {
        if(e.carry_player == world)
        {
            n++;
        }
        e = find(e, classname, VR_RELOAD_CLASS);
    }
    while(n >= most)
    {
        entity oldest = world;
        e = find(world, classname, VR_RELOAD_CLASS);
        while(e)
        {
            if(e.carry_player == world && (oldest == world || e.vr_ammo_born < oldest.vr_ammo_born))
            {
                oldest = e;
            }
            e = find(e, classname, VR_RELOAD_CLASS);
        }
        if(oldest == world)
        {
            return;
        }
        VR_Reload_Lose(oldest, "too many lying about");
        n--;
    }
};

// A new round of `xCount` of ammo `xAid` at `xOrg`: a carried thing (taken, thrown, force grabbed; a rigid body loose).
entity(float xAid, float xCount, vector xOrg, vector xAngles) VR_Reload_MakeRound =
{
    VR_Reload_MakeRoom();
    entity r = spawn();
    r.classname = VR_RELOAD_CLASS;
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
    r.throwhit = QVR_THROWHIT_NEVER_HIT; // (a shell thrown hurts nothing)
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
// The pouch

// `xHand` of self (empty, at the pouch, its grip just pressed) takes a round for the gun in the other hand: a shell (a
// taped pair with vr_reload_shell_pairs and two or more left), out of the reserve. None left: the empty pouch's dull pats.
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
    float n = (cvar_hget(cvarh_vr_reload_shell_pairs) && have >= 2) ? 2 : 1;
    entity r = VR_Reload_MakeRound(aid, n, VRGetEntHandPos(self, xHand), VRGetEntHandRot(self, xHand));
    VR_AmmoUtil_DelAmmoCountByAmmoId(self, aid, n);
    VR_Reload_AmmoChanged(self);
    vr_carry_quiet = TRUE; // (the pouch's own sound)
    VR_Carry_Start(self, xHand, r);
    vr_carry_quiet = FALSE;
    VR_Reload_Sound(self, VRGetHandChannel(xHand), "vr/reload_pouch.wav", 0.9);
    VR_Reload_Haptic(self, xHand, 0.07, 120, 0.7);
    VR_Reload_Log(sprintf("%s taken from the pouch by hand %g: %g left", n >= 2 ? "a taped pair" : "a shell", xHand,
        VR_AmmoUtil_GetAmmoCountByAmmoId(self, aid)));
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
    vector port = gunHand == cVR_MainHand ? self.loadportpos : self.offloadportpos;
    float d = vlen(r.origin - port);
    if(cvar_hget(cvarh_vr_reload_debug) >= 2)
    {
        dprint(sprintf("reload: a held round %.1f units from the port (leniency %g)\n", d,
            cvar_hget(cvarh_vr_reload_port_leniency)));
    }
    if(d > cvar_hget(cvarh_vr_reload_port_leniency))
    {
        r.vr_ammo_full = FALSE;
        return FALSE;
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
