"""Plate A: the whole agentic workshop in one picture (reference wireframe, 1920x1080)."""
import sys
from svgkit import Svg, PAL, flame

LANG = sys.argv[2] if len(sys.argv) > 2 else 'nl'
L = {
    'nl': dict(
        title='OTGW agentic werkplaats', sub='architectuuroverzicht · plaat A · peildatum 2026-11-15',
        people='MENSEN', channels='KANALEN', agents='AGENTLAAG', knowledge='KENNIS EN REGELS', pipeline='PIJPLIJN EN POORTEN',
        ships='NAAR BUITEN', sources='EXTERNE KENNISBRONNEN',
        persons=[('group', 'Eindgebruikers', 'meldingen en vragen'), ('key', 'Maintainer', 'scope, prioriteit, vrijgave'),
                 ('designer', 'SAT-ontwerper', 'bron van waarheid voor SAT'), ('tester', 'Alpha-testers', 'flashen, captures, geen beslismacht')],
        chans=[('rss', 'Tweakers-topic', 'RSS · alleen lezen'), ('branch', 'GitHub', 'issues · PR\'s · releases'),
               ('chat', 'Discord-support', '4 kanalen · lezen + aankondigingen'), ('mail', 'DM met maintainer', 'één vraag per bericht'),
               ('chat', '#dev-sat-mqtt', 'backlog-commando\'s'), ('chat', '#alpha-testing', 'builds · verslagen · SAT')],
        bot='BlueFlame Bot', botsub='coördinator en comms-sessie',
        hexes=[('S', 'Sonnet', 'implementatie'), ('H', 'Haiku', 'runners'), ('O', 'Opus', 'correctheidsreview'),
               ('A', 'Advisor', 'zelfreview van het plan'), ('L', 'Reviewlenzen', 'regels · tests · SAT')],
        bench='Bench-tester', benchsub='flasht en test als enige', others='Andere agents', otherssub='Codex · Copilot · CI-Claude', comms='Comms-sessie', commssub='Discord-gesprek · postconcepten',
        know=[('book', 'Regels', 'CLAUDE.md · AGENTS.md · rules/'), ('stamp', "ADR's + ADR-kit", '245 besluiten · 10 binding'),
              ('map', 'C4-kaart', 'welk component bezit welke code'), ('ledger', 'SAT-beslisregister', 'Sergeant D · eerst vastleggen'),
              ('queue', 'Open vragen', 'wachtrij met status'), ('kanban', 'Backlog.md', 'elke wijziging begint hier'),
              ('wolf', 'OpenWolf', 'anatomy · cerebrum · buglog'), ('graph', 'graphify', 'codegraaf van src/')],
        pipe=['Backlog-taak', 'Plan + zelfreview', 'Code rood → groen', 'Reviewteam', 'evaluate · tests', 'Bump + commit-hooks',
              'Build esp32-combo · lock', 'Bench via OTA', 'Push dev · post zip'],
        out=[('lock', '1.x LTS', 'otgw-1.x.x → main'), ('lock', 'Stabiel 2.x', 'release/2.x · na go'),
             ('clock', 'Beta 2.x', 'staand mandaat 08:00/20:00'), ('package', 'Alpha-zip', 'per getest taakresultaat')],
        srcs=[('globe', 'OpenTherm-matrix'), ('branch', 'thermo-nova'), ('book', 'OpenTherm-spec'), ('server', 'toolchains')],
        lines=dict(decide='besluit van een mens', work='werk tussen agents', read='alleen lezen', ship='naar buiten', gate='poort', stop='harde stop'),
        lbl=dict(know='leest regels · schrijft register en notes', pipe='voert uit', session='opdrachten in de sessie', loop='naar testers'),
    ),
    'en': dict(
        title='OTGW agentic workshop', sub='architecture overview · plate A · as of 2026-11-15',
        people='PEOPLE', channels='CHANNELS', agents='AGENT LAYER', knowledge='KNOWLEDGE & RULES', pipeline='PIPELINE & GATES',
        ships='WHAT SHIPS', sources='EXTERNAL KNOWLEDGE',
        persons=[('group', 'End users', 'reports and questions'), ('key', 'Maintainer', 'scope, priority, release'),
                 ('designer', 'SAT designer', 'source of truth for SAT'), ('tester', 'Alpha testers', 'flash, capture, no decisions')],
        chans=[('rss', 'Tweakers topic', 'RSS · read only'), ('branch', 'GitHub', 'issues · PRs · releases'),
               ('chat', 'Discord support', '4 channels · read + announcements'), ('mail', 'DM with maintainer', 'one question per message'),
               ('chat', '#dev-sat-mqtt', 'backlog commands'), ('chat', '#alpha-testing', 'builds · reports · SAT')],
        bot='BlueFlame Bot', botsub='coordinator and comms session',
        hexes=[('S', 'Sonnet', 'implementation'), ('H', 'Haiku', 'runners'), ('O', 'Opus', 'correctness review'),
               ('A', 'Advisor', 'plan self-review'), ('L', 'Review lenses', 'rules · tests · SAT')],
        bench='Bench tester', benchsub='the only one that flashes', others='Other agents', otherssub='Codex · Copilot · CI Claude', comms='Comms session', commssub='Discord talk · post drafts',
        know=[('book', 'Rules', 'CLAUDE.md · AGENTS.md · rules/'), ('stamp', 'ADRs + ADR kit', '245 decisions · 10 binding'),
              ('map', 'C4 map', 'which component owns which code'), ('ledger', 'SAT register', 'Sergeant D · record first'),
              ('queue', 'Open questions', 'queue with status'), ('kanban', 'Backlog.md', 'every change starts here'),
              ('wolf', 'OpenWolf', 'anatomy · cerebrum · buglog'), ('graph', 'graphify', 'code graph of src/')],
        pipe=['Backlog task', 'Plan + self-review', 'Code red → green', 'Review team', 'evaluate · tests', 'Bump + commit hooks',
              'Build esp32-combo · lock', 'Bench via OTA', 'Push dev · post zip'],
        out=[('lock', '1.x LTS', 'otgw-1.x.x → main'), ('lock', 'Stable 2.x', 'release/2.x · after go'),
             ('clock', 'Beta 2.x', 'standing permission 08:00/20:00'), ('package', 'Alpha zip', 'per tested task')],
        srcs=[('globe', 'OpenTherm matrix'), ('branch', 'thermo-nova'), ('book', 'OpenTherm spec'), ('server', 'toolchains')],
        lines=dict(decide='human decision', work='work between agents', read='read only', ship='ships', gate='gate', stop='hard stop'),
        lbl=dict(know='reads rules · writes register and notes', pipe='executes', session='instructions in the session', loop='to testers'),
    ),
}[LANG]

W, H = 1920, 1080
s = Svg(W, H, L['title'], 'Reference wireframe for an overview plate of the OTGW agentic development setup: people, channels, '
        'BlueFlame Bot and its subagents, knowledge and rules, the pipeline with gates, and what ships.')
P = PAL

# ---- title bar + legend
s.text(40, 42, L['title'], size=28, weight=800, family='d')
s.text(54 + len(L['title']) * 16.2, 42, L['sub'], size=13, fill=P['muted'], family='m')
lx, ly = 1110, 36
for color, dash, key in [(P['flame'], None, 'decide'), (P['cool'], None, 'work'), (P['cool'], '5 4', 'read'), (P['ok'], None, 'ship')]:
    s.path(f'M{lx},{ly - 4} H{lx + 26}', color, sw=2.6 if key in ('decide', 'ship') else 1.8, dash=dash)
    s.text(lx + 32, ly, L['lines'][key], size=12, fill=P['muted'])
    lx += 52 + len(L['lines'][key]) * 6.4
s.use('shield', lx + 8, ly - 4, 18, P['amber']); s.text(lx + 22, ly, L['lines']['gate'], size=12, fill=P['muted']); lx += 70
s.use('lock', lx + 8, ly - 4, 16, P['crit']); s.text(lx + 22, ly, L['lines']['stop'], size=12, fill=P['muted'])
s.path('M30,60 H1890', P['line'], sw=1)

# ---- people band
s.zone_label(40, 80, L['people'])
pcx = [269, 730, 1190, 1651]
for (icon, name, sub), cx in zip(L['persons'], pcx):
    s.rect(cx - 180, 90, 360, 76, fill=P['flame_soft'], stroke=P['flame'], sw=1.4)
    s.use(icon, cx - 140, 128, 34, P['flame'])
    s.text(cx - 110, 122, name, size=18, weight=700, family='d')
    s.text(cx - 110, 146, sub, size=13, fill=P['muted'])

# ---- channel band
s.zone_label(40, 202, L['channels'])
ccx = [160, 480, 800, 1120, 1440, 1760]
for (icon, name, sub), cx in zip(L['chans'], ccx):
    s.rect(cx - 135, 212, 270, 76, fill=P['bg'], stroke=P['muted'], sw=1.1, dash='5 4')
    s.use(icon, cx - 104, 250, 28, P['muted'])
    s.text(cx - 80, 244, name, size=16, weight=700, family='d')
    s.text(cx - 80, 266, sub, size=12, fill=P['muted'])

# people -> channels
def curve(x1, y1, x2, y2):
    my = (y1 + y2) / 2
    return f'M{x1},{y1} C{x1},{my} {x2},{my} {x2},{y2}'
for x2 in (160, 480, 800):
    s.path(curve(269, 166, x2, 210), P['muted'], sw=1.2, dash='3 4', end='ah-muted')
s.path(curve(730, 166, 1120, 210), P['flame'], sw=2.6, end='ah-flame')
s.path(curve(1190, 166, 1440, 210), P['flame'], sw=1.8, start='ah-flame', end='ah-flame')
s.path(curve(1190, 166, 1760, 210), P['flame'], sw=1.8, start='ah-flame', end='ah-flame')
s.path(curve(1651, 166, 1760, 210), P['muted'], sw=1.2, dash='3 4', start='ah-muted', end='ah-muted')

# ---- channel bus into the agent layer
BUS = 330
s.path(f'M160,{BUS} H1760', P['cool'], sw=1.6)
two_way = {480, 1120, 1440, 1760}
for cx in ccx:
    if cx in two_way:
        s.path(f'M{cx},290 V{BUS - 2}', P['cool'], sw=1.6, start='ah-cool', end='ah-cool')
    else:
        s.path(f'M{cx},290 V{BUS - 2}', P['cool'], sw=1.6, dash='5 4', end='ah-cool')

# ---- zones frame
s.zone_label(40, 382, L['knowledge'])
s.zone_label(460, 382, L['agents'])
s.zone_label(1510, 382, L['pipeline'])
s.rect(445, 360, 1030, 562, stroke=P['line'], sw=1, dash='2 5', rx=10)

# ---- knowledge cabinet
ky = 396
for icon, name, sub in L['know']:
    s.rect(30, ky, 390, 56, fill=P['surface'], stroke=P['line'])
    s.use(icon, 60, ky + 28, 28, P['cool'])
    s.text(88, ky + 24, name, size=16, weight=700, family='d')
    s.text(88, ky + 44, sub, size=12, fill=P['muted'])
    ky += 65

# ---- pipeline conveyor
py = 396
stations = []
for i, name in enumerate(L['pipe']):
    s.rect(1510, py, 380, 46, fill=P['surface'], stroke=P['cool'] if i not in (4, 5, 6, 7) else P['amber'], sw=1.2)
    s.use('shield', 1534, py + 23, 20, P['amber'])
    s.text(1556, py + 29, f'{i + 1}', size=12, fill=P['muted'], family='m')
    s.text(1578, py + 29, name, size=15, weight=600)
    if i < len(L['pipe']) - 1:
        s.path(f'M1700,{py + 46} V{py + 56}', P['cool'], sw=1.4, end='ah-cool')
    stations.append(py)
    py += 58

# ---- burner + flame
CX, CY = 960, 600
s.add(f'<circle cx="{CX}" cy="{CY}" r="92" fill="{P["blue_soft"]}" stroke="{P["blue"]}" stroke-width="2"/>')
flame(s, CX, CY + 4, 0.95)
s.text(CX, 724, L['bot'], size=24, weight=800, family='d', anchor='middle')
s.text(CX, 746, L['botsub'], size=13, fill=P['muted'], anchor='middle')

# bus -> burner and maintainer -> burner
s.path(f'M930,{BUS} V514', P['cool'], sw=2.0, start='ah-cool', end='ah-cool')
s.path(f'M760,166 C760,190 958,188 958,212 V292 C958,316 990,312 990,{BUS + 18} V512', P['flame'], sw=2.8, end='ah-flame')
s.text(1000, 352, L['lbl']['session'], size=12, fill=P['flame'])

# skill chips
chips_l = ['/implement-next-task', '/backlog_discord', '/check_otgw_issues']
chips_r = ['/beta-release-2x', '/release-2x', '/update-docs-2x', '/analyze-capture']
for i, c in enumerate(chips_l):
    s.rect(700, 392 + i * 34, 196, 26, fill=P['bg'], stroke=P['cool'], sw=1, rx=13)
    s.text(798, 410 + i * 34, c, size=12, fill=P['cool'], family='m', anchor='middle')
for i, c in enumerate(chips_r):
    s.rect(1026, 392 + i * 30, 180, 24, fill=P['bg'], stroke=P['cool'], sw=1, rx=12)
    s.text(1116, 409 + i * 30, c, size=12, fill=P['cool'], family='m', anchor='middle')

# subagent ring
hexpos = [(700, 612), (790, 762), (960, 812), (1130, 762), (1220, 612)]
for (mono, name, sub), (hx, hy) in zip(L['hexes'], hexpos):
    import math
    ang = math.atan2(hy - CY, hx - CX)
    sx, sy = CX + 94 * math.cos(ang), CY + 94 * math.sin(ang)
    ex, ey = hx - 40 * math.cos(ang), hy - 40 * math.sin(ang)
    if hx == CX:
        sy = 756  # start below the BlueFlame label so the line does not cross it
    s.path(f'M{sx:.0f},{sy:.0f} L{ex:.0f},{ey:.0f}', P['cool'], sw=1.4, end='ah-cool')
    s.hexagon(hx, hy, 34, P['cool_soft'], P['cool'])
    s.text(hx, hy + 7, mono, size=20, weight=800, family='d', fill=P['cool'], anchor='middle')
    s.text(hx, hy + 54, name, size=15, weight=700, family='d', anchor='middle')
    s.text(hx, hy + 72, sub, size=12, fill=P['muted'], anchor='middle')

# knowledge <-> burner, burner -> pipeline
s.path('M422,500 C600,505 760,540 870,572', P['muted'], sw=1.6, start='ah-muted', end='ah-muted')
s.text(470, 492, L['lbl']['know'], size=12, fill=P['muted'])
s.path(f'M1050,572 C1200,520 1380,470 1506,{stations[1] + 23}', P['cool'], sw=2.0, end='ah-cool')
s.text(1300, 470, L['lbl']['pipe'], size=12, fill=P['cool'])

# others (bottom-left of agent zone) and bench tester (bottom-right)
s.rect(462, 842, 205, 70, fill=P['surface'], stroke=P['blue'], sw=1.3)
s.use('chat', 486, 877, 26, P['blue'])
s.text(508, 872, L['comms'], size=15, weight=700, family='d')
s.text(508, 893, L['commssub'], size=11, fill=P['muted'])
s.path('M600,842 C640,780 820,700 880,668', P['blue'], sw=1.4, dash='4 3', start='ah-blue', end='ah-blue')
s.rect(677, 842, 205, 70, fill=P['surface'], stroke=P['line'])
for i in range(3):
    s.use('sat', 694 + (i % 2) * 16, 860 + i * 13, 15, P['muted'])
s.text(724, 872, L['others'], size=15, weight=700, family='d')
s.text(724, 893, L['otherssub'], size=11, fill=P['muted'])
s.rect(1190, 842, 270, 70, fill=P['surface'], stroke=P['cool'], sw=1.4)
s.use('bench', 1226, 877, 40, P['cool'])
s.text(1256, 872, L['bench'], size=16, weight=700, family='d')
s.text(1256, 894, L['benchsub'], size=12, fill=P['muted'])
s.path(f'M1460,877 C1485,877 1490,{stations[7] + 23} 1506,{stations[7] + 23}', P['cool'], sw=1.6, start='ah-cool', end='ah-cool')

# ---- bottom band
s.path('M30,932 H1890', P['line'], sw=1)
s.zone_label(40, 958, L['sources'])
for i, (icon, name) in enumerate(L['srcs']):
    x = 80 + i * 170
    s.add(f'<circle cx="{x}" cy="1004" r="24" fill="{P["surface"]}" stroke="{P["muted"]}" stroke-width="1"/>')
    s.use(icon, x, 1004, 26, P['muted'])
    s.text(x, 1052, name, size=12, fill=P['muted'], anchor='middle')
s.zone_label(790, 958, L['ships'])
ox = [790, 1065, 1340, 1615]
for (icon, name, sub), x in zip(L['out'], ox):
    color = P['crit'] if icon == 'lock' else (P['amber'] if icon == 'clock' else P['ok'])
    fill = {'lock': P['crit_soft'], 'clock': P['amber_soft'], 'package': P['ok_soft']}[icon]
    s.rect(x, 972, 255, 78, fill=fill, stroke=P['ok'] if icon == 'package' else P['line'], sw=1.4)
    s.use(icon, x + 30, 1011, 26, color)
    s.text(x + 54, 1004, name, size=17, weight=700, family='d')
    s.text(x + 54, 1026, sub, size=12, fill=P['muted'])
# push -> alpha zip, loop to #alpha-testing
s.path(f'M1742,{stations[8] + 46} V968', P['ok'], sw=3.0, end='ah-ok')
s.path('M1870,1011 H1906 V252 H1898', P['ok'], sw=1.6, dash='6 5', end='ah-ok')
s.text(1780, 962, L['lbl']['loop'], size=12, fill=P['ok'])

open(sys.argv[1], 'w').write(s.render())
print('wrote', sys.argv[1])
