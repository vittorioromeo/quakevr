// vr_bayonet.qc -- the bayonet on its own (progs/bayonet.mdl, Misc/quakevr/make_bayonet.py): a small knife-like prop,
// taken, swung and thrown as it is, its hold in Held Object Offsets (quakevr/data/props.txt slot 71: held along its
// leather-wrapped shank and socket, its blade's tip the striking end). On a shotgun it is an attachment
// (docs/vr-port/ATTACHMENTS.md, "The bayonet").

// Always precached (a saved game may hold one; the attachments draw it on the guns).
void() VR_Bayonet_Precache =
{
    precache_model("progs/bayonet.mdl");
};

/*QUAKED vr_bayonet (0.5 0.5 0.6) (-1 -1 -1) (7 1 1)
Quake VR: a bayonet lying loose (progs/bayonet.mdl): taken, swung and thrown as it is (a small knife; the toolgun's
Props, "Bayonet"). "angle": its yaw. It lies on its side on whatever is below its origin (within 128 units).
*/
void() vr_bayonet =
{
    setmodel(self, "progs/bayonet.mdl");
    traceline(self.origin + '0 0 1', self.origin - '0 0 128', TRUE, self);
    setorigin(self, trace_endpos + '0 0 0.5');
    self.angles = '0 0 90' + self.angles_y * '0 1 0'; // on its side: the blade flat
    setsize(self, '-1 -1 -0.5', '6.5 1 0.5');
    self.classname = "vr_bayonet";
    self.movetype = MOVETYPE_TOSS;
    self.solid = SOLID_NOT_BUT_TOUCHABLE;
    self.flags = self.flags | FL_FORCEGRABBABLE | FL_EASYHANDTOUCH;
    self.touch = forcegrabbable_touch; // thrown hard, it hurts what it hits as a thrown prop does
    self.handtouch = SUB_Null;         // nothing to take (VR_Carry_Setup keeps it as carry_use)
    self.throwhit = QVR_THROWHIT_HIT;
    self.owner = world;
    SetThrownWeaponDealDamage(self, FALSE);
    SetThrownWeaponDisappearOnHit(self, FALSE);
    VR_Carry_Setup(self); // carried, nudged, thrown; a rigid body (vr_rigid 1)
};
