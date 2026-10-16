"""Checks that recorded-demo benchmark configs use Quake's supported wait command."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import re
import unittest


MODULE_PATH = Path(__file__).parents[1] / "bench" / "qvrdemo.py"
SPEC = importlib.util.spec_from_file_location("qvrdemo", MODULE_PATH)
assert SPEC and SPEC.loader
qvrdemo = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(qvrdemo)


class DemoConfigTests(unittest.TestCase):
    def test_waits_are_supported_and_cover_the_requested_window(self) -> None:
        frames = 450
        commands = qvrdemo.build("smoke", frames, 72, "sample", "vrfiringrange")

        self.assertFalse(any(re.fullmatch(r"wait\d+", command) for command in commands))
        self.assertEqual(commands.count("wait"), 120 + frames + 20)
        self.assertLess(commands.index("echo BENCH_SETUP_DONE"), commands.index("vr_bench_begin smoke 450"))
        self.assertLess(commands.index("playdemo sample"), commands.index("vr_bench_end"))
        self.assertEqual(commands[-1], "quit")
        self.assertNotIn("disconnect", commands)


if __name__ == "__main__":
    unittest.main()
