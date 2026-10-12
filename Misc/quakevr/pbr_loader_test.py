"""Stage private loader fixtures; retire moves them under scratch (never deletes files).

Run in an agent worktree: python Misc/quakevr/pbr_loader_test.py stage <id1-folder>
Then run the engine with vr_extmaps_dir pointing to scratch/pbrtest_ext, vr_texture_smooth 1,
gl_texturemode GL_NEAREST_MIPMAP_LINEAR,
map e1m1; vr_pbr_test fixtures; vr_pbr_stats all; imagehash; vid_restart; imagehash.
Use 'retire' to restore any production companions after the run. No fixture contains shipped map assets.
"""
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]
TRACK = ROOT / "scratch/pbrtest-files.json"


def png(path, width, height, rgba):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    rows = b"".join(b"\0" + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def pak(path):
    data = path.read_bytes()
    magic, offset, size = struct.unpack_from("<4sII", data)
    assert magic == b"PACK"
    result = {}
    for start in range(offset, offset + size, 64):
        name, pos, length = struct.unpack_from("<56sII", data, start)
        result[name.split(b"\0", 1)[0].decode("latin1")] = data[pos:pos + length]
    return result


def stage(id1):
    assets = {}
    for file in sorted(Path(id1).glob("pak*.pak"), key=lambda p: int(p.stem[3:])):
        assets.update(pak(file))
    palette = assets["gfx/palette.lmp"]
    bsp = assets["maps/e1m1.bsp"]
    offset, length = struct.unpack_from("<II", bsp, 4 + 2 * 8)
    textures = bsp[offset:offset + length]
    count = struct.unpack_from("<I", textures)[0]
    files = []
    backups = []

    def save(path, color):
        file = ROOT / path
        if file.exists():
            assert path.startswith("quakevr/textures/pbr/") and path.endswith("_norm.png"), f"Refusing to overwrite {file}"
            backup = ROOT / "scratch/pbrtest_backup" / path
            backup.parent.mkdir(parents=True, exist_ok=True)
            file.replace(backup)
            backups.append(path)
        file.parent.mkdir(parents=True, exist_ok=True)
        png(file, 4, 4, bytes(color) * 16)
        files.append(path)

    for name, color in [("generated", (128, 160, 250, 191)), ("normal", (140, 150, 245, 255)),
                        ("complete", (145, 155, 240, 213)), ("bump", (221, 221, 221, 255))]:
        save(f"quakevr/textures/pbrtest/{name}.png", color)
    selected = []
    seen = set()
    for i in range(count):
        pos = struct.unpack_from("<i", textures, 4 + i * 4)[0]
        if pos < 0:
            continue
        name, w, h, mip = struct.unpack_from("<16sIII", textures, pos)
        name = name.split(b"\0", 1)[0].decode("latin1")
        if not name or name[0] in "*{#+" or name.startswith("sky"):
            continue
        indices = textures[pos + mip:pos + mip + w * h]
        rgba = bytes(channel for index in indices for channel in (*palette[index * 3:index * 3 + 3], 255))
        digest = hashlib.sha256(b"qvr-pbr-v1\0" + struct.pack("<II", w, h) + rgba).hexdigest()
        if digest in seen:
            continue
        seen.add(digest)
        save(f"quakevr/textures/pbr/{digest[:32]}_norm.png", (128, 160, 250, 191))
        n = len(selected)
        if n == 0:
            save(f"quakevr/textures/e1m1/{name}_norm.png", (140, 150, 245, 255))
        elif n == 1:
            save(f"quakevr/textures/e1m1/{name}_norm.png", (145, 155, 240, 213))
        elif n == 2:
            save(f"quakevr/textures/e1m1/{name}_bump.png", (221, 221, 221, 255))
        elif n >= 4:
            base = name.lower()
            imagepath = ROOT / f"scratch/pbrtest_ext/{base}.png"
            imagepath.parent.mkdir(parents=True, exist_ok=True)
            png(imagepath, w, h, rgba)
            files.append(str(imagepath.relative_to(ROOT)))
            save(f"scratch/pbrtest_ext/{base}_norm.png", (144, 152, 240, 255 if n == 4 else 207))
        selected.append({"texture": name, "hash": digest, "case": n})
        if len(selected) == 6:
            break
    TRACK.parent.mkdir(parents=True, exist_ok=True)
    TRACK.write_text(json.dumps({"files": files, "backups": backups, "textures": selected}, indent=2))
    print("loader fixture textures:", [(t["texture"], t["hash"][:32], t["case"]) for t in selected])


def retire():
    report = json.loads(TRACK.read_text())
    for path in report["files"]:
        src = ROOT / path
        dest = ROOT / "scratch/pbrtest_retired" / path
        dest.parent.mkdir(parents=True, exist_ok=True)
        src.replace(dest) # Overwrite our own prior fixture evidence on repeat verification.
    for path in report.get("backups", []):
        (ROOT / "scratch/pbrtest_backup" / path).replace(ROOT / path)
    print("retired", len(report["files"]), "private fixtures")


def verify():
    report = json.loads(TRACK.read_text())
    hashes = {line.split()[4]: line.split()[0] for line in (ROOT / "quakevr/imagehash.txt").read_text().splitlines()}
    colors = [(140, 150, 245, 191), (145, 155, 240, 213), (128, 160, 250, 221),
              (128, 160, 250, 191), (144, 152, 240, 191), (144, 152, 240, 207)]
    for t in report["textures"]:
        n = t["case"]
        generated = f"textures/pbr/{t['hash'][:32]}_norm"
        normal = f"textures/e1m1/{t['texture']}_norm" if n < 2 else ""
        bump = f"textures/e1m1/{t['texture']}_bump" if n == 2 else ""
        if n == 1:
            name = f"textures/e1m1/{t['texture']}_norm_vrnorm"
        elif n == 5:
            name = f"vrext/{t['texture'].lower()}_norm_vrnorm"
        elif n == 3:
            name = generated + "_vrnorm"
        else:
            identity = generated.encode() + normal.encode() + b"\0" + bump.encode() + b"\0"
            if n == 4:
                identity = generated.encode() + f"vrext/{t['texture'].lower()}_norm".encode() + b"\0external\0"
            name = f"pbrmix/{hashlib.sha256(identity).hexdigest()[:32]}_vrnorm"
        value = 14695981039346656037
        for byte in bytes(colors[n]) * 16:
            value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
        expected = f"{value:016x}"
        assert hashes.get(name) == expected, (t["texture"], name, expected, hashes.get(name))
    print("GPU authored/generated component precedence 6/6 PASS; external flat height fill and complete map preserved")


if __name__ == "__main__":
    if sys.argv[1] == "stage":
        stage(sys.argv[2])
    elif sys.argv[1] == "retire":
        retire()
    elif sys.argv[1] == "verify":
        verify()
    else:
        raise SystemExit("expected stage <id1-folder> or retire")
