# Runs the `precision` movement bench scenario ten times under each target mode, keeps every shot and every hit,
# and writes a summary. The raw events of each run go to precision_<mode>_<run>.csv next to the summary.
# Paths are those of this machine.
import collections, csv, json, os, re, statistics, subprocess, sys

EXE = r"F:\wz-build\src\warzone2100.exe"
OUT = r"F:\wz-precision"
# the modes compared, by name, with the WZ_TARGETMODE number of each
MODE_NUMBERS = {"default": 0, "by type": 1, "smart": 2, "nearest": 3, "focus": 4, "threat focus": 5, "kill order": 6,
                "smart v2": 7, "smart v2 sensors": 8, "smart v2 artillery": 9}
# usage: precision_runs.py [scenario] [mode,mode,...]
SCENARIO = sys.argv[1] if len(sys.argv) > 1 else "precision"
MODES = sys.argv[2].split(",") if len(sys.argv) > 2 else ["default", "smart v2"]
# ten different starts: five spawn arrangements, each with two random seeds
RUNS = [(arr, seed) for seed in (None, 111) for arr in range(5)]

unit_re = re.compile(r"UNIT id=(\d+) side=(\w+) kind=(\w+) label=(.+?)\s*$")
battle_re = re.compile(r"BATTLE t=(\d+) attacker=(\d+) health=(\d+) defender=(\d+) health=(\d+) structures=(\d+) health=(\d+)")
fire_re = re.compile(r"FIRE t=(\d+) p=(\d+) src=(\d+) w=(\S+) tgt=(\d+) aim=(\d+)")
lost_re = re.compile(r"LOST t=(\d+) p=(\d+) src=(\d+) w=(\S+)")
hit_re = re.compile(r"HIT t=(\d+) p=(\d+) src=(\d+) w=(\S+) tgt=(\d+) tp=(\d+) dmg=(\d+) left=(\d+) killed=(\d)")
extra_re = re.compile(r"BATTLE t=(\d+) .* far=(\d+) sensors=(\d+)")
state_re = re.compile(r"STATE t=(\d+) (.*)$")
danger_re = re.compile(r"DANGER t=(\d+) p=0 tgt=(\d+) danger=([\d.]+) hp=(\d+)")
focus_re = re.compile(r"FOCUS t=(\d+) p=0 tgt=(\d+) threat=(-?\d+)(?: danger=([\d.]+) ttk=([\d.]+))?")

WEAPON_NAMES = {
    "MG4ROTARYMk1": "Assault Gun", "Rocket-BB": "Bunker Buster", "Rocket-HvyA-T": "Tank Killer", "Mortar2Mk1": "Bombard",
    "Cyb-Hvywpn-Mcannon": "Super Heavy-Gunner", "Cyb-Hvywpn-Acannon": "Super Auto-Cannon", "MG1Mk1": "Machinegun",
    "MG4ROTARY-Pillbox": "Assault Gun (hardpoint)", "Rocket-HvyA-T-Hardpoint": "Tank Killer (hardpoint)",
    "Cannon6TwinAslt": "Twin Assault Cannon", "Mortar1Mk1": "Mortar", "Howitzer03-Rot": "Hellstorm",
}


def weapon_name(w):
    return WEAPON_NAMES.get(w, w)


os.makedirs(OUT, exist_ok=True)
runs = []
for mode in MODES:
    mode_index = MODE_NUMBERS[mode]
    for run, (arr, seed) in enumerate(RUNS):
        env = dict(os.environ, WZ_TARGETMODE=str(mode_index), WZ_FIRELOG="1")
        cmd = [EXE, f"--movementbench={SCENARIO}", f"--movementarrangement={arr}"] + ([f"--movementseed={seed}"] if seed else [])
        out = subprocess.run(cmd, env=env,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace",
                             cwd=r"F:\wz-build\src", timeout=900).stdout
        units = {}
        events = []
        battles = []
        last_state = None
        focus = []
        danger = {}
        extras = []
        for line in out.splitlines():
            m = danger_re.search(line)
            if m:
                danger[int(m.group(2))] = (float(m.group(3)), int(m.group(4)))
                continue
            m = focus_re.search(line)
            if m:
                focus.append((int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4), m.group(5)))
                continue
            m = unit_re.search(line)
            if m:
                units[int(m.group(1))] = {"side": m.group(2), "kind": m.group(3), "label": m.group(4)}
                continue
            m = battle_re.search(line)
            if m:
                battles.append(tuple(int(x) for x in m.groups()))
                e = extra_re.search(line)
                if e:
                    extras.append((int(e.group(1)), int(e.group(2)), int(e.group(3))))
                continue
            m = fire_re.search(line)
            if m:
                t, p, src, w, tgt, aim = m.groups()
                # a shot already decided to miss has no target, so what it was aimed at is the shooter's own target
                events.append({"t": int(t), "event": "fire", "player": int(p), "shooter": int(src), "weapon": w,
                               "target": int(aim), "on_target": int(tgt) != 0})
                continue
            m = lost_re.search(line)
            if m:
                t, p, src, w = m.groups()
                events.append({"t": int(t), "event": "lost", "player": int(p), "shooter": int(src), "weapon": w, "target": 0})
                continue
            m = hit_re.search(line)
            if m:
                t, p, src, w, tgt, tp, dmg, left, killed = m.groups()
                events.append({"t": int(t), "event": "kill" if killed == "1" else "hit", "player": int(p), "shooter": int(src),
                               "weapon": w, "target": int(tgt), "damage": int(dmg), "left": int(left)})
                continue
            m = state_re.search(line)
            if m:
                last_state = (int(m.group(1)), {int(k): int(v) for k, v in (x.split(":") for x in m.group(2).split())})

        with open(os.path.join(OUT, f"{SCENARIO}_{mode.replace(' ', '')}_{run}.csv"), "w", newline="", encoding="utf-8") as f:
            wr = csv.writer(f)
            wr.writerow(["time_s", "event", "side", "shooter_id", "shooter", "weapon", "target_id", "target", "damage", "hp_left", "decided_miss"])
            for e in events:
                shooter = units.get(e["shooter"], {})
                target = units.get(e["target"], {})
                wr.writerow([e["t"] / 1000, e["event"], "attacker" if e["player"] == 0 else "defender", e["shooter"],
                             shooter.get("label", "?"), weapon_name(e["weapon"]), e["target"] or "",
                             target.get("label", "") if e["target"] else "", e.get("damage", ""), e.get("left", ""),
                             ("yes" if not e["on_target"] else "no") if e["event"] == "fire" else ""])

        t_done = next((b[0] for b in battles if b[3] == 0 and b[5] == 0), None)
        t_units = next((b[0] for b in battles if b[3] == 0), None)
        end = next((b for b in battles if b[3] == 0 and b[5] == 0), battles[-1] if battles else None)
        deaths = {e["target"]: (e["t"], e["shooter"], e["weapon"]) for e in events if e["event"] == "kill"}
        t_far = next((x[0] for x in extras if x[1] == 0), None)
        t_sensors = next((x[0] for x in extras if x[2] == 0), None)
        damage_taken = sum(e["damage"] for e in events if e["player"] == 1 and e["event"] in ("hit", "kill"))
        runs.append({"t_far": t_far, "t_sensors": t_sensors, "damage_taken": damage_taken, "has_extras": bool(extras),
                     "mode": mode, "run": run, "units": units, "events": events, "t_done": t_done, "t_units": t_units,
                     "attackers_left": end[1] if end else None, "attacker_health": end[2] if end else None, "deaths": deaths,
                     "last_state": last_state, "focus": focus, "danger": danger})
    print("done", mode, flush=True)


def mean(xs):
    xs = [x for x in xs if x is not None]
    return statistics.mean(xs) if xs else float("nan")


lines = []
p = lines.append
p(f"# Scenario {SCENARIO}: {len(RUNS)} runs per target mode\n")
p("Attacker: 12 Python tracked tanks (3 Assault Gun, 3 Bunker Buster, 3 Tank Killer, 3 Bombard), target mode under test.")
p("Defender, always default: Assault Gun Hardpoint, Tank Killer Hardpoint, Hardened Sensor Tower, 2 Twin Assault Cannon Bunkers, "
  "2 Super Heavy-Gunner and 2 Super Auto-Cannon Cyborgs, 3 Machinegun Python tanks, 2 Mortar Pits.\n")

p("## Outcome\n")
def spread(xs):
    xs = [x for x in xs if x is not None]
    return statistics.stdev(xs) if len(xs) > 1 else 0.0


def concentration(run):
    # for each second, the share of the attackers' shots that went at that second's most shot-at target
    by_second = collections.defaultdict(collections.Counter)
    for e in run["events"]:
        if e["player"] == 0 and e["event"] == "fire" and e["target"]:
            by_second[e["t"] // 1000][e["target"]] += 1
    total = sum(sum(c.values()) for c in by_second.values())
    top = sum(c.most_common(1)[0][1] for c in by_second.values() if c)
    return 100 * top / total if total else float("nan")


p("| mode | all defender units dead | everything destroyed (each run) | attackers left | attackers' health (100 per unit at the start) | fire on one target |")
p("|---|---|---|---|---|---|")
for mode in MODES:
    r = [x for x in runs if x["mode"] == mode]
    times = ", ".join(f"{x['t_done']/1000:.0f}" if x["t_done"] else "-" for x in r)
    p(f"| {mode} | {mean([x['t_units'] for x in r])/1000:.1f} s | {mean([x['t_done'] for x in r])/1000:.1f} s "
      f"+/- {spread([x['t_done'] for x in r])/1000:.1f} ({times}) | "
      f"{mean([x['attackers_left'] for x in r]):.1f} | {mean([x['attacker_health'] for x in r]):.0f} "
      f"+/- {spread([x['attacker_health'] for x in r]):.0f} | {mean([concentration(x) for x in r]):.0f}% |")
p("")
p("Fire on one target: in each second, the share of the attackers' shots that went at that second's most shot-at target.")

p("\n## Damage taken, and the defender's artillery and sensors\n")
p("| mode | damage taken per battle | paired against default | sensors all down | far Hellstorms all down |")
p("|---|---|---|---|---|")
base_runs = {x["run"]: x for x in runs if x["mode"] == "default"}
for mode in MODES:
    r = [x for x in runs if x["mode"] == mode]
    diffs = [x["damage_taken"] - base_runs[x["run"]]["damage_taken"] for x in r if x["run"] in base_runs]
    paired = (f"{statistics.mean(diffs):+.0f} +/- {statistics.stdev(diffs)/len(diffs)**0.5:.0f}, less in "
              f"{sum(1 for d in diffs if d < 0)}/{len(diffs)}") if mode != "default" and len(diffs) > 1 else "-"
    sens = [x["t_sensors"] for x in r]
    far = [x["t_far"] for x in r]
    sens_txt = f"{mean(sens)/1000:.0f} s ({sum(1 for v in sens if v)}/{len(r)})" if any(x["has_extras"] for x in r) else "-"
    far_txt = f"{mean(far)/1000:.0f} s ({sum(1 for v in far if v)}/{len(r)})" if any(x["has_extras"] for x in r) else "-"
    p(f"| {mode} | {mean([x['damage_taken'] for x in r]):.0f} | {paired} | {sens_txt} | {far_txt} |")

p("\n## What each attacker weapon fired at (all runs together)\n")
for mode in MODES:
    r = [x for x in runs if x["mode"] == mode]
    p(f"### {mode}\n")
    p("| weapon | shots | missed or target gone | hits | damage dealt | kills | aimed at (share of shots) |")
    p("|---|---|---|---|---|---|---|")
    for w in ["MG4ROTARYMk1", "Rocket-BB", "Rocket-HvyA-T", "Mortar2Mk1", "Howitzer03-Rot"]:
        fired = collections.Counter()
        n_fired = n_lost = n_hits = dmg = kills = 0
        for x in r:
            for e in x["events"]:
                if e["player"] != 0 or e["weapon"] != w:
                    continue
                if e["event"] == "fire":
                    n_fired += 1
                    fired[x["units"].get(e["target"], {}).get("label", "unknown")] += 1
                elif e["event"] == "lost":
                    n_lost += 1
                else:
                    n_hits += 1
                    dmg += e["damage"]
                    kills += e["event"] == "kill"
        share = ", ".join(f"{k} {100*v/n_fired:.0f}%" for k, v in fired.most_common()) if n_fired else "-"
        p(f"| {weapon_name(w)} | {n_fired} | {100*n_lost/max(n_fired,1):.0f}% | {n_hits} | {dmg} | {kills} | {share} |")
    p("")

p("## When each defender fell, and to what (average over the runs it fell in; the counts are kills over all runs)\n")
p("| defender | " + " | ".join(MODES) + " |")
p("|---|" + "---|" * len(MODES))
labels = []
for x in runs:
    for uid, u in x["units"].items():
        if u["side"] == "defender" and u["label"] not in labels:
            labels.append(u["label"])
for label in labels:
    cells = []
    for mode in MODES:
        times = []
        killers = collections.Counter()
        for x in [y for y in runs if y["mode"] == mode]:
            for uid, u in x["units"].items():
                if u["label"] == label and uid in x["deaths"]:
                    t, src, w = x["deaths"][uid]
                    times.append(t)
                    killers[weapon_name(w)] += 1
        cells.append(f"{mean(times)/1000:.0f} s, " + ", ".join(f"{k} {v}" for k, v in killers.most_common()) if times else "survived")
    p(f"| {label} | " + " | ".join(cells) + " |")

for x in runs:
    if x["mode"] == "unit threat" and x["run"] == 0 and x["danger"]:
        p("\n## Danger of each defender as the unit threat mode reckons it (first run, when first seen)\n")
        p("| defender | danger (damage per second to the attackers) | hitpoints |")
        p("|---|---|---|")
        for tgt, (d, hp) in sorted(x["danger"].items(), key=lambda kv: -kv[1][0]):
            p(f"| {x['units'].get(tgt, {}).get('label', tgt)} | {d:.2f} | {hp} |")
    if x["mode"] == "threat focus" and x["run"] == 0 and x["focus"]:
        p("\n## Order in which the threat focus mode turned on the defence (first run)\n")
        p("| time | target | threat (damage per minute against the attackers) |")
        p("|---|---|---|")
        for t, tgt, threat, danger, ttk in x["focus"]:
            p(f"| {t/1000:.1f} s | {x['units'].get(tgt, {}).get('label', tgt)} | {threat} |")
    if x["mode"] == "kill order" and x["run"] == 0 and x["focus"]:
        p("\n## Order in which the kill order mode turned on the defence (first run)\n")
        p("| time | target | danger (damage per second to the attackers) | time to kill (s) | priority = danger / time to kill |")
        p("|---|---|---|---|---|")
        for t, tgt, threat, danger, ttk in x["focus"]:
            p(f"| {t/1000:.1f} s | {x['units'].get(tgt, {}).get('label', tgt)} | {danger} | {ttk} | {threat/1000:.1f} |")

p("\n## Damage the defence did to the attackers (sum over all runs)\n")
p("| mode | defender weapon | hits | damage |")
p("|---|---|---|---|")
for mode in MODES:
    c = collections.Counter()
    h = collections.Counter()
    for x in [y for y in runs if y["mode"] == mode]:
        for e in x["events"]:
            if e["player"] == 1 and e["event"] in ("hit", "kill"):
                c[weapon_name(e["weapon"])] += e["damage"]
                h[weapon_name(e["weapon"])] += 1
    for w, d in c.most_common():
        p(f"| {mode} | {w} | {h[w]} | {d} |")

report = "\n".join(lines)
open(os.path.join(OUT, f"{SCENARIO}_summary.md"), "w", encoding="utf-8").write(report)
print(report)
