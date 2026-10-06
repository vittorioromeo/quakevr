# vtune_attr.py <topdown.tsv> [stack-regex] [rows]: VTune's top-down tree (vtune -report top-down -format csv
# -csv-delimiter tab) as self time charged to the nearest of the game's own functions above it (malloc, free, glm, za
# and the allocation counter's wrappers skipped), then the leaves under each. stack-regex keeps only the stacks that
# pass through a matching frame (e.g. "qvr::hull"). docs/vr-port/PROFILING_2026-10.md.
import sys, re, collections
f = open(sys.argv[1], encoding='utf-8', errors='replace')
hdr = f.readline().rstrip('\n').split('\t')
iself = hdr.index('CPU Time:Self')
target = re.compile(sys.argv[2]) if len(sys.argv) > 2 else None
skip = re.compile(r"^(`anonymous namespace'::(allocate|deallocate|account)$|operator new|operator delete|new$|delete$|glm::|za::|std::|operator|malloc|free|_|Rtl|Nt|func@|\[|Wait|Switch|memcpy|memset|memmove)")
stack = []; acc = collections.Counter(); leafacc = collections.Counter(); total = 0.0
for line in f:
    cols = line.rstrip('\n').split('\t')
    name = cols[0]; depth = len(name) - len(name.lstrip(' ')); name = name.strip()
    del stack[depth:]; stack.append(name)
    try: s = float(cols[iself])
    except: continue
    if s <= 0: continue
    if target and not any(target.search(x) for x in stack): continue
    total += s
    anc = next((x for x in reversed(stack) if not skip.match(x) and 'lambda' not in x and 'FunctionRef' not in x), '?')
    acc[anc] += s
    leafacc[(name[:50], anc[:70])] += s
print('total %.2f s' % total)
for k, v in acc.most_common(int(sys.argv[3]) if len(sys.argv) > 3 else 25): print('%7.2f  %s' % (v, k[:150]))
print('--- leaf <- ancestor')
for k, v in leafacc.most_common(25): print('%7.2f  %s <- %s' % (v, k[0], k[1]))
