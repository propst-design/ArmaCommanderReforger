# Make an AC: Actual map

AC: Actual is designed to be easy to make missions for. You can adapt a shipped scenario, build a
hand-placed battlefield or let the mode generate a layout on a suitable terrain. Most setup uses
World Editor settings and the mission header; you do not need to write scripts to get started.

This guide takes you from your own addon to a playable scenario, then covers the settings,
logistics, testing and sharing. Start with [Quick start: copy Arland](#quick-start-copy-arland)
for an authored layout, or its generated-layout alternative for a procedural battlefield.

> AC: Actual is an alpha. Arland is the main automated test map; Everon is the main large-map
> playtest terrain. Kolguyev uses generated layouts. New maps, faction combinations and multiplayer
> setup paths still need playtesting. AC: Actual requires standalone BBB (`BBB0261015000001`); install both in Workbench. Source developers keep the checkouts side by side. Custom scenarios also need their terrain dependencies. Mod log lines use `[ACR]` (the included AI systems also use `[BBB]`).

## Contents

1. [Before you start](#before-you-start)
2. [Quick start: copy Arland](#quick-start-copy-arland)
3. [What a world needs](#what-a-world-needs)
4. [Entity reference](#entity-reference)
5. [Mission header](#mission-header)
6. [Save, export and run](#save-export-and-run)
7. [Host options](#host-options)
8. [Designing a good map](#designing-a-good-map)
9. [Current limits](#current-limits)
10. [Battlefield character](#battlefield-character)
11. [Testing your scenario](#testing-your-scenario)
12. [Play a custom map with a friend](#play-a-custom-map-with-a-friend)

## Before you start

- **Arma Reforger Tools** (Workbench) from Steam. Workbench is the editor used to make and test maps.
- **The AC: Actual addon.** Use the installed Workshop project if available, or get the
  `ArmaCommanderReforger` folder from the [release ZIP](https://github.com/propst-design/ACActual/releases/latest)
  and add its `addon.gproj` to Workbench with **Add Existing Project**. Avoid loading two copies with
  the same addon ID. Keep custom work out of an installation that self-updates.
- **Your own addon** (your own mod project). Set AC: Actual, GUID `AC230926A0010001`, as a dependency so
  your map can use AC: Actual's game mode and settings. Keep your work in your own addon so an AC: Actual
  update does not replace it.
- **Licence:** AC: Actual is APL-SA (see [LICENSE.md](LICENSE.md)). Scenarios that include or modify its
  files must stay APL-SA and credit the original authors.

In this guide, a **world** is the map and the objects placed on it. An **addon** is the mod folder that
contains and shares your work. A **mission header** gives your world a name in the game's scenario list.
**RP** means requisition points: the money sides spend on units and support.

## Quick start: copy Arland

For a hand-placed layout, copy the working Arland setup and move its objects:

1. In your addon, create a world using the terrain you want (for example, Everon).
2. In AC: Actual's `Worlds/ACR_Arland.ent`, copy everything from the `default` layer and paste it into
   your new world's `default` layer. These objects set up the game mode, bases, and units.
3. Move the spawn point, bases, faction settings, and starting units to suit your map.
4. Set the AI world to use this terrain's navigation files. These files tell soldiers and vehicles where
   they can move; see [AI world and navmesh](#ai-world-and-navmesh).
5. Create a mission header so the game can show your map by name. See [Mission header](#mission-header).
6. Play it from Workbench and check that bases, units, and the command panel appear. See
   [Testing your scenario](#testing-your-scenario).

For a generated layout, use `Worlds/ACR_Kolguyev.ent` and its layer as the example instead: copy the
managers and game mode to a world on your terrain, configure its navmesh and factions, and provide a
player spawn point for each side. Leave out `ACR_Base` objects to generate bases automatically. The
terrain needs usable named map places and enough dry land to form a plan; generation is not
guaranteed on every terrain. See [Generated layouts](#generated-layouts).

To play a custom map with a friend, see [Play a custom map with a
friend](#play-a-custom-map-with-a-friend).

## What a world needs

Put these objects in the world's `default` layer. A **prefab** is a ready-made object you choose from
Workbench's browser. Paths starting with `ACR_` are part of AC: Actual; the others come with the game.

| Entity | Prefab / class | Needed? | Why |
|---|---|---|---|
| Game mode | `Prefabs/MP/Modes/Plain/GameMode_Plain.et` | Yes | Lets the game handle spawning, the map, and construction components (below) |
| Faction manager | e.g. `Prefabs/MP/Managers/Factions/FactionManager_USxUSSR.et` | Yes | Sets the sides in the match. Their faction keys must match the AC: Actual settings |
| Loadout manager | e.g. `Prefabs/MP/Managers/Loadouts/LoadoutManager_USxUSSR.et` | Yes | Sets what players spawn with (AC: Actual uses officer gear; see [Current limits](#current-limits)) |
| Player spawn point | e.g. `Prefabs/MP/Spawning/SpawnPoint_US.et` | Yes, one per playable side | Marks that side's HQ. Players can also deploy at every other base their side can deploy units at; the mode adds those spawn points itself |
| AI world | `Prefabs/AI/SCR_AIWorld.et` with the map's navigation files | Yes | Gives AI soldiers and vehicles routes they can use |
| Perception manager | `Prefabs/World/Game/PerceptionManager.et` | Yes | Lets AI notice other units |
| Radio manager | `Prefabs/Systems/Radio/RadioManager.et` | Yes | Radio system |
| Chat entity | `Prefabs/MP/Campaign/CampaignMPChatEntity.et` | Yes for chat | Copy `ACR_Chat` from a shipped world; the plain game mode does not supply chat |
| **`ACR_Mode`** | class `ACR_Mode` | Yes, exactly one | Starts the mode |
| **`ACR_FactionSettings`** | class `ACR_FactionSettings` | Recommended, one per side | RP, buy list, AI commander, supports |
| **`ACR_Base`** | class `ACR_Base` | Hand-built layouts only | Capture/deployment points. With none, the mode generates a layout |
| **`ACR_StartingUnit`** | class `ACR_StartingUnit` | Optional | Units that exist when the match starts |

### Settings on the game mode

Add these components to the game mode object. The Arland map has working examples you can copy:

- **`SCR_RespawnSystemComponent`** with an `SCR_MenuSpawnLogic` (leave its forced faction empty), plus an
  **`SCR_RespawnTimerComponent`**, and untick **Allow Faction Change** on the game mode. Players then pick
  their side, an officer loadout and where to deploy on the game's deploy screen, after the host presses
  Start; the respawn timer uses `m_iRespawnSeconds`. A player stays on the side they first deployed on for
  the whole match, reconnects included; an admin can move them with the chat command
  `/acr side west|east <player>`. A world with an `SCR_AutoSpawnLogic` still works, but everyone spawns on
  its forced side.
- **`SCR_MapConfigComponent`** (optional): gadget map config
  `{AC230926A0010098}Configs/Map/ACR_MapFullscreen.conf` gives the command map finer contour lines.
- **`SCR_CampaignBuildingManagerComponent`** (optional): lets construction trucks build. Without it,
  construction trucks are just trucks.
  Some structures players build this way do something in AC: Actual (anywhere on the map, owned by the
  builder's side, until dismantled or destroyed):
  - **Field Hospital**: soldiers of its side within 25 m are healed after about 20 s there.
  - **Vehicle Maintenance**, **Fuel Storage**: vehicles within 25 m are slowly repaired / refuelled.
  - **Ammo Storage**: soldiers within 25 m get magazines and launchers back; vehicle guns are refilled.
  - **Helipad**: a pad for support helicopters and where bought helicopters appear, for the side's nearest base.
  - **Headquarters / Player Hub**: away from existing bases it becomes a forward base ("FOB 1"...) where
    units can be bought and reinforcements deploy once a radio relay is built; hidden from the enemy until found, no income.
  - **Radio relay** (small Antenna service composition): a completed friendly relay inside the
    FOB's circle enables new-unit deployment and RP reinforcement. The native relay costs 250
    construction supplies. Its blueprint, the CP's built-in radio and enemy/outside relays do not count.
  - **Supply depot** (Supply Cache / Supply Storage): inside a friendly FOB's circle, increases
    its capacity by 2,000 supplies. This bonus applies once per FOB, even with multiple depots.
  AI squads that need ammo, repairs, fuel or healing also head for supplied service buildings.
  By default, static service buildings require a friendly FOB in whose circle they stand and
  consume its supplies; placing them outside the circle does not bypass that cost. Healing,
  infantry ammunition, each 15% repair/fuel step cost 10 supplies; each turret magazine costs 5,
  and a replacement disposable launcher costs 20. Service pauses while the FOB is contested.
  The startup **Logistics settings** page can restore free field services. Mobile service trucks
  retain their own stock system. Initial construction
  draws from construction-truck stock. A built FOB starts with an empty **1,000-supply** store
  (the vanilla command post's storage capacity). Select its map marker as commander and order
  **500 supplies for 20 RP**, delivered after **60 uncontested seconds**. One delivery at a time;
  at least 500 free capacity is required. Standing inside the FOB's 40 m circle, choose **Build at
  FOB** to use this stock without a truck. Deliveries fill the CP's native physical storage; its
  engineering table opens the same paid build menu, including service buildings and supply depots.
  Construction and automatic services share this one balance. Supplies are charged when placing a blueprint; completing
  it does not charge again. Destroying/dismantling the command post removes its store and loses any
  pending paid delivery; there is no refund. Built FOBs never provide free truck restocking, and paid
  stock cannot be loaded into trucks. Existing non-built friendly deployment bases retain truck
  restocking. Losing the last depot reduces capacity to 1,000 and loses excess stored stock;
  a paid delivery that no longer fits waits until space is available. Depot bonuses never add
  supplies themselves. AI base/helipad building is not implemented.

  **RP reinforcement remains available at relay-equipped FOBs**, including with an empty supply store. In supplied
  service mode, instant infantry healing/ammunition from the Reinforce action costs 3 RP per soldier
  actually serviced; replacement soldiers still cost 3 RP each. Instant vehicle rearm/repair retains
  its existing RP price. Supplied service buildings provide gradual, cheaper service and require the
  matching building (hospital, ammo, maintenance, fuel); a supply depot provides storage only.

  Heavy maintenance bays repair 21% per 10-second service step versus light bays' 15%, with the
  same range and supply cost. Completed friendly living quarters inside a built FOB add 11
  defenders for the small (275-supply) variant or 19 for the large (475-supply) variant. Multiple
  quarters stack. Defenders join the normal garrison system, deploying when enemies approach and
  keeping casualties when packed away. Quarters grant defenders once; losing quarters lowers
  capacity and recalls excess AI guards. Captured enemy quarters give no bonus. Engineers do not
  automatically complete player blueprints; players still use the shovel/build interaction.

### Deployment and reinforcement logistics

Generated sites have explicit roles. HQs, captured military sites (including airfields) and built
FOBs with a completed friendly radio relay can deploy new squads/vehicles. Losing the relay disables
FOB deployment and RP reinforcement until it is rebuilt. Construction, paid deliveries and automatic
supply-paid services continue without it. The same eligibility applies to AI purchases; AI construction
remains future work. Captured military bases and villages retain their existing rules. The relay gate
applies even under the all-points policies; disable it separately with `m_iAcrFobRelay = 0`,
`-acrFobRelay 0`, or **FOB radio relay required: off** in startup Logistics settings.
Cities, towns, villages and settlements can send infantry
replacements but cannot deploy new formations or provide instant vehicle service. Industry and
landmarks remain income/territorial objectives. Replacements still walk from the nearest eligible
friendly base when the squad is outside one. The existing enemy-within-300-m and contested-base
checks remain; no encirclement or road-supply-line simulation is added.

Authored bases default to `AUTHORED`, retaining their deployment behavior. Mission makers can assign
`m_eLogisticsRole` and independently disable new-unit deployment with `m_bAllowSpawn` or infantry
replacements with `m_bAllowReinforce`. Player respawn locations are not changed by formation-deployment
policy. Hosts can override logistics on the startup screen before Start; header defaults and CLI
flags also work through the normal options precedence.

### Minefields

Infantry squads can lay minefields and disarm mines on the commander's orders (map panel: **AP
mines**, **AT mines**, **Mixed mines**, **Disarm mines**). A field is a 30 m square: AT mines go in
both lanes of a road within 20 m of the click (across the area without one) and AP mines off the
road; vanilla minefield signs mark the corners and the area shows on the owning side's command map.
Orders cost 15 / 20 / 25 RP (AP / AT / mixed) or 3 RP to disarm and lift every armed mine within
25 m (8 s per mine, 4 s with engineers); they are paid up front and refund the share not laid when
called off. Work pauses while the site is contested and runs twice
as fast with sappers / engineers or a construction truck. Fields are refused within 50 m of a base
centre. The owning side's AI never sets off its own mines; its players do. USSR, Russian, SLA and
FIA factions lay PMN-4 / TM-62M mines, the others M14 / M15. Hosts change the three options below
on the startup Logistics settings page.

### Roadblocks

Infantry squads build roadblocks on the commander's orders (map panel: **Roadblock S**,
**Roadblock M**, **Roadblock L**, **Barricade**, then right-click a road). The roadblock snaps to
the road centre within 20 m of the click and faces the nearest enemy your side knows of within
600 m, else the nearest enemy base your side can see, else along the road.

| Order | Builds | RP | Truck supplies | Work |
|---|---|---|---|---|
| Roadblock S | small checkpoint, no machine gun | 12 | 75 | 35 s |
| Roadblock M | checkpoint with a machine gun | 20 | 225 | 45 s |
| Roadblock L | large checkpoint with a heavy machine gun | 30 | 350 | 70 s |
| Barricade | barricade sized to the road (small up to 6 m wide, medium up to 9 m, else large), no gun | 8 | 40 | 25 s |

USSR, Russian (also RHS AFRF) and SLA sides build the Soviet compositions, the others the US ones.
FIA has no checkpoints: it builds log barricades sized to the road at the barricade's price, and
its panel shows only **Barricade**. A construction truck within 50 m of the squad pays in supplies
when it holds enough (never one another squad has reserved for a FOB); otherwise the side pays RP.
Like any new order, a roadblock order replaces the squad's current job, and what that job gives
back counts towards the price. An order called off before the roadblock goes in (a new order, Halt,
the squad lost, 3 minutes at the road without progress, or a walk there that takes too long) is
refunded where it was paid from; a destroyed truck's supplies are lost. Time while a regroup or a
player has set the order aside doesn't count. The squad waits beside the road, clear of the spot.
Work pauses in contact, and the roadblock goes in once no vehicle stands on the spot. The squad
then holds it and mans its gun; the roadblock stays when the squad leaves. Roadblocks are refused
within 120 m of a base and 180 m of another roadblock. AI commanders build medium checkpoints paid
in RP, and the 3D commander camera's **Roadblock** button orders a medium checkpoint.

### Fortify position

**Fortify** (map panel) sends an infantry squad to dig in where you right-click. The position
faces the nearest enemy your side knows of within 600 m of the spot, else the nearest enemy base
your side can see, else away from the squad (the way it was going). Ordering Fortify again within 25 m of one of your positions upgrades it:

| Tier | Pieces | Cost | Work |
|---|---|---|---|
| I, hasty | a line of three sandbag positions, a barbed-tape fence about 12 m in front (the guns fire through it) | 10 RP | 45 s |
| II, dug in | a machine-gun nest in place of the middle position (the gun in the centre of the line), a sandbag bunker under a camo net behind it | 15 RP | 60 s |
| III, strongpoint | a bunker instead of the covered post, a heavy MG past the right end of the line turned in across the front for crossfire (the left end when the right has no clear place or line of fire) | 25 RP | 90 s |

The pieces are the side's own (US, USSR and Russian, or FIA). They go in when the work is done
and no vehicle stands on a spot. A blocked spot tries points beside it, under the same rules as
FOB defences, and never stands on a big rock; asphalt and concrete (a runway, a car park) take a
piece only when no other spot will. The guns sit a little ahead of the posts, and each needs a
clear line of fire: nothing but wire within 15 m in front (another piece, a building, a tree or
rising ground makes it try the next point, never one further back). A new position skips a spot that stays blocked but needs at least one piece besides its
wire; an upgrade needs every new piece to fit, or nothing goes in and it is refunded, so it can be
ordered again. Work pauses while the site is contested and runs twice as fast with
sappers / engineers or a construction truck. The order is paid up front (what the squad's current
job gives back counts) and refunded in full if it is called off before the pieces go in. Positions
are refused within 30 m of a base centre and 40 m of another of your positions. The squad then
holds the position and mans its guns; the pieces stay when it leaves. The position shows on its
side's command map as a pale ring until every piece is destroyed or dismantled, and its wire goes
with it then. Tier II and III hold defenders in reserve at the position, from the side's garrison
soldiers (2 at tier II, 4 at tier III; an upgrade adds the difference). Like a base's garrison
they deploy when an enemy comes within 400 m, defend the position, pack up once none is within
450 m and are used up as they die; a position that is gone takes its reserve with it. Hosts
change the options below on the startup Logistics settings page; the defender sizes are launch
flags and header fields only.

### AI world and navmesh

On the `SCR_AIWorld` object, choose the navigation files for your terrain: one for soldiers (`NavmeshWorld`),
one for vehicles (`ChimeraNavmeshWorld`), and the low-detail one if available. A navigation file is the
map's movement guide for AI. Arland and Everon use the Game Master files (`worlds/GameMaster/Navmeshes/GM_Arland*.nmn`
and `GM_Eden*.nmn`). Find the matching files in the game's data, or make them in Workbench for a custom
terrain. AC: Actual finds roads and checks for land on its own.

## Entity reference

The tables below explain the settings for each AC: Actual object. Workbench usually hides the internal
prefix: for example, `m_sName` appears as **Name**. If you leave a setting empty, AC: Actual uses the
default shown here.

### ACR_Mode

One per world. Place it anywhere.

| Setting | Default | What it does |
|---|---|---|
| `m_bHostGameMaster` | on | The host and server admins also get Game Master |
| `m_bSharedCommand` | on | Every player of a side may command all its squads. Off: one commander per side |
| `m_iAISkill` | 70 (Veteran) | Skill of all AI soldiers in the mode |
| `m_fAIPerception` | 1.6 | How fast AI spot and identify targets (game default 1) |
| `m_iRespawnSeconds` | 15 | Delay before a player whose command unit was killed can redeploy |

### ACR_FactionSettings

Add one for each side and place it anywhere (Arland puts each beside its HQ to keep the layer tidy).
If you leave one out, that side still gets 50 RP, the automatic buy list, and all support calls, but its
AI commander is turned off.

| Setting | Default | What it does |
|---|---|---|
| `m_sFaction` | `US` | Which side these settings are for. It must match a side in the faction manager, such as `US` or `USSR` |
| `m_iStartingRP` | 100 | Requisition points at the start |
| `m_bAICommander` | on | An AI commander leads this side whenever no player is in command |
| `m_iAIUnitCap` | 8 | Maximum number of squads the AI commander keeps in the field |
| `m_iAIAttacks` | 1 | How many base attacks the AI commander can run at once |
| `m_eAIDoctrine` | `RANDOM` | How the AI commander fights: `RANDOM` (a new pick each match), `BALANCED`, `BLITZ`, `FIREPOWER`, `FORTRESS` or `RAIDER` (see AI commander doctrines). The setup screen or `-acrDoctrineWest`/`-acrDoctrineEast` override it |
| `m_ePricing` | `STEPPED` | How the buy list limits purchases: `STEPPED` or `FIXED_STOCK` (see Prices below) |
| `m_bAutoFill` | on | Add the faction's eligible Game Master catalog units to the buy list at automatic prices |
| `m_aRequisitions` | empty | Units you add to the buy list yourself. An entry replaces the matching automatic item |
| `m_aExclude` | empty | Groups or vehicles to leave out of the automatic buy list |
| `m_aGarrisonSoldiers` | empty | Soldiers used to guard bases. Empty uses the faction's riflemen, machine gunners, grenadiers, and AT soldiers |
| `m_aPrefabMappings` | empty | Replace units, crews, shells, rockets, or helipad models with chosen prefabs. If no match is found, keep the original and log a fallback |
| `m_bSupports` | on | Let the commander call support |
| `m_aSupports` | empty | Support calls shown to the commander. Empty offers all calls at their default prices |
| `m_aSupportUnits` | empty | Settings for support units that can be bought. No row keeps the current choice; a row can change or hide that unit |
| `m_sSupportHelicopter` | empty | Legacy gunship airframe fallback; a `GUNSHIP` support-unit row takes precedence |
| `m_sSupportTransportHelicopter` | empty | Legacy door-gunner airframe fallback; a `DOOR_GUN_HELI` support-unit row takes precedence |
| `m_sSupportRocket` | empty | Rocket for rocket strikes and gunships. Empty = Hydra 70 / S-5 |

AC: Actual uses faction keys to tell sides apart. For default support assets, `RHS_AFRF` and keys
starting with `USSR` count as eastern; otherwise keys containing `US`, or exactly `NATO` or
`BLUFOR`, count as western. Other keys use eastern defaults unless prefab mappings replace them.

#### AI commander doctrines

Each AI commander starts the match with one doctrine. By default each side gets a random one;
the setup screen (Blue/Red AI doctrine), `-acrDoctrineWest`/`-acrDoctrineEast` or `m_eAIDoctrine`
pick it. Doctrines change how the commander plans, attacks, defends, moves, fires and buys, never how
big its army is: squad cap, RP and stock are the same for all of them (Raider's 2-3 man teams take
half a squad-cap slot each, so it fields about as many men; the soldier cap still applies).

| Doctrine | How it fights |
|---|---|
| Balanced | The standard commander |
| Blitz | Buys about twice as many armed vehicles and an extra truck, rides from 600 m and air-lifts from 1.7 km, grabs more empty bases, attacks on thinner odds and sooner after a lull, shorter base of fire, presses on with heavier losses, builds FOBs closer to the front, lays one minefield at most, refits late, fires fewer mortar missions, keeps little RP for fire missions, keeps a reserve only with a bigger force |
| Firepower | Buys mortar teams, gunships and forward observers first and keeps the RP for two fire missions (air and rocket strikes too), air or rocket preparation on more targets and on defence against smaller attacks, mortars fire more often, longer base of fire, wants better odds, one extra front at most, defenders hold the point, falls back sooner |
| Fortress | Sends more defenders, most of them out to a forward line, and they hold to 25% strength, gives a base up only against 6x its defence, counterattacks a lost base on 55% of the usual force for 10 minutes, keeps a reserve sooner, up to three roadblocks and four minefields, clears FIA pockets near its bases sooner, refits early, walks farther before a ride, fewer grabs and fronts. Builds and mans more: twice the AI build budget's share and trickle, up to four Fortify positions, and up to four builders that stay on what they built (never pulled away to defend a base) |
| Raider | More small teams, goes for weakly held bases and FOBs first, opens many fronts with two squads each, air-lifts from 1.5 km, breaks off an attack at half strength (and leaves that base alone for 5 minutes), defenders slip away at half strength, leaves FIA pockets alone longer, fewer vehicles and minefields |

**Discovering a doctrine.** The other side can work out an AI commander's doctrine from what it sees:
each enemy squad reported in contact (more for vehicles, armour and aircraft), an enemy FOB found, a
base taken from that commander (a lot) or lost to it. At 30 points the side gets a hint (the doctrine
and the one it looks most like, e.g. "Blitz or Raider"), at 60 the doctrine is confirmed. Each step is
a feed line, a notice and a report timeline entry, and the map panel's Report shows "Enemy doctrine:
unknown / X or Y? / X (confirmed)" with the intel so far. Players on an AI-commanded side see their own
commander's doctrine there too. An AI commander gets the same picture and nothing more.

**Changing doctrine.** From 15 minutes in and every 10 minutes after, an AI commander reviews the
last window from its side's report: two or more bases lost net means Fortress (Firepower if already
Fortress), heavy losses (12+ and 1.6x what it killed) mean Firepower (or Fortress), nothing changing
hands means Blitz (or Raider); a commander that is winning keeps its doctrine (bases gained don't count as winning while it
loses men that heavily). After a change the
next review only lets the new doctrine settle in, so it is judged on its own 10 minutes (changes
at most every 20 minutes). If its intel has
confirmed the enemy's doctrine and it isn't winning, it picks the counter instead: Fortress against
Blitz, Firepower against Fortress, Raider against Firepower, Blitz against Raider. Its own side is told
(feed, notice, report). The enemy's picture goes out of date without them knowing: they keep seeing
the old doctrine until 10 points of new intel show the change ("the enemy commander has changed
tactics"), then work the new one out from scratch. `-acrDoctrineAdapt 0` (setup row AI doctrines adapt)
keeps every commander on its first doctrine.

#### Factions and mods

The external ZIP/development launcher menu offers four side pairings: the game's US vs USSR, RHS US
vs RHS AFRF, UA vs RHS AFRF, or RHS US vs UA. The launcher selects the RHS/UA addon IDs but does not
download them; its current search path for optional faction mods is `E:\ArmaReforgerMods\addons`,
which may need adapting on another PC. The UA mod replaces the game's US side for that session. ACE
All in One is a separate optional choice. Everyone joining must use the same side pairing and ACE
choice as the host. With no extra faction mods selected, the game uses its usual US vs USSR sides.

**Any faction your world lists.** Besides those pairs, the in-game setup screen (Blue side / Red
side) and `-acrFactions WEST,EAST` can pick any two eligible factions in your world's faction
manager whose Game Master catalogs have soldiers and groups. The in-game screen only selects among
already-loaded mods; it does not load or download faction mods. The world's US-side bases, units and
spawns go to the first, USSR-side to the second. To offer a faction mod, add its faction config to
the `Factions` list of the world's `SCR_FactionManager` (an entry whose mod isn't loaded is simply
absent in that session):

```
  SCR_Faction "{<a new GUID>}" : "{<the mod's faction config GUID>}Configs/Factions/<Faction>.conf" {
  }
```

A faction without its own `ACR_FactionSettings` entity copies the settings of the side it replaces
(RP, AI commander, caps, prices, supports) and builds its buy list and garrisons from its catalog; its
command unit is the catalog's officer, else its squad leader. This is not hand-fitting: an untuned faction
can look mixed (several uniforms), and anything it lacks falls back to vanilla units. Only RHS and Ukraine
Armed Forces RHS are hand-fitted. Request faction-mod support in [Discord #suggestions](https://discord.gg/WK8xGSztB)
or on [GitHub](https://github.com/propst-design/ArmaCommanderReforger/issues).

You can use `m_aPrefabMappings` to replace a unit with one from another mod. Each row is an
`ACR_PrefabMapping` with two resource pickers:

| Setting | Default | What it does |
|---|---|---|
| `m_sFrom` | empty | Exact original prefab to replace |
| `m_sTo` | empty | Replacement prefab, resolved when this faction uses the mapping; load its supplying mod before selecting the faction |

Choose both from the prefab browser. AC: Actual also tries to match starting units by their catalog
name or role. If it cannot find a replacement, it uses the original unit and writes a `[ACR] FACTION
fallback` line in the server log.

**Automatic buy list.** AC: Actual adds each enabled infantry group and vehicle from the faction's Game
Master catalog. It leaves out guards, fixed weapons, and planes. Prices: infantry 3 RP per soldier (at least 6); vehicles
12 (car), 15 (truck), 45 (APC) or 60 (helicopter), +10 armed, +15 armoured, +6 crew. Cheaper units have more
stock. The AI commander never buys aircraft (it can't fly them) and buys unarmed trucks only to ferry squads.

**Prices (`m_ePricing`).** `STEPPED` (default): infantry, cars and trucks can be bought without limit, but
each purchase makes that unit cost more for that side: +15% of the base price per purchase at normal
threat, +25% at low, +8% at high. Rare units (aircraft, APCs and armour, mortar teams, support helicopters)
keep finite stock at a fixed price, scaled by threat: High quadruples rare `m_iStock`; Low halves infantry
stock and cuts vehicle stock to a third (rounded, minimum one). A BTR with base stock 1 offers 1 on
Low/Normal and 4 on High. `FIXED_STOCK`: the older system, every unit has
its `m_iStock` per match (scaled by threat level) at a fixed price. The host can override it with
`-acrPricing`.

#### Requisition entry (`m_aRequisitions`)

Click **+** to add an item. Choose its infantry group and vehicle (if any) from the prefab browser.

| Setting | Default | What it does |
|---|---|---|
| `m_sName` | | Name in the buy list and on the map; numbered per purchase |
| `m_sGroup` | | Infantry group to spawn |
| `m_sVehicle` | | Vehicle to spawn (optional) |
| `m_bCrewed` | on | Spawn the vehicle with its driver and gunners |
| `m_sPassengers` | | Group that rides in the vehicle (optional) |
| `m_iCost` | 20 | Cost in RP |
| `m_iStock` | 2 | How many can be bought per match (fixed-stock pricing, and rare units under stepped pricing) |

For vehicles in the faction's Game Master catalog, AC: Actual fills in whether they are aircraft,
transports, armed, or rare. A vehicle outside that catalog is treated as an armed ground vehicle.

#### Support entry (`m_aSupports`)

Each entry is one action in the commander's support menu, such as a mortar strike or helicopter run.

| Setting | Default | What it does |
|---|---|---|
| `m_eType` | | Kind of support (table below) |
| `m_bEnabled` | on | Offer this call-in; uncheck to omit it from this faction's support menu |
| `m_sName` | empty = default | Name in the command panel |
| `m_iCost` | -1 = default | Cost in RP |
| `m_iCooldown` | -1 = default | Seconds before it can be called again |
| `m_iAICost` | -1 = half the cost | `AIR_LIFT` only: RP when the AI commander calls it |
| `m_eBombRunDelivery` | `VIRTUAL` | Bomb-run delivery: existing virtual blasts, a flyover prefab plus virtual blasts, or an editable call-in prefab |
| `m_sBombRunPrefab` | empty | Aircraft/flyover prefab (`FLYOVER`) or call-in spawner prefab (`EDITABLE_CALL_IN`) |
| `m_fBombRunAltitude` | 300 m | Flyover height above the target, and never under 150 m over a hill on the way; clamped to 30-1,000 m |
| `m_fBombRunSpeed` | 180 m/s | Flyover speed; sets bomb spacing in time in `FLYOVER` mode; clamped to 20-400 m/s |
| `m_iBombRunBombs` | 4 | Virtual bombs in `VIRTUAL` or `FLYOVER` mode; clamped to 1-12 |
| `m_fBombRunSpacing` | 28 m | Distance between impact points in `FLYOVER` mode; clamped to 1-500 m |
| `m_fBombRunScatter` | 5 m | Random lateral impact scatter in metres; clamped to 0-100 m |

| Type | Default name | RP | Cooldown (s) |
|---|---|---|---|
| `MORTAR_HE_SMALL` | Mortars: HE (small) | 10 | 90 |
| `MORTAR_HE_LARGE` | Mortars: HE (large) | 25 | 180 |
| `MORTAR_CREEPING` | Mortars: creeping barrage | 25 | 180 |
| `MORTAR_SMOKE` | Mortars: smoke screen | 6 | 60 |
| `ILLUMINATION` | Illumination flares | 4 | 45 |
| `ROCKET_STRIKE` | Air: rocket strike | 25 | 180 |
| `BOMB_RUN` | Air: bomb run | 35 | 240 |
| `HELI_GUN_RUN` | Helicopter: rocket run | 30 | 180 |
| `HELI_LOITER` | Helicopter: gunship (90 s) | 45 | 300 |
| `HELI_DOOR_GUNS` | Helicopter: door gunners (120 s) | 20 | 150 |
| `ARTILLERY_BATTERY` | Artillery: howitzer battery | 60 | 300 |
| `AIR_LIFT` | Helicopter: air lift | 40 (AI commander 20) | 240 |

Set the bomb-run style in each side's support row. `VIRTUAL` uses AC: Actual's existing explosions and
does not show a plane. `FLYOVER` sends the chosen aircraft or model over the target, then uses AC: Actual's
explosions. `EDITABLE_CALL_IN` starts the chosen prefab at the target and lets that prefab handle its own
attack. This can work with mod call-ins, such as RHS CAS, without AC: Actual needing RHS-specific code.
Use `FLYOVER` for a model that only needs to fly across the map. Use `EDITABLE_CALL_IN` for a prefab that
already handles its own attack. Leave the prefab empty to keep the standard virtual bomb run.

#### Requisition support unit (`m_aSupportUnits`)

Click **+** to choose a support unit to offer for purchase. Pick its assets from the prefab browser.
Roles without a row keep their current settings. Use **Enabled** to show or hide the unit in the buy list,
and set its price and how many times it can be bought in a match. These are separate from the price and
wait time for calling in support above.

| Setting | What it does |
|---|---|
| `m_eRole` | `MORTAR_TEAM`, `OBSERVER_TEAM`, `GUNSHIP`, or `DOOR_GUN_HELI` |
| `m_bEnabled` | Offer this support unit for requisition; on by default |
| `m_sName` | Buy-list name (empty = current default) |
| `m_iCost` | Purchase cost in RP (-1 = current default) |
| `m_iStock` | Base purchases per match (-1 = current default; finite stock in either pricing mode is scaled by threat) |
| `m_sGroupPrefab` | Observer team's base group (observer role) |
| `m_sPrimaryPrefab` | Spotter character (observer) |
| `m_sSecondaryPrefab` | Radio operator character (observer) |
| `m_sVehiclePrefab` | Support helicopter airframe (gunship or door-gunner role) |
| `m_sCrewPrefab` | Crew character used for pilots and gunners, including replacement crew after servicing |

Observer characters use the gear saved on their character prefab. For a helicopter, the chosen crew
replaces the airframe's pilots and gunners when it is first parked at a base; it is also used if crew
are replaced during servicing. Empty airframe and crew fields keep the current faction choices. The older `m_sSupportHelicopter` and
`m_sSupportTransportHelicopter` values remain as airframe fallbacks when that role has no
`m_sVehiclePrefab`. Mortar-team prefab choices continue to use `m_aPrefabMappings`; its support-unit
row controls whether the team is offered and its name, cost and stock.

Mortar calls and `ILLUMINATION` need a **mortar team**. AC: Actual adds one to each side's buy list
(30 RP, up to 2 per match; M252 for western sides, 2B14 for other sides). The team sets up on its own
after stopping on clear, level ground, and packs the mortar before moving. It can only answer calls
within range (about 2.9 km for the M252 or 2.3 km for the 2B14, and at least 100 m away). If no team is
ready or the target is out of range, the call is declined without spending RP. If the mortar is destroyed,
use Reinforce (10 RP) to replace it.

Each buy list also has a "Forward observer team" (12 RP, up to 2 per match: a spotter and radio operator).
Turn on **Fire at will** in the command panel to let that team call a small mortar strike on enemies it
spots. The target must be at least 150 m away, with no friendly within 120 m. The call uses the normal
price and cooldown and can happen once every 45 seconds per side. Observer teams start with this option
on. AI commanders' observers and mortar teams always use it. Mortar teams also fire two free, scattered
rounds every 30 seconds at spotted enemies they can reach.

`AIR_LIFT` flies an infantry squad with no player-controlled member. It must be on foot and not a
vehicle crew. Capacity depends on the helicopter; one or two fly, and troops without seats stay
behind. Select the squad, pick the air lift, and right-click where it should land (at least 400 m
away). One transport helicopter (two if the squad needs more seats) comes from off the map, lands on
clear, level ground within 250 m of the squad, waits up to two minutes for it to board, and sets it
down on clear ground within 200 m of the clicked point; the squad then holds there and the
helicopters leave. No helipad is needed. The call is declined without a charge if there is no clear
landing spot at either end. While a squad is on an air lift it takes no orders until it has landed.
The AI commander pays half (`m_iAICost`), since it cannot fly bought helicopters. Support lists made
before the air lift existed get it added; add an `AIR_LIFT` row with **Enabled** off to leave it
out.

`ARTILLERY_BATTERY` needs no team and can hit anywhere on the map. It arrives 75-120 seconds after the
call: three groups of six heavy rounds, 10 seconds apart, landing within 120 m of the target. You can
hear each group coming in.

Support helicopters use **helipads** (see `ACR_Helipad` below). If the map has a helipad, each side can
buy up to four gunships (80 RP each) and four door-gunner helicopters (50 RP each). A helicopter waits
at a free pad with its crew. After a call, it starts up (about 20 seconds), flies its mission, returns to
the pad, and is repaired and rearmed (60 seconds). Buy another one if it is destroyed. If no ready
helicopter is parked at a pad at a base the side still owns, the call is declined without a charge. The
support list says whether one is ready, being serviced, or needs to be bought. Gunships handle rocket
runs and gunship calls; door-gunner helicopters handle door-gunner calls. The AI commander keeps one
gunship if a pad is free. Without a helipad, helicopters arrive from off the map and do not need to be
bought.

### ACR_Helipad

A landing pad for support helicopters. It belongs to the nearest base, or the base named in `m_sBase`.
The side that owns that base owns the pad. Leave flat, open ground around it: the Mi-8 needs about 11 m
of clear space on every side.

| Setting | Default | What it does |
|---|---|---|
| `m_sBase` | empty | Name of the base this pad belongs to, as shown on the map. Empty = the nearest base |
| `m_bModel` | on | Places a helipad model (US or USSR style, from the base's starting owner). Off when the world already has a helipad at this spot |

### ACR_Base

A base is a point the sides can capture. Its position is the centre of the capture circle.

| Setting | Default | What it does |
|---|---|---|
| `m_sName` | `Base N` | Name on the map |
| `m_sOwner` | empty | Starting owner faction key; empty = neutral |
| `m_iGarrison` | 10 | AI soldiers guarding it. They exist only while enemies are within 400 m, and are used up as they die. They start at the windows of the base's buildings within 80 m of its centre (the rest hold round the centre) |
| `m_iValue` | 1 | RP it adds to its owner's income (shown on the map as `Name [value]`) |
| `m_bIncome` | on | Produces income. Off = purely tactical point |
| `m_bHidden` | off | Hidden from other sides until one of their units enters it |
| `m_bAllowSpawn` | on | Allows new-unit deployment, additionally filtered by logistics policy; also retains its existing player-respawn use |
| `m_bAllowReinforce` | on | Allows RP infantry replacements, additionally filtered by reinforcement policy |
| `m_eLogisticsRole` | `AUTHORED` (0) | `AUTHORED`, `HQ`, `MILITARY`, `SETTLEMENT`, `OBJECTIVE` (0–4); generated layouts assign these automatically. Authored preserves existing mission flags; select another role to opt into site restrictions |
| `m_fRadius` | 50 | Capture radius in metres |

**Income:** every 60 seconds, each side gets 5 RP plus the value of each income base it owns.
**Capture:** soldiers inside the circle capture it for their side. More soldiers capture it faster.

### ACR_StartingUnit

A unit AC: Actual creates at this object's position 2 seconds after the match starts. Its vehicle appears
6 m away. The unit's prefab decides which side it belongs to.

| Setting | Default | What it does |
|---|---|---|
| `m_sName` | | Name on the map, used as written (e.g. `1st Squad`) |
| `m_sGroup` | | Infantry group |
| `m_sVehicle` | | Vehicle (optional) |
| `m_bCrewed` | on | Vehicle comes with driver and gunners |
| `m_sPassengers` | | Group riding in the vehicle (optional) |

AC: Actual also finds other AI groups already on the map, or added later by Game Master, and makes them
commandable for their side. Use `ACR_StartingUnit` when you also want to give a group a name and vehicle.

### Generated layouts

AC: Actual can lay out a match from a terrain's named places when it has enough usable sites and dry
land. Choose "Generated" under Battlefield on the setup screen (or `-acrLayout generated`), with the
whole map, half of it or a flashpoint (`-acrSize`). A world without any `ACR_Base` always plays a
generated layout: a scenario for a new map then needs only the game mode, the managers, the faction
settings and one player spawn point per side (see `Worlds/ACR_Kolguyev` for a complete example).

Site kinds are based on the hand-built Everon layout: settlements have nominal value 1 and
capture radii 90/75/60/45 m; military sites value 3 (60 m); industry value 2 (50 m); airports
value 3 (80 m). Hidden landmarks are tactical objectives (40 m), with no passive income.
Generated layouts now distinguish income-producing sites from tactical capture/deployment points.
The four nearest ordinary objectives per side and non-landmark sites within 2 km of either HQ
keep their full original income. Further economic sites are selected for strategic value and
spread (normally 800 m from another income site), aiming for about half the ordinary objectives
plus the two HQs. Protected opening sites can exceed that target; compact flashpoints protect
two opening objectives per side. Other sites remain useful for control and deployment but pay
no RP. Basic income, starting RP, capture rules and reinforcement prices are unchanged. This
selection applies to generated layouts; hand-authored bases still use their own `m_bIncome` flags.
Seeds select later income sites as a group, retaining the original number of earning sites
and total nominal RP for that layout. Individual site values never change: for example, two
2-RP sites can replace a 1-RP and a 3-RP site when all spacing constraints hold. Small complete
sets of up to 128 valid arrangements use an even seed cycle; consecutive seeds visit each
arrangement before repeating. Larger sets use seeded group selection. Each phase
examines at most 4,096 candidate branches (8,192 combined); failure keeps the original income
flags intact. HQs, the four nearest ordinary objectives per side and every ordinary site within 2 km of either HQ are protected. Remaining
non-income objectives are selected with seeded flank and settlement/industry/military preferences;
about 20-40% of optional tactical sites may be omitted, retaining at least one when available.
Hidden landmarks stay tactical. Omitting tactical sites does not lower the original income target.
Manual moves/deletions still apply afterwards, and moved HQs relocate opening-income protection.

Flashpoints draw a seeded 1.25-1.8 km neighbourhood around a town/village (falling back to 1.8 km
where necessary), then select 5-7 accessible sites with varied composition. Up to eight local
regions are tried if HQ/access filtering leaves fewer than five sites. HQs remain the two farthest
ordinary sites in that candidate region. Their two nearest ordinary opening objectives per side
are protected during selection; flashpoint income keeps its existing rules and 300 m spacing.

Ordinary generated objectives are normally at least 350 m apart; two nearby opening objectives
per side may be 250 m apart to preserve the opening. Compact flashpoints keep their existing
circle-spacing rule. Bases remain on their map places, moved onto dry ground when necessary;
capture circles can shrink to 35 m. HQ candidates remain widely separated (at least 80% of the
greatest eligible military/airport separation). Half-map seeds choose rotated, shifted bands or
compact regions containing roughly 45-55% of the ordinary sites before access/spacing filtering.
HQ selection checks up to 12 pairs: the three farthest with the fewest rear objectives plus up
to nine seeded alternatives with at most two extra rear objectives. It chooses among pairs
within one accessible objective of the best checked pair; every retained objective still passes
the same access and spacing filters. Hidden landmarks do not define the region boundary.
The generator removes objectives directly behind HQ approaches and checks dry-land access from
both sides while excluding the opposing HQ perimeter (350 m, or 100 m in a compact flashpoint).
This cached coarse terrain check catches HQ/peninsula bottlenecks; it does not guarantee roads,
navmesh paths, bridge use or good combat terrain. HQs pay 2 RP, with six guards and a 70 m circle
where there is room. Other generated bases start neutral with no garrison. Each side starts with the world's own
`ACR_StartingUnit` objects, or two rifle squads, a weapons team, a truck and a car, and its player
spawn point moves to its HQ. Hand-built helipads are not used: both sides get the off-map support
helicopters, and helicopters need a live helipad for purchase (players can build one with a
construction truck). The AI commander builds none, so it uses no bought helicopters there (it uses
the off-map air lift support call instead for long moves and detours). The setup screen's "Layout
seed" row rolls a new seed; the same seed, size and terrain data reproduce a plan within the same
version, but manual moves and later mod/terrain changes can alter it; the log lists the layout
(`[ACR] GEN BASE`). "Show and edit the layout on the map" opens the map with the planned bases:
click a base, then click where it should go. A spot is refused (with the reason) when it is off the
map, in water, too close to another base's capture circle, or would bring the two HQs within 1000 m
(300 m in a flashpoint). These are placement checks, not a guarantee of road access or good combat
terrain. Moving a CP updates its name to a nearby named place or hill, preserving its type/value. Income
selection follows the edited positions, including protection around moved HQs. Automatic spacing
and rear/access filtering run before manual edits, so they do not erase intentional host moves.
Select a CP and use "Delete selected CP" to remove it before Start. HQs cannot be deleted, and
at least one ordinary CP must remain. Preview markers show HQ/CP value, type and hidden status;
the selected label is bracketed. Hidden sites remain concealed during play until discovered.
Use "Return to match setup" to leave the map. "Undo layout edits" restores moves and deletions.
Edits belong to one seed and size: changing either hides them; returning restores them unless
you have edited a different layout or used Undo. Edits apply to this match only; there is no saved-layout
export.

### ACR_Skirmish

This optional object defines a smaller match area on a large map: half the map, or a small fight
around about five bases. Add one object for each layout; its position does not matter. The host can
choose the layout by name (`-acrLayout`) or choose a size (`-acrSize`) and get a random layout of
that size. If no layout is chosen, the whole map is used. Everon has north and south halves, plus
small areas around Saint-Philippe, Montignac, and Levie. Put a flashpoint's HQs on bases inside it,
at opposite ends, so commanders start in the fight rather than at a far-off base. Everon's halves
have two helipads at each HQ; its flashpoints have none yet, so both sides there get the off-map
support helicopters. Add one or two `ACR_Helipad` objects for each HQ you list if you want both
sides to park support helicopters.

| Setting | Default | What it does |
|---|---|---|
| `m_sName` | | Layout name (the setup menu lists it; `-acrLayout` matches it ignoring spaces, `_` and `-`) |
| `m_eSize` | `FLASHPOINT` | `HALF` or `FLASHPOINT` |
| `m_sBases` | | Bases in play, by their `m_sName`, separated by commas. The HQs are added automatically |
| `m_sHQs` | | One HQ per side as `faction=base`, separated by semicolons, e.g. `US=Airbase; USSR=Power Plant` |

When the match starts with a layout, every base not in it is removed (off the map and out of play), the
HQs become their sides' owned bases (with at least the garrison of the side's HQ as placed) and other
bases that started owned turn neutral. Each side's starting units and player spawn point move to its new
HQ when it changed (a starting unit belongs to the side whose placed HQ is nearest; units appear at clear
spots in the new HQ's circle). Helipads of removed bases are out of play; a side whose layout leaves it no
helipad gets the off-map support helicopters instead of buying them for pads.

## Mission header

Create an **ACR_MissionHeader** in Workbench and choose your saved world. This supplies the scenario
metadata. The game's Scenarios/Host lists discover published Workshop scenarios; an unpacked addon
loaded with `-addonsDir` does not acquire a menu entry just by having a header. Workbench fills in
most of the details for you; this is what the saved file looks like:

```
ACR_MissionHeader {
 World "{<your world's GUID>}Worlds/<YourWorld>.ent"
 m_sName "AC: Actual - <Map>"
 m_sAuthor "<you>"
 m_sDescription "<one line>"
 m_sGameMode "Arma Commander Actual"
 m_iPlayerCount 16
 m_iAcrSetup 1
 m_sAcrSize "half"
}
```

Use the class `ACR_MissionHeader` (a plain `SCR_MissionHeader` still works, without the fields
below). Its extra fields are the scenario's default [host options](#host-options), so you can ship
several scenarios on one world (for example a full map and a flashpoint) that differ only in their
header. Leave a text field blank or a number at `-1` to keep the world's own setting. A host's
launch option beats the header; a dedicated server's config can also override header fields by name
under `gameProperties.missionHeader`. AC: Actual's custom fields have been tested through mission
headers, but their JSON override path still needs validation on an installed Workshop server. The
in-game setup screen, when enabled, overrides these defaults at Start.

| Field | Same as |
|---|---|
| `m_sAcrLayout` | `-acrLayout` |
| `m_sAcrSize` | `-acrSize` |
| `m_sAcrPockets` | `-acrPockets few` (default), `many`, `off` |
| `m_sAcrFactions` | `-acrFactions` |
| `m_sAcrPlayers` | `-acrPlayers` |
| `m_sAcrThreat` | `-acrThreat` |
| `m_sAcrPricing` | `-acrPricing` |
| `m_sAcrAI` | `-acrAI` |
| `m_sAcrDoctrineWest` | `-acrDoctrineWest` |
| `m_sAcrDoctrineEast` | `-acrDoctrineEast` |
| `m_sAcrDoctrineAdapt` | `-acrDoctrineAdapt` |
| `m_iAcrUnitCap` | `-acrUnitCap` |
| `m_iAcrSeed` | `-acrSeed` |
| `m_iAcrSoldierCap` | `-acrSoldierCap` |
| `m_iAcrStartRP` | `-acrStartRP` |
| `m_fAcrIncome` | `-acrIncome` |
| `m_iAcrSkill` | `-acrSkill` |
| `m_fAcrReaction` | `-acrReaction` |
| `m_iAcrSharedCommand` | `-acrSharedCommand` (0 or 1) |
| `m_iAcrRespawnSeconds` | `-acrRespawnSeconds` |
| `m_iAcrGameMasterAll` | `-acrGameMasterAll` (0 or 1) |
| `m_iAcrMarksmen` | `-acrMarksmen` (0 or 1) |
| `m_iAcrSetup` | `-acrSetup` (0 or 1) |
| `m_sAcrDeployment` | `-acrDeployment military` (default), `all`, `hq`; new formations at military/HQ/FOB, all spawn-enabled points, or HQ/FOB only |
| `m_sAcrReinforcement` | `-acrReinforcement settlements` (default), `all`, `deployment`; infantry replacements at settlements plus deployment sites, all reinforcement-enabled points, or deployment sites only |
| `m_sAcrFobServices` | `-acrFobServices supplies` (default) or `free`; automatic static service buildings draw FOB supplies or retain unlimited free service anywhere |
| `m_iAcrFobRelay` | `-acrFobRelay 1` (default) or `0`; require a completed friendly radio relay within a built FOB for new-unit deployment and RP reinforcement; separate from player respawn |
| `m_iAcrDepotCapacity` | `-acrDepotCapacity 2000` default, 0–10000; one depot's extra FOB storage; zero disables the bonus |
| `m_iAcrServiceSupplyCost` | `-acrServiceSupplyCost 10` default, 0–100; supply cost per service load (turret magazines half, disposable launchers double) |
| `m_iAcrFobRPServiceCost` | `-acrFobRPServiceCost 3` default, 0–100; RP per soldier actually serviced instantly at a FOB in supplied mode; zero makes this fallback free |
| `m_iAcrMines` | `-acrMines 1` (default) or `0`; the commander's minefield orders (see [Minefields](#minefields)) |
| `m_iAcrHoldGuns` | `-acrHoldGuns 1` (default) or `0`; gunners stay on the static guns of a spot their squad holds |
| `m_iAcrMinesRealistic` | `-acrMinesRealistic 0` (default) or `1`; with 1 the owning side's AI sets off its own mines too |
| `m_iAcrMinesAI` | `-acrMinesAI 0` (default) or `1`; AI commanders lay minefields on their approaches |
| `m_iAcrFortify` | `-acrFortify 1` (default) or `0`; the commander's Fortify order (see [Fortify position](#fortify-position)) |
| `m_iAcrFortifyAI` | `-acrFortifyAI 0` (default) or `1`; AI commanders dig in and upgrade positions on their approaches |
| `m_iAcrFortifyDefenders1` | `-acrFortifyDefenders1 1` default, 0–12; defenders in reserve at a Fortify tier I position (0: none) |
| `m_iAcrFortifyDefenders2` | `-acrFortifyDefenders2 2` default, 0–12; defenders in reserve at a Fortify tier II position (0: none) |
| `m_iAcrFortifyDefenders3` | `-acrFortifyDefenders3 4` default, 0–12; defenders in reserve at a tier III position |
| `m_iAcrRoadblockDefenders` | `-acrRoadblockDefenders 0` default (none), 0–8; defenders in reserve at a checkpoint |
| `m_iAcrAIBuildShare` | `-acrAIBuildShare 0` (default, off) to `30`; percent of each AI commander's income set aside for roadblocks, minefields and Fortify positions |

The header describes the scenario; it does not publish, package or copy the world. Workbench creates
the `.meta` files beside the world and header. Keep them there and do not edit them. When you choose
the world in the header, Workbench fills in its resource ID and path for you. The world's ID is
different from the addon's ID shown in `addon.gproj`.

## Save, export and run

There is no separate **Export Mission** button. Workbench saves the map in your addon project, and you
can test it from there. Think of the addon as the folder you edit and eventually share.

Keep these matching resources together in the addon that owns the mission:

```text
Worlds/ACR_MyScenario.ent
Worlds/ACR_MyScenario.ent.meta
Worlds/ACR_MyScenario_Layers/default.layer
Missions/ACR_MyScenario.conf
Missions/ACR_MyScenario.conf.meta
```

Workbench makes the `.meta` files and resource IDs for you. If you copied a world, Workbench gives the
copy a new ID. Keep the world, its layer, its `.meta` file, and the mission header together in your addon.

### Run a map through the AC: Actual launcher

The included Play and Host launchers can start maps saved inside the AC: Actual project. They look for
world files named `Worlds/ACR_*.ent`, with each map's layer in
`Worlds/ACR_<Name>_Layers/default.layer`.

1. Save your map in the AC: Actual project as `Worlds/ACR_<Name>.ent`. Keep its layer at
   `Worlds/ACR_<Name>_Layers/default.layer`. The terrain can stay in a separate addon.
2. Put its header in `Missions/ACR_<Name>.conf`. Workbench fills in which world it opens.
3. To play alone, run `Start-Test-Range.ps1 -World "Worlds/ACR_<Name>.ent"`. The setup menu opens.
   Select your map in option 1 (the menu restores remembered settings), choose the other settings
   and press Enter. `Play-ACA.cmd` opens the same menu,
   using the remembered selection (Arland on first use); you can switch to your map there.
4. To host a multiplayer game, run `Host-ACA.cmd` from the AC: Actual development project and select
   your map in the host menu. The launcher starts the server and then joins it on your PC. Your friend
   runs `Join-ACA.cmd` from a matching copy of that project and enters the address shown in your host
   window. You both need the same map files and the same faction-mod choices.

These launchers use the files in the AC: Actual project directly, so there is no Workshop upload or
package build for a local test. They start the selected world file directly; the mission header is
for the game's scenario list. To let a friend play, give them a matching copy of the development
project with your map included. Use that project copy, not the release zip: the release launcher
updates itself and can replace your custom map with the released files. The helper loads AC: Actual,
its selected faction/ACE mods, and a locally detected Workshop copy of All-in-One Command Menu. A
custom terrain or separate scenario addon is not added automatically.

### Run a map kept in your own addon

Keeping your map in your own addon is best for protecting your work when AC: Actual updates. Workbench
can test it there. You can also publish your addon so it appears in the game's **Scenarios** list. In
Workbench, publish the addon with AC: Actual listed as a dependency, then enable both addons in the game.
The [scenario setup guide](https://community.bistudio.com/wiki/Arma_Reforger:Capture_%26_Hold_Setup)
shows how the header makes a scenario appear in that list.

Use the visibility and testing options offered by Workbench's publisher for your intended players;
check the [publishing
guide](https://community.bistudio.com/wiki/Arma_Reforger:Mod_Publishing_Process). Do not assume an
uploader-only item is available to friends.

The supplied launcher does not load a separate mission-maker addon or list its worlds. For the
normal **Multiplayer > Host** flow, both players must be able to install and enable the scenario
addon, AC: Actual and any terrain/faction dependencies through the game. An unpacked local AC:
Actual copy alone is not discoverable in those scenario lists. For a local launcher test, use the
matching development-project route above, with any additional addon dependencies added to the launch
arguments.

## Host options

Options resolve from mission-header defaults, then `-acr...` launch arguments, then choices on the
in-game setup screen. Workshop scenarios and ZIP/development Play and Host launchers enable that
screen. Launchers choose only the map and optional mods; old saved match settings are ignored.
Play passes `-acrSetup 1`; Host also enables the existing `-acrFirstPlayerHost 1` fallback because
it runs a separate local server: the host must connect first, and access never transfers after
disconnect. A world launched directly has no mission header. The table retains launch options for
mission makers, automated tests and server operators; players choose match settings in game.

| Option | What it does |
|---|---|
| `-acrPlayers US` / `USSR` / `any` | Put every player on one side (the deploy screen skips the side choice). The mode places a spawn point for that side at one of its bases if the world has none |
| `-acrFactions vanilla` / `ru_us` / `ru_ua` / `us_ua` / `<west>,<east>` | Select the playable faction pair: a named pair or any two faction keys your world's faction manager lists, e.g. `US,FIA`. The world's US-side bases and units go to the first, USSR-side to the second. A faction is playable when the world lists it and its catalog has soldiers and groups; a side without an `ACR_FactionSettings` entity copies the replaced side's settings (RP, AI commander, caps, supports) and builds its buy list from its catalog |
| `-acrMarksmen 0` | Disable the automatic DMR carrier in full UA/Russian rifle squads (enabled by default; keeps squad size and support weapons) |
| `-acrAI both` / `none` / `<faction>` | Which sides an AI commander leads (overrides `m_bAICommander`) |
| `-acrDoctrineWest <doctrine>` / `-acrDoctrineEast <doctrine>` | The blue / red side AI commander's doctrine: `random`, `balanced`, `blitz`, `firepower`, `fortress` or `raider` (overrides `m_eAIDoctrine`; default random each match). The log names each side's pick (`AI commander <faction> doctrine:`) |
| `-acrDoctrineAdapt 1` / `0` | AI commanders change doctrine when theirs isn't working (default on; see AI commander doctrines). Each review logs `AI commander <faction> doctrine review:` |
| `-acrUnitCap N` | AI commander squad cap for every side (overrides `m_iAIUnitCap`) |
| `-acrThreat low` / `normal` / `high` | Threat level. Low: prices climb +25% per purchase (stepped pricing; finite stock in either mode: half the infantry stock, a third of the vehicles, at least one each), 0.75x soldier limit, 0.6x AI squad cap, one AI attack at a time, AI buys few vehicles. High: prices climb +8% (finite stock in either mode: rare units get quadruple base stock, other units double stock), 1.5x soldier limit and AI squad cap, one more AI attack at a time. Normal: +15%, nothing else changes |
| `-acrSize full` / `half` / `flashpoint` | Skirmish size: the whole map (default) or a random `ACR_Skirmish` layout of that size. Hand-built worlds without a matching layout play in full; generated layouts support all three sizes |
| `-acrPockets few` / `many` / `off` | FIA pockets of resistance (default few with the setup screen, off without it): on whole-map and half-map battles, neutral bases away from both starting bases start held by an FIA garrison (villages 4-6 men, industry 6-8, military sites 8-10 plus a 2-man patrol). Few = 7 on a whole map, 3 on a half; many = 12 / 6; never on flashpoints, or when FIA plays a main side. Other sides see a pocket as unclaimed until one of their units comes within 400 m. Whoever clears one captures the base |
| `-acrLayout <name>` | Play this `ACR_Skirmish` layout (spaces may be written as `_`); overrides `-acrSize`. `-acrLayout generated` builds a layout from the map instead (see Generated layouts) |
| `-acrSeed N` | Seed of a generated layout: the same seed, size and terrain data give the same plan within a version (before manual moves). Unset or 0 = a new random layout each match |
| `-acrPricing stepped` / `stock` | Overrides every side's `m_ePricing` (stepped prices or fixed stock) |
| `-acrStartRP N` | Positive starting RP for every side (overrides `m_iStartingRP`; 0 does not override) |
| `-acrIncome X` | Income multiplier (default 1) |
| `-acrSkill N` | AI skill (overrides `m_iAISkill`) |
| `-acrReaction X` | AI spotting speed (overrides `m_fAIPerception`) |
| `-acrSoldierCap N` | Soft living-AI threshold per side before purchases/reinforcements stop (default 100 before threat scaling). A purchase begun below it can take the side over. The active-AI limit is raised to at least 2 x the resolved cap + 64 to avoid the vanilla 128-agent ceiling |
| `-acrSharedCommand 0` / `1` | Overrides `m_bSharedCommand` |
| `-acrRespawnSeconds N` | Overrides `m_iRespawnSeconds` |
| `-acrGameMasterAll` | Game Master for every player, not just host and admins |
| `-acrMines 0` | Turn off the commander's minefield orders (on by default; see [Minefields](#minefields)) |
| `-acrHoldGuns 0` | Vanilla gunners: by default a squad holding a spot (Defend, a checkpoint, a Fortify position) keeps its men on the static guns there, unless an enemy seen in the last 10 s is within 60 m; an empty held gun is manned again after 8 s. Vanilla gets a gunner out whenever anything he knows of is outside the gun's arc. Setup row **Gunners stay on held guns** |
| `-acrSquadTactics 1` | AI squads use squad tactics inside an objective (off by default): an AI squad assaulting a base uses Better Battle Buddies' squad tactics (assault, flank, hold, search) and fire and manoeuvre once it is inside the base's ring, never leaving it, and squads defending a base on a Defend order may bound to cover or push back an enemy inside the base, never chasing out of it. Garrisons in buildings, squads staging or riding, and squads under a player's command are never affected |
| `-acrMinesRealistic 1` | Mines go off for everyone. By default the owning side's AI never sets off its own mines; its players do |
| `-acrFortify 0` | Turn off the commander's Fortify order (on by default; see [Fortify position](#fortify-position)) |
| `-acrFortifyAI 1` | AI commanders fortify: every 60 s, with RP to spare, one order at a time, they upgrade their position nearest the frontline base or (fewer than two positions) dig a free infantry squad in 80-160 m out towards the nearest enemy base (off by default) |
| `-acrFortifyDefenders1 N` / `-acrFortifyDefenders2 N` / `-acrFortifyDefenders3 N` | Defenders held in reserve at a Fortify tier I / II / III position (1 / 2 / 4 by default, 0 none, at most 12; see [Fortify position](#fortify-position)) |
| `-acrRoadblockDefenders N` | Defenders held in reserve at every checkpoint (small, medium, large; not a plain barricade), as at a Fortify position: they deploy when an enemy comes within 400 m and are used up as they die (0, none, by default; at most 8) |
| `-acrAIBuildShare N` | AI commanders set N% (at most 30) of the RP they gain aside as an engineering budget, up to 100 RP: unit purchases leave it alone, so their roadblocks, minefields (`-acrMinesAI`) and Fortify positions (`-acrFortifyAI`) get built even when they would otherwise spend everything on units; those orders are paid out of it, and it gets at least 2 RP every strategy tick (15 s) on top of the share (twice both for a Fortress). A side with more than 600 RP banked digs in with the surplus: its share rises with it, to twice the share at the 1000 RP cap (at most 50%), and the budget is kept at least at that share of what is over 600 (still at most 100 RP), since a side at the cap gains nothing to take a share of. Shown in the AI status log line. Off by default; the setup screen's **AI build budget** row sets it (off, 10, 15 or 25%) |
| `-acrVariety 0` | AI choices always take the single best option. By default (`1`) an AI choice made from scored options (where to site a Fortify position, say) is picked at random among those within 12% of the best, the best most likely, seeded from the match's seed so a replayed seed makes the same choices |
| `-acrMinesAI 1` | AI commanders lay minefields: every 30 s, with RP to spare and fewer than two fields down, a free infantry squad (sappers first) mines the approach 150-270 m out from its frontline base towards the nearest enemy base (off by default) |
| `-acrSetup 1` / `0` | Setup screen before the match. With 1, nothing spawns until the host (or a logged-in admin on a dedicated server) picks the battlefield, factions, threat, prices, AI commanders, AI squad cap, starting RP, income and shared command in a dialog and presses Start; other players wait. A dedicated server starts by itself 5 minutes after the first player joins if nobody does. Our Arland, Everon and Kolguyev scenarios set it in their header; the AC: Actual Play and Host launchers pass `-acrSetup 1` and leave match settings to this screen |
| `-acrFirstPlayerHost` | On dedicated servers, additionally trust the first connected player for this session. Off by default for ordinary dedicated servers; Host-ACA enables it for its separate local server, so the launching host must connect before friends. Configured admins remain trusted, and authority does not transfer after the first player disconnects |
| `-acrAdminPassword <password>` | Chat login for servers without a `-config` file (such as local test servers): a player who types `/login <password>` in chat becomes admin for the session, which unlocks the host tools. Off without the option; use a private password and don't share it in logs or screenshots |

## Designing a good map

- **Give each side a home base.** Set it as owned, allow units to spawn there, add a garrison, and put
  that side's spawn point nearby. Place its starting units around the base.
- **Leave room between bases.** The AI commander plans attacks outside the target base and needs space
  between that plan and other enemy bases. Bases less than about 400 m apart can confuse its plans; try
  600 m to 1.5 km between front-line bases.
- **Keep garrisons where they matter.** A base's guards appear only when enemies come within 400 m, so a
  large garrison does not use soldiers while the area is quiet.
- **Place buildings near important bases.** Guards and defending squads use nearby buildings for cover,
  including windows and doorways within 80 m of the base centre. Bases in the open are defended from cover
  near the centre instead.
- **Set each base's value to show its importance.** A value of 2 or 3 makes a useful objective. Use value
  0, no income, and no spawning for small tactical points such as a lighthouse or crossroads.
- **Use hidden bases** for rear areas and surprise supply points.
- **Check road access.** Squads use trucks on roads when that is faster than walking. Bases away from roads
  are reached on foot.
- **Keep the AI count in mind.** The default limit is about 100 soldiers per side. Very large maps can
  affect server performance.

## Current limits

- **The game's US and USSR sides** are the most tested. The host menu also offers RHS and UA pairings;
  see [Factions and mods](#factions-and-mods). Other custom sides may not have player command gear yet.
  FIA selection, starting units, buy lists and command loadouts have automated coverage; broad
  live faction/mod combinations remain less tested.
- **Everon, Arland and Kolguyev.** Everon is the main large-map playtest terrain; travel, rides
  and AI behavior are still being tuned. Kolguyev uses generated layouts; other terrains need
  their own scenario setup and working navigation data.
- Advancing infantry in Combat mode can detour around a fresh infantry contact after a short
  stalemate. Nearby squads take turns manoeuvring; the original destination and queued orders
  remain. Defend, Suppress, Fast and Silent orders keep their assigned roles. Terrain and native
  pathfinding limit which flanks can be used; live combat acceptance is still pending.
- **No victory conditions**, by design: the match runs until the host ends it.
- **The AI commander cannot fly bought aircraft.** Combat support and off-map air lift are
  script-flown; the AI can call air lifts for long moves and road detours.
- **Player construction only:** AI base/helipad building is not implemented. There is no save/resume system.
- **Intel includes automatic base reports:** enemies within an owned base's radius plus 60 m
  are reported without line of sight. Reinforce also refuses enemies within 300 m even when unseen.
- **PC controls:** keyboard/mouse is the tested control scheme; console/controller play is unverified.
- Settings may still change between alpha builds. This page is updated with them.

## Battlefield character

Vanilla church organs have a rare AI Easter egg: each organ has a 0.1% chance per
minute to send one conscious AI on foot within 50 m to play its 25-second toccata.
Any faction can play; player-controlled soldiers are excluded. Each trigger has a
30-minute cooldown. The group keeps its orders, and new orders or a player at the
keyboard cancel the detour.

## Testing your scenario

1. Start the map from Workbench. For a map saved inside the AC: Actual project, you can also use
   `Start-Test-Range.ps1 -World "Worlds/ACR_<Name>.ent"` to play alone, or
   `ACR-Multiplayer.ps1 -Mode Host` to host it; select your map in the menu. See [Save, export and run](#save-export-and-run).
2. Open Workbench's console, or the game's log in `Documents/My Games/ArmaReforger/logs/<date>/console.log`.
   Check that:
   - `[ACR] Scenario started` appears: AC: Actual is running on the map.
   - `[ACR] Requisition list for US: N entries` has entries: the side's buy list loaded. If it says `0`,
     check that your faction key matches the faction manager.
   - `[ACR] Starting unit: <name>` appears for each starting unit you placed.
   - `[ACR] Host options: ...` shows the settings you selected.
3. Open the map with **M** or **F9**. You should see bases and squads, and be able to open the command panel.

Problems or questions: join [Discord](https://discord.gg/WK8xGSztB) for **#issues** (bugs; choose **AC: Actual**)
or **#general** (questions), or [open a GitHub issue](https://github.com/propst-design/ArmaCommanderReforger/issues/new/choose).
Include the relevant log with bug reports; remove passwords, tokens, IP addresses and personal information first.

## Play a custom map with a friend

For a private playtest with the supplied launcher, copy your map into the AC: Actual project on both
computers. Keep the world, its layer, and its Workbench `.meta` files together. Send your friend a matching
copy of the development project with the map included. The host starts `Host-ACA.cmd` and picks the map;
the friend starts `Join-ACA.cmd` from their copy and enters the address shown in the host window. Use the
development project, not the release zip, so the updater does not replace the custom map. The package's
`READ ME FIRST.txt` explains the address and connection setup.

If you keep your map in a separate addon, you can test it in Workbench and publish it for the game's
scenario list. But the supplied AC: Actual launcher cannot load that separate addon yet. A published
scenario can use the game's normal Multiplayer > Host flow only when both players can install and
enable the mission addon and its AC: Actual dependency. Both players need a compatible AC: Actual
version and all dependencies. An unpacked local copy does not by itself create a Scenarios/Host
entry. See [Run a map kept in your own addon](#run-a-map-kept-in-your-own-addon) for the current
limitations and options.

### RU, UA and RHS USMC default formations

With RHS Russia, RHS USMC or the UA replacement faction selected, an empty requisition list
with automatic filling enabled offers curated formations. Explicit requisition lists
keep their authored entries and existing catalog behavior. Prices below are base RP;
ordinary units retain the selected stepped or fixed-stock pricing mode.

| Formation | RU size / RP | UA size / RP |
|---|---:|---:|
| Rifle squad | 6 / 24 | 9 / 30 |
| Maneuver team | 4 / 14 | 4 / 14 |
| Machine gun team | 3 / 12 | 3 / 12 |
| RPG-7 anti-tank team | 3 / 14 | 3 / 14 |
| Disposable AT team | — | 3 / 12 |
| Scout team | 4 / 18 | 4 / 18 |
| Sniper pair | 2 / 16 | 2 / 16 |
| Western rifle squad | — | 8 / 34 |
| Western fire team | — | 4 / 18 |
| Special Forces team | 6 / 48 | 6 / 48 |
| Engineer team | 4 / 16 | 4 / 16 |
| Medical team | 3 / 10 | 3 / 10 |

UA ordinary formations retain mostly Soviet equipment with some Western rifles;
Western formations explicitly provide M4/M203, M249/M240 and M21 equipment.
Special Forces receive suppressed primary weapons, modern armor and night vision,
with no thermal sights. Their fixed stock is 1/2/4 at Low/Normal/High threat.
AI commanders field at most one SF formation at a time and do not increase its
purchase weight when its price rises. Existing caps and replacement costs apply.
Mortar and observer requisitions retain their existing configurable support entries.

RHS USMC offers these additional curated defaults:

| Formation | Size / base RP |
|---|---:|
| Marine Rifle Squad | 9 / 34 |
| Marine Fire Team | 4 / 18 |
| Light Infantry Team | 4 / 14 |
| Machine Gun Team | 3 / 12 |
| Grenadier Team | 4 / 18 |
| Light AT Team | 4 / 16 |
| Scout Team | 4 / 20 |
| Sniper Pair | 2 / 16 |
| Engineer Team | 4 / 16 |
| Medical Team | 3 / 10 |
| MARSOC Team | 6 / 48 |

The Marine rifle squad retains two M249s, two M203s, two M72 carriers and an M38
marksman. Ordinary M27 carriers use 4x RCO optics; M203, M240, M38 and M40 roles
retain their dedicated sights. Native MEF garrisons and officers also exchange their
8x VC18DSCO for an RCO. MARSOC uses distinct woodland clothing, suppressed HK416,
M27 and M38 weapons, modern armor/headsets and mounted dual PVS14 night vision.
It follows the same stock and AI restrictions as RU/UA SF, with no thermal optics.

Shared default vehicle base prices across these factions are Humvee 20 unarmed / 32
armed, transport/ammo/repair/fuel truck 22 and construction truck 24. RU additionally
uses BTR-70 72, BRDM-2 52, Typhoon 38 unarmed / 52 armed and UAZ 16 unarmed / 26
armed. Ambulances, LAV-25, aircraft and UA APCs retain their existing catalog prices.
Explicit mission-authored requisition prices remain unchanged.
