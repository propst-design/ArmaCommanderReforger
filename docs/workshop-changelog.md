0.6.0

Decks and a deck builder, the reworked AI commander on by default, a growing insurgency, the rest of the contained outbreak and counter-battery fire. Needs Better Battle Buddies 0.2.0.

NEW

- Decks: each side buys from a deck of cards picked from its faction's catalogue within a points budget (Deck points row, -acrDeckPoints). The standard deck is the usual buy list, so nothing changes unless someone builds one.
- Deck builder: press F3 on the map or during setup. Save several decks per faction, share them as one-line codes, add custom teams from your faction's own soldiers (optionally with a ride), and hand a deck in during setup. The Blue deck and Red deck rows pick what each side plays: standard, a handed-in deck, or a deck the AI commander builds from its doctrine. RHS and Ukrainian factions get extra kitted team cards.
- Insurgency (with -acrGuerrillas): FIA hideouts recruit and spread to empty towns, plant roadside IEDs with a watching triggerman, send sabotage teams against FOBs and roadblocks, and run technicals from depots. Wiping out FIA teams marks roughly where their hideout is.
- Counter-battery: firing a mortar gives it away. The other side gets a ring that narrows with every round heard, mortars answer pinned-down tubes, and AI mortar teams under fire move.
- Contained outbreak: civilian evacuation (give a squad the Evac point order near a town marked Civilians, then drive them to a safe base for 5 RP each), supply airdrops worth 30 RP that either side can recover, and a truce between the two sides against the dead, which any shot or strike on the other side breaks at once.
- Truck supplies for FOBs, in every mode: park a construction truck by your FOB's HQ and unload its paid load into the FOB's store.
- Mission intro in the Arma 2 style: a short orbit of your HQ with music and the title, then into your soldier. Space or Enter skips it; Mission intro row or -acrIntro 0 turns it off.
- Civilians on the map: a vague violet ring where your people see civilians (with the Civil Population mod and its compat add-on).
- The 2IC can build a FOB where you ask (Ask the 2IC menu, Build a FOB here).

BETTER

- The AI commander rework is on by default: one main push with a supporting attack, jobs weighed together, attack forecasts from its own intel, covered withdrawals. AI commander row set to classic (or -acrAICommander classic) brings back the old one for this release.
- AI squads low on ammo go and get more, from an ammo truck when it is safe or at a quiet base.
- Zombie mode: soldiers fight zombies like zombies (Better Battle Buddies 0.2.0), the outbreak keeps growing while nobody contains it, zombies show on the map as zombies, and nests send up smoke you can see from far off. Nest bounties are paid for settled nests only, at most 60 RP, split between the sides that fought there.
- Taking the lead of a squad keeps its order until you give your first one.
- F9 in the RTS camera goes straight back to the command map.
- Soldiers who surrender (Better Battle Buddies 0.2.0) leave their squad and hold no base.
- Experimental, off by default: with Better Battle Buddies' driver on (-bbbDriver 1), BBB drives the AI commander's vehicles on long moves, never a player's squad. Tested on 12 route trips only; convoys, towns, combat and multiplayer are untested.
- The command map sends and redraws far less: markers are kept rather than rebuilt, hidden soldiers get none, and positions go out only while the map or RTS camera is open. This helps busy multiplayer matches.

FIXED

- Riders get out of AI trucks only once the truck has stopped, and it waits until they are clear.
- AI trucks that gave up stuck try their trip again, and stop being offered for rides right after they stall.
- AI drivers judge a vehicle in the way by their own lane, so trucks side by side or nose to nose no longer block each other.
- An unarmed AI vehicle driving out of fire that gets hung up on the way tries once more before its crew bails out.
- AI service trucks drop a job they can't finish instead of keeping it all match.
- A worn-down AI squad that can't reach a base to refill tries another one.
- The 2IC channel keeps open requests and their Cancel buttons; accepted defence advice isn't offered again.
- A FOB with a supply depot really stores 3,000 supplies.
- Zombie messages name the town again.
- A purchase, support call or setup click sent right after another request is no longer dropped silently; one sent too fast says so.
- Typing a name in the deck editor no longer closes the map, and joined players with a modded faction pair open it on their own side.
- A second match on the same server starts clean, and setup's auto-start waits while the host or an admin is on the server.
- With AC: Actual loaded but not playing (Game Master, Conflict), respawning, the map's tool bar and disconnects work as in vanilla again.
- Men who leave a squad alive (AiO Recruit or Dismiss, a Game Master) come off its strength instead of being bought back.
- A squad with a player riding along as a rifleman keeps its AI orders running.
- A FOB's store can't be filled or emptied by dragging supplies in the inventory.
- A captured town's base works are planned for its new owner, and at the cap a new fortified position or minefield replaces the side's farthest one.
- Rides no longer wait for unconscious riders, and an air lift that runs out of time sets its squad down.
- Garrisons stand down when only civilians are in town.
