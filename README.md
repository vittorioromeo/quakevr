<p align="center">
  <img width="460" height="250" src="https://vittorioromeo.info/Misc/quakevrlogo.png">
</p>

# quakevr

> **One of the best first person shooters ever, modded and rebalanced for virtual reality.**

[*(See the release trailer on YouTube.)*](https://www.youtube.com/watch?v=AevBPsuMab0)

## Main Features

* Smooth locomotion
* Fully room-scale *(including jumping)*
* Hand tracking
* Weapon models with hands and ironsights
* VR interactions *(melee, pick up items, press buttons)*
* Rebalanced for VR *(hitboxes, difficulty, projectile speed)*
* Steam VR Input API support *(can rebind inputs in Steam VR settings)*
* Many customization options *(e.g. melee damage/range, hand/body item pickup, and more)*

## Disclaimer

* This is a free open-source project done for fun and to contribute to the VR community. Donations are appreciated, but making money is not a goal for `quakevr`.
* The project is still in an early stage. Expect bugs and lack of polish. It was fully tested only with a Valve Index headset and controllers.
* Tweaking the settings in-game and in `config.cfg` might be required for a good experience on your particular VR setup.
* The game heavily relies on modified *quakeC* files. Therefore, mods or expansion packs are not compatible out of the box, but should be easy to port.
* Feel free to open issues or contact me at <vittorio.romeo@outlook.com> for any issue, feature request, or question about the code.

## Installation

1. Install [Visual C++ x64 Redistributable](https://support.microsoft.com/en-gb/help/2977003/the-latest-supported-visual-c-downloads) package.

2. Install Quake 1. The [Steam version](https://store.steampowered.com/app/2310/QUAKE/) works fine.

3. Grab the latest binary from [the releases page](https://github.com/SuperV1234/quakevr/releases).

4. Extract the contents of the package directly in the Quake installation folder *(e.g. `C:/Program Files/Steam/steamapps/common/Quake`)*.

5. Run the game by launching the `.exe` you just extracted.

### Highly Recommended Addons

* [HD Textures and Soundtrack](https://drive.google.com/file/d/1noIH27xA8gnr_hwqouXiSoMQBIyuf__V/view)

    * [Installation Instructions](https://old.reddit.com/r/ValveIndex/comments/fbs1nh/quake_vr_release_trailer_v001/fj7205c/)

## Credits

* Built on top of QuakeSpasm and existing forks
    * [Fishbiter's OpenVR port](https://github.com/Fishbiter/Quakespasm-OpenVR)
    * [Zackin5's OpenVR port](https://github.com/Zackin5/Quakespasm-OpenVR)
    * [Dominic Szablewski's (Phoboslab) Oculus port](https://github.com/phoboslab/Quakespasm-Rift)

* Hands and ironsights modeled by me
    * Using the [Authentic Model Improvements](https://github.com/NightFright2k19/authmdl) project as a base

*(Please forgive me if I am forgetting someone. I will update this list as needed.)*

## CVars

*(This list is not exhaustive. More options are exposed in the in-game menu, or available via the console.)*

* `vr_crosshair` - 0: disabled, 1: point, 2: laser sight
* `vr_crosshair_size` - Sets the diameter of the crosshair dot/laser from 1-32 pixels wide. Default 3.
* `vr_crosshair_depth` - Projection depth for the crosshair. Use `0` to automatically project on nearest wall/entity. Default 0.
* `vr_crosshair_alpha` - Sets the opacity for the crosshair dot/laser. Default 0.25.
* `vr_aimmode` - 7: Head Aiming, 2: Head Aiming + mouse pitch, 3: Mouse aiming, 4: Mouse aiming + mouse pitch, 5: Mouse aims, with YAW decoupled for limited area, 6: Mouse aims, with YAW decoupled for limited area and pitch decoupled completely, 7: controller attached. Default 7. (Note I haven't been very careful about maintaining these other modes, since they're obsolete from my point of view).
* `vr_deadzone` - Deadzone in degrees for `vr_aimmode 5`. Default 30.
* `vr_viewkick` - 0: disables viewkick on player damage/gun fire, 1: enable
* `vr_world_scale` - 1: Size of the player compared to normal quake character.
* `vr_floor_offset` - -16: height (in Quake units) of the player's origin off the ground (probably not useful to change)
* `vr_snap_turn` - 0: If 0, smooth turning, otherwise the size in degrees of each snap turn.
* `vr_turn_speed` - 1.0: Adjusts turn speed when using VR controllers, suggested 2.0-3.0

### Weapon Offsets

Quake's weapons don't seem to be particularly consistently sized or offset. To work around this there are cvars to position/scale correct the weapons. Set up for the default weapons are included but mods may require new offsets.

There are 20 slots for weapon VR offsets. There are 5 cvars for each (nn can be 01 to 20):

* `vr_wofs_id_nn` - The model name to offset (this name will be shown when equipping a weapon that doesn`t have a VR offset
* `vr_wofs_scale_nn` - The model`s scale
* `vr_wofs_x_nn` - X offset
* `vr_wofs_y_nn` - Y offset
* `vr_wofs_z_nn` - Z offset

These can be modified directly in the in-game menu. A reasonable set of defaults is provided in the `config.cfg` shipped with `quakevr` releases.
