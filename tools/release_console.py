"""Local release guard. Never uploads or deletes releases. Records confirmed publication in source."""
from pathlib import Path
import argparse
import contextlib
import datetime as dt
import hashlib
import json
import re
import subprocess
import sys
import uuid
import zlib

ROOT = Path(__file__).resolve().parent.parent
SPECS = {
    'ACA': dict(folder='ArmaCommanderReforger', title='Arma Commander: Actual',
                guid='AC230926A0010001', dirs=['Scripts', 'Configs', 'Missions', 'Prefabs', 'Worlds'],
                files=['LICENSE.md', 'README.md', 'MISSION-MAKING.md']),
    'BBB': dict(folder='BetterBattleBuddies', title='Better Battle Buddies',
                guid='BBB0261015000001', dirs=['Scripts/Game/BBB', 'Configs/BBB', 'Prefabs/BBB', 'UI/layouts/BBB'],
                files=['LICENSE.md', 'README.md']),
    'AIO': dict(folder='AIOCommandMenu', title='AiO Command Menu (Reforger)',
                guid='629405AEFC950C12', dirs=['Scripts', 'Configs'],
                files=['COPYING', 'LICENSE.md', 'LICENSE.txt', 'README.md', 'CHANGELOG.md']),
}


class Blocked(RuntimeError):
    pass


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix('.tmp-' + uuid.uuid4().hex)
    temp.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')
    temp.replace(path)


def git(source, *args):
    return subprocess.check_output(['git', '-C', str(source), *args], text=True).strip()


def clean_commit(source):
    if git(source, 'status', '--porcelain', '--untracked-files=normal'):
        raise Blocked(f'{source.name}: unsaved source changes. Ask the coding agent to finish and commit them first.')
    return git(source, 'rev-parse', 'HEAD')


def no_links(path):
    for part in (path, *path.parents):
        if part.is_symlink() or (hasattr(part, 'is_junction') and part.is_junction()):
            raise Blocked(f'Linked path refused: {part}')


def latest_release_notes(source, dependency_only=False):
    path = source / 'CHANGELOG.md'
    no_links(path)
    if not path.is_file():
        raise Blocked(f'{source.name}: add and commit CHANGELOG.md before preparing a release.')
    text = path.read_text(encoding='utf-8-sig').replace('\r\n', '\n')
    ledger = source / 'WORKSHOP-RELEASES.json'
    if ledger.is_file():
        releases = read_json(ledger)['releases']
        for release in releases:
            if release.get('notesSha256') and release['notesSha256'] != notes_digest(text, release['version']):
                raise Blocked('Published release notes changed. Put new changes under the next version; preserve published history.')
    sections = list(re.finditer(r'^##\s+(.+)$', text, re.M))
    if dependency_only and len(sections) > 1 and not text[sections[0].end():sections[1].start()].strip():
        sections = sections[1:]
    if not sections:
        raise Blocked('CHANGELOG.md needs a top release heading: ## 0.1.0')
    heading = sections[0]
    version = re.match(r'(\d+\.\d+\.\d+)(?:\s|$)', heading[1])
    if not version:
        raise Blocked('The first CHANGELOG.md release must have a numeric version, not Unreleased/TBD.')
    body = text[heading.end():sections[1].start() if len(sections) > 1 else len(text)].strip()
    if not re.search(r'^[-*] \S', body, re.M) or re.search(r'\b(TODO|TBD|FIXME)\b', body, re.I):
        raise Blocked('The latest changelog needs reviewed release bullets without placeholders.')
    version = version[1]
    return version, f'{version}\n\n{body}\n'


def notes_digest(text, version):
    section = re.search(r'^##\s+' + re.escape(version) + r'(?:\s[^\n]*)?\n(.*?)(?=^##\s|\Z)', text, re.M | re.S)
    if not section:
        raise Blocked(f'Missing published changelog section {version}.')
    return hashlib.sha256(section[1].strip().encode('utf-8')).hexdigest()


class Console:
    def __init__(self, root=ROOT, output=None):
        self.root = root.resolve()
        self.addons = self.root.parent
        self.output = Path(output or self.addons.parent / '_ModReleases').absolute()
        no_links(self.output)
        if self.output.resolve().is_relative_to(self.addons):
            raise Blocked('Release output must be OUTSIDE Workbench/addons.')
        self.state = self.root / '.release-console'

    def source(self, mod):
        return self.addons / SPECS[mod]['folder']

    def pointer(self, mod):
        return self.state / (mod + '.json')

    def release_history(self, mod):
        path = self.source(mod) / 'WORKSHOP-RELEASES.json'
        return read_json(path) if path.is_file() else dict(mod=mod, releases=[])

    def require_new_version(self, mod, version):
        releases = self.release_history(mod)['releases']
        if releases and tuple(map(int, version.split('.'))) <= max(tuple(map(int, r['version'].split('.'))) for r in releases):
            raise Blocked(f'{mod} {version} is already published. New changes need the next version.')

    def record_publication(self, mod, version, evidence, candidate=None):
        """Record confirmed publication in source; never uploads or commits."""
        if not re.fullmatch(r'\d+\.\d+\.\d+', version) or not evidence.strip():
            raise Blocked('A numeric version and publication evidence are required.')
        source = self.source(mod)
        history = self.release_history(mod)
        if any(r['version'] == version for r in history['releases']):
            return history  # Idempotent; never overwrite original evidence.
        self.require_new_version(mod, version)
        changelog = source / 'CHANGELOG.md'
        text = changelog.read_text(encoding='utf-8-sig').replace('\r\n', '\n')
        for release in history['releases']:
            if release.get('notesSha256') != notes_digest(text, release['version']):
                raise Blocked('Published release notes changed. Preserve published history.')
        match = re.search(r'^##\s+' + re.escape(version) + r'(?:\s[^\n]*)?$', text, re.M)
        if not match:
            raise Blocked('Record the confirmed version in CHANGELOG.md before recording publication.')
        entry = dict(version=version, confirmedAt=dt.datetime.now(dt.timezone.utc).isoformat(),
                     evidence=evidence, notesSha256=notes_digest(text, version))
        if candidate:
            manifest = self.verify_manifest(candidate, current=False)
            if manifest['mod'] != mod or manifest['releaseVersion'] != version:
                raise Blocked('Publication candidate/version mismatch.')
            candidate_text = (Path(candidate) / 'CHANGELOG.md').read_text(encoding='utf-8-sig').replace('\r\n', '\n')
            if notes_digest(candidate_text, version) != entry['notesSha256']:
                raise Blocked('Source notes differ from the published candidate. Reconcile before recording publication.')
            entry.update(sourceCommit=manifest['commit'],
                         candidate=Path(candidate).absolute().relative_to(self.output.parent).as_posix())
        # Validate everything before writing. An interrupted operation can be repeated safely.
        text = text[:match.start()] + f'## {version} — released' + text[match.end():]
        first = re.search(r'^##\s+', text, re.M)
        if first.start() == match.start():
            major, minor, patch = map(int, version.split('.'))
            text = text[:first.start()] + f'## {major}.{minor}.{patch + 1} — unreleased\n\n' + text[first.start():]
        changelog.write_bytes(text.encode('utf-8'))
        history['releases'].append(entry)
        write_json(source / 'WORKSHOP-RELEASES.json', history)
        return history

    def stage(self, mod, dependency_only=False):
        spec = SPECS[mod]
        source = self.source(mod)
        sha = clean_commit(source)
        no_links(source)
        version, change_note = latest_release_notes(source, dependency_only=dependency_only)
        if not dependency_only:
            self.require_new_version(mod, version)
        reviewed = git(source, 'log', '-1', '--format=%H', '--', 'CHANGELOG.md')
        if not reviewed:
            raise Blocked('Commit CHANGELOG.md before preparing a release.')
        changed = git(source, 'diff', '--name-only', reviewed, 'HEAD', '--',
                      'addon.gproj', *spec['dirs']).splitlines()
        changed = [p for p in changed if not p.startswith(('Scripts/Game/ACR_Tests/',
                   'Scripts/Game/BBB_Tests/')) and 'BBB_TestArland' not in p]
        if changed:
            raise Blocked(f'{mod}: production changed since CHANGELOG.md was reviewed. Update and commit its release notes first.')
        dependency = None
        if mod == 'ACA':
            dependency = read_json(source / 'tools/bbb-source.json')['commit']
            if clean_commit(self.source('BBB')) != dependency:
                raise Blocked('BBB source differs from ACA\'s pinned dependency. Ask the coding agent to reconcile it.')
            subprocess.run([sys.executable, str(source / 'tools/check_mission_doc.py')], check=True,
                           stdout=sys.stderr)
        project = (source / 'addon.gproj').read_text(encoding='utf-8-sig')
        if not re.search(r'\bGUID\s+"' + spec['guid'] + '"', project):
            raise Blocked(f'{mod}: unexpected addon ID.')
        if mod == 'ACA' and f'"{SPECS["BBB"]["guid"]}"' not in project:
            raise Blocked('ACA is missing its BBB dependency.')
        tracked = set(git(source, 'ls-files', '-z').split('\0'))
        paths = set(spec['files'] + ['addon.gproj', 'resourceDatabase.rdb'])
        for directory in spec['dirs']:
            paths.update(p for p in tracked if p.startswith(directory + '/'))
        paths = {p for p in paths if not (p.startswith('Scripts/Game/ACR_Tests/') or
                 p.startswith('Scripts/Game/BBB_Tests/') or 'BBB_TestArland' in p)}
        # AiO currently calls its test classes directly; preserve its existing builder's file set.
        for relative in paths:
            path = source / relative
            no_links(path)
            if not path.is_file():
                raise Blocked(f'Missing {path}. Open the development project in Workbench if its resource database needs creating.')
        stamp = dt.datetime.now(dt.timezone.utc).strftime('%Y%m%d-%H%M%S')
        parent = self.output / mod / (sha[:12] + '-' + stamp + '-' + uuid.uuid4().hex[:6])
        addon = parent / spec['folder']
        addon.mkdir(parents=True, exist_ok=False)
        files = []
        for relative in sorted(paths):
            data = (source / relative).read_bytes()
            if relative == 'addon.gproj':
                data = re.sub(rb'(?m)^\s*TITLE\s+"[^"\r\n]*"',
                              (' TITLE "' + spec['title'] + '"').encode(), data)
            target = addon / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            files.append(dict(path=relative, sha256=digest(target)))
        sidecars = source / 'tools/workshop'
        if sidecars.exists():
            for path in sidecars.iterdir():
                if path.is_file():
                    (parent / path.name).write_bytes(path.read_bytes())
        description = source / 'docs/workshop-description.md'
        if description.is_file():
            (parent / 'WORKSHOP-DESCRIPTION.txt').write_bytes(description.read_bytes())
        (parent / 'CHANGELOG.md').write_bytes((source / 'CHANGELOG.md').read_bytes())
        (parent / 'WORKSHOP-CHANGELOG.txt').write_text(change_note, encoding='utf-8')
        manifest = dict(mod=mod, commit=sha, bbbCommit=dependency, sourceRoot=str(source),
                        addonRoot=str(addon), builtUtc=stamp, files=files, releaseVersion=version,
                        releaseNotes=[dict(path=name, sha256=digest(parent / name))
                                      for name in ['CHANGELOG.md', 'WORKSHOP-CHANGELOG.txt']])
        write_json(parent / 'WORKSHOP-BUILD.json', manifest)
        # Detect source edits while copying, including ignored resource database changes.
        self.verify_manifest(parent, current=True)
        for item in files:
            if item['path'] != 'addon.gproj' and digest(source / item['path']) != item['sha256']:
                raise Blocked('Source changed during staging; prepare again when edits have stopped.')
        return parent

    def verify_manifest(self, parent, current=True):
        parent = Path(parent).absolute()
        no_links(parent)
        if not parent.resolve().is_relative_to(self.output.resolve()):
            raise Blocked('This candidate was not built in the managed release area.')
        manifest = read_json(parent / 'WORKSHOP-BUILD.json')
        mod = manifest['mod']
        addon = parent / SPECS[mod]['folder']
        if Path(manifest['addonRoot']) != addon or Path(manifest['sourceRoot']) != self.source(mod):
            raise Blocked('Candidate identity/path mismatch.')
        expected = set()
        for item in manifest['files']:
            path = addon / item['path']
            no_links(path)
            if not path.resolve().is_relative_to(addon.resolve()):
                raise Blocked('Unsafe candidate file path.')
            expected.add(item['path'])
            # Workbench regenerates this index on project open. Its inputs (.meta files,
            # scripts/configs/assets) remain hash-locked; the generated index must exist.
            if not path.is_file() or (item['path'] != 'resourceDatabase.rdb' and digest(path) != item['sha256']):
                raise Blocked(f'{mod}: candidate was edited or damaged: {item["path"]}. Prepare a fresh copy.')
        actual = {p.relative_to(addon).as_posix() for p in addon.rglob('*') if p.is_file()}
        if actual != expected:
            raise Blocked(f'{mod}: candidate file set changed. Prepare a fresh copy.')
        notes = manifest.get('releaseNotes', [])
        if {item['path'] for item in notes} != {'CHANGELOG.md', 'WORKSHOP-CHANGELOG.txt'}:
            raise Blocked(f'{mod}: candidate predates required changelog handling. Prepare again.')
        for item in notes:
            path = parent / item['path']
            no_links(path)
            if not path.is_file() or digest(path) != item['sha256']:
                raise Blocked(f'{mod}: release notes changed after preparation. Update source and prepare again.')
        if current and clean_commit(self.source(mod)) != manifest['commit']:
            raise Blocked(f'{mod}: source moved on since this candidate. Prepare a fresh copy.')
        if current:
            if digest(self.source(mod) / 'CHANGELOG.md') != next(i['sha256'] for i in notes if i['path'] == 'CHANGELOG.md'):
                raise Blocked(f'{mod}: source changelog changed since staging. Prepare again.')
            for item in manifest['files']:
                if item['path'] != 'addon.gproj' and digest(self.source(mod) / item['path']) != item['sha256']:
                    raise Blocked(f'{mod}: source content changed since staging: {item["path"]}. Prepare again.')
        if current and mod == 'ACA':
            pin = read_json(self.source(mod) / 'tools/bbb-source.json')['commit']
            if pin != manifest['bbbCommit'] or clean_commit(self.source('BBB')) != pin:
                raise Blocked('ACA/BBB dependency changed. Prepare the pair again.')
        return manifest

    def smoke(self, parent, dependency=None):
        manifest = self.verify_manifest(parent)
        args = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                str(self.root / 'tools/Release-Smoke.ps1'), '-Mod', manifest['mod'],
                '-AddonRoot', manifest['addonRoot'], '-ReportDirectory', str(parent / 'startup')]
        if dependency:
            args += ['-DependencyRoot', str(dependency / SPECS['BBB']['folder'])]
        subprocess.run(args, check=True, stdout=sys.stderr)
        self.verify_manifest(parent)
        write_json(parent / 'STARTUP-CHECK.json', dict(
            manifest=digest(parent / 'WORKSHOP-BUILD.json'),
            dependencyManifest=digest(dependency / 'WORKSHOP-BUILD.json') if dependency else None,
            checkedUtc=dt.datetime.now(dt.timezone.utc).isoformat()))
        write_json(self.pointer(manifest['mod']), dict(candidate=str(parent)))

    def prepare(self, mod):
        dependency = None
        if mod == 'ACA':
            dependency = self.stage('BBB', dependency_only=True)
            self.smoke(dependency)
        parent = self.stage(mod)
        self.smoke(parent, dependency)
        return parent

    def notes_info(self, mod):
        parent = self.checked(mod)
        manifest = read_json(parent / 'WORKSHOP-BUILD.json')
        self.require_new_version(mod, manifest['releaseVersion'])
        return dict(mod=mod, version=manifest['releaseVersion'],
                    project=str(Path(manifest['addonRoot']) / 'addon.gproj'),
                    changelogFile=str(parent / 'WORKSHOP-CHANGELOG.txt'))

    def downloaded_version(self, mod, installed_root):
        matches = []
        for directory in Path(installed_root).iterdir():
            project = directory / 'addon.gproj'
            if project.is_file() and re.search(r'\bGUID\s+"' + SPECS[mod]['guid'] + '"',
                                               project.read_text(encoding='utf-8-sig')):
                matches.append(directory)
        if len(matches) != 1:
            raise Blocked('CLI updates require exactly one downloaded Workshop copy to establish the current version.')
        data = matches[0] / 'ServerData.json'
        # The game's ServerData.json can contain a truncated/non-JSON trailer.
        if data.is_file():
            match = re.search(r'"revision"\s*:\s*\{\s*"version"\s*:\s*"(\d+\.\d+\.\d+)"',
                              data.read_text(encoding='utf-8-sig', errors='replace'))
            if match:
                return match[1]
        versions = [m[1] for path in matches[0].glob('addon.gproj_*_manifest.json')
                    if (m := re.fullmatch(r'addon\.gproj_(\d+\.\d+\.\d+)_manifest\.json', path.name))]
        if len(versions) != 1:
            raise Blocked('Cannot determine installed Workshop version. Download the current release before CLI publication.')
        return versions[0]

    def publish_info(self, mod, installed_root):
        info = self.notes_info(mod)
        current = self.downloaded_version(mod, installed_root)
        if tuple(map(int, info['version'].split('.'))) <= tuple(map(int, current.split('.'))):
            raise Blocked(f'{mod} {current} is already installed. A new update needs a higher version and reviewed changelog; do not republish for tooling-only changes.')
        if mod == 'ACA':
            self.installed('BBB', installed_root)
        info['installedVersion'] = current
        return info

    def checked(self, mod):
        if not self.pointer(mod).is_file():
            raise Blocked(f'{mod}: prepare this update first.')
        parent = Path(read_json(self.pointer(mod))['candidate'])
        manifest = self.verify_manifest(parent)
        if manifest['mod'] != mod:
            raise Blocked('Release pointer identifies a different mod. Prepare the selected mod again.')
        receipt = read_json(parent / 'STARTUP-CHECK.json')
        if receipt['manifest'] != digest(parent / 'WORKSHOP-BUILD.json'):
            raise Blocked('Startup check belongs to different candidate contents. Prepare again.')
        if mod == 'ACA':
            bbb = self.checked('BBB')
            if read_json(bbb / 'WORKSHOP-BUILD.json')['commit'] != manifest['bbbCommit']:
                raise Blocked('Prepared BBB differs from the ACA dependency.')
            if receipt['dependencyManifest'] != digest(bbb / 'WORKSHOP-BUILD.json'):
                raise Blocked('ACA was checked against a different BBB candidate. Prepare the pair again.')
        return parent

    def installed(self, mod, installed_root):
        """Compare packed scripts/configs with the exact prepared candidate; tolerate CRLF conversion."""
        import pakread
        parent = self.checked(mod)
        manifest = read_json(parent / 'WORKSHOP-BUILD.json')
        installed_root = Path(installed_root)
        matches = []
        if installed_root.is_dir():
            for directory in installed_root.iterdir():
                project = directory / 'addon.gproj'
                if project.is_file() and re.search(r'\bGUID\s+"' + SPECS[mod]['guid'] + '"',
                                                   project.read_text(encoding='utf-8-sig')):
                    matches.append(directory)
        if len(matches) != 1:
            raise Blocked(f'{mod}: need exactly one downloaded Workshop copy in {installed_root}; found {len(matches)}. Download/update it in-game first.')
        def dependencies(text):
            section = re.search(r'\bDependencies\s*\{([^}]*)\}', text)
            return set(re.findall(r'"([A-Fa-f0-9]{16})"', section[1])) if section else set()
        wanted_project = (Path(manifest['addonRoot']) / 'addon.gproj').read_text(encoding='utf-8-sig')
        installed_project = (matches[0] / 'addon.gproj').read_text(encoding='utf-8-sig')
        if dependencies(wanted_project) != dependencies(installed_project):
            raise Blocked(f'{mod}: downloaded Workshop dependency declarations differ from the prepared release.')
        index_path = self.state / ('installed-' + mod + '.json')
        self.state.mkdir(parents=True, exist_ok=True)
        # CLI stdout is reserved for the single checked project path consumed by PowerShell.
        with contextlib.redirect_stdout(sys.stderr):
            pakread.index(str(matches[0]), str(index_path))
        index = {k.lower(): v for k, v in read_json(index_path).items()}
        expected = {item['path'].lower(): item for item in manifest['files']
                    if item['path'].startswith(('Scripts/', 'Configs/')) and
                    item['path'].endswith(('.c', '.conf'))}
        actual = {name for name in index if name.startswith(('scripts/', 'configs/')) and
                  name.endswith(('.c', '.conf'))}
        if actual != set(expected):
            raise Blocked(f'{mod}: downloaded Workshop script/config file set differs from prepared release.')
        for name, item in expected.items():
            pak, offset, compressed, size, compression = index[name]
            with open(pak, 'rb') as handle:
                handle.seek(offset)
                data = handle.read(compressed)
            if compression:
                data = zlib.decompress(data)
            source = (Path(manifest['addonRoot']) / item['path']).read_bytes()
            if data.replace(b'\r\n', b'\n') != source.replace(b'\r\n', b'\n'):
                raise Blocked(f'{mod}: downloaded Workshop content is stale: {item["path"]}. Upload the prepared project, then download the update.')
        print(f'{mod}: downloaded scripts/configs match prepared {manifest["commit"][:12]}.', file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['prepare', 'check', 'status', 'installed', 'notes', 'publish-info', 'record-publication'])
    parser.add_argument('mod', choices=SPECS, nargs='?')
    parser.add_argument('--installed-root', type=Path, default=Path('E:/ArmaReforgerMods/addons'))
    parser.add_argument('--version')
    parser.add_argument('--evidence')
    parser.add_argument('--candidate', type=Path)
    options = parser.parse_args()
    console = Console()
    if options.action == 'status':
        for mod in SPECS:
            releases = console.release_history(mod)['releases']
            print(f'{mod}: published ' + (', '.join(r['version'] for r in releases) or 'not recorded'))
            try:
                parent = console.checked(mod)
                print(f'{mod}: CHECKED {read_json(parent / "WORKSHOP-BUILD.json")["commit"][:12]}')
            except (Blocked, OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
                print(f'{mod}: NOT READY - {error}')
        return
    if not options.mod:
        parser.error('Choose a mod.')
    if options.action == 'record-publication':
        if not options.version or not options.evidence:
            parser.error('record-publication requires --version and --evidence.')
        print(json.dumps(console.record_publication(options.mod, options.version, options.evidence, options.candidate)))
        return
    if options.action in ['notes', 'publish-info']:
        info = console.publish_info(options.mod, options.installed_root) if options.action == 'publish-info' else console.notes_info(options.mod)
        print(json.dumps(info))
        return
    if options.action == 'prepare':
        parent = console.prepare(options.mod)
    else:
        parent = console.checked(options.mod)
    if options.action == 'installed':
        console.installed(options.mod, options.installed_root)
    elif options.action == 'check' and options.mod == 'ACA':
        console.installed('BBB', options.installed_root)
    print(Path(read_json(parent / 'WORKSHOP-BUILD.json')['addonRoot']) / 'addon.gproj')


if __name__ == '__main__':
    try:
        main()
    except (Blocked, OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f'BLOCKED: {error}', file=sys.stderr)
        sys.exit(1)
