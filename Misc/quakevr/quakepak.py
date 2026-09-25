# quakepak.py -- reading Quake's .pak files, for the scripts here (make_swords.py, relight_maps.py).

import os
import struct


def read_pak(path):
    """{name (lower case): bytes} of every file in a .pak ({} if it is not one)."""
    with open(path, "rb") as f:
        data = f.read()
    ident, offset, length = struct.unpack_from("<4sii", data, 0)
    if ident != b"PACK":
        return {}
    files = {}
    for i in range(length // 64):
        raw, pos, size = struct.unpack_from("<56sii", data, offset + i * 64)
        files[raw.split(b"\0")[0].decode("latin-1").lower()] = data[pos : pos + size]
    return files


def game_paks(game_dir):
    """The paths of a game folder's pak<N>.pak files, in the engine's order (later ones win)."""
    paks = []
    for entry in os.listdir(game_dir):
        base, ext = os.path.splitext(entry.lower())
        if ext == ".pak" and base.startswith("pak") and base[3:].isdigit():
            paks.append((int(base[3:]), entry))
    return [os.path.join(game_dir, entry) for _, entry in sorted(paks)]
