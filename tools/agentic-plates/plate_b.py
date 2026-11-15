"""Plate B: BlueFlame Bot, the pilot flame that watches Discord, GitHub and Tweakers (reference wireframe, 1920x1080)."""
import sys
import math
from svgkit import Svg, PAL, flame

LANG = sys.argv[2] if len(sys.argv) > 2 else 'nl'
L = {
    'nl': dict(
        title='BlueFlame Bot: de waakvlam', sub='bewaking en reactie per kanaal · plaat B · peildatum 2026-11-15',
        loops_lbl='BEWAKINGSLUSSEN',
        loops=[('/backlog_discord', 'vroege stop bij < 5 min · onbeantwoord-sweep draait altijd', 'stap 0, vóór elke nieuwe taak'),
               ('GitHub-sweep', 'open issues en PR\'s · ontdubbelen op URL', 'stap 0b, vóór elke nieuwe taak'),
               ('/check_otgw_issues', 'vroege stop als alle drie bronnen < 2 uur', 'met de hand gestart, met checkpoints')],
        discord='DISCORD', discord_sub='via discord-mcp (en bot-REST) · posts als BlueFlame Bot',
        doors=[('#alpha-testing', 'builds · verslagen · SAT-vragen · testers', 2), ('#dev-sat-mqtt', 'backlog-commando\'s · SAT-rondes', 2),
               ('DM met maintainer', 'één vraag per bericht, met advies', 2), ('#beta-testing', 'gelezen · beta-aankondigingen · sinds 11-15 ook verzoeken', 3),
               ('#devs-esp-firmware', 'alleen lezen', 1), ('#english-support', 'gelezen · release-post (EN) · praktijk: antwoorden', 3), ('#nederlandse-ondersteuning', 'gelezen · release-post (NL) · praktijk: antwoorden', 3)],
        github='GITHUB', github_sub='via gh · onder het maintainer-account',
        gh_tiles=[('Issues', 'lezen · captures vragen · vraag beantwoorden', 2), ('Pull requests', 'review als commentaar · mergen na ja', 2),
                  ('Releases', 'beta-prerelease (draft eerst)', 0)],
        tweakers='TWEAKERS', tweakers_sub='forumtopic via RSS · alleen lezen',
        tw_tile=('Forumtopic', 'NL-berichten → EN-samenvatting · keyword-filter'),
        bot='BlueFlame Bot', botsub='stem van coördinator en comms-sessie',
        shields=[('Engels,', 'antwoord eerst'), ('Geen', 'boardnaam'), ('Eerst', 'vastleggen'), ('Testers', 'adviseren'), ('Eén', 'tokenhouder')],
        out_lbl='WAT HIJ OPLEVERT',
        outs=[('chat', 'Antwoord', 'met echte <@ID>-mention', 'cool'), ('kanban', 'Backlog-taak', 'bug · github · needs-maintainer-ok', 'cool'),
              ('ledger', 'Register-entry', 'SAT-besluit vóór de bouw', 'cool'), ('queue', 'Triagelijst', 'in de sessie, ontwikkelaar kiest', 'cool'),
              ('mail', 'Vraag aan maintainer', 'DM: opties + advies', 'flame'), ('package', 'Alpha-zip + verslag', 'na OK van de bench', 'ok')],
        found=[('server', 'discord-mcp (Java) · :8085 · supervisor · geplande taak'),
               ('hourglass', 'watermerken: *_last_checked.txt'),
               ('branch', 'gh als maintainer · RSS · build-webhook · bot-REST')],
        legend=dict(beam='bewaking', two='lezen en antwoorden', read='alleen lezen', dm='naar de maintainer', ship='naar buiten', rule='spelregel'),
    ),
    'en': dict(
        title='BlueFlame Bot: the pilot flame', sub='monitoring and replies per channel · plate B · as of 2026-11-15',
        loops_lbl='WATCH LOOPS',
        loops=[('/backlog_discord', 'early exit under 5 min · unanswered sweep always runs', 'step 0, before every new task'),
               ('GitHub sweep', 'open issues and PRs · dedupe on URL', 'step 0b, before every new task'),
               ('/check_otgw_issues', 'early exit when all three sources < 2 h', 'started by hand, with checkpoints')],
        discord='DISCORD', discord_sub='through discord-mcp (and bot REST) · posts as BlueFlame Bot',
        doors=[('#alpha-testing', 'builds · reports · SAT questions · testers', 2), ('#dev-sat-mqtt', 'backlog commands · SAT rounds', 2),
               ('DM with maintainer', 'one question per message, with advice', 2), ('#beta-testing', 'read · beta announcements · requests since Nov 15', 3),
               ('#devs-esp-firmware', 'read only', 1), ('#english-support', 'read · release post (EN) · replies in practice', 3), ('#nederlandse-ondersteuning', 'read · release post (NL) · replies in practice', 3)],
        github='GITHUB', github_sub='through gh · under the maintainer account',
        gh_tiles=[('Issues', 'read · ask for captures · answer questions', 2), ('Pull requests', 'review as comment · merge after yes', 2),
                  ('Releases', 'beta prerelease (draft first)', 0)],
        tweakers='TWEAKERS', tweakers_sub='forum topic via RSS · read only',
        tw_tile=('Forum topic', 'Dutch posts → English summary · keyword filter'),
        bot='BlueFlame Bot', botsub='voice of the coordinator and the comms session',
        shields=[('English,', 'answer first'), ('No', 'board name'), ('Record', 'first'), ('Testers', 'advise'), ('One', 'token holder')],
        out_lbl='WHAT IT PRODUCES',
        outs=[('chat', 'Reply', 'with a real <@ID> mention', 'cool'), ('kanban', 'Backlog task', 'bug · github · needs-maintainer-ok', 'cool'),
              ('ledger', 'Register entry', 'SAT decision before the build', 'cool'), ('queue', 'Triage list', 'in the session, developer picks', 'cool'),
              ('mail', 'Question to maintainer', 'DM: options + advice', 'flame'), ('package', 'Alpha zip + report', 'after the bench OK', 'ok')],
        found=[('server', 'discord-mcp (Java) · :8085 · supervisor · scheduled task'),
               ('hourglass', 'watermarks: *_last_checked.txt'),
               ('branch', 'gh as maintainer · RSS · build webhook · bot REST')],
        legend=dict(beam='monitoring', two='read and reply', read='read only', dm='to the maintainer', ship='ships', rule='ground rule'),
    ),
}[LANG]

W, H = 1920, 1080
s = Svg(W, H, L['title'], 'Reference wireframe for a plate about BlueFlame Bot: the watch loops, the Discord, GitHub and Tweakers '
        'outposts with read-only or two-way arrows, the ground rules around the flame, the foundation and what the bot produces.')
P = PAL

# title + legend
s.text(40, 42, L['title'], size=28, weight=800, family='d')
s.text(54 + len(L['title']) * 16.2, 42, L['sub'], size=13, fill=P['muted'], family='m')
lx, ly = 1010, 36
for color, dash, key, sw, ends in [(P['blue'], None, 'beam', 5, None), (P['cool'], None, 'two', 1.8, 'both'),
                                    (P['cool'], '5 4', 'read', 1.8, 'end'), (P['flame'], None, 'dm', 2.4, 'end'), (P['ok'], None, 'ship', 2.6, 'end')]:
    st = 'ah-cool' if ends == 'both' else None
    en = {'both': 'ah-cool', 'end': 'ah-cool' if color == P['cool'] else ('ah-flame' if color == P['flame'] else 'ah-ok'), None: None}[ends]
    s.path(f'M{lx},{ly - 4} H{lx + 28}', color, sw=sw, dash=dash, start=st, end=en, opacity=0.5 if key == 'beam' else None)
    s.text(lx + 36, ly, L['legend'][key], size=12, fill=P['muted'])
    lx += 56 + len(L['legend'][key]) * 6.4
s.use('shield', lx + 8, ly - 4, 18, P['amber']); s.text(lx + 22, ly, L['legend']['rule'], size=12, fill=P['muted'])
s.path('M30,60 H1890', P['line'], sw=1)

# watch loops band
s.zone_label(40, 84, L['loops_lbl'])
for i, (name, rule, when) in enumerate(L['loops']):
    x = 40 + i * 620
    s.rect(x, 94, 590, 70, fill=P['surface'], stroke=P['blue'], sw=1.2)
    s.use('loop', x + 30, 129, 30, P['blue'])
    s.text(x + 58, 122, name, size=16, weight=700, family='m', fill=P['fg'])
    if i != 1:
        s.use('hourglass', x + 570, 112, 16, P['amber'])
    s.text(x + 58, 143, rule, size=12, fill=P['muted'])
    s.text(x + 58, 158, when, size=12, fill=P['blue'])

FX, FY = 960, 470
# beams (behind everything in the middle band)
for poly in [f'{FX},{FY} 660,232 660,760', f'{FX},{FY} 1262,196 1262,468', f'{FX},{FY} 1262,500 1262,700']:
    s.add(f'<polygon points="{poly}" fill="{P["blue"]}" opacity="0.07"/>')
for (x2, y2) in [(660, 506), (1262, 332), (1262, 600)]:
    s.path(f'M{FX},{FY} L{x2},{y2}', P['blue'], sw=5, opacity=0.35)

# Discord outpost
s.rect(40, 186, 590, 584, fill=P['surface'], stroke=P['line'], rx=10)
s.use('chat', 72, 214, 28, P['fg'])
s.text(96, 220, L['discord'], size=18, weight=800, family='d', spacing='1')
s.text(220, 220, L['discord_sub'], size=12, fill=P['muted'])
s.path('M660,236 V754', P['cool'], sw=1.4)
for i, (name, sub, mode) in enumerate(L['doors']):
    y = 236 + i * 76
    s.rect(58, y, 554, 64, fill=P['bg'], stroke=P['cool'] if mode == 2 else P['muted'], sw=1.2, dash=None if mode == 2 else '5 4')
    s.use('mail' if name.startswith('DM') else 'chat', 88, y + 33, 26, P['cool'] if mode == 2 else P['muted'])
    s.text(114, y + 28, name, size=16, weight=700, family='m')
    s.text(114, y + 50, sub, size=12, fill=P['muted'])
    if mode == 2:
        s.path(f'M614,{y + 33} H656', P['cool'], sw=1.8, start='ah-cool', end='ah-cool')
    elif mode == 3:
        s.path(f'M614,{y + 24} H656', P['cool'], sw=1.6, dash='4 3', end='ah-cool')
        s.path(f'M656,{y + 42} H616', P['ok'], sw=1.8, end='ah-ok')
    else:
        s.path(f'M614,{y + 33} H656', P['cool'], sw=1.6, dash='4 3', end='ah-cool')
    if name.startswith('DM'):
        s.path(f'M614,{y + 33} H656', P['flame'], sw=2.4, start='ah-flame', end='ah-flame')

# GitHub outpost
s.rect(1290, 186, 590, 284, fill=P['surface'], stroke=P['line'], rx=10)
s.use('branch', 1322, 214, 28, P['fg'])
s.text(1346, 220, L['github'], size=18, weight=800, family='d', spacing='1')
s.text(1450, 220, L['github_sub'], size=12, fill=P['muted'])
s.path('M1262,236 V460', P['cool'], sw=1.4)
for i, (name, sub, mode) in enumerate(L['gh_tiles']):
    y = 236 + i * 76
    s.rect(1308, y, 554, 64, fill=P['bg'], stroke=P['cool'] if mode == 2 else P['ok'], sw=1.2)
    s.text(1334, y + 27, name, size=16, weight=700, family='d')
    s.text(1334, y + 48, sub, size=12, fill=P['muted'])
    if mode == 2:
        s.path(f'M1266,{y + 32} H1304', P['cool'], sw=1.8, start='ah-cool', end='ah-cool')
    else:
        s.path(f'M1266,{y + 32} H1304', P['ok'], sw=2.2, end='ah-ok')

# Tweakers outpost
s.rect(1290, 496, 590, 210, fill=P['surface'], stroke=P['line'], rx=10)
s.use('rss', 1322, 524, 28, P['fg'])
s.text(1346, 530, L['tweakers'], size=18, weight=800, family='d', spacing='1')
s.text(1480, 530, L['tweakers_sub'], size=12, fill=P['muted'])
s.path('M1262,560 V680', P['cool'], sw=1.4)
s.rect(1308, 556, 554, 120, fill=P['bg'], stroke=P['muted'], sw=1.2, dash='5 4')
s.text(1334, 590, L['tw_tile'][0], size=16, weight=700, family='d')
s.text(1334, 614, L['tw_tile'][1], size=12, fill=P['muted'])
s.text(1334, 650, 'gathering.tweakers.net · RSS', size=12, fill=P['muted'], family='m')
s.path('M1304,616 H1266', P['cool'], sw=1.6, dash='4 3', end='ah-cool')

# the flame
s.add(f'<circle cx="{FX}" cy="{FY}" r="104" fill="{P["blue_soft"]}" stroke="{P["blue"]}" stroke-width="2.2"/>')
flame(s, FX, FY + 4, 1.1)
s.text(FX, 618, L['bot'], size=26, weight=800, family='d', anchor='middle')
s.text(FX, 642, L['botsub'], size=13, fill=P['muted'], anchor='middle')

# shields around the flame: (x, y, label side)
SH = [(960, 300, 'top'), (762, 392, 'left'), (1158, 392, 'right'), (762, 556, 'left'), (1158, 556, 'right')]
for (l1, l2), (x, y, side) in zip([L['shields'][i] for i in (0, 4, 1, 3, 2)], SH):
    s.add(f'<circle cx="{x}" cy="{y}" r="22" fill="{P["bg"]}" stroke="{P["amber"]}" stroke-width="1.2"/>')
    s.use('shield', x, y, 26, P['amber'])
    if side == 'top':
        s.text(x, y - 46, l1, size=12, fill=P['amber'], anchor='middle', weight=600)
        s.text(x, y - 31, l2, size=12, fill=P['amber'], anchor='middle', weight=600)
    elif side == 'left':
        s.text(x - 30, y - 3, l1, size=12, fill=P['amber'], anchor='end', weight=600)
        s.text(x - 30, y + 12, l2, size=12, fill=P['amber'], anchor='end', weight=600)
    else:
        s.text(x + 30, y - 3, l1, size=12, fill=P['amber'], anchor='start', weight=600)
        s.text(x + 30, y + 12, l2, size=12, fill=P['amber'], anchor='start', weight=600)

# output bus
s.zone_label(40, 796, L['out_lbl'])
s.path(f'M{FX},660 V{808}', P['cool'], sw=2)
centers = [40 + i * 305 + 145 for i in range(6)]
s.path(f'M{centers[0]},808 H{centers[-1]}', P['cool'], sw=1.6)
for i, (icon, name, sub, tone) in enumerate(L['outs']):
    x = 40 + i * 305
    color = {'cool': P['cool'], 'flame': P['flame'], 'ok': P['ok']}[tone]
    fill = {'cool': P['surface'], 'flame': P['flame_soft'], 'ok': P['ok_soft']}[tone]
    s.path(f'M{centers[i]},808 V{836}', color, sw=2 if tone != 'cool' else 1.6, end={'cool': 'ah-cool', 'flame': 'ah-flame', 'ok': 'ah-ok'}[tone])
    s.rect(x, 840, 290, 92, fill=fill, stroke=color, sw=1.3)
    s.use(icon, x + 32, 886, 30, color)
    s.text(x + 60, 878, name, size=16, weight=700, family='d')
    s.text(x + 60, 902, sub, size=12, fill=P['muted'])

# foundation
s.path('M30,958 H1890', P['line'], sw=1)
s.text(40, 984, 'FUNDAMENT' if LANG == 'nl' else 'FOUNDATION', size=13, fill=P['muted'], weight=700, family='m', spacing='2')
for i, (icon, text) in enumerate(L['found']):
    x = 40 + i * 620
    s.rect(x, 996, 590, 56, fill=P['surface'], stroke=P['line'])
    s.use(icon, x + 30, 1024, 24, P['muted'])
    s.text(x + 56, 1029, text, size=14, fill=P['fg'], family='m')

open(sys.argv[1], 'w').write(s.render())
print('wrote', sys.argv[1])
