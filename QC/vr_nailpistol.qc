//
//
//
// ----------------------------------------------------------------------------
// The nail pistol (WID_NAILPISTOL; docs/vr-port/ROUND22.md, "The nail pistol")
// ----------------------------------------------------------------------------
//
// Quake VR's own sidearm (the art: Misc/quakevr/models_src/nailpistol; progs/v_nailpistol.mdl, frame 0 at rest, frame 1
// its copper slide back): one-handed, SEMI-AUTOMATIC (a shot a pull of the trigger, at most one each
// vr_nailpistol_cooldown s), fast precise nails (vr_nailpistol_speed, vr_nailpistol_spread; vr_dmg_nailpistol each: a
// grunt dies to two in the head or three in the body) from a vr_nailpistol_mag round magazine in its grip, a nail of your
// nails a round. Immersive reloading takes it as the other magazine guns (vr_reload.qc: B/Y ejects it, the pouch gives a
// new one, slapped up into the grip; progs/vr_mag_npistol.mdl, Misc/quakevr/make_nailpistol_mags.py); the other reload
// modes as any gun. Its slide snaps back each shot and runs home, and stays back on an empty gun. Weapon settings slot 36
// (vr_weapons.inc: its grip, sights, holsters, recoil, flash). weapon_nailpistol lies ready to be picked up.
// Armed grunts carry it as a gun one hand holds (a pair dual wielded: vr_armedgrunt.qc), firing it through
// W_FireNailPistol.

.float vr_np_rearm;     // the trigger pulled since its last shot (or dry click): it fires again only once let go
.float vr_np_rearm2;
.float vr_np_slide;     // until when its slide stays back after a shot
.float vr_np_slide2;
.float vr_np_smoke;     // when the wisp of smoke after its last shot rises (0 none)
.float vr_np_smoke2;
.float vr_np_heat;      // its recent shots (more smoke after a quick string): decays a shot a quarter second
.float vr_np_heat2;
.float vr_np_heat_at;
.float vr_np_heat_at2;

float vr_np_shots;      // shots fired (tests)
float vr_np_dry;        // dry clicks (tests)
float vr_np_hits;       // nails into vr_np_target (tests)
float vr_np_lastfactor; // the last nail's hit part's factor (tests)
entity vr_np_target;
#ifdef QVR_TESTS
void() VR_NailPistol_TestFrame; // (vr_nailpistol_test.qc)
#endif

void() VR_NailPistol_Precache =
{
    precache_model("progs/v_nailpistol.mdl");
    precache_model("progs/vr_mag_npistol.mdl");
    precache_sound("vr/nailpistol_shot.wav");
    precache_sound("weapons/spike2.wav");
    precache_sound("gunclick.wav");
};

float() VR_NailPistol_MagSize =
{
    return max(1, rint(cvar_hget(cvarh_vr_nailpistol_mag)));
};

// The weapon cycle's next weapon id from `xWeapon` (+1 or -1): the ids in order, the nail pistol between the super shotgun
// and the nailgun (the nails' slot: weapons.qc W_ChangeWeapon's impulse 4).
float(float xWeapon, float xDir) VR_NailPistol_CycleStep =
{
    if(xDir > 0)
    {
        if(xWeapon == WID_SUPER_SHOTGUN) { return WID_NAILPISTOL; }
        if(xWeapon == WID_NAILPISTOL)    { return WID_NAILGUN; }
    }
    else
    {
        if(xWeapon == WID_NAILGUN)       { return WID_NAILPISTOL; }
        if(xWeapon == WID_NAILPISTOL)    { return WID_SUPER_SHOTGUN; }
    }
    return xWeapon + xDir;
};

// Its nail's hit (spike_touch's, with its own damage: vr_dmg_nailpistol a player's, 9 a monster's).
void() VR_NailPistol_SpikeTouch =
{
    if(VR_OwnerSpared(other))
        return;
    if(other.solid == SOLID_TRIGGER)
        return;
    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }
    float dmg = VR_WpnDmg(self.owner, cvarh_vr_dmg_nailpistol, 9);
    VR_Proj_Begin(QVR_PROJ_BOLT, dmg);
    makeforward_mdl(self.angles);
    float bleed = FALSE;
    if(other.takedamage)
    {
        float adjDmg = dmg * vr_proj_factor; // (its part: VR_Proj_Begin)
        vr_np_lastfactor = vr_proj_factor;
        if(vr_np_target && other == vr_np_target) { vr_np_hits++; }
        if(cvar_hget(cvarh_vr_debug_shots))
        {
            dprint(sprintf("nailpistol: nail hit %s at %v, part x%.2f: %g (health %g)\n", other.classname, vr_proj_at,
                vr_proj_factor, adjDmg, other.health));
        }
        if(T_Damage(other, self, self.owner, adjDmg))
        {
            spawn_touchblood(adjDmg);
            bleed = TRUE;
        }
        AD_NailPierce(other); // AD support: AD's Nail Piercer (vr_ad_items.qc)
    }
    if(!bleed)
    {
        spike_bullethole();
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_SPIKE);
        WriteVec3(MSG_BROADCAST, self.origin);
    }
    VR_Proj_End();
    remove(self);
};

// Its shot from `xHand` of self (a player, or an armed grunt: vr_armedgrunt.qc): a nail of its magazine (its clip;
// without reloading, your nails), fast and nearly straight; its punch, flash, kick, buzz and smoke.
void(float xHand) W_FireNailPistol =
{
    if(getCurrentAmmoOrClip(xHand) < 1)
    {
        return;
    }
    makevectors(VRGetWeaponFireRot(xHand));
    float vol = max(0, min(4, cvar_hget(cvarh_vr_nailpistol_volume)));
    // Its shot: our crack and the slide's clack (vr/nailpistol_shot.wav, make_nailpistol_sounds.py) over the super
    // nailgun's punch pitched up (a lighter, smaller gun).
    sound(self, VRGetGunChannel(xHand), "vr/nailpistol_shot.wav", vol, ATTN_NORM);
    sound_pitch(self, CHAN_AUTO, "weapons/spike2.wav", vol * 0.7, ATTN_NORM, 135 + crandom() * 4);
    delAmmoOrClipAndUpdateCounter(xHand, AID_NAILS, 1);

    float k = max(0, cvar_hget(cvarh_vr_nailpistol_spread)) * 0.0174533; // (radians: small, its tangent)
    vector dir = normalize(v_forward + (v_right * crandom() + v_up * crandom()) * k);
    entity newmis = launch_spike(VRGetMuzzlePos(xHand), dir);
    VR_ShotLaunched(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)
    newmis.touch = VR_NailPistol_SpikeTouch;
    newmis.velocity = dir * max(100, cvar_hget(cvarh_vr_nailpistol_speed));
    newmis.velocity += vrProjectileVelocity(xHand, 1.0);
    VR_Burn_NailLaunched(newmis); // VR: through a torch's flame, a lava nail (vr_burning.qc)

    self.effects = self.effects | EF_MUZZLEFLASH;
    self.punchangle_x = -1;
    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp; slot 36's Recoil, Flash)
    VRGunHaptic(xHand, 0.06, 160, 0.85);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);

    if(self.flags & FL_CLIENT)
    {
        // The slide back (frame 1) for a moment, the wisp of smoke after the string (VR_NailPistol_PlayerFrame).
        float heat = VR_IMPL_SELECT(xHand, self.vr_np_heat2, self.vr_np_heat);
        float since = time - VR_IMPL_SELECT(xHand, self.vr_np_heat_at2, self.vr_np_heat_at);
        heat = max(0, heat - since * 4) + 1;
        float until = time + max(0, cvar_hget(cvarh_vr_nailpistol_slide_time));
        if(xHand == cVR_OffHand)
        {
            self.vr_np_slide2 = until;
            self.vr_np_heat2 = heat;
            self.vr_np_heat_at2 = time;
            self.vr_np_smoke2 = time + 0.3;
        }
        else
        {
            self.vr_np_slide = until;
            self.vr_np_heat = heat;
            self.vr_np_heat_at = time;
            self.vr_np_smoke = time + 0.3;
        }
        VRSetWeaponFrame(xHand, 1);
        vr_np_shots++;
    }
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("nailpistol: hand %g fired from %v at %v, %g left\n", xHand, VRGetMuzzlePos(xHand),
            newmis.velocity, getCurrentAmmoOrClip(xHand)));
    }
};

// W_AttackImpl's: the trigger pulled (time past its cooldown). Semi-automatic: a new pull fires once (or clicks dry,
// once), then nothing until the trigger is let go (VR_NailPistol_PlayerFrame re-arms it).
void(float xHand, float xAttackFinished, float* xOutAttackFinished) VR_NailPistol_Attack =
{
    float rearm = VR_IMPL_SELECT(xHand, self.vr_np_rearm2, self.vr_np_rearm);
    if(cvar_hget(cvarh_vr_debug_shots) >= 2)
    {
        dprint(sprintf("nailpistol: the trigger, hand %g, re-arm %g\n", xHand, rearm));
    }
    if(rearm)
    {
        return;
    }
    if(xHand == cVR_OffHand) { self.vr_np_rearm2 = TRUE; } else { self.vr_np_rearm = TRUE; }
    float cool = max(0.02, cvar_hget(cvarh_vr_nailpistol_cooldown));
    if(!W_CheckNoAmmo(xHand))
    {
        sound(self, VRGetHandChannel(xHand), "gunclick.wav", 1, ATTN_NORM);
        VRGunHaptic(xHand, 0.02, 200, 0.3);
        *xOutAttackFinished = time + cool;
        vr_np_dry++;
        if(cvar_hget(cvarh_vr_debug_shots))
        {
            dprint(sprintf("nailpistol: dry click, hand %g\n", xHand));
        }
        return;
    }
    self.show_hostile = time + 1;
    VR_Stealth_Shot(self, WID_NAILPISTOL); // QVR: heard by its loudness (vr_stealth.qc)
    RuneApplyBlackNoise(self);
    SuperDamageSound();
    W_FireNailPistol(xHand);
    *xOutAttackFinished = time + RuneApplyHell(cool, self);
};

// Whether the nail pistol in `xHand` of self has no round to fire: its magazine empty or out (its slide locks back).
float(float xHand) VR_NailPistol_Empty =
{
    if(VRIsWeaponReloadingEnabledFor(self, xHand))
    {
        return VRGetWeaponClip(xHand) < 1 || (VR_Reload_ImmersiveFor(WID_NAILPISTOL) && self.ishuman &&
               (VRGetWeaponFlags(xHand) & QVR_WPNFLAG_NOMAG));
    }
    return getCurrentAmmoCount(xHand) < 1;
};

void(float xHand) VR_NailPistol_HandFrame =
{
    if(VRGetWeapon(xHand) != WID_NAILPISTOL)
    {
        if(xHand == cVR_OffHand) { self.vr_np_rearm2 = FALSE; self.vr_np_smoke2 = 0; }
        else                     { self.vr_np_rearm = FALSE; self.vr_np_smoke = 0; }
        return;
    }
    if(!VRGetFireButtonPressed(xHand))
    {
        if(xHand == cVR_OffHand) { self.vr_np_rearm2 = FALSE; } else { self.vr_np_rearm = FALSE; }
    }
    // The slide: back a moment after a shot (the renderer runs it home between the frames), locked back while empty.
    float back = (time < VR_IMPL_SELECT(xHand, self.vr_np_slide2, self.vr_np_slide) || VR_NailPistol_Empty(xHand));
    VRSetWeaponFrame(xHand, back ? 1 : 0);
    // The wisp of smoke: a moment after the last shot, more after a quick string.
    float at = VR_IMPL_SELECT(xHand, self.vr_np_smoke2, self.vr_np_smoke);
    if(at > 0 && time >= at)
    {
        float n = cvar_hget(cvarh_vr_nailpistol_smoke);
        float heat = VR_IMPL_SELECT(xHand, self.vr_np_heat2, self.vr_np_heat);
        if(n > 0)
        {
            particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_SMOKE, rint(min(6, 1 + heat * 0.6) * n));
        }
        if(xHand == cVR_OffHand) { self.vr_np_smoke2 = 0; } else { self.vr_np_smoke = 0; }
    }
};

// Every player frame (client.qc PlayerPreThink, self the player).
void() VR_NailPistol_PlayerFrame =
{
    VR_NailPistol_HandFrame(cVR_MainHand);
    VR_NailPistol_HandFrame(cVR_OffHand);
};

/*QUAKED weapon_nailpistol (0 .5 .8) (-16 -16 0) (16 16 32)
Quake VR's nail pistol: one-handed, semi-automatic, a 12-round magazine in its grip fed from your nails. Picked up as the
other weapons are (with 24 nails); its own model lies there (no classic pickup model).
*/
void() weapon_nailpistol =
{
    impl_weapon_item("progs/v_nailpistol.mdl", WID_NAILPISTOL);
};
