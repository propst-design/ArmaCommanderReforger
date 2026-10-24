# Changelog

## 0.5.7 — unreleased

- AI commanders keep spending in long games: the squad cap counts only fighting units (trucks, ambulances and 1-man remnants no longer fill it), and badly hurt idle squads walk back from the front to a quiet base to refill instead of staying at their post at a third of their strength.

- Commander minefield orders: infantry squads lay AP, AT or mixed minefields (15 / 20 / 25 RP) and disarm and lift mines (3 RP) from the map panel. AT mines go in both lanes of the road through the field and AP mines off it, minefield signs mark the corners, and the area shows on the owning side's command map. Work pauses while contested and runs twice as fast with engineers or a construction truck; a cancelled order refunds the share not laid. The owning side's AI never sets off its own mines unless `-acrMinesRealistic 1`. `-acrMines 0` turns the orders off and `-acrMinesAI 1` lets AI commanders mine their approaches; all three are on the startup Logistics settings page and in the mission header.

- Fixed late-game match stalls and infinite supply truck pathfinding loops: AI commanders scale active attack fronts (up to 8) and unit caps on large maps when holding surplus territory and RP, dismounted infantry no longer embark on multi-kilometer cross-island grabs on foot, and supply trucks en route to bases avoid redundant 15-second route recalculations.
- Replaced 4-point map status bar polygons with paired triangles, eliminating Enfusion canvas ear-clipping triangulation errors when viewing the command map.
- Fixed player movement stall false alarms caused by evaluating active input contexts while stationary.

- Air-lift passengers receive individual boarding assignments for both helicopters, so soldiers who cannot fit in the first Huey walk to the second. Scripted placement is reserved for stalled boarding.

- Host and Play launchers start every Better Battle Buddies behaviour switched on, and its options sit in one BBB submenu instead of separate main-menu toggles.

- Fixed the 3D commander camera missing its mouse cursor and movement controls. WASD pans, right-drag rotates and the wheel changes altitude; exiting restores the previous view.

- Roadblocks snap to the road centre instead of staying at the clicked position; clearance checks use the actual construction site.
- AI squads become available for combat orders after finishing a roadblock, instead of being tied to it indefinitely. Roadblock construction leaves the commander's reserve squad available.

- RU, UA and RHS USMC requisitions use distinct infantry formations with revised prices, dedicated support ammunition and role-specific replacement kits. UA keeps mostly Soviet equipment alongside Western-equipped squads and fire teams. Rare Special Forces carry suppressed weapons, modern armor and night vision without thermal sights; AI selection does not increase with their price. Marines use combat optics, dedicated scout/engineer/sniper teams and rare suppressed MARSOC; shared utility and logistics vehicles use aligned prices.

- Fixed UA and Russian Typhoon purchases failing to deploy because their driver and gunner crews never spawned.

- Fixed persistent AI vehicle horn audio loops: AI drivers cap continuous warning honks at 0.8 s with a 5 s silence cooldown, vacant/incapacitated vehicles immediately mute their audio signals, and per-frame post-simulation enforcement silences audio loops on dedicated servers, listen servers, and remote clients.

- Generated battles vary tactical objectives and select later income sites together while preserving opening income, earning-site count, total RP and spacing. Consecutive seeds cycle evenly through small complete sets of valid income arrangements. Flashpoints vary their local area and objective mix, keeping 5-7 accessible sites and protected opening objectives.

- Generated half-map battles use varied diagonal, shifted and compact regions, with more suitable HQ arrangements across seeds. Existing objective access, spacing and opening-income safeguards remain in place.

## 0.5.6 — released

Take the fight forward. Build a base behind your advance, keep it supplied and defend it against an AI commander that can build, reinforce and raid too.

### Build, supply and defend

- Establish FOB turns a squad with a construction truck and 500 supplies into a forward-base builder. Pick a clear site on the map and keep the squad there for the 90-second build. Cancelling refunds the reserved supplies; losing the truck loses them. Stalled construction gives up with a refund after three minutes without progress.
- Develop each FOB through eight upgrade spots: hospital, maintenance, ammo, fuel, helipad, radio relay, barracks and supply depot. Maintenance and barracks have two tiers. Upgrades use the FOB's supplies, build faster with engineers nearby and pause while contested. Parked vehicles must clear a finished building's spot before it appears.
- Fortify the perimeter through three Defences tiers: sandbags, machine-gun nests, then bunkers and guard towers. Blocked positions are skipped. Barracks send part of the garrison on perimeter patrols, and eligible AI defenders can man the guns when BBB static weapons are enabled.
- Blocked upgrade spots are identified individually, and matching buildings already inside the FOB count toward its upgrades. FIA keeps its smaller maintenance and barracks models at the higher tier, with the full upgrade effect.
- The AI commander establishes FOBs near a distant front, develops and resupplies them while keeping an RP reserve, deploys from suitable forward bases and rebuilds lost headquarters.

### Raid, capture or rebuild

- AI sappers can plant a real demolition charge at an undefended enemy FOB headquarters, withdraw and detonate it after 45 seconds. Defenders can use the normal Disarm action; AI defenders are sent to the charge too.
- Players can destroy an enemy headquarters with an M112 or TSh-400 detonated within 10 m. Shovel dismantling requires the FOB to be contested with no defenders within 30 m.
- A destroyed headquarters cooks off with a heavy blast and secondary explosions. Stay clear: the main blast is lethal nearby and can wound out to about 30 m. AI sappers wait for their squad to get clear before firing.
- Losing headquarters takes the FOB offline: deployment, reinforcement, services, deliveries and upgrades stop. Surviving buildings remain and transfer with a captured FOB. Establish FOB inside a friendly offline base rebuilds its headquarters.

### A more coordinated opponent

- Attacks keep a base of fire in overwatch even before enemies are spotted at the objective. It fires at seen enemies and suppresses reported contacts instead of shooting at an empty objective centre.
- Assault squads form up on separate approach lines and commit together, giving nearby reinforcements time to arrive. Distant latecomers no longer trickle into an assault already under way; specialist support teams stay out of the assault force.
- Threatened AI bases defend in depth: some defenders move 100-300 m toward a spotted attack while others stay in reserve. Forward squads fall back when overrun, badly hurt or no longer needed.
- Improved AI purchasing and reinforcement decisions, including saving for complete units and avoiding repeated construction-truck purchases.

### Transport and fixes

- Air lifts search farther for a second landing spot when a squad needs two helicopters. Helicopters bank through turns and change pitch more smoothly; landing checks no longer treat nearby soldiers as rotor-blocking obstacles.
- Improved gunner-drive hill starts and recovery from stalled or unreachable vehicle orders.
- Gunners stay on a stopped vehicle's working, loaded gun when its driver is lost under fire; passenger squads can still dismount. Fixed soldiers repeatedly mounting and abandoning parked guns.
- Fixed FOB upgrades being invisible to joined players, destroyed antennas blocking relay replacement, overlapping construction wasting supplies and headquarters spawning over the construction truck. Blocked completions refund reserved supplies. FOB construction and removal now update nearby AI navigation.
- Planted demolition fuses survive command handovers and cancelled attack plans.
- Squads use an awake soldier as their reference when the leader is unconscious. Unconscious soldiers no longer hold, contest or spot bases, or block FOB construction. Fixed reserve squads repeatedly abandoning their regroup order.
- LAV commander positions count as crew and stay aboard during passenger dismounts. RHS rifles with folded backup sights now raise them when no optic is fitted.

### Feedback

- Report bugs in #issues and ideas in #suggestions on Discord: https://discord.gg/WK8xGSztB. The match setup screen links there too; GitHub Issues remain available.

## 0.5.5 — released

### FOBs and logistics

- Built FOBs start with an empty 1,000-supply store. Order 500 supplies for 20 RP, delivered after 60 uncontested seconds, then build from the FOB without keeping a construction truck nearby.
- A completed friendly radio relay enables FOB unit deployment and reinforcement. Losing it disables both until rebuilt; building, deliveries and supply-paid services can continue.
- Supply depots increase FOB storage to 3,000, with one storage bonus per FOB. Hospitals, ammo stores, maintenance bays and fuel buildings use local supplies and pause when the FOB is empty or contested. FOB supplies cannot be loaded into trucks, and FOBs no longer refill service trucks for free. Instant vehicle service still costs RP; instant infantry service at FOBs has a configurable RP fee.
- Fixed paid deliveries failing to reach the FOB's construction store. The engineering table now opens the funded build menu with service buildings and supply depots available.
- Heavy maintenance bays repair 40% more per service step at the same supply cost. Small and large living quarters add 11 and 19 garrison defenders; multiple quarters stack, casualties persist, and losing quarters reduces capacity.
- On generated battlefields, settlements provide infantry replacements; HQs, military sites, airfields and eligible FOBs deploy new squads and vehicles. The new Logistics setup page lets hosts adjust deployment, replacement and service rules. Mission makers also have header and per-base controls.

### Battlefields and requisitions

- Generated layouts have fewer crowded objectives and passive-income sites, while protecting home-region income. HQ placement better accounts for rear access, steep terrain, roads and bridges.
- Rare-unit stock now scales correctly with threat under stepped pricing. High threat provides four times the stock; rare-unit prices stay fixed.

### Command map

- Reinforcement results and refusal reasons remain visible in the command panel for 12 seconds, with several recent results shown together.
- Map labels are shorter, with full site details available on hover. Fixed duplicate markers and labels appearing briefly in the map corner.
- Opening the command map preserves its pan and zoom.
- Before Start, moved bases take nearby place names, non-HQ bases can be deleted, and Undo layout edits restores moves and deletions.

### AI and transport

- Medics and teammates use smoke cover before approaching exposed casualties. Passengers can dismount from a slowing vehicle under fire while fighting crews stay aboard.
- Improved helicopter air-lift landing clearance, acceleration and climbs over steep terrain. Lift helicopters now participate in collision avoidance and use separate cruising heights.
- Improved stalled-vehicle detection and reduced the wait after a taxi driver is lost under fire.
- Spread squad status updates over time to reduce simultaneous processing spikes.

### Setup and mission making

- Play and Host launchers now leave match settings to the in-game setup screen. Old saved launcher settings no longer override the match.
- Expanded mission-making guidance and added a dedicated Workshop section linking to the GitHub setup and sharing guide.

## 0.5.4 — released

Workshop publication confirmed by Jacob on 2026-10-17 (confirmation date, not the release date).

- Moved shared AI improvements into the required standalone Better Battle Buddies addon. Workshop installations now resolve BBB as a dependency.
- Updated installation guidance for Workshop and the separate BBB dependency. Legacy ZIP installations require the complete paired package once.

## 0.5.3 — previously published

- Published the corrected build with the return control and input handling for leaving the generated-layout preview map during match setup.

Earlier releases predate this versioned changelog; their development records remain in Git and the handoff log.
