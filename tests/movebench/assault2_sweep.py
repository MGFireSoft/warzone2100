# Runs the `assault2` movement bench scenario under every target mode, over several spawn arrangements and seeds.
# For each mode it prints how long the attackers take to destroy the defender's tanks, bunkers and everything else, what
# they lose doing it, and how many of the shots they fire come down with nothing to hit (a miss, or the target already gone). Paths are those of this machine.
import collections, json, math, os, re, statistics, subprocess

EXE = r"F:\wz-build\src\warzone2100.exe"
MODES = ["default", "by type", "smart"]
SEEDS = [None, 111, 222]
ARRANGEMENTS = range(9)
ROLES = ["other", "anti-tank", "anti-personnel", "all-rounder", "bunker buster", "flamer", "artillery", "incendiary artillery"]

battle = re.compile(r"BATTLE t=(\d+) attacker=(\d+) health=(\d+) tanks=(\d+) cyborgs=(\d+) bunkers=(\d+) defences=(\d+) walls=(\d+)")
shot = re.compile(r"SHOT role=(\d+) lost=(\d)")

results = {m: [] for m in MODES}
for mode_index, mode in enumerate(MODES):
    for seed in SEEDS:
        for arr in ARRANGEMENTS:
            env = dict(os.environ, WZ_TARGETMODE=str(mode_index), WZ_TARGETLOG="1")
            cmd = [EXE, "--movementbench=assault2", f"--movementarrangement={arr}"]
            if seed is not None:
                cmd.append(f"--movementseed={seed}")
            out = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                 errors="replace", cwd=r"F:\wz-build\src", timeout=900).stdout
            t_tanks = t_bunkers = t_clear = None
            at_clear = None
            last = None
            for m in battle.finditer(out):
                t, an, ah, tk, cy, bk, df, wl = (int(x) for x in m.groups())
                last = (t, an, ah)
                if tk == 0 and t_tanks is None:
                    t_tanks = t
                if bk == 0 and t_bunkers is None:
                    t_bunkers = t
                if tk == 0 and cy == 0 and bk == 0 and df == 0 and t_clear is None:
                    t_clear = t
                    at_clear = (an, ah)
            if last is None:
                continue
            fired = collections.Counter()
            lost = collections.Counter()
            for m in shot.finditer(out):
                (lost if m.group(2) == "1" else fired)[int(m.group(1))] += 1
            final = at_clear if at_clear is not None else (last[1], last[2])
            results[mode].append({"arr": arr, "seed": seed, "t_tanks": t_tanks, "t_bunkers": t_bunkers, "t_clear": t_clear,
                                  "an": final[0], "ah": final[1], "fired": dict(fired), "lost": dict(lost)})
    print("done", mode, flush=True)

json.dump(results, open(r"F:\tmp\assault2_results.json", "w"))


def stats(values):
    ok = [v for v in values if v is not None]
    if not ok:
        return "never"
    return f"{statistics.mean(ok)/1000:6.1f}s"


print()
print(f"{'mode':9} | tanks dead | bunkers dead | all destroyed (done) | attackers left: units / health%  | shots lost")
for mode in MODES:
    r = results[mode]
    done = sum(1 for x in r if x["t_clear"] is not None)
    fired = sum(sum(x["fired"].values()) for x in r)
    lost = sum(sum(x["lost"].values()) for x in r)
    print(f"{mode:9} | {stats([x['t_tanks'] for x in r]):>10} | {stats([x['t_bunkers'] for x in r]):>12} | "
          f"{stats([x['t_clear'] for x in r]):>10} ({done:2}/{len(r)})   | "
          f"{statistics.mean(x['an'] for x in r):4.1f} / {statistics.mean(x['ah'] for x in r)/30:5.1f}%            | "
          f"{100*lost/max(fired, 1):4.1f}% of {fired}")

base = {(x["arr"], x["seed"]): x for x in results["default"]}
print()
print("paired difference against default")
for mode in MODES[1:]:
    def paired(key, scale=1.0):
        d = [(x[key] - base[(x["arr"], x["seed"])][key]) / scale for x in results[mode]
             if x[key] is not None and base[(x["arr"], x["seed"])][key] is not None]
        return d

    def line(name, d, unit):
        if len(d) < 2:
            return
        se = statistics.stdev(d) / math.sqrt(len(d))
        print(f"  {mode:8} {name:22} {statistics.mean(d):+7.1f}{unit}  +/- {se:.1f}")
    line("time, tanks dead (s)", paired("t_tanks", 1000.0), "s")
    line("time, bunkers dead (s)", paired("t_bunkers", 1000.0), "s")
    line("time, all destroyed (s)", paired("t_clear", 1000.0), "s")
    line("attackers left", paired("an"), "")
    line("health left (pp)", paired("ah", 30.0), "")

print()
print("shots that came down with nothing to hit, by kind of weapon (share of the shots fired)")
print(f"  {'weapon':22}" + "".join(f"{m:>12}" for m in MODES))
for role in range(1, 8):
    row = []
    for mode in MODES:
        f = sum(x["fired"].get(role, 0) for x in results[mode])
        l = sum(x["lost"].get(role, 0) for x in results[mode])
        row.append(f"{100*l/f:5.1f}% ({f})" if f else "-")
    print(f"  {ROLES[role]:22}" + "".join(f"{v:>12}" for v in row))
