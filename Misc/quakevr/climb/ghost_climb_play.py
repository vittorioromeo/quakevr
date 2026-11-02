# ghost_climb_play.py <out.txt>: the mock play of ghost_climb_test.sh: both hands onto a large crate's lip and a hang
# (climb_plays.py's pull up, as prop_climb_test.sh's pc_hang).
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import climb_plays as c

L, t = c.pullup(1.52, 1.37, -0.69, -0.69)
c.write(sys.argv[1], L)
