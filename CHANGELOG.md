# Changelog

## 0.5.8 — unreleased

### Players and spawning

- Choose your side: players now pick a side, an officer loadout and where to deploy on the game's deploy screen once the host presses Start, so both sides can be played by people (#39). Before, everyone spawned as a West commander unless the host forced the whole lobby onto one side. An AI commander still runs any side with no player in command, so player-vs-player, co-op and mixed games all work from the same scenario.
- Deploy at any base your side holds that can deploy units (HQs, military sites and built FOBs by default): each has a spawn point named after it on the deploy map, closed while the base is contested or the FOB is offline. The HQ is the default, and a side that has lost everywhere else can always fall back to its HQ point.
- Each player has their own respawn timer (the host's respawn delay), shown on the deploy screen. Before, one dead commander held up the whole side's spawn point.
- You stay on the side you first deployed on for the whole match, reconnects included. Hosts and admins can move a player with `/acr side west|east <player>`.
- The scenarios offer 16 player slots (8 per side). They advertised 8 before.
- In a solo game, the war now waits while the game is paused (the Esc menu, and the deploy screen in a solo game). Before, AI commanders kept buying squads that couldn't appear while paused, and those purchases timed out and were refunded.

### AI commanders

- AI commanders refill worn-down squads before buying new units: a squad back at a base gets its replacements even if that dips into the fire-support reserve, and new purchases wait while one is waiting for the RP. Before, refills had to leave the whole reserve untouched, so in a long Everon game one side refilled 3 squads and bought 21 units while squads of 1-3 men sat at bases.
- AI attacks on a held base need infantry that can walk in and take it: recon teams don't make up an attack, an attack whose infantry is killed or falls back is called off at once, one whose squads never got near their staging point is called off instead of sending a lone squad in from far out, and a base of fire goes in itself when it is the last squad left. Before, a counterattack could be a single recon vehicle 2.5 km away that overwatched 400 m short of the base until the attack timed out.
- AI squads on foot take a covered way to their staging point: when the direct line runs in view of the objective, they swing round it out of sight (behind a rise, through dead ground) as long as the detour stays reasonable, and the forming up waits for the longer walk. Staging points move a little further round or out where the objective can't see them. Before, a flanking squad walked the shortest path across the front of the objective.
- A soldier stranded off walkable ground (on top of a sandbag pile, say) steps back onto it when his move order keeps getting dropped, and an AI squad that can't reach the base it was sent to for reinforcements gives up after a minute instead of being re-sent every 15 seconds. Before, one RU machine gunner on a FOB's sandbags lost every order for over half an hour.
- The AI commander doesn't send a medic team or another support team to grab an empty base any more, as it already kept them out of attacks (#34).
- An AI attack or grab whose squads all report the target unreachable is called off at once, so they get new orders within seconds. Before, the commander waited out the attack's full time limit: on Everon a grab of Gallows Base, which can't be walked into from the south, kept its squad standing idle for 17 minutes.
- AI mortar teams follow the fighting: they move up to the own base nearest the enemy's known bases and the bases being attacked, instead of staying at the HQ whenever any unclaimed site happened to be in range (on Everon both teams sat at their HQs all game and fired one mission in two hours).
- A changed AI doctrine gets one review to settle in before it is judged, so doctrines change at most every 20 minutes. Before, a side taking losses could flip between Firepower and Fortress every 10 minutes.
- An AI doctrine that changed is kept at least 30 minutes, and the AI never switches straight back to the doctrine it just left for the same kind of reason (#29): losing ground and heavy losses count as one reason, and every counter as one. Before, a side that kept bleeding swapped between Fortress and Firepower at every review it was judged, and two AI sides could chase each other's counters all match.
- AI doctrine reviews don't count bases taken as winning while the side is losing men heavily: early on most bases taken are empty, and a commander losing 20 men for 4 kills used to think its doctrine was working.
- AI defences are built, upgraded and manned (playtest: one roadblock and one empty hasty position all game). Roadblocks, minefields and new Fortify positions go to whichever of the side's three bases nearest the enemy lacks one, not only the single nearest base. Its positions are upgraded (lowest tier first) before new ones are dug. One minefield is laid at a time, and no squad (AI or player) is sent to lay a field where another of its side is already on its way to lay one. With no free squad near the base (450 m for a roadblock, 600 m for mines, 500 m for Fortify), the nearest free squad within 1.5 km takes the job; past the doctrine's ride distance it rides out in a truck when one is free and is given, and pays for, the job at the drop. A squad on its way to refill isn't sent. A builder now stays on the roadblock or position it built (up to 2 per side); those squads aren't sent on attacks, and leave to defend a base only when their side is outnumbered there, or as a last resort. A lower cap keeps those holding longest. An empty position at a front base (its holder killed, or gone to defend) is manned again by the nearest free squad while the doctrine's holders allow, and only positions in use (manned, or at a front base) count towards the doctrine's number, so ones the front has left behind no longer stop new ones. A manned tier II position counts its holders half again towards its base's defence, a tier III twice, so the AI values keeping it manned. With the build budget on, engineering is paid out of it, and it gets at least 2 RP every 15 s on top of its share. A **Fortress** AI builds and mans more: twice the budget's share and trickle, up to four Fortify positions, and up to four builders that stay on what they built, pulled away only when their side is outnumbered at a base. A position whose upgrade was refused twice for the same reason is given up by the AI. Rate-limited log lines say what held a build up.
- A rich AI side digs in (#28): with the AI build budget on, a side with more than 600 RP banked raises the budget's share with the surplus, to twice its share at the 1000 RP cap (still at most 50%), and keeps the budget topped up from what it holds over 600 (still at most 100 RP), so its roadblocks, minefields and positions get built instead of the RP sitting at the cap. In overnight Everon games the leading side sat at the cap from about 105 minutes with nothing left to buy. At or below 600 RP nothing changes.
- A rich AI side fires more (#28): over 600 RP banked, by the same measure that raises its build budget (x1 at 600, x2 at the 1000 RP cap), it keeps RP for one more fire mission and counts air and rocket strikes whatever its doctrine, up to twice its doctrine's fire-support cap; it shells the attackers of a base that is merely in a fight (at the cap, from half its defence) rather than only one that is losing; it calls gun runs, rocket strikes and air preparation at lower threats and against fewer defenders (half at the cap); and its forward observers call missions more often. So the surplus turns into fire missions instead of sitting at the cap. At or below 600 RP nothing changes.
- A rich AI side buys quality, not quantity (#28): over 600 RP banked its purchases lean towards dearer units, more the richer it is (at the 1000 RP cap a 76 RP APC is three times as likely, a 12 RP team half as likely), armed vehicles are held back less, and when it needs a transport it takes an armed one such as an APC over a plain truck when it can. The squad cap is unchanged, and refills still come before any purchase.
- AI vehicle crews stay aboard on fast moves (#57): the crew of an armed vehicle no longer climb out at a gate or to heal a crewmate while their squad's order is in Fast mode, as they already didn't in Combat mode. Before, every Fast order counted as a player's order to Better Battle Buddies, which leaves those alone, so on AI commanders' attack moves and reinforcement runs LAV and BTR crews got out at gates and to treat a wounded commander. Orders a player gives a squad they command directly, and Game Master orders, still let them get out.
- A vehicle crewman Better Battle Buddies turned out of his hatch on a quiet stretch goes back in when a Fast or player order arrives, instead of riding into the fight turned out (Better Battle Buddies #7).
- AI squads can use squad tactics inside an objective (#31; host option, off by default): with `-acrSquadTactics 1`, once an attacking squad is inside the base it was sent to take, Better Battle Buddies' squad tactics run it (assault by bounds, flank a dug-in enemy, hold when outnumbered, search when contact is lost), never leaving the base's ring; the AI commander still picks the target, stages the squads and sets the base of fire, and squads staging or riding are left alone. Squads defending a base on a Defend order may bound to cover or push back an enemy inside the base, never chasing out of it (garrisons in buildings stay put). Off by default because AI-vs-AI test games showed no clear gain yet; Better Battle Buddies' squad tactics are being revisited first.
- AI building budget (host option, off by default): `-acrAIBuildShare N` makes each AI commander set N% (at most 30) of the RP it gains aside for engineering, up to 100 RP. Unit purchases leave that money alone, so its roadblocks, minefields (`-acrMinesAI`) and Fortify positions (`-acrFortifyAI`) get built instead of losing out to the next squad. The AI status log line shows the budget. The setup screen has a row for it: **AI build budget** (off, 10, 15 or 25% of income).
- AI minefields go where they do most: mixed fields across the road 60-150 m ahead of the side's own roadblock, where its gunners cover them, more on the enemy's road route and at chokepoints; AP fields in dead ground and tree lines in front of its positions. The spots are scored and tried best first, the old ring last, so the AI never lays fewer fields than before.
- The AI picks its FOB sites by score instead of taking the first that fits: every one of the old spots back from the front base that is clear is scored for room (the HQ and all its upgrade and defence spots clear), concealment from the enemy base and its approach, a road close by for the trucks, the distance back from the front, cover from our roadblocks and Fortify positions, the enemy's road route (off it is better) and towns (outside is better). Never fewer FOBs: the old first fit is always one of the candidates. The search is spread over a few strategy ticks.
- AI Fortify positions go where they see the approach: spots 80-200 m out towards the enemy are scored on their field of fire from a gunner's eye, slight high ground, no dead ground close in front, cover behind (counted twice), upgrades that fit, and overlooking the side's own roadblock or field, and tried best first. Spots on paved ground score less for every point of the position on it, spots on an airfield (by its runway lights, runway decals or an apron's concrete panels) and spots walled in by a building or other tall structure within 30 m in front (its field of fire stops there) score least, so the edge of open ground with cover behind wins; in a playtest both of a side's starting positions went on Arland's airfield, its longest field of fire, and a position faced a wall. The first spot tried is picked at random among those within 12% of the best score (seeded from the match's seed), so the AI doesn't dig in on the same spot every match. The old line spots are still tried, so the AI never digs fewer positions than before.
- AI choices vary between matches: where an AI choice is made from scored options, it is picked at random among those within 12% of the best (the best most likely, never a poor one), seeded from the match's seed so the same seed makes the same choices. `-acrVariety 0` always takes the best. The AI's Fortify spots use it first.
- AI attacks only count the squads that can get there: on foot within 2.1 km, in their own vehicles, by a taxi they can actually get (each taxi counted once), or by a ready air lift. A target beyond their reach is skipped, and the forming up waits for the slowest squad's walk or ride, up to 20 minutes. Before, squads 2-3 km off were sent on foot and the attack went in before they came (#27).

### Engineering

- Fortify positions get defenders: tier I holds 1 of the side's garrison soldiers in reserve at the position, tier II 2 and tier III 4 (an upgrade adds the difference). Like a base's garrison they deploy when an enemy comes within 400 m, defend the position, pack up once none is within 450 m and are used up as they die; a position that is gone takes its reserve with it. Sizes: `-acrFortifyDefenders1` / `-acrFortifyDefenders2` / `-acrFortifyDefenders3` (0 for none). Checkpoints can get them too with `-acrRoadblockDefenders N` (off by default).
- Sandbag positions, barbed wire and barricades lie on sloping ground instead of standing dead level: tilted to the ground under the ends of their footprint, at most 12 degrees. On a slope the site checks accept, one end used to float while the other was buried. Tents, buildings, bunkers, towers, checkpoints and gun nests stay upright. `-acrGroundTilt 0` places everything level as before.

### The battlefield

- Gunners stay on held guns (on by default; setup row **Gunners stay on held guns**, `-acrHoldGuns 0` for vanilla): a squad holding a spot (Defend, a checkpoint, a Fortify position, FOB defences) keeps its men on the static guns there. Vanilla took a gunner off his gun whenever anything he knew of, even a minute old, or just the sound of firing, was outside the gun's arc, so checkpoint and nest guns kept standing empty. Now he leaves only for an enemy he saw in the last 10 s within 60 m (a real flank), and a held gun that stands empty for 8 s is manned again by the nearest man of the squad.
- A Fortify position's guns stay manned while its squad is away: when the squad holding a tier II or III position leaves it for another job (laying mines in front, say), men from the position's reserve defenders man its guns, and they go back into the reserve once a squad holds it again. The squad goes as ordered. Before, the nest stood empty until the squad came back (playtest).

### Fixes

- A ride waits at the drop until its AI riders are out (up to 20 s, as it already did for a player). Before, the transport drove home as soon as it dropped them off, and an LAV, whose rear seats don't let men out once it moves, carried some of them home (#23). A man still aboard after the dismount's second try is taken out without a door; in the LAV's rear seats, where every exit was refused (the game said nothing was pending), his vehicle action queue is cleared first, and as a last resort he moves to another seat and gets out from there.
- A stuck vehicle giving a ride re-routes like a soft vehicle instead of holding where it is like a fighting one. A ride never takes a vehicle with a manned gun, so an LAV taxi that held only dropped the ride, with its riders still aboard (#23).
- Arland now starts on a dedicated server set up with a server config (the usual way to host). The Arland scenario and the Western medic shared one resource ID, so the server loaded the medic instead of the mission and stopped (#41). The medic has its own ID now; the scenario's ID is unchanged.
- AI FOB builds are no longer given up while the construction truck is still driving to a far site. The build's 3-minute give-up clock ran from when the truck was sent, so a site more than about 3 minutes' drive away was abandoned on the way. Now every 10 m the truck gets closer counts as progress, and a truck that stops getting closer (stuck or blocked) still gives up 3 minutes later. In Everon soaks, 8 builds were given up in 2 hours and USSR built no FOB; with the fix none was given up in 100 minutes, USSR had its FOB at 22 minutes and US had FOBs at 53 and 73 minutes.
- An AI side no longer marks the site of a FOB it has just built as abandoned. For up to 2 seconds after a build finished, before the FOB appeared, the AI could take the build for one that was given up, so it avoided the new FOB's own site for 10 minutes and wouldn't rebuild a lost HQ there in that time.

### Command and setup

- AC:A Think (hosts and admins): an **AC:A Think** button in the map's Debug box, or `/acr think [west|east|off] [log]` in chat anywhere, opens a card showing what each AI commander is thinking, live: squads free, defending and attacking, each attack's phase, staging points and base of fire, RP, support and build budget and doctrine; then either every squad of that side (task, order, target, how long since its last order and how many it got in 5 minutes, flagged when that looks like a loop, and whether it is recovering a dropped move) or the commander's last 20 decisions. The button cycles West squads, West decisions, East squads, East decisions and off. Only the player looking gets the updates, every 3 s while the card is open.
- The RTS camera has a Minefield button next to Roadblock: it cycles AP, AT and mixed mines (and off), the toast gives the price, and a right-click lays them with the selected squad, as the map panel's minefield orders do (#4).

### Support

- A mortar team sets up out of the way of vehicles: not on or within 5 m of a road, and not within 30 m of a base's centre, where bought vehicles appear (#33). It looks up to 40 m round for such a spot (24 m before).
- A mortar team whose last two fire missions fired nothing takes no more missions until its tube is set up again (the support list says why). An AI commander's team moves a short way and sets up again by itself. Before, the AI kept paying for or queueing missions on a team that couldn't fire, every few minutes.
- A helicopter whose engine or rotor was shot up while it stood on its pad is repaired by servicing. Before, it looked fully healthy, wasn't repaired, and went back into servicing every minute for the rest of the game (the side couldn't use it or buy another). Vehicles serviced at depots and on reinforcement get the same full repair.

### Intel

- "The enemy commander has changed tactics" only names the old doctrine when it had been confirmed. A hint such as "Firepower or Fortress" could include the new doctrine, so the line could say "no longer Fortress" as the enemy switched to Fortress.

## 0.5.7 — released

Dig in and read the enemy. Fortify positions, mine the approaches, size your checkpoints, and face AI commanders that fight to their own doctrine and change it when it fails.

### Engineering

- Fortify position: a commander order that digs an infantry squad in where you right-click, facing the nearest known enemy or enemy base. Ordering it again on the position upgrades it from hasty (a line of three sandbag positions behind a barbed-tape fence, 10 RP) to dug in (an MG nest in place of the middle position, so the gun is in the centre of the line, and a sandbag bunker under a camo net, 15 RP) to a strongpoint (a bunker, and a heavy MG on a flank turned in across the front for crossfire, 25 RP). Pieces avoid big rocks, and each gun is placed where nothing but wire blocks its line of fire (a gun may aim a little up rising ground). An upgrade goes in whole or not at all: its pieces look further afield when their usual places are blocked (a road, rocks, a slope), and an upgrade that still can't fit is refused when you order it, with the reason and nothing charged. The AI commander picks sites where its upgrades will fit and leaves a position alone for a while after an upgrade there is refused. Work pauses while contested and runs twice as fast with engineers; the squad then holds the position and mans its guns, and the position shows on its side's map until its pieces are gone. `-acrFortify 0` turns the order off and `-acrFortifyAI 1` lets AI commanders fortify their approaches; both are on the startup Logistics settings page and in the mission header.
- Commander minefield orders: infantry squads lay AP, AT or mixed minefields (15 / 20 / 25 RP) and disarm and lift mines (3 RP) from the map panel. AT mines go in both lanes of the road through the field and AP mines off it, minefield signs mark the corners, and the area shows on the owning side's command map. Work pauses while contested and runs twice as fast with engineers or a construction truck; a cancelled order refunds the share not laid. The owning side's AI never sets off its own mines unless `-acrMinesRealistic 1`. `-acrMines 0` turns the orders off and `-acrMinesAI 1` lets AI commanders mine their approaches; all three are on the startup Logistics settings page and in the mission header.
- Minefield orders survive a regroup: a squad that stops to regroup on the way to its field now carries on and lays it afterwards. Before, the regroup cancelled the order and refunded it, and the squad then stood at the site doing nothing until it got a new order.
- Minefield orders no longer time out after a long pause: time with the order set aside (a regroup, a player holding the squad) doesn't count towards the walk limit or the 3-minute give-up timer, so the order isn't abandoned when it resumes.
- Roadblock sizes and barricades (a port of the ACA Field Engineering add-on): the map panel's Roadblock button becomes **Roadblock S / M / L** and **Barricade**. The small checkpoint has no machine gun (12 RP, 35 s), the medium is the old roadblock (20 RP, 45 s), the large carries a heavy machine gun (30 RP, 70 s), and barricades are sized to the road (8 RP, 25 s). RHS AFRF now builds the Soviet set, and FIA builds log barricades. A construction truck with the squad pays in supplies (75 / 225 / 350 / 40) when it has them, else RP. A build called off is refunded where it was paid from, Halt included (it used not to refund). The 3-minute give-up timer starts when the squad reaches the road (the walk has its own limit), and the roadblock waits for vehicles to leave the spot. AI commanders still build medium checkpoints paid in RP.
- Roadblocks ordered by a player face the threat: the nearest enemy their side knows of within 600 m, else the nearest enemy base it can see (the AI commander's already faced the enemy base). Before, the checkpoint faced whichever way the road happened to run.
- Roadblocks snap to the road centre instead of staying at the clicked position; clearance checks use the actual construction site.
- Minefield, roadblock and Fortify orders given to a squad still riding in a vehicle now carry on once the squad is out. Before, getting out cancelled the order and refunded it.

### AI commanders

- AI commander doctrines: each AI side fights to one of five styles for the whole match. Balanced is the old commander; Blitz goes mechanised and attacks early; Firepower leans on mortars, gunships and preparation fire; Fortress defends in depth and counterattacks lost bases at once; Raider sends small teams after weak bases and FOBs. New setup rows "Blue AI doctrine" and "Red AI doctrine" (random by default). No doctrine fields a bigger army.
- Doctrines now shape the whole commander: rides and air lifts, defence in depth and how long defenders hold, defensive air strikes, mortar fire missions and forward observers, base-of-fire time, minefields, FOB placement, refits and FIA pockets. Firepower really keeps RP for two fire missions now, and Raider no longer re-attacks the base that just beat it or fields fewer men than the others.
- Enemy doctrines can be discovered: contacts, enemy FOBs found and bases changing hands build up intel until the side gets a hint ("Blitz or Raider?") and then a confirmed doctrine, shown in the map panel's Report. Players under an AI commander see their own commander's doctrine there.
- AI commanders change doctrine when theirs isn't working (losing ground, heavy losses, stalemate), or to counter an enemy doctrine they have confirmed. The other side only notices once fresh intel shows the change. New setup row "AI doctrines adapt" / `-acrDoctrineAdapt 0` to keep the first doctrine.
- AI commanders keep spending in long games: the squad cap counts only fighting units (trucks, ambulances and 1-man remnants no longer fill it), and badly hurt idle squads walk back from the front to a quiet base to refill instead of staying at their post at a third of their strength.
- AI squads become available for combat orders after finishing a roadblock, instead of being tied to it indefinitely. Roadblock construction leaves the commander's reserve squad available.
- Recon teams (snipers, scouts, spotters) take overwatch on the front, armoured vehicles hold a standoff ring around an objective instead of driving into it, and mortars move 15 seconds after a fire mission. Heavily defended targets get gunship and rocket preparation fire.

### The battlefield

- FIA pockets of resistance: on whole-map and half-map battles a few empty bases away from both HQs start held by an FIA garrison (bigger at military sites, which also keep a small patrol). They look unclaimed until your units get close, so a squad sent to grab an empty village may have a fight on its hands. Clear one to capture it. New setup row "FIA resistance": few (default), many or off.
- Generated battles vary tactical objectives and select later income sites together while preserving opening income, earning-site count, total RP and spacing. Consecutive seeds cycle evenly through small complete sets of valid income arrangements. Flashpoints vary their local area and objective mix, keeping 5-7 accessible sites and protected opening objectives.
- Generated half-map battles use varied diagonal, shifted and compact regions, with more suitable HQ arrangements across seeds. Existing objective access, spacing and opening-income safeguards remain in place.
- RU, UA and RHS USMC requisitions use distinct infantry formations with revised prices, dedicated support ammunition and role-specific replacement kits. UA keeps mostly Soviet equipment alongside Western-equipped squads and fire teams. Rare Special Forces carry suppressed weapons, modern armor and night vision without thermal sights; AI selection does not increase with their price. Marines use combat optics, dedicated scout/engineer/sniper teams and rare suppressed MARSOC; shared utility and logistics vehicles use aligned prices.
- Fixed UA and Russian Typhoon purchases failing to deploy because their driver and gunner crews never spawned.

### Command and setup

- The setup screen is sorted into tabs: **Battlefield** (sides, battlefield, seed, the map preview, FIA resistance and threat), **Command**, **Economy**, **Logistics** and **Engineers**. The map preview now sits right under the seed it shows, and a line at the top sums up the match (sides, battlefield, seed) from every tab. Before, the list could run off a 1080p screen, and the map preview was a page away from the seed. A screen too short for a tab still gets a page button. The tabs and rows use the command panel's blue buttons, so the row under the mouse no longer turns white on white.
- Shift + right-click on a squad that is holding where it arrived now sends it there at once. Before, the point was queued behind its hold, which never ends, so it never went (the panel said "waypoint added" and the squad stood still).
- Fixed the 3D commander camera missing its mouse cursor and movement controls. WASD pans, right-drag rotates and the wheel changes altitude; exiting restores the previous view.
- 3D commander camera: each mouse-wheel notch is a small altitude step, panning speeds up with height, and friendly squads and bases get on-screen markers with name and distance that you can click to select.
- Taking over a soldier in a squad on a ride no longer ends the ride and sends the transport home. The truck waits at the pickup and drop-off for a player rider, and F8 puts the riders back on the ride's order. After death you return to the squad you possessed, not the last squad you ordered.
- Host and Play launchers start every Better Battle Buddies behaviour switched on, and its options sit in one BBB submenu instead of separate main-menu toggles.

### Air lifts

- Off-map lift helicopters arrive already airborne with rotors spun up, saving about 20 seconds. When a squad needs two helicopters and only one landing spot fits, the second holds short until the first has unloaded and left.
- Air-lift passengers receive individual boarding assignments for both helicopters, so soldiers who cannot fit in the first Huey walk to the second. Scripted placement is reserved for stalled boarding.
- Two air lifts no longer pick landing spots within 40 m of each other, and loose weapons and gear dropped at a landing zone no longer stop a helicopter touching down.

### Fixes

- Fixed late-game match stalls and infinite supply truck pathfinding loops: AI commanders scale active attack fronts (up to 8) and unit caps on large maps when holding surplus territory and RP, dismounted infantry no longer embark on multi-kilometer cross-island grabs on foot, and supply trucks en route to bases avoid redundant 15-second route recalculations.
- Replaced 4-point map status bar polygons with paired triangles, eliminating Enfusion canvas ear-clipping triangulation errors when viewing the command map.
- Fixed player movement stall false alarms caused by evaluating active input contexts while stationary.
- No more "Defend Waypoint not provided" script errors when a defending squad is given a new order (an ambush ending, a roadblock squad sent elsewhere).
- Fixed persistent AI vehicle horn audio loops: AI drivers cap continuous warning honks at 0.8 s with a 5 s silence cooldown, vacant/incapacitated vehicles immediately mute their audio signals, and per-frame post-simulation enforcement silences audio loops on dedicated servers, listen servers, and remote clients.
- A squad recalled from manning its own roadblock gun now walks to its new order instead of finishing it on the spot.

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
