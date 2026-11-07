#!/usr/bin/env python3
"""TASK-1705: the pump in the v2 home schematic is drawn as the circulation-pump symbol.

Field report (tjfs, 2026-11-07): the pump looked like "a manic mushroom", a disc with cut-out
wedges. The common pump symbol is a circle with an inscribed triangle whose apex points along
the flow. This test reads the real src/OTGW-firmware/data/v2.html and v2.css and checks:

  P1  the pump element (#pumpBlades) is one straight-edged triangle, no arc wedges;
  P2  the triangle is inscribed in the pump circle;
  P3  its apex points the same way as the return-pipe flow arrow (.arrowCold);
  P4  the pump element is not rotated by an animation (a spinning arrow loses its direction);
  P5  its colours come from design tokens (var(--...)), so light and dark theme both work.

OTGW_V2_HTML / OTGW_V2_CSS point at other copies (e.g. `git show <rev>:...`) for the old-vs-fix
run; the old wedge drawing fails P1.

Run: python tests/test_webui_pump_icon.py
"""
import math
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DATA = REPO / "src" / "OTGW-firmware" / "data"
HTML = Path(os.environ.get("OTGW_V2_HTML", DATA / "v2.html")).read_text(encoding="utf-8")
CSS = Path(os.environ.get("OTGW_V2_CSS", DATA / "v2.css")).read_text(encoding="utf-8")


def attr(tag, name):
    m = re.search(r'\b' + name + r'="([^"]*)"', tag)
    return m.group(1) if m else None


def pump_parts():
    m = re.search(r'<circle[^>]*class="pump-body"[^>]*/>', HTML)
    assert m, "pump-body circle not found in v2.html"
    circle = m.group(0)
    m = re.search(r'<path[^>]*id="pumpBlades"[^>]*/>', HTML, re.S)
    assert m, "#pumpBlades path not found in v2.html"
    return circle, m.group(0)


def path_points(d):
    """Vertices of an absolute M/L/Z path; raises on any other command."""
    cmds = re.findall(r'[A-Za-z]', d)
    other = sorted(set(c for c in cmds if c not in "MLZ"))
    if other:
        raise AssertionError(f"pump path uses commands {other} (arcs/curves), not a straight-edged triangle: {d!r}")
    if cmds.count("M") != 1:
        raise AssertionError(f"pump path has {cmds.count('M')} subpaths, expected 1: {d!r}")
    nums = [float(n) for n in re.findall(r'-?\d+(?:\.\d+)?', d)]
    return list(zip(nums[0::2], nums[1::2]))


def css_rules_for(selector_re):
    body = []
    for sel, decl in re.findall(r'([^{}]+)\{([^{}]*)\}', CSS):
        if re.search(selector_re, sel):
            body.append((sel.strip(), decl))
    return body


class PumpIcon(unittest.TestCase):
    def setUp(self):
        self.circle, self.path = pump_parts()
        self.cx = float(attr(self.circle, "cx"))
        self.cy = float(attr(self.circle, "cy"))
        self.r = float(attr(self.circle, "r"))

    def triangle(self):
        pts = path_points(attr(self.path, "d"))
        self.assertEqual(len(pts), 3, f"pump path has {len(pts)} vertices, expected a triangle: {pts}")
        return pts

    def test_p1_triangle(self):
        self.triangle()

    def test_p2_inscribed(self):
        for x, y in self.triangle():
            dist = math.hypot(x - self.cx, y - self.cy)
            self.assertLessEqual(dist, self.r, f"vertex ({x},{y}) lies outside the pump circle")
            self.assertGreaterEqual(dist, self.r * 0.8, f"vertex ({x},{y}) does not reach the circle edge")

    def test_p3_apex_follows_return_flow(self):
        pts = self.triangle()
        # the symbol is equilateral, so the apex is the vertex opposite the base that stands
        # across the pipe: the two base vertices share one x (vertical base on a horizontal pipe)
        txs = [p[0] for p in pts]
        apexes = [p for p in pts if txs.count(p[0]) == 1]
        self.assertEqual(len(apexes), 1, f"no single apex across a vertical base: {pts}")
        base_tx = next(x for x in txs if txs.count(x) == 2)
        ax, ay = apexes[0][0] - base_tx, apexes[0][1] - self.cy
        m = re.search(r'<polygon[^>]*class="[^"]*arrowCold[^"]*"[^>]*points="([^"]+)"', HTML)
        self.assertTrue(m, "return-pipe arrow (.arrowCold) not found")
        ap = [tuple(map(float, p.split(","))) for p in m.group(1).split()]
        # the arrow tip is the vertex whose x differs from the other two
        xs = [p[0] for p in ap]
        tip = next(p for p in ap if xs.count(p[0]) == 1)
        base_x = next(x for x in xs if xs.count(x) == 2)
        flow_x = tip[0] - base_x
        self.assertGreater(abs(ax), abs(ay), "pump apex does not point along the horizontal return pipe")
        self.assertGreater(ax * flow_x, 0, f"pump apex points {ax:+.1f} in x, return flow is {flow_x:+.1f}")

    def test_p4_not_rotated(self):
        classes = (attr(self.path, "class") or "").split()
        sel = r'#pumpBlades\b' + "".join(r'|\.' + re.escape(c) + r'\b' for c in classes)
        rules = css_rules_for(sel)
        self.assertTrue(rules, "no CSS rule styles the pump element")
        spinning = {m.group(1) for _, d in rules for m in re.finditer(r'animation:\s*([\w-]+)', d)}
        for name in spinning:
            kf = re.search(r'@keyframes\s+' + re.escape(name) + r'\s*\{(.*?)\}\s*\}', CSS, re.S)
            self.assertTrue(kf, f"keyframes {name} not found")
            self.assertNotIn("rotate", kf.group(1), f"pump element is rotated by @keyframes {name}")

    def test_p5_token_colours(self):
        classes = (attr(self.path, "class") or "").split()
        sel = r'#pumpBlades\b' + "".join(r'|\.' + re.escape(c) + r'\b' for c in classes)
        decls = " ".join(d for _, d in css_rules_for(sel)) + " " + self.path
        fills = re.findall(r'(?:fill|stroke)\s*[:=]\s*"?([^;"\s}]+)', decls)
        self.assertTrue(fills, "pump element has no fill/stroke rule")
        for v in fills:
            if v.replace(".", "").isdigit():
                continue  # stroke-width style numbers
            self.assertTrue(v.startswith("var(--"), f"pump colour {v!r} is not a design token")


if __name__ == "__main__":
    unittest.main(verbosity=2)
