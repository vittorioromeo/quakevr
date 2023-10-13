<a href="https://vittorioromeo.info/quakevr">
    <p align="center">
        <img width="460" height="250" src="https://vittorioromeo.info/Misc/quakevrlogo.png">
    </p>
</a>

# [Quake VR](https://vittorioromeo.info/quakevr)

> **The timeless classic from 1996, reimagined for virtual reality.**

[*(Visit the official landing page for videos and more information.)*](https://vittorioromeo.info/quakevr)

## Main Features

* Smooth locomotion
* Teleportation
* Fully room-scale *(including jumping)*
* Hand tracking *(including full finger tracking)*
* Weapon models with hands and ironsights
* VR interactions *(melee, headbutts, pick up items, press buttons)*
* Rebalanced for VR *(hitboxes, difficulty, projectile speed)*
* Steam VR Input API support *(can rebind inputs in Steam VR settings)*
* Many customization options *(e.g. melee damage/range, hand/body item pickup, and more)*
* Brand new particle system *(smoke, blood, lightning effects, and more)*
* Haptic feedback *(weapons, melee, damage, interactions with items)*
* Full Scourge of Armagon support *(weapons, enemies, maps)*
* Full Dissolution of Eternity support *(weapons, enemies, maps)*
* Two-handed weapon aiming *(with virtual stock support)*
* Weapon weight simulation
* Grappling hook *(from Dissolution of Eternity)*
* Positional damage *(headshots, leg shots)*
* Dual-wielding
* Weapon holster system *(hip and shoulders, fully customizable)*
* Weapon throwing & force grabbing
* Multiplayer support *(also with bots)*
* Weapon reload system
* Sandbox map

## Disclaimer

* This is a free open-source project done for fun and to contribute to the VR community. **Donations are appreciated**, but making money is not a goal for `Quake VR`.
* The project is still in an early stage. Expect bugs and lack of polish. It was fully tested only with a Valve Index headset and controllers.
* **Tweaking the settings in-game is highly recommended before playing.** The default settings might not be ideal for your hardware and body proportions.
* The game heavily relies on modified *QuakeC* files. Therefore, mods or expansion packs are not compatible out of the box, but should be easy to port.
* Feel free to open issues or contact me at <vittorio.romeo@outlook.com> for any issue, feature request, or question about the code.
* Join us on the [**official Discord**](http://discord.me/quakevr) for discussion, feedback, and beta testing. *[(Direct invite link.)](https://discord.gg/wBBcjpn)*

## Installation

1. Install ["Visual C++ x64 Redistributable"](https://support.microsoft.com/en-gb/help/2977003/the-latest-supported-visual-c-downloads) package.

2. Install [7zip](https://www.7-zip.org/)

3. Install "Quake". The [Steam version](https://store.steampowered.com/app/2310/QUAKE/) works well.

4. Grab the latest binary from [the releases page](https://github.com/SuperV1234/quakevr/releases).

5. Extract the Quake VR archive into a new, empty folder *(e.g. `C:/Games/id/Quake/quakevr`)*.

6. Navigate to the `id1` folder in your installation of Quake *(e.g. `C:/Program Files/Steam/steamapps/common/Quake/id1`)* and copy both `PAK0.PAK` and `PAK1.PAK` to Quake VR's `id1` folder *(e.g. `C:/Games/id/Quake/quakevr/id1`)*.

7. Run the `quakevr.exe` executable in the root folder to launch the game!

8. (optional) To enable music, copy the `music` folder from the 'remaster' *(e.g. `C:/Program Files/Steam/steamapps/common/Quake/rerelease/id1/music`)* to Quake VR's `id1` folder as well.

### Mission Packs

Quake VR supports both of the official mission packs. They can be installed alongside the regular Quake files, enabling you to switch between them without having to restart Quake VR.

NOTE: With the release of the Quake 'remaster', both mission packs are included with the base game and no longer need to be purchased seperately.

#### Mission Pack 1: Scourge of Armagon

1. Navigate to the `hipnotic` folder in your installation of Quake *(e.g. `C:/Program Files/Steam/steamapps/common/Quake/hipnotic`)*

2. Make a copy of `PAK0.PAK`, and rename it to `PAK2.PAK`.

3. Copy the newly created `PAK2.PAK` into your Quake VR's `id1` folder *(e.g. `C:/Games/id/Quake/quakevr/id1`)*.

#### Mission Pack 2: Dissolution of Eternity

1. Navigate to the `rogue` folder in your installation of Quake *(e.g. `C:/Program Files/Steam/steamapps/common/Quake/rogue`)*

2. Make a copy of `PAK0.PAK`, and rename it to `PAK3.PAK`.

3. Copy the newly created `PAK3.PAK` into your Quake VR's `id1` folder *(e.g. `C:/Games/id/Quake/quakevr/id1`)*.

The contents of your `id1` folder should now contain:
   * Official Quake files: `PAK0.PAK` and `PAK1.PAK`;
   * Scourge of Armagon mission pack files: `PAK2.PAK`;
   * Dissolution of Eternity mission pack files: `PAK3.PAK`;
   * Quake VR files: `PAK10.PAK`, `PAK11.PAK`, `PAK12.PAK`.

   *(The load order of `.PAK` files is based on their naming. You can rename the mission pack files to anything else, as long as it is before the Quake VR specific `.PAK` files. This allows you to also add extra `.PAK` files, such as high-resolution models or other compatible additions.)*

When a mission pack is installed, it overwrites Quake 1's `start.bsp` with its own starting map. In order to play Quake 1 maps, use the *"Change Map"* functionality under *"Quake VR Settings"*.

### Highly Recommended Addons

#### HD Textures and Soundtrack

The [HD Textures and Soundtrack](https://drive.google.com/file/d/1noIH27xA8gnr_hwqouXiSoMQBIyuf__V/view) ([alternative link](https://drive.google.com/file/d/1UAH4la2uOv3lwMkMk05yZuYmiPIyExU_/view?usp=sharing)) addon is highly recommended to improve immersion.

* [Installation Instructions](https://old.reddit.com/r/ValveIndex/comments/fbs1nh/quake_vr_release_trailer_v001/fj7205c/)

#### Extra Multiplayer Maps with Bot Support

Quake VR comes with [Frikbot X++](http://neogeographica.com/site/pages/mods/fbxpp.html), which adds AI opponents for deathmatch. As the vanilla multiplayer map pool is quite limited, the following map pack and Frikbot waypoint collection are strongly advised if you are interested in playing against bots:

* [The Definitive Frikbot Mappack](https://www.quaddicted.com/files/misc/DefinitiveFrikbotMappackV2.zip)
* [The Definitive Frikbot Waypack](https://www.quaddicted.com/files/misc/DefinitiveFrikbotWaypackV2.zip) *([forum discussion](http://quakeone.com/forum/quake-mod-releases/finished-works/5313-the-definitive-frikbot-waypack))*

### Custom Maps

For supported custom maps, please see [CUSTOM_MAPS.md](https://github.com/SuperV1234/quakevr/blob/master/CUSTOM_MAPS.md).

### Vispatching/Colored Lighting

The Quake VR engine supports transparent water and colored lighting, however original Quake maps need to be patched in order for it to work properly. It is therefore highly recommended that you patch your Quake maps.

You can find the patch files [here](https://quakewiki.org/wiki/External_Lit_And_Vis_Files). Merely copy all the `.vis` and `.lit` files from the `maps` directory of the downloaded archive into your `maps` directory in your Quake VR installation's `id1` folder *(e.g. `C:/Games/id/Quake/quakevr/id1/maps`)* (this might break lighting on the level select maps `start.bsp` but shouldn't interfere with the actual levels. If you want, you can delete `start.lit` and `start.vis` from the maps folder to prevent this).

Once your maps are patched, go through the options in the *"Transparency Options"* menu under *"Quake VR Settings"* to enable/tweak transparency levels.

## First Steps

### SteamVR Bindings

The first thing you should do after starting Quake VR is opening the *"Controller Bindings"* interface on SteamVR and ensure that in-game actions are mapped to the motion controllers. There are two action sets to bind: one for in-game actions, and one for menu control. See an [example video **here**](https://giant.gfycat.com/ThornyEducatedBushbaby.mp4).

### In-Game Configuration

After setting up your bindings, please go through all the options in *"Quake VR Settings"*, and tweak the game to your liking. Do not forget to:

* Calibrate your height;
* Tweak the position of the *"VR torso"* and of the holsters (*"hotspots"*);
* Go through the *"Immersion Settings*".

There is no "best" way of playing Quake VR. Simply use the settings that you enjoy the most!

## Multiplayer

### Versus Bots

To play against bots, select *"Multiplayer & Bots"* from the main menu, then *"New Game"* and follow the on-screen instructions. When in-game, spawn new bots from the *"Bot Control"* menu under *"Multiplayer & Bots"*.

### On the Internet

Coordinate with other players on the [official Quake VR Discord](http://discord.me/quakevr). The host of the game will start a server by going to *"Multiplayer & Bots"* from the main menu, then *"New Game"*. The host will then share their [public IPV4 address](https://www.whatismyip.com/what-is-my-public-ip-address) with other players, which will connect from the *"Join a Game"* menu. The virtual keyboard can be used to insert the IP address.

Note that the host needs to [properly forward their ports](https://portforward.com/quake/) and ensure that their public IP is accessible from the Internet.

At the moment there is no server browser and no dedicated server support. Also, only Windows servers are supported. Dedicated Linux servers and an easier way to play together will be the next goals for Quake VR.

## Troubleshooting

* > The game seems to work fine, but there is no audio!

    * Ensure that the correct audio output device is set (HMD) and that the volume is turned up.

    * Open the Desktop view from your Steam VR dashboard, and make sure that the Quake VR window is in focus. You can bring it in focus by clicking on it.

    * Check that the in-game audio settings are correct.

* > How do I obtain the grappling hook?

    * The grappling hook is available in the sandbox map, accessible through the "single player" menu.

    * The grappling hook is also available through cheats: it is a very fun *toy* but it would completely break the progression of most levels. To obtain it, go to the "Debug Utilities" sub-menu under the "Quake VR Dev Tools" menu - there will be an option to enable it there. You can then spawn it by using the "Impulse 14 (Spawn All)" command, or cycle to it if you have weapon cycling enabled.

* > My issue is not listed here!

    * Join the [official Quake VR Discord](http://discord.me/quakevr) and ask for help in the `#troubleshooting` channel.

## Credits

* Built on top of QuakeSpasm and existing forks
    * [Fishbiter's OpenVR port](https://github.com/Fishbiter/Quakespasm-OpenVR)
    * [Zackin5's OpenVR port](https://github.com/Zackin5/Quakespasm-OpenVR)
    * [Dominic Szablewski's (Phoboslab) Oculus port](https://github.com/phoboslab/Quakespasm-Rift)
    * [Quakespasm-Spiked](http://triptohell.info/moodles/qss/)

* Hand models with finger tracking support created by *[CrazyHairGuy](https://www.crazyhairguy.com/)*
    * Many thanks!

* Weapon ironsights modeled by me
    * Using the [Authentic Model Improvements](https://github.com/NightFright2k19/authmdl) project as a base

* Many thanks to the following beta testers, who provided a lot of great feedback: *carn1x*, *GeekyGami*, *Sly VR*.

* Many thanks to *Spoike* for helping me out with QuakeC and Quake engine internals.

*(Please forgive me if I am forgetting someone. I will update this list as needed.)*

## Fan Coverage

*(Thanks everyone for playing Quake VR! If you have created content related to the mod, please let me know and I'll add it to this list.)*

* VHS Productions's [gameplay video](https://www.youtube.com/watch?v=fwyHMHvGOiI) with commentary and webcam.

* Sly VR's gameplay videos: [part 1](https://www.youtube.com/watch?v=lAlJubb64g0), [part 2](https://www.youtube.com/watch?v=M0Pv66638Hc), [part 3](https://www.youtube.com/watch?v=ST9w7dwW6Rw), [part 4](https://www.youtube.com/watch?v=pzSvgJWMnr8), and [part 5](https://www.youtube.com/watch?v=-gXMjqcgPdE). With commentary.

* UploadVR [article](https://uploadvr.com/new-quake-vr-mod/) and [gameplay video](https://www.youtube.com/watch?v=fBzCdMSF2-U).

* My own [let's play series](https://www.youtube.com/playlist?list=PLTEcWGdSiQemN50YKFbEpR9har292EJns) with developer commentary and webcam. Nightmare difficulty!

* Elia1995's [gameplay video](https://www.youtube.com/watch?v=rvigiMdIT-M) with webcam.

* Stoni's [gameplay video](https://www.youtube.com/watch?v=Jserap1p2Ho) with webcam and commentary. And [another one](https://www.youtube.com/watch?v=qcxznJU7jFI) covering update v0.0.5!

* Custom Gamer's [video](https://www.youtube.com/watch?v=eeqtFDf3tkM) showcasing his awesome custom map in VR!

* Gamertag VR's [livestream video](https://www.youtube.com/watch?v=W1_WG_PHUpw) with webcam and commentary.

* Tom's [gameplay video](https://www.youtube.com/watch?v=96xC_khtrUE).

* GmanLives's [classic FPS in VR video](https://www.youtube.com/watch?v=FukJelV3-Yk), which also includes Quake VR.

* DoomSly82's ["Remember To Reload" Gameplay Video](https://www.youtube.com/watch?v=EKaRcYbmYHE), which covers update v0.0.6.
