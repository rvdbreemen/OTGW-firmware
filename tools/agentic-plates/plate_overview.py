"""Overview plate: 'OTGW agentic workshop: architecture at a glance' (2560x1440, light theme, English).

Everything on one poster: people and decision rights, channels, BlueFlame Bot with its watch loops,
sessions and agents, rule layers and enforcement, the knowledge base, the ADR kit, the pipeline with
its gates, what ships, external sources and a strip of key numbers. Facts come from
docs/process/agentic-setup.md (as of 2026-11-15). Reuses the palette, symbols and flame of svgkit.py.

Every text carries a data-box attribute with the card it must stay inside, so a renderer-side check can
compare each text's bounding box with its card.
"""
import math
import sys

from svgkit import Svg, PAL as P, flame, marker

W, H = 2560, 1440

# Extra 24x24 symbols for this plate only (svgkit.py stays untouched so plates A and B render the same).
EXTRA_SYMBOLS = {
    'flame_s': '<path d="M12,2 C15,6 19,9 18.5,14 C18,19 15,22 12,22 C9,22 6,19 5.5,14 C5.2,11 7,9 9,7.5 '
               'C9.3,10 10.5,11.5 11.7,12 C12.3,9 11,5 12,2 Z" fill="currentColor"/>',
    'shield_f': '<path d="M12,2 L20,5 V11 C20,16 16,20 12,22 C8,20 4,16 4,11 V5 Z" fill="currentColor" stroke="currentColor" '
                'stroke-width="1.6" stroke-linejoin="round"/><path d="M8,12 L11,15 L16,9" fill="none" stroke="#ffffff" stroke-width="2"/>',
    'shield_o': '<path d="M12,2 L20,5 V11 C20,16 16,20 12,22 C8,20 4,16 4,11 V5 Z" fill="none" stroke="currentColor" '
                'stroke-width="1.6" stroke-linejoin="round" stroke-dasharray="3 2.4"/>',
    'terminal': '<rect x="2.5" y="4" width="19" height="16" rx="2.5" fill="none" stroke="currentColor" stroke-width="1.6"/>'
                '<path d="M6.5,9 L10,12 L6.5,15 M12,15.5 H17" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>',
    'spark': '<path d="M12,2 C12.6,8 16,11.4 22,12 C16,12.6 12.6,16 12,22 C11.4,16 8,12.6 2,12 C8,11.4 11.4,8 12,2 Z" fill="none" '
             'stroke="currentColor" stroke-width="1.5" stroke-linejoin="round"/>',
}
EXTRA_DEFS = (marker('ah-amber', P['amber']) + marker('ah-crit', P['crit'])
              + ''.join(f'<symbol id="{k}" viewBox="0 0 24 24">{v}</symbol>' for k, v in EXTRA_SYMBOLS.items()))


class Plate(Svg):
    """Svg with a 'current card': every text written while a card is current must stay inside it."""
    cur = None

    def text(self, x, y, s, box='cur', **kw):
        if not s:
            return
        super().text(x, y, s, **kw)
        b = self.cur if box == 'cur' else box
        if b:
            self.parts[-1] = self.parts[-1].replace('<text ', '<text data-box="%g,%g,%g,%g" ' % b, 1)

    def card(self, x, y, w, h, **kw):
        self.rect(x, y, w, h, **kw)
        self.cur = (x, y, w, h)

    def free(self):
        self.cur = None

    def zl(self, x, y, s):
        self.free()
        self.text(x, y, s, size=15, fill=P['flame'], weight=700, family='m', spacing='2.5')
        return x + len(s) * 11.6 + 14

    def cap(self, x, y, s, anchor='start', fill=None):
        self.text(x, y, s, size=13, fill=fill or P['muted'], family='m', spacing='1.2', anchor=anchor, box=None)

    def pill(self, x, y, w, h, s, fill, stroke, color=None, family='m', size=13, weight=600, dash=None, sw=1.1):
        d = f' stroke-dasharray="{dash}"' if dash else ''
        self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{h / 2}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"{d}/>')
        self.text(x + w / 2, y + h / 2 + size * 0.35, s, size=size, fill=color or stroke, weight=weight, family=family,
                  anchor='middle', box=(x, y, w, h))

    def circle(self, cx, cy, r, fill, stroke, sw=1.2, dash=None, opacity=None):
        d = f' stroke-dasharray="{dash}"' if dash else ''
        o = f' opacity="{opacity}"' if opacity else ''
        self.add(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"{d}{o}/>')

    def render(self):
        return super().render().replace('</defs>', EXTRA_DEFS + '</defs>', 1)


def curve(x1, y1, x2, y2):
    my = (y1 + y2) / 2
    return f'M{x1},{y1} C{x1},{my} {x2},{my} {x2},{y2}'


s = Plate(W, H, 'OTGW agentic workshop: architecture at a glance',
          'Overview poster of the OTGW agentic development setup as of 2026-11-15: people and decision rights, channels, '
          'BlueFlame Bot and its three watch loops, sessions and agents, rule layers and how they are enforced, the knowledge base, '
          'the ADR kit, the pipeline with its gates, what ships, external sources and key numbers.')

# =====================================================================================================
# 1. Header: title, as-of line, legend
# =====================================================================================================
s.text(40, 58, 'OTGW agentic workshop: architecture at a glance', size=40, weight=800, family='d')
s.text(42, 86, 'as of 2026-11-15 · firmware 2.0.0-beta.327', size=15, fill=P['muted'], family='m')

lx, ly = 1236, 44
s.cap(lx, ly + 1, 'COLOUR')
lx += 78
for soft, strong, label in [('flame_soft', 'flame', 'people · decision'), ('cool_soft', 'cool', 'agents'),
                            ('blue_soft', 'blue', 'BlueFlame Bot'), ('amber_soft', 'amber', 'gate'),
                            ('ok_soft', 'ok', 'ships'), ('crit_soft', 'crit', 'hard stop'),
                            ('surface', 'muted', 'store')]:
    s.add(f'<rect x="{lx}" y="{ly - 13}" width="18" height="18" rx="4" fill="{P[soft]}" stroke="{P[strong]}" stroke-width="1.5"/>')
    s.text(lx + 26, ly + 1, label, size=14, fill=P['muted'])
    lx += 26 + len(label) * 7.0 + 26
lx, ly = 1236, 78
s.cap(lx, ly + 1, 'LINES')
lx += 78
for color, dash, sw, st, en, label in [(P['flame'], None, 3.0, None, 'ah-flame', 'human decision'),
                                       (P['cool'], None, 1.8, 'ah-cool', 'ah-cool', 'two-way'),
                                       (P['cool'], '5 4', 1.8, None, 'ah-cool', 'read only'),
                                       (P['ok'], None, 3.0, None, 'ah-ok', 'ships')]:
    s.path(f'M{lx + 4},{ly - 4} H{lx + 36}', color, sw=sw, dash=dash, start=st, end=en)
    s.text(lx + 48, ly + 1, label, size=14, fill=P['muted'])
    lx += 48 + len(label) * 7.0 + 26
for sym, color, label in [('shield_f', P['amber'], 'refuses'), ('shield_o', P['amber'], 'convention or review'),
                          ('lock', P['crit'], 'per-action yes')]:
    s.use(sym, lx + 9, ly - 4, 20, color)
    s.text(lx + 26, ly + 1, label, size=14, fill=P['muted'])
    lx += 26 + len(label) * 7.0 + 26
s.path(f'M40,104 H{W - 40}', P['line'], sw=1)

# =====================================================================================================
# 2. People and decision rights
# =====================================================================================================
s.cap(s.zl(40, 132, 'PEOPLE'), 132, 'two decide · the others bring input and evidence')
PY, PH, PW = 144, 74, 292
people = [  # symbol, name, sub, decides
    ('group', 'End users', 'issues, PRs, forum posts become tasks', False),
    ('person', 'Read-only members', 'no effect on the bot · answered once', False),
    ('tester', 'Alpha testers', 'flash, capture, report · no decisions', False),
    ('designer', 'Sergeant D · SAT designer', 'decides SAT behaviour · recorded first', True),
    ('key', 'Robert · maintainer', 'scope, priority, waivers, releases', True),
]
pcx = []
for i, (sym, name, sub, decides) in enumerate(people):
    x = 40 + i * (PW + 12)
    pcx.append(x + PW / 2)
    s.card(x, PY, PW, PH, fill=P['flame_soft'] if decides else P['surface'], stroke=P['flame'], sw=2.2 if decides else 1.1)
    s.use(sym, x + 30, PY + 37, 34, P['flame'])
    s.text(x + 58, PY + 31, name, size=17, weight=700, family='d')
    s.text(x + 58, PY + 54, sub, size=13, fill=P['muted'])
s.free()

# decision-rights matrix
MX, MY = 1596, 132
s.zl(MX, MY, 'WHO DECIDES WHAT')
cols = [('Robert', P['flame']), ('Sergeant D', P['flame']), ('Testers · users', P['muted']), ('Agents', P['cool'])]
AW, CW = 292, 158
for j, (name, color) in enumerate(cols):
    s.text(MX + AW + j * CW + CW / 2, MY + 30, name, size=14, weight=700, family='d', fill=color, anchor='middle')
rows = [  # action lines, values per column
    (['Have a task created'], ['dec', 'yes', 'gated', 'stand']),
    (['Scope · priority · waivers'], ['dec', 'no', 'no', 'no']),
    (['SAT behaviour'], ['exc', 'dec', 'no', 'no']),
    (['Push to dev and the 1.x line'], ['dec', 'no', 'no', 'stand']),
    (['Flash a bench unit'], ['dec', 'no', 'no', 'bench']),
    (['Public 2.x beta · 08:00 / 20:00 slot'], ['dec', 'no', 'no', 'stand']),
    (['Stable release · push to main · force-push', 'PIC flash · merging another person\'s PR'], ['per', 'no', 'no', 'no']),
]
val = {
    'dec': ('decides', P['flame_soft'], P['flame'], None), 'yes': ('yes, directly', P['flame_soft'], P['flame'], None),
    'exc': ('exceptions only', P['surface'], P['flame'], '4 3'), 'gated': ('input, partly gated', P['amber_soft'], P['amber'], None),
    'stand': ('standing permission', P['cool_soft'], P['cool'], None), 'bench': ('bench tester only', P['cool_soft'], P['cool'], None),
    'per': ('per-action yes via DM', P['crit_soft'], P['crit'], None),
}
ry = MY + 40
for lines, vals in rows:
    rh = 30 if len(lines) == 1 else 46
    s.path(f'M{MX},{ry} H{MX + AW + 4 * CW}', P['line'], sw=1)
    hard = vals[0] == 'per'
    if hard:
        s.use('lock', MX + 13, ry + rh / 2, 17, P['crit'])
    for k, ln in enumerate(lines):
        s.text(MX + (26 if hard else 8), ry + (20 if len(lines) == 1 else 19 + k * 17), ln, size=14, weight=600,
               box=(MX, ry, AW, rh))
    for j, v in enumerate(vals):
        cx = MX + AW + j * CW + CW / 2
        if v == 'no':
            s.text(cx, ry + rh / 2 + 5, 'no', size=13, fill=P['muted'], anchor='middle')
        else:
            label, fill, stroke, dash = val[v]
            s.pill(cx - 74, ry + rh / 2 - 11, 148, 22, label, fill, stroke, color=P['fg'], family='b', dash=dash)
    ry += rh
s.path(f'M{MX},{ry} H{MX + AW + 4 * CW}', P['line'], sw=1)

# =====================================================================================================
# 3. Channels and the channel bus
# =====================================================================================================
s.zl(40, 250, 'CHANNELS')
CY0, CH, CWD = 262, 84, 240
chans = [  # symbol, name, sub1, sub2, two-way
    ('rss', 'Tweakers topic', 'RSS · read only', 'no way to post', False),
    ('branch', 'GitHub', 'issues · PRs · releases', 'two-way · maintainer account', True),
    ('chat', 'Discord support', 'read + announcements', 'replies in practice', True),
    ('chat', '#alpha-testing', 'zips · reports · SAT questions', 'two-way', True),
    ('chat', '#dev-sat-mqtt', 'backlog commands · SAT', 'two-way', True),
    ('mail', 'DM with maintainer', 'one question per message', 'decisions · hard-stop yes', True),
]
ccx = []
pitch = (1508 - CWD) / 5
for i, (sym, name, s1, s2, two) in enumerate(chans):
    x = 40 + i * pitch
    ccx.append(x + CWD / 2)
    s.card(x, CY0, CWD, CH, fill=P['bg'], stroke=P['muted'], sw=1.1, dash='5 4')
    s.use(sym, x + 26, CY0 + 30, 28, P['muted'])
    s.text(x + 50, CY0 + 30, name, size=16, weight=700, family='d')
    s.text(x + 50, CY0 + 52, s1, size=13, fill=P['muted'])
    s.text(x + 50, CY0 + 70, s2, size=13, fill=P['cool'] if two and i != 2 else P['muted'])
s.free()

# people -> channels (no line for read-only members: nothing they write triggers the bot)
PB = PY + PH
for cx in ccx[0:3]:
    s.path(curve(pcx[0] - 20, PB, cx, CY0 - 3), P['muted'], sw=1.3, dash='3 4', end='ah-muted')
s.path(curve(pcx[2], PB, ccx[3] - 30, CY0 - 3), P['muted'], sw=1.3, dash='3 4', start='ah-muted', end='ah-muted')
s.path(curve(pcx[3] - 20, PB, ccx[3] + 30, CY0 - 3), P['flame'], sw=1.9, start='ah-flame', end='ah-flame')
s.path(curve(pcx[3] + 20, PB, ccx[4], CY0 - 3), P['flame'], sw=1.9, start='ah-flame', end='ah-flame')
s.path(f'M{ccx[5]},{PB + 3} V{CY0 - 3}', P['flame'], sw=2.4, start='ah-flame', end='ah-flame')

BUS = 368
FX, FY, FR = 1280, 606, 104
s.path(f'M{ccx[0]},{BUS} H{ccx[5]}', P['cool'], sw=1.8)
for cx, (_, _, _, _, two) in zip(ccx, chans):
    if two:
        s.path(f'M{cx},{CY0 + CH + 3} V{BUS - 2}', P['cool'], sw=1.8, start='ah-cool', end='ah-cool')
    else:
        s.path(f'M{cx},{CY0 + CH + 3} V{BUS - 2}', P['cool'], sw=1.8, dash='5 4', end='ah-cool')
bx = FX - 20
s.path(f'M{bx},{BUS} V{FY - 100}', P['cool'], sw=2.4, start='ah-cool', end='ah-cool')
s.text(bx - 12, 412, 'watch and reply', size=13, fill=P['cool'], anchor='end')
# Robert -> the session directly
ox = FX + 22
s.path(f'M{40 + 5 * PW + 48},{PY + PH / 2} H1560 Q1570,{PY + PH / 2} 1570,{PY + PH / 2 + 10} V380 Q1570,390 1560,390 '
       f'H{ox + 10} Q{ox},390 {ox},400 V{FY - 100}', P['flame'], sw=3.0, end='ah-flame')
s.text(ox + 14, 412, 'instructions in the session', size=13, fill=P['flame'])

# =====================================================================================================
# 4. The heart: BlueFlame Bot, three watch loops, rules of play
# =====================================================================================================
s.cap(s.zl(760, 418, 'BLUEFLAME BOT'), 418, 'a persona, not a program')
s.circle(FX, FY, FR, P['blue_soft'], P['blue'], sw=2.2)
flame(s, FX, FY + 4, 1.08)
RINGS = (114, 124, 134)
for r, dash in zip(RINGS, (None, '7 4', '2 5')):  # the three watch loops as orbits around the flame
    s.circle(FX, FY, r, 'none', P['cool'], sw=1.2, dash=dash, opacity='0.7')
s.text(FX, FY + 176, 'BlueFlame Bot', size=27, weight=800, family='d', anchor='middle')
s.text(FX, FY + 198, 'the voice of the workshop', size=14, fill=P['muted'], anchor='middle')

LX, LW, LH = 760, 352, 80
s.cap(LX, 446, 'THREE WATCH LOOPS')
loops = [
    ('/backlog_discord', 'before every task', 'unanswered sweep always runs', 'commands: early exit if checked < 5 min ago', None),
    ('GitHub sweep', 'before every task', 'open issues and PRs via gh, as maintainer', 'no early exit · deduplicated on the URL', '7 4'),
    ('/check_otgw_issues', 'by hand', 'Discord · GitHub · Tweakers, triage only', 'early exit if all three checked < 2 h ago', '2 5'),
]
for i, (name, when, l1, l2, dash) in enumerate(loops):
    y = 458 + i * (LH + 12)
    cy = y + LH / 2
    ex = FX - math.sqrt(RINGS[i] ** 2 - (cy - FY) ** 2)
    s.path(f'M{LX + LW + 1},{cy} H{ex:.1f}', P['cool'], sw=1.3, dash=dash)
    s.circle(round(ex, 1), cy, 3.5, P['cool'], P['cool'])
    s.card(LX, y, LW, LH, fill=P['surface'], stroke=P['cool'], sw=1.3)
    s.use('loop', LX + 26, cy, 30, P['cool'])
    s.text(LX + 52, y + 27, name, size=15, weight=700, family='m')
    s.text(LX + LW - 12, y + 27, when, size=13, weight=600, fill=P['cool'], anchor='end')
    s.text(LX + 52, y + 49, l1, size=13, fill=P['muted'])
    s.text(LX + 52, y + 67, l2, size=13, fill=P['muted'])
s.free()

RX = 1456
s.cap(RX, 446, 'RULES OF PLAY')
plays = ['English · answer first · real mention', 'Never names the 2.x gateway board in public',
         'Boiler or thermostat question: equipment matrix first', 'SAT claim: thermo-nova citation + register check',
         'Technical claims verified in the code first', 'Zip post pings no one and must match HEAD']
for i, t in enumerate(plays):
    y = 458 + i * 46
    s.use('shield_f' if i == len(plays) - 1 else 'shield_o', RX + 12, y + 17, 22, P['amber'])
    s.text(RX + 34, y + 22, t, size=14, box=(RX, y, 362, 34))

# rules -> bot and knowledge <-> bot
LNK = 792
s.path(f'M706,{LNK} H1080 C1130,{LNK} 1150,750 {FX - 96},{FY + 44}', P['muted'], sw=1.5, end='ah-muted')
s.text(716, LNK - 8, 'rules load into every session', size=13, fill=P['muted'])
s.path(f'M1854,{LNK} H1480 C1430,{LNK} 1410,750 {FX + 96},{FY + 44}', P['muted'], sw=1.5, start='ah-muted', end='ah-muted')
s.text(1844, LNK - 8, 'reads knowledge · writes register and notes', size=13, fill=P['muted'], anchor='end')

# =====================================================================================================
# 5. Sessions and agents
# =====================================================================================================
SY = 826
s.cap(s.zl(760, SY, 'SESSIONS & AGENTS'), SY, 'one worktree per live session')
sess = [  # symbol, name, line1, line2, bot?
    ('branch', 'Developer lanes', 'own worktree or lane', 'push only after the bench OK', False),
    ('terminal', 'Coordinator', 'picks tasks · owns DM and ledger', 'speaks as BlueFlame Bot', True),
    ('bench', 'Bench tester', 'the only one that flashes', 'OTA first · ticks the bench ACs', False),
    ('chat', 'Comms session', 'Discord talk · post drafts', 'speaks as BlueFlame Bot', True),
]
SW_, SH_ = 254, 76
for i, (sym, name, l1, l2, bot) in enumerate(sess):
    x = 760 + i * (SW_ + 12)
    y = SY + 12
    s.card(x, y, SW_, SH_, fill=P['blue_soft'] if bot else P['cool_soft'], stroke=P['blue'] if bot else P['cool'], sw=1.5)
    s.use(sym, x + 26, y + 30, 30, P['blue'] if bot else P['cool'])
    s.text(x + 52, y + 27, name, size=16, weight=700, family='d')
    s.text(x + 52, y + 47, l1, size=13, fill=P['muted'])
    if bot:
        s.use('flame_s', x + 58, y + 61, 14, P['blue'])
        s.text(x + 68, y + 66, l2, size=13, fill=P['blue'], weight=600)
    else:
        s.text(x + 52, y + 66, l2, size=13, fill=P['muted'])
s.free()

# subagents per model
HY = SY + 122
s.cap(760, HY - 17, 'SUBAGENTS PER MODEL', )
subs = [('S', 'Sonnet', ['implementation']), ('H', 'Haiku', ['runners,', 'verbatim output']), ('O', 'Opus', ['correctness', 'review']),
        ('A', 'Advisor', ['plan self-review'])]
for i, (mono, name, sub) in enumerate(subs):
    x = 760 + i * 156
    s.hexagon(x + 26, HY + 18, 24, P['cool_soft'], P['cool'])
    s.text(x + 26, HY + 24, mono, size=17, weight=800, family='d', fill=P['cool'], anchor='middle')
    s.text(x + 58, HY + 14, name, size=15, weight=700, family='d', box=(x, HY - 8, 156, 52))
    for k, part in enumerate(sub):
        s.text(x + 58, HY + 31 + k * 15, part, size=13, fill=P['muted'], box=(x, HY - 8, 156, 62))
# other agents
OX = 1414
s.cap(OX, HY - 17, 'OTHER AGENTS · SAME CORE BLOCK')
for i, (name, sub) in enumerate([('Codex', 'via AGENTS.md'), ('Copilot', '13 agent profiles'), ('Claude', 'in GitHub Actions')]):
    x = OX + i * 134
    s.use('spark', x + 12, HY + 16, 20, P['muted'])
    s.text(x + 30, HY + 14, name, size=15, weight=700, family='d', box=(x, HY - 8, 134, 52))
    s.text(x + 30, HY + 31, sub, size=13, fill=P['muted'], box=(x, HY - 8, 134, 52))

# skills
KY = SY + 186
s.cap(760, KY + 19, 'SKILLS')
x = 822
for name, locked in [('/implement-next-task', False), ('/backlog_discord', False), ('/check_otgw_issues', False)]:
    w = len(name) * 7.9 + 20
    s.pill(x, KY, w, 28, name, P['bg'], P['cool'])
    x += w + 8
x = OX
for name, locked in [('/beta-release-2x', False), ('/release-2x', True), ('analyze-capture', False)]:
    w = len(name) * 7.9 + 20 + (16 if locked else 0)
    if locked:
        s.add(f'<rect x="{x}" y="{KY}" width="{w}" height="28" rx="14" fill="{P["bg"]}" stroke="{P["crit"]}" stroke-width="1.1"/>')
        s.use('lock', x + 16, KY + 14, 15, P['crit'])
        s.text(x + 26, KY + 18.5, name, size=13, weight=600, fill=P['crit'], family='m', box=(x, KY, w, 28))
    else:
        s.pill(x, KY, w, 28, name, P['bg'], P['cool'])
    x += w + 8

# =====================================================================================================
# 6. Rules and enforcement (left column)
# =====================================================================================================
s.zl(40, 418, 'RULES & ENFORCEMENT')
s.cap(40, 446, 'LAYERS · WHAT AN AGENT READS')
layers = [  # name, sub, tag, fill, stroke
    ('Maintainer directives', 'dated and quoted · highest authority', '', P['flame_soft'], P['flame']),
    ('Shared core block', 'identical in three instruction files', 'every session', P['amber_soft'], P['amber']),
    ('CLAUDE.md', '189 lines, test limit 230 · wins on conflict', 'every session', P['surface'], P['muted']),
    ('OpenWolf protocol', 'anatomy · cerebrum · buglog · Claude only', 'every session', P['surface'], P['muted']),
    ('Always-loaded rules', 'sat-ground-truth · discord · openwolf', 'every session', P['surface'], P['muted']),
    ('Path-loaded rules', 'firmware-coding · navigation', 'on demand', P['surface'], P['muted']),
    ('Skills and commands', '11 skills + 2 commands, loaded when invoked', 'on demand', P['cool_soft'], P['cool']),
    ('ADRs', '10 binding, each names a gate · rest: review', '', P['amber_soft'], P['amber']),
    ('SAT ground truth', 'register → thermo-nova → spec → firmware', '', P['flame_soft'], P['flame']),
]
SLW, SLH = 312, 52
for i, (name, sub, tag, fill, stroke) in enumerate(layers):
    y = 458 + i * (SLH + 9)
    s.add(f'<rect x="43" y="{y + 5}" width="{SLW}" height="{SLH}" rx="6" fill="{P["line"]}"/>')  # the slab's edge
    s.add(f'<rect x="40" y="{y}" width="{SLW}" height="{SLH}" rx="6" fill="{fill}" stroke="{stroke}" stroke-width="1.2"/>')
    s.add(f'<rect x="40" y="{y}" width="7" height="{SLH}" rx="3" fill="{stroke}"/>')
    s.cur = (40, y, SLW, SLH)
    s.text(60, y + 22, name, size=15, weight=700, family='d')
    if tag:
        s.text(40 + SLW - 12, y + 22, tag, size=13, fill=P['muted'], anchor='end', italic=True)
    s.text(60, y + 41, sub, size=13, fill=P['muted'])
s.free()

EX, EW, EH = 382, 320, 60
s.cap(EX, 446, 'WHAT ACTUALLY STOPS A CHANGE')
enf = [  # name, tag, kind, line1, line2
    ('Claude hooks', 'REFUSES', 'gate', 'backlog guard blocks Edit and Write, not Bash', 'post-edit evaluate returns the FAIL lines'),
    ('Git hooks', 'REFUSES', 'gate', 'no bump or no TASK id: commit rejected', 'the adr-judge pass is off by default'),
    ('evaluate.py', 'REFUSES', 'gate', '42 checks, 34 in --quick · exit 1 on FAIL', 'String in hot paths only warns'),
    ('Tests', 'LOCAL ONLY', 'local', '274 Python tests through run_tests.py', 'CI runs only test_adr_governance.py'),
    ('CI on push to dev', 'AFTER PUSH', 'local', 'evaluate.yml · build.yml esp32-combo', 'adr-governance.yml · red after the push'),
    ('Script refusals', 'REFUSES', 'gate', 'build lock: one build, at most 4 jobs', 'post script: the zip must come from HEAD'),
    ('Review', 'JUDGEMENT', 'review', 'the reviewer team is told to refute', 'main session reads the raw evidence itself'),
    ('Convention only', 'NO GATE', 'conv', 'hard stops · wrapper build · DM route', 'only the bench tester flashes · register first'),
]
kind = {'gate': (P['amber_soft'], P['amber'], 'shield_f', None), 'local': (P['bg'], P['muted'], 'shield_o', None),
        'review': (P['cool_soft'], P['cool'], 'shield_o', None), 'conv': (P['crit_soft'], P['crit'], 'shield_o', '5 4')}
for i, (name, tag, k, l1, l2) in enumerate(enf):
    y = 458 + i * (EH + 6)
    soft, strong, sym, dash = kind[k]
    s.card(EX, y, EW, EH, fill=P['surface'], stroke=strong, sw=1.2, dash=dash)
    s.use(sym, EX + 20, y + 19, 22, strong)
    s.text(EX + 38, y + 22, name, size=15, weight=700, family='d')
    tw = len(tag) * 7.9 + 14
    s.add(f'<rect x="{EX + EW - tw - 8}" y="{y + 8}" width="{tw}" height="20" rx="4" fill="{soft}" stroke="{strong}" stroke-width="1"/>')
    s.text(EX + EW - tw / 2 - 8, y + 22.5, tag, size=13, weight=700, fill=strong, family='m', anchor='middle')
    s.text(EX + 14, y + 40, l1, size=13, fill=P['muted'])
    s.text(EX + 14, y + 55, l2, size=13, fill=P['muted'])
s.free()
s.text(40, 1017, 'Codex and Copilot do not load .claude/rules.', size=13, fill=P['muted'], italic=True)
s.text(EX, 1004, 'In the repo the hard stops are convention:', size=13, fill=P['crit'], italic=True)
s.text(EX, 1021, 'no hook guards a push to main or a force-push.', size=13, fill=P['crit'], italic=True)

# =====================================================================================================
# 7. Knowledge base (right column)
# =====================================================================================================
KX = 1860
s.cap(s.zl(KX, 418, 'KNOWLEDGE BASE'), 418, 'what the workshop remembers')
stores = [
    ('kanban', 'Backlog.md', '477', 'active tasks + 1,323 archived'),
    ('stamp', 'ADRs', '245', '10 binding · written with the ADR kit'),
    ('ledger', 'SAT register', '353', 'entries · record first, then build'),
    ('queue', 'Open questions', '227', 'questions to Sergeant D · 18 open'),
    ('map', 'C4 map', '20', 'docs · which component owns which code'),
    ('wolf', 'OpenWolf', '224', 'buglog entries · anatomy · cerebrum'),
    ('graph', 'graphify', '1,628', 'nodes · code graph of src/'),
    ('book', 'Reference projects', '', 'thermo-nova · PIC firmware · pyotgw'),
]
KW, KH = 324, 74
for i, (sym, name, num, sub) in enumerate(stores):
    x = KX + (i % 2) * (KW + 12)
    y = 436 + (i // 2) * (KH + 10)
    s.card(x, y, KW, KH, fill=P['surface'], stroke=P['line'], sw=1.2)
    s.use(sym, x + 28, y + 37, 30, P['cool'])
    s.text(x + 54, y + 31, name, size=16, weight=700, family='d')
    if num:
        s.text(x + KW - 14, y + 33, num, size=22, weight=800, family='d', fill=P['cool'], anchor='end')
    s.text(x + 54, y + 54, sub, size=13, fill=P['muted'])
s.free()

# =====================================================================================================
# 8. ADR kit
# =====================================================================================================
AY = 816
s.cap(s.zl(KX, AY, 'ADR KIT'), AY, 'how a decision becomes binding')
ay = AY + 26
steps = [('Proposed', 118, P['bg'], P['muted'], '5 4'), ('Four gates', 128, P['amber_soft'], P['amber'], None),
         ('Accepted', 118, P['ok_soft'], P['ok'], None), ('Superseded', 128, P['bg'], P['muted'], None)]
x = KX
gates_cx = None
for i, (name, w, fill, stroke, dash) in enumerate(steps):
    s.pill(x, ay, w, 32, name, fill, stroke, color=P['fg'], family='d', size=14, weight=700, dash=dash, sw=1.4)
    if name == 'Four gates':
        gates_cx = x + w / 2
    if i < len(steps) - 1:
        s.path(f'M{x + w + 5},{ay + 16} H{x + w + 49}', P['muted'], sw=1.5, end='ah-muted')
    x += w + 56
s.text(KX + 118 + 56 + 128 + 28, ay - 4, 'maintainer yes', size=13, fill=P['flame'], weight=600, anchor='middle')
gy = ay + 50
gw = 158
s.path(f'M{gates_cx},{ay + 32} V{gy - 8} M{KX + gw / 2},{gy} V{gy - 8} H{KX + 3 * (gw + 9.3) + gw / 2} V{gy}', P['amber'], sw=1.2)
for i, g in enumerate(['Completeness', 'Evidence', 'Clarity', 'Consistency']):
    gx = KX + i * (gw + 9.3)
    if 0 < i < 3:
        s.path(f'M{gx + gw / 2},{gy - 8} V{gy}', P['amber'], sw=1.2)
    s.pill(gx, gy, gw, 26, g, P['surface'], P['amber'], color=P['fg'], family='b', size=13)
s.text(KX, gy + 44, 'Never self-accepted · content immutable after acceptance', size=13, fill=P['muted'])

ey = gy + 52
s.cap(KX, ey + 12, 'ENFORCEMENT PATH')
# lane 1: Enforcement block -> adr-judge (off)
l1 = ey + 20
s.pill(KX, l1, 164, 28, 'Enforcement block', P['surface'], P['muted'], color=P['fg'], family='b', size=13)
s.path(f'M{KX + 169},{l1 + 14} H{KX + 199}', P['muted'], sw=1.4, dash='4 3', end='ah-muted')
s.pill(KX + 204, l1, 220, 28, 'adr-judge in pre-commit', P['bg'], P['crit'], color=P['fg'], family='b', size=13, dash='5 4')
s.text(KX + 436, l1 + 19, 'currently off by default', size=13, fill=P['crit'], weight=600)
# lane 2: Binding ADR -> check gate -> CI
l2 = l1 + 34
s.pill(KX, l2, 164, 28, 'Binding ADR', P['amber_soft'], P['amber'], color=P['fg'], family='b', size=13)
s.path(f'M{KX + 169},{l2 + 14} H{KX + 199}', P['amber'], sw=1.6, end='ah-amber')
s.pill(KX + 204, l2, 220, 28, 'check_* gate in evaluate.py', P['amber_soft'], P['amber'], color=P['fg'], family='b', size=13)
s.path(f'M{KX + 429},{l2 + 14} H{KX + 459}', P['amber'], sw=1.6, end='ah-amber')
s.pill(KX + 464, l2, 196, 28, 'CI gate on push to dev', P['amber_soft'], P['amber'], color=P['fg'], family='b', size=13)
s.text(KX, l2 + 46, 'Of the 10 binding ADRs: 4 gated in CI · 4 by local tests or build · 2 via adr-judge, now off', size=13, fill=P['muted'])

# =====================================================================================================
# 9. Pipeline and gates, what ships
# =====================================================================================================
PZ = 1078
s.cap(s.zl(40, PZ, 'PIPELINE & GATES'), PZ, 'from backlog task to alpha zip · under each step: what makes it refuse or stop')
pipe = [  # name, hard gate?, refuse line 1, refuse line 2
    ('Backlog task', False, 'no task, no work', ''),
    ('Plan + self-review', False, 'SAT control change waits', 'for Sergeant D\'s OK'),
    ('Red → green', False, 'green on the fix alone', 'is no proof'),
    ('Reviewer team', False, 'told to refute,', 'not to approve'),
    ('evaluate · tests', True, 'exit 1 on any FAIL', 'no new FAIL allowed'),
    ('Bump + commit', True, 'no prerelease bump,', 'or TASK id without its file'),
    ('Build + lock', True, 'one build per machine:', 'the second one waits'),
    ('Bench via OTA', False, 'a failed bench test', 'publishes nothing'),
    ('Push to dev', False, 'a freeze is announced,', 'or the bench OK is missing'),
    ('Zip post + ledger', True, 'zip is not HEAD, tag already', 'posted, or zip above 10 MB'),
]
PCY, PCH, PCW, PP = PZ + 24, 50, 164, 176
for i, (name, hard, r1, r2) in enumerate(pipe):
    x = 40 + i * PP
    bench = name.startswith('Bench')
    stroke = P['amber'] if hard else P['cool']
    s.card(x, PCY, PCW, PCH, fill=P['amber_soft'] if hard else P['surface'], stroke=stroke, sw=1.5 if hard else 1.2)
    s.use('shield_f' if hard else 'shield_o', x + 19, PCY + 25, 22, P['amber'])
    s.text(x + 36, PCY + 30, name, size=14, weight=700, family='d')
    s.cur = (x - 4, PCY + PCH, PP - 4, 50)
    s.text(x + 4, PCY + PCH + 22, r1, size=13, fill=P['muted'])
    s.text(x + 4, PCY + PCH + 39, r2, size=13, fill=P['muted'])
    if i < len(pipe) - 1:
        s.path(f'M{x + PCW + 1},{PCY + 25} H{x + PP - 3}', P['cool'], sw=1.5, end='ah-cool')
s.free()
agree = ['A refusal is reported, never bypassed', 'Exit 0 is not evidence', 'Done only with evidence from the same session',
         'One prerelease tag per firmware commit']
x = 40
for a in agree:
    s.circle(x + 5, PCY + PCH + 79, 3.5, P['cool'], P['cool'])
    s.text(x + 16, PCY + PCH + 84, a, size=14, weight=600)
    x += len(a) * 7.3 + 60

# sessions -> pipeline, bench tester <-> bench step
bx8 = 1400
s.path(f'M{bx8},{SY + 12 + SH_ + 3} V{PCY - 4}', P['cool'], sw=1.8, start='ah-cool', end='ah-cool')
s.text(bx8 + 12, 1064, 'bench request · OK', size=13, fill=P['cool'])
rx = 1352
BR = PCY - 10  # bracket over all steps: the sessions run the whole pipeline
s.path(f'M40,{BR + 5} V{BR} H{bx8 - 9} M{bx8 + 9},{BR} H{40 + 9 * PP + PCW} V{BR + 5}', P['cool'], sw=1.3)
s.path(f'M{rx},{KY + 34} V{BR - 3}', P['cool'], sw=1.8, end='ah-cool')
s.text(rx - 12, 1064, 'sessions run the pipeline', size=13, fill=P['cool'], anchor='end')

# what ships
SX = 1840
s.zl(SX, PZ, 'WHAT SHIPS')
ships = [  # symbol, name, line1, line2, soft, strong
    ('package', 'Alpha zip', 'every firmware tag · from dev', 'bench OK + post script → #alpha-testing', 'ok_soft', 'ok'),
    ('clock', 'Beta 2.x', 'standing permission · 08:00 / 20:00 slot', '1 h soak · GitHub prerelease', 'amber_soft', 'amber'),
    ('lock', 'Stable 2.x', 'per-action yes via DM · release/2.x', '3 checkpoints · 24 h soak · freeze', 'crit_soft', 'crit'),
    ('lock', '1.x LTS', 'per-action yes · otgw-1.x.x → main', 'skill lives in the 1.x worktree', 'crit_soft', 'crit'),
]
HW, HH = 334, 82
for i, (sym, name, l1, l2, soft, strong) in enumerate(ships):
    x = SX + (i % 2) * (HW + 12)
    y = PCY + (i // 2) * (HH + 10)
    s.card(x, y, HW, HH, fill=P[soft], stroke=P[strong], sw=1.5)
    s.use(sym, x + 28, y + 41, 28, P[strong])
    s.text(x + 54, y + 28, name, size=17, weight=700, family='d')
    s.text(x + 54, y + 50, l1, size=13, fill=P['fg'])
    s.text(x + 54, y + 68, l2, size=13, fill=P['muted'])
s.free()
s.path(f'M{40 + 9 * PP + PCW + 2},{PCY + 25} H{SX - 5}', P['ok'], sw=3.0, end='ah-ok')

# =====================================================================================================
# 10-12. External sources, key numbers, findings note
# =====================================================================================================
s.path(f'M40,1282 H{W - 40}', P['line'], sw=1)
BZ = 1308
s.zl(40, BZ, 'EXTERNAL KNOWLEDGE & SERVICES')
ext = [('globe', 'Equipment matrix', 'check first: boilers'), ('branch', 'thermo-nova', 'pinned · read only'),
       ('book', 'OpenTherm spec', 'v4.2'), ('server', 'Toolchains', 'pinned registries'),
       ('chat', 'Discord', 'MCP + webhook'), ('terminal', 'GitHub', 'gh as maintainer'),
       ('rss', 'Tweakers', 'RSS · read only'), ('spark', 'AI services', 'Claude · Copilot · Codex')]
for i, (sym, name, sub) in enumerate(ext):
    cx = 104 + i * 139
    s.circle(cx, BZ + 42, 22, P['surface'], P['muted'], sw=1.1, dash='4 3')
    s.use(sym, cx, BZ + 42, 24, P['muted'])
    s.text(cx, BZ + 86, name, size=14, weight=700, family='d', anchor='middle')
    s.text(cx, BZ + 104, sub, size=13, fill=P['muted'], anchor='middle')

NX = 1200
s.zl(NX, BZ, 'THE WORKSHOP IN NUMBERS')
nums = [('327', 'firmware beta,', 'beta.79 on 11-04'), ('8.7', 'prerelease tags', 'per day'), ('42', 'checks in', 'evaluate.py'),
        ('274', 'Python tests', ''), ('11 + 2', 'skills and', 'commands'), ('7', 'GitHub', 'workflows'),
        ('293', 'register entries', 'by Sergeant D')]
for i, (n, a, b) in enumerate(nums):
    x = NX + i * 141
    if i:
        s.path(f'M{x - 14},{BZ + 22} V{BZ + 104}', P['line'], sw=1)
    s.text(x, BZ + 58, n, size=34, weight=800, family='d', fill=P['cool'])
    s.text(x, BZ + 84, a, size=13, fill=P['muted'])
    s.text(x, BZ + 101, b, size=13, fill=P['muted'])

s.card(2212, BZ + 12, 308, 94, fill=P['surface'], stroke=P['flame'], sw=1.3, dash='6 4')
s.text(2228, BZ + 40, '11 new findings', size=17, weight=700, family='d')
s.text(2228, BZ + 63, '3 decisions open for the maintainer', size=14, fill=P['flame'], weight=600)
s.text(2228, BZ + 88, 'chapters 13 and 14 of agentic-setup.md', size=13, fill=P['muted'])
s.free()

open(sys.argv[1], 'w').write(s.render())
print('wrote', sys.argv[1])
