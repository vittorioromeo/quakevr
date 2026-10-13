"""Focused stock census parser/search-precedence tests. Fixtures contain no game assets."""
import tempfile
from pathlib import Path
import struct
import unittest

import numpy as np
from PIL import Image
import pbr_stock_audit as audit

ROOT = Path(__file__).resolve().parents[2]


def pack(path, entries):
    payload, directory = bytearray(), bytearray()
    for name, data in entries.items():
        directory.extend(struct.pack("<56sii", name.encode(), 12 + len(payload), len(data)))
        payload.extend(data)
    path.write_bytes(struct.pack("<4sii", b"PACK", 12 + len(payload), len(directory)) + payload + directory)


class StockAuditTests(unittest.TestCase):
    def setUp(self):
        # Retain private evidence; the agent protocol does not delete files.
        self.folder = Path(tempfile.mkdtemp(prefix="stock-audit-", dir=ROOT / "scratch"))

    def test_pak_gap_and_pack_before_loose(self):
        (self.folder / "same.txt").write_bytes(b"loose")
        pack(self.folder / "pak0.pak", {"same.txt": b"pak0"})
        pack(self.folder / "pak1.pak", {"same.txt": b"pak1"})
        pack(self.folder / "pak10.pak", {"ignored.txt": b"not-mounted"})
        files = audit.engine_entries(self.folder)
        self.assertEqual(audit.pbr.read_entry(files["same.txt"]), b"pak1")
        self.assertNotIn("ignored.txt", files)

    def test_extensions_before_root_and_map_before_global(self):
        high, low = self.folder / "high", self.folder / "low"
        for folder in (high, low):
            (folder / "textures/test").mkdir(parents=True)
        Image.new("RGBA", (2, 2), "red").save(high / "textures/material.tga")
        Image.new("RGBA", (2, 2), "blue").save(low / "textures/material.png")
        Image.new("RGBA", (2, 2), "green").save(high / "textures/test/material.tga")
        indexes = [audit.engine_entries(high), audit.engine_entries(low)]
        global_image = audit.find_image(["textures/material"], indexes)
        self.assertEqual(tuple(global_image[0][0, 0]), (0, 0, 255, 255))
        exact_image = audit.find_image(["textures/test/material", "textures/material"], indexes)
        self.assertEqual(tuple(exact_image[0][0, 0]), (0, 128, 0, 255))

    def test_mdl_single_and_group_skins(self):
        header = bytearray(84)
        struct.pack_into("<4si", header, 0, b"IDPO", 6)
        struct.pack_into("<iii", header, 48, 2, 2, 2)
        single = struct.pack("<i", 0) + bytes((1, 2, 3, 4))
        group = struct.pack("<iiff", 1, 2, .1, .2) + bytes((5, 6, 7, 8, 9, 10, 11, 12))
        frames = list(audit.mdl_skins(header + single + group))
        self.assertEqual([name for name, _ in frames], ["skin0", "skin1.0", "skin1.1"])
        np.testing.assert_array_equal(frames[2][1], [[9, 10], [11, 12]])
        with self.assertRaisesRegex(ValueError, "truncated"):
            list(audit.mdl_skins(header + single + group[:-1]))

    def test_rgba_protection_identity_includes_dimensions_and_alpha(self):
        rgba = np.full((2, 4, 4), 255, dtype=np.uint8)
        digest = audit.pbr.content_hash(rgba)
        self.assertNotEqual(digest, audit.pbr.content_hash(rgba.reshape(4, 2, 4)))
        rgba[0, 0, 3] = 0
        self.assertNotEqual(digest, audit.pbr.content_hash(rgba))


if __name__ == "__main__":
    unittest.main()
