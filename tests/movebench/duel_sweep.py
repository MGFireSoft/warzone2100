# Runs the `duel` movement bench scenario under every target mode and prints who wins, with what is left on each side.
# Paths are those of this machine.
import os, re, subprocess, statistics, json

EXE = r"F:\wz-build\src\warzone2100.exe"
MODES = ["default", "by type", "smart"]
SEEDS = [None, 111]
ARRANGEMENTS = range(9)
A0, D0 = 3000.0, 2200.0   # starting health sums

pattern = re.compile(r"BATTLE t=(\d+) attacker=(\d+) health=(\d+) defender=(\d+) health=(\d+) towers=(\d+)")
results = {m: [] for m in MODES}
for mode_index, mode in enumerate(MODES):
    for seed in SEEDS:
        for arr in ARRANGEMENTS:
            env = dict(os.environ, WZ_TARGETMODE=str(mode_index))
            cmd = [EXE, "--movementbench=duel", f"--movementarrangement={arr}"]
            if seed is not None:
                cmd.append(f"--movementseed={seed}")
            out = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                 errors="replace", cwd=r"F:\wz-build\src", timeout=600).stdout
            last = None
            t_end = None
            for m in pattern.finditer(out):
                t, an, ah, dn, dh, tw = (int(x) for x in m.groups())
                last = (t, an, ah, dn, dh, tw)
                if t_end is None and (an == 0 or dn == 0):
                    t_end = t
            if last is None:
                continue
            t, an, ah, dn, dh, tw = last
            if dn == 0 and an > 0:
                winner = "attacker"
            elif an == 0 and dn > 0:
                winner = "defender"
            elif an == 0 and dn == 0:
                winner = "draw"
            else:
                winner = "attacker" if ah / A0 > dh / D0 else "defender"
                winner += " (unfinished)"
            results[mode].append({"arr": arr, "seed": seed, "winner": winner, "t_end": t_end, "t_last": t,
                                  "an": an, "ah": ah, "dn": dn, "dh": dh, "tw": tw})
    print("done", mode, flush=True)

json.dump(results, open(r"F:\tmp\duel_results.json", "w"))

print()
print(f"{'mode':11} | attacker wins | decisive time  | attacker left: units / health% | defender left: units / health% | advantage")
for mode in MODES:
    r = results[mode]
    n = len(r)
    wins = sum(1 for x in r if x["winner"].startswith("attacker"))
    dec = [x["t_end"] for x in r if x["t_end"] is not None]
    adv = statistics.mean((x["ah"] / A0 - x["dh"] / D0) * 100 for x in r)
    print(f"{mode:11} | {wins:2}/{n:<2}         | "
          f"{(statistics.mean(dec)/1000 if dec else float('nan')):5.0f}s ({len(dec)}/{n}) | "
          f"{statistics.mean(x['an'] for x in r):5.1f} / {statistics.mean(x['ah'] for x in r)/A0*100:5.1f}%            | "
          f"{statistics.mean(x['dn'] for x in r):5.1f} / {statistics.mean(x['dh'] for x in r)/D0*100:5.1f}%            | {adv:+6.1f}")
