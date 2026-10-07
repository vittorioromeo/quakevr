# Explosion debris and fire particles

Merged into `vr-ironwail` (`vr_explosiondebris.cpp`, `vr_fireparticles.cpp`), with the improved explosion texture from the preceding change.

Settings: **Advanced VR > Graphics > Particles**. This page contains **Large Fireballs per Explosion**, **Explosion Debris**, and **Fire Particles**. Fire Particles is also linked from Wall Torches.

## Explosion debris

The ordinary explosion, colored explosion, tarbaby explosion, classic particle-count-1024 explosion, and Quake VR Explosion particle preset spawn physical model chunks. This covers the game's weapons, monsters, explosive boxes and mission-pack temp-entity explosions. One event makes one set; the particle-off preset fallback does not double it.

The chunks use the existing debris mesh, scaled like small gibs, with molten fullbright skins. They have gravity, tumble, swept collision against level and moving brush geometry, friction and configurable restitution. They are client simulation objects like shell casings, with a fixed 256-slot pool and oldest-first eviction. They have no server edicts, collision/damage callbacks, character collisions, pickup or gameplay knockback. They cannot injure players or monsters.

Each expires after its independently randomized lifetime and fades in the last fraction of a second. Flames and smoke trail moving chunks. The nearest chunks cast small unshadowed orange dynamic lights; a separate light budget (default 8, maximum 16) preserves room for other game lights. Their model skins remain incandescent even outside that budget. Reducing the active limit or disabling the effect retires chunks and lights immediately. Map changes and loads reset the pool.

Defaults: 12 chunks, 3–9 m/s, upward direction bias 0.65, diameters 1.4–3.2 Quake units, 2–4 seconds, bounciness 0.45, at most 96 chunks, trail density 1, 8 lights with 48-unit radii. Speed/size/light radius scale with VR world scale. Reversed min/max endpoints are accepted; console values are bounded to the menu's safe extended limits.

`vr_explosion_particles` is the exact number (0–16) of large textured fireball sprites per ordinary, colored or tarbaby explosion, independent of `vr_particle_mult`. Each already gets independent size, rotation, position and velocity. Zero keeps the other blast particles. Tarbaby fireballs retain their violet tint.

Rebuild the molten mesh skins with `python Misc/quakevr/make_explosion_debris.py` (reuses `vr_rock1.mdl` and the bundled Quake palette).

## Fire particles and torch tilt

The generated `particle_fire.tga` is a soft curled tongue of fire, neutral in color for engine tinting. Its full-resolution Imagegen source and prompt are retained beside the explosion source. The existing low-poly models remain.

The emitter scans static and current dynamic `flame.mdl`, `flame2.mdl`, candle and lantern models. This includes map fires and every attached flame on burning monsters, corpses and objects. Taken/hung torch flames, which are generated temporary model entities, emit at their actual transformed flame position through the torch renderer. Emission does not depend on the torch-light budget. Out-of-range sources pause, extinguished/missing sources are pruned, and map/game-directory/model changes release emitter clocks.

Controls include exact particles per burst, bursts per second, min/max horizontal outward speed, min/max world-up speed, min/max life, size, opacity and emission distance. Default: 8 particles at 10 bursts/s, 0.1–0.4 m/s outward, 0.3–0.9 m/s up, 0.25–0.65 seconds, size 2.5, fully opaque, from 0.2 of the way up the flame (the author's, config 95). Fire particles require both particle toggles; emission frequency or count zero stops new emissions without removing flame models.

A taken torch has one flame at its head. It follows the torch's direction, keeps full height through 90°, then smoothly shortens to 15% at 180°. `vr_walltorch_inv_size` now controls inverted height (0.05–0.3); legacy saved values above 0.3 use 0.15. Width remains unchanged. Particles always rise in world space. Existing fuel, water/slime extinguishing, relighting and burning-contact rules remain.

## Validation

Release x64 and clean QuakeC builds; `Misc/quakevr/check_statics.py`; `git diff --check`.

Runtime diagnostics:

- `vr_explosion_debris_test [normal|colored|tar|preset|clear]`: effects ahead, without blast damage.
- `vr_explosion_debris_stats`: pool, collision, trail, light, lifetime/speed and health counters.
- `vr_fire_particles_stats`: static/dynamic/torch sources and emitted count.
- `vr_walltorch_tilt_test`: actual flame transforms at 0°, 90°, 180°.

`effects_review.ps1 -Mode debris|fire|sources -Base <isolated game base> -Exe <branch engine>` uses the mock backend and a hidden rendered window. The base needs id1, hipnotic and rogue game data, and a quakevr directory with this branch's progs.dat, models, textures, maps and config. It replaces quakevr/autoexec.cfg and writes screenshots/logs to that base and disables config writes. `sources` exercises a held torch on e1m2 and a newly spawned burning monster. Run `python check_effects_review.py <debris.log> <fire.log> [sources.log]` to assert the outcomes.

A fresh Windows build also exposed pre-existing tips-module issues: local identifiers collided with Windows' `near` macro, and a function-local static failed the repository check. Those are fixed without changing tip behavior.
