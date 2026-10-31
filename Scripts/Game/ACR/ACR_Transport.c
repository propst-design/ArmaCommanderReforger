// Transport helpers for squads: who is riding in a vehicle and which vehicle a
// squad crews. Seats and the "settled" check are in BBB_Transport.
class ACR_Transport
{
 static const float LZ_RADIUS = 100;      // queued unload triggers within this of the LZ

 static IEntity VehicleOf(IEntity body) { return BBB_Transport.VehicleOf(body); }
 static ECompartmentType SeatOf(IEntity body) { return BBB_Transport.SeatOf(body); }

 // A commander seat: the engine calls it CARGO, but it's wired to a commander sight
 // turret (LAV-25). Its occupant is crew, not a passenger.
 static bool CommanderSeat(BaseCompartmentSlot slot)
 {
  return slot && slot.GetType() == ECompartmentType.CARGO && !slot.GetAutoConnectTurret().IsEmpty();
 }

 // Riding in the back (a cargo seat that isn't a commander seat).
 static bool Passenger(IEntity body)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(body);
  if (!character || !character.IsInVehicle() || !character.GetCompartmentAccessComponent()) return false;
  BaseCompartmentSlot slot = character.GetCompartmentAccessComponent().GetCompartment();
  return slot && slot.GetType() == ECompartmentType.CARGO && !CommanderSeat(slot);
 }
 static bool Settled(IEntity vehicle, out float height, out float speed) { return BBB_Transport.Settled(vehicle, height, speed); }

 // Squads with soldiers in the passenger (cargo) seats of this vehicle.
 static void PassengerSquads(ACR_Manager manager, IEntity vehicle, array<ACR_Squad> result)
 {
  result.Clear();
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (VehicleOf(body) == vehicle && Passenger(body))
    {
     result.Insert(squad);
     break;
    }
   }
  }
 }

 // The squad crewing a vehicle (driver/pilot/gunners), if the squad is one.
 static IEntity CrewedVehicle(ACR_Squad squad)
 {
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   IEntity vehicle = VehicleOf(body);
   if (vehicle && !Passenger(body)) return vehicle;
  }
  return null;
 }

 // ---------------------------------------------------------------- vehicle horn governor
 static const float MAX_HONK_MS = 800.0;        // max continuous AI honk duration (ms)
 static const float HONK_COOLDOWN_MS = 5000.0;   // silence cooldown after an AI honk (ms)
 protected static ref map<IEntity, float> s_mHonkStart = new map<IEntity, float>();
 protected static ref map<IEntity, float> s_mHonkCooldown = new map<IEntity, float>();
 static void ResetHorns() { s_mHonkStart.Clear(); s_mHonkCooldown.Clear(); }
 protected static ref array<IEntity> s_aHornScanVehicles = new array<IEntity>();

 // Mutes the vehicle horn input context and resets the vehicle's replicated HornState audio signal.
 static void MuteVehicleHorn(IEntity vehicle, ChimeraCharacter driver = null)
 {
  if (driver)
  {
   CharacterControllerComponent controller = driver.GetCharacterController();
   if (controller)
   {
    CharacterInputContext input = controller.GetInputContext();
    if (input && input.GetVehicleHorn() != 0) input.SetVehicleHorn(0);
   }
  }
  if (vehicle)
  {
   SignalsManagerComponent signals = SignalsManagerComponent.Cast(vehicle.FindComponent(SignalsManagerComponent));
   if (signals)
   {
    int sig = signals.FindSignal("HornState");
    if (sig >= 0 && signals.GetSignalValue(sig) > 0)
     signals.SetSignalValue(sig, 0.0);
   }
  }
 }

 // Limits AI driver horn duration to 0.8 s, suppresses honks when parked/stopped,
 // prevents infinite horn spam with a 5 s cooldown, and mutes vacant/incapacitated vehicles.
 static void SanitizeVehicleHorn(IEntity vehicle, bool forceMute = false)
 {
  if (!vehicle) return;
  ACR_Vehicles.Register(vehicle);
  CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
  if (!car) return;

  BaseCompartmentSlot pilot = car.GetPilotCompartmentSlot();
  ChimeraCharacter driver = null;
  if (pilot) driver = ChimeraCharacter.Cast(pilot.GetOccupant());

  float now = GetGame().GetWorld().GetWorldTime();

  // 1. Vacant or incapacitated driver: immediately silence the vehicle
  if (!driver || !ACR_Respawn.IsAlive(driver) || driver.GetCharacterController().IsUnconscious())
  {
   MuteVehicleHorn(vehicle, driver);
   s_mHonkStart.Remove(vehicle);
   return;
  }

  // 2. Human player driver: do not interfere with human controls
  CharacterControllerComponent controller = driver.GetCharacterController();
  if (!controller) return;
  if (controller.IsPlayerControlled() || ACR_Squad.FindController(driver) > 0)
  {
   s_mHonkStart.Remove(vehicle);
   s_mHonkCooldown.Remove(vehicle);
   return;
  }

  // 3. AI Driver input context
  CharacterInputContext input = controller.GetInputContext();
  if (!input) return;

  // 4. Force mute requested (e.g. Halt, StuckHold, EndUnstick, or parked/stopped)
  if (forceMute)
  {
   MuteVehicleHorn(vehicle, driver);
   s_mHonkCooldown.Set(vehicle, now + HONK_COOLDOWN_MS);
   s_mHonkStart.Remove(vehicle);
   return;
  }

  bool isHonking = (input.GetVehicleHorn() != 0);

  // 5. Cooldown enforcement: suppress repeated engine-AI honk spam
  float cooldownUntil;
  if (s_mHonkCooldown.Find(vehicle, cooldownUntil))
  {
   if (now < cooldownUntil)
   {
    if (isHonking)
    {
     input.SetVehicleHorn(0);
     MuteVehicleHorn(vehicle, driver);
    }
    return;
   }
   else
   {
    s_mHonkCooldown.Remove(vehicle);
   }
  }

  if (isHonking)
  {
   Physics phys = vehicle.GetPhysics();
   float speed = 0;
   if (phys) speed = phys.GetVelocity().Length();

   float startTime;
   if (!s_mHonkStart.Find(vehicle, startTime))
   {
    s_mHonkStart.Set(vehicle, now);
   }
   else if (now - startTime >= MAX_HONK_MS || (speed < 0.5 && now - startTime >= 500.0))
   {
    MuteVehicleHorn(vehicle, driver);
    s_mHonkCooldown.Set(vehicle, now + HONK_COOLDOWN_MS);
    s_mHonkStart.Remove(vehicle);
   }
  }
  else
  {
   s_mHonkStart.Remove(vehicle);
  }
 }

 // Per-frame horn watchdog and audio governor (runs on all machines in EOnPostFrame,
 // right after simulation and before audio rendering).
 static void UpdateHorns()
 {
  ACR_Vehicles.Living(s_aHornScanVehicles);
  float now = GetGame().GetWorld().GetWorldTime();

  for (int i = s_aHornScanVehicles.Count() - 1; i >= 0; i--)
  {
   IEntity vehicle = s_aHornScanVehicles[i];
   if (!vehicle) continue;

   SignalsManagerComponent signals = SignalsManagerComponent.Cast(vehicle.FindComponent(SignalsManagerComponent));
   int sig = -1;
   float hornVal = 0;
   if (signals)
   {
    sig = signals.FindSignal("HornState");
    if (sig >= 0) hornVal = signals.GetSignalValue(sig);
   }

   CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
   ChimeraCharacter driver = null;
   if (car)
   {
    BaseCompartmentSlot pilot = car.GetPilotCompartmentSlot();
    if (pilot) driver = ChimeraCharacter.Cast(pilot.GetOccupant());
   }

   CharacterControllerComponent controller = null;
   CharacterInputContext input = null;
   if (driver)
   {
    controller = driver.GetCharacterController();
    if (controller) input = controller.GetInputContext();
   }

   bool isHonking = (hornVal > 0);
   if (!isHonking && input && input.GetVehicleHorn() != 0) isHonking = true;

   if (!isHonking)
   {
    s_mHonkStart.Remove(vehicle);
    continue;
   }

   // 1. Vacant or incapacitated driver: immediately silence the vehicle
   if (!driver || !controller || !ACR_Respawn.IsAlive(driver) || controller.IsUnconscious() || controller.IsDead())
   {
    if (sig >= 0 && hornVal > 0) signals.SetSignalValue(sig, 0.0);
    if (input && input.GetVehicleHorn() != 0) input.SetVehicleHorn(0);
    s_mHonkStart.Remove(vehicle);
    s_mHonkCooldown.Remove(vehicle);
    continue;
   }

   // 2. Human player driver: do not interfere with human controls
   if (controller.IsPlayerControlled() || ACR_Squad.FindController(driver) > 0)
   {
    s_mHonkStart.Remove(vehicle);
    s_mHonkCooldown.Remove(vehicle);
    continue;
   }

   // 3. AI driver cooldown enforcement: suppress continuous honk loops
   float cooldownUntil;
   if (s_mHonkCooldown.Find(vehicle, cooldownUntil))
   {
    if (now < cooldownUntil)
    {
     if (sig >= 0 && hornVal > 0) signals.SetSignalValue(sig, 0.0);
     if (input && input.GetVehicleHorn() != 0) input.SetVehicleHorn(0);
     continue;
    }
    else
    {
     s_mHonkCooldown.Remove(vehicle);
    }
   }

   // 4. AI driver active honk duration governor
   float startTime;
   if (!s_mHonkStart.Find(vehicle, startTime))
   {
    s_mHonkStart.Set(vehicle, now);
   }
   else
   {
    Physics phys = vehicle.GetPhysics();
    float speed = 0;
    if (phys) speed = phys.GetVelocity().Length();

    if (now - startTime >= MAX_HONK_MS || (speed < 0.5 && now - startTime >= 500.0))
    {
     if (sig >= 0 && hornVal > 0) signals.SetSignalValue(sig, 0.0);
     if (input && input.GetVehicleHorn() != 0) input.SetVehicleHorn(0);
     s_mHonkCooldown.Set(vehicle, now + HONK_COOLDOWN_MS);
     s_mHonkStart.Remove(vehicle);
    }
   }
  }
 }

 // Prunes deleted entities from internal honk tracking maps.
 static void PruneHornMaps()
 {
  for (int i = s_mHonkStart.Count() - 1; i >= 0; i--)
   if (!s_mHonkStart.GetKey(i)) s_mHonkStart.RemoveElement(i);
  for (int j = s_mHonkCooldown.Count() - 1; j >= 0; j--)
   if (!s_mHonkCooldown.GetKey(j)) s_mHonkCooldown.RemoveElement(j);
 }
}
