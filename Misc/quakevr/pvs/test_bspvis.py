"""Run with python -m unittest discover -s Misc/quakevr/pvs."""
from pathlib import Path
import struct
import tempfile
import unittest

from bspvis import BSP, decompress_vis
from extract_pak import extract


class VisibilityTests(unittest.TestCase):
    def test_literals_and_zero_runs(self):
        self.assertEqual(decompress_vis(bytes([0x80, 0, 2, 0x03, 0xFF]), 0, 5),
                         bytes([0x80, 0, 0, 0x03, 0xFF]))

    def test_invalid_rows_fail_instead_of_inventing_visibility(self):
        for blob in (b"", b"\0", b"\0\0", b"\0\3", b"\x80"):
            with self.subTest(blob=blob), self.assertRaises(ValueError):
                decompress_vis(blob, 0, 2)

    def test_leaf_numbers_exclude_solid_and_padding(self):
        bsp = BSP.__new__(BSP)
        bsp.numleafs = 9
        self.assertEqual(bsp.bits(bytes([0x81, 0xFF])), [1, 8, 9])

    def test_brush_entities_without_origin_have_searchable_centres(self):
        bsp = BSP.__new__(BSP)
        bsp.models = [(0,) * 16, (353, 1601, -15, 735, 2015, -1) + (0,) * 10]
        self.assertEqual(bsp.entity_origin({"model": "*1"}), (544, 1808, -8))
        self.assertEqual(bsp.entity_origin({"origin": "278 1728 24"}), (278, 1728, 24))

    def test_pak_directory_at_declared_offset(self):
        # The directory is at the end, not at a fixed offset, and its size is bytes, not entry count.
        payload = b"test BSP payload"
        directory = 12 + len(payload)
        pak = struct.pack("<4sii", b"PACK", directory, 128) + payload
        pak += struct.pack("<56sii", b"maps/start.bsp", 12, len(payload))
        pak += struct.pack("<56sii", b"unrelated.dat", 12, len(payload))
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "pak0.pak"
            path.write_bytes(pak)
            output = Path(tmp) / "output"
            extract(path, output)
            self.assertEqual((output / "start.bsp").read_bytes(), payload)
            self.assertEqual([p.name for p in output.iterdir()], ["start.bsp"])


if __name__ == "__main__":
    unittest.main()
