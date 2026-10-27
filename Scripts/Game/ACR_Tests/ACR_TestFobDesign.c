// ACR_TestFobDesign: Jacob's 18 Oct FOB decisions and review fixes, on real compositions.
// Layout and footprint checks, smart slots (adoption, blocked spots, vehicles on a spot),
// destructible parts, FIA tier-1 effects, sapper planting/defusal/HQ-only detonation and fuse
// handover, HQ-less FOBs (offline, capture, AI rebuild, retirement), builder give-up, and the
// AI's construction-truck purchase guard. Called by srv:structures (tick 50).
class ACR_TestFobDesign
{
 static const int CHECKS = 35; // (one per Check below)
 static const ResourceName USSR_RIFLEMAN = "{DCB41B3746FDD1BE}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Rifleman.et";
 static const ResourceName US_RIFLEMAN = "{26A9756790131354}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Rifleman.et";
 static const ResourceName HOSPITAL = "{D0281DAF0B131743}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_FieldHospital_M_Conflict_US_01.et";

 static ACR_Base s_Fob2;
 static IEntity s_Fuel2, s_Depot2;

 static void Run(ACR_Manager manager, ACR_Base fob1, ACR_Squad alpha, IEntity truck)
 {
  if (!manager || !fob1 || !alpha || !alpha.GetAnchor() || !truck)
  {
   ACR_TestRegress.Abort("structures", "FOB design test fixture missing");
   return;
  }
  vector home = alpha.GetAnchor().GetOrigin();
  Layout();
  Footprints(manager, home);
  FiaTiers(home);
  if (!MakeFob2(manager, home, fob1))
  {
   ACR_TestRegress.Abort("structures", "no spot for the second FOB fixture");
   return;
  }
  SmartSlots(manager);
  Sapper(manager, alpha, home);
  HqLost(manager, alpha, truck);
  Purchases(manager, truck);
  ACR_AICommander responder = new ACR_AICommander(manager, "US");
  ACR_TestRegress.Check("fobdesign: AI sends its nearest squad to defuse a charge", responder.RespondToCharge(home + Vector(20, 0, 0)), "");
  alpha.ClearOrders();
 }

 static void Layout()
 {
  bool apart = true;
  string detail = "";
  for (int a = 0; a < ACR_FobSlots.SLOT_COUNT; a++)
  {
   vector pa = ACR_FobSlots.SlotOffset(a);
   float ra = ACR_FobSlots.SlotRadius(a, ACR_FobSlots.MaxTier(a));
   if (pa.Length() > ACR_FobBuild.CP_RADIUS || pa.Length() < ra + 10) { apart = false; detail += string.Format(" slot %1 vs HQ/circle", a); }
   for (int b = a + 1; b < ACR_FobSlots.SLOT_COUNT; b++)
   {
    float rb = ACR_FobSlots.SlotRadius(b, ACR_FobSlots.MaxTier(b));
    if (vector.DistanceXZ(pa, ACR_FobSlots.SlotOffset(b)) < ra + rb) { apart = false; detail += string.Format(" %1-%2", a, b); }
   }
  }
  ACR_TestRegress.Check("fobdesign: slot footprints never overlap each other or the HQ", apart, detail);
  bool parkingClear = ACR_FobBuild.PARKING_OFFSET.Length() >= ACR_FobBuild.HQ_RADIUS + 3;
  for (int p = 0; p < ACR_FobSlots.SLOT_COUNT; p++)
   if (vector.DistanceXZ(ACR_FobBuild.PARKING_OFFSET, ACR_FobSlots.SlotOffset(p)) < ACR_FobSlots.SlotRadius(p, ACR_FobSlots.MaxTier(p)) + 3) parkingClear = false;
  ACR_TestRegress.Check("fobdesign: construction truck parks clear of the HQ and every slot", parkingClear, "");
 }

 static void Footprints(ACR_Manager manager, vector home)
 {
  string why;
  ACR_TestRegress.Check("fobdesign: footprint refuses water", !ACR_FobBuild.FootprintClear(Vector(0, -50, 0), 8, null, why) && why.Contains("water"), why);
  vector spot = ACR_Manager.Ground(home + Vector(0, 0, -260));
  IEntity hospital = ACR_TestStructures.Place(HOSPITAL, spot);
  string blockWhy;
  bool blocked = hospital && !ACR_FobBuild.FootprintClear(spot + Vector(6, 0, 0), 8, null, blockWhy) && blockWhy.Contains("hospital");
  ACR_TestRegress.Check("fobdesign: footprint refuses an existing composition", blocked, blockWhy);
  manager.m_Structures.ACR_TestForceTick();
  // Vanilla service tents carry no destruction component, so damage cannot wreck them;
  // dismantling (deletion) is what frees a slot (covered with the adopted fuel storage).
  ACR_Structure structure = manager.m_Structures.FindStructure(hospital);
  ACR_TestRegress.Check("fobdesign: vanilla service tents are not destructible", structure && structure.m_aMajorParts.IsEmpty() && !ACR_Structures.IsWrecked(structure), "");
  if (hospital) SCR_EntityHelper.DeleteEntityAndChildren(hospital);
 }

 static void FiaTiers(vector home)
 {
  bool sameModel = ACR_FobSlots.PrefabFor(ACR_EStructure.BARRACKS, "FIA", 1) == ACR_FobSlots.PrefabFor(ACR_EStructure.BARRACKS, "FIA", 0) && ACR_FobSlots.SlotName(ACR_FobSlots.SLOT_BARRACKS, 1, "FIA").Contains("small model") && !ACR_FobSlots.SlotName(ACR_FobSlots.SLOT_BARRACKS, 1, "US").Contains("small model");
  ACR_TestRegress.Check("fobdesign: FIA tier-1 keeps the small model and says so", sameModel, ACR_FobSlots.SlotName(ACR_FobSlots.SLOT_BARRACKS, 1, "FIA"));
  IEntity quarters = ACR_TestStructures.Place(ACR_FobSlots.BARRACKS_S_FIA, ACR_Manager.Ground(home + Vector(0, 0, -330)));
  IEntity bay = ACR_TestStructures.Place(ACR_FobSlots.REPAIR_S_FIA, ACR_Manager.Ground(home + Vector(0, 0, -360)));
  int smallDefenders = ACR_Structures.HousingDefenders(quarters);
  float smallRepair = ACR_Structures.RepairFraction(bay);
  SCR_CampaignBuildingCompositionComponent quartersComp, bayComp;
  if (quarters) quartersComp = SCR_CampaignBuildingCompositionComponent.Cast(quarters.FindComponent(SCR_CampaignBuildingCompositionComponent));
  if (bay) bayComp = SCR_CampaignBuildingCompositionComponent.Cast(bay.FindComponent(SCR_CampaignBuildingCompositionComponent));
  if (quartersComp) quartersComp.ACR_SetTier(1);
  if (bayComp) bayComp.ACR_SetTier(1);
  bool tierEffect = smallDefenders == 11 && smallRepair == 0.15 && ACR_Structures.HousingDefenders(quarters) == 19 && ACR_Structures.RepairFraction(bay) == 0.21;
  ACR_TestRegress.Check("fobdesign: FIA tier-1 barracks houses 19 and tier-1 bay repairs 21%", tierEffect, string.Format("%1/%2 -> %3/%4", smallDefenders, smallRepair, ACR_Structures.HousingDefenders(quarters), ACR_Structures.RepairFraction(bay)));
  if (quarters) SCR_EntityHelper.DeleteEntityAndChildren(quarters);
  if (bay) SCR_EntityHelper.DeleteEntityAndChildren(bay);
 }

 static bool MakeFob2(ACR_Manager manager, vector home, ACR_Base fob1)
 {
  vector spot2;
  bool found = false;
  for (int ring = 0; ring < 3 && !found; ring++)
  {
   for (int i = 0; i < 24; i++)
   {
    float angle = i * 15 * Math.DEG2RAD;
    float dist = 800 + ring * 200;
    vector candidate = ACR_Manager.Ground(home + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist));
    if (!ACR_Manager.OnMap(candidate) || BBB_Terrain.IsWaterAt(candidate) || ACR_TestStructures.InsideABase(candidate, 150)) continue;
    if (vector.DistanceXZ(candidate, fob1.GetOrigin()) < 300) continue;
    spot2 = candidate;
    found = true;
    break;
   }
  }
  if (!found) return false;
  ACR_TestStructures.Place(ACR_FobBuild.HQ_USSR, spot2);
  s_Fuel2 = ACR_TestStructures.Place(ACR_FobSlots.FUEL_USSR, ACR_FobSlots.SlotPosition(spot2, ACR_FobSlots.SLOT_FUEL));
  manager.m_Structures.ACR_TestForceTick();
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && base.m_bBuilt && !base.m_bRetired && vector.DistanceXZ(base.GetOrigin(), spot2) < 5) s_Fob2 = base;
  ACR_TestRegress.Check("fobdesign: Soviet FOB fixture built from its HQ", s_Fob2 && s_Fob2.m_sOwner == "USSR" && s_Fob2.m_FobSupply, "");
  return s_Fob2 != null;
 }

 static void SmartSlots(ACR_Manager manager)
 {
  ACR_FobSlots.Advance(s_Fob2, 0.1);
  ACR_FobSlot fuel = s_Fob2.m_aFobSlots[ACR_FobSlots.SLOT_FUEL];
  ACR_TestRegress.Check("fobdesign: existing fuel storage counts as its slot", fuel.m_eState == ACR_EFobSlotState.BUILT && fuel.m_bAdopted && fuel.m_Entity == s_Fuel2, "");
  ACR_TestRegress.Check("fobdesign: panel shows the adopted slot as built", ACR_FobSlots.FeedState(s_Fob2, ACR_FobSlots.SLOT_FUEL) == 300, ACR_FobSlots.FeedState(s_Fob2, ACR_FobSlots.SLOT_FUEL).ToString());
  // Dismantling the adopted building frees its slot; a new one is adopted again.
  if (s_Fuel2) SCR_EntityHelper.DeleteEntityAndChildren(s_Fuel2);
  ACR_FobSlots.Advance(s_Fob2, 0.1);
  bool freed = fuel.m_eState == ACR_EFobSlotState.EMPTY && !fuel.m_Entity;
  s_Fuel2 = ACR_TestStructures.Place(ACR_FobSlots.FUEL_USSR, ACR_FobSlots.SlotPosition(s_Fob2.GetOrigin(), ACR_FobSlots.SLOT_FUEL));
  manager.m_Structures.ACR_TestForceTick();
  ACR_FobSlots.Advance(s_Fob2, 0.1);
  ACR_TestRegress.Check("fobdesign: dismantled building frees its slot", freed && fuel.m_Entity == s_Fuel2, "");

  SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(s_Fob2.m_FobSupply);
  if (store) store.SetResourceValue(1000);
  s_Depot2 = ACR_TestStructures.Place(ACR_FobSlots.DEPOT_USSR, ACR_FobSlots.SlotPosition(s_Fob2.GetOrigin(), ACR_FobSlots.SLOT_AMMO));
  string why;
  string ordered = ACR_FobSlots.OrderUpgradeBase(manager, "USSR", s_Fob2, ACR_FobSlots.SLOT_AMMO, why);
  ACR_FobSlot ammo = s_Fob2.m_aFobSlots[ACR_FobSlots.SLOT_AMMO];
  ammo.m_fNextSpotCheck = 0;
  ACR_FobSlots.RefreshSpots(s_Fob2, GetGame().GetWorld().GetWorldTime());
  ACR_TestRegress.Check("fobdesign: blocked spot refuses its upgrade and shows as blocked", ordered.IsEmpty() && why.Contains("spot blocked") && ACR_FobSlots.FeedState(s_Fob2, ACR_FobSlots.SLOT_AMMO) == 1 && store.GetResourceValue() == 1000, why);

  ACR_FobSlots.s_bSkipSpotChecksForTest = true;
  string relayOrder = ACR_FobSlots.OrderUpgradeBase(manager, "USSR", s_Fob2, ACR_FobSlots.SLOT_RELAY, why);
  ACR_FobSlot relay = s_Fob2.m_aFobSlots[ACR_FobSlots.SLOT_RELAY];
  relay.m_fProgress = relay.m_fDuration - 0.1;
  IEntity parked = ACR_TestStructures.Place(ACR_TestStructures.HUMVEE, ACR_FobSlots.SlotPosition(s_Fob2.GetOrigin(), ACR_FobSlots.SLOT_RELAY));
  ACR_FobSlots.Advance(s_Fob2, 1.0);
  bool held = relay.m_eState == ACR_EFobSlotState.BUILDING && relay.m_bAwaitingClear;
  if (parked) parked.SetOrigin(parked.GetOrigin() + Vector(300, 0, 0));
  ACR_FobSlots.Advance(s_Fob2, 1.0);
  ACR_FobSlots.s_bSkipSpotChecksForTest = false;
  ACR_TestRegress.Check("fobdesign: vehicle on a finished spot holds completion until it moves", !relayOrder.IsEmpty() && held && relay.m_eState == ACR_EFobSlotState.BUILT && relay.m_Entity, why);
  if (parked) SCR_EntityHelper.DeleteEntityAndChildren(parked);
  manager.m_Structures.ACR_TestForceTick();
 }

 static void Sapper(ACR_Manager manager, ACR_Squad alpha, vector home)
 {
  ACR_AICommander attacker = new ACR_AICommander(manager, "US");
  ACR_AIAttack raid = new ACR_AIAttack();
  raid.m_Target = s_Fob2;
  raid.m_bFobRaid = true;
  raid.m_Squads.Insert(alpha);
  raid.m_SapperDemo = new ACR_SapperDemo(manager, attacker, raid);
  IEntity hq = manager.m_Structures.HqEntityOf(s_Fob2);
  if (!hq)
  {
   ACR_TestRegress.Abort("structures", "FOB 2 has no HQ entity");
   return;
  }
  vector hqPos = hq.GetOrigin();
  IEntity guard = ACR_Manager.SpawnPrefab(USSR_RIFLEMAN, ACR_Manager.Ground(hqPos + Vector(6, 0, 0)));
  alpha.GetAnchor().SetOrigin(hqPos + Vector(2, 0, 2));
  float t = 100000;
  raid.m_SapperDemo.Tick(t);
  raid.m_SapperDemo.Tick(t + 1000);
  ACR_TestRegress.Check("fobdesign: sapper waits while a defender is within 30 m", guard && raid.m_SapperDemo.State() == ACR_ESapperDemoState.APPROACHING, string.Format("state=%1", raid.m_SapperDemo.State()));
  if (guard) SCR_EntityHelper.DeleteEntityAndChildren(guard);
  raid.m_SapperDemo.Tick(t + 1500);
  ACR_TestRegress.Check("fobdesign: sapper starts planting once the HQ is clear", raid.m_SapperDemo.State() == ACR_ESapperDemoState.PLANTING && alpha.OrderState() == ACR_EOrderState.HOLD && raid.m_SapperDemo.Planter() != null, string.Format("state=%1", raid.m_SapperDemo.State()));
  // The live planting (animation, wiring, detonator) runs over real ticks: ACR_TestSapperLive.
  alpha.GetAnchor().SetOrigin(home);
  raid.m_SapperDemo.SetStateForTest(ACR_ESapperDemoState.NONE);
  alpha.ClearOrders();

  // A primed vanilla charge by the HQ: an AI defender standing at it disarms it.
  ACR_AIAttack defuseRaid = new ACR_AIAttack();
  defuseRaid.m_Target = s_Fob2;
  defuseRaid.m_bFobRaid = true;
  defuseRaid.m_SapperDemo = new ACR_SapperDemo(manager, attacker, defuseRaid);
  IEntity armed = ACR_Manager.SpawnPrefab(ACR_Demolition.CHARGE_WEST, ACR_Manager.Ground(hqPos + Vector(3, 0, 0)));
  SCR_ExplosiveChargeComponent armedComp = ACR_SapperDemo.ChargeComponent(armed);
  if (armedComp) armedComp.ArmWithTimedFuze(true);
  defuseRaid.m_SapperDemo.SetChargeForTest(armed);
  defuseRaid.m_SapperDemo.SetTargetOwnerForTest("USSR");
  defuseRaid.m_SapperDemo.SetStateForTest(ACR_ESapperDemoState.FALLBACK);
  defuseRaid.m_SapperDemo.SetFuseDeadlineForTest(t + 60000);
  IEntity defuser;
  if (armed) defuser = ACR_Manager.SpawnPrefab(USSR_RIFLEMAN, armed.GetOrigin() + Vector(1, 0, 0));
  defuseRaid.m_SapperDemo.Tick(t + 6600);
  defuseRaid.m_SapperDemo.Tick(t + 8600);
  defuseRaid.m_SapperDemo.Tick(t + 10600);
  defuseRaid.m_SapperDemo.Tick(t + 12600);
  ACR_TestRegress.Check("fobdesign: defender at a real armed charge defuses it in 5 s", armedComp && defuseRaid.m_SapperDemo.State() == ACR_ESapperDemoState.NONE && defuseRaid.m_SapperDemo.IsDefused() && armed == null && manager.m_Structures.HqEntityOf(s_Fob2) == hq, string.Format("state=%1 progress=%2", defuseRaid.m_SapperDemo.State(), defuseRaid.m_SapperDemo.DefuseProgress()));
  if (defuser) SCR_EntityHelper.DeleteEntityAndChildren(defuser);

  // Shovel dismantling of an enemy FOB HQ: owners always; enemies only contested and clear.
  IEntity enemy = ACR_Manager.SpawnPrefab(US_RIFLEMAN, ACR_Manager.Ground(hqPos + Vector(0, 0, 5)));
  IEntity owner = ACR_Manager.SpawnPrefab(USSR_RIFLEMAN, ACR_Manager.Ground(hqPos + Vector(0, 0, -5)));
  string gateWhy;
  ACR_TestRegress.Check("fobdesign: owners may dismantle their own HQ", ACR_Demolition.CanDismantle(hq, owner, gateWhy), gateWhy);
  s_Fob2.m_bContested = false;
  ACR_TestRegress.Check("fobdesign: enemy cannot dismantle an uncontested FOB HQ", !ACR_Demolition.CanDismantle(hq, enemy, gateWhy) && gateWhy.Contains("Contest"), gateWhy);
  s_Fob2.m_bContested = true;
  ACR_TestRegress.Check("fobdesign: enemy cannot dismantle while defenders are within 30 m", !ACR_Demolition.CanDismantle(hq, enemy, gateWhy) && gateWhy.Contains("defenders"), gateWhy);
  if (owner) SCR_EntityHelper.DeleteEntityAndChildren(owner);
  ACR_TestRegress.Check("fobdesign: enemy may dismantle a contested HQ with no defenders near", ACR_Demolition.CanDismantle(hq, enemy, gateWhy), gateWhy);
  s_Fob2.m_bContested = false;
  if (enemy) SCR_EntityHelper.DeleteEntityAndChildren(enemy);

  // Anyone's vanilla charge that goes off by the HQ destroys it (players and AI alike).
  IEntity playerCharge = ACR_Manager.SpawnPrefab(ACR_Demolition.CHARGE_EAST, ACR_Manager.Ground(hqPos + Vector(4, 0, 0)));
  SCR_ExplosiveChargeComponent playerComp = ACR_SapperDemo.ChargeComponent(playerCharge);
  if (playerComp && playerComp.GetTrigger()) playerComp.GetTrigger().UseTrigger();
  ACR_FobSlot fuel = s_Fob2.m_aFobSlots[ACR_FobSlots.SLOT_FUEL];
  ACR_TestRegress.Check("fobdesign: a demolition charge going off by the HQ destroys the HQ only", playerComp && hq == null && s_Fob2.m_bHqLost && !s_Fob2.m_bRetired && fuel.m_eState == ACR_EFobSlotState.BUILT && s_Fuel2, string.Format("hq=%1 lost=%2 retired=%3", hq != null, s_Fob2.m_bHqLost, s_Fob2.m_bRetired));

  string why;
  bool upgradeRefused = ACR_FobSlots.OrderUpgradeBase(manager, "USSR", s_Fob2, ACR_FobSlots.SLOT_HOSPITAL, why) == "" && why.Contains("HQ destroyed");
  string delivery = ACR_FobSupply.Order(manager, "USSR", s_Fob2);
  bool offline = !ACR_LogisticsRules.CanDeploy(s_Fob2) && !ACR_LogisticsRules.CanReinforce(s_Fob2) && upgradeRefused && delivery.Contains("HQ destroyed") && ACR_FobSupply.Nearby("USSR", s_Fob2.GetOrigin()) == null;
  ACR_TestRegress.Check("fobdesign: an HQ-less FOB is offline (deploy, services, deliveries, upgrades)", offline, delivery);

  ACR_AIAttack again = new ACR_AIAttack();
  again.m_Target = s_Fob2;
  again.m_bFobRaid = true;
  again.m_Squads.Insert(alpha);
  again.m_SapperDemo = new ACR_SapperDemo(manager, attacker, again);
  again.m_SapperDemo.Tick(t + 16000);
  ACR_TestRegress.Check("fobdesign: no demolition raid on an HQ-less FOB", again.m_SapperDemo.State() == ACR_ESapperDemoState.NONE, "");

  // A primed timed charge outlives its plan and the commander, and goes off once.
  ACR_AIAttack orphan = new ACR_AIAttack();
  orphan.m_Target = s_Fob2;
  orphan.m_bFobRaid = true;
  orphan.m_SapperDemo = new ACR_SapperDemo(manager, attacker, orphan);
  orphan.m_SapperDemo.SetStateForTest(ACR_ESapperDemoState.FALLBACK);
  IEntity orphanCharge = ACR_Manager.SpawnPrefab(ACR_Demolition.CHARGE_WEST, ACR_Manager.Ground(s_Fob2.GetOrigin() + Vector(0, 0, 300)));
  SCR_ExplosiveChargeComponent orphanComp = ACR_SapperDemo.ChargeComponent(orphanCharge);
  if (orphanComp)
  {
   orphanComp.SetFuzeTime(600, true); // our deadline below decides, not the game's clock
   orphanComp.ArmWithTimedFuze(true);
  }
  orphan.m_SapperDemo.SetChargeForTest(orphanCharge);
  orphan.m_SapperDemo.SetTargetOwnerForTest("USSR");
  orphan.m_SapperDemo.SetTimedForTest(true);
  float u = 200000;
  orphan.m_SapperDemo.SetFuseDeadlineForTest(u + 20000);
  manager.RegisterDemolition(orphan);
  ACR_SapperDemo orphanDemo = orphan.m_SapperDemo;
  orphan = null;
  manager.TickDemolitions(u + 10000);
  int pendingBefore = manager.ACR_TestPendingDemolitions();
  manager.TickDemolitions(u + 25000);
  ACR_TestRegress.Check("fobdesign: an orphaned primed charge waits for its fuse, then goes off once", pendingBefore >= 1 && manager.ACR_TestPendingDemolitions() == 0 && orphanDemo && orphanDemo.IsDetonated(), string.Format("pending %1", pendingBefore));
 }

 // On the way to its site, a truck closing in keeps the build alive past three minutes; one that
 // stops getting nearer gives up three minutes later (a pn11 Everon soak, 27 Oct: builds 2-4 km off
 // were given up while their trucks still drove).
 static void OnTheWay(ACR_Squad alpha, IEntity truck, vector site, float t0)
 {
  vector from = truck.GetOrigin();
  bool ordered = alpha.EstablishFob(site);
  alpha.SetFobLastProgressForTest(t0 - 170000);
  alpha.TickEstablishFob(t0);
  vector closer = from + (site - from).Normalized() * 40;
  truck.SetOrigin(ACR_Manager.Ground(closer));
  alpha.TickEstablishFob(t0 + 175000);
  bool kept = alpha.OrderState() == ACR_EOrderState.BUILD_FOB;
  alpha.TickEstablishFob(t0 + 175000 + 181000);
  bool gaveUp = alpha.OrderState() != ACR_EOrderState.BUILD_FOB;
  ACR_TestRegress.Check("fobdesign: on the way, closing in on the site counts as progress; no nearer for three minutes gives up", ordered && kept && gaveUp, string.Format("ordered %1, kept after 40 m closer %2, gave up when stopped %3; the truck %4 m from the site", ordered, kept, gaveUp, (int)vector.DistanceXZ(from, site)));
  if (alpha.OrderState() == ACR_EOrderState.BUILD_FOB) alpha.CancelEstablishFob();
  truck.SetOrigin(from);
 }

 static void HqLost(ACR_Manager manager, ACR_Squad alpha, IEntity truck)
 {
  manager.m_Bases.DebugCapture(manager, s_Fob2, "US");
  ACR_Structure fuelStructure = manager.m_Structures.FindStructure(s_Fuel2);
  ACR_FobSlot fuel = s_Fob2.m_aFobSlots[ACR_FobSlots.SLOT_FUEL];
  ACR_TestRegress.Check("fobdesign: capture hands the FOB's buildings over", s_Fob2.m_sOwner == "US" && fuelStructure && fuelStructure.m_sFaction == "US" && ACR_FobSlots.IsBuiltForOwner(fuel, "US"), "");

  ACR_AICommander builderAI = new ACR_AICommander(manager, "US");
  ACR_TestRegress.Check("fobdesign: AI picks its own HQ-less FOB to rebuild first", builderAI.HqLostFob() == s_Fob2, "");

  vector truckPos = truck.GetOrigin();
  IEntity leader = alpha.GetLeader();
  vector leaderPos;
  if (leader) leaderPos = leader.GetOrigin();
  float truckStock = manager.m_Logistics.StockOf(truck);
  manager.m_Logistics.Use(truck, truckStock);
  manager.m_Logistics.AddStock(truck, 40);
  truck.SetOrigin(alpha.GetAnchor().GetOrigin() + Vector(10, 0, 0));
  float now = GetGame().GetWorld().GetWorldTime();
  bool ordered = alpha.EstablishFob(s_Fob2.GetOrigin() + Vector(6, 0, 6));
  alpha.SetFobLastProgressForTest(now - 181000);
  alpha.TickEstablishFob(now + 1000);
  ACR_TestRegress.Check("fobdesign: three minutes without progress abandons the build and refunds", ordered && alpha.OrderState() != ACR_EOrderState.BUILD_FOB && alpha.FobReservedLoads() == 0 && manager.m_Logistics.StockOf(truck) == 40 && alpha.m_fFobGaveUpAt == now + 1000, "");
  OnTheWay(alpha, truck, s_Fob2.GetOrigin() + Vector(6, 0, 6), now + 5000);

  // The AI commander's own loop (Stage 3): restore first, using the squad with the truck.
  builderAI.SetFobEvalTimeForTest(0);
  builderAI.ManageFobBuild();
  ACR_Squad aiBuilder = builderAI.FobBuilder();
  ACR_TestRegress.Check("fobdesign: AI dispatches its truck squad to rebuild the HQ at the FOB centre", aiBuilder && aiBuilder.FobRestore() == s_Fob2 && vector.DistanceXZ(aiBuilder.FobSite(), s_Fob2.GetOrigin()) < 0.1, string.Format("builder=%1", aiBuilder != null));
  // Finish the rebuild with Alpha (another squad near the truck may have been picked).
  if (aiBuilder && aiBuilder != alpha) aiBuilder.Halt("Holding position");
  if (alpha.OrderState() != ACR_EOrderState.BUILD_FOB) alpha.EstablishFob(s_Fob2.GetOrigin() + Vector(6, 0, 6));
  if (leader) leader.SetOrigin(s_Fob2.GetOrigin() + Vector(15, 0, 0));
  alpha.SetFobBuildProgressForTest(90.0);
  alpha.TickEstablishFob(now + 2000);
  manager.m_Structures.ACR_TestForceTick();
  int fobsThere = 0;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && base.m_bBuilt && !base.m_bRetired && vector.DistanceXZ(base.GetOrigin(), s_Fob2.GetOrigin()) < 60) fobsThere++;
  ACR_TestRegress.Check("fobdesign: rebuilt HQ brings the same FOB back online with its upgrades", !s_Fob2.m_bHqLost && manager.m_Structures.HqEntityOf(s_Fob2) && fobsThere == 1 && fuel.m_eState == ACR_EFobSlotState.BUILT && s_Fob2.m_FobSupply, string.Format("lost=%1 fobs=%2", s_Fob2.m_bHqLost, fobsThere));
  if (leader) leader.SetOrigin(leaderPos);
  truck.SetOrigin(truckPos);
  alpha.m_iAITask = ACR_EAITask.FREE;

  vector aiSite;
  bool aiFound = builderAI.FindFobSite(builderAI.GetHomeBase(), aiSite);
  string aiWhy;
  bool aiSafe = true;
  if (aiFound)
  {
   foreach (ACR_Base enemyBase : ACR_Base.s_All)
    if (enemyBase && !enemyBase.m_bRetired && !enemyBase.m_sOwner.IsEmpty() && enemyBase.m_sOwner != "US" && vector.DistanceXZ(enemyBase.GetOrigin(), aiSite) < 600) aiSafe = false;
  }
  ACR_TestRegress.Check("fobdesign: AI site search returns only valid, safe sites", !aiFound || (ACR_FobBuild.CheckSpot(aiSite, aiWhy) && aiSafe), string.Format("found=%1 %2", aiFound, aiWhy));

  IEntity hq = manager.m_Structures.HqEntityOf(s_Fob2);
  if (hq) SCR_EntityHelper.DeleteEntityAndChildren(hq);
  ACR_FobSlots.Cleanup(s_Fob2);
  if (s_Fuel2) SCR_EntityHelper.DeleteEntityAndChildren(s_Fuel2);
  if (s_Depot2) SCR_EntityHelper.DeleteEntityAndChildren(s_Depot2);
  manager.m_Structures.ACR_TestForceTick();
  ACR_TestRegress.Check("fobdesign: an HQ-less FOB with nothing left retires", s_Fob2.m_bRetired, "");
 }

 static void Purchases(ACR_Manager manager, IEntity truck)
 {
  ACR_AICommander buyer = new ACR_AICommander(manager, "US");
  ACR_Base home = buyer.GetHomeBase();
  manager.m_Logistics.Use(truck, manager.m_Logistics.StockOf(truck));
  manager.m_Logistics.AddStock(truck, 40);
  int rp = manager.m_Bases.GetRP("US");
  bool bought = buyer.BuyConstructionTruck(home);
  ACR_TestRegress.Check("fobdesign: no second construction truck while a loaded one exists", !bought && manager.m_Bases.GetRP("US") == rp, "");

  ACR_FactionData data = manager.m_Bases.GetFaction("US");
  bool offered = false;
  foreach (ACR_RequisitionEntry offer : data.m_aRequisitions)
   if (!offer.m_sVehicle.IsEmpty() && (ACR_Logistics.ServiceOfName(offer.m_sVehicle) & ACR_EService.CONSTRUCTION) != 0 && offer.m_iRemaining > 0) offered = true;
  if (!offered)
  {
   ACR_TestRegress.Skip("fobdesign: at most one construction truck per five minutes", "no construction truck in the US list");
   return;
  }
  manager.m_Logistics.Use(truck, manager.m_Logistics.StockOf(truck));
  if (buyer.HasLoadedConstructionTruck())
  {
   ACR_TestRegress.Skip("fobdesign: at most one construction truck per five minutes", "another loaded US construction truck is in play");
   return;
  }
  bool first = buyer.BuyConstructionTruck(home);
  int afterFirst = manager.m_Bases.GetRP("US");
  bool second = buyer.BuyConstructionTruck(home);
  // (RP is charged once the purchase completes, so only the refusal is checked here.)
  ACR_TestRegress.Check("fobdesign: at most one construction truck per five minutes", first && !second && manager.m_Bases.GetRP("US") == afterFirst, string.Format("first=%1 second=%2", first, second));
 }
}
