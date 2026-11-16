"""Install the QVR Feature Title template (and its fonts) for DaVinci Resolve, for the current user (no admin).

    python Misc/trailer/titles/install_titles.py [--no-fonts] [--no-script] [--dry-run]

- The template goes to %APPDATA%/Blackmagic Design/DaVinci Resolve/Support/Fusion/Templates/Edit/Titles/: Resolve's
  Edit page lists it under Effects > Titles > Fusion Titles as "QVR Feature Title".
- The fonts (DpQuake by Dead Pete, free to distribute with its dpquake.txt; Pirata One, Cinzel, IM Fell English, SIL
  OFL; licences beside them in fonts/) go to
  %LOCALAPPDATA%/Microsoft/Windows/Fonts and are registered under HKCU (Windows' own per-user font install).
- A launcher, "QVR Apply Titles.py", goes to .../Support/Fusion/Scripts/Edit/: Workspace > Scripts > QVR Apply Titles
  runs apply_titles.py from this folder on titles.txt beside it (so it works without external scripting too).

Run it again after changing the template: it overwrites the installed copy. Resolve reads new templates and fonts when
it starts: restart it after the first install (an update to an already listed template is picked up by new titles).
"""
import argparse
import ctypes
import filecmp
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TEMPLATE = "QVR Feature Title.setting"
FONTS = [  # file, the name Windows registers it under
    ("dpquake_.ttf", "DpQuake (TrueType)"),
    ("PirataOne-Regular.ttf", "Pirata One (TrueType)"),
    ("Cinzel-wght.ttf", "Cinzel (TrueType)"),
    ("IMFellEnglish-Regular.ttf", "IM FELL English Roman (TrueType)"),
]


def resolve_support():
    return os.path.join(os.environ["APPDATA"], "Blackmagic Design", "DaVinci Resolve", "Support", "Fusion")


def copy(src, dst_dir, dry, name=None):
    dst = os.path.join(dst_dir, name or os.path.basename(src))
    same = os.path.isfile(dst) and filecmp.cmp(src, dst, shallow=False)
    print("  %s -> %s%s" % (os.path.basename(src), dst, " (already there)" if same else ""))
    if not dry and not same:  # an installed font in use (Resolve running) can't be overwritten: skip identical ones
        os.makedirs(dst_dir, exist_ok=True)
        shutil.copyfile(src, dst)
    return dst


def install_fonts(dry):
    import winreg
    fonts_dir = os.path.join(os.environ["LOCALAPPDATA"], "Microsoft", "Windows", "Fonts")
    key_path = r"Software\Microsoft\Windows NT\CurrentVersion\Fonts"
    for fname, reg_name in FONTS:
        dst = copy(os.path.join(HERE, "fonts", fname), fonts_dir, dry)
        if dry:
            continue
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER, key_path) as key:
            winreg.SetValueEx(key, reg_name, 0, winreg.REG_SZ, dst)
        ctypes.windll.gdi32.AddFontResourceW(dst)        # this session too, without logging off
    if not dry:
        HWND_BROADCAST, WM_FONTCHANGE, SMTO_ABORTIFHUNG = 0xFFFF, 0x001D, 0x0002
        ctypes.windll.user32.SendMessageTimeoutW(HWND_BROADCAST, WM_FONTCHANGE, 0, 0, SMTO_ABORTIFHUNG, 1000, None)


LAUNCHER = '''# QVR Apply Titles: installed by install_titles.py; runs the repository's apply_titles.py on its titles.txt.
import runpy, sys
sys.argv = ["apply_titles.py", %r]
runpy.run_path(%r, init_globals={"resolve": globals().get("resolve")}, run_name="__main__")
'''


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--no-fonts", action="store_true", help="only the template (and launcher)")
    ap.add_argument("--no-script", action="store_true", help="no Workspace > Scripts launcher")
    ap.add_argument("--dry-run", action="store_true", help="print what would be copied")
    a = ap.parse_args()
    if os.name != "nt":
        sys.exit("install_titles: Windows only (copy the .setting to Resolve's Fusion/Templates/Edit/Titles by hand)")
    sup = resolve_support()
    print("Template:")
    copy(os.path.join(HERE, TEMPLATE), os.path.join(sup, "Templates", "Edit", "Titles"), a.dry_run)
    if not a.no_script:
        dst = os.path.join(sup, "Scripts", "Edit", "QVR Apply Titles.py")
        print("Launcher:\n  -> %s" % dst)
        if not a.dry_run:
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, "w", encoding="utf-8") as f:
                f.write(LAUNCHER % (os.path.join(HERE, "titles.txt"), os.path.join(HERE, "apply_titles.py")))
    if not a.no_fonts:
        print("Fonts (current user):")
        install_fonts(a.dry_run)
    print("install_titles: done%s. Restart Resolve if it was running for the first install."
          % (" (dry run)" if a.dry_run else ""))


if __name__ == "__main__":
    main()
