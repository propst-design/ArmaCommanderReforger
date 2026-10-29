// Insurgency (#89, scope agreed with Jacob 28 Oct): FIA as a presence in the world, not a side
// (no commander, no RP). Its pieces: the pockets (ACR_Pockets), the guerrilla cells
// (ACR_Guerrillas), and here what the two sides make of it:
//  - Allegiance (-acrFIAAllegiance hostile|west|east, setup row "FIA sides with"): FIA is hostile
//    to both sides, or allied to one. Allied: the game's factions are friendly (the AI on both
//    sides holds its fire), cells and pockets never deploy against that side, it can't take an
//    FIA pocket and its AI doesn't attack or shell one, and it knows where the pockets are.
//  - Dangerous roads: where a side loses men or vehicles to FIA on a road, the spot is marked on
//    that side's map ("FIA ambush", DANGER_MS). Its AI commander sends a free combat squad to clear
//    it, and holds supply runs whose way passes it until it is cleared or the mark runs out.
class ACR_DangerRoad : Managed
{
 string m_sSide;
 vector m_vPos;
 float m_fUntil;
 int m_iLosses;
 ACR_Squad m_Sweeper;   // sent to clear it (ACR_AICommander.ManageDangerRoads)
 float m_fQuietSince;   // the sweeper there with no contact since (0: not yet)
 int m_iSweeps;         // sweepers sent
}

class ACR_Insurgency
{
 static const float DANGER_MS = 1200000;  // a dangerous road is marked this long after the last loss
 static const float DANGER_RADIUS = 250;
 static const float ROAD_REACH = 120;     // a loss this close to a road counts (not one inside a pocket's yard)
 static const float CLEAR_QUIET_MS = 60000; // the sweeper there this long without contact: cleared
 static ref array<ref ACR_DangerRoad> s_aRoads = {};

 // ---------------------------------------------------------------- allegiance

 // The main side FIA is allied to ("" = hostile to both), read once at Apply (Friendly runs
 // for every sighting).
 protected static string s_sAlly;
 static string Ally() { return s_sAlly; }
 protected static string AllyOption()
 {
  string value;
  if (!ACR_Options.Get("acrFIAAllegiance", value)) return "";
  value.ToLower();
  if (value == "west") return ACR_FactionSelection.West();
  if (value == "east") return ACR_FactionSelection.East();
  return "";
 }

 // Two factions that don't fight: the same one, or FIA and its ally.
 static bool Friendly(string a, string b)
 {
  if (a == b) return true;
  if (a != ACR_Pockets.FACTION && b != ACR_Pockets.FACTION) return false;
  return !s_sAlly.IsEmpty() && (a == s_sAlly || b == s_sAlly);
 }

 // Server, once the layout is applied (ACR_Skirmish.ApplyOnce): the game's factions, and the ally
 // knows the pockets.
 static void Apply()
 {
  s_aRoads.Clear();
  s_sAlly = AllyOption();
  string ally = s_sAlly;
  if (ally.IsEmpty()) return;
  SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
  if (!factions) return;
  SCR_Faction fia = SCR_Faction.Cast(factions.GetFactionByKey(ACR_Pockets.FACTION));
  SCR_Faction friend = SCR_Faction.Cast(factions.GetFactionByKey(ally));
  if (!fia || !friend || fia == friend) return;
  factions.SetFactionsFriendly(fia, friend);
  int known;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base.m_bPocket || base.m_aDetectedBy.Contains(ally)) continue;
   base.m_aDetectedBy.Insert(ally);
   known++;
  }
  PrintFormat("[ACR] INSURGENCY FIA sides with %1 (%2 pockets known to it)", ally, known);
 }

 // ---------------------------------------------------------------- dangerous roads

 // A soldier or vehicle of 'side' lost at 'pos' to FIA (ACR_Report).
 static void OnLoss(string side, vector pos, string by)
 {
  if (by != ACR_Pockets.FACTION || side.IsEmpty() || side == by || !Replication.IsServer()) return;
  if (ACR_Minefields.RoadClearance(pos) > ROAD_REACH) return;
  // A fight for a pocket is a base fight, not an ambush on the road.
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_bPocket && vector.DistanceXZ(base.GetOrigin(), pos) < base.m_fRadius + 100) return;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_DangerRoad known : s_aRoads)
  {
   if (known.m_sSide != side || vector.DistanceXZ(known.m_vPos, pos) > DANGER_RADIUS) continue;
   known.m_fUntil = now + DANGER_MS;
   known.m_iLosses++;
   return;
  }
  ACR_DangerRoad road = new ACR_DangerRoad();
  road.m_sSide = side;
  road.m_vPos = pos;
  road.m_fUntil = now + DANGER_MS;
  road.m_iLosses = 1;
  s_aRoads.Insert(road);
  PrintFormat("[ACR] INSURGENCY %1 lost a man to FIA at %2: a dangerous road for %3 min", side, ACR_Manager.Grid(pos), (int)(DANGER_MS / 60000));
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Intel intel = manager.IntelFor(side);
  if (intel) intel.AddFeed(now, string.Format("FIA ambush at %1: the road there is dangerous", ACR_Manager.Grid(pos)));
 }

 // Every second (ACR_Manager.Tick): marks run out.
 static void Tick()
 {
  if (s_aRoads.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = s_aRoads.Count() - 1; i >= 0; i--)
  {
   if (now < s_aRoads[i].m_fUntil) continue;
   PrintFormat("[ACR] INSURGENCY the dangerous road at %1 for %2 ran out", ACR_Manager.Grid(s_aRoads[i].m_vPos), s_aRoads[i].m_sSide);
   Release(s_aRoads[i]);
   s_aRoads.RemoveOrdered(i);
  }
 }

 // Cleared by a sweeper (ACR_AICommander.ManageDangerRoads).
 static void Clear(ACR_DangerRoad road, string why)
 {
  PrintFormat("[ACR] INSURGENCY the dangerous road at %1 for %2 cleared (%3)", ACR_Manager.Grid(road.m_vPos), road.m_sSide, why);
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(road.m_sSide);
  if (intel) intel.AddFeed(GetGame().GetWorld().GetWorldTime(), string.Format("The road at %1 is clear again", ACR_Manager.Grid(road.m_vPos)));
  Release(road);
  s_aRoads.RemoveItem(road);
 }

 protected static void Release(ACR_DangerRoad road)
 {
  if (road.m_Sweeper && road.m_Sweeper.m_iAITask == ACR_EAITask.SWEEP) road.m_Sweeper.m_iAITask = ACR_EAITask.FREE;
  road.m_Sweeper = null;
 }

 // The first of side's dangerous roads within reach of the way from a to b (null: none).
 static ACR_DangerRoad OnTheWay(string side, vector a, vector b)
 {
  foreach (ACR_DangerRoad road : s_aRoads)
   if (road.m_sSide == side && ACR_AICommander.DistanceToSegment(road.m_vPos, a, b) < DANGER_RADIUS) return road;
  return null;
 }

 // The side's command map: a danger ring per dangerous road.
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  foreach (ACR_DangerRoad road : s_aRoads)
  {
   if (road.m_sSide != faction) continue;
   positions.Insert(road.m_vPos);
   radii.Insert(DANGER_RADIUS);
   labels.Insert("FIA ambush");
  }
 }
}
