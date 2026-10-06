import sys, armsweep as a, contin as c
print("%-6s %-5s %8s %7s %8s %8s %9s %8s" % ("pos", "tag", "meanDev", "dev>60", "down<8", "maxSwiv", "medStrain", "meanTuck"))
for p in a.ORIENT_POS:
    for t in sys.argv[1:]:
        rows = [(n, s, v) for n, s, v in a.parse(open("sweep_%s_orient_%s.log" % (t, p)).read())]
        devs, downs, sw, st, tk = [], 0, [], [], []
        for n, s, v in rows:
            m = a.metrics(s, v)
            devs.append(c.swing_from_ideal(v[0:3], v[3:6], v[9:12], s))
            downs += m["down"] < 8; sw.append(abs(m["swivel"])); st.append(m["strain"]); tk.append(m["tuck"])
        st.sort()
        print("%-6s %-5s %8.0f %4d/%-3d %5d %8.0f %9.2f %8.2f" % (p, t, sum(devs) / len(devs), sum(d > 60 for d in devs), len(devs), downs, max(sw), st[len(st) // 2], sum(tk) / len(tk)))
