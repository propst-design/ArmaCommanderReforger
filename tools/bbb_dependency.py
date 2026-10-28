"""Resolve the pinned, clean sibling BBB checkout for reproducible player packages."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent.parent
BBB_ID = 'BBB0261015000001'


def source():
    root = ROOT.parent / 'BetterBattleBuddies'
    for directory in ['Scripts/Game/BBB', 'Configs/BBB']:
        if any(p.is_file() for p in (ROOT / directory).rglob('*')):
            raise SystemExit(f'BBB is a dependency; remove duplicate ACA-owned files in {directory}.')
    pin = json.loads((ROOT / 'tools/bbb-source.json').read_text(encoding='utf-8'))
    commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    if commit != pin['commit']:
        raise SystemExit(f'BBB checkout {commit} differs from pinned {pin["commit"]}. Review and update tools/bbb-source.json.')
    if subprocess.check_output(['git', '-C', str(root), 'status', '--porcelain'], text=True).strip():
        raise SystemExit('Commit or stash BBB changes before packaging ACA.')
    if f'GUID "{BBB_ID}"' not in (root / 'addon.gproj').read_text():
        raise SystemExit('Unexpected BBB addon identity.')
    if not (root / 'resourceDatabase.rdb').is_file():
        raise SystemExit('Open BBB addon.gproj in Workbench to build resourceDatabase.rdb first.')
    return root, commit


def release_files(root):
    for name in ['addon.gproj', 'resourceDatabase.rdb', 'README.md', 'LICENSE.md']:
        yield root / name
    for directory in ['Scripts/Game/BBB', 'Configs/BBB', 'UI/layouts/BBB']:
        yield from (p for p in sorted((root / directory).rglob('*')) if p.is_file())


if __name__ == '__main__':
    root, commit = source()
    print(commit)
