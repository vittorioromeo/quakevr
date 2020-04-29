
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
