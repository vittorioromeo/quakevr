# Two-handed torch crash

Fixed on `vr-ironwail`: `portals::splitBounds` now selects the server QuakeC VM while checking portal triggers, then restores the previously selected VM.

Two Windows Application Error records from 5 October (18:14:03 and 18:14:29) reported access violation `0xc0000005` at executable offset `0x5bdd4a`. Resolving that address against the installed executable/PDB identified `vr_portals.cpp:2084`, the portal-trigger `EDICT_NUM` check inside `splitBounds`.

The same crash was reproduced with mock hands on `start`, immediately after the second torch grab. The recorded stack was:

```
portals::splitBounds
box3d::boxInLevel
box3d::holdClear
VR_RelinkHeld
CL_ReadFromServer
```

Two-handed prop placement runs both on the server and during client entity relinking. The client call has no selected QuakeC VM. `EDICT_NUM` and `triggerActive` require the server's entity storage, strings and time; using the ambient null VM crashed as soon as the query encountered a portal entry. The firing-range tests did not reproduce it because that map had no portal entries to check. Any two-handed prop using this clearance path could be affected, even far from a portal.

The fix follows existing portal client-query code: push `sv.qcvm` after the active-world checks, perform the bounds query and pop the previous VM before returning. Bounds tests, portal activation rules and clearance behavior remain unchanged.

Validation:

- Release build passed.
- The exact crashing acquisition script now completes.
- `Misc/quakevr/torch_twohand_test.py` passed `start`, `e1m1` and `vrfiringrange`: three two-hand acquisitions and two hand transfers per map, with movement while held and final release.
- Prop-query regression passed mixed props, portals, hand transfers and ragdolls. Its vanilla fixture passed with `-nomapindex`; the initial vanilla run completed its collision checks but crashed during shutdown in the unrelated background map-index download (`libcurl.dll` → `Download` → `mapindex::run`). That downloader issue is not changed by this torch fix.
- Raw reproduction stack, scripts and logs are local artifacts under `build-cmake/torch-twohand-20261005/`; the stack is saved as `reproduced-crash.txt`.

The helper uses a disposable benchmark base and broad second-hand reach to isolate acquisition from finger/model settings. It still exercises the actual QC carry logic and server/client physics clearance paths.
