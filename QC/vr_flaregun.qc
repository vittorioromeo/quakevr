//
//
//
// ----------------------------------------------------------------------------
// The flare gun (WID_FLAREGUN; docs/vr-port/ROUND22.md, "Flare gun: the weapon")
// ----------------------------------------------------------------------------
//
// Quake VR's own disposable double-barrelled flare gun (the art: Misc/quakevr/models_src/flaregun; progs/v_flaregun.mdl,
// skins 0 loaded, 1 the top barrel fired, 2 both; the flare progs/vr_flare.mdl). Two flares, one a barrel (the top's
// first), a shot a pull of the trigger; then it is spent (its clip, its only ammo: AID_NONE; nothing refills it), and is
// thrown away (a spent one lying about: no loot beam, no force grab, as a spent enemy gun: vr_wpnforcegrab.qc).
// The skin follows its clip everywhere it is drawn: in a hand and a holster the engine's (the clip's stats), lying about
// and in an armed grunt's hand QC's (VR_FlareGun_Skin).
//
// A flare (classname vr_flare; its skin tells the client what light it gives: vr_emissive.cpp):
// - flies in a shallow arc (vr_flaregun_speed, vr_flaregun_gravity), a small red light (skin 0);
// - into a monster, a player, a corpse or a crate: vr_dmg_flaregun (a monster's 6) and it is set on fire (Quake VR's
//   burning: vr_burning.qc VR_Burn_Ignite, as a lava nail); an explosive box goes off (vr_flare_burn 0: the damage
//   only); the flare drops at its feet and burns there;
// - into a wall, the floor or the ceiling: sticks there (lying flat on a floor), burning red and flickering with sparks
//   and smoke for vr_flare_stick_time s (skin 1); a door or a lift: it drops off and lies burning below;
// - fired up into the air (at least vr_flare_air_pitch degrees up): at the top of its climb it drifts down slowly
//   (vr_flare_air_fall units a second, swaying), lighting a wide area for vr_flare_air_time s (skin 2); landing, it lies
//   burning out the rest of that time;
// - burns out over its last 1.5 s (its alpha: the client's light fades with it), then is gone; the sky swallows one.
// The client lights the nearest vr_flare_lights flares (air flares first), vr_flare_light their size, vr_flare_sparks
// their sparks and smoke (vr_emissive.cpp).
//
// Weapon settings slot 37 (vr_weapons.inc: its grip, sights, holsters, recoil, flash). weapon_flaregun lies ready to be
// picked up; crates about the maps may have one on top (vr_crate_flaregun: the engine's plan, vr_crates.cpp). Armed
// grunts may carry one as a second weapon (vr_agrunt_flaregun: vr_armedgrunt.qc), firing it through W_FireFlareGun,
// then throwing it away and taking their own.
// Tests: vr_flare_test (vr_flaregun_test.qc; Debug > Tests > Flare Gun); developer 1 prints "flaregun:" lines.

float VR_FLARE_FLYING = 0;  // in flight
float VR_FLARE_STUCK = 1;   // stuck in a wall, lying on the floor
float VR_FLARE_AIR = 2;     // drifting down from the top of its climb
float VR_FLARE_FALLING = 3; // dropping off a monster it struck, a door (it lies down where it lands)
float VR_FLARE_FADE = 1.5;  // s its burning out takes
float VR_FLAREGUN_BARREL_GAP = 2.3; // model units from the top bore's axis down to the bottom's (the art's README)

.float vr_fg_rearm;     // the trigger pulled since its last shot (or dry click): it fires again only once let go
.float vr_fg_rearm2;
.float vr_fg_smoke;     // when the wisp of smoke after its last shot rises (0 none)
.float vr_fg_smoke2;
.float vr_fg_barrel;    // the barrel of the last shot (0 the top, 1 the bottom): where its smoke rises
.float vr_fg_barrel2;

.float vr_flare_state;  // a flare's: VR_FLARE_*
.float vr_flare_end;    // ... when it burns out
.float vr_flare_up;     // ... fired up enough to drift down from the top of its climb
.float vr_flare_born;   // ... when it was fired
.float vr_flare_struck; // ... it has struck something that takes damage (once only)
.float vr_flare_phase;  // ... its sway's phase (drifting)

float vr_fg_shots;      // flares fired by players (tests)
float vr_fg_dry;        // dry clicks (tests)
float vr_fg_ignited;    // things set on fire by flares (tests)
float vr_fg_hits;       // flares into things that take damage (tests)
float vr_fg_booms;      // explosive boxes set off by flares (tests)
entity vr_fg_lastflare; // a player's last flare (tests)
vector vr_fg_lastfrom;  // where it left from
vector vr_fg_lastfwd;   // the shot line then
float vr_fg_lastbarrel; // and from which barrel (0 the top, 1 the bottom)
entity vr_fg_out;       // the last flare burnt out (tests)
float vr_fg_out_at;     // ... when
#ifdef QVR_TESTS
void() VR_FlareGun_TestFrame; // (vr_flaregun_test.qc)
#endif

void() VR_FlareGun_Precache =
{
    precache_model("progs/v_flaregun.mdl");
    precache_model("progs/vr_flare.mdl");
    precache_sound("weapons/grenade.wav");
    precache_sound("weapons/sgun1.wav");
    precache_sound("ambience/fire1.wav");
    precache_sound("gunclick.wav");
    precache_sound("misc/null.wav");
};

// Its flares: one a barrel.
float() VR_FlareGun_Barrels = { return 2; };

// Its skin with `xClip` flares left: 0 loaded, 1 the top barrel fired, 2 both.
float(float xClip) VR_FlareGun_Skin = { return max(0, min(2, VR_FlareGun_Barrels() - xClip)); };

// updateAmmoCounter: its counter is its flares left (its only ammo).
float(float xHand) VR_FlareGun_Counter =
{
    if(VRGetWeapon(xHand) != WID_FLAREGUN)
    {
        return FALSE;
    }
    VRSetAmmoCounter(xHand, VRGetWeaponClip(xHand));
    VRSetWeaponClipSize(xHand, 0);
    return TRUE;
};

// The muzzle of barrel `xBarrel` (0 the top: the weapon's muzzle anchor; 1 the bottom: VR_FLAREGUN_BARREL_GAP model units
// below it, at the drawn gun's scale) of the flare gun in `xHand` of self (a player). v_forward, v_right, v_up: its fire
// rotation's.
vector(float xHand, float xBarrel) VR_FlareGun_Muzzle =
{
    makevectors(VRGetWeaponFireRot(xHand));
    vector m = VRGetMuzzlePos(xHand);
    if(xBarrel > 0)
    {
        float k = cvar("vr_wofs_scale_38"); // (its slot 37's Scale: vr_weapons.inc)
        m = m - v_up * (VR_FLAREGUN_BARREL_GAP * (k > 0 ? k : 0.45));
    }
    return m;
};

// ----------------------------------------------------------------------------
// The flare
// ----------------------------------------------------------------------------

void(entity f, string xWhy) VR_Flare_Log =
{
    if(cvar("developer"))
    {
        dprint(sprintf("flaregun: flare %s at %v (state %g, burns %.1f s more)\n", xWhy, f.origin,
            f.vr_flare_state, max(0, f.vr_flare_end - time)));
    }
};

// Burns out: fades over VR_FLARE_FADE (the client's light with its alpha), then gone.
void() VR_Flare_Think;
void(entity f) VR_Flare_Out =
{
    sound(f, CHAN_BODY, "misc/null.wav", 1, ATTN_IDLE);
    VR_Flare_Log(f, "burnt out");
    vr_fg_out = f;
    vr_fg_out_at = time;
    remove(f);
};

// Its burning hiss (a loop on its body channel: stopped as it goes out).
void(entity f) VR_Flare_Hiss =
{
    sound(f, CHAN_BODY, "ambience/fire1.wav", f.vr_flare_state == VR_FLARE_AIR ? 0.5 : 0.35, ATTN_IDLE);
};

// Into a wall, the ceiling or the floor at its place (the surface `n` its normal; `dir` its flight): it stays there,
// burning vr_flare_stick_time s from now (`xKeepEnd`: its end as it was, a drifting flare landing).
void(entity f, vector n, vector dir, float xKeepEnd) VR_Flare_Stick =
{
    f.vr_flare_state = VR_FLARE_STUCK;
    f.movetype = MOVETYPE_NONE;
    f.solid = SOLID_NOT;
    f.touch = SUB_Null;
    f.velocity = '0 0 0';
    f.avelocity = '0 0 0';
    f.skin = VR_FLARE_STUCK;
    if(n_z > 0.7)
    {
        // On a floor: lying on its side, its burning cap the way it flew.
        vector a = vectoangles(dir);
        f.angles = '0 0 0';
        f.angles_y = a_y;
        setorigin(f, f.origin + n * 0.45);
    }
    else
    {
        // In a wall or the ceiling: its tail in the hole it made, its burning cap out (back along its flight, a little
        // off the wall's normal).
        f.angles = vectoangles(normalize(n * 0.6 - dir * 0.4));
        setorigin(f, f.origin + n * 0.6);
    }
    if(!xKeepEnd)
    {
        f.vr_flare_end = time + max(0.5, cvar_hget(cvarh_vr_flare_stick_time));
    }
    VR_Flare_Hiss(f);
    VR_Flare_Log(f, n_z > 0.7 ? "lies on the floor" : "stuck in a wall");
};

// Off something it can't stick in (a monster it struck, a door): it drops down and lies where it lands.
void(entity f, entity xFrom) VR_Flare_Drop =
{
    f.vr_flare_state = VR_FLARE_FALLING;
    f.movetype = MOVETYPE_TOSS;
    f.gravity = 1;
    f.owner = xFrom; // (it passes through what it dropped off)
    vector v = f.velocity * -0.08;
    v_z = 40;
    f.velocity = v;
    f.avelocity = randomVec(200);
    if(f.vr_flare_end < time + 1)
    {
        f.vr_flare_end = time + max(0.5, cvar_hget(cvarh_vr_flare_stick_time));
    }
};

// A hit on `e` (it takes damage): vr_dmg_flaregun, and set on fire (vr_flare_burn); an explosive box goes off.
void(entity f, entity e) VR_Flare_Strike =
{
    f.vr_flare_struck = TRUE;
    vr_fg_hits++;
    float dmg = VR_WpnDmg(f.owner, cvarh_vr_dmg_flaregun, 6);
    vector dir = normalize(f.velocity);
    float burn = cvar_hget(cvarh_vr_flare_burn) != 0;
    if(burn && VR_Burn_Can(e))
    {
        VR_Burn_Ignite(e, f.owner, f.origin, dir, VR_BURN_NAIL); // (before its damage: a piece it lights burns, not bursts)
        if(e.burn_fire != world)
        {
            vr_fg_ignited++;
            VR_Flare_Log(f, sprintf("set %s on fire", e.classname));
        }
    }
    if(burn && (e.th_die == barrel_explode || e.classname == "misc_explobox" || e.classname == "misc_explobox2"))
    {
        dmg = max(dmg, e.health + 1); // (its burning flare in an explosive box: it goes off)
        vr_fg_booms++;
        VR_Flare_Log(f, "set off an explosive box");
    }
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("flaregun: flare hit %s: %g (health %g)\n", e.classname, dmg, e.health));
    }
    entity os = self;
    self = f; // (the inflictor's touch)
    if(T_Damage(e, f, f.owner, dmg))
    {
        spawn_touchblood(dmg);
    }
    self = os;
};

void() VR_Flare_Touch =
{
    if(VR_OwnerSpared(other))
    {
        return;
    }
    if(other.solid == SOLID_TRIGGER || self.vr_flare_state == VR_FLARE_STUCK)
    {
        return;
    }
    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        VR_Flare_Log(self, "into the sky");
        sound(self, CHAN_BODY, "misc/null.wav", 1, ATTN_IDLE);
        remove(self);
        return;
    }
    vector dir = normalize(self.velocity);
    if(dir == '0 0 0')
    {
        dir = '0 0 -1';
    }
    if(other != world && other.takedamage && !self.vr_flare_struck && self.vr_flare_state == VR_FLARE_FLYING)
    {
        VR_Flare_Strike(self, other);
        if(self)
        {
            VR_Flare_Drop(self, other);
        }
        return;
    }
    if(other != world)
    {
        // A door, a lift, a monster it already struck, a drifting flare's: it drops off and lies below.
        if(self.vr_flare_state != VR_FLARE_FALLING || other.solid == SOLID_BSP)
        {
            VR_Flare_Drop(self, other);
        }
        return;
    }
    // The world: the surface it struck (traced along its flight to its face).
    traceline(self.origin - dir * 8, self.origin + dir * 8, TRUE, self);
    vector n = trace_fraction < 1 ? trace_plane_normal : -dir;
    if(trace_fraction < 1)
    {
        if(pointcontents(trace_endpos + dir) == CONTENT_SKY)
        {
            VR_Flare_Log(self, "into the sky");
            sound(self, CHAN_BODY, "misc/null.wav", 1, ATTN_IDLE);
            remove(self);
            return;
        }
        setorigin(self, trace_endpos);
    }
    // (A drifting or dropping flare landing keeps its time; one flying in: a stuck flare's from now.)
    VR_Flare_Stick(self, n, dir, self.vr_flare_state != VR_FLARE_FLYING);
};

// At the top of its climb (fired up): it drifts down slowly, swaying, lighting a wide area.
void(entity f) VR_Flare_Drift =
{
    f.vr_flare_state = VR_FLARE_AIR;
    f.skin = VR_FLARE_AIR;
    f.movetype = MOVETYPE_FLY;
    vector v = f.velocity * 0.05;
    v_z = -max(1, cvar_hget(cvarh_vr_flare_air_fall));
    f.velocity = v;
    f.angles = '90 0 0'; // (its cap down: it hangs under its chute)
    f.angles_y = random() * 360;
    f.vr_flare_end = time + max(0.5, cvar_hget(cvarh_vr_flare_air_time));
    VR_Flare_Hiss(f);
    VR_Flare_Log(f, "drifts down");
};

void() VR_Flare_Think =
{
    self.nextthink = time + 0.05;
    if(time >= self.vr_flare_end)
    {
        VR_Flare_Out(self);
        return;
    }
    // Burning out: its alpha fades (the client's light and sparks with it).
    float left = self.vr_flare_end - time;
    self.alpha = left < VR_FLARE_FADE ? max(0.05, left / VR_FLARE_FADE) : 1;
    if(self.vr_flare_state == VR_FLARE_FLYING)
    {
        if(self.velocity != '0 0 0')
        {
            self.angles = vectoangles(self.velocity);
        }
        if(self.vr_flare_up && self.velocity_z <= 0 && time - self.vr_flare_born > 0.2)
        {
            VR_Flare_Drift(self);
        }
    }
    else if(self.vr_flare_state == VR_FLARE_AIR)
    {
        // Swaying as it sinks (its chute in the air).
        float t = time - self.vr_flare_born;
        float fall = max(1, cvar_hget(cvarh_vr_flare_air_fall));
        self.velocity_x = self.velocity_x * 0.9 + sin(t * 1.3 + self.vr_flare_phase) * fall * 0.015; // (about 0.15 of its fall at most)
        self.velocity_y = self.velocity_y * 0.9 + cos(t * 1.1 + self.vr_flare_phase) * fall * 0.015;
        self.velocity_z = -fall;
    }
};

// A flare from `xFrom` going `xDir` at `xSpeed` plus `xCarried`, fired by self (a player, a monster).
entity(vector xFrom, vector xDir, float xSpeed, vector xCarried) VR_Flare_Launch =
{
    entity f = spawn();
    f.classname = "vr_flare";
    f.owner = self;
    f.movetype = MOVETYPE_TOSS;
    f.solid = SOLID_BBOX;
    f.gravity = max(0.01, cvar_hget(cvarh_vr_flaregun_gravity));
    setmodel(f, "progs/vr_flare.mdl");
    setsize(f, '0 0 0', '0 0 0');
    setorigin(f, xFrom);
    f.velocity = xDir * xSpeed + xCarried;
    f.angles = vectoangles(f.velocity);
    f.touch = VR_Flare_Touch;
    f.think = VR_Flare_Think;
    f.nextthink = time + 0.05;
    f.vr_flare_state = VR_FLARE_FLYING;
    f.vr_flare_born = time;
    f.vr_flare_phase = random() * 6.28;
    f.vr_flare_end = time + 10; // (still flying then: lost somewhere, out)
    f.vr_flare_up = xDir_z >= sin(max(0, cvar_hget(cvarh_vr_flare_air_pitch)) * 0.0174533);
    f.skin = VR_FLARE_FLYING;
    VR_Flare_Hiss(f);
    return f;
};

// ----------------------------------------------------------------------------
// The gun
// ----------------------------------------------------------------------------

// Its shot from `xHand` of self (a player, or an armed grunt: vr_armedgrunt.qc, whose clip it keeps itself): a flare
// from the next loaded barrel (the top's first), its thump, flash, kick, buzz and smoke. A player's clip goes down by one.
void(float xHand) W_FireFlareGun =
{
    float clip = VRGetWeaponClip(xHand);
    if(clip < 1)
    {
        return;
    }
    float player = (self.flags & FL_CLIENT) != 0;
    float barrel = clip >= VR_FlareGun_Barrels() ? 0 : 1; // (2 left: the top's; 1: the bottom's)
    vector from;
    if(player)
    {
        from = VR_FlareGun_Muzzle(xHand, barrel); // (makevectors: its fire rotation's)
    }
    else
    {
        makevectors(VRGetWeaponFireRot(xHand));
        from = VRGetMuzzlePos(xHand);
    }
    vector dir = v_forward;
    float speed = max(100, cvar_hget(cvarh_vr_flaregun_speed));
    if(!player && self.enemy)
    {
        // A monster lobs it a little over the line to its target (its flight's drop at that range).
        float d = vlen(self.enemy.origin - from);
        float g = max(0.01, cvar_hget(cvarh_vr_flaregun_gravity)) * cvar("sv_gravity");
        dir_z = dir_z + min(0.2, g * d / (2 * speed * speed));
        dir = normalize(dir);
    }
    float vol = max(0, min(4, cvar_hget(cvarh_vr_flaregun_volume)));
    // Its shot: the grenade launcher's thump pitched up (a light pistol's charge) and the rocket's whoosh, quieter.
    sound_pitch(self, VRGetGunChannel(xHand), "weapons/grenade.wav", vol, ATTN_NORM, 122 + crandom() * 4);
    sound_pitch(self, CHAN_AUTO, "weapons/sgun1.wav", vol * 0.35, ATTN_NORM, 140 + crandom() * 6);
    vector carried = player ? vrProjectileVelocity(xHand, 1.0) : '0 0 0';
    entity f = VR_Flare_Launch(from, dir, speed, carried);
    if(player)
    {
        VRSetWeaponClip(xHand, clip - 1);
        updateAmmoCounters();
        VR_ShotLaunched(f, xHand); // behind what the muzzle is inside (VR_ShotPlace)
    }

    self.effects = self.effects | EF_MUZZLEFLASH;
    self.punchangle_x = -2;
    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp; slot 37's Recoil, Flash)
    VRGunHaptic(xHand, 0.1, 90, 1);
    particle2(from, '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 2);

    if(player)
    {
        if(xHand == cVR_OffHand)
        {
            self.vr_fg_smoke2 = time + 0.3;
            self.vr_fg_barrel2 = barrel;
        }
        else
        {
            self.vr_fg_smoke = time + 0.3;
            self.vr_fg_barrel = barrel;
        }
        vr_fg_shots++;
        vr_fg_lastflare = f;
        vr_fg_lastfrom = f.origin;
        vr_fg_lastfwd = VRGetWeaponFireRotFwd(xHand);
        vr_fg_lastbarrel = barrel;
    }
    if(cvar_hget(cvarh_vr_debug_shots) || cvar("developer"))
    {
        dprint(sprintf("flaregun: hand %g fired its %s barrel from %v at %v, %g left\n", xHand, barrel ? "bottom" : "top",
            from, f.velocity, player ? VRGetWeaponClip(xHand) : clip - 1));
    }
};

// W_AttackImpl's: the trigger pulled (time past its cooldown). A new pull fires a flare (or clicks dry once, spent), then
// nothing until the trigger is let go (VR_FlareGun_PlayerFrame re-arms it).
void(float xHand, float xAttackFinished, float* xOutAttackFinished) VR_FlareGun_Attack =
{
    float rearm = VR_IMPL_SELECT(xHand, self.vr_fg_rearm2, self.vr_fg_rearm);
    if(rearm)
    {
        return;
    }
    if(xHand == cVR_OffHand) { self.vr_fg_rearm2 = TRUE; } else { self.vr_fg_rearm = TRUE; }
    float cool = max(0.05, cvar_hget(cvarh_vr_flaregun_cooldown));
    if(VRGetWeaponClip(xHand) < 1)
    {
        // Spent: its hammer falls on nothing (throw it away).
        sound(self, VRGetHandChannel(xHand), "gunclick.wav", 1, ATTN_NORM);
        VRGunHaptic(xHand, 0.02, 200, 0.3);
        *xOutAttackFinished = time + cool;
        vr_fg_dry++;
        if(cvar_hget(cvarh_vr_debug_shots) || cvar("developer"))
        {
            dprint(sprintf("flaregun: dry click, hand %g (spent)\n", xHand));
        }
        return;
    }
    self.show_hostile = time + 1;
    VR_Stealth_Shot(self, WID_FLAREGUN); // QVR: heard by its loudness (vr_stealth.qc)
    RuneApplyBlackNoise(self);
    SuperDamageSound();
    W_FireFlareGun(xHand);
    *xOutAttackFinished = time + RuneApplyHell(cool, self);
};

void(float xHand) VR_FlareGun_HandFrame =
{
    if(VRGetWeapon(xHand) != WID_FLAREGUN)
    {
        if(xHand == cVR_OffHand) { self.vr_fg_rearm2 = FALSE; self.vr_fg_smoke2 = 0; }
        else                     { self.vr_fg_rearm = FALSE; self.vr_fg_smoke = 0; }
        return;
    }
    if(!VRGetFireButtonPressed(xHand))
    {
        if(xHand == cVR_OffHand) { self.vr_fg_rearm2 = FALSE; } else { self.vr_fg_rearm = FALSE; }
    }
    // The wisp of smoke from the barrel just fired, a moment after.
    float at = VR_IMPL_SELECT(xHand, self.vr_fg_smoke2, self.vr_fg_smoke);
    if(at > 0 && time >= at)
    {
        float b = VR_IMPL_SELECT(xHand, self.vr_fg_barrel2, self.vr_fg_barrel);
        particle2(VR_FlareGun_Muzzle(xHand, b), '0 0 0', QVR_PARTICLE_PRESET_SMOKE, 3);
        if(xHand == cVR_OffHand) { self.vr_fg_smoke2 = 0; } else { self.vr_fg_smoke = 0; }
    }
};

// Every player frame (client.qc PlayerPreThink, self the player).
void() VR_FlareGun_PlayerFrame =
{
    VR_FlareGun_HandFrame(cVR_MainHand);
    VR_FlareGun_HandFrame(cVR_OffHand);
};

// A flare gun at `xOrg` (loaded with `xClip` flares), moving at `xVel` ('0 0 0': dropped where it is), turned `xAngles`
// ('0 0 0': any way): a weapon lying ready to be picked up (a thrown weapon at rest: it falls and settles). `xFrom`: what
// it comes from (a crate, a monster; world: none).
entity(entity xFrom, vector xOrg, vector xAngles, vector xVel, float xClip) VR_FlareGun_Spawn =
{
    if(xAngles == '0 0 0')
    {
        xAngles = randomVec(180);
    }
    return CreateThrownWeapon(xFrom, WID_FLAREGUN, 0 /* weaponflags */, WeaponInst_New(xClip),
        xOrg, xAngles,
        xVel, xVel == '0 0 0' ? '0 0 0' : randomVec(60),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(WID_FLAREGUN));
};

// A flare gun lying on crate `xCrate`, number `i` of the engine's plan, if the plan puts one there (vr_crate_flaregun;
// the top of a stack with no crowbar on it): the engine lays it on its side on the top at the plan's yaw and place
// (vr_crates.cpp putCrowbar, the crowbars' fitting), just above it; it settles there by physics.
void(entity xCrate, float i) VR_FlareGun_OnCrate =
{
    if(cratecrowbar(i) != 2) // (1: a crowbar there, vr_crowbar.qc)
    {
        return;
    }
    entity g = VR_FlareGun_Spawn(xCrate, xCrate.origin, '0 0 90', '0 0 0', VR_FlareGun_Barrels());
    if(!g || !cratecrowbarput(g, i))
    {
        if(g) { remove(g); }
        return;
    }
    if(cvar("developer"))
    {
        dprint(sprintf("flaregun: one on crate %g at %v\n", i, g.origin));
    }
};

/*QUAKED weapon_flaregun (0 .5 .8) (-16 -16 0) (16 16 32)
Quake VR's flare gun: disposable, double-barrelled, two flares (no ammo of its own: spent, throw it away). Its flares set
what they hit on fire, stick burning red in walls, or drift down lighting a wide area fired into the air. It lies where
it is placed (it falls and settles); "angle" or "angles": how it lies (none: any way).
*/
void() weapon_flaregun =
{
    VR_FlareGun_Spawn(self, self.origin, self.angles, '0 0 0', VR_FlareGun_Barrels());
    remove(self);
};
