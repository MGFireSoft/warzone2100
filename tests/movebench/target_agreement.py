# For each shot fired by the attacker in the `assault2` scenario, compares what the unit shoots at with what the target choice
# gives at that moment, per kind of weapon and target mode. Needs WZ_TARGETLOG, which this sets. Paths are those of this machine.
import collections, os, re, subprocess
EXE = r"F:\wz-build\src\warzone2100.exe"
ROLES = ["other", "anti-tank", "anti-personnel", "all-rounder", "bunker buster", "flamer", "artillery", "incendiary artillery"]
WALL = 11
pat = re.compile(r"SHOTCHK role=(\d+) actual=(-?\d+) best=(-?\d+) tierActual=(-?\d+) tierBest=(-?\d+) order=(\d+) action=(\d+) same=(\d)")
res = {}
for mode, name in ((0, "default"), (2, "smart")):
    cats = collections.defaultdict(collections.Counter)
    for arr in range(9):
        env = dict(os.environ, WZ_TARGETMODE=str(mode), WZ_TARGETLOG="1")
        out = subprocess.run([EXE, "--movementbench=assault2", f"--movementarrangement={arr}"], env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, errors="replace", cwd=r"F:\wz-build\src", timeout=900).stdout
        for m in pat.finditer(out):
            role, actual, best, ta, tb, order, action, same = (int(x) for x in m.groups())
            if same:
                c = "same target as the choice"
            elif actual == WALL:
                c = "a wall in the way"
            elif tb == -2:
                c = "choice finds nothing now (keeps its old target)"
            elif ta == -1:
                c = "something the mode forbids"
            elif ta == tb:
                c = "different target, same priority"
            elif ta < tb:
                c = "a better priority than the choice"
            else:
                c = "a worse priority than the choice"
            cats[role][c] += 1
            cats[0][c] += 1  # all roles
    res[name] = cats

order = ["same target as the choice", "different target, same priority", "a better priority than the choice",
         "a worse priority than the choice", "something the mode forbids", "a wall in the way",
         "choice finds nothing now (keeps its old target)"]
for name in ("default", "smart"):
    print(f"\n=== mode: {name}  (shots, and what the unit was shooting at compared with what the target choice gives at that moment)")
    for role in [0, 1, 2, 3, 4, 5, 6, 7]:
        c = res[name][role]
        total = sum(c.values())
        if not total:
            continue
        label = "ALL WEAPONS" if role == 0 else ROLES[role]
        print(f"  {label:22} {total:6} shots: " + ", ".join(f"{100*c[k]/total:.0f}% {k}" for k in order if c[k]))
