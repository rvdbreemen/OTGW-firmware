"""Tiny helpers to write the agentic-architecture reference plates as plain SVG."""
from xml.sax.saxutils import escape

PAL = {
    # light palette of docs/process/agentic-setup.html (:root), plus a blue flame accent
    'bg': '#f4f6f7', 'surface': '#ffffff', 'line': '#d5dce2', 'fg': '#151a1f', 'muted': '#56616c',
    'flame': '#c2410c', 'flame_soft': '#fde7d9', 'cool': '#0e7490', 'cool_soft': '#d9eff4',
    'blue': '#1d4ed8', 'blue_soft': '#dbeafe', 'cyan': '#06b6d4', 'core': '#ffffff',
    'amber': '#a15c00', 'amber_soft': '#fdf0d5', 'ok': '#2f7d32', 'ok_soft': '#e3f1e4', 'crit': '#b42318', 'crit_soft': '#fde4e1',
}

FONT_D = "Archivo, 'Helvetica Neue', Arial, sans-serif"
FONT_B = "'Source Sans 3', 'Segoe UI', system-ui, sans-serif"
FONT_M = "'JetBrains Mono', ui-monospace, Consolas, monospace"


class Svg:
    def __init__(self, w, h, title, desc):
        self.w, self.h = w, h
        self.parts = []
        self.title, self.desc = title, desc

    def add(self, s):
        self.parts.append(s)

    def text(self, x, y, s, size=14, fill=None, weight=400, family='b', anchor='start', spacing=None, italic=False):
        fam = {'d': FONT_D, 'b': FONT_B, 'm': FONT_M}[family]
        extra = f' letter-spacing="{spacing}"' if spacing else ''
        st = ' font-style="italic"' if italic else ''
        self.add(f'<text x="{x}" y="{y}" font-family="{fam}" font-size="{size}" font-weight="{weight}" '
                 f'fill="{fill or PAL["fg"]}" text-anchor="{anchor}"{extra}{st}>{escape(s)}</text>')

    def zone_label(self, x, y, s):
        self.text(x, y, s, size=13, fill=PAL['flame'], weight=700, family='m', spacing='2')

    def rect(self, x, y, w, h, fill=None, stroke=None, sw=1.2, rx=6, dash=None, opacity=None):
        d = f' stroke-dasharray="{dash}"' if dash else ''
        o = f' opacity="{opacity}"' if opacity else ''
        sh = ' filter="url(#card)"' if fill and fill not in ('none', PAL['bg']) else ''
        self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill or "none"}" '
                 f'stroke="{stroke or "none"}" stroke-width="{sw}"{d}{o}{sh}/>')

    def path(self, d, stroke, sw=1.6, dash=None, start=None, end=None, fill='none', opacity=None):
        ds = f' stroke-dasharray="{dash}"' if dash else ''
        ms = f' marker-start="url(#{start})"' if start else ''
        me = f' marker-end="url(#{end})"' if end else ''
        o = f' opacity="{opacity}"' if opacity else ''
        self.add(f'<path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}" stroke-linecap="round" '
                 f'stroke-linejoin="round"{ds}{ms}{me}{o}/>')

    def use(self, sym, x, y, size, color):
        self.add(f'<use href="#{sym}" x="{x - size / 2}" y="{y - size / 2}" width="{size}" height="{size}" '
                 f'style="color:{color}"/>')

    def hexagon(self, cx, cy, r, fill, stroke, sw=1.5):
        k = r * 0.866
        pts = [(cx, cy - r), (cx + k, cy - r / 2), (cx + k, cy + r / 2), (cx, cy + r), (cx - k, cy + r / 2), (cx - k, cy - r / 2)]
        p = ' '.join(f'{a:.1f},{b:.1f}' for a, b in pts)
        self.add(f'<polygon points="{p}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')

    def render(self):
        defs = DEFS
        body = '\n'.join(self.parts)
        return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.w} {self.h}" width="{self.w}" height="{self.h}" '
                f'role="img" aria-labelledby="t d">\n<title id="t">{escape(self.title)}</title>\n'
                f'<desc id="d">{escape(self.desc)}</desc>\n{defs}\n'
                f'<rect width="{self.w}" height="{self.h}" fill="{PAL["bg"]}"/>\n{body}\n</svg>\n')


def marker(mid, color):
    return (f'<marker id="{mid}" viewBox="0 0 10 10" refX="8.5" refY="5" markerWidth="7" markerHeight="7" '
            f'orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="{color}"/></marker>')


# Symbols use currentColor so <use style="color:..."> recolours them. All drawn in a 24x24 box.
SYMBOLS = {
    'person': '<circle cx="12" cy="7" r="4" fill="none" stroke="currentColor" stroke-width="1.8"/>'
              '<path d="M4,22 C4,15 20,15 20,22" fill="none" stroke="currentColor" stroke-width="1.8"/>',
    'group': '<circle cx="12" cy="7" r="3.2" fill="none" stroke="currentColor" stroke-width="1.6"/>'
             '<circle cx="5" cy="9" r="2.4" fill="none" stroke="currentColor" stroke-width="1.4"/>'
             '<circle cx="19" cy="9" r="2.4" fill="none" stroke="currentColor" stroke-width="1.4"/>'
             '<path d="M6,21 C6,15 18,15 18,21 M1,20 C1,16 6,15 7.5,15.5 M23,20 C23,16 18,15 16.5,15.5" fill="none" stroke="currentColor" stroke-width="1.5"/>',
    'tester': '<circle cx="9" cy="7" r="3.5" fill="none" stroke="currentColor" stroke-width="1.6"/>'
              '<path d="M2,22 C2,15 16,15 16,22" fill="none" stroke="currentColor" stroke-width="1.6"/>'
              '<path d="M19,4 L19,14 C19,16 22,16 22,14 L22,4 M18,4 L23,4" fill="none" stroke="currentColor" stroke-width="1.4"/>',
    'designer': '<circle cx="9" cy="7" r="3.5" fill="none" stroke="currentColor" stroke-width="1.6"/>'
                '<path d="M2,22 C2,15 16,15 16,22" fill="none" stroke="currentColor" stroke-width="1.6"/>'
                '<path d="M20,3 L17,13 M20,3 L23,13 M18,9 L22,9" fill="none" stroke="currentColor" stroke-width="1.4"/>',
    'key': '<circle cx="9" cy="7" r="3.5" fill="none" stroke="currentColor" stroke-width="1.6"/>'
           '<path d="M2,22 C2,15 16,15 16,22" fill="none" stroke="currentColor" stroke-width="1.6"/>'
           '<circle cx="20" cy="5" r="2.5" fill="none" stroke="currentColor" stroke-width="1.4"/>'
           '<path d="M20,7.5 L20,15 M20,11 L22.5,11 M20,14 L22,14" fill="none" stroke="currentColor" stroke-width="1.4"/>',
    'chat': '<path d="M3,4 H21 V16 H10 L5,20 V16 H3 Z" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/>'
            '<path d="M9.5,7 L8.5,13 M13.5,7 L12.5,13 M7,9 H15 M6.5,11 H14.5" fill="none" stroke="currentColor" stroke-width="1.2"/>',
    'mail': '<rect x="3" y="5" width="18" height="14" rx="2" fill="none" stroke="currentColor" stroke-width="1.6"/>'
            '<path d="M3,6 L12,13 L21,6" fill="none" stroke="currentColor" stroke-width="1.6"/>',
    'branch': '<circle cx="6" cy="5" r="2.5" fill="none" stroke="currentColor" stroke-width="1.6"/>'
              '<circle cx="6" cy="19" r="2.5" fill="none" stroke="currentColor" stroke-width="1.6"/>'
              '<circle cx="18" cy="8" r="2.5" fill="none" stroke="currentColor" stroke-width="1.6"/>'
              '<path d="M6,7.5 V16.5 M18,10.5 C18,15 6,13 6,16.5" fill="none" stroke="currentColor" stroke-width="1.6"/>',
    'rss': '<rect x="3" y="3" width="18" height="18" rx="3" fill="none" stroke="currentColor" stroke-width="1.6"/>'
           '<circle cx="8" cy="16" r="1.6" fill="currentColor"/>'
           '<path d="M7,11 A6,6 0 0 1 13,17 M7,7 A10,10 0 0 1 17,17" fill="none" stroke="currentColor" stroke-width="1.6"/>',
    'book': '<path d="M4,4 H11 C12,4 12,5 12,6 V20 C12,19 11,18 10,18 H4 Z M20,4 H13 C12,4 12,5 12,6 V20 C12,19 13,18 14,18 H20 Z" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round"/>',
    'stamp': '<path d="M9,3 H15 V9 C15,11 18,11 18,13 V15 H6 V13 C6,11 9,11 9,9 Z" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round"/>'
             '<path d="M4,19 H20 M8,15 V19 M16,15 V19" fill="none" stroke="currentColor" stroke-width="1.5"/>',
    'map': '<rect x="3" y="3" width="18" height="18" rx="2" fill="none" stroke="currentColor" stroke-width="1.5"/>'
           '<rect x="6" y="6" width="12" height="12" rx="1" fill="none" stroke="currentColor" stroke-width="1.3"/>'
           '<rect x="9" y="9" width="6" height="6" rx="1" fill="none" stroke="currentColor" stroke-width="1.2"/>',
    'ledger': '<rect x="5" y="3" width="14" height="18" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.5"/>'
              '<path d="M8,3 V21 M10,8 H16 M10,11 H16 M10,14 H16 M10,17 H14" fill="none" stroke="currentColor" stroke-width="1.2"/>',
    'queue': '<rect x="2" y="8" width="6" height="8" rx="1" fill="none" stroke="currentColor" stroke-width="1.4"/>'
             '<rect x="9" y="8" width="6" height="8" rx="1" fill="none" stroke="currentColor" stroke-width="1.4"/>'
             '<rect x="16" y="8" width="6" height="8" rx="1" fill="none" stroke="currentColor" stroke-width="1.4"/>',
    'kanban': '<rect x="3" y="3" width="18" height="18" rx="2" fill="none" stroke="currentColor" stroke-width="1.5"/>'
              '<path d="M9,3 V21 M15,3 V21" stroke="currentColor" stroke-width="1.2"/>'
              '<rect x="4.5" y="6" width="3" height="3" fill="currentColor"/><rect x="10.5" y="6" width="3" height="3" fill="currentColor"/>'
              '<rect x="10.5" y="11" width="3" height="3" fill="currentColor"/><rect x="16.5" y="6" width="3" height="3" fill="currentColor"/>',
    'wolf': '<path d="M4,3 L8,9 H16 L20,3 L21,13 L12,21 L3,13 Z" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round"/>'
            '<circle cx="9" cy="12" r="1" fill="currentColor"/><circle cx="15" cy="12" r="1" fill="currentColor"/>',
    'graph': '<circle cx="5" cy="6" r="2" fill="none" stroke="currentColor" stroke-width="1.4"/>'
             '<circle cx="18" cy="5" r="2" fill="none" stroke="currentColor" stroke-width="1.4"/>'
             '<circle cx="12" cy="13" r="2" fill="none" stroke="currentColor" stroke-width="1.4"/>'
             '<circle cx="5" cy="19" r="2" fill="none" stroke="currentColor" stroke-width="1.4"/>'
             '<circle cx="19" cy="19" r="2" fill="none" stroke="currentColor" stroke-width="1.4"/>'
             '<path d="M6.5,7.5 L10.5,11.5 M16.5,6.5 L13.5,11.5 M10.5,14.5 L6.5,17.5 M13.5,14.5 L17.5,17.5" stroke="currentColor" stroke-width="1.2"/>',
    'shield': '<path d="M12,2 L20,5 V11 C20,16 16,20 12,22 C8,20 4,16 4,11 V5 Z" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/>'
              '<path d="M8,12 L11,15 L16,9" fill="none" stroke="currentColor" stroke-width="1.8"/>',
    'lock': '<rect x="5" y="10" width="14" height="11" rx="2" fill="none" stroke="currentColor" stroke-width="1.7"/>'
            '<path d="M8,10 V7 C8,2 16,2 16,7 V10" fill="none" stroke="currentColor" stroke-width="1.7"/>',
    'clock': '<circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="1.6"/>'
             '<path d="M12,6 V12 L16,14" fill="none" stroke="currentColor" stroke-width="1.6"/>',
    'bench': '<path d="M2,15 H22 M4,15 V21 M20,15 V21" fill="none" stroke="currentColor" stroke-width="1.6"/>'
             '<rect x="4" y="9" width="7" height="6" rx="1" fill="none" stroke="currentColor" stroke-width="1.4"/>'
             '<rect x="13" y="9" width="7" height="6" rx="1" fill="none" stroke="currentColor" stroke-width="1.4"/>'
             '<path d="M8,5 A6,6 0 0 1 16,5 M10,7 A3,3 0 0 1 14,7" fill="none" stroke="currentColor" stroke-width="1.3"/>',
    'sat': '<circle cx="12" cy="12" r="3" fill="none" stroke="currentColor" stroke-width="1.5"/>'
           '<path d="M3,12 H8 M16,12 H21 M5,9 V15 M19,9 V15" fill="none" stroke="currentColor" stroke-width="1.5"/>',
    'hourglass': '<path d="M6,3 H18 M6,21 H18 M7,3 C7,9 17,9 17,12 C17,15 7,15 7,21 M17,3 C17,9 7,9 7,12 C7,15 17,15 17,21" fill="none" stroke="currentColor" stroke-width="1.4"/>',
    'loop': '<path d="M19,12 A7,7 0 1 1 15,5.6" fill="none" stroke="currentColor" stroke-width="1.7"/>'
            '<path d="M14,2.5 L16.5,6 L12.5,7.5" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round"/>',
    'package': '<path d="M12,2 L21,7 V17 L12,22 L3,17 V7 Z M3,7 L12,12 L21,7 M12,12 V22" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round"/>',
    'globe': '<circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="1.5"/>'
             '<path d="M3,12 H21 M12,3 C8,7 8,17 12,21 C16,17 16,7 12,3" fill="none" stroke="currentColor" stroke-width="1.3"/>',
    'server': '<rect x="4" y="4" width="16" height="7" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.5"/>'
              '<rect x="4" y="13" width="16" height="7" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.5"/>'
              '<circle cx="8" cy="7.5" r="1" fill="currentColor"/><circle cx="8" cy="16.5" r="1" fill="currentColor"/>',
}


def symbols_block():
    return ''.join(f'<symbol id="{k}" viewBox="0 0 24 24">{v}</symbol>' for k, v in SYMBOLS.items())


DEFS = ('<defs>'
        + marker('ah-muted', PAL['muted']) + marker('ah-cool', PAL['cool']) + marker('ah-flame', PAL['flame'])
        + marker('ah-ok', PAL['ok']) + marker('ah-blue', PAL['blue'])
        + '<radialGradient id="glow" cx="50%" cy="55%" r="50%"><stop offset="0%" stop-color="#3b82f6" stop-opacity="0.32"/>'
          '<stop offset="60%" stop-color="#06b6d4" stop-opacity="0.08"/><stop offset="100%" stop-color="#f4f6f7" stop-opacity="0"/></radialGradient>'
        + '<linearGradient id="flamefill" x1="0" y1="1" x2="0" y2="0"><stop offset="0%" stop-color="#dbeafe"/>'
          '<stop offset="35%" stop-color="#2563eb"/><stop offset="100%" stop-color="#06b6d4"/></linearGradient>'
        + '<filter id="soft" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="6"/></filter>'
        + '<filter id="card" x="-5%" y="-10%" width="110%" height="130%"><feDropShadow dx="0" dy="1.5" stdDeviation="2" flood-color="#151a1f" flood-opacity="0.08"/></filter>'
        + symbols_block() + '</defs>')


def flame(svg, cx, cy, scale=1.0):
    """Blue pilot flame centred on (cx, cy): outer flame, inner core, glow."""
    s = scale

    def P(x, y):
        return f'{cx + x * s:.1f},{cy + y * s:.1f}'
    outer = (f'M{P(0,-62)} C{P(22,-30)} {P(44,-8)} {P(40,18)} C{P(36,44)} {P(18,56)} {P(0,56)} '
             f'C{P(-18,56)} {P(-36,44)} {P(-40,18)} C{P(-43,-2)} {P(-30,-18)} {P(-18,-30)} '
             f'C{P(-16,-12)} {P(-8,-4)} {P(-2,0)} C{P(2,-22)} {P(-6,-40)} {P(0,-62)} Z')
    inner = (f'M{P(0,-8)} C{P(12,8)} {P(20,22)} {P(17,34)} C{P(14,46)} {P(6,50)} {P(0,50)} '
             f'C{P(-6,50)} {P(-14,46)} {P(-17,34)} C{P(-19,22)} {P(-10,8)} {P(0,-8)} Z')
    svg.add(f'<circle cx="{cx}" cy="{cy}" r="{150 * s:.0f}" fill="url(#glow)"/>')
    svg.add(f'<path d="{outer}" fill="#2563eb" opacity="0.28" filter="url(#soft)"/>')
    svg.add(f'<path d="{outer}" fill="url(#flamefill)" stroke="#1d4ed8" stroke-width="1.4"/>')
    svg.add(f'<path d="{inner}" fill="#ffffff" opacity="0.95"/>')
