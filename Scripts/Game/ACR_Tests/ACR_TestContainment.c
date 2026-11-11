// srv:containment (Everon whole map, about 2.5 min): a contained outbreak (ACR_Containment, #107,
// -acrOutbreak contain). Without Project Outbreak loaded: no nests, no Outbreak start row.
// With it (swap Outbreak into tools/testserver-args.txt for the run): one nest only, ground zero,
// clear of both starting bases; survivors in the towns round it; both sides mark it. Ground zero
// sends a horde and the town it goes for gets survivors; a US team at a site rescues them and the
// US side gets RP; a horde reaching survivors with nobody there takes them; an AI commander sends
// a squad to a site; ground zero's bounty and the RP cap; a town taken near a nest is counterattacked
// (once per place); the spread is reported; messages name towns (#241); left alone it escalates and
// hordes stay 15+ (#240).
class ACR_TestContainment
{
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 protected static ACR_DefenderPost s_Zero;
 protected static ref ACR_SurvivorSite s_Rescue; // ref: a rescued site leaves the list
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
  if (second == 14) GoRescue(manager);
  if (second == 36) Rescued(manager);
  if (second == 38) SendToSurvivors();
  if (second == 56) Overrun();
  if (second == 58) AIRescueSetup(manager);
  if (second == 70) AIRescue(manager);
  if (second == 72) Bounty(manager);
  if (second == 74) Counterattack();
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
  if (!s_Zero) { ACR_TestRegress.Complete("containment", 16); return; }
  bool clear = true;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (!base.m_sOwner.IsEmpty() && !base.m_bPocket && !base.m_bRetired && vector.DistanceXZ(base.GetOrigin(), s_Zero.m_vPos) < ACR_Outbreak.HQ_CLEAR) clear = false;
  Check("containment: ground zero is clear of both starting bases", clear, ACR_Containment.GroundZero());
  Check("containment: towns round ground zero have survivors from the start", ACR_Containment.SiteCount() >= 1, string.Format("%1 sites", ACR_Containment.SiteCount()));
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
  Check("containment: a horde from ground zero leaves survivors to save in the town it goes for", sent && there, string.Format("to %1, sent %2, sites made %3 -> %4", goalName, sent, before, ACR_Containment.s_iSitesMade));
 }

 protected static void GoRescue(ACR_Manager manager)
 {
  if (ACR_Containment.SiteCount() == 0) return;
  s_Rescue = ACR_Containment.SiteAt(0);
  s_iRPBefore = manager.m_Bases.GetRP("US");
  Spawn(manager, "Rescue Team", s_Rescue.m_vPos, 15);
 }

 protected static void Rescued(ACR_Manager manager)
 {
  bool gone = s_Rescue && !ACR_Containment.Has(s_Rescue);
  int rp = manager.m_Bases.GetRP("US");
  string detail = "no site";
  if (s_Rescue) detail = string.Format("%1, gone %2, rescued %3", s_Rescue.m_sTown, gone, ACR_Containment.s_iRescued);
  Check("containment: a US team at a survivor site rescues them", gone && ACR_Containment.s_iRescued > 0, detail);
  Check("containment: the rescue pays the US side RP", rp > s_iRPBefore || s_iRPBefore >= ACR_BaseSystem.POINTS_CAP, string.Format("%1 -> %2", s_iRPBefore, rp));
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
   if (d < ACR_Outbreak.GOAL_MIN || d > ACR_Outbreak.GOAL_MAX || (s_Rescue && vector.DistanceXZ(base.GetOrigin(), s_Rescue.m_vPos) < 600)) continue;
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
  Check("containment: a horde reaching survivors nobody guards takes them", ACR_Containment.s_iLost > s_iLostBefore, string.Format("lost %1 -> %2", s_iLostBefore, ACR_Containment.s_iLost));
 }

 // A site 700 m from a US team the AI can send.
 protected static void AIRescueSetup(ACR_Manager manager)
 {
  ACR_Base town;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_bHidden || !base.m_sOwner.IsEmpty() || vector.DistanceXZ(base.GetOrigin(), s_Zero.m_vPos) < 600) continue;
   town = base;
   break;
  }
  if (!town) return;
  ACR_Containment.AddSurvivors(town.GetOrigin(), town.m_sName);
  Spawn(manager, "Sent Team", town.GetOrigin(), 700);
 }

 protected static void AIRescue(ACR_Manager manager)
 {
  ACR_AICommander ai = manager.FobAICommanderFor("US");
  if (ai) ai.ManageRescuesForTest();
  ACR_Squad rescuer;
  if (ai) rescuer = ai.RescuerForTest();
  bool sent = rescuer && rescuer.m_iAITask == ACR_EAITask.RESCUE;
  string name = "none";
  if (rescuer) name = rescuer.GetName();
  Check("containment: an AI commander sends a squad to rescue survivors", sent, string.Format("ai %1, rescuer %2, sites %3", ai != null, name, ACR_Containment.SiteCount()));
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
  ACR_TestRegress.Complete("containment", 16);
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
