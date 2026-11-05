// -acrReconTest 1 (server-capable, ~1 min): recon flight and radio intercept (ACR_Recon). A Soviet
// squad, lined up where it stands, is photographed by a US recon flight: in the US intel, kept 5 min,
// fuzzed by one offset per group; the Soviets are told a jet went over; a summary line for the US.
// Radio intercept: a Soviet transmission gives the US a ring that holds the transmitter off its
// centre, and the same transmitter again moves the ring. The AI commander drops its blind guess at a
// photographed base and flies its recon over a fresh intercept near its bases first.
class ACR_TestRecon
{
 static const int CHECKS = 10;
 static const int CALL_TICK = 6;
 static const int CHECK_TICK = 50;
 static ACR_Squad s_Enemy;
 static ref array<IEntity> s_Bodies;
 static ref array<vector> s_TrueAt;
 static int s_iRP;
 static float s_fCallAt;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
   // Deterministic: everything under the track is seen, every transmission heard.
   ACR_ReconRun.SEE_OPEN = 1;
   ACR_ReconRun.SEE_COVERED = 1;
   ACR_ReconRun.SEE_VEHICLE = 1;
   ACR_RadioIntercept.INTERCEPT_CHANCE = 1;
  }
  if (!alpha || !alpha.GetAnchor()) return;
  if (tick == 4) Pure(manager);
  if (tick == CALL_TICK) Call(h, manager, alpha);
  // Held where they were put (a halted squad still walks back into formation), so the spots the
  // flight reports can be measured against where they really stood.
  if (tick > CALL_TICK && tick < CHECK_TICK && s_Bodies)
   foreach (int i, IEntity body : s_Bodies) if (body) body.SetOrigin(s_TrueAt[i]);
  if (tick == CHECK_TICK) Results(manager);
 }

 // Geometry, names, intercepts and the AI's use of them: no flight needed.
 static void Pure(ACR_Manager manager)
 {
  vector start = Vector(1000, 0, 1000);
  vector north = Vector(0, 0, 1);
  bool inside = ACR_ReconRun.InCorridor(Vector(1250, 0, 2000), start, north, 2400, 300);
  bool wide = ACR_ReconRun.InCorridor(Vector(1350, 0, 2000), start, north, 2400, 300);
  bool behind = ACR_ReconRun.InCorridor(Vector(1000, 0, 950), start, north, 2400, 300);
  bool beyond = ACR_ReconRun.InCorridor(Vector(1000, 0, 3500), start, north, 2400, 300);
  ACR_TestRegress.Check("recon: the corridor is the track's length and width", inside && !wide && !behind && !beyond, string.Format("inside %1 wide %2 behind %3 beyond %4", inside, wide, behind, beyond));

  ACR_Intel us = manager.IntelFor("US");
  us.m_Intercepts.Clear();
  vector from = ACR_Manager.Ground(Vector(4000, 0, 6000));
  ACR_RadioIntercept.Transmitted("USSR", from, "Test transmitter");
  float offset = -1;
  if (us.m_Intercepts.Count() == 1) offset = vector.DistanceXZ(us.m_Intercepts[0].m_vCentre, from);
  float r = ACR_RadioIntercept.INTERCEPT_RADIUS;
  array<vector> zonePos = {};
  array<float> zoneRadii = {};
  array<string> zoneLabels = {};
  ACR_RadioIntercept.Zones("US", zonePos, zoneRadii, zoneLabels);
  bool ring = offset >= r * 0.25 - 1 && offset <= r * 0.66 + 1 && zoneLabels.Contains("~Radio intercept");
  ACR_TestRegress.Check("radio intercept: a ring that holds the transmitter, off its centre, on the map", ring, string.Format("rings %1 offset %2 labels %3", us.m_Intercepts.Count(), offset, zoneLabels.Count()));
  ACR_RadioIntercept.Transmitted("USSR", from + Vector(30, 0, 0), "Test transmitter");
  ACR_TestRegress.Check("radio intercept: the same transmitter again moves its ring", us.m_Intercepts.Count() == 1, string.Format("rings %1", us.m_Intercepts.Count()));

  ACR_AICommander ai = manager.EnsureAICommander("US");
  ACR_Base enemyBase;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && !base.m_bRetired && base.m_sOwner == "USSR" && base.IsVisibleTo("US")) { enemyBase = base; break; }
  int before = -1;
  int after = -1;
  if (ai && enemyBase)
  {
   ai.AssessForTest();
   before = ai.DefenceOfForTest(enemyBase);
   us.Photographed(enemyBase, GetGame().GetWorld().GetWorldTime());
   ai.AssessForTest();
   after = ai.DefenceOfForTest(enemyBase);
  }
  ACR_TestRegress.Check("AI: a photographed enemy base loses the blind guess at its defence", before - after == ACR_AICommander.UNKNOWN_DEFENCE, string.Format("%1: %2 -> %3", enemyBase, before, after));

  ACR_Base ownBase;
  foreach (ACR_Base mine : ACR_Base.s_All)
   if (mine && !mine.m_bRetired && mine.m_sOwner == "US") { ownBase = mine; break; }
  us.m_Intercepts.Clear();
  string why;
  vector picked;
  vector ringAt;
  if (ai && ownBase)
  {
   ACR_RadioIntercept.Heard(us, ownBase.GetOrigin() + Vector(300, 0, 0), "USSR:near");
   ringAt = us.m_Intercepts[0].m_vCentre;
   picked = ai.ReconTarget(why);
  }
  ACR_TestRegress.Check("AI: its recon goes over a fresh radio intercept near its bases first", ringAt != vector.Zero && vector.DistanceXZ(picked, ringAt) < 1, string.Format("ring %1 picked %2 (%3)", ringAt, picked, why));
  us.m_Intercepts.Clear();
 }

 static void Call(ACR_TestHarness h, ACR_Manager manager, ACR_Squad alpha)
 {
  if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == "USSR" && squad.CountAlive() >= 3 && !squad.m_bMortar && !squad.Aircraft() && squad.GetAnchor() && !ACR_Transport.VehicleOf(squad.GetAnchor())) { s_Enemy = squad; break; }
  if (!s_Enemy)
  {
   ACR_TestRegress.Abort("recon", "no Soviet squad to photograph");
   return;
  }
  // Where it stands (on land, away from Alpha): its men in a line there, halted.
  vector target = ACR_Manager.Ground(s_Enemy.GetAnchor().GetOrigin());
  s_Bodies = {};
  s_TrueAt = {};
  array<AIAgent> agents = {};
  s_Enemy.GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || ACR_Transport.VehicleOf(body)) continue;
   vector at = ACR_Manager.Ground(target + Vector(i * 3, 0, 0));
   body.SetOrigin(at);
   s_Bodies.Insert(body);
   s_TrueAt.Insert(at);
  }
  s_Enemy.Halt("Test");
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  us.m_iRP = 500;
  int index = h.SupportIndex(manager, ACR_ESupportType.RECON_FLIGHT);
  // The AI commanders may have flown one before they were switched off: cooldown cleared.
  if (index >= 0) us.m_aSupports[index].m_fReadyAt = 0;
  s_fCallAt = GetGame().GetWorld().GetWorldTime();
  string result = manager.m_Supports.Call("US", index, target);
  s_iRP = us.m_iRP;
  PrintFormat("[ACR] RECONTEST call (entry %1) over %2 with %3 men of %4: %5", index, target, s_Bodies.Count(), s_Enemy.GetName(), result);
  ACR_TestRegress.Check("recon: the call is accepted and paid", index >= 0 && result.Contains("overhead") && s_iRP == 500 - us.m_aSupports[index].m_iCost, string.Format("entry %1 RP %2: %3", index, s_iRP, result));
 }

 static void Results(ACR_Manager manager)
 {
  if (!s_Enemy || !s_Bodies) return;
  ACR_Intel us = manager.IntelFor("US");
  float now = GetGame().GetWorld().GetWorldTime();
  int kept;
  bool within = true;
  bool together = true;
  bool moved;
  vector firstOffset;
  bool haveFirst;
  foreach (int i, IEntity body : s_Bodies)
  {
   if (!body || us.KeepUntil(body) < s_fCallAt + ACR_ReconRun.RECON_MEMORY_MS) continue; // photographed by this flight
   kept++;
   vector offset = us.m_SeenAt.Get(body) - s_TrueAt[i];
   offset[1] = 0;
   if (offset.Length() > ACR_ReconRun.FUZZ_INFANTRY + 5) within = false;
   if (offset.Length() > 1) moved = true;
   if (!haveFirst) { firstOffset = offset; haveFirst = true; }
   else if (vector.DistanceXZ(offset, firstOffset) > 4) together = false;
  }
  PrintFormat("[ACR] RECONTEST %1 of %2 photographed (recon sightings total %3), offset %4", kept, s_Bodies.Count(), ACR_ReconRun.s_iSeen, firstOffset);
  ACR_TestRegress.Check("recon: the squad under the track is in the side's intel, kept 5 min", s_Bodies.Count() > 0 && kept * 2 >= s_Bodies.Count(), string.Format("%1 of %2", kept, s_Bodies.Count()));
  ACR_TestRegress.Check("recon: spots fuzzed within the limit, one offset for the group", kept > 0 && within && together && moved, string.Format("within %1 together %2 moved %3", within, together, moved));
  bool warned;
  foreach (string line : manager.IntelFor("USSR").m_Feed) if (line.Contains("reconnaissance aircraft")) warned = true;
  ACR_TestRegress.Check("recon: the side flown over hears the jet", warned, string.Format("USSR feed %1", manager.IntelFor("USSR").m_Feed.Count()));
  bool summary;
  foreach (string usLine : us.m_Feed) if (usLine.Contains(ACR_ReconRun.SPOTTER + " over")) summary = true;
  ACR_TestRegress.Check("recon: a summary line in the caller's feed", summary, string.Format("US feed %1", us.m_Feed.Count()));
  ACR_TestRegress.Complete("recon", CHECKS);
 }
}
