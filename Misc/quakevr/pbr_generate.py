#!/usr/bin/env python3
"""Content-addressed Quake BSP PBR companions. See docs/vr-port/PBR_PIPELINE.md."""
from __future__ import annotations
import argparse
import collections
import hashlib
import io
import json
import re
import struct
import sys
import time
from pathlib import Path

import numpy as np
from PIL import Image

VERSION = 1
CACHE_VERSION = 2  # Bump when inference/packing semantics change; discovery-only changes retain cached pixels.
NORMAL_SHA256 = "b0a18270da765f02eaae3c228203bee0677fc28b2854a562515e4aae9c61223b"
HEIGHT_SHA256 = "5b973ecb8bae9d96d14d77b8a8f1d88fb6a8580bcc1bb55d2872811acdc4277d"
TAG = b"qvr-pbr-v1\0"
EXCLUDE = {"clip", "skip", "hint", "trigger", "origin", "null", "nodraw", "black"}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def content_hash(rgba):
    h, w = rgba.shape[:2]
    return sha(TAG + struct.pack("<II", w, h) + rgba.tobytes())


def json_write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True), encoding="utf-8")


def entries(folder):
    """PAK precedence numerically, then loose files; retain locations, not whole PAKs."""
    result = {}
    paks = sorted(folder.glob("pak*.pak"), key=lambda p: int(p.stem[3:]))
    for pak in paks:
        with pak.open("rb") as f:
            magic, offset, size = struct.unpack("<4sii", f.read(12))
            if magic != b"PACK" or offset < 12 or size % 64 or offset + size > pak.stat().st_size:
                raise ValueError(f"invalid PAK directory: {pak}")
            f.seek(offset)
            directory = f.read(size)
            for i in range(0, size, 64):
                raw, start, length = struct.unpack_from("<56sii", directory, i)
                name = raw.split(b"\0")[0].decode("latin1").replace("\\", "/").lower()
                if start < 0 or length < 0 or start + length > pak.stat().st_size:
                    raise ValueError(f"invalid PAK entry: {pak}:{name}")
                result[name] = (pak, start, length)
    for p in sorted(folder.rglob("*")):
        if p.is_file() and p.suffix.lower() != ".pak":
            result[p.relative_to(folder).as_posix().lower()] = (p, 0, p.stat().st_size)
    return result


def read_entry(location):
    path, offset, length = location
    with path.open("rb") as f:
        f.seek(offset)
        data = f.read(length)
    if len(data) != length:
        raise ValueError(f"truncated entry: {path}")
    return data


def palette_for(sources, palette_path):
    data = Path(palette_path).read_bytes() if palette_path else None
    if data is None:
        for folder in sources:
            e = entries(folder)
            if "gfx/palette.lmp" in e:
                data = read_entry(e["gfx/palette.lmp"])
                break
    if data is None or len(data) != 768:
        raise ValueError("Quake palette required: --palette palette.lmp or --palette-game id1")
    return np.frombuffer(data, dtype=np.uint8).reshape(256, 3), sha(data)


def bsp_textures(data):
    if len(data) < 124 or struct.unpack_from("<I", data)[0] not in (29, 0x32505342, 0x42535032):
        raise ValueError("unsupported BSP version/header")
    offset, size = struct.unpack_from("<ii", data, 20)
    if not size:
        return
    if offset < 124 or size < 4 or offset + size > len(data):
        raise ValueError("invalid texture lump")
    lump = memoryview(data)[offset:offset + size]
    count = struct.unpack_from("<i", lump)[0]
    if count < 0 or 4 + count * 4 > size:
        raise ValueError("invalid miptex count")
    for i in range(count):
        pos = struct.unpack_from("<i", lump, 4 + i * 4)[0]
        if pos == -1:
            continue
        if pos < 0 or pos + 40 > size:
            raise ValueError("invalid miptex offset")
        name = bytes(lump[pos:pos + 16]).split(b"\0")[0].decode("latin1").lower()
        w, h, pixels = struct.unpack_from("<III", lump, pos + 16)
        if not pixels:
            continue
        if not w or not h or w > 4096 or h > 4096 or pixels < 40 or pos + pixels + w * h > size:
            raise ValueError(f"invalid miptex dimensions: {name}")
        yield name, np.frombuffer(lump[pos + pixels:pos + pixels + w * h], np.uint8).reshape(h, w)


def pack_name(name):
    if name.startswith(("*", "#")):
        name = name[1:]
    if name.startswith("+") and len(name) > 2:
        return name[2:] + "+" + name[1]
    return name


def eligible(name):
    return name not in EXCLUDE and not name.startswith(("*", "sky"))


def find_image(root, names):
    if root is None:
        return None
    for name in names:
        for ext in (".png", ".tga", ".jpg"):
            p = root / (name + ext)
            if p.is_file():
                return p
    return None


def read_image(path, size=None):
    with Image.open(path) as image:
        image = image.convert("RGBA")
        if size and image.size != size:
            image = image.resize(size, Image.Resampling.BILINEAR)
        return np.asarray(image).copy()


def selected_override(mapname, name, indexes):
    """Map filename before global; PNG/TGA/JPG before game-root priority, exactly Image_LoadImage."""
    mapbase = mapname[5:] if mapname.startswith("maps/") else mapname
    mapbase = str(Path(mapbase).with_suffix("")).replace("\\", "/")
    for base in (f"textures/{mapbase}/{name}", f"textures/{name}"):
        for ext in (".png", ".tga", ".jpg"):
            filename = base + ext
            for index in indexes:
                if filename in index:
                    location = index[filename]
                    with Image.open(io.BytesIO(read_entry(location))) as image:
                        rgba = np.asarray(image.convert("RGBA")).copy()
                    path, offset, _ = location
                    origin = str(path) if offset == 0 else f"{path}:{filename}"
                    return rgba, origin
    return None


def scan(args, palette):
    sources, skipped, errors = {}, collections.Counter(), []
    bsps, overrides = 0, 0
    indexed_games = {}
    def index_game(folder):
        resolved = folder.resolve()
        if resolved not in indexed_games:
            indexed_games[resolved] = entries(folder)
        return indexed_games[resolved]
    for folder in args.game:
        e = index_game(folder)
        roots = list(getattr(args, "texture_game", []))
        if folder.resolve() not in {p.resolve() for p in roots}:
            roots.append(folder)
        override_indexes = [index_game(p) for p in roots]
        for mapname, location in sorted(e.items()):
            if not mapname.endswith(".bsp"):
                continue
            bsps += 1
            try:
                for name, indexed in bsp_textures(read_entry(location)):
                    if not eligible(name):
                        skipped[name] += 1
                        continue
                    rgba = np.concatenate((palette[indexed], np.full((*indexed.shape, 1), 255, np.uint8)), axis=2)
                    add_source(sources, rgba, name, mapname, "embedded", None,
                               (indexed == 255) if name.startswith(("{", "#")) else None,
                               (indexed >= 224) & (indexed != 255))
                    selected = selected_override(mapname, name, override_indexes)
                    if selected:
                        replacement, origin = selected
                        add_source(sources, replacement, name, mapname, "selected-override", origin)
                        overrides += 1
                    # Replacement images may be selected independently of embedded diffuse at runtime.
                    if args.authored:
                        base = pack_name(name)
                        p = find_image(args.authored, (base,))
                        if p:
                            add_source(sources, read_image(p), name, mapname, "external", p)
            except Exception as ex:
                errors.append({"map": mapname, "error": str(ex)})
    image_files = 0
    for folder in args.images:
        for path in sorted(folder.rglob("*")):
            if path.suffix.lower() not in (".png", ".tga", ".jpg", ".jpeg"):
                continue
            name = path.relative_to(folder).with_suffix("").as_posix().lower()
            if re.search(r"_(norm|normal|bump|height|spec|roughness|metallic|luma|glow|ao)$", name):
                continue
            if not eligible(path.stem.lower()):
                skipped[name] += 1
                continue
            try:
                add_source(sources, read_image(path), name, name, "loose-image", path)
                image_files += 1
            except Exception as ex:
                errors.append({"image": str(path), "error": str(ex)})
    return sources, {"bsp_files": bsps, "image_files": image_files, "selected_override_occurrences": overrides,
                     "texture_game_priority": [str(p) for p in getattr(args, "texture_game", [])],
                     "excluded_names": dict(skipped), "scan_errors": errors}


def add_source(sources, rgba, name, mapname, kind, path, holes=None, emission=None):
    digest = content_hash(rgba)
    s = sources.setdefault(digest, {"rgba": rgba, "names": set(), "maps": set(), "kinds": set(), "diffuse": set()})
    holes = (rgba[:, :, 3] < 128) if holes is None else holes
    emission = np.zeros(rgba.shape[:2], bool) if emission is None else emission
    s["holes"] = s.get("holes", False) | holes
    s["emission"] = s.get("emission", False) | emission
    s["names"].add(name)
    s["maps"].add(mapname)
    s["kinds"].add(kind)
    if path:
        s["diffuse"].add(str(path))


class Predictor:
    def __init__(self, path, device, threads):
        self.path = path
        self.checksum = sha(path.read_bytes())
        if path.suffix.lower() == ".onnx":
            import onnxruntime as ort
            providers = ["CUDAExecutionProvider", "CPUExecutionProvider"] if device != "cpu" else ["CPUExecutionProvider"]
            options = ort.SessionOptions()
            options.intra_op_num_threads = threads
            self.model = ort.InferenceSession(str(path), sess_options=options, providers=providers)
            self.backend = "onnxruntime"
            self.device = self.model.get_providers()[0]
        else:
            import torch
            from spandrel import ModelLoader
            torch.set_num_threads(threads)
            self.device = "cuda" if device != "cpu" and torch.cuda.is_available() else "cpu"
            self.model = ModelLoader().load_from_file(str(path)).eval().to(self.device)
            self.backend = "torch-spandrel"

    def __call__(self, rgb, padding, tile):
        """Overlap tiles use true periodic source context; only center predictions survive."""
        h, w = rgb.shape[:2]
        result = None
        for y in range(0, h, tile):
            for x in range(0, w, tile):
                ch, cw = min(tile, h - y), min(tile, w - x)
                ys = np.arange(y - padding, y + ch + padding) % h
                xs = np.arange(x - padding, x + cw + padding) % w
                patch = rgb[ys[:, None], xs[None, :], :3].astype(np.float32) / 255
                tensor = patch.transpose(2, 0, 1)[None].copy()
                if self.backend == "onnxruntime":
                    pred = self.model.run(None, {self.model.get_inputs()[0].name: tensor})[0]
                else:
                    import torch
                    with torch.inference_mode():
                        pred = self.model(torch.from_numpy(tensor).to(self.device)).float().cpu().numpy()
                pred = pred[0].transpose(1, 2, 0)
                if pred.shape[:2] != patch.shape[:2]:
                    raise ValueError("PBR model must have 1x output scale")
                if not np.isfinite(pred).all():
                    raise ValueError("nonfinite model output")
                if result is None:
                    result = np.empty((h, w, pred.shape[2]), np.float32)
                result[y:y + ch, x:x + cw] = pred[padding:padding + ch, padding:padding + cw]
        return np.clip(result, 0, 1)


def fill_masked(rgb, mask):
    """Periodic nearest-neighbour expansion excludes cutout/glow colours from inferred relief."""
    result = rgb[:, :, :3].copy()
    valid = ~mask
    if not valid.any():
        result[:] = 128
        return result
    for _ in range(max(mask.shape) * 2):
        if valid.all():
            break
        old_valid = valid.copy()
        old_result = result.copy()
        for axis, shift in ((0, 1), (0, -1), (1, 1), (1, -1)):
            neighbour = np.roll(old_valid, shift, axis)
            take = ~valid & neighbour
            result[take] = np.roll(old_result, shift, axis)[take]
            valid[take] = True
    return result


def normal_green_correlation(normal, height):
    gradient = np.roll(height.astype(float), -1, axis=0) - np.roll(height.astype(float), 1, axis=0)
    green = normal[:, :, 1].astype(float) - 127.5
    denominator = np.sqrt(np.sum(gradient ** 2) * np.sum(green ** 2))
    return float(np.sum(gradient * green) / denominator) if denominator > 1e-6 else 0.0


def normal_bytes(pred, flip_green):
    if pred.shape[2] != 3:
        raise ValueError("normal model must output RGB")
    vectors = pred * 2 - 1
    if flip_green:
        vectors[:, :, 1] *= -1
    # Preserve hemisphere and unit lengths after clamping, for engine XY encoding.
    vectors[:, :, 2] = np.maximum(vectors[:, :, 2], 0.05)
    vectors /= np.maximum(np.linalg.norm(vectors, axis=2, keepdims=True), 1e-6)
    return np.rint((vectors + 1) * 127.5).astype(np.uint8)


def detail_signature(rgba):
    """Exact runtime 64x64, 4x4 sample grid, periodic 5x5 high-pass signature."""
    h,w=rgba.shape[:2]
    ys=np.arange(256)*h//256
    xs=np.arange(256)*w//256
    sampled=rgba[ys[:,None],xs[None,:],:3].astype(np.float64)
    luminance=sampled @ np.array((.299,.587,.114))
    cells=luminance.reshape(64,4,64,4).mean(axis=(1,3))
    blur=np.zeros_like(cells)
    for y in range(-2,3):
        for x in range(-2,3):
            blur+=np.roll(np.roll(cells,y,axis=0),x,axis=1)
    return cells-blur/25


def detail_match(a,b):
    if a.shape[1]*b.shape[0] != a.shape[0]*b.shape[1]:
        return -1.0
    sa,sb=detail_signature(a),detail_signature(b)
    sa-=sa.mean();sb-=sb.mean()
    norm=np.sqrt(np.sum(sa*sa)*np.sum(sb*sb))
    return float(np.sum(sa*sb)/norm) if norm>1e-6 else 0.0


def authored_components(source, root, need=.5):
    if root is None:
        return None, None, {}
    normals, heights, rejected = [], [], {}
    for name in sorted(source["names"]):
        base = pack_name(name)
        n = find_image(root, (base + "_norm",))
        b = find_image(root, (base + "_bump",))
        diffuse = find_image(root, (base,))
        if (n or b) and diffuse:
            score=detail_match(source["rgba"],read_image(diffuse))
            if score<need:
                rejected[name]=round(score,5)
                continue
        if n:
            normals.append(n)
        if b:
            heights.append(b)
    # A hash shared by several texture names must not silently pick conflicting authored maps.
    if len({sha(p.read_bytes()) for p in normals}) > 1 or len({sha(p.read_bytes()) for p in heights}) > 1:
        raise ValueError("same diffuse hash references conflicting authored components")
    normal, height, info = None, None, ({"rejected_match":rejected} if rejected else {})
    if normals:
        p = normals[0]
        image = read_image(p)
        normal = image[:, :, :3]
        info["normal"] = {"path": str(p), "sha256": sha(p.read_bytes())}
        if np.ptp(image[:, :, 3]) > 0:
            height = image[:, :, 3]
            info["height"] = dict(info["normal"], channel="alpha")
    if height is None and heights:
        p = heights[0]
        height = read_image(p)[:, :, 0]
        info["height"] = {"path": str(p), "sha256": sha(p.read_bytes()), "channel": "red"}
    return normal, height, info


def resize_channel(array, size):
    if array.shape[1::-1] == size:
        return array
    return np.asarray(Image.fromarray(array).resize(size, Image.Resampling.BILINEAR))


def calibrated_height(prediction, holes, depth):
    """Use bounded relative relief, rather than the model's arbitrary DC offset."""
    valid = prediction[~holes]
    if valid.size < 16:
        return np.full(prediction.shape, 255, np.uint8)
    low, high = np.percentile(valid, (5, 95))
    span = float(high - low)
    # Nearly constant predictions do not establish relief. Limit amplification of weak signals.
    if span < .02:
        return np.full(prediction.shape, 255, np.uint8)
    gain = min(1 / span, 8)
    recession = np.clip((high - prediction) * gain, 0, 1)
    return np.rint(255 * (1 - depth * recession)).astype(np.uint8)


def validate_image(path, generated_normal, generated_height, depth):
    rgba = read_image(path)
    normal = rgba[:, :, :3].astype(np.float32) / 127.5 - 1
    length = np.linalg.norm(normal, axis=2)
    if generated_normal and (np.max(np.abs(length - 1)) > .02 or normal[:, :, 2].min() < 0):
        raise ValueError("invalid generated tangent normals")
    if generated_height and rgba[:, :, 3].min() < round(255 * (1 - depth)) - 1:
        raise ValueError("height exceeds configured depth bound")
    # Seam delta is reported, not forced to zero: true texture borders need not match each other.
    seam_x = float(np.mean(np.abs(rgba[:, 0].astype(float) - rgba[:, -1].astype(float))))
    seam_y = float(np.mean(np.abs(rgba[0].astype(float) - rgba[-1].astype(float))))
    return {"width": rgba.shape[1], "height": rgba.shape[0], "alpha_min": int(rgba[:, :, 3].min()),
            "alpha_max": int(rgba[:, :, 3].max()), "normal_length_error_max": round(float(np.max(abs(length - 1))), 5),
            "border_delta_x": round(seam_x, 3), "border_delta_y": round(seam_y, 3), "bytes": path.stat().st_size}


def animations(sources):
    families = collections.defaultdict(list)
    for digest, source in sources.items():
        if "selected-override" in source["kinds"]:
            kind = "selected-override"
        elif "external" in source["kinds"]:
            kind = "external"
        else:
            kind = "embedded"
        for name in source["names"]:
            if re.match(r"\+[0-9a-j]", name):
                for mapname in source["maps"]:
                    families[(mapname, name[2:], kind, source["rgba"].shape)].append((name[1], digest))
    anchors = {}
    conflicts = []
    for key, frames in sorted(families.items()):
        anchor = sorted(frames)[0][1]
        for _, digest in frames:
            # Lighting-only frames share relief; moving patterns and different shapes keep their own.
            if digest != anchor:
                a, b = sources[anchor]["rgba"], sources[digest]["rgba"]
                sa, sb = detail_signature(a), detail_signature(b)
                sa -= sa.mean()
                sb -= sb.mean()
                norm = np.sqrt(np.sum(sa * sa) * np.sum(sb * sb))
                same = float(np.sum(sa * sb) / norm) if norm > 1e-6 else 1.0
                moved = same
                if .5 <= same < .999:
                    for shift in range(1, 64):
                        moved = max(moved, float(np.sum(sa * np.roll(sb, shift, 0)) / norm),
                                    float(np.sum(sa * np.roll(sb, shift, 1)) / norm))
                        if moved > same + .01:
                            break
                if same < .5 or moved > same + .01:
                    anchors[digest] = digest
                    continue
            if digest in anchors and anchors[digest] != anchor:
                conflicts.append(digest)
            else:
                anchors[digest] = anchor
    for digest, source in sources.items():
        if digest in anchors and any(not re.match(r"\+[0-9a-j]", n) for n in source["names"]):
            anchors[digest] = digest
    # For ambiguous shared hashes prefer their own image; never overwrite a content key with unrelated relief.
    for digest in conflicts:
        anchors[digest] = digest
    return anchors, sorted(set(conflicts))


def generate(args, sources, census, palette_sha):
    started = time.monotonic()
    for path, expected in ((args.normal_model, args.normal_sha256), (args.height_model, args.height_sha256)):
        if sha(path.read_bytes()) != expected.lower():
            raise ValueError(f"model checksum mismatch: {path}")
    import importlib.metadata
    runtime = {package: importlib.metadata.version(package) for package in ("numpy", "Pillow")}
    for package in ("torch", "spandrel", "onnxruntime"):
        try:
            runtime[package] = importlib.metadata.version(package)
        except importlib.metadata.PackageNotFoundError:
            pass
    models = {"normal": Predictor(args.normal_model, args.device, args.threads),
              "height": Predictor(args.height_model, args.device, args.threads)}
    config = {"version": VERSION, "cache_version": CACHE_VERSION, "runtime": runtime, "generator_sha256": sha(Path(__file__).read_bytes()), "models": {k: {"sha256": p.checksum, "file": p.path.name, "backend": p.backend,
              "device": p.device} for k, p in models.items()}, "padding": args.padding, "tile": args.tile,
              "depth": args.depth, "height_calibration": {"percentiles": [5, 95], "gain_limit": 8, "flat_span": .02}, "green": args.green, "authored_match":args.authored_match, "palette_sha256": palette_sha}
    anchors, conflicts = animations(sources)
    # Source discovery and documentation changes must not rerun unchanged model inference.
    cache_config = dict(config)
    cache_config.pop("generator_sha256")
    cache_config["cache_version"] = CACHE_VERSION
    config_sha = sha(json.dumps(cache_config, sort_keys=True).encode())
    previous = {}
    old_manifest_path = args.output / "pbr-manifest.json"
    if old_manifest_path.is_file():
        old_manifest = json.loads(old_manifest_path.read_text())
        old_config = dict(old_manifest["configuration"])
        old_config.pop("generator_sha256", None)
        old_config.setdefault("cache_version", 1)
        if old_config == cache_config:
            previous = {t["sha256"]: t for t in old_manifest["textures"]}
    manifest = {"schema": VERSION, "hash_contract": "SHA256(qvr-pbr-v1 NUL + uint32LE width,height + RGBA)",
                "configuration": config, "census": census, "animation_conflicts": conflicts, "textures": [], "failures": []}
    completed, reused = 0, 0
    for index, (digest, source) in enumerate(sorted(sources.items())):
        if args.limit and index >= args.limit:
            break
        try:
            authored_normal, authored_height, authored = authored_components(source, args.authored,args.authored_match)
            anchor = anchors.get(digest, digest)
            anchor_source = sources[anchor]
            rgb = fill_masked(anchor_source["rgba"], anchor_source["holes"] | anchor_source["emission"])
            size = authored_normal.shape[1::-1] if authored_normal is not None else authored_height.shape[1::-1] if authored_height is not None else rgb.shape[1::-1]
            identity = sha((config_sha + digest + anchor + json.dumps(authored, sort_keys=True)).encode())
            cache = args.cache / (identity + ".png")
            relative = f"textures/pbr/{digest[:32]}_norm.png"
            destination = args.output / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            generated_normal, generated_height = authored_normal is None, authored_height is None
            # Migrate validated prior output into the stable cache without another inference pass.
            old = previous.get(digest)
            if not cache.is_file() and old and destination.is_file() and old["authored"] == authored and \
                    old["animation_anchor"] == anchor and old["generated_normal"] == generated_normal and \
                    old["generated_height"] == generated_height and \
                    old.get("cutout_fraction", 0) == round(float(np.mean(source["holes"])), 5) and \
                    old.get("emissive_fraction", 0) == round(float(np.mean(source["emission"])), 5) and \
                    sha(destination.read_bytes()) == old["output_sha256"]:
                validate_image(destination, generated_normal, generated_height, args.depth)
                cache.parent.mkdir(parents=True, exist_ok=True)
                cache.write_bytes(destination.read_bytes())
            if cache.is_file():
                validate_image(cache, generated_normal, generated_height, args.depth)
                reused += 1
            else:
                normal = authored_normal if authored_normal is not None else normal_bytes(models["normal"](rgb, args.padding, args.tile), args.green == "flip")
                prediction = models["height"](rgb, args.padding, args.tile).mean(axis=2) if generated_height else None
                height = authored_height if authored_height is not None else calibrated_height(prediction, anchor_source["holes"], args.depth)
                normal = resize_channel(normal, size).copy()
                height = resize_channel(height, size).copy()
                # Keep normal orientation independent of relief calibration (including authored RGB).
                orientation_height = authored_height if authored_height is not None else np.rint(255 * (.75 + .25 * prediction)).astype(np.uint8)
                correlation = normal_green_correlation(normal, resize_channel(orientation_height, size))
                if args.green == "auto" and correlation < -.2:
                    normal[:, :, 1] = 255 - normal[:, :, 1]
                holes = resize_channel(source["holes"].astype(np.uint8), size) > 0
                if generated_normal:
                    normal[holes] = (128, 128, 255)
                if generated_height:
                    height[holes] = 255
                rgba = np.dstack((normal, height))
                cache.parent.mkdir(parents=True, exist_ok=True)
                Image.fromarray(rgba).save(cache, compress_level=6)
                validate_image(cache, generated_normal, generated_height, args.depth)
            destination.write_bytes(cache.read_bytes())
            item = {"sha256": digest, "file": relative, "output_sha256": sha(destination.read_bytes()),
                    "names": sorted(source["names"]), "maps": sorted(source["maps"]), "source_kinds": sorted(source["kinds"]),
                    "source_images": sorted(source["diffuse"]),
                    "authored": authored, "animation_anchor": anchor,
                    "cutout_fraction": round(float(np.mean(source["holes"])), 5),
                    "emissive_fraction": round(float(np.mean(source["emission"])), 5),
                    "generated_normal": generated_normal,
                    "generated_height": generated_height,
                    "authored_normal_green_converted": bool(not generated_normal and not np.array_equal(
                        read_image(destination)[:, :, 1], resize_channel(authored_normal, size)[:, :, 1])),
                    "green_correlation":round(normal_green_correlation(read_image(destination)[:,:,:3],read_image(destination)[:,:,3]),5),
                    "validation": validate_image(destination, generated_normal, generated_height, args.depth)}
            manifest["textures"].append(item)
            completed += 1
        except Exception as ex:
            manifest["failures"].append({"sha256": digest, "names": sorted(source["names"]), "error": str(ex)})
        if (index + 1) % 50 == 0:
            manifest["runtime_seconds"] = round(time.monotonic() - started, 2)
            json_write(args.output / "pbr-manifest.json", manifest)
            if (index + 1) % 250 == 0:
                print(f"{index + 1}/{len(sources)} complete={completed} cached={reused} failed={len(manifest['failures'])}", flush=True)
    manifest["runtime_seconds"] = round(time.monotonic() - started, 2)
    manifest["summary"] = {"eligible_sources": len(sources), "completed": completed, "cached": reused,
                           "failed": len(manifest["failures"]), "bytes": sum(t["validation"]["bytes"] for t in manifest["textures"])}
    json_write(args.output / "pbr-manifest.json", manifest)
    print(json.dumps(manifest["summary"], sort_keys=True))
    return 1 if manifest["failures"] or census["scan_errors"] else 0


def self_test():
    rgba = np.zeros((2, 3, 4), np.uint8)
    rgba[:, :, 3] = 255
    expected = hashlib.sha256(b"qvr-pbr-v1\0" + struct.pack("<II", 3, 2) + rgba.tobytes()).hexdigest()
    assert content_hash(rgba) == expected
    assert content_hash(rgba) != content_hash(rgba[::-1, ::-1].copy() + np.uint8(1))
    assert pack_name("+0button") == "button+0"
    assert not eligible("*water") and not eligible("sky1") and eligible("#fence")
    # Real predictor tiling path exercised with an identity backend (no model required).
    predictor = object.__new__(Predictor)
    predictor.backend = "onnxruntime"
    class Identity:
        def get_inputs(self):
            return [type("Input", (), {"name": "rgb"})()]
        def run(self, _, inputs):
            return [inputs["rgb"]]
    predictor.model = Identity()
    rng = np.random.default_rng(123)
    rgb = rng.integers(0, 256, (35, 49, 3), dtype=np.uint8)
    assert np.allclose(predictor(rgb, 24, 16), rgb / 255, atol=1e-7)
    n = normal_bytes(np.full((5, 7, 3), [0.5, 0.5, 1.0], np.float32), False)
    assert np.all(n[:, :, 2] == 255)
    masked = np.zeros((5, 7), bool)
    masked[2, 3] = True
    filled = fill_masked(np.full((5, 7, 3), 77, np.uint8), masked)
    assert np.all(filled == 77)
    height = np.repeat(np.arange(5, dtype=np.uint8)[:, None] * 20, 7, axis=1)
    normal = np.full((5, 7, 3), 128, np.uint8)
    normal[:, :, 1] = 128 + (np.roll(height.astype(int), -1, 0) - np.roll(height.astype(int), 1, 0))
    assert normal_green_correlation(normal, height) > .9
    data = bytearray(124 + 8 + 40 + 16)
    struct.pack_into("<I", data, 0, 29)
    struct.pack_into("<ii", data, 20, 124, len(data) - 124)
    struct.pack_into("<ii", data, 124, 1, 8)
    data[132:136] = b"test"
    struct.pack_into("<III", data, 148, 4, 4, 40)
    assert list(bsp_textures(data))[0][1].shape == (4, 4)
    struct.pack_into("<I", data, 148, 9000)
    try:
        list(bsp_textures(data))
        raise AssertionError("corrupt miptex accepted")
    except ValueError:
        pass
    print("PBR self-test PASS: canonical hash, eligibility, wrap-padding tiled crop, normal units, BSP bounds")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("mode", choices=("scan", "generate", "validate", "self-test"))
    p.add_argument("--game", type=Path, action="append", default=[])
    p.add_argument("--texture-game", type=Path, action="append", default=[],
                   help="Additional effective texture game roots in highest-first priority; source game is appended if absent")
    p.add_argument("--images", type=Path, action="append", default=[])
    p.add_argument("--palette-game", type=Path, action="append", default=[])
    p.add_argument("--palette", type=Path)
    p.add_argument("--authored", type=Path)
    p.add_argument("--output", type=Path, default=Path("scratch/pbr-bundle"))
    p.add_argument("--cache", type=Path, default=Path("scratch/pbr-cache"))
    p.add_argument("--normal-model", type=Path)
    p.add_argument("--height-model", type=Path)
    p.add_argument("--normal-sha256", default=NORMAL_SHA256)
    p.add_argument("--height-sha256", default=HEIGHT_SHA256)
    p.add_argument("--device", choices=("auto", "cpu", "cuda"), default="auto")
    p.add_argument("--threads", type=int, default=4)
    p.add_argument("--padding", type=int, default=48)
    p.add_argument("--tile", type=int, default=256)
    p.add_argument("--depth", type=float, default=.5)
    p.add_argument("--authored-match", type=float, default=.5)
    p.add_argument("--green", choices=("auto", "keep", "flip"), default="auto")
    p.add_argument("--limit", type=int, default=0)
    a = p.parse_args()
    if a.mode == "self-test":
        self_test()
        return 0
    if not 0 < a.depth <= 1 or a.padding < 0 or a.tile < 16 or a.threads < 1:
        p.error("invalid depth, padding, tile or threads")
    if a.mode == "validate":
        manifest = json.loads((a.output / "pbr-manifest.json").read_text())
        for item in manifest["textures"]:
            path = a.output / item["file"]
            assert sha(path.read_bytes()) == item["output_sha256"], f"modified output: {path}"
            validate_image(path, item["generated_normal"], item["generated_height"], manifest["configuration"]["depth"])
        print(f"PBR bundle validation PASS: {len(manifest['textures'])} checksummed companions")
        return 1 if manifest["failures"] or manifest["census"]["scan_errors"] else 0
    if not a.game and not a.images:
        p.error("--game or --images is required")
    palette, palette_sha = palette_for(a.game + a.palette_game, a.palette) if a.game else (None, None)
    sources, census = scan(a, palette)
    census["eligible_sources"] = len(sources)
    census["eligible_names"] = len({n for s in sources.values() for n in s["names"]})
    census["palette_sha256"] = palette_sha
    json_write(a.output / "pbr-census.json", census)
    print(json.dumps({k: v for k, v in census.items() if k not in ("excluded_names",)}, sort_keys=True), flush=True)
    if a.mode == "generate":
        if not a.normal_model or not a.height_model:
            p.error("--normal-model and --height-model are required")
        return generate(a, sources, census, palette_sha)
    return 1 if census["scan_errors"] else 0


if __name__ == "__main__":
    sys.exit(main())
