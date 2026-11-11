// srv:guerrillas (Everon whole map, about 2 min): FIA guerrilla cells (ACR_Guerrillas, #89).
// Checks: a few cells (road ambushes and houses) away from both starting bases and outside every
// base; a US fire team 400 m up the road deploys an ambush lying in wait off the road, holding its
// fire; the team in the kill zone springs it (fire at will); it pulls back after its fight; with
// nobody near it packs up, its barricade (if any) goes, and it moves on to another road later; a
// forced tripod gun (s_bGunForTest) is manned with the rest on crouched posts, left when they pull
// back and gone when they pack up; a
// house cell deploys round its building holding its fire; traffic on a far road draws a waiting
// ambush there.
// Depots (#96 part 3, ACR_Depots): a depot pocket parks a technical; it drives out with a crew of
// two to a road 300 m off, strikes and drives home, where the crew stands down; a crew that loses
// its vehicle walks to the nearest cell and joins it.
class ACR_TestGuerrillas
{
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 protected static ACR_DefenderPost s_Ambush;
 protected static ACR_DefenderPost s_House;
 protected static vector s_vOldSite;
 protected static IEntity s_Heap;
 protected static IEntity s_Gun;
 protected static ref ACR_Depot s_Depot;
 protected static vector s_vDepotTarget;
 protected static int s_iJoinBefore;
 protected static bool s_bWalked;
 protected static IEntity s_Lost;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 10)
  {
   ACR_Guerrillas.s_bGunForTest = true;
   Placement(manager);
  }
  if (!s_Ambush) return;
  // A heap on the hide site: terrain height alone would stand the men inside it (playtest pn20: a bunker's mound).
  if (second == 11) s_Heap = ACR_Manager.SpawnPrefab(ACR_TestSpawnHeap.GRAVEL, ACR_Manager.Ground(s_Ambush.m_vPos));
  if (second == 12) Approach(manager, "Guerrilla Bait", s_Ambush.m_Cell.m_vKillZone, 400);
  // The bait holds its fire: the ambush must spring on the kill zone, not on a firefight.
  if (second == 16) HoldFire(manager, "Guerrilla Bait");
  if (second == 32) Deployed(manager);
  if (second == 34) IntoKillZone(manager);
  if (second == 38) Sprung();
  if (second == 40) s_Ambush.m_Cell.m_fSprungAt -= ACR_Guerrillas.HIT_S * 1000;
  if (second == 42) PulledBack();
  // Before the bait goes: with nobody near, the cell packs up (and its gun goes) within one 5 s tick.
  if (second == 43)
  {
   GunLeft();
   Away(manager, "Guerrilla Bait");
  }
  // Again: with the reworked commander (#232) the bait gets reinforced, and a man joining late
  // would stand at the kill zone.
  if (second == 45 || second == 48) Away(manager, "Guerrilla Bait");
  if (second == 57) MovedOn();
  if (second == 58) FollowsTraffic();
  if (!s_House) return;
  if (second == 58) Approach(manager, "House Bait", s_House.m_Cell.m_vKillZone, 300);
  if (second == 62) HoldFire(manager, "House Bait");
  if (second == 77) HouseDeployed();
  if (second == 80) DepotMade(manager);
  if (!s_Depot) return;
  if (second == 83) CrewMounts(manager);
  if (second == 145) Struck(manager);
  if (second == 205) Home(manager);
  if (second == 206) ACR_Depots.Launch(s_Depot, s_vDepotTarget);
  if (second == 209) LoseIt(manager);
  if (second == 210) DropIt();
  if (second >= 210 && second < 224 && s_Depot.m_iState == ACR_Depots.WALK) s_bWalked = true;
  if (second == 217) Walking(manager);
  if (second == 224) Joined(manager);
 }

 protected static void Placement(ACR_Manager manager)
 {
  array<ACR_Base> starts = {};
  foreach (ACR_Base base : ACR_Base.s_All) if (!base.m_sOwner.IsEmpty() && !base.m_bPocket && !base.m_bRetired) starts.Insert(base);
  int roads, houses;
  bool clear = true;
  bool outside = true;
  string detail = "";
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell || post.m_Cell.m_bHideout) continue; // hideouts: srv:hideouts
   if (post.m_Cell.m_bHouse) { houses++; if (!s_House) s_House = post; }
   else { roads++; if (!s_Ambush) s_Ambush = post; }
   vector site = post.m_Cell.m_vKillZone;
   float keep = 1000;
   if (!post.m_Cell.m_bHouse) keep = ACR_Guerrillas.AMBUSH_HQ_CLEAR;
   foreach (ACR_Base start : starts) if (vector.DistanceXZ(site, start.GetOrigin()) < keep) clear = false;
   foreach (ACR_Base base : ACR_Base.s_All) if (!base.m_bRetired && vector.DistanceXZ(site, base.GetOrigin()) < base.m_fRadius) outside = false;
   detail += string.Format(" %1(%2)", post.m_sName, post.m_iReserve);
  }
  Check("guerrillas: a few road ambushes and houses on the whole map", roads >= 2 && roads <= 4 && houses >= 1 && houses <= 3, string.Format("%1 roads, %2 houses:%3", roads, houses, detail));
  Check("guerrillas: away from both starting bases (ambushes 900 m, houses 1 km)", clear, "");
  Check("guerrillas: outside every base", outside, "");
  if (!s_Ambush) Finish();
  else
  {
   s_vOldSite = s_Ambush.m_Cell.m_vKillZone;
   Check("guerrillas: an ambush watches a road and hides off it", ACR_Minefields.RoadClearance(s_Ambush.m_Cell.m_vKillZone) < 3 && ACR_Minefields.RoadClearance(s_Ambush.m_vPos) >= 15, string.Format("kill zone %1 m from a road, hiding place %2 m", (int)ACR_Minefields.RoadClearance(s_Ambush.m_Cell.m_vKillZone), (int)ACR_Minefields.RoadClearance(s_Ambush.m_vPos)));
  }
 }

 // A US fire team 'distance' m from 'at', towards the map centre.
 protected static void Approach(ACR_Manager manager, string name, vector at, float distance)
 {
  vector dir = Vector(6400, 0, 6400) - at;
  dir[1] = 0;
  dir.Normalize();
  vector spot = at + dir * distance;
  vector road, roadDir;
  float width;
  if (ACR_Minefields.Snap(spot, 80, road, roadDir, width)) spot = road;
  spot = ACR_Manager.Ground(spot);
  manager.m_sSpawningFaction = "US";
  manager.SpawnUnit(name, FIRE_TEAM, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
  PrintFormat("[ACR] GUERRILLAS test: %1 at %2, %3 m from %4", name, spot, (int)vector.DistanceXZ(spot, at), at);
 }

 protected static void Deployed(ACR_Manager manager)
 {
  SCR_AIGroup group = s_Ambush.m_Group;
  int men, offRoad, buried;
  if (group)
  {
   array<AIAgent> agents = {};
   group.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity man = agent.GetControlledEntity();
    if (!man) continue;
    men++;
    if (ACR_Minefields.RoadClearance(man.GetOrigin()) >= 8) offRoad++;
    // A man on the tripod gun sits under it: the trace from above meets the gun, not a heap.
    ChimeraCharacter body = ChimeraCharacter.Cast(man);
    if (body && body.IsInVehicle()) continue;
    if (ACR_BaseSystem.Buried(man.GetOrigin())) buried++;
   }
  }
  Check("guerrillas: the ambush deploys when enemies come up the road", group != null && men > 0, string.Format("%1 men", men));
  Check("guerrillas: nobody lies inside what stands on the hide site", s_Heap && men > 0 && buried == 0, string.Format("%1 of %2 buried, heap %3", buried, men, s_Heap != null));
  Check("guerrillas: they lie in wait off the road, holding their fire", men > 0 && offRoad == men && CombatMode(group) == EAIGroupCombatMode.HOLD_FIRE && !s_Ambush.m_Cell.m_bSprung, string.Format("%1 of %2 off the road, combat mode %3", offRoad, men, typename.EnumToString(EAIGroupCombatMode, CombatMode(group))));
  // An FIA barricade blocks the road: no lane cut through it for anyone (#85's lane is for a side's own roadblocks).
  ACR_Roadblock block = s_Ambush.m_Cell.m_Block;
  // The gun on the best-covered spot with an FIA man on it, the others on posts (Jacob 6 Nov).
  s_Gun = s_Ambush.m_Cell.m_Gun;
  BaseCompartmentSlot seat = ACR_Defenders.TurretSeat(s_Gun);
  IEntity gunner;
  if (seat) gunner = seat.GetOccupant();
  bool fia = gunner && ACR_Intel.FactionKeyOf(gunner) == ACR_Pockets.FACTION;
  Check("guerrillas: they man a tripod gun, the rest on crouched posts", s_Gun && fia && group && ACR_Garrison.HasPosts(group), string.Format("gun %1, gunner %2 (FIA %3), posts %4, cover %5 at the hiding place %6 m off the road", s_Gun != null, gunner != null, fia, group && ACR_Garrison.HasPosts(group), ACR_Ambush.Cover(s_Ambush.m_vPos), (int)ACR_Minefields.RoadClearance(s_Ambush.m_vPos)));
  if (block) Check("guerrillas: its barricade has no lane through it", block.m_iLaneMoved == 0, string.Format("%1 pieces moved", block.m_iLaneMoved));
  else ACR_TestRegress.Skip("guerrillas: its barricade has no lane through it", "this ambush didn't block the road (a 40 % roll)");
 }

 protected static void IntoKillZone(ACR_Manager manager)
 {
  ACR_Squad bait = Squad(manager, "Guerrilla Bait");
  if (!bait) return;
  array<AIAgent> agents = {};
  bait.GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man) man.SetOrigin(ACR_Manager.Ground(s_Ambush.m_Cell.m_vKillZone + Vector(i * 2, 0, 0)));
  }
 }

 protected static void Sprung()
 {
  ACR_GuerrillaCell cell = s_Ambush.m_Cell;
  Check("guerrillas: an enemy in the kill zone springs it: fire at will", cell.m_bSprung && CombatMode(s_Ambush.m_Group) == EAIGroupCombatMode.FIRE_AT_WILL, string.Format("sprung %1 (%2), combat mode %3", cell.m_bSprung, cell.m_sSprung, typename.EnumToString(EAIGroupCombatMode, CombatMode(s_Ambush.m_Group))));
 }

 protected static void PulledBack()
 {
  AIWaypoint waypoint;
  if (s_Ambush.m_Group) waypoint = s_Ambush.m_Group.GetCurrentWaypoint();
  bool away = waypoint && vector.DistanceXZ(waypoint.GetOrigin(), s_Ambush.m_Cell.m_vKillZone) > 200;
  Check("guerrillas: after its fight it pulls back from the road", s_Ambush.m_Cell.m_bWithdrawing && away, string.Format("withdrawing %1, waypoint %2", s_Ambush.m_Cell.m_bWithdrawing, waypoint != null));
 }

 // Home to a US starting base: every cell keeps 1 km or more from those, so nobody is near one.
 protected static void Away(ACR_Manager manager, string name)
 {
  ACR_Squad squad = Squad(manager, name);
  if (!squad)
  {
   PrintFormat("[ACR] GUERRILLAS test: no squad %1 to send away", name);
   return;
  }
  vector home = vector.Zero;
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sOwner == "US" && !base.m_bPocket) home = base.GetOrigin();
  if (home == vector.Zero) return;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man) man.SetOrigin(ACR_Manager.Ground(home + Vector(i * 2, 0, 0)));
  }
  PrintFormat("[ACR] GUERRILLAS test: %1 sent home, %2 agents", name, agents.Count());
 }

 protected static void HoldFire(ACR_Manager manager, string name)
 {
  ACR_Squad squad = Squad(manager, name);
  if (squad && squad.m_Group && squad.m_Group.GetGroupUtilityComponent()) squad.m_Group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
 }

 protected static void MovedOn()
 {
  ACR_GuerrillaCell cell = s_Ambush.m_Cell;
  bool later = cell.m_fReadyAt > GetGame().GetWorld().GetWorldTime() + 60000;
  bool moved = vector.DistanceXZ(cell.m_vKillZone, s_vOldSite) > 1;
  bool gone = s_Ambush.m_iReserve > 0 || s_Ambush.m_bGone;
  Check("guerrillas: with nobody near it packs up, its barricade goes", !s_Ambush.m_Group && !cell.m_Block && gone, string.Format("group %1, barricade %2, reserve %3, gone %4%5", s_Ambush.m_Group != null, cell.m_Block != null, s_Ambush.m_iReserve, s_Ambush.m_bGone, NearestEnemy()));
  Check("guerrillas: survivors move on to another road later", s_Ambush.m_bGone || (cell.m_iMoves == 1 && moved && later), string.Format("moves %1, site %2 -> %3, ready in %4 s", cell.m_iMoves, s_vOldSite, cell.m_vKillZone, (int)((cell.m_fReadyAt - GetGame().GetWorld().GetWorldTime()) / 1000)));
  Check("guerrillas: packed up, its gun goes", !s_Gun && !cell.m_Gun, string.Format("gun still there %1", s_Gun != null));
  if (!s_House) Finish();
 }

 // Pulling back they leave the gun: nobody of the cell on it, and it is not held any more.
 protected static void GunLeft()
 {
  BaseCompartmentSlot seat = ACR_Defenders.TurretSeat(s_Gun);
  IEntity gunner;
  if (seat) gunner = seat.GetOccupant();
  bool fiaOn = gunner && ACR_Respawn.IsAlive(gunner) && ACR_Intel.FactionKeyOf(gunner) == ACR_Pockets.FACTION;
  Check("guerrillas: pulling back they leave the gun", s_Gun && !fiaOn && !ACR_Guerrillas.HeldGun(s_Ambush), string.Format("gun %1, FIA gunner on it %2, held %3", s_Gun != null, fiaOn, ACR_Guerrillas.HeldGun(s_Ambush) != null));
 }

 // Traffic on a stretch far from every cell: one waiting ambush moves there.
 protected static void FollowsTraffic()
 {
  array<vector> busy = {};
  foreach (vector road : ACR_Guerrillas.s_aRoads)
  {
   if (busy.Count() >= 5) break;
   // Stretches an ambush could take (cc89rs5guer: two picked by distance alone had no hiding place).
   bool far = ACR_Guerrillas.AmbushSiteOk(road, null);
   foreach (vector other : busy) if (vector.DistanceXZ(other, road) < 800) far = false;
   if (far) busy.Insert(road);
  }
  foreach (int n, vector stretch : busy)
   for (int k = 0; k < 6 + n; k++) ACR_Guerrillas.NoteTraffic(stretch);
  bool moved = ACR_Guerrillas.Resite();
  bool there;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
   if (post.m_Cell && !post.m_Cell.m_bHouse)
    foreach (vector point : busy) if (vector.DistanceXZ(post.m_Cell.m_vKillZone, point) < ACR_Guerrillas.TRAFFIC_REACH) there = true;
  Check("guerrillas: a waiting ambush moves to the roads the sides drive", moved && there, string.Format("%1 busy stretches, moved %2, a cell there %3", busy.Count(), moved, there));
 }

 protected static void HouseDeployed()
 {
  SCR_AIGroup group = s_House.m_Group;
  int men;
  if (group) men = group.GetAgentsCount();
  Check("guerrillas: a house cell deploys round its building, holding its fire", group != null && men > 0 && CombatMode(group) == EAIGroupCombatMode.HOLD_FIRE, string.Format("%1 men, combat mode %2", men, typename.EnumToString(EAIGroupCombatMode, CombatMode(group))));
 }

 protected static void Check(string name, bool ok, string detail)
 {
  ACR_TestRegress.Check(name, ok, detail);
 }

 // ---------------------------------------------------------------- depots (#96 part 3)

 // A pocket (or an open base made one) as a depot with its technical; a road about 300 m off.
 protected static void DepotMade(ACR_Manager manager)
 {
  // One the match made (ACR_Depots.Apply), else a pocket (or an open base made one) made a depot.
  ACR_Base pocket;
  if (!ACR_Depots.s_aDepots.IsEmpty()) s_Depot = ACR_Depots.s_aDepots[0];
  if (s_Depot) pocket = s_Depot.m_Base;
  foreach (ACR_Base base : ACR_Base.s_All) if (!pocket && base.m_bPocket) pocket = base;
  if (!pocket)
  {
   foreach (ACR_Base open : ACR_Base.s_All) if (!pocket && ACR_FIAGrowth.SpreadableBase(open)) pocket = open;
   if (pocket) ACR_Pockets.Add(pocket, 4);
  }
  if (pocket && !s_Depot) s_Depot = ACR_Depots.MakeDepot(pocket);
  bool parked = s_Depot && !s_Depot.m_aVehicles.IsEmpty() && vector.DistanceXZ(s_Depot.m_aVehicles[0].GetOrigin(), pocket.GetOrigin()) <= pocket.m_fRadius + 60;
  string vehicle = "none";
  if (s_Depot && !s_Depot.m_aVehicles.IsEmpty()) vehicle = ACR_Vehicles.NameOf(s_Depot.m_aVehicles[0]);
  Check("guerrillas: a depot pocket parks an FIA technical in its yard", parked, string.Format("pocket %1, technical %2", pocket != null, vehicle));
  if (!s_Depot)
  {
   Finish();
   return;
  }
  // Along the road from the yard, so the technical can drive there (a road picked by direction
  // alone can be one vanilla AI finds no route to: its crew gets out).
  vector yard = ACR_Depots.Yard(s_Depot);
  vector start, startDir;
  float width;
  if (ACR_Minefields.Snap(yard, 100, start, startDir, width))
  {
   for (int side = 0; side < 2 && s_vDepotTarget == vector.Zero; side++)
   {
    vector at = start;
    vector dir = startDir;
    if (side == 1) dir = startDir * -1;
    for (int step = 0; step < 40; step++)
    {
     vector next, nextDir;
     if (!ACR_Minefields.Snap(at + dir * 10, 15, next, nextDir, width)) break;
     if (vector.Dot(nextDir, dir) < 0) nextDir = nextDir * -1;
     at = next;
     dir = nextDir;
     if (vector.DistanceXZ(at, yard) >= 250)
     {
      s_vDepotTarget = at;
      break;
     }
    }
   }
  }
  if (s_vDepotTarget == vector.Zero)
  {
   Check("guerrillas: the technical drives to its target and strikes", false, "no road 300 m from the depot");
   Finish();
   s_Depot = null;
   return;
  }
  ACR_Depots.s_fStrikeMsForTest = 5000;
  ACR_Depots.Launch(s_Depot, s_vDepotTarget);
 }

 protected static int Aboard(IEntity vehicle)
 {
  array<IEntity> crew = {};
  ACR_PendingPurchase.Occupants(vehicle, crew);
  return crew.Count();
 }

 protected static void CrewMounts(ACR_Manager manager)
 {
  int aboard = 0;
  if (s_Depot.m_Out) aboard = Aboard(s_Depot.m_Out);
  Check("guerrillas: the technical's crew of two mounts it", s_Depot.m_iState == ACR_Depots.OUT && aboard == ACR_Depots.CREW, string.Format("state %1, %2 aboard", s_Depot.m_iState, aboard));
 }

 protected static void Struck(ACR_Manager manager)
 {
  float d = -1;
  if (s_Depot.m_Out) d = vector.DistanceXZ(s_Depot.m_Out.GetOrigin(), s_vDepotTarget);
  Check("guerrillas: the technical drives to its target and strikes", s_Depot.m_fArrivedAt > 0, string.Format("state %1, %2 m from the target", s_Depot.m_iState, (int)d));
 }

 protected static void Home(ACR_Manager manager)
 {
  IEntity vehicle;
  if (!s_Depot.m_aVehicles.IsEmpty()) vehicle = s_Depot.m_aVehicles[0];
  float d = -1;
  if (vehicle) d = vector.DistanceXZ(vehicle.GetOrigin(), ACR_Depots.Yard(s_Depot));
  Check("guerrillas: the technical drives home and its crew stands down", s_Depot.m_iState == ACR_Depots.IDLE && !s_Depot.m_Crew && vehicle && d <= ACR_Depots.HOME + 20 && Aboard(vehicle) == 0, string.Format("state %1, %2 m from the yard", s_Depot.m_iState, (int)d));
 }

 // Out again, then its technical gone under it: the crew on foot.
 protected static void LoseIt(ACR_Manager manager)
 {
  IEntity vehicle = s_Depot.m_Out;
  if (!vehicle || !s_Depot.m_Crew) return;
  array<AIAgent> agents = {};
  s_Depot.m_Crew.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (body && body.IsInVehicle()) body.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
  }
  // Gone a second later, once they're out (deleting it with them aboard takes them too).
  s_Lost = vehicle;
 }

 protected static void DropIt()
 {
  if (!s_Lost) return;
  s_Depot.m_aVehicles.RemoveItem(s_Lost);
  SCR_EntityHelper.DeleteEntityAndChildren(s_Lost);
  s_Lost = null;
 }

 protected static void Walking(ACR_Manager manager)
 {
  if (s_Depot.m_WalkTo) s_iJoinBefore = s_Depot.m_WalkTo.m_iReserve;
  // Put them there (the walk itself is a vanilla move order).
  if (s_Depot.m_iState == ACR_Depots.WALK && s_Depot.m_Crew)
  {
   array<AIAgent> agents = {};
   s_Depot.m_Crew.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (body) body.SetOrigin(ACR_Manager.Ground(s_Depot.m_vWalkTo + Vector(3, 0, 0)));
   }
  }
 }

 protected static void Joined(ACR_Manager manager)
 {
  bool joined = s_Depot.m_iState == ACR_Depots.IDLE && !s_Depot.m_Crew;
  int after = -1;
  if (s_Depot.m_WalkTo) after = s_Depot.m_WalkTo.m_iReserve;
  string to = "the depot";
  if (s_Depot.m_WalkTo) to = s_Depot.m_WalkTo.m_sName;
  Check("guerrillas: a crew that loses its technical walks to the nearest cell and joins it", s_bWalked && joined && (!s_Depot.m_WalkTo || after > s_iJoinBefore), string.Format("walked %1, to %2, reserve %3 -> %4, state %5", s_bWalked, to, s_iJoinBefore, after, s_Depot.m_iState));
  ACR_Depots.s_fStrikeMsForTest = 0;
  Finish();
 }

 protected static void Finish()
 {
  ACR_TestRegress.Complete("guerrillas", 22);
 }

 protected static EAIGroupCombatMode CombatMode(SCR_AIGroup group)
 {
  if (!group) return -1;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  if (!utility) return -1;
  return utility.GetCombatModeExternal();
 }

 // Still out: the enemy nearest its men (StayOut keeps it out with one within CLEAR).
 protected static string NearestEnemy()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !s_Ambush.m_Group) return "";
  array<AIAgent> men = {};
  s_Ambush.m_Group.GetAgents(men);
  string nearest = "";
  float best = ACR_Guerrillas.CLEAR + 1;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction() || ACR_Insurgency.Friendly(squad.GetFaction().GetFactionKey(), s_Ambush.m_sFaction)) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (!body || !ACR_Respawn.IsAlive(body)) continue;
    foreach (AIAgent man : men)
    {
     if (!man.GetControlledEntity()) continue;
     float distance = vector.DistanceXZ(body.GetOrigin(), man.GetControlledEntity().GetOrigin());
     if (distance >= best) continue;
     best = distance;
     nearest = string.Format(", nearest enemy %1 at %2 m (%3)", squad.GetName(), (int)distance, body.GetOrigin());
    }
   }
  }
  return nearest;
 }

 protected static ACR_Squad Squad(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == name) return squad;
  return null;
 }
}
