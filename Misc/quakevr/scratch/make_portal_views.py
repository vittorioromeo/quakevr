"""Generate three visible gates leading to rooms with one, two and three columns.

Requires the author's local Quake texture WAD; the generated map/BSP are test
assets, not redistributed. Example:
  python Misc/quakevr/scratch/make_portal_views.py build-cmake/slipgate-review/quakevr/maps/portalviews.map --wad C:/TrenchBroom/QUAKE101.WAD
Compile with qbsp, vis and light. See TESTING.md for camera and live-limit checks.
"""
import argparse
from pathlib import Path


def box(x0, y0, z0, x1, y1, z1, texture):
    planes = [
        ((x0,y0,z0), (x0,y0+1,z0), (x0,y0,z0+1)),
        ((x0,y0,z0), (x0,y0,z0+1), (x0+1,y0,z0)),
        ((x0,y0,z0), (x0+1,y0,z0), (x0,y0+1,z0)),
        ((x1,y1,z1), (x1,y1+1,z1), (x1+1,y1,z1)),
        ((x1,y1,z1), (x1+1,y1,z1), (x1,y1,z1+1)),
        ((x1,y1,z1), (x1,y1,z1+1), (x1,y1+1,z1)),
    ]
    axes = ["0 -1 0", "1 0 0", "-1 0 0", "1 0 0", "-1 0 0", "0 1 0"]
    vertical = ["0 0 -1", "0 0 -1", "0 -1 0", "0 -1 0", "0 0 -1", "0 0 -1"]
    faces = []
    for points, u, v in zip(planes, axes, vertical):
        coords = " ".join("( %d %d %d )" % point for point in points)
        faces.append(f"{coords} {texture} [ {u} 0 ] [ {v} 0 ] 0 1 1")
    return "{\n" + "\n".join(faces) + "\n}"


def entity(keys, brushes=()):
    return "{\n" + "".join(f'"{k}" "{v}"\n' for k, v in keys.items()) + "\n".join(brushes) + "\n}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--wad", type=Path, required=True)
    args = parser.parse_args()
    if not args.wad.is_file():
        parser.error("local texture WAD not found")
    brushes, entities = [], []
    wall = "brick1_1"
    def room(x0, y0, x1, y1):
        brushes.extend([
            box(x0-16,y0-16,-16,x1+16,y1+16,0,wall),
            box(x0-16,y0-16,256,x1+16,y1+16,272,wall),
            box(x0-16,y0,0,x0,y1,256,wall),
            box(x1,y0,0,x1+16,y1,256,wall),
            box(x0-16,y0-16,0,x1+16,y0,256,wall),
            box(x0-16,y1,0,x1+16,y1+16,256,wall),
        ])
    room(0, 0, 768, 384)
    entities.append(entity({"classname":"info_player_start", "origin":"384 48 24", "angle":"90"}))
    for index, x in enumerate([256,384,512]):
        brushes.append(box(x-48,296,16,x+48,304,144,"*teleport"))
        # Cover the other liquid-brush faces; only the front belongs to the gate.
        brushes.extend([
            box(x-64,280,0,x-48,320,160,wall),
            box(x+48,280,0,x+64,320,160,wall),
            box(x-48,280,0,x+48,320,16,wall),
            box(x-48,280,144,x+48,320,160,wall),
            box(x-64,304,0,x+64,320,160,wall),
        ])
        entities.append(entity({"classname":"trigger_teleport", "target":f"exit{index}"},
                               [box(x-48,280,16,x+48,320,144,"trigger")]))
        destination = 1200 + index * 1200
        room(destination-384, 384, destination+384, 1024)
        entities.append(entity({"classname":"info_teleport_destination", "targetname":f"exit{index}",
                               "origin":f"{destination} 650 24", "angle":"90"}))
        for column in range(index+1):
            cx = destination + (column * 2 - index) * 36
            brushes.append(box(cx-12,820,0,cx+12,844,112,wall))
        entities.append(entity({"classname":"light", "origin":f"{destination} 740 190", "light":"400"}))
    entities.append(entity({"classname":"light", "origin":"384 160 190", "light":"400"}))
    world = entity({"classname":"worldspawn", "wad":args.wad.resolve().as_posix(),
                    "message":"Three portal view regression", "_minlight":"60"}, brushes)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(world + "\n" + "\n".join(entities) + "\n", encoding="ascii")


if __name__ == "__main__":
    main()
