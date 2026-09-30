"""Prints functions from qc.asm (fteqcc -Fwasm) without line numbers: python dump.py [name ...] (none: every t_*)."""
import re
import sys

t = open("qc.asm").read()
for m in re.finditer(r"\n(\S[^\n]*?(t_\w+)[^\n]*= asm\n\{\n(?:local[^\n]*\n)*)(.*?)\n\}", t, re.S):
    if not sys.argv[1:] or m.group(2) in sys.argv[1:]:
        print("==", m.group(2))
        print(re.sub(r"\s*/\*\d+\*/", "", m.group(3)).replace("\t\t", " ").replace("\t", " "))
