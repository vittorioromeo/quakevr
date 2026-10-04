import struct, sys, zlib, os

pak = r"C:/OHWorkspace/qvr-kit/bases/slipgate-pvs/qbase/id1/pak0.pak"
out = os.path.dirname(os.path.abspath(__file__))

with open(pak, "rb") as f:
    data = f.read()
assert data[:4] == b"PACK"
n = struct.unpack_from("<i", data, 8)[0]
ents = []
for i in range(n):
    off = 14 + i * 56
    name, offset, size = struct.unpack_from("<56si i", data, off)
    name = name.split(b"\0")[0].decode()
    ents.append((name, offset, size))

want = [e for e in ents if e[0].lower().startswith("maps/start.")]
for name, offset, size in want:
    print(name, size)
    target = os.path.join(out, os.path.basename(name))
    with open(target, "wb") as g:
        g.write(data[offset:offset + size])
