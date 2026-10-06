import sys, armsweep as a
rows = a.parse(open(sys.argv[1]).read())
ms = [(n, a.metrics(s, v)) for n, s, v in rows if s == 'R']
ms.sort(key=lambda x: -abs(x[1]['out']))
for n, m in ms[:int(sys.argv[2]) if len(sys.argv) > 2 else 12]:
    print('%-16s out %5.1f back %5.1f down %5.1f ang %4.0f sw %4.0f str %5.2f tuck %.2f torso %5.1f W %s' % (n, m['out'], m['back'], m['down'], m['angle'], m['swivel'], m['strain'], m['tuck'], m['torso'], [round(x, 1) for x in m['W']]))
