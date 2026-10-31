# Fails when MISSION-MAKING.md misses a World Editor setting or host option the mode reads.
# Run by make_package.py, so a release can't ship with the mission-maker guide out of date.
import re, sys, pathlib
root = pathlib.Path(__file__).resolve().parent.parent
doc = (root / 'MISSION-MAKING.md').read_text(encoding='utf-8')
acr = root / 'Scripts/Game/ACR'
# Classes mission makers place or fill in, and the files they live in.
classes = {
    'ACR_Mode.c': ['ACR_Mode', 'ACR_StartingUnit'],
    'ACR_FactionSettings.c': ['ACR_RequisitionEntry', 'ACR_PrefabMapping', 'ACR_SupportUnitSettings', 'ACR_FactionSettings'],
    'ACR_Base.c': ['ACR_Base'],
    'ACR_Helipads.c': ['ACR_Helipad'],
    'ACR_Skirmish.c': ['ACR_Skirmish'],
    'ACR_Supports.c': ['ACR_SupportEntry'],
    'ACR_Options.c': ['ACR_MissionHeader'],
    'ACR_AISkill.c': ['ACR_AISkillProfile'],
}
missing = []
for fname, names in classes.items():
    text = (acr / fname).read_text(encoding='utf-8-sig')
    for cls in names:
        m = re.search(r'^class ' + cls + r'\b[^\n]*\n\{(.*?)^\}', text, re.S | re.M)
        if not m:
            missing.append(f'class {cls} not found in {fname}')
            continue
        for field in re.findall(r'\[Attribute\([^\n]*\)\]\s*(?:ref\s+)?[\w<>\s]+?\s(m_\w+);', m.group(1)):
            if f'`{field}`' not in doc: missing.append(f'{cls}.{field}')
    if fname == 'ACR_Supports.c':
        enum = re.search(r'enum ACR_ESupportType\s*\{(.*?)\}', text, re.S).group(1)
        for value in re.findall(r'^\s*([A-Z_]+)', enum, re.M):
            if f'`{value}`' not in doc: missing.append(f'support type {value}')
# Host options (test flags excluded).
flags = set()
for f in acr.glob('*.c'):
    flags |= set(re.findall(r'(?:GetCLIParam|IsCLIParam|ACR_Options\.Get|ACR_Options\.Flag)\("(acr\w+)"', f.read_text(encoding='utf-8-sig')))
for flag in sorted(flags):
    if flag.endswith('Test'): continue
    if f'`-{flag}' not in doc: missing.append(f'host option -{flag}')
if missing:
    print('MISSION-MAKING.md is missing:\n  ' + '\n  '.join(missing))
    sys.exit(1)
print('MISSION-MAKING.md covers every setting and host option')
