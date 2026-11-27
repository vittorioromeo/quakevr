// vr_armourstudio.qc -- the Armour Studio's subject (Quake/vr/vr_armourstudio.cpp; docs/vr-port/ARMOUR_STUDIO.md): an armed
// humanoid of a kind put ahead of you with its fists, unhurt by anything, held still by vr_agrunt_freeze (the engine sets it
// while the studio runs), its armour given by the engine (VR_AgArmor_SetCmd). One at a time: a new one takes the old one's
// place.

entity vr_as_subject;

// `kind`: VR_AM_GRUNT .. VR_AM_DKNIGHT; below 0 none (the old one removed). `ahead`: units in front of you.
entity(float kind, float ahead) VR_ArmourStudio_Spawn =
{
    if(vr_as_subject)
    {
        remove(vr_as_subject);
        vr_as_subject = world;
    }
    if(kind < 0 || kind >= VR_AM_COUNT || self.classname != "player")
    {
        return world;
    }
    // (as the armour tests' VR_ART_SpawnKind: facing you, its fists)
    entity p = self;
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    self = spawn();
    setorigin(self, p.origin + fw * ahead);
    self.angles_y = vectoyaw(p.origin - self.origin);
    self.weapon = -1;
    VR_AM_Spawn(kind);
    entity g = self;
    self = p;
    if(g.classname == "" || !(g.flags & FL_MONSTER))
    {
        return world;
    }
    g.takedamage = DAMAGE_NO;
    g.health = 1000;
    vr_as_subject = g;
    return g;
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
