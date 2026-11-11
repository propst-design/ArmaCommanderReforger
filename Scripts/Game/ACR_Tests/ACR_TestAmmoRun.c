// -acrAmmoRunTest 1 (server-capable, about 5 min): AI squads low on ammo (ACR_AmmoRuns,
// ACR_AICommander.ManageAmmoRuns; the user, 7 Nov 2026: "when a squad gets low on ammo they can
// request a ammo truck if safe or they will rtb to the closest base to refill on supplies").
// Variety off (ACR_Variety.s_bOff): every choice is the cheapest. On the test range: Farm (US),
// Crossroads (nobody's, made ours for one check), Outpost (USSR) 620 m south of Farm; an ammo truck
// parked by Farm, Bravo (a US fire team) and a USSR fire team far off whose men the side "sees" where
// the test says.
// - the arithmetic: a squad's line, low and dry, the costs and the pick, the topped-up guard;
// - Alpha shot dry 360 m east of Farm: with enemies seen near it no truck is sent, it walks back to
//   Farm; with none it calls the truck, which meets it, tops it up and is let go;
// - Alpha and Bravo both dry out there with one truck: one truck run only;
// - Alpha dry by Crossroads (ours, under attack): Crossroads is passed over, it walks back to Farm
//   and is restocked there, free for its next job;
// - a squad shot nearly dry is left out of new attacks; -acrAmmoRuns 0 starts no run.
class ACR_TestAmmoRun
{
 static const int CHECKS = 12;
 static const ResourceName AMMO_TRUCK = "{92264FF932676C13}Prefabs/Vehicles/Wheeled/M923A1/M923A1_ammo.et";
 static const string TRUCK_AT = "1400 0 2832";
 static const string EAST = "1700 0 2800";       // out in the field, 360 m east of Farm
 static const string BY_CROSSROADS = "1460 0 2900"; // 45 m from Crossroads, 150 m from Farm
 static const int WALK_S = 200;                  // a walk or drive and the hand-out, at most
 static ref ACR_AICommander s_AI;
 static ACR_Squad s_Crew;
 static IEntity s_Truck;
 static SCR_AIGroup s_BravoGroup;
 static SCR_AIGroup s_EnemyGroup;
 static ACR_Base s_Farm;
 static ACR_Base s_Cross;
 static string s_sCrossOwner;
 static bool s_bCrossChanged;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iNextLook;
 static string s_sDetail;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Variety.s_bOff = true;
  }
  if (s_iStep >= 99) return;
  if (tick > 470)
  {
   Fail(manager, string.Format("timed out in step %1 (%2)", s_iStep, s_sDetail));
   return;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor() || !alpha.GetFaction())
  {
   if (tick == 60) Fail(manager, "no Alpha squad");
   return;
  }
  if (tick == 4)
  {
   vector home = alpha.GetAnchor().GetOrigin();
   manager.SpawnUnit("Ammo Truck", "", AMMO_TRUCK, true, "", h.Ground(TRUCK_AT), h.Ground("1405 0 2836"));
   s_BravoGroup = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, "US"), ACR_Manager.Ground(home + Vector(-12, 0, 12))));
   s_EnemyGroup = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, "USSR"), h.Ground("300 0 1500")));
   return;
  }
  if (tick < 15) return;
  int waited = tick - s_iStepAt;
  ACR_Squad bravo = SquadOf(manager, s_BravoGroup);
  ACR_Squad enemy = SquadOf(manager, s_EnemyGroup);

  // 0: everyone there; the arithmetic; Alpha and Bravo out in the field (a teleport shows next frame).
  if (s_iStep == 0)
  {
   s_Crew = h.NamedSquad(manager, "Ammo Truck");
   if (s_Crew) s_Truck = ACR_Transport.CrewedVehicle(s_Crew);
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sName == "Farm") s_Farm = base;
    if (base.m_sName == "Crossroads") s_Cross = base;
   }
   s_sDetail = string.Format("truck %1, Bravo %2, enemy %3, Farm %4, Crossroads %5", s_Truck != null, bravo != null, enemy != null, s_Farm != null, s_Cross != null);
   if (!s_Truck || !bravo || !enemy || !s_Farm || !s_Cross)
   {
    if (tick > 60) Fail(manager, "the test's units or bases never turned up: " + s_sDetail);
    return;
   }
   if (alpha.GetFaction().GetFactionKey() != "US" || s_Farm.m_sOwner != "US")
   {
    Fail(manager, "Alpha or Farm isn't US");
    return;
   }
   s_AI = new ACR_AICommander(manager, "US");
   s_AI.SetDoctrineForTest(ACR_AIDoctrine.Find("balanced"));
   Arithmetic(alpha);
   Place(alpha, h.Ground(EAST));
   Place(bravo, h.Ground(EAST) + Vector(15, 0, 25));
   Next(tick);
   return;
  }

  // 1: Alpha shot dry there, enemies seen near it: no truck, it walks back.
  if (s_iStep == 1)
  {
   if (waited < 3) return;
   if (!Out(alpha, h.Ground(EAST), waited)) return;
   Strip(alpha);
   alpha.UpdateStatus(manager.m_Logistics);
   bravo.UpdateStatus(manager.m_Logistics);
   Seen(enemy, h.Ground(EAST) + Vector(220, 0, 0), true);
   string danger = s_AI.TruckUnsafeForTest(alpha, alpha.GetAnchor().GetOrigin());
   s_AI.ManageLogisticsForTest();
   ACR_AmmoRun run = s_AI.AmmoRunOf(alpha);
   bool walks = run && !run.m_Crew && run.m_Base == s_Farm && !s_AI.ServiceJobForTest(s_Crew, alpha) && !danger.IsEmpty();
   ACR_TestRegress.Check("ammorun: shot dry out in the field with enemies seen near it, no truck is sent: it walks back to the nearest base", walks,
    string.Format("Alpha %1 pct ammo (%2 pct of its men dry), %3 m from Farm, truck unsafe '%4'; %5", alpha.m_iAmmoPct, alpha.m_iDryPct, (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Farm.GetOrigin()), danger, Describe(run)));
   s_AI.ForgetAmmoForTest(alpha);
   alpha.Halt("Holding");
   Seen(enemy, vector.Zero, false);
   Next(tick);
   return;
  }

  // 2: nothing seen now: it calls the truck.
  if (s_iStep == 2)
  {
   if (waited < 2) return;
   alpha.UpdateStatus(manager.m_Logistics);
   s_AI.ManageLogisticsForTest();
   ACR_AmmoRun truckRun = s_AI.AmmoRunOf(alpha);
   bool truck = truckRun && truckRun.m_Crew == s_Crew && s_AI.ServiceJobForTest(s_Crew, alpha);
   ACR_TestRegress.Check("ammorun: shot dry out in the field where it is safe, it calls the ammo truck, which drives out to meet it", truck,
    string.Format("Alpha %1 pct ammo, %2 m from Farm, the truck %3 m off; %4", alpha.m_iAmmoPct, (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Farm.GetOrigin()), (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Truck.GetOrigin()), Describe(truckRun)));
   if (!truck)
   {
    ACR_TestRegress.Skip("ammorun: the truck tops the squad up where they meet, and is let go", "no truck run");
    s_iStep = 3;
   }
   Next(tick);
   s_iNextLook = tick + 5;
   return;
  }

  // 3: the drive, the meeting and the hand-out.
  if (s_iStep == 3)
  {
   if (tick < s_iNextLook) return;
   s_iNextLook = tick + 5;
   alpha.UpdateStatus(manager.m_Logistics);
   s_AI.ManageLogisticsForTest();
   ACR_AmmoRun meeting = s_AI.AmmoRunOf(alpha);
   float toTruck = vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Truck.GetOrigin());
   s_sDetail = string.Format("Alpha %1 pct ammo, %2 m from the truck, truck stock %3 pct; %4", alpha.m_iAmmoPct, (int)toTruck, manager.m_Logistics.StockPercent(s_Truck), Describe(meeting));
   PrintFormat("[ACR] AMMORUN +%1 s %2", waited, s_sDetail);
   if (meeting && waited < WALK_S) return;
   bool served = !meeting && alpha.m_iAmmoPct >= ACR_AmmoRuns.DONE_PCT && !s_AI.ServiceJobForTest(s_Crew, alpha) && alpha.m_iAITask == ACR_EAITask.FREE;
   ACR_TestRegress.Check("ammorun: the truck tops the squad up where they meet, and is let go", served, string.Format("after %1 s: %2", waited, s_sDetail));
   s_AI.ForgetAmmoForTest(alpha);
   alpha.Halt("Holding");
   Place(bravo, alpha.GetAnchor().GetOrigin() + Vector(15, 0, 25));
   Next(tick);
   return;
  }

  // 4: Alpha and Bravo both dry by one truck: one truck run.
  if (s_iStep == 4)
  {
   if (waited < 3) return;
   s_AI.ForgetAmmoForTest(bravo);
   bravo.Halt("Holding");
   Strip(alpha);
   Strip(bravo);
   alpha.UpdateStatus(manager.m_Logistics);
   bravo.UpdateStatus(manager.m_Logistics);
   s_AI.ManageLogisticsForTest();
   ACR_AmmoRun runA = s_AI.AmmoRunOf(alpha);
   ACR_AmmoRun runB = s_AI.AmmoRunOf(bravo);
   int trucks;
   if (runA && runA.m_Crew == s_Crew) trucks++;
   if (runB && runB.m_Crew == s_Crew) trucks++;
   ACR_TestRegress.Check("ammorun: two squads dry by one truck: one truck run, the other squad isn't sent to it", trucks == 1,
    string.Format("Alpha %1 pct: %2; Bravo %3 pct, %4 m from Alpha: %5", alpha.m_iAmmoPct, Describe(runA), bravo.m_iAmmoPct, (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), bravo.GetAnchor().GetOrigin()), Describe(runB)));
   s_AI.ForgetAmmoForTest(alpha);
   s_AI.ForgetAmmoForTest(bravo);
   alpha.Halt("Holding");
   KillGroup(h, s_BravoGroup);
   s_sCrossOwner = s_Cross.m_sOwner;
   s_Cross.m_sOwner = "US";
   s_bCrossChanged = true;
   s_AI.SetThreatForTest(s_Cross, 3, 0);
   Place(alpha, h.Ground(BY_CROSSROADS));
   Next(tick);
   return;
  }

  // 5: by Crossroads, ours and under attack: passed over for Farm.
  if (s_iStep == 5)
  {
   if (waited < 3) return;
   if (!Out(alpha, h.Ground(BY_CROSSROADS), waited)) return;
   Strip(alpha);
   alpha.UpdateStatus(manager.m_Logistics);
   s_AI.ManageLogisticsForTest();
   ACR_AmmoRun back = s_AI.AmmoRunOf(alpha);
   bool farm = back && !back.m_Crew && back.m_Base == s_Farm;
   ACR_TestRegress.Check("ammorun: a base of ours under attack is passed over: it walks back to the next one", farm,
    string.Format("Alpha %1 m from Crossroads, %2 m from Farm, %3 pct ammo; %4", (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Cross.GetOrigin()), (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Farm.GetOrigin()), alpha.m_iAmmoPct, Describe(back)));
   if (!farm)
   {
    ACR_TestRegress.Skip("ammorun: walked back to the base it is restocked there and free for its next job", "no walk back to Farm");
    RestoreCrossroads();
    s_iStep = 6;
   }
   Next(tick);
   s_iNextLook = tick + 5;
   return;
  }

  // 6: the walk back and the restock.
  if (s_iStep == 6)
  {
   if (tick < s_iNextLook) return;
   s_iNextLook = tick + 5;
   alpha.UpdateStatus(manager.m_Logistics);
   s_AI.ManageLogisticsForTest();
   ACR_AmmoRun walk = s_AI.AmmoRunOf(alpha);
   float toFarm = vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Farm.GetOrigin());
   s_sDetail = string.Format("Alpha %1 pct ammo, %2 m from Farm, task %3, order '%4'; %5", alpha.m_iAmmoPct, (int)toFarm, typename.EnumToString(ACR_EAITask, alpha.m_iAITask), alpha.m_Order, Describe(walk));
   PrintFormat("[ACR] AMMORUN +%1 s %2", waited, s_sDetail);
   if (walk && waited < WALK_S) return;
   bool restocked = !walk && alpha.m_iAmmoPct >= ACR_AmmoRuns.DONE_PCT && alpha.m_iAITask == ACR_EAITask.FREE && toFarm <= s_Farm.m_fRadius + 10;
   ACR_TestRegress.Check("ammorun: walked back to the base it is restocked there and free for its next job", restocked, string.Format("after %1 s: %2", waited, s_sDetail));
   RestoreCrossroads();
   // Full again for the next check, whichever way the last one went.
   if (alpha.m_iAmmoPct < ACR_AmmoRuns.DONE_PCT) Refill(alpha);
   Next(tick);
   return;
  }

  // 7: shot nearly dry, it isn't sent on a new attack; -acrAmmoRuns 0 starts no run.
  if (s_iStep == 7)
  {
   if (waited < 5) return;
   alpha.UpdateStatus(manager.m_Logistics);
   bool fullShort = s_AI.AmmoShortForAttackForTest(alpha);
   int fullPct = alpha.m_iAmmoPct;
   Strip(alpha);
   alpha.UpdateStatus(manager.m_Logistics);
   array<ACR_Squad> pool = {};
   s_AI.AttackPoolForTest(pool);
   bool dryShort = s_AI.AmmoShortForAttackForTest(alpha);
   bool kept = !fullShort && dryShort && !pool.Contains(alpha);
   ACR_TestRegress.Check("ammorun: a squad shot nearly dry is left out of new attacks (it would only fall back at once)", kept,
    string.Format("full (%1 pct) short %2, dry (%3 pct) short %4, in the attack pool %5", fullPct, fullShort, alpha.m_iAmmoPct, dryShort, pool.Contains(alpha)));
   ACR_Options.Set("acrAmmoRuns", "0");
   s_AI.ManageLogisticsForTest();
   ACR_AmmoRun off = s_AI.AmmoRunOf(alpha);
   ACR_Options.Unset("acrAmmoRuns");
   ACR_TestRegress.Check("ammorun: -acrAmmoRuns 0 starts no ammo run (the old rules)", !off,
    string.Format("Alpha %1 pct ammo, task %2, order '%3'; %4", alpha.m_iAmmoPct, typename.EnumToString(ACR_EAITask, alpha.m_iAITask), alpha.m_Order, Describe(off)));
   s_AI.ForgetAmmoForTest(alpha);
   if (alpha.m_iAITask == ACR_EAITask.RESUPPLY) alpha.SetAITask(ACR_EAITask.FREE);
   alpha.Halt("Holding");
   ACR_TestRegress.Complete("ammorun", CHECKS);
   s_iStep = 99;
  }
 }

 // The squad's anchor got to where it was teleported (a teleport shows on the next frame or so).
 protected static bool Out(ACR_Squad squad, vector to, int waited)
 {
  float off = vector.DistanceXZ(squad.GetAnchor().GetOrigin(), to);
  if (off < 30) return true;
  if (waited > 20) Fail(ACR_Manager.GetInstance(), string.Format("%1 never got to %2 (%3 m off)", squad.GetName(), to, (int)off));
  return false;
 }

 // Every man given his magazines back, as a base's restock would.
 protected static void Refill(ACR_Squad squad)
 {
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   if (!soldier || !ACR_Respawn.IsAlive(soldier)) continue;
   SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
   int target = Math.Max(2, squad.MagazineTarget(soldier) - 1);
   if (inventory && inventory.IsResupplyMagazinesAvailable(target)) inventory.ResupplyMagazines(target);
  }
 }

 // The checks that need no battle: the line, low and done, the costs and the pick, the topped guard.
 protected static void Arithmetic(ACR_Squad alpha)
 {
  float exact = s_AI.AmmoLineForTest(alpha);
  ACR_Variety.s_bOff = false;
  float varied = ACR_AmmoRuns.Line(45, "ammo US Alpha 1");
  float again = ACR_AmmoRuns.Line(45, "ammo US Alpha 1");
  float other = ACR_AmmoRuns.Line(45, "ammo US Bravo 2");
  ACR_Variety.s_bOff = true;
  bool spread = varied >= 45 * (1 - ACR_AmmoRuns.LINE_SPREAD) - 0.01 && varied <= 45 * (1 + ACR_AmmoRuns.LINE_SPREAD) + 0.01 && other >= 38 && other <= 52;
  ACR_TestRegress.Check("ammorun: a squad's line is its doctrine's (45 balanced), varied up to 15 pct per squad and the same for the same seed", Math.AbsFloat(exact - 45) < 0.01 && varied == again && spread,
   string.Format("variety off %1; on: %2 (again %3), another squad %4", exact, varied, again, other));

  bool low = ACR_AmmoRuns.Low(40, 0, 45) && !ACR_AmmoRuns.Low(60, 0, 45) && ACR_AmmoRuns.Low(70, 40, 45) && !ACR_AmmoRuns.Low(-1, 90, 45);
  bool done = ACR_AmmoRuns.Done(85, 0) && !ACR_AmmoRuns.Done(85, 40) && !ACR_AmmoRuns.Done(60, 0) && ACR_AmmoRuns.Done(-1, 0);
  ACR_TestRegress.Check("ammorun: low under its line or with a third of its men nearly dry; done back at 80 pct with nobody dry", low && done,
   string.Format("low %1, done %2", low, done));

  // Shot dry 360 m out, a truck 300 m off: the truck. No truck: the walk back. Barely low and 2 km out: it carries on.
  float truck = ACR_AmmoRuns.TruckCost(300, 0, 0, 1);
  float walk = ACR_AmmoRuns.BaseCost(320, false, false, 0, false, 0);
  float empty = ACR_AmmoRuns.WaitCost(1);
  float barely = ACR_AmmoRuns.WaitCost(0.05);
  float far = ACR_AmmoRuns.BaseCost(2000, false, false, 0, false, 0);
  array<float> withTruck = {empty, truck, walk};
  array<float> noTruck = {empty, walk};
  array<float> farOut = {barely, far};
  int pickTruck = ACR_AmmoRuns.Pick(withTruck, "test");
  int pickWalk = ACR_AmmoRuns.Pick(noTruck, "test");
  int pickWait = ACR_AmmoRuns.Pick(farOut, "test");
  bool weighs = ACR_AmmoRuns.BaseCost(100, false, true, 0, false, 0) > ACR_AmmoRuns.BaseCost(100, false, false, 0, false, 0) && ACR_AmmoRuns.BaseCost(300, false, false, 3, false, 0) < walk && ACR_AmmoRuns.TruckCost(300, 0, 1, 1) > truck;
  ACR_TestRegress.Check("ammorun: the cheapest is taken with variety off: the truck, else the walk back, and a squad barely low far out carries on", pickTruck == 1 && pickWalk == 1 && pickWait == 0 && weighs,
   string.Format("costs: truck %1 s, walk %2 s, carry on empty %3 s; picks %4 / %5 / %6; holders, missing men and risk weighed %7", (int)truck, (int)walk, (int)empty, pickTruck, pickWalk, pickWait, weighs));

  bool holds = ACR_AmmoRuns.ToppedHolds(100000, 50000, 60, 55);
  bool expires = !ACR_AmmoRuns.ToppedHolds(50000 + ACR_AmmoRuns.TOPPED_MS + 1, 50000, 60, 55);
  bool drops = !ACR_AmmoRuns.ToppedHolds(100000, 50000, 60, 35);
  ACR_TestRegress.Check("ammorun: a refill that couldn't bring it back starts no new run for a while, unless its ammo drops much further", holds && expires && drops,
   string.Format("holds %1, expires %2, a further drop %3", holds, expires, drops));
 }

 // Every spare magazine taken and the loaded ones emptied.
 protected static void Strip(ACR_Squad squad)
 {
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   if (!soldier || !ACR_Respawn.IsAlive(soldier)) continue;
   InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(soldier.FindComponent(InventoryStorageManagerComponent));
   if (inventory)
   {
    array<IEntity> items = {};
    inventory.GetItems(items);
    foreach (IEntity item : items)
    {
     if (!item || !item.FindComponent(BaseMagazineComponent)) continue;
     if (item.GetParent() && item.GetParent().FindComponent(BaseWeaponComponent)) continue; // loaded
     inventory.TryDeleteItem(item);
    }
   }
   BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
   if (!weapons) continue;
   array<IEntity> list = {};
   weapons.GetWeaponsList(list);
   foreach (IEntity weaponEntity : list)
   {
    BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
    if (!weapon) continue;
    string slot = weapon.GetWeaponSlotType();
    if (slot != "primary" && slot != "secondary") continue;
    BaseMagazineComponent loaded = weapon.GetCurrentMagazine();
    if (loaded) loaded.SetAmmoCount(0);
   }
  }
 }

 // The squad's men stood in a short line at 'at'.
 protected static void Place(ACR_Squad squad, vector at)
 {
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (int m, AIAgent agent : agents)
  {
   BaseGameEntity body = BaseGameEntity.Cast(agent.GetControlledEntity());
   if (!body) continue;
   vector mat[4];
   Math3D.MatrixIdentity4(mat);
   mat[3] = ACR_Manager.Ground(at + Vector(m * 1.5, 0, 0));
   body.Teleport(mat);
  }
 }

 // The test's AI commander "has seen" the enemy's men at 'at' (or forgets them).
 protected static void Seen(ACR_Squad enemy, vector at, bool seen)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  array<AIAgent> agents = {};
  enemy.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   if (seen) s_AI.NoteAtForTest(body, at, now);
   else s_AI.ForgetForTest(body);
  }
 }

 protected static string Describe(ACR_AmmoRun run)
 {
  if (!run) return "no ammo run";
  if (run.m_Crew) return string.Format("truck run with %1 (arrived %2, gathered %3)", run.m_Crew.GetName(), run.m_fArrivedAt > 0, run.m_bGathered);
  if (run.m_Base) return string.Format("walking back to %1 (arrived %2, served %3)", run.m_Base.m_sName, run.m_fArrivedAt > 0, run.m_fServedAt > 0);
  return "an ammo run going nowhere";
 }

 protected static ACR_Squad SquadOf(ACR_Manager manager, SCR_AIGroup group)
 {
  if (!group) return null;
  return manager.FindSquadByGroup(group);
 }

 protected static void KillGroup(ACR_TestHarness h, SCR_AIGroup group)
 {
  if (!group) return;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents) h.Kill(agent.GetControlledEntity());
 }

 protected static void RestoreCrossroads()
 {
  if (!s_bCrossChanged || !s_Cross) return;
  s_Cross.m_sOwner = s_sCrossOwner;
  if (s_AI) s_AI.SetThreatForTest(s_Cross, 0, 0);
  s_bCrossChanged = false;
 }

 protected static void Next(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 protected static void Fail(ACR_Manager manager, string why)
 {
  if (s_iStep >= 99) return;
  RestoreCrossroads();
  ACR_Options.Unset("acrAmmoRuns");
  ACR_TestRegress.Abort("ammorun", why);
  s_iStep = 99;
 }
}
