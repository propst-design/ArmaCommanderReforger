// Containment evac (#107, Jacob 7 Nov 2026): the only way civilians get out of a town the dead are
// coming for. Replaces the abstract survivor rescue (5 RP the moment a soldier got close).
//  - A commander's squad sets an XP (evac point, squad order "Evac") within VILLAGE_REACH of a town
//    holding out (ACR_Containment's sites, the "Civilians" rings on the map). The town's people
//    become real civilians then, not before (AI slots): Civil Population's own when its compat
//    plugs into ACR_CivilianSocket.Evacuees, else ACA's vanilla civilians.
//  - About LEFT_SHARE of them (stragglers, wounded) stay in the town; the rest walk to the XP "on
//    rails": a move waypoint, and a walker who hasn't moved for NUDGE_MS is stepped on towards it.
//  - Any vehicle of a living side parked (stopped, on the ground) within XP_RADIUS gets every
//    civilian there put straight into its free passenger seats. No fallback: with no seat they wait.
//    A helicopter counts (air lift), but its rotors near an XP bring the dead (ACR_Outbreak.CounterAttack).
//  - Pay on delivery: a civilian aboard a vehicle that reaches a base a living side holds, with no
//    zombie in it (zombies block a base), pays that base's side RP_EACH and is gone. In TvTvZ it is
//    a race: whoever's vehicle loads them and whichever side's base they reach.
//  - The XP ends once nobody is left to bring out; the left-behind go once no soldier is near.
// AI commanders run evacs with a free transport (ACR_AICommander.ManageEvacs). Under "I'm the 2IC"
// the commander can keep extracts to themselves (TwoICExtracts, command panel).
class ACR_EvacPoint : Managed
{
 int m_iId;
 vector m_vPos;
 string m_sSide;      // who set it
 string m_sTown;
 vector m_vTown;
 SCR_AIGroup m_Walkers; // the walk (one move waypoint to the XP)
 SCR_AIGroup m_Riders;  // those put aboard (no waypoint, so they stay in their seats)
 ref array<IEntity> m_aPeople = {};
 ref array<vector> m_aLast = {};
 ref array<float> m_aMovedAt = {};
 ref array<IEntity> m_aLeft = {};
 int m_iWalked;
 int m_iDelivered;
 int m_iLost;
 float m_fNoiseAt = -1;
}

class ACR_Evac
{
 static const ResourceName CIVILIAN = "{22E43956740A6794}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_Randomized.et";
 static const float XP_RADIUS = 75;        // m: a parked vehicle this close loads the civilians there
 static const float VILLAGE_REACH = 600;   // m: an XP must be this close to a town holding out
 static const float ARRIVED = 15;          // m from the XP: arrived, waiting for a ride
 static const float NUDGE_MS = 20000;      // a walker who hasn't moved for this long ...
 static const float NUDGE_STEP = 12;       // ... is stepped this far on towards the XP
 static const float LEFT_SHARE = 0.2;      // stragglers and wounded left in the town
 static const float PARKED_SPEED = 1;      // m/s: slower than this is parked
 static const float NOISE_RANGE = 250;     // m: a running helicopter this close to an XP is heard
 static const float NOISE_GAP_MS = 600000; // and heard again after this long at the same XP
 static const float LEFT_CLEAR = 500;      // m: the left-behind go once no soldier is this close
 static const int MOST_PER_SIDE = 3;       // open XPs per side

 protected static ref array<ref ACR_EvacPoint> s_aPoints;
 protected static ref array<IEntity> s_aLeftBehind;
 protected static ref array<IEntity> s_aFound;
 protected static ref set<string> s_NoTwoIC;  // sides whose commander keeps extracts from the 2IC
 protected static ref array<ACR_Base> s_aDeliveredTo; // this update's deliveries (TellDeliveries)
 protected static ref array<int> s_aDeliveredCount;
 protected static ref array<int> s_aDeliveredRP;
 protected static int s_iNextId;
 static int s_iDelivered;    // civilians delivered this match (tests)
 static int s_iSeated;       // civilians put aboard (tests)
 static int s_iNudged;       // walker steps (tests)
 static int s_iNoise;        // rotor-noise counterattacks (tests)
 static int s_iHeard;        // rotor noise heard at an XP (tests)
 static int s_iHanded;       // civilians the socket handed over (tests)

 static void ACR_InitStatics()
 {
  s_aPoints = {};
  s_aLeftBehind = {};
  s_aFound = {};
  s_NoTwoIC = new set<string>();
  s_aDeliveredTo = {};
  s_aDeliveredCount = {};
  s_aDeliveredRP = {};
 }

 static void Reset()
 {
  foreach (ACR_EvacPoint point : s_aPoints) Close(point, true);
  s_aPoints.Clear();
  foreach (IEntity left : s_aLeftBehind) if (left) SCR_EntityHelper.DeleteEntityAndChildren(left);
  s_aLeftBehind.Clear();
  s_NoTwoIC.Clear();
  s_iNextId = 0;
  s_iDelivered = 0;
  s_iSeated = 0;
  s_iNudged = 0;
  s_iNoise = 0;
  s_iHeard = 0;
  s_iHanded = 0;
 }

 static int Count() { return s_aPoints.Count(); }
 static ACR_EvacPoint At(int i) { return s_aPoints[i]; }
 static bool Has(ACR_EvacPoint point) { return point != null && s_aPoints.Contains(point); }

 static int CountFor(string side)
 {
  int n;
  foreach (ACR_EvacPoint point : s_aPoints) if (point.m_sSide == side) n++;
  return n;
 }

 // ---------------------------------------------------------------- 2IC duty (-acr2IC 2)

 // Whether "I'm the 2IC" runs this side's evacs (the commander can keep them).
 static bool TwoICExtracts(string side) { return !s_NoTwoIC.Contains(side); }

 static string ToggleTwoIC(string side)
 {
  if (s_NoTwoIC.Contains(side))
  {
   s_NoTwoIC.RemoveItem(side);
   return "2IC runs evacs again";
  }
  s_NoTwoIC.Insert(side);
  return "Evacs are yours: the 2IC leaves them alone";
 }

 // ---------------------------------------------------------------- setting an XP

 // The town holding out nearest 'at' within VILLAGE_REACH, or -1.
 static int TownNear(vector at)
 {
  int best = -1;
  float bestDistance = VILLAGE_REACH;
  for (int i = 0; i < ACR_Containment.SiteCount(); i++)
  {
   float d = vector.DistanceXZ(ACR_Containment.SiteAt(i).m_vPos, at);
   if (d <= bestDistance) { bestDistance = d; best = i; }
  }
  return best;
 }

 // 'side' sets an XP at 'at' (a squad order, or the AI): the nearest town's people start out.
 // Empty refusal when set; 'made' is the new XP.
 static string Place(string side, vector at, out ACR_EvacPoint made)
 {
  made = null;
  if (!ACR_Containment.On()) return "evac points are for zombie containment (-acrOutbreak contain)";
  if (side.IsEmpty() || ACR_TvZ.IsAbsent(side)) return "no side";
  if (CountFor(side) >= MOST_PER_SIDE) return string.Format("%1 evac points already open: bring those people out first", MOST_PER_SIDE);
  // Not where a ride would be paid for on the spot (0.5.13 review #12): an XP inside or beside a
  // base DeliveryBase pays at loaded its civilians straight into a vehicle that was already there.
  ACR_Base payer = PayingBaseNear(at, XP_RADIUS);
  if (payer) return string.Format("too close to %1: set it outside the base, the civilians have to be driven out", payer.m_sName);
  int town = TownNear(at);
  if (town < 0) return string.Format("no civilians within %1 m: set it near a town holding out (the rings on the map)", VILLAGE_REACH);
  ACR_SurvivorSite site = ACR_Containment.SiteAt(town);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return "not ready";
  ACR_EvacPoint point = new ACR_EvacPoint();
  point.m_iId = ++s_iNextId;
  point.m_vPos = ACR_Manager.Ground(at);
  point.m_sSide = side;
  point.m_sTown = site.m_sTown;
  if (point.m_sTown.IsEmpty()) point.m_sTown = ACR_Manager.Grid(site.m_vPos);
  point.m_vTown = site.m_vPos;
  int count = site.m_iCount;
  int left = Math.Round(count * LEFT_SHARE);
  if (count >= 4) left = Math.Max(1, left);
  int walking = count - left;
  ACR_Containment.Take(site);
  point.m_Walkers = NewGroup(point.m_vTown, string.Format("Civilians leaving %1", point.m_sTown));
  point.m_Riders = NewGroup(point.m_vTown, string.Format("Civilians out of %1", point.m_sTown));
  if (!point.m_Walkers || !point.m_Riders)
  {
   Close(point, true);
   return "could not move the civilians";
  }
  // A civilian-population mod's own people first (its compat add-on), then ours.
  array<IEntity> handed = {};
  int got = ACR_CivilianSocket.Get().Evacuees(point.m_vTown, 150, walking, handed);
  foreach (IEntity person : handed)
  {
   if (!person || !ACR_Respawn.IsAlive(person) || point.m_aPeople.Count() >= walking) continue;
   if (!Join(point.m_Walkers, person)) continue;
   Track(point, person);
   s_iHanded++;
  }
  IEntity navAgent = ACR_Garrison.AnyAgent();
  while (point.m_aPeople.Count() < walking)
  {
   IEntity man = Spawn(point.m_vTown, point.m_Walkers, navAgent);
   if (!man) break;
   Track(point, man);
  }
  for (int l = 0; l < left; l++)
  {
   IEntity straggler = Spawn(point.m_vTown, null, navAgent);
   if (straggler) point.m_aLeft.Insert(straggler);
  }
  point.m_iWalked = point.m_aPeople.Count();
  AIWaypoint move = AIWaypoint.Cast(GetGame().SpawnEntityPrefabLocal(Resource.Load(ACR_Squad.WP_MOVE), null, Params(point.m_vPos)));
  if (move)
  {
   move.SetCompletionRadius(ARRIVED * 0.5);
   point.m_Walkers.AddWaypoint(move);
  }
  s_aPoints.Insert(point);
  made = point;
  PrintFormat("[ACR] EVAC %1 XP %2 at %3 for %4: %5 walking (%6 from the socket), %7 left behind", side, point.m_iId, ACR_Manager.Grid(point.m_vPos), point.m_sTown, point.m_iWalked, got, left);
  ACR_Containment.Feed(side, string.Format("Evac point set by %1: %2 civilians walking out, %3 stragglers and wounded stay. Park a vehicle there to load them and bring them to a base", point.m_sTown, point.m_iWalked, left), point.m_vPos);
  array<string> sides = ACR_Containment.Sides();
  foreach (string other : sides)
   if (other != side) ACR_Containment.Feed(other, string.Format("Civilians are leaving %1", point.m_sTown), point.m_vTown);
  return "";
 }

 protected static void Track(ACR_EvacPoint point, IEntity person)
 {
  point.m_aPeople.Insert(person);
  point.m_aLast.Insert(person.GetOrigin());
  point.m_aMovedAt.Insert(GetGame().GetWorld().GetWorldTime());
 }

 protected static EntitySpawnParams Params(vector at)
 {
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  parameters.Transform[3] = at;
  return parameters;
 }

 // A group that is never a squad (registered with the garrisons before anyone joins).
 protected static SCR_AIGroup NewGroup(vector at, string name)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases) return null;
  SCR_AIGroup.IgnoreSpawning(true);
  SCR_AIGroup group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(ACR_BaseSystem.GROUP_BASE, at));
  SCR_AIGroup.IgnoreSpawning(false);
  if (!group) return null;
  group.SetMaxUnitsToSpawn(0);
  manager.m_Bases.m_GarrisonGroups.Insert(group);
  group.SetDeleteWhenEmpty(false);
  group.SetCustomName(name, 0);
  ACR_Squad.KeepSimulated(group);
  return group;
 }

 protected static void DropGroup(SCR_AIGroup group)
 {
  if (!group) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && manager.m_Bases) manager.m_Bases.m_GarrisonGroups.RemoveItem(group);
  SCR_EntityHelper.DeleteEntityAndChildren(group);
 }

 // A vanilla civilian in 'group' (none: standing on his own) round 'at'.
 protected static IEntity Spawn(vector at, SCR_AIGroup group, IEntity navAgent)
 {
  vector spot = ACR_BaseSystem.SoldierSpot(at, 25, navAgent);
  IEntity man = ACR_Manager.SpawnPrefab(CIVILIAN, spot);
  if (!man) return null;
  if (group && !group.AddAIEntityToGroup(man))
  {
   SCR_EntityHelper.DeleteEntityAndChildren(man);
   return null;
  }
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(man.FindComponent(FactionAffiliationComponent));
  if (affiliation) affiliation.SetAffiliatedFactionByKey("CIV");
  ACR_CivilianSocket.Get().Register(man); // the sides' civilian rings (ACR_Intel.CivilianActivity)
  return man;
 }

 // Moves a person into 'group' (out of whatever group he was in).
 protected static bool Join(SCR_AIGroup group, IEntity person)
 {
  AIControlComponent control = AIControlComponent.Cast(person.FindComponent(AIControlComponent));
  if (!control || !group) return false;
  AIAgent agent = control.GetControlAIAgent();
  if (!agent) return false;
  AIGroup was = agent.GetParentGroup();
  if (was == group) return true;
  if (was) was.RemoveAgent(agent);
  group.AddAgent(agent);
  return true;
 }

 protected static void Close(ACR_EvacPoint point, bool all)
 {
  foreach (IEntity left : point.m_aLeft) if (left) s_aLeftBehind.Insert(left);
  point.m_aLeft.Clear();
  if (all)
  {
   foreach (IEntity person : point.m_aPeople) if (person) SCR_EntityHelper.DeleteEntityAndChildren(person);
   point.m_aPeople.Clear();
  }
  DropGroup(point.m_Walkers);
  DropGroup(point.m_Riders);
  point.m_Walkers = null;
  point.m_Riders = null;
 }

 // ---------------------------------------------------------------- every 2 s (ACR_Containment.Tick)

 static void Tick(ACR_Manager manager)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = s_aPoints.Count() - 1; i >= 0; i--)
  {
   ACR_EvacPoint point = s_aPoints[i];
   Update(manager, point, now);
   if (!point.m_aPeople.IsEmpty()) continue;
   PrintFormat("[ACR] EVAC XP %1 at %2 done: %3 delivered, %4 lost", point.m_iId, point.m_sTown, point.m_iDelivered, point.m_iLost);
   ACR_Containment.Feed(point.m_sSide, string.Format("Evac from %1 over: %2 delivered, %3 lost", point.m_sTown, point.m_iDelivered, point.m_iLost), point.m_vPos);
   Close(point, false);
   s_aPoints.Remove(i);
  }
  for (int l = s_aLeftBehind.Count() - 1; l >= 0; l--)
  {
   IEntity left = s_aLeftBehind[l];
   if (!left) { s_aLeftBehind.Remove(l); continue; }
   if (SoldierNear(manager, left.GetOrigin(), LEFT_CLEAR)) continue;
   SCR_EntityHelper.DeleteEntityAndChildren(left);
   s_aLeftBehind.Remove(l);
  }
 }

 protected static bool SoldierNear(ACR_Manager manager, vector at, float range)
 {
  array<string> sides = ACR_Containment.Sides();
  foreach (string side : sides) if (ACR_Outbreak.SideNear(manager, side, at, range)) return true;
  return false;
 }

 protected static void Update(ACR_Manager manager, ACR_EvacPoint point, float now)
 {
  bool waiting;
  for (int i = point.m_aPeople.Count() - 1; i >= 0; i--)
  {
   IEntity person = point.m_aPeople[i];
   if (!person || !ACR_Respawn.IsAlive(person))
   {
    point.m_iLost++;
    Forget(point, i);
    continue;
   }
   IEntity vehicle = ACR_Transport.VehicleOf(person);
   if (vehicle)
   {
    if (Deliver(manager, point, i, vehicle)) continue;
    point.m_aMovedAt[i] = now;
    continue;
   }
   vector at = person.GetOrigin();
   float left = vector.DistanceXZ(at, point.m_vPos);
   if (left <= XP_RADIUS) waiting = true;
   if (left <= ARRIVED)
   {
    point.m_aMovedAt[i] = now;
    continue;
   }
   // On rails: walking, or stepped on when stuck. A walker who was put aboard and got out stands.
   if (vector.DistanceXZ(at, point.m_aLast[i]) > 1.5)
   {
    point.m_aLast[i] = at;
    point.m_aMovedAt[i] = now;
    continue;
   }
   if (now - point.m_aMovedAt[i] < NUDGE_MS) continue;
   vector step = point.m_vPos - at;
   step[1] = 0;
   step.Normalize();
   vector to = ACR_Manager.Ground(at + step * Math.Min(NUDGE_STEP, left - ARRIVED * 0.5));
   person.SetOrigin(to);
   point.m_aLast[i] = to;
   point.m_aMovedAt[i] = now;
   s_iNudged++;
  }
  TellDeliveries(point);
  if (waiting) Load(point);
  Noise(point, now);
 }

 protected static void Forget(ACR_EvacPoint point, int i)
 {
  point.m_aPeople.Remove(i);
  point.m_aLast.Remove(i);
  point.m_aMovedAt.Remove(i);
 }

 // ---------------------------------------------------------------- loading (forced get-in)

 protected static bool CollectVehicle(IEntity e)
 {
  if (Vehicle.Cast(e) && !s_aFound.Contains(e)) s_aFound.Insert(e);
  return true;
 }

 // A vehicle of a living side, stopped on the ground, not wrecked.
 static bool Parked(IEntity vehicle)
 {
  if (!vehicle || ACR_HeliStation.Destroyed(vehicle)) return false;
  Physics physics = vehicle.GetPhysics();
  if (physics && physics.GetVelocity().Length() > PARKED_SPEED) return false;
  vector at = vehicle.GetOrigin();
  if (at[1] - ACR_Manager.Ground(at)[1] > 3) return false;
  return SideOf(vehicle) != "";
 }

 // The living side a vehicle serves: its driver's (pilot's), else whose it is; "" for none.
 static string SideOf(IEntity vehicle)
 {
  string side;
  ChimeraCharacter driver = ACR_Squad.DriverOf(vehicle);
  if (driver && ACR_Respawn.IsAlive(driver)) side = ACR_Intel.FactionKeyOf(driver);
  if (side.IsEmpty() || side == "CIV") side = ACR_Vehicles.FactionOf(vehicle);
  array<string> sides = ACR_Containment.Sides();
  if (sides.Contains(side)) return side;
  return "";
 }

 protected static void Load(ACR_EvacPoint point)
 {
  s_aFound.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(point.m_vPos, XP_RADIUS, CollectVehicle, null, EQueryEntitiesFlags.DYNAMIC);
  foreach (IEntity vehicle : s_aFound)
  {
   if (!Parked(vehicle)) continue;
   SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
   if (!compartments) continue;
   array<BaseCompartmentSlot> seats = {};
   compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
   int seated;
   foreach (BaseCompartmentSlot seat : seats)
   {
    if (seat.GetOccupant() || seat.IsReserved()) continue;
    ChimeraCharacter rider = NextRider(point);
    if (!rider) break;
    CompartmentAccessComponent access = rider.GetCompartmentAccessComponent();
    if (!access) continue;
    Join(point.m_Riders, rider);
    if (!access.GetInVehicle(vehicle, seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false)) continue;
    seated++;
    s_iSeated++;
   }
   if (seated > 0) PrintFormat("[ACR] EVAC XP %1: %2 civilians put aboard a vehicle of %3", point.m_iId, seated, SideOf(vehicle));
  }
  s_aFound.Clear();
 }

 // A civilian of this XP on foot within its radius, not already getting in.
 protected static ChimeraCharacter NextRider(ACR_EvacPoint point)
 {
  foreach (IEntity person : point.m_aPeople)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(person);
   if (!character || !ACR_Respawn.IsAlive(character) || ACR_Transport.VehicleOf(character)) continue;
   if (vector.DistanceXZ(character.GetOrigin(), point.m_vPos) > XP_RADIUS) continue;
   CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
   if (!access || access.IsGettingIn()) continue;
   return character;
  }
  return null;
 }

 // ---------------------------------------------------------------- delivery

 // A base a living side holds with no zombie in it that 'at' is inside, or null.
 // A base DeliveryBase could pay at within its reach plus 'margin' of 'at' (zombies or not), or null.
 static ACR_Base PayingBaseNear(vector at, float margin)
 {
  array<string> sides = ACR_Containment.Sides();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || (base.m_bBuilt && base.m_bHqLost) || !sides.Contains(base.m_sOwner)) continue;
   if (vector.DistanceXZ(base.GetOrigin(), at) <= Math.Max(base.m_fRadius, 60) + margin) return base;
  }
  return null;
 }

 // 'at', or the nearest spot just outside any base an XP there would be refused for (PayingBaseNear),
 // straight out from that base's middle: where the AI sets an XP for a town it holds.
 static vector OutsideBases(vector at)
 {
  for (int tries = 0; tries < 3; tries++)
  {
   ACR_Base payer = PayingBaseNear(at, XP_RADIUS);
   if (!payer) break;
   vector away = at - payer.GetOrigin();
   away[1] = 0;
   if (away.Length() < 1) away = Vector(1, 0, 0);
   away.Normalize();
   at = ACR_Manager.Ground(payer.GetOrigin() + away * (Math.Max(payer.m_fRadius, 60) + XP_RADIUS + 15));
  }
  return at;
 }

 static ACR_Base DeliveryBase(vector at)
 {
  array<string> sides = ACR_Containment.Sides();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || (base.m_bBuilt && base.m_bHqLost) || !sides.Contains(base.m_sOwner)) continue;
   float reach = Math.Max(base.m_fRadius, 60);
   if (vector.DistanceXZ(base.GetOrigin(), at) > reach) continue;
   if (ACR_Containment.ZombiesNear(base.GetOrigin(), reach)) continue;
   return base;
  }
  return null;
 }

 protected static bool Deliver(ACR_Manager manager, ACR_EvacPoint point, int i, IEntity vehicle)
 {
  ACR_Base base = DeliveryBase(vehicle.GetOrigin());
  if (!base) return false;
  // Not a base that has come to the evac point since it was set (a FOB built on it, a town taken
  // round it): loading there would pay on the spot, as Place refuses (broad review #15).
  if (vector.DistanceXZ(base.GetOrigin(), point.m_vPos) <= Math.Max(base.m_fRadius, 60) + XP_RADIUS) return false;
  IEntity person = point.m_aPeople[i];
  Forget(point, i);
  int rp = ACR_Containment.Give(manager, base.m_sOwner, ACR_Containment.RP_EACH);
  point.m_iDelivered++;
  s_iDelivered++;
  // One feed line per base per update (a truckload arrives at once).
  int at = s_aDeliveredTo.Find(base);
  if (at < 0)
  {
   s_aDeliveredTo.Insert(base);
   s_aDeliveredCount.Insert(0);
   s_aDeliveredRP.Insert(0);
   at = s_aDeliveredTo.Count() - 1;
  }
  s_aDeliveredCount[at] = s_aDeliveredCount[at] + 1;
  s_aDeliveredRP[at] = s_aDeliveredRP[at] + rp;
  SCR_EntityHelper.DeleteEntityAndChildren(person);
  return true;
 }

 // The deliveries of one update, one line per base.
 protected static void TellDeliveries(ACR_EvacPoint point)
 {
  foreach (int i, ACR_Base base : s_aDeliveredTo)
  {
   if (!base) continue;
   int n = s_aDeliveredCount[i];
   PrintFormat("[ACR] EVAC XP %1: %2 civilians from %3 delivered to %4 (%5, +%6 RP)", point.m_iId, n, point.m_sTown, base.m_sName, base.m_sOwner, s_aDeliveredRP[i]);
   string line = string.Format("%1 civilians from %2 delivered to %3: +%4 RP", n, point.m_sTown, base.m_sName, s_aDeliveredRP[i]);
   if (n == 1) line = string.Format("A civilian from %1 delivered to %2: +%3 RP", point.m_sTown, base.m_sName, s_aDeliveredRP[i]);
   ACR_Containment.Feed(base.m_sOwner, line, base.GetOrigin());
  }
  s_aDeliveredTo.Clear();
  s_aDeliveredCount.Clear();
  s_aDeliveredRP.Clear();
 }

 // ---------------------------------------------------------------- rotor noise

 protected static void Noise(ACR_EvacPoint point, float now)
 {
  if (point.m_fNoiseAt >= 0 && now - point.m_fNoiseAt < NOISE_GAP_MS) return;
  s_aFound.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(point.m_vPos, NOISE_RANGE, CollectVehicle, null, EQueryEntitiesFlags.DYNAMIC);
  string side;
  foreach (IEntity vehicle : s_aFound)
  {
   VehicleHelicopterSimulation rotors = VehicleHelicopterSimulation.Cast(vehicle.FindComponent(VehicleHelicopterSimulation));
   if (!rotors || !rotors.EngineIsOn() || ACR_HeliStation.Destroyed(vehicle)) continue;
   side = SideOf(vehicle);
   if (!side.IsEmpty()) break;
  }
  s_aFound.Clear();
  if (side.IsEmpty()) return;
  point.m_fNoiseAt = now;
  s_iHeard++;
  int hordes = ACR_Outbreak.CounterAttack(point.m_vPos, "the evac point at " + point.m_sTown, null, side);
  if (hordes > 0) s_iNoise++;
  PrintFormat("[ACR] EVAC XP %1: rotor noise (%2), %3 hordes come for it", point.m_iId, side, hordes);
  if (hordes > 0) ACR_Containment.Feed(point.m_sSide, string.Format("The rotors at %1 woke the dead: they are coming for the evac point", point.m_sTown), point.m_vPos);
 }

 // Rotor noise now, whatever the clock (tests).
 static int NoiseForTest(ACR_EvacPoint point)
 {
  point.m_fNoiseAt = -1;
  int before = s_iHeard;
  Noise(point, GetGame().GetWorld().GetWorldTime());
  return s_iHeard - before;
 }

 // ---------------------------------------------------------------- map (ACR_MapFeed via ACR_Containment.Zones)

 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  foreach (ACR_EvacPoint point : s_aPoints)
  {
   if (point.m_sSide != faction) continue;
   int onFoot;
   foreach (IEntity person : point.m_aPeople) if (person && !ACR_Transport.VehicleOf(person)) onFoot++;
   positions.Insert(point.m_vPos);
   radii.Insert(XP_RADIUS);
   labels.Insert(string.Format("Evac point %1 (%2 on foot, %3 aboard)", point.m_sTown, onFoot, point.m_aPeople.Count() - onFoot));
  }
 }

 // How many of an XP's civilians are aboard a vehicle / that vehicle.
 static int Aboard(ACR_EvacPoint point, IEntity vehicle = null)
 {
  int n;
  foreach (IEntity person : point.m_aPeople)
  {
   if (!person) continue;
   IEntity ride = ACR_Transport.VehicleOf(person);
   if (ride && (!vehicle || ride == vehicle)) n++;
  }
  return n;
 }

 // Civilians of this XP still on foot.
 static int OnFoot(ACR_EvacPoint point)
 {
  int n;
  foreach (IEntity person : point.m_aPeople) if (person && !ACR_Transport.VehicleOf(person)) n++;
  return n;
 }
}
