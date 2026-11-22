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
// The flip grip (ROUND25.md, "flipgrip"; his notes 2026-11-14): the grip is the blade's way in the hand, through every way out
// of and into one. Let go of (dropped, thrown), the weapon keeps the way it was drawn (DropWeaponInHandScaled: the engine's
// weapondrawnpose 49, 50 bake the reverse grip's turn into the prop's angles); the other hand's carry keeps it too (the engine
// draws it so); every grab chooses by the blade's way: one lying about or in the air (VR_RevGrip_OnTake), one stuck in a body
// or a crate (VR_RevGrip_Embed), the one the other hand carries taken back (VR_RevGrip_TakeCarried: weapondrawnpose 47), a
// force grab's catch with vr_revgrip_forcegrab 1 (0, the default: upright, note 1).

float revgripKeep; // TRUE while a blade comes back into the hand that held it (VR_Embed_Free): its own grip kept
.float vr_revgrip_sec; // bits, 1 the off hand, 2 the main: its eject button held last frame
float vr_revgrip_flips; // (the tests: the flips made by the button)
float vr_revgrip_refused; // (the tests: the flips refused by the gate: the other hand off the handle)

// The flags `xFlags` of the weapon `ent` taken by `p`'s `xHand` (wpnthrow_handtouch_impl), with its grip: upright for a force
// grab's catch (`xUpright`) or a weapon that may not be reversed; its own kept (revgripKeep); else by its blade's way.
float(entity ent, entity p, float xHand, float xFlags, float xUpright) VR_RevGrip_OnTake =
{
    float own = xFlags & QVR_WPNFLAG_REVGRIP;
    float flags = xFlags - own;
    if((xUpright && !cvar_hget(cvarh_vr_revgrip_forcegrab)) || (xFlags & QVR_WPNFLAG_FOREGRIP_CARRIED))
    {
        return flags;
    }
    if(revgripKeep)
    {
        return flags | own;
    }
    vector c = weapondrawnpose(ent, xHand, 43, p); // (p's own hand and vr_revgrip: MP_PLAN.md B3f)
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

// The flags `xFlags` of the weapon `p`'s other hand carries off its handle, taken back by `xHand` by its handle (VRTakeCarriedWeapon):
// upright or reversed by the blade's way in `xHand` as it closes (the engine's weapondrawnpose 47: the carried weapon as drawn); its
// own grip when the engine can't tell (another player's).
float(entity p, float xHand, float xFlags) VR_RevGrip_TakeCarried =
{
    vector c = weapondrawnpose(p, xHand, 47);
    if(!c_y)
    {
        return xFlags;
    }
    if(cvar_hget(cvarh_developer))
    {
        dprint(sprintf("revgrip: the carried weapon taken back %s (blade to the thumb's side: cos %.2f)\n", c_x ? "reversed" : "upright", c_z));
    }
    float flags = xFlags - (xFlags & QVR_WPNFLAG_REVGRIP);
    return c_x ? flags | QVR_WPNFLAG_REVGRIP : flags;
};

void(float xHand) VR_RevGrip_Flip;

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
    if(!sec || secWas || VR_FlatPlay(self))
    {
        return;
    }
    VR_RevGrip_Flip(xHand);
};

// The flip itself (the eject button's, and the flat holster menu's FLIP GRIP: impulses 84, 85): the grip of the sword or
// dagger in `xHand` of self turned over, with its sound and pulse. Nothing when the hand holds none that may be flipped.
// (dwellweap: and Dwell's Crystal Lance held as a lance, a pole weapon with no magazine to eject: its spear turned over,
// the crystal head down for a stab downwards; weapons.txt slot 55's `rev`.)
float(float xHand) DW_LanceHeld; // (dwell/dw_items.qc)
void(float xHand) VR_RevGrip_Flip =
{
    if(self.health <= 0 || !self.ishuman || (VRGetEntWeapon(self, xHand) != WID_SWORD && !DW_LanceHeld(xHand)))
    {
        return;
    }
    float flags = VRGetEntWeaponFlags(self, xHand);
    vector may = weapondrawnpose(self, xHand, 46);
    if((flags & QVR_WPNFLAG_FOREGRIP_CARRIED) || !may_z)
    {
        return;
    }
    // The flip gate (ROUND25.md "flipgate"; his note 2026-11-15): held in two hands, it flips only with both on its handle; the
    // other hand holding it anywhere else or by its blade refuses it, with a light pulse (vr_revgrip_2h_gate, _blocked_pulse).
    if(may_z < 0)
    {
        vr_revgrip_refused = vr_revgrip_refused + 1;
        float pulse = cvar_hget(cvarh_vr_revgrip_blocked_pulse);
        if(pulse > 0)
        {
            haptic(xHand, 0, 0.03, 40, min(1, pulse));
        }
        if(cvar_hget(cvarh_developer))
        {
            dprint(sprintf("revgrip: hand %g's flip refused: the other hand holds it off its handle\n", xHand));
        }
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
