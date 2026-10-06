// Saved native campaign context; engine restores it before map/resource spawn.
float vr_save_campaign;
// Saved precache layout: 1 + Hipnotic + 2*Rogue. It must match when a save is restored.
float vr_save_packmask;

// Optional mission packs. The engine validates their own archives before mounting them.
float() VR_Pack_Hipnotic = { return cvar("vr_hipnotic_available"); };
float() VR_Pack_Rogue = { return cvar("vr_rogue_available"); };

// ---- What Quake VR can run without the mission packs ---------------------------------------------------------
// A feature is available exactly when the files it loads exist: Quake VR's own (quakevr), the stock game's (id1), or
// an installed pack's. So whatever Quake VR ships works out of the box, wherever it came from (the Hipnotic weapons,
// the grapple, the breakable walls, the knight statues and the ogre boss), and what only a pack has (its monsters, its
// models) comes on by itself once the pack is installed. Read once per map (VR_Pack_Refresh, at worldspawn), never
// saved: a save is loaded with the files of the installation it is loaded on.

// A file on the search path ("" counts as present: the helpers below take a fixed number of names).
float(string xPath) VR_Has = { return xPath == "" || fileexists(xPath); };
float(string a, string b, string c, string d) VR_Has4 = { return VR_Has(a) && VR_Has(b) && VR_Has(c) && VR_Has(d); };

// The pack's file when it is there, else Quake VR's own stand-in (one of the two literals: precache-safe).
string(string xPack, string xOwn) VR_ModelOr = { if(fileexists(xPack)) { return xPack; } return xOwn; };
// A statue knight's (Rogue's statue sounds and stone gibs when the pack is there, else the knight's own sounds and
// Quake VR's rock debris: the statues are always available).
string(string xStatue, string xOwn) VR_StatueFile = { if(!vr_pack_refreshed) { VR_Pack_Refresh(); } if(vr_have_statue) { return xStatue; } return xOwn; };

// The same for a sound: its name as sound() takes it (no "sound/"), its file on disk as `xPackFile`.
string(string xPack, string xOwn, string xPackFile) VR_SoundOr = { if(fileexists(xPackFile)) { return xPack; } return xOwn; };

nosave float vr_have_lava;          // lava nails (Rogue's lava spike)
nosave float vr_have_multirockets;  // the rocket launcher's multi-rockets (Rogue's split rockets)
nosave float vr_have_plasma;        // the lightning gun's plasma
nosave float vr_have_wetsuit, vr_have_empathy, vr_have_horn;          // Hipnotic's items
nosave float vr_have_shield, vr_have_belt, vr_have_equake, vr_have_sphere; // Rogue's items
nosave float vr_have_fireball;      // Rogue's fireball trap projectile
nosave float vr_have_lavarock;      // Hipnotic's lavaball trap projectile
nosave float vr_have_statue;        // Rogue's statue sounds and stone gibs (else the knights' own and rock debris)
nosave float vr_have_playham;       // Hipnotic's third-person player holding Mjolnir
nosave float vr_have_ctf;           // Rogue's capture-the-flag models and sounds
nosave float vr_pack_refreshed;

void() VR_Pack_Refresh =
{
    vr_have_lava = VR_Has("progs/lspike.mdl");
    vr_have_multirockets = VR_Has4("progs/rockup.mdl", "progs/rockup_d.mdl", "", "");
    vr_have_plasma = VR_Has4("progs/plasma.mdl", "sound/plasma/fire.wav", "sound/plasma/flight.wav", "sound/plasma/explode.wav");
    vr_have_wetsuit = VR_Has("progs/wetsuit.mdl");
    vr_have_empathy = VR_Has4("progs/empathy.mdl", "sound/hipitems/empathy.wav", "sound/hipitems/empathy2.wav", "");
    vr_have_horn = VR_Has4("progs/horn.mdl", "sound/hipitems/horn.wav", "", "");
    vr_have_shield = (VR_Has4("progs/shield.mdl", "progs/p_shield.mdl", "sound/shield/hit.wav", "sound/shield/pickup.wav") &&
                      VR_Has("sound/shield/fadeout.wav"));
    vr_have_belt = VR_Has4("progs/beltup.mdl", "sound/belt/use.wav", "sound/belt/pickup.wav", "sound/belt/fadeout.wav");
    vr_have_equake = TRUE; // (Rogue's rumble, else Quake's own misc/quake.wav: rogue_earthq.qc)
    vr_have_sphere = VR_Has4("progs/sphere.mdl", "sound/sphere/sphere.wav", "", "");
    vr_have_fireball = VR_Has("progs/fireball.mdl");
    vr_have_lavarock = VR_Has("progs/lavarock.mdl");
    vr_have_statue = (VR_Has4("progs/statgib1.mdl", "progs/statgib2.mdl", "progs/statgib3.mdl", "sound/statue/see.wav") &&
                      VR_Has4("sound/statue/idle.wav", "sound/statue/pain.wav", "sound/statue/death.wav", ""));
    vr_have_playham = VR_Has("progs/playham.mdl");
    vr_have_ctf = (VR_Has4("progs/ctfbase.mdl", "progs/ctfmodel.mdl", "sound/misc/flagcap.wav", "sound/misc/flagret.wav") &&
                   VR_Has("sound/misc/flagtk.wav"));
    vr_pack_refreshed = TRUE;
};

// The weapons: Quake VR ships every one's models and sounds (the Hipnotic weapons' and the grapple's too).
float(float wid) VR_Pack_WeaponAvailable =
{
    return TRUE;
};

// A secondary ammo whose projectile only a pack has.
float(float aid) VR_Pack_AmmoAvailable =
{
    if(!vr_pack_refreshed) { VR_Pack_Refresh(); }
    if(aid == AID_LAVA_NAILS) { return vr_have_lava; }
    if(aid == AID_PLASMA) { return vr_have_plasma; }
    return TRUE; // (multi-rockets: the grenade launcher's are Quake VR's own; the rocket launcher's are checked below)
};

// The weapon's secondary ammo dropped when its projectile is not there.
float(float wid, float flags) VR_Pack_WeaponFlags =
{
    if(!vr_pack_refreshed) { VR_Pack_Refresh(); }
    if(((wid == WID_NAILGUN || wid == WID_SUPER_NAILGUN) && !vr_have_lava) ||
       (wid == WID_ROCKET_LAUNCHER && !vr_have_multirockets) || (wid == WID_LIGHTNING && !vr_have_plasma))
    {
        return flags - (flags & QVR_WPNFLAG_USE_SECONDARY_AMMO);
    }
    return flags;
};

float(float iid) VR_Pack_ItemAvailable =
{
    if(!vr_pack_refreshed) { VR_Pack_Refresh(); }
    if(iid == IID_WETSUIT) { return vr_have_wetsuit; }
    if(iid == IID_EMPATHY_SHIELDS) { return vr_have_empathy; }
    if(iid == IID_HORN_OF_CONJURING) { return vr_have_horn; }
    if(iid == IID_LAVA_NAILGUN || iid == IID_LAVA_SUPER_NAILGUN || iid == IID_LAVA_NAILS) { return vr_have_lava; }
    if(iid == IID_MULTI_ROCKET) { return vr_have_multirockets; }
    if(iid == IID_PLASMA_GUN || iid == IID_PLASMA_AMMO) { return vr_have_plasma; }
    if(iid == IID_SHIELD) { return vr_have_shield; }
    if(iid == IID_ANTIGRAV) { return vr_have_belt; }
    if(iid == IID_EARTHQUAKE) { return vr_have_equake; }
    if(iid == IID_V_SPHERE) { return vr_have_sphere; }
    return TRUE; // (Mjolnir, the proximity and laser cannons, the grapple, the multi-grenade, the multi-rocket ammo box)
};

// `bits` without `bit` when its item is not available here (the level parms carried from another installation).
float(float bits, float bit, float iid) VR_Pack_KeepBit =
{
    if(VR_Pack_ItemAvailable(iid)) { return bits; }
    return bits - (bits & bit);
};

float(float available, string pack) VR_Pack_RequireSpawn =
{
    if(available) { return TRUE; }
    dprint(sprintf("VR: unavailable %s entity %s skipped\n", pack, self.classname));
    remove(self);
    return FALSE;
};
