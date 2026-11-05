#!/usr/bin/env python3
"""Flags game code that writes ACR_Squad.m_iAITask directly.

A squad's AI job (ACR_EAITask) changes only through ACR_Squad.SetAITask (AI allocator phase 0,
3 Nov 2026), so every job change goes through one place that later phases can track and
arbitrate. Reading m_iAITask anywhere is fine. Tests (Scripts/Game/ACR_Tests) may set it
directly to build fixtures, so only game code is checked by default.

  python3 tools/check_ai_task_writes.py [paths...]   (default: Scripts/Game/ACR)
  Exit 1 when it finds something. A deliberate line can end with `// ai-task-write ok`.
"""
import re
import sys
from pathlib import Path

ALLOW = 'ai-task-write ok'
WRITE = re.compile(r'\bm_iAITask\s*(?:[-+*/|&^]?=)(?!=)')


def scan(path):
    found = []
    text = path.read_text(encoding='utf-8-sig', errors='replace')
    for number, line in enumerate(text.split('\n'), 1):
        code = line.split('//', 1)[0]
        if WRITE.search(code) and ALLOW not in line:
            found.append(number)
    return found


def main(argv):
    roots = [Path(a) for a in argv] or [Path('Scripts/Game/ACR')]
    bad = 0
    for root in roots:
        files = [root] if root.is_file() else sorted(root.rglob('*.c'))
        for f in files:
            for number in scan(f):
                print('%s:%d: m_iAITask written directly: use squad.SetAITask(...)' % (f.as_posix(), number))
                bad += 1
    if bad:
        print('%d direct m_iAITask write(s); see the docstring of tools/check_ai_task_writes.py' % bad)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
