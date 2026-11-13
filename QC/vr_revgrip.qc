// vr_revgrip.qc -- the reverse ("blade down") grip of swords and daggers (ROUND25.md, "revgrip"; his request 2026-11-13:
// "make it so that daggers and swords can be held 'upside down' easily ... grabbing them with the pointy bit downwards would
// enable different natural moves in VR").
//
// A weapon's QVR_WPNFLAG_REVGRIP: the engine draws it turned half round in the hand (vr_view_weapon.cpp reverseTurn), so its
// blade line, its stabs and swings, its body in Box3D and an embedded blade's handle hold all follow. Which weapons: the
// engine's Weapon Offsets' Reverse Grip (weapons.txt `rev`: the swords and the dagger), with vr_revgrip on; none in flat-screen
// play. His notes:
//   1. a force grab always takes it upright (VR_RevGrip_OnTake's xUpright);
//   2. a hand's catch or grab of it lying about or in the air chooses by its blade: to the thumb's side upright, to the little
//      finger's reversed (the engine's weapondrawnpose 43: the blade's way against the normal grip's at the hand's turn);
//   3. holstered and drawn again it keeps its grip (the flag travels with it, as its flags do);
//   4. its eject button (B/Y, as a magazine's) flips the grip: the engine turns it over vr_revgrip_flip_time, a sound
//      (vr_revgrip_volume) and a pulse (VR_RevGrip_HandFrame); nothing else for a blade.
// A blade come out of a body into the hand that held it by its handle keeps the grip it was held by (revgripKeep:
// VR_Embed_Free); a fresh grip on a stuck blade's handle chooses (VR_RevGrip_Embed).

float revgripKeep; // TRUE while a blade comes back into the hand that held it (VR_Embed_Free): its own grip kept
.float vr_revgrip_sec; // bits, 1 the off hand, 2 the main: its eject button held last frame
float vr_revgrip_flips; // (the tests: the flips made by the button)

// The flags `xFlags` of the weapon `ent` taken by `p`'s `xHand` (wpnthrow_handtouch_impl), with its grip: upright for a force
// grab's catch (`xUpright`) or a weapon that may not be reversed; its own kept (revgripKeep); else by its blade's way.
float(entity ent, entity p, float xHand, float xFlags, float xUpright) VR_RevGrip_OnTake =
{
    float own = xFlags & QVR_WPNFLAG_REVGRIP;
    float flags = xFlags - own;
    if(xUpright || (xFlags & QVR_WPNFLAG_FOREGRIP_CARRIED))
    {
        return flags;
    }
    if(revgripKeep)
    {
        return flags | own;
    }
    vector c = weapondrawnpose(ent, xHand, 43);
    if(cvar_hget(cvarh_developer) && c_y)
    {
        dprint(sprintf("revgrip: %s taken %s (blade to the thumb's side: cos %.2f)\n", ent.netname, c_x ? "reversed" : "upright", c_z));
    }
    return c_x ? flags | QVR_WPNFLAG_REVGRIP : flags;
};

// A hand's fresh grip on the handle of the blade `e` stuck in a body or a prop (VR_Embed_Grip, before it takes hold): its grip
// chosen as for a blade taken from the ground, unless a hand holds it already.
void(entity e, entity p, float xHand) VR_RevGrip_Embed =
{
    if(e.vr_hold_hands)
    {
        return;
    }
    e.weaponflags = VR_RevGrip_OnTake(e, p, xHand, e.weaponflags, FALSE);
};

// Every player frame, for `xHand` of self: its eject button (B/Y) pressed with a sword or dagger in it, held by its handle,
// flips the grip.
void(float xHand) VR_RevGrip_HandFrame =
{
    float bit = xHand == cVR_MainHand ? 2 : 1;
    float sec = VRGetEntSecondaryPressed(self, xHand);
    float secWas = (self.vr_revgrip_sec & bit) != 0;
    self.vr_revgrip_sec = self.vr_revgrip_sec - (self.vr_revgrip_sec & bit);
    if(sec)
    {
        self.vr_revgrip_sec = self.vr_revgrip_sec | bit;
    }
    if(!sec || secWas || self.health <= 0 || !self.ishuman || VRGetEntWeapon(self, xHand) != WID_SWORD)
    {
        return;
    }
    float flags = VRGetEntWeaponFlags(self, xHand);
    vector may = weapondrawnpose(self, xHand, 46);
    if((flags & QVR_WPNFLAG_FOREGRIP_CARRIED) || !may_y || VR_FlatPlay(self))
    {
        return;
    }
    flags = (flags & QVR_WPNFLAG_REVGRIP) ? flags - QVR_WPNFLAG_REVGRIP : flags | QVR_WPNFLAG_REVGRIP;
    VRSetEntWeaponFlags(self, xHand, flags);
    vr_revgrip_flips = vr_revgrip_flips + 1;
    // The hand closing on its handle again (vr/phys/grab_metal1..3, as a weapon taken back) and a short pulse.
    float vol = cvar_hget(cvarh_vr_revgrip_volume);
    if(vol > 0)
    {
        float k = floor(random() * 3);
        VR_SoundVaried(self, VRGetHandChannel(xHand),
            k < 1 ? "vr/phys/grab_metal1.wav" : (k < 2 ? "vr/phys/grab_metal2.wav" : "vr/phys/grab_metal3.wav"), min(1, vol), ATTN_NORM);
    }
    haptic(xHand, 0, 0.06, 80, 0.5);
    if(cvar_hget(cvarh_developer))
    {
        dprint(sprintf("revgrip: hand %g flips its grip: %s\n", xHand, (flags & QVR_WPNFLAG_REVGRIP) ? "reversed" : "upright"));
    }
};
