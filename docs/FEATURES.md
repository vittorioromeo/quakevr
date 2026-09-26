# Quake VR features

What Quake VR does and how to use it. Menu paths are under **Options > VR Settings** (the main page) and
**VR Settings > Advanced VR Options** (the other pages). [SETTINGS.md](SETTINGS.md) lists the settings themselves.

The controls named here are the defaults: trigger, grip, A/B on the main hand, X/Y on the off hand, and the sticks.
The main hand is your right hand unless *Left Handed* is on.

- [Weapons](#weapons)
- [Holsters and reloading](#holsters-and-reloading)
- [Throwing, carrying and physics](#throwing-carrying-and-physics)
- [Force grab](#force-grab)
- [Melee](#melee)
- [Damage and gore](#damage-and-gore)
- [Movement](#movement)
- [Body](#body)
- [HUD, menus and screens](#hud-menus-and-screens)
- [Flashlight](#flashlight)
- [Haptics](#haptics)
- [Graphics](#graphics)
- [Maps, mission packs and mods](#maps-mission-packs-and-mods)
- [Voice notes](#voice-notes)

## Weapons

- **One in each hand.** Each hand holds its own weapon, and the triggers fire them. Pick weapons up by hand: grip
  the weapon. Weapons, armour, powerups and keys lying in the level are a little smaller than in Quake and rest on
  the floor, so you crouch to take them.
- **Switching:** B and Y cycle through your weapons, and the holsters hold the ones you carry.
- **Weapon grip:** by default you hold a weapon only while you hold the grip button. *Weapon Grip: Sticky* keeps it
  in your hand once gripped. Grip again and open your hand to throw it or holster it.
- **Two-handed aiming:** with a gun in one hand, grip its foregrip with your other, empty hand. The hand snaps onto
  the gun, and both hands aim it. *Two-Handed* has three settings: *Off*, *Basic*, and *Virtual stock*, where a hand
  near your shoulder steadies the aim like a stock. With *Two-Handed Hand-Off*, letting go with the handle hand
  leaves the gun hanging from the foregrip hand: grip the handle again to take it back. Swords change hands, and
  you can hold a sword two-handed, including by the blade (the off hand near the tip).
- **Weight:** heavy weapons trail your hand a little. Hands and barrels stop at walls.
- **Ammo screens:** each gun shows its ammo (and its clip, where it has one) on a small glowing screen that lights
  the gun.
- **Second ammo types:** in the mission packs, guns with a second ammo type have a button on top that switches
  between the types, for example lava nails, multi-grenades and plasma.
- **Crosshair:** off by default. Pick a dot, a laser or a soft laser from the muzzle (VR Settings > Crosshair; more
  on the *Crosshair* page).
- **Knights' swords:** knights and hell knights can drop their swords (Gameplay > *Knights Drop Swords*). A sword
  is a melee weapon with more reach and damage than the axe.
- **Grappling hook:** from Dissolution of Eternity.

## Holsters and reloading

- **Holsters** are at your hips, on the front of your chest and behind your shoulders. A holster lights up and
  buzzes when your hand is over it. Let go of a weapon there to holster it, and grip there to draw.
- **Passing weapons:** bring both hands together to pass a weapon from one to the other.
- **Weapon Mode** (Immersion page): *Immersive* holsters hold one weapon each, and taking it out empties the
  holster. With *Quick Slots* the holsters never empty.
- **Reloading** (Immersion > *Weapon Reloading Mode*): bring the weapon to a hip holster (the default) or to any
  holster. The super shotgun also opens with a flick of the wrist.
- The holsters' positions and sizes are on the *Hotspots* page. The *Show...* switches there draw them so you can
  place them.

## Throwing, carrying and physics

- **Throwing:** swing and let go of the grip. The throw speed is measured from your controller around the moment
  of release, as in Half-Life: Alyx. The grip is read as an analogue value, so a weapon leaves your hand as soon as
  your grip eases during a fast swing (*Analog Release*). A flick of the wrist adds spin and speed.
- **True-scale flight:** thrown things fly under real gravity (*Throw Gravity: Real*, or *Quake*), spin as your
  hand did, bounce, slide, come to rest on a flat side, and float in water. A thrown weapon hurts what it hits.
  *Aim Assist* (off by default) bends a throw towards a nearby monster.
- **Carrying boxes:** grip an ammo or health box or a backpack to hold it. Let go of it at a holster to take it
  (*Take a Box* can use the trigger instead). A hand or gun touching a box without gripping nudges it. Punching with
  a box in your hand hits harder, and thrown boxes hurt.
- **Armour** is a physics object too: take it, and let go of it over your torso to put it on (only if it beats what
  you wear). Throw it, knock it, force-grab it (Carrying and Gibs > *Armour*).
- A closed hand never grabs by moving onto something: carrying starts when you press the grip.
- **Gibs and heads** can be picked up, thrown (they burst against walls) and force-grabbed (*Gibs and Heads*).
  Corpses can be gibbed by shots and blows.
- Settings: the *Throwing and Physics* page.

## Force grab

Point an open, empty hand at a weapon, backpack, or ammo or health box. It glows and sparkles. Pull the trigger to
lock on, then flick your hand back or up. The item flies to you in an arc and arrives in about half a second.
Close your hand (grip) as it arrives to catch it. Too early or too late, and it drops at your feet. It flies
through walls, so it can't get stuck. Settings: the *Force Grab* page, and *Force Grab* on the main page.

## Melee

- **Blows:** punch, or swing the axe, a sword or any gun. A blow has to be a real one: the wrist must move fast
  enough (*Swing Speed*) and far enough (*Blow Distance*), so waving and flicks don't hit. Harder blows do more
  damage, and straight punches more than slaps. You can hit a gib held in your other hand.
- **Parry:** hold a weapon (one or two hands) level across in front of you as a monster's blow lands. It takes less
  damage, with a clang and sparks. A one-handed parry may knock the weapon out of your hand. *Unarmed Parry*: cross
  your forearms in an X.
- **Bash:** from a guard (a weapon held level across, or both hands together), push forward hard. It does little
  damage, but it throws the monster back and staggers it. One open hand, palm ahead, shoves half as hard. Shove
  monsters off ledges.
- **Headbutt:** lunge your head at a monster.
- **Batting projectiles:** swing a weapon or your fist through a spike, laser, spit or grenade to send it back. A
  bash or a shove with a weapon in hand bats them back too, with a more lenient reach and timing.
- **Blade or bash:** a sword hits with the blade whatever its angle; the hilt landing first doesn't spoil the cut.
  A bash needs the guard held still first, a shove open palms pushed hard at the enemy.
- **Knockback:** your blows push monsters, their blows push you, and a parry pushes you both apart.
- Settings: the *Melee* page, and *Gameplay* (Parry and Bash, Feel, Headbutt). The *firing range* has a training
  dummy that shows every hit's damage, kind and body part.

## Damage and gore

- **Positional damage:** headshots (with a crack you can hear), and weaker arm and leg shots, on humanoid monsters.
- **Damage settings:** damage to enemies, to you, and from your own explosives (*Gameplay* page).
- **Gore** (the *Gore* page; *Gore*: normal, more, or over the top, the default): blood sprays on walls and floors,
  gibs that stick to ceilings and walls and drip, pools spreading under corpses, and blood trails from gibs. When
  you are hurt, your arms and hands get bloodier and drip on the floor.
- **Decals:** blood, scorch marks and bullet holes (Graphics, or Gore > *Marks*).

## Movement

- **Smooth locomotion** with the off-hand stick, towards your head or your off hand (*Move Towards*). Run or walk by
  default (*Default Speed*); clicking the off-hand stick switches to the other.
- **Turning:** smooth, or snap 30, 45 or 90 degrees.
- **Teleport** (off by default): turn on *Teleport* and bind a button to `+teleport`, for example
  `bind LTHUMB +teleport`. Aim with the off hand and let go on a blue spot.
- **Room-scale:** walking in your room moves you in the game, with collision (*Room Scale* sets the ratio). Jumping
  for real jumps (*Roomscale Jump*). You can crouch, and lean up to a wall or over a railing before your body
  follows (*Lean*).
- **Swimming:** in water the stick slows, and strokes of your hands (palm first) move you where you look. Point
  your off hand up or down while pushing forward to swim up or down, or use the main stick's up and down. The
  *Swimming* page tunes the strokes.
- **Ledge grab (experimental):** Locomotion > *Ledge Grab*. Grip a ledge with an empty hand to hang from it, and
  pull down to climb onto it. It only works on ledges, not walls.

## Body

- **Body** (VR Settings > Body): *Off*, *Torso and arms*, or *Full body* (the default), with legs that walk and step
  round as you turn. Your arms reach your hands (inverse kinematics), and the body crouches and leans with your
  head.
- **Build:** lean, athletic or brawny.
- **What you wear:** the ranger's clothes, pauldrons, your armour (green, yellow or red plates), your wounds, and
  your powerups (quad damage sparks round your hands, the pentagram glows red, the ring fades you).
- **Fingers** curl with the trigger (index), the grip (middle to little finger) and your thumb resting on a button
  or stick. Each weapon's finger and thumb positions and openness can be tuned (Weapon Offsets > Fingers).
- **Holsters** at the hips follow your thighs as the legs walk; in water the legs trail and kick.
- **Placement:** *Torso Offset*, *Legs Offset* and *Shoulders Offset* on the main page. The *Body* page has much
  more.

## HUD, menus and screens

- **Wrist gadget** (the default HUD): a device on the back of your off-hand forearm. Raise your forearm across your
  chest, like reading a watch. Its screen shows health, armour, ammo, and optionally the level and stats, with a CRT
  look. Console messages float above it while it faces you. The *Wrist Gadget* page sets its arm, size, placement
  and colours.
- **Status bar:** the classic Quake status bar on a hand (VR Settings > *HUD: Status bar*).
- **Messages:** the game's messages (a key needed, a secret found, pickups) float as a hologram the wrist gadget
  projects over its screen (*Game Messages as Hologram*); centre prints float in front of you.
- **Menus** float in front of you, with modern sliders and switches. Point with the laser from your hand, pull the
  trigger to click, and drag sliders. The sticks work too. *Back to Game* is at the top left, or you can hold the
  menu button. Settings pages show their changes live, and in single player the game keeps running underneath.
- **Map boards** (the text signs in the hub, tutorial and firing range) are CRT screens.
- **Colours:** one *Player Effects Hue* colours the gadget's screen, the force grab, the teleport arc, the crosshair
  and the menu laser (Wrist Gadget > Colours).

## Flashlight

A torch clipped to the front of your chest (VR Settings > *Chest Flashlight*). Put a hand at it and pull the
trigger to switch it on or off. Grip it to take it in your hand, and let go to send it back. Bring it to the gun
in your other hand and press B or Y to clip it on the gun; it comes off with the other hand and B or Y, or goes
back to your chest when the gun is holstered or dropped. It stays on across level changes and goes off when you
start a map afresh. It's a real spotlight: it lights models and casts shadows. Settings: the *Body* page, under Flashlight.

## Haptics

Your controllers buzz when you fire, hit, get hit (on the side the hit comes from), hover over a holster, or catch
a force-grabbed item. Explosions nearby rumble, and a heartbeat beats at low health. You can turn all of it off
(*Haptics* on the main page) or tune it on the *Gameplay* and *Immersion* pages.

## Graphics

All of these have switches on the *Graphics* page, and the *Preset* there sets most of them at once.

- **Relit maps** (optional, see [INSTALL.md](INSTALL.md#relit-maps-and-see-through-water)): Quake's maps relit on
  your PC with ericw-tools, with soft shadows, ambient occlusion in corners, coloured light, and lamps, light panels
  and glowing textures that light their surroundings. The relit maps also get **see-through water** (water, slime
  and teleporters; lava stays opaque) and the data for real light directions on bumps (deluxemaps). *Relit Maps*
  switches between them and the original lighting.
- **The look:** darker shade and stronger light, inspired by DarkPlaces. Your shots, explosions and glowing
  projectiles light up the room in their colours, and lamps, buttons and screens glow (bloom). *Off (Quake)*
  restores Quake's look.
- **Real-time shadows:** explosions, rockets, your muzzle flash (optional) and torches cast shadows. The map lights
  nearest you cast the shadows of monsters and of your own body. The graphics *Preset* (Advanced VR Options > Graphics) goes from *Off
  (Quake)* through *Low*, *Medium* and *High* to *Ultra*.
- **Model lighting:** monsters, items, weapons, hands and your body are lit from the map's own lights, with
  directional ambient, a rim light, and reflections on your weapons. Dynamic lights light models per pixel.
- **Ambient occlusion on what moves:** monsters, your body and items darken the floor and walls round them, lifts
  and doors their shafts, and models their own creases (*Ambient Occlusion*, Graphics - Shadows).
- **Surfaces:** bump maps made from every texture (or a texture pack's own normal maps), with a sheen under
  dynamic lights. Parallax makes walls look deep. Detail textures (stone, metal, wood grain) sharpen surfaces up
  close, where Quake's textures would blur. There is also anti-aliasing for sheen and for fences and grates.
- **Water and liquids:** waves that move the surface, reflection and refraction, glints, caustics, big splashes
  and ripples that move the waves, shoreline foam, heat haze over lava, fog and a gentle wobble under water, and water sounds.
- **Particles and effects:** textured smoke, sparks, blood and explosions (*Quake VR Particles*) that fade softly
  into walls, shell casings, and torches whose light flickers.
- **Tone and colour:** tone mapping, exposure, colour grades and dither, plus the headset's own gamma and contrast.
- **HD textures:** replacement textures (such as QRP's) are filtered smoothly and get bump maps.
- **Headset:** *Render Scale* renders the eyes at a lower or higher resolution (0.5 to 1.5), with *Upscaling* (FSR 1
  or NIS) sharpening a lower one back up; *Foveated Rendering* (NVIDIA cards) shades the edges of the view, which
  the lenses blur, at a lower rate; *Hide Lens Corners* skips the pixels the lenses never show.

## Maps, mission packs and mods

- **The VR hub** (`vrstart`) is where the game starts. Pick Quake, Scourge of Armagon or Dissolution of Eternity,
  and step into the portal. Its signs explain the basics. From *Advanced VR Options > Play* you can go back to the
  hub, the **tutorial** or the **firing range** (weapons to try, both swords, and a training dummy).
- **Mission packs:** installed ones (the `hipnotic` and `rogue` folders) are used automatically, with their
  weapons, monsters and maps. Quake VR's QuakeC contains all three campaigns.
- **Custom maps** without their own `progs.dat` play with Quake VR's gameplay. **Other mods** run in a
  compatibility mode: your hand aims and their weapons fire from your gun, and you can walk the room and teleport,
  but there are no off-hand weapons, holsters, throwing or melee. See [INSTALL.md](INSTALL.md#custom-maps-and-mods).
- **Bots:** *Play* > Bots adds and kicks FrikBot bots for multiplayer.
- **Flat screen:** `vr_enabled 0` plays on the monitor, with a mouse and keyboard.

## Voice notes

A playtesting aid (Gameplay > *Voice Notes*, on by default). Raise your off hand to your mouth, like a radio, and
hold **Y** to talk. Let go to save the note. "REC" shows while you record, and the hand buzzes as a note starts and
ends. Away from your mouth, Y does what it always does. A note shorter than 0.8 seconds counts as an accidental press
and is dropped.

Each note is saved in `quakevr\notes\` as a `.wav`, with a screenshot and where you were: the map, your position,
health, and what each hand held. By default the microphone is Virtual Desktop's if there is one, otherwise the
default microphone (`vr_note_device`; `vr_note_devices` lists them). [INSTALL.md](INSTALL.md#voice-notes) explains
how to turn them into text.
