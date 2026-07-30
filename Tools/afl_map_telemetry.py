#!/usr/bin/env python3
# AFL MAP TELEMETRY READOUT -- offline parser for the s6 data-science loop (AFL-0213 / Arena_01_DESIGN.md s11).
#
# Reads a UE log for AFL_TELEMETRY lines, aggregates the s6 metrics, renders the KILL + TRAVERSAL heatmaps with
# the map's power positions overlaid + OUT-OF-WINDOW flagged, and prints a PASS/FAIL line per s11 window. The
# readout's job: make a GEOMETRY problem jump out (a hot power position rendered red, a dead zone highlighted)
# rather than buried in a number.
#
# REUSABLE for every map: only the MAP CONFIG block below (bounds + features + windows) is per-map.
# Deps: numpy + matplotlib (pip install numpy matplotlib). Scalars compute without matplotlib; heatmaps need it.
# Usage: python afl_map_telemetry.py [path\to\Bag_Man.log]   (defaults to the newest Saved\Logs\*.log)

import re, sys, glob, os, math
from datetime import datetime

# ============================== MAP CONFIG (per-map; swap this block) ==============================
ARENA_01 = {
    "name": "Arena_01",
    "bounds_cm": {"x": (-3025, 3025), "y": (-3925, 3925)},     # the imported blockout extent (the 100x-verified size)
    # (label, x_cm, y_cm, radius_cm) -- power positions + extract + spawns: overlaid + used for kill-share regions
    "features": [
        ("Mid Tower / Extract", 0,      0,    900),
        ("A Overlook",          -2200,  0,    700),
        ("B Ramp",               2200,  0,    700),
        ("Spawn S",              0,     -3500, 600),
        ("Spawn N",              0,      3500, 600),
    ],
    "kill_share_regions": ["Mid Tower / Extract", "A Overlook", "B Ramp"],  # the <=35% rule applies to power positions
    "windows": {                       # (lo, hi) inclusive bands from Arena_01_DESIGN.md s11
        "ttfc_s":       (8.0, 15.0),   # median TTFC (proxy: round-start -> first elimination; first-KILL upper-bounds TTFC)
        "kill_share":   (0.0, 0.35),   # any single power-position kill share <= 35%
        "contest_rate": (0.40, 0.70),  # extract contest rate
        "hold_deny":    (0.40, 0.60),  # hold-vs-deny ~ 50/50 (+-10)
        "side_balance": (0.45, 0.55),  # team-slot win-rate within +-5% (first-order; swap accounting is approximate)
    },
    "deadzone_floor_frac": 0.05,       # a playable heatmap bin below 5% of the median occupied bin = a dead zone
}
CONFIG = ARENA_01
# ==================================================================================================

TS_RE = re.compile(r'^\[([\d]{4}\.[\d.]+-[\d.:]+)\]')          # UE log timestamp prefix: [2026.06.26-14.30.45:123]
EV_RE = re.compile(r'AFL_TELEMETRY:\s+(\w+)\s*(.*?)\s*$')
KV_RE = re.compile(r'(\w+)=(-?[\w.]+)')

def parse_ts(s):
    try:
        d, t = s.split('-'); hms, ms = t.rsplit(':', 1)
        Y, Mo, Da = d.split('.'); H, Mi, S = hms.split('.')
        return datetime(int(Y), int(Mo), int(Da), int(H), int(Mi), int(S), int(ms) * 1000).timestamp()
    except Exception:
        return None

def newest_log():
    cands = glob.glob(os.path.join("Saved", "Logs", "*.log"))
    return max(cands, key=os.path.getmtime) if cands else None

def parse_log(path):
    ev = []
    with open(path, "r", errors="ignore") as f:
        for line in f:
            m = EV_RE.search(line)
            if not m:
                continue
            d = {k: v for k, v in KV_RE.findall(m.group(2))}
            d["_ev"] = m.group(1)
            mt = TS_RE.match(line); d["_t"] = parse_ts(mt.group(1)) if mt else None
            ev.append(d)
    return ev

def fxy(d):  # world x/y as floats (cm)
    try: return float(d["x"]), float(d["y"])
    except Exception: return None

# ---------------------------------- aggregate ----------------------------------
def aggregate(ev):
    A = {"kills": [], "traverse": [], "round_starts": [], "rounds": [], "extracts": []}
    for e in ev:
        k = e["_ev"]
        if k == "afl_elimination" and fxy(e):
            A["kills"].append((fxy(e), e.get("_t")))
        elif k == "afl_traverse" and fxy(e):
            A["traverse"].append(fxy(e))
        elif k == "afl_round_start":
            A["round_starts"].append((int(e.get("round", 0)), e.get("_t")))
        elif k == "afl_round_resolved":
            A["rounds"].append((int(e.get("round", 0)), int(e.get("team", -1)), e.get("reason", "?")))
        elif k == "afl_extract_contest":
            A["extracts"].append(("contest", int(e.get("contested", 0))))
        elif k == "afl_extract_outcome":
            A["extracts"].append(("outcome", int(e.get("success", 0))))
    return A

def region_of(x, y):
    for (lab, fx, fy, r) in CONFIG["features"]:
        if (x - fx) ** 2 + (y - fy) ** 2 <= r * r:
            return lab
    return None

# ---------------------------------- metrics ----------------------------------
def metrics(A):
    M = {}
    # TTFC: per round, first-elimination timestamp - round_start timestamp (proxy = first-KILL; upper-bounds TTFC).
    # Bucket each kill into the round whose start is the greatest <= the kill time; keep the earliest per round.
    starts = {rnd: t for rnd, t in A["round_starts"] if t is not None}
    sorted_starts = sorted((t, rnd) for rnd, t in starts.items())
    first_kill = {}
    for (_, t) in A["kills"]:
        if t is None: continue
        cand = [(st, rnd) for st, rnd in sorted_starts if st <= t]
        if not cand: continue
        st, rnd = cand[-1]
        if rnd not in first_kill or t < first_kill[rnd]:
            first_kill[rnd] = t
    ttfc = [t - starts[rnd] for rnd, t in first_kill.items()]
    M["ttfc_list"] = sorted(ttfc)
    M["ttfc_median"] = (M["ttfc_list"][len(ttfc)//2] if ttfc else None)

    # kill share per power-position region
    total_k = len(A["kills"]); share = {}
    if total_k:
        counts = {}
        for ((x, y), _t) in A["kills"]:
            reg = region_of(x, y)
            if reg: counts[reg] = counts.get(reg, 0) + 1
        for reg in CONFIG["kill_share_regions"]:
            share[reg] = counts.get(reg, 0) / total_k
    M["kill_share"] = share
    M["kill_share_max"] = (max(share.values()) if share else 0.0)

    # extract contest rate + hold-vs-deny (of contested, how many the channeler still banked = success)
    contests = [v for (k, v) in A["extracts"] if k == "contest"]
    outcomes = [v for (k, v) in A["extracts"] if k == "outcome"]
    M["contest_rate"] = (sum(contests) / len(contests)) if contests else None
    M["hold_deny"] = (sum(outcomes) / len(outcomes)) if outcomes else None   # success-rate as the hold proxy

    # side balance: team-slot-0 win-rate of decisive rounds (reason != Replay/team==-1)
    decisive = [t for (_, t, reason) in A["rounds"] if t >= 0 and reason.lower() != "replay"]
    if decisive:
        slot0 = decisive[0]   # treat the first decisive winner's id as "slot 0" reference
        M["side_balance"] = sum(1 for t in decisive if t == slot0) / len(decisive)
    else:
        M["side_balance"] = None
    M["decisive_rounds"] = len(decisive)
    M["total_kills"] = total_k
    M["total_traverse"] = len(A["traverse"])
    return M

def verdict(val, win, fmt="{:.2f}"):
    if val is None: return "NO DATA", "?"
    lo, hi = win
    ok = lo <= val <= hi
    return ("PASS" if ok else "FAIL"), fmt.format(val)

# ---------------------------------- render ----------------------------------
def render_heatmaps(A, M):
    try:
        import numpy as np
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from matplotlib.patches import Circle
    except Exception as ex:
        print("[heatmap] matplotlib/numpy not available (%s) -- scalars above are still valid; "
              "pip install numpy matplotlib for the PNGs." % ex)
        return

    bx, by = CONFIG["bounds_cm"]["x"], CONFIG["bounds_cm"]["y"]
    NB = 48
    def panel(ax, pts, title, deadzones=False):
        ax.set_title(title); ax.set_xlim(bx); ax.set_ylim(by); ax.set_aspect("equal")
        if pts:
            xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
            h, xe, ye = np.histogram2d(xs, ys, bins=NB, range=[bx, by])
            ax.imshow(h.T, origin="lower", extent=[bx[0], bx[1], by[0], by[1]],
                      cmap="inferno", aspect="equal", alpha=0.9)
            if deadzones:
                occ = h[h > 0]
                if occ.size:
                    floor = CONFIG["deadzone_floor_frac"] * float(np.median(occ))
                    dz = (h <= floor)
                    xc = (xe[:-1] + xe[1:]) / 2; yc = (ye[:-1] + ye[1:]) / 2
                    for i in range(NB):
                        for j in range(NB):
                            if dz[i, j]:
                                ax.add_patch(plt.Rectangle((xe[i], ye[j]), xe[1]-xe[0], ye[1]-ye[0],
                                             fill=False, edgecolor="cyan", lw=0.4, alpha=0.5))
        # overlay features + flag over-window kill share
        for (lab, fx, fy, r) in CONFIG["features"]:
            hot = (lab in M["kill_share"] and M["kill_share"][lab] > CONFIG["windows"]["kill_share"][1])
            col = "red" if hot else "white"
            ax.add_patch(Circle((fx, fy), r, fill=False, edgecolor=col, lw=1.6 if hot else 1.0, ls="--"))
            tag = lab
            if lab in M["kill_share"]:
                tag += "  %d%%" % round(M["kill_share"][lab] * 100)
                if hot: tag += " OVERPOWERED"
            ax.text(fx, fy + r + 80, tag, color=col, ha="center", fontsize=8, weight="bold" if hot else "normal")
        ax.set_xlabel("X cm"); ax.set_ylabel("Y cm")

    fig, axes = plt.subplots(1, 2, figsize=(15, 9))
    panel(axes[0], [p for (p, _t) in A["kills"]],
          "KILL DENSITY (n=%d)  [power positions: white ok, RED = >35%% share]" % M["total_kills"])
    panel(axes[1], A["traverse"],
          "TRAVERSAL DENSITY (n=%d)  [cyan = dead-zone bins < %d%% of median]" % (
              M["total_traverse"], int(CONFIG["deadzone_floor_frac"] * 100)), deadzones=True)
    fig.suptitle("%s -- s6 readout (read against the s11 windows)" % CONFIG["name"], fontsize=13)
    out = "%s_heatmaps.png" % CONFIG["name"]
    fig.savefig(out, dpi=110, bbox_inches="tight")
    print("[heatmap] wrote %s" % os.path.abspath(out))

# ---------------------------------- main ----------------------------------
def main():
    path = sys.argv[1] if len(sys.argv) > 1 else newest_log()
    if not path or not os.path.exists(path):
        print("No log found. Pass a path, or run from the project root with Saved\\Logs present."); return
    print("Reading: %s" % path)
    ev = parse_log(path)
    if not ev:
        print("No AFL_TELEMETRY lines found -- did a round run? (afl.Round.Start, then play; LogAFLCombat must be at Log)."); return
    A = aggregate(ev); M = metrics(A)
    w = CONFIG["windows"]
    print("\n==================  %s  s6 READOUT  ==================" % CONFIG["name"])
    print("  events: kills=%d traverse=%d rounds(decisive)=%d" % (M["total_kills"], M["total_traverse"], M["decisive_rounds"]))
    print("  ---------------------------------------------------------------")
    print("  metric            value     window         verdict")
    rows = [
        ("TTFC median (s)",  M["ttfc_median"], w["ttfc_s"],       "{:.1f}"),
        ("max kill-share",   M["kill_share_max"], w["kill_share"], "{:.2f}"),
        ("contest rate",     M["contest_rate"], w["contest_rate"], "{:.2f}"),
        ("hold-vs-deny",     M["hold_deny"],   w["hold_deny"],     "{:.2f}"),
        ("side balance",     M["side_balance"], w["side_balance"], "{:.2f}"),
    ]
    for (name, val, win, fmt) in rows:
        verd, sval = verdict(val, win, fmt)
        flag = "" if verd in ("PASS", "?") else "   <-- GEOMETRY TUNE"
        print("  %-17s %-9s [%.2f..%.2f]   %-5s%s" % (name, sval, win[0], win[1], verd, flag))
    if M["kill_share"]:
        print("  ---  kill share by power position (<=35%%):")
        for reg, s in sorted(M["kill_share"].items(), key=lambda kv: -kv[1]):
            print("        %-22s %3d%%%s" % (reg, round(s * 100), "  OVERPOWERED" if s > w["kill_share"][1] else ""))
    print("  ===============================================================\n")
    render_heatmaps(A, M)

if __name__ == "__main__":
    main()
