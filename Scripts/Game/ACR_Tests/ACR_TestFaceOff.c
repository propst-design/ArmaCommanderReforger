// -acrFaceOffTest 1 (server-capable, ~60 s): the drive watch's "one of ours ahead" (queueing, not
// stuck) and "one right behind" (don't back up) go by the vehicle's own lane (Everon soak, 7 Nov:
// Humvee 1 in the Airbase motor pool was kept from backing up 64 times in 80 minutes, and it and a
// bought truck kept starting to queue in the same second). Four pairs of transport trucks, 40 m apart, drivers
// frozen (AI off) so every truck with an order stands still:
//  - nose to nose, each ordered past the other: the older one waits, the newer one doesn't wait for
//    it and backs up;
//  - a truck with one parked beside it, a little back: it backs up on its first try;
//  - a truck in the lane behind a stopped one (a convoy): it waits; the lead, with it right behind,
//    plans again instead of backing up into it;
//  - a truck with one parked beside its lane ahead: it doesn't wait for it, it backs up.
class ACR_TestFaceOff
{
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static ref array<string> s_aNames;
 static ref array<ACR_Squad> s_aSquads;
 static ref array<IEntity> s_aVehicles;
 static ACR_Squad s_Earlier; // the face-off truck earlier in the squad list (waits)
 static ACR_Squad s_Later;   // ... and the later one (gives way)
 static bool s_bEarlierQueued, s_bLaterQueued, s_bTailQueued, s_bYardQueued;
 static int s_iLaterFirstBackUp = -1;
 static int s_iEarlierStuckThen = -1;
 static bool s_bEarlierQueuedThen;
 static int s_iBoxedFirstTry = -1;
 static bool s_bBoxedBackedUpFirstTry;
 static int s_iLeadTries; // the most tries the lead truck was seen on (a re-route sets them back to 0)
 static bool s_bReady;

 static void Freeze(IEntity vehicle)
 {
  ChimeraCharacter driver = ACR_Squad.DriverOf(vehicle);
  if (!driver) return;
  AIControlComponent ai = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
  if (ai && ai.IsAIActivated()) ai.DeactivateAI();
 }

 // Held still: a frozen driver lets a truck roll on a slope, which reads as moving.
 static void Pin(IEntity vehicle)
 {
  Physics phys = vehicle.GetPhysics();
  if (!phys) return;
  phys.SetVelocity(vector.Zero);
  phys.SetAngularVelocity(vector.Zero);
 }

 static void Face(IEntity vehicle, float yaw)
 {
  vehicle.SetYawPitchRoll(Vector(yaw, 0, 0));
  vehicle.Update();
  Pin(vehicle);
 }

 static void Spawn(ACR_TestHarness h, ACR_Manager manager, string name, string at, float yaw)
 {
  manager.SpawnUnit(name, "", TRUCK, true, "", h.Ground(at), h.Ground(at));
  foreach (IEntity vehicle, string crewName : manager.m_PendingCrewNames)
   if (crewName == name && vehicle) Face(vehicle, yaw);
 }

 static ACR_Squad Named(string name)
 {
  int i = s_aNames.Find(name);
  if (i < 0) return null;
  return s_aSquads[i];
 }

 static IEntity VehicleNamed(string name)
 {
  int i = s_aNames.Find(name);
  if (i < 0) return null;
  return s_aVehicles[i];
 }

 static string State(string name)
 {
  ACR_Squad squad = Named(name);
  IEntity vehicle = VehicleNamed(name);
  if (!squad || !vehicle) return name + ": missing";
  return string.Format("%1: order='%2' queueing=%3 tries=%4 backups=%5 unsticking=%6 at %7", name, squad.m_Order, squad.Queueing(), squad.UnstickTries(), squad.m_iStuckCount, squad.IsUnsticking(), vehicle.GetOrigin());
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
   ACR_Squad.s_fStallSeconds = 8;
   s_aNames = {"Nose North", "Nose South", "Boxed Truck", "Beside Truck", "Lead Truck", "Tail Truck", "Yard Truck", "Parked Truck"};
   s_aSquads = {};
   s_aVehicles = {};
  }
  if (tick == 5)
  {
   // Yaw 0 faces north (+z). "Nose North" stands north of "Nose South" and faces it.
   Spawn(h, manager, "Nose North", "1360 0 2800", 180);
   Spawn(h, manager, "Nose South", "1360 0 2785", 0);
   Spawn(h, manager, "Boxed Truck", "1400 0 2795", 0);
   Spawn(h, manager, "Beside Truck", "1405.5 0 2792", 0);
   Spawn(h, manager, "Lead Truck", "1440 0 2800", 0);
   Spawn(h, manager, "Tail Truck", "1440 0 2791", 0);
   Spawn(h, manager, "Yard Truck", "1480 0 2795", 0);
   Spawn(h, manager, "Parked Truck", "1487 0 2805", 0);
  }
  if (tick == 14)
  {
   foreach (string name : s_aNames)
   {
    ACR_Squad squad = h.NamedSquad(manager, name);
    IEntity vehicle;
    if (squad) vehicle = ACR_Transport.CrewedVehicle(squad);
    if (!vehicle) { ACR_TestRegress.Abort("faceoff", name + " or its crew is missing"); return; }
    s_aSquads.Insert(squad);
    s_aVehicles.Insert(vehicle);
    Freeze(vehicle);
   }
   // Turned as asked (the checks depend on it).
   float northZ = VehicleNamed("Nose North").GetTransformAxis(2)[2];
   float southZ = VehicleNamed("Nose South").GetTransformAxis(2)[2];
   if (northZ > -0.9 || southZ < 0.9) { ACR_TestRegress.Abort("faceoff", string.Format("the face-off trucks aren't nose to nose (facing z %1 and %2)", northZ, southZ)); return; }
   Named("Nose North").MoveTo(h.Ground("1360 0 2550"));
   Named("Nose South").MoveTo(h.Ground("1360 0 3050"));
   Named("Boxed Truck").MoveTo(h.Ground("1400 0 3050"));
   Named("Lead Truck").MoveTo(h.Ground("1440 0 3050"));
   Named("Tail Truck").MoveTo(h.Ground("1440 0 3050"));
   Named("Yard Truck").MoveTo(h.Ground("1480 0 3050"));
   int north = manager.m_Squads.Find(Named("Nose North"));
   int south = manager.m_Squads.Find(Named("Nose South"));
   s_Earlier = Named("Nose North");
   s_Later = Named("Nose South");
   if (south < north) { s_Earlier = Named("Nose South"); s_Later = Named("Nose North"); }
   PrintFormat("[ACR] FACEOFF earlier in the squad list: %1 (%2), later: %3 (%4)", s_Earlier.GetName(), Math.Min(north, south), s_Later.GetName(), Math.Max(north, south));
   s_bReady = true;
  }
  if (!s_bReady || tick <= 14) return;
  foreach (int i, ACR_Squad squad : s_aSquads)
  {
   IEntity vehicle = s_aVehicles[i];
   if (!squad || !vehicle || squad.IsUnsticking()) continue;
   Freeze(vehicle);
   Pin(vehicle);
  }
  // Face-off: what the earlier one was doing when the later one first backed up.
  if (s_iLaterFirstBackUp < 0)
  {
   if (s_Earlier.Queueing()) s_bEarlierQueued = true;
   if (s_Later.Queueing()) s_bLaterQueued = true;
   if (s_Later.m_iStuckCount > 0)
   {
    s_iLaterFirstBackUp = tick;
    s_iEarlierStuckThen = s_Earlier.m_iStuckCount;
    s_bEarlierQueuedThen = s_bEarlierQueued;
   }
  }
  ACR_Squad boxed = Named("Boxed Truck");
  if (s_iBoxedFirstTry < 0 && boxed.UnstickTries() > 0)
  {
   s_iBoxedFirstTry = tick;
   s_bBoxedBackedUpFirstTry = boxed.m_iStuckCount >= boxed.UnstickTries();
  }
  if (Named("Tail Truck").Queueing()) s_bTailQueued = true;
  if (Named("Lead Truck").UnstickTries() > s_iLeadTries) s_iLeadTries = Named("Lead Truck").UnstickTries();
  if (Named("Yard Truck").Queueing()) s_bYardQueued = true;
  if (tick % 5 == 0)
  {
   foreach (string name : s_aNames) PrintFormat("[ACR] FACEOFF t=%1 %2", tick, State(name));
  }
  if (tick == 40)
  {
   string faceOff = string.Format("later first backed up at t=%1 (queued before: %2); earlier queued: %3, its back-ups then: %4 | %5 | %6", s_iLaterFirstBackUp, s_bLaterQueued, s_bEarlierQueuedThen, s_iEarlierStuckThen, State(s_Earlier.GetName()), State(s_Later.GetName()));
   ACR_TestRegress.Check("faceoff: nose to nose, the older truck waits and the newer one backs up instead of waiting too", s_iLaterFirstBackUp > 0 && !s_bLaterQueued && s_bEarlierQueuedThen && s_iEarlierStuckThen == 0, faceOff);
   ACR_TestRegress.Check("faceoff: a truck parked beside, not behind, doesn't stop a back-up", s_iBoxedFirstTry > 0 && s_bBoxedBackedUpFirstTry, string.Format("first try at t=%1, backed up on it: %2 | %3", s_iBoxedFirstTry, s_bBoxedBackedUpFirstTry, State("Boxed Truck")));
   ACR_TestRegress.Check("faceoff: a truck in the lane behind a stopped one waits (a convoy)", s_bTailQueued && Named("Tail Truck").m_iStuckCount == 0, State("Tail Truck"));
   ACR_TestRegress.Check("faceoff: with a truck right behind, the lead plans again instead of backing up into it", s_iLeadTries > 0 && Named("Lead Truck").m_iStuckCount == 0, string.Format("most tries seen %1 | %2", s_iLeadTries, State("Lead Truck")));
   ACR_TestRegress.Check("faceoff: a truck parked beside the lane ahead isn't waited for", !s_bYardQueued && Named("Yard Truck").m_iStuckCount > 0, State("Yard Truck"));
   ACR_TestRegress.Complete("faceoff", 5);
  }
  if (tick == 42) Print("[ACR] TEST COMPLETE");
 }
}
