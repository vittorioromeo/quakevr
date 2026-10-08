# Logo splash (trailer motion graphics)

A 12 s, 60 fps logo sting on a transparent background, for the Quake VR: Unleashed trailer:

| Frames  | Time        | What happens |
|---------|-------------|--------------|
| 0-67    | 0.00-1.12 s | A grunt idles (id's `soldier.mdl`, stand1-8 lerped as Ironwail does). |
| 50-68   | 0.83-1.13 s | Quake VR's axe flies in from the left, spinning, motion blurred. |
| 68      | 1.13 s      | He bursts: a flash, a red mist, gibs (`gib1-3`, `h_guard`, Quake VR's brains) and a wide spiky splash of wet blood on the wall. Drips start to run. |
| 96-188  | 1.6-3.13 s  | The logo's "QUAKE VR" slams in letter by letter from the camera, its shadow closing in (each lands at a `LAND` frame): a kick, a crown of blood squirted out, a ripple through the blood, dust, chips. |
| 214-296 | 3.57-4.93 s | "Unleashed" is wiped into the blood as negative space, stroke by stroke. |
| 300-342 | 5.0-5.7 s   | The red "UNLEASHED" materialises in the wiped letters (a hot front rising through each), then a glint. |
| 352-719 | 5.87-12 s   | Everything bursts into flames (the installer's fire colours and embers), then keeps burning (the last 3.5 s steady). The letters keep their steel colour: the fire's light only lifts them a little, with a hint of warm flicker. |

The timeline, layout, camera and shake are all in `common.py`.

## Files

- `common.py`: the timeline, the layout, the camera, the axe and gib trajectories, the shake (plain Python, shared by
  both Pythons).
- `trace_logo.py`: the logo's own lettering ("QUAKE VR" with its Q and nail, the red "UNLEASHED") traced from
  `docs/images/quakevr-unleashed-square.webp` into vector outlines (`letters.json` in the output folder), in the
  logo's layout. Run it first.
- `blender_scene.py`: the 3D layers, rendered with Cycles (OptiX) headless: the grunt, axe and gibs (`fg`), the falling
  letters (`letters`), and stills (the letters at rest, each alone, and fire-lit; "UNLEASHED" plain and fire-lit).
  It reads id1's `pak0.pak` in place (nothing extracted) and Quake VR's `quakevr/progs/v_axe.mdl` and
  `gib_brain*.mdl`. The lettering is the traced outlines, extruded and chamfered: steel for the title, glossy
  red for "UNLEASHED"; the gibs are dark, glossy and smeared with blood.
- `fx.py`: the 2D effects in numpy: the blood (thickness fields with an arrival frame per pixel, drips, the wet
  shading: Beer-Lambert colour, a darker rim, a sharp highlight), the finger writing (a skeleton of the letters walked
  stroke by stroke), particles, dust, the fire and its embers.
- `fx_precompute.py`: the fields that don't change per frame.
- `composite.py`: the frames, in parallel; premultiplied float internally, written as **straight alpha** RGBA PNGs
  (8 bits, dithered; transparent pixels carry the colour around them, so there are no dark fringes when Resolve
  filters or scales them). `--shake 0` renders without the baked camera shake.
- `encode.py`: ProRes 4444 with alpha (PyAV's `prores_ks`, `yuva444p10le`, Rec.709), the preview MP4 over a checker,
  the contact sheet, the alpha test (frames over white, a bright picture and black), and the 1080p copy (scaled with
  premultiplied alpha).
- `head_clip.py`: the decapitated head overlay (below).
- `sound.py`: the sound track (Quake's sounds and synthesis; SOUNDS.md).
- `render_all.sh`: everything, in order (the full version).

## The no-grunt version (to lay over a real in-game kill)

The same intro without the idling grunt and the thrown axe: `composite.py --variant nogrunt` (Blender's
`fg --nogrunt 1` layer: the gibs alone), and `sound.py --variant nogrunt` for its track.

- **Impact (the burst) = frame 30** (0.500 s at 60 fps). Frames 0-29 are an empty, fully transparent lead-in for
  lining up. Its frame g is the full version's frame g + 38. It is 682 frames long (11.367 s).
- **Where:** the burst starts at **(960, 481) in 1920x1080** (horizontally centred, 44.5 % down from the top; (1920,
  962) at 4K): where the grunt's chest was. Line that point up with the grunt in the footage, and frame 30 with the
  frame his body bursts.
- Its track: a 48 kHz, 24-bit WAV, exactly 682 frames long (545,600 samples), frame-synced (SOUNDS.md lists the
  sources).

## The decapitated head (an overlay on top of the kill and the no-grunt intro)

Quake VR's grunt head gib (`quakevr/progs/h_guard.mdl`, the head `vr_decap.qc` throws for `monster_army`) flies off
the neck, tumbling up and to the right, spurting from the stump (a burst at the cut, then about four arterial spurts
a second, dying away over 1.3 s), with a slight motion blur. It leaves the frame on the right at about frame 78.

- `blender_scene.py --pass head` (night lighting: a blue-grey moon from above, flickering warm torch rims from the
  left and the right, vrstart's colours; switch to `vrtrailer`'s once that map ships), then `head_clip.py` (the blood,
  the shake of the no-grunt intro), then `sound.py --variant head` (its own stem: the rip and pop, the spurts).
- Numbered as the no-grunt intro: 131 frames, **the cut is frame 30**, frames 0-29 empty. The head's neck starts at
  **(966, 392) in 1920x1080**, 89 px above the burst's centre (960, 481): where the intro grunt's neck was. Put it on
  a track above the intro at the same start frame.

## Versions

- v1 (2026-10-08, `logo_splash_3840_*`, `png_3840`, `png_1920`): Book Antiqua capitals, 8.5 s.
- v2 (`logo_splash_v2_*`, preview only so far: 1080p MP4 over checker, contact sheet, shake/no-shake clips, a v1/v2
  comparison of the letters in the fire): the logo's own lettering, stronger landings (crown, kick, shadows), darker
  wet gibs, 12 s, steel letters in the fire. Its layers and frames are in `work_v2_1080`. Render the full v2 with
  `render_all.sh` once approved.

## Render

Needs Blender 5.2 (headless only), Python 3.13 with numpy, Pillow and PyAV, and the player's id1 `pak0.pak`
(default `C:\OHWorkspace\qvr-kit\qbase\id1\pak0.pak`; `blender_scene.py --pak` to change it).

```
bash Misc/trailer/logo_splash/render_all.sh          # 4K, then the 1080p copy
bash Misc/trailer/logo_splash/render_all.sh 1920     # a quick 1080p pass (about 8 minutes)
```

Outputs go to `C:\OHWorkspace\qvr-trailer\logo_splash\` (override with `QVR_SPLASH_OUT`):

- `png_3840\logo_splash_0000.png` ... `_0509.png`: RGBA PNG sequence, straight alpha, 3840x2160.
- `png_1920\`: the same at 1920x1080.
- `logo_splash_3840_prores4444.mov`, `logo_splash_1920_prores4444.mov`: ProRes 4444 with alpha, 60 fps.
- `preview_over_checker.mp4`: H.264 over a dark checker, for a quick look.
- `contact_sheet.png`, `alpha_test.png`.
- `work\`: Blender's layers and the precomputed fields (can be deleted; `render_all.sh` makes them again).

Render times at 4K (RTX 4090, i9-13900K, 2026-10-08): Blender stills 36 s, letters 6 min, foreground 9.5 min
(Cycles OptiX, 64 samples, denoised, motion blur); fields 30 s; the 510 frames about 11 min (6 s a frame per worker,
5-6 workers: each needs about 4 GB); ProRes 4K 3.5 min (6.1 GB), preview 2.5 min. About 35 minutes in all. A 1080p
pass (`render_all.sh 1920`) takes about 8 minutes.

To iterate on the 2D look only, rerun `fx_precompute.py` and `composite.py` (Blender's layers are reused); for a few
frames: `composite.py --frames 300-400 --step 10 --out <dir>`.

## Into DaVinci Resolve

Either source works; both carry **straight (unpremultiplied) alpha**.

- **PNG sequence**: Media pool, import the `png_3840` folder (Resolve shows it as one clip when "Show individual
  frames" is off). Clip Attributes: frame rate 60, Alpha mode **Straight**.
- **ProRes 4444 .mov**: import it; in Clip Attributes set Alpha mode to **Straight** if Resolve doesn't already.
- Put it on a track above the footage. Composite mode Normal. The red aura round the blood and the fire's soft edges
  are semi-transparent by design.
- The camera shake is baked into the overlay (subtle: about 14 px at 4K). For a version without it, render the frames
  with `composite.py --shake 0`; the shake curve is `common.shake()` if the footage should shake too.

The grunt, gibs and axe are rendered from the player's own Quake data: the outputs are trailer footage. No id asset or
extracted file is ever written to the repository.
