# Runs the `assault` movement bench scenario under every target mode, over several spawn arrangements and seeds,
# and prints how long the attackers take to destroy everything the defender has, and what they lose doing it.
# Paths are those of this machine.
import os, re, subprocess, statistics, json, math

EXE = r"F:\wz-build\src\warzone2100.exe"
MODES = ["default", "by type", "smart"]
SEEDS = [None, 111, 222]
ARRANGEMENTS = range(9)

pattern = re.compile(r"BATTLE t=(\d+) attacker=(\d+) health=(\d+) defender=(\d+) structures=(\d+) walls=(\d+)")
results = {m: [] for m in MODES}
for mode_index, mode in enumerate(MODES):
    for seed in SEEDS:
        for arr in ARRANGEMENTS:
            env = dict(os.environ, WZ_TARGETMODE=str(mode_index))
            cmd = [EXE, "--movementbench=assault", f"--movementarrangement={arr}"]
            if seed is not None:
                cmd.append(f"--movementseed={seed}")
            out = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                 errors="replace", cwd=r"F:\wz-build\src", timeout=900).stdout
            last = None
            t_units = t_clear = None
            at_clear = None
            for m in pattern.finditer(out):
                t, an, ah, dn, st, w = (int(x) for x in m.groups())
                last = (t, an, ah, dn, st, w)
                if dn == 0 and t_units is None:
                    t_units = t
                if dn == 0 and st == 0 and t_clear is None:
                    t_clear = t
                    at_clear = (an, ah)
            if last is None:
                continue
            t, an, ah, dn, st, w = last
            final = at_clear if at_clear is not None else (an, ah)
            results[mode].append({"arr": arr, "seed": seed, "t_units": t_units, "t_clear": t_clear,
                                  "an": final[0], "ah": final[1], "dn": dn, "st": st})
    print("done", mode, flush=True)

json.dump(results, open(r"F:\tmp\assault_results.json", "w"))

def stats(values):
    ok = [v for v in values if v is not None]
    if not ok:
        return "never"
    return f"{statistics.mean(ok)/1000:6.1f}s (min {min(ok)/1000:.0f}, max {max(ok)/1000:.0f})"

print()
print(f"{'mode':9} | defender units all dead      | everything but walls destroyed | attackers left: units / health%")
for mode in MODES:
    r = results[mode]
    n = len(r)
    done = sum(1 for x in r if x["t_clear"] is not None)
    print(f"{mode:9} | {stats([x['t_units'] for x in r]):28} | {stats([x['t_clear'] for x in r]):28} {done}/{n} | "
          f"{statistics.mean(x['an'] for x in r):4.1f} / {statistics.mean(x['ah'] for x in r)/30:5.1f}%")

base = {(x["arr"], x["seed"]): x for x in results["default"]}
print()
print("paired difference against default (negative time is faster, positive attackers is better)")
for mode in MODES[1:]:
    dt = [x["t_clear"] - base[(x["arr"], x["seed"])]["t_clear"] for x in results[mode]
          if x["t_clear"] is not None and base[(x["arr"], x["seed"])]["t_clear"] is not None]
    da = [x["an"] - base[(x["arr"], x["seed"])]["an"] for x in results[mode]]
    dh = [(x["ah"] - base[(x["arr"], x["seed"])]["ah"]) / 30 for x in results[mode]]
    def line(name, d, unit):
        se = statistics.stdev(d) / math.sqrt(len(d)) if len(d) > 1 else 0
        print(f"  {mode:8} {name:18} {statistics.mean(d):+7.1f}{unit}  +/- {se:.1f}  (better in {sum(1 for v in d if (v < 0 if name.startswith('time') else v > 0))}/{len(d)})")
    line("time to clear (s)", [v / 1000 for v in dt], "s")
    line("attackers left", da, "")
    line("health left (pp)", dh, "")
