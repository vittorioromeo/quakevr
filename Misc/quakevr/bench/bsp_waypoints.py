"""Camera waypoints for a benchmark tour of a map (BENCHMARKS.md, "Tours").

    python Misc/quakevr/bench/bsp_waypoints.py <map.bsp> [count]

Reads the BSP's entity lump and prints `count` (6) waypoints spread over the map: the origins of its pickups (item_*,
weapon_*: they lie on the floor, in the open, where a player goes), every n-th in the file's order (so the same map
always gives the same tour), raised 24 units (a standing player's origin over the floor); the spawn point first. The
result is pasted into scenarios.py (TOURS), so a tour never depends on parsing at run time.
"""
import re
import struct
import sys


def entities(path):
    with open(path, "rb") as f:
        data = f.read()
    # BSP29 / BSP2 / 2PSB: a 4-byte version, then 15 lumps of (offset, length); the entities are lump 0.
    offset, length = struct.unpack_from("<ii", data, 4)
    text = data[offset:offset + length].decode("latin-1")
    for block in re.findall(r"\{([^{}]*)\}", text):
        yield dict(re.findall(r'"([^"]*)"\s*"([^"]*)"', block))


def waypoints(path, count=6):
    ents = list(entities(path))
    spawn = next((e for e in ents if e.get("classname") == "info_player_start"), None)
    picks = [e for e in ents if re.match(r"(item_|weapon_)", e.get("classname", "")) and "origin" in e]
    out = []
    if spawn and "origin" in spawn:
        out.append((spawn["origin"], float(spawn.get("angle", 0))))
    step = max(1, len(picks) // max(1, count - len(out)))
    for e in picks[::step]:
        if len(out) >= count:
            break
        out.append((e["origin"], float(e.get("angle", 0))))
    result = []
    for origin, yaw in out:
        x, y, z = (float(v) for v in origin.split()[:3])
        result.append((round(x), round(y), round(z) + 24, round(yaw)))
    return result


if __name__ == "__main__":
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 6
    print(waypoints(sys.argv[1], n))
