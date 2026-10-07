"""Dimension of the Machine (MG1) entity coverage: check_mg3_entities.py on the owned rerelease/mg1/pak0.pak.

  python check_mg1_entities.py [--pak <rerelease/mg1/pak0.pak>] [--qc <QC dir>] [--map NAME] [--quiet]
                               [--expect-missing N] [--expect-placements N] [--expect-fields N]

Every MG1 map (story, hub, mgend, the seven Horde arenas) must resolve: `--expect-missing 0 --expect-fields 0`.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_mg3_entities  # noqa: E402

if __name__ == "__main__":
    sys.exit(check_mg3_entities.main("mg1"))
