//
//
//
// ----------------------------------------------------------------------------
// Armed monsters: the toolgun's spawn page (vr_toolgun.cpp `armed`, vr_toolgun_panel.cpp armedPage; ROUND22.md,
// "Armed monsters: the toolgun's spawn page"; docs/vr-port/ARMED_MONSTERS.md)
// ----------------------------------------------------------------------------
//
// The engine asks the table here: its rows (each one a kind on the page's Kind list), what each row may hold of every
// "weapon" key (its Right Hand and Left Hand lists), and how a row holding a loadout looks standing (the spawn ghost).
// A new row in VR_AM_Row shows up on the page by itself.

float() VR_AM_UiCount = { return VR_AM_COUNT; };
string(float k) VR_AM_UiKindName = { VR_AM_Row(k); return vr_am_name; };
string(float k) VR_AM_UiKindSpawn = { VR_AM_Row(k); return vr_am_spawnname; };
string(float k) VR_AM_UiKindModel = { VR_AM_Row(k); return vr_am_model; };
float(float k) VR_AM_UiKindFrame = { VR_AM_Row(k); return vr_am_f_stand; };

// What row `k` may hold of the "weapon" key `code` (VR_AG_Decode's: a weapon id, an AD upgrade's or melee kind's code):
// 0 nothing (not a weapon here: no data for it, the grapple, the toolgun, its fists, a class its row doesn't hold);
// else 1, + 2 when it pairs (VR_AG_Pairable: a one-handed melee weapon, or a gun not too big; two pair when both do and
// both are melee or both guns: VR_AM_UiPairs, the engine's toolgun::armed::pairs), + 4 * its class (VR_AG_CLASS_*: the
// page's order).
float(float w1, float w2) VR_AM_UiPairs =
{
    float m1 = floor(w1 / 4) == VR_AG_CLASS_MELEE;
    float m2 = floor(w2 / 4) == VR_AG_CLASS_MELEE;
    return (w1 & 2) && (w2 & 2) && m1 == m2;
};

float(float k, float code) VR_AM_UiWeapon =
{
    if(code <= 0 || code == WID_FIST || !VR_AG_Decode(code))
    {
        return 0;
    }
    float c = VR_AG_Class(vr_ag_dec_wid);
    VR_AM_Row(k);
    if(c == VR_AG_CLASS_FISTS || !(vr_am_classes & (1 << c)))
    {
        return 0;
    }
    return 1 + (VR_AG_Pairable(vr_ag_dec_wid, vr_ag_dec_flags) ? 2 : 0) + 4 * c;
};

// The weapon `code`'s name (as the log's: VR_AG_NameOf; AD's upgrades by their own names).
string(float code) VR_AM_UiWeaponName =
{
    VR_AG_Decode(code);
    if(code == VR_AG_CODE_SHADOWAXE) { return "Shadow Axe"; }
    if(code == VR_AG_CODE_WIDOWMAKER) { return "Widowmaker"; }
    if(code == VR_AG_CODE_PLASMAGUN) { return "Plasma Gun"; }
    return VR_AG_NameOf(vr_ag_dec_wid, vr_ag_dec_flags, world);
};

// The ghost's held weapons (VR_AM_UiGhost): their models (""), places and angles, the monster standing at the origin
// facing east (+x).
string vr_am_ui_model1;
vector vr_am_ui_org1;
vector vr_am_ui_ang1;
string vr_am_ui_model2;
vector vr_am_ui_org2;
vector vr_am_ui_ang2;

// A held weapon's look for the ghost: an entity with the weapon of `code`'s model and keys (VR_AG_MakeProp's, nothing
// else: no record, no think).
entity(entity g, float code) VR_AM_UiProp =
{
    VR_AG_Decode(code);
    entity p = spawn();
    p.classname = "vr_am_ghost";
    p.owner = g;
    p.weapon = vr_ag_dec_wid;
    p.weaponflags = vr_ag_dec_flags;
    VR_ADGun_Placed(p.weapon); // (AD's guns' and melee weapons' models made: any map)
    VR_ADMelee_Placed(p.weapon, p.weaponflags);
    string m = WeaponIdToModel(p.weapon, p.weaponflags);
    if(vr_ag_dec_up && vr_ad_data)
    {
        m = AD_VRModelName(p.weapon); // (an upgrade's: AD_WeaponModel's, with no record made for it)
        AD_EnsureVRModel(m);
    }
    precache_model(m);
    setmodel(p, m);
    return p;
};

// Row `k` holding `code` in its right hand (0, -1: none) and `code2` in its left (0: none; only a one-handed pair) as it
// stands holding them a moment after it is made: the layer's own pose (VR_AGP_Pose, quiet: placed, not sent; a melee
// weapon's stance settled) on stand-ins removed at once. The weapons drawn: 0, 1 or 2 (vr_am_ui_*).
float(float k, float code, float code2) VR_AM_UiGhost =
{
    vr_am_ui_model1 = "";
    vr_am_ui_model2 = "";
    float w1 = code > 0 ? VR_AM_UiWeapon(k, code) : 0;
    if(!w1)
    {
        return 0;
    }
    float w2 = code2 > 0 ? VR_AM_UiWeapon(k, code2) : 0;
    VR_AM_Row(k);
    entity g = spawn();
    g.classname = "vr_am_ghost";
    g.vr_ag_kind = k;
    precache_model(vr_am_model);
    setmodel(g, vr_am_model);
    g.frame = vr_am_f_stand;
    setorigin(g, '0 0 0');
    entity p = VR_AM_UiProp(g, code);
    g.vr_ag_prop = p;
    g.vr_ag_wid = p.weapon;
    g.vr_ag_wflags = p.weaponflags;
    entity pl = world;
    if(VR_AM_UiPairs(w1, w2))
    {
        pl = VR_AM_UiProp(g, code2);
        pl.vr_ag_left = 1;
        g.vr_ag_prop2 = pl;
        g.vr_ag_wid2 = pl.weapon;
        g.vr_ag_wflags2 = pl.weaponflags;
        g.vr_ag_dual = 1;
    }
    g.vr_ag_readyw = 0.7; // (its stance settled, as it stands a moment after it is made: VR_AGP_Melee eases it to 0.7)
    float keep = frametime;
    frametime = 0;
    vr_agp_quiet = 1;
    VR_AGP_Pose(g, p);
    vr_agp_quiet = 0;
    frametime = keep;
    vr_am_ui_model1 = p.model;
    vr_am_ui_org1 = p.origin;
    vr_am_ui_ang1 = p.angles;
    remove(p);
    if(pl)
    {
        vr_am_ui_model2 = pl.model;
        vr_am_ui_org2 = pl.origin;
        vr_am_ui_ang2 = pl.angles;
        remove(pl);
    }
    remove(g);
    return pl ? 2 : 1;
};

string(float hand) VR_AM_UiGhostModel = { return hand ? vr_am_ui_model2 : vr_am_ui_model1; };
vector(float hand) VR_AM_UiGhostOrigin = { return hand ? vr_am_ui_org2 : vr_am_ui_org1; };
vector(float hand) VR_AM_UiGhostAngles = { return hand ? vr_am_ui_ang2 : vr_am_ui_ang1; };
