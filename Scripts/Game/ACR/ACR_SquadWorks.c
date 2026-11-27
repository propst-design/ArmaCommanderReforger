// Squad, works: roadblocks, minefields, fortify and base works, and Tactics (the per-tick order upkeep;
// it calls every part, so it lives in the last layer).
//
// Last layer of ACR_Squad (see ACR_SquadVehicles.c for how the layers load).
modded class ACR_Squad
{
 // ================================================================ roadblocks (aca-f23)
 // type: ACR_ERoadblockType. A construction truck with the squad pays in supplies when it has
 // them (truckMayPay: the AI commander pays RP, as before), otherwise RP.
 bool EstablishRoadblock(vector site, float yaw, int type = ACR_ERoadblockType.MEDIUM, bool truckMayPay = true)
 {
  string refusal;
  if (!ACR_Roadblocks.CanOrder(this, refusal, type, truckMayPay)) return false;
  string factionKey = "";
  if (GetFaction()) factionKey = GetFaction().GetFactionKey();
  type = ACR_Roadblocks.TypeFor(type, factionKey);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  // Who pays is settled first, counting what NewOrder gives back from the current job, so that
  // job is only replaced once this order is sure to be paid for.
  IEntity truck;
  float loads;
  if (!ACR_Roadblocks.Payer(this, type, truckMayPay, truck, loads)) return false;
  vector from = site;
  if (GetAnchor()) from = GetAnchor().GetOrigin();

  NewOrder();
  ClearPlans();
  m_bRegrouping = false;

  string paid;
  if (truck && manager.m_Logistics.Use(truck, loads))
  {
   m_RoadblockTruck = truck;
   m_fRoadblockLoads = loads;
   m_fRoadblockPaid = manager.m_Logistics.LastPaidUsed();
   paid = string.Format("%1 supplies from the construction truck at %2", ACR_Roadblocks.Supplies(type), truck.GetOrigin());
  }
  else if (manager.m_Bases.Spend(factionKey, ACR_Roadblocks.Cost(type)))
  {
   m_iRoadblockPaidRP = ACR_Roadblocks.Cost(type);
   paid = string.Format("%1 RP", m_iRoadblockPaidRP);
  }
  else
  {
   // Payer counted only what NewOrder gave back, so this should not happen.
   SetOrder("Holding position", ACR_EOrderState.HOLD);
   PrintFormat("[ACR] SQUAD %1: could not pay for the %2", m_Name, ACR_Roadblocks.Name(type));
   return false;
  }

  m_iRoadblockType = type;
  m_vRoadblockSite = site;
  m_fRoadblockYaw = yaw;
  m_fRoadblockProgress = 0.0;
  m_fRoadblockLastTick = GetGame().GetWorld().GetWorldTime();
  m_fRoadblockLastProgress = m_fRoadblockLastTick;
  m_bRoadblockArrived = false;
  m_fRoadblockDeadline = m_fRoadblockLastTick + vector.DistanceXZ(from, site) / ACR_Roadblocks.TRAVEL_SPEED * 1000 + FOB_GIVE_UP_MS;
  m_vRoadblockStage = ACR_Roadblocks.Stage(site, yaw, type, from);
  m_RoadblockTurret = null;
  m_ActiveRoadblock = null;

  SetOrder(RoadblockLabel(), ACR_EOrderState.BUILD_ROADBLOCK);
  m_OrderKind = ACR_EOrderKind.MOVE;
  m_MoveTarget = m_vRoadblockStage;
  IssueWaypoint(WP_MOVE, m_vRoadblockStage, 4.0);
  PrintFormat("[ACR] SQUAD %1: building %2 at %3 (yaw %4), paid %5", m_Name, ACR_Roadblocks.Name(type), site, yaw, paid);
  return true;
 }

 // "Building Roadblock L (40% - contact)"
 protected string RoadblockLabel(string status = "")
 {
  int pct = Math.ClampInt((int)(m_fRoadblockProgress / ACR_Roadblocks.Duration(m_iRoadblockType) * 100), 0, 100);
  string label = string.Format("Building %1 (%2%%", ACR_Roadblocks.Name(m_iRoadblockType), pct);
  if (status != "") label += " - " + status;
  return label + ")";
 }

 // What NewOrder gives back from the squad's current job, so a new order can count it: RP from
 // a roadblock, minefield or Fortify order ...
 int PendingRP()
 {
  int rp = m_iRoadblockPaidRP;
  if (m_MinePlan) rp += ACR_Minefields.Refund(m_MinePlan);
  if (m_FortifyPlan) rp += m_FortifyPlan.m_iCost;
  return rp;
 }

 // ... and loads back to this truck from a roadblock or FOB build.
 float PendingLoads(IEntity truck)
 {
  if (!truck) return 0;
  float loads;
  if (m_RoadblockTruck == truck) loads += m_fRoadblockLoads;
  if (m_ReservedTruck == truck) loads += m_fReservedLoads;
  return loads;
 }

 void TickEstablishRoadblock(float now)
 {
  if (m_fRoadblockLastTick <= 0) m_fRoadblockLastTick = now;
  float dt = (now - m_fRoadblockLastTick) / 1000.0;
  m_fRoadblockLastTick = now;
  if (dt <= 0) return;
  if (dt > 5.0)
  {
   // The order was set aside (a regroup, a player taking the squad): that time counts for
   // neither the walk's limit nor the give-up timer.
   m_fRoadblockDeadline += (dt - 1.0) * 1000;
   m_fRoadblockLastProgress += (dt - 1.0) * 1000;
   dt = 1.0;
  }

  if (CountAlive() <= 0)
  {
   CancelRoadblock("abandoned: the squad is gone");
   return;
  }

  bool squadPresent = false;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   if (vector.DistanceXZ(body.GetOrigin(), m_vRoadblockSite) <= ACR_Roadblocks.CP_RADIUS || vector.DistanceXZ(body.GetOrigin(), m_vRoadblockStage) <= 6)
   {
    squadPresent = true;
    break;
   }
  }

  // The give-up timer runs from the squad's arrival; the walk there has its own limit.
  if (squadPresent && !m_bRoadblockArrived)
  {
   m_bRoadblockArrived = true;
   m_fRoadblockLastProgress = now;
  }
  if (!m_bRoadblockArrived && m_fRoadblockDeadline > 0 && now > m_fRoadblockDeadline)
  {
   CancelRoadblock("abandoned: the squad could not reach the road");
   return;
  }
  if (m_bRoadblockArrived && now - m_fRoadblockLastProgress >= FOB_GIVE_UP_MS)
  {
   PrintFormat("[ACR] SQUAD %1: roadblock at %2 abandoned after 3 minutes without progress", m_Name, m_vRoadblockSite);
   CancelRoadblock("abandoned");
   return;
  }

  bool contested = KnownEnemies(60, true) > 0;
  if (contested)
  {
   EquipShovels(false);
   SetOrder(RoadblockLabel("contact"), ACR_EOrderState.BUILD_ROADBLOCK);
   return;
  }

  if (!squadPresent)
  {
   if (m_Group && !m_Group.GetCurrentWaypoint() && !PlayerSteers())
   {
    IssueWaypoint(WP_MOVE, m_vRoadblockStage, 4.0);
   }
   SetOrder(RoadblockLabel("moving to road"), ACR_EOrderState.BUILD_ROADBLOCK);
   return;
  }

  float duration = ACR_Roadblocks.Duration(m_iRoadblockType);
  if (m_fRoadblockProgress < duration)
  {
   EquipShovels(true);
   m_fRoadblockLastProgress = now;
   m_fRoadblockProgress = Math.Min(m_fRoadblockProgress + dt, duration);
  }
  if (m_fRoadblockProgress < duration)
  {
   SetOrder(RoadblockLabel(), ACR_EOrderState.BUILD_ROADBLOCK);
   return;
  }
  // Done: it goes in once no vehicle stands on the spot (3 minutes, then it gives up).
  if (ACR_FobSlots.VehicleOnSpot(m_vRoadblockSite, ACR_Roadblocks.Footprint(m_iRoadblockType)))
  {
   EquipShovels(false);
   SetOrder(RoadblockLabel("move vehicles off the road"), ACR_EOrderState.BUILD_ROADBLOCK);
   return;
  }
  CompleteRoadblock();
 }

 void CompleteRoadblock()
 {
  EquipShovels(false);
  string factionKey = "";
  if (GetFaction()) factionKey = GetFaction().GetFactionKey();
  // Stop the walk to the site first: the composition and its navmesh rebuild land on the
  // builders, and a move still running there fails natively (Everon soak 23 Oct: VM exception
  // "Failed move ... to waypoint" 1.7 s after a roadblock completed).
  ClearOrders();
  ACR_Roadblock rb = ACR_Roadblocks.SpawnRoadblock(factionKey, m_vRoadblockSite, m_fRoadblockYaw, m_Name, m_iRoadblockType);
  if (!rb)
  {
   CancelRoadblock("failed: the composition did not spawn");
   return;
  }
  ACR_Wings.ForRoadblock(rb);
  // Built: nothing left to refund (a dismantle gives half back to whoever paid).
  rb.m_PaidTruck = m_RoadblockTruck;
  rb.m_fPaidLoads = m_fRoadblockLoads;
  rb.m_fPaidShare = m_fRoadblockPaid;
  rb.m_iPaidRP = m_iRoadblockPaidRP;
  m_iRoadblockPaidRP = 0;
  m_RoadblockTruck = null;
  m_fRoadblockLoads = 0;

  m_ActiveRoadblock = rb;
  m_RoadblockTurret = rb.m_Turret;
  // Native Defend owns turret allocation; do not seat a soldier before assigning it.

  SetOrder("Holding roadblock", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  IssueWaypoint(WP_DEFEND, rb.m_vPost, rb.m_fPostRadius);
  PrintFormat("[ACR] SQUAD %1: completed %2 at %3", m_Name, ACR_Roadblocks.Name(rb.m_iType), m_vRoadblockSite);
 }

 void ManRoadblockTurret()
 {
  if (!m_RoadblockTurret || !m_Group || PlayerSteers()) return;
  // Defend reserves compartments and sends native get-in behaviors. Replacing it
  // each tick sends native get-out messages and starts the departure loop again.
  // Beside the lane, off the road (ACR_RoadblockLane.Post), the gun inside the Defend's radius.
  vector post = m_vRoadblockSite;
  float radius = 15.0;
  if (m_ActiveRoadblock && m_ActiveRoadblock.m_vPost != vector.Zero)
  {
   post = m_ActiveRoadblock.m_vPost;
   radius = m_ActiveRoadblock.m_fPostRadius;
  }
  if (!m_Group.GetCurrentWaypoint()) IssueWaypoint(WP_DEFEND, post, radius);
 }

 void TickRoadblockDefence(float now)
 {
  if (!m_ActiveRoadblock) return;
  // Dismantled (a shovel, Game Master) or pruned: the ref is strong, so it outlived the list and
  // the squad held an empty road all match, counted as a holder and sending tripwire alerts.
  IEntity built = m_ActiveRoadblock.m_Entity;
  if (!built || built.IsDeleted() || !ACR_Roadblocks.s_aRoadblocks.Contains(m_ActiveRoadblock))
  {
   m_ActiveRoadblock = null;
   m_RoadblockTurret = null;
   if (m_Order == "Holding roadblock") SetOrder("Holding position", ACR_EOrderState.DEFEND);
   PrintFormat("[ACR] SQUAD %1: its roadblock is gone", m_Name);
   return;
  }
  if (PlayerSteers()) return;
  if (m_RoadblockTurret && !ACR_Roadblocks.IsTurretOccupied(m_RoadblockTurret))
  {
   ManRoadblockTurret();
  }

  // Tripwire intel detection: alert if enemies seen near road
  if (!m_ActiveRoadblock.m_bAlerted && KnownEnemies(250, true) > 0)
  {
   m_ActiveRoadblock.m_bAlerted = true;
   ACR_Manager mgr = ACR_Manager.GetInstance();
   if (mgr)
   {
    ACR_Intel intel = mgr.IntelFor(m_ActiveRoadblock.m_sFaction);
    if (intel)
    {
     intel.RadioRoadblockAlert(m_Name, m_ActiveRoadblock.m_vPos);
    }
   }
  }
 }

 // ================================================================ minefields (0.5.7)
 // ACR_Minefields plans the order; the squad pays, walks to the near edge of the site and
 // works there: mines go in (or are disarmed) as the work progresses. NewOrder calls off and
 // refunds whatever job it had.
 bool LayMinefield(ACR_MinePlan plan)
 {
  if (!plan) return false;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases.Spend(plan.m_sFaction, plan.m_iCost)) return false;

  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  m_MinePlan = plan;
  SetOrder(MineLabel("moving to the site"), MineState());
  m_OrderKind = ACR_EOrderKind.MOVE;
  m_MoveTarget = plan.m_vStage;
  IssueWaypoint(WP_MOVE, plan.m_vStage, 4.0);
  PrintFormat("[ACR] SQUAD %1: %2 at %3, paid %4 RP", m_Name, ACR_Minefields.Name(plan.m_eKind), plan.m_vSite, plan.m_iCost);
  return true;
 }

 // "Laying AT minefield (40% - contact)", "Disarming mines (3 disarmed)"
 protected string MineLabel(string status = "")
 {
  string label;
  if (m_MinePlan.m_eKind == ACR_EMineKind.DISARM) label = string.Format("Disarming mines (%1 disarmed", m_MinePlan.m_iDisarmed);
  else label = string.Format("Laying %1 (%2%%", ACR_Minefields.Name(m_MinePlan.m_eKind), (int)(m_MinePlan.m_fProgress / m_MinePlan.m_fDuration * 100));
  if (status != "") label += " - " + status;
  return label + ")";
 }

 void TickMineWork(float now)
 {
  ACR_MinePlan plan = m_MinePlan;
  // A regroup or a player taking the squad sets the order aside and restores it: the work
  // pauses meanwhile and stays paid for (NewOrder calls it off and refunds).
  if (OrderState() != MineState()) return;
  float dt = (now - plan.m_fLastTick) / 1000.0;
  plan.m_fLastTick = now;
  if (dt <= 0) return;
  if (dt > 5.0)
  {
   // The order was set aside (a regroup, a player taking the squad): that time counts for
   // neither the walk's limit nor the give-up timer.
   plan.m_fDeadline += (dt - 1.0) * 1000;
   plan.m_fLastProgress += (dt - 1.0) * 1000;
   dt = 1.0;
  }
  if (CountAlive() <= 0)
  {
   CancelMineWork("abandoned: the squad is gone");
   return;
  }

  bool present = false;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || vector.DistanceXZ(body.GetOrigin(), plan.m_vSite) > plan.m_fRadius) continue;
   present = true;
   break;
  }
  if (present && !plan.m_bArrived)
  {
   plan.m_bArrived = true;
   plan.m_fLastProgress = now;
  }
  if (!plan.m_bArrived && now > plan.m_fDeadline)
  {
   CancelMineWork("abandoned: the squad could not reach the site");
   return;
  }
  if (plan.m_bArrived && now - plan.m_fLastProgress >= FOB_GIVE_UP_MS)
  {
   PrintFormat("[ACR] SQUAD %1: minefield work at %2 abandoned after 3 minutes without progress", m_Name, plan.m_vSite);
   CancelMineWork("abandoned");
   return;
  }

  if (ACR_FobBuild.IsContested(plan.m_vSite, plan.m_sFaction) || KnownEnemies(60, true) > 0)
  {
   EquipShovels(false);
   SetOrder(MineLabel("contact"), MineState());
   return;
  }
  if (!present)
  {
   if (m_Group && !m_Group.GetCurrentWaypoint() && !PlayerSteers()) IssueWaypoint(WP_MOVE, plan.m_vStage, 4.0);
   SetOrder(MineLabel("moving to the site"), MineState());
   return;
  }

  EquipShovels(true);
  float work = dt * ACR_Minefields.Speed(this);
  plan.m_fLastProgress = now;
  plan.m_fProgress = Math.Min(plan.m_fProgress + work, plan.m_fDuration);
  bool done;
  if (plan.m_eKind == ACR_EMineKind.DISARM) done = ACR_Minefields.Disarm(plan, work);
  else
  {
   ACR_Minefields.Lay(plan, plan.m_fProgress / plan.m_fDuration, GetAnchor());
   done = plan.m_fProgress >= plan.m_fDuration;
  }
  if (done) CompleteMineWork();
  else SetOrder(MineLabel(), MineState());
 }

 protected void CompleteMineWork()
 {
  ACR_MinePlan plan = m_MinePlan;
  m_MinePlan = null;
  EquipShovels(false);
  string done;
  if (plan.m_eKind == ACR_EMineKind.DISARM) done = string.Format("%1 finished disarming mines: %2 disarmed", m_Name, plan.m_iDisarmed);
  else
  {
   int laid;
   if (plan.m_Field) laid = plan.m_Field.m_aMines.Count();
   done = string.Format("%1 finished the %2: %3 mines", m_Name, ACR_Minefields.Name(plan.m_eKind), laid);
  }
  ACR_Minefields.Finish(plan);
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(plan.m_sFaction);
  if (intel) done += " " + intel.Place(plan.m_vSite);
  if (manager) manager.Command().Feed(plan.m_sFaction, done, plan.m_vSite);
  ACR_Report.Event(plan.m_sFaction, done);
  PrintFormat("[ACR] SQUAD %1: %2", m_Name, done);

  // Hold just outside the field (its near edge is 8 m from the stage).
  ClearOrders();
  SetOrder("Holding position", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  IssueWaypoint(WP_DEFEND, plan.m_vStage, 8.0);
 }

 // ================================================================ fortify (0.5.7)
 // ACR_Fortify plans the tier; the squad pays, works from behind the position and holds it once
 // the pieces are in.
 bool Fortify(ACR_FortifyPlan plan)
 {
  if (!plan) return false;
  // Affordable counting what NewOrder gives back from the current job; paid after it.
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || manager.m_Bases.GetRP(plan.m_sFaction) + PendingRP() < plan.m_iCost) return false;

  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!manager.m_Bases.Spend(plan.m_sFaction, plan.m_iCost))
  {
   SetOrder("Holding position", ACR_EOrderState.HOLD);
   return false;
  }
  m_FortifyPlan = plan;
  SetOrder(FortifyLabel("moving to the site"), ACR_EOrderState.FORTIFY);
  m_OrderKind = ACR_EOrderKind.MOVE;
  m_MoveTarget = plan.m_vStage;
  IssueWaypoint(WP_MOVE, plan.m_vStage, 4.0);
  PrintFormat("[ACR] SQUAD %1: fortifying (%2) at %3 facing %4, paid %5 RP", m_Name, ACR_Fortify.TierName(plan.m_iTier), plan.m_vSite, (int)plan.m_fYaw, plan.m_iCost);
  return true;
 }

 // "Fortifying: dug in (40% - contact)"
 protected string FortifyLabel(string status = "")
 {
  string what = ACR_Fortify.TierWords(m_FortifyPlan.m_iTier);
  if (m_FortifyPlan.m_Work) what = ACR_BaseWorks.Words(m_FortifyPlan);
  string label = string.Format("Fortifying: %1 (%2%%", what, (int)(m_FortifyPlan.m_fProgress / m_FortifyPlan.m_fDuration * 100));
  if (status != "") label += " - " + status;
  return label + ")";
 }

 void TickFortify(float now)
 {
  ACR_FortifyPlan plan = m_FortifyPlan;
  // A regroup or a player taking the squad sets the order aside and restores it: the work
  // pauses meanwhile and stays paid for (NewOrder calls it off and refunds).
  if (OrderState() != ACR_EOrderState.FORTIFY) return;
  float dt = (now - plan.m_fLastTick) / 1000.0;
  plan.m_fLastTick = now;
  if (dt <= 0) return;
  if (dt > 5.0)
  {
   // The order was set aside (a regroup, a player taking the squad): that time counts for
   // neither the walk's limit nor the give-up timer.
   plan.m_fDeadline += (dt - 1.0) * 1000;
   plan.m_fLastProgress += (dt - 1.0) * 1000;
   plan.m_fWalkAt += (dt - 1.0) * 1000;
   dt = 1.0;
  }
  if (CountAlive() <= 0)
  {
   CancelFortify("abandoned: the squad is gone");
   return;
  }

  bool present = false;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || vector.DistanceXZ(body.GetOrigin(), plan.m_vSite) > plan.m_fRadius) continue;
   present = true;
   break;
  }
  if (present && !plan.m_bArrived)
  {
   plan.m_bArrived = true;
   plan.m_fLastProgress = now;
  }
  if (!plan.m_bArrived && now > plan.m_fDeadline)
  {
   if (SkipWorksSite(plan)) return;
   CancelFortify("abandoned: the squad could not reach the site");
   return;
  }
  if (plan.m_bArrived && now - plan.m_fLastProgress >= FOB_GIVE_UP_MS)
  {
   PrintFormat("[ACR] SQUAD %1: fortifying at %2 abandoned after 3 minutes without progress", m_Name, plan.m_vSite);
   if (SkipWorksSite(plan)) return;
   CancelFortify("abandoned");
   return;
  }

  if (ACR_FobBuild.IsContested(plan.m_vSite, plan.m_sFaction) || KnownEnemies(60, true) > 0)
  {
   EquipShovels(false);
   SetOrder(FortifyLabel("contact"), ACR_EOrderState.FORTIFY);
   return;
  }
  if (!present)
  {
   if (m_Group && !m_Group.GetCurrentWaypoint() && !PlayerSteers()) IssueWaypoint(WP_MOVE, plan.m_vStage, 4.0);
   else if (!PlayerSteers()) KeepWalking(plan, now);
   SetOrder(FortifyLabel("moving to the site"), ACR_EOrderState.FORTIFY);
   return;
  }

  if (plan.m_fProgress < plan.m_fDuration)
  {
   EquipShovels(true);
   plan.m_fProgress = Math.Min(plan.m_fProgress + dt * ACR_Minefields.Speed(this), plan.m_fDuration);
   plan.m_fLastProgress = now;
  }
  if (plan.m_fProgress < plan.m_fDuration)
  {
   SetOrder(FortifyLabel(), ACR_EOrderState.FORTIFY);
   return;
  }
  // Done: the pieces go in once no vehicle stands on a spot (3 minutes, then it gives up).
  if (ACR_Fortify.VehicleOnSpots(plan))
  {
   EquipShovels(false);
   SetOrder(FortifyLabel("move vehicles off the site"), ACR_EOrderState.FORTIFY);
   return;
  }
  CompleteFortify();
 }

 void KeepWalking(ACR_FortifyPlan plan, float now)
 {
  IEntity anchor = GetAnchor();
  if (!anchor) return;
  vector at = anchor.GetOrigin();
  if ((plan.m_vWalkFrom[0] == 0 && plan.m_vWalkFrom[2] == 0) || vector.DistanceXZ(at, plan.m_vWalkFrom) > WALK_MOVED_M)
  {
   plan.m_vWalkFrom = at;
   plan.m_fWalkAt = now;
   return;
  }
  if (now - plan.m_fWalkAt < WALK_STOPPED_MS) return;
  plan.m_fWalkAt = now;
  plan.m_iWalkRetries++;
  StepOntoNavmesh(0);
  string how = "same spot";
  if (plan.m_iWalkRetries > 2 && !IsMounted() && !EntityUtils.IsPlayer(anchor))
  {
   vector ahead = plan.m_vStage - at;
   ahead[1] = 0;
   float left = ahead.Length();
   float step = Math.Min(WALK_STEP_M * (plan.m_iWalkRetries - 2), WALK_STEP_MOST_M);
   vector spot;
   if (left > 1)
   {
    // Straight at the site first, then turned off either side: the one spot ahead can be a wall or a building
    // where he stuck, and he stayed put till a longer step (baseworks: stuck at ~200 m, try 3 never stepped).
    vector dir = ahead * (1 / left);
    float reach = Math.Min(step, left);
    array<float> turns = {0, 35, -35, 70, -70};
    foreach (float turn : turns)
    {
     float c = Math.Cos(turn * Math.DEG2RAD);
     float s = Math.Sin(turn * Math.DEG2RAD);
     vector way = Vector(dir[0] * c - dir[2] * s, 0, dir[0] * s + dir[2] * c);
     if (!ReachableNear(at + way * reach, WALK_STEP_M, spot) || !ACR_BaseSystem.ReachesOpen(spot, ACR_BaseSystem.Pathing(null))) continue;
     // Only a step that brings him nearer the site.
     if (vector.DistanceXZ(spot, plan.m_vStage) >= left - 1) continue;
     anchor.SetOrigin(spot);
     how = string.Format("leader stepped %1 m ahead, turned %2", (int)vector.DistanceXZ(at, spot), (int)turn);
     break;
    }
   }
  }
  IssueWaypoint(WP_MOVE, plan.m_vStage, 4.0);
  PrintFormat("[ACR] SQUAD %1: stopped at %2, %3 m from the site at %4; on again, try %5 (%6)", m_Name, at, (int)vector.DistanceXZ(at, plan.m_vSite), plan.m_vSite, plan.m_iWalkRetries, how);
 }

 protected bool SkipWorksSite(ACR_FortifyPlan plan)
 {
  if (!plan.m_Work || ACR_FobBuild.IsContested(plan.m_vSite, plan.m_sFaction) || KnownEnemies(60, true) > 0) return false;
  m_FortifyPlan = null;
  EquipShovels(false);
  ClearOrders();
  ACR_BaseWorks.SiteSkipped(this, plan);
  return true;
 }

 protected void CompleteFortify()
 {
  ACR_FortifyPlan plan = m_FortifyPlan;
  m_FortifyPlan = null;
  EquipShovels(false);
  // Stop the walk first: the pieces and their navmesh rebuild land round the builders.
  ClearOrders();
  if (plan.m_Work)
  {
   ACR_BaseWorks.SiteDone(this, plan);
   return;
  }
  string why;
  ACR_Fortification fort = ACR_Fortify.Build(plan, why);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!fort)
  {
   if (manager) manager.m_Bases.Refund(plan.m_sFaction, plan.m_iCost);
   if (m_iAITask == ACR_EAITask.FORTIFY) SetAITask(ACR_EAITask.FREE);
   SetOrder("Holding position", ACR_EOrderState.HOLD);
   PrintFormat("[ACR] SQUAD %1: could not fortify (%2), refunded %3 RP", m_Name, why, plan.m_iCost);
   // Said where the player sees it (playtest 25 Oct: only the RP was seen to go).
   string lost = string.Format("%1 could not fortify: %2 (%3 RP refunded)", m_Name, why, plan.m_iCost);
   if (manager) manager.Command().Feed(plan.m_sFaction, lost, plan.m_vSite);
   if (manager && m_iOrderedBy != 0) manager.Notify(m_iOrderedBy, lost);
   return;
  }
  m_Fortification = fort;
  string done = string.Format("%1 dug in: %2 (%3)", m_Name, ACR_Fortify.TierName(fort.m_iTier), ACR_Fortify.TierWords(fort.m_iTier));
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(plan.m_sFaction);
  if (intel) done += " " + intel.Place(fort.m_vPos);
  if (manager) manager.Command().Feed(plan.m_sFaction, done, fort.m_vPos);
  ACR_Report.Event(plan.m_sFaction, done);
  PrintFormat("[ACR] SQUAD %1: %2", m_Name, done);
  // Hold it: native Defend puts gunners on the position's guns.
  SetOrder("Holding position", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  IssueWaypoint(WP_DEFEND, ACR_Fortify.HoldCentre(fort), ACR_Fortify.HOLDER_RADIUS);
 }

 // The base works' squad with nothing left to do there: it holds the base.
 void HoldBaseWorks(ACR_BaseWork work)
 {
  if (m_iAITask == ACR_EAITask.FORTIFY) SetAITask(ACR_EAITask.FREE);
  SetOrder("Holding position", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  vector at;
  float radius;
  int line;
  if (work && !ACR_Transport.CrewedVehicle(this) && ACR_BaseWorks.ManSquad(work, m_Group, Math.Max(1, CountAlive()), at, radius, line) && IssueWaypoint(WP_GARRISON, at, radius)) return;
  if (work && ACR_BaseWorks.HoldSpot(work, at, radius)) IssueWaypoint(WP_DEFEND, at, radius);
  else if (work && work.m_Base) IssueWaypoint(WP_DEFEND, work.m_Base.GetOrigin(), 60);
 }

 // Back on a roadblock of ours (ACR_AICommander.ReturnToPosts): held as when its builder
 // completed it, its gun by native Defend.
 bool ManRoadblock(ACR_Roadblock rb)
 {
  if (!rb) return false;
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  m_vRoadblockSite = rb.m_vPos;
  if (!IssueWaypoint(WP_DEFEND, m_vRoadblockSite, 15.0)) return false;
  m_ActiveRoadblock = rb;
  m_RoadblockTurret = rb.m_Turret;
  SetOrder("Holding roadblock", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  return true;
 }

 // Put fire on an area from where the squad stands (support by fire).
 bool Suppress(vector position)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_SUPPRESS, position)) return false;
  SetOrder("Suppressing", ACR_EOrderState.SUPPRESS);
  m_OrderKind = ACR_EOrderKind.MOVE;
  return true;
 }

 // Is anyone of this squad aboard its own vehicle?
 bool InOwnVehicle()
 {
  if (!m_OwnVehicle) return false;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Respawn.IsAlive(body) && ACR_Transport.VehicleOf(body) == m_OwnVehicle) return true;
  }
  return false;
 }

 // The vehicle that defines this squad's map symbol: its own (if still
 // intact), else whatever it is crewing.
 IEntity MarkVehicle()
 {
  if (m_OwnVehicle && ACR_Logistics.Health(m_OwnVehicle) > 0) return m_OwnVehicle;
  return ACR_Transport.CrewedVehicle(this);
 }

 // Small-unit sense, every second for squads no player is leading:
 //  - a Fast (forced) move stops ignoring the enemy once the squad makes contact;
 //  - a squad that reaches its destination takes up positions instead of standing around;
 //  - soldiers get the mission's AI skill.
 // Escort CO (ORDER_ESCORT): follows the issuing player's primary command unit, boarding
 // their vehicle when open seats are within reach, and holding ground if the CO falls.
 protected static int FreeEscortSeats(IEntity vehicle)
 {
  if (!vehicle) return 0;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  int free;
  array<BaseCompartmentSlot> cargo = {};
  compartments.GetCompartmentsOfType(cargo, ECompartmentType.CARGO);
  foreach (BaseCompartmentSlot seat : cargo)
   if (seat && !seat.GetOccupant()) free++;
  array<BaseCompartmentSlot> turrets = {};
  compartments.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
  foreach (BaseCompartmentSlot slot : turrets)
   if (slot && !slot.GetOccupant()) free++;
  return free;
 }

 protected int CountAboard(IEntity vehicle)
 {
  if (!vehicle) return 0;
  int count;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   IEntity man = agent.GetControlledEntity();
   if (man && ACR_Respawn.IsAlive(man) && ACR_Transport.VehicleOf(man) == vehicle) count++;
  }
  return count;
 }

 bool StartEscort(int playerId, out string refusal)
 {
  refusal = "";
  if (playerId <= 0) { refusal = "no commander"; return false; }
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
  if (!pc) { refusal = "commander unavailable"; return false; }
  IEntity main = pc.GetMainEntity();
  if (!main || !ACR_Respawn.IsAlive(main)) { refusal = "your command unit is dead"; return false; }
  if (m_iEscortPlayerId == playerId)
  {
   m_iEscortPlayerId = 0;
   m_vEscortAnchor = vector.Zero;
   m_EscortVehicle = null;
   if (GetAnchor()) Defend(GetAnchor().GetOrigin());
   return true;
  }
  vector coPos = main.GetOrigin();
  IEntity coVeh = ACR_Transport.VehicleOf(main);
  IEntity escortVeh;
  if (coVeh && !IsMounted() && GetAnchor() && vector.DistanceXZ(GetAnchor().GetOrigin(), coPos) <= 60 && FreeEscortSeats(coVeh) > 0)
  {
   BoardVehicle(Vehicle.Cast(coVeh), coVeh.GetOrigin());
   escortVeh = coVeh;
  }
  else if (GetAnchor() && vector.DistanceXZ(GetAnchor().GetOrigin(), coPos) > 45)
  {
   MoveTo(coPos);
  }
  else
  {
   Defend(coPos);
  }
  m_iEscortPlayerId = playerId;
  m_vEscortAnchor = coPos;
  m_EscortVehicle = escortVeh;
  SetOrder("Escorting CO", m_eOrderState);
  return true;
 }

 void TickEscort()
 {
  if (m_iEscortPlayerId <= 0) return;
  if (PlayerSteers() || m_bAICommanded)
  {
   m_iEscortPlayerId = 0;
   m_vEscortAnchor = vector.Zero;
   m_EscortVehicle = null;
   return;
  }
  IEntity anchor = GetAnchor();
  if (!anchor) return;
  PlayerManager pm = GetGame().GetPlayerManager();
  SCR_PlayerController pc;
  if (pm) pc = SCR_PlayerController.Cast(pm.GetPlayerController(m_iEscortPlayerId));
  IEntity main;
  if (pc) main = pc.GetMainEntity();
  if (!main || !ACR_Respawn.IsAlive(main))
  {
   int lostId = m_iEscortPlayerId;
   m_iEscortPlayerId = 0;
   m_vEscortAnchor = vector.Zero;
   m_EscortVehicle = null;
   Defend(anchor.GetOrigin());
   ACR_Manager manager = ACR_Manager.GetInstance();
   if (manager) manager.Notify(lostId, m_Name + ": holding position after CO lost");
   return;
  }
  vector coPos = main.GetOrigin();
  IEntity coVeh = ACR_Transport.VehicleOf(main);
  int keepId = m_iEscortPlayerId;
  IEntity keepVeh = m_EscortVehicle;

  // If the CO left the vehicle we rode in with them, dismount and resume foot escort.
  if (m_EscortVehicle && coVeh != m_EscortVehicle)
  {
   if (CountAboard(m_EscortVehicle) > 0)
   {
    if (OrderState() == ACR_EOrderState.GET_OUT)
    {
     m_vEscortAnchor = coPos;
     return;
    }
    if (GetOut())
    {
     m_iEscortPlayerId = keepId;
     m_EscortVehicle = keepVeh;
     m_vEscortAnchor = coPos;
     SetOrder("Escorting CO", m_eOrderState);
     return;
    }
   }
   else
   {
    m_EscortVehicle = null;
    keepVeh = null;
   }
  }

  if (coVeh)
  {
   int aboard = CountAboard(coVeh);
   int alive = CountAlive();
   int freeSeats = FreeEscortSeats(coVeh);
   if (aboard > 0 && (aboard >= alive || freeSeats <= 0))
   {
    m_EscortVehicle = coVeh;
    m_vEscortAnchor = coPos;
    if (m_Order != "Escorting CO") SetOrder("Escorting CO", ACR_EOrderState.NONE);
    return;
   }
   bool alreadyBoarding = OrderState() == ACR_EOrderState.GET_IN && m_BoardTarget == coVeh;
   if (alreadyBoarding)
   {
    if (aboard > 0 || vector.DistanceXZ(anchor.GetOrigin(), coPos) <= 80)
    {
     m_vEscortAnchor = coPos;
     return;
    }
   }
   else if ((!IsMounted() || aboard > 0) && vector.DistanceXZ(anchor.GetOrigin(), coPos) <= 60 && freeSeats > 0)
   {
    BoardVehicle(Vehicle.Cast(coVeh), coVeh.GetOrigin());
    m_iEscortPlayerId = keepId;
    m_EscortVehicle = coVeh;
    m_vEscortAnchor = coPos;
    SetOrder("Escorting CO", m_eOrderState);
    return;
   }
  }

  bool staleBoarding = OrderState() == ACR_EOrderState.GET_IN && (!coVeh || m_BoardTarget != coVeh || vector.DistanceXZ(anchor.GetOrigin(), coPos) > 80);
  if (staleBoarding || vector.DistanceXZ(coPos, m_vEscortAnchor) > 35)
  {
   if (vector.DistanceXZ(anchor.GetOrigin(), coPos) > 50) MoveTo(coPos);
   else Defend(coPos);
   m_iEscortPlayerId = keepId;
   m_EscortVehicle = keepVeh;
   m_vEscortAnchor = coPos;
   SetOrder("Escorting CO", m_eOrderState);
  }
 }

 void Tactics(int skill, float now)
 {
  WatchCrew();
  TickEscort();
  // A builder fights normally (Jacob, 18 Oct); its progress pauses while in contact.
  if (OrderState() == ACR_EOrderState.BUILD_FOB) TickEstablishFob(now);
  // A regroup or a player taking the squad sets the order aside and restores it: the build
  // pauses meanwhile and stays paid for (NewOrder refunds it).
  if (OrderState() == ACR_EOrderState.BUILD_ROADBLOCK) TickEstablishRoadblock(now);
  // ... but a squad wiped out meanwhile still gets the payment back.
  else if (RoadblockPending() && CountAlive() <= 0) CancelRoadblock("abandoned: the squad is gone");
  if (m_ActiveRoadblock) TickRoadblockDefence(now);
  if (m_MinePlan) TickMineWork(now);
  if (m_Ambush) ACR_Ambush.Tick(this, now);
  if (m_FortifyPlan) TickFortify(now);
  if (!m_Group || m_Commander != 0) return;
  array<AIAgent> agents = {};
  GetAgents(agents);
  // Skill and reaction (faster target identification: less standing and staring at a player
  // while deciding whether he's an enemy): the mode's, or his side's or unit's (ACR_AISkill).
  ACR_Manager manager = ACR_Manager.GetInstance();
  int vetSteps = ACR_Veterancy.SkillSteps(this);
  float vetReaction = ACR_Veterancy.ReactionScale(this);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !manager) continue;
   SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(body.FindComponent(SCR_AICombatComponent));
   float reaction = manager.m_fAIPerception;
   if (!ACR_AISkill.IsPinned(body)) reaction *= vetReaction * ACR_CommandPresence.ReactionScale(this);
   ACR_AISkill.Apply(combat, m_Group, skill, reaction, "", vetSteps);
  }
  // A player leads (or drives) this squad: they give the orders, e.g. with the command wheel.
  // Re-issuing our last map order would wipe theirs. A player rifleman doesn't: the squad's order
  // upkeep (route legs, recovery, the stuck watch) runs as for any AI squad (review, 12 Nov).
  if (PlayerSteers())
  {
   m_CombatFlank.YieldToPlayer(this);
   return;
  }
  WatchBoardingTarget();
  EvadeTick(now);
  HuntTick(now);
  ACR_GunHold.Tick(this);
  if (m_Qrf) ACR_Qrf.Tick(this, now);
  if (m_bGarrisonPending && m_Order == "Garrisoning") PlaceGarrison(m_GarrisonPending);
  else if (m_sOrderPrefab == WP_GARRISON && m_Order == "Garrisoning" && m_Group.GetCurrentWaypoint() == null && m_iGarrisonReissues < 2)
  {
   // The game completed the garrison waypoint (a failed group move does that): again.
   m_iGarrisonReissues++;
   PrintFormat("[ACR] GARRISON %1: the game dropped the order, issuing it again", m_Name);
   if (!ACR_Garrison.HasPosts(m_Group)) PlaceGarrison(m_MoveTarget);
   else IssueWaypoint(WP_GARRISON, m_MoveTarget, m_fOrderRadius);
  }
  bool contact = KnownEnemies(350) > 0;
  if (contact) m_fLastContactTime = now;
  if (m_CombatFlank.Tick(this, now)) return;
  FollowRoute();
  Cohesion(now, contact);
  RecoverMovementWaypoint(now);
  ReportUnreachable();
  bool runningBack = WatchFallBack(now, contact);
  bool movingOrder = m_sOrderPrefab == WP_MOVE || m_sOrderPrefab == WP_DEFEND || m_sOrderPrefab == WP_SEARCH_DESTROY || m_sOrderPrefab == WP_GARRISON;
  // (not after a Halt: m_Waypoint is gone then, and the old order must not come back)
  if (m_bForced && movingOrder && contact && m_Waypoint && !IsMounted() && !m_bRegrouping && !runningBack)
  {
   ACR_EMoveMode mode = m_Mode;
   m_Mode = ACR_EMoveMode.COMBAT;
   IssueWaypoint(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius);
   m_Mode = mode;
   PrintFormat("[ACR] TACTICS %1: contact, fighting its way forward", m_Name);
  }
  else if (m_Mode == ACR_EMoveMode.FAST && movingOrder && !contact && m_Waypoint && !IsMounted() && !m_bRegrouping && m_fLastContactTime > 0 && now - m_fLastContactTime >= 15000)
  {
   m_fLastContactTime = -1;
   IssueWaypoint(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius, true);
   PrintFormat("[ACR] TACTICS %1: contact clear, resuming fast march", m_Name);
  }
  if (OrderState() == ACR_EOrderState.MOVE && m_OrderKind == ACR_EOrderKind.MOVE && !IsMounted())
  {
   array<AIWaypoint> waypoints = {};
   m_Group.GetWaypoints(waypoints);
   IEntity anchor = GetAnchor();
   if (waypoints.IsEmpty() && anchor && vector.DistanceXZ(anchor.GetOrigin(), m_MoveTarget) < 60)
   {
    IssueWaypoint(WP_DEFEND, m_MoveTarget, 20);
    SetOrder("Holding position", ACR_EOrderState.DEFEND);
    m_OrderKind = ACR_EOrderKind.DEFEND;
    PrintFormat("[ACR] TACTICS %1: arrived, taking positions", m_Name);
   }
  }
  HoldRetryWatch(now);
  DriveWatch(now, contact);
  CheckTacticalDismount(now, contact);
 }
}
