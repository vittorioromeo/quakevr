<p align="center">
  <img width="460" height="300" src="https://vittorioromeo.info/Misc/quakevrlogo.png">
</p>

# Quakespasm-OpenVR

OpenVR support integrated into Quakespasm. This version contains a few changes I wanted to make for my own playthroughs of Quake in VR.

Forked from [Fishbiter's OpenVR port of Quakespasm](https://github.com/Fishbiter/Quakespasm-OpenVR) Forked from [Zackin5's OpenVR port of Dominic Szablewski's (Phoboslab) Oculus modification of Quakespasm](https://github.com/Zackin5/Quakespasm-OpenVR)
Forked from [Dominic Szablewski's (Phoboslab) Oculus modification of Quakespasm](https://github.com/phoboslab/Quakespasm-Rift) and utilizing the [OpenVR C wrapper by Ben Newhouse](https://github.com/newhouseb/openvr-c).

# Expansion pack and mod support
This version of Quakespasm-OpenVR has support for the Quake expansion packs _Scourge of Armagon_ and _Dissolution of Eternity_ and for the _Arcane Dimensions_ mod. The new weapons which have been added have been scaled and aligned to decently match VR hand position.

__Known issue for DoE:__ When using the Lava Nailgun, no targeting dot/laser sight are drawn, but the weapon still fires where you would expect it to

__Notes for AD:__

* The mod should be launched by using the `-game ad` command line argument, this will ensure that the correct weapon offsets are used
* When launching AD will not display anything in VR. Press the __Enter__ key twice in order to get in game and play in VR.

# Cvars

* `vr_enabled` – 0: disabled, 1: enabled
* `vr_crosshair` – 0: disabled, 1: point, 2: laser sight
* `vr_crosshair_size` - Sets the diameter of the crosshair dot/laser from 1-32 pixels wide. Default 3.
* `vr_crosshair_depth` – Projection depth for the crosshair. Use `0` to automatically project on nearest wall/entity. Default 0.
* `vr_crosshair_alpha` – Sets the opacity for the crosshair dot/laser. Default 0.25.
* `vr_aimmode` – 7: Head Aiming, 2: Head Aiming + mouse pitch, 3: Mouse aiming, 4: Mouse aiming + mouse pitch, 5: Mouse aims, with YAW decoupled for limited area, 6: Mouse aims, with YAW decoupled for limited area and pitch decoupled completely, 7: controller attached. Default 7. (Note I haven't been very careful about maintaining these other modes, since they're obsolete from my point of view).
* `vr_deadzone` – Deadzone in degrees for `vr_aimmode 5`. Default 30.
* `vr_viewkick`– 0: disables viewkick on player damage/gun fire, 1: enable
* `vr_world_scale` - 1: Size of the player compared to normal quake character.
* `vr_floor_offset` - -16: height (in Quake units) of the player's origin off the ground (probably not useful to change)
* `vr_snap_turn` - 0: If 0, smooth turning, otherwise the size in degrees of each snap turn.
---
__New cvars for analog stick (and touchpad?) tuning on VR controllers.__ Default values should behave the same as before, but note that this version has not been tested with snap turning enabled. These have only been tested with analog sticks (Oculus Touch and Index Controllers), no idea how they behave with Vive touchpads.
* `vr_joystick_yaw_multi` - 1.0: Adjusts turn speed when using VR controllers, suggested 2.0-3.0
* `vr_joystick_axis_deadzone` - 0.25: Deadzone value for joysticks, suggested 0.1-0.2
* `vr_joystick_axis_exponent` - 1.0: Exponent for axis input, suggested 2.0. Larger numbers increase the 'low speed' portion of the movement range, numbers under 1.0: decrease it, 1.0 is linear response. 2.0 makes it easier to make fine adjustments at low speed
* `vr_joystick_deadzone_trunc` - 1 If enabled (value 1) then minimum movement speed will be given by the deadzone value, so it will be impossible to move at speeds below the deadzone value. When disabled (value 0) movement speed will ramp up from complete standstill to maximum speed while above the deadzone, so any speed is possible. Suggest setting to 0 to disable

# Note about weapons

Quake's weapons don't seem to be particularly consistently sized or offset. To work around this there are cvars to position/scale correct the weapons. Set up for the default weapons are included but mods may require new offsets.

There are 20 slots for weapon VR offsets. There are 5 cvars for each (nn can be 01 to 20):

* 'vr_wofs_id_nn' : The model name to offset (this name will be shown when equipping a weapon that doesn't have a VR offset
* 'vr_wofs_scale_nn' : The model's scale
* 'vr_wofs_x_nn' : X offset
* 'vr_wofs_y_nn' : Y offset
* 'vr_wofs_z_nn' : Z offset

You can place these in an autoexec.cfg in the mod's directory.
