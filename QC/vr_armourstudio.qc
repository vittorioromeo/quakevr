// vr_armourstudio.qc -- the Armour Studio's subject (Quake/vr/vr_armourstudio.cpp; docs/vr-port/ARMOUR_STUDIO.md): an armed
// humanoid of a kind put ahead of you with its fists, unhurt by anything, held still by vr_agrunt_freeze (the engine sets it
// while the studio runs), its armour given by the engine (VR_AgArmor_SetCmd). One at a time: a new one takes the old one's
// place.

entity vr_as_subject;
entity vr_dzs_subject; // the Dazzle Studio's (vr_dazzlestudio.cpp)

// A studio's subject (the Armour Studio's, the Dazzle Studio's): an armed humanoid of row `kind` `ahead` units in front of
// the player `self`, facing him, holding weapon `wid` (-1 its fists) and `wid2` in its left hand (0 none), unhurt. World:
// none (no such kind, AD's without AD's data, not a player).
entity(float kind, float ahead, float wid, float wid2) VR_StudioSubject =
{
    float last = cvar_hget(cvarh_vr_ad_mounted) ? VR_AM_ROWS : VR_AM_COUNT;
    if(kind < 0 || kind >= last || self.classname != "player")
    {
        return world;
    }
    // (as the armour tests' VR_ART_SpawnKind: facing you)
    entity p = self;
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    self = spawn();
    setorigin(self, p.origin + fw * ahead);
    self.angles_y = vectoyaw(p.origin - self.origin);
    self.weapon = wid;
    self.weapon2 = wid2;
    VR_AM_Spawn(kind);
    entity g = self;
    self = p;
    if(g.classname == "" || !(g.flags & FL_MONSTER))
    {
        return world;
    }
    g.takedamage = DAMAGE_NO;
    g.health = 1000;
    return g;
};

// A studio's subject removed with the weapons it holds.
void(entity g) VR_StudioSubject_Remove =
{
    if(g.vr_ag_prop)
    {
        g.vr_ag_prop.weaponinst = world;
        remove(g.vr_ag_prop);
    }
    g.vr_ag_prop = world;
    if(g.vr_ag_prop2)
    {
        g.vr_ag_prop2.weaponinst = world;
        remove(g.vr_ag_prop2);
    }
    g.vr_ag_prop2 = world;
    remove(g);
};

// `kind`: VR_AM_GRUNT .. VR_AM_DKNIGHT; below 0 none (the old one removed). `ahead`: units in front of you. Its fists.
entity(float kind, float ahead) VR_ArmourStudio_Spawn =
{
    if(vr_as_subject)
    {
        remove(vr_as_subject);
        vr_as_subject = world;
    }
    if(kind >= VR_AM_COUNT)
    {
        return world; // (as before: AD's two by their own table, not here)
    }
    vr_as_subject = VR_StudioSubject(kind, ahead, -1, 0);
    return vr_as_subject;
};

// The Dazzle Studio's subject: row `kind` holding `wid` (and `wid2` in its left hand: its dual-wielding set), `ahead` units
// in front of you, held in its shield pose by the engine (.vr_ag_dazzle_hold); below 0 none (the old one removed).
entity(float kind, float ahead, float wid, float wid2) VR_DazzleStudio_Spawn =
{
    if(vr_dzs_subject)
    {
        VR_StudioSubject_Remove(vr_dzs_subject);
        vr_dzs_subject = world;
    }
    if(kind < 0)
    {
        return world;
    }
    vr_dzs_subject = VR_StudioSubject(kind, ahead, wid, wid2);
    if(vr_dzs_subject)
    {
        vr_dzs_subject.vr_ag_dazzle_hold = 1;
    }
    return vr_dzs_subject;
};

void() monster_dog;
void() monster_ogre;
void() monster_wizard;
void() monster_demon1;
void() monster_shambler;

// A beast subject held where it stands (no AI: the engine plays its frames and gives its armour), every tenth of a second.
void() VR_AS_BeastHold =
{
    self.nextthink = time + 0.1;
    self.velocity = '0 0 0';
    self.enemy = world;
};

// A beast of model `mdl` (vr_beastarmour.cpp's kinds: the rottweiler, ogre, scrag, fiend; the shambler once it has a table)
// `ahead` units in front of you, facing you, its armour roll held (the engine gives it the studio's tier), held still.
entity(string mdl, float ahead) VR_ArmourStudio_SpawnBeast =
{
    if(vr_as_subject)
    {
        remove(vr_as_subject);
        vr_as_subject = world;
    }
    if(self.classname != "player")
    {
        return world;
    }
    entity p = self;
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    self = spawn();
    setorigin(self, p.origin + fw * ahead + (mdl == "progs/wizard.mdl" ? '0 0 24' : '0 0 8'));
    self.angles_y = vectoyaw(p.origin - self.origin);
    vr_ar_hold_roll = 1;
    if(mdl == "progs/dog.mdl") { self.classname = "monster_dog"; monster_dog(); }
    else if(mdl == "progs/ogre.mdl") { self.classname = "monster_ogre"; monster_ogre(); }
    else if(mdl == "progs/wizard.mdl") { self.classname = "monster_wizard"; monster_wizard(); }
    else if(mdl == "progs/demon.mdl") { self.classname = "monster_demon1"; monster_demon1(); }
    else if(mdl == "progs/shambler.mdl") { self.classname = "monster_shambler"; monster_shambler(); }
    else { remove(self); self = p; vr_ar_hold_roll = 0; return world; }
    vr_ar_hold_roll = 0;
    entity g = self;
    self = p;
    if(g.classname == "" || g.model == "")
    {
        return world;
    }
    g.flags = g.flags | FL_MONSTER; // (VR_AgArmor_SetCmd's wearers)
    g.takedamage = DAMAGE_NO;
    g.health = 1000;
    g.think = VR_AS_BeastHold;
    g.nextthink = time + 0.1;
    g.movetype = MOVETYPE_NONE;
    vr_as_subject = g;
    return g;
};
