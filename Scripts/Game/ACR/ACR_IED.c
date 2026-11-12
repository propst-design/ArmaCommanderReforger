// Wired IEDs (#96 part 2): a real charge on a busy road and a real man who fires it. Not a mine:
// it goes off only when an enemy military vehicle is over it, so friendly traffic, civilians and
// the owner's allies pass safely.
//  - A hideout buys a two-man team (ACR_FIAGrowth.Spend -> Recruit, IED_COST recruits) while it is
//    under the cap. They bury a vanilla charge (ACR_Demolition.ChargeFor) on the busiest stretch of
//    road in reach (ACR_Guerrillas' traffic counts, which already weigh the sides FIA hates double
//    and leave out the ones it spares), out of sight of everyone (no enemy within PLANT_CLEAR), and
//    wire it to a blasting machine. The triggerman's place is TRIGGER_NEAR-TRIGGER_FAR m off the
//    road, in the best cover that sees the charge.
//  - The team is a defender post (ACR_DefenderPost.m_IED): nobody is there until an enemy comes
//    within RANGE; then they lie at the triggerman's place, holding their fire, the detonator in
//    his hand. Every WATCH_MS while out, a hostile military vehicle about to pass within TRIGGER_RADIUS
//    of the charge sets it off (the charge's own trigger with him as its user, and a heavy blast).
//  - Counterplay: shoot the triggerman (he has to watch the road, so he can be seen); a team that
//    takes a hit fires early if a target is close, else runs and leaves the charge. Reach the charge
//    and use the game's Disarm action; an AI soldier of a side sweeping that road (a danger mark
//    there) disarms one he stands at for DISARM_MS.
//  - Never on any map. A blast marks the road dangerous for the side hit (ACR_Insurgency.OnLoss),
//    so its AI sends a sweep squad. A charge still there after LIFT_MS with nobody near is lifted.
// Owner-agnostic for #97 (a playable FIA side planting by order): Plant takes the owner faction and
// Target decides who it is for (FIA: anyone it isn't friendly with or sparing; another side: the
// game's hostile factions).
class ACR_IED : Managed
{
 static const int IED_COST = 2;              // recruits (a two-man team)
 static const float IED_REACH = 3000;        // m from the hideout to the road it mines
 static const float SPACING = 600;           // m between two charges
 static const float PLANT_CLEAR = 700;       // no enemy this close while they plant
 static const float TRIGGER_NEAR = 80;       // m off the road the triggerman lies ...
 static const float TRIGGER_FAR = 200;       // ... to this far
 static const float RANGE = 700;             // an enemy this close brings the team out (vehicles come fast)
 static const float TRIGGER_RADIUS = 4.5;    // a vehicle's middle this close to the charge sets it off
 static const float WATCH_REACH = 80;        // vehicles this close are watched
 static const int WATCH_MS = 250;
 static const float EARLY_REACH = 40;        // shot at: fires at a target this close, else runs
 static const float RUN = 300;
 static const float DISARM_REACH = 2.5;
 static const float DISARM_MS = 5000;
 static const float LIFT_MS = 2400000;       // 40 min
 static float s_fLiftMs;                     // tests shorten it (0: LIFT_MS)

 string m_sOwner;
 vector m_vPos;              // the charge
 vector m_vTrigger;          // the triggerman's place
 IEntity m_Charge;
 IEntity m_Detonator;
 ACR_DefenderPost m_Post;    // the team
 ACR_DefenderPost m_From;    // the hideout that sent them (null: planted by order)
 float m_fPlacedAt;
 bool m_bFired;
 bool m_bDisarmed;
 bool m_bRan;                // the team ran: nobody will fire it
 bool m_bWired;              // connected to the blasting machine (else fired by script only)
 bool m_bLead;               // the team's loss has given its hideout away once (one lead per team)
 int m_iOut;                 // men out at deploy (a loss: shot at)
 float m_fDisarmMs;
 string m_sDisarmSide;

 static ref array<ref ACR_IED> s_aIEDs;
 protected static bool s_bWatching;

 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_aIEDs = {};
 }

 // Per match (ACR_Manager.Stop): charges went with the world.
 static void Reset()
 {
  if (s_bWatching) GetGame().GetCallqueue().Remove(Watch);
  s_bWatching = false;
  s_aIEDs.Clear();
  s_fLiftMs = 0;
 }

 static int Cap()
 {
  if (ACR_FIAGrowth.Many()) return 4;
  return 2;
 }

 // Charges standing (not fired, disarmed or lifted).
 static int Live()
 {
  int live;
  foreach (ACR_IED ied : s_aIEDs) if (!ied.m_bFired && !ied.m_bDisarmed) live++;
  return live;
 }

 // Is a vehicle of 'side' a target for a charge of 'owner'?
 static bool Target(string owner, string side)
 {
  if (side.IsEmpty() || side == owner) return false;
  if (side != ACR_FactionSelection.West() && side != ACR_FactionSelection.East() && side != ACR_Pockets.FACTION) return false;
  if (owner == ACR_Pockets.FACTION) return !ACR_Insurgency.Friendly(owner, side) && !ACR_Insurgency.Spared(side);
  if (side == ACR_Pockets.FACTION) return !ACR_Insurgency.Friendly(owner, side);
  FactionManager factions = GetGame().GetFactionManager();
  if (!factions) return false;
  SCR_Faction mine = SCR_Faction.Cast(factions.GetFactionByKey(owner));
  Faction theirs = factions.GetFactionByKey(side);
  return mine != null && theirs != null && !mine.IsFactionFriendly(theirs);
 }

 // ---------------------------------------------------------------- planting

 // A hideout's turn to buy (ACR_FIAGrowth.Spend): a team and a charge on a busy road in reach.
 static bool Recruit(ACR_DefenderPost hideout)
 {
  if (hideout.m_Cell.m_fPool < IED_COST || Live() >= Cap()) return false;
  // FIA spares both sides (friends, or neutral with leaning): no one to mine the roads for.
  if (!Target(ACR_Pockets.FACTION, ACR_FactionSelection.West()) && !Target(ACR_Pockets.FACTION, ACR_FactionSelection.East())) return false;
  vector road;
  if (!PickRoad(hideout.m_vPos, road)) return false;
  ACR_IED ied = Plant(road, ACR_Pockets.FACTION, hideout);
  if (!ied) return false;
  hideout.m_Cell.m_fPool -= IED_COST;
  return true;
 }

 // The busiest watched stretch in reach (else a surveyed road site), usable as an ambush site
 // would be, apart from other charges, nobody of a target side near.
 protected static bool PickRoad(vector from, out vector road)
 {
  float best = -1;
  foreach (int key, float unused : ACR_Guerrillas.s_mTraffic)
  {
   vector at = ACR_Guerrillas.s_mTrafficAt.Get(key);
   if (vector.DistanceXZ(at, from) > IED_REACH) continue;
   float busy = ACR_Guerrillas.TrafficNear(at);
   if (busy < ACR_Guerrillas.RESITE_MIN || busy <= best || !RoadOk(at, ACR_Pockets.FACTION)) continue;
   best = busy;
   road = at;
  }
  if (best >= 0) return true;
  array<vector> sites = {};
  foreach (vector site : ACR_Guerrillas.s_aRoads) if (vector.DistanceXZ(site, from) <= IED_REACH) sites.Insert(site);
  for (int tries = 0; tries < 6 && !sites.IsEmpty(); tries++)
  {
   int pick = Math.RandomInt(0, sites.Count());
   if (RoadOk(sites[pick], ACR_Pockets.FACTION))
   {
    road = sites[pick];
    return true;
   }
   sites.Remove(pick);
  }
  return false;
 }

 protected static bool RoadOk(vector at, string owner)
 {
  if (!ACR_Guerrillas.UsableAmbush(at)) return false;
  foreach (ACR_IED ied : s_aIEDs) if (!ied.m_bFired && !ied.m_bDisarmed && vector.DistanceXZ(ied.m_vPos, at) < SPACING) return false;
  return !HostileNear(owner, at, PLANT_CLEAR);
 }

 // A living soldier of a side the owner's charges are for within range.
 static bool HostileNear(string owner, vector pos, float range)
 {
  array<string> sides = {};
  ACR_Insurgency.SidesNear(pos, range, sides);
  foreach (string side : sides) if (Target(owner, side)) return true;
  return false;
 }

 // A charge on the road at 'near' for 'owner', wired, with the triggerman's place found and his
 // team as a post (null: no road, or nowhere for him to lie). 'from': the hideout that sent them.
 static ACR_IED Plant(vector near, string owner, ACR_DefenderPost from)
 {
  vector road, dir;
  float width = 6;
  if (!ACR_Minefields.Snap(near, 40, road, dir, width)) return null;
  vector trigger;
  if (!TriggerSite(road, dir, width, trigger)) return null;
  // Off the middle, in a wheel track.
  vector side = Vector(dir[2], 0, -dir[0]);
  vector spot = ACR_Manager.Ground(road + side * (width * 0.2));
  IEntity charge = ACR_Manager.SpawnPrefab(ACR_Demolition.ChargeFor(owner), spot);
  if (!charge) return null;
  ACR_IED ied = new ACR_IED();
  ied.m_sOwner = owner;
  ied.m_vPos = spot;
  ied.m_vTrigger = trigger;
  ied.m_Charge = charge;
  ied.m_From = from;
  ied.m_fPlacedAt = GetGame().GetWorld().GetWorldTime();
  // Wired now, so the game's Disarm works on it from the start, but the blasting machine isn't
  // left lying at the triggerman's place for anyone to pick up and fire: it goes, and each time
  // the team comes out a new one is wired into his hands (HandDetonator). The charge keeps its
  // REMOTE fuze when a detonator is deleted (vanilla only unwires through Disconnect or Disarm).
  IEntity wiring = ACR_Manager.SpawnPrefab(ACR_Demolition.DetonatorFor(owner), trigger);
  ied.m_bWired = Wire(charge, wiring);
  if (wiring) SCR_EntityHelper.DeleteEntityAndChildren(wiring);
  string name = string.Format("IED team %1", ACR_Manager.Grid(spot));
  ACR_DefenderPost post = ACR_Defenders.Add(owner, trigger, name, 8);
  post.m_iReserve = 2;
  post.m_fSpawnRange = RANGE;
  post.m_IED = ied;
  ied.m_Post = post;
  s_aIEDs.Insert(ied);
  string fromName = "an order";
  if (from) fromName = from.m_sName;
  PrintFormat("[ACR] IED %1 planted at %2 by %3 (wired %4, triggerman %5 m off the road, %6 of %7 live)", owner, ACR_Manager.Grid(spot), fromName, ied.m_bWired, (int)ACR_Minefields.RoadClearance(trigger), Live(), Cap());
  return ied;
 }

 // The charge connected to the blasting machine (as the game's Arm action does): armed, so the
 // game's Disarm action works on it. False: not wired (fired by script, can't be disarmed).
 protected static bool Wire(IEntity charge, IEntity detonatorEntity)
 {
  SCR_ExplosiveChargeComponent chargeComp = ACR_SapperDemo.ChargeComponent(charge);
  if (!chargeComp || !detonatorEntity) return false;
  SCR_DetonatorGadgetComponent detonator = SCR_DetonatorGadgetComponent.Cast(detonatorEntity.FindComponent(SCR_DetonatorGadgetComponent));
  if (!detonator) return false;
  RplId detonatorId = Replication.FindItemId(detonator);
  if (!detonatorId.IsValid() || !chargeComp.GetRplId().IsValid()) return false;
  detonator.ConnectNewCharge(chargeComp.GetRplId());
  chargeComp.ConnectDetonator(SCR_EFuzeType.REMOTE, detonatorId, true, true); // silent: no wiring click at the charge when the team comes out
  return chargeComp.GetUsedFuzeType() != SCR_EFuzeType.NONE;
 }

 // TRIGGER_NEAR-TRIGGER_FAR m off the road, on either side, seeing the charge, in the most cover
 // (ACR_Ambush's cover and sight rules, as an ambush's hiding place); false with none.
 static bool TriggerSite(vector road, vector dir, float width, out vector best)
 {
  vector side = Vector(dir[2], 0, -dir[0]);
  float bestScore = -1000;
  bool found;
  array<float> sides = {1, -1};
  array<float> offs = {80, 100, 125, 150, 175, 200};
  array<float> alongs = {0, -30, 30, -60, 60};
  foreach (float s : sides)
  {
   foreach (float o : offs)
   {
    foreach (float a : alongs)
    {
     vector candidate = ACR_Manager.Ground(road + side * (s * (width * 0.5 + o)) + dir * a);
     if (BBB_Terrain.IsWaterAt(candidate) || ACR_Minefields.RoadClearance(candidate) < ACR_Guerrillas.HIDE_CLEAR) continue;
     if (ACR_BaseSystem.Covered(candidate) || !ACR_Ambush.SeesKillZone(candidate, road)) continue;
     float score = ACR_Ambush.Cover(candidate) * ACR_Guerrillas.COVER_WEIGHT + ACR_Ambush.Height(candidate, road) - (o - TRIGGER_NEAR) * 0.01;
     if (score <= bestScore) continue;
     bestScore = score;
     best = candidate;
     found = true;
    }
   }
  }
  return found;
 }

 // ---------------------------------------------------------------- ACR_Defenders hooks

 // May the team come out? (Not once fired, disarmed or run.)
 static bool Ready(ACR_DefenderPost post)
 {
  ACR_IED ied = post.m_IED;
  return !ied.m_bFired && !ied.m_bDisarmed && !ied.m_bRan;
 }

 // Just out: both men lying at the triggerman's place facing the charge, holding their fire, the
 // blasting machine with the triggerman (the group's leader).
 static void Deployed(ACR_Manager manager, ACR_DefenderPost post)
 {
  ACR_IED ied = post.m_IED;
  SCR_AIGroup group = post.m_Group;
  ied.m_iOut = manager.m_Bases.GuardsAlive(group);
  ACR_Guerrillas.SetCombatMode(group, EAIGroupCombatMode.HOLD_FIRE);
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  array<ref ACR_GarrisonSpot> held = {};
  vector look = ied.m_vPos - ied.m_vTrigger;
  look[1] = 0;
  if (look.LengthSq() > 0.01) look.Normalize();
  vector across = Vector(look[2], 0, -look[0]);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   vector spot = ACR_Manager.Ground(ied.m_vTrigger + across * (i * 4.0));
   body.SetOrigin(spot);
   ACR_GarrisonSpot place = new ACR_GarrisonSpot();
   place.m_Pos = spot;
   place.m_Look = look;
   held.Insert(place);
  }
  float spread = 8;
  if (!held.IsEmpty() && ACR_Garrison.PostsAt(group, held, ied.m_vTrigger, spread) > 0) ACR_Guerrillas.Order(group, ACR_Squad.WP_GARRISON, ied.m_vTrigger, 8, true);
  else ACR_Guerrillas.Order(group, ACR_Squad.WP_DEFEND, ied.m_vTrigger, 6, false);
  HandDetonator(ied, Triggerman(post));
  StartWatching();
  PrintFormat("[ACR] IED %1: %2 men lying %3 m from the charge, watching", post.m_sName, ied.m_iOut, (int)vector.DistanceXZ(ied.m_vTrigger, ied.m_vPos));
 }

 // The man who fires it: the group's leader, alive (null: none).
 static IEntity Triggerman(ACR_DefenderPost post)
 {
  if (!post || !post.m_Group || !post.m_Group.GetLeaderAgent()) return null;
  IEntity body = post.m_Group.GetLeaderAgent().GetControlledEntity();
  if (!body || !ACR_Respawn.IsAlive(body)) return null;
  ChimeraCharacter character = ChimeraCharacter.Cast(body);
  if (character && character.GetCharacterController() && character.GetCharacterController().IsUnconscious()) return null;
  return body;
 }

 // A blasting machine wired to the charge, straight into his hands. It goes with him when the
 // team packs up (PackUpGuards deletes the men with what they carry); a dead triggerman's can be
 // looted. Never left on the ground: if it doesn't fit, it goes (Fire works by script anyway).
 protected static void HandDetonator(ACR_IED ied, IEntity man)
 {
  if (!man || !ied.m_Charge || (ied.m_Detonator && ied.m_Detonator.GetParent())) return;
  if (ied.m_Detonator) SCR_EntityHelper.DeleteEntityAndChildren(ied.m_Detonator);
  ied.m_Detonator = ACR_Manager.SpawnPrefab(ACR_Demolition.DetonatorFor(ied.m_sOwner), man.GetOrigin());
  if (!ied.m_Detonator) return;
  if (Wire(ied.m_Charge, ied.m_Detonator)) ied.m_bWired = true;
  SCR_InventoryStorageManagerComponent inventory = ACR_SapperDemo.InventoryOf(man);
  if (!inventory || !inventory.TryInsertItem(ied.m_Detonator, EStoragePurpose.PURPOSE_ANY, null))
  {
   SCR_EntityHelper.DeleteEntityAndChildren(ied.m_Detonator);
   ied.m_Detonator = null;
   return;
  }
  SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(man);
  if (gadgets) gadgets.SetGadgetMode(ied.m_Detonator, EGadgetMode.IN_HAND);
 }

 // Out while a target side is near the charge or the team; after running, until clear of them.
 static bool StayOut(ACR_Manager manager, ACR_DefenderPost post)
 {
  ACR_IED ied = post.m_IED;
  if (HostileNear(ied.m_sOwner, ied.m_vPos, RANGE + 50)) return true;
  return HostileNear(ied.m_sOwner, post.m_vPos, ACR_Guerrillas.CLEAR);
 }

 // Packed up: a team whose job is over (fired, disarmed, ran) goes home, a man back in its
 // hideout's pool; one still watching waits for the next enemy.
 static void PackedUp(ACR_DefenderPost post)
 {
  ACR_IED ied = post.m_IED;
  if (post.m_iReserve <= 0)
  {
   PrintFormat("[ACR] IED %1: the team is dead", post.m_sName);
   Lead(ied, post.m_vPos, "a dead IED team");
   ACR_Defenders.Remove(post);
   return;
  }
  if (Ready(post)) return;
  if (ied.m_From && !ied.m_From.m_bGone) ied.m_From.m_Cell.m_fPool = Math.Min(ACR_FIAGrowth.PoolCap(), ied.m_From.m_Cell.m_fPool + 1);
  PrintFormat("[ACR] IED %1: the team goes home", post.m_sName);
  ACR_Defenders.Remove(post);
 }

 // The team's loss gives its hideout away: once per team (a dead triggerman, then the whole
 // team dead, used to tighten the ring twice).
 protected static void Lead(ACR_IED ied, vector at, string what)
 {
  if (ied.m_bLead) return;
  ied.m_bLead = true;
  ACR_FIAIntel.Wiped(at, ied.m_From, what);
 }

 // The group's leader is gone or dead (unconscious: no).
 protected static bool LeaderDead(ACR_DefenderPost post)
 {
  if (!post || !post.m_Group || !post.m_Group.GetLeaderAgent()) return true;
  IEntity body = post.m_Group.GetLeaderAgent().GetControlledEntity();
  return !body || !ACR_Respawn.IsAlive(body);
 }

 // ---------------------------------------------------------------- watching the road

 protected static void StartWatching()
 {
  if (s_bWatching) return;
  s_bWatching = true;
  GetGame().GetCallqueue().CallLater(Watch, WATCH_MS, true);
 }

 // Every WATCH_MS while a team is out: a target vehicle about to pass over its charge sets it off;
 // a team that took a hit fires early or runs; AI sweepers disarm. Stops with no team out.
 static void Watch()
 {
  bool any;
  foreach (ACR_IED ied : s_aIEDs)
  {
   if (!ied.m_Post || !ied.m_Post.m_Group || !Ready(ied.m_Post)) continue;
   any = true;
   if (Defused(ied)) continue;
   IEntity man = Triggerman(ied.m_Post);
   ACR_Manager manager = ACR_Manager.GetInstance();
   int alive;
   if (manager) alive = manager.m_Bases.GuardsAlive(ied.m_Post.m_Group);
   bool hit = alive < ied.m_iOut || !man;
   float closest;
   IEntity vehicle = NextVehicle(ied, closest);
   if (hit)
   {
    // Dead, not just knocked out (Triggerman is null for both).
    if (!man && LeaderDead(ied.m_Post)) Lead(ied, ied.m_vTrigger, "a dead triggerman");
    if (man && vehicle && closest < EARLY_REACH) Fire(ied, man, vehicle, "early: the team was shot at");
    else Run(ied, "shot at before a target came");
    continue;
   }
   if (vehicle && closest < TRIGGER_RADIUS) Fire(ied, man, vehicle, "a target over it");
  }
  if (any) return;
  GetGame().GetCallqueue().Remove(Watch);
  s_bWatching = false;
 }

 // The target vehicle that will come closest to the charge within the next watch (its middle's
 // nearest pass over the next WATCH_MS, from its speed); 'closest' that pass. Null: none near.
 static IEntity NextVehicle(ACR_IED ied, out float closest)
 {
  closest = 100000;
  IEntity best;
  array<IEntity> vehicles = {};
  VehiclesNear(ied.m_vPos, WATCH_REACH, vehicles);
  foreach (IEntity vehicle : vehicles)
  {
   if (!TargetVehicle(ied.m_sOwner, vehicle)) continue;
   vector rel = ied.m_vPos - vehicle.GetOrigin();
   rel[1] = 0;
   float pass = rel.Length();
   Physics physics = vehicle.GetPhysics();
   if (physics)
   {
    vector v = physics.GetVelocity();
    v[1] = 0;
    float speedSq = v.LengthSq();
    if (speedSq > 0.25)
    {
     float t = Math.Clamp(vector.Dot(rel, v) / speedSq, 0, WATCH_MS / 1000.0);
     vector miss = rel - v * t;
     pass = miss.Length();
    }
   }
   if (pass >= closest) continue;
   closest = pass;
   best = vehicle;
  }
  return best;
 }

 // A vehicle with a living soldier of a target side aboard (a civilian car, an empty one or the
 // owner's own: no).
 static bool TargetVehicle(string owner, IEntity vehicle)
 {
  array<IEntity> crew = {};
  ACR_PendingPurchase.Occupants(vehicle, crew);
  foreach (IEntity man : crew)
  {
   if (!ACR_Respawn.IsAlive(man)) continue;
   if (Target(owner, ACR_Intel.FactionKeyOf(man))) return true;
  }
  return false;
 }

 protected static ref array<IEntity> s_aFound;
 static void VehiclesNear(vector pos, float range, notnull array<IEntity> result)
 {
  s_aFound = result;
  GetGame().GetWorld().QueryEntitiesBySphere(pos, range, AddVehicle, null, EQueryEntitiesFlags.DYNAMIC);
  s_aFound = null;
 }

 protected static bool AddVehicle(IEntity e)
 {
  if (Vehicle.Cast(e) && !s_aFound.Contains(e)) s_aFound.Insert(e);
  return true;
 }

 // Off: the charge's own trigger (him as its user, so the losses are his) and a heavy blast
 // (a road charge, not a 400 g block: ACR_Supports' bomb with its scripted lethality). The sides
 // aboard mark the road dangerous; the team runs.
 static void Fire(ACR_IED ied, IEntity man, IEntity vehicle, string why)
 {
  if (ied.m_bFired) return;
  ied.m_bFired = true;
  vector at = ied.m_vPos;
  array<string> hit = {};
  array<IEntity> crew = {};
  if (vehicle) ACR_PendingPurchase.Occupants(vehicle, crew);
  foreach (IEntity aboard : crew)
  {
   string side = ACR_Intel.FactionKeyOf(aboard);
   if (!side.IsEmpty() && !hit.Contains(side)) hit.Insert(side);
  }
  SCR_ExplosiveChargeComponent chargeComp = ACR_SapperDemo.ChargeComponent(ied.m_Charge);
  if (chargeComp && chargeComp.GetTrigger())
  {
   if (man) chargeComp.GetTrigger().SetUser(man);
   chargeComp.GetTrigger().UseTrigger();
  }
  else if (ied.m_Charge) SCR_EntityHelper.DeleteEntityAndChildren(ied.m_Charge);
  ied.m_Charge = null;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager)
  {
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   manager.m_Supports.ExplodeAt(at);
  }
  string vehicleName = "nothing";
  if (vehicle) vehicleName = ACR_Vehicles.NameOf(vehicle);
  PrintFormat("[ACR] IED %1 went off at %2 under %3 (%4)", ied.m_sOwner, ACR_Manager.Grid(at), vehicleName, why);
  foreach (string struck : hit) ACR_Insurgency.OnLoss(struck, at, ied.m_sOwner);
  Withdraw(ied);
 }

 // Shot at with no target close: they run, the charge stays (wired, but nobody will fire it).
 static void Run(ACR_IED ied, string why)
 {
  if (ied.m_bRan) return;
  ied.m_bRan = true;
  PrintFormat("[ACR] IED %1: the team runs (%2), the charge stays", ied.m_Post.m_sName, why);
  Withdraw(ied);
 }

 protected static void Withdraw(ACR_IED ied)
 {
  ACR_DefenderPost post = ied.m_Post;
  if (!post || !post.m_Group) return;
  vector away = post.m_vPos - ied.m_vPos;
  away[1] = 0;
  if (away.LengthSq() < 0.01) away = Vector(1, 0, 0);
  away.Normalize();
  ACR_Garrison.Release(post.m_Group);
  ACR_Guerrillas.SetCombatMode(post.m_Group, EAIGroupCombatMode.RETURN_FIRE);
  ACR_Guerrillas.Order(post.m_Group, ACR_Squad.WP_MOVE, ACR_Manager.Ground(post.m_vPos + away * RUN), 20, false);
 }

 // Disarmed (the game's Disarm action leaves the charge unfused) or an AI sweeper of a target side
 // stood at it long enough: the charge goes, the side hears, the team runs.
 protected static bool Defused(ACR_IED ied)
 {
  if (ied.m_bFired || ied.m_bDisarmed) return true;
  if (!ied.m_Charge)
  {
   Disarmed(ied, "", "the charge is gone");
   return true;
  }
  // Picked up (an unwired charge isn't locked) or carried off and dropped: it's whoever took it
  // now, never fired by script. Not deleted: it may be in a player's pack. (15 m: a wheel
  // nudging it isn't taking it.)
  if (ied.m_Charge.GetParent() || vector.DistanceXZ(ied.m_Charge.GetOrigin(), ied.m_vPos) > 15)
  {
   string taker = NearestSide(ied);
   ied.m_Charge = null;
   Disarmed(ied, taker, "taken");
   return true;
  }
  SCR_ExplosiveChargeComponent chargeComp = ACR_SapperDemo.ChargeComponent(ied.m_Charge);
  if (ied.m_bWired && chargeComp && chargeComp.GetUsedFuzeType() == SCR_EFuzeType.NONE)
  {
   Disarmed(ied, NearestSide(ied), "disarmed");
   return true;
  }
  return false;
 }

 // The target side with a man nearest the charge ("" none within 30 m).
 protected static string NearestSide(ACR_IED ied)
 {
  array<string> sides = {};
  ACR_Insurgency.SidesNear(ied.m_vPos, 30, sides);
  foreach (string side : sides) if (Target(ied.m_sOwner, side)) return side;
  return "";
 }

 static void Disarmed(ACR_IED ied, string side, string why)
 {
  if (ied.m_bDisarmed || ied.m_bFired) return;
  ied.m_bDisarmed = true;
  if (ied.m_Charge) SCR_EntityHelper.DeleteEntityAndChildren(ied.m_Charge);
  ied.m_Charge = null;
  PrintFormat("[ACR] IED at %1 %2 (%3)", ACR_Manager.Grid(ied.m_vPos), why, side);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && !side.IsEmpty() && manager.IntelFor(side)) manager.IntelFor(side).AddFeed(GetGame().GetWorld().GetWorldTime(), string.Format("IED disarmed at %1", ACR_Manager.Grid(ied.m_vPos)));
  Withdraw(ied);
 }

 // Every second (ACR_Guerrillas.Tick): AI sweepers at a charge disarm it; old charges are lifted.
 static void Tick()
 {
  if (s_aIEDs.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  float lift = LIFT_MS;
  if (s_fLiftMs > 0) lift = s_fLiftMs;
  for (int i = s_aIEDs.Count() - 1; i >= 0; i--)
  {
   ACR_IED ied = s_aIEDs[i];
   if (ied.m_bFired || ied.m_bDisarmed)
   {
    if (!ied.m_Post || ied.m_Post.m_bGone) Forget(i);
    continue;
   }
   if (Defused(ied)) continue;
   SweepDisarm(ied, 1000);
   if (ied.m_bDisarmed) continue;
   if (now - ied.m_fPlacedAt < lift || (ied.m_Post && ied.m_Post.m_Group) || HostileNear(ied.m_sOwner, ied.m_vPos, PLANT_CLEAR)) continue;
   PrintFormat("[ACR] IED at %1 lifted after %2 min", ACR_Manager.Grid(ied.m_vPos), (int)((now - ied.m_fPlacedAt) / 60000));
   if (ied.m_Charge) SCR_EntityHelper.DeleteEntityAndChildren(ied.m_Charge);
   ied.m_Charge = null;
   ied.m_bDisarmed = true;
   if (ied.m_From && !ied.m_From.m_bGone) ied.m_From.m_Cell.m_fPool = Math.Min(ACR_FIAGrowth.PoolCap(), ied.m_From.m_Cell.m_fPool + 1);
   if (ied.m_Post && !ied.m_Post.m_Group) ACR_Defenders.Remove(ied.m_Post);
  }
 }

 protected static void Forget(int i)
 {
  ACR_IED ied = s_aIEDs[i];
  if (ied.m_Detonator && !ied.m_Detonator.GetParent()) SCR_EntityHelper.DeleteEntityAndChildren(ied.m_Detonator);
  s_aIEDs.RemoveOrdered(i);
 }

 // An AI soldier on foot of a side sweeping that road (a danger mark over the charge) standing at
 // it: after DISARM_MS he has disarmed it. Players use the game's Disarm action.
 // Known design gap (review 0.5.13): the danger mark only comes from a blast (ACR_Insurgency.OnLoss),
 // so AI sweepers almost never find a live charge; in practice LIFT_MS clears the rest. Not built.
 static void SweepDisarm(ACR_IED ied, float elapsedMs)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  string at = "";
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction()) continue;
   string side = squad.GetFaction().GetFactionKey();
   if (!Target(ied.m_sOwner, side) || !Sweeping(side, ied.m_vPos)) continue;
   IEntity anchor = squad.GetAnchor();
   if (!anchor || vector.DistanceXZ(anchor.GetOrigin(), ied.m_vPos) > 300) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
    if (!body || !ACR_Respawn.IsAlive(body) || body.IsInVehicle() || ACR_SapperDemo.IsPlayerBody(body)) continue;
    if (vector.DistanceXZ(body.GetOrigin(), ied.m_vPos) <= DISARM_REACH) at = side;
   }
  }
  if (at.IsEmpty() || at != ied.m_sDisarmSide)
  {
   ied.m_fDisarmMs = 0;
   ied.m_sDisarmSide = at;
   if (at.IsEmpty()) return;
  }
  ied.m_fDisarmMs += elapsedMs;
  if (ied.m_fDisarmMs >= DISARM_MS) Disarmed(ied, at, "disarmed by a sweep");
 }

 // A danger mark of 'side' over 'pos' (its AI sweeps there: ACR_AICommander.ManageDangerRoads).
 protected static bool Sweeping(string side, vector pos)
 {
  foreach (ACR_DangerRoad road : ACR_Insurgency.s_aRoads)
   if (road.m_sSide == side && !road.m_bZombies && vector.DistanceXZ(road.m_vPos, pos) <= ACR_Insurgency.Radius(road)) return true;
  return false;
 }

 // The hideout's teams: their charges (ACR_FIAGrowth, tests).
 static int TeamsOf(ACR_DefenderPost hideout)
 {
  int teams;
  foreach (ACR_IED ied : s_aIEDs) if (ied.m_From == hideout && !ied.m_bFired && !ied.m_bDisarmed) teams++;
  return teams;
 }
}
