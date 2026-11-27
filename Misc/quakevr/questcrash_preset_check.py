"""questcrash_preset_check.py <es.log> <pc.log> -- checks questcrash_preset_test.sh's runs (see its header).
Prints PASS/FAIL lines and "questpreset: N passed, M failed"; exits 1 on a failure."""
import re
import sys


def sections(path):
    out, cur = {}, None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(r"^QP (\w+)", line)
            if m:
                cur = out.setdefault(m.group(1), {})
                continue
            m = re.match(r'^"(\w+)" is "([^"]*)"', line)
            if m and cur is not None:
                cur[m.group(1)] = m.group(2)
    return out


def main():
    es, pc = sections(sys.argv[1]), sections(sys.argv[2])
    es_text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
    passed = failed = 0

    def check(ok, what):
        nonlocal passed, failed
        print(("PASS " if ok else "FAIL ") + what)
        passed += ok
        failed += not ok

    budget = {"vr_render_scale": "0.9", "vr_xr_foveation": "3", "r_oit": "0", "vid_fsaa": "0", "vr_dynres": "1"}
    for name in ("standalone", "off", "low", "medium", "kept", "reverted"):
        s = es.get(name)
        check(s is not None, f"ES {name}: section printed")
        if s is None:
            continue
        bad = {k: s.get(k) for k, v in budget.items() if s.get(k) != v}
        check(not bad, f"ES {name}: the budget rows at Standalone's {bad or ''}")
    check(es.get("off", {}).get("vr_tonemap") == "0", "ES off: Quake (off)'s own look row set (vr_tonemap 0)")
    check(es.get("low", {}).get("vr_shadow_dlights") == "0", "ES low: Low's shadowed dynamic lights held at 0")
    check(es.get("medium", {}).get("vr_shadow_maplights") == "0", "ES medium: Medium's shadowed map lights held at 0")
    check(es.get("medium", {}).get("vr_normalmaps") == "1", "ES medium: Medium's look applied (vr_normalmaps 1)")
    check(es.get("kept", {}).get("vr_normalmaps") == "0", "ES kept: Low at 20 ms frames kept (vr_normalmaps 0)")
    check(es_text.count("put back as before the pick") == 1, "ES: exactly one pick put back (the 500 ms one)")
    check(es.get("reverted", {}).get("vr_normalmaps") == "0", "ES reverted: High put back to Low (vr_normalmaps 0)")
    o = es.get("oldcfg", {})
    check(o.get("r_oit") == "0" and o.get("vr_dynres") == "1" and o.get("vr_render_scale") in ("0.9", "0.75", "0.65")
          and o.get("vr_xr_foveation") in ("3", "1"),
          f"ES oldcfg: a pre-questcrash config's budget rows set at a Standalone level by vr_quest_check {o}")
    check(o.get("vr_normalmaps") == "0", "ES oldcfg: the look's rows kept (vr_normalmaps 0)")
    check("vr_quest_check: the config has the PC's values" in es_text, "ES oldcfg: the repair said on the console")
    p = pc.get("pcoff", {})
    check(p.get("vr_render_scale") not in (None, "0.9") and p.get("vr_xr_foveation") == "0",
          "PC: leaving Standalone puts its rows back as before it (not the budget)")
    print(f"questpreset: {passed} passed, {failed} failed")
    sys.exit(1 if failed else 0)


main()
