// srv:structures: an AI FOB build ends when the truck's squad spawns the HQ, but the FOB itself only
// appears on the next structures tick (every 2 s). An AI commander's check in that gap used to put
// the new FOB's own site on its avoid list ("FOB established", then "FOB site ... abandoned,
// avoiding it for 10 minutes" 2 s later: an Everon soak, 27 Oct). A build that ends without a FOB
// still avoids its site.
class ACR_TestFobBuiltRace
{
 static const int CHECKS = 3; // (one per Check below)

 static void Run(ACR_Manager manager, ACR_Squad alpha, IEntity truck)
 {
  if (!alpha || !truck || !alpha.GetAnchor()) return;
  vector truckPos = truck.GetOrigin();
  float truckStock = manager.m_Logistics.StockOf(truck);
  manager.m_Logistics.AddStock(truck, ACR_FobBuild.LOADS_RESERVED);
  truck.SetOrigin(alpha.GetAnchor().GetOrigin() + Vector(10, 0, 0));

  vector site;
  bool found = Site(alpha.GetAnchor().GetOrigin(), site);
  bool ordered = found && alpha.EstablishFob(site);
  if (ordered) alpha.CompleteEstablishFob();
  int fobsBefore = FobsAt(site);

  ACR_TestFobRacePlanner planner = new ACR_TestFobRacePlanner(manager, "US");
  planner.Track(alpha, site);
  bool busy = planner.HasActiveFobProject();
  bool avoided = planner.Avoided(site);
  ACR_TestRegress.Check("fobbuiltrace: the AI doesn't avoid the site of a FOB it just built, before the FOB appears", ordered && fobsBefore == 0 && !busy && !avoided, string.Format("site found %1, built %2, FOBs there yet %3, still busy %4, avoided %5", found, ordered && alpha.OrderState() != ACR_EOrderState.BUILD_FOB, fobsBefore, busy, avoided));

  manager.m_Structures.ACR_TestForceTick();
  ACR_TestRegress.Check("fobbuiltrace: the FOB appears on the next structures tick", FobsAt(site) == 1, string.Format("%1 FOBs there", FobsAt(site)));

  // A build that ended short of a FOB (given up, or the squad wiped out) still avoids its site.
  vector other = site + Vector(60, 0, 0);
  planner.Track(alpha, other);
  planner.HasActiveFobProject();
  ACR_TestRegress.Check("fobbuiltrace: a build that ended without a FOB still avoids its site", planner.Avoided(other), "");

  IEntity hq = alpha.FobBuiltHqForTest();
  if (hq) SCR_EntityHelper.DeleteEntityAndChildren(hq);
  manager.m_Structures.ACR_TestForceTick();
  alpha.m_iAITask = ACR_EAITask.FREE;
  truck.SetOrigin(truckPos);
  manager.m_Logistics.Use(truck, manager.m_Logistics.StockOf(truck));
  manager.m_Logistics.AddStock(truck, truckStock);
 }

 // A clear spot for a new FOB 150-600 m out, outside every base.
 static bool Site(vector from, out vector site)
 {
  for (int ring = 150; ring <= 600; ring += 75)
  {
   for (int bearing = 0; bearing < 360; bearing += 30)
   {
    float rad = bearing * Math.DEG2RAD;
    vector spot = ACR_Manager.Ground(from + Vector(Math.Sin(rad) * ring, 0, Math.Cos(rad) * ring));
    bool inBase = false;
    foreach (ACR_Base base : ACR_Base.s_All)
     if (base && !base.m_bRetired && vector.DistanceXZ(base.GetOrigin(), spot) <= base.m_fRadius + ACR_Structures.FOB_RADIUS + 10) inBase = true;
    string refusal;
    if (inBase || !ACR_FobBuild.CheckSpot(spot, refusal)) continue;
    site = spot;
    return true;
   }
  }
  return false;
 }

 static int FobsAt(vector site)
 {
  int count;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && base.m_bBuilt && !base.m_bRetired && vector.DistanceXZ(base.GetOrigin(), site) < 20) count++;
  return count;
 }
}

// The AI commander's FOB tracking, set directly: this builder, this site.
class ACR_TestFobRacePlanner : ACR_AICommander
{
 void ACR_TestFobRacePlanner(ACR_Manager manager, string factionKey)
 {
  m_Manager = manager;
  m_sFaction = factionKey;
 }
 void Track(ACR_Squad builder, vector site)
 {
  m_FobBuilder = builder;
  m_vFobTargetSite = site;
 }
}
