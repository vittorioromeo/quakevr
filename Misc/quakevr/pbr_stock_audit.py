"""Audit stock/mission-pack relief against generated companions, without copying id images.

Python dependencies and hash conventions are shared with pbr_generate.py. Re-run when installing
replacement packs. All BSP miptex variants and MDL single/group skins are scanned. Runtime sweep
logs, rather than offline similarity guesses, establish authored external material precedence.
"""
import argparse
import collections
import io
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image
import pbr_generate as pbr


def engine_entries(folder):
    # COM_AddGameDirectory: contiguous pak0..N, each PAK above loose files (the generator uses the same lookup).
    return pbr.entries(folder)


def mdl_skins(data):
    if len(data) < 84 or data[:4] != b"IDPO" or struct.unpack_from("<i", data, 4)[0] != 6:
        raise ValueError("unsupported MDL")
    count, w, h = struct.unpack_from("<iii", data, 48)
    if min(count, w, h) < 1 or max(w, h) > 4096:
        raise ValueError("bad MDL skin dimensions")
    pos = 84
    for skin in range(count):
        group = struct.unpack_from("<i", data, pos)[0]
        pos += 4
        frames = 1
        if group:
            frames = struct.unpack_from("<i", data, pos)[0]
            if frames < 1 or frames > 4096:
                raise ValueError("bad MDL skin group")
            pos += 4 + 4 * frames
        for frame in range(frames):
            if pos + w * h > len(data):
                raise ValueError("truncated MDL skin")
            yield f"skin{skin}.{frame}" if group else f"skin{skin}", np.frombuffer(data[pos:pos + w * h], np.uint8).reshape(h, w)
            pos += w * h


def effective_indexes(roots, indexes):
    merged = {}
    for root in reversed(roots):
        merged.update(indexes[root])
    return merged


def find_image(names, indexes):
    for name in names:
        for ext in (".png", ".tga", ".jpg"):
            for index in indexes:
                location = index.get(name + ext)
                if location:
                    rgba = np.asarray(Image.open(io.BytesIO(pbr.read_entry(location))).convert("RGBA")).copy()
                    return rgba, str(location[0]), name
    return None


def scan(args):
    stock = collections.defaultdict(list)
    for text in args.stock:
        campaign, folder = text.split("=", 1)
        if campaign not in ("id1", "hipnotic", "rogue"):
            raise ValueError("stock campaign must be id1, hipnotic or rogue")
        stock[campaign].append(Path(folder).resolve())
    roots = list(dict.fromkeys([Path(p).resolve() for p in args.texture_game] +
                               [p for folders in stock.values() for p in folders]))
    indexes = {p: engine_entries(p) for p in roots}
    palette, _ = pbr.palette_for(stock["id1"], None)
    bundle = Path(args.bundle)
    provenance = {line.split()[0]: int(line.split()[2]) for line in
                  (bundle / "textures/pbr/provenance.txt").read_text().splitlines()[1:]}
    records, protected, errors = [], set(), []
    # Native VR mounts optional mission-pack resources in every stock campaign; only start maps are isolated.
    order = {campaign: ["rogue", "hipnotic", "id1"] for campaign in stock}
    for campaign, own_roots in stock.items():
        # Inventory campaign-owned effective assets only; dependencies are counted in their own campaign.
        assets = effective_indexes(own_roots, indexes)
        lookup_roots = list(dict.fromkeys([Path(p).resolve() for p in args.texture_game] +
                            [p for c in order[campaign] for p in stock[c]]))
        lookup = [indexes[p] for p in lookup_roots]
        for asset, location in sorted(assets.items()):
            kind = "bsp" if asset.endswith(".bsp") else "mdl" if asset.endswith(".mdl") else None
            if kind is None:
                continue
            try:
                data = pbr.read_entry(location)
                textures = pbr.bsp_textures(data) if kind == "bsp" else mdl_skins(data)
                for name, pixels in textures:
                    rgba = np.concatenate((palette[pixels], np.full((*pixels.shape, 1), 255, np.uint8)), axis=2)
                    variants = [("embedded", rgba, str(location[0]), None)]
                    # Brush assets under progs use the same mapname derivation as gl_model.c (name + 5).
                    mapbase = asset[5:].rsplit(".", 1)[0]
                    image_bases = [f"textures/{mapbase}/{name}", f"textures/{name}"] if kind == "bsp" else [
                        asset + "_" + name.removeprefix("skin").replace(".", "_")]
                    replacement = find_image(image_bases, lookup)
                    if replacement:
                        variants.append(("selected-override", *replacement))
                    for variant, image, origin, base in variants:
                        digest = pbr.content_hash(image)
                        if kind == "bsp":
                            protected.add(digest[:32]) # Alias skins are excluded in code, regardless of pixels.
                        companion = bundle / f"textures/pbr/{digest[:32]}_norm.png"
                        flags = provenance.get(digest[:32], 0)
                        file_normal = find_image([p + "_norm" for p in ([base] if base else image_bases)], lookup)
                        file_bump = find_image([p + "_bump" for p in ([base] if base else image_bases)], lookup)
                        normal = bool(file_normal)
                        height = bool(file_normal and np.any(file_normal[0][:, :, 3] != 255)) or bool(file_bump)
                        records.append({"campaign": campaign, "asset": asset, "texture": name, "kind": kind,
                            "variant": variant, "source": origin, "hash": digest,
                            "width": image.shape[1], "height": image.shape[0], "eligible_world": kind == "bsp" and pbr.eligible(name),
                            "companion": companion.exists(), "generated_flags": flags,
                            "file_normal": file_normal[2] if file_normal else None,
                            "file_height": file_normal[2] if file_normal and np.any(file_normal[0][:, :, 3] != 255) else file_bump[2] if file_bump else None,
                            "external_authored_resolution": "GPU sweep; exact matching/animation fallback is decided by the engine",
                            "prior_class": "alias_skin_excluded" if kind == "mdl" else
                                "complete_authored" if normal and height else "authored_normal_missing_height" if normal else
                                "authored_bump" if height else "procedural_world_or_brush",
                            "selected": variant == "selected-override" or not replacement,
                            "animated": name.startswith("+")})
            except Exception as exc:
                errors.append({"asset": asset, "source": str(location[0]), "error": str(exc)})
    collisions = [r for r in records if r["companion"] and r["generated_flags"] and r["eligible_world"]]
    summary = {"records": len(records), "protected_hashes": len(protected), "errors": errors,
               "companion_overlap_hashes": len({r["hash"] for r in records if r["companion"] and r["eligible_world"]}),
               "baked_authored_overlap_hashes": len({r["hash"] for r in records if r["companion"] and not r["generated_flags"] and r["eligible_world"]}),
               "collision_hashes": len({r["hash"] for r in collisions}), "collision_occurrences": len(collisions),
               "collision_by_campaign": dict(collections.Counter(r["campaign"] for r in collisions)),
               "collision_by_prior_class": dict(collections.Counter(r["prior_class"] for r in collisions)),
               "collision_by_variant": dict(collections.Counter(r["variant"] for r in collisions)),
               "by_campaign": {c: {"bsp_assets": len({r["asset"] for r in records if r["campaign"] == c and r["kind"] == "bsp"}),
                                   "mdl_assets": len({r["asset"] for r in records if r["campaign"] == c and r["kind"] == "mdl"}),
                                   "records": sum(r["campaign"] == c for r in records)} for c in stock},
               "alias_collision_hashes": len({r["hash"] for r in records if r["kind"] == "mdl" and r["companion"]})}
    report = {"format": "qvr-stock-pbr-audit-v1", "summary": summary, "records": records}
    Path(args.report).write_text(json.dumps(report, indent=2) + "\n")
    Path(args.protected).write_text("qvr-pbr-stock-v1\n" + "".join(h + "\n" for h in sorted(protected)), newline="\n")
    print(json.dumps(summary, separators=(",", ":")))
    if errors:
        raise SystemExit("Incomplete scan: resolve every error before shipping stock protection")


def verify_logs(paths):
    import re
    rows = collections.defaultdict(dict)
    pattern = re.compile(r"pbr audit: ([01]) (\S+) (\S+) (\S+) (\d+) (\d+) ([0-9a-f]{64}) (\S+) ([\d.]+) ([\d.]+)(?: ([0-9a-f]{64}) ([0-9a-f]{64})(?: ([01]))?)?")
    for path in paths:
        for mode, world, owner, texture, w, h, digest, source, ns, hs, rgb, alpha, authored in pattern.findall(Path(path).read_text()):
            rows[(Path(path).stem + ":" + world, owner, texture)][mode] = (w, h, digest, source, ns, hs, rgb, alpha, authored)
    missing = [key for key, v in rows.items() if set(v) != {"0", "1"}]
    changed = [{"key": key, "baseline": v["0"], "pbr": v["1"]} for key, v in rows.items() if set(v) == {"0", "1"} and v["0"] != v["1"]]
    def material_class(key, value):
        if value[3].startswith("vrext/"):
            return "external_authored"
        authored = value[8] == "1" if value[8] else value[3].endswith(("_norm", "_bump"))
        # Early audit logs inferred authored from a changed pointer. Stock animation reuse can instead
        # select frame 0's procedural normal; direct file normals always have a _norm/_bump suffix.
        if ".bsp" in key[1] and value[3].startswith("textures/") and not value[3].endswith(("_norm", "_bump")):
            authored = False
        if value[3].endswith(".bsp"):
            authored = False # Ammo-frame reuse can select another brush model's procedural map.
        if authored:
            return "file_authored_alias" if ".mdl" in key[1] else "file_authored_world_or_brush"
        return "procedural_alias" if ".mdl" in key[1] else "procedural_world_or_brush"
    classes = collections.Counter(material_class(key, v["0"]) for key, v in rows.items() if "0" in v)
    changed_classes = collections.Counter(material_class(c["key"], c["baseline"]) for c in changed)
    result = {"maps": len({k[0] for k in rows}), "textures": len(rows), "missing_pairs": len(missing),
              "classes": dict(classes), "changed_classes": dict(changed_classes),
              "changed_normal_rgb": sum(c["baseline"][6] != c["pbr"][6] for c in changed),
              "changed_height_alpha": sum(c["baseline"][7] != c["pbr"][7] for c in changed), "changed": changed}
    Path(paths[0]).with_suffix(".verified.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({**result, "changed": len(changed)}))
    if not rows or missing or changed:
        raise SystemExit("Stock runtime relief changed or census incomplete")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stock", action="append", default=[], help="campaign=game-root; repeat highest first per campaign")
    parser.add_argument("--texture-game", action="append", default=[], help="additional game roots, highest first")
    parser.add_argument("--bundle", default="quakevr")
    parser.add_argument("--report", default="docs/vr-port/pbr_stock_audit.json")
    parser.add_argument("--protected", default="quakevr/textures/pbr/stock.txt")
    parser.add_argument("--verify-log", action="append")
    args = parser.parse_args()
    verify_logs(args.verify_log) if args.verify_log else scan(args)
