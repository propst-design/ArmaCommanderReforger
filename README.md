# Arma Commander Actual (AC: Actual)


> **Source development:** `main` contains ongoing development and can be ahead of
> the Workshop release or contain unfinished changes. For normal play, use Workshop.
> Contributions are welcome: see [CONTRIBUTING.md](CONTRIBUTING.md) for setup,
> testing and pull requests. No direct repository access is needed.

[![Game: Arma Reforger](https://img.shields.io/badge/Arma-Reforger-556b2f?style=flat-square)](https://reforger.armaplatform.com/)
[![Development stage: Alpha](https://img.shields.io/badge/Status-Alpha-yellow?style=flat-square)](#maps-and-match-options)
[![Requires Better Battle Buddies](https://img.shields.io/badge/Requires-Better_Battle_Buddies-teal?style=flat-square)](#maps-and-match-options)
[![License: APL-SA](https://img.shields.io/badge/License-APL--SA-orange?style=flat-square)](LICENSE.md)

Command squads from the map, then take control of a friendly AI soldier and join
the fight. Capture bases, requisition units and coordinate support across Arland,
Everon and Kolguyev. Play solo against AI commanders or share command with friends.

An unofficial Reforger rebuild of Arma 3's **Arma Commander**, licensed under
APL-SA. Original authors and contributors are credited below and in [LICENSE.md](LICENSE.md).

This is the main public source, development and [issue tracker](https://github.com/propst-design/ArmaCommanderReforger/issues) for AC: Actual.
Install and update through the in-game Workshop for normal play. The older
[ACActual repository](https://github.com/propst-design/ACActual) retains historical
documentation, existing issues and legacy ZIP downloads/updaters. File new bug
reports in [Discord](https://discord.gg/WK8xGSztB) under **#issues** and ideas under
**#suggestions**; no GitHub account needed. This repository's issue tracker remains
available as an alternative.

The current Workshop release is **0.5.8**, with **Better Battle Buddies 0.1.7**.
See [CHANGELOG.md](CHANGELOG.md) for the release history.

## Playing from Workshop

Workshop is the normal install route: open Workshop in Reforger, search for **Arma Commander
Actual**, download and enable it, then select an included AC: Actual scenario in Scenarios or Host. The host/admin picks the sides and match options on the setup screen, then presses Start. No
external launcher is needed for Workshop play. Report bugs in [Discord](https://discord.gg/WK8xGSztB)
under **#issues** (choose **AC: Actual**), or use [GitHub Issues](https://github.com/propst-design/ArmaCommanderReforger/issues).
Include the map, mod set, steps to reproduce and logs if available. No GitHub account is needed for Discord reports.

## Maps and match options

Everon, Arland and Kolguyev are included. Everon has a hand-built full-island layout, north/south
halves and three flashpoints. Arland also has a hand-built layout. All
three support generated layouts; Kolguyev always generates its bases and HQs from named map
locations. Generated layouts can be previewed, rerolled and adjusted by clicking a base and then its
new position on the in-game setup map. Other terrains need an AC: Actual scenario setup; installing
a terrain alone does not add one.

Capture bases for RP, requisition squads and vehicles, and call mortar, artillery and air support.
Construction trucks build service stations, forward deploy points and helipads. There are no victory
conditions or saved matches. AC: Actual requires
[Better Battle Buddies](https://github.com/propst-design/BetterBattleBuddies), available
separately on Workshop. Allow Workshop to install the dependency when prompted.

In **0.5.6**, use **Establish FOB** with a squad and a construction truck carrying
500 supplies. Pick a clear site and keep the squad there for the 90-second build.
Select the completed FOB to order supplies and develop its eight upgrade spots:
hospital, maintenance, ammo, fuel, helipad, relay, barracks and supply depot.
Upgrade Defences through sandbags, machine-gun nests, then bunkers and guard towers.
The AI commander can build, supply and raid FOBs too. Destroyed headquarters take a
FOB offline; rebuild friendly headquarters with Establish FOB.

FOBs start with an empty 1,000-supply store. Order 500 supplies for 20 RP, delivered
after 60 uncontested seconds. Upgrades spend local supplies and build faster with
engineers nearby; subsequent construction needs no truck. FOBs do not refill trucks for free.
FOBs require a completed friendly radio relay inside their circle before deploying units or providing
RP reinforcement. Losing the relay disables both until rebuilt; construction, deliveries and supplied
services continue. Mission makers and hosts can disable this requirement in Logistics settings.
One supply depot expands a FOB to 3,000 capacity. Hospital, ammo, maintenance and fuel buildings
use its supplies for automatic service; RP reinforcement remains available. Generated settlements
send infantry replacements, while HQs, military sites and FOBs deploy new formations. The startup
**Logistics settings** page lets hosts change these policies, capacity bonuses and service costs;
mission makers can set defaults and per-base roles.
Heavy maintenance bays repair 40% more per step than light bays for the same supplies. Completed
small/large living quarters inside a friendly FOB add 11/19 defenders; multiple quarters stack,
casualties persist, and losing quarters reduces capacity. Engineers do not automatically build blueprints.

## Legacy ZIP / development launchers

The release ZIP includes **Play-ACA.cmd**, **Host-ACA.cmd**, **Join-ACA.cmd** and a **READ ME
FIRST.txt** guide. Play and Host ask only for the map and optional mods before loading; choose
playable sides and match settings on the in-game startup screen, then press **Start**.
New paired ZIPs use **ACActual-WithBBB.zip** and contain both addon folders. Users of the older
single-addon ZIP must download/extract the complete paired ZIP once; the old updater cannot install
BBB. After that migration, published paired ZIPs self-update. Keep custom work in a separate addon or use a development copy
without the release updater source.

## Playing the test range

Close Reforger, then double-click **Play-Test-Range.cmd** (offline, solo). In the development
checkout, **Host-Test-Range.cmd** opens the normal game menu with the addon loaded. Unpacked local
addons do not appear in the game's Scenarios/Host lists; use **Host-ACA.cmd** for a local
multiplayer scenario instead. The ZIP does not include Host-Test-Range.cmd.

## Factions and mods

The host setup menu can run vanilla US vs USSR, RHS US vs RHS Russia, RHS Russia vs Ukraine, or RHS
US vs Ukraine. RHS and Ukraine Armed Forces RHS are optional; the launcher adds the required addon
IDs when selected; it does not download those mods. ZIP launcher users may need to adjust addon
search paths to match their installation. Ukraine Armed Forces RHS replaces
the vanilla US faction for that session, so choose a pair containing **UA** to play it. ACE All in
One is a separate optional toggle. Play/Host/Join also automatically load All-in-One Command Menu if
its Workshop copy is found locally; it is a separate optional mod and joiners need it when the host
loads it. Joiners should select the same faction pair and ACE setting as the host; a mod mismatch
can prevent joining, and a different faction choice can produce the wrong setup. The join menu
explains the selections to check.

**Other faction mods (no promises).** AC: Actual can play any two eligible factions a map lists,
picked as Blue side / Red side on the in-game setup screen (or `-acrFactions WEST,EAST`). The three
scenario worlds list the game's US, USSR and FIA, plus RHS US and RHS Russia when RHS is loaded.
Another faction mod works only if the map lists it (one line in the map's faction manager) and the
mod sets up its factions the standard way (Game Master catalogs of soldiers, groups and vehicles).
Even then it may look rough: mixed uniforms, a squad leader as your command unit, vanilla stand-ins
for what the mod lacks. Only RHS and Ukraine Armed Forces RHS are hand-fitted. Want a faction mod
supported properly? Ask in [Discord #suggestions](https://discord.gg/WK8xGSztB) or on
[GitHub](https://github.com/propst-design/ArmaCommanderReforger/issues).

## Command map and controls

Open with M or F9:

- Left-click a squad or soldier marker to select that squad. Left-click empty map to deselect.
- The panel on the right shows the squad. **Switch to squad** takes over its leader (or another free soldier).
- Commander only: **Combat / Fast / Silent** set how the squad moves; **right-click** the map to move it there.
  **Defend** or **Get in**, then right-click, to defend a spot or board a vehicle near the click.
  **Get out** dismounts it; **Unload** dismounts other squads riding in its vehicles.
- Multiple commanders can share a side, its squads and its requisition pool.
  **Leave command** gives up command; **Take command** joins it when available.

| Key | Action |
| --- | --- |
| F7 | Possess the soldier under your crosshair, or the marker under the map cursor |
| F8 | Return to your command unit |
| F9 | Open/close the command map (no map item needed) |
| F6 (in a vehicle) | Drop every passenger squad now (a second press within 3 s overrides the "too high/fast" warning) |
| F5 | Move selected squad to cursor (map) or 35 m ahead |
| W/A/S/D in a turret | Gunner drive with an AI driver (a map order hands control back) |
| Command menu key (default: backtick) | Native squad orders, only while possessing a squad leader |

The ACA panel and INTEL's header can be dragged anywhere on the map (double click: back home). Off the map the keys show along the top of the screen; **Key reminders** on the ACA panel hides them.

F7 only takes over living soldiers in your side's commandable squads, never another player's soldier
or command unit. Keys can be rebound under **Settings > Controls > AC: Actual**. PC keyboard/mouse
is the tested control scheme; console/controller play remains unverified.

Intel comes from AI perception, player line of sight within 300 m, and automatic reports inside an
owned base's radius plus 60 m (no line-of-sight requirement). Contacts turn suspected after 30 s and
expire after 90 s without an update. The AI commander uses its side's intel.

Logs: normal play uses `Documents/My Games/ArmaReforger/logs`. Mod lines use
`[ACR]` and `[BBB]`; ZIP-host server logs use
`ArmaCommanderReforger/.local-server/logs`.

## Making scenarios and contributing

See [MISSION-MAKING.md](MISSION-MAKING.md) to build scenarios in your own addon.

Maintainers: [Workshop-to-Discord changelog relay setup and recovery](docs/workshop-relay.md).

## Made with AI

This project is openly vibecoded. Nearly all of the code, launchers and documentation
were written by **Claude** (Anthropic's AI model, working in Claude Cowork), directed,
designed and play-tested by **Jacob** ([propst-design](https://github.com/propst-design)).
**Codex** (OpenAI) and **Antigravity** (Google) also contribute.
Expect rough edges; bug reports with logs help a lot.

Playtesters: **PoppiPoppins**, **Hexxyz** and **Frost**.
