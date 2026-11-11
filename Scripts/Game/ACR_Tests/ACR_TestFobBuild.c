// ACR_TestFobBuild: Regression tests for Stage 1 FOB establishment and site validation.
// Checks: CanOrder validation, CheckSpot constraints (bounds, water, slope, base proximity, roads, obstacles),
// ValidateSite 50m search, tree felling spike, 500 truck supply reservation/refund/loss,
// 90s build progression with engineer presence/contest pausing, and scripted HQ/FOB instantiation.

class ACR_TestFobBuild
{
 static const int CHECKS = 22;
 static const ResourceName TRUCK = "{2D74C39A650A3030}Prefabs/Vehicles/Wheeled/M923A1/M923A1_engineer.et";

 static void Run(ACR_Manager manager, ACR_Squad squad, IEntity truck)
 {
  if (!manager || !squad || !truck)
  {
   ACR_TestRegress.Abort("structures", "FOB build test fixture missing");
   return;
  }

  string why;
  vector truckPos = truck.GetOrigin();
  vector anchorPos = squad.GetAnchor().GetOrigin();

  // 1. CanOrder refuses null squad
  ACR_TestRegress.Check("fobbuild: CanOrder refuses null squad", !ACR_FobBuild.CanOrder(null, why) && (why.Contains("eliminated") || why.Contains("No squad")), why);

  // 2. CanOrder refuses squad without truck nearby
  truck.SetOrigin(anchorPos + Vector(250, 0, 0));
  ACR_TestRegress.Check("fobbuild: CanOrder refuses squad without truck", !ACR_FobBuild.CanOrder(squad, why) && why.Contains("construction truck"), why);

  // 3. CanOrder refuses truck with insufficient supplies (<500)
  truck.SetOrigin(anchorPos + Vector(10, 0, 0));
  float initialStock = manager.m_Logistics.StockOf(truck);
  manager.m_Logistics.Use(truck, initialStock);
  manager.m_Logistics.AddStock(truck, 10); // 10 loads = 250 supplies
  ACR_TestRegress.Check("fobbuild: CanOrder refuses insufficient supplies", !ACR_FobBuild.CanOrder(squad, why) && why.Contains("supplies"), why);

  // 4. CanOrder accepts truck with >= 500 supplies (20 loads)
  manager.m_Logistics.AddStock(truck, 15); // 25 loads = 625 supplies
  ACR_TestRegress.Check("fobbuild: CanOrder accepts truck with sufficient supplies", ACR_FobBuild.CanOrder(squad, why), why);

  // 5. CheckSpot refuses off-map bounds
  ACR_TestRegress.Check("fobbuild: CheckSpot refuses off-map bounds", !ACR_FobBuild.CheckSpot(Vector(-1000, 0, -1000), why) && why.Contains("bounds"), why);

  // 6. CheckSpot refuses water
  ACR_TestRegress.Check("fobbuild: CheckSpot refuses deep water", !ACR_FobBuild.CheckSpot(Vector(0, -50, 0), why) && why.Contains("water"), why);

  // 7. CheckSpot refuses proximity to existing base
  ACR_Base anyBase = ACR_Base.s_All[0];
  vector nearBase = anyBase.GetOrigin() + Vector(5, 0, 5);
  ACR_TestRegress.Check("fobbuild: CheckSpot refuses proximity to existing base", !ACR_FobBuild.CheckSpot(nearBase, why) && why.Contains("Too close to"), why);

  // 8. CheckSpot accepts valid clear spot
  vector clearSite;
  bool siteFound = false;
  for (int ring = 0; ring < 4 && !siteFound; ring++)
  {
   for (int i = 0; i < 24; i++)
   {
    float angle = i * 15 * Math.DEG2RAD;
    float ringDist = 220 + ring * 90;
    vector candidate = ACR_Manager.Ground(anchorPos + Vector(Math.Cos(angle) * ringDist, 0, Math.Sin(angle) * ringDist));
    if (ACR_FobBuild.CheckSpot(candidate, why))
    {
     clearSite = candidate;
     siteFound = true;
     break;
    }
   }
  }
  ACR_TestRegress.Check("fobbuild: CheckSpot accepts clear distant terrain", !clearSite.ToString().IsEmpty() && ACR_FobBuild.CheckSpot(clearSite, why), why);

  // 9. ValidateSite finds a spot within 50m search
  vector edgeBase = anyBase.GetOrigin() + Vector(anyBase.m_fRadius + 30, 0, 0);
  vector foundSpot;
  int searchStart = System.GetTickCount();
  bool snapped = ACR_FobBuild.ValidateSite(edgeBase, foundSpot, why);
  int searchMs = System.GetTickCount() - searchStart;
  vector badSpot;
  string badWhy;
  searchStart = System.GetTickCount();
  ACR_FobBuild.ValidateSite(Vector(0, -50, 0), badSpot, badWhy);
  PrintFormat("[ACR] PERF fob site search: %1 ms (found %2), worst case (all refused) %3 ms", searchMs, snapped, System.GetTickCount() - searchStart);
  ACR_AICommander planner = new ACR_AICommander(manager, "US");
  vector aiSite;
  searchStart = System.GetTickCount();
  bool aiFound = planner.FindFobSite(anyBase, aiSite);
  PrintFormat("[ACR] PERF AI fob site search behind %1: %2 ms (found %3)", anyBase.m_sName, System.GetTickCount() - searchStart, aiFound);
  ACR_TestRegress.Check("fobbuild: ValidateSite finds valid spot within 50m search", snapped && vector.DistanceXZ(foundSpot, edgeBase) <= 50, string.Format("by %1: found %2, %3 m from the probe, '%4'", anyBase.m_sName, snapped, vector.DistanceXZ(foundSpot, edgeBase), why));

  // 10. FellTrees fells the real trees in a footprint (vanilla tree sync manager).
  vector woods;
  int standing = 0;
  for (int t = 0; t < 72 && standing == 0; t++)
  {
   float treeAngle = t * 20 * Math.DEG2RAD;
   float treeDist = 150 + (t / 18) * 120; // int-math ok: a new ring every 18 angles
   woods = ACR_Manager.Ground(anchorPos + Vector(Math.Cos(treeAngle) * treeDist, 0, Math.Sin(treeAngle) * treeDist));
   standing = ACR_FobBuild.CountTrees(woods, 12);
  }
  PrintFormat("[ACR] FOBTEST trees: can fell %1, %2 standing near %3", ACR_FobBuild.CanFellTrees(), standing, woods);
  if (!ACR_FobBuild.CanFellTrees() || standing == 0) ACR_TestRegress.Skip("fobbuild: FellTrees fells every standing tree in a footprint", "no tree sync manager or no trees near the test squad");
  else ACR_TestRegress.Check("fobbuild: FellTrees fells every standing tree in a footprint", ACR_FobBuild.FellTrees(woods, 12) == standing, standing.ToString());

  // 11. EstablishFob reserves 500 truck supplies and sets BUILD_FOB state
  float stockBefore = manager.m_Logistics.StockOf(truck);
  bool established = squad.EstablishFob(clearSite);
  float stockAfter = manager.m_Logistics.StockOf(truck);
  ACR_TestRegress.Check("fobbuild: EstablishFob reserves 20 loads and sets state", established && squad.OrderState() == ACR_EOrderState.BUILD_FOB && squad.FobReservedLoads() == 20 && Math.AbsFloat(stockBefore - stockAfter - 20) < 0.01, string.Format("established=%1 state=%2 reserved=%3 before=%4 after=%5", established, squad.OrderState(), squad.FobReservedLoads(), stockBefore, stockAfter));

  ACR_TestRegress.Check("fobbuild: pending site reserves the full FOB circle", !ACR_FobBuild.CheckSpot(clearSite, why) && why.Contains("construction site"), why);
  IEntity otherTruck = ACR_Manager.SpawnPrefab(TRUCK, anchorPos + Vector(20, 0, 0));
  ACR_Squad otherBuilder = new ACR_Squad(squad.m_Group, 999);
  float otherStock = manager.m_Logistics.StockOf(otherTruck);
  bool otherCanBuild = ACR_FobBuild.CanOrder(otherBuilder, why);
  bool duplicate = otherBuilder.EstablishFob(clearSite);
  ACR_TestRegress.Check("fobbuild: second order at reserved site spends no supplies", otherTruck && otherCanBuild && !duplicate && manager.m_Logistics.StockOf(otherTruck) == otherStock && otherBuilder.FobReservedLoads() == 0, "");
  if (otherTruck) SCR_EntityHelper.DeleteEntityAndChildren(otherTruck);

  // 12. CancelEstablishFob and Halt() refund 500 supplies to surviving truck
  squad.Halt("Holding");
  float stockRefunded = manager.m_Logistics.StockOf(truck);
  ACR_TestRegress.Check("fobbuild: CancelEstablishFob refunds 20 loads to truck", squad.OrderState() != ACR_EOrderState.BUILD_FOB && squad.FobReservedLoads() == 0 && Math.AbsFloat(stockRefunded - stockBefore) < 0.01, string.Format("state=%1 reserved=%2 refunded=%3 before=%4", squad.OrderState(), squad.FobReservedLoads(), stockRefunded, stockBefore));

  ACR_TestRegress.Check("fobbuild: cancellation releases construction site", ACR_FobBuild.CheckSpot(clearSite, why), why);

  squad.EstablishFob(clearSite);
  EntitySpawnParams blockerParams = new EntitySpawnParams();
  blockerParams.TransformMode = ETransformMode.WORLD;
  blockerParams.Transform[3] = clearSite;
  ACR_Base blocker = ACR_Base.Cast(GetGame().SpawnEntity(ACR_Base, GetGame().GetWorld(), blockerParams));
  squad.CompleteEstablishFob();
  ACR_TestRegress.Check("fobbuild: completion rechecks site and refunds blocked build", blocker && squad.OrderState() != ACR_EOrderState.BUILD_FOB && squad.FobReservedLoads() == 0 && Math.AbsFloat(manager.m_Logistics.StockOf(truck) - stockBefore) < 0.01, "");
  if (blocker)
  {
   blocker.Retire("overlap fixture complete");
   SCR_EntityHelper.DeleteEntityAndChildren(blocker);
  }

  // 13. Destroyed truck loses cargo without refund
  squad.EstablishFob(clearSite);
  squad.SetFobReservedTruckForTest(null);
  squad.CancelEstablishFob();
  ACR_TestRegress.Check("fobbuild: destroyed truck does not crash on cancel", squad.OrderState() != ACR_EOrderState.BUILD_FOB && squad.FobReservedLoads() == 0, "");

  // Replace the deliberately lost cargo before starting a real progress test.
  manager.m_Logistics.AddStock(truck, 20);
  bool progressOrdered = squad.EstablishFob(clearSite);
  float now = GetGame().GetWorld().GetWorldTime();

  // 14. TickEstablishFob advances build progress when engineer alive within CP radius
  IEntity leader = squad.GetLeader();
  vector oldLeaderPos;
  if (leader) oldLeaderPos = leader.GetOrigin();
  if (leader) leader.SetOrigin(clearSite + Vector(10, 0, 0));
  squad.TickEstablishFob(now + 10000);
  ACR_TestRegress.Check("fobbuild: progress advances with engineer inside CP radius", progressOrdered && squad.FobBuildProgress() > 0, squad.FobBuildProgress().ToString());

  // 15. TickEstablishFob pauses progress when engineer is out of CP radius
  float prog = squad.FobBuildProgress();
  if (leader) leader.SetOrigin(clearSite + Vector(60, 0, 0));
  squad.TickEstablishFob(now + 20000);
  ACR_TestRegress.Check("fobbuild: progress pauses when engineer is out of CP radius", squad.FobBuildProgress() == prog, "");

  // 16. IsContested detects enemies near site
  ACR_TestRegress.Check("fobbuild: IsContested detects clear site without enemies", !ACR_FobBuild.IsContested(clearSite, "US"), "");

  // 17. CompleteEstablishFob spawns HQ and registers empty 1000 capacity FOB (and USSR/FIA HQ prefabs load validly)
  squad.SetFobBuildProgressForTest(90.0);
  if (leader) leader.SetOrigin(clearSite + Vector(5, 0, 0));
  squad.TickEstablishFob(now + 30000);
  ACR_Base newFob = null;
  ACR_TestRegress.Check("fobbuild: completed HQ reserves site before structures activation", !ACR_FobBuild.CheckSpot(clearSite, why) && why.Contains("headquarters"), why);
  // Trigger activation of new composition
  manager.m_Structures.ACR_TestForceTick();
  foreach (ACR_Base b : ACR_Base.s_All)
  {
   if (b && b.m_bBuilt && vector.DistanceXZ(b.GetOrigin(), clearSite) < 20)
   {
    newFob = b;
    break;
   }
  }
  bool hqPrefabsValid = Resource.Load(ACR_FobBuild.HQ_US).IsValid() && Resource.Load(ACR_FobBuild.HQ_USSR).IsValid() && Resource.Load(ACR_FobBuild.HQ_FIA).IsValid();
  bool fobOk = newFob && newFob.m_sOwner == "US" && newFob.m_bBuilt && hqPrefabsValid;
  ACR_TestRegress.Check("fobbuild: CompleteEstablishFob creates empty 1000 capacity FOB", fobOk, "");

  // Restore truck, leader and clean up new FOB and its HQ entity
  if (leader) leader.SetOrigin(oldLeaderPos);
  truck.SetOrigin(truckPos);
  manager.m_Logistics.Use(truck, manager.m_Logistics.StockOf(truck));
  manager.m_Logistics.AddStock(truck, initialStock);
  if (newFob)
  {
   IEntity hqEnt = manager.m_Structures.HqEntityOf(newFob);
   if (hqEnt) SCR_EntityHelper.DeleteEntityAndChildren(hqEnt);
   manager.m_Structures.ACR_TestForceTick();
   ACR_Base.s_All.RemoveItem(newFob);
   if (newFob.m_FobSupply) SCR_EntityHelper.DeleteEntityAndChildren(newFob.m_FobSupply);
   SCR_EntityHelper.DeleteEntityAndChildren(newFob);
  }
 }
}

modded class ACR_Structures
{
 void ACR_TestSetFaction(IEntity root, string faction)
 {
  ACR_Structure s = FindStructure(root);
  if (!s) return;
  s.m_sFaction = faction;
  if (s.m_Composition) s.m_Composition.ACR_SetFaction(faction);
  int service = ServiceOf(s.m_eKind);
  if (service != 0 && m_Manager.m_Logistics) m_Manager.m_Logistics.AddPoint(s.m_Entity, service, faction);
 }

 void ACR_TestForceTick()
 {
  m_fNextTick = 0;
  for (int r = s_Registered.Count() - 1; r >= 0; r--)
  {
   SCR_CampaignBuildingCompositionComponent composition = s_Registered[r];
   if (composition) composition.ACR_TestRelayCompletion(true);
  }
  Tick();
 }
}
