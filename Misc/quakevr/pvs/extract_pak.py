"""Extract maps/start.* from a Quake PAK: python extract_pak.py <pak> [output_directory]."""
from pathlib import Path
import struct
import sys


def extract(pak, output):
    data = Path(pak).read_bytes()
    magic, directory, size = struct.unpack_from("<4sii", data)
    if magic != b"PACK" or directory < 12 or size < 0 or size % 64 or directory + size > len(data):
        raise ValueError("invalid PAK directory")
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    for pos in range(directory, directory + size, 64):
        name, offset, length = struct.unpack_from("<56sii", data, pos)
        name = name.split(b"\0", 1)[0].decode("ascii")
        if not name.lower().startswith("maps/start."):
            continue
        if offset < 12 or length < 0 or offset + length > len(data):
            raise ValueError(f"invalid PAK entry: {name}")
        target = output / Path(name).name
        target.write_bytes(data[offset:offset + length])
        print(f"{name}: {length} bytes -> {target}")


if __name__ == "__main__":
    extract(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else Path(__file__).parent)
