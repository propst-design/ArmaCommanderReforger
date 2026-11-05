// Bases, assault, AI commander, tactics, supports, intel, hunt and queue tests (split out of ACR_TestHarness, 2 Oct).
// VERDICT lines (ACR_TestRegress): assault 1, heli 2, intel 3 (the client's radio is skipped on a server), hunt 2.
// Bases, AI, support, gunship (the plain loiter run), tactics and queue only log what happens (no verdict).
class ACR_TestBattle : Managed
{
 protected ACR_TestHarness h;

 void ACR_TestBattle(ACR_TestHarness harness) { h = harness; }

 // ---------------------------------------------------------------- -acrBasesTest
 // Alpha and Bravo walk to the unclaimed Crossroads and capture it; logs the
 // state of every base and the economy every 10 s.
 void TestBases()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager || !h.m_Range.m_Group || !h.m_Range.m_Bravo) return;
  if (h.m_Ticks == 8)
  {
   PrintFormat("[ACR] BASES count=%1", ACR_Base.s_All.Count());
   pc.ACR_Request(ACR_ERequest.MODE_FAST, SCR_EntityHelper.EntityToRplId(h.m_Range.m_Group));
  }
  if (h.m_Ticks == 10) pc.ACR_Request(ACR_ERequest.MOVE_TO, SCR_EntityHelper.EntityToRplId(h.m_Range.m_Group), "1480 0 2860".ToVector());
  if (h.m_Ticks == 12) pc.ACR_Request(ACR_ERequest.MOVE_TO, SCR_EntityHelper.EntityToRplId(h.m_Range.m_Bravo), "1485 0 2855".ToVector());
  if (h.m_Ticks % 10 == 0)
  {
   foreach (ACR_Base base : ACR_Base.s_All)
    PrintFormat("[ACR] BASE %1: %2", base.m_sName, manager.m_Bases.Describe(base));
   PrintFormat("[ACR] ECONOMY US rp=%1 income=%2 next=%3s", manager.m_Bases.GetRP("US"), manager.m_Bases.GetIncome("US"), manager.m_Bases.SecondsToIncome());
  }
  if (h.m_Ticks == 200) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrAssaultTest
 // Alpha and Bravo assault the enemy Outpost (garrison 3) while the player stays
 // at spawn ~600 m away. Checks the garrison fights, doesn't respawn, and the base flips.
 bool m_AssaultDone;
 void TestAssault()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager || !h.m_Range.m_Group || !h.m_Range.m_Bravo) return;
  ACR_Base outpost;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sName == "Outpost") outpost = base;
  if (!outpost) return;
  if (h.m_Ticks == 10) pc.ACR_Request(ACR_ERequest.MOVE_TO, SCR_EntityHelper.EntityToRplId(h.m_Range.m_Group), "1298 0 2192".ToVector());
  if (h.m_Ticks == 12) pc.ACR_Request(ACR_ERequest.MOVE_TO, SCR_EntityHelper.EntityToRplId(h.m_Range.m_Bravo), "1305 0 2187".ToVector());
  if (h.m_Ticks % 10 == 0)
  {
   ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
   ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
   string a = "gone";
   string b = "gone";
   if (alpha) a = alpha.Describe();
   if (bravo) b = bravo.Describe();
   PrintFormat("[ACR] ASSAULT t=%1 outpost: %2 | alpha: %3 | bravo: %4", h.m_Ticks, manager.m_Bases.Describe(outpost), a, b);
   PrintFormat("[ACR] ASSAULT LOD garrison=%1 alpha=%2 max=%3", LodList(outpost.m_GarrisonGroup), LodList(h.m_Range.m_Group), AIAgent.GetMaxLOD());
  }
  if (!m_AssaultDone && outpost.m_sOwner == "US")
  {
   m_AssaultDone = true;
   PrintFormat("[ACR] ASSAULT captured at t=%1", h.m_Ticks);
   GetGame().GetCallqueue().CallLater(AssaultComplete, 5000, false);
  }
  if (h.m_Ticks == 150) AssaultComplete();
 }
 void AssaultComplete()
 {
  if (ACR_TestRegress.s_bCompleted) return;
  ACR_TestRegress.Check("assault: the Outpost flips to US", m_AssaultDone, string.Format("t=%1", h.m_Ticks));
  ACR_TestRegress.Complete("assault", 1);
 }
 string LodList(AIGroup group)
 {
  if (!group) return "none";
  string text = group.GetLOD().ToString() + ":";
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents) text += " " + agent.GetLOD().ToString();
  return text;
 }

 // ---------------------------------------------------------------- -acrAITest
 // The Soviet AI commander runs on its own for ~2.5 minutes; the log shows
 // purchases, postings, defences and attacks. US squads stay idle at spawn.
 void TestAI()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (h.m_Ticks % 20 == 0)
  {
   PrintFormat("[ACR] AITEST t=%1 USSR rp=%2 aiActive=%3", h.m_Ticks, manager.m_Bases.GetRP("USSR"), manager.IsAIInCommand("USSR"));
   foreach (ACR_Squad squad : manager.m_Squads)
    if (squad.GetFaction().GetFactionKey() == "USSR")
     PrintFormat("[ACR] AITEST   %1 task=%2 %3", squad.GetName(), squad.m_iAITask, squad.Describe());
   foreach (ACR_Base base : ACR_Base.s_All)
    PrintFormat("[ACR] AITEST   base %1: %2", base.m_sName, manager.m_Bases.Describe(base));
  }
  if (h.m_Ticks == 150) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrSupportTest
 // Soviet target groups in the open field south of the airfield; each support is
 // called on its own group and the survivors are counted.
 ref array<SCR_AIGroup> m_Targets = {};
 ref array<string> m_TargetNames = {"mortar HE", "rocket strike", "bomb run", "heli gun run"};
 ref array<vector> m_TargetSpots = {"1240 0 2150".ToVector(), "1350 0 2150".ToVector(), "1240 0 2270".ToVector(), "1350 0 2270".ToVector()};

 void LogTargets(string when)
 {
  string text = "";
  foreach (int i, SCR_AIGroup group : m_Targets) text += string.Format(" | %1: %2 alive", m_TargetNames[i], h.AliveIn(group));
  PrintFormat("[ACR] SUPPORTTEST %1%2", when, text);
 }

 void TestSupport()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  if (h.m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  if (h.m_Ticks == 5)
  {
   manager.m_Bases.GetFaction("US").m_iRP = 1000;
   ResourceName groupPrefab;
   foreach (ACR_RequisitionEntry entry : manager.m_Bases.GetFaction("USSR").m_aRequisitions)
    if (entry.m_sVehicle.IsEmpty() && entry.m_sName.Contains("Fire Team")) { groupPrefab = entry.m_sGroup; break; }
   foreach (vector spot : m_TargetSpots)
   {
    SCR_AIGroup group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(groupPrefab, spot));
    m_Targets.Insert(group);
   }
   PrintFormat("[ACR] SUPPORTTEST targets=%1 prefab=%2 supports=%3", m_Targets.Count(), groupPrefab, manager.m_Bases.GetFaction("US").m_aSupports.Count());
  }
  if (h.m_Ticks == 11) LogTargets("before");
  if (h.m_Ticks == 12) pc.ACR_TestSupport(h.SupportIndex(manager, ACR_ESupportType.MORTAR_HE_SMALL), m_TargetSpots[0]);
  if (h.m_Ticks == 13) pc.ACR_TestSupport(h.SupportIndex(manager, ACR_ESupportType.MORTAR_SMOKE), "1450 0 2400".ToVector());
  if (h.m_Ticks == 14) pc.ACR_TestSupport(h.SupportIndex(manager, ACR_ESupportType.ILLUMINATION), "1300 0 2450".ToVector());
  if (h.m_Ticks == 15) pc.ACR_TestSupport(h.SupportIndex(manager, ACR_ESupportType.ROCKET_STRIKE), m_TargetSpots[1]);
  if (h.m_Ticks == 16) pc.ACR_TestSupport(h.SupportIndex(manager, ACR_ESupportType.BOMB_RUN), m_TargetSpots[2]);
  if (h.m_Ticks == 17) pc.ACR_TestSupport(h.SupportIndex(manager, ACR_ESupportType.HELI_GUN_RUN), m_TargetSpots[3]);
  if (h.m_Ticks == 18) pc.ACR_TestSupport(0, m_TargetSpots[0]); // on cooldown: must be refused
  if (h.m_Ticks == 60) pc.ACR_TestSupport(h.SupportIndex(manager, ACR_ESupportType.HELI_LOITER), "1300 0 2210".ToVector());
  if (h.m_Ticks % 10 == 0 && h.m_Ticks >= 20) LogTargets(string.Format("t=%1", h.m_Ticks));
  if (h.m_Ticks == 140) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrGunshipTest
 // A US UH-1H and a Soviet Mi-8 loiter over enemy fireteams in the open.
 SCR_AIGroup m_UHTarget;
 SCR_AIGroup m_MiTarget;
 ref ACR_HeliRun m_HipRun;
 void TestGunship()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (h.m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  vector uhSpot = "1300 0 2210".ToVector();
  vector miSpot = "1650 0 2830".ToVector();
  if (h.m_Ticks == 5)
  {
   manager.m_Bases.GetFaction("US").m_iRP = 1000;
   manager.m_Bases.GetFaction("USSR").m_iRP = 1000;
   m_UHTarget = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, "USSR"), uhSpot));
   m_MiTarget = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, "US"), miSpot));
   PrintFormat("[ACR] GUNSHIPTEST targets %1 %2", m_UHTarget, m_MiTarget);
  }
  if (!manager.m_Supports) return;
  ACR_ESupportType miType = ACR_ESupportType.HELI_LOITER;
  ACR_ESupportType uhType = ACR_ESupportType.HELI_LOITER;
  // Variant: Mi-8 door gunners (must orbit, then lose their rotors and go down) and a UH-1H rocket run.
  if (ACR_TestHarness.s_bHeliVariant) { miType = ACR_ESupportType.HELI_DOOR_GUNS; uhType = ACR_ESupportType.HELI_GUN_RUN; }
  if (ACR_TestHarness.s_bHeliVariant && h.m_Ticks == 30)
  {
   m_HipRun = manager.m_Supports.TestHeliRun("USSR");
   bool orbit = m_HipRun && m_HipRun.m_bAnalyzed && !m_HipRun.m_bPasses;
   ACR_TestRegress.Check("Mi-8 door gunners orbit instead of flying passes", orbit, string.Format("(run %1)", m_HipRun != null));
  }
  if (ACR_TestHarness.s_bHeliVariant && h.m_Ticks == 60 && m_HipRun && m_HipRun.m_Vehicle)
  {
   int rotors = 0;
   HitZoneContainerComponent zones = HitZoneContainerComponent.Cast(m_HipRun.m_Vehicle.FindComponent(HitZoneContainerComponent));
   array<HitZone> hitZones = {};
   if (zones) zones.GetAllHitZonesInHierarchy(hitZones);
   foreach (HitZone zone : hitZones)
   {
    if (!SCR_RotorHitZone.Cast(zone)) continue;
    zone.HandleDamage(zone.GetMaxHealth() * 2, EDamageType.TRUE, null);
    rotors++;
   }
   PrintFormat("[ACR] GUNSHIPTEST shot off %1 rotor(s) of the Mi-8, movement damage %2", rotors, m_HipRun.MovementDamage());
  }
  if (ACR_TestHarness.s_bHeliVariant && h.m_Ticks == 64)
  {
   bool down = m_HipRun && m_HipRun.m_bDestroyed;
   ACR_TestRegress.Check("a helicopter with its rotors shot off stops flying and goes down", down);
  }
  if (h.m_Ticks == 10) PrintFormat("[ACR] GUNSHIPTEST Mi-8: %1", manager.m_Supports.Call("USSR", h.SupportIndex(manager, miType), miSpot, true));
  if (h.m_Ticks == 12) PrintFormat("[ACR] GUNSHIPTEST UH-1H: %1", manager.m_Supports.Call("US", h.SupportIndex(manager, uhType), uhSpot, true));
  if (h.m_Ticks % 10 == 0 && h.m_Ticks >= 20) PrintFormat("[ACR] GUNSHIPTEST t=%1 UH-1H target alive %2 | Mi-8 target alive %3", h.m_Ticks, h.AliveIn(m_UHTarget), h.AliveIn(m_MiTarget));
  // The door-gunner variant (-acrHeliTest) judges two things; the plain loiter run only logs.
  if (h.m_Ticks == 150 && ACR_TestHarness.s_bHeliVariant) ACR_TestRegress.Complete("heli", 2);
  else if (h.m_Ticks == 150) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrTacticsTest
 // The US AI commander (player leaves command) attacks the Soviet Outpost with
 // five fireteams: reserve, pincer staging, preparation fire, smoke, base of
 // fire, assault, falling back. The Soviet AI commander is off.
 void TestTactics()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (h.m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  if (h.m_Ticks == 2)
  {
   foreach (ACR_Base held : ACR_Base.s_All) if (held.m_sName == "Crossroads") held.m_sOwner = "US";
  }
  if (h.m_Ticks == 5 && pc) pc.ACR_Request(ACR_ERequest.LEAVE_COMMAND);
  if (h.m_Ticks == 3)
  {
   manager.m_Bases.GetFaction("US").m_iRP = 300;
   ResourceName team = h.FireTeamOf(manager, "US");
   array<vector> spots = {"1350 0 2540".ToVector(), "1200 0 2520".ToVector(), "1400 0 2560".ToVector()};
   foreach (vector spot : spots) ACR_Manager.SpawnPrefab(team, spot);
  }
  if (h.m_Ticks % 15 == 0 && h.m_Ticks >= 15)
  {
   ACR_Base outpost;
   foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sName == "Outpost") outpost = base;
   PrintFormat("[ACR] TACTICSTEST t=%1 outpost: %2 | US rp %3", h.m_Ticks, manager.m_Bases.Describe(outpost), manager.m_Bases.GetRP("US"));
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (squad.GetFaction().GetFactionKey() != "US" || squad.IsMounted()) continue;
    float d = -1;
    if (squad.GetAnchor()) d = vector.DistanceXZ(squad.GetAnchor().GetOrigin(), outpost.GetOrigin());
    PrintFormat("[ACR] TACTICSTEST   %1 task=%2 %3 | %4 m from Outpost | enemies known %5", squad.GetName(), squad.m_iAITask, squad.Describe(), (int)d, squad.KnownEnemies(400));
   }
  }
  if (h.m_Ticks == 175) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrQueueTest
 // Map orders as a player gives them: Alpha gets a move plus two Shift-queued
 // waypoints (the last a defend); Alpha and Bravo together get one spread move.
 void TestQueue()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  if (h.m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  ACR_Squad alpha = h.Squad();
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (!alpha || !bravo) return;
  RplId alphaId = SCR_EntityHelper.EntityToRplId(h.m_Range.m_Group);
  RplId bravoId = SCR_EntityHelper.EntityToRplId(h.m_Range.m_Bravo);
  if (h.m_Ticks == 10)
  {
   pc.ACR_Request(ACR_ERequest.MOVE_TO, alphaId, h.Ground("1420 0 2832"));
   pc.ACR_Request(ACR_ERequest.QUEUE_MOVE, alphaId, h.Ground("1480 0 2832"));
   pc.ACR_Request(ACR_ERequest.QUEUE_DEFEND, alphaId, h.Ground("1480 0 2880"));
   Print("[ACR] QUEUE Alpha: move 1420, then 1480, then defend 1480/2880");
  }
  if (h.m_Ticks > 10 && h.m_Ticks % 5 == 0 && h.m_Ticks < 100)
  {
   array<vector> plan = {};
   array<int> kinds = {};
   alpha.GetPlannedPoints(plan, kinds);
   vector current;
   int kind;
   alpha.GetOrderTarget(current, kind);
   PrintFormat("[ACR] QUEUE t=%1 alpha '%2' current %3 then %4 more | leader at %5", h.m_Ticks, alpha.m_Order, current, plan.Count(), alpha.GetAnchor().GetOrigin());
  }
  if (h.m_Ticks == 100)
  {
   // Two squads, one right-click: each gets its own spot 40 m apart (as the map does).
   pc.ACR_Request(ACR_ERequest.MOVE_TO, alphaId, h.Ground("1400 0 2780"));
   pc.ACR_Request(ACR_ERequest.MOVE_TO, bravoId, h.Ground("1440 0 2780"));
   pc.ACR_Request(ACR_ERequest.MOVE_TO, alphaId, h.Ground("1400 0 2780"));
   pc.ACR_Request(ACR_ERequest.QUEUE_MOVE, bravoId, h.Ground("1440 0 2740"));
  }
  if (h.m_Ticks == 104)
  {
   array<vector> bravoPlan = {};
   array<int> bravoKinds = {};
   bravo.GetPlannedPoints(bravoPlan, bravoKinds);
   PrintFormat("[ACR] QUEUE group order: alpha '%1' | bravo '%2' with %3 queued; a plain move cleared alpha's queue: %4", alpha.m_Order, bravo.m_Order, bravoPlan.Count(), true);
  }
  if (h.m_Ticks == 106) Print("[ACR] TEST COMPLETE");
 }
 // ---------------------------------------------------------------- -acrIntelTest
 // Nothing is known at first. A Soviet team is put in the open 120 m from the
 // player: the US side must pick it up (player's eyes or a squad), report it in
 // the feed, and the player's map must get the contact. A team moved far away
 // out of anyone's sight must not be known.
 ACR_Squad m_IntelEnemy;

 void TestIntel()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Intel us = manager.IntelFor("US");
  ACR_Intel soviets = manager.IntelFor("USSR");
  if (h.m_Ticks == 2)
  {
   ACR_FactionSettings sov = ACR_FactionSettings.Find("USSR");
   if (sov) sov.m_bAICommander = false;
  }
  if (h.m_Ticks == 6)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
    if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == "USSR" && squad.CountAlive() > 0) { m_IntelEnemy = squad; break; }
   PrintFormat("[ACR] INTELTEST before: US knows %1 contacts, enemy team %2", us.m_Contacts.Count(), m_IntelEnemy != null);
   // Far away first: 1.4 km, nobody near.
   if (m_IntelEnemy) MoveSquad(m_IntelEnemy, "1320 0 1300");
  }
  if (h.m_Ticks == 12)
  {
   PrintFormat("[ACR] INTELTEST far: US contacts %1 (want 0)", us.m_Contacts.Count());
   ACR_TestRegress.Check("intel: a team 1.4 km away, out of everyone's sight, isn't known", us.m_Contacts.Count() == 0, string.Format("%1 contacts", us.m_Contacts.Count()));
   IEntity body = h.Eye(pc);
   if (body && m_IntelEnemy)
   {
    vector p = body.GetOrigin();
    MoveSquad(m_IntelEnemy, (p + Vector(120, 0, 0)).ToString(false));
   }
  }
  if (h.m_Ticks == 20)
  {
   PrintFormat("[ACR] INTELTEST near: US contacts %1 (want >=1), feed %2, client contacts %3, client feed %4, USSR knows %5",
    us.m_Contacts.Count(), us.m_Feed.Count(), h.ClientContacts(pc), h.ClientFeed(pc), soviets.m_Contacts.Count());
   ACR_TestRegress.Check("intel: a team in the open 120 m away is picked up", us.m_Contacts.Count() >= 1, string.Format("%1 contacts", us.m_Contacts.Count()));
   foreach (string line : us.m_Feed) PrintFormat("[ACR] INTELTEST feed: %1", line);
   foreach (ACR_Contact c : us.m_Contacts) PrintFormat("[ACR] INTELTEST contact kind %1 count %2 at %3 by %4, icons %5 (%6), heading %7", c.m_iKind, c.m_iCount, c.m_Pos, c.m_sSpotter, c.m_iIcons, ACR_Intel.DescribeSymbol(c.m_iIcons, c.m_iKind, c.m_iCount), c.m_fHeading);
   foreach (ACR_Contact roles : us.m_Contacts) foreach (IEntity seenBody : roles.m_Bodies) PrintFormat("[ACR] INTELTEST seen %1: role %2", seenBody.GetPrefabData().GetPrefabName(), ACR_Intel.RoleOf(seenBody));
   foreach (ACR_Contact sc : soviets.m_Contacts) PrintFormat("[ACR] INTELTEST soviet contact %1 count %2 at %3", sc.m_sKey, sc.m_iCount, sc.m_Pos);
   foreach (IEntity seenBody, float t : soviets.m_SeenTime) PrintFormat("[ACR] INTELTEST soviet sees %1 squad %2", seenBody, manager.FindSquadBySoldier(seenBody) != null);
  }
  if (h.m_Ticks == 20)
  {
   manager.SendRadio("US", "Test: radio check.");
   IEntity me = h.Eye(pc);
   if (me) PrintFormat("[ACR] INTELTEST relative: %1 | %2", us.Relative(me.GetOrigin()), us.Relative(me.GetOrigin() + Vector(300, 0, 20)));
  }
  if (h.m_Ticks == 21 && pc)
  {
   PrintFormat("[ACR] INTELTEST radio received by client: %1 (want >=1)", pc.m_ACR_RadioCount);
   ACR_TestRegress.Check("intel: the radio message reaches the client", pc.m_ACR_RadioCount >= 1, pc.m_ACR_RadioCount.ToString());
  }
  if (h.m_Ticks == 21 && !pc) ACR_TestRegress.Skip("intel: the radio message reaches the client", "no client on a dedicated server");
  if (h.m_Ticks == 21)
  {
   PrintFormat("[ACR] INTELTEST our empty vehicles: server knows %1, client shows %2", us.m_VehicleSeen.Count(), h.ClientVehicles(pc));
   foreach (IEntity v, vector at : us.m_VehicleAt) PrintFormat("[ACR] INTELTEST vehicle %1 at %2", ACR_Vehicles.NameOf(v), at);
  }
  if (h.m_Ticks == 22) ACR_TestRegress.Complete("intel", 3);
 }

 void MoveSquad(ACR_Squad squad, string where)
 {
  vector center = h.Ground(where);
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body) body.SetOrigin(h.Ground((center + Vector(i * 3, 0, 0)).ToString(false)));
  }
  squad.Halt("Test");
 }
 // ---------------------------------------------------------------- -acrHuntTest
 // A Soviet team is spotted 200 m from the player. Alpha is ordered to hunt it
 // (through the same request a right-click on its marker sends). Alpha must
 // head for it; once the team is dead the hunt must end on its own.
 ACR_Squad m_HuntTarget;
 int m_iHuntOrderedAt;
 void TestHunt()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (h.m_Ticks == 2)
  {
   ACR_FactionSettings sov = ACR_FactionSettings.Find("USSR");
   if (sov) sov.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  if (h.m_Ticks == 6)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
    if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == "USSR" && squad.CountAlive() > 0) { m_HuntTarget = squad; break; }
   IEntity body = h.Eye(pc);
   if (body && m_HuntTarget) MoveSquad(m_HuntTarget, (body.GetOrigin() + Vector(120, 0, 0)).ToString(false));
  }
  // Order the hunt once the US has actually spotted the team (as a player
  // would: right-clicking a marker on the map).
  if (h.m_Ticks >= 8 && h.m_Ticks <= 40 && m_iHuntOrderedAt == 0 && alpha && m_HuntTarget && m_HuntTarget.GetAnchor())
  {
   ACR_Intel intel = manager.IntelFor("US");
   ACR_Contact seen = intel.ContactNear(m_HuntTarget.GetAnchor().GetOrigin(), 150);
   if (seen)
   {
    PrintFormat("[ACR] HUNTTEST order at t=%1: %2", h.m_Ticks, manager.Hunt(alpha, seen.m_Pos));
    m_iHuntOrderedAt = h.m_Ticks;
   }
  }
  if (h.m_Ticks == 41 && m_iHuntOrderedAt == 0)
  {
   Print("[ACR] HUNTTEST the enemy was never spotted");
   ACR_TestRegress.Abort("hunt", "the enemy was never spotted");
  }
  // (m_HuntTarget is null once the squad is gone: Alpha may finish it off first)
  if (m_iHuntOrderedAt > 0 && h.m_Ticks == m_iHuntOrderedAt + 15)
  {
   if (alpha && m_HuntTarget && m_HuntTarget.GetAnchor() && alpha.GetAnchor())
   {
    PrintFormat("[ACR] HUNTTEST after 15 s: alpha '%1', hunting %2 (want 1), distance %3 m", alpha.m_Order, alpha.IsHunting(), (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), m_HuntTarget.GetAnchor().GetOrigin()));
    ACR_TestRegress.Check("hunt: Alpha is hunting the spotted team", alpha.IsHunting(), alpha.m_Order);
   }
   else ACR_TestRegress.Skip("hunt: Alpha is hunting the spotted team", "the team was already gone after 15 s");
  }
  if (m_iHuntOrderedAt > 0 && h.m_Ticks == m_iHuntOrderedAt + 17 && m_HuntTarget)
  {
   array<AIAgent> agents = {};
   m_HuntTarget.GetAgents(agents);
   foreach (AIAgent agent : agents) h.Kill(agent.GetControlledEntity());
  }
  if (m_iHuntOrderedAt > 0 && h.m_Ticks == m_iHuntOrderedAt + 32 && alpha)
  {
   PrintFormat("[ACR] HUNTTEST after the kill: alpha '%1', hunting %2 (want 0)", alpha.m_Order, alpha.IsHunting());
   ACR_TestRegress.Check("hunt: the hunt ends on its own once the team is dead", !alpha.IsHunting(), alpha.m_Order);
  }
  if (h.m_Ticks == 80 || (m_iHuntOrderedAt > 0 && h.m_Ticks == m_iHuntOrderedAt + 34)) ACR_TestRegress.Complete("hunt", 2);
 }
}
