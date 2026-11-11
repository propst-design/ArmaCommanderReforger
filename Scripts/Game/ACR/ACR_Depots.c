// FIA depots and technical raids (#96 part 3): pockets on industrial or military sites can be
// depots, a yard with one or two armed FIA light vehicles (a UAZ with a PKM or UK-59; a rare BRDM on
// a military site) parked in it, guarded by the pocket's own garrison.
//  - Raids: when a target is in reach (a moving vehicle of a target side on a road within REACH of
//    the depot, or an FIA cell fighting within REACH), one technical drives out with a crew of two,
//    strikes and drives back. One raid per depot at a time. Vanilla AI drives it on plain move
//    waypoints (BBB looks after ACA squads only, ACR_BBBHooks.GetGroups: the draft's option (a)).
//  - A crew that loses its vehicle walks to the nearest cell or hideout and joins it.
//  - Back in the yard the crew stands down (nothing is out while nothing is near, as every post).
//  - Taking a depot ends its raids; its parked technicals stay where they are for the taker
//    (Jacob's open question 1: the plan suggests yes). Growth around it stops as round any held
//    base (ACR_FIAGrowth.FillRate).
//  - Hideouts buy more technicals (ACR_FIAGrowth.Spend -> Recruit, TECH_COST) up to Cap.
class ACR_Depot : Managed
{
 ACR_Base m_Base;
 ref array<IEntity> m_aVehicles = {};
 IEntity m_Out;              // the technical out on a raid
 SCR_AIGroup m_Crew;
 int m_iState;               // ACR_Depots.IDLE ...
 vector m_vTarget;
 float m_fStateAt;
 float m_fArrivedAt;
 ACR_DefenderPost m_WalkTo;  // on foot: the cell or hideout they join
 vector m_vWalkTo;
 vector m_vYard;
 int m_iNudges;              // re-orders while it sits in the yard (vanilla AI's move sometimes fails: "Failed move")
 float m_fNudgedAt;             // where its first technical was parked: home for every trip
 float m_fEmptySince;        // the technical out stood empty since (its crew got out to fight, or can't drive it)
}

class ACR_Depots
{
 static const int IDLE = 0;
 static const int OUT = 1;
 static const int BACK = 2;
 static const int WALK = 3;
 static const float REACH = 1500;         // targets this close to the depot
 static const float STRIKE_MS = 90000;    // at the target this long, then home
 static const float TRIP_MAX_MS = 360000; // out this long without getting there: home
 static const float ARRIVE = 80;          // m from the target: there
 static const float HOME = 40;            // m from the yard: back
 static const float JOIN = 40;            // m from the cell they walk to: joined
 static const float BRDM_CHANCE = 0.25;   // a military depot's first technical
 static const int TECH_COST = 4;
 static const int CREW = 2;
 static const float EMPTY_MS = 30000;
 static const float NUDGE_MS = 20000;
 static const int NUDGES = 3;
 static float s_fStrikeMsForTest;         // tests shorten the strike (0: STRIKE_MS)
 static ref array<ref ACR_Depot> s_aDepots;
 protected static ref array<ResourceName> s_aTechnicals; // FIA's armed UAZs, from its catalog
 protected static ResourceName s_Brdm;
 protected static int s_iSeconds;

 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_aDepots = {};
  s_aTechnicals = {};
 }

 static void Reset()
 {
  s_aDepots.Clear();
  s_aTechnicals.Clear();
  s_Brdm = "";
  s_iSeconds = 0;
  s_fStrikeMsForTest = 0;
 }

 // Technicals FIA keeps at most, parked or out.
 static int Cap()
 {
  if (ACR_FIAGrowth.Many()) return 4;
  return 2;
 }

 static int Technicals()
 {
  int count;
  foreach (ACR_Depot depot : s_aDepots)
   foreach (IEntity vehicle : depot.m_aVehicles) if (vehicle && ACR_Logistics.Health(vehicle) > 0) count++;
  return count;
 }

 // FIA's armed UAZs and BRDM from its own vehicle catalog (no prefab of ours: a mod that replaces
 // FIA's vehicles leaves FIA without depots).
 protected static void ReadCatalog()
 {
  s_aTechnicals.Clear();
  s_Brdm = "";
  SCR_Faction fia = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(ACR_Pockets.FACTION));
  if (!fia) return;
  SCR_EntityCatalog catalog = fia.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, false);
  if (!catalog) return;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  foreach (SCR_EntityCatalogEntry entry : entries)
  {
   if (!entry.IsEnabled()) continue;
   string leaf = FilePath.StripExtension(FilePath.StripPath(entry.GetPrefab()));
   if (leaf == "UAZ469_PKM_FIA" || leaf == "UAZ469_UK59_FIA") s_aTechnicals.Insert(entry.GetPrefab());
   if (leaf == "BRDM2_FIA") s_Brdm = entry.GetPrefab();
  }
 }

 // Server, after the guerrillas (ACR_Guerrillas.Apply): up to half the cap's depots among the
 // pockets on industrial or military sites, a technical each.
 static void Apply()
 {
  Reset();
  if (ACR_Guerrillas.WantedCells() <= 0) return;
  ReadCatalog();
  if (s_aTechnicals.IsEmpty())
  {
   Print("[ACR] DEPOTS none: no FIA technicals in its catalog");
   return;
  }
  int wanted = Cap() / 2;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (s_aDepots.Count() >= wanted) break;
   if (base.m_bPocket && base.m_iValue >= 2) MakeDepot(base);
  }
  PrintFormat("[ACR] DEPOTS %1 of %2 wanted", s_aDepots.Count(), wanted);
 }

 // A depot at this pocket with one technical parked in its yard (null: no room for it).
 static ACR_Depot MakeDepot(ACR_Base base)
 {
  if (s_aTechnicals.IsEmpty()) ReadCatalog();
  if (s_aTechnicals.IsEmpty()) return null;
  ACR_Depot depot = new ACR_Depot();
  depot.m_Base = base;
  if (!Park(depot, base.m_iValue >= 3 && Math.RandomFloat01() < BRDM_CHANCE)) return null;
  s_aDepots.Insert(depot);
  PrintFormat("[ACR] DEPOTS %1 is an FIA depot (%2)", base.m_sName, ACR_Vehicles.NameOf(depot.m_aVehicles[0]));
  return depot;
 }

 protected static bool Park(ACR_Depot depot, bool brdm)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Requisition) return false;
  ResourceName prefab = s_aTechnicals.GetRandomElement();
  if (brdm && !s_Brdm.IsEmpty()) prefab = s_Brdm;
  vector spot = manager.m_Requisition.VehicleSpot(depot.m_Base, 10, 40, 4);
  if (spot == vector.Zero) return false;
  IEntity vehicle = ACR_Manager.SpawnPrefab(prefab, spot);
  if (!vehicle) return false;
  depot.m_aVehicles.Insert(vehicle);
  if (depot.m_vYard == vector.Zero) depot.m_vYard = spot;
  return true;
 }

 // A hideout's turn to buy (ACR_FIAGrowth.Spend): a second technical for a depot in reach.
 static bool Recruit(ACR_DefenderPost hideout)
 {
  if (hideout.m_Cell.m_fPool < TECH_COST || Technicals() >= Cap()) return false;
  foreach (ACR_Depot depot : s_aDepots)
  {
   if (!Active(depot) || depot.m_aVehicles.Count() >= 2 || vector.DistanceXZ(depot.m_Base.GetOrigin(), hideout.m_vPos) > ACR_Guerrillas.SUPPLY_REACH) continue;
   if (!Park(depot, false)) continue;
   hideout.m_Cell.m_fPool -= TECH_COST;
   PrintFormat("[ACR] DEPOTS %1 buys a technical for %2", hideout.m_sName, depot.m_Base.m_sName);
   return true;
  }
  return false;
 }

 // Still FIA's (taken: no more raids).
 static bool Active(ACR_Depot depot)
 {
  return depot.m_Base && !depot.m_Base.m_bRetired && depot.m_Base.m_bPocket && depot.m_Base.m_sOwner == ACR_Pockets.FACTION;
 }

 // ---------------------------------------------------------------- raids

 // Every second (ACR_Guerrillas.Tick): raids move on; every 5 s an idle depot looks for a target.
 static void Tick()
 {
  if (s_aDepots.IsEmpty()) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases) return;
  s_iSeconds++;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_Depot depot : s_aDepots)
  {
   if (depot.m_iState != IDLE)
   {
    Step(manager, depot, now);
    continue;
   }
   if (s_iSeconds % 5 != 0 || !Active(depot)) continue;
   vector target;
   if (FindTarget(manager, depot, target)) Launch(depot, target);
  }
 }

 // A target side's moving vehicle on a road in reach (a supply run or a lone vehicle), else an FIA
 // cell fighting in reach.
 static bool FindTarget(ACR_Manager manager, ACR_Depot depot, out vector target)
 {
  vector home = depot.m_Base.GetOrigin();
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction() || !ACR_IED.Target(ACR_Pockets.FACTION, squad.GetFaction().GetFactionKey())) continue;
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   if (!vehicle || vector.DistanceXZ(vehicle.GetOrigin(), home) > REACH) continue;
   Physics physics = vehicle.GetPhysics();
   if (!physics || physics.GetVelocity().Length() < ACR_Guerrillas.MOVING || ACR_Minefields.RoadClearance(vehicle.GetOrigin()) > 8) continue;
   target = vehicle.GetOrigin();
   return true;
  }
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell || post.m_Cell.m_bHideout || !post.m_Cell.m_bSprung || post.m_Cell.m_bWithdrawing) continue;
   if (vector.DistanceXZ(post.m_Cell.m_vKillZone, home) > REACH) continue;
   target = post.m_Cell.m_vKillZone;
   return true;
  }
  return false;
 }

 // One parked technical out with a crew of two, driving at the target.
 static bool Launch(ACR_Depot depot, vector target)
 {
  if (depot.m_iState != IDLE || depot.m_Crew) return false;
  ACR_Manager manager = ACR_Manager.GetInstance();
  IEntity vehicle;
  foreach (IEntity parked : depot.m_aVehicles) if (!vehicle && parked && ACR_Logistics.Health(parked) > 0 && !Occupied(parked)) vehicle = parked;
  if (!manager || !vehicle) return false;
  SCR_AIGroup crew = manager.m_Bases.SpawnGuards(ACR_Pockets.FACTION, vehicle.GetOrigin() + Vector(3, 0, 3), CREW, string.Format("FIA technical %1", depot.m_Base.m_sName), 5);
  if (!crew) return false;
  Seat(crew, vehicle);
  depot.m_Out = vehicle;
  depot.m_Crew = crew;
  depot.m_vTarget = target;
  depot.m_iState = OUT;
  depot.m_fStateAt = GetGame().GetWorld().GetWorldTime();
  depot.m_fArrivedAt = 0;
  depot.m_fEmptySince = 0;
  depot.m_iNudges = 0;
  depot.m_fNudgedAt = depot.m_fStateAt;
  ACR_Guerrillas.SetCombatMode(crew, EAIGroupCombatMode.FIRE_AT_WILL);
  ACR_Guerrillas.Order(crew, ACR_Squad.WP_MOVE, target, 30, false);
  PrintFormat("[ACR] DEPOTS %1: a technical drives out for %2 (%3 m)", depot.m_Base.m_sName, ACR_Manager.Grid(target), (int)vector.DistanceXZ(vehicle.GetOrigin(), target));
  return true;
 }

 protected static bool Occupied(IEntity vehicle)
 {
  array<IEntity> crew = {};
  ACR_PendingPurchase.Occupants(vehicle, crew);
  return !crew.IsEmpty();
 }

 // Driver first, then the gun, the rest anywhere (teleported in: they start at the vehicle).
 protected static void Seat(SCR_AIGroup crew, IEntity vehicle)
 {
  SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!seats) return;
  // The group's own vehicle (as ACR_Mortars does a tube): vanilla AI keeps to it on a move order
  // instead of getting out at once.
  SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
  SCR_AIGroupUtilityComponent utility = crew.GetGroupUtilityComponent();
  if (usage && utility && !utility.IsUsableVehicle(usage)) utility.AddUsableVehicle(usage);
  array<BaseCompartmentSlot> order = {};
  array<BaseCompartmentSlot> found = {};
  seats.GetCompartmentsOfType(found, ECompartmentType.PILOT);
  order.InsertAll(found);
  found.Clear();
  seats.GetCompartmentsOfType(found, ECompartmentType.TURRET);
  order.InsertAll(found);
  array<AIAgent> agents = {};
  crew.GetAgents(agents);
  int next;
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || next >= order.Count()) continue;
   CompartmentAccessComponent access = body.GetCompartmentAccessComponent();
   if (access) access.GetInVehicle(vehicle, order[next], true, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   next++;
  }
 }

 protected static void Step(ACR_Manager manager, ACR_Depot depot, float now)
 {
  int alive;
  if (depot.m_Crew) alive = manager.m_Bases.GuardsAlive(depot.m_Crew);
  if (alive <= 0)
  {
   PrintFormat("[ACR] DEPOTS %1: the technical's crew is gone", depot.m_Base.m_sName);
   EndRaid(manager, depot);
   return;
  }
  if (depot.m_iState == WALK)
  {
   if (CrewNear(depot.m_Crew, depot.m_vWalkTo, JOIN)) Join(manager, depot);
   return;
  }
  // The vehicle lost (burnt, or empty for EMPTY_MS: nobody can drive it): they walk to the nearest
  // cell or hideout. A short dismount to fight is not a loss.
  bool lost = !depot.m_Out || ACR_Logistics.Health(depot.m_Out) <= 0;
  if (!lost && !Occupied(depot.m_Out))
  {
   if (depot.m_fEmptySince <= 0) depot.m_fEmptySince = now;
   lost = now - depot.m_fEmptySince >= EMPTY_MS;
  }
  else if (!lost) depot.m_fEmptySince = 0;
  // Out of a sound technical that never left the yard (no way there): they stand down, the
  // technical stays parked.
  if (lost && depot.m_Out && ACR_Logistics.Health(depot.m_Out) > 0 && vector.DistanceXZ(depot.m_Out.GetOrigin(), Yard(depot)) <= HOME)
  {
   PrintFormat("[ACR] DEPOTS %1: the technical never got going, its crew stands down", depot.m_Base.m_sName);
   EndRaid(manager, depot);
   return;
  }
  if (lost)
  {
   Walk(depot);
   return;
  }
  float strike = STRIKE_MS;
  if (s_fStrikeMsForTest > 0) strike = s_fStrikeMsForTest;
  if (depot.m_iState == OUT)
  {
   if (depot.m_iNudges < NUDGES && depot.m_fArrivedAt <= 0 && now - depot.m_fNudgedAt >= NUDGE_MS && vector.DistanceXZ(depot.m_Out.GetOrigin(), Yard(depot)) < 20)
   {
    depot.m_iNudges++;
    depot.m_fNudgedAt = now;
    ACR_Guerrillas.Order(depot.m_Crew, ACR_Squad.WP_MOVE, depot.m_vTarget, 30, false);
    PrintFormat("[ACR] DEPOTS %1: the technical hasn't set off, ordered again", depot.m_Base.m_sName);
   }
   if (depot.m_fArrivedAt <= 0 && vector.DistanceXZ(depot.m_Out.GetOrigin(), depot.m_vTarget) <= ARRIVE)
   {
    depot.m_fArrivedAt = now;
    PrintFormat("[ACR] DEPOTS %1: the technical strikes at %2", depot.m_Base.m_sName, ACR_Manager.Grid(depot.m_vTarget));
   }
   bool done = depot.m_fArrivedAt > 0 && now - depot.m_fArrivedAt >= strike;
   if (done || now - depot.m_fStateAt >= TRIP_MAX_MS) Home(depot, now);
   return;
  }
  // Back: home in the yard, the crew stands down.
  // Stopped first: crew out of a rolling vehicle leaves it to coast off.
  if (vector.DistanceXZ(depot.m_Out.GetOrigin(), Yard(depot)) <= HOME && Speed(depot.m_Out) < 1.0)
  {
   PrintFormat("[ACR] DEPOTS %1: the technical is back", depot.m_Base.m_sName);
   EndRaid(manager, depot);
  }
 }

 protected static float Speed(IEntity vehicle)
 {
  Physics physics = vehicle.GetPhysics();
  if (!physics) return 0;
  return physics.GetVelocity().Length();
 }

 static vector Yard(ACR_Depot depot)
 {
  if (depot.m_vYard != vector.Zero) return depot.m_vYard;
  return depot.m_Base.GetOrigin();
 }

 protected static void Home(ACR_Depot depot, float now)
 {
  depot.m_iState = BACK;
  depot.m_fStateAt = now;
  ACR_Guerrillas.SetCombatMode(depot.m_Crew, EAIGroupCombatMode.RETURN_FIRE);
  ACR_Guerrillas.Order(depot.m_Crew, ACR_Squad.WP_MOVE, Yard(depot), 20, false);
  PrintFormat("[ACR] DEPOTS %1: the technical drives home", depot.m_Base.m_sName);
 }

 // On foot to the nearest standing cell or hideout (none: the depot itself).
 protected static void Walk(ACR_Depot depot)
 {
  vector at = depot.m_Base.GetOrigin();
  IEntity leader;
  if (depot.m_Crew.GetLeaderAgent()) leader = depot.m_Crew.GetLeaderAgent().GetControlledEntity();
  if (leader) at = leader.GetOrigin();
  depot.m_WalkTo = null;
  depot.m_vWalkTo = depot.m_Base.GetOrigin();
  float nearest = vector.DistanceXZ(at, depot.m_vWalkTo);
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell || post.m_bGone || post.m_Group) continue;
   float d = vector.DistanceXZ(post.m_vPos, at);
   if (d >= nearest) continue;
   nearest = d;
   depot.m_WalkTo = post;
   depot.m_vWalkTo = post.m_vPos;
  }
  depot.m_iState = WALK;
  depot.m_Out = null;
  ACR_Guerrillas.SetCombatMode(depot.m_Crew, EAIGroupCombatMode.RETURN_FIRE);
  ACR_Guerrillas.Order(depot.m_Crew, ACR_Squad.WP_MOVE, depot.m_vWalkTo, 15, false);
  string to = depot.m_Base.m_sName;
  if (depot.m_WalkTo) to = depot.m_WalkTo.m_sName;
  PrintFormat("[ACR] DEPOTS %1: the technical is lost, its crew walks to %2 (%3 m)", depot.m_Base.m_sName, to, (int)nearest);
 }

 // Arrived on foot: into the cell's reserve (or the pocket's garrison).
 protected static void Join(ACR_Manager manager, ACR_Depot depot)
 {
  int back = manager.m_Bases.PackUpGuards(depot.m_Crew);
  if (depot.m_WalkTo && !depot.m_WalkTo.m_bGone) depot.m_WalkTo.m_iReserve += back;
  else if (Active(depot)) depot.m_Base.m_iGarrisonLeft += back;
  string to = depot.m_Base.m_sName;
  if (depot.m_WalkTo) to = depot.m_WalkTo.m_sName;
  PrintFormat("[ACR] DEPOTS %1: %2 of the technical's crew joined %3", depot.m_Base.m_sName, back, to);
  depot.m_Crew = null;
  depot.m_iState = IDLE;
 }

 protected static bool CrewNear(SCR_AIGroup crew, vector pos, float range)
 {
  array<AIAgent> agents = {};
  crew.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Respawn.IsAlive(body) && vector.DistanceXZ(body.GetOrigin(), pos) <= range) return true;
  }
  return false;
 }

 // The raid is over: the crew stands down (out of the vehicle and gone), the technical stays.
 protected static void EndRaid(ACR_Manager manager, ACR_Depot depot)
 {
  if (depot.m_Crew)
  {
   array<AIAgent> agents = {};
   depot.m_Crew.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
    if (body && body.IsInVehicle() && body.GetCompartmentAccessComponent()) body.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   }
   manager.m_Bases.PackUpGuards(depot.m_Crew);
  }
  if (depot.m_Out && depot.m_Out.GetPhysics())
  {
   depot.m_Out.GetPhysics().SetVelocity(vector.Zero);
   depot.m_Out.GetPhysics().SetAngularVelocity(vector.Zero);
  }
  depot.m_Crew = null;
  depot.m_Out = null;
  depot.m_iState = IDLE;
 }
}
