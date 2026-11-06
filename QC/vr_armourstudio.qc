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
