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
//  - Stolen vehicles (insurgency part 4, ACR_FIASpoils): a spoils team that reaches an empty, sound
//    vehicle of a side FIA fights drives it to the nearest idle depot, or to the stash of the
//    hideout feeding the area (a depot kept at a hideout, made on first use), and joins there. It
//    is FIA's from then on and raids like a technical (an unarmed one too: it carries two men to
//    a fight). Stolen vehicles may take FIA STOLEN_EXTRA over Cap; a depot keeps YARD_MAX.
class ACR_Depot : Managed
{
 ACR_Base m_Base;            // a pocket's depot ...
 ACR_DefenderPost m_Hideout; // ... or a hideout's stash (stolen vehicles, insurgency part 4)
 bool m_bTheft;              // the trip out is a stolen vehicle driven home: its crew joins there
 ref array<IEntity> m_aVehicles = {};
 ref array<vector> m_aSpots = {};   // where each was parked (m_aVehicles' order)
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
 float m_fRestUntil;         // a raid that never got there: no new one before this
 int m_iRetargets;           // new targets this raid took after one it never got going to
 ref array<vector> m_aBadTargets = {}; // targets no technical drives to (ACR_Depots.Drivable) ...
 ref array<float> m_aBadUntil = {};    // ... skipped until this time
 vector m_vBackPos;                    // where the technical was when BACK movement was last seen
 float m_fBackMovedAt;                 // when it last moved BACK_MOVE_M on the drive home
 float m_fBackBestGap;                 // nearest it has been to the yard on the drive home
 float m_fBackProgressAt;              // when it last got BACK_MOVE_M nearer the yard
 int m_iBackNudges;                    // re-orders while stalled on the drive home
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
 static const float INTACT = 0.5;        // a vehicle this sound or better is worth stealing
 static const float PLAYER_CLEAR = 300;   // never one with a player this close (his, or one he may come back to)
 static const int STOLEN_EXTRA = 2;       // stolen vehicles may take FIA this far over Cap
 static const float STEAL_REACH = 4000;   // m to a depot or stash a stolen vehicle is driven to
 static const int YARD_MAX = 3;           // vehicles a depot or stash keeps at most
 static const float STASH_OFFSET = 25;    // m from a hideout's middle its stash parks
 static const float WITNESS_REACH = 500;  // the side hears of a theft with men this close
 static const float PARK_NEAR = 200;      // a stolen vehicle this close to its yard is parked where it stands (vanilla AI won't drive so short a way)
 static const float EMPTY_MS = 30000;
 static const float NUDGE_MS = 20000;
 static const int NUDGES = 3;      // a stolen vehicle's drive home: re-orders before its crew stands down
 static const int RAID_NUDGES = 1; // a raid: one re-order, then another target (RETARGETS)
 static const int RETARGETS = 2;
 static const float BACK_MOVE_M = 20;      // m moved or gained toward the yard to count as progress
 static const float BACK_NUDGE_MS = 45000; // stalled this long on the way home: re-order, then dismount
 static const int BACK_NUDGES = 2;
 // Raid targets a wheeled vehicle can drive to (Jacob, 21 Nov: srv:guerrillas' technical sat in its
 // yard, re-ordered three times at a target off the roads it could reach): a road point within
 // TARGET_ROAD_M of the target, with a road route from the yard of at most ROUTE_MAX_M. A target that
 // fails, or that a technical never got going to, is skipped within BAD_NEAR for BAD_MS.
 static const float TARGET_ROAD_M = 150;
 static const float ROUTE_MAX_M = 2500;
 static const float BAD_NEAR = 150;
 static const float BAD_MS = 600000;
 static bool s_bNoRaidsForTest;    // srv:guerrillas: the depot raids only when the test launches it
 static const float REST_MS = 600000;     // after a raid that never reached its target (no route there)
 static const float WALK_MAX_MS = 600000; // on foot this long without getting there: they join it anyway, out of contact
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
  s_bNoRaidsForTest = false;
 }

 // Technicals FIA keeps at most, parked or out.
 static int Cap()
 {
  if (ACR_FIAGrowth.Many()) return 4;
  return 2;
 }

 // FIA's technicals: those in the yards of depots it still holds, and any out on a raid.
 static int Technicals()
 {
  int count;
  foreach (ACR_Depot depot : s_aDepots)
  {
   Prune(depot);
   if (Active(depot)) count += depot.m_aVehicles.Count();
   else if (depot.m_Out) count++;
  }
  return count;
 }

 // In its yard: near where it was parked, or the depot's home spot (where raids drive back to).
 static bool AtHome(ACR_Depot depot, int i)
 {
  IEntity vehicle = depot.m_aVehicles[i];
  if (vector.DistanceXZ(vehicle.GetOrigin(), Yard(depot)) <= HOME) return true;
  return i < depot.m_aSpots.Count() && vector.DistanceXZ(vehicle.GetOrigin(), depot.m_aSpots[i]) <= HOME;
 }

 // Wrecks and technicals left away from the yard (the crew lost or killed out there, or players
 // took it) are no longer the depot's: it never relaunches one wherever it stands, and has room
 // to buy another. The one out on a raid stays until the raid is over.
 static void Prune(ACR_Depot depot)
 {
  for (int i = depot.m_aVehicles.Count() - 1; i >= 0; i--)
  {
   IEntity vehicle = depot.m_aVehicles[i];
   if (vehicle && vehicle == depot.m_Out) continue;
   bool dead = !vehicle || ACR_Logistics.Health(vehicle) <= 0;
   if (!dead && AtHome(depot, i)) continue;
   if (!dead) PrintFormat("[ACR] DEPOTS %1: a technical left %2 m from the yard is no longer the depot's", NameOf(depot), (int)vector.DistanceXZ(vehicle.GetOrigin(), Yard(depot)));
   depot.m_aVehicles.RemoveOrdered(i);
   if (i < depot.m_aSpots.Count()) depot.m_aSpots.RemoveOrdered(i);
  }
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
  // COIN: the insurgency's stronghold (what was the absent army's HQ) always has a depot.
  ACR_Base stronghold = ACR_Coin.Stronghold();
  if (stronghold && stronghold.m_bPocket) MakeDepot(stronghold);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (s_aDepots.Count() >= wanted) break;
   if (base.m_bPocket && base.m_iValue >= 2 && base != stronghold) MakeDepot(base);
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
  // Its wreck is cleared like any other (broad review: technical wrecks piled up over a long match).
  if (manager.m_Cleanup) manager.m_Cleanup.Watch(vehicle);
  depot.m_aVehicles.Insert(vehicle);
  depot.m_aSpots.Insert(spot);
  if (depot.m_vYard == vector.Zero) depot.m_vYard = spot;
  return true;
 }

 // A hideout's turn to buy (ACR_FIAGrowth.Spend): a second technical for a depot in reach.
 static bool Recruit(ACR_DefenderPost hideout)
 {
  if (hideout.m_Cell.m_fPool < TECH_COST || Technicals() >= Cap()) return false;
  foreach (ACR_Depot depot : s_aDepots)
  {
   if (!Active(depot) || !depot.m_Base || depot.m_aVehicles.Count() >= 2 || vector.DistanceXZ(HomeOf(depot), hideout.m_vPos) > ACR_Guerrillas.SUPPLY_REACH) continue;
   if (!Park(depot, false)) continue;
   hideout.m_Cell.m_fPool -= TECH_COST;
   PrintFormat("[ACR] DEPOTS %1 buys a technical for %2", hideout.m_sName, NameOf(depot));
   return true;
  }
  return false;
 }

 // Still FIA's (taken: no more raids).
 static bool Active(ACR_Depot depot)
 {
  if (depot.m_Hideout) return !depot.m_Hideout.m_bGone;
  return depot.m_Base && !depot.m_Base.m_bRetired && depot.m_Base.m_bPocket && depot.m_Base.m_sOwner == ACR_Pockets.FACTION;
 }

 static string NameOf(ACR_Depot depot)
 {
  if (depot.m_Hideout) return depot.m_Hideout.m_sName;
  if (depot.m_Base) return depot.m_Base.m_sName;
  return "FIA depot";
 }

 static vector HomeOf(ACR_Depot depot)
 {
  if (depot.m_Hideout) return depot.m_Hideout.m_vPos;
  return depot.m_Base.GetOrigin();
 }

 // ---------------------------------------------------------------- stolen vehicles (insurgency part 4)

 // A vehicle FIA can take (ACR_FIASpoils): a land vehicle of a side FIA fights, sound, empty, not a
 // living squad's own vehicle, no player near it (his, or one he may come back to), no arsenal or
 // spawn point on it (a side's supply or command truck stays theirs), and not FIA's already.
 static bool Stealable(IEntity vehicle)
 {
  if (!vehicle || !Vehicle.Cast(vehicle) || !vehicle.FindComponent(CarControllerComponent)) return false;
  if (ACR_Logistics.Health(vehicle) < INTACT || !Abandoned(vehicle) || ACR_Vehicles.ArsenalOf(vehicle)) return false;
  string side = ACR_Vehicles.FactionOf(vehicle);
  if (!ACR_IED.Target(ACR_Pockets.FACTION, side)) return false;
  foreach (ACR_Depot depot : s_aDepots) if (depot.m_aVehicles.Contains(vehicle) || depot.m_Out == vehicle) return false;
  if (ACR_FIAPrisoners.PlayerNear(vehicle.GetOrigin(), PLAYER_CLEAR)) return false;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad && squad.m_OwnVehicle == vehicle && squad.CountAlive() > 0) return false;
  }
  return Technicals() < Cap() + STOLEN_EXTRA;
 }

 // Nobody in it can use it: empty, or its crew dead or out of the fight in their seats (never a player).
 static bool Abandoned(IEntity vehicle)
 {
  if (ACR_Vehicles.IsEmpty(vehicle)) return true;
  array<IEntity> crew = {};
  ACR_PendingPurchase.Occupants(vehicle, crew);
  foreach (IEntity man : crew)
  {
   if (!man || !ACR_Respawn.IsAlive(man)) continue;
   if (ACR_Squad.FindController(man) > 0 || !ACR_FIASpoils.OutOfFight(ChimeraCharacter.Cast(man))) return false;
  }
  return true;
 }

 // A hideout's stash (made on first use): its stolen vehicles park by it and raid from it.
 static ACR_Depot StashOf(ACR_DefenderPost hideout)
 {
  if (!hideout || hideout.m_bGone) return null;
  foreach (ACR_Depot depot : s_aDepots) if (depot.m_Hideout == hideout) return depot;
  ACR_Depot stash = new ACR_Depot();
  stash.m_Hideout = hideout;
  stash.m_vYard = ACR_Manager.Ground(hideout.m_vPos + Vector(STASH_OFFSET, 0, 0));
  s_aDepots.Insert(stash);
  PrintFormat("[ACR] DEPOTS %1 keeps a stash of stolen vehicles", hideout.m_sName);
  return stash;
 }

 // Where a stolen vehicle at 'pos' goes: the nearest idle depot or stash with room in STEAL_REACH,
 // else the stash of the hideout feeding the area (null: nowhere to take it).
 static ACR_Depot StealDestination(vector pos)
 {
  ACR_Depot best;
  float nearest = STEAL_REACH;
  foreach (ACR_Depot depot : s_aDepots)
  {
   if (!Active(depot) || depot.m_iState != IDLE || depot.m_Crew || depot.m_aVehicles.Count() >= YARD_MAX) continue;
   float d = vector.DistanceXZ(HomeOf(depot), pos);
   if (d >= nearest) continue;
   nearest = d;
   best = depot;
  }
  // The stash of the hideout feeding the area when it is nearer (a long drive is a lost vehicle).
  ACR_DefenderPost hideout = ACR_FIAIntel.SupplierOf(pos);
  if (best && (!hideout || vector.DistanceXZ(hideout.m_vPos, pos) >= nearest)) return best;
  ACR_Depot stash = StashOf(hideout);
  if (best && (!stash || stash.m_iState != IDLE || stash.m_Crew || stash.m_aVehicles.Count() >= YARD_MAX)) return best;
  if (!stash || stash.m_iState != IDLE || stash.m_Crew || stash.m_aVehicles.Count() >= YARD_MAX) return null;
  return stash;
 }

 // A spoils team takes 'vehicle' (ACR_FIASpoils): it becomes the crew driving it home, where it
 // joins FIA's vehicles (and the men the depot's pocket or hideout). False: nowhere to take it.
 // crewTaken: the team became its crew (false: parked by the yard where it stood, the team is still the caller's).
 static bool Steal(IEntity vehicle, SCR_AIGroup team, string from, out bool crewTaken)
 {
  crewTaken = false;
  if (!Stealable(vehicle) || !team) return false;
  ACR_Depot depot = StealDestination(vehicle.GetOrigin());
  if (!depot) return false;
  string side = ACR_Vehicles.FactionOf(vehicle);
  // Its crew out of the seats first: the living taken prisoner (no player is near: they leave the
  // field now), the dead looted. One left alive (no room at the hideout): no theft.
  array<IEntity> crew = {};
  ACR_PendingPurchase.Occupants(vehicle, crew);
  int living;
  foreach (IEntity occupant : crew)
  {
   ChimeraCharacter aliveBody = ChimeraCharacter.Cast(occupant);
   if (aliveBody && ACR_Respawn.IsAlive(aliveBody)) living++;
  }
  if (living > 0)
  {
   ACR_DefenderPost hideout = ACR_FIAIntel.SupplierOf(vehicle.GetOrigin());
   if (!hideout || hideout.m_bGone || !hideout.m_Cell) return false;
   if (ACR_FIAPrisoners.HeldAt(hideout) + living > ACR_FIAPrisoners.HOLD_MAX) return false;
  }
  foreach (IEntity man : crew)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(man);
   if (!body) continue;
   ACR_FIASpoils.TakeBody(body);
   if (ACR_Respawn.IsAlive(body) && !ACR_FIAPrisoners.Vanishing(body)) return false;
  }
  foreach (IEntity seated : crew) if (seated) SCR_EntityHelper.DeleteEntityAndChildren(seated);
  ACR_Vehicles.SetOwner(vehicle, ACR_Pockets.FACTION);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && manager.m_Cleanup) manager.m_Cleanup.Watch(vehicle);
  float now = GetGame().GetWorld().GetWorldTime();
  depot.m_aVehicles.Insert(vehicle);
  float toYard = vector.DistanceXZ(vehicle.GetOrigin(), Yard(depot));
  if (toYard <= PARK_NEAR)
  {
   // Already by the yard: it stays there, its new home spot; the team rejoins its group.
   depot.m_aSpots.Insert(vehicle.GetOrigin());
   PrintFormat("[ACR] DEPOTS %1 steals a %2 of %3 at %4 and parks it there (%5 m from %6)", from, ACR_Vehicles.NameOf(vehicle), side, ACR_Manager.Grid(vehicle.GetOrigin()), (int)toYard, NameOf(depot));
   TellSide(vehicle, side);
   return true;
  }
  depot.m_aSpots.Insert(Yard(depot));
  crewTaken = true;
  depot.m_Out = vehicle;
  depot.m_Crew = team;
  depot.m_bTheft = true;
  depot.m_iState = BACK;
  depot.m_fStateAt = now;
  depot.m_fArrivedAt = now; // no rest after: it got what it went for
  depot.m_fEmptySince = 0;
  depot.m_iNudges = 0;
  InitBackProgress(depot, now);
  team.SetCustomName(string.Format("FIA stolen %1", ACR_Vehicles.NameOf(vehicle)), 0);
  int seated = Seat(team, vehicle);
  ACR_Guerrillas.SetCombatMode(team, EAIGroupCombatMode.RETURN_FIRE);
  ACR_Guerrillas.Order(team, ACR_Squad.WP_MOVE, Yard(depot), 20, false);
  PrintFormat("[ACR] DEPOTS %1 steals a %2 of %3 at %4, driving it to %5 (%6 m, %7 seated)", from, ACR_Vehicles.NameOf(vehicle), side, ACR_Manager.Grid(vehicle.GetOrigin()), NameOf(depot), (int)vector.DistanceXZ(vehicle.GetOrigin(), Yard(depot)), seated);
  TellSide(vehicle, side);
  return true;
 }

 // The side hears of a theft only if it has men near enough to see it go.
 protected static void TellSide(IEntity vehicle, string side)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  array<string> sides = {};
  ACR_Insurgency.SidesNear(vehicle.GetOrigin(), WITNESS_REACH, sides);
  if (manager && sides.Contains(side) && manager.IntelFor(side)) manager.IntelFor(side).AddFeed(GetGame().GetWorld().GetWorldTime(), string.Format("FIA stole our %1 at %2", ACR_Vehicles.NameOf(vehicle), ACR_Manager.Grid(vehicle.GetOrigin())), vehicle.GetOrigin());
 }

 static int Stolen()
 {
  int count;
  foreach (ACR_Depot depot : s_aDepots)
   foreach (IEntity vehicle : depot.m_aVehicles) if (vehicle && ACR_Vehicles.BoughtBy(vehicle) == ACR_Pockets.FACTION) count++;
  return count;
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
   if (ACR_InsurgentSide.On() || s_iSeconds % 5 != 0 || !Active(depot) || now < depot.m_fRestUntil) continue;
   vector target;
   if (FindTarget(manager, depot, target)) Launch(depot, target);
  }
 }

 // A target side's moving vehicle on a road in reach (a supply run or a lone vehicle), else an FIA
 // cell fighting in reach.
 static bool FindTarget(ACR_Manager manager, ACR_Depot depot, out vector target)
 {
  if (s_bNoRaidsForTest) return false;
  vector home = HomeOf(depot);
  string insurgentKey = ACR_InsurgentSide.Key();
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction() || !ACR_IED.Target(insurgentKey, squad.GetFaction().GetFactionKey())) continue;
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   if (!vehicle || vector.DistanceXZ(vehicle.GetOrigin(), home) > REACH) continue;
   Physics physics = vehicle.GetPhysics();
   if (!physics || physics.GetVelocity().Length() < ACR_Guerrillas.MOVING || ACR_Minefields.RoadClearance(vehicle.GetOrigin()) > 8) continue;
   if (Drivable(depot, vehicle.GetOrigin(), target)) return true;
  }
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell || post.m_Cell.m_bHideout || !post.m_Cell.m_bSprung || post.m_Cell.m_bWithdrawing) continue;
   if (vector.DistanceXZ(post.m_Cell.m_vKillZone, home) > REACH) continue;
   if (Drivable(depot, post.m_Cell.m_vKillZone, target)) return true;
  }
  return false;
 }

 // Where a technical from this depot drives to strike at 'at': the nearest road point, if the road
 // network joins it to the yard within ROUTE_MAX_M. False (and 'at' set aside for BAD_MS): no drive.
 static bool Drivable(ACR_Depot depot, vector at, out vector target)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = depot.m_aBadTargets.Count() - 1; i >= 0; i--)
  {
   if (now >= depot.m_aBadUntil[i])
   {
    depot.m_aBadTargets.Remove(i);
    depot.m_aBadUntil.Remove(i);
    continue;
   }
   if (vector.DistanceXZ(at, depot.m_aBadTargets[i]) <= BAD_NEAR) return false;
  }
  vector road;
  if (!BBB_Terrain.RoadPointNear(at, TARGET_ROAD_M, road))
  {
   SetAside(depot, at, "no road near it");
   return false;
  }
  array<vector> legs = {};
  int nodes, millis;
  if (!BBB_Roads.Route(Yard(depot), road, legs, nodes, millis))
  {
   SetAside(depot, at, "no road route from the yard");
   return false;
  }
  float length = BBB_Roads.s_fLastLength;
  if (length > ROUTE_MAX_M)
  {
   SetAside(depot, at, string.Format("%1 m by road", (int)length));
   return false;
  }
  target = road;
  return true;
 }

 static void SetAside(ACR_Depot depot, vector at, string why)
 {
  depot.m_aBadTargets.Insert(at);
  depot.m_aBadUntil.Insert(GetGame().GetWorld().GetWorldTime() + BAD_MS);
  PrintFormat("[ACR] DEPOTS %1: no raid at %2 (%3)", NameOf(depot), ACR_Manager.Grid(at), why);
 }

 // One technical parked in the yard out with a crew of two, driving at the target.
 static bool Launch(ACR_Depot depot, vector target)
 {
  if (depot.m_iState != IDLE || depot.m_Crew) return false;
  Prune(depot);
  ACR_Manager manager = ACR_Manager.GetInstance();
  IEntity vehicle;
  foreach (IEntity parked : depot.m_aVehicles) if (!vehicle && parked && ACR_Logistics.Health(parked) > 0 && !Occupied(parked)) vehicle = parked;
  if (!manager || !vehicle) return false;
  SCR_AIGroup crew = manager.m_Bases.SpawnGuards(ACR_Pockets.FACTION, vehicle.GetOrigin() + Vector(3, 0, 3), CREW, string.Format("FIA technical %1", NameOf(depot)), 5);
  if (!crew) return false;
  Seat(crew, vehicle);
  depot.m_Out = vehicle;
  depot.m_Crew = crew;
  depot.m_vTarget = target;
  depot.m_iState = OUT;
  depot.m_bTheft = false;
  depot.m_fStateAt = GetGame().GetWorld().GetWorldTime();
  depot.m_fArrivedAt = 0;
  depot.m_fEmptySince = 0;
  depot.m_iNudges = 0;
  depot.m_iRetargets = 0;
  depot.m_fNudgedAt = depot.m_fStateAt;
  ACR_Guerrillas.SetCombatMode(crew, EAIGroupCombatMode.FIRE_AT_WILL);
  ACR_Guerrillas.Order(crew, ACR_Squad.WP_MOVE, target, 30, false);
  PrintFormat("[ACR] DEPOTS %1: a technical drives out for %2 (%3 m)", NameOf(depot), ACR_Manager.Grid(target), (int)vector.DistanceXZ(vehicle.GetOrigin(), target));
  return true;
 }

 // The target it never got going to is set aside; the next one FindTarget finds, if any.
 protected static bool Retarget(ACR_Manager manager, ACR_Depot depot, out vector next)
 {
  depot.m_aBadTargets.Insert(depot.m_vTarget);
  depot.m_aBadUntil.Insert(GetGame().GetWorld().GetWorldTime() + BAD_MS);
  bool test = s_bNoRaidsForTest;
  s_bNoRaidsForTest = false;
  bool found = FindTarget(manager, depot, next);
  s_bNoRaidsForTest = test;
  return found;
 }

 protected static bool Occupied(IEntity vehicle)
 {
  array<IEntity> crew = {};
  ACR_PendingPurchase.Occupants(vehicle, crew);
  return !crew.IsEmpty();
 }

 // Driver first, then the gun, the rest anywhere (teleported in: they start at the vehicle).
 // Returns how many were asked in and accepted.
 protected static int Seat(SCR_AIGroup crew, IEntity vehicle)
 {
  SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!seats) return 0;
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
  found.Clear();
  // An unarmed (stolen) vehicle: the second man rides in the back.
  seats.GetCompartmentsOfType(found, ECompartmentType.CARGO);
  order.InsertAll(found);
  array<AIAgent> agents = {};
  crew.GetAgents(agents);
  int next, accepted;
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || next >= order.Count()) continue;
   CompartmentAccessComponent access = body.GetCompartmentAccessComponent();
   if (access && access.GetInVehicle(vehicle, order[next], true, -1, ECloseDoorAfterActions.LEAVE_OPEN, false)) accepted++;
   next++;
  }
  return accepted;
 }

 protected static void Step(ACR_Manager manager, ACR_Depot depot, float now)
 {
  int alive;
  if (depot.m_Crew) alive = manager.m_Bases.GuardsAlive(depot.m_Crew);
  if (alive <= 0)
  {
   PrintFormat("[ACR] DEPOTS %1: the technical's crew is gone", NameOf(depot));
   EndRaid(manager, depot);
   return;
  }
  if (depot.m_iState == WALK)
  {
   if (CrewNear(depot.m_Crew, depot.m_vWalkTo, JOIN)) Join(manager, depot);
   // No way there: once out of contact and away from players they join it all the same (as any post packs up).
   else if (now - depot.m_fStateAt >= WALK_MAX_MS && !CrewInContact(depot.m_Crew) && !CrewNearPlayer(depot.m_Crew))
   {
    PrintFormat("[ACR] DEPOTS %1: the technical's crew never got there on foot", NameOf(depot));
    Join(manager, depot);
   }
   return;
  }
  // The vehicle lost (burnt, or empty for EMPTY_MS: nobody can drive it): they walk to the nearest
  // cell or hideout. A short dismount to fight is not a loss.
  // A stolen vehicle's crew not in it yet (their seats refused, or they got out): seated again.
  if (depot.m_bTheft && depot.m_Out && !Occupied(depot.m_Out) && depot.m_iNudges < NUDGES && CrewNear(depot.m_Crew, depot.m_Out.GetOrigin(), 15))
  {
   depot.m_iNudges++;
   Seat(depot.m_Crew, depot.m_Out);
   ACR_Guerrillas.Order(depot.m_Crew, ACR_Squad.WP_MOVE, Yard(depot), 20, false);
   PrintFormat("[ACR] DEPOTS %1: the stolen vehicle's crew isn't aboard, seated again (%2)", NameOf(depot), depot.m_iNudges);
  }
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
   PrintFormat("[ACR] DEPOTS %1: the technical never got going, its crew stands down", NameOf(depot));
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
   int nudges = NUDGES;
   if (!depot.m_bTheft) nudges = RAID_NUDGES;
   bool inYard = depot.m_fArrivedAt <= 0 && now - depot.m_fNudgedAt >= NUDGE_MS && vector.DistanceXZ(depot.m_Out.GetOrigin(), Yard(depot)) < 20;
   vector next;
   if (inYard && depot.m_iNudges >= nudges && !depot.m_bTheft && depot.m_iRetargets < RETARGETS && Retarget(manager, depot, next))
   {
    depot.m_iRetargets++;
    depot.m_iNudges = 0;
    depot.m_fNudgedAt = now;
    depot.m_fStateAt = now;
    depot.m_vTarget = next;
    ACR_Guerrillas.Order(depot.m_Crew, ACR_Squad.WP_MOVE, next, 30, false);
    PrintFormat("[ACR] DEPOTS %1: the technical never got going, it tries %2 instead (%3 m)", NameOf(depot), ACR_Manager.Grid(next), (int)vector.DistanceXZ(depot.m_Out.GetOrigin(), next));
   }
   else if (inYard && depot.m_iNudges < nudges)
   {
    depot.m_iNudges++;
    depot.m_fNudgedAt = now;
    ACR_Guerrillas.Order(depot.m_Crew, ACR_Squad.WP_MOVE, depot.m_vTarget, 30, false);
    PrintFormat("[ACR] DEPOTS %1: the technical hasn't set off, ordered again", NameOf(depot));
   }
   // Still in the yard after every re-order: no route there. Stand down now (not after
   // TRIP_MAX_MS), and EndRaid rests the depot so it doesn't relaunch at the same target.
   else if (inYard)
   {
    PrintFormat("[ACR] DEPOTS %1: the technical never got going, its crew stands down", NameOf(depot));
    EndRaid(manager, depot);
    return;
   }
   if (depot.m_fArrivedAt <= 0 && vector.DistanceXZ(depot.m_Out.GetOrigin(), depot.m_vTarget) <= ARRIVE)
   {
    depot.m_fArrivedAt = now;
    PrintFormat("[ACR] DEPOTS %1: the technical strikes at %2", NameOf(depot), ACR_Manager.Grid(depot.m_vTarget));
   }
   bool done = depot.m_fArrivedAt > 0 && now - depot.m_fArrivedAt >= strike;
   if (done || now - depot.m_fStateAt >= TRIP_MAX_MS) Home(depot, now);
   return;
  }
  // Back: home in the yard, the crew stands down.
  // Stopped first: crew out of a rolling vehicle leaves it to coast off.
  if (vector.DistanceXZ(depot.m_Out.GetOrigin(), Yard(depot)) <= HOME && Speed(depot.m_Out) < 1.0)
  {
   PrintFormat("[ACR] DEPOTS %1: the technical is back", NameOf(depot));
   EndRaid(manager, depot);
   return;
  }
  // Progress & stall tracking on the drive home: an actively driving technical (including a long
  // road route or a stolen vehicle from up to STEAL_REACH) never times out mid-drive. If it stalls
  // out of contact, re-order it first; only if genuinely stuck (or making no net progress for
  // TRIP_MAX_MS) does the crew dismount and walk home via Walk().
  vector pos = depot.m_Out.GetOrigin();
  float gap = vector.DistanceXZ(pos, Yard(depot));
  if (depot.m_vBackPos == vector.Zero || vector.DistanceXZ(pos, depot.m_vBackPos) >= BACK_MOVE_M)
  {
   depot.m_vBackPos = pos;
   depot.m_fBackMovedAt = now;
   depot.m_iBackNudges = 0;
  }
  if (gap <= depot.m_fBackBestGap - BACK_MOVE_M)
  {
   depot.m_fBackBestGap = gap;
   depot.m_fBackProgressAt = now;
  }
  if (CrewInContact(depot.m_Crew))
  {
   depot.m_fBackMovedAt = now;
   depot.m_fBackProgressAt = now;
   return;
  }
  bool stalled = now - depot.m_fBackMovedAt >= BACK_NUDGE_MS;
  if (stalled && depot.m_iBackNudges < BACK_NUDGES)
  {
   depot.m_iBackNudges++;
   depot.m_fBackMovedAt = now;
   ACR_Guerrillas.Order(depot.m_Crew, ACR_Squad.WP_MOVE, Yard(depot), 20, false);
   PrintFormat("[ACR] DEPOTS %1: the technical stalled on the way home (%2 m out), ordered again (%3/%4)", NameOf(depot), (int)gap, depot.m_iBackNudges, BACK_NUDGES);
   return;
  }
  bool noProgress = now - depot.m_fBackProgressAt >= TRIP_MAX_MS;
  if (stalled || noProgress)
  {
   PrintFormat("[ACR] DEPOTS %1: the technical is stuck on the way home (%2 m out), its crew dismounts", NameOf(depot), (int)gap);
   Dismount(depot.m_Crew, depot.m_Out);
   Walk(depot);
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
  return HomeOf(depot);
 }

 protected static void Home(ACR_Depot depot, float now)
 {
  depot.m_iState = BACK;
  depot.m_fStateAt = now;
  InitBackProgress(depot, now);
  ACR_Guerrillas.SetCombatMode(depot.m_Crew, EAIGroupCombatMode.RETURN_FIRE);
  ACR_Guerrillas.Order(depot.m_Crew, ACR_Squad.WP_MOVE, Yard(depot), 20, false);
  PrintFormat("[ACR] DEPOTS %1: the technical drives home", NameOf(depot));
 }

 protected static void InitBackProgress(ACR_Depot depot, float now)
 {
  depot.m_fBackMovedAt = now;
  depot.m_fBackProgressAt = now;
  depot.m_iBackNudges = 0;
  if (depot.m_Out)
  {
   depot.m_vBackPos = depot.m_Out.GetOrigin();
   depot.m_fBackBestGap = vector.DistanceXZ(depot.m_vBackPos, Yard(depot));
  }
  else
  {
   depot.m_vBackPos = vector.Zero;
   depot.m_fBackBestGap = float.MAX;
  }
 }

 protected static void Dismount(SCR_AIGroup crew, IEntity vehicle)
 {
  if (!crew) return;
  if (vehicle)
  {
   SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
   SCR_AIGroupUtilityComponent utility = crew.GetGroupUtilityComponent();
   if (usage && utility && utility.IsUsableVehicle(usage)) utility.RemoveUsableVehicle(usage);
  }
  array<AIAgent> agents = {};
  crew.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || !body.IsInVehicle() || ACR_Squad.FindController(body) > 0 || ACR_FIASpoils.OutOfFight(body)) continue;
   CompartmentAccessComponent access = body.GetCompartmentAccessComponent();
   if (!access) continue;
   if (!access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.LEAVE_OPEN, false))
    access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
  }
 }

 // On foot to the nearest standing cell or hideout (none: the depot itself).
 protected static void Walk(ACR_Depot depot)
 {
  vector at = HomeOf(depot);
  IEntity leader;
  if (depot.m_Crew.GetLeaderAgent()) leader = depot.m_Crew.GetLeaderAgent().GetControlledEntity();
  if (leader) at = leader.GetOrigin();
  depot.m_WalkTo = null;
  depot.m_vWalkTo = HomeOf(depot);
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
  depot.m_fStateAt = GetGame().GetWorld().GetWorldTime();
  depot.m_Out = null;
  ACR_Guerrillas.SetCombatMode(depot.m_Crew, EAIGroupCombatMode.RETURN_FIRE);
  ACR_Guerrillas.Order(depot.m_Crew, ACR_Squad.WP_MOVE, depot.m_vWalkTo, 15, false);
  string to = NameOf(depot);
  if (depot.m_WalkTo) to = depot.m_WalkTo.m_sName;
  PrintFormat("[ACR] DEPOTS %1: the technical is lost, its crew walks to %2 (%3 m)", NameOf(depot), to, (int)nearest);
 }

 // Arrived on foot: into the cell's reserve (or the pocket's garrison).
 protected static void Join(ACR_Manager manager, ACR_Depot depot)
 {
  int back;
  if (depot.m_WalkTo && !depot.m_WalkTo.m_bGone)
  {
   back = manager.m_Bases.PackUpGuards(depot.m_Crew);
   depot.m_WalkTo.m_iReserve += back;
  }
  // Into the base's reserve up to its set garrison; the rest hold the base as a group (Jacob, 19 Nov).
  else if (depot.m_Hideout && !depot.m_Hideout.m_bGone)
  {
   back = manager.m_Bases.PackUpGuards(depot.m_Crew);
   depot.m_Hideout.m_iReserve += back;
  }
  else if (Active(depot) && depot.m_Base) back = manager.m_Bases.TakeIntoReserve(depot.m_Base, depot.m_Crew, ACR_Pockets.FACTION, string.Format("FIA technical crew %1", NameOf(depot)));
  else back = manager.m_Bases.PackUpGuards(depot.m_Crew);

  string to = NameOf(depot);
  if (depot.m_WalkTo) to = depot.m_WalkTo.m_sName;
  PrintFormat("[ACR] DEPOTS %1: %2 of the technical's crew joined %3", NameOf(depot), back, to);
  depot.m_Crew = null;
  depot.m_bTheft = false;
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

 // A hostile of FIA's near one of the crew's living men.
 protected static bool CrewInContact(SCR_AIGroup crew)
 {
  array<AIAgent> agents = {};
  crew.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Respawn.IsAlive(body) && ACR_IED.HostileNear(ACR_Pockets.FACTION, body.GetOrigin(), ACR_Guerrillas.CLEAR)) return true;
  }
  return false;
 }

 protected static bool CrewNearPlayer(SCR_AIGroup crew)
 {
  array<AIAgent> agents = {};
  crew.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Respawn.IsAlive(body) && ACR_FIAPrisoners.PlayerNear(body.GetOrigin(), PLAYER_CLEAR)) return true;
  }
  return false;
 }

 protected static bool PlayerAboard(IEntity vehicle)
 {
  if (!vehicle) return false;
  array<IEntity> crew = {};
  ACR_PendingPurchase.Occupants(vehicle, crew);
  foreach (IEntity man : crew)
   if (man && ACR_Squad.FindController(man) > 0) return true;
  return false;
 }

 // The raid is over: the crew stands down (out of the vehicle and gone), the technical stays
 // (in the yard: parked for the next raid; anywhere else: Prune drops it). A raid that never got
 // there rests the depot (an unreachable target would relaunch it every few minutes).
 // A stolen vehicle home: its crew joins the hideout's reserve or the pocket's.
 protected static void JoinHome(ACR_Manager manager, ACR_Depot depot)
 {
  int back;
  if (depot.m_Hideout && !depot.m_Hideout.m_bGone)
  {
   back = manager.m_Bases.PackUpGuards(depot.m_Crew);
   depot.m_Hideout.m_iReserve += back;
  }
  else if (depot.m_Base && Active(depot)) back = manager.m_Bases.TakeIntoReserve(depot.m_Base, depot.m_Crew, ACR_Pockets.FACTION, string.Format("FIA crew %1", NameOf(depot)));
  else back = manager.m_Bases.PackUpGuards(depot.m_Crew);
  PrintFormat("[ACR] DEPOTS %1: the stolen vehicle is home, %2 of its crew joined", NameOf(depot), back);
 }

 protected static void EndRaid(ACR_Manager manager, ACR_Depot depot)
 {
  if (depot.m_iState != IDLE && depot.m_fArrivedAt <= 0) depot.m_fRestUntil = GetGame().GetWorld().GetWorldTime() + REST_MS;
  if (depot.m_Crew)
  {
   array<AIAgent> agents = {};
   depot.m_Crew.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
    if (body && body.IsInVehicle() && body.GetCompartmentAccessComponent()) body.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   }
   if (depot.m_bTheft) JoinHome(manager, depot);
   else manager.m_Bases.PackUpGuards(depot.m_Crew);
  }
  depot.m_bTheft = false;
  if (depot.m_Out && !PlayerAboard(depot.m_Out) && depot.m_Out.GetPhysics())
  {
   depot.m_Out.GetPhysics().SetVelocity(vector.Zero);
   depot.m_Out.GetPhysics().SetAngularVelocity(vector.Zero);
  }
  depot.m_Crew = null;
  depot.m_Out = null;
  depot.m_iState = IDLE;
 }
}
