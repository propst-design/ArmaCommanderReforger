# AC: Actual server setups

How to match AC: Actual's AI load to your server, what each limit really does, and where to start
for your player count. Everything AC: Actual does on its own is checked against the mod's code and
marked **(checked)**. Starting values and anything about other mods we haven't measured are marked
**(suggestion)**: they are a place to start, not a promise, so watch your server and adjust.

Every option below is also listed in [Host options](MISSION-MAKING.md#host-options). Most can be
set on the in-game setup screen; the soldier cap is a launch option (`-acrSoldierCap`) or a mission
header field only.

## Contents

1. [What costs the server](#what-costs-the-server)
2. [The limits, in plain terms](#the-limits-in-plain-terms)
3. [AI that AC: Actual adds on top of the commanders](#ai-that-ac-actual-adds-on-top-of-the-commanders)
4. [Starting setups](#starting-setups)
5. [Big mod stacks](#big-mod-stacks)
6. [Is the server overloaded?](#is-the-server-overloaded)
7. [What to turn down first](#what-to-turn-down-first)

## What costs the server

Living AI soldiers are the biggest cost in any AC: Actual match: each one thinks, finds paths and
shoots on the server every frame. Vehicles add driving and physics on top. Bodies and wrecks are
cleared up as the match goes. Players are not AI, so they don't count against any of the limits
below, but more players means more parts of the map active at once.

So the question for a server is: **how many AI are alive at the same time, from every source?**
The commanders' squads are only one source. FIA, zombies, garrisons and other mods' AI all add up.

## The limits, in plain terms

### Soldier cap per side: `-acrSoldierCap` (checked)

- Default **100** living AI soldiers per side, before the threat level and map scaling change it.
- It is a **soft** cap: a side at or above it can't buy new units or reinforcements ("Too many
  soldiers in the field"). A purchase started just below it still goes through, so a side can end
  up a squad over.
- It counts the side's AI squads, including men in squads asleep in reserve. It doesn't count
  players, base garrisons, support teams (artillery, air) or zombies.
- AI groups from other mods that belong to one of the playing factions **are** counted against that
  side, because AC: Actual picks up every AI group in the world as a squad.

### AI squad cap: `-acrUnitCap` (checked)

- The most squads an **AI commander** keeps in the field. Default **8** per side, unless the
  scenario sets its own. Setup screen row: **AI squad cap** (4 to 24).
- It limits AI commanders only. A player commander is limited by the soldier cap and RP.

### Threat level: `-acrThreat` (checked)

| Threat | Soldier cap | AI squad cap | Prices (stepped pricing) | Stock (rare units, or every unit with fixed stock) |
|---|---|---|---|---|
| Low | x0.75 (at least 20) | x0.6 (at least 2) | +25% per purchase | half for infantry, a third for vehicles, at least 1 |
| Normal | x1 | x1 | +15% per purchase | as listed |
| High | x1.5 | x1.5 | +8% per purchase | double; armour and support units quadruple |

Low threat also limits each AI commander to one attack at a time and makes it buy few vehicles;
High allows one more attack at a time. With the defaults, Low gives 75 soldiers and 5 AI squads per
side, High gives 150 soldiers and 12.

### Squad caps scale with map: `-acrCapScale 1` (checked)

Off by default. When on, the AI squad cap and soldier cap of every side are multiplied once the match
starts, after the threat level, by the map's bases / 12 or the spread of its bases / 5 km, whichever
is larger, between x1 and x2. The soldier cap stops at 250 (a higher cap you set yourself stays).
The log line `[ACR] CAP SCALE` shows the factor and the new caps.

Stacked, these add up fast: High threat (150) on a big map at x2 gives **250 soldiers per side**,
500 on the map, before FIA, zombies or other mods.

### The active-AI limit (checked)

The game itself has a limit on active AI for the whole world (128 by default). Above it, the engine
removes whole groups: their soldiers are deleted and empty markers stay behind. AC: Actual raises
this limit at the start to **2 x the soldier cap + 64** (plus room for Civil Population's civilians
when its compat add-on is loaded). The extra 64 is for garrisons, support teams, FIA and the like.
The log shows `[ACR] Active AI limit N (was M)`, and `[ACR] AI LIMIT: engine evicted ...` whenever
the engine still removes groups.

The raise doesn't add room for zombies or for other mods' AI. On a server with lots of either, watch
for that `AI LIMIT` line (see [Is the server overloaded?](#is-the-server-overloaded)).

### Vehicle stock and prices (checked)

- **Stock is per match, not at one time.** Each purchase takes one from the stock and a destroyed
  vehicle doesn't give it back. Only a purchase that fails is refunded.
- **Stepped pricing** (the default, `-acrPricing stepped`): infantry, cars and trucks have no stock
  limit, but each purchase raises that unit's price by the threat level's step. Rare units (armour
  and APCs, aircraft, support helicopters, mortar and observer teams) keep a fixed stock.
- **Fixed stock** (`-acrPricing stock`): every unit has a stock per match at a fixed price.
- The threat level scales those stocks (table above). In stepped pricing it scales only the rare
  units; infantry, cars and trucks stay unlimited at every threat level, and their price is the limit.

Vehicle stock doesn't cap how many vehicles are on the map at once: the soldier cap and RP do that,
since every vehicle needs a crew.

### How scarce RP is (checked)

- `-acrStartRP N`: starting RP for every side (setup row **Starting RP**).
- `-acrIncome X`: income multiplier, default 1 (setup row **Income**, 0.5 to 3).

Less RP means fewer squads bought, which means fewer AI alive, without any hard cap. **(suggestion)**
Income 0.5 to 0.75 slows the build-up on a server that copes early in a match but struggles later.

## AI that AC: Actual adds on top of the commanders

| Source | Option | How many (checked) | Counted against the soldier cap? |
|---|---|---|---|
| Base garrisons | always on | Appear only when an enemy comes within 400 m of the base | No |
| Quick start garrisons | `-acrQuickStart` | Front bases by kind of site, a small one at rear bases | No (garrisons) |
| FIA pockets | `-acrPockets` | Village 4-6, industry 6-8, military 8-10 plus a 2-man patrol; few = 7 pockets on a whole map, many = 12 (half on a half map) | No (FIA's own) |
| FIA guerrillas | `-acrGuerrillas` | Ambushes 4-6, house cells 3-5, hideouts 6-8; few = 4 ambushes, 3 houses, 2 hideouts on a whole map; many = 8 and 6. Hideouts recruit (8 each with few, 14 with many) | No (FIA's own) |
| FIA technicals | with pockets and guerrillas | 2 in all with few, 4 with many | No |
| Zombie nests and hordes | `-acrZombies` (Project Outbreak) | Few: nests of 15-25, at most 300 zombies in all; many: 25-40, at most 500 (half on a half map, x1.5 in TvZ) | No: own cap |
| Evacuees | `-acrOutbreak contain` | A town's people, at most 3 evac points open per side | No |

Most of these only exist while someone is near: garrisons and house cells within 400 m, ambushes
within 700 m, nests within 300 m (450 m at night). The zombie cap counts the zombies still in their
nests too, so far fewer than the cap are usually walking about. Flashpoint maps have no FIA or zombies.

FIA cells and zombies get **no** room of their own from the soldier cap. They share the extra 64 in
the active-AI limit with garrisons and support teams.

## Starting setups

All values in this section are **(suggestion)**. We don't have measured numbers for particular
server hardware; these are ranges from how the limits above add up. Start in the range, play a match,
check the signs in [Is the server overloaded?](#is-the-server-overloaded), then move one setting at
a time.

### Solo, or 2-3 players with the host playing

The host's own PC runs the server and its game, so every AI costs the host's frame rate too.

| Setting | Start with | Why |
|---|---|---|
| Size | `half` or `quarter` on Everon-sized maps | Fewer bases in play, fewer garrisons waking up |
| Threat | normal or low | Low cuts both caps (75 soldiers, 5 AI squads per side) |
| `-acrSoldierCap` | 60-80 | About 120-160 AI from the commanders, well under the raised limit |
| AI squad cap | 6-8 | Default 8 |
| Squads scale with map | off | |
| FIA guerrillas / pockets | few | |
| Zombies | off or few | |

### 4-8 players, dedicated server

| Setting | Start with | Why |
|---|---|---|
| Size | full on Arland, half or full on Everon | |
| Threat | normal | |
| `-acrSoldierCap` | 100 (the default) | The value the mod is balanced around |
| AI squad cap | 8 | |
| Squads scale with map | off; try on only if the server keeps up at the defaults | Up to x2 on big maps |
| FIA guerrillas / pockets | few | |
| Zombies | few | |

### Bigger public servers (10 or more players)

| Setting | Start with | Why |
|---|---|---|
| Threat | normal; high only if the server has room | High alone is 150 soldiers and 12 AI squads per side |
| `-acrSoldierCap` | 100-150 | Raise it in steps of about 25 and watch the server between matches |
| AI squad cap | 8-12 | |
| Squads scale with map | pick it **or** High threat, not both at first | Together they reach 250 per side |
| FIA, zombies | few | `many` roughly doubles them |

More players means more places active at once (garrisons, FIA, nests near each group of players), so
a server with many players often needs a **lower** soldier cap than the same server with few.

## Big mod stacks

Other mods' AI (insurgent, PMC or bandit factions, ambient spawners) add to everything above. What
AC: Actual does with them **(checked)**:

- Every AI group in the world that isn't a player group, a garrison, a support team or zombies is
  picked up and tracked as a squad.
- If such a group belongs to one of the two playing factions, it counts against that side's soldier
  cap and can stop that side buying units.
- If it belongs to another faction, AC: Actual tracks it but no AC: Actual cap limits it.
- AC: Actual's raise of the active-AI limit doesn't make room for it.

**(suggestion)** With a mod that spawns its own AI, lower `-acrSoldierCap` by roughly the number of AI
that mod keeps alive at once, divided between the two sides, and keep FIA and zombies at few or off.

### Project Outbreak (zombies)

- **(checked)** AC: Actual runs its own nests and hordes with Outbreak's zombies and AI, under its own
  zombie cap (300 few, 500 many). You don't need to place any of Outbreak's own spawners or directors.
  If a scenario or Game Master adds them as well, those zombies are outside AC: Actual's cap.
- **(checked)** Outbreak has its own server-side budgets, for example one that clears piled corpses
  early when too many ragdolls are moving at once.
- **(suggestion)** Start with `-acrZombies few` and the default soldier cap. Night is the heaviest
  time: nests come out from further away, refill faster, send hordes more often and may raid bases.
- **(suggestion)** TvZ (`-acrZombieMode tvz`) has only one living side, so a soldier cap of 100 means
  about half the soldiers of a normal match, but 1.5 times the zombie cap. That's often an easier load
  than TvTvZ with both sides at full strength.

### Civil Population

- **(checked)** Civil Population reads its server settings from
  `ShadowCompany/SCCP_CivilPopulation.json` in the server's profile folder (written with defaults on
  first start). In its `budget` section: `maxCivilians` (default 60, at most 96), `maxAgents`
  (walking civilians with AI, default 16, at most 32), `maxActiveZones` (default 4, at most 6),
  `perZoneAgentCap` (default 10), `activeAiReserve` (default 8), and `yieldStart` / `yieldFloor`.
  Its `traffic` section has `enabled` and `maxCars`.
- **(checked)** Civil Population gives way to mission AI by itself: it never uses more AI agents than
  the active-AI limit minus `activeAiReserve` minus the mission's own AI, and it thins out its
  civilians once that share passes `yieldStart`.
- **(checked)** AC: Actual's raise of the active-AI limit only makes room for Civil Population's
  civilians when the AC: Actual Civil Population compat add-on is loaded. That add-on is still
  experimental and isn't on the Workshop yet. Without it, the civilians share the 64 spare slots and
  shrink as the battle grows.
- **(suggestion)** On a busy server, lower `maxAgents` and `maxActiveZones` before touching AC:
  Actual's caps: the civilians are atmosphere, the squads are the game.

## Is the server overloaded?

**Server FPS** is the best single sign. Check it with your host's tools or the server's own console
output, and note what it is early in a quiet match. **(suggestion)** If it falls well below that
under load and stays there, the AI respond late, vehicles drive badly and hits feel off for players.

Signs you can see in play **(checked as behaviour, the causes are a suggestion)**:

- AI squads freeze for a moment, or vehicles stop and re-plan often.
- Squad markers on the map with nobody in them: the engine is removing groups.
- Garrisons or FIA appearing late, when you're already inside a town.

Lines AC: Actual writes to the server log **(checked)**:

| Line | What it tells you |
|---|---|
| `[ACR] Host options: ... soldier cap N ... unit cap N ...` | The caps after the threat level |
| `[ACR] Threat level: ...` | Which threat level is in force |
| `[ACR] CAP SCALE ...: xF; squad caps ...; soldier cap A->B` | What map scaling did (only with it on) |
| `[ACR] Active AI limit N (was M)` | The raised engine limit |
| `[ACR] AI LIMIT: engine evicted G group(s); active A / limit L` | **Overloaded:** the engine is deleting AI groups. Lower the caps |
| `[ACR] Civilian socket: Civil Population plugged in, N AI slots` | The compat add-on is loaded and its room is added |
| `[ACR] ZOMBIES few: ... cap N ...` | The zombie setup and its cap |

Remove passwords, IP addresses and personal details before sharing a log.

## What to turn down first

**(suggestion)** In this order, one step at a time, playing a match between changes:

1. **Squads scale with map** off, if it's on.
2. **Threat** from high to normal.
3. **`-acrSoldierCap`** down by 20-25.
4. **FIA guerrillas, pockets and zombies** from many to few, or few to off.
5. **Other mods' AI**: their own population or spawn settings (Civil Population's `maxAgents` and
   `maxActiveZones`).
6. **Map size**: half instead of full on large maps.
7. **AI squad cap** down by 2-4.

If you find settings that work well for your server, or numbers that don't match this page, tell us
on [Discord](https://discord.gg/WK8xGSztB) or in a
[GitHub issue](https://github.com/propst-design/ArmaCommanderReforger/issues/new/choose).
