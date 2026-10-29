// srv:insurgency (Everon whole map, about 2 min): Insurgency v1 (ACR_Insurgency, #89).
// Checks: with FIA allied to the US, the game's factions are friendly, the US knows the pockets,
// its AI takes none for an enemy base, a US team beside an FIA cell doesn't deploy it; USSR is
// still FIA's enemy. A US loss to FIA on a road marks a dangerous road on the US map with a feed
// line; a supply run past it waits; a free US combat squad is sent to clear it, and there with
// no contact the road is cleared and the squad freed.
class ACR_TestInsurgency
{
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 protected static ACR_DefenderPost s_Cell;
 protected static vector s_vRoad;

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
  ACR_TestRegress.Complete("insurgency", 8);
 }

 protected static void Spawn(ACR_Manager manager, string name, vector at, float distance)
 {
  vector spot = ACR_Manager.Ground(at + Vector(distance, 0, 0));
  manager.m_sSpawningFaction = ACR_FactionSelection.West();
  manager.SpawnUnit(name, FIRE_TEAM, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
 }
}
