#!/usr/bin/env python3
"""The VR menus' coverage check (ROUND21.md, "Menus reorganized").

`menu_vr dump` prints every page reached from the VR Settings through the pages' links: MDPAGE lines (number, depth,
the page linking it first, title, rows, settings), MDROW lines (page, title, kind, header above, label, cvar, page
opened) and MDLINKS lines (the links into each page). Run it in a game (a gun in the main hand, so that the weapon
pages are built the same way each time) and keep the console log:

    menu_coverage.py after.log                 the tree, rows a page, and the settings on more than one page
    menu_coverage.py before.log after.log      the same, and what moved: every setting and action of before must still
                                               be on a page reached (exit 1 if one is lost)

Kinds: H header, S slider, C cycle, A action, O a link to a page, I a line of text. A setting is its cvar; an action
its label (actions have no cvar). Links, headers and text are navigation, not options.
"""
import collections
import sys


def load(path):
    lines = []
    for raw in open(path, encoding='utf-8', errors='replace'):
        raw = raw.rstrip('\r\n')
        if raw.startswith('MD'):
            lines.append(raw)
        elif lines and raw and not raw.startswith('exit='):
            lines[-1] += raw  # a line the console wrapped (the space kept at the end of the first part)
    pages, rows, links = {}, [], {}
    for line in lines:
        f = line.split('|')
        if f[0] == 'MDPAGE':
            pages[int(f[1])] = {'depth': int(f[2]), 'from': int(f[3]), 'title': f[4], 'rows': int(f[5]), 'settings': int(f[6])}
        elif f[0] == 'MDROW' and len(f) >= 8:
            rows.append({'page': int(f[1]), 'title': f[2], 'kind': f[3], 'section': f[4], 'label': f[5], 'cvar': f[6],
                         'target': int(f[7])})
        elif f[0] == 'MDLINKS':
            links[int(f[1])] = {'title': f[2], 'in': int(f[3]), 'depth': int(f[4])}
    return pages, rows, links


def options(rows):
    """Every option (a cvar, or an action's label) and the pages it is on."""
    where = collections.defaultdict(list)
    for r in rows:
        if r['kind'] in 'SC' and r['cvar']:
            where['cvar ' + r['cvar']].append(r['title'])
        elif r['kind'] == 'A':
            where['action ' + r['label']].append(r['title'])
    return where


def tree(pages, rows):
    children = collections.defaultdict(list)
    for p, info in pages.items():
        if info['from'] >= 0:
            children[info['from']].append(p)
    order = {}
    for i, r in enumerate(rows):
        if r['kind'] == 'O':
            order.setdefault((r['page'], r['target']), i)
    out = []

    def walk(p, depth):
        info = pages[p]
        flag = '  <-- over 30 rows' if info['rows'] > 30 else ''
        out.append('%s%s [%d] %d rows, %d settings%s' % ('    ' * depth, info['title'], p, info['rows'], info['settings'], flag))
        for c in sorted(children[p], key=lambda c: order.get((p, c), 1 << 30)):
            walk(c, depth + 1)

    walk(0, 0)
    return out


def summary(path):
    pages, rows, links = load(path)
    print('== %s' % path)
    print('\n'.join(tree(pages, rows)))
    print('pages reached: %d, deepest: %d, over 30 rows: %s' % (
        len(pages), max(p['depth'] for p in pages.values()),
        ', '.join('%s (%d)' % (p['title'], p['rows']) for p in pages.values() if p['rows'] > 30) or 'none'))
    unreached = [l['title'] for l in links.values() if l['depth'] < 0]
    print('not reached by a link: %s' % (', '.join(unreached) or 'none'))
    return options(rows)


def main():
    if len(sys.argv) == 2:
        where = summary(sys.argv[1])
        multi = {k: v for k, v in where.items() if len(v) > 1}
        print('options on more than one page: %d' % len(multi))
        for k in sorted(multi):
            print('  %s: %s' % (k, ', '.join(multi[k])))
        return 0
    before = summary(sys.argv[1])
    after = summary(sys.argv[2])
    lost = sorted(k for k in before if k not in after)
    new = sorted(k for k in after if k not in before)
    moved = sorted(k for k in before if k in after and sorted(before[k]) != sorted(after[k]))
    print('\n== coverage: %d options before, %d after' % (len(before), len(after)))
    print('lost (on no page now): %d' % len(lost))
    for k in lost:
        print('  LOST %s (was on %s)' % (k, ', '.join(before[k])))
    print('new: %d' % len(new))
    for k in new:
        print('  NEW %s (on %s)' % (k, ', '.join(after[k])))
    fewer = [k for k in moved if len(after[k]) < len(before[k])]
    more = [k for k in moved if len(after[k]) > len(before[k])]
    print('on fewer pages (duplicates removed): %d' % len(fewer))
    for k in fewer:
        print('  %s: %s -> %s' % (k, ', '.join(before[k]), ', '.join(after[k])))
    print('on more pages: %d' % len(more))
    for k in more:
        print('  %s: %s -> %s' % (k, ', '.join(before[k]), ', '.join(after[k])))
    print('moved to other pages: %d' % (len(moved) - len(fewer) - len(more)))
    for k in moved:
        if k not in fewer and k not in more:
            print('  %s: %s -> %s' % (k, ', '.join(before[k]), ', '.join(after[k])))
    print('RESULT: %s' % ('FAIL, options lost' if lost else 'PASS, no option lost'))
    return 1 if lost else 0


if __name__ == '__main__':
    sys.exit(main())
