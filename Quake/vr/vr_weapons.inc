// vr_weapons.inc -- the per-weapon settings' keys (Weapon Offsets, Weapon Weights).
//   QVR_WEAPON_KEY(Enum, "key"): a key of quakevr/data/weapons.txt, where every slot's values are (its `defaults`
//   block: every slot's value unless the slot's block has its own; "1 by default" below is that block's). The config
//   and the console name a slot's value vr_wofs_<key>_NN (NN = slot + 1; vr_weapons.cpp's stand-ins, not cvars); the
//   named values (vr_weaponvalues.inc: the reload keys of the guns that have them) go by their old cvar names.
// Five keys look retired (they are what rounds 18 to 21 moved into the hotspots and the fitted grasp) but are read:
// hand_av, hand_x/_y/_z by the holster zones' grip sphere (vr_view.cpp's publishGrip through view::anchorPosition,
// vr_holsterzones.cpp);
// 2h_hand_av by view::secondGrip (vr_view.cpp: the helping hand's point on a gun whose Cup hotspot sits far enough
// ahead to be a handle of its own: vr_agarms.cpp, vr_hitreact_skin.cpp, vr_builtins_held.inc, vr_axestick.cpp).

#ifdef QVR_WEAPON_KEY
QVR_WEAPON_KEY(TwoHFixedHandPitch, "2h_fxd_hp")
QVR_WEAPON_KEY(TwoHFixedHandRoll, "2h_fxd_hr")
QVR_WEAPON_KEY(TwoHFixedHandYaw, "2h_fxd_hy")
QVR_WEAPON_KEY(TwoHHandAnchor, "2h_hand_av")
QVR_WEAPON_KEY(TwoHMode, "2h_mode")
QVR_WEAPON_KEY(TwoHPitch, "2h_pitch")
QVR_WEAPON_KEY(TwoHRoll, "2h_roll")
QVR_WEAPON_KEY(TwoHOffsetX, "2h_x")
QVR_WEAPON_KEY(TwoHOffsetY, "2h_y")
QVR_WEAPON_KEY(TwoHYaw, "2h_yaw")
QVR_WEAPON_KEY(TwoHOffsetZ, "2h_z")
QVR_WEAPON_KEY(CrosshairMode, "ch_mode_z")
QVR_WEAPON_KEY(HandAnchor, "hand_av")
QVR_WEAPON_KEY(HandOffsetX, "hand_x")
QVR_WEAPON_KEY(HandOffsetY, "hand_y")
QVR_WEAPON_KEY(HandOffsetZ, "hand_z")
QVR_WEAPON_KEY(HideHand, "hide_hand")
QVR_WEAPON_KEY(ID, "id")
QVR_WEAPON_KEY(MuzzleAnchor, "muzzle_av")
QVR_WEAPON_KEY(MuzzleOffsetX, "muzzle_x")
QVR_WEAPON_KEY(MuzzleOffsetY, "muzzle_y")
QVR_WEAPON_KEY(MuzzleOffsetZ, "muzzle_z")
QVR_WEAPON_KEY(Pitch, "pitch")
QVR_WEAPON_KEY(Roll, "roll")
QVR_WEAPON_KEY(Scale, "scale")
QVR_WEAPON_KEY(WpnButtonAnchor, "wpnbtn_av")
QVR_WEAPON_KEY(WpnButtonPitch, "wpnbtn_pitch")
QVR_WEAPON_KEY(WpnButtonRoll, "wpnbtn_roll")
QVR_WEAPON_KEY(WpnButtonX, "wpnbtn_x")
QVR_WEAPON_KEY(WpnButtonY, "wpnbtn_y")
QVR_WEAPON_KEY(WpnButtonYaw, "wpnbtn_yaw")
QVR_WEAPON_KEY(WpnButtonZ, "wpnbtn_z")
QVR_WEAPON_KEY(WpnButtonMode, "wpnbtnmode")
QVR_WEAPON_KEY(WpnTextAnchor, "wpntxt_av")
QVR_WEAPON_KEY(WpnTextPitch, "wpntxt_pitch")
QVR_WEAPON_KEY(WpnTextRoll, "wpntxt_roll")
QVR_WEAPON_KEY(WpnTextScale, "wpntxt_scale")
QVR_WEAPON_KEY(WpnTextX, "wpntxt_x")
QVR_WEAPON_KEY(WpnTextY, "wpntxt_y")
QVR_WEAPON_KEY(WpnTextYaw, "wpntxt_yaw")
QVR_WEAPON_KEY(WpnTextZ, "wpntxt_z")
QVR_WEAPON_KEY(WpnTextMode, "wpntxtmode")
QVR_WEAPON_KEY(OffsetX, "x")
QVR_WEAPON_KEY(OffsetY, "y")
QVR_WEAPON_KEY(Yaw, "yaw")
QVR_WEAPON_KEY(OffsetZ, "z")
// Round 20: where the thumb sits on the hand (hand model units, added to vr_fingers_* and vr_finger_thumb_*: +x
// towards the fingertips, +y the palm's side, +z the index finger's side). The openness keys this block also held
// (fgr_open and the five fingers' own, the helping hand's 2h_fgr_open and 2h_fgr_thumb) and the fingers' shared
// offsets (fgr_x/_y/_z) are out of the store (2026-11-04, configVersion 222): the fitted grasp (vr_grasp.cpp) curls
// the fingers to the grip it is holding, with the tweaks below on top, and the helping hand's grip is a hotspot.
// Round 18's second grip on a blade (2h_blade) is out too: the blade's helping hand holds a hotspot zone (Edit Blade
// Shape, vr_bladesets.cpp), and vr_twohand.cpp reads that, not a share of the way to the muzzle point.
QVR_WEAPON_KEY(FingerThumbX, "fgr_thumb_x")
QVR_WEAPON_KEY(FingerThumbY, "fgr_thumb_y")
QVR_WEAPON_KEY(FingerThumbZ, "fgr_thumb_z")
// Round 21 (fitted hands): the hand is where the controller is and its fingers wrap the weapon (vr_grasp.cpp);
// these tweak each finger on top of that, as a share of a full curl (negative: more open). What round 21 left
// unread (the hand's own anchor and offset, the fingers' openness and offsets, the unused ones) is out of the
// store now, 2026-11-04 and configVersion 222; the twelve it only moved are read, and who reads each is named
// at the top of this file.
QVR_WEAPON_KEY(FingerThumbBias, "fgr_bias_thumb")
QVR_WEAPON_KEY(FingerIndexBias, "fgr_bias_index")
QVR_WEAPON_KEY(FingerMiddleBias, "fgr_bias_middle")
QVR_WEAPON_KEY(FingerRingBias, "fgr_bias_ring")
QVR_WEAPON_KEY(FingerPinkyBias, "fgr_bias_pinky")
// Round 21: where the other hand may hold the weapon, up to four hotspots (the two-handed grip's foregrip and the
// sword's blade grip became hotspots 1 and 2): type (0 none; 1 a grip point: a foregrip, a pump, a magazine; 2 the
// blade: the half-sword grip along it), where (a grip: a point in the weapon's model space, model units, as its
// vertices are drawn -- weapons::hotspot; the blade: X the share of the way from the hand to the tip the grip is
// centred on), and a bias (units taken off the distance a hand picks it by: larger, easier to take). The helping
// hand takes the one nearest it, less its bias.
QVR_WEAPON_KEY(Hotspot1Type, "hs1_type")
QVR_WEAPON_KEY(Hotspot1X, "hs1_x")
QVR_WEAPON_KEY(Hotspot1Y, "hs1_y")
QVR_WEAPON_KEY(Hotspot1Z, "hs1_z")
QVR_WEAPON_KEY(Hotspot1Bias, "hs1_bias")
QVR_WEAPON_KEY(Hotspot2Type, "hs2_type")
QVR_WEAPON_KEY(Hotspot2X, "hs2_x")
QVR_WEAPON_KEY(Hotspot2Y, "hs2_y")
QVR_WEAPON_KEY(Hotspot2Z, "hs2_z")
QVR_WEAPON_KEY(Hotspot2Bias, "hs2_bias")
QVR_WEAPON_KEY(Hotspot3Type, "hs3_type")
QVR_WEAPON_KEY(Hotspot3X, "hs3_x")
QVR_WEAPON_KEY(Hotspot3Y, "hs3_y")
QVR_WEAPON_KEY(Hotspot3Z, "hs3_z")
QVR_WEAPON_KEY(Hotspot3Bias, "hs3_bias")
QVR_WEAPON_KEY(Hotspot4Type, "hs4_type")
QVR_WEAPON_KEY(Hotspot4X, "hs4_x")
QVR_WEAPON_KEY(Hotspot4Y, "hs4_y")
QVR_WEAPON_KEY(Hotspot4Z, "hs4_z")
QVR_WEAPON_KEY(Hotspot4Bias, "hs4_bias")
// Round 21, second pass: type 3, a cup (a two-handed pistol grip: the other hand under and round the holding hand's
// grip; no two-handed aim); and each hotspot's hand turn (the helping hand's pitch, yaw and roll there, degrees, on top
// of how it is turned by the weapon) and its style (0: the fingers and the thumb wrap round it; 1: the thumb along the
// top).
QVR_WEAPON_KEY(Hotspot1Pitch, "hs1_pitch")
QVR_WEAPON_KEY(Hotspot1Yaw, "hs1_yaw")
QVR_WEAPON_KEY(Hotspot1Roll, "hs1_roll")
QVR_WEAPON_KEY(Hotspot1Style, "hs1_style")
QVR_WEAPON_KEY(Hotspot2Pitch, "hs2_pitch")
QVR_WEAPON_KEY(Hotspot2Yaw, "hs2_yaw")
QVR_WEAPON_KEY(Hotspot2Roll, "hs2_roll")
QVR_WEAPON_KEY(Hotspot2Style, "hs2_style")
QVR_WEAPON_KEY(Hotspot3Pitch, "hs3_pitch")
QVR_WEAPON_KEY(Hotspot3Yaw, "hs3_yaw")
QVR_WEAPON_KEY(Hotspot3Roll, "hs3_roll")
QVR_WEAPON_KEY(Hotspot3Style, "hs3_style")
QVR_WEAPON_KEY(Hotspot4Pitch, "hs4_pitch")
QVR_WEAPON_KEY(Hotspot4Yaw, "hs4_yaw")
QVR_WEAPON_KEY(Hotspot4Roll, "hs4_roll")
QVR_WEAPON_KEY(Hotspot4Style, "hs4_style")
// Round 21, second pass: the slot (1..32, as the cvars' _NN; 0 none) whose settings this weapon uses (the author:
// "inherit all the offsets from the base model"): every key but the model's name, this one and the models' anchors
// (points on its own model), wherever this weapon's own is still its default. The other ammo's models inherit their base's.
QVR_WEAPON_KEY(InheritFrom, "inherit")
// Round 21, third pass (tuning): the hand and the weapon moved and turned together, last, in the controller's aim frame
// (x forward, y left, z up; degrees: pitch up, yaw left, roll), about the controller's point: a real placement change
// (the muzzle, the aim and the melee points follow). As for the main hand; mirrored for the off hand.
QVR_WEAPON_KEY(WholeX, "whole_x")
QVR_WEAPON_KEY(WholeY, "whole_y")
QVR_WEAPON_KEY(WholeZ, "whole_z")
QVR_WEAPON_KEY(WholePitch, "whole_pitch")
QVR_WEAPON_KEY(WholeYaw, "whole_yaw")
QVR_WEAPON_KEY(WholeRoll, "whole_roll")
// The drawn hand alone moved and turned on the weapon (the same frame; turned about the palm's middle): the weapon, its
// muzzle and aim stay, the fingers wrap it again.
QVR_WEAPON_KEY(HandOnlyX, "hand_only_x")
QVR_WEAPON_KEY(HandOnlyY, "hand_only_y")
QVR_WEAPON_KEY(HandOnlyZ, "hand_only_z")
QVR_WEAPON_KEY(HandOnlyPitch, "hand_only_pitch")
QVR_WEAPON_KEY(HandOnlyYaw, "hand_only_yaw")
QVR_WEAPON_KEY(HandOnlyRoll, "hand_only_roll")
// How far the holding hand's fingers and palm may sink into the weapon: 0 not at all (they stop on its surface) .. 1
// a centimetre (default 0.3).
QVR_WEAPON_KEY(GripOverlap, "overlap")
// Per hotspot: the same overlap for the hand holding it there, and that hand's drawn pose moved and turned once it
// holds it (the holding hand's aim frame, about the palm's middle; as for the off hand helping, mirrored for the main
// hand): visual only, the grab and the aim unchanged.
QVR_WEAPON_KEY(Hotspot1Overlap, "hs1_overlap")
QVR_WEAPON_KEY(Hotspot1VisualX, "hs1_vx")
QVR_WEAPON_KEY(Hotspot1VisualY, "hs1_vy")
QVR_WEAPON_KEY(Hotspot1VisualZ, "hs1_vz")
QVR_WEAPON_KEY(Hotspot1VisualPitch, "hs1_vpitch")
QVR_WEAPON_KEY(Hotspot1VisualYaw, "hs1_vyaw")
QVR_WEAPON_KEY(Hotspot1VisualRoll, "hs1_vroll")
QVR_WEAPON_KEY(Hotspot2Overlap, "hs2_overlap")
QVR_WEAPON_KEY(Hotspot2VisualX, "hs2_vx")
QVR_WEAPON_KEY(Hotspot2VisualY, "hs2_vy")
QVR_WEAPON_KEY(Hotspot2VisualZ, "hs2_vz")
QVR_WEAPON_KEY(Hotspot2VisualPitch, "hs2_vpitch")
QVR_WEAPON_KEY(Hotspot2VisualYaw, "hs2_vyaw")
QVR_WEAPON_KEY(Hotspot2VisualRoll, "hs2_vroll")
QVR_WEAPON_KEY(Hotspot3Overlap, "hs3_overlap")
QVR_WEAPON_KEY(Hotspot3VisualX, "hs3_vx")
QVR_WEAPON_KEY(Hotspot3VisualY, "hs3_vy")
QVR_WEAPON_KEY(Hotspot3VisualZ, "hs3_vz")
QVR_WEAPON_KEY(Hotspot3VisualPitch, "hs3_vpitch")
QVR_WEAPON_KEY(Hotspot3VisualYaw, "hs3_vyaw")
QVR_WEAPON_KEY(Hotspot3VisualRoll, "hs3_vroll")
QVR_WEAPON_KEY(Hotspot4Overlap, "hs4_overlap")
QVR_WEAPON_KEY(Hotspot4VisualX, "hs4_vx")
QVR_WEAPON_KEY(Hotspot4VisualY, "hs4_vy")
QVR_WEAPON_KEY(Hotspot4VisualZ, "hs4_vz")
QVR_WEAPON_KEY(Hotspot4VisualPitch, "hs4_vpitch")
QVR_WEAPON_KEY(Hotspot4VisualYaw, "hs4_vyaw")
QVR_WEAPON_KEY(Hotspot4VisualRoll, "hs4_vroll")
// Round 21, third pass: the fingers set by hand instead of wrapped by the solver (the escape hatch: 0 automatic, 1
// manual): each finger's curl (0 open .. 1 closed) and the thumb across the palm (0 .. 1: 45 degrees), for the main hand
// on the weapon and per hotspot for the hand holding it there. The thumb's place and the finger tweaks still apply; the
// index finger still follows the trigger on the weapon's own hand.
QVR_WEAPON_KEY(FingerManual, "fgr_manual")
QVR_WEAPON_KEY(FingerCurlThumb, "fgr_curl_thumb")
QVR_WEAPON_KEY(FingerCurlIndex, "fgr_curl_index")
QVR_WEAPON_KEY(FingerCurlMiddle, "fgr_curl_middle")
QVR_WEAPON_KEY(FingerCurlRing, "fgr_curl_ring")
QVR_WEAPON_KEY(FingerCurlPinky, "fgr_curl_pinky")
QVR_WEAPON_KEY(FingerThumbAcross, "fgr_thumb_across")
QVR_WEAPON_KEY(Hotspot1Manual, "hs1_manual")
QVR_WEAPON_KEY(Hotspot1CurlThumb, "hs1_curl_thumb")
QVR_WEAPON_KEY(Hotspot1CurlIndex, "hs1_curl_index")
QVR_WEAPON_KEY(Hotspot1CurlMiddle, "hs1_curl_middle")
QVR_WEAPON_KEY(Hotspot1CurlRing, "hs1_curl_ring")
QVR_WEAPON_KEY(Hotspot1CurlPinky, "hs1_curl_pinky")
QVR_WEAPON_KEY(Hotspot1ThumbAcross, "hs1_thumb_across")
QVR_WEAPON_KEY(Hotspot2Manual, "hs2_manual")
QVR_WEAPON_KEY(Hotspot2CurlThumb, "hs2_curl_thumb")
QVR_WEAPON_KEY(Hotspot2CurlIndex, "hs2_curl_index")
QVR_WEAPON_KEY(Hotspot2CurlMiddle, "hs2_curl_middle")
QVR_WEAPON_KEY(Hotspot2CurlRing, "hs2_curl_ring")
QVR_WEAPON_KEY(Hotspot2CurlPinky, "hs2_curl_pinky")
QVR_WEAPON_KEY(Hotspot2ThumbAcross, "hs2_thumb_across")
QVR_WEAPON_KEY(Hotspot3Manual, "hs3_manual")
QVR_WEAPON_KEY(Hotspot3CurlThumb, "hs3_curl_thumb")
QVR_WEAPON_KEY(Hotspot3CurlIndex, "hs3_curl_index")
QVR_WEAPON_KEY(Hotspot3CurlMiddle, "hs3_curl_middle")
QVR_WEAPON_KEY(Hotspot3CurlRing, "hs3_curl_ring")
QVR_WEAPON_KEY(Hotspot3CurlPinky, "hs3_curl_pinky")
QVR_WEAPON_KEY(Hotspot3ThumbAcross, "hs3_thumb_across")
QVR_WEAPON_KEY(Hotspot4Manual, "hs4_manual")
QVR_WEAPON_KEY(Hotspot4CurlThumb, "hs4_curl_thumb")
QVR_WEAPON_KEY(Hotspot4CurlIndex, "hs4_curl_index")
QVR_WEAPON_KEY(Hotspot4CurlMiddle, "hs4_curl_middle")
QVR_WEAPON_KEY(Hotspot4CurlRing, "hs4_curl_ring")
QVR_WEAPON_KEY(Hotspot4CurlPinky, "hs4_curl_pinky")
QVR_WEAPON_KEY(Hotspot4ThumbAcross, "hs4_thumb_across")
// After the posing test: the direction the weapon's shots, projectiles and beams go, turned from the hand's aim (degrees:
// pitch up, yaw left; in the aim frame; yaw mirrored for the off hand) without moving anything drawn: the shot line
// (Show Controller Laser's red, the posing mode's) lined up with the sights. weapons::shotAngles.
QVR_WEAPON_KEY(ShotPitch, "shot_pitch")
QVR_WEAPON_KEY(ShotYaw, "shot_yaw")
// The weapon as it sits in a holster (the author: "tweak the positions per weapon"), per kind of holster (the hips, the
// chest, the back: the weapon lies differently on each), on top of the holster's own place and turn: moved (units: X off
// the body, Y outwards, Z up) and turned about its grip (degrees: pitch tips its top off the body, yaw turns it
// outwards, roll tips its top outwards) in the holster's frame; Y, yaw and roll mirrored for the left holsters, as the
// holsters' own sliders. Drawn only: where the hand reaches for the holster stays. weapons::holsteredPose.
QVR_WEAPON_KEY(HipHolsterX, "hol_hip_x")
QVR_WEAPON_KEY(HipHolsterY, "hol_hip_y")
QVR_WEAPON_KEY(HipHolsterZ, "hol_hip_z")
QVR_WEAPON_KEY(HipHolsterPitch, "hol_hip_pitch")
QVR_WEAPON_KEY(HipHolsterYaw, "hol_hip_yaw")
QVR_WEAPON_KEY(HipHolsterRoll, "hol_hip_roll")
QVR_WEAPON_KEY(UpperHolsterX, "hol_upper_x")
QVR_WEAPON_KEY(UpperHolsterY, "hol_upper_y")
QVR_WEAPON_KEY(UpperHolsterZ, "hol_upper_z")
QVR_WEAPON_KEY(UpperHolsterPitch, "hol_upper_pitch")
QVR_WEAPON_KEY(UpperHolsterYaw, "hol_upper_yaw")
QVR_WEAPON_KEY(UpperHolsterRoll, "hol_upper_roll")
QVR_WEAPON_KEY(ShoulderHolsterX, "hol_shoulder_x")
QVR_WEAPON_KEY(ShoulderHolsterY, "hol_shoulder_y")
QVR_WEAPON_KEY(ShoulderHolsterZ, "hol_shoulder_z")
QVR_WEAPON_KEY(ShoulderHolsterPitch, "hol_shoulder_pitch")
QVR_WEAPON_KEY(ShoulderHolsterYaw, "hol_shoulder_yaw")
QVR_WEAPON_KEY(ShoulderHolsterRoll, "hol_shoulder_roll")
// Weight's spring model (vr_weight.cpp; ROUND21.md, "Weight: spring model, stamina, held object offsets; explosive
// boxes"): the weapon's mass (kg; 0: none, as the empty hand's), its centre of mass ahead of the grip (cm, along the aim)
// and its length (cm: with the mass, how sluggishly it turns). Weapon Weights page (vr_menu.cpp).
QVR_WEAPON_KEY(Mass, "w_mass")
QVR_WEAPON_KEY(Balance, "w_com")
QVR_WEAPON_KEY(Span, "w_len")
// Round 21, "Spring only; Weapon Weights and Held Object Weights; weight and damage": the weapon's own multipliers of
// the spring's global settings (Aiming: Spring: vr_weight_spring_*), and of its melee and thrown damage. Every slot's
// default is 1 (weapons.txt's defaults), so a config without them is unchanged.
QVR_WEAPON_KEY(SpringStiffness, "w_stiff")
QVR_WEAPON_KEY(SpringDamping, "w_damp")
QVR_WEAPON_KEY(SpringStrength, "w_strength")
QVR_WEAPON_KEY(SpringSag, "w_sag")
QVR_WEAPON_KEY(SpringSwing, "w_swing")
QVR_WEAPON_KEY(SpringTwoHanded, "w_2h")
QVR_WEAPON_KEY(SpringSnap, "w_snap")
// Times Aiming: Roll Weight (vr_weight_spring_roll) for the weapon: its inertia about its barrel, how slowly a roll of the
// wrist turns it (NOTES.md vrfiringrange_2026-10-02_01-14-49; 1 by default: weapons.txt's defaults).
QVR_WEAPON_KEY(SpringRoll, "w_roll")
QVR_WEAPON_KEY(MeleeDamage, "w_meleedmg")
QVR_WEAPON_KEY(ThrowDamage, "w_throwdmg")
// Sticky grips (ROUND21.md, "Heavy weapons: wrenched out, sticky grips, heavy melee"): each hotspot's Stickiness, times how
// far the helping hand may go off it (and out of line, and past the muzzle) once it holds it (Hotspot field 23:
// weapons::Hotspot::sticky; 1 by default: weapons.txt's defaults).
QVR_WEAPON_KEY(Hotspot1Sticky, "hs1_sticky")
QVR_WEAPON_KEY(Hotspot2Sticky, "hs2_sticky")
QVR_WEAPON_KEY(Hotspot3Sticky, "hs3_sticky")
QVR_WEAPON_KEY(Hotspot4Sticky, "hs4_sticky")
// Cup grips that aim (his voice notes 2026-10-26): a Cup hotspot with this on, ahead of the holding hand, aims the
// weapon by the line between the hands as a Grip does (still taken and drawn by the palm); off, a cup never aims (a cup
// beside or under the holding hand never does). Hotspot field 24: weapons::Hotspot::cupAim; 0 by default, the
// Widowmaker's hotspot 1 (its front handle) on.
QVR_WEAPON_KEY(Hotspot1CupAim, "hs1_aim2h")
QVR_WEAPON_KEY(Hotspot2CupAim, "hs2_aim2h")
QVR_WEAPON_KEY(Hotspot3CupAim, "hs3_aim2h")
QVR_WEAPON_KEY(Hotspot4CupAim, "hs4_aim2h")
// Round 21, "Thrown axes: blade leniency, spin in the air": times Throwing and Physics' Spin Alignment
// (vr_throw_spin_align) for the weapon thrown (Weapon Weights; 1 by default: weapons.txt's defaults).
QVR_WEAPON_KEY(SpinAlign, "w_spinalign")
// Round 21, "Heavy throws' hits, spin and two-handed throws" (NOTES.md vrfiringrange_2026-10-02_01-12-39): the weapon's
// own multipliers of its hits' speed thresholds and damage curves (Weapon Weights: Damage Thresholds and Curves): thrown
// (the least speed it hurts at, weight::throwHitSpeed; the damage's rise with speed, weight::throwDamage) and in melee
// (every speed a blow with it needs, QC VR_Weight_Leniency; how fast its strength rises over that, VR_Melee_Decide).
// 1 by default (weapons.txt's defaults).
QVR_WEAPON_KEY(ThrowMinSpeed, "w_throwmin")
QVR_WEAPON_KEY(ThrowCurve, "w_throwcurve")
QVR_WEAPON_KEY(MeleeMinSpeed, "w_meleemin")
QVR_WEAPON_KEY(MeleeCurve, "w_meleecurve")
// Weapon effects (vr_weaponfx.cpp; ROUND21.md, "Weapon effects: recoil, muzzle flash, tracers"; Weapon Offsets >
// Effects): the programmatic recoil of the drawn weapon (looks only: the aim and the shots stay; ROUND23.md, "Recoil in
// cm, degrees and seconds"): how far a shot kicks it back (Kick Back, cm; 0 none), how far it tips its muzzle up (Muzzle
// Rise, degrees, and a quarter of that turned sideways; 0 none) and the seconds from that peak back to rest (Return
// Time; the kick up before it takes min(35 ms, 3/7 of it)), all times Weapon Effects' Recoil Amount (vr_recoil_amount);
// 0, 0 and 0.115 s by default (weapons.txt's defaults); held in both hands each (and the haptic punch) times its Weapon Effects'
// Two-Handed slider (vr_recoil_2h_kick, _rise, _time, _haptic) times the weapon's own (Recoil2hKick, ...; 1:
// weaponfx::twoHandRecoil; until 2026-10-31 one for all four, fx_recoil_2h: an old config's value goes to all four,
// retiredTwoHandLine). Until vr_wofs_version 74
// these were multipliers (fx_recoil on/off,
// fx_recoil_str, fx_recoil_kick, fx_recoil_rise, fx_recoil_time, fx_recoil_rec and the global vr_recoil_kick and
// vr_recoil_rise): retired, an old config's values converted (vr_weapons.cpp takeRecoilUnits74). A muzzle flash at its
// muzzle anchor (0 off, 1 on), its size (times the shotgun's) and the seconds it shows; its hitscan shots' bullet tracers
// (0 as Weapon Effects' vr_tracers, 1 never, 2 always), times Weapon Effects' speed, length, thickness and chance a
// pellet, and a colour of its own (0 Weapon Effects', 1 R G B 0..1). Every slot's multipliers default to 1 and its colour
// to the global one (weapons.txt's defaults).
QVR_WEAPON_KEY(RecoilKick, "fx_kick_cm")
QVR_WEAPON_KEY(RecoilRise, "fx_rise_deg")
QVR_WEAPON_KEY(RecoilReturn, "fx_return_s")
QVR_WEAPON_KEY(Recoil2hKick, "fx_2h_kick")
QVR_WEAPON_KEY(Recoil2hRise, "fx_2h_rise")
QVR_WEAPON_KEY(Recoil2hTime, "fx_2h_time")
QVR_WEAPON_KEY(Recoil2hHaptic, "fx_2h_haptic")
QVR_WEAPON_KEY(Flash, "fx_flash")
QVR_WEAPON_KEY(FlashSize, "fx_flash_size")
QVR_WEAPON_KEY(FlashTime, "fx_flash_time")
QVR_WEAPON_KEY(Tracers, "fx_tracers")
QVR_WEAPON_KEY(TracerSpeed, "fx_tr_speed")
QVR_WEAPON_KEY(TracerLength, "fx_tr_len")
QVR_WEAPON_KEY(TracerWidth, "fx_tr_width")
QVR_WEAPON_KEY(TracerChance, "fx_tr_chance")
QVR_WEAPON_KEY(TracerOwnColour, "fx_tr_owncol")
QVR_WEAPON_KEY(TracerRed, "fx_tr_r")
QVR_WEAPON_KEY(TracerGreen, "fx_tr_g")
QVR_WEAPON_KEY(TracerBlue, "fx_tr_b")
// The other hand gripping the weapon held by its handle away from its hotspots (vr_weapon_grab_anywhere): -1 as
// vr_weapon_anygrip_mode, 0 steers the aim as a foregrip, 1 supports its weight only, 2 a rigid two-handed hold.
QVR_WEAPON_KEY(AnyGripMode, "anygrip_mode")
// The virtual stock (vr_2h_mode 2; vr_twohand.cpp applyHotspots; Weapon Offsets > Virtual Stock): degrees turned on top of
// the two-handed aim (and its Aim Pitch, Yaw and Roll) as far as the stock is engaged (the holding hand at the shoulder):
// pitch up, yaw left, roll right (the off hand's yaw and roll mirrored). 0 by default (weapons.txt's defaults): no migration.
QVR_WEAPON_KEY(StockPitch, "stock_pitch")
QVR_WEAPON_KEY(StockYaw, "stock_yaw")
QVR_WEAPON_KEY(StockRoll, "stock_roll")
// The flashlight clipped on this weapon (vr_flashlight.cpp gunPose; Weapon Offsets > Flashlight), on top of the global
// vr_flashlight_gun_forward/_up/_out and the spot fitted to the model: metres forward, up and out (away from the body),
// and degrees turned about the torch's middle: pitch up, yaw out, roll out (its top away from the body; the off hand's
// mirrored like the out). The gun's clip zone moves with the position. 0 by default (weapons.txt's defaults): no migration.
QVR_WEAPON_KEY(TorchForward, "torch_fwd")
QVR_WEAPON_KEY(TorchUp, "torch_up")
QVR_WEAPON_KEY(TorchOut, "torch_out")
QVR_WEAPON_KEY(TorchPitch, "torch_pitch")
QVR_WEAPON_KEY(TorchYaw, "torch_yaw")
QVR_WEAPON_KEY(TorchRoll, "torch_roll")
// Whether the flashlight clips on this weapon at all (Weapon Offsets > Flashlight, Flashlight Can Clip On): 1 (every
// slot's default, weapons.txt) it can (its zone, B/Y, Clip on Gun When Let Go); 0 none of them. Off by default for the
// melee weapons below (weapons.txt: the axe, Mjolnir, the swords, the chainsaw, the crowbar).
QVR_WEAPON_KEY(TorchClip, "torch_clip")
// Heavy handling (ROUND22.md, "Heavy weapons: struggle, droop, recoil, two-handed swings"; vr_weight.cpp; Weapon Weights >
// Heavy Handling): the weapon's weight class, its Heft: 0 light (no struggle) .. 1 the heaviest (the laser cannon); -1
// (every slot's default, weapons.txt) from its mass (vr_heavy_auto_from .. _full kg). One-handed, a heavy weapon lags an
// abrupt wrist snap springily, its barrel droops and its shots kick it up; two-handed, slower deliberate swings strike
// (vr_heavy_*). Its own multipliers of the droop and the one-handed kick (1 by default; 0 off for this weapon).
// New cvars: no migration.
QVR_WEAPON_KEY(Heft, "w_heft")
QVR_WEAPON_KEY(Droop, "w_droop")
QVR_WEAPON_KEY(HeavyRecoil, "w_hrecoil")
// The blade's shape for melee (ROUND22.md, "Blade shape editor"; vr_blade.hpp; edited in VR by Weapon Offsets > Edit Blade
// Shape, vr_bladeedit.cpp; QC VR_Melee_Line, _At, _Sweep through meleeblade). The blow line runs from the hand to the tip
// (the Muzzle anchor and offset, as before); each key's default keeps the shape the code gave before (weapons.txt's defaults):
// the near end (pommel, butt, handle's end) behind the hand, a share of the hand-to-tip line (-1: the kind's own, QC
// VR_Melee_NearBack); the blade's width (times the swept points' radius, VR_MELEE_RADIUS); the curve (Spine 0: the
// model's own, the curved AD swords' made spine, else straight; 1: a smooth spline through five points at 1/6 .. 5/6 of
// the line, each off it along the line's normal in the blade's plane and along the model's +y, shares of its length);
// a head's box (Head 0: the model's own, the AD hammer's and mace's; 1: the box below, shares of the line: x along it
// from the hand, y along its normal in the weapon's plane, z along the model's +y). New cvars: no migration.
QVR_WEAPON_KEY(BladeNear, "bl_near")
QVR_WEAPON_KEY(BladeWidth, "bl_width")
QVR_WEAPON_KEY(BladeSpine, "bl_spine")
QVR_WEAPON_KEY(BladeSpine1Off, "bl_s1_off")
QVR_WEAPON_KEY(BladeSpine1Side, "bl_s1_side")
QVR_WEAPON_KEY(BladeSpine2Off, "bl_s2_off")
QVR_WEAPON_KEY(BladeSpine2Side, "bl_s2_side")
QVR_WEAPON_KEY(BladeSpine3Off, "bl_s3_off")
QVR_WEAPON_KEY(BladeSpine3Side, "bl_s3_side")
QVR_WEAPON_KEY(BladeSpine4Off, "bl_s4_off")
QVR_WEAPON_KEY(BladeSpine4Side, "bl_s4_side")
QVR_WEAPON_KEY(BladeSpine5Off, "bl_s5_off")
QVR_WEAPON_KEY(BladeSpine5Side, "bl_s5_side")
QVR_WEAPON_KEY(BladeHead, "bl_head")
QVR_WEAPON_KEY(BladeHeadLoX, "bl_head_lo_x")
QVR_WEAPON_KEY(BladeHeadLoY, "bl_head_lo_y")
QVR_WEAPON_KEY(BladeHeadLoZ, "bl_head_lo_z")
QVR_WEAPON_KEY(BladeHeadHiX, "bl_head_hi_x")
QVR_WEAPON_KEY(BladeHeadHiY, "bl_head_hi_y")
QVR_WEAPON_KEY(BladeHeadHiZ, "bl_head_hi_z")
// Where the blade proper (the part that strikes: an axe's head, a sword's blade past its guard) starts (ROUND22.md, "The
// blade's start; the axe's head box"): From the share of the line from the hand (-1: the kind's own, QC VR_Melee_Layout's
// first striking point, nothing bent), FromOff and FromSide the point off the line (shares of its length, as the curve's
// points): the line runs straight from the hand to it and from it to the tip, its striking points from it to the tip.
// New cvars: no migration.
QVR_WEAPON_KEY(BladeFrom, "bl_from")
QVR_WEAPON_KEY(BladeFromOff, "bl_from_off")
QVR_WEAPON_KEY(BladeFromSide, "bl_from_side")
// The line's two ends of its own (ROUND22.md, "The blade's line start"): Org the line's start (Edit Blade Shape's Line
// Start) off the holding hand, Tip the line's far end off the Muzzle anchor and offset (a gun's Tip handle: its Muzzle
// offset is where its shots start, so a gun's bash line has its own); both in units along the hand's forward, right, up
// (right mirrored in the off hand, as its model is). 0 0 0: the hand and the muzzle, as before. New cvars: no migration.
QVR_WEAPON_KEY(BladeOrgX, "bl_org_x")
QVR_WEAPON_KEY(BladeOrgY, "bl_org_y")
QVR_WEAPON_KEY(BladeOrgZ, "bl_org_z")
QVR_WEAPON_KEY(BladeTipX, "bl_tip_x")
QVR_WEAPON_KEY(BladeTipY, "bl_tip_y")
QVR_WEAPON_KEY(BladeTipZ, "bl_tip_z")
// The weapon's haptic punch (ROUND23.md, "Punchier gunfire"; Weapon Offsets > Effects, Haptic Punch; vr_gunsfx.cpp
// punchAt): times the shot's buzz in the hand (a sharp spike as hard as its recoil, then a short tail as long as its
// kick). 1 by default (weapons.txt's defaults).
QVR_WEAPON_KEY(HapticPunch, "fx_punch")
// The weapon's stream shake (ROUND23.md, "Stream shake"; Weapon Offsets > Effects, Stream Shake and Stream Rumble;
// vr_weaponfx.cpp streamShake): while a stream weapon fires (the AD flamethrower's and poison sprayer's flames: their
// QC's weaponfired hand + 2 each flame), the drawn weapon trembles a little (a share of its Recoil Kick Back and Muzzle
// Rise, fading out over its kick's time as it stops) and the hand rumbles lightly. 0 by default (weapons.txt's defaults):
// on for those two (below). New cvars: no migration.
QVR_WEAPON_KEY(StreamShake, "fx_stream_shake")
QVR_WEAPON_KEY(StreamHaptic, "fx_stream_haptic")
// The weapon's gunfire particles (ROUND23.md, "Gunfire VFX settings"; Weapon Offsets > Effects; vr_gunfx.cpp emit,
// barrelWisps): times Weapon Effects > Gunfire Particles' Spark Amount, Spark Size, Smoke Amount, Smoke Size and its
// muzzle wisps' opacity (Wisp Amount), for this weapon's shots (yours; a monster's gun by its model, else its weapon
// id's). 1 by default (weapons.txt's defaults). New cvars: no migration.
QVR_WEAPON_KEY(GunfxSparks, "fx_gfx_sparks")
QVR_WEAPON_KEY(GunfxSparkSize, "fx_gfx_spark_size")
QVR_WEAPON_KEY(GunfxSmoke, "fx_gfx_smoke")
QVR_WEAPON_KEY(GunfxSmokeSize, "fx_gfx_smoke_size")
QVR_WEAPON_KEY(GunfxWisps, "fx_gfx_wisps")
// The weapon's muzzle wisps (ROUND23.md, "Persistent muzzle wisps"; vr_gunfx.cpp wispLook): a shot's share of the full
// wisps (0, the default: it leaves none), their opacity at full (before Muzzle Wisp Opacity) and its barrels (1 or 2:
// one wisp each). Data only (weapons.txt), no menu row.
QVR_WEAPON_KEY(GunfxWispCharge, "fx_gfx_wisp_charge")
QVR_WEAPON_KEY(GunfxWispAlpha, "fx_gfx_wisp_alpha")
QVR_WEAPON_KEY(GunfxWispBarrels, "fx_gfx_wisp_barrels")
// The weapon's barrel heat (ROUND23.md, "Barrel heat settings"; Weapon Offsets > Effects; vr_gunfx.cpp heatShot,
// heatCool): times the heat its shots add (Weapon Effects' Heat-Up Time divided by it; 0: it never heats) and times
// Weapon Effects' Cool-Down Time (0: it cools at once). 1 by default (weapons.txt's defaults). New cvars: no migration.
QVR_WEAPON_KEY(GunfxHeat, "fx_gfx_heat")
QVR_WEAPON_KEY(GunfxCool, "fx_gfx_cool")
// The weapon's barrel glow's length (ROUND23.md, "Barrel glow on the metal"; Weapon Offsets > Effects, Barrel Glow
// Length; vr_hotblade.cpp barrelBlade): the share of the gun's length (its grip to its muzzle, along the barrel) that
// glows when hot, from the muzzle back, fading out towards its far end. 0.5 by default (weapons.txt's defaults).
QVR_WEAPON_KEY(GunfxBarrel, "fx_gfx_barrel")
// The weapon's barrel mask (ROUND23.md, "Barrel glow: the fire frames, the revolver, a painted barrel"; Weapon Offsets >
// Effects, Paint Barrel Mask; vr_barrelmask.hpp): the triangles of its model that glow when hot, "<triangles>:<bits>"
// (pre-marked in weapons.txt by Misc/quakevr/barrel_masks.py); "0" (the default): none, the glow's ramp alone.
QVR_WEAPON_KEY(GunfxBarrelMask, "fx_gfx_mask")
// Reloading (the named per-weapon values: vr_weaponvalues.inc names each gun's by its old cvar). Each gun's load
// point moved (model units: +x forward, +y left, +z up) and its radius (world units); a magazine's reference point
// moved (its model units) and its radius; a magazine gun's well moved and turned (visual only); the other hand
// holding its attached magazine: moved, turned, Fingers (0 fitted, 1 the curls).
QVR_WEAPON_KEY(ReloadPortX, "reload_port_x")
QVR_WEAPON_KEY(ReloadPortY, "reload_port_y")
QVR_WEAPON_KEY(ReloadPortZ, "reload_port_z")
QVR_WEAPON_KEY(ReloadPortRadius, "reload_port_radius")
QVR_WEAPON_KEY(ReloadMagX, "reload_mag_x")
QVR_WEAPON_KEY(ReloadMagY, "reload_mag_y")
QVR_WEAPON_KEY(ReloadMagZ, "reload_mag_z")
QVR_WEAPON_KEY(ReloadMagRadius, "reload_mag_radius")
QVR_WEAPON_KEY(ReloadWellX, "reload_well_x")
QVR_WEAPON_KEY(ReloadWellY, "reload_well_y")
QVR_WEAPON_KEY(ReloadWellZ, "reload_well_z")
QVR_WEAPON_KEY(ReloadWellPitch, "reload_well_pitch")
QVR_WEAPON_KEY(ReloadWellYaw, "reload_well_yaw")
QVR_WEAPON_KEY(ReloadWellRoll, "reload_well_roll")
QVR_WEAPON_KEY(ReloadHoldX, "reload_hold_x")
QVR_WEAPON_KEY(ReloadHoldY, "reload_hold_y")
QVR_WEAPON_KEY(ReloadHoldZ, "reload_hold_z")
QVR_WEAPON_KEY(ReloadHoldPitch, "reload_hold_pitch")
QVR_WEAPON_KEY(ReloadHoldYaw, "reload_hold_yaw")
QVR_WEAPON_KEY(ReloadHoldRoll, "reload_hold_roll")
QVR_WEAPON_KEY(ReloadHoldFingers, "reload_hold_fingers")
QVR_WEAPON_KEY(ReloadHoldCurlThumb, "reload_hold_curl_thumb")
QVR_WEAPON_KEY(ReloadHoldCurlIndex, "reload_hold_curl_index")
QVR_WEAPON_KEY(ReloadHoldCurlMiddle, "reload_hold_curl_middle")
QVR_WEAPON_KEY(ReloadHoldCurlRing, "reload_hold_curl_ring")
QVR_WEAPON_KEY(ReloadHoldCurlPinky, "reload_hold_curl_pinky")
// Embedded blades (ROUND23.md, "Embedded blades"; vr_axestick.cpp): how the weapon sticks in what it meets: 0 not at all,
// 1 by its point (its tip: the Muzzle anchor and offset; its line from the hand there; the blade's start: BladeFrom), 2 by
// its edge. An edge's weapon: its blades' edges in its model's own space (an alias model's vertices as scale * v +
// scale_origin): each one's ends (A, B) and the way it faces (O: out of the edge, square to it, in the blade's plane), the
// head's middle on the handle's axis and the handle's far end (Butt). New keys: no migration.
QVR_WEAPON_KEY(Embed, "embed")
QVR_WEAPON_KEY(EmbedE1AX, "em_e1a_x")
QVR_WEAPON_KEY(EmbedE1AY, "em_e1a_y")
QVR_WEAPON_KEY(EmbedE1AZ, "em_e1a_z")
QVR_WEAPON_KEY(EmbedE1BX, "em_e1b_x")
QVR_WEAPON_KEY(EmbedE1BY, "em_e1b_y")
QVR_WEAPON_KEY(EmbedE1BZ, "em_e1b_z")
QVR_WEAPON_KEY(EmbedE1OX, "em_e1o_x")
QVR_WEAPON_KEY(EmbedE1OY, "em_e1o_y")
QVR_WEAPON_KEY(EmbedE1OZ, "em_e1o_z")
QVR_WEAPON_KEY(EmbedE2AX, "em_e2a_x")
QVR_WEAPON_KEY(EmbedE2AY, "em_e2a_y")
QVR_WEAPON_KEY(EmbedE2AZ, "em_e2a_z")
QVR_WEAPON_KEY(EmbedE2BX, "em_e2b_x")
QVR_WEAPON_KEY(EmbedE2BY, "em_e2b_y")
QVR_WEAPON_KEY(EmbedE2BZ, "em_e2b_z")
QVR_WEAPON_KEY(EmbedE2OX, "em_e2o_x")
QVR_WEAPON_KEY(EmbedE2OY, "em_e2o_y")
QVR_WEAPON_KEY(EmbedE2OZ, "em_e2o_z")
QVR_WEAPON_KEY(EmbedHeadX, "em_head_x")
QVR_WEAPON_KEY(EmbedHeadY, "em_head_y")
QVR_WEAPON_KEY(EmbedHeadZ, "em_head_z")
QVR_WEAPON_KEY(EmbedButtX, "em_butt_x")
QVR_WEAPON_KEY(EmbedButtY, "em_butt_y")
QVR_WEAPON_KEY(EmbedButtZ, "em_butt_z")
#endif


#ifdef QVR_WEAPON_OWN
// An inheriting slot's defaults that are its own (vr_weapons.cpp ownDefaults): QVR_WEAPON_OWN(slot, key) keeps that
// slot's default of the key its own, never its parent's. Without it every default of an inheriting slot is a
// placeholder that gives way to the parent's value (ROUND21: "own" is what differs from the default), as before.
// None now: the Widowmaker's and the Plasma Gun's (slots 34, 35) went with their inheriting (round 22, adnoinherit:
// no AD weapon inherits; every value of theirs is their own default).
#endif
