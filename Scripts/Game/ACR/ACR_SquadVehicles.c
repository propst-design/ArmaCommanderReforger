// Squad, vehicles: stalled and stuck vehicles, transport (boarding, dismounts, sweeps), getting away
// from armour and driving out of fire.
//
// Split out of ACR_Squad.c (12 Nov 2026). Enforce has no partial classes, so each part is a modded layer
// of the same class. Layers apply in file-name order and a layer can call only itself, earlier layers and
// the base, never a later one (base code calling a later layer fails to compile):
// ACR_Squad.c < ACR_SquadVehicles.c < ACR_SquadWorks.c. Fields and anything an earlier part calls stay in ACR_Squad.c.
modded class ACR_Squad
{
 bool Queueing() { return m_fQueueSince >= 0; }

 // Gave up stuck lately and still stands about where it did: sending it on a ride ends the same way.
 bool StuckHere(IEntity vehicle, float now)
 {
  if (now - m_fStuckAt >= STUCK_RETRY_MS) return false;
  if (!vehicle || m_vStuckPos == vector.Zero) return true;
  return vector.DistanceXZ(vehicle.GetOrigin(), m_vStuckPos) < STUCK_LEFT_M;
 }

 int UnstickTries() { return m_iUnsticks; }

 // Times DriveWatch took this squad's vehicle as stuck (unstick, re-route or hold): tests.
 int m_iStallsTaken;

 int Reroutes() { return m_iReroutes; }

 bool IsUnsticking() { return m_bUnsticking; }

 protected bool IsMember(IEntity body)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (agent.GetControlledEntity() == body) return true;
  return false;
 }

 // A living squad member in a turret of this vehicle whose gun still has ammunition.
 bool HasArmedGunner(IEntity vehicle)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || !ACR_Respawn.IsAlive(body) || body.GetCharacterController().IsUnconscious()) continue;
   if (ACR_Transport.VehicleOf(body) != vehicle || ACR_Transport.SeatOf(body) != ECompartmentType.TURRET) continue;
   CompartmentAccessComponent access = body.GetCompartmentAccessComponent();
   if (access && BBB_VehicleHelpers.TurretHasAmmo(access.GetCompartment())) return true;
  }
  return false;
 }

 static bool HasTurret(IEntity vehicle)
 {
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return false;
  array<BaseCompartmentSlot> turrets = {};
  compartments.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
  return !turrets.IsEmpty();
 }

 static bool NativeCommandAllowed(bool aircraftCrew)
 {
  return !aircraftCrew;
 }

 // Suitability as a base of fire (support-role teams are not).
 int FirePower()
 {
  if (m_Group && m_Group.m_eACRFormation != ACR_EFormation.NONE)
  {
   if (ACR_InfantryCatalog.Specialist(m_Group.m_eACRFormation)) return 0;
   return CountAlive();
  }
  string name = m_Name;
  name.ToLower();
  if (name.Contains("logistic") || name.Contains("medic") || name.Contains("engineer") || name.Contains("sapper") || name.Contains("demolition") || name.Contains("pioneer") || name.Contains("supply") || name.Contains("sniper"))
   return 0;
  int value = CountAlive();
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (vehicle && HasGunner(vehicle)) value += 6;
  return value;
 }

 protected void DriveWatch(float now, bool contact)
 {
  if (m_bUnsticking || !m_Group) return;
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (!vehicle) return;
  if (!CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent))) return;

  Physics phys = vehicle.GetPhysics();
  float speed = 0;
  if (phys) speed = phys.GetVelocity().Length();

  ChimeraCharacter driver = DriverOf(vehicle);
  ACR_Manager manager = ACR_Manager.GetInstance();

  bool hitRecently = BBB_Transport.VehicleHitRecently(vehicle, 6000);
  bool underFire = (contact && KnownEnemies(300) > 0) || hitRecently;

  if (underFire && (!driver || !ACR_Respawn.IsAlive(driver)))
  {
   if (speed < 2.0)
   {
    // A gunner with ammo keeps a stopped gun in the fight (20 Oct Everon: a man on a
    // parked HMMWV M2 was pulled out every second, BTR and M1025 crews abandoned
    // working guns when the driver fell). Only other squads riding along get out.
    if (HasArmedGunner(vehicle))
    {
     if (m_DriverlessGun != vehicle)
     {
      m_DriverlessGun = vehicle;
      PrintFormat("[ACR] DRIVE %1: no driver under fire, gunner keeps fighting", m_Name);
     }
     if (manager)
     {
      array<ACR_Squad> cargo = {};
      ACR_Transport.PassengerSquads(manager, vehicle, cargo);
      foreach (ACR_Squad rider : cargo)
       if (rider != this && !rider.HasPlayer() && rider.OrderState() != ACR_EOrderState.DISMOUNT) rider.DismountNow();
     }
     return;
    }
    Bail(vehicle, "driver lost under fire", true);
    return;
   }
  }
  m_DriverlessGun = null;

  bool eligible = driver != null && ACR_Respawn.IsAlive(driver) && IsMember(driver) && FindController(driver) == 0;
  if (eligible && manager && manager.IsGunnerDriven(vehicle)) eligible = false;
  LaneSpeed(vehicle, eligible);
  bool wantsToMove;
  if (eligible)
  {
   array<AIWaypoint> waypoints = {};
   m_Group.GetWaypoints(waypoints);
   bool movingOrder = m_sOrderPrefab == WP_MOVE || m_sOrderPrefab == WP_DEFEND || m_sOrderPrefab == WP_SEARCH_DESTROY || m_sOrderPrefab == WP_GARRISON;
   wantsToMove = !waypoints.IsEmpty() && movingOrder && vector.DistanceXZ(vehicle.GetOrigin(), m_MoveTarget) > STALL_ARRIVED;
  }
  // Parked, unarmed and under fire: driven off before the game's AI gets the crew out beside it.
  ACR_EOrderState parkedState = OrderState();
  bool parked = parkedState == ACR_EOrderState.HOLD || parkedState == ACR_EOrderState.NONE || parkedState == ACR_EOrderState.RETURN || parkedState == ACR_EOrderState.DEFEND;
  if (eligible && !wantsToMove && parked && underFire && speed < 2 && !HasTurret(vehicle) && now >= m_fNextDriveOut && DriveOut(vehicle, now, "parked under fire"))
   return;
  if (!underFire) m_bDroveOut = false;
  ACR_Transport.SanitizeVehicleHorn(vehicle, !eligible || !wantsToMove);
  if (!wantsToMove || vehicle != m_StallVehicle || (speed > 1.0 && vector.DistanceXZ(vehicle.GetOrigin(), m_StallPos) > STALL_PROGRESS_M))
  {
   if (vehicle != m_StallVehicle || !wantsToMove) { m_iUnsticks = 0; m_iReroutes = 0; }
   else if (m_iUnsticks > 0 && vector.DistanceXZ(vehicle.GetOrigin(), m_vUnstickFrom) > UNSTICK_CLEAR) { m_iUnsticks = 0; m_iReroutes = 0; } // got past it: the next stuck spot may re-route too
   m_StallVehicle = vehicle;
   if (vehicle) m_StallPos = vehicle.GetOrigin();
   m_fStallSince = now;
   m_fBoardingSince = -1;
   m_fQueueSince = -1;
   return;
  }
  // Waiting for its own men to get in (or out) isn't being stuck; under fire the usual limits apply.
  if (!underFire && CrewBoarding(vehicle))
  {
   if (m_fBoardingSince < 0)
   {
    m_fBoardingSince = now;
    PrintFormat("[ACR] DRIVE %1: waiting for its crew to get in or out, not stuck", m_Name);
   }
   if (now - m_fBoardingSince < BOARDING_WAIT_MS)
   {
    m_fStallSince = now;
    return;
   }
  }
  else m_fBoardingSince = -1;
  // Queueing behind another of our vehicles that's stopped too isn't being stuck (convoys, gates, the
  // roadblock lane); out of contact only, and for QUEUE_WAIT_MS at most: the one ahead may never move
  // (srv:stall, 29 Oct: a truck waited 150 s behind a stuck one).
  ACR_Squad queueBehind;
  if (!underFire) queueBehind = VehicleNear(vehicle, true, QUEUE_AHEAD);
  if (queueBehind)
  {
   if (m_fQueueSince < 0)
   {
    m_fQueueSince = now;
    PrintFormat("[ACR] DRIVE %1: queueing behind one of ours, not stuck (%2, %3 m ahead)", m_Name, queueBehind.GetName(), (int)vector.DistanceXZ(vehicle.GetOrigin(), ACR_Transport.CrewedVehicle(queueBehind).GetOrigin()));
   }
   if (now - m_fQueueSince < QUEUE_WAIT_MS)
   {
    m_fStallSince = now;
    return;
   }
  }
  else m_fQueueSince = -1;
  // BBB is driving it itself (backing away from an armed vehicle brakes to a stop first, unstick,
  // pull-over): not stalled, and nobody bails out mid-manoeuvre. From 1.9 that brake really stops
  // the truck, so the 3 s unarmed-under-fire bail fired every time (pak:mannersevade 0/3).
  if (BBB_PilotHold.Driving(vehicle))
  {
   m_fStallSince = now;
   return;
  }
  float stalled = (now - m_fStallSince) / 1000;
  bool armed = HasGunner(vehicle);
  // Fighting vehicles (anything with a turret: LAV, BTR, BRDM, gun trucks) keep
  // their crew aboard; only soft, unarmed vehicles are abandoned. A taxi isn't one: a ride never
  // takes a vehicle with a manned gun, so holding it stuck only stranded its riders (#23: a stuck
  // LAV taxi held, a new order that dropped the ride); it re-routes like a soft vehicle.
  bool fightingVehicle = HasTurret(vehicle) && !(manager && manager.m_Rides && manager.m_Rides.RideOf(this) != null);

  if (underFire && speed < 3.5 && manager)
  {
   array<ACR_Squad> riders = {};
   ACR_Transport.PassengerSquads(manager, vehicle, riders);
   foreach (ACR_Squad rider : riders)
   {
    if (rider != this && !rider.HasPlayer() && rider.OrderState() != ACR_EOrderState.DISMOUNT)
    {
     PrintFormat("[ACR] DRIVE %1: ordering cargo riders %2 to dismount under fire", m_Name, rider.GetName());
     rider.DismountAndContinue(rider.m_sOrderPrefab, rider.m_MoveTarget, rider.m_fOrderRadius, rider.m_Order, rider.m_OrderKind);
    }
   }
  }

  // Getting away from an armed vehicle: BBB has just braked it and backed it off, and the driver needs a
  // few seconds to turn it round before standing still means it is stuck (pak:mannersevade, 1.9).
  float helpless = STALL_SECONDS_HELPLESS;
  if (m_Evade) helpless = STALL_SECONDS_EVADING;
  if (m_bDroveOut || m_bFireEvade) helpless = DRIVE_OUT_GRACE_S;
  if (underFire && !fightingVehicle && stalled >= helpless)
  {
   if (!m_bDroveOut && manager)
   {
    // Its riders were told to get out (above): a few seconds for them, then it drives out empty.
    array<ACR_Squad> aboard = {};
    ACR_Transport.PassengerSquads(manager, vehicle, aboard);
    aboard.RemoveItem(this);
    if (!aboard.IsEmpty() && stalled < helpless + DRIVE_OUT_RIDERS_S) return;
    // Already on its way out of fire and stopped again (hung up turning, or the road it was sent to): one fresh
    // try from where it faces now, before the crew gets out (srv:stall, 12 Nov: half the runs bailed 10-15 m out).
    if (aboard.IsEmpty() && DriveOut(vehicle, now, "stopped under fire, unarmed", m_bFireEvade))
    {
     m_bDroveOut = true;
     m_fStallSince = now;
     return;
    }
   }
   Bail(vehicle, "stopped under fire, unarmed", false);
   return;
  }
  float limit = s_fStallSeconds;
  if (underFire || armed) limit = STALL_SECONDS_FIGHTING;
  if (stalled < limit) return;
  m_iStallsTaken++;
  if (m_iUnsticks >= UNSTICK_TRIES)
  {
   if (fightingVehicle)
   {
    // Hold where it is and fight from there instead of walking away from the vehicle.
    Halt("Stuck, holding");
    m_iUnsticks = 0;
    m_iReroutes = 0;
    NoteStuck(vehicle, now);
    PrintFormat("[ACR] DRIVE %1: stuck, holding position (armed vehicle) at %2", m_Name, DriveSpot(vehicle));
    return;
   }
   if (m_iReroutes < REROUTE_TRIES && Reroute(vehicle))
   {
    m_iReroutes++;
    m_iUnsticks = 0;
    m_StallPos = vehicle.GetOrigin();
    m_fStallSince = now;
    return;
   }
   StuckHold(vehicle, now);
   return;
  }
  m_iUnsticks++;
  // Never reverse into a vehicle right behind: plan again from here instead.
  ACR_Squad behind = VehicleNear(vehicle, false, QUEUE_BEHIND);
  if (behind)
  {
   PrintFormat("[ACR] DRIVE %1: stuck, a vehicle right behind, planning again instead of backing up (try %2; %3, %4 m) at %5", m_Name, m_iUnsticks, behind.GetName(), (int)vector.DistanceXZ(vehicle.GetOrigin(), ACR_Transport.CrewedVehicle(behind).GetOrigin()), DriveSpot(vehicle));
   m_StallPos = vehicle.GetOrigin();
   m_fStallSince = now;
   if (!m_sOrderPrefab.IsEmpty())
   {
    int kind = m_OrderKind;
    IssueWaypoint(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius);
    m_OrderKind = kind;
   }
   return;
  }
  BeginUnstick(vehicle, driver);
 }

 // Every living member of the squad is in this vehicle.
 protected bool CrewAboard(IEntity vehicle)
 {
  if (!vehicle) return false;
  array<AIAgent> agents = {};
  GetAgents(agents);
  int alive, aboard;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   alive++;
   if (ACR_Transport.VehicleOf(body) == vehicle) aboard++;
  }
  return alive > 0 && aboard == alive;
 }

 // Another ACA vehicle in this one's lane within range: ahead of its nose and nearly stopped, or
 // behind its tail (any speed), where backing up takes it. Its own facing, not the line to its
 // target: that half-plane took vehicles parked beside it for ones ahead or behind, and a truck
 // facing away from its target "saw" the one in front of its nose as behind it (Everon soak, 7 Nov:
 // Humvee 1 in the Airbase motor pool was kept from backing up 64 times in 80 minutes).
 // Nose to nose, each waited for the other until the wait ran out, then both backed up, met again
 // and waited again: the newer of the two (later in the squad list) gives way and is taken as stuck
 // as usual, the older one waits.
 protected ACR_Squad VehicleNear(IEntity vehicle, bool ahead, float range)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return null;
  vector here = vehicle.GetOrigin();
  vector facing = vehicle.GetTransformAxis(2);
  int mine = manager.m_Squads.Find(this);
  foreach (int i, ACR_Squad other : manager.m_Squads)
  {
   if (!other || other == this) continue;
   IEntity theirs = ACR_Transport.CrewedVehicle(other);
   if (!theirs || theirs == vehicle) continue;
   vector there = theirs.GetOrigin();
   if (!InLane(here, facing, there, range, ahead)) continue;
   if (!ahead) return other;
   Physics phys = theirs.GetPhysics();
   if (phys && phys.GetVelocity().Length() > 1.5) continue;
   if (mine > i && InLane(there, theirs.GetTransformAxis(2), here, range, true)) continue;
   return other;
  }
  return null;
 }

 // Whether 'to' stands in the lane of a vehicle at 'from' facing 'facing': in front of it (with
 // front false, behind it), within range, and at most QUEUE_LANE off its line (in front, a little
 // more the farther out, for bends and gates at an angle).
 static bool InLane(vector from, vector facing, vector to, float range, bool front)
 {
  vector rel = to - from;
  rel[1] = 0;
  float dist = rel.Length();
  if (dist > range || dist < 0.1) return false;
  facing[1] = 0;
  if (facing.Length() < 0.1) return false;
  facing.Normalize();
  float along = vector.Dot(rel, facing);
  vector side = rel - facing * along;
  if (!front) along = -along;
  if (along <= 0) return false;
  float lane = QUEUE_LANE;
  if (front) lane = QUEUE_LANE + QUEUE_LANE_WIDEN * along;
  return side.Length() <= lane;
 }

 int MoveFailsKept() { return m_iMoveFailsKept; }

 protected void LaneSpeed(IEntity vehicle, bool aiDriven)
 {
  ACR_Roadblock near = null;
  if (vehicle && GetFaction()) near = ACR_Roadblocks.OwnNear(vehicle.GetOrigin(), GetFaction().GetFactionKey());
  // Its holders step out of the way of any of our vehicles, AI-driven or not.
  if (near) ACR_RoadblockLane.ClearFor(near, vehicle, aiDriven);
  ACR_Roadblock rb = null;
  if (aiDriven) rb = near;
  if (m_LaneSlowVehicle && (m_LaneSlowVehicle != vehicle || !rb))
  {
   AICarMovementComponent was = AICarMovementComponent.Cast(m_LaneSlowVehicle.FindComponent(AICarMovementComponent));
   if (was) was.ResetCruiseSpeed();
   PrintFormat("[ACR] DRIVE %1: past the roadblock lane, road speed again", m_Name);
   m_LaneSlowVehicle = null;
  }
  if (!rb) return;
  AICarMovementComponent movement = AICarMovementComponent.Cast(vehicle.FindComponent(AICarMovementComponent));
  if (!movement) return;
  movement.SetCruiseSpeed(ACR_Roadblocks.LANE_SPEED_KMH);
  if (m_LaneSlowVehicle == vehicle) return;
  m_LaneSlowVehicle = vehicle;
  PrintFormat("[ACR] DRIVE %1: slowing to %2 km/h through our roadblock lane at %3", m_Name, ACR_Roadblocks.LANE_SPEED_KMH, rb.m_vPos);
 }

 // Given up on driving: the crew stays with the vehicle and holds; AI passengers get
 // out and carry on (a ride sends its riders on to the objective on foot).
 protected void StuckHold(IEntity vehicle, float now)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  // Saved to try again later (HoldRetryWatch). Not a ride: it ends with this hold, its riders walk on.
  bool ride = manager && manager.m_Rides && (manager.m_Rides.RideOf(this) || manager.m_Rides.Unloading(this));
  ACR_OrderSnapshot retry = null;
  if (!ride && !PlayerSteers()) retry = OrderToRetry();
  if (manager)
  {
   array<ACR_Squad> riders = {};
   ACR_Transport.PassengerSquads(manager, vehicle, riders);
   foreach (ACR_Squad rider : riders) if (rider != this && !rider.HasPlayer()) rider.DismountNow();
  }
  Halt("Stuck, holding");
  // The same trip as last time counts its retries on; a new destination starts again.
  if (retry && vector.DistanceXZ(retry.m_Target, m_vHoldRetryGoal) > STUCK_LEFT_M) m_iHoldRetries = 0;
  m_HoldRetryOrder = null;
  if (retry && m_iHoldRetries < HOLD_RETRIES)
  {
   m_HoldRetryOrder = retry;
   m_vHoldRetryGoal = retry.m_Target;
   m_iHoldRetrySerial = m_iOrderSerial;
  }
  m_iStuckCount++;
  m_StallVehicle = null;
  m_iUnsticks = 0;
  m_iReroutes = 0;
  NoteStuck(vehicle, now);
  if (vehicle) ACR_Transport.SanitizeVehicleHorn(vehicle, true);
  PrintFormat("[ACR] DRIVE %1: stuck, crew stays aboard and holds at %2", m_Name, DriveSpot(vehicle));
 }

 static bool HoldRetryDue(float now, float stuckAt, int retriesDone)
 {
  return retriesDone < HOLD_RETRIES && now - stuckAt >= HOLD_RETRY_MS;
 }

 // The trip in hand: its final destination when it is on road legs (Reroute), else the order.
 protected ACR_OrderSnapshot OrderToRetry()
 {
  ResourceName prefab = m_sOrderPrefab;
  vector goal = m_MoveTarget;
  float radius = m_fOrderRadius;
  if (m_Route && !m_aRoute.IsEmpty())
  {
   prefab = m_Route.m_sPrefab;
   radius = m_Route.m_fRadius;
   goal = m_aRoute[m_aRoute.Count() - 1];
  }
  if (prefab.IsEmpty() || goal == vector.Zero) return null;
  ACR_EOrderState state = OrderState();
  if (state == ACR_EOrderState.HOLD || state == ACR_EOrderState.NONE) return null;
  return ACR_OrderSnapshot.Make(prefab, goal, radius, m_Order, state, m_OrderKind);
 }

 // Its job was ended by whoever gave it (a called-off ammo truck, a finished service job): no retry.
 void DropHoldRetry() { m_HoldRetryOrder = null; }

 protected void HoldRetryWatch(float now)
 {
  if (!m_HoldRetryOrder) return;
  if (m_iOrderSerial != m_iHoldRetrySerial || PlayerSteers())
  {
   m_HoldRetryOrder = null; // given another order meanwhile
   return;
  }
  if (!HoldRetryDue(now, m_fStuckAt, m_iHoldRetries)) return;
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (!vehicle || !CrewAboard(vehicle)) return; // its crew isn't all aboard: wait (Recrew or a new order)
  ChimeraCharacter driver = DriverOf(vehicle);
  if (!driver || !ACR_Respawn.IsAlive(driver) || FindController(driver) != 0) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && manager.IsGunnerDriven(vehicle)) return;
  if (KnownEnemies(300) > 0 || BBB_Transport.VehicleHitRecently(vehicle, 6000)) return;
  float fuel = ACR_Logistics.Fuel(vehicle);
  if (fuel >= 0 && fuel < 0.02) return;
  if (SCR_AIVehicleUsability.IsVehicleFlipped(vehicle)) return;
  ACR_OrderSnapshot order = m_HoldRetryOrder;
  m_HoldRetryOrder = null;
  m_iHoldRetries++;
  m_StallVehicle = null;
  m_iUnsticks = 0;
  m_iReroutes = 0;
  // An internal re-issue: the serial stays (a new order from anyone else still shows).
  RestoreOrder(order);
  PrintFormat("[ACR] DRIVE %1: stuck hold, trying again (%2 of %3) to %4 from %5", m_Name, m_iHoldRetries, HOLD_RETRIES, ACR_Manager.Grid(order.m_Target), DriveSpot(vehicle));
 }

 // A living, conscious member of this squad is getting in or out of a vehicle, or is on foot
 // within BOARDING_RANGE of this one (#53).
 bool CrewBoarding(IEntity vehicle)
 {
  if (!vehicle) return false;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || !ACR_Respawn.IsAlive(body) || body.GetCharacterController().IsUnconscious()) continue;
   CompartmentAccessComponent access = body.GetCompartmentAccessComponent();
   if (access && (access.IsGettingIn() || access.IsGettingOut())) return true;
   if (!ACR_Transport.VehicleOf(body) && vector.DistanceXZ(body.GetOrigin(), vehicle.GetOrigin()) < BOARDING_RANGE) return true;
  }
  return false;
 }

 protected void BeginUnstick(IEntity vehicle, ChimeraCharacter driver)
 {
  m_bUnsticking = true;
  m_iUnstickSerial = m_iOrderSerial;
  m_vUnstickFrom = vehicle.GetOrigin();
  m_iStuckCount++;
  float steer = 0.8;
  if (m_iUnsticks % 2 == 0) steer = -0.8;
  if (!m_UnstickListener) m_UnstickListener = new ACR_SquadUnstick(this);
  // BBB_Unstick parks the driver's AI and reverses (inputs applied every frame).
  BBB_Unstick.Begin(vehicle, driver, steer, m_UnstickListener);
  PrintFormat("[ACR] DRIVE %1: stuck, backing up (try %2) at %3", m_Name, m_iUnsticks, DriveSpot(vehicle));
 }

 // Backing out is over (BBB_Unstick has given the driver his AI back).
 void EndUnstick()
 {
  m_bUnsticking = false;
  if (m_StallVehicle)
  {
   m_StallPos = m_StallVehicle.GetOrigin();
   ACR_Transport.SanitizeVehicleHorn(m_StallVehicle, true);
  }
  m_fStallSince = GetGame().GetWorld().GetWorldTime();
  // A fresh order only if the waypoint is gone: re-issuing makes the driver stop and plan again. Not
  // after a newer order (Halt while it backed out kept m_sOrderPrefab: the truck drove off to the old
  // destination labelled "Holding"), nor for a squad a player steers (review, 12 Nov).
  if (m_Group && !m_sOrderPrefab.IsEmpty() && !m_Group.GetCurrentWaypoint() && m_iUnstickSerial == m_iOrderSerial && !PlayerSteers())
  {
   // Label and state stay as they are.
   int kind = m_OrderKind;
   IssueWaypoint(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius);
   m_OrderKind = kind;
  }
 }

 // Everyone out of the vehicle, passengers included. With resume the crew then
 // carries on to its destination on foot; otherwise it takes cover where it is.
 protected void Bail(IEntity vehicle, string why, bool resume)
 {
  PrintFormat("[ACR] DRIVE %1: %2, crew bails out at %3", m_Name, why, DriveSpot(vehicle));
  if (vehicle) ACR_Transport.SanitizeVehicleHorn(vehicle, true);
  // Handbrake on: from Reforger 1.9 the automatic gearbox drops a driverless vehicle into neutral, and an
  // abandoned truck rolled 40 m downhill into the BTR its crew had just bailed out from (pak:mannersevade).
  CarControllerComponent bailCar;
  if (vehicle) bailCar = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
  if (bailCar && !BBB_PilotHold.Driving(vehicle)) bailCar.SetPersistentHandBrake(true);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager)
  {
   array<ACR_Squad> riders = {};
   ACR_Transport.PassengerSquads(manager, vehicle, riders);
   foreach (ACR_Squad rider : riders) if (rider != this && !rider.HasPlayer()) rider.DismountNow();
  }
  // Anyone whose AI was parked (backing up, gunner drive) must be able to walk away.
  array<AIAgent> crew = {};
  GetAgents(crew);
  foreach (AIAgent member : crew)
  {
   IEntity body = member.GetControlledEntity();
   if (!body || FindController(body) != 0) continue;
   AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
   if (control && !control.IsAIActivated()) control.ActivateAI();
  }
  ResourceName prefab = m_sOrderPrefab;
  if (!resume) prefab = "";
  DismountAndContinue(prefab, m_MoveTarget, m_fOrderRadius, m_Order, m_OrderKind);
  m_StallVehicle = null;
  m_iUnsticks = 0;
 }

 // Whoever set a resume order restored it another way (an air lift does it itself).
 void ForgetResume() { m_bResumeOnFoot = false; }

 // A job (mines, a roadblock) given while aboard a ride: everyone out, the job left as it is to
 // carry on on foot. DismountNow's Halt would end it, refunded (25 Oct playtest: a minefield
 // ordered to a squad on a truck was paid and refunded in the same second, the squad then
 // staying aboard under the job's label).
 void GetOutForJob()
 {
  GetEveryoneOut();
  m_bHasLZ = false;
  m_iDismountSweeps = 0;
  m_iJobSweepSerial = m_iOrderSerial;
  GetGame().GetCallqueue().Remove(SweepOutForJob);
  GetGame().GetCallqueue().CallLater(SweepOutForJob, 3000, false);
 }

 // Soldiers the engine took away for its active-AI limit (group dormant); they still count
 // toward the soldier cap so the AI doesn't keep buying empty squads.
 int CountDormant()
 {
  if (!m_Group || !m_Group.IsDormant()) return 0;
  return Math.Max(0, m_Group.GetDormantAliveCount());
 }

 // Rough fighting value: soldiers, plus 5 per armed (manned gun) vehicle (Arma Commander's default vehicle value).
 int Strength()
 {
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  int armed;
  foreach (IEntity vehicle : vehicles) if (HasGunner(vehicle)) armed++;
  return CountAlive() + armed * 5;
 }

 // Strength() less fixed guns (a roadblock's or a nest's MG, a tripod): what the squad takes with it
 // when it moves off. Attacks and grabs count this, so a gunner leaving his post doesn't read as
 // losses (6 Nov: a grab from a roadblock set out at 7, was 2 a second later, called off "too many losses").
 int FieldStrength()
 {
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  int armed;
  foreach (IEntity vehicle : vehicles) if (HasGunner(vehicle) && !ACR_Intel.StaticGun(vehicle)) armed++;
  return CountAlive() + armed * 5;
 }

 // In a vehicle that drives, not just a man on a fixed gun. IsMounted() counts both, so a squad
 // manning a roadblock's MG passed for mounted: grabbed at driving speed, past the foot limit.
 bool InMobileVehicle()
 {
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  foreach (IEntity vehicle : vehicles) if (!ACR_Intel.StaticGun(vehicle)) return true;
  return false;
 }

 // Board a specific vehicle if one is within 30 m of the point, otherwise the nearest free one.
 bool GetIn(vector position)
 {
  return BoardVehicle(FindVehicleNear(position, 30), position);
 }

 // Board this vehicle (or, without one, the nearest free vehicle to the point).
 bool BoardVehicle(Vehicle vehicle, vector position)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (vehicle)
  {
   if (!IssueWaypoint(WP_GET_IN, vehicle.GetOrigin(), -1, true, vehicle)) return false;
  }
  else if (!IssueWaypoint(WP_GET_IN_NEAREST, position, 20, true)) return false;
  SetOrder("Getting in", ACR_EOrderState.GET_IN);
  m_OrderKind = ACR_EOrderKind.GET_IN;
  TrackBoarding(vehicle);
  return true;
 }

 void CancelLostBoarding()
 {
  Halt("Boarding vehicle lost");
  PrintFormat("[ACR] BOARD %1: target lost, order cancelled", m_Name);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && GetFaction()) manager.NotifyFaction(GetFaction().GetFactionKey(), m_Name + ": boarding cancelled, vehicle lost.");
 }

 protected void WatchBoardingTarget()
 {
  if (OrderState() != ACR_EOrderState.GET_IN || !m_Group || m_Group.GetCurrentWaypoint() != m_Waypoint) return;
  SCR_BoardingEntityWaypoint boarding = SCR_BoardingEntityWaypoint.Cast(m_Waypoint);
  if (boarding && (!boarding.GetEntity() || ACR_Logistics.Health(boarding.GetEntity()) <= 0)) CancelLostBoarding();
 }

 // The pull-over is done: out now (unless a new order came meanwhile).
 void PulledOver(int serial)
 {
  if (serial != m_iOrderSerial || serial != m_iPullOverSerial || !m_Group) return;
  m_bPulledOver = true;
  GetOut();
 }

 bool Evade(IEntity vehicle, vector threat)
 {
  if (!s_bTestEvade && ACR_LogisticsRules.Choice("acrEvadeArmour", "0") != "1") return false;
  if (!vehicle || !m_Group || m_Evade || PlayerSteers()) return false;
  float speed;
  if (vehicle.GetPhysics()) speed = vehicle.GetPhysics().GetVelocity().Length();
  if (OrderState() != ACR_EOrderState.MOVE && speed < 2) return false;
  vector at = vehicle.GetOrigin();
  vector away = at - threat;
  away[1] = 0;
  if (away.LengthSq() < 1) return false;
  away.Normalize();
  vector target = at + away * EVADE_M;
  vector road;
  if (BBB_Terrain.RoadPointNear(target, 150, road)) target = road;
  else if (BBB_Terrain.IsWaterAt(target)) return false;
  ACR_OrderSnapshot snapshot = SnapshotOrder();
  if (!IssueWaypoint(WP_MOVE, target, 25, true)) return false;
  m_Evade = snapshot;
  m_vEvadeThreat = threat;
  float now = GetGame().GetWorld().GetWorldTime();
  m_fEvadeStart = now;
  m_fEvadeClearAt = now + EVADE_CLEAR_MS;
  SetOrder("Getting away", ACR_EOrderState.MOVE);
  PrintFormat("[ACR] EVADE %1: unarmed, an armed enemy vehicle %2 m away: gets away to %3", m_Name, Math.Round(vector.DistanceXZ(at, threat)), target);
  return true;
 }

 // The nearest enemy this squad knows of within 'radius'.
 bool NearestKnownEnemy(float radius, out vector at)
 {
  if (!m_Group) return false;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  IEntity anchor = GetAnchor();
  if (!utility || !utility.m_Perception || !anchor) return false;
  float best = radius;
  bool found;
  foreach (IEntity enemy : utility.m_Perception.m_aTargetEntities)
  {
   if (!enemy || !ACR_Respawn.IsAlive(enemy)) continue;
   float d = vector.DistanceXZ(enemy.GetOrigin(), anchor.GetOrigin());
   if (d > best) continue;
   best = d;
   at = enemy.GetOrigin();
   found = true;
  }
  return found;
 }

 // A player leads, rides in or ordered this vehicle's squad: he decides, the old rules stand.
 protected bool PlayerHasSay(IEntity vehicle)
 {
  if (HumanInCharge()) return true;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  array<ACR_Squad> riders = {};
  ACR_Transport.PassengerSquads(manager, vehicle, riders);
  foreach (ACR_Squad rider : riders) if (rider.PlayerSteers()) return true;
  return false;
 }

 // Pure (tests): pushing on along its order gets it away from the enemy (the way there doesn't lead
 // toward him: within 90 degrees of straight away from him).
 static bool OnIsAway(vector at, vector goal, vector enemy)
 {
  vector way = goal - at;
  vector away = at - enemy;
  way[1] = 0;
  away[1] = 0;
  if (way.LengthSq() < 400) return false; // nearly there: it isn't going anywhere
  return vector.Dot(way, away) > 0;
 }

 // Pure (tests): which way to drive out, within 80 degrees of straight 'away' from the enemy, the one
 // most along where the vehicle faces ('forward'). srv:stall 6 Nov: sent straight away with the enemy
 // behind it, the driver spent 40 s on a three-point turn under fire and the crew bailed.
 static vector DriveOutDir(vector away, vector forward)
 {
  away[1] = 0;
  forward[1] = 0;
  away.Normalize();
  if (forward.LengthSq() > 0.01) forward.Normalize();
  vector best = away;
  float bestScore = -10;
  for (int step = -4; step <= 4; step++)
  {
   float rad = step * 20 * Math.DEG2RAD;
   float c = Math.Cos(rad);
   float sn = Math.Sin(rad);
   vector dir = Vector(away[0] * c - away[2] * sn, 0, away[0] * sn + away[2] * c);
   float score = vector.Dot(dir, forward) + 0.5 * vector.Dot(dir, away);
   if (score > bestScore)
   {
    bestScore = score;
    best = dir;
   }
  }
  return best;
 }

 // Drives the vehicle out of fire (see above). False: it couldn't (no enemy to drive from and no
 // order to push on with, water that way, a player's say).
 // 'again': it is already driving out of fire and stopped: a fresh way, off the roads, keeping the order to restore.
 protected bool DriveOut(IEntity vehicle, float now, string why, bool again = false)
 {
  if (!vehicle || !m_Group || (m_Evade && !again) || PlayerHasSay(vehicle)) return false;
  vector at = vehicle.GetOrigin();
  vector enemy;
  bool known = NearestKnownEnemy(DRIVE_OUT_ENEMY, enemy);
  bool moving = OrderState() == ACR_EOrderState.MOVE || OrderState() == ACR_EOrderState.RETURN || OrderState() == ACR_EOrderState.DEFEND;
  // On along its order when that gets it away (or no enemy is known: hit by someone unseen).
  if (!again && moving && !m_sOrderPrefab.IsEmpty() && (!known || OnIsAway(at, m_MoveTarget, enemy)))
  {
   int kind = m_OrderKind;
   if (!IssueWaypoint(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius, true)) return false;
   m_OrderKind = kind;
   m_fNextDriveOut = now + DRIVE_OUT_AGAIN_MS;
   PrintFormat("[ACR] DRIVE %1: %2, drives on out of it", m_Name, why);
   return true;
  }
  if (!known) return false;
  vector away = at - enemy;
  away[1] = 0;
  if (away.LengthSq() < 1) return false;
  away.Normalize();
  vector target = at + DriveOutDir(away, vehicle.GetTransformAxis(2)) * DRIVE_OUT_M;
  vector road;
  if (!again && BBB_Terrain.RoadPointNear(target, 150, road)) target = road;
  else if (BBB_Terrain.IsWaterAt(target)) return false;
  // Driving out again keeps the order it had before the first drive out.
  ACR_OrderSnapshot snapshot = m_Evade;
  if (!snapshot) snapshot = SnapshotOrder();
  if (!IssueWaypoint(WP_MOVE, target, 25, true)) return false;
  m_Evade = snapshot;
  m_bFireEvade = true;
  m_vEvadeThreat = enemy;
  m_fEvadeStart = now;
  m_fEvadeClearAt = now + EVADE_CLEAR_MS;
  m_fNextDriveOut = now + DRIVE_OUT_AGAIN_MS;
  SetOrder("Getting away", ACR_EOrderState.MOVE);
  string how = "";
  if (again) how = ", again a fresh way";
  PrintFormat("[ACR] DRIVE %1: %2, drives out %3 m away from the enemy %4 m off%5", m_Name, why, (int)DRIVE_OUT_M, Math.Round(vector.DistanceXZ(at, enemy)), how);
  return true;
 }

 // The threat lies near the straight way from 'from' to 'to' (within 150 m of it, between the two).
 static bool PastThreat(vector from, vector to, vector threat)
 {
  vector way = to - from;
  way[1] = 0;
  float len2 = vector.Dot(way, way);
  if (len2 < 1) return false;
  vector rel = threat - from;
  rel[1] = 0;
  float t = vector.Dot(rel, way) / len2;
  if (t < 0 || t > 1.2) return false;
  vector closest = from + way * t;
  return vector.DistanceXZ(closest, threat) < 150;
 }

 protected void EvadeTick(float now)
 {
  if (!m_Evade) return;
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  IEntity vehicle;
  if (!vehicles.IsEmpty()) vehicle = vehicles[0];
  vector threat;
  if (vehicle && BBB_Traffic.ArmedEnemySeen(vehicle, threat))
  {
   m_fEvadeClearAt = now + EVADE_CLEAR_MS;
   m_vEvadeThreat = threat;
  }
  else if (vehicle && m_bFireEvade && (BBB_Transport.VehicleHitRecently(vehicle, 6000) || NearestKnownEnemy(300, threat)))
  {
   m_fEvadeClearAt = now + EVADE_CLEAR_MS;
   if (threat != vector.Zero) m_vEvadeThreat = threat;
  }
  if (vehicle && now < m_fEvadeClearAt && now - m_fEvadeStart < EVADE_MAX_MS) return;
  ACR_OrderSnapshot back = m_Evade;
  m_Evade = null;
  m_bFireEvade = false;
  // Its order leads past the armed vehicle (within 150 m of the way there): it waits instead of driving
  // back into it; the commander or the player gives the next order.
  if (vehicle && PastThreat(vehicle.GetOrigin(), back.m_Target, m_vEvadeThreat))
  {
   Halt("Waiting: armed enemy vehicle on the way");
   PrintFormat("[ACR] EVADE %1: clear, but its order leads past the armed vehicle: waiting", m_Name);
   return;
  }
  RestoreOrder(back);
  PrintFormat("[ACR] EVADE %1: clear, back to its order (%2)", m_Name, m_Order);
 }

 Vehicle FindVehicleNear(vector position, float radius)
 {
  m_FoundVehicle = null;
  m_SearchVehicleAt = position;
  m_fVehicleSearchDistance = radius;
  GetGame().GetWorld().QueryEntitiesBySphere(position, radius, CollectNearestVehicle);
  return m_FoundVehicle;
 }

 protected bool CollectNearestVehicle(IEntity entity)
 {
  Vehicle vehicle = Vehicle.Cast(entity);
  if (!vehicle || ACR_Logistics.Health(vehicle) <= 0) return true;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments || !HasOpenSeat(compartments)) return true;
  float distance = vector.DistanceXZ(vehicle.GetOrigin(), m_SearchVehicleAt);
  if (distance <= m_fVehicleSearchDistance)
  {
   m_fVehicleSearchDistance = distance;
   m_FoundVehicle = vehicle;
  }
  return true;
 }

 protected bool HasOpenSeat(SCR_BaseCompartmentManagerComponent compartments)
 {
  array<ECompartmentType> types = {ECompartmentType.PILOT, ECompartmentType.TURRET, ECompartmentType.CARGO};
  foreach (ECompartmentType type : types)
  {
   array<BaseCompartmentSlot> slots = {};
   compartments.GetCompartmentsOfType(slots, type);
   foreach (BaseCompartmentSlot slot : slots)
    if (slot && !slot.GetOccupant()) return true;
  }
  return false;
 }
}
