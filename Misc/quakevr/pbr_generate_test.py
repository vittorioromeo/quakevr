#!/usr/bin/env python3
"""Focused regression tests for the PBR extraction/merging/cache contract (no models needed)."""
import argparse
import hashlib
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from PIL import Image
import pbr_generate as pbr


class FixturePredictor:
    def __init__(self, path, device, threads):
        self.path = path
        self.checksum = pbr.sha(path.read_bytes())
        self.backend = "fixture"
        self.device = "cpu"

    def __call__(self, rgb, padding, tile):
        out = np.full((*rgb.shape[:2], 3), .5, np.float32)
        if "normal" in self.path.name:
            out[:, :, 2] = 1
        else:
            out[:] = rgb[:, :, :3] / 255
        return out


def make_bsp(name, indices):
    h, w = indices.shape
    data = bytearray(124 + 8 + 40 + w * h)
    struct.pack_into("<I", data, 0, 29)
    struct.pack_into("<ii", data, 20, 124, len(data) - 124)
    struct.pack_into("<ii", data, 124, 1, 8)
    encoded = name.encode("ascii")
    data[132:132 + len(encoded)] = encoded
    struct.pack_into("<III", data, 148, w, h, 40)
    data[172:] = indices.tobytes()
    return bytes(data)


def make_pak(path, content):
    data = bytearray(12)
    records = []
    for name, raw in content.items():
        records.append(struct.pack("<56sii", name.encode("ascii"), len(data), len(raw)))
        data.extend(raw)
    start = len(data)
    directory = b"".join(records)
    data.extend(directory)
    struct.pack_into("<4sii", data, 0, b"PACK", start, len(directory))
    path.write_bytes(data)


class PipelineTests(unittest.TestCase):
    def test_generated_height_relative_calibration(self):
        prediction = np.linspace(.45, .55, 100).reshape(10, 10)
        holes = np.zeros((10, 10), bool)
        height = pbr.calibrated_height(prediction, holes, .5)
        self.assertEqual(int(height.max()), 255)
        self.assertGreater(int(height.max() - height.min()), 80)
        self.assertGreaterEqual(int(height.min()), 127)
        # A constant learned offset carries no geometric information.
        flat = pbr.calibrated_height(np.full((10, 10), .5), holes, .5)
        self.assertTrue(np.all(flat == 255))
        weak = pbr.calibrated_height(prediction * .1, holes, .5)
        self.assertTrue(np.all(weak == 255))
        # Excluded cutouts must not set the calibration range.
        prediction[0] = 0
        holes[0] = True
        calibrated = pbr.calibrated_height(prediction, holes, .5)
        self.assertEqual(int(calibrated[~holes].max()), 255)

    def setUp(self):
        scratch = Path(__file__).resolve().parents[2] / "scratch"
        scratch.mkdir(exist_ok=True)
        self.root = Path(tempfile.mkdtemp(prefix="pbr-tests-", dir=scratch))
        self.args = argparse.Namespace(images=[], game=[], authored=self.root / "authored", output=self.root / "output",
                cache=self.root / "cache", normal_model=self.root / "normal.pth", height_model=self.root / "height.pth",
                device="cpu", threads=1, padding=48, tile=256, depth=.25, green="auto", authored_match=.5, limit=0)
        self.args.authored.mkdir()
        self.args.normal_model.write_bytes(b"normal fixture")
        self.args.height_model.write_bytes(b"height fixture")
        self.args.normal_sha256 = pbr.sha(self.args.normal_model.read_bytes())
        self.args.height_sha256 = pbr.sha(self.args.height_model.read_bytes())
        self.sources = {}

    def source(self, name="wall", rgb=77, holes=None):
        image = np.full((8, 8, 4), rgb, np.uint8)
        image[:, :, 3] = 255
        pbr.add_source(self.sources, image, name, "maps/test.bsp", "embedded", None, holes=holes)
        return pbr.content_hash(image)

    def run_generation(self):
        with patch.object(pbr, "Predictor", FixturePredictor):
            status = pbr.generate(self.args, self.sources, {"scan_errors": []}, "palette fixture")
        manifest = json.loads((self.args.output / "pbr-manifest.json").read_text())
        self.assertEqual(status, 0)
        expected = []
        for entry in manifest["textures"]:
            with Image.open(self.args.output / entry["file"]) as image:
                digest = hashlib.sha256(b"qvr-pbr-v1\0" + struct.pack("<II", *image.size) +
                                        image.convert("RGBA").tobytes()).hexdigest()
            flags = int(entry["generated_normal"]) + 2 * int(entry["generated_height"])
            expected.append(f"{entry['sha256'][:32]} {digest} {flags}")
        index = (self.args.output / "textures/pbr/provenance.txt").read_bytes()
        self.assertEqual(index, ("qvr-pbr-provenance-v1\n" + "\n".join(sorted(expected)) + "\n").encode("ascii"))
        item = manifest["textures"][0]
        return pbr.read_image(self.args.output / item["file"]), manifest

    def test_provenance_independent_flags_alpha_dimensions_and_checksum(self):
        self.source()
        rgba, manifest = self.run_generation()
        item = manifest["textures"][0]
        original_digest = pbr.provenance_line(self.args.output, item).split()[1]
        for flags in range(4):
            item["generated_normal"], item["generated_height"] = bool(flags & 1), bool(flags & 2)
            self.assertEqual(int(pbr.provenance_line(self.args.output, item).split()[2]), flags)
        path = self.args.output / item["file"]
        rgba[0, 0, 3] ^= 1
        Image.fromarray(rgba).save(path)
        with self.assertRaisesRegex(ValueError, "modified output"):
            pbr.provenance_line(self.args.output, item)
        item["output_sha256"] = pbr.sha(path.read_bytes())
        self.assertNotEqual(pbr.provenance_line(self.args.output, item).split()[1], original_digest)
        # Same row-major pixels under different dimensions must produce a different digest.
        original_digest = pbr.provenance_line(self.args.output, item).split()[1]
        Image.fromarray(rgba.reshape(4, 16, 4)).save(path)
        item["output_sha256"] = pbr.sha(path.read_bytes())
        self.assertNotEqual(pbr.provenance_line(self.args.output, item).split()[1], original_digest)

    def test_provenance_offline_refresh_drops_removed_sources_without_inference(self):
        self.source("wall", 77)
        self.source("stone", 123)
        _, manifest = self.run_generation()
        self.assertEqual(len(manifest["textures"]), 2)
        # A failed or removed output cannot keep a flag through a refreshed index.
        removed = self.args.output / manifest["textures"].pop()["file"]
        pbr.json_write(self.args.output / "pbr-manifest.json", manifest)
        with patch.object(pbr, "Predictor", side_effect=AssertionError("unexpected model load")), \
                patch.object(sys, "argv", ["pbr_generate.py", "provenance", "--output", str(self.args.output)]):
            self.assertEqual(pbr.main(), 0)
        lines = (self.args.output / "textures/pbr/provenance.txt").read_text().splitlines()
        self.assertEqual(len(lines), 2)
        self.assertEqual(lines[1], pbr.provenance_line(self.args.output, manifest["textures"][0]))
        self.assertTrue(removed.is_file())
        with self.assertRaisesRegex(ValueError, "duplicate"):
            pbr.provenance_text([lines[1], lines[1]])

    def test_provenance_checkpoint_and_generation_refresh(self):
        for index in range(51):
            self.source(f"wall{index}", 10 + index)
        counts = []
        original_writer = pbr.write_provenance
        def checkpoint(output, lines):
            counts.append(len(lines))
            manifest = json.loads((output / "pbr-manifest.json").read_text())
            self.assertEqual(len(lines), len(manifest["textures"]))
            original_writer(output, lines)
        with patch.object(pbr, "write_provenance", side_effect=checkpoint):
            self.run_generation()
            self.assertEqual(counts, [50, 51])
        # Remove a discovered source: a regular cache-only refresh must omit its entry.
        self.sources.pop(next(iter(self.sources)))
        with patch.object(FixturePredictor, "__call__", side_effect=AssertionError("unexpected inference")):
            _, manifest = self.run_generation()
        self.assertEqual(len(manifest["textures"]), 50)

    def test_shipped_provenance_matches_every_manifest_output(self):
        output = Path(__file__).resolve().parents[2] / "quakevr"
        manifest = json.loads((output / "pbr-manifest.json").read_text())
        expected = []
        for item in manifest["textures"]:
            path = output / item["file"]
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), item["output_sha256"])
            with Image.open(path) as image:
                digest = hashlib.sha256(b"qvr-pbr-v1\0" + struct.pack("<II", *image.size) +
                                        image.convert("RGBA").tobytes()).hexdigest()
            expected.append(f"{item['sha256'][:32]} {digest} {int(item['generated_normal']) + 2 * int(item['generated_height'])}")
        index = (output / "textures/pbr/provenance.txt").read_bytes()
        self.assertEqual(index, ("qvr-pbr-provenance-v1\n" + "\n".join(sorted(expected)) + "\n").encode("ascii"))

    def test_pak_and_loose_precedence_collision(self):
        # The engine's order (COM_AddGameDirectory): loose files, then pak0, pak1, ... on top; a gap ends the PAKs.
        def value(entries):
            return int(list(pbr.bsp_textures(pbr.read_entry(entries["maps/test.bsp"])))[0][1][0, 0])
        (self.root / "maps").mkdir()
        (self.root / "maps/test.bsp").write_bytes(make_bsp("same", np.full((4,4), 99, np.uint8)))
        self.assertEqual(value(pbr.entries(self.root)), 99)
        make_pak(self.root / "pak3.pak", {"maps/test.bsp": make_bsp("same", np.full((4,4), 3, np.uint8))})
        make_pak(self.root / "pak_old.pak", {"maps/test.bsp": make_bsp("same", np.full((4,4), 7, np.uint8))})
        self.assertEqual(value(pbr.entries(self.root)), 99)  # no pak0: pak3 and pak_old are never mounted
        make_pak(self.root / "pak0.pak", {"maps/test.bsp": make_bsp("same", np.full((4,4), 0, np.uint8))})
        self.assertEqual(value(pbr.entries(self.root)), 0)  # a PAK above the loose file
        make_pak(self.root / "pak1.pak", {"maps/test.bsp": make_bsp("same", np.full((4,4), 1, np.uint8))})
        self.assertEqual(value(pbr.entries(self.root)), 1)  # pak1 above pak0; pak3 after the missing pak2 still not
        self.source(rgb=1)
        self.source(rgb=99)
        self.assertEqual(len(self.sources), 2)

    def test_effective_texture_override_precedence(self):
        high, low = self.root / "high", self.root / "low"
        high.mkdir()
        low.mkdir()
        def picture(colour, ext):
            import io
            out = io.BytesIO()
            Image.new("RGBA", (8, 8), colour).save(out, format=ext)
            return out.getvalue()
        red = picture((255, 0, 0, 255), "TGA")
        blue = picture((0, 0, 255, 0), "TGA")
        green = picture((0, 255, 0, 255), "PNG")
        make_pak(high / "pak0.pak", {"textures/wall.tga": red,
                                     "textures/ext/wall.tga": red})
        make_pak(low / "pak0.pak", {"textures/test/wall.tga": blue,
                                    "textures/ext/wall.png": green})
        indexes = [pbr.entries(high), pbr.entries(low)]
        # Map-specific lower game overrides generic highest game.
        chosen, origin = pbr.selected_override("maps/test.bsp", "wall", indexes)
        self.assertTrue(np.all(chosen == (0, 0, 255, 0)))
        self.assertIn("pak0.pak:textures/test/wall.tga", origin)
        # PNG in lower game wins over TGA in higher game for the same filename.
        chosen, _ = pbr.selected_override("maps/ext.bsp", "wall", indexes)
        self.assertTrue(np.all(chosen == (0, 255, 0, 255)))
        # Highest game wins when the extensions agree; its PAKs override its loose files (as the engine mounts them).
        (high / "textures").mkdir()
        (high / "textures/wall.tga").write_bytes(blue)
        chosen, origin = pbr.selected_override("maps/other.bsp", "wall", [pbr.entries(high),pbr.entries(low)])
        self.assertTrue(np.all(chosen == (255, 0, 0, 255)))
        self.assertIn("pak0.pak:textures/wall.tga", origin)
        # A loose file the PAKs don't have is still found.
        (high / "textures/loose.tga").write_bytes(blue)
        chosen, origin = pbr.selected_override("maps/other.bsp", "loose", [pbr.entries(high),pbr.entries(low)])
        self.assertTrue(np.all(chosen == (0, 0, 255, 0)))
        self.assertTrue(origin.endswith("loose.tga"))

    def test_discovery_change_reuses_verified_previous_output(self):
        self.source()
        original, _ = self.run_generation()
        for cache in self.args.cache.glob("*.png"):
            cache.rename(self.root / ("retained-" + cache.name))
        manifest_path = self.args.output / "pbr-manifest.json"
        manifest = json.loads(manifest_path.read_text())
        manifest["configuration"]["generator_sha256"] = "previous-discovery-implementation"
        pbr.json_write(manifest_path, manifest)
        with patch.object(FixturePredictor, "__call__", side_effect=AssertionError("unexpected inference")):
            result, manifest = self.run_generation()
        self.assertTrue(np.array_equal(result, original))
        self.assertEqual(manifest["summary"]["cached"], 1)

    def test_previous_height_packing_invalidates_resume(self):
        self.source()
        self.run_generation()
        for cache in self.args.cache.glob("*.png"):
            cache.rename(self.root / ("old-packing-" + cache.name))
        manifest_path = self.args.output / "pbr-manifest.json"
        manifest = json.loads(manifest_path.read_text())
        manifest["configuration"]["cache_version"] = 1
        pbr.json_write(manifest_path, manifest)
        _, manifest = self.run_generation()
        self.assertEqual(manifest["summary"]["cached"], 0)

    def test_authored_rgb_generated_alpha_resume(self):
        self.source()
        authored = np.full((16, 16, 3), (137, 114, 250), np.uint8)
        Image.fromarray(authored).save(self.args.authored / "wall_norm.png")
        result, manifest = self.run_generation()
        self.assertTrue(np.array_equal(result[:,:,:3], authored))
        self.assertGreaterEqual(int(result[:,:,3].min()), 191)
        self.assertFalse(manifest["textures"][0]["generated_normal"])
        self.assertTrue(manifest["textures"][0]["generated_height"])
        again, manifest = self.run_generation()
        self.assertTrue(np.array_equal(result, again))
        self.assertEqual(manifest["summary"]["cached"], 1)

    def test_authored_nonflat_rgba_preserved(self):
        self.source()
        authored = np.full((8,8,4),(137,114,250,255), np.uint8)
        authored[2:5,2:5,3] = 7
        Image.fromarray(authored).save(self.args.authored / "wall_norm.png")
        result, manifest = self.run_generation()
        self.assertTrue(np.array_equal(result, authored))
        self.assertFalse(manifest["textures"][0]["generated_height"])

    def test_authored_height_generated_rgb_cutouts(self):
        self.source(holes=np.eye(8, dtype=bool))
        height = np.arange(64, dtype=np.uint8).reshape(8,8)
        Image.fromarray(height).save(self.args.authored / "wall_bump.png")
        result, _ = self.run_generation()
        self.assertTrue(np.array_equal(result[:,:,3], height))
        self.assertTrue(np.all(result[:,:,:3][np.eye(8, dtype=bool)] == (128,128,255)))

    def test_generated_animation_consistency(self):
        self.source("+0switch", 30)
        self.source("+1switch", 190)
        _, manifest = self.run_generation()
        self.assertEqual(len(manifest["textures"]), 2)
        self.assertEqual(manifest["textures"][0]["output_sha256"], manifest["textures"][1]["output_sha256"])

    def test_scrolling_animation_keeps_own_relief(self):
        rgba = np.random.default_rng(17).integers(0, 256, (16, 16, 4), dtype=np.uint8)
        rgba[:, :, 3] = 255
        pbr.add_source(self.sources, rgba, "+0arrow", "maps/test.bsp", "embedded", None)
        shifted = np.roll(rgba, 4, axis=1)
        pbr.add_source(self.sources, shifted, "+1arrow", "maps/test.bsp", "embedded", None)
        _, manifest = self.run_generation()
        self.assertNotEqual(manifest["textures"][0]["output_sha256"], manifest["textures"][1]["output_sha256"])

    def test_authored_colliding_name_match_gate(self):
        rgba=np.full((8,8,4),255,np.uint8)
        rgba[:,:,:3]=np.arange(64,dtype=np.uint8).reshape(8,8,1)*3
        pbr.add_source(self.sources,rgba,"wall","maps/test.bsp","embedded",None)
        Image.fromarray(rgba).save(self.args.authored / "wall.png")
        authored=np.full((8,8,3),(137,114,250),np.uint8)
        Image.fromarray(authored).save(self.args.authored / "wall_norm.png")
        self.assertGreater(pbr.detail_match(rgba,rgba),.99)
        accepted,_,_=pbr.authored_components(next(iter(self.sources.values())),self.args.authored)
        self.assertTrue(np.array_equal(accepted,authored))
        rgba[:,:,:3]=255-rgba[:,:,:3]
        pbr.add_source(self.sources,rgba,"wall","maps/other.bsp","embedded",None)
        rejected,_,info=pbr.authored_components(self.sources[pbr.content_hash(rgba)],self.args.authored)
        self.assertIsNone(rejected)
        self.assertLess(info["rejected_match"]["wall"],-.99)

    def test_indexed_fence_coverage(self):
        game=self.root / "game"
        game.mkdir()
        indices=np.full((8,8), 20, np.uint8)
        indices[2:4,2:4]=255
        make_pak(game / "pak0.pak", {"maps/test.bsp":make_bsp("{fence",indices)})
        self.args.game=[game]
        palette=np.tile(np.arange(256,dtype=np.uint8)[:,None],(1,3))
        sources,census=pbr.scan(self.args,palette)
        self.assertEqual(sum(int(s["holes"].sum()) for s in sources.values()),4)
        self.assertEqual(census["scan_errors"],[])

    def test_checksum_rejection(self):
        self.source()
        self.args.normal_sha256 = "0" * 64
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            self.run_generation()

    def test_loose_image_dedup_and_exclusions(self):
        images = self.root / "images"
        images.mkdir()
        picture = Image.new("RGB", (8,8), (21,35,70))
        for name in ("a.png", "b.png", "a_norm.png", "sky1.png", "a_roughness.png"):
            picture.save(images / name)
        self.args.images = [images]
        sources, census = pbr.scan(self.args, None)
        self.assertEqual(len(sources), 1)
        self.assertEqual(census["image_files"], 2)
        self.assertEqual(census["scan_errors"], [])


if __name__ == "__main__":
    unittest.main()
