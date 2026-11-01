// Arma 3's "gunner drives": a player in a vehicle turret with an AI driver steers the
// vehicle with WASD. The client sends its keys to the server, which parks the driver's
// AI and feeds throttle/steering/brake straight into the vehicle simulation.
// A new map order for the squad, leaving the turret, or 20 s without input hands the
// vehicle back to the AI.
class ACR_GunnerDrive
{
 static const float IDLE_RELEASE_MS = 20000;
 static const float SHIFT_COOLDOWN_MS = 1500; // the gearbox's own clutch needs time to couple after a shift


 IEntity m_Vehicle;
 ChimeraCharacter m_Driver;
 ACR_Squad m_Squad;
 int m_PlayerId;
 float m_Throttle;
 float m_Steer;
 // Holding W eases the throttle in (1 Oct live test: a full-throttle start put the
 // Humvee into a barrier before the gunner could steer, and the crash killed the driver).
 static const float THROTTLE_START = 0.3;
 static const float THROTTLE_RAMP_PER_S = 0.3; // full throttle after about 2.3 s
 float m_fThrottleRamp;
 float m_fLastApply = -1;
 // Steering also eases in and out (a key is all-or-nothing).
 static const float STEER_RATE_PER_S = 2.5;
 float m_fSteerNow;
 float m_LastInput;
 float m_LastShift;
 float m_LastDiag;
 float m_LastFineDiag;
 // Last inputs written; re-written before every physics step (see Reapply).
 float m_SetThrottle;
 float m_SetBrake;
 float m_SetSteer;
 float m_SetClutch = 1;
 bool m_bHasSet;
 int m_Overrides;
 int m_Steps;
 int m_StepOverrides; // inputs found changed right before a physics step
 int m_StepWrites;
 float m_MaxSlip;
 string m_SeenOverride;
 static int s_iSimulateCalls;
 static int s_iFixedCalls;
 int m_FixedOverrides;
 static int s_iPostFixedCalls;
 bool m_AIWasOn;
 bool m_VehicleAIWasOn;
 bool m_bTest; // server test: no player in the turret
 static bool s_bParkVehicleAI = false;
 static bool s_bParkDriverAI = true;
 static bool s_bSlipClutch = false;
 static bool s_bOwnGearbox = true;
 static bool s_bHillStart = true; // hill start, lugging down-shift and hold (test switches it off for a before/after run)
 static const float HILL_START_SPEED = 3;       // m/s: slip the clutch below this until the engine is in its torque band
 static const float HILL_START_MIN_CLUTCH = 0.2;
 static const float HOLD_AFTER_DOWNSHIFT_MS = 6000;
 static const float HILL_PITCH = 5;          // degrees nose-up in the direction of travel
 static const float HILL_HOLD_MS = 2500;
 float m_fHillMovingAt;
 static const float ROLLBACK_BRAKE_SPEED = 2.5; // m/s the wrong way before the brake comes on
 float m_LastDownshift = -100000;
 static int s_iShifts; // test: shifts made
 // The vehicle's controller keeps feeding the (parked) driver seat's empty input into
 // the simulation every physics step: throttle and steering went to 0 on every other
 // step, so the wheels stuttered and the vehicle crawled. Unhooking the driver seat
 // from the controller for the session stops that (restored in End).
 static bool s_bDetachPilot = true;
 PilotCompartmentSlot m_DetachedPilot;
 static bool s_bFixedOnly = false; // test variant: write inputs only in the fixed (physics) frame
 float m_fClutch = 1;
 string m_EndReason;

 // The AI gunner of one of the squad's vehicles (null if none, or a player holds it).
 // Test/diagnostic: every turret seat of a vehicle and its parts, and who sits there.
 static string DescribeTurrets(IEntity vehicle)
 {
  string text = "";
  array<IEntity> parts = {vehicle};
  IEntity child = vehicle.GetChildren();
  while (child)
  {
   parts.Insert(child);
   child = child.GetSibling();
  }
  foreach (IEntity part : parts)
  {
   SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(part.FindComponent(SCR_BaseCompartmentManagerComponent));
   if (!compartments) continue;
   array<BaseCompartmentSlot> slots = {};
   compartments.GetCompartments(slots);
   foreach (BaseCompartmentSlot slot : slots)
   {
    string who = "empty";
    if (slot.GetOccupant()) who = FilePath.StripPath(slot.GetOccupant().GetPrefabData().GetPrefabName());
    text += string.Format(" [%1 %2: %3]", FilePath.StripPath(part.GetPrefabData().GetPrefabName()), typename.EnumToString(ECompartmentType, slot.GetType()), who);
   }
  }
  return text;
 }

 // Read from each soldier's own seat, not the vehicle's compartment list: modded vehicles
 // (Cold War pack, RHS...) often put the turret on a child entity with its own compartment
 // manager, which the root's list doesn't include. A gun with ammunition beats a sight turret.
 static ChimeraCharacter SquadGunner(ACR_Squad squad)
 {
  if (!squad) return null;
  ChimeraCharacter fallback;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter gunner = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!gunner || !gunner.IsInVehicle() || !gunner.GetCharacterController() || gunner.GetCharacterController().IsDead()) continue;
   if (!gunner.GetCompartmentAccessComponent()) continue;
   BaseCompartmentSlot turret = gunner.GetCompartmentAccessComponent().GetCompartment();
   if (!turret || turret.GetType() != ECompartmentType.TURRET || turret.GetOccupant() != gunner) continue;
   if (ACR_Squad.FindController(gunner)) continue;
   if (BBB_VehicleHelpers.TurretHasAmmo(turret)) return gunner;
   if (!fallback) fallback = gunner;
  }
  return fallback;
 }

 // Vehicle whose turret the body sits in (null if none).
 static IEntity TurretVehicleOf(IEntity body)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(body);
  if (!character || !character.GetCompartmentAccessComponent()) return null;
  BaseCompartmentSlot slot = character.GetCompartmentAccessComponent().GetCompartment();
  if (!slot || slot.GetType() != ECompartmentType.TURRET) return null;
  return slot.GetVehicle();
 }

 // Vehicle the body may drive from its turret (null if not a turret seat, no AI driver, ...).
 static IEntity DrivableFrom(IEntity body, out ChimeraCharacter driver)
 {
  driver = null;
  ChimeraCharacter character = ChimeraCharacter.Cast(body);
  if (!character) return null;
  CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
  if (!access) return null;
  BaseCompartmentSlot slot = access.GetCompartment();
  if (!slot || slot.GetType() != ECompartmentType.TURRET) return null;
  IEntity vehicle = slot.GetVehicle();
  if (!vehicle) return null;
  CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
  if (!car || !car.GetSimulation()) return null;
  PilotCompartmentSlot pilot = car.GetPilotCompartmentSlot();
  if (!pilot) return null;
  driver = ChimeraCharacter.Cast(pilot.GetOccupant());
  if (!driver || !driver.GetCharacterController() || driver.GetCharacterController().IsDead()) return null;
  if (ACR_Squad.FindController(driver)) return null; // a player is driving
  return vehicle;
 }

 void Begin()
 {
  m_Squad.Halt("Driven by gunner", ACR_EOrderState.DRIVEN);
  AIControlComponent ai = AIControlComponent.Cast(m_Driver.FindComponent(AIControlComponent));
  if (s_bParkDriverAI && ai && ai.IsAIActivated())
  {
   ai.DeactivateAI();
   m_AIWasOn = true;
  }
  // Parked mid-honk (at a soldier in its way), the AI never lets go of the horn.
  if (m_Vehicle) ACR_Transport.SanitizeVehicleHorn(m_Vehicle, true);
  CharacterInputContext input = m_Driver.GetCharacterController().GetInputContext();
  if (input && input.GetVehicleHorn() != 0) input.SetVehicleHorn(0);
  // The vehicle has its own AI (it does the actual driving for an AI driver). Left on,
  // it keeps braking and centring the wheel against the gunner's input every frame:
  // that felt like a stuck handbrake, worst when turning.
  AIControlComponent vehicleAI = AIControlComponent.Cast(m_Vehicle.FindComponent(ChimeraAIVehicleControlComponent));
  if (s_bParkVehicleAI && vehicleAI && vehicleAI.IsAIActivated())
  {
   vehicleAI.DeactivateAI();
   m_VehicleAIWasOn = true;
  }
  CarControllerComponent car = Car();
  if (car && s_bDetachPilot)
  {
   m_DetachedPilot = car.GetPilotCompartmentSlot();
   car.SetPilotCompartmentSlot(null);
   PrintFormat("[ACR] GUNNER pilot slot detached: %1", m_DetachedPilot != null);
  }
  if (car)
  {
   car.SetPersistentHandBrake(false);
   if (!car.IsEngineOn()) car.ForceStartEngine();
  }
 }

 CarControllerComponent Car()
 {
  if (!m_Vehicle) return null;
  return CarControllerComponent.Cast(m_Vehicle.FindComponent(CarControllerComponent));
 }

 // Nose up in the direction the keys drive (W forwards, S backwards).
 bool Uphill()
 {
  float travel = 1;
  if (m_Throttle < 0) travel = -1;
  return m_Vehicle.GetYawPitchRoll()[1] * travel > HILL_PITCH;
 }

 // Forward speed in m/s (negative = rolling backwards).
 float ForwardSpeed()
 {
  Physics physics = m_Vehicle.GetPhysics();
  if (!physics) return 0;
  return vector.Dot(physics.GetVelocity(), m_Vehicle.GetTransformAxis(2));
 }

 // True while the body still sits in a turret of our vehicle with our driver at the wheel.
 // (DrivableFrom can't be used once the session runs: the driver seat is unhooked from
 // the controller then, so it no longer finds a driver and every session ended at once.)
 bool StillGunning(IEntity body)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(body);
  if (!character || !m_Vehicle || !m_Driver) return false;
  CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
  if (!access) return false;
  BaseCompartmentSlot slot = access.GetCompartment();
  if (!slot || slot.GetType() != ECompartmentType.TURRET || slot.GetVehicle() != m_Vehicle) return false;
  PilotCompartmentSlot pilot = m_DetachedPilot;
  if (!pilot)
  {
   CarControllerComponent car = Car();
   if (car) pilot = car.GetPilotCompartmentSlot();
  }
  return pilot && pilot.GetOccupant() == m_Driver;
 }

 // Returns false when the session should end.
 bool Apply(float now)
 {
  if (!m_Vehicle || !m_Driver || m_Driver.GetCharacterController().IsDead()) { m_EndReason = "vehicle or driver lost"; return false; }
  PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(m_PlayerId);
  ChimeraCharacter driver;
  if (!m_bTest && (!pc || !StillGunning(pc.GetControlledEntity()))) { m_EndReason = "gunner left"; return false; }
  if (now - m_LastInput > IDLE_RELEASE_MS) { m_EndReason = "idle"; return false; }
  if (m_Squad.m_Group)
  {
   array<AIWaypoint> orders = {};
   m_Squad.m_Group.GetWaypoints(orders);
   if (!orders.IsEmpty()) { m_EndReason = "new order"; return false; }
  }
  CarControllerComponent car = Car();
  if (!car) { m_EndReason = "no car controller"; return false; }
  VehicleWheeledSimulation sim = car.GetSimulation();
  if (!sim) { m_EndReason = "no simulation"; return false; }
  if (m_Throttle != 0 && !car.IsEngineOn()) car.ForceStartEngine();
  // The parked AI driver (or the game) can leave the handbrake on: the rear wheels
  // lock, the front skids sideways and the vehicle crawls up hills. Keep it off
  // whenever the gunner wants to move.
  if (m_Throttle != 0 || m_Steer != 0) car.SetPersistentHandBrake(false);
  // A stopped vehicle's physics falls asleep and then ignores throttle.
  Physics physics = m_Vehicle.GetPhysics();
  if (physics && (m_Throttle != 0 || m_Steer != 0) && !physics.IsActive()) physics.SetActive(ActiveState.ACTIVE);

  // Did anything else change the inputs since our last write (previous frame)?
  if (m_bHasSet)
  {
   m_Steps++;
   if (Math.AbsFloat(sim.GetSteering() - m_SetSteer) > 0.01 || Math.AbsFloat(sim.GetThrottle() - m_SetThrottle) > 0.01) m_Overrides++;
  }
  float dt = 0;
  if (m_fLastApply >= 0) dt = Math.Clamp((now - m_fLastApply) / 1000, 0, 0.1);
  m_fLastApply = now;
  if (m_Throttle == 0) m_fThrottleRamp = 0;
  else if (m_fThrottleRamp == 0) m_fThrottleRamp = THROTTLE_START;
  else m_fThrottleRamp = Math.Min(1, m_fThrottleRamp + THROTTLE_RAMP_PER_S * dt);
  float steerStep = STEER_RATE_PER_S * dt;
  m_fSteerNow += Math.Clamp(m_Steer - m_fSteerNow, -steerStep, steerStep);
  int neutral = sim.GearboxGearsCount() - sim.GearboxForwardGearsCount() - 1;
  int gear = sim.GetGear();
  float speed = ForwardSpeed();
  float throttle = 0;
  float brake = 0;
  if (m_Throttle > 0)
  {
   // Still rolling back fast: stop first. Creeping back on a hill start keeps the
   // throttle (braking there cut it every other frame: the Humvee slid back at 1 m/s).
   if (speed < -ROLLBACK_BRAKE_SPEED) brake = 1;
   else
   {
    if (gear <= neutral) Shift(sim, neutral + 1, now, true);
    // Pull away in first, not whatever gear it stopped in (it set off up a hill in 2nd).
    else if (s_bHillStart && gear > neutral + 1 && Math.AbsFloat(speed) < 1) Shift(sim, neutral + 1, now, true);
    throttle = m_Throttle * m_fThrottleRamp;
   }
  }
  else if (m_Throttle < 0)
  {
   if (speed > ROLLBACK_BRAKE_SPEED) brake = 1; // still rolling forward fast: stop first
   else
   {
    if (gear >= neutral) Shift(sim, neutral - 1, now, true);
    throttle = -m_Throttle * m_fThrottleRamp;
   }
  }
  // Keys released: coast like a real driver lifting off (braking at once made every
  // tap of W and every A/D on its own feel like a stuck handbrake); hold only once slow.
  else if (Math.AbsFloat(speed) < 1.5) brake = 1;

  // Hill start (Jacob 19 Oct): pulling away uphill, hold the brake while the engine
  // revs into its torque band (at most HILL_HOLD_MS per stop), and brake any roll-back
  // while the throttle stays on. Without it the slipping clutch let the Humvee slide
  // 7 m back down a 31 % road before it bit.
  if (s_bHillStart && throttle > 0)
  {
   float travel = 1;
   if (m_Throttle < 0) travel = -1;
   float along = speed * travel;
   bool uphill = Uphill();
   if (along >= 0.5 || !uphill) m_fHillMovingAt = now;
   if (uphill && along < -0.3) brake = 1;
   else if (uphill && along < 0.5 && sim.EngineGetRPM() < sim.EngineGetRPMPeakTorque() * 0.85 && now - m_fHillMovingAt < HILL_HOLD_MS) brake = 1;
  }

  // Simple automatic gearbox with a slipping clutch, like a torque converter: with
  // the clutch locked the engine was dragged down to 1000-1500 rpm, far below its
  // power band, and the truck lugged in first gear (worse in turns, which load it).
  // Slipping lets the engine rev to its peak torque; the clutch locks as road speed
  // catches up.
  gear = sim.GetGear();
  float clutch = m_fClutch;
  float rpm = sim.EngineGetRPM();
  float idle = sim.EngineGetRPMIdle();
  float peakTorque = sim.EngineGetRPMPeakTorque();
  if (s_bOwnGearbox && throttle > 0 && gear > neutral)
  {
   // Shift on the drivetrain's rpm (what the wheels turn the engine at), not the
   // engine's own: the gearbox's automatic clutch slips when pulling away, so the
   // engine sits at the limiter long before first gear is used up. Shifting on that
   // left the clutch uncoupled and the truck lugging in the wrong gear.
   float driven = sim.EngineGetRPMFeedback();
   // One gear at a time, and only once the last shift has gone through (the gearbox
   // reports the new gear late, so shifting again on the old one skipped gears).
   bool settled = now - m_LastShift > SHIFT_COOLDOWN_MS;
   // Shift points like the vehicles' own automatic gearbox (Humvee: up near 2500,
   // down near 1200 drivetrain rpm). Up at 90% of peak power was never reached while
   // steering or climbing: play-tests stayed in first at the limiter (~30 km/h).
   // The gap leaves room for the next gear's ratio (1st->2nd drops rpm to ~0.6x),
   // so it doesn't hunt between gears.
   // Climbing (Jacob 19 Oct, Kolguyev): 2nd couldn't hold the speed 1st reached, so it
   // hunted 1-2-1 all the way up. After a down-shift for lugging, stay in the lower
   // gear a while unless the engine nears the limiter.
   float peakPower = sim.EngineGetRPMPeakPower();
   bool heldDown = s_bHillStart && now - m_LastDownshift < HOLD_AFTER_DOWNSHIFT_MS && driven < peakPower * 0.9;
   if (settled && !heldDown && driven > peakPower * 0.65 && gear < sim.GearboxGearsCount() - 1) Shift(sim, gear + 1, now, true);
   // Down while the clutch is coupled (engine and drivetrain agree): right after an
   // up-shift the drivetrain rpm reads low until the clutch bites, and shifting back
   // down on that made it hunt. A clutch still slipping 3 s after the shift is no
   // longer that: it is lugging up a hill, so shift down anyway.
   else if (settled && driven < peakTorque * 0.6 && gear > neutral + 1 && (Math.AbsFloat(rpm - driven) < 250 || (s_bHillStart && now - m_LastShift > 3000)))
   {
    Shift(sim, gear - 1, now, true);
    m_LastDownshift = now;
   }
  }
  if (s_bSlipClutch && throttle > 0 && peakTorque > idle)
   clutch = Math.Min(m_fClutch, Math.Clamp((rpm - idle) / (peakTorque - idle), 0.25, 1));
  // Hill start (Jacob 19 Oct: a slope the vehicle can do forwards it only managed in
  // reverse). Pulling away uphill the gearbox's clutch held the engine at idle under
  // full throttle (650 rpm, no torque), and only reverse's lower gearing got it moving.
  // Slipping the clutch while slow lets the engine rev into its torque band first.
  // Uphill only: on the flat it cost about a second pulling away.
  else if (s_bHillStart && throttle > 0 && Uphill() && Math.AbsFloat(speed) < HILL_START_SPEED && peakTorque > idle && rpm < peakTorque)
   clutch = Math.Min(m_fClutch, Math.Clamp((rpm - idle) / (peakTorque - idle), HILL_START_MIN_CLUTCH, 1));
  m_SetClutch = clutch;

  if (!s_bFixedOnly)
  {
   sim.SetClutch(clutch);
   sim.SetThrottle(throttle);
   sim.SetBreak(brake, false);
  }
  if (brake == 0 && sim.IsHandbrakeOn()) car.SetPersistentHandBrake(false);
  // Less steering lock at speed, so a tap of A/D doesn't spin the vehicle out.
  float steerScale = Math.Clamp(1 - Math.AbsFloat(speed) / 25, 0.35, 1);
  if (!s_bFixedOnly) sim.SetSteering(m_fSteerNow * steerScale);
  m_SetThrottle = throttle;
  m_SetBrake = brake;
  m_SetSteer = m_fSteerNow * steerScale;
  m_bHasSet = true;
  if (m_bTest && now - m_LastFineDiag > 150 && m_Throttle != 0)
  {
   m_LastFineDiag = now;
   PrintFormat("[ACR] DRIVE fine thr=%1 steer=%2 gear=%3 rpm=%4 driven=%5 speed=%6", sim.GetThrottle(), sim.GetSteering(), sim.GetGear(), (int)sim.EngineGetRPM(), (int)sim.EngineGetRPMFeedback(), speed);
  }
  // Diagnostics while this is being tuned in play-tests (every 2 s per vehicle).
  if (now - m_LastDiag > 2000)
  {
   m_LastDiag = now;
   AIControlComponent vehicleAI = AIControlComponent.Cast(m_Vehicle.FindComponent(ChimeraAIVehicleControlComponent));
   AIControlComponent driverAI = AIControlComponent.Cast(m_Driver.FindComponent(AIControlComponent));
   PrintFormat("[ACR] GUNNER DIAG keys thr=%1 steer=%2 | sim thr=%3 steer=%4 brake=%5 hb=%6 carHb=%7 persistHb=%8 gear=%9", m_Throttle, m_Steer, sim.GetThrottle(), sim.GetSteering(), sim.GetBrake(), sim.IsHandbrakeOn(), car.GetHandBrake(), car.GetPersistentHandBrake(), sim.GetGear());
   PrintFormat("[ACR] GUNNER DIAG speed=%1 rpm=%2 engine=%3 vehicleAI=%4 driverAI=%5 inputs changed by something else in %6 of %7 frames, clutch %8 driven=%9", speed, (int)sim.EngineGetRPM(), car.IsEngineOn(), vehicleAI != null && vehicleAI.IsAIActivated(), driverAI != null && driverAI.IsAIActivated(), m_Overrides, m_Steps, sim.GetClutch(), (int)sim.EngineGetRPMFeedback());
   PrintFormat("[ACR] GUNNER DIAG steps: inputs changed before %1 of %2 physics steps, max wheel slip %3 (fixed-frame %8), simulate/fixed/postfixed events %4/%5/%6, last foreign inputs: %7", m_StepOverrides, m_StepWrites, m_MaxSlip, s_iSimulateCalls, s_iFixedCalls, s_iPostFixedCalls, m_SeenOverride, m_FixedOverrides);
   m_FixedOverrides = 0;
   m_Overrides = 0;
   m_Steps = 0;
   m_StepOverrides = 0;
   m_StepWrites = 0;
   m_MaxSlip = 0;
  }
  return true;
 }

 // Before every physics step: something in the vehicle (its controller, reading the
 // parked driver's empty input) writes its own inputs between our once-per-frame
 // writes, so the wheel twitched between centre and the gunner's steer and the
 // throttle pulsed (stuck in first gear, lugging). Writing again right before
 // each step makes ours the ones the physics uses.
 void Reapply(int source = 0)
 {
  if (!m_bHasSet || !m_Vehicle) return;
  if (s_bFixedOnly && source != 1) return;
  CarControllerComponent car = Car();
  if (!car || !car.GetSimulation()) return;
  VehicleWheeledSimulation sim = car.GetSimulation();
  m_StepWrites++;
  if (Math.AbsFloat(sim.GetSteering() - m_SetSteer) > 0.01 || Math.AbsFloat(sim.GetThrottle() - m_SetThrottle) > 0.01 || Math.AbsFloat(sim.GetBrake() - m_SetBrake) > 0.01)
  {
   m_StepOverrides++;
   if (source == 1) m_FixedOverrides++;
   m_SeenOverride = string.Format("thr %1 steer %2 brake %3 clutch %4 gear %5", sim.GetThrottle(), sim.GetSteering(), sim.GetBrake(), sim.GetClutch(), sim.GetGear());
  }
  for (int w = 0; w < sim.WheelCount(); w++) m_MaxSlip = Math.Max(m_MaxSlip, Math.AbsFloat(sim.WheelGetLongitudinalSlip(w)));
  sim.SetClutch(m_SetClutch);
  sim.SetThrottle(m_SetThrottle);
  sim.SetBreak(m_SetBrake, false);
  sim.SetSteering(m_SetSteer);
 }

 void Shift(VehicleWheeledSimulation sim, int gear, float now, bool force)
 {
  if (!force && now - m_LastShift < SHIFT_COOLDOWN_MS) return;
  if (System.IsCLIParam("acrGunnerTest")) PrintFormat("[ACR] GUNNER shift %1 -> %2 rpm=%3 peakPower=%4 peakTorque=%5 max=%6", sim.GetGear(), gear, (int)sim.EngineGetRPM(), (int)sim.EngineGetRPMPeakPower(), (int)sim.EngineGetRPMPeakTorque(), (int)sim.EngineGetRPMMax());
  sim.SetGear(gear);
  m_LastShift = now;
  s_iShifts++;
 }

 void End()
 {
  CarControllerComponent car = Car();
  if (car && m_DetachedPilot) car.SetPilotCompartmentSlot(m_DetachedPilot);
  if (car && car.GetSimulation())
  {
   VehicleWheeledSimulation sim = car.GetSimulation();
   sim.SetThrottle(0);
   sim.SetSteering(0);
   sim.SetBreak(1, false);
  }
  if (m_VehicleAIWasOn && m_Vehicle)
  {
   AIControlComponent vehicleAI = AIControlComponent.Cast(m_Vehicle.FindComponent(ChimeraAIVehicleControlComponent));
   if (vehicleAI && !vehicleAI.IsAIActivated()) vehicleAI.ActivateAI();
  }
  if (m_AIWasOn && m_Driver && !ACR_Squad.FindController(m_Driver))
  {
   AIControlComponent ai = AIControlComponent.Cast(m_Driver.FindComponent(AIControlComponent));
   if (ai && !ai.IsAIActivated()) ai.ActivateAI();
  }
  if (m_Vehicle) ACR_Transport.SanitizeVehicleHorn(m_Vehicle, true);
  if (m_Squad && m_Squad.OrderState() == ACR_EOrderState.DRIVEN) m_Squad.SetOrder("Holding", ACR_EOrderState.HOLD);
 }
}
