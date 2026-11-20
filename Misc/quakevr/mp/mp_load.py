"""mp_load.py -- a CPU-load generator for the multiplayer harness (MP_PLAN B25): the suite's flakes came from other
work on the machine (other builds and tests) starving the games, which keep real time. `mp_test.py --load N` runs N of
these busy workers at normal priority (above the games' below-normal) for the whole run, as a compiler's processes
would, so such a failure can be reproduced here and the scenario made to wait on the state it needs.

Run as a worker: python mp_load.py <duty> [<harness pid>] -- spins `duty` of every 50 ms (1.0: always), until it is
stopped or that process is gone.
"""

import os
import subprocess
import sys
import time

NO_WINDOW = 0x08000000
SLICE_S = 0.05  # a worker's busy-then-idle period


def parent_alive(handle):
    """The harness that started this worker still running (a Windows process handle; elsewhere: the parent's pid)."""
    if handle is None:
        return os.getppid() != 1
    import ctypes
    return ctypes.windll.kernel32.WaitForSingleObject(handle, 0) == 0x102  # (WAIT_TIMEOUT: still running)


def open_parent(pid):
    if os.name != "nt" or not pid:
        return None
    import ctypes
    return ctypes.windll.kernel32.OpenProcess(0x00100000, False, pid) or None  # (SYNCHRONIZE)


def spin(duty, parent=0):
    """The worker: busy `duty` of each slice (integer work, as a compiler's), idle the rest; until the harness that
    started it is gone (a killed run leaves no worker spinning)."""
    handle = open_parent(parent)
    x = 1
    slices = 0
    while True:
        slices += 1
        if slices % 20 == 0 and parent and not parent_alive(handle):
            return
        start = time.perf_counter()
        busy_until = start + SLICE_S * duty
        while time.perf_counter() < busy_until:
            for _ in range(2000):
                x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        rest = start + SLICE_S - time.perf_counter()
        if rest > 0:
            time.sleep(rest)


class Load:
    """`count` busy workers (`duty` of the time each) at normal priority, started by start(), stopped by stop()."""

    def __init__(self, count, duty=1.0):
        self.count = max(0, int(count))
        self.duty = min(1.0, max(0.05, float(duty)))
        self.procs = []

    def start(self):
        for _ in range(self.count):
            self.procs.append(subprocess.Popen(
                [sys.executable, os.path.abspath(__file__), "%g" % self.duty, str(os.getpid())],
                stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                creationflags=NO_WINDOW if os.name == "nt" else 0))
        if self.count:
            print("mp_load: %d busy workers at normal priority (%.0f%% of the time each)" % (self.count, 100 * self.duty),
                  flush=True)
        return self

    def stop(self):
        for p in self.procs:
            if p.poll() is None:
                p.terminate()
        for p in self.procs:
            try:
                p.wait(10)
            except subprocess.TimeoutExpired:
                p.kill()
        if self.procs:
            print("mp_load: %d workers stopped" % len(self.procs), flush=True)
        self.procs = []


if __name__ == "__main__":
    spin(float(sys.argv[1]) if len(sys.argv) > 1 else 1.0, int(sys.argv[2]) if len(sys.argv) > 2 else 0)
