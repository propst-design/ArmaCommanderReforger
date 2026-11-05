#!/usr/bin/env python3
"""Nightly test sweep across Jacob's Reforger mods (ACA, BBB, AiO, MCC, R7, CWL, SPG9).

  python tools/sweep/sweep.py ingest            # add every test result on this PC to the history
  python tools/sweep/sweep.py run [--tier nightly|weekly] [--only ACA,BBB] [--match REGEX] [--until 08:00] [--dry-run]
  python tools/sweep/sweep.py report [--days 7]  # write the morning report
  python tools/sweep/sweep.py stats [REPO]       # per-preset pass rate / duration from the history
  python tools/sweep/sweep.py tested ACA [REF]   # what passed at a commit (before merging a PR)

Tests run headless only (hidden dedicated servers, never the game client), one at a time, each
under the shared _wt/test-server.lock (see _wt/lockrun.sh), on detached worktrees of each repo's
main in _wt/sweep/. Results go to _sweep/ beside addons/ (history.csv, results/, reports/), which
outlives the watchers' 14-day pruning. The plan (what runs in which tier) is tools/sweep/plan.txt.
"""
import argparse, csv, datetime as dt, os, re, shutil, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent


def find_root():
    if os.environ.get('SWEEP_ROOT'):
        return Path(os.environ['SWEEP_ROOT'])
    # Works from the main checkout and from any worktree: the common .git is addons/ACA/.git.
    out = subprocess.run(['git', '-C', str(HERE), 'rev-parse', '--git-common-dir'], capture_output=True, text=True)
    common = Path(out.stdout.strip())
    if not common.is_absolute():
        common = (HERE / common).resolve()
    return common.parent.parent.parent


ROOT = find_root()                      # ...\ArmaReforgerWorkbench
ADDONS = ROOT / 'addons'
WT = ROOT / '_wt' / 'sweep'             # the sweep's own worktrees (detached at main)
DATA = ROOT / '_sweep'
HISTORY = DATA / 'history.csv'
LOCK = ROOT / '_wt' / 'test-server.lock'
LOCK_STALE = 2400                       # seconds; same rule as _wt/lockrun.sh
ME = 'nightly-sweep'
WORKSHOP = Path(r'E:\ArmaReforgerMods\addons')

# folder, mod GUID, has a GitHub remote
REPOS = {
    'ACA': ('ArmaCommanderReforger', 'AC230926A0010001', True),
    'BBB': ('BetterBattleBuddies', 'BBB0261015000001', True),
    'AIO': ('AIOCommandMenu', '629405AEFC950C12', True),
    'MCC': ('MCCSandboxReforger', '7A3FC4563CFC8DE3', False),
    'R7': ('RPG7Rounds', 'E1449AD34E401261', False),
    'CWL': ('ColdWarLaunchers', 'A23FD9596646EACD', False),
    'SPG9': ('SPG9', '2AB715B4B16B5824', False),
    'ACACW': ('ACAColdWar', 'AC230926CC000002', True),
    'ACAMOD': ('ACAModern', 'AC230926DD000003', True),
}
FOLDER_TO_REPO = {v[0].lower(): k for k, v in REPOS.items()}

FIELDS = ['started', 'repo', 'preset', 'mode', 'build', 'bbb', 'status', 'verdict', 'passed', 'failed',
          'skipped', 'missing', 'expected', 'seconds', 'errors', 'origin', 'file', 'note']

# Vanilla noise, same list as Remove-KnownNoise in tools/ACR-Watcher.ps1 (keep them in step).
NOISE = [
    r'SCR_InstigatorContextData: instigatorType',
    r'SCR_AIProcessFailedMovementResult',
    r'No suppression volume provided',
    r"Variable 'agent'\s+Class:\s+'SCR_AIOccupyDriversInUsedVehicles'",
    r'Math\.RandomFloat: invalid parameters min = 0\.100000 max = 0\.000000',
    r'SCR_ResupplySelfSupportStationAction.*no storage manager found',
    r'SCR_AISoundHandling: SetSignalsAndTransmit',
    r'SCR_AICommsHandler: SCR_AISoundHandling',
    r'SCR_SpawnLogic|SCR_MapUIElementContainer|SCR_PlayerArsenalLoadout|ScenarioFramework|SpinningWidget|SCR_ScenarioUICommon',
]
NOISE_RE = re.compile('|'.join(NOISE))
ERROR_RE = re.compile(r"SCRIPT\s+\(E\)|Can't compile|Virtual Machine Exception")


# ---------------------------------------------------------------- parsing results

def read_text(p):
    try:
        return Path(p).read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return ''


def header(text):
    h = {}
    for line in text.splitlines()[:12]:
        if line.startswith('---'):
            break
        if '=' in line and not line.startswith(' '):
            k, v = line.split('=', 1)
            h[k.strip()] = v.strip()
    return h


def count_errors(text):
    n = 0
    lines = text.splitlines()
    for i, line in enumerate(lines):
        if ERROR_RE.search(line):
            ctx = ' '.join(lines[i:i + 3])  # a VM exception names its class on the next lines
            if not NOISE_RE.search(ctx):
                n += 1
    return n


def kv(s):
    return dict(re.findall(r'(\w+)=(\S+)', s))


def judge(status, text, preset):
    """-> (verdict, passed, failed, skipped, missing, expected)"""
    st = (status or '').lower()
    if st and st not in ('completed', 'done', 'failed'):   # Test-BBB: failed = finished with VERDICT FAIL
        if 'refused' in st or 'busy' in st:
            return 'REFUSED', '', '', '', '', ''
        if 'timeout' in st:
            return 'TIMEOUT', '', '', '', '', ''
        return 'ERROR', '', '', '', '', ''
    verdicts = re.findall(r'\[\w+\] VERDICT (PASS|FAIL)([^\r\n]*)', text)
    if verdicts:
        f = {}
        for _, rest in verdicts:
            for k, v in kv(rest).items():
                if v.isdigit():
                    f[k] = f.get(k, 0) + int(v)
        passed = f.get('passed', f.get('checks', 0) - f.get('failed', 0) if 'checks' in f else '')
        bad = any(v == 'FAIL' for v, _ in verdicts)
        exp = f.get('expected', '')
        verdict = 'FAIL' if bad else ('PASS' if exp != '' else 'PASS-UNDECLARED')
        return verdict, passed, f.get('failed', ''), f.get('skipped', ''), f.get('missing', ''), exp
    m = re.search(r'\[AIO\] TEST RESULT (\d+) passed, (\d+) failed', text) or \
        re.search(r'\[MCC\] TEST DONE (\d+) checks, (\d+) failed', text)
    if m:
        a, b = int(m.group(1)), int(m.group(2))
        if 'checks' in m.group(0):
            a -= b
        if b:
            return 'FAIL', a, b, '', '', ''
        return ('PASS-UNDECLARED' if a else 'NOVERDICT'), a, b, '', '', ''
    if st == 'failed':
        return 'FAIL', '', '', '', '', ''
    if preset.split(':')[-1].split('-')[0] == 'compile' or preset.startswith('matrix:'):
        return 'PASS', '', '', '', '', ''   # a boot that reached its done marker
    return 'NOVERDICT', '', '', '', '', ''


def origin_of(path):
    parts = [x.lower() for x in Path(path).parts]
    if '_sweep' in parts:
        return 'sweep'
    if '_wt' in parts:
        return parts[parts.index('_wt') + 1]
    return 'main'


def repo_of(path):
    queue = {'.acr-requests': 'ACA', '.aio-requests': 'AIO', '.bbb-results': 'BBB'}
    for part in Path(path).parts:
        if part.lower() in queue:
            return queue[part.lower()]
    for part in reversed(Path(path).parts):
        r = FOLDER_TO_REPO.get(part.lower())
        if r:
            return r
    return '?'


def parse_result(path, repo=None):
    path = Path(path)
    text = read_text(path)
    if not text:
        return None
    h = header(text)
    repo = repo or repo_of(path)
    preset = h.get('preset', '')
    mode = 'client'
    for m in ('srv', 'pak', 'matrix'):
        if preset.startswith(m + ':'):
            mode = m
    if repo == 'AIO' and mode == 'client':
        mode = 'srv'   # AiO's plain presets are dedicated-server tests
    if repo in ('BBB', 'MCC', 'R7', 'CWL', 'SPG9'):
        mode = 'srv' if mode == 'client' else mode
    name = preset.split(':', 1)[1] if mode in ('srv', 'pak') and ':' in preset else preset
    build, bbb = h.get('build', ''), h.get('bbb', '')
    bfile = path.with_name(path.name.replace('.result.txt', '.build.txt'))
    if not build and bfile.exists():
        label = read_text(bfile).strip().split()
        build = label[0] if label else ''
        if 'bbb' in label:
            bbb = label[label.index('bbb') + 1] if len(label) > label.index('bbb') + 1 else ''
    if not build and h.get('source'):
        build = h['source'][:9] + ('+dirty' if h['source'].endswith('+dirty') else '')
    started = h.get('started', '')
    if not started:
        started = dt.datetime.fromtimestamp(path.stat().st_mtime).strftime('%Y-%m-%dT%H:%M:%S')
    verdict, passed, failed, skipped, missing, expected = judge(h.get('status', ''), text, preset)
    seconds = h.get('seconds', '')
    ts = re.match(r'(\d{8}-\d{6})-', path.name)   # Test-BBB ids are start stamps; the file is written at the end
    if not seconds and ts:
        t0 = dt.datetime.strptime(ts.group(1), '%Y%m%d-%H%M%S')
        seconds = str(max(0, int(path.stat().st_mtime - t0.timestamp())))
        if not h.get('started'):
            started = t0.strftime('%Y-%m-%dT%H:%M:%S')
    return dict(started=started, repo=repo, preset=name, mode=mode, build=build, bbb=bbb,
                status=h.get('status', ''), verdict=verdict, passed=passed, failed=failed, skipped=skipped,
                missing=missing, expected=expected, seconds=seconds, errors=count_errors(text),
                origin=origin_of(path), file=str(path), note=h.get('reason', ''))


# ---------------------------------------------------------------- history

def load_history():
    if not HISTORY.exists():
        return []
    with HISTORY.open(newline='', encoding='utf-8') as f:
        return list(csv.DictReader(f))


def append_history(rows):
    DATA.mkdir(parents=True, exist_ok=True)
    new = not HISTORY.exists()
    with HISTORY.open('a', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        if new:
            w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k, '') for k in FIELDS})


def result_files():
    pats = ['.acr-requests/*.result.txt', '.aio-requests/*.result.txt', '.bbb-results/*.result.txt']
    roots = [ADDONS] + sorted((ROOT / '_wt').glob('*'))
    seen = set()
    for base in roots:
        if not base.is_dir():
            continue
        for repo_dir in [base] + [d for d in base.iterdir() if d.is_dir()]:
            for pat in pats:
                for p in repo_dir.glob(pat):
                    key = str(p.resolve()).lower()
                    if key not in seen:
                        seen.add(key)
                        yield p
    yield from (DATA / 'results').glob('**/*.result.txt')


def cmd_ingest(args):
    known = {r['file'].lower() for r in load_history()}
    rows = []
    for p in result_files():
        if str(p).lower() in known:
            continue
        r = parse_result(p)
        if r and r['preset']:
            rows.append(r)
    rows.sort(key=lambda r: r['started'])
    append_history(rows)
    print(f'ingested {len(rows)} new results -> {HISTORY}')


# ---------------------------------------------------------------- plan

def load_plan():
    """plan.txt: REPO|PRESET|TIER[|timeout s]   # note. Matrix rows: matrix|name|...|tier (see file)."""
    tests, matrix = [], {}
    for line in (HERE / 'plan.txt').read_text(encoding='utf-8').splitlines():
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        f = [x.strip() for x in line.split('|')]
        if f[0] == 'matrix':
            # matrix|name|server|world|addons (comma GUIDs)|done regex|tier
            matrix[f[1]] = dict(server=f[2], world=f[3], addons=f[4], done=f[5], tier=f[6])
            tests.append(dict(repo='MATRIX', preset='matrix:' + f[1], tier=f[6], timeout=600, extra='', label='matrix:' + f[1]))
        else:
            extra = f[4] if len(f) > 4 else ''
            tests.append(dict(repo=f[0], preset=f[1], tier=f[2], timeout=int(f[3]) if len(f) > 3 and f[3] else 0,
                              extra=extra, label=f'{f[1]} [{extra}]' if extra else f[1]))
    return tests, matrix


def in_tier(test_tier, tier):
    if tier == 'weekly':
        return test_tier in ('nightly', 'weekly', 'track', 'control')
    return test_tier in ('nightly', 'track')


# ---------------------------------------------------------------- environment

def git(repo_dir, *a, check=True):
    r = subprocess.run(['git', '-C', str(repo_dir)] + list(a), capture_output=True, text=True)
    if check and r.returncode:
        raise RuntimeError(f'git {" ".join(a)} in {repo_dir}: {r.stderr.strip()}')
    return r.stdout.strip()


def prepare_worktree(repo):
    folder, _, remote = REPOS[repo]
    main = ADDONS / folder
    if not (main / '.git').exists():
        return None
    wt = WT / folder
    if remote:
        git(main, 'fetch', '-q', 'origin', 'main', check=False)
        ref = 'origin/main'
    else:
        ref = 'main'
    if not wt.exists():
        WT.mkdir(parents=True, exist_ok=True)
        git(main, 'worktree', 'add', '-q', '--detach', str(wt), ref)
    else:
        git(wt, 'checkout', '-q', '--detach', '--force', ref)
        git(wt, 'clean', '-q', '-fd', '-e', '.local-*', '-e', '.acr-requests', '-e', '.aio-requests',
            '-e', '.bbb-results', '-e', 'resourceDatabase.rdb')
    # Fresh worktrees have no resourceDatabase.rdb (gitignored): without it the mod's own prefab
    # GUIDs don't resolve on a server (1 Nov overnight). Take the main checkout's.
    rdb = main / 'resourceDatabase.rdb'
    if rdb.exists() and not (wt / 'resourceDatabase.rdb').exists():
        shutil.copy2(rdb, wt / 'resourceDatabase.rdb')
    return wt


def build_label(repo):
    wt = WT / REPOS[repo][0]
    return git(wt, 'log', '-1', '--format=%h', check=False) if wt.exists() else ''


def proc_running(*names):
    out = subprocess.run(['tasklist', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout.lower()
    return [n for n in names if f'"{n.lower()}' in out]


def jacob_playing():
    # The game client (not our hidden servers): stop the sweep so it doesn't eat his frames.
    return bool(proc_running('ArmaReforgerSteam.exe', 'ArmaReforgerSteamDiag.exe'))


def pending_requests():
    for folder, q in (('ArmaCommanderReforger', '.acr-requests'), ('AIOCommandMenu', '.aio-requests')):
        if list((ADDONS / folder / q).glob('*.request')):
            return True
    return False


def take_lock(deadline):
    while time.time() < deadline:
        if LOCK.exists():
            age = time.time() - LOCK.stat().st_mtime
            owner = read_text(LOCK)
            if ME not in owner and age < LOCK_STALE:
                time.sleep(30)
                continue
        if pending_requests():
            time.sleep(30)
            continue
        LOCK.write_text(f'{ME} {dt.datetime.now():%Y-%m-%d %H:%M:%S}\n')
        return True
    return False


def touch_lock():
    if LOCK.exists() and ME in read_text(LOCK):
        LOCK.touch()


def drop_lock():
    if LOCK.exists() and ME in read_text(LOCK):
        LOCK.unlink()


def find_server(experimental=False):
    name = 'Arma Reforger Experimental Server' if experimental else 'Arma Reforger Server'
    for drive in 'CDEFG':
        for lib in (f'{drive}:\\SteamLibrary', f'{drive}:\\Program Files (x86)\\Steam'):
            exe = Path(lib) / 'steamapps' / 'common' / name / 'ArmaReforgerServer.exe'
            if exe.exists():
                return exe
    return None


# ---------------------------------------------------------------- runners

NO_WINDOW = 0x08000000


class Watcher:
    """Starts the repo's own test launcher on the sweep worktree (hidden), queues requests to it."""

    def __init__(self, repo):
        self.repo = repo
        self.wt = WT / REPOS[repo][0]
        self.queue = self.wt / ('.acr-requests' if repo == 'ACA' else '.aio-requests')
        script = self.wt / 'tools' / ('ACR-Watcher.ps1' if repo == 'ACA' else 'AIO-Watcher.ps1')
        log = open(DATA / f'watcher-{repo}.log', 'a', encoding='utf-8')
        self.proc = subprocess.Popen(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script)],
                                     cwd=str(self.wt), stdout=log, stderr=subprocess.STDOUT, creationflags=NO_WINDOW)

    def running(self):
        # A launcher that found another one holding its mutex exits at once (AiO's mutex was one
        # per PC until AiO's per-checkout launcher change; Jacob's own launcher would block us).
        return self.proc.poll() is None

    def run(self, rid, preset, timeout):
        if not self.running():
            return write_result(rid, self.repo, preset, 'refused', 0,
                                'reason=the test launcher exited at start (another launcher holds its mutex?): '
                                + read_text(DATA / f'watcher-{self.repo}.log')[-300:], build_label(self.repo))
        self.queue.mkdir(exist_ok=True)
        (self.queue / f'{rid}.build.txt').write_text(f'{build_label(self.repo)} bbb {build_label("BBB")}')
        (self.queue / f'{rid}.request').write_text(preset + '\n')
        result = self.queue / f'{rid}.result.txt'
        end = time.time() + timeout + 300
        while time.time() < end:
            if result.exists():
                time.sleep(2)
                return result
            touch_lock()
            time.sleep(5)
        (self.queue / f'{rid}.request').unlink(missing_ok=True)
        return None

    def stop(self):
        if self.proc.poll() is None:
            subprocess.run(['taskkill', '/PID', str(self.proc.pid), '/T', '/F'], capture_output=True)


def run_script(rid, repo, preset, timeout, extra=''):
    """BBB, MCC and the weapons add-ons: their runner scripts, stdout captured into our result file."""
    wt = WT / REPOS[repo][0]
    if repo == 'BBB':
        cmd = ['-File', str(wt / 'tools' / 'Test-BBB.ps1'), '-Preset', preset, '-AddonRoot', str(wt),
               '-WaitForServerSeconds', '900'] + (['-ExtraArgs', extra] if extra else [])
    else:
        runner = next((wt / 'tools').glob('Run-*-Test.ps1'), None)
        if not runner:
            return write_result(rid, repo, preset, 'refused', 0, 'reason=no tools/Run-*-Test.ps1 on main yet', build_label(repo))
        cmd = ['-File', str(runner), preset]
    t0 = time.time()
    try:
        r = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass'] + cmd, cwd=str(wt),
                           capture_output=True, text=True, timeout=timeout + 960, creationflags=NO_WINDOW)
        out, code = r.stdout + r.stderr, r.returncode
    except subprocess.TimeoutExpired as e:
        out, code = (e.stdout or '') if isinstance(e.stdout, str) else '', -1
    secs = int(time.time() - t0)
    m = re.search(r'result=(.+?\.result\.txt)', out)   # Test-BBB's own result file (path has spaces)
    if repo == 'BBB' and m and Path(m.group(1)).exists():
        body = read_text(m.group(1))
        status = header(body).get('status', 'error')
    else:
        body = out
        # MCC: "status=done log=...", weapons runners: "RESULT PASS|FAIL preset=x status=done ...":
        # a finished run is 'completed' and its VERDICT line decides; anything else is an error.
        sm = re.search(r'status=(\w+)', out)
        status = (sm.group(1) if sm else ('completed' if code == 0 else 'error'))
        status = 'completed' if status == 'done' else status
    return write_result(rid, repo, preset, status, secs, body, build_label(repo))


def write_result(rid, repo, preset, status, secs, body, build):
    day = DATA / 'results' / dt.date.today().isoformat()
    day.mkdir(parents=True, exist_ok=True)
    p = day / f'{rid}.result.txt'
    started = (dt.datetime.now() - dt.timedelta(seconds=secs)).strftime('%Y-%m-%dT%H:%M:%S')
    folder = REPOS.get(repo, ('',))[0]
    p.write_text('\n'.join([f'status={status}', f'preset={preset}', f'started={started}', f'seconds={secs}',
                            f'build={build}', f'repo={folder}', '---', body]), encoding='utf-8')
    return p


def run_matrix(rid, name, spec, timeout):
    """Boot a dedicated server with a mod set and stop at the done marker or the first script error.
    Catches what single-mod runs can't: the shared static-initializer budget (2 Nov), GUID clashes,
    a mod that only breaks beside another. Experimental rows track 1.9 readiness."""
    server = find_server(spec['server'] == 'experimental')
    if not server:
        return write_result(rid, 'MATRIX', f'matrix:{name}', 'refused', 0, f'reason={spec["server"]} server not installed', '')
    dirs = [str(WT)] + ([str(WORKSHOP)] if WORKSHOP.exists() else [])
    profile = DATA / 'matrix-profile' / name
    shutil.rmtree(profile, ignore_errors=True)
    profile.mkdir(parents=True)
    world = spec['world']
    args = [str(server), '-server', world, '-addonsDir', ','.join(dirs), '-addons', spec['addons'],
            '-profile', str(profile), '-maxFPS', '60', '-noBackend']
    t0 = time.time()
    proc = subprocess.Popen(args, cwd=str(server.parent), creationflags=NO_WINDOW,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    status, text = 'timeout', ''
    done = re.compile(spec['done'])
    try:
        while time.time() - t0 < timeout:
            time.sleep(3)
            touch_lock()
            logs = sorted(profile.glob('logs/*/console.log'))
            if logs:
                text = read_text(logs[-1])
                if re.search(r"Can't compile|SCRIPT\s+\(E\)|Too many instructions", text):
                    time.sleep(3)
                    text = read_text(logs[-1])
                    status = 'error'
                    break
                if done.search(text):
                    status = 'completed'
                    break
            if proc.poll() is not None:
                status = 'server exited'
                break
    finally:
        if proc.poll() is None:
            subprocess.run(['taskkill', '/PID', str(proc.pid), '/T', '/F'], capture_output=True)
    builds = ' '.join(f'{r}={build_label(r)}' for r in REPOS if (WT / REPOS[r][0]).exists())
    # Script errors first (Workshop mods' own RESOURCES (E) noise stays out), then our mods' start
    # lines and the end of the boot: a long list of mod GUID warnings must not hide how it ended.
    lines = text.splitlines()
    errs = [l for l in lines if re.search(r"SCRIPT\s+\(E\)|Can't compile|Too many instructions", l)]
    ours = [l for l in lines if re.search(r'\[(ACR|BBB|AIO|MCC|R7|CWL|SPG9)\]', l)]
    body = (f'server={server}\naddons={spec["addons"]}\nworld={world}\n'
            + '\n'.join(errs[:100] + ['...'] + ours[:20] + ['...'] + lines[-60:]))
    return write_result(rid, 'MATRIX', f'matrix:{name}', status, int(time.time() - t0), body, builds)


# ---------------------------------------------------------------- run

def cmd_run(args):
    tests, matrix = load_plan()
    only = set(args.only.split(',')) if args.only else None
    until = None
    if args.until:
        hh, mm = map(int, args.until.split(':'))
        until = dt.datetime.now().replace(hour=hh, minute=mm, second=0)
        if until <= dt.datetime.now():
            until += dt.timedelta(days=1)
    todo = [t for t in tests if in_tier(t['tier'], args.tier) and (not only or t['repo'] in only)]
    if args.match:   # e.g. --match "regress|compile": a few rows, any tier but skip
        todo = [t for t in tests if t['tier'] != 'skip' and re.search(args.match, t['label'])
                and (not only or t['repo'] in only)]
    if args.tier == 'nightly' and not args.match:
        # Rotation: after the nightly rows, weekly rows that waited longest since their last sweep
        # run, until --until. Everything gets covered over a few nights without a long weekend run.
        last = {}
        for r in load_history():
            if r['origin'] == 'sweep':
                last[(r['repo'], r['preset'])] = max(last.get((r['repo'], r['preset']), ''), r['started'])
        rest = [t for t in tests if t['tier'] in ('weekly', 'control') and (not only or t['repo'] in only)]
        rest.sort(key=lambda t: last.get((t['repo'], t['label'].split(':', 1)[-1] if t['repo'] != 'MATRIX' else t['label']), ''))
        todo += rest if until else []
    if args.dry_run:
        total = sum(t['timeout'] or 300 for t in todo)
        for t in todo:
            print(f"{t['repo']:6} {t['label']:32} {t['tier']}")
        print(f'{len(todo)} runs, worst case ~{total / 3600:.1f} h of timeouts')
        return
    DATA.mkdir(parents=True, exist_ok=True)
    repos = {t['repo'] for t in todo} - {'MATRIX'}
    if any(t['repo'] == 'MATRIX' for t in todo):
        repos |= {r for r in REPOS if (ADDONS / REPOS[r][0] / '.git').exists()}
    if repos & {'ACA', 'AIO'}:
        repos |= {'ACA', 'BBB'}   # ACA srv: loads the sibling BBB; AiO -aca presets pack both
    for r in sorted(repos):
        wt = prepare_worktree(r)
        print(f'{r}: {"main " + build_label(r) if wt else "not checked out, skipped"}', flush=True)
    stamp = dt.datetime.now().strftime('%m%d-%H%M')
    watchers = {}
    log = DATA / 'sweep.log'
    ran = []
    try:
        for i, t in enumerate(todo):
            if until and dt.datetime.now() >= until:
                print('reached --until, stopping', flush=True)
                break
            waited = False
            while jacob_playing() and not (until and dt.datetime.now() >= until):
                if not waited:
                    print(f'{dt.datetime.now():%H:%M} game client open: waiting for it to close', flush=True)
                    waited = True
                time.sleep(60)
            if until and dt.datetime.now() >= until:
                print('reached --until, stopping', flush=True)
                break
            repo, preset = t['repo'], t['preset']
            if repo != 'MATRIX' and not (WT / REPOS[repo][0]).exists():
                continue
            timeout = t['timeout'] or 600
            if not take_lock(time.time() + 3 * 3600):
                print('lock held for 3 h: stopping', flush=True)
                break
            rid = f'sw{stamp}-{i:03d}-{repo}-{re.sub(r"[^A-Za-z0-9]+", "-", preset)}'
            t0 = time.time()
            try:
                if repo in ('ACA', 'AIO'):
                    if repo not in watchers:
                        watchers[repo] = Watcher(repo)
                        time.sleep(8)
                    res = watchers[repo].run(rid, preset, timeout)
                    if res:   # keep a copy: the watcher prunes after 14 days
                        day = DATA / 'results' / dt.date.today().isoformat()
                        day.mkdir(parents=True, exist_ok=True)
                        text = read_text(res)
                        label = read_text(res.with_name(f'{rid}.build.txt')).split()
                        extra = f'build={label[0] if label else ""}\nbbb={label[2] if len(label) > 2 else ""}\n'
                        hdr, _, body = text.partition('---')
                        (day / res.name).write_text(hdr + extra + '---' + body, encoding='utf-8')
                        res = day / res.name
                    else:
                        res = write_result(rid, repo, preset, 'timeout', int(time.time() - t0), 'no result from the watcher', build_label(repo))
                elif repo == 'MATRIX':
                    name = preset.split(':', 1)[1]
                    res = run_matrix(rid, name, matrix[name], timeout)
                else:
                    res = run_script(rid, repo, preset, timeout, t.get('extra', ''))
            finally:
                drop_lock()
            row = parse_result(res, repo='MATRIX' if repo == 'MATRIX' else repo) if res else None
            if row:
                row['origin'] = 'sweep'
                if t.get('extra'):   # a control: same preset with a feature switched off
                    row['preset'] = row['preset'] + f" [{t['extra']}]"
                    row['note'] = 'control'
                append_history([row])
                ran.append(row)
                line = f"{dt.datetime.now():%H:%M} {repo:6} {preset:30} {row['verdict']:16} {row['seconds']}s err={row['errors']}"
            else:
                line = f'{dt.datetime.now():%H:%M} {repo:6} {preset:30} (no result)'
            print(line, flush=True)
            with log.open('a', encoding='utf-8') as f:
                f.write(rid + ' ' + line + '\n')
    finally:
        for w in watchers.values():
            w.stop()
        drop_lock()
    if ran:
        cmd_report(argparse.Namespace(days=7, out=None))


# ---------------------------------------------------------------- report

def ok(v):
    return v in ('PASS', 'PASS-UNDECLARED')


def num(x, default=0.0):
    try:
        return float(x)
    except (TypeError, ValueError):
        return default


def cmd_stats(args):
    rows = [r for r in load_history() if not args.repo or r['repo'] == args.repo]
    by = {}
    for r in rows:
        if r['mode'] in ('srv', 'pak', 'matrix'):
            by.setdefault((r['repo'], r['preset']), []).append(r)
    print(f"{'repo':6} {'preset':30} {'runs':>4} {'pass':>4} {'med s':>6}  last")
    for (repo, preset), rs in sorted(by.items()):
        rs.sort(key=lambda r: r['started'])
        secs = sorted(num(r['seconds']) for r in rs if r['seconds'])
        med = secs[len(secs) // 2] if secs else 0
        print(f"{repo:6} {preset:30} {len(rs):4} {sum(ok(r['verdict']) for r in rs):4} {med:6.0f}  {rs[-1]['verdict']} {rs[-1]['started'][:10]}")


def cmd_tested(args):
    """What passed at a commit (any session's runs): the evidence to read before merging a PR."""
    folder = REPOS[args.repo][0]
    repo_dir = ADDONS / folder
    ref = args.ref or 'HEAD'
    full = git(repo_dir, 'rev-parse', ref, check=False)
    if not full:
        sys.exit(f'unknown ref {ref} in {folder}')
    rows = [r for r in load_history() if r['repo'] == args.repo and r['build'] and full.startswith(r['build'].split('+')[0])]
    if not rows:
        print(f'{args.repo} {full[:9]}: no test results at this commit on this PC')
        return 1
    print(f'{args.repo} {full[:9]} ({git(repo_dir, "log", "-1", "--format=%s", full, check=False)[:70]})')
    last = {}
    for r in sorted(rows, key=lambda r: r['started']):
        last[r['preset']] = r
    for name, r in sorted(last.items()):
        dirty = ' (uncommitted changes on top)' if '+dirty' in r['build'] else ''
        bbb = f" with BBB {r['bbb']}" if r.get('bbb') else ''
        print(f"  {r['verdict']:16} {name:28} {r['passed'] or ''}/{r['failed'] or ''} {r['started'][:16]} [{r['origin']}]{bbb}{dirty}")
    must = 'regress' if args.repo == 'ACA' else 'compile'   # a passing regress has compiled too
    if must not in last or not ok(last[must]['verdict']) or '+dirty' in last[must]['build']:
        print(f'  MISSING: no clean passing {must} at this commit')
    return 0


def cmd_report(args):
    rows = load_history()
    since = (dt.datetime.now() - dt.timedelta(days=args.days)).strftime('%Y-%m-%dT%H:%M:%S')
    sweep = [r for r in rows if r['origin'] == 'sweep']
    if not sweep:
        print('no sweep runs in the history yet')
        return
    sys.path.insert(0, str(HERE))
    last_day = max(r['started'][:10] for r in sweep)
    # The latest sweep = sweep rows from its start (a sweep crosses midnight).
    # Its tag is in each result's name (sw<MMDD-HHMM>-...); paths differ by repo, so compare tags.
    tags = [m.group(1) for m in (re.search(r'sw(\d{4}-\d{4})-', r['file']) for r in sweep) if m]
    tag = getattr(args, 'tag', None) or (max(tags) if tags else '')
    tonight = [r for r in sweep if f'sw{tag}-' in r['file']] if tag else [r for r in sweep if r['started'][:10] == last_day]
    prev = {}
    for r in sorted(sweep, key=lambda r: r['started']):
        if r not in tonight:
            prev[(r['repo'], r['preset'])] = r
    plan_tiers = {(t['repo'], t['label'].split(':', 1)[-1] if t['repo'] != 'MATRIX' else t['label']): t['tier'] for t in load_plan()[0]}

    def key(r):
        return (r['repo'], r['preset'])

    newly, still, fixed, broken_track, controls = [], [], [], [], []
    for r in tonight:
        p = prev.get(key(r))
        tier = plan_tiers.get(key(r), '')
        if tier == 'control':
            controls.append(r)
            continue
        if ok(r['verdict']):
            if p and not ok(p['verdict']):
                fixed.append((r, p))
        elif tier == 'track':
            broken_track.append(r)
        elif p and ok(p['verdict']):
            newly.append((r, p))
        else:
            still.append((r, p))
    # Flaky: mixed results at one build (any origin), or 3+ flips across the last 10 runs, in the window.
    window = [r for r in rows if r['started'] >= since and r['mode'] in ('srv', 'pak', 'matrix')]
    flaky = []
    groups = {}
    for r in window:
        groups.setdefault(key(r), []).append(r)
    for k, rs in sorted(groups.items()):
        rs.sort(key=lambda r: r['started'])
        res = [ok(r['verdict']) for r in rs if r['verdict'] not in ('REFUSED', 'NOVERDICT')]
        same_build = {}
        for r in rs:
            # Same code = same commit and, for ACA, the same BBB it loaded; uncommitted runs don't count.
            if r['build'] and '+dirty' not in r['build'] + r.get('bbb', '') and r['verdict'] not in ('REFUSED', 'NOVERDICT', 'TIMEOUT'):
                b = r['build'] + (f" (BBB {r['bbb']})" if r.get('bbb') else '')
                same_build.setdefault(b, set()).add(ok(r['verdict']))
        mixed = [b for b, s in same_build.items() if len(s) == 2]
        flips = sum(1 for a, b in zip(res[-10:], res[-9:]) if a != b)
        if mixed or flips >= 3:
            flaky.append((k, len(res), sum(res), mixed[:3], flips))
    # Duration drift vs the median of earlier passing runs.
    drift = []
    for r in tonight:
        past = sorted(num(x['seconds']) for x in rows if key(x) == key(r) and ok(x['verdict'])
                      and x['seconds'] and x['started'] < r['started'])
        if len(past) >= 3 and num(r['seconds']):
            med = past[len(past) // 2]
            if med > 30 and num(r['seconds']) > med * 1.5:
                drift.append((r, med))
    noverdict = sorted({key(r) for r in tonight if r['verdict'] in ('NOVERDICT', 'PASS-UNDECLARED')})
    bad_counts = sum(1 for r in tonight if not ok(r['verdict']) and r not in controls)
    blind = sum(1 for r in controls if ok(r['verdict']))

    def builds(r):
        return f"{r['build']}" + (f" (BBB {r['bbb']})" if r.get('bbb') else '')

    L = [f'# Test sweep report: {tag or last_day}', '',
         f'{len(tonight) - len(controls)} runs, {len(tonight) - len(controls) - bad_counts} passed, {bad_counts} not passed; '
         f'{len(controls)} controls, {blind} blind. '
         f'History window for flakiness: {args.days} days ({len(window)} runs from every session on this PC).', '']
    L += ['## Newly failing (passed in the previous sweep)', '']
    L += [f"- **{r['repo']} {r['preset']}**: {r['verdict']} {r['failed'] or ''} at {builds(r)}; "
          f"passed at {builds(p)} ({p['started'][:16]}). Commits to look at: `{p['build']}..{r['build']}`. "
          f"Result: `{r['file']}`" for r, p in newly] or ['- none']
    L += ['', '## Still failing', '']
    L += [f"- {r['repo']} {r['preset']}: {r['verdict']} (failed={r['failed'] or '-'}, errors={r['errors']})"
          + (f"; also not passing on {p['started'][:10]}" if p else '; first sweep run') for r, p in still] or ['- none']
    L += ['', '## Fixed since the previous sweep', '']
    L += [f"- {r['repo']} {r['preset']} (was {p['verdict']})" for r, p in fixed] or ['- none']
    L += ['', '## Tracked, not counted (e.g. 1.9 Experimental, known-broken)', '']
    L += [f"- {r['repo']} {r['preset']}: {r['verdict']}, {r['errors']} error lines. `{r['file']}`" for r in broken_track] or ['- all passing']
    L += ['', '## Controls (a feature switched off: its test must fail, or it cannot tell the feature works)', '']
    for r in controls:
        if r['verdict'] == 'FAIL':
            L.append(f"- {r['repo']} {r['preset']}: fails with the feature off, as it should ({r['failed']} checks)")
        elif ok(r['verdict']):
            L.append(f"- **{r['repo']} {r['preset']}: PASSES with the feature off.** The test can't tell this feature "
                     f"works; strengthen it (or the switch isn't honoured). `{r['file']}`")
        else:
            L.append(f"- {r['repo']} {r['preset']}: {r['verdict']}, didn't run cleanly. `{r['file']}`")
    if not controls:
        L.append('- none ran')
    L += ['', '## Flaky (mixed results at one commit, or 3+ flips in the last 10 runs)', '']
    L += [f"- {k[0]} {k[1]}: {p}/{n} passed" + (f"; mixed at {', '.join(m)}" if m else '') + f", {f} flips"
          for k, n, p, m, f in flaky] or ['- none']
    L += ['', '## Slower than usual (>1.5x the median passing run)', '']
    L += [f"- {r['repo']} {r['preset']}: {r['seconds']} s vs median {med:.0f} s" for r, med in drift] or ['- none']
    L += ['', '## Ran without a machine verdict (no expected count, or none at all)', '']
    L += [f"- {k[0]} {k[1]}" for k in noverdict] or ['- none']
    L += ['', '## Script errors in passing runs', '']
    L += [f"- {r['repo']} {r['preset']}: {r['errors']} non-noise error lines. `{r['file']}`"
          for r in tonight if ok(r['verdict']) and num(r['errors']) > 0] or ['- none']
    L += ['', '## All runs', '', '| repo | preset | verdict | passed/failed | s | build |', '|---|---|---|---|---|---|']
    L += [f"| {r['repo']} | {r['preset']} | {r['verdict']} | {r['passed']}/{r['failed']} | {r['seconds']} | {builds(r)} |"
          for r in sorted(tonight, key=key)]
    try:   # errors from Jacob's own play sessions since the last report
        import playlog
        L += [''] + playlog.render(playlog.harvest())
    except Exception as e:   # the sweep report must never fail on a log it can't read
        L += ['', f'(play-session logs not read: {e})']
    out = Path(args.out) if args.out else DATA / 'reports' / f'{tag or last_day}.md'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text('\n'.join(L) + '\n', encoding='utf-8')
    shutil.copy2(out, DATA / 'reports' / 'latest.md')
    print(f'report: {out}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('ingest')
    r = sub.add_parser('run')
    r.add_argument('--tier', default='nightly', choices=['nightly', 'weekly'])
    r.add_argument('--only', help='comma list of repos (ACA,BBB,AIO,MCC,R7,CWL,SPG9,MATRIX)')
    r.add_argument('--until', help='stop starting tests after this local time, e.g. 08:00')
    r.add_argument('--match', help='regex on preset names: run only those rows (any tier except skip)')
    r.add_argument('--dry-run', action='store_true')
    s = sub.add_parser('stats')
    s.add_argument('repo', nargs='?')
    t = sub.add_parser('tested')
    t.add_argument('repo', choices=list(REPOS))
    t.add_argument('ref', nargs='?', help='commit, branch or PR head (default HEAD of the main checkout)')
    p = sub.add_parser('report')
    p.add_argument('--days', type=int, default=7)
    p.add_argument('--out')
    p.add_argument('--tag', help='report this sweep run (MMDD-HHMM from its result names), default the latest')
    a = ap.parse_args()
    sys.exit({'ingest': cmd_ingest, 'run': cmd_run, 'stats': cmd_stats, 'report': cmd_report,
              'tested': cmd_tested}[a.cmd](a))


if __name__ == '__main__':
    main()
