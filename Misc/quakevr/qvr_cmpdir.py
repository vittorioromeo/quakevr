#!/usr/bin/env python3
# qvr_cmpdir.py <folder> <reference folder> -- prints the names of the files in <folder> whose bytes differ from the
# same-named file in <reference folder> (space-separated; nothing when every one is the same). For the tests.
import filecmp
import os
import sys

a, b = sys.argv[1], sys.argv[2]
print(" ".join(f for f in sorted(os.listdir(a))
               if not os.path.exists(os.path.join(b, f)) or not filecmp.cmp(os.path.join(a, f), os.path.join(b, f), False)))
