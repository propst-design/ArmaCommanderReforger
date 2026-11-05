// #73 (srv:regress, srv:pockets on Everon's towns): garrison soldiers don't spawn inside walls. At the base with the most
// buildings round its centre, 40 spots from ACR_BaseSystem.SoldierSpot must each have room for a
// soldier, be open to the sky and lie on the soldiers' navmesh. The old way (a random spot, kept
// when FindEmptyTerrainPosition found no room within 5 m) is counted for comparison only.
// 3 Nov: FIA still spawned in walls, because the room check passed spots inside them. Points just
// inside the walls of the buildings there (found by tracing in from outside) must all be refused;
// how many the game's own cylinder finder passes is counted for comparison.
class ACR_TestGarrisonSpawn
{
 static const int SPOTS = 40;

 // navAgent: a soldier whose navmesh is used (null: any squad's leader).
 static void Check(IEntity navAgent = null)
 {
  ACR_Base densest;
  int most = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   array<IEntity> buildings = {};
   ACR_Garrison.BuildingsWithin(base.GetOrigin(), 25, buildings);
   if (buildings.Count() > most) { most = buildings.Count(); densest = base; }
  }
  if (!navAgent) navAgent = ACR_Garrison.AnyAgent();
  if (!densest || !navAgent)
  {
   ACR_TestRegress.Skip("garrison soldiers spawn clear of walls, outdoors and on the navmesh (#73)", "no base or no soldier for the navmesh");
   return;
  }
  AIPathfindingComponent pathing = AIPathfindingComponent.Cast(navAgent.FindComponent(AIPathfindingComponent));
  vector center = densest.GetOrigin();
  float spread = Math.Min(densest.m_fRadius * 0.6, 25);
  int blocked = 0;
  int roofed = 0;
  int offMesh = 0;
  int oldBad = 0;
  int unloaded = 0;
  NavmeshWorldComponent mesh;
  if (pathing) mesh = pathing.GetNavmeshComponent();
  for (int i = 0; i < SPOTS; i++)
  {
   vector spot = ACR_BaseSystem.SoldierSpot(center, spread, navAgent);
   if (!HasRoom(spot)) blocked++;
   if (ACR_BaseSystem.Roofed(spot)) roofed++;
   vector onMesh;
   if (mesh && !mesh.IsTileLoaded(spot)) unloaded++;
   else if (!pathing || !pathing.GetClosestPositionOnNavmesh(spot, Vector(1.5, 2, 1.5), onMesh)) offMesh++;
   // The old way, for comparison.
   vector old = ACR_Manager.Ground(center + Vector(Math.RandomFloat(-spread, spread), 0, Math.RandomFloat(-spread, spread)));
   vector free;
   if (SCR_WorldTools.FindEmptyTerrainPosition(free, old, 5, 0.6, 2)) old = ACR_Manager.Ground(free);
   if (!HasRoom(old) || ACR_BaseSystem.Roofed(old)) oldBad++;
  }
  int probes, inWallPassed, inWallFinder;
  WallProbes(center, probes, inWallPassed, inWallFinder);
  string detail = string.Format("%1 (%2 buildings): of %3 spots %4 blocked, %5 roofed, %6 off the navmesh (%7 not loaded); the old way %8 bad; of %9 points inside walls", densest.m_sName, most, SPOTS, blocked, roofed, offMesh, unloaded, oldBad, probes);
  detail += string.Format(" %1 passed (the game's finder alone %2)", inWallPassed, inWallFinder);
  ACR_TestRegress.Check("garrison soldiers spawn clear of walls, outdoors and on the navmesh (#73)", blocked == 0 && roofed == 0 && offMesh == 0 && inWallPassed == 0, detail);
 }

 // Room for a soldier standing here (the spot itself, not a nearby one).
 static bool HasRoom(vector spot)
 {
  return ACR_BaseSystem.RoomAt(spot);
 }

 // Points 0.12 m inside the walls of buildings round 'center': from 15 m out on four sides, a
 // line at chest height towards each building's origin; where it meets the building, a step on.
 static void WallProbes(vector center, out int probes, out int passed, out int finderPassed)
 {
  array<IEntity> buildings = {};
  ACR_Garrison.BuildingsWithin(center, 60, buildings);
  BaseWorld world = GetGame().GetWorld();
  TraceParam trace = new TraceParam();
  trace.Flags = TraceFlags.ENTS;
  array<vector> sides = {Vector(1, 0, 0), Vector(-1, 0, 0), Vector(0, 0, 1), Vector(0, 0, -1)};
  foreach (int b, IEntity building : buildings)
  {
   if (b >= 12) break;
   vector origin = building.GetOrigin();
   foreach (vector side : sides)
   {
    vector start = ACR_Manager.Ground(origin + side * 15) + Vector(0, 1, 0);
    vector end = Vector(origin[0], start[1], origin[2]);
    trace.Start = start;
    trace.End = end;
    float hit = world.TraceMove(trace, null);
    if (hit >= 1 || !trace.TraceEnt || trace.TraceEnt.GetRootParent() != building) continue;
    vector inside = ACR_Manager.Ground(start + (end - start) * hit - side * 0.12);
    probes++;
    if (ACR_BaseSystem.RoomAt(inside)) passed++;
    vector free;
    if (SCR_WorldTools.FindEmptyTerrainPosition(free, inside, 0.7, 0.6, 2) && vector.DistanceXZ(free, inside) < 0.1) finderPassed++;
   }
  }
 }
}
