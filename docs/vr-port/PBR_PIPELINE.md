# Quake VR bump and parallax generation

`Misc/quakevr/pbr_generate.py` reads installed Quake/AD PAKs and loose BSPs, or a loose texture directory, then writes
content-addressed PNG companions. It never changes or exports source diffuse textures. Each PNG contains tangent
normal RGB plus surface-height alpha: 255 is the surface, lower values are deeper.

The matching engine loader hashes the selected diffuse content rather than its texture name and reads
`textures/pbr/<first-32-hex-digits>_norm.png`. The full digest is SHA256 of `qvr-pbr-v1` plus NUL, width and height as
little-endian uint32, then row-major RGBA. Indexed miptex pixels expand through the raw Quake palette with alpha 255,
including index 255; cutout handling is separate. Reused names with different pixels cannot alias. Full digests,
source names/maps, model hashes, runtime versions, validation and failures are retained in the manifest.

## Headless setup

The pinned release is [PBRify_Remix 1.7.2](https://github.com/Kim2091/PBRify_Remix/releases/tag/1.7.2).
Its archive `PBRify_Remix-1.7.2.zip` has SHA256
`d787a88f644c542620f869e0c61e71f795488020fcece9a97e774ca2c2cecb0b` (116606222 bytes).
Models are CC0 according to the [upstream README](https://github.com/Kim2091/PBRify_Remix).

This runner executes the release's same 1x SPAN NormalV3 and Height weights through spandrel/PyTorch. It does not
launch chaiNNer's Electron UI or claim a chaiNNer chain ran. This is equivalent to the chain's 1x inference stages;
periodic context, animation handling, depth bounds and RGBA packing are Quake-specific. The 4x diffuse upscaler is
omitted to preserve originals. Roughness inference is omitted because Quake VR's specular texture is not an
equivalent roughness parameter; an uncalibrated inversion would change materials without a sound conversion.

Create an isolated Python 3.13 environment and install the pinned runtime from the repository root:

```powershell
C:/Python313/python.exe -m venv scratch/env
scratch/env/Scripts/python.exe -m pip install torch==2.8.0 torchvision==0.23.0 --index-url https://download.pytorch.org/whl/cu128
scratch/env/Scripts/python.exe -m pip install spandrel==0.4.1 numpy==2.5.2 Pillow==12.3.0
```

Download the archive from the release page, verify it with `Get-FileHash -Algorithm SHA256`, and extract under
`scratch/pbrify172`. Models belong in `scratch/pbrify172/PBRify_Remix-1.7.2/Models`. No GUI is required.
`--device cpu` also works; the recorded AD run used CUDA on an RTX 4090. ONNX exports are supported through an
installed onnxruntime with explicit `--normal-sha256` and `--height-sha256` for the exported files.

| Model | Verified SHA256 |
| --- | --- |
| `1x-PBRify_NormalV3.pth` | `b0a18270da765f02eaae3c228203bee0677fc28b2854a562515e4aae9c61223b` |
| `1x-PBRify_Height.pth` | `5b973ecb8bae9d96d14d77b8a8f1d88fb6a8580bcc1bb55d2872811acdc4277d` |

The runner refuses a checkpoint mismatch. Different intentional models require their explicit hashes; their
identity changes the cache. Weights, extracted originals and the environment stay in ignored scratch directories.

## Full AD run

This one command writes a companion bundle into `scratch/ad-pbr`, using installed AD and the player's own id1
folder for `gfx/palette.lmp`. Replace the example installation paths. The optional `--authored` folder is read-only.

```powershell
scratch/env/Scripts/python.exe Misc/quakevr/pbr_generate.py generate --game C:/Games/Quake/ad --texture-game quakevr --palette-game C:/Games/Quake/id1 --authored quakevr/textures_quetoo --normal-model scratch/pbrify172/PBRify_Remix-1.7.2/Models/1x-PBRify_NormalV3.pth --height-model scratch/pbrify172/PBRify_Remix-1.7.2/Models/1x-PBRify_Height.pth --output scratch/ad-pbr
```

Use `scan` instead of `generate` to census without loading models. Use `--limit 12 --output scratch/pbr-preview`
for a short sample. `--palette path/to/palette.lmp` provides an explicit 768-byte palette. Within each game folder,
numeric pakN precedence applies followed by loose files. Packed and loose BSP29/BSP2 texture lumps are bounds-checked.
Repeat `--game` for independent mods: each is scanned with its own PAK/loose precedence. The first game/palette-game
with a palette supplies it; prefer an explicit palette if a mod changes it.

The source game's effective packed and loose diffuse replacements are discovered automatically. Repeat
`--texture-game` to list additional game roots in the engine's highest-first order; the source game is appended if
absent. For a complete explicit order, list every root including AD, for example `--texture-game quakevr
--texture-game C:/Games/Quake/ad --texture-game C:/Games/Quake/id1`. Filename lookup first tries
`textures/<map>/<name>`, then `textures/<name>`. Within either filename it tries PNG, TGA and JPG across all game
roots in that order: a map-specific replacement in a lower game wins over a generic higher-game replacement, and
a lower-game PNG wins over a higher-game TGA. Each root retains numeric PAK/loose precedence. Selected images are
hashed with their decoded RGBA, including transparency; their origin and maps are recorded. This includes packed
AD replacements such as `textures/{speedbz_grid1.tga`, rather than assuming all map pixels come from BSP miptex.

For a native Steam installation with HD textures in `id1/textures`, include the lower mission-pack and id1 game
roots as well as AD. Listing only AD and quakevr misses the HD image actually selected by the renderer. A reusable
example from the repository root is:

```powershell
$taskSteamQuake = 'C:/Program Files (x86)/Steam/steamapps/common/Quake'
scratch/env/Scripts/python.exe Misc/quakevr/pbr_generate.py generate --game "$taskSteamQuake/ad" --texture-game quakevr --texture-game "$taskSteamQuake/ad" --texture-game "$taskSteamQuake/rogue" --texture-game "$taskSteamQuake/hipnotic" --texture-game "$taskSteamQuake/id1" --palette-game "$taskSteamQuake/id1" --authored quakevr/textures_quetoo --normal-model scratch/pbrify172/PBRify_Remix-1.7.2/Models/1x-PBRify_NormalV3.pth --height-model scratch/pbrify172/PBRify_Remix-1.7.2/Models/1x-PBRify_Height.pth --output quakevr
```

Match the exact running engine's `path` output when additional game roots are mounted. In the agent kit, insert
`qvr-kit/qbase/rogue`, `qvr-kit/qbase/hipnotic` and `qvr-kit/qbase/id1` immediately after the corresponding Steam
game root. Do not list nonexistent worktree roots printed by an engine configuration. The manifest records the
effective root order so refresh uses the same selected images. `metal1_3` and `wizwood1_5`, for example, are 512x512
loose PNGs in Steam `id1/textures`, whose canonical hashes differ from AD's indexed miptex and Quetoo comparison
images. Run the full corpus with these roots rather than generating exceptions for individual texture names.

## Other PAKs and loose image packs

For another mod change `--game`, the palette and output directory. For loose textures replace `--game` and
`--palette-game` with `--images C:/Materials/MyPack`: no palette is required. PNG, TGA, JPG and JPEG are scanned
recursively. Repeat `--images` for several directories. Companion suffixes `_norm`, `_normal`, `_bump`, `_height`,
`_spec`, `_roughness`, `_metallic`, `_luma`, `_glow` and `_ao` are excluded from diffuse inference. Set `--authored`
to that image folder to preserve its `_norm`/`_bump` channels, including names in subdirectories. Standalone WADs
are not parsed: export their diffuse images first or use BSPs containing their miptex images.

Authored comparison-folder diffuse images also get source hashes, covering external diffuse selection independently
of indexed BSP pixels. This is extra coverage for Quetoo: its pictures normally are compared rather than drawn,
so those hashes may never be used in AD.

## Relief and validation

- Sky, `*` liquids and tool textures (`clip`, `skip`, `hint`, `trigger`, `origin`, `null`, `nodraw`, `black`) are excluded.
- Native source resolution is retained unless an authored channel establishes a larger output size. Authored normal
  vectors and nonflat alpha height are preserved; an authored `_bump` fills missing alpha. Missing counterparts are
  inferred. Green is converted only when required for engine orientation; originals remain unchanged.
- A named authored map with a comparison diffuse must meet the runtime's detail correlation of 0.5: 64x64 detail,
  4x4 samples per cell and a periodic 5x5 high-pass. Aspect ratios must agree. `--authored-match` changes the threshold;
  rejected scores are recorded. With no diffuse comparison, explicit standalone authored components are accepted.
- Generated height has the global `--depth 0.25` bound: alpha at least 191. Authored height retains its own depth.
  There is no per-image contrast stretching. Generated normals are unit vectors in the positive hemisphere.
- Tiles have 48 pixels of periodic source context and only center predictions survive. Default tile size is 256.
  Border differences are measured rather than forcing unlike border texels to match.
- Indexed `{` fences preserve index-255 cutouts. Transparent/emissive colours are filled from periodic nearby valid
  pixels before inference. Generated relief in holes is flat; diffuse transparency and glow are unchanged.
- Generated `+0..+9` and `+a..+j` frames at the same size and matching detail share their first frame's relief to avoid
  pulsing with light colour. Different shapes or detail that correlates better after a scroll retain their own relief.
  Authored components retain precedence. Hashes in incompatible families retain their own inference and are listed
  under `animation_conflicts`. Scrolling or changing geometric animations still deserve visual review.
- `--green auto` aligns green with height rise down image rows, matching the engine; `keep` and `flip` are diagnostic
  overrides. Painted shading can still become inferred relief, as the upstream height model documents.

The manifest records checksums, sizes, authored/generated channels, animation anchors, cutout/emissive fractions,
green correlations and seam deltas. Errors remain explicit and produce a nonzero exit. A checkpoint manifest is
written every 50 sources; writes are not atomic, so rerun after an interrupted write to complete it.

Each generation checkpoint and completed refresh/resume also writes `textures/pbr/provenance.txt`. Its exact ASCII
contract is a `qvr-pbr-provenance-v1` header followed by sorted lines `<source-prefix32> <output-digest64> <flags>`.
The source prefix is the first 32 lowercase hex digits of the source hash. The output digest is SHA256 of
`qvr-pbr-v1` plus NUL, output width and height as little-endian uint32, then decoded row-major output RGBA bytes.
Retain output alpha: it contains height, so replacing it with diffuse alpha 255 would break the binding. Decimal
flags are independently derived from the manifest: bit 1 means generated normal RGB, bit 2 means generated height
alpha, giving values 0 through 3. Authored channels remain unflagged. Missing or stale metadata is treated as
authored by the runtime, protecting authored overrides from generated-only relief scaling.

## Refresh, resume and install

Rerun the same command after adding a PAK or loose BSP. Source content, model hashes, authored component contents,
parameters, runtime and inference-cache version select the cache. Generator identity is recorded for provenance;
discovery-only code changes retain inference pixels. Unchanged inference is reused and validated; changed
sources get new outputs. Removed-source outputs are left harmlessly unused and omitted from the new manifest.
No files are deleted automatically.

When adopting a discovery-only update, the previous bundle's checksum-verified output can migrate into the cache if
its inference configuration, authored channels, animation anchor and material masks still agree. This avoids
rerunning thousands of unchanged textures simply to discover new overrides. Inference or packing changes must bump
the generator's `CACHE_VERSION`; those changes invalidate reuse.

```powershell
scratch/env/Scripts/python.exe Misc/quakevr/pbr_generate.py self-test
scratch/env/Scripts/python.exe Misc/quakevr/pbr_generate_test.py
scratch/env/Scripts/python.exe Misc/quakevr/pbr_generate.py validate --output scratch/ad-pbr
```

To add or rebuild provenance for an existing manifest and PNG bundle without model loading or inference, run
`scratch/env/Scripts/python.exe Misc/quakevr/pbr_generate.py provenance --output scratch/ad-pbr`. This verifies each
PNG's manifest file checksum before decoding it and writes only the index. `validate` also checks the entire index
against the current outputs, dimensions and channel flags. Refresh omits failed or removed sources from the index
while retaining their unused PNGs. Install the index alongside the companion PNGs.

Copy the bundle's `textures/pbr` directory into the chosen game's `textures/pbr`. Keep the census, manifest and
attribution with the bundle for review. `--output quakevr` prepares repository assets directly. Install the matching
content-hash engine loader: older engines cannot discover these names. No diffuse replacements, BSPs, palette or
model weights need shipping.

Source material licences still apply: CC0 weights do not make derivative images CC0. Quetoo-derived companions
retain CC BY-SA 4.0 and the source/authors in `Misc/quakevr/pbr-notices/Quetoo_README.md` and `Quetoo_LICENSE.md`.
Changes to those channels are packaging/resampling where necessary and green convention conversion; missing
channels may be inferred through PBRify. The author's original material folder remains unchanged.
