# Counts, for each kind of weapon, what kind of target it chose, per target mode, in the `assault` scenario.
# Needs the WZ_TARGETLOG environment variable, which this sets. Paths are those of this machine.
import os, re, subprocess, collections

EXE = r"F:\wz-build\src\warzone2100.exe"
MODES = ["default", "by type", "smart"]
ROLES = ["other", "anti-tank", "anti-personnel", "all-rounder", "bunker buster", "flamer", "artillery", "incendiary artillery"]
KINDS = ["artillery unit", "cyborg", "hover tank", "wheeled tank", "halftrack tank", "tracked tank", "support unit", "other unit",
         "artillery structure", "defence", "bunker", "wall", "other structure"]
pat = re.compile(r"TGT role=(\d+) kind=(\d+)")
counts = {m: collections.defaultdict(collections.Counter) for m in MODES}
for mi, mode in enumerate(MODES):
    for arr in range(6):
        env = dict(os.environ, WZ_TARGETMODE=str(mi), WZ_TARGETLOG="1")
        out = subprocess.run([EXE, "--movementbench=assault", f"--movementarrangement={arr}"], env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, errors="replace", cwd=r"F:\wz-build\src", timeout=900).stdout
        for m in pat.finditer(out):
            counts[mode][int(m.group(1))][int(m.group(2))] += 1

def share(mode, role, kinds):
    c = counts[mode][role]
    total = sum(c.values())
    if not total:
        return "   - "
    return f"{100*sum(c[k] for k in kinds)/total:4.0f}%"

groups = {
    "tank": [2, 3, 4, 5],
    "cyborg": [1],
    "artillery (unit or pit)": [0, 8],
    "defence": [9],
    "bunker": [10],
    "wall/other structure": [11, 12],
    "support/other unit": [6, 7],
}
print("share of the targets each kind of weapon chose, by mode (all six arrangements together)")
for role in [1, 2, 3, 4, 5, 6, 7]:
    print(f"\n{ROLES[role]}   (choices: " + ", ".join(f"{m} {sum(counts[m][role].values())}" for m in MODES) + ")")
    print(f"  {'target':24}" + "".join(f"{m:>10}" for m in MODES))
    for name, kinds in groups.items():
        print(f"  {name:24}" + "".join(f"{share(m, role, kinds):>10}" for m in MODES))
