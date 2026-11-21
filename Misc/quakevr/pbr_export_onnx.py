#!/usr/bin/env python3
"""PBRify's two CC0 models (PBR_PIPELINE.md's pinned 1.7.2 checkpoints) exported once to ONNX for the installer's
"Make reliefs for Dwell and Quoth" step (Installer/README.md, "Reliefs"; DWELL.md 9.9 option A), then a parity check
of the whole generator: `pbr_generate.py generate --limit N` over AD with the .pth files and with the .onnx files, the
companions compared pixel by pixel.

  export:  python pbr_export_onnx.py export --models <dir with the two .pth> --output Installer/models
  parity:  python pbr_export_onnx.py parity --models <dir> --onnx Installer/models --game <Quake>/ad
           --palette-game <Quake>/id1 --work scratch/reliefs_parity [--limit 40]

Needs torch and spandrel (export), onnx and onnxruntime (parity): a build-time environment, never the player's.
The exported graphs take any 1x3xHxW float input in [0,1] and give 1x3xHxW (1x scale), as the .pth through spandrel.
"""
from __future__ import annotations
import argparse
import hashlib
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
MODELS = {"normal": ("1x-PBRify_NormalV3.pth", "b0a18270da765f02eaae3c228203bee0677fc28b2854a562515e4aae9c61223b"),
          "height": ("1x-PBRify_Height.pth", "5b973ecb8bae9d96d14d77b8a8f1d88fb6a8580bcc1bb55d2872811acdc4277d")}
OPSET = 17


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def onnx_name(kind):
    return f"pbrify_{kind}.onnx"


def export(args):
    import torch
    from spandrel import ModelLoader
    args.output.mkdir(parents=True, exist_ok=True)
    for kind, (file, expected) in MODELS.items():
        path = args.models / file
        if sha(path) != expected:
            raise SystemExit(f"checksum mismatch: {path}")
        model = ModelLoader().load_from_file(str(path)).eval().model.float()
        sample = torch.rand(1, 3, 64, 80)
        destination = args.output / onnx_name(kind)
        with torch.inference_mode():
            reference = model(sample)
            torch.onnx.export(model, sample, str(destination), input_names=["rgb"], output_names=["out"],
                              dynamic_axes={"rgb": {2: "h", 3: "w"}, "out": {2: "h", 3: "w"}},
                              opset_version=OPSET, do_constant_folding=True, dynamo=False)
        import onnxruntime as ort
        session = ort.InferenceSession(str(destination), providers=["CPUExecutionProvider"])
        for h, w in ((64, 80), (97, 131)):
            x = torch.rand(1, 3, h, w)
            with torch.inference_mode():
                want = model(x).numpy()
            got = session.run(None, {"rgb": x.numpy()})[0]
            error = float(np.max(np.abs(want - got)))
            print(f"{kind}: {destination.name} {h}x{w} max |torch - onnx| = {error:.2e}")
            if got.shape != want.shape or error > 1e-3:
                raise SystemExit(f"{kind}: exported graph differs from torch")
        print(f"{kind}: {destination} {destination.stat().st_size} bytes sha256 {sha(destination)}")


def parity(args):
    """The same generator run twice (pth, onnx) on the first --limit AD sources; every companion compared."""
    def run(label, normal, height, extra):
        out = args.work / label
        command = [sys.executable, str(HERE / "pbr_generate.py"), "generate", "--game", str(args.game),
                   "--palette-game", str(args.palette_game), "--texture-game", str(args.game),
                   "--normal-model", str(normal), "--height-model", str(height), "--output", str(out),
                   "--cache", str(args.work / f"cache_{label}"), "--limit", str(args.limit), "--device", "cpu", *extra]
        subprocess.run(command, check=True)
        return out
    torch_out = run("torch", args.models / MODELS["normal"][0], args.models / MODELS["height"][0], [])
    normal_onnx, height_onnx = args.onnx / onnx_name("normal"), args.onnx / onnx_name("height")
    onnx_out = run("onnx", normal_onnx, height_onnx, ["--normal-sha256", sha(normal_onnx), "--height-sha256", sha(height_onnx)])
    from PIL import Image
    worst, mean_errors, count, mismatched = 0, [], 0, 0
    for a in sorted((torch_out / "textures/pbr").glob("*_norm.png")):
        b = onnx_out / "textures/pbr" / a.name
        if not b.is_file():
            mismatched += 1
            continue
        x = np.asarray(Image.open(a).convert("RGBA")).astype(int)
        y = np.asarray(Image.open(b).convert("RGBA")).astype(int)
        if x.shape != y.shape:
            mismatched += 1
            continue
        d = np.abs(x - y)
        worst = max(worst, int(d.max()))
        mean_errors.append(float(d.mean()))
        count += 1
    mean = float(np.mean(mean_errors)) if mean_errors else 0.0
    ok = count > 0 and mismatched == 0 and mean <= args.mean_limit
    print(f"pbr parity: {count} companions, {mismatched} missing or resized, mean |torch - onnx| {mean:.4f} "
          f"(limit {args.mean_limit}), worst texel {worst}: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("mode", choices=("export", "parity"))
    p.add_argument("--models", type=Path, required=True, help="the folder with PBRify 1.7.2's two .pth checkpoints")
    p.add_argument("--output", type=Path, help="export: where the .onnx files go")
    p.add_argument("--onnx", type=Path, help="parity: the folder with the exported .onnx files")
    p.add_argument("--game", type=Path, help="parity: Arcane Dimensions' folder")
    p.add_argument("--palette-game", type=Path, help="parity: id1 (gfx/palette.lmp)")
    p.add_argument("--work", type=Path, help="parity: a scratch folder for the two runs")
    p.add_argument("--limit", type=int, default=40)
    p.add_argument("--mean-limit", type=float, default=0.5, help="parity: largest mean texel difference (0-255)")
    args = p.parse_args()
    if args.mode == "export":
        export(args)
        return 0
    return parity(args)


if __name__ == "__main__":
    sys.exit(main())
