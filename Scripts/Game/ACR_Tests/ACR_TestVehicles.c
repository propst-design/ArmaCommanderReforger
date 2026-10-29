// Vehicle, gunner, transport, stall, ride, gunner-drive and road tests (split out of ACR_TestHarness, 2 Oct).
class ACR_TestVehicles : Managed
{
 protected ACR_TestHarness h;

 void ACR_TestVehicles(ACR_TestHarness harness) { h = harness; }

 // ---------------------------------------------------------------- -acrVehicleTest
 // Alpha boards the truck, Bravo boards as passengers, Alpha drives, unloads
 // Bravo, then dismounts itself. Logs how many of each squad are in the truck.
 vector m_TruckStart;
 void LogVehicle(string step)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range.m_Truck) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  PrintFormat("[ACR] VEHICLE %1 alphaIn=%2 bravoIn=%3 truckMoved=%4", step, CountInTruck(alpha), CountInTruck(bravo), vector.Distance(h.m_Range.m_Truck.GetOrigin(), m_TruckStart));
 }
 int CountInTruck(ACR_Squad squad)
 {
  if (!squad) return -1;
  int count;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !character.IsInVehicle()) continue;
   CompartmentAccessComponent compartment = character.GetCompartmentAccessComponent();
   if (compartment && compartment.GetVehicleIn(character) == h.m_Range.m_Truck) count++;
  }
  return count;
 }
 void TestVehicle()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc || !pc.GetControlledEntity() || !h.m_Range.m_Truck || !h.m_Range.m_Group || !h.m_Range.m_Bravo) return;
  RplId alpha = SCR_EntityHelper.EntityToRplId(h.m_Range.m_Group);
  RplId bravo = SCR_EntityHelper.EntityToRplId(h.m_Range.m_Bravo);
  vector truckPos = h.m_Range.m_Truck.GetOrigin();
  if (h.m_Ticks == 12)
  {
   m_TruckStart = truckPos;
   pc.ACR_Request(ACR_ERequest.ORDER_GET_IN, alpha, truckPos);
  }
  if (h.m_Ticks == 35) { LogVehicle("alphaBoarded"); pc.ACR_Request(ACR_ERequest.ORDER_GET_IN, bravo, truckPos); }
  if (h.m_Ticks == 60) { LogVehicle("bravoBoarded"); pc.ACR_Request(ACR_ERequest.MOVE_TO, alpha, m_TruckStart + Vector(80, 0, 0)); }
  if (h.m_Ticks == 95) { LogVehicle("drove"); pc.ACR_Request(ACR_ERequest.ORDER_UNLOAD, alpha); }
  if (h.m_Ticks == 115) { LogVehicle("unloaded"); pc.ACR_Request(ACR_ERequest.ORDER_GET_OUT, alpha); }
  if (h.m_Ticks == 135) LogVehicle("dismounted");
 }

 // ---------------------------------------------------------------- -acrGunnerTest
 IEntity m_GunVehicle;
 ChimeraCharacter m_Gunner;
 vector m_GunStart;
 float m_GunYaw;

 string GunState()
 {
  if (!m_GunVehicle) return "no vehicle";
  vector angles = m_GunVehicle.GetYawPitchRoll();
  CarControllerComponent car = CarControllerComponent.Cast(m_GunVehicle.FindComponent(CarControllerComponent));
  string sim = "";
  if (car && car.GetSimulation())
  {
   VehicleWheeledSimulation s = car.GetSimulation();
   sim = string.Format("gear=%1 (n=%2) rpm=%3 thr=%4 brk=%5 steer=%6 engine=%7", s.GetGear(), s.GearboxGearsCount() - s.GearboxForwardGearsCount() - 1, (int)s.EngineGetRPM(), s.GetThrottle(), s.GetBrake(), s.GetSteering(), car.IsEngineOn());
  }
  vector offset = m_GunVehicle.GetOrigin() - m_GunStart;
  float forward = vector.Dot(offset, Vector(Math.Sin(m_GunYaw * Math.DEG2RAD), 0, Math.Cos(m_GunYaw * Math.DEG2RAD)));
  float speed = 0;
  if (m_GunVehicle.GetPhysics()) speed = m_GunVehicle.GetPhysics().GetVelocity().Length();
  return string.Format("moved=%1 forward=%2 yaw=%3 speed=%4 %5", offset.Length(), forward, angles[0], speed, sim);
 }

 void TestGunner()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  if (h.m_Ticks == 5) manager.SpawnUnit("Gun Truck", "", "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et", true, "", h.Ground("1390 0 2795"), h.Ground("1395 0 2790"));
  if (h.m_Ticks == 14)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (squad.GetName() != "Gun Truck") continue;
    array<IEntity> vehicles = {};
    squad.GetVehicles(vehicles);
    if (!vehicles.IsEmpty()) m_GunVehicle = vehicles[0];
   }
   if (!m_GunVehicle) { Print("[ACR] GUNNER FAIL: no gun truck"); return; }
   SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_GunVehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<BaseCompartmentSlot> turrets = {};
   if (compartments) compartments.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
   foreach (BaseCompartmentSlot turret : turrets)
    if (turret.GetOccupant()) m_Gunner = ChimeraCharacter.Cast(turret.GetOccupant());
   PrintFormat("[ACR] GUNNER turrets=%1 gunner=%2", turrets.Count(), m_Gunner);
   // The panel's "Take over gunner" button.
   ACR_Squad gunSquad = manager.FindSquadBySoldier(m_Gunner);
   PrintFormat("[ACR] GUNNER squad gunner found=%1", ACR_GunnerDrive.SquadGunner(gunSquad) == m_Gunner);
   if (gunSquad) pc.ACR_Request(ACR_ERequest.TAKE_GUNNER, SCR_EntityHelper.EntityToRplId(gunSquad.m_Group));
  }
  if (!m_GunVehicle) return;
  if (h.m_Ticks == 17)
  {
   ChimeraCharacter driver;
   IEntity drivable = ACR_GunnerDrive.DrivableFrom(pc.GetControlledEntity(), driver);
   PrintFormat("[ACR] GUNNER possessed=%1 drivable=%2 driver=%3", pc.GetControlledEntity() == m_Gunner, drivable == m_GunVehicle, driver);
   m_GunStart = m_GunVehicle.GetOrigin();
   m_GunYaw = m_GunVehicle.GetYawPitchRoll()[0];
  }
  if (h.m_Ticks >= 18 && h.m_Ticks < 24) pc.ACR_TestDrive(m_GunVehicle, 1, 0);
  if (h.m_Ticks >= 24 && h.m_Ticks < 26) pc.ACR_TestDrive(m_GunVehicle, 0, 0);
  if (h.m_Ticks >= 26 && h.m_Ticks < 31) pc.ACR_TestDrive(m_GunVehicle, 1, 1);
  if (h.m_Ticks >= 31 && h.m_Ticks < 35) pc.ACR_TestDrive(m_GunVehicle, 0, 0);
  if (h.m_Ticks == 35) { m_GunStart = m_GunVehicle.GetOrigin(); m_GunYaw = m_GunVehicle.GetYawPitchRoll()[0]; }
  if (h.m_Ticks >= 35 && h.m_Ticks < 41) pc.ACR_TestDrive(m_GunVehicle, -1, 0);
  if (h.m_Ticks >= 41 && h.m_Ticks < 44) pc.ACR_TestDrive(m_GunVehicle, 0, 0);
  if (h.m_Ticks >= 17 && h.m_Ticks <= 45) PrintFormat("[ACR] GUNNER t=%1 drivenByGunner=%2 %3", h.m_Ticks, manager.IsGunnerDriven(m_GunVehicle), GunState());
  if (h.m_Ticks == 46)
  {
   ACR_Squad squad = manager.FindSquadBySoldier(m_Gunner);
   if (squad) pc.ACR_Request(ACR_ERequest.MOVE_TO, SCR_EntityHelper.EntityToRplId(squad.m_Group), m_GunVehicle.GetOrigin() + Vector(0, 0, 60));
  }
  if (h.m_Ticks == 50 || h.m_Ticks == 60) PrintFormat("[ACR] GUNNER after map order: drivenByGunner=%1 %2", manager.IsGunnerDriven(m_GunVehicle), GunState());
  if (h.m_Ticks == 62) Print("[ACR] TEST COMPLETE");
 }
 ChimeraCharacter m_HueyPilot;
 vector m_HoverAt;

 void Hover()
 {
  if (!h.m_Huey) return;
  vector transform[4];
  Math3D.AnglesToMatrix(Vector(90, 0, 0), transform);
  transform[3] = m_HoverAt;
  h.m_Huey.SetTransform(transform);
  Physics physics = h.m_Huey.GetPhysics();
  if (physics)
  {
   physics.EnableGravity(false);
   physics.SetVelocity(vector.Zero);
   physics.SetAngularVelocity(vector.Zero);
  }
 }

 void TestTransport()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  vector lz = h.Ground("1600 0 2830");
  if (h.m_Ticks == 5) manager.SpawnUnit("Huey", "", "{70BAEEFC2D3FEE64}Prefabs/Vehicles/Helicopters/UH1H/UH1H.et", true, "", h.Ground("1420 0 2830"), h.Ground("1425 0 2820"));
  if (h.m_Ticks == 12)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (squad.GetName() != "Huey") continue;
    array<IEntity> vehicles = {};
    squad.GetVehicles(vehicles);
    if (!vehicles.IsEmpty()) h.m_Huey = vehicles[0];
   }
   if (!h.m_Huey) { Print("[ACR] TRANSPORT FAIL: no helicopter"); return; }
   pc.ACR_Request(ACR_ERequest.ORDER_GET_IN, SCR_EntityHelper.EntityToRplId(h.m_Range.m_Group), h.m_Huey.GetOrigin());
  }
  if (h.m_Ticks == 13 && h.m_Huey) pc.ACR_Request(ACR_ERequest.ORDER_GET_IN, SCR_EntityHelper.EntityToRplId(h.m_Range.m_Bravo), h.m_Huey.GetOrigin());
  if (!h.m_Huey) return;
  if (h.m_Ticks >= 14 && h.m_Ticks <= 50 && h.m_Ticks % 6 == 0) PrintFormat("[ACR] TRANSPORT t=%1 aboard: Alpha %2 Bravo %3", h.m_Ticks, h.Aboard(h.m_Range.m_Group), h.Aboard(h.m_Range.m_Bravo));
  if (h.m_Ticks == 52) pc.ACR_Request(ACR_ERequest.ORDER_SET_LZ, SCR_EntityHelper.EntityToRplId(h.m_Range.m_Group), lz);
  if (h.m_Ticks == 54) m_HoverAt = lz + Vector(0, 25, 0);
  if (h.m_Ticks >= 54 && h.m_Ticks < 60) Hover();
  if (h.m_Ticks == 59) PrintFormat("[ACR] TRANSPORT hovering 25 m over the LZ: Alpha aboard %1 (expect all)", h.Aboard(h.m_Range.m_Group));
  if (h.m_Ticks == 60) m_HoverAt = lz + Vector(0, 1.5, 0);
  if (h.m_Ticks >= 60 && h.m_Ticks < 80) Hover();
  if (h.m_Ticks == 66) PrintFormat("[ACR] TRANSPORT set down at the LZ: Alpha aboard %1 (expect 0), Bravo aboard %2", h.Aboard(h.m_Range.m_Group), h.Aboard(h.m_Range.m_Bravo));
  if (h.m_Ticks == 67)
  {
   SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(h.m_Huey.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<BaseCompartmentSlot> pilots = {};
   if (compartments) compartments.GetCompartmentsOfType(pilots, ECompartmentType.PILOT);
   foreach (BaseCompartmentSlot seat : pilots) if (seat.GetOccupant()) { m_HueyPilot = ChimeraCharacter.Cast(seat.GetOccupant()); break; }
   if (m_HueyPilot) pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(m_HueyPilot));
  }
  if (h.m_Ticks == 70) m_HoverAt = lz + Vector(40, 10, 0);
  if (h.m_Ticks >= 70 && h.m_Ticks < 90) Hover();
  if (h.m_Ticks == 74) PrintFormat("[ACR] TRANSPORT possessed pilot=%1, Bravo aboard %2", pc.GetControlledEntity() == m_HueyPilot, h.Aboard(h.m_Range.m_Bravo));
  if (h.m_Ticks == 75 || h.m_Ticks == 76) pc.ACR_Request(ACR_ERequest.UNLOAD_HERE);
  if (h.m_Ticks == 82)
  {
   PrintFormat("[ACR] TRANSPORT after F6 x2 at 10 m: Bravo aboard %1 (expect 0), Bravo alive %2", h.Aboard(h.m_Range.m_Bravo), h.AliveIn(h.m_Range.m_Bravo));
   foreach (ACR_Squad squad : manager.m_Squads) PrintFormat("[ACR] TRANSPORT   %1: %2", squad.GetName(), squad.Describe());
  }
  if (h.m_Ticks == 88) PrintFormat("[ACR] TRANSPORT final: Bravo aboard %1, alive %2", h.Aboard(h.m_Range.m_Bravo), h.AliveIn(h.m_Range.m_Bravo));
  if (h.m_Ticks == 89) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrStallTest
 // Stuck Truck: its driver "freezes" (AI off) on a move order, and again after each
 // back-up: two tries, then the crew bails and walks on. Lone Truck: gunner killed,
 // made too heavy to move, enemies close: the crew bails at once and takes cover.
 ACR_Squad m_StuckSquad;
 ACR_Squad m_LoneSquad;
 ACR_Squad m_SoftSquad;     // unarmed truck, frozen: backs up twice, is re-routed, then holds with its crew aboard
 IEntity m_SoftVehicle;
 vector m_SoftStart;
 bool m_bSoftRerouted;
 IEntity m_StuckVehicle;
 IEntity m_LoneVehicle;
 bool m_bWasUnsticking;
 vector m_UnstickFrom;
 vector m_StallStart;
 vector m_LoneStart;

 bool m_bLoneOut;

 // A frozen driver lets a truck roll on a slope, which reads as moving (a frozen truck rolled
 // 20-95 m after its second back-up, 29 Oct): held still between back-ups.
 void Pin(IEntity vehicle)
 {
  if (!vehicle) return;
  Physics phys = vehicle.GetPhysics();
  if (!phys) return;
  phys.SetVelocity(vector.Zero);
  phys.SetAngularVelocity(vector.Zero);
 }

 void Freeze(IEntity vehicle)
 {
  ChimeraCharacter driver = ACR_Squad.DriverOf(vehicle);
  if (!driver) return;
  AIControlComponent ai = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
  if (ai && ai.IsAIActivated()) ai.DeactivateAI();
 }

 string StallState(ACR_Squad squad, IEntity vehicle, vector from)
 {
  if (!squad) return "no squad";
  int alive;
  int aboard;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body)) continue;
   alive++;
   if (ACR_Transport.VehicleOf(body)) aboard++;
  }
  float moved;
  if (vehicle) moved = vector.DistanceXZ(vehicle.GetOrigin(), from);
  float walked;
  IEntity anchor = squad.GetAnchor();
  if (anchor && vehicle) walked = vector.DistanceXZ(anchor.GetOrigin(), vehicle.GetOrigin());
  return string.Format("alive=%1 aboard=%2 order='%3' tries=%4 unsticking=%5 vehicleMoved=%6m squadFromVehicle=%7m combat=%8 moveFailsKept=%9", alive, aboard, squad.m_Order, squad.UnstickTries(), squad.IsUnsticking(), (int)moved, (int)walked, squad.IsCombatUnit(), squad.MoveFailsKept());
 }

 bool AllAboard(ACR_Squad squad, IEntity vehicle)
 {
  if (!squad || !vehicle) return false;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  int alive, aboard;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body)) continue;
   alive++;
   if (ACR_Transport.VehicleOf(body) == vehicle) aboard++;
  }
  return alive > 0 && aboard == alive;
 }

 void TestStall()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (h.m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   // On the test server nobody holds US command: keep its AI from re-tasking the trucks.
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
   // The soft truck's give-up sequence must finish before the enemy team walks into range.
   ACR_Squad.s_fStallSeconds = 8;
  }
  ResourceName gunTruck = "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et";
  if (h.m_Ticks == 5)
  {
   manager.SpawnUnit("Stuck Truck", "", gunTruck, true, "", h.Ground("1390 0 2795"), h.Ground("1395 0 2790"));
   manager.SpawnUnit("Lone Truck", "", gunTruck, true, "", h.Ground("1300 0 2530"), h.Ground("1305 0 2535"));
   manager.SpawnUnit("Soft Truck", "", "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et", true, "", h.Ground("1375 0 2775"), h.Ground("1380 0 2780"));
  }
  if (h.m_Ticks == 14)
  {
   m_StuckSquad = h.NamedSquad(manager, "Stuck Truck");
   m_LoneSquad = h.NamedSquad(manager, "Lone Truck");
   if (!m_StuckSquad || !m_LoneSquad) { Print("[ACR] STALL FAIL: trucks missing"); return; }
   m_StuckVehicle = ACR_Transport.CrewedVehicle(m_StuckSquad);
   m_LoneVehicle = ACR_Transport.CrewedVehicle(m_LoneSquad);
   if (!m_StuckVehicle || !m_LoneVehicle) { Print("[ACR] STALL FAIL: crews not aboard"); return; }
   array<AIAgent> agents = {};
   m_LoneSquad.GetAgents(agents);
   foreach (AIAgent agent : agents)
    if (ACR_Transport.SeatOf(agent.GetControlledEntity()) == ECompartmentType.TURRET) h.Kill(agent.GetControlledEntity());
   // Out of fuel: stranded.
   SCR_FuelManagerComponent tank = SCR_FuelManagerComponent.Cast(m_LoneVehicle.FindComponent(SCR_FuelManagerComponent));
   if (tank) tank.SetTotalFuelPercentage(0);
   ResourceName team = h.FireTeamOf(manager, "USSR");
   if (!team.IsEmpty()) ACR_Manager.SpawnPrefab(team, h.Ground("1300 0 2400"));
   m_StuckSquad.MoveTo(h.Ground("1700 0 2800"));
   m_LoneSquad.MoveTo(h.Ground("1300 0 2250"));
   m_SoftSquad = h.NamedSquad(manager, "Soft Truck");
   if (m_SoftSquad) m_SoftVehicle = ACR_Transport.CrewedVehicle(m_SoftSquad);
   if (m_SoftVehicle)
   {
    m_SoftStart = m_SoftVehicle.GetOrigin();
    // Nearly no fuel as well: frozen drivers let a truck roll on a slope, which reads as moving. Not
    // empty: a crew whose tank is empty gets out as vanilla has it (the Lone Truck's check).
    SCR_FuelManagerComponent softTank = SCR_FuelManagerComponent.Cast(m_SoftVehicle.FindComponent(SCR_FuelManagerComponent));
    if (softTank) softTank.SetTotalFuelPercentage(0.02);
    m_SoftSquad.MoveTo(h.Ground("1700 0 2850"));
    Freeze(m_SoftVehicle);
   }
   m_StallStart = m_StuckVehicle.GetOrigin();
   m_LoneStart = m_LoneVehicle.GetOrigin();
   Freeze(m_StuckVehicle);
   PrintFormat("[ACR] STALL start: enemy team=%1", team);
  }
  if (h.m_Ticks == 170)
  {
   ACR_TestRegress.Check("stall: unarmed truck re-routed over the roads before giving up", m_bSoftRerouted);
   bool softHeld = m_SoftSquad && m_SoftSquad.m_Order == "Stuck, holding";
   ACR_TestRegress.Check("stall: unarmed stuck truck holds", softHeld, StallState(m_SoftSquad, m_SoftVehicle, m_SoftStart));
   bool softAboard = AllAboard(m_SoftSquad, m_SoftVehicle);
   ACR_TestRegress.Check("stall: its crew stays aboard (no bail when not under fire)", softAboard);
   ACR_TestRegress.Check("stall: stuck time recorded (AI won't re-crew it at once)", m_SoftSquad && m_SoftSquad.m_fStuckAt > 0);
   bool armedHeld = m_StuckSquad.m_Order == "Stuck, holding" && AllAboard(m_StuckSquad, m_StuckVehicle);
   ACR_TestRegress.Check("stall: armed stuck truck holds with its crew", armedHeld, StallState(m_StuckSquad, m_StuckVehicle, m_StallStart));
   // An empty tank: the crew gets out as vanilla has it (ACR_Squad.OnGroupMoveFailed leaves it be).
   ACR_TestRegress.Check("stall: an out-of-fuel truck's crew gets out", m_bLoneOut, StallState(m_LoneSquad, m_LoneVehicle, m_LoneStart));
   ACR_TestRegress.Complete("stall", 6);
  }
  if (h.m_Ticks == 172) Print("[ACR] TEST COMPLETE");
  if (!m_StuckSquad || h.m_Ticks <= 14) return;
  bool unsticking = m_StuckSquad.IsUnsticking();
  if (unsticking && !m_bWasUnsticking)
  {
   m_UnstickFrom = m_StuckVehicle.GetOrigin();
   PrintFormat("[ACR] STALL unstick try %1 began at t=%2", m_StuckSquad.UnstickTries(), h.m_Ticks);
  }
  if (!unsticking && m_bWasUnsticking)
  {
   PrintFormat("[ACR] STALL unstick ended at t=%1: backed %2 m", h.m_Ticks, (int)vector.DistanceXZ(m_StuckVehicle.GetOrigin(), m_UnstickFrom));
   Freeze(m_StuckVehicle); // still "stuck"
  }
  m_bWasUnsticking = unsticking;
  if (!unsticking) Pin(m_StuckVehicle);
  // The Lone Truck's crew getting out is seen as it happens: the enemy team may kill them after.
  if (!m_bLoneOut && m_LoneSquad && m_LoneSquad.CountAlive() > 0 && !AllAboard(m_LoneSquad, m_LoneVehicle))
  {
   m_bLoneOut = true;
   PrintFormat("[ACR] STALL lone truck's crew got out at t=%1", h.m_Ticks);
  }
  if (m_SoftSquad && m_SoftVehicle)
  {
   if (m_SoftSquad.Reroutes() > 0 && !m_bSoftRerouted) { m_bSoftRerouted = true; PrintFormat("[ACR] STALL soft truck re-routed at t=%1", h.m_Ticks); }
   if (!m_SoftSquad.IsUnsticking()) { Freeze(m_SoftVehicle); Pin(m_SoftVehicle); } // stays stuck, whatever route it is given
   if (h.m_Ticks % 5 == 0) PrintFormat("[ACR] STALL t=%1 soft: %2", h.m_Ticks, StallState(m_SoftSquad, m_SoftVehicle, m_SoftStart));
  }
  if (h.m_Ticks % 5 == 0)
  {
   PrintFormat("[ACR] STALL t=%1 stuck: %2", h.m_Ticks, StallState(m_StuckSquad, m_StuckVehicle, m_StallStart));
   if (m_LoneSquad) PrintFormat("[ACR] STALL t=%1 lone: %2", h.m_Ticks, StallState(m_LoneSquad, m_LoneVehicle, m_LoneStart));
  }
 }

 // ---------------------------------------------------------------- -acrRideTest
 // Bravo takes the "Ride" order to a point 560 m south; a crewed truck 70 m away
 // fetches it. The fetch limits are checked directly. Alpha walks east along the
 // runway with two men dragged 90 m behind, and must regroup before moving on.
 ACR_Squad m_RideSquad;
 ACR_Squad m_WalkSquad;
 ACR_Squad m_TaxiSquad;
 vector m_RideDest;

 string RideState()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  string state = "none";
  ACR_Ride ride;
  if (manager.m_Rides) ride = manager.m_Rides.RideOf(m_RideSquad);
  if (ride) state = typename.EnumToString(ACR_ERideState, ride.m_State);
  float toDest = -1;
  IEntity anchor = m_RideSquad.GetAnchor();
  if (anchor) toDest = vector.DistanceXZ(anchor.GetOrigin(), m_RideDest);
  float taxiMoved = -1;
  IEntity truck = ACR_Transport.CrewedVehicle(m_TaxiSquad);
  if (truck) taxiMoved = vector.DistanceXZ(truck.GetOrigin(), h.Ground("1400 0 2850"));
  vector truckAt;
  if (truck) truckAt = truck.GetOrigin();
  return string.Format("ride=%1 bravo='%2' aboard=%3/%4 toDest=%5m | taxi='%6' truckFromStart=%7m truck at %8", state, m_RideSquad.m_Order, h.Aboard(h.m_Range.m_Bravo), h.AliveIn(h.m_Range.m_Bravo), (int)toDest, m_TaxiSquad.m_Order, (int)taxiMoved, truckAt);
 }

 void TestRide()
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
   manager.SpawnUnit("Taxi 1", "", "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et", true, "", h.Ground("1400 0 2850"), h.Ground("1405 0 2855"));
  if (h.m_Ticks == 14)
  {
   m_TaxiSquad = h.NamedSquad(manager, "Taxi 1");
   m_RideSquad = manager.FindSquadByGroup(h.m_Range.m_Bravo);
   m_WalkSquad = h.Squad();
   if (!m_TaxiSquad || !m_RideSquad || !m_WalkSquad || !manager.m_Rides) { Print("[ACR] RIDE FAIL: setup incomplete"); return; }
   float d;
   ACR_Squad nearOk = manager.m_Rides.FindTransport("US", h.Ground("1300 0 2250"), 2000, 3, d);
   ACR_Squad tooFar = manager.m_Rides.FindTransport("US", h.Ground("1300 0 2250"), 1000, 3, d);
   ACR_Squad veryFar = manager.m_Rides.FindTransport("US", h.Ground("400 0 2250"), 5000, 3, d);
   PrintFormat("[ACR] RIDE limits: 600 m away on a 2 km trip -> %1 (expect Taxi 1) | 600 m on a 1 km trip -> %2 (expect none) | 1 km away -> %3 (expect none) | free=%4 seats=%5",
    nearOk, tooFar, veryFar, manager.m_Rides.IsFreeTransport(m_TaxiSquad, "US"), ACR_Rides.FreeSeats(ACR_Transport.CrewedVehicle(m_TaxiSquad)));
   // Land routing: a few long lines across the island; each leg of a route must be dry.
   array<vector> ends = {"1320 0 2832".ToVector(), "1450 0 1150".ToVector(), "3200 0 2700".ToVector(), "2500 0 1300".ToVector(), "600 0 1500".ToVector()};
   for (int e = 0; e + 1 < ends.Count(); e++)
   {
    vector from = h.Ground(string.Format("%1 0 %2", ends[e][0], ends[e][2]));
    vector to = h.Ground(string.Format("%1 0 %2", ends[e + 1][0], ends[e + 1][2]));
    array<vector> legs = {};
    int started = System.GetTickCount();
    bool routed = BBB_Terrain.LandRoute(from, to, legs);
    int took = System.GetTickCount() - started;
    float worst;
    vector prev = from;
    foreach (vector leg : legs)
    {
     worst = Math.Max(worst, BBB_Terrain.WaterAlong(prev, leg));
     prev = leg;
    }
    PrintFormat("[ACR] ROUTETEST %1 -> %2: straight water %3 m, routed=%4 legs=%5 worst leg water %6 m, %7 ms", from, to, (int)BBB_Terrain.WaterAlong(from, to), routed, legs.Count(), (int)worst, took);
   }
   // South to 1290/2300 the roads go 2 km round for 550 m: refused, they'd walk.
   string why;
   ACR_Ride refused = manager.m_Rides.Request(m_RideSquad, h.Ground("1290 0 2300"), 0, ACR_Squad.WP_MOVE, -1, "Moving", 0, why);
   PrintFormat("[ACR] RIDE CHECK long way round refused: %1 (%2)", refused == null, why);
   // East along a decent road instead.
   m_RideDest = h.Ground("2000 0 3050");
   pc.ACR_Request(ACR_ERequest.ORDER_RIDE, SCR_EntityHelper.EntityToRplId(h.m_Range.m_Bravo), m_RideDest);
   m_WalkSquad.MoveTo(h.Ground("1750 0 2832"));
  }
  if (!m_TaxiSquad || !m_RideSquad || !m_WalkSquad || h.m_Ticks < 15) return;
  if (h.m_Ticks == 22)
  {
   // Drag two of Alpha's men 90 m back along the route.
   IEntity leader = m_WalkSquad.GetLeader();
   array<AIAgent> agents = {};
   m_WalkSquad.GetAgents(agents);
   int moved;
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (!body || body == leader || moved >= 2 || !leader) continue;
    body.SetOrigin(h.Ground(string.Format("%1 0 %2", leader.GetOrigin()[0] - 90, leader.GetOrigin()[2] + moved * 3)));
    body.Update();
    moved++;
   }
   PrintFormat("[ACR] RIDE cohesion: dragged %1 back, stragglers(60)=%2", moved, m_WalkSquad.Stragglers(60));
  }
  if (h.m_Ticks % 5 == 0)
  {
   PrintFormat("[ACR] RIDE t=%1 %2", h.m_Ticks, RideState());
   PrintFormat("[ACR] RIDE t=%1 alpha='%2' stragglers(60)=%3 stragglers(25)=%4", h.m_Ticks, m_WalkSquad.m_Order, m_WalkSquad.Stragglers(60), m_WalkSquad.Stragglers(25));
  }
  if (h.m_Ticks == 170) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrDriveTest
 // Gunner drive inputs on the server, no player needed: full throttle straight,
 // then full throttle with full left lock. The diagnostics count how often
 // something else changed the inputs between physics steps.
 ACR_GunnerDrive m_TestDriveSession;
 void TestDrive()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (h.m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings ours = ACR_FactionSettings.Find("US");
   if (ours) ours.m_bAICommander = false;
  }
  if (h.m_Ticks == 5) manager.SpawnUnit("Gun Truck", "", "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et", true, "", h.Ground("1390 0 2795"), h.Ground("1395 0 2790"));
  if (h.m_Ticks == 14)
  {
   ACR_Squad squad = h.NamedSquad(manager, "Gun Truck");
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   ChimeraCharacter driver = ACR_Squad.DriverOf(vehicle);
   PrintFormat("[ACR] DRIVE vehicle=%1 driver=%2", vehicle != null, driver != null);
   // Variants: -acrDriveAITest keeps the driver's AI on, -acrDriveLockTest never slips the clutch.
   if (System.IsCLIParam("acrDriveAITest")) ACR_GunnerDrive.s_bParkDriverAI = false;
   if (System.IsCLIParam("acrDriveFixedTest")) ACR_GunnerDrive.s_bFixedOnly = true;
   if (System.IsCLIParam("acrDriveAttachedTest")) ACR_GunnerDrive.s_bDetachPilot = false;
   if (System.IsCLIParam("acrDriveLockTest")) { ACR_GunnerDrive.s_bSlipClutch = false; ACR_GunnerDrive.s_bOwnGearbox = false; }
   PrintFormat("[ACR] DRIVE variant: park driver AI %1, park vehicle AI %2, slip clutch %3, pilot detached %4", ACR_GunnerDrive.s_bParkDriverAI, ACR_GunnerDrive.s_bParkVehicleAI, ACR_GunnerDrive.s_bSlipClutch, ACR_GunnerDrive.s_bDetachPilot);
   if (vehicle && driver) m_TestDriveSession = manager.TestDrive(vehicle, driver, squad);
  }
  // After the hand-back the session is gone: watch the AI driver take over.
  if (h.m_Ticks > 43 && h.m_Ticks <= 75 && h.m_Ticks % 4 == 0 && m_DriveBackCar)
  {
   CarControllerComponent hc = CarControllerComponent.Cast(m_DriveBackCar.FindComponent(CarControllerComponent));
   PrintFormat("[ACR] DRIVE handback t=%1 moved=%2 m session=%3 pilot=%4", h.m_Ticks, (int)vector.Distance(m_DriveBackFrom, m_DriveBackCar.GetOrigin()), manager.IsGunnerDriven(m_DriveBackCar), hc && hc.GetPilotCompartmentSlot() != null);
  }
  // Regression (1 Oct live tests): inputs after the first one were dropped, so the vehicle
  // kept its first key for good. Feed two inputs as the gunner through the real path.
  if (h.m_Ticks == 35 && m_TestDriveSession)
  {
   ChimeraCharacter feeder = ACR_GunnerDrive.SquadGunner(m_TestDriveSession.m_Squad);
   manager.GunnerDriveInputFor(feeder, 0, m_TestDriveSession.m_Squad.GetFaction(), m_TestDriveSession.m_Vehicle, 0.4, 0);
   bool first = m_TestDriveSession.m_Throttle == 0.4;
   manager.GunnerDriveInputFor(feeder, 0, m_TestDriveSession.m_Squad.GetFaction(), m_TestDriveSession.m_Vehicle, 0, -1);
   bool second = m_TestDriveSession.m_Throttle == 0 && m_TestDriveSession.m_Steer == -1;
   PrintFormat("[ACR] REGRESS %1 gunner drive takes every input, not just the first (first %2 second %3)", ACR_TestRegress.Verdict(first && second), first, second);
  }
  // Regression (1 Oct MP): every real session ended at once with "gunner left".
  if (h.m_Ticks == 16 && m_TestDriveSession)
  {
   ChimeraCharacter gunner = ACR_GunnerDrive.SquadGunner(m_TestDriveSession.m_Squad);
   PrintFormat("[ACR] REGRESS %1 gunner still counted in the turret after the driver seat is unhooked (gunner %2)", ACR_TestRegress.Verdict(gunner && m_TestDriveSession.StillGunning(gunner)), gunner != null);
  }
  if (h.m_Ticks == 76) Print("[ACR] TEST COMPLETE");
  if (!m_TestDriveSession) return;
  float now = GetGame().GetWorld().GetWorldTime();
  m_TestDriveSession.m_LastInput = now;
  // Clutch 1 for the first run, 0 for the second (which way round is "engaged"?).
  if (h.m_Ticks >= 15 && h.m_Ticks < 23) { m_TestDriveSession.m_Throttle = 1; m_TestDriveSession.m_Steer = 0; m_TestDriveSession.m_fClutch = 1; }
  if (h.m_Ticks >= 23 && h.m_Ticks < 28) { m_TestDriveSession.m_Throttle = 1; m_TestDriveSession.m_Steer = -1; }
  if (h.m_Ticks >= 28 && h.m_Ticks < 32) { m_TestDriveSession.m_Throttle = 0; m_TestDriveSession.m_Steer = 0; }
  if (h.m_Ticks >= 32 && h.m_Ticks < 40) { m_TestDriveSession.m_Throttle = 1; m_TestDriveSession.m_Steer = 0; m_TestDriveSession.m_fClutch = 1; }
  if (h.m_Ticks >= 40) { m_TestDriveSession.m_Throttle = 0; m_TestDriveSession.m_Steer = 0; m_TestDriveSession.m_fClutch = 1; }
  if (h.m_Ticks >= 15 && h.m_Ticks <= 42 && m_TestDriveSession.m_Vehicle)
  {
   IEntity car = m_TestDriveSession.m_Vehicle;
   float speed = 0;
   if (car.GetPhysics()) speed = car.GetPhysics().GetVelocity().Length();
   PrintFormat("[ACR] DRIVE t=%1 speed=%2 yaw=%3", h.m_Ticks, speed, car.GetYawPitchRoll()[0]);
   CarControllerComponent cc = CarControllerComponent.Cast(car.FindComponent(CarControllerComponent));
   if (cc && cc.GetSimulation() && h.m_Ticks == 17)
   {
    VehicleWheeledSimulation ws = cc.GetSimulation();
    PrintFormat("[ACR] DRIVE engine load=%1 torque %2/%3 power %4/%5 gearbox eff %6/%7 rpmFeedback=%8 gears=%9", ws.EngineGetLoad(), ws.EngineGetPeakTorqueState(), ws.EngineGetPeakTorque(), ws.EngineGetPeakPowerState(), ws.EngineGetPeakPower(), ws.GearboxGetEfficiencyState(), ws.GearboxGetEfficiency(), ws.EngineGetRPMFeedback(), ws.GearboxGearsCount());
    PrintFormat("[ACR] DRIVE rpm idle=%1 peakTorque=%2 peakPower=%3 max=%4", ws.EngineGetRPMIdle(), ws.EngineGetRPMPeakTorque(), ws.EngineGetRPMPeakPower(), ws.EngineGetRPMMax());
   }
  }
  // Hand back: a map order ends the session; the AI driver must drive again.
  if (h.m_Ticks == 43 && m_TestDriveSession.m_Squad && m_TestDriveSession.m_Vehicle)
  {
   vector ahead = m_TestDriveSession.m_Vehicle.GetOrigin() + m_TestDriveSession.m_Vehicle.GetTransformAxis(2) * -150;
   m_DriveBackFrom = m_TestDriveSession.m_Vehicle.GetOrigin();
   m_DriveBackCar = m_TestDriveSession.m_Vehicle;
   PrintFormat("[ACR] DRIVE handback order: %1", m_TestDriveSession.m_Squad.MoveTo(ACR_Manager.Ground(ahead)));
  }
 }
 vector m_DriveBackFrom;
 IEntity m_DriveBackCar;

 // ---------------------------------------------------------------- -acrRoadsTest
 // Road routes for a few trips: length by road against the straight line.
 void TestRoads()
 {
  if (h.m_Ticks != 5) return;
  array<vector> from = {"1365 0 2857", "1236 0 2795", "1236 0 2795", "1445 0 2367", "1365 0 2857", "1365 0 2857", "1365 0 2857", "1365 0 2857"};
  array<vector> to = {"1305 0 2311", "3163 0 2778", "1445 0 2367", "2580 0 2373", "1900 0 2850", "2000 0 3050", "1800 0 2600", "1700 0 3100"};
  foreach (int i, vector a : from)
  {
   array<vector> legs = {};
   int nodes, millis;
   bool ok = BBB_Roads.Route(ACR_Manager.Ground(a), ACR_Manager.Ground(to[i]), legs, nodes, millis);
   PrintFormat("[ACR] ROADS trip %1: road=%2 length=%3 m direct=%4 m hops=%5 nodes=%6 (%7 ms)", i, ok, (int)BBB_Roads.s_fLastLength, (int)vector.DistanceXZ(a, to[i]), legs.Count(), nodes, millis);
  }
  Print("[ACR] TEST COMPLETE");
 }
}
