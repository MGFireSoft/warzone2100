# Runs the `targeting` movement bench scenario under every target mode, over several spawn arrangements and seeds,
# and prints how long the attackers take to kill the enemy cannon tanks. Paths are those of this machine.
import os, re, subprocess, statistics, sys, json

EXE = r"F:\wz-build\src\warzone2100.exe"
MODES = ["default", "by type", "smart"]
SEEDS = [None, 111]
ARRANGEMENTS = range(9)

results = {m: [] for m in MODES}
for mode_index, mode in enumerate(MODES):
    for seed in SEEDS:
        for arr in ARRANGEMENTS:
            env = dict(os.environ, WZ_TARGETMODE=str(mode_index))
            cmd = [EXE, "--movementbench=targeting", f"--movementarrangement={arr}"]
            if seed is not None:
                cmd.append(f"--movementseed={seed}")
            out = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace", cwd=r"F:\wz-build\src", timeout=300).stdout
            t_first_tank = t_tanks = t_all = None
            hp_final = hp_at_tanks = None
            strong_final = None
            for m in re.finditer(r"BATTLE t=(\d+) strong=(\d+) health=(\d+) tanks=(\d+) cyborgs=(\d+) structures=(\d+)", out):
                t, s_, hp, tk, cy, st = (int(x) for x in m.groups())
                hp_final = hp
                if tk == 0 and hp_at_tanks is None:
                    hp_at_tanks = hp
                strong_final = s_
                if tk < 8 and t_first_tank is None:
                    t_first_tank = t
                if tk == 0 and t_tanks is None:
                    t_tanks = t
                if tk == 0 and cy == 0 and t_all is None:
                    t_all = t
            results[mode].append({"seed": seed, "arr": arr, "first": t_first_tank, "tanks": t_tanks, "all": t_all, "strong": strong_final, "hp": hp_at_tanks if hp_at_tanks is not None else hp_final})
    print("done", mode, flush=True)

json.dump(results, open(r"F:\tmp\targeting_results.json", "w"))

def stats(values):
    ok = [v for v in values if v is not None]
    if not ok:
        return "never"
    return f"{statistics.mean(ok)/1000:6.1f}s (min {min(ok)/1000:.0f}, max {max(ok)/1000:.0f}, {len(ok)}/{len(values)} finished)"

print()
print(f"{'mode':11} | first cannon tank dies | all cannon tanks dead | attackers' health left (of 2400)")
for mode in MODES:
    r = results[mode]
    hp = statistics.mean(x["hp"] for x in r if x["hp"] is not None)
    print(f"{mode:11} | {stats([x['first'] for x in r]):22} | {stats([x['tanks'] for x in r]):22} | {hp:.0f}")
