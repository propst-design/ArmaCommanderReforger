#!/bin/bash
# Test request helper (Git Bash on Windows, or any bash):
#   bash tools/runtest.sh <unique id> <preset> <max polls of 5 s>
# Drops a request for the test launcher (tools/ACR-Watcher.ps1) and prints the
# result header plus script errors. Use a new id every time: a reused id is refused,
# since its old result would come back under the new build label.
# The build label is the commit, +dirty for uncommitted game files (Scripts, Prefabs,
# Configs, Worlds, Missions), and the sibling BBB checkout's commit that srv: runs load.
cd "$(dirname "$0")/../.acr-requests" || exit 1
id="$1"
if [ -z "$id" ] || [ -z "$2" ]; then echo "usage: runtest.sh <id> <preset> <polls>"; exit 2; fi
if [ -f "$id.result.txt" ] || [ -f "$id.request" ]; then echo "Id '$id' was used already: use a new id"; exit 2; fi
label=$(git -C .. log -1 --format=%h | tr -d "\n")
git -C .. diff --quiet HEAD -- Scripts Prefabs Configs Worlds Missions || label="$label+dirty"
if [ -d ../../BetterBattleBuddies/.git ] || [ -f ../../BetterBattleBuddies/.git ]; then
  bbb=$(git -C ../../BetterBattleBuddies log -1 --format=%h | tr -d "\n")
  git -C ../../BetterBattleBuddies diff --quiet HEAD -- Scripts || bbb="$bbb+dirty"
  label="$label bbb $bbb"
fi
echo -n "$label" > "$id.build.txt"
echo "$2" > "$id.request"
for i in $(seq 1 "$3"); do sleep 5; [ -f "$id.result.txt" ] && break; done
head -6 "$id.result.txt" 2>/dev/null
grep "(E)" "$id.result.txt" 2>/dev/null | grep -v "SCR_SpawnLogic\|SCR_MapUIElementContainer\|SCR_PlayerArsenalLoadout\|ScenarioFramework\|SpinningWidget\|SCR_ScenarioUICommon\|Can't compile" | head -20
bash ../tools/verdict.sh "$id"
