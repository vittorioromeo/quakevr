// vr_fields.inc -- Quake VR entity fields used by the engine: QVR_FIELD(name).
// Declared in QC after end_sys_fields (QC/defs.qc, QC/vr_sys_fields.qc) and resolved by name
// when progs load; a field missing from the progs has offset -1.

// Head, hands and weapons (written from the VR usercmd).
QVR_FIELD(v_viewangle)
QVR_FIELD(vryaw)
QVR_FIELD(handpos)
QVR_FIELD(handrot)
QVR_FIELD(handvel)
QVR_FIELD(handthrowvel)
QVR_FIELD(handvelmag)
QVR_FIELD(handavel)
QVR_FIELD(handthrowpos)
QVR_FIELD(handthrowage)
QVR_FIELD(handthrowflick)
QVR_FIELD(offhandpos)
QVR_FIELD(offhandrot)
QVR_FIELD(offhandvel)
QVR_FIELD(offhandthrowvel)
QVR_FIELD(offhandvelmag)
QVR_FIELD(offhandavel)
QVR_FIELD(offhandthrowpos)
QVR_FIELD(offhandthrowage)
QVR_FIELD(offhandthrowflick)
QVR_FIELD(headvel)
QVR_FIELD(headpos)
QVR_FIELD(muzzlepos)
QVR_FIELD(offmuzzlepos)
QVR_FIELD(loadportpos)
QVR_FIELD(vr_pouch_kind)
QVR_FIELD(vr_pouch_count)
QVR_FIELD(stl_hud) // the stealth gem's readings (QC vr_stealth.qc; STAT_QVR_STEALTH)
QVR_FIELD(offloadportpos)
QVR_FIELD(loadportaxis) // the way a round lies going in at each gun's port (vr_reload.qc VR_Reload_LooseFrame)
QVR_FIELD(offloadportaxis)
QVR_FIELD(loadportface) // and the way its opening faces
QVR_FIELD(offloadportface)
QVR_FIELD(vr_ammo_pass) // a loose round passing through a player's guns to their ports: the hands (vr_reload.qc; vr_box3d.cpp reachSkips)
QVR_FIELD(vr_ammo_passer) // and the player
QVR_FIELD(vr_ammo_passgun) // a loose round passing through a gun lying about to its port: that gun (vr_reload.qc VR_Reload_PropsFrame; vr_box3d.cpp preSolve)
QVR_FIELD(magboxpos) // the main hand's gun's attached magazine, a box: its middle and half-axes (vr_reload.qc)
QVR_FIELD(magboxx)
QVR_FIELD(magboxy)
QVR_FIELD(magboxz)
QVR_FIELD(offmagboxpos)
QVR_FIELD(offmagboxx)
QVR_FIELD(offmagboxy)
QVR_FIELD(offmagboxz)
QVR_FIELD(shotrot)
QVR_FIELD(offshotrot)
QVR_FIELD(vrbits0)
QVR_FIELD(teleport_target)
QVR_FIELD(offhand_hotspot)
QVR_FIELD(sawcord)
QVR_FIELD(handdrop)
QVR_FIELD(mainhand_hotspot)
QVR_FIELD(roomscalemove)
QVR_FIELD(button3)

// Hand and weapon touching.
QVR_FIELD(handtouch)
QVR_FIELD(vr_wpntouch)
QVR_FIELD(touchinghand)
QVR_FIELD(handtouch_hand)
QVR_FIELD(handtouch_ent)
QVR_FIELD(ishuman)

// Physics.
QVR_FIELD(think2)
QVR_FIELD(vr_rigid)
QVR_FIELD(vr_spin)
QVR_FIELD(vr_pickup_spin) // a weapon pickup drawn as its prop (no EF_ROTATE): drawn spinning as a pickup while not a rigid body (U_QVR_SPIN)
QVR_FIELD(vr_impact) // a prop's hits (vr_box3d.cpp): .void(float speed) called with how hard it hit something (m/s)
QVR_FIELD(throwhit) // QVR_THROWHIT_NEVER_HIT (0) while a thrown thing's throw lasts (vr_box3d.cpp's hit box)
QVR_FIELD(vr_blocksight) // a solid prop monsters can't see through (a crate, an explosive box: sightblocked, vr_crates.cpp)
QVR_FIELD(vr_shot_radius)  // a grenade shots set off (vr_hitmodel.cpp VR_ShotTargetClip): the half size of the cube shots meet round its middle; 0 none
QVR_FIELD(vr_shot_ownfrom) // and when its owner's own shots start to meet it (not as it leaves his launcher or hand)
QVR_FIELD(nextthink2)
QVR_FIELD(lastwatertime)

// A thrown axe stuck in something (vr_axestick.cpp, QC/vr_axestick.qc).
QVR_FIELD(vr_stick_kind)   // 0 not stuck; 1 the level or a brush entity; 2 a prop; 3 a monster's model; 4 a monster's box; -1 came out
QVR_FIELD(vr_stick_ent)    // what it is in (world: the level)
QVR_FIELD(vr_stick_ofs)    // its origin in that thing's frame
QVR_FIELD(vr_stick_ang)    // its turn in that frame (angles as a brush model's)
QVR_FIELD(vr_stick_anchor) // kind 3: the model's triangle, and where on it (tri, u, v)
QVR_FIELD(vr_stick_edge)   // which blade
QVR_FIELD(vr_stick_at)     // the engine's, each frame: the middle of the blade's edge in the world, and
QVR_FIELD(vr_stick_dir)    // the way the blade faces (into what it is in)
QVR_FIELD(vr_stick_wiggle) // QC's: degrees the axe is rocked about its edge (a force grab's tug)
QVR_FIELD(vr_stick_vel)    // in flight: its velocity and
QVR_FIELD(vr_stick_spin)   // spin at the last step (a change: it hit something, and never sticks after)

// Model transforms (networked as entity update extras).
QVR_FIELD(model_scale)
QVR_FIELD(model_scale_origin)
QVR_FIELD(model_offset)

// Weapons, holsters and ammo (networked as stats).
QVR_FIELD(weaponflags)
QVR_FIELD(weapon2)
QVR_FIELD(weaponmodel2)
QVR_FIELD(weaponframe2)
QVR_FIELD(weaponflags2)
QVR_FIELD(weaponinst)  // the weapons' records (QC vr_weaponinst.qc): the main hand's, a weapon prop's
QVR_FIELD(weaponinst2) // the off hand's
QVR_FIELD(wi_uid)      // a record's id (networked: the hands' STAT_QVR_WEAPONUID*, a prop's U_QVR_WEAPONUID)
QVR_FIELD(wi_clip)     // a record's magazine
QVR_FIELD(weaponclipsize)
QVR_FIELD(weaponclipsize2)
QVR_FIELD(currentammo2)
QVR_FIELD(ammocounter)
QVR_FIELD(ammocounter2)
QVR_FIELD(holsterweapon0)
QVR_FIELD(holsterweapon1)
QVR_FIELD(holsterweapon2)
QVR_FIELD(holsterweapon3)
QVR_FIELD(holsterweapon4)
QVR_FIELD(holsterweapon5)
QVR_FIELD(holsterweaponmodel0)
QVR_FIELD(holsterweaponmodel1)
QVR_FIELD(holsterweaponmodel2)
QVR_FIELD(holsterweaponmodel3)
QVR_FIELD(holsterweaponmodel4)
QVR_FIELD(holsterweaponmodel5)
QVR_FIELD(holsterweaponflags0)
QVR_FIELD(holsterweaponflags1)
QVR_FIELD(holsterweaponflags2)
QVR_FIELD(holsterweaponflags3)
QVR_FIELD(holsterweaponflags4)
QVR_FIELD(holsterweaponflags5)
QVR_FIELD(holsterweaponinst0)
QVR_FIELD(holsterweaponinst1)
QVR_FIELD(holsterweaponinst2)
QVR_FIELD(holsterweaponinst3)
QVR_FIELD(holsterweaponinst4)
QVR_FIELD(holsterweaponinst5)
QVR_FIELD(mainhand_fgtarget) // force grab (vr_wpnforcegrab.qc), for its look (vr_fgfx.cpp)
QVR_FIELD(offhand_fgtarget)
QVR_FIELD(mainhand_fglocked)
QVR_FIELD(offhand_fglocked)
QVR_FIELD(mainhand_fgpulled)
QVR_FIELD(offhand_fgpulled)
QVR_FIELD(fg_state)
QVR_FIELD(fg_player)
QVR_FIELD(fg_hand)
QVR_FIELD(mainhand_held) // carrying (vr_carry.qc), drawn in the hand (vr_held.cpp)
QVR_FIELD(carry_offset) // a held prop's origin in the hand (vr_carry.qc), kept at a regrip (vr_grip.cpp)
QVR_FIELD(offhand_held)
QVR_FIELD(carry_teleported) // until then what the hands carry follows them from the player (vr_carry.qc VR_Carry_FollowTeleported): set by the VR teleport
QVR_FIELD(vr_melee_hud) // parry stamina and the counter's window, for the gadget and the counter glow (vr_melee.qc VR_Melee_Hud; vr_meleehud.cpp)
QVR_FIELD(vr_hitclass) // precise hits (vr_hitmodel.cpp): a projectile's class for its tolerance (0 a gun's, 1 the grappling hook's; -1: boxes)
QVR_FIELD(vr_stamina_used) // the stamina pool (vr_melee.qc): what is spent, when last, its coming back traced; climbing drains it (vr_climb.cpp)
QVR_FIELD(vr_stamina_time)
QVR_FIELD(vr_stamina_said)
QVR_FIELD(vr_climb_drain) // 1 while a hang drains the pool (the gadget shows it: VR_Melee_Hud)
QVR_FIELD(vr_climb_knockoff) // set by the QC: a blast or a shove knocks the player off its holds (vr_climb.cpp lets go, keeps the velocity)
QVR_FIELD(vr_foegrab_hands) // set by the engine on a player: the hands holding an enemy (vr_foegrab.cpp), bit 1 the off hand, 2 the main: no melee blows from them
QVR_FIELD(vr_foegrab_hold) // set by the engine on a monster: how firmly hands hold it now, 0..1 (0 not held): its shoves are resisted (QC vr_enemyshove.qc)
QVR_FIELD(vr_foegrab_letgo) // set by the QC on a monster: every hand holding it lets go (a shove breaking free, vr_foegrab_shove 1)
QVR_FIELD(vr_eshove_speed) // a player's slide from an enemy's shove, units/s (QC vr_enemyshove.qc; 0 none): what his hands hold is carried along (vr_foegrab.cpp)
QVR_FIELD(vr_sgib) // a small gib: when it was made (QC vr_smallgibs.qc; vr_box3d.cpp: its blow grace, vr_smallgibs_blow_grace)
QVR_FIELD(vr_sgib_id) // its serial (vr_smallgibs_trace's lines)
QVR_FIELD(vr_monster_drop) // a weapon or backpack a monster dropped: when (QC vr_carry.qc; vr_box3d.cpp: made inside a body it passes through it, vr_prop_drop_pass_inside)
QVR_FIELD(vr_prop_mass) // a prop's own mass, kg (0: its model's, Held Object Offsets): a small gib (QC vr_smallgibs.qc; vr_box3d.cpp massSetting)
QVR_FIELD(vr_xdebris) // an explosion's chunk (vr_explosiondebris.cpp): when it was made (its entry's check; 0: not one)
QVR_FIELD(attack_finished) // slow motion with the player in its own time (vr_timescale.cpp: vr_sandevistan, vr_timescale_move_realtime): its timers set in its frame run in its time
QVR_FIELD(offhand_attack_finished)
QVR_FIELD(mainhand_melee_attack_finished)
QVR_FIELD(offhand_melee_attack_finished)
QVR_FIELD(vr_corpse) // 1 dying, 2 a corpse lying still (QC combat.qc VR_Corpse_Arm; vr_box3d.cpp: its body, vr_corpse_collide)
QVR_FIELD(vr_gib) // a gib or head (QC): not a corpse (vr_box3d.cpp)
QVR_FIELD(vr_headless) // its head cut off (QC vr_decap.qc): its ragdoll is made headless, a saved game's too (vr_box3d.cpp)
QVR_FIELD(vr_limbcut) // the ragdoll bones cut off (bits; QC vr_limbs.qc, ROUND21.md "Limb gore"): made so again in a saved game (vr_box3d.cpp)
QVR_FIELD(vr_deathdoll) // a player's ragdoll body's player (an entity; QC vr_deathdoll.qc) (vr_deathview.cpp: the camera follows its head)
QVR_FIELD(vr_deathdoll_body) // a dead player's ragdoll body (an entity; QC vr_deathdoll.qc): he is not sent to the others while it is (sv_main.c)
QVR_FIELD(vr_knockdown) // a monster knocked down by a shove (QC vr_knockdown.qc): 1 a ragdoll while alive (vr_box3d.cpp), 2 getting up
QVR_FIELD(vr_shock_until) // lightning killed it or struck it dead (QC vr_shock.qc): its ragdoll convulses until then (vr_box3d.cpp, "Shocked ragdolls")
QVR_FIELD(vr_shock_len) // ... the shock's length (s): the convulsions fade over it

// The firing range's training dummy (QC vr_dummy.qc): 1 + its enemy (vr_dummy_type) while it stands, else 0.
QVR_FIELD(vr_dummy_kind)

// When the entity was made (ED_Alloc: VR_OnEdictAlloc, vr_cheats.cpp): the map's own and those placed as it loads
// before its first second is out; vr_scene_clean props (QC VR_Scene_Clean) removes the loose props made later.
QVR_FIELD(vr_born)

// Quake's drawing extensions, set by the engine on what it makes (vr_explosiondebris.cpp: a chunk's size and its fade).
QVR_FIELD(alpha)
QVR_FIELD(scale)
