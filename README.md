# Arma Commander Actual (AC: Actual)


> **Source development:** `main` contains ongoing development and can be ahead of
> the Workshop release or contain unfinished changes. For normal play, use Workshop.
> Contributions are welcome: see [CONTRIBUTING.md](CONTRIBUTING.md) for setup,
> testing and pull requests. No direct repository access is needed.

[![Game: Arma Reforger](https://img.shields.io/badge/Arma-Reforger-556b2f?style=flat-square)](https://reforger.armaplatform.com/)
[![Development stage: Alpha](https://img.shields.io/badge/Status-Alpha-yellow?style=flat-square)](#requirements)
[![Requires Better Battle Buddies](https://img.shields.io/badge/Requires-Better_Battle_Buddies-teal?style=flat-square)](#requirements)
[![License: APL-SA](https://img.shields.io/badge/License-APL--SA-orange?style=flat-square)](LICENSE.md)

Command squads from the map, then take control of any friendly AI soldier and join
the fight. Capture bases, requisition units and coordinate support across Arland,
Everon and Kolguyev. Play solo against AI commanders, share command with friends, or
play side against side.

An unofficial Reforger rebuild of Arma 3's **Arma Commander**, licensed under
APL-SA. Original authors and contributors are credited below and in [LICENSE.md](LICENSE.md).

The current Workshop release is **0.5.12**, with **Better Battle Buddies 0.1.10**; `main` is
heading for 0.6.0 with Better Battle Buddies 0.2.0.
See [CHANGELOG.md](CHANGELOG.md) for the release history and what is waiting for the next one.

## Requirements

- **Arma Reforger 1.9** (the 1.9 fixes have been on Workshop since ACA 0.5.9 and
  Better Battle Buddies 0.1.8).
- **[Better Battle Buddies](https://github.com/propst-design/BetterBattleBuddies)** (BBB),
  available separately on Workshop. It is a declared dependency: let Workshop install it when
  prompted. BBB runs every soldier's fighting behaviour; ACA runs the commanders, economy and orders.
- Optional: [All-in-One Command Menu](https://github.com/propst-design/AIOCommandMenu) for
  ordering the squad you lead, ACE, RHS and Ukraine Armed Forces RHS, and ACA's own add-ons
  (content packs and extra maps, below). Everyone in a multiplayer game needs the same mods.

## Play from Workshop

1. Open Workshop in Reforger, search for **Arma Commander Actual**, download and enable it
   (Better Battle Buddies comes with it).
2. Pick an AC: Actual scenario (Arland, Everon or Kolguyev) under **Scenarios** to play solo,
   or under **Host** to play with friends.
3. The host (or a logged-in admin on a dedicated server) chooses the battlefield, factions and
   match options on the **setup screen**, then presses **Start**.
4. Each player then picks a side, an officer loadout and where to deploy on the deploy screen.
   You stay on that side for the match, reconnects included; hosts and admins can move a player
   with `/acr side west|east [player name]` in chat.

Scenarios have 16 player slots (8 per side). An AI commander runs any side with no player in
command, so solo, co-op, player-vs-player and mixed games all work. There are no victory
conditions and no saved matches: play until you decide it's over.

### Hosting a dedicated server

Run one of the AC: Actual scenarios like any Workshop scenario, with ACA and BBB in the server
config's mod list. Nothing spawns until an admin presses Start on the setup screen; if nobody
does, the server starts by itself 5 minutes after the first player joins. Every setup-screen row
also has a launch parameter (`-acr...`), so a server can preset its match: see
[Host options](MISSION-MAKING.md#host-options) for the full list. BBB's own options are
`-bbb...` parameters or a BBB preset (see the BBB README). BBB's experimental scripted driver
(`-bbbDriver 1`, off by default) also drives AI squads' wheeled vehicles on long moves, but not
a squad a player is in or commands, one getting out of fire, or near its own roadblocks; it is
untested in real matches. To match the AI load to your server
and player count, or to run ACA in a big mod stack, see [SERVER-SETUPS.md](SERVER-SETUPS.md).

## The game

**Maps.** Everon, Arland and Kolguyev are included. Everon has a hand-built full-island layout,
north/south halves and three flashpoints; Arland also has a hand-built layout. All three can
generate a layout instead, and Kolguyev always generates its bases and HQs from the map's named
places. On the setup map a generated layout can be previewed, rerolled and adjusted (click a
base, then its new position). Other terrains need an AC: Actual scenario: installing a terrain
alone does not add one.

**Economy.** Capture bases for requisition points (RP), buy squads and vehicles, and call
mortar, artillery, air and recon support. Threat level, pricing, starting RP, income and caps
are setup-screen choices.

**Decks.** Each side buys from a deck: cards picked from its faction's whole catalogue within a
points budget (100, 150 or 250 by threat level, or the setup screen's **Deck points** row). The
standard deck is the usual buy list, whatever its points; the catalogue also has extra kitted teams (RHS and the
Ukrainian forces mod), and you can make your own **custom cards**: a team of your faction's own
soldiers, two to twelve men, optionally with its own ride. Press **F3** on the map, or during
match setup, to open the deck builder: decks are saved in your profile, **Copy code** and
**Paste code** share them, and **Hand in** offers one for the match. The host's **Blue deck** /
**Red deck** rows pick what each side plays (any player's handed-in deck is listed there): left on
auto, a side plays the latest deck the host handed in for it, else the standard deck; **AI picks** has the side's AI commander build its own from its
company and doctrine. A buy list fixed by the
mission overrides any deck, and the **Decks** row turns them off.

**FOBs.** **Establish FOB** with a squad and a construction truck carrying 500 supplies: pick a
clear site and keep the squad there for the 90-second build. A FOB's store starts empty
(1,000 supplies); order 500 supplies for 20 RP, delivered after 60 uncontested seconds, or
(from 0.6.0) unload a construction truck's paid load beside its HQ. Its
eight upgrade spots take a hospital, maintenance bay, ammo dump, fuel, helipad, radio relay,
barracks and supply depot. By default a FOB needs a finished radio relay before it can deploy
units or reinforce. The setup screen's **Logistics** page changes these rules.

**Engineering.** Squads can dig in with **Fortify** (three tiers, with defenders held in reserve),
fortify a whole base stretch by stretch, set up roadblocks and checkpoints, and lay minefields of
any shape. AI commanders build too.

**The opposition.** AI commanders follow a doctrine (Balanced, Blitz, Firepower, Fortress or
Raider) and change it when it isn't working. FIA can hold pockets of resistance, run guerrilla
cells and hideouts, and side with whoever treats it best. Each is a setup-screen row.

**Zombies.** With the Project Outbreak mod loaded, the setup screen adds zombie nests in open
towns and the countryside, a players-against-the-dead mode and a contained outbreak that spreads
from one town, with civilian evacuations, supply airdrops and a truce between the two sides
(0.6.0; any hostile fire breaks it). Clearing a settled nest pays a bounty of at most 60 RP. See
`-acrZombies`, `-acrZombieMode` and `-acrOutbreak` in [Host options](MISSION-MAKING.md#host-options).

## Command map and controls

Open the command map with **F9** (no map item needed) or the game map with **M**:

- Left-click a squad or soldier marker to select that squad; left-click empty map to deselect.
- The ACA panel on the right shows the squad. **Switch to squad** takes over its leader (or
  another free soldier). From 0.6.0 the squad keeps its current order when you take over: the men
  follow you to it, and your first order replaces it.
- Commander only: **Combat / Fast / Silent** set how the squad moves; **right-click** the map to
  move it there. Arm **Defend**, **Get in**, **Fortify**, a roadblock or a minefield, then
  right-click (or right-drag to aim it). **Get out** dismounts the squad; **Unload** dismounts
  other squads riding in its vehicles. Standing orders include **Patrol** and **QRF**.
- **Ctrl + 1-9** keeps the selected squads as a control group; **1-9** selects them again.
- Several commanders can share a side, its squads and its RP. **Leave command** gives up
  command; **Take command** joins it when available.
- Every map panel (the ACA panel, INTEL, the Squads list) can be dragged by its title bar.
  The calm map is the default; `-acrMapDeclutter 0` brings back the classic one.

| Key | Action |
| --- | --- |
| F9 | Open/close the command map |
| F7 | Take over the soldier under your crosshair, or the marker under the map cursor |
| F8 | Return to your command unit |
| F5 | Move the selected squad to the cursor (map) or 35 m ahead |
| F6 (driver, pilot or commander of a vehicle) | Drop every passenger squad now (a second press within 3 s overrides the "too high/fast" warning) |
| F10 | RTS 3D camera (H hides its UI, F follows the selected squad, W/A/S/D pans, F9 goes straight back to the map) |
| F11 | Second-in-command panel on the map (with `-acr2IC 1`) |
| F3 | Deck builder (on the map, and during match setup) |
| W/A/S/D in a turret | Gunner drive with an AI driver (a map order hands control back) |
| Ping (unbound by default) | Tap to mark where you look, hold to pick a ping |
| Command menu key (default: backtick) | Native squad orders, only while possessing a squad leader |

F7 only takes over living soldiers in your side's commandable squads, never another player's
soldier or command unit. Rebind keys under **Settings > Controls > AC: Actual**. Off the map
the keys show along the top of the screen; **Key reminders** on the ACA panel (under **More**)
hides them. PC keyboard and mouse is the tested
control scheme; console/controller play is unverified.

**Intel** comes from what your soldiers perceive, from what a player sees within 300 m, and from
automatic reports inside an owned base's radius plus 60 m. A contact drops off the map 90 s after
its last update. The AI commander uses its own side's intel the same way.

## Factions and mods

The setup screen picks any two eligible factions a map lists as Blue side and Red side
(or `-acrFactions <west>,<east>`). The scenarios list the game's US, USSR and FIA, plus RHS US
and RHS Russia when RHS is loaded. Named pairs: vanilla US vs USSR, RHS Russia vs RHS US,
RHS Russia vs Ukraine, RHS US vs Ukraine (`vanilla`, `ru_us`, `ru_ua`, `us_ua`). Ukraine Armed
Forces RHS replaces the vanilla US faction for that session. RHS, Ukraine Armed Forces RHS and
ACE are optional and are not downloaded for you.

**Content packs and map add-ons.** Optional ACA add-ons bring vehicle and kit mods into the buy
lists or add an ACA scenario for another terrain. Load them like any mod, with their own
Workshop dependencies.

- **Cold War pack** and **Modern pack**: each side's buy lists and its own attack helicopter on
  the support menu (Cold War: AH-1S Cobra and Mi-24V Hind; Modern: AH-64D Apache and Mi-24V
  Hind for the RHS sides). The Modern pack builds on RHS, which needs an update for
  Reforger 1.9 before the Modern pack runs on 1.9.
- **Map add-ons**: Chernarus Minus, Gulfcoast Islands, Kunar Province, West Zagoria and
  Zimnitrita, each an AC: Actual scenario for that community terrain.

**Other faction mods (no promises).** Another faction mod works only if the map lists it (one line
in the map's faction manager) and the mod sets up its factions the standard way (Game Master
catalogs of soldiers, groups and vehicles). Even then it may look rough: mixed uniforms, a squad
leader as your command unit, vanilla stand-ins for what the mod lacks. Only RHS and Ukraine Armed
Forces RHS are hand-fitted. Want a faction mod supported properly? Ask in
[Discord #suggestions](https://discord.gg/WK8xGSztB) or on
[GitHub](https://github.com/propst-design/ArmaCommanderReforger/issues).

**AiO Command Menu** works beside ACA for the squad you lead; ACA's own orders and the AI
commander handle the rest.

## Reporting problems

Report bugs in [Discord](https://discord.gg/WK8xGSztB) under **#issues** (choose
**AC: Actual**) and ideas under **#suggestions**; no GitHub account needed. Or use
[GitHub Issues](https://github.com/propst-design/ArmaCommanderReforger/issues): search first and
add to an existing report when it describes the same problem.

Include the map, mod list, whether you hosted, joined or played solo, steps to reproduce, and logs
if you have them: `console.log` and `script.log` from `Documents/My Games/ArmaReforger/logs`
(dedicated servers may log elsewhere). ACA's lines start with `[ACR]`, BBB's with `[BBB]`.
Remove passwords, IP addresses and personal details before posting logs.

The older [ACActual repository](https://github.com/propst-design/ACActual) keeps historical
documentation, old issues and legacy ZIP downloads.

## Legacy ZIP and development launchers

Workshop is the normal route. For a source checkout or the legacy paired ZIP
(**ACActual-WithBBB.zip**, both addon folders), Windows launchers are included:

- **Play-ACA.cmd** (solo), **Host-ACA.cmd** (a local server you and friends join) and
  **Join-ACA.cmd** ask only for the map and optional mods (RHS / Ukraine Armed Forces RHS, ACE,
  an expansion pack, a map add-on); match settings are chosen on the in-game setup screen. They load AiO too when its
  Workshop copy is found. Joiners pick the same mods as the host; the join menu explains what to
  check.
- **Play-Test-Range.cmd** opens a small offline test range.

Unpacked local addons do not appear in the game's Scenarios/Host lists; use the launchers.
Users of the older single-addon ZIP must download the paired ZIP once; after that it updates itself.

## Making scenarios and contributing

- [MISSION-MAKING.md](MISSION-MAKING.md): build an AC: Actual map in your own addon, every
  World Editor setting and every `-acr...` host option.
- [SERVER-SETUPS.md](SERVER-SETUPS.md): what each AI limit does and starting setups by server
  size and player count.
- [CONTRIBUTING.md](CONTRIBUTING.md): set up a working copy, test and open a pull request.
  The pinned **Roadmap** issue lists what is being worked on.

## Made with AI

This project is openly vibecoded. Nearly all of the code, launchers and documentation
were written by **Claude** (Anthropic's AI model), directed, designed and play-tested by
**Jacob** ([propst-design](https://github.com/propst-design)), with **SlyZilla**
([SlyZilla](https://github.com/SlyZilla)) as co-developer.
**Codex** (OpenAI) and **Antigravity** (Google) also contribute.
Expect rough edges; bug reports with logs help a lot.

Playtesters: **PoppiPoppins**, **Hexxyz** and **Frost**.

## Licence

APL-SA (Arma Public License Share Alike); see [LICENSE.md](LICENSE.md) for the terms and the
original Arma Commander credits.
