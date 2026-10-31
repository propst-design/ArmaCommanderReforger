"""Check tracked public source for accidentally included local/private material.

Run from any directory: python tools/check_public_source.py
This is a publication hygiene check, not a gameplay or exhaustive secret audit.
"""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
PRIVATE = re.compile(
    r"(?i)(^|/)(?:AGENTS\.md|CLAUDE(?:\.local)?\.md|HANDBOOK\.md|"
    r"WORKSHOP-RELEASES\.json|resourceDatabase\.rdb|\.env(?:\..*)?|"
    r"game-paths\.txt|host-settings\.txt|play-settings\.txt|join-settings\.txt|"
    r"last-host\.txt|launcher-log\.txt|zerotier\.txt|VALIDATION\.md|STEP-1\.md)$|"
    r"(^|/)(?:\.local-[^/]*|\.claude|\.codex|\.agents|\.cache|__pycache__|"
    r"\.bbb-results|\.acr-requests|\.aio-requests|\.release-console|dist)(/|$)|"
    r"^docs/(?:handoff|validation)(/|$)|\.(?:log|bak)$"
)
CONTENT = re.compile(
    r"(?i)[A-Z]:[\\/](?:Users|Documents and Settings)[\\/]|/" r"Users/|"
    r"claude\.ai/(?:chat|project|public|artifact|code)/|"
    r"discord(?:app)?\.com/api/webhooks/\d{17,20}/[\w-]{40,}|"
    r"[\w.+-]+@(?:gmail|googlemail|outlook|hotmail|live|yahoo|icloud|proton|protonmail)\.[a-z.]+|"
    r"gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|"
    r"AKIA[0-9A-Z]{16}|-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----"
)

def main():
    paths = subprocess.check_output(
        ['git', '-C', str(ROOT), 'ls-files', '-z']).decode('utf-8').split('\0')
    failures = []
    for rel in filter(None, paths):
        if PRIVATE.search(rel) and Path(rel).name != '.env.example':
            failures.append(f'{rel}: private/generated file is tracked')
        path = ROOT / rel
        if not path.is_file():
            failures.append(f'{rel}: tracked file missing')
            continue
        data = path.read_bytes()
        if b'\0' in data:
            continue
        for line_no, line in enumerate(data.decode('utf-8-sig', errors='replace').splitlines(), 1):
            if CONTENT.search(line):
                failures.append(f'{rel}:{line_no}: possible personal path, private link or credential (content withheld)')
    if failures:
        print('\n'.join(failures))
        return 1
    print(f'Public-source check passed ({len(list(filter(None, paths)))} tracked files).')
    return 0

if __name__ == '__main__':
    sys.exit(main())
