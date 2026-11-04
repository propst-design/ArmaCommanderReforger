// -acrFieldRoutesTest 1 (server-capable, ~3 min): foot squads off the roads and over bridges
// (3 Nov 2026). 1) The map's bridges: a road over water too deep to wade, with a deck. A route
// across each goes over it (a leg at each end) and no other leg has deep
// water between it and the one before; BBB's road exit leaves a soldier on the deck alone.
// 2) Alpha (field legs) and Bravo (one waypoint: the game's own path) walk the same straight
// road stretch; the share of soldier-seconds on the road is counted for each. The test range
// (Arland) has no road over deep water: -acrFieldBridgesTest 1 runs the bridge checks on Everon.
class ACR_TestFieldRoutes
{
 static const float STRETCH_MIN = 550;   // a straight road stretch this long...
 static const float STRETCH_MAX = 750;
 static const float STRETCH_BEND = 20;   // ...whose road keeps within this of the line
 static const float ON_ROAD = 3;         // within this of a road's edge: on it
 static const float START_SKIP = 50;     // the first metres from the start (on the road) don't count
 static const float ROAD_SHARE_MAX = 0.2;
 static const int ORDER_TICK = 16;
 static const int WALK_S = 150;
 static const int EXPECTED = 7;
 static const int BRIDGE_EXPECTED = 4;
 static vector s_vFrom, s_vTo;
 static int s_iAlphaOn, s_iAlphaAll, s_iBravoOn, s_iBravoAll;
 static bool s_bReady;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
   ACR_Squad.s_bFieldAnywhere = true;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (!alpha || !bravo || !alpha.GetAnchor() || !bravo.GetAnchor())
  {
   if (tick > 60) ACR_TestRegress.Abort("fieldroutes", "no Alpha/Bravo");
   return;
  }
  if (tick == 4)
  {
   Bridges(alpha);
   if (!PickStretch(h.m_Range.m_Rally))
   {
    ACR_TestRegress.Abort("fieldroutes", "no straight dry road stretch near the range");
    return;
   }
   Place(alpha, s_vFrom);
   Place(bravo, s_vFrom - (s_vTo - s_vFrom).Normalized() * 8);
   NavmeshWorldComponent mesh = GetGame().GetAIWorld().GetNavmeshWorldComponent("Soldiers");
   vector dir = (s_vTo - s_vFrom).Normalized();
   vector across = Vector(-dir[2], 0, dir[0]);
   for (float along = 0; along <= vector.DistanceXZ(s_vFrom, s_vTo); along += 60)
    for (int side = -2; side <= 2; side++) mesh.LoadTileIn(s_vFrom + dir * along + across * (side * 60));
   s_bReady = true;
  }
  if (!s_bReady) return;
  if (tick == ORDER_TICK)
  {
   array<vector> legs = {};
   int planStarted = System.GetTickCount();
   bool field = alpha.FieldLegs(s_vFrom, s_vTo, legs);
   PrintFormat("[ACR] FIELD TEST stretch %1 -> %2 (%3 m), %4 field legs in %5 ms", s_vFrom, s_vTo, Math.Round(vector.DistanceXZ(s_vFrom, s_vTo)), legs.Count(), System.GetTickCount() - planStarted);
   foreach (vector leg : legs) PrintFormat("[ACR] FIELD TEST   leg %1, %2 m off the road", leg, Math.Round(ACR_Minefields.RoadClearance(leg)));
   ACR_TestRegress.Check("field routes: a road stretch off friendly ground gets field legs, at most 70 m apart along the line", field && legs.Count() >= 6 && MaxGap(s_vFrom, legs) <= 70, string.Format("planned %1, %2 legs, widest gap %3 m", field, legs.Count(), Math.Round(MaxGap(s_vFrom, legs))));
   alpha.RouteTo(s_vTo, ACR_Squad.WP_MOVE, -1, "Moving", 0);
   bravo.MoveTo(s_vTo);
  }
  if (tick > ORDER_TICK + 3 && tick <= ORDER_TICK + WALK_S)
  {
   Count(alpha, s_iAlphaOn, s_iAlphaAll);
   Count(bravo, s_iBravoOn, s_iBravoAll);
  }
  if (tick == ORDER_TICK + WALK_S)
  {
   float alphaShare = Share(s_iAlphaOn, s_iAlphaAll);
   float bravoShare = Share(s_iBravoOn, s_iBravoAll);
   PrintFormat("[ACR] FIELD TEST on the road: Alpha (field legs) %1 of %2 soldier-seconds (%3 pct), Bravo (one waypoint) %4 of %5 (%6 pct)", s_iAlphaOn, s_iAlphaAll, Math.Round(alphaShare * 100), s_iBravoOn, s_iBravoAll, Math.Round(bravoShare * 100));
   float made = vector.DistanceXZ(s_vFrom, alpha.GetAnchor().GetOrigin());
   ACR_TestRegress.Check("field routes: Alpha made its way along the stretch", made >= 300, string.Format("%1 m from the start", Math.Round(made)));
   ACR_TestRegress.Check("field routes: Alpha on its field legs keeps off the road", s_iAlphaAll >= 60 && ROAD_SHARE_MAX >= alphaShare, string.Format("%1 of %2 soldier-seconds on the road (Bravo on one waypoint: %3 of %4)", s_iAlphaOn, s_iAlphaAll, s_iBravoOn, s_iBravoAll));
   ACR_Squad.s_bFieldAnywhere = false;
   ACR_TestRegress.Complete("fieldroutes", EXPECTED);
  }
 }

 // -acrFieldBridgesTest 1 (Everon, ~1 min, from ACR_Mode): the bridge checks on a map with rivers.
 static void RunBridges(int seconds)
 {
  if (seconds == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
   ACR_Squad.s_bFieldAnywhere = true;
  }
  if (seconds < 20 || ACR_TestRegress.s_bCompleted) return;
  // Any squad will do (FieldLegs wants its side); a generated map may take a while to field one.
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Squad squad;
  if (manager) foreach (ACR_Squad candidate : manager.m_Squads) if (!squad && candidate && candidate.GetFaction()) squad = candidate;
  if (!squad)
  {
   if (seconds >= 90) ACR_TestRegress.Abort("fieldbridges", "no squad on the map");
   return;
  }
  Bridges(squad);
  ACR_Squad.s_bFieldAnywhere = false;
  ACR_TestRegress.Complete("fieldbridges", BRIDGE_EXPECTED);
 }

 // Every road over deep water on the map; the bridge nearest the range gets the route checks.
 protected static void Bridges(ACR_Squad alpha)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  vector mins, maxs;
  GetGame().GetWorld().GetBoundBox(mins, maxs);
  array<BaseRoad> roads = {};
  aiWorld.GetRoadNetworkManager().GetRoadsInAABB(mins, maxs, roads);
  int started = System.GetTickCount();
  int wetRoads, fords, bridges, samples, anyWater;
  float deepest;
  vector deepestAt;
  array<vector> allNear = {};
  array<vector> allFar = {};
  array<vector> allWet = {};
  vector range = (mins + maxs) * 0.5;
  array<vector> points = {};
  array<vector> seen = {};
  foreach (BaseRoad road : roads)
  {
   points.Clear();
   road.GetPoints(points);
   bool wet;
   for (int i = 1; i < points.Count() && !wet; i++)
   {
    int steps = Math.Max(1, Math.Ceil(vector.DistanceXZ(points[i - 1], points[i]) / 10));
    for (int s = 0; s < steps && !wet; s++)
    {
     vector p = points[i - 1] + (points[i] - points[i - 1]) * (s / (float)steps);
     samples++;
     float depth = BBB_Terrain.WaterDepthAt(p);
     if (depth > 0) anyWater++;
     if (depth > deepest) { deepest = depth; deepestAt = p; }
     if (BBB_Terrain.WADE_DEPTH > depth) continue;
     wet = true;
     wetRoads++;
     vector nearEnd, farEnd, nearApproach, farApproach;
     if (!BBB_Terrain.BridgeNear(p, range, 40, nearEnd, farEnd, nearApproach, farApproach)) { fords++; continue; }
     bool known;
     foreach (vector other : seen) if (vector.DistanceXZ(other, nearEnd) < 20) known = true;
     if (known) continue;
     seen.Insert(nearEnd);
     bridges++;
     PrintFormat("[ACR] FIELD TEST bridge %1 to %2 (%3 m), sea-level water test there %4", nearEnd, farEnd, Math.Round(vector.DistanceXZ(nearEnd, farEnd)), BBB_Terrain.IsWaterAt((nearEnd + farEnd) * 0.5));
     allNear.Insert(nearEnd);
     allFar.Insert(farEnd);
     allWet.Insert(p);
    }
   }
  }
  PrintFormat("[ACR] FIELD TEST %1 roads, %2 over deep water: %3 bridges, %4 without a deck (%5 ms)", roads.Count(), wetRoads, bridges, fords, System.GetTickCount() - started);
  PrintFormat("[ACR] FIELD TEST %1 road samples, %2 in any water, deepest %3 m at %4; world box %5 to %6", samples, anyWater, deepest, deepestAt, mins, maxs);
  if (bridges == 0)
  {
   ACR_TestRegress.Skip("water: a river or pond under a bridge reads as deep water, though the sea-level test misses it", "no bridge over deep water on this map");
   ACR_TestRegress.Skip("bridge route: a route across a river goes over the bridge, a leg at each end, on and off it on its own side", "no bridge on this map");
   ACR_TestRegress.Skip("bridge route: no leg has deep water between it and the one before", "no bridge on this map");
   ACR_TestRegress.Skip("road exit: a soldier on a bridge deck isn't sent over the side", "no bridge on this map");
   return;
  }
  // Every bridge: a route across it along its line, both ways out, and a soldier on its deck.
  int deepOk, planned, overOk, dryOk, exitOk;
  foreach (int b, vector bridgeNear : allNear)
  {
   vector bridgeFar = allFar[b];
   vector middle = (bridgeNear + bridgeFar) * 0.5;
   // The water under its road, where the survey found it (a viaduct's middle may be over land).
   vector wet = allWet[b];
   if (BBB_Terrain.IsDeepWaterAt(wet) && !BBB_Terrain.IsWaterAt(wet)) deepOk++;
   else PrintFormat("[ACR] FIELD TEST bridge %1: water under it %2 m, sea-level test %3", b, BBB_Terrain.WaterDepthAt(wet), BBB_Terrain.IsWaterAt(wet));
   vector axis = bridgeFar - bridgeNear;
   axis[1] = 0;
   axis.Normalize();
   array<vector> legs = {};
   bool routed;
   vector from, to;
   for (float reach = 450; reach >= 250 && !routed; reach -= 50)
   {
    from = ACR_Manager.Ground(middle - axis * reach);
    to = ACR_Manager.Ground(middle + axis * reach);
    if (BBB_Terrain.IsDeepWaterAt(from) || BBB_Terrain.IsDeepWaterAt(to)) continue;
    int planStarted = System.GetTickCount();
    routed = alpha.FieldLegs(from, to, legs);
    if (routed) PrintFormat("[ACR] FIELD TEST bridge %1: route over %2 m, %3 legs in %4 ms", b, reach * 2, legs.Count(), System.GetTickCount() - planStarted);
   }
   if (routed)
   {
    planned++;
    // The bridge's two ends in a row (either order: 'from' may lie on either side), the way off on the far side.
    int over = -1;
    bool flipped;
    for (int k = 0; k + 1 < legs.Count(); k++)
    {
     if (vector.DistanceXZ(legs[k], bridgeNear) < 8 && vector.DistanceXZ(legs[k + 1], bridgeFar) < 8) over = k;
     if (vector.DistanceXZ(legs[k], bridgeFar) < 8 && vector.DistanceXZ(legs[k + 1], bridgeNear) < 8) { over = k; flipped = true; }
    }
    vector landed = bridgeFar;
    vector left = bridgeNear;
    if (flipped) { landed = bridgeNear; left = bridgeFar; }
    bool offFar = over >= 0 && (over + 2 >= legs.Count() || vector.DistanceXZ(legs[over + 2], landed) < vector.DistanceXZ(legs[over + 2], left));
    if (over >= 0 && offFar) overOk++;
    else foreach (vector leg : legs) PrintFormat("[ACR] FIELD TEST bridge %1:   leg %2 (over at %3, way off on the far side %4)", b, leg, over, offFar);
    float wetWorst;
    vector previous = from;
    for (int n = 0; n < legs.Count(); n++)
    {
     float wetHere = BBB_Terrain.DeepWaterAlong(previous, legs[n]);
     if (n != over + 1 || over < 0) wetWorst = Math.Max(wetWorst, wetHere);
     if (wetHere > 0 && n != over + 1) PrintFormat("[ACR] FIELD TEST bridge %1:   %2 m of deep water from %3 to leg %4 %5", b, wetHere, previous, n, legs[n]);
     previous = legs[n];
    }
    if (wetWorst <= 0) dryOk++;
   }
   else
   {
    PrintFormat("[ACR] FIELD TEST bridge %1: no route across (other deep water without a bridge near)", b);
   }
   // BBB's road exit: a soldier on the deck stays to vanilla's cover search.
   TraceParam trace = new TraceParam();
   trace.Start = wet + Vector(0, 40, 0);
   trace.End = Vector(wet[0], GetGame().GetWorld().GetSurfaceY(wet[0], wet[2]), wet[2]);
   trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
   float fraction = GetGame().GetWorld().TraceMove(trace, null);
   vector deck = trace.Start + (trace.End - trace.Start) * fraction + Vector(0, 0.1, 0);
   vector spot;
   int exitResult = BBB_RoadExit.OffRoadSpot(deck, deck + axis * 200, spot);
   if (exitResult == 0) exitOk++;
   else PrintFormat("[ACR] FIELD TEST bridge %1: deck at %2 (%3 m up), road exit %4 to %5", b, deck, deck[1] - GetGame().GetWorld().GetSurfaceY(deck[0], deck[2]), exitResult, spot);
  }
  ACR_TestRegress.Check("water: a river or pond under a bridge reads as deep water, though the sea-level test misses it", deepOk == bridges, string.Format("%1 of %2 bridges", deepOk, bridges));
  ACR_TestRegress.Check("bridge route: a route across a river goes over the bridge, a leg at each end, on and off it on its own side", planned > 0 && overOk == planned, string.Format("%1 of %2 routes (%3 bridges)", overOk, planned, bridges));
  ACR_TestRegress.Check("bridge route: no leg has deep water between it and the one before", planned > 0 && dryOk == planned, string.Format("%1 of %2 routes", dryOk, planned));
  ACR_TestRegress.Check("road exit: a soldier on a bridge deck isn't sent over the side", exitOk == bridges, string.Format("%1 of %2 decks", exitOk, bridges));
 }

 // A straight, dry road stretch near the range: s_vFrom/s_vTo.
 protected static bool PickStretch(vector range)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  array<BaseRoad> roads = {};
  aiWorld.GetRoadNetworkManager().GetRoadsInAABB(range - Vector(2000, 1000, 2000), range + Vector(2000, 3000, 2000), roads);
  float best = float.MAX;
  array<vector> points = {};
  foreach (BaseRoad road : roads)
  {
   points.Clear();
   road.GetPoints(points);
   for (int i = 0; i < points.Count(); i++)
   {
    for (int j = i + 1; j < points.Count(); j++)
    {
     float chord = vector.DistanceXZ(points[i], points[j]);
     if (STRETCH_MIN > chord) continue;
     if (chord > STRETCH_MAX) break;
     bool straight = true;
     for (int k = i + 1; k < j && straight; k++) straight = ACR_AICommander.DistanceToSegment(points[k], points[i], points[j]) <= STRETCH_BEND;
     if (!straight) break;
     float d = vector.DistanceXZ(points[i], range);
     if (d >= best || BBB_Terrain.DeepWaterAlong(points[i], points[j]) > 0) continue;
     best = d;
     s_vFrom = ACR_Manager.Ground(points[i]);
     s_vTo = ACR_Manager.Ground(points[j]);
    }
   }
  }
  return best < float.MAX;
 }

 protected static void Place(ACR_Squad squad, vector at)
 {
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (int n, AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   int column = n % 3;
   int row = n / 3;
   if (body) body.SetOrigin(ACR_Manager.Ground(at + Vector(column * 2, 0, row * 2)));
  }
 }

 // Soldier-seconds on the road, past the start and short of the approach, where field legs apply.
 protected static void Count(ACR_Squad squad, inout int onRoad, inout int all)
 {
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   vector p = body.GetOrigin();
   if (START_SKIP > vector.DistanceXZ(p, s_vFrom) || ACR_Squad.FIELD_APPROACH > vector.DistanceXZ(p, s_vTo)) continue;
   all++;
   if (ON_ROAD > ACR_Minefields.RoadClearance(p)) onRoad++;
  }
 }

 // Widest gap between legs along the line (a leg may cross to the road's other side). The last
 // leg is the destination, past the approach the legs leave alone.
 protected static float MaxGap(vector from, array<vector> legs)
 {
  if (legs.IsEmpty()) return 0;
  vector dir = legs[legs.Count() - 1] - from;
  dir[1] = 0;
  dir.Normalize();
  float widest;
  float previous = 0;
  for (int i = 0; i + 1 < legs.Count(); i++)
  {
   float along = vector.Dot(legs[i] - from, dir);
   widest = Math.Max(widest, along - previous);
   previous = along;
  }
  return widest;
 }

 protected static float Share(int part, int whole)
 {
  if (whole <= 0) return 0;
  return part / (float)whole;
 }
}
