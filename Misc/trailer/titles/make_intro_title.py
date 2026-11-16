"""Generate Misc/trailer/titles/QVR Intro Title.setting (a Fusion macro) from the tables below.

    python Misc/trailer/titles/make_intro_title.py [out.setting]

Kept as a generator because the macro is ~70 tools with long Custom-tool expressions; the .setting is the product
(committed; install_titles.py installs it). Change the look here, run this, then install_titles.py.
"""
import os
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                         "QVR Intro Title.setting")

# ---- user controls on Ctl: (id, label, kind, default, extra) --------------------------------------------------------
# kind: text, slider, int, color0/1/2 (group), combo, check, hidden (expression)
CTL = [
    ("Line1", "Line 1", "text", "Quake VR", {}),
    ("Blood", "Blood", "slider", 1, {"min": 0, "max": 2}),
    ("AccentRed", "Blood Colour", "color", 0.42, {"group": 1, "id": 0}),
    ("AccentGreen", "Green", "color", 0.015, {"group": 1, "id": 1}),
    ("AccentBlue", "Blue", "color", 0.01, {"group": 1, "id": 2}),
    ("MetalRed", "Metal Tint", "color", 0.56, {"group": 5, "id": 0}),
    ("MetalGreen", "Green", "color", 0.46, {"group": 5, "id": 1}),
    ("MetalBlue", "Blue", "color", 0.34, {"group": 5, "id": 2}),
    ("Relief", "Relief Depth", "slider", 1, {"min": 0, "max": 2}),
    ("SweepGain", "Light Sweep", "slider", 1, {"min": 0, "max": 2}),
    ("SweepDir", "Sweep Direction", "combo", 0, {"items": ["Left to Right", "Right to Left"]}),
    ("SweepStart", "Sweep Start (frames)", "int", 24, {"min": 0, "max": 240}),
    ("SweepFrames", "Sweep Length (frames)", "int", 110, {"min": 1, "max": 300}),
    ("Intensity", "Intensity", "slider", 1, {"min": 0, "max": 2}),
    ("L1Start", "Line 1 Start (frames)", "int", 0, {"min": 0, "max": 240}),
    ("L1In", "Line 1 In (frames)", "int", 46, {"min": 1, "max": 240}),
    ("L2Start", "Line 2 Start (frames)", "int", 34, {"min": 0, "max": 240}),
    ("L2In", "Line 2 In (frames)", "int", 44, {"min": 1, "max": 240}),
    ("RuleStart", "Rule Start (frames)", "int", 66, {"min": 0, "max": 240}),
    ("RuleIn", "Rule In (frames)", "int", 26, {"min": 1, "max": 240}),
    ("L3Start", "Line 3 Start (frames)", "int", 84, {"min": 0, "max": 240}),
    ("L3In", "Line 3 In (frames)", "int", 34, {"min": 1, "max": 240}),
    ("HoldFrames", "Hold (frames, all lines in; 0 = to the clip end)", "int", 180, {"min": 0, "max": 900}),
    ("OutFrames", "Out (frames)", "int", 36, {"min": 1, "max": 240}),
    ("OutAt", "Out Start (frames, 0 = clip end - Out)", "int", 0, {"min": 0, "max": 900}),
    ("Backdrop", "Dark Backdrop", "check", 0, {}),
    ("Draft", "Draft (fast preview)", "check", 0, {}),
    ("ScrimAmount", "Scrim", "slider", 0.55, {"min": 0, "max": 1, "maxa": 1}),
    ("ShadowOpacity", "Shadow Opacity", "slider", 0.8, {"min": 0, "max": 1, "maxa": 1}),
    ("ShadowSoftness", "Shadow Softness (px at 1080p)", "slider", 12, {"min": 0, "max": 40}),
    ("ShadowOffset", "Shadow Offset (px at 1080p)", "slider", 8, {"min": 0, "max": 40}),
    ("VOffset", "Vertical Offset", "slider", 0, {"min": -0.25, "max": 0.25, "mina": -1}),
    ("RuleWidth", "Rule Width", "slider", 0.6, {"min": 0.1, "max": 1}),
    ("L3Track", "Line 3 Tracking", "slider", 1.16, {"min": 0.8, "max": 2}),
    # hidden constants
    ("GapD", None, "hidden", 0.075, {}),
    ("Gap12", None, "hidden", 0.07, {}),
    ("Gap3", None, "hidden", 0.034, {}),
]
EXPR = [  # hidden computed values on Ctl
    ("LT", "time - comp.RenderStart"),
    # the out: at Out Start when set, else after the hold (from when the last line is in), and always so that it ends
    # by the clip's end
    ("OutStart", "iif(OutAt > 0, OutAt, min(comp.RenderEnd - comp.RenderStart + 1 - OutFrames, iif(HoldFrames > 0, "
                 "max(max(L1Start + L1In, L2Start + L2In), max(RuleStart + RuleIn, L3Start + L3In)) + HoldFrames, "
                 "1000000)))"),
    ("P1", "min(max((LT - L1Start)/max(L1In, 1), 0), 1)"),
    ("P2", "min(max((LT - L2Start)/max(L2In, 1), 0), 1)"),
    ("PR", "min(max((LT - RuleStart)/max(RuleIn, 1), 0), 1)"),
    ("P3", "min(max((LT - L3Start)/max(L3In, 1), 0), 1)"),
    ("POut", "min(max((LT - OutStart)/max(OutFrames, 1), 0), 1)"),
    ("E1", "1 - (1 - P1)^2.4"),
    ("ER", "1 - (1 - PR)^3"),
    ("E3", "1 - (1 - P3)^2.4"),
    ("EOut", "POut^1.6"),
    ("Env", "min(1, max(P1, P2)*1.6)*(1 - EOut)"),
    ("Flick", "1 - min(1, Intensity)*0.7*max(1 - P1, POut)*iif(abs(sin(LT*78.233 + LT*LT*0.131)) > 0.6, 1, 0)"),
    ("Torch", "0.9 + 0.1*sin(LT*0.23)*sin(LT*0.071 + 1.3)"),
    ("Low1", "1.06 - 1.12*E1 + 1.15*EOut"),
    ("Low2", "-0.06 + 1.15*EOut"),
    ("Low3", "1.06 - 1.12*E3 + 1.15*EOut"),
    ("Mold", "min(1, max(0, (LT - L2Start)/max(0.2*L2In, 1)))"),
    ("Flood", "1 - (1 - min(1, max(0, (LT - L2Start - 0.18*L2In)/max(0.82*L2In, 1))))^1.7"),
    ("Splash", "min(1, max(0, (LT - L2Start - 0.16*L2In)/8))"),
    ("Grow", "min(1, max(0, (LT - L2Start - 0.45*L2In)/(L2In + 140)))^0.5"),
    ("Pool", "min(1, max(0, (LT - L2Start - 0.7*L2In)/(L2In + 100)))^0.6"),
    ("Shake", "Intensity*iif(LT > L2Start + 0.16*L2In, 0.8^(LT - L2Start - 0.16*L2In), 0)*(1 - EOut)"),
    ("PSweep", "min(max((LT - SweepStart)/max(SweepFrames, 1), 0), 1)"),
    ("USweep", "iif(SweepDir == 0, PSweep*PSweep*(3 - 2*PSweep), 1 - PSweep*PSweep*(3 - 2*PSweep))"),
    ("LightEnv", "SweepGain*min(1, PSweep*5)*min(1, (1 - PSweep)*5)"),
    ("Glisten", "0.8 + 0.2*sin(LT*0.37)*sin(LT*0.113 + 0.7)"),
    ("LeadQ", "iif(Ctl.Line1.Value:sub(1, 1):upper() == 'Q', 1, 0)"),
    ("YR", "0.35 + VOffset"),
]

# ---- published inputs, in Inspector order: (name, sourceop, source, label or None, extra) ---------------------------
PUB = [
    ("Line1", "Ctl", "Line1", "Line 1", {}),
    ("Line2", "L2Text", "StyledText", "Line 2", {}),
    ("Line3", "L3Text", "StyledText", "Line 3", {}),
    ("Draft", "Ctl", "Draft", None, {}),
    ("Blood", "Ctl", "Blood", None, {}),
    ("AccentRed", "Ctl", "AccentRed", "Blood Colour", {"cg": 1}),
    ("AccentGreen", "Ctl", "AccentGreen", None, {"cg": 1}),
    ("AccentBlue", "Ctl", "AccentBlue", None, {"cg": 1}),
    ("MetalRed", "Ctl", "MetalRed", "Metal Tint", {"cg": 5}),
    ("MetalGreen", "Ctl", "MetalGreen", None, {"cg": 5}),
    ("MetalBlue", "Ctl", "MetalBlue", None, {"cg": 5}),
    ("Relief", "Ctl", "Relief", None, {}),
    ("SweepGain", "Ctl", "SweepGain", None, {}),
    ("SweepDir", "Ctl", "SweepDir", None, {}),
    ("SweepStart", "Ctl", "SweepStart", None, {}),
    ("SweepFrames", "Ctl", "SweepFrames", None, {}),
    ("Intensity", "Ctl", "Intensity", None, {}),
    ("L1Start", "Ctl", "L1Start", None, {}),
    ("L1In", "Ctl", "L1In", None, {}),
    ("L2Start", "Ctl", "L2Start", None, {}),
    ("L2In", "Ctl", "L2In", None, {}),
    ("RuleStart", "Ctl", "RuleStart", None, {}),
    ("RuleIn", "Ctl", "RuleIn", None, {}),
    ("L3Start", "Ctl", "L3Start", None, {}),
    ("L3In", "Ctl", "L3In", None, {}),
    ("HoldFrames", "Ctl", "HoldFrames", None, {}),
    ("OutFrames", "Ctl", "OutFrames", None, {}),
    ("OutAt", "Ctl", "OutAt", None, {}),
    ("Backdrop", "Ctl", "Backdrop", None, {}),
    ("Scrim", "Ctl", "ScrimAmount", None, {}),
    ("ShadowOpacity", "Ctl", "ShadowOpacity", None, {}),
    ("ShadowSoftness", "Ctl", "ShadowSoftness", None, {}),
    ("ShadowOffset", "Ctl", "ShadowOffset", None, {}),
    ("VOffset", "Ctl", "VOffset", None, {}),
    ("RuleWidth", "Ctl", "RuleWidth", None, {}),
    ("Line1Size", "L1Text", "Size", "Line 1 Size", {"def": 0.16}),
    ("Line1Tracking", "L1Text", "CharacterSpacing", "Line 1 Tracking", {"def": 1.06}),
    ("Line2Size", "L2Text", "Size", "Line 2 Size", {"def": 0.1}),
    ("Line2Font", "L2Text", "Font", "Line 2 Font", {"cg": 2}),
    ("Line2Style", "L2Text", "Style", None, {"cg": 2}),
    ("Line2Tracking", "L2Text", "CharacterSpacing", "Line 2 Tracking", {"def": 1.24}),
    ("Line3Size", "L3Text", "Size", "Line 3 Size", {"def": 0.034}),
    ("Line3Font", "L3Text", "Font", "Line 3 Font", {"cg": 4}),
    ("Line3Style", "L3Text", "Style", None, {"cg": 4}),
    ("L3Track", "Ctl", "L3Track", None, {}),
]

# hash of a column index i1 with salt k, in [0, 1)
def H(k, v="i1"):
    return "(sin(%s*12.9898 + %s)*43758.5453 - floor(sin(%s*12.9898 + %s)*43758.5453))" % (v, k, v, k)

FULL = "UseFrameFormatSettings = Input { Value = 1, }, Width = Input { Value = 1920, }, Height = Input { Value = 1080, }, "


def q(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def inp(name, v):
    """v: number -> Value; str starting with '=' -> Expression; ('op', 'out') -> link; ('fuid', x); ('text', x)."""
    if isinstance(v, tuple):
        if v[0] == "fuid":
            return '%s = Input { Value = FuID { "%s" }, },' % (name, v[1])
        if v[0] == "text":
            return "%s = Input { Value = %s, }," % (name, q(v[1]))
        return '%s = Input { SourceOp = "%s", Source = "%s", },' % (name, v[0], v[1])
    if isinstance(v, str):
        if v.startswith("="):
            return "%s = Input { Expression = %s, }," % (name, q(v[1:]))
        return "%s = Input { Value = %s, }," % (name, q(v))
    return "%s = Input { Value = %s, }," % (name, repr(v))


TOOLS = []   # (name, type, {inputs}, pos, extra_text)


def tool(name, typ, inputs, pos, extra=""):
    TOOLS.append((name, typ, inputs, pos, extra))


def holder_controls(names):
    return "UserControls = ordered() {\n" + "".join(
        '\t\t\t\t\t\t%s = { LINKS_Name = "%s", LINKID_DataType = "Number", INPID_InputControl = "SliderControl", '
        'IC_Visible = false, ICS_ControlPage = "Controls", },\n' % (n, n) for n in names) + "\t\t\t\t\t},"


# ======================================================================================================================
# Ctl: the scrim layer, every user control and the timing.
ctl_inputs = {"EffectMask": ("ScrimMask", "Mask")}
ctl_inputs.update({"UseFrameFormatSettings": 1, "Width": 1920, "Height": 1080,
                   "TopLeftRed": 0.012, "TopLeftGreen": 0.008, "TopLeftBlue": 0.008,
                   "TopLeftAlpha": "=ScrimAmount*Env"})
for cid, label, kind, default, ex in CTL:
    ctl_inputs[cid] = ("text", default) if kind == "text" else default
for cid, e in EXPR:
    ctl_inputs[cid] = "=" + e
uc = []
for cid, label, kind, default, ex in CTL:
    base = 'ICS_ControlPage = "Controls",'
    if kind == "text":
        uc.append('%s = { LINKS_Name = "%s", LINKID_DataType = "Text", INPID_InputControl = "TextEditControl", '
                  'TEC_Lines = 1, %s },' % (cid, label, base))
    elif kind == "color":
        uc.append('%s = { LINKS_Name = "%s", LINKID_DataType = "Number", INPID_InputControl = "ColorControl", '
                  'INP_Default = %r, INP_MinScale = 0, INP_MaxScale = 1, IC_ControlGroup = %d, IC_ControlID = %d, %s },'
                  % (cid, label, default, ex["group"], ex["id"], base))
    elif kind == "combo":
        uc.append('%s = { LINKS_Name = "%s", LINKID_DataType = "Number", INPID_InputControl = "ComboControl", '
                  'INP_Default = %r, INP_Integer = true, %s CC_LabelPosition = "Horizontal", %s },'
                  % (cid, label, default, base, ", ".join('{ CCS_AddString = "%s" }' % s for s in ex["items"])))
    elif kind == "check":
        uc.append('%s = { LINKS_Name = "%s", LINKID_DataType = "Number", INPID_InputControl = "CheckboxControl", '
                  'INP_Default = %r, INP_Integer = true, CBC_TriState = false, %s },' % (cid, label, default, base))
    elif kind in ("slider", "int"):
        extra = "INP_MinScale = %r, INP_MaxScale = %r, " % (ex["min"], ex["max"])
        extra += "INP_MinAllowed = %r, " % ex.get("mina", ex["min"] if kind == "int" or ex["min"] >= 0 else -1e9)
        if "maxa" in ex:
            extra += "INP_MaxAllowed = %r, " % ex["maxa"]
        if kind == "int":
            extra += "INP_Integer = true, "
        uc.append('%s = { LINKS_Name = "%s", LINKID_DataType = "Number", INPID_InputControl = "SliderControl", '
                  'INP_Default = %r, %s%s },' % (cid, label, default, extra, base))
    else:
        uc.append('%s = { LINKS_Name = "%s", LINKID_DataType = "Number", INPID_InputControl = "SliderControl", '
                  'INP_Default = %r, IC_Visible = false, %s },' % (cid, cid, default, base))
for cid, e in EXPR:
    uc.append('%s = { LINKS_Name = "%s", LINKID_DataType = "Number", INPID_InputControl = "SliderControl", '
              'IC_Visible = false, ICS_ControlPage = "Controls", },' % (cid, cid))
tool("Ctl", "Background", ctl_inputs, (-660, -60),
     "UserControls = ordered() {\n" + "".join("\t\t\t\t\t\t%s\n" % u for u in uc) + "\t\t\t\t\t},")
tool("ScrimMask", "EllipseMask", {"SoftEdge": 0.35, "Center": "=Point(0.5, Ctl.YR + 0.16)", "Width": 1.25,
                                  "Height": 0.72}, (-660, -110))
tool("Blank", "Background", {"UseFrameFormatSettings": 1, "Width": 1920, "Height": 1080, "TopLeftAlpha": 0}, (-550, -300))

# ---- backdrop -------------------------------------------------------------------------------------------------------
tool("BackNoise", "FastNoise", {"UseFrameFormatSettings": 1, "Width": 1920, "Height": 1080, "Detail": 8,
                                "Contrast": 1.3, "Brightness": -0.8, "XScale": 3, "SeetheRate": 0.06,
                                "Color1Red": 0.22, "Color1Green": 0.15, "Color1Blue": 0.12}, (-660, 60))
tool("VigMask", "EllipseMask", {"Invert": 1, "SoftEdge": 0.5, "Center": "=Point(0.5, Ctl.YR + 0.16)",
                                "Width": 1.05, "Height": 0.95}, (-550, 100))
tool("VigFill", "Background", {"EffectMask": ("VigMask", "Mask"), "UseFrameFormatSettings": 1, "Width": 1920,
                               "Height": 1080, "TopLeftRed": 0, "TopLeftGreen": 0, "TopLeftBlue": 0,
                               "TopLeftAlpha": 0.92}, (-550, 60))
tool("BackA", "Merge", {"Background": ("Blank", "Output"), "Foreground": ("BackNoise", "Output"),
                        "Blend": "=Ctl.Backdrop", "PerformDepthMerge": 0}, (-440, 60))
tool("BackB", "Merge", {"Background": ("BackA", "Output"), "Foreground": ("VigFill", "Output"),
                        "Blend": "=Ctl.Backdrop", "PerformDepthMerge": 0}, (-330, 60))
tool("MergeScrim", "Merge", {"Background": ("BackB", "Output"), "Foreground": ("Ctl", "Output"),
                             "PerformDepthMerge": 0}, (-220, 60))

# ---- reveal field: high at the centre, low at the sides, noisy ---------------------------------------------------
# RevealRamp = 1 - 1.5|x - 0.5| - 0.25|y - 0.55|, from GPU gradients (a full-frame Custom tool cost ~0.3 s a frame):
# min(0.1125 + 1.5x, 1.6125 - 1.5x) + min(0.25y, 0.275 - 0.25y); the mins by Darken merges, the sum by an additive
# merge (Gain 0: the foreground's alpha hides nothing)
def ramp(name, kind, a, b, pos):
    """A grey gradient: Horizontal a (left) to b (right), Vertical a (bottom) to b (top)."""
    far = "TopRight" if kind == "Horizontal" else "TopLeft"
    near = "TopLeft" if kind == "Horizontal" else "BottomLeft"
    d = {"UseFrameFormatSettings": 1, "Width": 1920, "Height": 1080, "Type": ("fuid", kind)}
    for c in ("Red", "Green", "Blue"):
        d[near + c], d[far + c] = a, b
    tool(name, "Background", d, pos)


ramp("RampH1", "Horizontal", 0.1125, 1.6125, (-660, 140))
ramp("RampH2", "Horizontal", 1.6125, 0.1125, (-660, 180))
ramp("RampV1", "Vertical", 0.0, 0.25, (-660, 220))
ramp("RampV2", "Vertical", 0.275, 0.025, (-660, 260))
tool("RampH", "Merge", {"Background": ("RampH1", "Output"), "Foreground": ("RampH2", "Output"),
                        "ApplyMode": ("fuid", "Darken"), "PerformDepthMerge": 0}, (-550, 160))
tool("RampV", "Merge", {"Background": ("RampV1", "Output"), "Foreground": ("RampV2", "Output"),
                        "ApplyMode": ("fuid", "Darken"), "PerformDepthMerge": 0}, (-550, 240))
tool("RevealRamp", "Merge", {"Background": ("RampH", "Output"), "Foreground": ("RampV", "Output"), "Gain": 0,
                             "PerformDepthMerge": 0}, (-440, 160))
tool("RevealNoise", "FastNoise", {"UseFrameFormatSettings": 1, "Width": 1920, "Height": 1080, "Detail": 8,
                                  "Contrast": 1.6, "XScale": 9, "SeetheRate": 0.25}, (-330, 160))
tool("RevealField", "Merge", {"Background": ("RevealRamp", "Output"), "Foreground": ("RevealNoise", "Output"),
                              "Blend": 0.38, "PerformDepthMerge": 0}, (-220, 160))
for nm, lo, dh, pos in (("L1Mask", "Ctl.Low1", 0.035, 200), ("L1InkMask", "Ctl.Low1 - 0.1", 0.03, 240),
                        ("L2Mask", "Ctl.Low2", 0.04, 280), ("L3Mask", "Ctl.Low3", 0.05, 320)):
    hi = "Ctl.Low1 - 0.07" if nm == "L1InkMask" else "%s + %r" % (lo, dh)
    tool(nm, "BitmapMask", {"Image": ("RevealField", "Output"), "Channel": ("fuid", "Luminance"),
                            "Low": "=" + lo, "High": "=" + hi}, (-110, pos))

# ======================================================================================================================
# Line 1: "Quake VR" in DpQuake, lit metal. A leading Q is drawn as DpQuake's O plus a blade tail (L1Tail).
tool("L1CLS", "StyledTextCLS", {"Text": "=Text((Ctl.Line1.Value:gsub('^Q', 'O'):gsub('^q', 'o')))"}, (0, -420))
TEXT_COMMON = {"UseFrameFormatSettings": 1, "Width": 1920, "Height": 1080, "HorizontalLeftCenterRight": 0,
               "HorizontalJustificationNew": 1, "VerticalJustificationNew": 3, "Red1": 1, "Green1": 1, "Blue1": 1}
d = dict(TEXT_COMMON)
d.update({"Depth": 4, "StyledText": ("L1CLS", "StyledText"), "Font": "DpQuake", "Style": "Regular", "Size": 0.16,
          "CharacterSpacing": 1.06,
          "Center": "=Point(0.5, L2Text.Output[0].DataWindow[4]/L2Text.Output[0].Height + Ctl.Gap12 - (L1Lead.Output[0].DataWindow[2]/L1Lead.Output[0].Height - 0.5))",
          "VerticalTopCenterBottom": 1, "Enabled2": 1, "Name2": "Outline", "Thickness2": 0.03, "OutsideOnly2": 1,
          "JoinStyle2": 0, "Red2": 0, "Green2": 0, "Blue2": 0, "Alpha2": 0.9, "Softness2": 1})
tool("L1Text", "TextPlus", d, (0, -380))
d = dict(TEXT_COMMON)
d.update({"StyledText": "=Text((Ctl.Line1.Value:sub(1, 1):gsub('q', 'o'):gsub('[^o]', 'O')))",
          "Font": "DpQuake", "Style": "Regular", "Size": "=L1Text.Size",
          "Center": "=Point(0.5, 0.5)", "VerticalTopCenterBottom": 1})
tool("L1Lead", "TextPlus", d, (0, -460))
G1 = [
    ("W", "L1Text.Output[0].Width"), ("H", "L1Text.Output[0].Height"),
    ("X1", "L1Text.Output[0].DataWindow[1]"), ("X2", "L1Text.Output[0].DataWindow[3]"),
    ("Y1", "L1Text.Output[0].DataWindow[2]"), ("Y2", "L1Text.Output[0].DataWindow[4]"),
    ("OW", "(L1Lead.Output[0].DataWindow[3] - L1Lead.Output[0].DataWindow[1])"),
    ("OH", "(L1Lead.Output[0].DataWindow[4] - L1Lead.Output[0].DataWindow[2])"),
    ("Res", "W/1920"),
    ("OL", "0.029*L1Text.Size*W"),
    ("OB", "(L1Text.Center.Y + L1Lead.Output[0].DataWindow[2]/L1Lead.Output[0].Height - 0.5)*H"),
    ("LX", "(X1 - 0.3*(X2 - X1) + Ctl.USweep*1.6*(X2 - X1))/W"),
    ("LY", "(Y1 + 0.78*(Y2 - Y1))/H"),
    ("PX0", "floor(max(0, X1 - 0.03*W))"),
    ("PY0", "floor(max(0, min(Y1, OB - 0.45*OH) - 0.03*H))"),
    ("PW", "2*ceil((min(W, X2 + 0.03*W) - PX0)/2)"),
    ("PH", "2*ceil((min(H, Y2 + 0.03*H) - PY0)/2)"),
    ("CX0", "PX0/W"), ("CY0", "PY0/H"), ("CW", "PW/W"), ("CH", "PH/H"),
    ("CY", "(Y1 + Y2)/2/H"),
]
tool("G1", "Background", dict([("Width", 16), ("Height", 16)] + [(k, "=" + v) for k, v in G1]), (110, -460),
     holder_controls([k for k, _ in G1]))
tool("L1Crop", "Crop", {"Input": ("L1Text", "Output"), "XOffset": "=G1.PX0", "YOffset": "=G1.PY0",
                        "XSize": "=G1.PW", "YSize": "=G1.PH"}, (0, -340))


def full(prefix, G, pos):
    """The crop over a transparent canvas of its size: a full data window, so Custom tools fill the whole crop."""
    x, y = pos
    tool(prefix + "Canvas", "Background", {"Width": "=%s.PW" % G, "Height": "=%s.PH" % G, "TopLeftAlpha": 0}, (x, y - 40))
    tool(prefix + "Full", "Merge", {"Background": (prefix + "Canvas", "Output"), "Foreground": (prefix + "Crop", "Output"),
                                    "PerformDepthMerge": 0}, (x, y))


full("L1", "G1", (55, -300))
# the tail: a tapered blade from inside the O's bowl sweeping down-right under the baseline (u, v: the O's box)
TAIL_I = {
    "NumberIn1": "=G1.X1 + G1.OL - G1.PX0", "NumberIn2": "=G1.OB - G1.PY0", "NumberIn3": "=G1.OW",
    "NumberIn4": "=G1.OH", "NumberIn5": "=Ctl.LeadQ", "NumberIn6": "=G1.OL",
    "Intermediate1": "((x*w1 - n1)/max(n3, 1) - 0.40)/0.85",
    "Intermediate2": "min(max(i1, 0), 1)",
    "Intermediate3": "n5*n4*0.09*if(i1 > 0, if(i1 < 1, sin(180*i2^0.55), 0), 0)",
    "Intermediate4": "abs(y*h1 - n2 - n4*(0.16 - 0.36*(1 - (1 - i2)^2)))",
    "RedExpression": "max(r1, min(1, max(0, i3 - i4 + 0.5))*min(1, i3))",
    "GreenExpression": "max(g1, min(1, max(0, i3 - i4 + 0.5))*min(1, i3))",
    "BlueExpression": "max(b1, min(1, max(0, i3 - i4 + 0.5))*min(1, i3))",
    "AlphaExpression": "max(a1, 0.9*min(1, max(0, i3 + n6 - i4 + 0.5))*min(1, i3), min(1, max(0, i3 - i4 + 0.5))*min(1, i3))",
    "Image1": ("L1Full", "Output"), "Image2": ("L1Lead", "Output"),
}
tool("L1Tail", "Custom", TAIL_I, (110, -340))

LIGHT_R = ("n4*max(0, (0.7 - 0.45*i1 + 0.55*i2)*i3) + n5/(1 + (sqrt((p1x - x)^2 + ((p1y - y)*h1/w1)^2 + n2*n2)/n6)^2)"
           "*max(0, (i1*(p1x - x) + i2*(p1y - y)*h1/w1 + n2)*i3/sqrt((p1x - x)^2 + ((p1y - y)*h1/w1)^2 + n2*n2))")
LIGHT_G = ("%s*n4*max(0, (1.7 - 0.45*i1 + 0.55*i2)*i3/1.842)^n3 + n5/(1 + (sqrt((p1x - x)^2 + ((p1y - y)*h1/w1)^2 + n2*n2)/n6)^2)"
           "*max(0, (i1*(p1x - x) + i2*(p1y - y)*h1/w1 + n2 + sqrt((p1x - x)^2 + ((p1y - y)*h1/w1)^2 + n2*n2))*i3"
           "/sqrt(2*((p1x - x)^2 + ((p1y - y)*h1/w1)^2 + n2*n2) + 2*n2*sqrt((p1x - x)^2 + ((p1y - y)*h1/w1)^2 + n2*n2)))^n3")


def gated(expr, gate):
    """The lighting only where the colour tool shows it (`gate`: a test on Image2, the colour tool's own coverage
    input; zero elsewhere, where the colour tool multiplies it by 0), the sweeping light's term only while it is on.
    Custom tools evaluate `if` lazily: same image, a fraction of the work."""
    key, point = expr.split(" + n5/", 1)
    return "if(%s, %s + if(n5 > 0, n5/%s, 0), 0)" % (gate, key, point)


def relief(name, src, G, strength, step, spec_exp, spec_amb, key, light_gain, lh, lr, pos, gate, gate_src):
    tool(name, "Custom", {
        "NumberIn1": "=" + strength, "NumberIn2": "=%r/%s.CW" % (lh, G), "NumberIn3": spec_exp, "NumberIn4": key,
        "NumberIn5": "=%s*Ctl.LightEnv" % light_gain, "NumberIn6": "=%r/%s.CW" % (lr, G), "NumberIn7": "=%s.Res" % G,
        "NumberIn8": "=Ctl.Draft",
        "PointIn1": "=Point((%s.LX - %s.CX0)/%s.CW, (%s.LY - %s.CY0)/%s.CH)" % (G, G, G, G, G, G),
        "Setup1": "%r*n7/w1" % step, "Setup2": "%r*n7/h1" % step,
        "Intermediate1": "if(n8 > 0.5, 0, if(%s, (getr1d(x - s1, y) - getr1d(x + s1, y))*n1, 0))" % gate,
        "Intermediate2": "if(n8 > 0.5, 0, if(%s, (getr1d(x, y - s2) - getr1d(x, y + s2))*n1, 0))" % gate,
        "Intermediate3": "if(n8 > 0.5, 1, if(%s, 1/sqrt(i1*i1 + i2*i2 + 1), 1))" % gate,
        "RedExpression": "if(n8 > 0.5, 0.7*n4, %s)" % gated(LIGHT_R, gate),
        "GreenExpression": "if(n8 > 0.5, 0, %s)" % gated(LIGHT_G % spec_amb, gate),
        "BlueExpression": "r1", "AlphaExpression": "1",
        "Image1": (src, "Output"), "Image2": (gate_src, "Output")}, pos)


tool("L1Height", "Blur", {"Input": ("L1Tail", "Output"),
                          "XBlurSize": "=G1.Res*(5 + 7*min(Ctl.Relief, 2))*(1 - Ctl.Draft)"}, (220, -340))
# gate: L1Color's coverage i1 > 0 (Image2 = its Image1)
relief("L1Relief", "L1Height", "G1", "2.8*Ctl.Relief", 2.4, 22, 0.35, 0.75, "1.6", 0.12, 0.3, (330, -380),
       "r2 > 0.02*a2", "L1Tail")
tool("L1Grime", "FastNoise", {"Width": "=G1.PW", "Height": "=G1.PH", "Detail": 7, "Contrast": 1.8,
                              "XScale": "=30*G1.CW"}, (330, -420))
tool("L1Color", "Custom", {
    "NumberIn1": "=Ctl.MetalRed", "NumberIn2": "=Ctl.MetalGreen", "NumberIn3": "=Ctl.MetalBlue",
    "NumberIn4": 0.18, "NumberIn5": 1.3, "NumberIn6": 0.6,
    "Intermediate1": "min(max((r1 - 0.02*a1)/0.98, 0), 1)",
    "Intermediate2": "n6*min(max((getr3w(x*1.9 + 0.31, y*1.9 + 0.17) - 0.52)*3.5, 0), 1)",
    "Intermediate3": "(0.38 + 0.62*min(1, b2*1.5))*(0.5 + 0.85*r3)",
    "RedExpression": "i1*((n1*(1 - i2) + 0.36*i2)*i3*(n4 + r2) + g2*n5*(0.6*n1 + 0.4)*(1 - 0.8*i2)) + (a1 - i1)*0.012",
    "GreenExpression": "i1*((n2*(1 - i2) + 0.15*i2)*i3*(n4 + r2) + g2*n5*(0.6*n2 + 0.4)*(1 - 0.8*i2)) + (a1 - i1)*0.008",
    "BlueExpression": "i1*((n3*(1 - i2) + 0.07*i2)*i3*(n4 + r2) + g2*n5*(0.6*n3 + 0.4)*(1 - 0.8*i2)) + (a1 - i1)*0.008",
    "AlphaExpression": "a1",
    "Image1": ("L1Tail", "Output"), "Image2": ("L1Relief", "Output"), "Image3": ("L1Grime", "Output")}, (440, -380))
tool("L1Place", "Merge", {"Background": ("Blank", "Output"), "Foreground": ("L1Color", "Output"),
                          "Center": "=Point(G1.CX0 + G1.CW/2, G1.CY0 + G1.CH/2)", "PerformDepthMerge": 0}, (550, -380))
tool("L1Alpha", "BitmapMask", {"Image": ("L1Place", "Output"), "Channel": ("fuid", "Alpha")}, (550, -300))


def shadow(prefix, alpha, soft, off, blur_extra, pos):
    x, y = pos
    tool(prefix + "ShadowFill", "Background", {"EffectMask": (alpha, "Mask"), "UseFrameFormatSettings": 1,
                                               "Width": 1920, "Height": 1080, "TopLeftRed": 0, "TopLeftGreen": 0,
                                               "TopLeftBlue": 0}, (x, y))
    tool(prefix + "ShadowShift", "Transform", {"Input": (prefix + "ShadowFill", "Output"),
         "Center": "=Point(0.5 + %s*Ctl.ShadowOffset/1920, 0.5 - %s*Ctl.ShadowOffset/1080)" % (off, off)}, (x + 110, y))
    tool(prefix + "ShadowBlur", "Blur", {"Input": (prefix + "ShadowShift", "Output"),
         "XBlurSize": "=G1.Res*(%s*Ctl.ShadowSoftness + %s)" % (soft, blur_extra)}, (x + 220, y))


shadow("L1", "L1Alpha", "1", "1", "Ctl.Intensity*14*(1 - Ctl.E1)^2 + 8*Ctl.EOut", (660, -460))
tool("L1InkFill", "Background", {"EffectMask": ("L1Alpha", "Mask"), "UseFrameFormatSettings": 1, "Width": 1920,
                                 "Height": 1080, "TopLeftRed": "=Ctl.AccentRed", "TopLeftGreen": "=Ctl.AccentGreen",
                                 "TopLeftBlue": "=Ctl.AccentBlue"}, (660, -300))
tool("L1Glow", "Blur", {"Input": ("L1InkFill", "Output"), "XBlurSize": "=34*G1.Res"}, (770, -340))
tool("L1Ink", "Blur", {"Input": ("L1InkFill", "Output"),
                       "XBlurSize": "=G1.Res*(1 + Ctl.Intensity*9*(1 - Ctl.E1) + 6*Ctl.EOut)"}, (770, -300))
tool("L1Blur", "Blur", {"Input": ("L1Place", "Output"),
                        "XBlurSize": "=G1.Res*(Ctl.Intensity*14*(1 - Ctl.E1)^2 + 8*Ctl.EOut)"}, (660, -380))
tool("L1MA", "Merge", {"EffectMask": ("L1Mask", "Mask"), "Background": ("Blank", "Output"),
                       "Foreground": ("L1ShadowBlur", "Output"), "Blend": "=Ctl.ShadowOpacity*(1 - Ctl.EOut)",
                       "PerformDepthMerge": 0}, (990, -460))
tool("L1MB", "Merge", {"EffectMask": ("L1InkMask", "Mask"), "Background": ("L1MA", "Output"),
                       "Foreground": ("L1Glow", "Output"),
                       "Blend": "=min(1, Ctl.Intensity*(0.35 + 0.9*sin(pi*Ctl.P1) + 0.8*sin(pi*Ctl.POut))*Ctl.Torch)",
                       "PerformDepthMerge": 0}, (1045, -460))
tool("L1MC", "Merge", {"EffectMask": ("L1InkMask", "Mask"), "Background": ("L1MB", "Output"),
                       "Foreground": ("L1Ink", "Output"), "Blend": "=Ctl.Flick", "PerformDepthMerge": 0}, (1100, -460))
tool("L1MD", "Merge", {"EffectMask": ("L1Mask", "Mask"), "Background": ("L1MC", "Output"),
                       "Foreground": ("L1Blur", "Output"), "Blend": "=Ctl.Flick", "PerformDepthMerge": 0}, (1155, -460))
tool("L1Slam", "Transform", {"Input": ("L1MD", "Output"), "Center": "=Point(0.5, 0.5)", "Pivot": "=Point(0.5, G1.CY)",
                             "Size": "=1 + 0.12*Ctl.Intensity*(1 - Ctl.E1)^3 + 0.04*Ctl.EOut"}, (1210, -460))

# ======================================================================================================================
# The rule: a tapered metal blade slicing out from the centre, a hot spark at each front.
tool("RuleCanvas", "Background", {"Width": "=2*ceil((Ctl.RuleWidth + 0.06)*G1.W/2)", "Height": "=2*ceil(0.05*G1.H/2)",
                                  "TopLeftAlpha": 0}, (0, 380))
tool("RuleGrit", "FastNoise", {"Width": "=RuleCanvas.Width", "Height": "=RuleCanvas.Height", "Detail": 6,
                               "Contrast": 2, "LockXY": 0, "XScale": 140, "YScale": 10}, (110, 420))
SHADE = "(0.3 + 0.75*min(1, max(0, 0.5 + i2/(2*i3 + 1))))"
RULE_C = ("i4*({m}*{s}*(0.3 + 0.7*r2) + {k}*(0.22*2.718^(0 - (i2/(0.8*n6))*(i2/(0.8*n6))) + n5*2.718^(0 - ((x - n4)/0.045)*((x - n4)/0.045))*{s}))"
          " + {g}*n7*(2.718^(0 - ((i1 - n1)/(5*n6))*((i1 - n1)/(5*n6)) - (i2/(2.2*n6))*(i2/(2.2*n6))) + 0.5*2.718^(0 - ((i1 - n1)/(30*n6))*((i1 - n1)/(30*n6)) - (i2/(5*n6))*(i2/(5*n6))))")
tool("RuleCol", "Custom", {
    "NumberIn1": "=0.5*Ctl.RuleWidth*G1.W*Ctl.ER*(1 - Ctl.EOut)", "NumberIn2": "=0.5*Ctl.RuleWidth*G1.W",
    "NumberIn3": "=0.0052*G1.H", "NumberIn4": "=0.5 + (Ctl.USweep - 0.5)*1.2*Ctl.RuleWidth*G1.W/RuleCanvas.Width",
    "NumberIn5": "=Ctl.LightEnv", "NumberIn6": "=G1.Res",
    "NumberIn7": "=Ctl.Intensity*iif(Ctl.PR > 0, 1, 0)*(1 - Ctl.PR)^0.6",
    "PointIn1": "=Point(Ctl.MetalRed, Ctl.MetalGreen)", "PointIn2": "=Point(Ctl.MetalBlue, 0)",
    "Intermediate1": "abs(x*w1 - w1/2)",
    "Intermediate2": "y*h1 - h1/2",
    "Intermediate3": "n3*max(0, 1 - (i1/max(n2, 1))^2)^0.6*min(1, max(0, n1 - i1 + 0.5))",
    "Intermediate4": "min(1, max(0, i3 - abs(i2) + 0.5))*min(1, i3)",
    "RedExpression": RULE_C.format(m="p1x", s=SHADE, k="1", g="1.0"),
    "GreenExpression": RULE_C.format(m="p1y", s=SHADE, k="0.95", g="0.72"),
    "BlueExpression": RULE_C.format(m="p2x", s=SHADE, k="0.85", g="0.42"),
    "AlphaExpression": "min(1, max(0.92*min(1, max(0, i3 + 1.8*n6 - abs(i2) + 0.5))*min(1, i3), i4, "
                       "n7*(2.718^(0 - ((i1 - n1)/(5*n6))*((i1 - n1)/(5*n6)) - (i2/(2.2*n6))*(i2/(2.2*n6))) + 0.5*2.718^(0 - ((i1 - n1)/(30*n6))*((i1 - n1)/(30*n6)) - (i2/(5*n6))*(i2/(5*n6))))))",
    "Image1": ("RuleCanvas", "Output"), "Image2": ("RuleGrit", "Output")}, (110, 380))
tool("RulePlace", "Merge", {"Background": ("Blank", "Output"), "Foreground": ("RuleCol", "Output"),
                            "Center": "=Point(0.5, Ctl.YR)", "PerformDepthMerge": 0}, (220, 380))

# ======================================================================================================================
# Line 2: "UNLEASHED" in Cinzel Black: a dark mould, then blood floods in from the top, splashes, drips into a pool
# on the rule.
d = dict(TEXT_COMMON)
d.update({"Depth": 4, "StyledText": "UNLEASHED", "Font": "Cinzel", "Style": "Black", "Size": 0.1,
          "CharacterSpacing": 1.24, "Center": "=Point(0.5, Ctl.YR + Ctl.GapD)", "VerticalTopCenterBottom": 1})
tool("L2Text", "TextPlus", d, (0, -40))
G2 = [
    ("W", "L2Text.Output[0].Width"), ("H", "L2Text.Output[0].Height"),
    ("X1", "L2Text.Output[0].DataWindow[1]"), ("X2", "L2Text.Output[0].DataWindow[3]"),
    ("Y1", "L2Text.Output[0].DataWindow[2]"), ("Y2", "L2Text.Output[0].DataWindow[4]"),
    ("Res", "W/1920"),
    ("LX", "(X1 - 0.3*(X2 - X1) + Ctl.USweep*1.6*(X2 - X1))/W"),
    ("LY", "(Y1 + 0.7*(Y2 - Y1))/H"),
    ("PX0", "floor(max(0, X1 - 0.06*W))"),
    ("PY0", "floor(max(0, (Ctl.YR - 0.03)*H))"),
    ("PW", "2*ceil((min(W, X2 + 0.06*W) - PX0)/2)"),
    ("PH", "2*ceil((min(H, Y2 + 0.08*H) - PY0)/2)"),
    ("CX0", "PX0/W"), ("CY0", "PY0/H"), ("CW", "PW/W"), ("CH", "PH/H"),
    ("BY", "Y1 - PY0"), ("TY", "Y2 - PY0"), ("RY", "Ctl.YR*H - PY0"),
    ("DripMax", "(Y1 - Ctl.YR*H + 4*Res)*(0.55 + 0.5*min(Ctl.Blood, 2))"),
]
tool("G2", "Background", dict([("Width", 16), ("Height", 16)] + [(k, "=" + v) for k, v in G2]), (110, -120),
     holder_controls([k for k, _ in G2]))
tool("L2Crop", "Crop", {"Input": ("L2Text", "Output"), "XOffset": "=G2.PX0", "YOffset": "=G2.PY0",
                        "XSize": "=G2.PW", "YSize": "=G2.PH"}, (0, 0))
full("L2", "G2", (55, 40))
tool("L2Smear", "DirectionalBlur", {"Input": ("L2Full", "Output"), "Type": 0, "Angle": 90,
                                    "ClippingMode": ("fuid", "None"),
                                    "Length": "=max(0.0001, G2.DripMax*Ctl.Grow/G2.PW)"}, (110, 0))
tool("L2Near", "Blur", {"Input": ("L2Full", "Output"), "XBlurSize": "=40*G2.Res", "ClippingMode": ("fuid", "None")},
     (110, 40))
tool("SplatNoise", "FastNoise", {"Width": "=G2.PW", "Height": "=G2.PH", "Detail": 3, "Contrast": 2.4,
                                 "XScale": "=G2.PW/(30*G2.Res)"}, (110, 80))
tool("SplatCov", "Custom", {
    "NumberIn1": "=Ctl.Splash", "NumberIn2": "=min(Ctl.Blood, 2)", "NumberIn3": "=G2.Res",
    "NumberIn4": "=((G2.BY + G2.TY)/2)/G2.PH",
    "Intermediate1": "1/(0.55 + 0.45*n1)",
    "Intermediate2": "max(0.3*getr2b(x, y), getr2b(x, y + 0.22))",
    "Intermediate3": "getr1b(0.5 + (x - 0.5)*i1, n4 + (y - n4)*i1)",
    "RedExpression": "if(n1 > 0, 1, 0)*min(1, n2*1.5)*min(1, max(0, (i3 - (0.96 - 0.12*min(1, i2*3)*min(n2, 1.5)))*20))*min(1, i2*6)",
    "GreenExpression": "0", "BlueExpression": "0", "AlphaExpression": "1",
    "Image1": ("SplatNoise", "Output"), "Image2": ("L2Near", "Output")}, (220, 80))
# drips: per column i1 (n1 px wide) with a hashed chance, width and length threshold; t: 0 at the letter, 1 at the tip
THR = "(0.05 + 0.8*%s^1.5)" % H("2.1")
DRIP_W = ("if(i4 < 0.86, n3*(1.6 + 2.6*{h4})*(1 - 0.4*i4 + 1.8*(1 - i4)^8 + 0.6*2.718^(0 - ((i4 - 0.86)/0.08)*((i4 - 0.86)/0.08))), "
          "n3*(1.6 + 2.6*{h4})*1.256*sqrt(max(0, 1 - ((i4 - 0.86)/0.14)^2)))").format(h4=H("4.7"))
DRIP = ("if({h1} > n2, 1, 0)*if(i3 > {thr}, 1, 0)*min(1, max(0, {w} - abs(x*w1 - i2) + 0.5))"
        ).format(h1=H("1.3"), thr=THR, w=DRIP_W)
FRONT = ("n6 - (n6 - n7 + 12*n3)*n4 + 6*n3*sin(x*w1*4/n3) + 4*n3*sin(x*w1*1.3/n3 + 97)"
         " - if({h1} > n2, 1, 0)*22*n3*{h5}*n4").format(h1=H("1.3"), h5=H("5.9"))
FLOOD = "min(1, max(0, (y*h1 - (%s))/(2*n3) + 0.5))" % FRONT
POOL = ("min(1, max(0, n8*n3*(8 + 3*sin(x*w1*2.9/n3 + 57) + 2.5*sin(x*w1*0.75/n3))"
        "*sqrt(max(0, 1 - ((x*w1 - w1/2)/max(p1x*max(n8, 0.001)^0.4, 1))^2)) - abs(y*h1 - n7 - 1.5*n3*n8) + 0.5))")
tool("L2Cov", "Custom", {
    "NumberIn1": "=10*G2.Res", "NumberIn2": "=1 - 0.5*min(Ctl.Blood, 2)", "NumberIn3": "=G2.Res",
    "NumberIn4": "=Ctl.Flood", "NumberIn5": "=G2.BY", "NumberIn6": "=G2.TY", "NumberIn7": "=G2.RY",
    "NumberIn8": "=Ctl.Pool*min(Ctl.Blood, 1.5)",
    "PointIn1": "=Point(0.53*(G2.X2 - G2.X1), 0)",
    "Intermediate1": "floor(x*w1/n1)",
    "Intermediate2": "(i1 + 0.3 + 0.4*%s)*n1" % H("3.3"),
    "Intermediate3": "getr2b(i2/w1, y)",
    # (not gated: lossless gates on the drips, flood and pool measured 50% slower here, 0.43 -> 0.64 s a frame)
    "Intermediate4": "min(1, max(0, (1 - i3)/(1 - %s)))" % THR,
    "RedExpression": "max(max(r1, %s)*%s, max(%s, r3))" % (DRIP, FLOOD, POOL),
    "GreenExpression": "r1*%s" % FLOOD, "BlueExpression": "0", "AlphaExpression": "1",
    "Image1": ("L2Full", "Output"), "Image2": ("L2Smear", "Output"), "Image3": ("SplatCov", "Output")}, (330, 0))
tool("L2CovBlur", "Blur", {"Input": ("L2Cov", "Output"), "XBlurSize": "=G2.Res*2", "ClippingMode": ("fuid", "None")},
     (440, 0))
tool("L2Height", "Blur", {"Input": ("L2Cov", "Output"), "XBlurSize": "=G2.Res*6", "ClippingMode": ("fuid", "None")},
     (440, 40))
# gate: L2Color's coverage i1 > 0 (Image2 = its Image1)
relief("L2Relief", "L2Height", "G2", "5", 1.6, 60, 0.6, 0.7, "1.8", 0.1, 0.26, (550, 40), "r2 > 0.3", "L2CovBlur")
tool("L2Color", "Custom", {
    "NumberIn1": "=Ctl.AccentRed", "NumberIn2": "=Ctl.AccentGreen", "NumberIn3": "=Ctl.AccentBlue",
    "NumberIn4": "=1 - 0.5*Ctl.EOut", "NumberIn5": "=1.4*Ctl.Glisten",
    "Intermediate1": "min(max((r1 - 0.3)/0.22, 0), 1)*n4",
    "Intermediate2": "0.75 + 0.45*(1 - min(1, r3*1.2))",
    "RedExpression": "i1*(n1*(0.28 + 1.1*r2)*i2 + n5*g2)",
    "GreenExpression": "i1*(n2*(0.28 + 1.1*r2)*i2 + 0.8*n5*g2)",
    "BlueExpression": "i1*(n3*(0.28 + 1.1*r2)*i2 + 0.75*n5*g2)",
    "AlphaExpression": "i1",
    "Image1": ("L2CovBlur", "Output"), "Image2": ("L2Relief", "Output"), "Image3": ("L2Height", "Output")}, (660, 0))
tool("L2Place", "Merge", {"Background": ("Blank", "Output"), "Foreground": ("L2Color", "Output"),
                          "Center": "=Point(G2.CX0 + G2.CW/2, G2.CY0 + G2.CH/2)", "PerformDepthMerge": 0}, (770, 0))
tool("L2Alpha", "BitmapMask", {"Image": ("L2Place", "Output"), "Channel": ("fuid", "Alpha")}, (770, 40))
tool("L2TextAlpha", "BitmapMask", {"Image": ("L2Text", "Output"), "Channel": ("fuid", "Alpha")}, (110, -80))
tool("L2Mould", "Background", {"EffectMask": ("L2TextAlpha", "Mask"), "UseFrameFormatSettings": 1, "Width": 1920,
                               "Height": 1080, "TopLeftRed": 0.035, "TopLeftGreen": 0.012, "TopLeftBlue": 0.01},
     (220, -80))
shadow("L2", "L2Alpha", "1", "1", "6*Ctl.EOut", (880, 40))
tool("L2Blur", "Blur", {"Input": ("L2Place", "Output"), "XBlurSize": "=G2.Res*7*Ctl.EOut"}, (880, 0))
tool("L2MouldBlur", "Blur", {"Input": ("L2Mould", "Output"), "XBlurSize": "=G2.Res*(1 + 6*(1 - Ctl.Mold) + 7*Ctl.EOut)"},
     (330, -80))
tool("L2MA", "Merge", {"EffectMask": ("L2Mask", "Mask"), "Background": ("Blank", "Output"),
                       "Foreground": ("L2ShadowBlur", "Output"), "Blend": "=Ctl.ShadowOpacity*Ctl.Mold*(1 - Ctl.EOut)",
                       "PerformDepthMerge": 0}, (1100, 40))
tool("L2MB", "Merge", {"EffectMask": ("L2Mask", "Mask"), "Background": ("L2MA", "Output"),
                       "Foreground": ("L2MouldBlur", "Output"), "Blend": "=0.94*Ctl.Mold", "PerformDepthMerge": 0},
     (1155, 40))
tool("L2MC", "Merge", {"EffectMask": ("L2Mask", "Mask"), "Background": ("L2MB", "Output"),
                       "Foreground": ("L2Blur", "Output"), "PerformDepthMerge": 0}, (1210, 40))

# ======================================================================================================================
# Line 3: "v1.1 Release Trailer" in Cinzel, settling in (tracking closes, blur clears).
d = dict(TEXT_COMMON)
d.update({"StyledText": "v1.1 Release Trailer", "Font": "Cinzel", "Style": "Bold", "Size": 0.034,
          "CharacterSpacing": "=Ctl.L3Track + 0.4*(1 - Ctl.E3)^2", "Center": "=Point(0.5, Ctl.YR - Ctl.Gap3)",
          "VerticalTopCenterBottom": -1, "Red1": 0.84, "Green1": 0.8, "Blue1": 0.72,
          "Enabled2": 1, "Name2": "Outline", "Thickness2": 0.06, "OutsideOnly2": 1, "Red2": 0.01, "Green2": 0.005,
          "Blue2": 0.005, "Alpha2": 0.7, "Softness2": 2})
tool("L3Text", "TextPlus", d, (0, 460))
tool("L3Alpha", "BitmapMask", {"Image": ("L3Text", "Output"), "Channel": ("fuid", "Alpha")}, (110, 500))
shadow("L3", "L3Alpha", "0.5", "0.5", "Ctl.Intensity*8*(1 - Ctl.E3)^2 + 5*Ctl.EOut", (220, 500))
tool("L3Blur", "Blur", {"Input": ("L3Text", "Output"),
                        "XBlurSize": "=G1.Res*(Ctl.Intensity*9*(1 - Ctl.E3)^2 + 5*Ctl.EOut)"}, (110, 460))

# ======================================================================================================================
# the stack (on transparent), the impact shake, over the scrim/backdrop
tool("StackA", "Merge", {"Background": ("Blank", "Output"), "Foreground": ("L1Slam", "Output"),
                         "PerformDepthMerge": 0}, (1320, -60))
tool("StackB", "Merge", {"EffectMask": ("L2Mask", "Mask"), "Background": ("StackA", "Output"),
                         "Foreground": ("RulePlace", "Output"), "Blend": "=0.95*Ctl.Flick", "PerformDepthMerge": 0},
     (1375, -60))
tool("StackC", "Merge", {"Background": ("StackB", "Output"), "Foreground": ("L2MC", "Output"),
                         "PerformDepthMerge": 0}, (1430, -60))
tool("StackD", "Merge", {"EffectMask": ("L3Mask", "Mask"), "Background": ("StackC", "Output"),
                         "Foreground": ("L3ShadowBlur", "Output"), "Blend": "=Ctl.ShadowOpacity*(1 - Ctl.EOut)",
                         "PerformDepthMerge": 0}, (1485, -60))
tool("StackE", "Merge", {"EffectMask": ("L3Mask", "Mask"), "Background": ("StackD", "Output"),
                         "Foreground": ("L3Blur", "Output"), "Blend": "=0.5 + 0.5*Ctl.Flick", "PerformDepthMerge": 0},
     (1540, -60))
tool("Shake", "Transform", {"Input": ("StackE", "Output"),
                            "Center": "=Point(0.5 + 0.005*Ctl.Shake*sin(Ctl.LT*2.9), 0.5 + 0.007*Ctl.Shake*cos(Ctl.LT*3.7))"},
     (1595, -60))
tool("MergeAll", "Merge", {"Background": ("MergeScrim", "Output"), "Foreground": ("Shake", "Output"),
                           "PerformDepthMerge": 0}, (1650, -60))

# ======================================================================================================================
# Speed: Custom tools are per-pixel interpreted (the macro's main cost) and their `if` is lazy, so each colour tool's
# channels are worked out only where they can differ from what they are elsewhere: (condition, value elsewhere) per
# channel, each "elsewhere" being exactly what the full expression gives there (lossless; see each note).
GATES = {
    # coverage i1 is 0 where alpha is 0 (the text is premultiplied): every channel is 0 there
    # (the grime and shade intermediates are used only there)
    "L1Color": ("a1 > 0", {"RedExpression": "0", "GreenExpression": "0", "BlueExpression": "0",
                           "Intermediate2": "0", "Intermediate3": "0"}),
    # i1 = 0 at or below 0.3: every channel is i1*(...)
    "L2Color": ("r1 > 0.3", {"RedExpression": "0", "GreenExpression": "0", "BlueExpression": "0",
                             "AlphaExpression": "0", "Intermediate2": "0"}),
    # no tail (i3 <= 0): the max() keeps the O as it is
    "L1Tail": ("i3 > 0", {"RedExpression": "r1", "GreenExpression": "g1", "BlueExpression": "b1",
                          "AlphaExpression": "a1"}),
    # before the splash the factor if(n1 > 0, 1, 0) is 0
    "SplatCov": ("n1 > 0", {"RedExpression": "0"}),
    # outside the blade (i3 = 0, so i4 = 0) and with no sparks (n7 = 0) every term is 0
    "RuleCol": ("i3 + n7 > 0", {"RedExpression": "0", "GreenExpression": "0", "BlueExpression": "0",
                                "AlphaExpression": "0"}),
}
for _name, _typ, _inputs, _pos, _extra in TOOLS:
    if _name in GATES:
        _cond, _else = GATES[_name]
        for _ch, _v in _else.items():
            _inputs[_ch] = "if(%s, %s, %s)" % (_cond, _inputs[_ch], _v)

# ======================================================================================================================
LBL = {c[0]: c[1] for c in CTL}
DEF = {c[0]: c[3] for c in CTL}
out = ["{", "\tTools = ordered() {", "\t\tQVRIntroTitle = MacroOperator {", "\t\t\tCtrlWZoom = false,",
       "\t\t\tNameSet = true,", "\t\t\tCustomData = {",
       '\t\t\t\tHelpPage = "Misc/trailer/titles/README.md (Quake VR repository)",', "\t\t\t},",
       "\t\t\tInputs = ordered() {"]
for name, op, src, label, ex in PUB:
    lines = ['SourceOp = "%s",' % op, 'Source = "%s",' % src]
    if label:
        lines.append("Name = %s," % q(label))
    if "cg" in ex:
        lines.append("ControlGroup = %d," % ex["cg"])
    if "def" in ex:
        lines.append("Default = %r," % ex["def"])
    elif op == "Ctl" and src in DEF and not isinstance(DEF[src], str):
        lines.append("Default = %r," % DEF[src])
    out.append("\t\t\t\t%s = InstanceInput {" % name)
    out += ["\t\t\t\t\t" + l for l in lines]
    out.append("\t\t\t\t},")
out += ["\t\t\t},", "\t\t\tOutputs = {", "\t\t\t\tMainOutput1 = InstanceOutput {", '\t\t\t\t\tSourceOp = "MergeAll",',
        '\t\t\t\t\tSource = "Output",', "\t\t\t\t}", "\t\t\t},", "\t\t\tViewInfo = GroupInfo { Pos = { 0, 0 } },",
        "\t\t\tTools = ordered() {"]
for name, typ, inputs, pos, extra in TOOLS:
    out.append("\t\t\t\t%s = %s {" % (name, typ))
    out.append("\t\t\t\t\tNameSet = true,")
    out.append("\t\t\t\t\tInputs = {")
    for k, v in inputs.items():
        out.append("\t\t\t\t\t\t" + inp(k, v))
    out.append("\t\t\t\t\t},")
    out.append("\t\t\t\t\tViewInfo = OperatorInfo { Pos = { %d, %d } }," % pos)
    if extra:
        out.append("\t\t\t\t\t" + extra)
    out.append("\t\t\t\t},")
out += ["\t\t\t},", "\t\t},", "\t},", '\tActiveTool = "QVRIntroTitle"', "}", ""]
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(out))
print("wrote", OUT, len(TOOLS), "tools")
