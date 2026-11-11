// srv:containment (Everon whole map, about 2.5 min): a contained outbreak (ACR_Containment, #107,
// -acrOutbreak contain). Without Project Outbreak loaded: no nests, no Outbreak start row.
// With it (swap Outbreak into tools/testserver-args.txt for the run): one nest only, ground zero,
// clear of both starting bases; civilians in the towns round it; both sides mark it. Ground zero
// sends a horde and the town it goes for gets civilians; evac (ACR_Evac): no XP far from a town, an
// XP by one has its people walk out with a few left behind, a running helicopter by it is heard,
// a US truck parked on it gets them aboard, and brought to the US HQ they pay on delivery; a horde
// reaching civilians with nobody there takes them; an AI commander sets an XP and sends a free
// truck (kept out of rides meanwhile); ground zero's bounty and the RP cap; a town taken near a nest is counterattacked
// (once per place); the spread is reported; messages name towns (#241); left alone it escalates and
// hordes stay 15+ (#240).
class ACR_TestContainment
{
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static const ResourceName HELI = "{70BAEEFC2D3FEE64}Prefabs/Vehicles/Helicopters/UH1H/UH1H.et";
 static const int EXPECTED = 21;
 protected static ACR_DefenderPost s_Zero;
 protected static ref ACR_EvacPoint s_Evac; // ref: a finished XP leaves the list
 protected static IEntity s_Heli;
 protected static int s_iTownPeople;
 protected static float s_fFootBefore;
 protected static string s_sHome;
 protected static int s_iRPBefore;
 protected static int s_iLostBefore;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 5) SetHour(12);
  if (second == 10)
  {
   if (!ACR_Outbreak.Loaded()) { WithoutOutbreak(); return; }
   Placement(manager);
  }
  if (!s_Zero) return;
  if (second == 12) HordeSurvivors();
  if (second == 14) EvacSet(manager);
  if (second == 17) Noise();
  if (second == 44) Loaded(manager);
  if (second == 50) Delivered(manager);
  if (second == 52) SendToSurvivors();
  if (second == 70) Overrun();
  if (second == 72) AIEvacSetup(manager);
  if (second == 84) AIEvac(manager);
  if (second == 86) Bounty(manager);
  if (second == 88) Counterattack();
  if (second == 130) Spread();
 }

 protected static void SetHour(int hour)
 {
  ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
  if (world && world.GetTimeAndWeatherManager()) world.GetTimeAndWeatherManager().SetHoursMinutesSeconds(hour, 0, 0, true);
 }

 protected static void WithoutOutbreak()
 {
  int nests;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts) if (post.m_Nest) nests++;
  Check("containment: without Outbreak, no nests and nothing contained", nests == 0 && !ACR_Containment.On(), string.Format("%1 nests", nests));
  Check("containment: without Outbreak, no Outbreak start setup row", !ACR_Setup.Rows().Contains("outbreak\t"), "");
  ACR_TestRegress.Complete("containment-off", 2);
 }

 protected static void Placement(ACR_Manager manager)
 {
  int nests;
  string detail = "";
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Nest) continue;
   nests++;
   if (ACR_Containment.IsGroundZero(post)) s_Zero = post;
   detail += string.Format(" %1(%2)", post.m_sName, post.m_iReserve);
  }
  Check("containment: the outbreak starts as one nest, ground zero", ACR_Containment.On() && nests == 1 && s_Zero != null, string.Format("on %1, nests %2:%3", ACR_Containment.On(), nests, detail));
  if (!s_Zero) { ACR_TestRegress.Complete("containment", EXPECTED); return; }
  bool clear = true;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (!base.m_sOwner.IsEmpty() && !base.m_bPocket && !base.m_bRetired && vector.DistanceXZ(base.GetOrigin(), s_Zero.m_vPos) < ACR_Outbreak.HQ_CLEAR) clear = false;
  Check("containment: ground zero is clear of both starting bases", clear, ACR_Containment.GroundZero());
  Check("containment: towns round ground zero have civilians holding out from the start", ACR_Containment.SiteCount() >= 1, string.Format("%1 sites", ACR_Containment.SiteCount()));
  Check("containment: both sides mark ground zero as a zombie area", Area("US", s_Zero.m_vPos) && Area("USSR", s_Zero.m_vPos), "");
 }

 protected static bool Area(string side, vector pos)
 {
  foreach (ACR_DangerRoad road : ACR_Insurgency.s_aRoads)
   if (road.m_bZombies && road.m_sSide == side && vector.DistanceXZ(road.m_vPos, pos) < ACR_Outbreak.ZONE_RADIUS) return true;
  return false;
 }

 protected static void HordeSurvivors()
 {
  int before = ACR_Containment.s_iSitesMade;
  vector goal;
  string goalName;
  bool sent = ACR_Outbreak.PickGoal(s_Zero.m_vPos, goal, goalName) && ACR_Outbreak.SendHorde(s_Zero, 6, goal, goalName);
  // A town that already had a site isn't given another: count a site within reach of the goal.
  bool there;
  for (int i = 0; i < ACR_Containment.SiteCount(); i++)
   if (vector.DistanceXZ(ACR_Containment.SiteAt(i).m_vPos, goal) < ACR_Containment.OVERRUN_REACH) there = true;
  Check("containment: a horde from ground zero leaves civilians to save in the town it goes for", sent && there, string.Format("to %1, sent %2, sites made %3 -> %4", goalName, sent, before, ACR_Containment.s_iSitesMade));
 }

 // An XP 50 m from the first town holding out: refused far from any town, set by it; its people
 // walk, the stragglers stay; a US truck parked on it and a running helicopter beside it.
 protected static void EvacSet(ACR_Manager manager)
 {
  if (ACR_Containment.SiteCount() == 0) { Check("evac: a town holding out to evacuate", false, "no site"); return; }
  ACR_SurvivorSite site = ACR_Containment.SiteAt(0);
  s_iTownPeople = site.m_iCount;
  ACR_EvacPoint none;
  string far = ACR_Evac.Place("US", Vector(10, 0, 10), none);
  Check("evac: no evac point far from any town holding out", !far.IsEmpty() && !none, far);
  vector dir = Vector(6400, 0, 6400) - site.m_vPos;
  dir[1] = 0;
  dir.Normalize();
  string why = ACR_Evac.Place("US", site.m_vPos + dir * 50, s_Evac);
  int walking;
  int left;
  if (s_Evac) { walking = s_Evac.m_aPeople.Count(); left = s_Evac.m_aLeft.Count(); }
  Check("evac: an evac point by a town turns its people into civilians walking out, a few left behind", why.IsEmpty() && s_Evac && walking > 0 && left >= 1 && walking + left == s_iTownPeople && !ACR_Containment.Has(site), string.Format("%1: %2 walking, %3 left of %4 (%5)", why, walking, left, s_iTownPeople, ACR_Evac.s_iHanded));
  if (!s_Evac) return;
  s_fFootBefore = Spread(s_Evac);
  s_iRPBefore = manager.m_Bases.GetRP("US");
  manager.SpawnUnit("Evac Truck", "", TRUCK, true, "", ACR_Manager.Ground(s_Evac.m_vPos + dir * 6), ACR_Manager.Ground(s_Evac.m_vPos + dir * 10));
  s_Heli = ACR_Manager.SpawnPrefab(HELI, ACR_Manager.Ground(s_Evac.m_vPos - dir * 120));
 }

 // How far its people on foot are from the XP, on average.
 protected static float Spread(ACR_EvacPoint point)
 {
  float sum;
  int n;
  foreach (IEntity person : point.m_aPeople)
  {
   if (!person || ACR_Transport.VehicleOf(person)) continue;
   sum += vector.DistanceXZ(person.GetOrigin(), point.m_vPos);
   n++;
  }
  if (n == 0) return 0;
  return sum / n;
 }

 protected static void Noise()
 {
  if (!s_Evac || !s_Heli) { Check("evac: rotor noise by an evac point is heard", false, "no heli"); return; }
  VehicleHelicopterSimulation rotors = VehicleHelicopterSimulation.Cast(s_Heli.FindComponent(VehicleHelicopterSimulation));
  if (rotors) rotors.EngineStart();
  int heard = ACR_Evac.NoiseForTest(s_Evac);
  bool on = rotors && rotors.EngineIsOn();
  Check("evac: a running helicopter by an evac point is heard (the dead come for it)", heard == 1, string.Format("engine %1, heard %2, hordes so far %3", on, heard, ACR_Evac.s_iNoise));
  if (rotors) rotors.EngineStop();
  SCR_EntityHelper.DeleteEntityAndChildren(s_Heli);
 }

 protected static void Loaded(ACR_Manager manager)
 {
  if (!s_Evac) return;
  ACR_Squad truck = Named(manager, "Evac Truck");
  IEntity vehicle;
  if (truck) vehicle = ACR_Transport.CrewedVehicle(truck);
  int aboard;
  if (vehicle) aboard = ACR_Evac.Aboard(s_Evac, vehicle);
  float foot = Spread(s_Evac);
  Check("evac: civilians walk to the evac point (on rails)", aboard > 0 || foot < s_fFootBefore || ACR_Evac.s_iNudged > 0, string.Format("on foot %1 m -> %2 m from it, stepped %3, aboard %4", s_fFootBefore, foot, ACR_Evac.s_iNudged, aboard));
  Check("evac: a vehicle parked on the evac point gets them put aboard", vehicle && aboard > 0 && ACR_Evac.s_iSeated > 0, string.Format("truck %1, aboard %2, seated %3, on foot %4", vehicle != null, aboard, ACR_Evac.s_iSeated, ACR_Evac.OnFoot(s_Evac)));
  if (!vehicle) return;
  // Drive it home the quick way: the US HQ.
  ACR_Base home = ACR_DoctrineKits.HQOf("US");
  if (!home) return;
  s_sHome = home.m_sName;
  vector m[4];
  vehicle.GetWorldTransform(m);
  m[3] = ACR_Manager.Ground(home.GetOrigin() + Vector(8, 0, 8)) + Vector(0, 1, 0);
  vehicle.SetWorldTransform(m);
  Physics physics = vehicle.GetPhysics();
  if (physics) physics.SetVelocity(vector.Zero);
 }

 protected static void Delivered(ACR_Manager manager)
 {
  int rp = manager.m_Bases.GetRP("US");
  Check("evac: civilians brought to a held base are delivered and pay that side", ACR_Evac.s_iDelivered > 0 && (rp > s_iRPBefore || s_iRPBefore >= ACR_BaseSystem.POINTS_CAP), string.Format("delivered %1 to %2, RP %3 -> %4", ACR_Evac.s_iDelivered, s_sHome, s_iRPBefore, rp));
 }

 // A town holding out with a free US truck 700 m away: the AI sets an XP and sends it.
 protected static void AIEvacSetup(ACR_Manager manager)
 {
  ACR_Base town;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_bHidden || !base.m_sOwner.IsEmpty() || vector.DistanceXZ(base.GetOrigin(), s_Zero.m_vPos) < 600) continue;
   if (s_Evac && vector.DistanceXZ(base.GetOrigin(), s_Evac.m_vPos) < 1500) continue;
   town = base;
   break;
  }
  if (!town) return;
  ACR_Containment.AddSurvivors(town.GetOrigin(), town.m_sName);
  vector dir = Vector(6400, 0, 6400) - town.GetOrigin();
  dir[1] = 0;
  dir.Normalize();
  vector spot = ACR_Manager.Ground(town.GetOrigin() + dir * 700);
  manager.SpawnUnit("AI Evac Truck", "", TRUCK, true, "", spot, ACR_Manager.Ground(spot + dir * 4));
 }

 protected static void AIEvac(ACR_Manager manager)
 {
  ACR_AICommander ai = manager.FobAICommanderFor("US");
  if (ai) ai.ManageEvacsForTest();
  ACR_Squad taxi;
  if (ai) taxi = ai.EvacTaxiForTest();
  bool sent = taxi && taxi.m_iAITask == ACR_EAITask.RESCUE && ACR_Evac.CountFor("US") >= 1;
  string name = "none";
  if (taxi) name = taxi.GetName();
  Check("evac: an AI commander sets an evac point and sends a free truck", sent, string.Format("ai %1, taxi %2, US XPs %3", ai != null, name, ACR_Evac.CountFor("US")));
  bool reserved = taxi && !manager.m_Rides.IsFreeTransport(taxi, "US");
  Check("evac: the evac truck isn't taken for rides meanwhile", reserved, "");
 }

 protected static ACR_Squad Named(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == name) return squad;
  return null;
 }

 // Survivors in a town far from anyone, and a fast horde sent at them.
 protected static void SendToSurvivors()
 {
  s_iLostBefore = ACR_Containment.s_iLost;
  ACR_Base town;
  float best;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_bHidden || !base.m_sOwner.IsEmpty()) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), s_Zero.m_vPos);
   if (d < ACR_Outbreak.GOAL_MIN || d > ACR_Outbreak.GOAL_MAX || (s_Evac && vector.DistanceXZ(base.GetOrigin(), s_Evac.m_vTown) < 600)) continue;
   if (!town || d < best) { town = base; best = d; }
  }
  if (!town) return;
  ACR_Containment.AddSurvivors(town.GetOrigin(), town.m_sName);
  ACR_Outbreak.SendHorde(s_Zero, 4, town.GetOrigin(), town.m_sName);
  ACR_Outbreak.s_fWalkScaleForTest = 400;
 }

 protected static void Overrun()
 {
  ACR_Outbreak.s_fWalkScaleForTest = 1;
  Check("containment: a horde reaching civilians nobody guards takes them", ACR_Containment.s_iLost > s_iLostBefore, string.Format("lost %1 -> %2", s_iLostBefore, ACR_Containment.s_iLost));
 }

 protected static void Bounty(ACR_Manager manager)
 {
  int bounty = ACR_Containment.Bounty(s_Zero);
  ACR_Containment.Give(manager, "US", 100000);
  int rp = manager.m_Bases.GetRP("US");
  Check("containment: ground zero pays its bounty, and RP stops at the cap", bounty == ACR_Containment.RP_GROUND_ZERO && rp == ACR_BaseSystem.POINTS_CAP, string.Format("bounty %1, rp %2", bounty, rp));
 }

 protected static void Counterattack()
 {
  vector dir = Vector(6400, 0, 6400) - s_Zero.m_vPos;
  dir[1] = 0;
  dir.Normalize();
  vector at = ACR_Manager.Ground(s_Zero.m_vPos + dir * 700);
  int reserve = s_Zero.m_iReserve;
  int sent = ACR_Outbreak.CounterAttack(at, "Test town", null, "US");
  bool counter;
  ACR_DefenderPost horde;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
   if (post.m_Nest && post.m_Nest.m_bRoaming && post.m_Nest.m_bCounter && post.m_Nest.m_vGoal == at) { counter = true; horde = post; }
  Check("counterattack: a place taken near a nest has the dead come for it", sent >= 1 && counter, string.Format("sent %1, counter horde %2, ground zero had %3", sent, counter, reserve));
  // #241: messages name the town, never the horde's label ("Zombies counterattacking ...").
  string arrived = "none";
  if (horde)
  {
   vector was = horde.m_vPos;
   horde.m_vPos = at;
   arrived = ACR_Outbreak.Place(horde);
   horde.m_vPos = was;
  }
  string zero = ACR_Outbreak.Place(s_Zero);
  Check("zombies: messages name the town, not the horde or nest label", arrived == "Test town" && zero == ACR_Containment.GroundZero(), string.Format("counter horde at its town: %1; ground zero: %2", arrived, zero));
  int again = ACR_Outbreak.CounterAttack(at, "Test town", null, "US");
  Check("counterattack: once per place, not again straight away", again == 0, string.Format("%1", again));
 }

 protected static void Spread()
 {
  int level = ACR_Containment.LevelForTest();
  Check("containment: how far it has spread is worked out and reported", level >= 1, string.Format("level %1, %2 zombies", level, ACR_Outbreak.Total()));
  // #240: left alone it escalates (faster regrowth and spread), and the spread doesn't halve:
  // a nest of 30 sends 15, one of 29 waits, ground zero's hordes are 15+.
  int stage = ACR_Containment.Stage();
  ACR_Containment.EscalateForTest();
  float pace = ACR_Containment.Pace();
  Check("containment: left alone, the outbreak gains a stage and speeds up", ACR_Containment.Stage() == stage + 1 && pace > 1, string.Format("stage %1 -> %2, pace %3", stage, ACR_Containment.Stage(), pace));
  ACR_DefenderPost town = new ACR_DefenderPost();
  town.m_Nest = new ACR_ZombieNest();
  town.m_iReserve = 30;
  int thirty = ACR_Outbreak.HordeCount(town);
  town.m_iReserve = 29;
  int under = ACR_Outbreak.HordeCount(town);
  int zeroReserve = s_Zero.m_iReserve;
  s_Zero.m_iReserve = ACR_Containment.GZ_HORDE_MIN;
  int fromZero = ACR_Outbreak.HordeCount(s_Zero);
  s_Zero.m_iReserve = zeroReserve;
  Check("containment: hordes are 15 or more, and a town nest sends one from 30", thirty == ACR_Containment.HORDE_FLOOR && under == 0 && fromZero >= ACR_Containment.HORDE_FLOOR, string.Format("30 -> %1, 29 -> %2, ground zero at %3 -> %4", thirty, under, ACR_Containment.GZ_HORDE_MIN, fromZero));
  ACR_TestRegress.Complete("containment", EXPECTED);
 }

 protected static void Spawn(ACR_Manager manager, string name, vector at, float distance)
 {
  vector dir = Vector(6400, 0, 6400) - at;
  dir[1] = 0;
  dir.Normalize();
  vector spot = ACR_Manager.Ground(at + dir * distance);
  manager.m_sSpawningFaction = "US";
  manager.SpawnUnit(name, FIRE_TEAM, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
 }

 protected static void Check(string name, bool ok, string detail)
 {
  ACR_TestRegress.Check(name, ok, detail);
 }
}
