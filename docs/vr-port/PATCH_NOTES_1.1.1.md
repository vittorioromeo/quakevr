# Quake VR: UNLEASHED 1.1.1: patch notes

**Version 1.1.1**, compared with 1.1.0 (whose notes, PATCH_NOTES_1.1.md, are final). This file grows with every change
that ships in 1.1.1.

## Campaigns

- **Dwell (experimental).** Dwell's two episodes, and custom maps made for Copper (such as Underdark Overbright), now
  play, by Copper's rules: Select Campaign > Dwell, and the Map Library's Copper maps. The installer offers Dwell
  (free, by Fairweather and the Dwell team), ticked by default. Dwell's own monsters (the Kamikaze, the Brute, the
  Construct and both bosses) fight in its maps, and, with Dwell installed, in any map and from the toolgun; its
  weapons and powerups work as variants of ours. Dwell's Rotary Shotgun, Crystal Lance and Rifle look like themselves
  in your hands, holsters and on armed grunts (made from Dwell's own models, with a grip to hold), the Rotary's four
  barrels turning as you fire; each shows its ammo screen in a riveted bezel on its back, as our guns do; the holster menu (flat and on the wrist) names them, and AD's Widowmaker, Shadow Axe
  and Plasma Gun too. The Crystal Lance is now a two-handed lance, made from Dwell's own pickup model: a plain pole
  with its crystal head (no handle, no buttons), held like a spear, the pole through your fist; put your other hand
  anywhere on its pole (it slides along it) to aim it, swing its crystal head as a blade, and fire its beam from the
  tip, along the pole. In flatscreen play it is held as a spear in both hands, its pose set by the Spear settings
  (Debug > Tunables, `vr_flat_pole_*`). (Settings: `vr_dw_lance_pole 0` brings back the pistol). The Rotary Shotgun is held as a
  gauntlet, your hand inside its housing as in the super nailgun, and its shell port is a block on the housing's inner
  side: hold a handful of shells from the pouch there to fill its tube (`vr_dw_rotary_gauntlet 0`: the old grip). Dwell's weapons
  now stay what they are in your holsters, through saves, level changes and coop rejoins; the Rotary Shotgun reloads
  and spins up as the Widowmaker; Haste, Berserk and the Wetsuit show their seconds left on the wrist (and the flat
  visor), and Haste and Berserk tint the screen in Dwell's colours. An armed grunt that picks up one of Dwell's
  weapons fires it as Dwell's. Fixed: the Crystal Lance's first shot ended the game with a network error. Fixed:
  picking up Dwell's weapons in its maps failed with an error and they vanished; the toolgun now lays Dwell's weapons
  and Dawn of the Machine's bloody shotguns as props, like every other weapon (a bloody one still grants its bonus
  when you take it); in Dwell's start map, the runes placed on the pedestals now open the way to the episodes'
  teleporters; the Kamikaze chases you before it bursts, and its burst shows an explosion; the Brute's ragdoll no
  longer stretches a triangle between his hands; the Sculptor no longer traps you inside it when it flies or
  teleports onto you. Dwell's music no longer plays in Quake's and the mission packs' maps while Dwell is installed,
  and Dwell's start map and later maps play their own tracks.
- **Quoth (experimental).** Quoth 2.2's own maps (Select Campaign > Quoth) and custom maps made for Quoth play by
  Arcane Dimensions' rules, so they need AD. The installer offers Quoth (free, by Kell, Necros and Preach), downloaded
  from Quaddicted and installed as released: ticked when Arcane Dimensions is chosen, greyed out without it.
- **Heavy revolver headshots pop heads in Arcane Dimensions and Quoth too.** Where shells fly as projectiles (AD's and
  Quoth's maps, or Projectile Rules set to AD's), a heavy revolver's headshot kill never popped the head, and its slug
  did a big monster no extra damage. Its slug now follows the same head pop and big-body rules as everywhere else.

## Graphics

- **Arcane Dimensions' surface relief for Dwell and Quoth.** The depth in bricks, panels and rock that AD's textures
  have now reaches Dwell's and Quoth's: the installer makes it on your PC from your own copies, with the same free
  models ("Make reliefs for Dwell and Quoth", ticked when either is installed; a few minutes on a graphics card, only
  the missing ones at later runs). It is kept in the Quake VR folder's `reliefs` folder and removed by the uninstaller.
  Already playing Dwell or Quoth in a Quake VR folder of your own? Setup finds it and offers **Make reliefs only** on
  its first page: just the missing reliefs, nothing else installed or changed.
- **Eye-tracked foveation by default.** With an eye-tracking headset (a Quest Pro through Virtual Desktop with eye
  tracking shared to the PC, Varjo...) the sharp middle of Foveated Rendering follows your eyes without setting
  anything; without one, or while your eyes are not tracked, it stays in the middle of the lens. A line under VR
  Settings > Graphics > Eye-Tracked Foveation (`vr_foveated_gaze`, now on) says which is in use. Off never asks for
  the eye tracker.
- **Relighting about twice as fast.** Quake VR now ships its own build of ericw-tools' `light` (2.0.0-alpha11-qvr1)
  for the relighting (Graphics > Relighting, the installer's relight of the official maps, Download ericw-tools). With
  Bounced Light on (the default) it traces the bounced light from fewer samples and shares it between neighbours:
  e1m1 relights in 4 s instead of 8, e3m5 in 8.5 s instead of 21, Arcane Dimensions' ad_mountain in 50 s instead of
  95 to 113, the installer's relight of Quake's and the mission packs' maps in 6 minutes instead of 9, and the look is the same (a change of under one step in 255 on average). Maps you relit before stay as
  they are and are not relit again. Its source, with the changes and how to build it, is offered beside every release
  (GPL-3). The setting `vr_relight_bounceextra` (2; 0 = as before) is for comparisons.
- **Bloom without black or coloured blocks.** One extremely bright pixel can no longer spread a block of black or
  colour through the bloom.
- **Safe mode only after a real failed start.** The automatic GL safe mode (optional graphics features off for a
  start) now turns on only when the last start crashed, or died within seconds of building its shaders. Closing the
  game while it compiles its shaders on the first start, or after a slow compile, no longer puts the next start in
  safe mode; the start-up log (`quakevr/crash/gl_startup.log`) and the console say why either way.

## Multiplayer beta

Quake VR's multiplayer, rebuilt for VR: play the campaigns together or against each other with your real hands,
weapons, bodies and voices. It is a **beta**: tested on every build, but with little play over the internet yet.
The guide: [MULTIPLAYER.md](MULTIPLAYER.md).

- **Coop and deathmatch, up to 16 players**, from Multiplayer > Host Game; everyone needs this version of Quake VR.
- **Join by room code**: the host's Host Game page shows a 6-letter room code (C copies it); the others type it at
  Join Game > Join by room code (or `connect @CODE`). Quake VR's room service only introduces the two games (UDP hole
  punching): no port forwarding through most home routers, and the game goes directly between the players. Behind a
  router that doesn't allow it (a symmetric NAT) the joiner is told so, and the host forwards UDP port 26000 as before.
  The service sees the players' IP addresses only while the room lives and logs none; `vr_mp_room 0` hosts without a
  code, `vr_mp_rendezvous` names another service.
- **Is your port reachable?** While you host with a room code, Host Game says whether your game's port can be reached
  from the internet: "Port 26000: reachable" (anyone can join, whatever their router), "not reachable from the
  internet" (forward UDP 26000 for the players behind strict routers; room codes still work for most), or "unknown"
  (room service off). The net HUD shows it while you host, and the console says it once. The room service checks it
  by sending one unasked-for packet to your port from another of its ports.
- **One server socket**: the host talks to every player from his one port (26000), so a forwarded port works behind
  routers that change port numbers too (Quake gave each player a port of his own).
- **You see each other as you are**: heads, hands, fingers, held weapons and bodies sized by each player's own
  calibration, moving smoothly between the server's updates. A sliding player is seen sliding (legs out in front, leant
  back), not crouched; a dead player's body is his own body (his build, skin tone, gloves, hands), not Quake's ranger,
  for everyone including himself.
- **Ragdolls as the host has them**: dead monsters, staggering, knocked-down and climbing ones and dead players' bodies
  fall, tumble and get up the same way for everyone (before, a joining player saw the death animation instead); props,
  thrown weapons and gibs already moved the same for everyone, and still do while ragdolls are sent (the ragdolls only
  use the room the rest leaves). A corpse you hold follows your hand smoothly, as for the host.
- **Fixed: thrown props jumping at a joining player.** A thrown crate or sword (any moving thing) could move in small
  jumps, a moment ahead of everything else, when the connection was uneven (ragdolls moving, a hitch on the host); it
  now always moves smoothly.
- **Other players' guns and torches as they are**: the heavy revolver's cylinder swinging out and its shells, a super
  shotgun broken open, magazines in or dropped and the rounds they show, the ammo screen's count, the Widowmaker's
  spinning barrels; another player's flashlight is drawn where he carries it and lights the world where it points.
- **Other players' power-ups and gear**: a player with Quad Damage, the Pentagram or the Biosuit glows as you do (his
  hands and body, the light from his hands, Quad's arcs), under the Ring of Shadows he is faded (in deathmatch too); his
  holster sheaths, wrist gadget, grenade and ammo pouches are drawn on him, and a bot's holsters with what it carries;
  the rounds in his ammo pouch and his gadget screen's glow too.
- **Other players' shots**: their muzzle flash, sparks and smoke at their gun, and their spent casings thrown out of it;
  their shotgun's auto pump strokes and clacks after the shot, their ammo screen's faint light and their barrel's heat
  glow and light as yours.
- **Other players' wounds and sounds**: a hit player's copy is painted with the wound where the blow struck it (kept on
  his dead body, gone when he respawns), and bleeds as yours does: blood streams pouring from it (you hear them land),
  a splash of drops and drips for a while after; you hear another player's slide (the scuff and the scrape) from him.
- **Fixed: ragdolls a moment ahead at a joining player** after a hitch on an uneven connection (the parts jumped ahead
  and caught up over half a second): each ragdoll's poses now reach back as far as the smoothing does.
- **Fixed: a prop tipped up on end spinning at the other players** (a crate standing on its side): its turn is now
  smoothed as it is drawn.
- **Network version 43**: games of this version and older ones refuse each other with the reason.
- **Test runs stay off the internet**: with `-loopbackonly` (every test run) the map index, Slipseer, the update check
  and the add-on list are never fetched; asked for, each prints its "NET SAFETY" refusal.
- **Everything you can do alone works together**: grabbing, throwing, melee and parries, climbing and mantling, the
  grapple, holsters and pouches, reloads, force grab; hand weapons and ammo to each other, knock weapons out of other
  players' hands in deathmatch.
- **Your own movement predicted**: walking, jumping, swimming, climbing and the grapple respond at once, with the server
  correcting them smoothly; hits are judged where you saw the others when you fired (lag compensation). The server
  runs your moves one a tick even when your packets arrive bunched or out of order (on a jittery link it holds them a
  little longer, as long as your link's jitter needs), so lifts and doors meet you where your game showed.
- **Deathmatch with bots** (FrikBot: `impulse 100`) that fight, swing and grab with their hands like players.
- **Bots with a flat-screen player's hands**: they dual wield, swing melee weapons overhead and parry blows by their
  skill; **co-op bots** (VR Settings > Play > Add Co-op Bot) fight the monsters beside you and follow you to the next level.
- **Grab a bot's weapon where you see it**: a bot's weapon is placed once, by the server, and every player draws it
  there, so closing your hand on it takes it (grabbing beside it no longer does); its blows and parries come from that
  weapon too.
- **Debug > Show Disarm Volumes and Show Hit Zones work on bots and other players**, hosting or joining: the volumes
  a grab and a shot on their weapon test, and their head, body, limb and leg zones, outlined on them.
- **Bots toned down by skill**: at the lower skills they aim off and behind a moving target, wait a moment before
  firing at someone they've just seen, pause between bursts, turn slower and notice players from less far; their
  melee swings are held raised a little before they come down, so you can see them coming and parry. They're the
  size of a player now, and they draw and fire every gun they carry (the heavy revolver, the nail pistol and the
  mission packs' guns too), falling back to their melee weapon only when out of ammo.
- **Opt-in: bots that find their way** (`vr_bot_navmesh 1`, off by default): bots still roam by their waypoints for
  items, powerups and health, but chase a player they've seen through doors they never used, and reach goals their
  waypoints have no way to, along the map's navmesh.
- **Proximity voice chat** (Opus, with RNNoise noise suppression): push-to-talk or voice activation, heard at the
  talker's head; per-player mute and volume (VR Settings > HUD, Haptics and Sound > Sound > Voice Chat).
- **Server Browser** (Multiplayer > Join Game): favourites, the servers you joined last and the LAN's, with their
  maps, players and pings.
- **Map Library maps installed on joining**: a server's map package you lack is installed before you join.
- **Coming and going**: friends can join a coop game in progress; a player who leaves or crashes and comes back by the
  same name gets his slot and his things back.
- **A vanished server**: after 3 seconds of silence your view says "server not responding"; after 30 seconds
  (`vr_mp_client_timeout`) you are back at the Server Browser with the reason, not frozen for 5 minutes.
- **Your link**: a net HUD (ping, loss, corrections; Debug > Reports > Network: Net HUD) and a rate cap
  (`vr_net_rate`) that keeps what is near and ahead of you up to date first on a slow connection.
- The host's gameplay rules govern the game; each player keeps his own comfort, hand and display settings.
- **Dedicated servers** (`ironwail.exe -dedicated 8`): `server.cfg` read at start, a console status line with the
  players' pings every minute (`vr_sv_status`), pings in `status`, nobody can pause them.
- **Hardened against broken packets**: a malformed message from a player drops only him; a malformed one from a
  server ends that game with the reason instead of closing Quake VR (also a damaged demo in single player).
- **A flat-screen player aiming down the sights looks the part** to others (and in the third-person camera): his head
  at his eye behind the rear sight, the cheek on the stock, the chest following the gun up and down, the arms holding
  the gun from below (no forearm across the face), standing, crouched or sliding.
- **The scoreboard on your wrist**: in multiplayer the wrist gadget's outer button also turns to a scoreboard page:
  every player's colours, frags and name in the scoreboard's order, your row lit, who is talking on voice chat, and in
  coop the level's kills and secrets. Live, up to 16 players. Its Arcane Dimensions page is offered only when the
  server you joined has Arcane Dimensions' data, not because your last local game did.

- **Fixed**: a dead player's or bot's holstered dagger dropped as a knight's sword (every weapon he drops is now the
  weapon he had: its kind, magazine and variant); starting a campaign without deathmatch starts (Dwell from Select
  Campaign) in a deathmatch game stopped the game ("runaway loop error"): players now spawn at its coop starts; a
  player's or bot's old body turned back into Quake's ranger once he respawned: every body still lying looks like him
  (his build, skin and gloves).
- **New**: a dead player's ragdoll is his own body, not Quake's ranger's rigged under it: his build's shape at his height,
  its limbs his body's own lengths, falling, grabbed and struck as any ragdoll and seen the same by every player; single
  player your own corpse too (the death camera and third person), and the bots' in their build. Its arms are his arms'
  length and its shoulders where his are (his Body Calibration or arm length setting), and a flat-screen player's corpse
  is his flat-screen body height.

**Single player only**: time scale and bullet time, saving at will (the host's level-start coop autosave instead),
the crates Quake VR places about maps, Dimension of the Past and Dimension of the Machine.

**Known issues**: little real internet play yet (latency, loss and routers tested through simulated links); no relay
for the routers hole punching can't get through (the host forwards his port, or a virtual LAN); custom maps that
aren't Map Library packages aren't sent to joining players.

## Weapons

- **Faster shotgun shells in Arcane Dimensions and Quoth.** Where shells fly as projectiles, they now fly 30% faster,
  yours and the monsters' alike (the Player and Enemy Pellet Speed settings still multiply on top). Vanilla's hitscan
  shotguns are unchanged.
- **Flat screen: a blade stuck in an enemy keeps its hand.** Dual wielding a blade and a gun, a blade that stays in an
  enemy now keeps the hand that swung it on its handle until it comes out or the hand lets go; meanwhile the gun stays
  one-handed, the right mouse button fires the left hand's weapon and the ironsight stays down, as when dual wielding.
  Before, the hand let go of the blade to hold the gun in both hands, and the ironsight came up. In the headset too, a
  hand holding a stuck blade no longer takes the other gun's two-handed grip.
- **Being disarmed by enemies is predictable now (still off by default).** With Armed Grunts > "You Can Be
  Disarmed" on, enemies' hits jolt the weapon you hold instead of rolling dice: two jolts on one hand within three
  seconds knock it out, and the first one warns you (your hand is knocked aside, buzzes and rattles, "Your grip on
  your weapon loosens!"). A blade, axe or chainsaw landing on your weapon jolts it (a heavy blow knocks it out at
  once), an armed enemy's punch and any heavy blow on your body jolt it, a grunt's full blast or an enforcer's laser
  on your gun too. A knight's sword can disarm you now; parried blows, bites and light claws never do. The jolts,
  how long they last and what counts as heavy are new settings beside it. Other players' hits in multiplayer are
  unchanged.

## Movement

- **Ropes and chains.** Physical ropes and chains hang in maps (mappers: `vr_rope`, `vr_chain`, with their length,
  thickness, pieces, mass, stiffness and damping as keys, and a pinned bottom) and can be spawned with the toolgun
  (Spawn > Props > Ropes, with a Rope Settings page). They swing and settle, lie on the floor where they are longer
  than the drop, and are pushed by your hands, players, monsters, props, explosions and shots; every player sees the
  same rope. In VR you can climb them: grip one with either hand and go hand over hand up or down; your weight swings
  it, your legs wrap round it, and letting go or jumping off mid-swing keeps its speed. The toolgun's Remove and
  physgun aim at them, and Clear Props removes the ones you made. On the desktop screen, walk or jump into one to
  grab it: look up or down and hold forward to climb, look ahead and use the move keys to swing it, and jump off at the
  top of a swing to catch the next rope. In multiplayer, other players on a rope are drawn gripping it, their hips in
  close and their legs wrapped round it, even while it swings. Try them in the new test map `vrropes`.
- **Noclip flies where you look.** Moving forward in noclip now goes the way the camera points, so looking up or down
  flies up or down; strafing stays level and jump and crouch still go straight up and down. Flat and in the headset
  (there, what steers your walking steers the flight: the head, or the moving hand with hand-relative movement).
- **No more slipping through grates, bars, pillar rows or beside lifts.** Your narrower hitbox (and narrow monsters')
  could pass or fall through gaps Quake's own box never fit: between a grate's bars, a gate's bars, a row of pillars,
  a lift and its shaft's wall. Each map now gets invisible blockers in exactly those gaps when it loads (nothing in
  the map files changes; shots and sight go through them), so you are stopped where Quake stopped you, and still get
  as close to walls, bars and pillars as your width lets you. Settings > Movement > Player Hitbox > Close Gaps Quake's
  Box Never Fit (`vr_hull_holes`, on; takes effect at the next map load). The blockers now also close gaps between
  slanted bars, turned grates and round holes, between two brush models (two func_walls, a door and a lift), and
  between bars at two heights you'd fall through, and holes exactly as wide as Quake's box (a drain grating in
  Quoth's start that it only stood over), always only where Quake's own box can't reach. The limits are
  settings beside it (Narrower Than: 32, Wider Than: 8, and more), with Rebuild Gap Blockers Now to apply them without
  leaving the map; if you still get through a gap you shouldn't, Debug > Mark a Gap I Got Through writes down where and
  why (`quakevr/hull_holes_marks.txt`) so it can be fixed.
- **No more dropping down the corner beside a lift.** Where a lift leaves narrow slots on two sides of it, the square
  between the two slots (the lift's corner) stayed open and you could fall down the shaft there; it is closed now.
- **Opened bar gates let you through again.** The gap blockers beside a door now move with it, so bars that sink into
  the floor (DM1's grate) take theirs along when they open, instead of leaving them standing in the opened way.
- **Riding a lift no longer sinks you or the monsters into it.** Going up, your eye trailed a rising lift by up to 12
  units (the stair-step easing took the lift's rise for a step) and a monster riding it was drawn as far as 13 units
  into it; now your eye rides with the lift (only a step's jump is eased: `vr_view_lift_follow`, 400 units a second;
  0: as before) and a monster on a moving lift is drawn moving with it, both ways, flat and in the headset. How lifts
  push, crush and turn back is unchanged. Debug > Logging > Lift Riders measures it.

- **The heavy revolver's cylinder stays shut in a melee swing.** The wrist flick that swings it out is ignored while
  the gun's hand moves at a swing's speed and for a moment after it or after a blow lands; a deliberate flick still
  opens it (`vr_revolver_open_swing_speed`, 2 m/s; `vr_revolver_open_swing_hold`, 0.35 s).
- **Sliding in VR is on by default** (VR Settings > Locomotion > Sliding, `vr_slide`): while running, pull the turning
  stick straight down to slide. Your drawn hips and legs now point the way you slide, the chest turned back to your
  body, as on the desktop screen (Slide Legs Follow Slide, `vr_slide_legs`, on).
- **Sliding in VR: the desktop screen's slide, view and body.** While you slide in VR (Sliding),
  your view now moves quickly but smoothly down to the desktop screen's sliding eye height and back up when the slide
  ends, on top of your own head's movement; your drawn body takes exactly the desktop screen's slide pose, its legs out
  on the floor. VR Settings > Locomotion: Slide View (`vr_slide_camera`, on; off keeps the view at your real head) and
  Slide View Time (`vr_slide_camera_time`, 0.15 s). The comfort vignette counts the view's move as movement.
- **Climbing: choose when you pull up onto a ledge.** Advanced VR Options > Movement > Climbing > Mantle When
  (`vr_climb_mantle_trigger`): High Enough (the default, as before: at once), Let Go (hang and look over the top, pull
  up when your last hand lets go; let go lower and you drop), Jump, or Let Go or Jump. Flatscreen mantles and ladders
  are unchanged.
- **Quoth's and Arcane Dimensions' ladders climb like ours.** A mod's ladder drawn on a wall or a pillar (Quoth's
  kellbase1) now has rungs 16 units apart for your hands, as a ladder made of real rungs: grip and climb hand over hand,
  and over the top. On the desktop screen, walking into one climbs it as the tutorial's ladder (Desktop Screen Play >
  Ladders); with Ladders off, the mod's own climb (hold jump) works there again.
- **Sprint by clicking the stick.** VR Settings > Locomotion and Controls > Speed Button (`vr_sprint_toggle`): Toggle
  makes a click of the speed button (the left stick's click as shipped, or Shift) switch between walking and running
  instead of holding it. The switch ends by itself, back to your Default Speed, when you stop moving for a moment, die,
  change level or open the menu. Hold (the default) is as before.
- **The other speed by double-tapping the stick.** VR Settings > Locomotion and Controls > Double-Tap for Other
  Speed (`vr_move_doubletap`, off by default): push the moving stick twice quickly the same way (push, let go, push
  again) for the other speed than your Default Speed until you let the stick go back to the middle: run, or walk
  if your Default Speed is Run, whatever the speed button says. How quick the taps must be: Advanced VR Options >
  Movement > Locomotion > Double-Tap Window (`vr_move_doubletap_window`, 0.3 s).
- **A stick click never counts twice.** The thumbstick's click can bounce as it is let go (a second press a few
  hundredths of a second later), which switched the speed toggle straight back. Such a press is now ignored
  (Advanced VR Options > Movement > Locomotion > Stick Click Debounce, `vr_stick_click_debounce`, 0.08 s).
- **Ammo pouch position sliders.** VR Settings > Holster Calibration now has Ammo Pouch Forward, Right and Up beside the
  holsters' rows (the standing pouch; they agree with Holster Setup's drag and drop), and Reset Holsters resets the pouch
  too.
- **Fixed: the pouches with Body Type Only Hands.** With the body hidden (`vr_body_mode 0`) the ammo pouch and the
  grenade pouch sat inside your own belly and back, out of your hand's reach. They are now where they are with the body
  drawn, on the front and the back of your belt.

## Menus

- **Flat-screen keys in Options > Controls.** The keys flat-screen play reads itself (leans, swap hands, force grab,
  shove, punch, reload, drop, quick grenade, bullet time, flashlight, push-to-talk, holster menu, fly camera, toolgun)
  are now a "Quake VR (flat screen)" group at the end of Controls' keyboard and mouse list, rebound and cleared like
  any other binding; the separate Flat Keys page is gone (Locomotion's "Flat Keys" button opens Controls there, and
  "Reset Flat Keys" stays beside it). A key that is both a flat key and a binding is marked with "!" on both rows, and
  the row under the cursor says what it does: in flat-screen play the flat key wins (for example mouse 4 is Force
  Grab, not your binding, until you clear Force Grab). The `vr_flat_key_*` settings work in configs as before.
- **Flat screen: 9 dims the gear lights**, as the wrist gadget's side button does in the headset: the visor's panels
  and the guns' ammo screens dim, and 9 again lights them. Rebind it in Controls ("Gear lights").
- **Centre messages in the headset look like the flat screen's.** The game's centre prints in front of your head (a key needed, a secret found, a map's text) and the maps' own view messages now appear in the flat visor's own message popup, drawn by the same code: its dark glass pill with cut corners, glowing bevelled frame, font, wrapping, scanlines, pop-in and fade, in a size that reads at the menu's distance. (`vr_center_panel 0` brings back the old look; `vr_center_panel_scale` sizes it.)
- **Search filter (development builds).** The Search page has a "Show: Both / Cvars / Tunables" row over its text
  box: the menus' settings alone, the Tunables pages' constants alone, or both (the default). The results update
  at once; the choice lasts until the game quits. Release builds have no tunables, so they show no row and search
  as before.
- **No attract-mode demos.** Quake's demos no longer play by themselves at start-up or from a mod's start-up
  script, on the PC or the Quest, in VR or flat: flat goes straight to the main menu (VR starts in the hub as
  before). Options > Interface's "Start Demos" row is gone. `playdemo` and `timedemo` still play a demo.
- **Discord status.** With the Discord app running, your Discord profile shows what you play: the campaign, the
  map, single player, co-op, deathmatch or the Survival Arena's wave, VR or flat screen, and the time on the map. On
  by default; turn it off at Menu Settings > Discord Status. It talks only to the Discord app on your PC, never holds
  up the game (without Discord it quietly tries again now and then), and is not on the Quest.
- **Multiplayer menu.** Join Game and Host Game go straight to their page: Quake's "IPX / TCP/IP" choice is gone (IPX
  has been dead for decades, and "TCP/IP" always meant UDP). The "multiplayer is untested" note is gone too.
- **`-window` makes a normal window** with its title bar, to drag and resize, even with Borderless Window on (the
  setting is kept for the next launch without it). **`-windowpos <x> <y>`** places it (its inside's top-left, in
  desktop pixels) instead of centring it.


## Monsters

- **Monsters no longer get stuck at low doorways.** Since the gaps Quake's box never fitted were closed, a grunt
  could stand frozen in front of some doorways (e1m1's, by the start) while walking to a noise: its step was refused
  under the lintel. It walks through again.
- **Cuts follow each blade's drawn shape.** Beheading, cutting off limbs, a cut limb sticking to the blade, cutting
  hoses and the chainsaw's bar all used their own idea of where a blade starts, so the Super Axe's inner curve counted
  as its handle and never took a head or a limb off. Now every one of them uses the blade's shape as drawn in the
  blade shape editor (where its cutting part starts, its curve, its own line), the same shape melee blows already
  use: what the editor shows as the blade cuts, and what it shows as the handle doesn't.
- **A shove never makes an enemy bleed.** An enemy you had hurt earlier, even a single pellet long before, bled from
  every limb as a later shove sent it sprawling. Now a knocked-down or staggering enemy's limbs bleed only for blood
  drawn as it went down or while it is down (shot as you shove it, cut while it lies there); a corpse's still do.

- **The alarm spreads further.** A monster you hurt or kill before it noticed you cries out: the others within earshot
  (800 units in the open, less round a corner or through a doorway, much less through solid walls) come to look, and
  those near it or in sight of it turn on you, now for a kill in one shot too. Monsters placed in ambush keep Quake's
  meaning: they don't hear it, and armed ones no longer leave their post for a fallen comrade's weapon. Combat > Stealth
  AI > Cries Heard (0: off). The test map `vralert` shows each case.
- **Arcane Dimensions' and Quoth's soldiers are armed humanoids.** The rocket, grenade and plasma soldiers fight as
  grunts, the defenders, eliminators, pyros and fumigators as enforcers, the crossbow knights as knights, each holding
  its own gun with its own health: disarm them, tug their guns away, shoot the guns out of their hands, take them.
  Combat > Armed Grunts > Armed Humanoid Replacements: AD and Quoth Variants.
- **Armed humanoids wear the colour of their weapon.** Rocket and grenade launchers red, lightning and plasma blue,
  flamethrowers orange, poison green, on their clothes and backpacks, so you see what is coming; the rest look as
  before. The dye keeps the cloth's shading, and their faces, arms and hands keep their own colour. Same page: Recolour
  by Weapon. Debug > Tools > Texture Studio tunes the colours and which parts of each skin they dye, live.
- **Texture Studio: painting hits what you aim at.** Hands, forearms and the backs of heads paint where you point (the
  model as it is drawn, arms posed), a backpack no longer takes a stroke meant for its wearer (Paint On picks bodies or
  packs), the brush is a size on the model, and a yellow cursor plus a flat view of the skin show which texels a stroke
  will change. The lineup can come without backpacks, armour or weapons, with a backpack on its own stand, held still
  and turned like a turntable.
- **Recoloured armed monsters repainted.** The weapon recolours of the grunts, enforcers, knights and death knights (and
  their backpacks) dye new parts of the skins, and the grunts' and death knights' backpack colours are retuned.

## Performance

- **Crowded maps run lighter.** Armed grunts you can't see (behind walls, or far off and out of view) and that
  haven't noticed you cost far less to run; Dwell's biggest map (d2m8, 250 armed grunts) about half. They wake,
  aim and fight exactly as before the moment you see them, hit them or they notice you.
- **Lighter melee in a crowd of armed enemies.** Swinging at several armed grunts up close now checks whether each
  blow strikes their weapons far more cheaply (about a tenth less game-code work with six around you); what a blow
  strikes is exactly as before.

## Installer

- **Install to a Meta Quest from your PC.** The installer's first page now asks where to install: this PC (PC VR and
  flatscreen), a Meta Quest (standalone), or both at once. For the Quest it gets Google's adb itself if your PC has
  none, shows how to turn on developer mode and allow USB debugging (with a Check again button), installs the app and
  copies your Quake files, the expansions you own, the HD textures and the mods it installed (Dwell, Quoth, Arcane
  Dimensions) to the headset. Running it again only copies what changed; nothing on the headset is ever deleted, and
  your saves and settings there are left alone. Start it from Library > Unknown Sources.
