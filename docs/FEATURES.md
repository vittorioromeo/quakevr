# Quake VR features

What Quake VR does and how to use it. Menu paths are under **VR Settings** (the main page: the main menu's *VR
Settings*, or Options > VR Settings) and **Advanced VR Options** (every other page: the main menu's *Advanced VR* row or
the corner's *Advanced VR* button; "Combat > *Knockdowns*" means Advanced VR Options > Combat > Knockdowns). Pages marked *(Developer)* show at *Menu Detail: Developer*. [SETTINGS.md](SETTINGS.md) lists the settings
themselves; the menus' *Search* finds any of them by name.

The controls named here are the defaults: trigger, grip, A/B on the main hand, X/Y on the off hand, and the sticks.
The main hand is the right controller, the off hand the left: both hands hold, fire, swing and climb alike. *Handedness*
(Body and Display) sets everything that has a side at once, for right- or left-handed players; each also has its own
setting: *Swap Stick Functions* (the right stick moves), *Wrist Gadget Arm* and *Flashlight Side*.

- [Weapons](#weapons)
- [Holsters, reloading and grenades](#holsters-reloading-and-grenades)
- [Throwing, carrying and physics](#throwing-carrying-and-physics)
- [Force grab](#force-grab)
- [Melee](#melee)
- [Bullet time](#bullet-time)
- [Damage and gore](#damage-and-gore)
- [Ragdolls and knockdowns](#ragdolls-and-knockdowns)
- [Fire and lightning](#fire-and-lightning)
- [Movement](#movement)
- [Body and hands](#body-and-hands)
- [HUD, menus and screens](#hud-menus-and-screens)
- [Flashlight](#flashlight)
- [Haptics](#haptics)
- [Graphics](#graphics)
- [Sound](#sound)
- [Maps, campaigns and mods](#maps-campaigns-and-mods)
- [Multiplayer](#multiplayer)
- [Recording and trailers](#recording-and-trailers)
- [Playtesting tools](#playtesting-tools)

## Weapons

- **One in each hand.** Each hand holds its own weapon, and the triggers fire them. Pick weapons up by hand: grip
  the weapon. Weapons, armour, powerups and keys lying in the level are a little smaller than in Quake and rest on
  the floor, so you crouch to take them. The map's spinning weapons look like the weapons you hold (Carrying and
  Throwing > Carrying > *Weapon Pickups Look*).
- **Switching:** B and Y cycle through your weapons, and the holsters hold the ones you carry.
- **Weapon grip:** by default you hold a weapon only while you hold the grip button. *Weapon Grip: Sticky* keeps it
  in your hand once gripped. Grip again and open your hand to throw it or holster it.
- **Held anywhere:** a weapon can be taken by any part of it, as a prop is. Only a hand on the handle fires it; held
  elsewhere it still strikes and throws with all of its length.
- **Two-handed aiming:** with a gun in one hand, grip its foregrip with your other, empty hand. The hand snaps onto
  the gun, and both hands aim it. *Two-Handed* (VR Settings) has three settings: *Off*, *Basic*, and *Virtual stock*,
  where a hand near your shoulder steadies the aim like a stock. With *Two-Handed Hand-Off*, letting go with the
  handle hand leaves the gun hanging from the foregrip hand: grip the handle again to take it back. Swords change
  hands, and you can hold a sword two-handed, including by the blade (the off hand near the tip).
- **Weight:** every weapon has a mass. Heavy ones trail your hand on a spring, need two hands to aim and throw well,
  and a fast wrist snap can wrench them out of your hand (Weapon Weights *(Developer)*; *Heavy Weapons Wrenched Out*).
  Hands and barrels stop at walls and slide along them.
- **Weapon effects:** recoil, muzzle flashes and bullet tracers for every hitscan weapon, the enemies' too (Weapons >
  *Weapon Effects*).
- **Ammo screens:** each gun shows its ammo (and its clip, where it has one) on a small glowing screen that lights
  the gun.
- **Second ammo types:** in the mission packs, guns with a second ammo type have a button on top that switches
  between the types, for example lava nails, multi-grenades and plasma.
- **Crosshair:** off by default. Pick a dot, a laser or a soft laser from the muzzle (VR Settings > Crosshair; more
  on HUD and Menus > *Crosshair*).
- **Knights' swords:** knights and hell knights drop their swords. A sword is a melee weapon with more reach and damage
  than the axe.
- **The crowbar:** found in crates and on the firing range; held like a sword (one hand or two), blunt blows, a hook
  that hits hardest, and it pries crates open.
- **Thrown axes stick** in enemies, walls and props, blade first, and bleed; pull them out by hand or with the force
  grab (Carrying and Throwing > Throwing and Physics > *Thrown Axes*).
- **Enemies' weapons:** ogres drop their chainsaws (pull the cord to start one; it smokes, shakes and cuts gibs and
  heads), grunts their burst rifles and enforcers their laser rifles, for you to use (Combat > *Enemy Weapons*;
  World > *Enemy Weapon Drops*).
- **Grappling hook:** from Dissolution of Eternity, with a physical rope you can swing on and reel in or out, and
  that pulls props and monsters to you (Movement > *Grappling Hook*).
- **The lightning gun in water** shocks you and everything round you, and makes both hands drop what they hold
  (Weapons > *Lightning Gun in Water*).
- **Weapon damage:** every weapon's base damage, yours and the enemies', on Combat > *Weapon Damage*.
- **Quad Damage on melee:** its extra damage and reach for blows (Combat > Melee).
- **Offsets and sights:** per-weapon hand positions with a posing mode (pose the hand on a weapon held still in front
  of you), and *Align Sights to My Aim*, which lines the sights up with the way you naturally point (Weapon Offsets
  *(Developer)*, Hand/Gun Calibration).

## Holsters, reloading and grenades

- **Holsters** are at your hips, on the front of your chest and behind your shoulders. A holster lights up and
  buzzes when your hand is over it. Let go of a weapon there to holster it, and grip there to draw. The hip holsters
  follow your thighs as the legs walk.
- **Passing weapons:** bring both hands together to pass a weapon from one to the other.
- **Weapon Mode** (Weapons > *Immersion*): *Immersive* holsters hold one weapon each, and taking it out empties the
  holster. With *Quick Slots* the holsters never empty.
- **Reloading** (Weapons > Immersion > *Weapon Reloading Mode*): bring the weapon to a hip holster (the default) or
  to any holster. The super shotgun also opens with a flick of the wrist.
- **Hand grenades:** while you have rockets, grip the pouch at your back with an empty hand to take a grenade, pull
  the trigger to start its fuse, and throw it (multi-grenades too, when you have them). The pouch also takes carried
  pickups (Combat > *Batting and Catching* > Hand Grenades, Grenade Pouch).
- **Grenades are live things:** catch an enemy's grenade and throw it back, shoot or strike a grenade to set it off
  (yours, theirs, or one lying in wait) (Combat > Batting and Catching > *Grenades*).
- The holsters' positions and sizes are on Weapons > *Hotspots* and *Hip Holsters*; the *Show...*
  switches draw them so you can place them.

## Throwing, carrying and physics

- **Physics:** props, thrown weapons, gibs and heads are Box3D rigid bodies with true-scale gravity. They spin,
  bounce, slide, come to rest on a flat side and float in water; they knock and scrape with their own sounds
  (Carrying and Throwing > Carrying > *Physics Sounds*).
- **Throwing:** swing and let go of the grip. The throw speed is measured from your controller around the moment
  of release, as in Half-Life: Alyx, and feels the same at any frame rate and in bullet time. The grip is read as an
  analogue value, so a weapon leaves your hand as soon as your grip eases during a fast swing (*Analog Release*). A
  flick of the wrist adds spin and speed. Heavy things fly less far, and two hands throw them better.
- **True-scale flight:** thrown things fly under real gravity (*Throw Gravity: Real*, or *Quake*). A thrown weapon or
  prop hurts what it hits. *Aim Assist* (on by default) bends a throw towards a nearby monster.
- **Carrying:** grip an ammo or health box, a backpack or any prop to hold it; hold it in both hands to turn it with
  both. Let go of a box at a holster to take it (*Take a Box* can use the trigger instead). A hand or gun touching a
  prop without gripping nudges it. Punching with a box in your hand hits harder, and thrown boxes hurt.
- **Armour** is a physics object too: take it, and let go of it over your torso to put it on (only if it beats what
  you wear). Throw it, knock it, force-grab it (Carrying > *Armour*).
- A closed hand never grabs by moving onto something: carrying starts when you press the grip.
- **Props in the maps:** breakable wooden crates (with ammo or health inside, and a crowbar to pry them), throwable
  rocks and bricks, explosive boxes you can push, carry, stack and stand on, and wall torches you can take off the
  wall and carry (Carrying and Throwing > *Crates*, *Rocks and Bricks*, *Wall Torches*; Carrying > *Explosive Boxes*).
- **Standing on props:** you can stand on boxes, jump from them and climb a stack (Movement > Player Hitbox *(Developer)* >
  *Standing on Props*).
- **Shots push props:** pellets, nails and the lightning beam push the loose things they hit (Throwing and Physics >
  *Shots Push Props*).
- **Wall buttons** are pressed by your hands, your weapons, held props and thrown things.
- **Gibs and heads** can be picked up, thrown (they burst against walls) and force-grabbed (*Gibs and Corpses*).
  Corpses can be gibbed by shots and blows, and things meet them (*Corpse Collision*).
- **Never stuck:** a door or button that closes into you, or a prop you end up inside, lets you out.
- Settings: Carrying and Throwing > *Throwing and Physics* and *Carrying*; Held Object Offsets and Weights
  *(Developer)*.

## Force grab

Point an open, empty hand at a weapon, backpack, or ammo or health box. It glows and sparkles. Pull the trigger to
lock on, then flick your hand back or up. The item flies to you in an arc and arrives in about half a second, handle
first. Close your hand (grip) as it arrives to catch it. Too early or too late, and it drops at your feet. It flies
through walls, so it can't get stuck. Settings: Carrying and Throwing > *Force Grab*, and *Force Grab* on the main page.

## Melee

- **Blows:** punch, or swing the axe, a sword, the crowbar or any gun. A blow has to be a real one: the wrist must move
  fast enough (*Swing Speed*) and far enough (*Blow Distance*), so waving and flicks don't hit. Harder blows do more
  damage, and straight punches more than slaps. Hits land on the monster's real shape, not its box. You can hit a gib
  held in your other hand.
- **Parry:** hold a weapon (one or two hands) level across in front of you as a monster's blow lands. It takes less
  damage, with a clang and sparks, and stops the rest of that attack, the monster staggered (*Parry Stops Attacks*,
  *Parry Stagger*). Every melee monster can be parried. A one-handed parry may knock the weapon out of your hand.
  *Unarmed Parry*: cross your forearms in an X. A quick blow after a parry is a counter-attack.
- **Bash:** from a guard (a weapon held level across, or both hands together), push forward hard. It does little
  damage, but it throws the monster back and staggers it. One open hand, palm ahead, shoves half as hard. Shove
  monsters off ledges.
- **Headbutt:** lunge your head at a monster.
- **Batting projectiles:** swing a weapon or your fist through a spike, laser, spit or grenade to send it back. A
  bash or a shove with a weapon in hand bats them back too, with a more lenient reach and timing (Combat > *Batting
  and Catching*).
- **Blade or bash:** a sword hits with the blade whatever its angle; the hilt landing first doesn't spoil the cut.
- **Knockback:** your blows push monsters, their blows push you, and a parry pushes you both apart. Hits knock your
  hands and arms too (Combat > Damage and Knockback > *Hits Knock Your Hands*).
- **Enemy shoves:** grunts, enforcers and melee monsters shove you when you are too close; parry a shove, or be
  knocked off a hold (Combat > *Enemy Shoves*).
- **Stamina** (optional, Combat > *Stamina*): parries, shoves, strikes and climbing tire you; tired,
  you hit softer, your aim shakes and your hands get heavier (*Tired Arms*). The gadget shows it.
- **Training dummy** (the firing range): it shows every hit's damage, kind and body part, its health over its head,
  and stands as any enemy you pick, bleeding and dying as that enemy does (Gore > *Training Dummy*). It can attack
  you every few seconds so you can practise parrying (Combat > Parry and Bash > *Training Dummy Attacks*).
- Settings: Combat > *Melee* (with the headbutt), *Parry and Bash*, *Batting and Catching*, *Damage and Knockback*.

## Bullet time

Slow the world for a few seconds that recharge: tap your wrist, press the gadget's button, or press a stick
(Combat > Bullet Time > *Trigger*). *Sandevistan* keeps you at full speed while the world slows (your missiles too, if
you like). Throws stay true in it. Combat > *Bullet Time*.

## Damage and gore

- **Positional damage:** headshots (with a crack you can hear), and weaker arm and leg shots, on humanoid monsters.
- **Damage settings:** damage to enemies, to you, and from your own explosives (Combat > *Damage and Knockback*).
- **Gore** (the *Gore* page; *Gore*: normal, more, or over the top, the default): blood sprays on walls and floors,
  gibs that stick to ceilings and walls and drip, pools spreading under corpses, blood trails from gibs, and a blood
  mist from hard hits. A dying monster can be hit and gibbed as it falls.
- **Small gibs and brain chunks:** blows and shots tear small bits of meat from bodies; popped heads throw brain
  chunks. How many, per enemy (Gore > *Small Gibs*, *Small Gibs - Per Enemy*).
- **Wounds:** monsters show their wounds and burns where they were hit (Gore > *Wounds on Models*).
- **Blood on you:** when you are hurt, your arms and hands get bloodier and drip on the floor; blood also gets on your
  weapons (holstered ones too) and the props you hold. Water washes it off (Gore > *Bloody Hands and Washing*, *Blood
  on You and Your Gear*).
- **Decals:** blood, scorch marks and bullet holes (Gore > *Marks*).
- **Beheading and head pops:** a fast blade slash at the neck cuts the head off; shotgun blasts, lightning, lasers,
  thrown things and hard blunt blows can pop it, by a chance that depends on the weapon, the damage and the range;
  under Quad Damage every headshot kill pops (Gore > *Decapitation*, *Head Pop Chance*). A beheaded body runs on a
  moment.
- **Limb gore:** arms and legs are cut off or popped as heads are (Gore > *Limb Gore*).
- **Flies** buzz round severed heads (Gore > *Flies on Heads*, off by default).
- **Corpses** take damage until they gib, with health per monster (Gibs and Corpses > *Corpse Damage and Health*).
- **Enemies hurt by liquids:** monsters burn in slime and lava and drown (World > *Enemies Hurt by Liquids*).

## Ragdolls and knockdowns

- **Ragdolls** (Carrying and Throwing > Gibs and Corpses > *Ragdolls*): corpses go limp as physical bodies, every
  kind of monster from the grunt to the shambler and the vore, and the mission packs' gremlin, centroid and mummy,
  each with its own masses (Ragdoll Settings *(Developer)* has a page for each). Grab a
  limb to drag them, throw them with one or two hands, pile them up; they bleed and burn.
- **Knockdowns:** a hard shove can knock a monster down as a live ragdoll that struggles and gets up again; a shove
  that carries it over a ledge always knocks it down (Combat > *Knockdowns*).

## Fire and lightning

- **Burning:** flames spread between monsters, corpses and crates; lava nails set things alight, and burnt bodies
  smoulder after the flames go out (Combat > *Burning*). Torch flames and the maps' own flames burn your hands and
  body if you hold them there (Burning > *Your Own Torch*).
- **Lightning shock:** what the lightning gun kills keeps arcing, convulsing, crackling and smoking, with burn marks; the living
  it strikes arc too, and so do you when lightning hits you (Gore > *Lightning Shock*: *Arcs on the Living*, *Arcs on
  You*, *Smouldering Smoke*).

## Movement

- **Smooth locomotion** with the left stick (the right one with *Swap Stick Functions*), towards your head or that
  hand (*Move Towards*). Run or walk by default (*Default Speed*); clicking the off-hand stick switches to the other.
- **Turning:** smooth, or snap 30, 45 or 90 degrees. *Comfort* (Locomotion) sets turning, teleport and speed
  together.
- **Teleport** (off by default): turn on *Teleport* and bind a button to `+teleport`, for example
  `bind LTHUMB +teleport`. Aim with the off hand and let go on a blue spot.
- **Room-scale:** walking in your room moves you in the game, with collision (*Room Scale* sets the ratio). Jumping
  for real jumps (*Roomscale Jump*). You can crouch, and lean up to a wall or over a railing before your body
  follows (*Lean*).
- **Swimming:** in water the stick slows, and strokes of your hands (palm first) move you where you look. Point
  your off hand up or down while pushing forward to swim up or down, or use the main stick's up and down. *Air
  Supply* makes your breath last longer (Movement > *Swimming*).
- **Climbing** (Movement > *Climbing*, on by default): grip a ledge or a rung with an empty hand to hang from it,
  climb hand over hand, shimmy along, jump up to a ledge from against its wall, and pull down to mantle onto it
  (sloping tops too).
- **A smaller hitbox:** you fit through gaps your body fits through, on unmodified maps (Movement > *Player Hitbox*
  *(Developer)*); monsters can use their own widths against walls (*Monster Hitbox*, off by default).

## Body and hands

- **Body** (Body and Display > *Body*; *Body Type* on VR Settings): *Off*, *Torso and arms*, or *Full body* (the default), with
  legs that walk and step round as you turn. Your arms reach your hands (inverse kinematics), the elbows stay tucked
  when you aim by your face, and the body crouches and leans with your head. It casts a shadow, head and all. Hands,
  arms, the gadget and held weapons stop at each other instead of passing through.
- **Build:** lean, athletic or brawny.
- **Calibration:** *VR Calibration* (the main menu's first row) runs at the first start: it measures your height and
  body, then leaves you in a room whose wall buttons set the main options. *Body Calibration* (Body) and *Hand/Gun
  Calibration* (Weapons; both hands at once: VR Settings > Hand Calibration) fine-tune your arms and where your hands sit on the controllers.
- **What you wear:** the ranger's clothes, pauldrons, your armour (green, yellow or red plates), your wounds, and
  your powerups (quad damage sparks round your hands, the pentagram glows red, the ring fades you).
- **Hands:** fitted to what they hold, fingers that curl with the trigger (index), the grip (middle to little finger)
  and your thumb resting on a button or stick. Each weapon's finger positions can be tuned (Hand/Gun Calibration >
  *Fingers and Collisions* *(Developer)*).
- **Placement:** *Torso Offset*, *Legs Offset* and *Shoulders Offset* on the *Body* page; *Body - Arms and
  Pauldrons* has the arms.

## HUD, menus and screens

- **Wrist gadget** (the default HUD): a device on the back of your off-hand forearm. Raise your forearm across your
  chest, like reading a watch. Its screen shows health, armour, ammo, stamina, and optionally the level and stats,
  with a CRT look. The game's messages (a key needed, a secret found, pickups) float as a hologram it projects over
  its screen (Tips > Screens > *Game Messages as Hologram*); centre prints float in front of you. HUD and Menus >
  *Wrist Gadget* sets its arm, size, placement and colours.
- **Status bar:** the classic Quake status bar on a hand (VR Settings > *HUD: Status bar*).
- **Menus** float in front of you, with modern sliders, switches and drop-down lists. Point with the laser from your
  hand, pull the trigger to click, and drag sliders. The sticks work too. *Back to Game* is at the top left, or you
  can hold the menu button. Settings pages show their changes live, and in single player the game keeps running
  underneath. The main menu has *VR Calibration*, *Map Library*, *VR Settings* and *Advanced VR* rows.
- **Corner buttons** on every menu: Back to Game, Search, Console, VR Settings, Advanced VR, Levels, Map Library,
  Relighting and Checklist (on a flat screen too: HUD and Menus > Menu > *Corner Buttons on a Flat Screen*).
- **Search:** finds any setting or page by its name, what it does or its cvar, as you type on the keyboard under it;
  the last six results you opened are listed under the keyboard.
- **Console:** Quake's console with a keyboard, to type commands in the headset.
- **Menu Detail** (at the bottom of every page): *Standard*, *Advanced* or *Developer*. Changed settings have a `*`;
  *Changed Settings* (on Advanced VR Options) lists them, and each page's *Reset This Page* puts its settings back.
- **Tips:** the first time you come near something you can use (a wall torch, or what a map's author marked with a
  tip of their own), a small screen beside it, or the wrist gadget, explains it, once (the *Tips* page).
- **Map boards** (the text signs in the hub, tutorial and firing range) are CRT screens.
- **Colours:** one *Player Effects Hue* colours the gadget's screen, the force grab, the teleport arc, the crosshair
  and the menu laser (HUD and Menus > *Colours*).

## Flashlight

A torch on your belt, at the hip *Flashlight Side* chooses (Body and Display > *Chest Flashlight*). Put a
hand at it and pull the trigger to switch it on or off. Grip it to take it in your hand, and let go to send it back
on its chain. In the hand, B or Y, or a sharp flick of the wrist, turns it over between the low grip and the overhead
one (Flashlight > *Flick to Turn Over*). Bring it to the gun in your other hand and press B or Y to clip it on the gun
(or let go of it there); it comes off with the other hand and B or Y, or goes back to your belt when the gun is
holstered or dropped. It can clip on your head too. It stays on across level changes and goes off when you start a map
afresh. It's a real spotlight: it lights models and casts shadows. Settings: the *Flashlight* page and its grips' pages.

## Haptics

Your controllers buzz when you fire, hit, get hit (on the side the hit comes from), hover over a holster, or catch
a force-grabbed item. Explosions nearby rumble, and a heartbeat beats at low health. You can turn all of it off
(*Haptics* on the main page) or tune it on the *World* and Weapons > *Immersion* pages.

## Graphics

Most of these have switches on the *Graphics* pages, and the *Preset* there sets most of them at once.

- **Relit maps** (optional, see [RELIGHTING.md](RELIGHTING.md)): Quake's maps relit with ericw-tools, with soft
  shadows, ambient occlusion in corners, coloured light, and lamps, light panels and glowing textures that light their
  surroundings. *Graphics > Relighting* relights in the game: the map you are on, an episode, a game, the Map Library's
  maps or every map, in the background with a progress bar, with sliders for the look; it downloads ericw-tools for
  you if you have none. The script in `quakevr\tools` also makes **water, slime and teleporters see-through** (lava
  stays opaque). *Relit Maps* (Graphics) switches between them and the original lighting.
- **The look:** darker shade and stronger light, inspired by DarkPlaces. Your shots, explosions and glowing
  projectiles light up the room in their colours, and lamps, buttons and screens glow (bloom). *Off (Quake)*
  restores Quake's look.
- **Ambient light (optional):** a floor of light for the room itself (Graphics - Lights > *Ambient Light*, 0 to 0.4; 0
  keeps Quake's). Debug > *Light Probe* prints the light at six points round you.
- **Real-time shadows:** explosions, rockets, your muzzle flash (optional) and torches cast shadows. The map lights
  nearest you cast the shadows of monsters and of your own body. The graphics *Preset* goes from *Off (Quake)*
  through *Low*, *Medium* and *High* to *Ultra*.
- **Model lighting:** monsters, items, weapons, hands and your body are lit from the map's own lights, with
  directional ambient, a rim light, and reflections on your weapons. Dynamic lights light models per pixel.
- **Lit particles:** blood, smoke, dust and splashes take the light round them; fire and sparks glow (Graphics -
  Models and Effects > *Lit Particles*).
- **Ambient occlusion on what moves:** monsters, your body and items darken the floor and walls round them, lifts
  and doors their shafts, and models their own creases (*Ambient Occlusion*, Graphics - Shadows).
- **Surfaces:** bump maps made from every texture, or Quetoo's hand-made normal, specular and glow maps for the QRP
  textures (shipped; Graphics - Surfaces > *External Maps*), with a sheen under dynamic lights. Parallax makes walls
  look deep. Detail textures (stone, metal, wood grain) sharpen surfaces up close, where Quake's textures would blur.
  There is also anti-aliasing for sheen and for fences and grates.
- **Slipgates that show where they lead:** like Portal's portals, a slipgate shows the place it takes you to, live
  (monsters and all), in each eye, on any map. You walk through into the place you saw, keeping your speed and the way
  you face; rockets, grenades and nails fly through too, and monsters see and shoot through them. One switch (Graphics
  > Slipgates, `vr_slipgates`) turns all of it off, back to Quake's teleporters exactly as they were.
- **Water and liquids:** waves that move the surface, reflections of the room around them and refraction, glints,
  caustics, big splashes and ripples that move the waves, shoreline foam, heat haze over lava, fog and a gentle wobble
  under water.
- **Particles and effects:** textured smoke, sparks, blood and explosions with debris (*Quake VR Particles*) that
  fade softly into walls, shell casings, and torches whose light flickers.
- **Tone and colour:** tone mapping, exposure, colour grades and dither, plus the headset's own gamma and contrast.
- **Retro look** (optional): blocky, palette-snapped textures for the world or chosen kinds of things (each with its
  own page, and an in-game editor for one model or texture), and banded, dithered or blocky light, like
  software-rendered Quake (Graphics > *Retro Textures*, *Retro Lighting*).
- **HD textures:** replacement textures (such as QRP's) are filtered smoothly and get bump maps.
- **Headset:** *Render Scale* renders the eyes at a lower or higher resolution (0.5 to 1.5), with *Upscaling* (FSR 1
  or NIS) sharpening a lower one back up; *Foveated Rendering* (NVIDIA cards) shades the edges of the view, which
  the lenses blur, at a lower rate; *Hide Lens Corners* skips the pixels the lenses never show; *Near Clip* lets a
  gun come right up to your eye.

## Sound

- **Spatial sound** with Valve's Steam Audio (the *Sound* page; *Spatial Sound* on VR Settings): sounds around your head (HRTF), muffled behind
  walls (occlusion), the room's reverb, air absorption over distance, your weapons heard from your hands, and a
  muffle under water. Voices keep their full band (Quake's own mix cut the highs).
- **Physics sounds:** props, thrown weapons and gibs knock and scrape; a climbing hand grips with a sound, and a hand
  taking a crate, a rock, an explosive box or a gib sounds of what it is made of. What is missing or could be better:
  [vr-port/AUDIO_REVIEW.md](vr-port/AUDIO_REVIEW.md).
- **Music** plays from the soundtrack of the Quake you own (the Steam re-release's, read where it is installed), per
  campaign; nothing to copy. *Music Volume* is on VR Settings.

## Maps, campaigns and mods

- **The VR hub** (`vrstart`, an island in a lake at night) is where the game starts: a path from the pier past the
  campaign lecterns and their slipgate, a settings pavilion, a firing range and a lookout tower. Pick Quake, Scourge of
  Armagon, Dissolution of Eternity or Dimension of the Past and step into the slipgate. Its boards and tips explain the
  basics. (The old hub is `vrstart_old`, in the Debug menu.) From *Advanced VR Options > Play* you can go back to the
  hub, the **tutorial** or the **firing range** (weapons to try, both swords, the crowbar, props, and the training
  dummy).
- **Mission packs:** Hipnotic and Rogue are independent optional packs. Validated installed data is used
  automatically, with its weapons, monsters and maps. Quake, the hub, tutorial and firing range work without
  either pack; unavailable campaign buttons are labelled in the hub. Quake VR's QuakeC contains all three campaigns.
- **Official campaigns** (Single Player > *Official Campaigns*, or Play): the re-release's Dimension of the
  Past and Dimension of the Machine (with its Horde mode) play natively in VR, in single player, when you own them;
  Dawn of the Machine is detected, and its native port is in progress. See [INSTALL.md](INSTALL.md#official-campaigns).
- **Map Library** (the main menu, or the corner's *Map Library*): browse [Quaddicted](https://www.quaddicted.com/)'s
  archive of custom maps, then download, install and play one in the game; *Uninstall* and *Reinstall* manage what you
  installed, and the download cache is capped (Debug > Tools > *Download Cache Size*). See
  [INSTALL.md](INSTALL.md#custom-maps-and-mods).
- **Custom maps** without their own `progs.dat` play with Quake VR's gameplay. Mappers can place tips for new players
  (`func_vr_tip`) and use Quake VR's entities ([vr-port/MAPPING.md](vr-port/MAPPING.md)). **Other mods** run in a
  compatibility mode: your hand aims and their weapons fire from your gun, and you can walk the room and teleport,
  but there are no off-hand weapons, holsters, throwing or melee. See [INSTALL.md](INSTALL.md#custom-maps-and-mods).
- **Flat screen:** `vr_enabled 0` plays on the monitor, with a mouse and keyboard.

## Multiplayer

Multiplayer and FrikBot bots (*Play* > Bots adds and kicks them), carried over from the original. The server runs at
a fixed 72 ticks a second whatever the headset's refresh rate, so physics and melee behave the same on every machine;
the server decides melee timing and the debris everyone can touch, while purely visual debris stays on each client
([vr-port/MULTIPLAYER.md](vr-port/MULTIPLAYER.md)).

## Recording and trailers

For recording footage from the desktop window (Body and Display > *Recording*):

- **Spectator camera:** besides a smoothed copy of your view, a spectator camera for the desktop window, with a wider
  field of view, your eyes' glow shown, and the head-locked text left out; a small preview of it in the menus, and a switch on every
  menu.
- **Slow motion** for recording, to be sped up in editing, with a sound file in game time.
- **Highlight markers:** the game logs moments worth keeping (kills, headshots, gibs, multikills, explosions...) with a sync flash and beep,
  and a script turns them into a rough cut ([vr-port/TRAILER.md](vr-port/TRAILER.md)).

## Playtesting tools

- **Voice notes** (Debug > *Voice Notes* *(Developer)*; on by default). Raise your off hand to your mouth, like a radio,
  and hold **Y** to talk. Let go to save the note. "REC" shows while you record, and the hand buzzes as a note starts
  and ends. Away from your mouth, Y does what it always does. A note shorter than 0.8 seconds counts as an accidental
  press and is dropped. Each note is saved in `quakevr\notes\` as a `.wav`, with a screenshot and where you were: the
  map, your position, health, and what each hand held. By default the microphone is Virtual Desktop's if there is
  one, otherwise the default microphone (`vr_note_device`; `vr_note_devices` lists them).
  [INSTALL.md](INSTALL.md#voice-notes) explains how to turn them into text.
- **Checklist** (the corner's *Checklist*): what to test this round, one line each; tick what works, hide the ticked
  ones, and *Undo Last Tick* takes a tick back.
- **Motion recorder** and **Review Takes** *(Developer)*: record your own melee motions, labelled with what they
  should do, and replay them to check the melee ([vr-port/MOTIONS.md](vr-port/MOTIONS.md)).
- **Debug pages** *(Developer)*: views (hit zones, damage numbers, hitboxes), logs, the profiler and benchmarks,
  reports and tests ([vr-port/TESTING.md](vr-port/TESTING.md), [vr-port/BENCHMARKS.md](vr-port/BENCHMARKS.md)).
