0.6.1

Bug fixes across the AI commander, truce, evac, orders and deploying. Needs Better Battle Buddies 0.2.1.

FIXED

- Truce: neither the AI commander nor the 2IC attacks the truce partner's bases any more, attacks forming up against them end, and the AI no longer aims its main push at a partner's base, so its squads and base works should face a real enemy again.
- The AI commander's roadblocks, minefields, defensive positions and FOB sites should face the enemy it knows of, not the truce partner, an allied FIA pocket or a hideout it hasn't found yet.
- AI commanders whose HQ rebuild failed no longer send trucks and buy new ones every 30 seconds, and the squad sent to defuse a sapper charge is no longer pulled off the job.
- Evac: civilians gather on the evac point to wait instead of standing in a loose ring, and are delivered only from a vehicle on the ground at a base of the evac point's own side, not one right beside it. Only that side's vehicles load them, an empty vehicle left at the evac point counts as abandoned, and civilians in a vehicle the other side takes are lost.
- Evac: when the AI's evac truck gives up on its drive with civilians aboard, the AI should now send it on to a base again later.
- Deploying: when the enemy holds your HQ and your other deploy bases are under attack or offline, you deploy at your own best base instead of inside the enemy's garrison. A new command unit whose HQ can't take it should go to the FOB or base nearest the HQ.
- Orders: a Shift order queued behind Defend, Garrison, a post, a regroup, a vehicle escape or a flank is refused with a reason instead of being accepted and never starting, and with the 2IC on that refusal no longer pulls an AI-run squad off its job. An order queued behind a Hunt now runs when the hunt ends.
- A hunting squad whose leader a player takes over and lets go hunts its target again instead of standing at the old sighting for good.
- Garrisons no longer grow past their set size from survivors of lost bases or FIA technical crews: the men who don't fit should stay as their own group defending that base.
- Supply pallets: a side at the 1000 RP cap can no longer recover a pallet. It stays for the other side, the map label says why, and that side's AI doesn't go for pallets while capped.
- Zombie counterattacks no longer create phantom civilians, and hordes at match start follow this match's time of day.
- Lifting an IED gives its hideout a recruit back only if the team that laid it is still out.
- Loads refunded to a construction truck stay paid, a fortification upgrade whose position was destroyed mid-build is refunded, and a depot technical stuck on its way home no longer keeps its depot busy all match.
- A QRF that dismounts or bails out keeps its plan, and a refused new ride no longer leaves a squad waiting forever.
- The 3D command camera no longer leaves pings and key reminders frozen on screen or drops a stray ping.
- The FOB placement card no longer reveals undiscovered enemy bases or FOB sites, and FOB requests made in quick succession say "too fast" instead of being dropped.
- The Western AT assistant no longer shares a resource ID with the Arland world.
- Mission guide: a ready-made mission header template for map makers.
