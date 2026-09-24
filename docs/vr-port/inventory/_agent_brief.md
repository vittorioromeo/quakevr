# Inventory brief (shared by all inventory agents)

We are porting the **Quake VR** mod engine to **Ironwail**. Quake VR's engine is a heavily modified fork of
Quakespasm-Spiked (QSS). The author converted the whole engine from C to C++ (glm-based `qvec3` instead of
`vec3_t`, templates, lambdas, `std::` algorithms, headers split into many small files, `bool` instead of
`qboolean`, etc.) and interleaved real VR/gameplay changes with that style churn. We want to recover **only the
functional changes** so they can be re-implemented on Ironwail as small, isolated hooks.

## Trees (read-only — do NOT modify any of these)

- **BASE** — pristine QSS commit `36b2046f57`, clang-formatted with the Quake VR style and renamed `.c→.cpp`,
  `.h→.hpp` so it diffs cleanly: `C:\Users\vittorio\AppData\Local\Temp\qvr_up\base\Quake\`
- **QVR** — Quake VR engine as it is now: `C:\OHWorkspace\quakevr\Quake\` (ignore `*.bak` files)
- **IW** — Ironwail v0.8.2 (the port target, plain C): `C:\OHWorkspace\quakevr-iw\Quake\`
- QC game code for reference (to understand what engine features the mod's QuakeC relies on):
  `C:\OHWorkspace\quakevr\QC\` (e.g. `defs.qc`, `vr_*.qc`)

Useful command: `git diff --no-index -w <BASE file> <QVR file>` (from bash). Many QVR headers were split out
of `quakedef.h`/`server.h`/`client.h`/`progs.h` into new small headers (e.g. `edict.hpp`, `entity.hpp`,
`refdef.hpp`, `qcvm.hpp`, `util.hpp`, `quakeglm*.hpp`); follow definitions wherever they moved.

## What to ignore (style churn — do not list)

C→C++ conversion, `vec3_t`→`qvec3`/glm math, `VectorCopy`→assignment, loops→`std::` algorithms, templates or
lambdas that merely restructure identical behaviour, `NULL`→`nullptr`, `qboolean`→`bool`, added braces,
variable declarations moved, header splitting, `const` additions, renames with no behaviour change,
clang-format differences, copyright lines. **Do** briefly note a restructuring if it hides a real behaviour change
(e.g. a template that adds a second think function).

## What to report (functional changes)

For each functional change: new/changed behaviour, new cvars, new commands, new entity fields / globals,
new or changed QC builtins (number + name + signature), protocol/network message changes (new svc/clc, new
U_ bits, changed field encodings), save-game format changes, rendering changes, input changes, bug fixes,
and any upstream feature the author **removed**. Also flag upstream QSS behaviour that QVR still depends on
but that **Ironwail lacks or implements differently**.

## Output

Write ONE markdown file to the path given in your task (create it with the Write tool; that is the only file
you may create). Structure:

```
# <Subsystem> — Quake VR functional changes

## Summary
<5-10 lines: what VR needs from this subsystem, overall size, biggest risks for the Ironwail port>

## Changes
### <short title>
- **Where (QVR):** file:function (line numbers)
- **Upstream (BASE):** what it was
- **Change:** what QVR does now, precisely enough to re-implement (key constants, field names, message layout)
- **Purpose:** why (VR hands, weapons, etc.), if inferable
- **Ironwail:** where the equivalent code lives in IW, and whether it differs from BASE in ways that affect the port
- **Isolation idea:** how to re-implement as a minimal hook (e.g. "call VR_xxx() at end of SV_Physics_Client";
  "new file vr_protocol.c"; "can live entirely in the VR module")
- **Tag:** one of VR-CORE | GAMEPLAY | PROTOCOL | RENDER | UI | BUGFIX | PERF | REMOVED | MISC

## Dependencies on other subsystems
## Open questions
```

Be exhaustive about functional changes but terse in wording. Line numbers matter. Do not paste large code blocks;
short snippets (≤10 lines) only when a layout/constant must be exact. When finished, reply with just the output
path and a 5-line summary.
