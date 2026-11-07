#!/usr/bin/env python3
"""Plot an OTGW capture (TASK-1676): an overview chart and a zoom on runs.

Overview: one shared time axis, one quantity per panel (never two y-axes):
  1. status lanes from the slave status bits (heating, hot water, demand without flame),
     plus optional bands (for example an HW=0 window) and shaded periods (blocked hours);
  2. water temperatures: supply, return, hot-water tank, with the control setpoint as a line;
  3. optional extra series from CSV files (for example room sensors), one panel;
  4. outside temperature.
Numbered markers with a notes block come from a JSON file, so the story of the capture is
written down once and drawn on the chart.

Zoom: one panel per selected run, the demand-without-flame phase shaded, compressor or burner
on/off as dotted lines.

Usage:
  python -I capture_plot.py <capture.zip|.7z|folder> --out-dir DIR [--work DIR]
         [--extra "North room=north.csv" ...] [--shade 06:00-10:00 --shade 16:00-21:00]
         [--band "HW=0=2026-11-05T21:22:26/2026-11-06T04:31:01"] [--markers markers.json]
         [--zoom 1,3] [--title "..."]
markers.json: [{"n": 1, "t": "2026-11-05T21:13", "panel": "status|water|extra|outside",
                "y": 2, "text": "..."}]  ("y" for status: 2 heating, 1 hot water, 0 band lane)
Extra CSV: header row, first column ISO time (UTC "Z" or with offset), second column value.
"""
import argparse
import collections
import csv
import datetime as dt
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import capture_summary as cs  # noqa: E402

import matplotlib  # noqa: E402
matplotlib.use("Agg")
import matplotlib.dates as mdates  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.patches import Patch  # noqa: E402

# Reference palette of the dataviz skill, light mode, first three categorical slots only
# (they validate all-pairs); status lanes reuse them with alpha for the demand-only phase.
C = dict(blue="#2a78d6", orange="#eb6834", aqua="#1baf7a", mag="#d55181", ink="#0b0b0b", ink2="#52514e",
         muted="#8a8984", grid="#e6e5e0", surf="#fcfcfb", block="#ecebe6")
plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 9.5, "axes.edgecolor": C["grid"],
                     "axes.labelcolor": C["ink2"], "xtick.color": C["ink2"], "ytick.color": C["ink2"],
                     "axes.facecolor": C["surf"], "figure.facecolor": C["surf"]})


def per_min(s):
    b = collections.defaultdict(list)
    for t, v in s:
        b[t.replace(second=0, microsecond=0)].append(v)
    k = sorted(b)
    return k, [sorted(b[x])[len(b[x]) // 2] for x in k]


def read_extra(spec, offset):
    name, path = spec.split("=", 1)
    rows = []
    with open(path, encoding="utf-8-sig") as f:
        r = csv.reader(f)
        next(r, None)
        for row in r:
            if len(row) < 2:
                continue
            try:
                t = dt.datetime.fromisoformat(row[0].replace("Z", "+00:00"))
                if t.tzinfo:
                    t = t.astimezone(dt.timezone(offset)).replace(tzinfo=None)
                rows.append((t, float(row[1])))
            except ValueError:
                continue
    return name, rows


def tidy(ax):
    ax.grid(axis="y", color=C["grid"], lw=0.8)
    ax.set_axisbelow(True)
    for sp in ("top", "right"):
        ax.spines[sp].set_visible(False)


def mark(ax, t, y, n):
    ax.annotate(str(n), (t, y), fontsize=9, fontweight="bold", color="white", ha="center", va="center",
                bbox=dict(boxstyle="circle,pad=0.25", fc=C["ink"], ec="white", lw=1.5), zorder=10,
                annotation_clip=False)


def capture_offset(root, work):
    files = cs.frame_files(root, work)
    for line in open(files[0], encoding="utf-8", errors="replace"):
        if not line.startswith("#"):
            t = dt.datetime.fromisoformat(line.split("\t")[0])
            return t.utcoffset() or dt.timedelta(0)
    return dt.timedelta(0)


def main():
    ap = argparse.ArgumentParser(description="Overview and run-zoom charts of an OTGW capture")
    ap.add_argument("capture")
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--work")
    ap.add_argument("--extra", action="append", default=[], help='"Label=file.csv", repeatable')
    ap.add_argument("--shade", action="append", default=[], help="HH:MM-HH:MM daily shaded period, repeatable")
    ap.add_argument("--shade-label", default="Heating blocked")
    ap.add_argument("--band", action="append", default=[], help='"Label=ISO/ISO" shown in the bottom status lane')
    ap.add_argument("--markers", help="JSON file with numbered markers and their notes")
    ap.add_argument("--zoom", default="auto", help="run numbers to zoom (1-based, comma list), 'auto' or 'none'")
    ap.add_argument("--title", default="What switched, when, and what it did")
    a = ap.parse_args()

    src = Path(a.capture).resolve()
    work = Path(a.work) if a.work else src.with_name(src.name + ".work")
    work.mkdir(parents=True, exist_ok=True)
    out = Path(a.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    root = cs.unpack(src, work)
    frames = cs.read_frames(cs.frame_files(root, work))
    if not frames:
        sys.exit("no otframes.log found")
    offset = capture_offset(root, work)
    seg = cs.segments(frames)
    sup, ret, tank, outs = cs.series(frames, 25), cs.series(frames, 28), cs.series(frames, 26), cs.series(frames, 27)
    cset = [v for t, s, typ, m, hb, lb in frames if s == "T" and m == 1 and typ == 1 for v in [cs.f88(hb, lb)]]
    extras = [read_extra(e, offset) for e in a.extra]
    markers = json.load(open(a.markers, encoding="utf-8")) if a.markers else []
    x0, x1 = frames[0][0], frames[-1][0]

    panels = ["status", "water"] + (["extra"] if extras else []) + (["outside"] if outs else [])
    ratios = {"status": 1.15, "water": 2.6, "extra": 1.9, "outside": 1.1}
    notes_h = 0.19 * len(markers) + 0.15 if markers else 0.1
    fig, axs = plt.subplots(len(panels), 1, figsize=(13, 1.45 * sum(ratios[p] for p in panels) + notes_h + 1.3),
                            sharex=True, gridspec_kw={"height_ratios": [ratios[p] for p in panels], "hspace": 0.13})
    ax = dict(zip(panels, axs))
    shades = []
    day = x0.replace(hour=0, minute=0, second=0, microsecond=0)
    while day <= x1:
        for s in a.shade:
            h0, h1 = s.split("-")
            sa = day.replace(hour=int(h0[:2]), minute=int(h0[3:]))
            sb = day.replace(hour=int(h1[:2]), minute=int(h1[3:]))
            if sb > x0 and sa < x1:
                shades.append((max(sa, x0), min(sb, x1)))
        day += dt.timedelta(days=1)
    for axis in axs:
        for sa, sb in shades:
            axis.axvspan(sa, sb, color=C["block"], zorder=0, lw=0)
        tidy(axis)

    s_ax = ax["status"]
    for s, e, lb in seg:
        heat, dhw, flame = lb & 0x02, lb & 0x04, lb & 0x08
        if dhw:
            s_ax.broken_barh([(s, e - s)], (0.62, 0.76), color=C["mag"], alpha=1.0 if flame else 0.35, lw=0)
        elif heat or flame:
            s_ax.broken_barh([(s, e - s)], (1.62, 0.76), color=C["blue"], alpha=1.0 if flame else 0.35, lw=0)
        if flame and (e - s).total_seconds() >= 600:
            s_ax.text(s + (e - s) / 2, 2.42 if not dhw else 1.42, f"{(e - s).total_seconds() / 60:.0f} min",
                      ha="center", va="bottom", fontsize=8.5, color=C["ink2"])
    band_labels = []
    for b in a.band:
        label, span = b.split("=", 1) if b.count("=") >= 2 else ("band", b)
        if b.count("=") >= 2:
            label, span = b.rsplit("=", 1)
        ba, bb = (dt.datetime.fromisoformat(x) for x in span.split("/"))
        s_ax.broken_barh([(ba, bb - ba)], (-0.38, 0.76), color=C["orange"], alpha=0.55, lw=0)
        band_labels.append(label)
    s_ax.set_yticks([2, 1, 0])
    s_ax.set_yticklabels(["Heating", "Hot water", band_labels[0] if band_labels else ""])
    s_ax.set_ylim(-0.6, 2.75)
    s_ax.grid(False)
    s_ax.set_title(a.title, loc="left", fontsize=13, color=C["ink"], pad=44)
    s_ax.text(0, 1.3, f"OpenTherm capture {x0:%d %b %H:%M} to {x1:%d %b %H:%M} "
                      f"({(x1 - x0).total_seconds() / 3600:.1f} h). Numbers mark the moments listed below.",
              transform=s_ax.transAxes, fontsize=9, color=C["ink2"])
    leg = [Patch(color=C["blue"], label="Heating, flame/compressor on"),
           Patch(color=C["blue"], alpha=0.35, label="Heating demand, no flame"),
           Patch(color=C["mag"], label="Hot water, flame/compressor on")]
    if band_labels:
        leg.append(Patch(color=C["orange"], alpha=0.55, label=f"{band_labels[0]} active"))
    if shades:
        leg.append(Patch(color=C["block"], label=a.shade_label))
    s_ax.legend(handles=leg, loc="lower left", bbox_to_anchor=(0.0, 1.0), ncol=len(leg), frameon=False, fontsize=8.3)

    w_ax = ax["water"]
    for ser, col, lab in ((sup, C["blue"], "Supply"), (ret, C["orange"], "Return"), (tank, C["aqua"], "Hot-water tank")):
        if ser:
            k, v = per_min(ser)
            w_ax.plot(k, v, color=col, lw=1.6, label=lab)
    if cset and len(set(cset)) == 1:
        w_ax.axhline(cset[0], color=C["muted"], lw=1, ls=(0, (4, 3)))
        w_ax.text(x0, cset[0] + 0.8, f"control setpoint {cset[0]:g} °C (constant)", fontsize=8.3, color=C["ink2"])
    w_ax.set_ylabel("Water °C")
    w_ax.legend(loc="upper right", frameon=False, ncol=3, fontsize=9)

    if extras:
        e_ax = ax["extra"]
        for (name, rows), col in zip(extras, (C["blue"], C["orange"], C["aqua"])):
            e_ax.plot([t for t, _ in rows], [v for _, v in rows], color=col, lw=1.8, label=name)
        e_ax.set_ylabel("°C")
        e_ax.legend(loc="upper left", frameon=False, ncol=len(extras), fontsize=9)
    if outs:
        o_ax = ax["outside"]
        k, v = per_min(outs)
        o_ax.plot(k, v, color=C["ink2"], lw=1.5)
        o_ax.set_ylabel("Outside °C")
    axs[-1].xaxis.set_major_locator(mdates.AutoDateLocator(minticks=8, maxticks=16))
    axs[-1].xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
    axs[-1].set_xlim(x0, x1)

    for m in markers:
        if m.get("panel") in ax:
            mark(ax[m["panel"]], dt.datetime.fromisoformat(m["t"]), m["y"], m["n"])
    if markers:
        fig.text(0.13, 0.008, "\n".join(f"{m['n']}  {m['text']}" for m in markers), fontsize=8.8, color=C["ink"],
                 va="bottom", linespacing=1.45)
    fig.subplots_adjust(left=0.13, right=0.98, top=0.9, bottom=(notes_h / fig.get_figheight()) + 0.045)
    fig.savefig(out / "capture-overview.png", dpi=130)
    print(out / "capture-overview.png")

    runs = [(s, e, lb) for s, e, lb in seg if lb & 0x08]
    if a.zoom == "none" or not runs:
        return
    if a.zoom == "auto":
        picks = []
        for want in (lambda lb: not lb & 0x04, lambda lb: lb & 0x04):
            hit = next((i for i, r in enumerate(runs) if want(r[2])), None)
            if hit is not None:
                picks.append(hit)
    else:
        picks = [int(x) - 1 for x in a.zoom.split(",") if x.strip()]
    picks = [p for p in picks if 0 <= p < len(runs)]
    if not picks:
        return
    fig, zaxs = plt.subplots(1, len(picks), figsize=(6.5 * len(picks), 4.8), squeeze=False)
    for zax, p in zip(zaxs[0], picks):
        s, e, lb = runs[p]
        pre = next((q for q in seg if q[1] == s and not q[2] & 0x08 and q[2] & 0x06), None)
        a0 = (pre[0] if pre else s) - dt.timedelta(minutes=6)
        a1 = e + dt.timedelta(minutes=15)
        for ser, col, lab in ((sup, C["blue"], "Supply"), (ret, C["orange"], "Return"), (tank, C["aqua"], "Hot-water tank")):
            pts = [(t, v) for t, v in ser if a0 <= t <= a1]
            if pts:
                zax.plot([q[0] for q in pts], [q[1] for q in pts], color=col, lw=1.6, label=lab)
        if pre:
            zax.axvspan(pre[0], pre[1], color=C["blue"], alpha=0.15, lw=0)
        zax.axvline(s, color=C["ink2"], lw=0.8, ls=":")
        zax.axvline(e, color=C["ink2"], lw=0.8, ls=":")
        kind = "Hot-water run" if lb & 0x04 else "Heating run"
        zax.set_title(f"{kind} {p + 1}: flame/compressor {s:%H:%M}-{e:%H:%M} ({(e - s).total_seconds() / 60:.0f} min)",
                      loc="left", fontsize=11.5, color=C["ink"], pad=10)
        zax.set_ylabel("Water °C")
        zax.set_xlim(a0, a1)
        zax.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
        tidy(zax)
    handles, labels = zaxs[0][0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper right", frameon=False, fontsize=8.5, ncol=3)
    fig.text(0.05, 0.015, "Dotted lines: flame/compressor on and off. Light band: demand without flame. "
                          "Raw OpenTherm readings.", fontsize=8.3, color=C["ink2"])
    fig.subplots_adjust(left=0.06, right=0.99, top=0.84, bottom=0.15, wspace=0.16)
    fig.savefig(out / "capture-runs.png", dpi=130)
    print(out / "capture-runs.png")


if __name__ == "__main__":
    main()
