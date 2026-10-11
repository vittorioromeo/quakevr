# Toolgun "Brush" tool: design

Quake VR: UNLEASHED (Ironwail). Research and design, then stage 1 built (section 7). Code read at
`C:\OHWorkspace\quakevr-iw-cleanup` (bc3163ed4, branch vr-ironwail); toolgun UX assumes the panel of `feature/toolgun2`
(7baa39660..3513e018a) without depending on its internals. Date 2026-10-11.

What the tool is: in game, the player makes textured convex brushes (boxes first) that behave like the map's own:
solid to players, monsters and props, climbable, lit, shadowed, decal-able, walkable by monsters, saved with the game,
and optionally baked into the map with ericw-tools.

---

## 1. What other engines do (prior art)

| Engine / tool | How runtime brushes work | Ideas to take | Licence |
|---|---|---|---|
| **FTEQW** built-in editor (`engine/gl/gl_heightmap.c` brush section; CSQC UI in `quakec/csaddon/src/editor_brushes.qc`) | Brushes live in the "terrain" subsystem: each brush is a list of planes + per-face texture/alignment (Q3/Valve-220 style), grouped per "brush entity". Faces are generated from planes at edit time, rendered as ordinary batches, collided per brush (planes clipped directly in the trace, BIH-style, no clipnodes), lit with a quick in-engine lightmap pass / dynamic lights. Edits are QC builtins (`brush_create`, `brush_get`, `brush_delete`, `brush_selected`, `brush_getfacepoints`, `brush_calcfacepoints`, `brush_findinvolume`) called by the CSQC editor and forwarded to the server, which rebroadcasts; brushes are saved as `.map`-syntax text so the map can be compiled later. | Plane-set data model; brush text in map syntax; server-authoritative edit commands echoed to clients; per-face texture info; "findinvolume" for selection. | GPL-2.0 (compatible with ours) |
| **Sauerbraten / Cube 2** | Geometry is an octree of deformable cubes (8 corners on a 1/8 grid); every edit is a small message (`editface`, `editmat`, `copy/paste`) applied on every client deterministically. Lighting: `calclight` re-bakes lightmaps (patch area), Tesseract replaced it with fully dynamic deferred lighting. | Edits as tiny deterministic ops broadcast in coop; a coarse grid everywhere; "re-bake only the patch". | zlib |
| **DarkPlaces / QSS** | No runtime brush building of note; QSS implements a subset of FTE's extensions (unverified whether the brush builtins). | — | GPL-2.0 |
| **TrenchBroom** | `Brush` = set of `BrushFace`s (plane from 3 points + texture attributes: standard or Valve 220 axes); the polyhedron (vertices/edges) is derived; vertex/edge/face moves are checked for convexity and rejected if invalid; groups and brush entities are containers. | Faces dragged along their normal to resize; grid snap everywhere; texture lock; clip tool (later); Valve 220 for alignment that survives rotation; validation = "rebuild polyhedron, refuse if degenerate". | GPL-3.0 (ideas only, no code) |
| **Garry's Mod / s&box** | Building with props (PHX plates), weld/nocollide, Precision tool (snap to grid, move by exact amounts). | Snapping to other objects' faces/edges; "nocollide" style toggles; weld = group. | proprietary (ideas) |
| **VR editors** (UE VR Mode, Unity EditorXR, Rec Room Maker Pen, Horizon Worlds, Dreams, Gravity Sketch, Open Brush) | Two-hand grab = translate/rotate/scale; handles on a gizmo; strong snapping with haptic tick; radial/palette menus on the off hand; "draw a box by dragging" from one corner to the other. | Drag-to-create box between trigger press and release; face handles; two-hand scale; haptic snap ticks; grid visible only near the hand. | — |
| **ericw-tools** (qbsp/vis/light, and `bsputil --decompile` in 2.0) | Offline compilers; `light` can be run alone on a BSP (we already do: the in-game relight). `bsputil --decompile` makes a .map from a BSP (brushes from leaves/planes; texture alignment approximated) — usable but not byte-faithful. | Bake path; we already ship `light.exe` under GPL-3 as an aggregate (docs/RELIGHTING.md). | GPL-3.0 |

Sources: github.com/fte-team/fteqw (engine/gl/gl_heightmap.c, quakec/csaddon/src/editor_brushes.qc), fteqw wiki "Brush
editor"; sauerbraten.org (docs/editref.html), github.com/TrenchBroom/TrenchBroom (common/src/mdl/Brush*.cpp, manual
"Brush editing"); github.com/ericwa/ericw-tools (docs: qbsp/light/bsputil); Unreal Engine "VR Mode" docs.
**Caveat:** this table is from prior knowledge of these projects. The web-research pass's report never reached the
author of this document, so none of it was re-read this round. Treat FTE's exact builtin names and its lighting and
networking details, QSS's builtin coverage and `bsputil --decompile`'s quality as unverified until someone checks
the sources above.

The lesson common to FTE and TrenchBroom: **store planes + face attributes; derive everything else (polygons, hulls,
render mesh, physics hulls, ledges) from them, deterministically.** That is what makes save, network and bake trivial.

---

## 2. What our code already gives us (grounding)

(Read directly from the source for this document. Items marked "verify" are inferences that haven't been checked.)

### Models and collision
- **Inline submodels** `*N` are made once at load (`gl_model.c` ~2645-2700: copies of the world `qmodel_t` with
  `hulls[]` pointing into the world's clipnodes/planes). Not extendable at runtime.
- **Synthetic models exist**: `Mod_LoadModel` calls `VR_SyntheticModel(mod)` (gl_model.c:444; ragdoll skinned bodies,
  `vr_ragdoll.cpp:2653`) and `VR_DerivedModelFile(name)` (gl_model.c:446; the taken torch's flame, `vr_walltorch.cpp:402`).
  A brush model can be made the same way under a reserved name, e.g. `*qvrbrush/<id>.<rev>`.
- **Late precache works**: `VR_LatePrecacheModel` (`vr_progs.cpp` ~905) and `PF_precache_model` with
  `VR_AllowLatePrecache`; clients get `QVR_SVC_PRECACHE_MODEL` (`vr_server.cpp:984` → `vr_client.cpp:535`
  `parsePrecacheModel` → `Mod_ForName`). But **late brush models don't draw**: the bmodel VBO, lightmaps and indirect
  buffers are built at map load from `cl.model_precache` (`r_brush.c:446 GL_BuildLightmaps`, `:596
  GL_BuildBModelVertexBuffer`, `:770 GL_BuildBModelMarkBuffers`). TOOLGUN.md's known limit ("brush-model items show
  their ghost only on maps that already have them loaded") is this.
- **Traces**: `SV_HullForEntity` (world.c:143) takes `hulls[0/1/2]` of any `mod_brush` model for a SOLID_BSP +
  MOVETYPE_PUSH edict. A model with **its own private `planes`/`clipnodes` arrays** works unchanged; for one convex
  brush each hull is a chain of N clipnodes (front = empty, back = next plane, last back = solid), hull 1/2 being the
  planes pushed out by the box (`dist += dot(|n|, halfsize)`-style support offset) **plus axial and edge bevels**
  (Quake 2's bevels; without them hulls 1/2 of a non-box brush stick out at corners).
- **vr_hull** (`vr_hull.hpp`): already rebuilds every brush model's solid as convex brushes with Q2 bevels and sweeps
  arbitrary boxes against them (`clipBSP`, method 0), or compiles a clipping hull "grown by the box" (method 1). Its
  brush/bevel code is the thing to reuse for hulls 1/2 and for the narrowed player box against our brushes (its
  per-model cache is keyed by model; a new model name per brush revision fits).
- **Monsters**: `SV_CheckBottom` (sv_move.c:59) tries `SV_PointContents` (world only) then falls back to
  `SV_Move(..., nomonsters=true)` traces, which do clip SOLID_BSP entities. So monsters **can stand and walk on** a
  SOLID_BSP brush, as they do on plats (they won't path-find around it any better than around a crate: Quake AI is
  local steering). `SV_PointContents` ignores entities (a brush never makes "solid" contents for point tests).

### Physics (Box3D)
- Any SOLID_BSP edict with a `mod_brush` model is a **Kind::Mover** (`vr_box3d.cpp` ~2441): a kinematic body built
  from `moverHulls(model)` (`vr_box3d.cpp:1745`): hull 0's solid leaves → `b3HullData` each. So our brush needs only a
  valid hull 0 (a node/clipnode tree with one solid leaf) and props/ragdolls collide with it for free. A brush that
  never moves could later become a static body; not needed.
- `vr_convex.hpp` `convex::hull(points)` makes a Box3D hull from points if we prefer building it directly.

### Rendering and light
- Brush draw is GPU-driven over load-time buffers. The QVR **liquid mesh** (`r_world.c:776 R_DrawLiquidMesh`) is the
  model to copy: its own vertex/index buffer drawn with the **world program** through `R_AddBModelCall` with the
  world's instance. A "runtime brush mesh" draw (`R_DrawRuntimeBrushes`) built the same way gets the world shader's
  everything: lightmap sampling, clustered **dynamic lights** with shadows, map-light shadows, deluxemaps, detail
  textures, parallax, fog, and the decals' modulate pass on top.
- Lighting (docs/vr-port/LIGHTING.md): dynamic lights on world surfaces add to the lightmap value in the world
  shader; casters for dlight shadows include "doors, lifts and platforms (glvert_t positions from Ironwail's brush
  vertex buffer)"; map-light shadows of moving things have a per-frame "moving things" depth map. Our brushes must be
  registered as casters in both (the brush mesh buffer is enough).
- Light grid: maps compiled with `-lightgrid` (ours, and relit ones) carry one; models sample it. Usable as the
  ambient/fallback light for brush faces.
- `vr_relight*`: runs ericw `light.exe` in the background on a copy of the map and reloads "where you are"
  (`save "autosave/relight"` then `load`, vr_relight.cpp:2904/3743). This is the bake pipeline's skeleton.

### Decals, ledges
- Decals (`vr_decals.cpp:777 clipToWorld`): footprints clipped against **the world model's** node tree faces only
  ("none on doors or lifts"). The walk takes a `qmodel_t*`: generalise to `clipToModel(model, origin)` and include our
  brushes (static ones in world space; moving ones: drop decals on move, or store owner-relative).
- Ledges (`vr_ledges.cpp:758 of(model)`): built lazily per brush model **by name**, in model space, from its faces +
  hull 0; only `fromThisMap()` gates it to world/`*N`. A brush model with real faces/hull 0 gets ledges by widening
  that gate; a new revision = new name = fresh map, the old one evicted. Climb (`vr_climb.cpp`) already checks other
  models' solids when a hold is taken, so a brush against a wall works.

### Toolgun, saves
- Toolgun2: own panel (tiles with icons, per-tool choices, thumbnails grid with tabs), stick-press tool cycling,
  physgun with wrist turn and angle snap (`vr_toolgun_snap`), ghost previews, red "remove" glow (glow 2..3 in
  `vr_glsl.h`), Spawn tool places entities "as a map would" (server-side edicts).
- Saves: Quake text saves plus QVR hooks (`VR_OnBeginLoadGame`, `VR_OnLoadGame`, `VR_AddonOnSave` writes a
  `<save>.addon` sidecar: precedent for a `<save>.brushes` sidecar). Toolgun state (frozen props, joints) isn't saved
  today (TOOLGUN.md known limits).

---

## 3. Design

### 3.1 Data model (the source of truth)

```
struct BrushFace {           // one plane of a convex brush
    dvec4 plane;             // normal (unit) + dist, world space, normal pointing out; double for exact snaps
    char  texture[16];       // miptex name, looked up in the world's textures (fallback: a QVR dev texture)
    // Valve 220 alignment (survives rotation, maps 1:1 to .map):
    vec4 uAxis, vAxis;       // axis xyz + offset
    float rotation, scaleU, scaleV;
    uint8 flags;             // later: nodraw/skip, fullbright, liquid?
};
struct Brush {
    uint32 id;               // stable across saves (per map)
    uint32 rev;              // bumped on each edit (model name = *qvrbrush/<id>.<rev>)
    uint32 group;            // 0: alone; else a group (moved/deleted/baked together, one entity)
    vector<BrushFace> faces; // 4..64 planes, convex, validated
    uint8 kind;              // solid (default), detail (not in bake's vis), nonsolid (illusionary), clip
};
```

- **Derived, never stored**: polygons per face (plane clipping of a big box, epsilon-welded), vertices, bounds,
  render mesh, hulls 0/1/2, Box3D hulls, ledges, decal faces, lightmap.
- **Text form = .map brush syntax** (Valve 220), one block per brush plus a header line:
  ```
  // qvrbrush id 17 rev 3 group 0 kind solid
  {
  ( x y z ) ( x y z ) ( x y z ) TEXNAME [ ux uy uz uo ] [ vx vy vz vo ] rot sx sy
  ...
  }
  ```
  Three points per plane are generated from the plane (on the polygon, integer-snapped when the brush is on grid).
  This one format is the save format, the network payload (compressed), the clipboard for duplicate, and the bake input.
- Validation on every edit: polyhedron rebuilt; refuse if fewer than 4 faces with area, volume < 1 unit³, a dimension
  < 1 unit, a face with 0 area (dropped, not refused), coordinates outside the world bounds, > 64 faces.

### 3.2 Runtime representation: one synthetic brush model per brush (stage 1), per group later

The brush is an **edict** (server-authoritative), classname `func_qvr_brush`, SOLID_BSP, MOVETYPE_PUSH, origin at the
brush's centre (model space = brush space minus origin, so moving = changing origin only, no rebuild), `model` =
`*qvrbrush/<id>.<rev>`, plus a string field `vr_brush` holding the brush text (for save/networking; or the sidecar,
3.6).

The model is made by a `VR_SyntheticModel` branch (`vr_brushes.cpp`), filling a `qmodel_t` of type `mod_brush`
directly (no BSP file round-trip):

| Part | How |
|---|---|
| planes | the faces' planes (model space) + bevels for hulls 1/2 |
| hull 0 (`mnode_t`/`mleaf_t`) | a chain of N nodes, one per face plane: front child = one shared empty leaf, back = next node; last back = a solid leaf. Leaves have no marksurfaces, `visofs` -1. Hull 0 clipnodes (for `SV_HullForEntity` hull 0 and `moverHulls`) built from the same chain (as `Mod_MakeHull0` does). |
| hulls 1/2 | own `mclipnode_t` chains from expanded planes + bevels (port vr_hull's brush-expansion/bevel code; 1 brush ⇒ ≤ ~30 nodes). `clip_mins/maxs` Quake's. |
| surfaces | one `msurface_t` per face polygon, `texinfo` from the Valve axes (vecs[0], vecs[1] = axis/scale + offset), texture pointer = the world's `texture_t` of that name (shared gltextures), `extents`/`texturemins` computed as Quake does (for lightmaps). |
| render buffers | appended to a QVR **runtime brush mesh** (3.3), not Ironwail's bmodel VBO |
| bounds | mins/maxs, rmins/rmaxs (radius) |

Everything downstream keys off `qmodel_t*`/name: `SV_HullForEntity` and `SV_ClipMoveToEntity`, `vr_hull::clipBSP`
(narrow player box), Box3D `moverHulls`, `ledges::of`, decals' model walk. Each needs only "accept synthetic brush
models" (a predicate `VR_IsRuntimeBrush(model)`) and "forget model X" on rebuild/delete:
- `moverHulls` cache entry erased; the Mover body rebuilt when the edict's model changes (Box3D already rebuilds when
  an edict's model changes? verify; else an explicit `box3d::modelChanged(ent)`).
- `ledges` cache entry erased (by name); `fromThisMap` widened.
- vr_hull's per-model brushes rebuilt (single convex: microseconds).
- Old `qmodel_t` slots: `mod_known` has 4096 slots; revisions recycle a free-list of slots (the empty-slot path at
  gl_model.c:339 exists) so edit-drag doesn't exhaust them. Server model precache (`MAX_MODELS`, 4096 in Ironwail's
  protocol?) has the same issue: **the precache name should be per brush, not per revision** (`*qvrbrush/<id>`), with
  the revision carried in the brush data and the model rebuilt in place (same `qmodel_t*`, contents replaced, caches
  invalidated by a generation counter). Recommended over name-per-revision.

Groups (stage 2+): a group = one entity, one model with several convex brushes: hull 0 becomes a small BSP over the
brushes (or a list-of-brushes trace like vr_hull method 0: no tree needed, test each brush's bounds); hulls 1/2 the
same with expanded brushes. Simplest: keep 1 brush = 1 entity until perf says otherwise (edicts are cheap; see risks).

**During a drag** the brush is a **ghost** (toolgun's ghost path: translucent mesh, no collision); the real model is
rebuilt on release (commit). So per-frame rebuilds never happen.

### 3.3 Rendering

`R_DrawRuntimeBrushes`, modelled on `R_DrawLiquidMesh`: one dynamic VBO/IBO (`glvert_t`-compatible, the world's vertex
format: position, texture st, lightmap st, centre/swell 0) holding all runtime brushes' faces (world space for static
brushes; for moved ones the model matrix comes from the entity instance as for doors via `R_InitBModelInstance`).
Calls go through `R_AddBModelCall` with the entity's instance so the world program shades them exactly like brush
entities: lightmap, dlights (+shadows, angle term), deluxe, detail, fog, the red glow for remove, the decal pass.
Rebuild only the edited brush's range (sub-buffer update, fixed slots of 64 faces × 32 verts).

Alternative considered: making Ironwail's load-time bmodel buffers growable (late brush models in general, fixing
TOOLGUN.md's limit too). More invasive (indirect cmds per texture, lightmap atlas, marksurf buffers); worth doing later
if late-precached `b_*.bsp` items matter; not required for brushes.

Shadows: add the runtime brush mesh as a caster to (a) dlight shadow faces (the "doors, lifts" depth-only shader takes
`glvert_t` positions: point it at our buffer too) and (b) the map-light "moving things" depth map (`vr_shadow_maplights`)
so a placed box darkens the baked light behind it from nearby map lights. That makes new brushes sit in the scene
instead of floating, without any bake.

### 3.4 Lighting strategies (cheapest first)

1. **Stage 1: one lightmap per face, flat, from the light grid / lightpoint.** Each face's lightmap filled with the
   light grid value at its centre (or `R_LightPoint`-style sample on the floor under it where no grid), directional
   with the grid/deluxe direction when present. Dynamic lights and shadows add on top in the shader. Costs nothing,
   looks acceptable indoors, wrong in light/shadow transitions.
2. **Stage 2: quick in-engine bake per brush (≈ "mini light").** For each luxel (16 units, as Quake), sum the map's
   `light` entities (from the entity lump: origin, `light`, `wait`, `delay`, `_color`, spotlights) with Quake/ericw's
   formula `(light − dist × wait) × angle term` — the same estimate vr_lighting's map-light shadows already use — with
   a visibility trace (hull 0 of world + other brushes), plus minlight + a light-grid ambient term for bounce. A 128³
   box ≈ 400 luxels × ~20 lights ≈ 8k traces: a few ms on the job pool, done after commit, faded in. Relit maps' texture
   lights (lamps, lava) aren't entities: take them from the relight's light list if present (`vr_relight_lights`), else
   the grid ambient covers them roughly.
3. **Bake (3.7)**: the real thing: ericw `light` with bounce/dirt, and the brush shadows into the world's lightmap.

No lightmap at all (fullbright/dlight-only) is a debug option only: Quake's look depends on baked light.

### 3.5 Editing in VR (toolgun "Brush" tool tile)

Panel (toolgun2): Brush tile → choices: **Create / Edit / Texture / Duplicate / Delete**, grid size (1, 2, 4, 8, 16, 32,
64; default 8; units, shown in metres too), snap to world faces on/off, texture lock on/off, kind (solid / detail /
non-solid).

- **Create (box by drag)**: trigger down on a surface sets corner A (snapped to grid, on the hit face's plane); while
  held, the hand's beam point is corner B projected onto the plane → a rectangle; then raising/lowering the hand (or
  the gun's stick forward/back) extrudes along the face normal → box. Release commits. Live dimensions as 3D text
  (`vr_worldtext`). Default height if released flat: one grid step. Haptic tick on each snap step.
- **Edit**: aimed brush shows handles (as the Scale tool does: cyan face handles, white corner handles; arrow handles
  on faces). Face handle drag = move that plane along its normal (resize, snapped; stage 2 for non-box: the plane moves,
  the polyhedron is rebuilt, refused if degenerate). Body grab (trigger on the body) = move, snapped to grid (offset
  snapping: the brush's min corner on grid); the physgun-style wrist rotation with `vr_toolgun_snap` (15°) steps; A/X
  held + sticks = fine nudge one grid step per flick. Two-handed (off hand grips the brush too): scale about centre.
- **Texture**: panel grid of the map's own textures (`cl.worldmodel->textures`, thumbnails like toolgun2's spawn grid,
  tabs: "this map", "QVR dev", "recent"); trigger on a face applies to that face, grip-trigger to the whole brush;
  "pick from world" eyedropper (next shot copies the texture and alignment of the face hit, as TrenchBroom's
  alt-click). Alignment ops on the panel: fit, shift ±, rotate 90, scale ×½ ×2, world-aligned.
- **Duplicate**: copy the aimed brush's text; ghost follows the beam; trigger places (snapped).
- **Delete**: the Remove tool already does this (removing the edict removes the brush; `func_qvr_brush` hooks cleanup).
- Undo: a per-session ring of brush texts (create/delete/edit as text diffs). Cheap given the text model; worth it.
- Safety: refuse placing a brush that would enclose a player or a monster (box test against SOLID_SLIDEBOX/BBOX ents;
  or push them out with `vr_unstick`); a prop inside gets pushed out by Box3D anyway.

### 3.6 Persistence

- **In saves**: a `<save>.brushes` sidecar (precedent: `<save>.addon`) holding the map name + BSP hash and all brushes'
  text; written in the save hook, read in `VR_OnBeginLoadGame` **before** the edicts are parsed, so the synthetic models
  exist when `func_qvr_brush` edicts with `model "*qvrbrush/17"` are restored (precache list rebuilt in order:
  the sidecar registers the precaches first). The edicts carry only id/origin; the geometry lives in the sidecar.
  Alternative: the brush text in a string field of the edict (Quake saves strings fine; > 1 KB values hit
  `ED_ParseEdict` limits? verify) — sidecar is safer and also the bake input.
- **Across sessions without a save** ("my build on e1m1"): `quakevr/brushes/<map>.brushes`, same format, loaded on map
  spawn (option, off by default; like FTE's per-map brush file). Hash of the BSP recorded: a mismatching map warns.
- Format versioned (`// qvrbrushes 1`), plain text, diffable, hand-editable.

### 3.7 Baking (stage 4)

Two bakes, both reusing the relight machinery (background process, progress, reload where you are via
`autosave/...`):

**A. Light-only bake (works on every map, no .map needed). Recommended first.** Write a copy of the BSP with each
brush appended as a new inline submodel (faces, texinfo, edges, vertexes, nodes/leafs chains, clipnodes for hulls 1/2,
models lump entry) and an entity `{ "classname" "func_qvr_brush" "model" "*N" "_shadow" "1" "_qvr_brush" "17" }` per
brush; run `light.exe` with the relight's settings; read back the lit BSP (+ .lit/.lux). On reload the brushes are
ordinary inline bmodels: Ironwail's own draw path, real lightmaps with occlusion, shadows baked into the world's
lightmap (`_shadow 1`), ledges/Box3D/decals via the `*N` paths. The runtime brush entities in the restored save are
replaced by the baked `*N` ones (matched by `_qvr_brush` id). Editing a baked brush "un-bakes" it back to a runtime one
(its lightmap stays approximate until the next bake). No vis change (they're bmodels), no world hull merge (they're
entities): exactly as a func_wall. Blockers: writing BSP lumps correctly (BSP29 and BSP2: our maps are BSP2), the
relit-override naming (`VR_ModelFile` relit lookup must prefer the baked copy: name it as the relit map with a
"brushes" variant hash), and keeping submodel numbering stable (append only after the map's own submodels, so the
map's `*1..*K` keep their numbers and existing saves still match).

**B. Full compile (qbsp + vis + light) for maps with a .map source** (our generated maps: vrstart, vrtutorial,
vrtrailer; user TrenchBroom maps). Insert the brush blocks into worldspawn (or `func_detail` for kind detail, so they
don't split vis), run qbsp/vis/light with MAPPING.md's profiles, reload. Brushes become real world geometry (vis,
world hull, world decals). Blockers: we don't ship qbsp (would need the GPL-3 aggregate treatment as light.exe);
compile time (vrstart fast preset ~1.5 min, final ~8 min); **submodel renumbering**: worldspawn brushes don't change
`*N`, but any brush entity added before others would; append new entities last. For id maps: `bsputil --decompile`
then B is possible but the decompile is approximate (texture alignment, leaks, different face splits, vis changes):
not recommended for id maps; A covers them.

### 3.8 Networking / coop (later)

Server-authoritative: the toolgun sends `vr_brush_*` commands (create/edit/delete with the brush text) to the server;
the server validates, updates the edict and broadcasts a `QVR_SVC_BRUSH` sub-command (id, rev, compressed text) before
the late precache, so every client builds the identical model (deterministic derivation, doubles). Joining clients get
all brushes in the signon. Edicts move as normal entities. Single player first; TOOLGUN.md says the toolgun is single
player today.

---

## 4. Staged plan

| Stage | Scope | Effort |
|---|---|---|
| **1. Boxes** | `vr_brushes.cpp`: brush data/text, box-only polyhedron; synthetic `mod_brush` (hull 0 chain, hulls 1/2 = expanded box planes, surfaces/texinfo, world textures); `func_qvr_brush` edict (QC spawn fn + engine builtin, late precache); `R_DrawRuntimeBrushes` with the world program; flat lightmap from the light grid; dlight + map-light shadow casters; Box3D Mover (verify model change path); ledges gate; decals on static brushes (`clipToModel`); toolgun Brush tile: create by drag, move with grid snap, delete, texture picker grid; mock-headset test script (place box, walk on it, shoot decals, climb, drop a crate on it, grunt walks over it, `vr_box3d` hull count). | 6-9 days |
| **2. Convex + faces** | Arbitrary plane sets, face-handle resize, rotation with snap (Valve axes, texture lock), bevels for hulls 1/2 (port vr_hull's), duplicate, undo ring, per-face textures and alignment ops, quick in-engine luxel bake, vr_hull narrow-box support verified, groups (optional). | 6-8 days |
| **3. Persistence** | `<save>.brushes` sidecar, per-map brush files, versioned format, load ordering with precaches, tests: save/load/death reload/changelevel back. Also fixes nothing else in the toolgun's saving (frozen props/joints): separate. | 2-3 days |
| **4a. Light-only bake** | BSP29/BSP2 writer appending submodels + entities; relight pipeline call; baked-copy naming/override; reload-where-you-are with runtime→baked swap. | 5-7 days |
| **4b. Full compile** | .map insertion for maps with sources, qbsp shipping/licence, profiles, swap. | 3-4 days (+ packaging) |
| **5. Coop** | svc sub-command, signon, validation. | 2-3 days |

---

## 5. Risks and limits

- **PVS / vis**: runtime brushes don't occlude (no vis change) and are drawn when their leaves are visible, like doors.
  Fine for dozens; a fort of hundreds costs draw calls (mitigate: one mesh, one call per texture per frame, as planned).
- **Many brushes**: each is an edict (MAX_EDICTS budget; `vr_debris_edicts_left`-style guard), a Box3D kinematic body
  (cheap when still: consider static bodies for never-moved brushes), an SV_Move candidate (area node linking keeps
  this local). Cap at e.g. 256 brushes a map at first; groups when needed.
- **Monster AI**: walks on brushes (CheckBottom traces) but `SV_PointContents` ignores them and Quake steering doesn't
  plan; monsters may get stuck behind built walls exactly as behind crates. Portal/stealth AI (PORTAL_AI.md) uses the
  map's own data: won't know brushes. Sight: brushes block sight only if `visible()`'s trace hits them (it does for
  SOLID_BSP via SV_Move with nomonsters) — verify `sightblocked` path.
- **Players stuck**: a brush made over a player/monster; refuse or push out.
- **Hull 1/2 bevels** wrong ⇒ player snags on corners of rotated brushes (box-only stage 1 avoids it).
- **Precision**: double planes, snap to integer grid; derived polygons welded with an epsilon; same results on every
  machine for coop.
- **Lighting mismatch** with relit/bounced maps in stages 1-2 until baked; shadows from map lights only via the
  moving-things map near the player.
- **Decals on moving brushes** dropped when moved (stage 1); owner-relative decals later.
- **Bake**: BSP writer correctness (test by loading baked maps headless in the mock and comparing hulls/traces);
  relit-map file naming; save compatibility after bake (runtime→baked swap by id); qbsp not shipped.
- **Toolgun2 coupling**: only the panel's tile/choices API and the ghost/handle drawing are used; the brush system
  (`vr_brushes.cpp`) has no toolgun dependency (console commands `vr_brush_box x y z X Y Z tex`, `vr_brush_list`,
  `vr_brush_delete id`, `vr_brush_save/load` for tests and scripts).

## 6. Things to verify before stage 1 (checked 2026-10-11, worktree `brushtool`)

1. **Box3D, model changes:** a Mover's body is made again only when the edict's `qmodel_t*` changes (`stale`:
   `s.model != modelOf(ent)`), and `moverHulls` caches per pointer. A model rebuilt in place needed more: the slot keeps
   the model's revision (`brushes::revision`), `stale` compares it, and `moverHulls` drops a cache entry of an older
   revision (its hulls retired to a list freed with the world: a body made from them may live a frame). Removing a
   Mover wakes what rests on it (a crate on a removed box falls).
2. **vr_hull:** its brushes are cached by model index (`modelSub`) and keyed by the hull 0 clipnodes pointer: a model
   rebuilt in place would leave a stale (dangling) entry. Runtime brushes bypass it: `clipBSP` meets their convex planes
   directly (`brushes::clipBox`, Quake 2's box against planes + axial bevels, Quake's 1/32 back-off), `subOf` refuses
   them.
3. **R_AddBModelCall:** its `index` is a command in `gl_bmodel_indirect_buffer`, its instance the entity's (matrix
   from origin/angles: moved brushes are fine). Rather than a draw of our own (`R_DrawRuntimeBrushes` in 3.3), stage 1
   reserves slots at the end of Ironwail's own brush buffers at `R_NewMap` and points the model's
   `firstcmd`/`texofs`/`vbo_firstvert` into its slot: the ordinary bmodel path draws it (depth pre-pass, OIT, glow,
   decals' flag), and every system reading `vbo_firstvert` (the dlights' and map lights' casters) works unchanged.
4. **Sight:** QC's `visible()` (`point_visible` → `VR_Stealth_Line` → traceline nomonsters) hits SOLID_BSP entities:
   a box wall hides the player (tested: no enemy behind it, one without it). Note the classic quirk: a trace whose
   `passedict` is the world skips every entity the world owns (owner 0), brush entities included: `crates::traceLine`
   (crate placement) and any QC `traceline(..., world)` go through runtime brushes as through a `func_wall`.
5. **Edict field length:** `ED_ParseEdict` reads values with `COM_Parse` (`com_token[1024]`): a brush's text (six
   faces ≈ 600 bytes, a 16-face brush over 1 KB) would not fit safely. The sidecar (3.6) it is.
6. **MAX_MODELS:** 4096 (`MAX_MOD_KNOWN` 4096 too; the network's `U_MODEL2` carries 16 bits). With one precache name
   per brush *id* (ids reused) the slots used are bounded by the most brushes alive at once (`vr_brush_max` 256).

## 7. Stage 1 as built (2026-10-11, `agent/brushtool` on `feature/toolgun2`)

Files: `Quake/vr/vr_brushes.cpp/.hpp` (the brush, its text, the registry, the model, the slots, commands),
`vr_toolgun_brush.cpp/.hpp` (the tool), `r_brush.c` (the reserve: `R_RuntimeBrushReserve/Upload/Lightmap`),
`gl_model.c` (`VR_RuntimeBrushModel` in `Mod_LoadModel`), hooks in `vr_box3d.cpp`, `vr_hull.cpp`, `vr_ledges.cpp`,
`vr_decals.cpp`, `vr_lighting.cpp`, `vr_audiosim.cpp`, `vr_modelcollide.cpp`, `vr_builtins.cpp` (edict freed),
`vr_progs.cpp` (world reset), `r_world.c` (decals' flag). Test: `Misc/quakevr/brush_test.py`. ROUND22.md has the
section ("Runtime brushes: the toolgun's Brush tool, stage 1").

Differences from the plan above:
- **Model name** `*qvrbrush/<id>` (the `*` makes it a submodel to every system that asks: `modelmeta`; worldtrace's
  brush entities, so hands and held weapons stop at a box), built in place on each edit (no revision in the name).
  Code that reads a submodel's number from its name (`atoi(name + 1)`) must skip runtime brushes (`vr_audiosim`
  did; `vr_ao` checks `n > 0`).
- **Model space** = world minus the entity's origin (the brush's bounds' middle, whole units); a move rebuilds (the
  texture stays world-aligned). The physgun refuses boxes (the tool moves them on the grid).
- **Rendering**: no `R_DrawRuntimeBrushes`: the reserve in Ironwail's buffers (6.3). Each slot: 64 corners, 192
  indices, 8 texture commands, 16 lightmap texels (one a face: flat light, `R_LightPoint` 4 units out from the face's
  middle, shaded by the face's way: `vr_brush_shade`); a new map's reserve is `vr_brush_max` at its load. Stage 2's
  luxel bake needs real lightmap blocks per face (a bigger reserve with a small allocator).
- **Lux** (deluxemaps): the reserve's directions "straight on" (the bump term flat).
- **Decals**: laid on brush faces like the world's, drawn by the world shader's decal grid (the brush instances flagged
  as the world's), each mark tagged with the brush it lies on; a move, resize or removal drops the marks laid on the
  brush (stage 1: no owner-relative marks), the world's beside it stay; a paint (only the textures changed) keeps them,
  and keeps the brush's revision: its Box3D hulls, ledge map and held weapons' triangles stay as they are.
- **Edits elsewhere**: a box's edit drops only its own ledge map (a hold on the world's or another box's keeps its
  depth and slide data), and Box3D frees the box's old hulls once its mover body is made again.
- **No QC spawn function** for `func_qvr_brush` yet (the engine makes the entity); a map or bake placing one needs it
  (and the FGD entry).
- **Saves**: brushes are not saved; a save's brush entities are removed after its load (said once) and their model
  names load as empty models. Stage 3's sidecar: write `toText` of each brush with its id in the save hook, read it in
  `VR_OnBeginLoadGame` and `create` them with their ids before the edicts are parsed (or re-link the parsed entities to
  the brushes by model name: `*qvrbrush/<id>`).

Left for later stages (from stage 1's list): the per-face texture choice and alignment ops (the data has them; the tool
paints one face or all), duplicate across maps/clipboard text, undo, rotation, non-box shapes in the tool (the model
maker takes any convex set already: the self-test's wedge and prism), the luxel bake, saving, coop.

## 8. Stages 2-5 as built (2026-10-11, `agent/brush2` on `next`)

The request: "expand the brush tool to also support other shapes, baked lighting, saving, and full bake. Keep
multiplayer-specific features as a TODO for now." Four milestones, each a commit; `Misc/quakevr/brush_test.py` grew
with each (ROUND22.md has the numbers).

**Shapes and rotation (plan's stage 2).**
- `brushes::shape(Shape, lo, hi, up, rise, sides)`: box, wedge (a ramp: its slope from the low end's bottom edge to the
  high end's top edge), cylinder and pyramid (3..12 sides round the up axis, the polygon stretched to the box). The
  tool's Create makes the panel's shape (Shape row; Sides row) in the box dragged, its up the surface's normal, a
  wedge rising away from the gun.
- `rotate(brush, turn, pivot, lockTexture)`: planes turned about a pivot in doubles (`turnFromAngles`: Quake's
  AngleVectors, quarter turns exact: a box turned 90 degrees is axial again); with texture lock the Valve axes turn
  with the faces and the offsets keep the texture where it lay; without it each face gets the world's alignment.
  The tool's **Rotate** mode (value 6: the archived `vr_toolgun_brush_mode` numbers kept; the panel shows it after
  Resize): the trigger held on a brush turns it as the gun's hand turns since the grab, each angle in
  `vr_toolgun_snap` steps (15 if 0), about its bounds' middle on whole units; the ghost is the turned brush; let go,
  it is made. Panel: Tex Lock / Tex Free (`vr_toolgun_brush_texlock` 1).
- `scaleTo(brush, lo, hi, lo2, hi2)`: any shape stretched by its bounds (each plane through the affine map); Resize's
  six handles now stretch any shape (a box: its face's plane moves, as before); `vr_brush_set` likewise.
- **Bevels** (`bevels()` in `build`): the axial planes of the bounds and Quake 2's edge bevels (each non-axial edge
  crossed with each axis, kept when the whole brush is behind it). Hulls 1 and 2 are the faces and bevels pushed out
  by the boxes (the brush grown by the box, exactly: no snag off a turned box's corner or a wedge's edge); the narrow
  player box (`clipBox`, vr_hull_width) uses the same set (`Built::clipPlanes`).
- **Undo** (`vr_brush_undo`; the panel's Undo): the last 64 creates, changes and removals of this map (a change
  undone is the brush as it was; a removal made again with its id if free).
- `vr_brush_align <id> <face|all> <reset|shift du dv|scale su sv|turn deg>`: a face's alignment by hand. The tool's
  per-face painting is Paint: One Face, as stage 1. A slot has 96 corners now (16 faces have at most 84).

**Baked lighting in the game (plan's 3.4, strategy 2: `vr_brushlight.cpp`).**
- Each slot owns a 64 x 32 rectangle of the lightmap atlas (`QVR_BRUSH_SLOT_LMW/LMH`, one `AllocBlock` a slot at the
  map's load; `R_RuntimeBrushLightmap` writes the whole rectangle). A brush's faces get Quake-style lightmaps there
  (luxels `vr_brush_luxel` 16 texture units apart on the face's texinfo grid, doubled until they all fit; shelf
  packed), up to four styles side by side (the world shader's `lmofs`), the vertices' lightmap coordinates and styles
  written by `upload`.
- At once: each face one colour, the world's light under a point 4 units off its middle (`R_LightPoint`) **with every
  lightstyle at its average** (`cl_lightstyle[].average`), shaded (`vr_brush_shade`). Then the bake, `vr_brush_bake_ms`
  (3) a frame on the main thread: each luxel lit from the map's light entities as id's and ericw's light compute them
  (`light`/`_light`, `wait`, `delay` 0/1/2/3/5, `_color`, spotlights by `mangle` or `target` with `angle` (40) and
  `_softangle`, `_anglescale` 0.5; a light behind the face's plane skipped), each traced to the luxel through the
  world's hull 0 (solid and sky block, liquids don't) and the other brushes (`brushes::blocks`: their shadows); the
  sun (`_sunlight`, `_sun_mangle`, `_sunlight_color`) when the way to it meets the sky first; the minimum (`light`,
  `_minlight`); light.exe's rangescale (a value of 256 is a byte of 128). **Each light goes to its own lightstyle's
  map** (style 0 first, four at most a face, as Quake's), so a brush in a flickering room flickers with the room and a
  switched light's share comes and goes with the switch. What the map's own light has that its lights don't explain
  (bounce, the sky's dome, glowing textures, a relit map's extras) is added to style 0: the world's light under the
  face's middle less the same lights' sum at that floor spot (`vr_brush_bake_fill` 1). Luxels past a polygon's edge
  are lit at the edge; a luxel in the world's solid is moved toward the face's middle (light.exe's way). Light
  directions (deluxemaps) are the lights' weighted sum in the world shader's frame.
- A brush made, changed or removed rebakes the brushes within `vr_brush_bake_reach` (512) of it (its shadow on them).
  A paint keeps the light; an alignment change lays it out again. Results for an older layout are dropped (a serial).
- **The open question** ("should a box's light sample ignore animated lightstyles: a box made in a flickering room
  keeps one frame's brightness"): resolved both ways. The bake puts the flickering light in its style's map, so the
  box flickers as the walls do; the flat light before the bake (and with `vr_brush_bake 0`) reads every style at its
  average, never one frame's.
- `vr_brush_light <id>` (each face's luxels, spacing, styles, the first style's least/mean/most and the others' means),
  `vr_brush_lights [x y z | styled]` (the lights the bake reads; near a point the nearest five and whether the sun is
  seen), `vr_brush_rebake [id|all]`, `vr_brush_bake_report`.

**Saving (plan's stage 3; no sidecar after all).**
- A saved game keeps its brushes in the save itself: `// qvrb ` comment lines after its model list (COM_Parse skips
  comments; an older build loads the save without them, as before). `VR_ReadSaveInfo` reads them; after the edicts
  and models are restored (`VR_OnLoadGame`, after `rebindLoadedModels`) each brush is made again with its id on its
  own entity (the one whose model is `*qvrbrush/<id>`; its model name was bound by the load already). The `.sav` and
  its brushes can't part (a sidecar could be lost when saves are moved or copied).
- Brush files: `vr_brush_save [name]` writes `<game folder>/brushes/<map>[-name].brushes` (`// qvrbrushes 1 map <map>
  bsp <hash of the map's planes>` and the brushes' .map text); `vr_brush_load [name | @othermap] [add]` makes them
  (the map's removed first unless "add"; another map's or another version's file warns and loads);
  `vr_brush_autoload 1` gives a map started afresh (not a saved game) its file's brushes. Undo doesn't take back a
  load. `allToText`/`fromAllText` are the format.
- Before the client has the map (a save's brushes, made as the server loads), the brushes' capacity is what the
  client's load will reserve (`R_RuntimeBrushSlotsFor`); uploads wait for the client to be in the game (its
  lightstyles: the flat light and the bake read them).

**Full bake (plan's 3.7 A: `vr_brushbake.cpp`, through vr_relight).**
- `vr_brush_bake_map` (the panel's Bake Map, Debug > Boxes) relights the map in play with its brushes in it: the
  relight's input `.bsp` gets each brush appended as an inline submodel after the map's own (faces, planes, texinfo,
  vertices, edges; hull 0 a chain of nodes over one new empty leaf, hulls 1 and 2 chains of clipnodes: a valid model
  nobody uses; BSP29, BSP2 and 2PSB; per-face BSPX lumps of the input dropped, light makes its own) and an entity each
  for light (`func_qvr_brushbake`, its model, `_shadow 1`). light lights their faces with everything the relight's
  look has (bounce, dirt, soft shadows) and their shadows fall on the world's lightmaps. The result's entity lump is
  the map's own plus a comment line a brush (`// qvr_brushbake *<model> <hash> <faces>`); the result goes where the
  relight puts it (relit_custom) and the map reloads where you are (the save keeps the brushes).
- In that world a brush whose hash (its planes, alignment and textures, rounded: a save's text round trip keeps it)
  and face count match takes its faces' lightmaps from the submodel's faces (light.exe's samples and styles, every
  other luxel kept until they fit its rectangle; its .lux turned into the shader's frame): `vr_brush_light` says
  "relit". A brush changed since is baked in the game as before; the world keeps the old shape's shadows until the
  next bake or a plain relight (`vr_relight_revert` takes the bake out). The map's submodel numbers are kept, so its
  saves still load.
- `vr_brush_bake_write <name> [fake <level>]` (tests): the bake's input without light, written as a playable map
  (the submodels as func_walls, the notes in, each face lit `level`): the BSP writer and the read-back checked
  headless without light.exe.

**Plan's 4b (full compile: qbsp + vis + light, brushes as real world geometry)** is not built: qbsp isn't shipped.
The brush files are .map text a mapper can paste into TrenchBroom for that.

**Coop / multiplayer (plan's stage 5): TODO.** The brushes are the listen server's and the toolgun is single-player.
What's needed (3.8): the edits as server commands (`vr_brush_*` with the brush text) validated and applied by the
server, a `QVR_SVC_BRUSH` sub-command (id, revision, compressed text) sent before the late precache so every client
makes the identical model (`build` is deterministic in doubles); joining clients get every brush in the signon (the
`// qvrb` text is the payload); the client-side light (flat, bake) runs on each client; the full bake stays the
host's (its relit file would have to be sent). Undo per player. `brushes::blocks` and the bake read the server's registry:
a remote client needs its own copy of the brushes (made from the svc).

**Limits now.** 16 faces a brush; 4 lightstyles a face (a fifth style's light is left out); the in-game bake has no
bounce of its own (the fill term borrows the map's) and no shadows from doors or other bmodels; brushes don't cast
shadows on the world's baked light until the full bake (dynamic and moving-things shadows only); a huge brush's
luxels are spaced wider (its rectangle is 64 x 32 texels).
