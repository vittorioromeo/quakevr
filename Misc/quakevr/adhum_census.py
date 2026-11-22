# adhum_census.py <classname,classname,...>: counts those classnames placed in the .bsp files of AD's paks and Quoth's
# paks, read in place (docs/vr-port/ARMED_MONSTERS.md, "Audit 2"). AD_DIR, QUOTH_DIR: the folders (defaults: the kit's).
import struct, sys, os, re, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import quakepak

SETS = {
    "AD": os.environ.get("AD_DIR", "C:/OHWorkspace/qvr-kit/qbase/ad/"),
    "Quoth": os.environ.get("QUOTH_DIR", "C:/OHWorkspace/qvr-kit/bases/dagger/qbase/quoth/"),
}
WANT = sys.argv[1].split(",")
for label, d in SETS.items():
    count = collections.Counter()
    maps = collections.defaultdict(set)
    for p in quakepak.game_paks(d):
        for name, data in quakepak.read_pak(p).items():
            if not name.endswith(".bsp") or len(data) < 12:
                continue
            ver = struct.unpack_from("<i", data, 0)[0]
            off, ln = struct.unpack_from("<ii", data, 4)
            ents = data[off:off + ln].decode("latin-1", "replace")
            for cn in re.findall(r'"classname"\s*"([^"]+)"', ents):
                if cn in WANT:
                    count[cn] += 1
                    maps[cn].add(name[5:-4])
    for cn in WANT:
        if count[cn]:
            print("%s %s %d on %d maps" % (label, cn, count[cn], len(maps[cn])))
