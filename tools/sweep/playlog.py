#!/usr/bin/env python3
"""Script errors from Jacob's own play sessions, grouped and attributed to a mod.

  python tools/sweep/playlog.py              # sessions not reported yet -> _sweep/reports/playlog-<date>.md
  python tools/sweep/playlog.py --days 7     # re-read the last week (ignores what was reported)
  python tools/sweep/playlog.py --dry-run    # print, don't remember anything

Headless tests have no player, so map, menus, possession, RTS camera and MP code mostly run when
Jacob plays. Every session log he leaves is free coverage: this reads the game client's logs
(Documents/My Games/ArmaReforger*/logs) and the hosted servers' (addons/*/.local-server/logs),
never test runs, and lists each distinct error once with its count, stack and the mod it comes from.
It files nothing; the report suggests what to look at. Noise list shared with sweep.py.
"""
import argparse, datetime as dt, json, re
from pathlib import Path

import sweep

STATE = sweep.DATA / 'playlog-state.json'
GAME_DOCS = Path.home() / 'Documents' / 'My Games'
OURS = [('ACA', r'ACR_|/ACR/|ArmaCommanderReforger'), ('BBB', r'BBB_|/BBB/'), ('AiO', r'AIO_|/AIO/'),
        ('MCC', r'MCC_|/MCC/'), ('R7', r'R7_'), ('CWL', r'CWL_'), ('SPG9', r'SPG9_')]
GUIDS = {v[1]: k for k, v in sweep.REPOS.items()}
TS = re.compile(r'^(\d\d:\d\d:\d\d\.\d{3})\s')
ERR = re.compile(r"SCRIPT\s+\(E\)|Can't compile|Too many instructions")


def log_sources():
    for profile in sorted(GAME_DOCS.glob('ArmaReforger*')):
        if 'workbench' in profile.name.lower():
            continue
        for log in profile.glob('logs/*/console.log'):
            yield 'client' + (' (Experimental)' if 'experimental' in profile.name.lower() else ''), log
    for log in sweep.ADDONS.glob('*/.local-server/logs/*/console.log'):
        yield 'hosted ' + log.parts[-5], log


def mod_of(text):
    for mod, pat in OURS:
        if re.search(pat, text):
            return mod
    return None


def signature(msg, frames):
    m = re.sub(r'0x[0-9A-Fa-f]+|\b\d+(\.\d+)?\b', '#', msg)
    m = re.sub(r'<[^>]*>|@"[^"]*"', '<>', m)
    return m.strip() + ' | ' + ' > '.join(frames[:3])


def parse_session(kind, log):
    text = sweep.read_text(log)
    lines = text.splitlines()
    info = dict(kind=kind, log=str(log), engine='', world='', mods=[], start='', end='', errors={},
                compile_failed=bool(re.search(r"Can't compile \"Game\" script module", text)))
    m = re.search(r'started at (\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)', text[:600])
    info['start'] = m.group(1) if m else dt.datetime.fromtimestamp(log.stat().st_mtime).strftime('%Y-%m-%d %H:%M:%S')
    m = re.search(r'Initializing engine, version (\d+)', text)
    info['engine'] = m.group(1) if m else ''
    m = re.search(r'CLI Params:.*?-(?:world|server)\s+"?([^\s"]+)', text)
    info['world'] = m.group(1) if m else ''
    m = re.search(r'Loaded addons:\n((?:.*gproj:.*\n)+)', text)
    if m:
        for g in re.findall(r"guid: '([0-9A-F]{16})'", m.group(1)):
            info['mods'].append(GUIDS.get(g, g))
    for line in reversed(lines):
        t = TS.match(line)
        if t:
            info['end'] = t.group(1)
            break
    i = 0
    while i < len(lines):
        line = lines[i]
        if not ERR.search(line):
            i += 1
            continue
        t = TS.match(line)
        when = t.group(1) if t else ''
        msg = re.sub(r'^.*?\(E\):\s*', '', line).strip()
        block, frames = [line], []
        j = i + 1
        while j < len(lines) and not TS.match(lines[j]) and j - i < 40:
            block.append(lines[j])
            fm = re.match(r'(Scripts/\S+\.c):(\d+) Function (\w+)', lines[j].strip())
            if fm:
                frames.append(f'{fm.group(1).split("/")[-1]}:{fm.group(2)} {fm.group(3)}')
            elif lines[j].startswith('Reason:'):
                msg += ' ' + lines[j].strip()
            elif lines[j].startswith('Class:'):
                msg += ' ' + lines[j].split(':', 1)[1].strip()
            j += 1
        whole = '\n'.join(block)
        i = j
        if sweep.NOISE_RE.search(whole):
            continue
        ours = [mod_of(f) for f in frames if mod_of(f)]
        mod = ours[0] if ours else (mod_of(msg) or 'vanilla/other')
        sig = signature(msg, frames)
        e = info['errors'].setdefault(sig, dict(mod=mod, mods=sorted(set(ours)), msg=msg, frames=frames,
                                                count=0, first=when, last=when))
        e['count'] += 1
        e['last'] = when
    return info


def harvest(days=None, remember=True):
    state = json.loads(sweep.read_text(STATE) or '{}') if STATE.exists() else {}
    seen = set(state.get('seen', []))
    cutoff = dt.datetime.now() - dt.timedelta(days=days) if days else None
    playing = sweep.jacob_playing()
    sessions = []
    for kind, log in log_sources():
        mtime = dt.datetime.fromtimestamp(log.stat().st_mtime)
        if cutoff and mtime < cutoff:
            continue
        if not cutoff and str(log) in seen:
            continue
        if playing and (dt.datetime.now() - mtime).total_seconds() < 600:
            continue   # still being written
        if not days and not seen and (dt.datetime.now() - mtime).days > 7:
            continue   # first run: a week back is enough
        s = parse_session(kind, log)
        sessions.append(s)
        seen.add(str(log))
    if remember and not days:
        sweep.DATA.mkdir(parents=True, exist_ok=True)
        STATE.write_text(json.dumps({'seen': sorted(seen)}, indent=0), encoding='utf-8')
    return sorted(sessions, key=lambda s: s['start'])


def render(sessions):
    if not sessions:
        return ['## Your play sessions', '', '- no new session logs since the last report', '']
    L = ['## Your play sessions', '']
    total = {}
    for s in sessions:
        for sig, e in s['errors'].items():
            t = total.setdefault((e['mod'], sig), dict(e, count=0, sessions=0))
            t['count'] += e['count']
            t['sessions'] += 1
    ours = sorted([v for (m, _), v in total.items() if m != 'vanilla/other'], key=lambda v: (-v['sessions'], -v['count']))
    other = sorted([v for (m, _), v in total.items() if m == 'vanilla/other'], key=lambda v: -v['count'])
    L.append(f"{len(sessions)} sessions, {len(ours)} distinct errors from our mods, {len(other)} vanilla/other.")
    L.append('')
    for s in sessions:
        if s['compile_failed']:
            mods = ', '.join(m for m in s['mods'] if not re.fullmatch(r'[0-9A-F]{16}', m)) or '-'
            L.append(f"**{s['start']} ({s['kind']}): the Game script module failed to compile** with {len(s['mods'])} mods loaded "
                     f"({mods}, plus Workshop mods). If it says 'Too many instructions per function', that's the shared "
                     f"static-initializer budget (ACA HANDBOOK Pitfalls). `{s['log']}`")
            L.append('')
    L.append('| started | where | world | engine | our mods loaded | errors (ours) |')
    L.append('|---|---|---|---|---|---|')
    for s in sessions:
        mods = [m for m in s['mods'] if m in sweep.REPOS]
        n_ours = sum(e['count'] for e in s['errors'].values() if e['mod'] != 'vanilla/other')
        L.append(f"| {s['start']} | {s['kind']} | {s['world'].split('/')[-1]} | {s['engine']} | {', '.join(mods) or '-'} | {n_ours} |")
    L += ['', '### Errors from our mods (most sessions first)', '']
    for v in ours or []:
        via = f" (stack through {', '.join(v['mods'])})" if len(v['mods']) > 1 else ''
        L.append(f"- **{v['mod']}**{via}: {v['msg'][:200]} - {v['count']}x in {v['sessions']} session(s)")
        cut = next((k for k, f in enumerate(v['frames']) if mod_of(f)), 0)
        shown = v['frames'][:3] + (['...'] if cut > 3 else []) + v['frames'][max(3, cut):cut + 2]
        for f in shown:
            L.append(f'  - `{f}`' if f != '...' else '  - ...')
    if not ours:
        L.append('- none')
    L += ['', '### Vanilla / other mods (top 10)', '']
    L += [f"- {v['msg'][:160]} - {v['count']}x" + (f" at `{v['frames'][0]}`" if v['frames'] else '') for v in other[:10]] or ['- none']
    L.append('')
    return L


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--days', type=int, help='re-read logs from the last N days')
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()
    lines = render(harvest(a.days, remember=not a.dry_run))
    if a.dry_run:
        print('\n'.join(lines))
        return
    out = sweep.DATA / 'reports' / f'playlog-{dt.date.today().isoformat()}.md'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text('# Play-session errors\n\n' + '\n'.join(lines) + '\n', encoding='utf-8')
    print(f'report: {out}')


if __name__ == '__main__':
    main()
