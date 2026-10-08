# test_pulse_on_markers.py -- pulse_on_markers.py against a mock of Resolve's and Fusion's scripting objects:
# markers -> beats -> comp frames -> keys, and a whole run (the nodes made, wired, keyed; a second run rebuilding them).
#   python Misc/trailer/resolve/test_pulse_on_markers.py

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pulse_on_markers as P  # noqa: E402


class Out:
    def __init__(self, tool):
        self.tool = tool
        self.inputs = []

    def GetTool(self):
        return self.tool

    def GetConnectedInputs(self):
        return {i + 1: x for i, x in enumerate(self.inputs)}


class In:
    def __init__(self, tool, name):
        self.tool, self.name, self.src = tool, name, None

    def GetTool(self):
        return self.tool

    def GetConnectedOutput(self):
        return self.src

    def ConnectTo(self, out):
        if self.src is not None and self in self.src.inputs:
            self.src.inputs.remove(self)
        self.src = out
        out.inputs.append(self)


class Tool:
    def __init__(self, comp, reg, name):
        object.__setattr__(self, "comp", comp)
        object.__setattr__(self, "reg", reg)
        object.__setattr__(self, "Name", name)
        object.__setattr__(self, "Output", Out(self))
        object.__setattr__(self, "inputs", {"Input": In(self, "Input")})
        object.__setattr__(self, "keys", {})
        object.__setattr__(self, "mods", [])

    def __getattr__(self, k):
        return self.inputs.setdefault(k, In(self, k))

    def __setattr__(self, k, v):
        if isinstance(v, Out):
            self.inputs.setdefault(k, In(self, k)).ConnectTo(v)
        else:
            object.__setattr__(self, k, v)

    def SetAttrs(self, a):
        object.__setattr__(self, "Name", a["TOOLS_Name"])

    def AddModifier(self, inp, kind):
        self.mods.append((inp, kind))
        self.keys[inp] = {}
        return True

    def SetInput(self, inp, v, t):
        self.keys.setdefault(inp, {})[t] = v

    def Delete(self):
        for i in self.inputs.values():
            if i.src is not None:
                i.src.inputs.remove(i)
        for i in list(self.Output.inputs):
            i.src = None
        self.comp.tools.remove(self)


class Comp:
    def __init__(self, render_start):
        self.tools = []
        self.attrs = {"COMPN_RenderStart": render_start}
        self.CurrentFrame = None
        self.locked = 0
        mi = self.add("MediaIn", "MediaIn1")
        mo = self.add("MediaOut", "MediaOut1")
        mo.Input = mi.Output

    def add(self, reg, name):
        t = Tool(self, reg, name)
        self.tools.append(t)
        return t

    def AddTool(self, reg, x, y):
        return self.add(reg, reg + "1")

    def FindTool(self, name):
        return next((t for t in self.tools if t.Name == name), None)

    def GetToolList(self, sel, reg):
        return {i + 1: t for i, t in enumerate(x for x in self.tools if x.reg == reg)}

    def GetAttrs(self):
        return self.attrs

    def Lock(self):
        self.locked += 1

    def Unlock(self):
        self.locked -= 1

    def StartUndo(self, n):
        pass

    def EndUndo(self, keep):
        pass


class Item:
    def __init__(self, name, start, end, comp):
        self.name, self.start, self.end, self.comp = name, start, end, comp

    def GetName(self):
        return self.name

    def GetStart(self, sub=False):
        return self.start

    def GetEnd(self, sub=False):
        return self.end

    def GetLeftOffset(self, sub=False):
        return 0

    def GetFusionCompCount(self):
        return 1

    def GetFusionCompByIndex(self, i):
        return self.comp


class Timeline:
    def __init__(self, markers, items, start=86400):
        self.markers, self.items, self.start = markers, items, start

    def GetName(self):
        return "Trailer"

    def GetMarkers(self):
        return self.markers

    def GetStartFrame(self):
        return self.start

    def GetCurrentVideoItem(self):
        return self.items[0]

    def GetTrackCount(self, kind):
        return 1

    def GetItemListInTrack(self, kind, i):
        return self.items


class Resolve:
    def __init__(self, tl):
        self.tl = tl

    def GetProjectManager(self):
        return self

    def GetCurrentProject(self):
        return self

    def GetCurrentTimeline(self):
        return self.tl


def chain(comp):
    """The tool names from MediaIn1 down to MediaOut1."""
    t = comp.FindTool("MediaIn1")
    names = [t.Name]
    while t.Output.inputs:
        t = t.Output.inputs[0].GetTool()
        names.append(t.Name)
    return names


class Tests(unittest.TestCase):
    def test_beats_by_colour(self):
        m = {300: {"color": "Blue"}, 120: {"color": "blue"}, 200: {"color": "Red"}, 60.0: {"color": "Blue"}}
        self.assertEqual(P.beat_frames(m, "Blue"), [60, 120, 300])
        self.assertEqual(P.beat_frames(m, None), [60, 120, 200, 300])
        self.assertEqual(P.beat_frames({}, "Blue"), [])

    def test_comp_frames(self):
        # Timeline starts at 86400; the clip spans timeline 86500..86700, its comp starts at 24 (a trimmed head).
        self.assertEqual(P.to_comp_frames([50, 100, 200, 300, 400], 86400, 86500, 86700, 24), [24, 124])
        self.assertEqual(P.to_comp_frames([86600], 86400, 86500, 86700, 0, relative=False), [100])

    def test_keys(self):
        self.assertEqual(P.pulse_keys([10, 40], 1.0, 1.06, 2, 10),
                         {10: 1.0, 12: 1.06, 20: 1.0, 40: 1.0, 42: 1.06, 50: 1.0})
        # Beats 6 frames apart: the decay key would land past the next beat, so it's left out.
        self.assertEqual(P.pulse_keys([10, 16], 1.0, 1.06, 2, 10), {10: 1.0, 12: 1.06, 16: 1.0, 18: 1.06, 26: 1.0})
        # Beats 2 frames apart: no peak key on the next beat's frame.
        self.assertEqual(P.pulse_keys([10, 12], 1.0, 1.06, 2, 10), {10: 1.0, 12: 1.0, 14: 1.06, 22: 1.0})

    def test_run_twice(self):
        comp = Comp(render_start=0)
        tl = Timeline({100: {"color": "Blue"}, 130: {"color": "Blue"}, 115: {"color": "Green"}},
                      [Item("logo", 86450, 87000, comp)])
        P.resolve = Resolve(tl)
        P.GLOW = True
        try:
            P.main()
            self.assertEqual(chain(comp), ["MediaIn1", "Pulse", "PulseGlow", "MediaOut1"])
            pulse = comp.FindTool("Pulse")
            self.assertEqual(pulse.reg, "Transform")
            self.assertEqual(pulse.keys["Size"], {50: 1.0, 52: 1.06, 60: 1.0, 80: 1.0, 82: 1.06, 90: 1.0})
            self.assertEqual(comp.FindTool("PulseGlow").keys["Gain"][52], P.GLOW_PEAK)
            # A second run after moving a marker: rebuilt, no duplicate nodes, only the new keys.
            tl.markers = {110: {"color": "Blue"}}
            P.main()
            self.assertEqual(chain(comp), ["MediaIn1", "Pulse", "PulseGlow", "MediaOut1"])
            self.assertEqual(len([t for t in comp.tools if t.Name == "Pulse"]), 1)
            self.assertEqual(comp.FindTool("Pulse").keys["Size"], {60: 1.0, 62: 1.06, 70: 1.0})
            self.assertEqual(comp.locked, 0)
        finally:
            P.GLOW = False
            del P.resolve

    def test_by_name_and_nothing_to_do(self):
        comp = Comp(render_start=10)
        tl = Timeline({5: {"color": "Blue"}}, [Item("logo", 86450, 87000, comp)])
        P.resolve = Resolve(tl)
        P.CLIP_NAME = "logo"
        try:
            P.main()                                # the marker is before the clip: nothing keyed, nothing added
            self.assertIsNone(comp.FindTool("Pulse"))
        finally:
            P.CLIP_NAME = None
            del P.resolve


if __name__ == "__main__":
    unittest.main(verbosity=1)
