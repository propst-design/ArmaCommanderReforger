// srv:insurgency (Everon whole map, about 2 min): Insurgency v1 (ACR_Insurgency, #89).
// Checks: with FIA allied to the US, the game's factions are friendly, the US knows the pockets,
// its AI takes none for an enemy base, a US team beside an FIA cell doesn't deploy it; USSR is
// still FIA's enemy. A US loss to FIA on a road marks a dangerous road on the US map with a feed
// line; a supply run past it waits; a free US combat squad is sent to clear it, and there with
// no contact the road is cleared and the squad freed.
// IEDs (#96 part 2, ACR_IED): a wired charge with nobody there until an enemy comes; its team
// comes out for a Red car; the Blue (allied) car over it passes, the Red car sets it off and the
// road is marked for Red; a second team with its triggerman shot runs and the charge doesn't go
// off under the car; the game's Disarm leaves it dead; an old charge with nobody near is lifted.
class ACR_TestInsurgency
{
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 protected static ACR_DefenderPost s_Cell;
 protected static vector s_vRoad;
 static const ResourceName RED_CAR = "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et";
 static const ResourceName BLUE_CAR = "{4A71F755A4513227}Prefabs/Vehicles/Wheeled/M998/M1025.et";
 protected static ref ACR_IED s_IED;
 protected static ref ACR_IED s_IED2;
 protected static ref ACR_IED s_IED3;
 protected static ref array<vector> s_aUsed;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 10) Allegiance(manager);
  if (second == 12 && s_Cell) Spawn(manager, "Ally Team", s_Cell.m_Cell.m_vKillZone, 150);
  if (second == 25) AllyPasses(manager);
  if (second == 26) Spawn(manager, "Sweep Team", s_vRoad, 600);
  if (second == 40) DangerRoad(manager);
  if (second == 41) Sweep(manager);
  if (second == 45) Arrive(manager);
  // The AI commander is off here: its road clearing ticks each second while the sweeper waits.
  if (second > 45 && second < 110) Quiet(manager);
  if (second == 110) Cleared(manager);
  if (second == 112) PlantFirst(manager);
  if (!s_IED) return;
  if (second == 128) AlliedPasses(manager);
  if (second == 131) EnemySetsOff(manager);
  if (second == 134) RedCarOn(manager);
  if (second == 137) EnemyBlown(manager);
  if (second == 138) PlantSecond(manager);
  if (!s_IED2) return;
  if (second == 152) ShootTriggerman(manager);
  if (second == 155) CarSaved(manager);
  if (second == 158) DisarmIt(manager);
  if (second == 160) Disarmed(manager);
  if (second == 161) PlantOld(manager);
  if (second == 164) Lifted(manager);
 }

 protected static void Allegiance(ACR_Manager manager)
 {
  ACR_Options.Set("acrFIAAllegiance", "west");
  ACR_Insurgency.Apply();
  string us = ACR_FactionSelection.West();
  string ussr = ACR_FactionSelection.East();
  SCR_Faction fia = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(ACR_Pockets.FACTION));
  SCR_Faction west = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(us));
  bool game = fia && west && fia.IsFactionFriendly(west) && west.IsFactionFriendly(fia);
  ACR_TestRegress.Check("insurgency: FIA allied to the Blue side: friendly to it, hostile to Red", ACR_Insurgency.Friendly(ACR_Pockets.FACTION, us) && !ACR_Insurgency.Friendly(ACR_Pockets.FACTION, ussr) && game, string.Format("%1 / %2, game factions friendly %3", us, ussr, game));
  int pockets, known;
  ACR_Base pocket;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base.m_bPocket) continue;
   pockets++;
   if (base.OwnerSeenBy(us) == ACR_Pockets.FACTION) known++;
   if (!pocket) pocket = base;
  }
  ACR_TestRegress.Check("insurgency: the ally knows the pockets, the other side doesn't", pockets > 0 && known == pockets && (!pocket || pocket.OwnerSeenBy(ussr) == ""), string.Format("%1 of %2 known", known, pockets));
  ACR_AICommander ai = manager.FobAICommanderFor(us);
  ACR_Base nearest;
  if (ai && pocket) nearest = ai.NearestEnemyBase(pocket.GetOrigin());
  ACR_TestRegress.Check("insurgency: the ally's AI takes no pocket for an enemy base", pocket && ai && (!nearest || !nearest.m_bPocket), string.Format("nearest enemy base to %1: %2", pocket.m_sName, nearest));
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts) if (post.m_Cell && !post.m_Cell.m_bHouse && !s_Cell) s_Cell = post;
  if (!s_Cell) ACR_TestRegress.Abort("insurgency", "no guerrilla road cell");
  else s_vRoad = s_Cell.m_Cell.m_vKillZone;
 }

 protected static void AllyPasses(ACR_Manager manager)
 {
  ACR_TestRegress.Check("insurgency: an allied team beside a cell doesn't bring it out", s_Cell.m_Group == null && !ACR_Defenders.EnemyWithin(manager, ACR_Pockets.FACTION, s_Cell.m_vPos, 400), string.Format("deployed %1", s_Cell.m_Group != null));
 }

 protected static void DangerRoad(ACR_Manager manager)
 {
  string us = ACR_FactionSelection.West();
  ACR_Insurgency.OnLoss(us, s_vRoad, ACR_Pockets.FACTION);
  array<vector> positions = {};
  array<float> radii = {};
  array<string> labels = {};
  ACR_Insurgency.Zones(us, positions, radii, labels);
  array<vector> theirs = {};
  ACR_Insurgency.Zones(ACR_FactionSelection.East(), theirs, radii, labels);
  ACR_TestRegress.Check("insurgency: a loss to FIA on a road marks it on that side's map only", positions.Count() == 1 && theirs.IsEmpty() && labels.Contains("FIA ambush"), string.Format("%1 zones, %2 for the other side", positions.Count(), theirs.Count()));
  vector before = s_vRoad + Vector(-500, 0, 0);
  vector after = s_vRoad + Vector(500, 0, 0);
  ACR_TestRegress.Check("insurgency: a supply run past it waits", ACR_Insurgency.OnTheWay(us, before, after) != null && ACR_Insurgency.OnTheWay(us, before, before + Vector(0, 0, 200)) == null, "");
 }

 protected static void Sweep(ACR_Manager manager)
 {
  ACR_AICommander ai = manager.FobAICommanderFor(ACR_FactionSelection.West());
  if (ai) ai.ManageDangerRoadsForTest();
  ACR_DangerRoad road;
  if (!ACR_Insurgency.s_aRoads.IsEmpty()) road = ACR_Insurgency.s_aRoads[0];
  bool sent = road && road.m_Sweeper && road.m_Sweeper.m_iAITask == ACR_EAITask.SWEEP;
  string who = "none";
  if (sent) who = road.m_Sweeper.GetName() + " '" + road.m_Sweeper.m_Order + "'";
  ACR_TestRegress.Check("insurgency: a free combat squad is sent to clear the road", sent, who);
 }

 // The sweeper put on the spot, quiet for the time it takes.
 protected static void Arrive(ACR_Manager manager)
 {
  if (ACR_Insurgency.s_aRoads.IsEmpty()) return;
  ACR_DangerRoad road = ACR_Insurgency.s_aRoads[0];
  if (!road.m_Sweeper) return;
  array<AIAgent> agents = {};
  road.m_Sweeper.GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man) man.SetOrigin(ACR_Manager.Ground(road.m_vPos + Vector(i * 2, 0, 3)));
  }
 }

 // ACR_AICommander.ManageDangerRoads, as the AI's tick would run it.
 protected static void Quiet(ACR_Manager manager)
 {
  if (ACR_Insurgency.s_aRoads.IsEmpty()) return;
  ACR_AICommander ai = manager.FobAICommanderFor(ACR_FactionSelection.West());
  if (ai) ai.ManageDangerRoadsForTest();

 }

 protected static void Cleared(ACR_Manager manager)
 {
  ACR_DangerRoad road;
  if (!ACR_Insurgency.s_aRoads.IsEmpty()) road = ACR_Insurgency.s_aRoads[0];
  ACR_Squad sweeper;
  if (road) sweeper = road.m_Sweeper;
  ACR_AICommander ai = manager.FobAICommanderFor(ACR_FactionSelection.West());
  if (ai) ai.ManageDangerRoadsForTest();
  bool freed = !sweeper || sweeper.m_iAITask == ACR_EAITask.FREE;
  string detail = string.Format("%1 roads left", ACR_Insurgency.s_aRoads.Count());
  if (road && sweeper && sweeper.GetAnchor()) detail += string.Format(", sweeper %1 m off, %2 enemies known, quiet for %3 s, task %4", (int)vector.DistanceXZ(sweeper.GetAnchor().GetOrigin(), road.m_vPos), sweeper.KnownEnemies(300), (int)((GetGame().GetWorld().GetWorldTime() - road.m_fQuietSince) / 1000), typename.EnumToString(ACR_EAITask, sweeper.m_iAITask));
  ACR_TestRegress.Check("insurgency: there with no contact, the road is cleared and the squad freed", ACR_Insurgency.s_aRoads.IsEmpty() && freed, detail);
 }

 protected static void Spawn(ACR_Manager manager, string name, vector at, float distance)
 {
  vector spot = ACR_Manager.Ground(at + Vector(distance, 0, 0));
  manager.m_sSpawningFaction = ACR_FactionSelection.West();
  manager.SpawnUnit(name, FIRE_TEAM, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
 }
 // ---------------------------------------------------------------- IEDs (#96 part 2)

 // A road site for a charge with a triggerman's place, 900 m from every soldier and from the
 // charges before it.
 protected static vector FreeRoad(ACR_Manager manager)
 {
  foreach (vector site : ACR_Guerrillas.s_aRoads)
  {
   bool clear = !ACR_IED.HostileNear(ACR_FactionSelection.East(), site, 900) && !ACR_IED.HostileNear(ACR_FactionSelection.West(), site, 900);
   foreach (vector used : s_aUsed) if (vector.DistanceXZ(used, site) < 900) clear = false;
   if (!clear) continue;
   vector road, dir, trigger;
   float width;
   if (!ACR_Minefields.Snap(site, 40, road, dir, width) || !ACR_IED.TriggerSite(road, dir, width, trigger)) continue;
   s_aUsed.Insert(site);
   return site;
  }
  return vector.Zero;
 }

 protected static ACR_IED PlantAt(ACR_Manager manager)
 {
  vector site = FreeRoad(manager);
  if (site == vector.Zero) return null;
  return ACR_IED.Plant(site, ACR_Pockets.FACTION, null);
 }

 // A crewed car of 'faction' on the road 'distance' m from the charge (along it).
 protected static void Car(ACR_Manager manager, string name, string faction, ResourceName prefab, ACR_IED ied, float distance)
 {
  vector road, dir;
  float width;
  vector at = ied.m_vPos;
  if (ACR_Minefields.Snap(ied.m_vPos, 40, road, dir, width)) at = ACR_Manager.Ground(road + dir * distance);
  manager.m_sSpawningFaction = faction;
  manager.SpawnUnit(name, "", prefab, true, "", at, at);
  manager.m_sSpawningFaction = "";
 }

 protected static IEntity VehicleOf(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName().Contains(name)) return ACR_Transport.CrewedVehicle(squad);
  return null;
 }

 // The car put down at 'at', still: over the charge, or off it.
 protected static void Put(IEntity vehicle, vector at)
 {
  if (!vehicle) return;
  vector mat[4];
  vehicle.GetWorldTransform(mat);
  mat[3] = ACR_Manager.Ground(at) + Vector(0, 0.8, 0);
  vehicle.SetWorldTransform(mat);
  Physics physics = vehicle.GetPhysics();
  if (physics)
  {
   physics.SetVelocity(vector.Zero);
   physics.SetAngularVelocity(vector.Zero);
  }
 }

 protected static void PlantFirst(ACR_Manager manager)
 {
  s_aUsed = {};
  ACR_IED.Reset();
  s_IED = PlantAt(manager);
  if (!s_IED)
  {
   ACR_TestRegress.Abort("insurgency", "no road for an IED");
   return;
  }
  float off = vector.DistanceXZ(s_IED.m_vTrigger, s_IED.m_vPos);
  ACR_TestRegress.Check("insurgency: an IED is a wired charge on the road, its triggerman's place 80-200 m off, nobody out yet", s_IED.m_Charge && s_IED.m_bWired && s_IED.m_Post && !s_IED.m_Post.m_Group && off >= ACR_IED.TRIGGER_NEAR - 5 && off <= ACR_IED.TRIGGER_FAR + 15, string.Format("charge %1, wired %2, %3 m off, out %4", s_IED.m_Charge != null, s_IED.m_bWired, (int)off, s_IED.m_Post.m_Group != null));
  Car(manager, "Red Car", ACR_FactionSelection.East(), RED_CAR, s_IED, 250);
  Car(manager, "Blue Car", ACR_FactionSelection.West(), BLUE_CAR, s_IED, -250);
 }

 // The team is out for the Red car; the Blue (allied) car stands on the charge.
 protected static void AlliedPasses(ACR_Manager manager)
 {
  bool teamOut = s_IED.m_Post.m_Group != null;
  IEntity blue = VehicleOf(manager, "Blue Car");
  Put(blue, s_IED.m_vPos);
  ACR_TestRegress.Check("insurgency: an enemy car near brings the IED team out", teamOut, string.Format("team out %1, blue car %2", teamOut, blue != null));
 }

 protected static void EnemySetsOff(ACR_Manager manager)
 {
  IEntity blue = VehicleOf(manager, "Blue Car");
  bool passed = !s_IED.m_bFired && s_IED.m_Charge != null;
  ACR_TestRegress.Check("insurgency: an allied car over the IED passes safely", passed && blue != null, string.Format("fired %1, blue car %2", s_IED.m_bFired, blue != null));
  Put(blue, s_IED.m_vPos + Vector(0, 0, 60));
 }

 protected static void RedCarOn(ACR_Manager manager)
 {
  Put(VehicleOf(manager, "Red Car"), s_IED.m_vPos);
 }

 protected static void EnemyBlown(ACR_Manager manager)
 {
  bool marked;
  foreach (ACR_DangerRoad road : ACR_Insurgency.s_aRoads) if (road.m_sSide == ACR_FactionSelection.East() && vector.DistanceXZ(road.m_vPos, s_IED.m_vPos) < 50) marked = true;
  ACR_TestRegress.Check("insurgency: the enemy car sets the IED off and the road is marked for its side", s_IED.m_bFired && marked, string.Format("fired %1, Red danger road %2, red car %3", s_IED.m_bFired, marked, VehicleOf(manager, "Red Car") != null));
 }

 protected static void PlantSecond(ACR_Manager manager)
 {
  s_IED2 = PlantAt(manager);
  if (!s_IED2)
  {
   ACR_TestRegress.Abort("insurgency", "no road for a second IED");
   return;
  }
  Car(manager, "Lone Truck", ACR_FactionSelection.East(), RED_CAR, s_IED2, 250);
 }

 // The triggerman shot before the car comes: his mate runs; the car on the charge then lives.
 protected static void ShootTriggerman(ACR_Manager manager)
 {
  IEntity man = ACR_IED.Triggerman(s_IED2.m_Post);
  if (man) SCR_EntityHelper.DeleteEntityAndChildren(man);
  GetGame().GetCallqueue().CallLater(PutSecondCar, 1500, false);
 }

 protected static void PutSecondCar()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && s_IED2) Put(VehicleOf(manager, "Lone Truck"), s_IED2.m_vPos);
 }

 protected static void CarSaved(ACR_Manager manager)
 {
  ACR_TestRegress.Check("insurgency: shooting the triggerman first saves the car (his mate runs)", s_IED2.m_bRan && !s_IED2.m_bFired && s_IED2.m_Charge != null, string.Format("ran %1, fired %2, car %3", s_IED2.m_bRan, s_IED2.m_bFired, VehicleOf(manager, "Lone Truck") != null));
 }

 // As the game's Disarm action: the charge loses its fuze.
 protected static void DisarmIt(ACR_Manager manager)
 {
  SCR_ExplosiveChargeComponent charge = ACR_SapperDemo.ChargeComponent(s_IED2.m_Charge);
  if (charge) charge.DisarmCharge();
 }

 protected static void Disarmed(ACR_Manager manager)
 {
  ACR_TestRegress.Check("insurgency: Disarm leaves the IED dead", s_IED2.m_bDisarmed && !s_IED2.m_Charge && !s_IED2.m_bFired, string.Format("disarmed %1, charge %2", s_IED2.m_bDisarmed, s_IED2.m_Charge != null));
 }

 protected static void PlantOld(ACR_Manager manager)
 {
  s_IED3 = PlantAt(manager);
  ACR_IED.s_fLiftMs = 1000;
 }

 protected static void Lifted(ACR_Manager manager)
 {
  ACR_IED.s_fLiftMs = 0;
  bool lifted = s_IED3 && s_IED3.m_bDisarmed && !s_IED3.m_Charge;
  ACR_TestRegress.Check("insurgency: an IED left long with nobody near is lifted", lifted, string.Format("planted %1, lifted %2", s_IED3 != null, lifted));
  ACR_TestRegress.Complete("insurgency", 15);
 }
}
