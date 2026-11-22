# Writes Misc/quakevr/std_uses_allowlist.txt from `qvr_checks std_uses --list`, keeping each existing line's `# why`.
import subprocess, sys
import os
tree = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))).replace(chr(92), "/")
out = subprocess.run([tree + '/build/qvr_checks/qvr_checks.exe', '--tree', tree, 'std_uses', '--list'],
                     capture_output=True, text=True).stdout
path = tree + '/Misc/quakevr/std_uses_allowlist.txt'
why = {}
stars = []
head = []
for line in open(path, encoding='utf-8').read().splitlines():
    w = line.split('#', 1)[0].split()
    if len(w) == 3:
        if '#' in line:
            why[(w[0], w[1])] = line.split('#', 1)[1].strip()
        if w[1] == '*':
            stars.append(line)
    elif not w and not stars and not why:
        head.append(line)
rows = []
star_files = {s.split()[0] for s in stars}
code = incs = 0
for line in out.splitlines():
    w = line.split()
    if len(w) != 3 or w[0] == 'std_uses:':
        continue
    if w[1].startswith('<'):
        incs += int(w[2])
    else:
        code += int(w[2])
    if w[0] in star_files:
        continue
    r = '%s %s %s' % (w[0], w[1], w[2])
    if (w[0], w[1]) in why:
        r += '  # ' + why[(w[0], w[1])]
    rows.append(r)
with open(path, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(head).rstrip('\n') + '\n\n')
    for s in stars:
        f.write(s + '\n')
    for r in rows:
        f.write(r + '\n')
print('std:: uses', code, 'std includes', incs, 'rows', len(rows) + len(stars))
