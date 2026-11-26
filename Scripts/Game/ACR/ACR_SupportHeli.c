// Helicopter support runs (door/nose gun controller and gunship flight); split from ACR_Supports.c.

// One door/nose gun of a support helicopter. The script aims and fires it
// (the AI gunners barely engage from a moving helicopter): it picks an enemy the
// gun can reach and see, leads for the helicopter's own speed, and fires bursts.
class ACR_HeliGun : Managed
{
 BaseCompartmentSlot m_Slot;
 IEntity m_Turret;
 TurretComponent m_TurretComponent;
 TurretControllerComponent m_Controller;
 string m_sName;
 ref array<bool> m_aBins = {};  // 36 bins of 10 deg: can it reach a target at that bearing from the nose
 int m_iLastAmmo = -1;
 float m_fYawCmd;
 float m_fPitchCmd;
 float m_fYawSign = 1;
 float m_fPitchSign = 1;
 float m_fLastYawErr = 999;
 int m_iWorse;
 IEntity m_Target;
 bool m_bFiring;
 float m_fBurstEnd;
 float m_fPauseEnd;
 float m_fNextPick;
 vector m_Wander;        // slowly drifting aim error (metres at the target)
 vector m_WanderGoal;
 float m_fNextWander;
 float m_fSettleUntil;   // time to get on a new target before the first burst
 static const float WANDER_METRES = 2.5;

 static int Bin(float bearing)
 {
  int bin = Math.Round((bearing + 180) / 10);
  while (bin < 0) bin += 36;
  while (bin >= 36) bin -= 36;
  return bin;
 }
 bool CoversBearing(float bearing)
 {
  if (m_aBins.Count() != 36) return false;
  return m_aBins[Bin(bearing)];
 }
 string Describe()
 {
  string text = "";
  for (int i = 0; i < 36; i++) if (m_aBins[i]) text += (i * 10 - 180).ToString() + " ";
  return text;
 }

 int Ammo()
 {
  if (!m_Controller || !m_Controller.GetWeaponManager()) return -1;
  BaseWeaponComponent weapon = m_Controller.GetWeaponManager().GetCurrentWeapon();
  if (!weapon || !weapon.GetCurrentMuzzle()) return -1;
  return weapon.GetCurrentMuzzle().GetAmmoCount();
 }

 bool Covers(vector point)
 {
  if (!m_TurretComponent) return false;
  vector excess = m_TurretComponent.GetAimingAngleExcess(point);
  return Math.AbsFloat(excess[0]) < 0.5 && Math.AbsFloat(excess[1]) < 0.5;
 }

 IEntity Occupant()
 {
  if (!m_Slot) return null;
  return m_Slot.GetOccupant();
 }

 bool Visible(IEntity heli, vector point)
 {
  TraceParam trace = new TraceParam();
  vector from = m_Turret.GetOrigin();
  trace.Start = from + (point - from).Normalized() * 2;
  trace.End = point;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  trace.Exclude = heli;
  float fraction = GetGame().GetWorld().TraceMove(trace, null);
  if (fraction >= 0.97) return true;
  IEntity hit = trace.TraceEnt;
  return hit && (hit == m_Target || hit.GetRootParent() == m_Target);
 }

 // Where to point: chest height, led for the helicopter's own motion (bullets
 // inherit it) and a little bullet drop.
 vector LeadPoint(vector heliVelocity, IEntity target)
 {
  vector point = target.GetOrigin() + Vector(0, 1.1, 0);
  float flight = vector.Distance(point, m_Turret.GetOrigin()) / 820;
  point = point - heliVelocity * flight;
  point[1] = point[1] + 0.02 * flight * flight * 9.8 * 25;
  return point;
 }

 void StopFire()
 {
  if (m_bFiring && m_Controller) m_Controller.SetFireWeaponWanted(false);
  m_bFiring = false;
 }

 void Update(IEntity heli, vector heliVelocity, vector center, array<IEntity> enemies, float now)
 {
  IEntity gunner = Occupant();
  if (!m_Controller || !m_TurretComponent || !ACR_Respawn.IsAlive(gunner)) { StopFire(); return; }
  // The script is the gunner's brain: keep the AI from fighting over the gun.
  AIControlComponent ai = AIControlComponent.Cast(gunner.FindComponent(AIControlComponent));
  if (ai && ai.IsAIActivated()) ai.DeactivateAI();

  if (m_Target && (!ACR_Respawn.IsAlive(m_Target) || !Covers(LeadPoint(heliVelocity, m_Target)))) m_Target = null;
  if (!m_Target && now >= m_fNextPick)
  {
   m_fNextPick = now + 400;
   float best = float.MAX;
   foreach (IEntity enemy : enemies)
   {
    vector point = LeadPoint(heliVelocity, enemy);
    if (!Covers(point)) continue;
    float score = vector.Distance(enemy.GetOrigin(), center);
    if (score >= best) continue;
    if (!Visible(heli, point)) continue;
    best = score;
    m_Target = enemy;
   }
   m_iWorse = 0;
   m_fLastYawErr = 999;
   if (m_Target) m_fSettleUntil = now + Math.RandomFloat(900, 1600);
  }
  if (!m_Target) { StopFire(); return; }

  // A human gunner in a moving helicopter: the aim drifts around the target and
  // each new target takes a moment to get on. The gun's own dispersion adds on top.
  if (now >= m_fNextWander)
  {
   m_fNextWander = now + Math.RandomFloat(500, 1100);
   m_WanderGoal = Vector(Math.RandomFloat(-1, 1), Math.RandomFloat(-0.5, 0.7), Math.RandomFloat(-1, 1)) * WANDER_METRES;
  }
  m_Wander = m_Wander + (m_WanderGoal - m_Wander) * 0.05;
  vector aim = LeadPoint(heliVelocity, m_Target) + m_Wander;
  vector desired = (aim - m_Turret.GetOrigin()).Normalized();
  vector current = m_TurretComponent.GetAimingDirectionWorld();
  float errYaw = ACR_HeliRun.Wrap((Math.Atan2(desired[0], desired[2]) - Math.Atan2(current[0], current[2])) * Math.RAD2DEG);
  float errPitch = (Math.Asin(Math.Clamp(desired[1], -1, 1)) - Math.Asin(Math.Clamp(current[1], -1, 1))) * Math.RAD2DEG;
  // Closed loop on the gun's real aim; if the error keeps growing the axis sign is flipped.
  if (Math.AbsFloat(errYaw) > m_fLastYawErr + 0.2 && Math.AbsFloat(errYaw) > 5) m_iWorse++;
  else m_iWorse = 0;
  if (m_iWorse > 12)
  {
   m_fYawSign = -m_fYawSign;
   m_iWorse = 0;
   PrintFormat("[ACR] SUPPORT gun %1: flipped yaw sign", m_sName);
  }
  m_fLastYawErr = Math.AbsFloat(errYaw);
  m_fYawCmd = Math.Clamp(m_fYawCmd + m_fYawSign * errYaw * Math.DEG2RAD * 0.5, -3, 3);
  m_fPitchCmd = Math.Clamp(m_fPitchCmd + m_fPitchSign * errPitch * Math.DEG2RAD * 0.5, -1.5, 1.5);
  m_Controller.SetAimingAngles(m_fYawCmd, m_fPitchCmd);

  bool onTarget = Math.AbsFloat(errYaw) < 1.5 && Math.AbsFloat(errPitch) < 1.5;
  if (m_bFiring && now > m_fBurstEnd)
  {
   StopFire();
   m_fPauseEnd = now + Math.RandomFloat(400, 800);
  }
  else if (!m_bFiring && onTarget && now > m_fPauseEnd && now > m_fSettleUntil)
  {
   m_Controller.SetFireWeaponWanted(true);
   m_bFiring = true;
   m_fBurstEnd = now + Math.RandomFloat(700, 1300);
  }
  else if (m_bFiring && !onTarget && Math.AbsFloat(errYaw) > 6) StopFire();
 }
}

// A crewed gunship flown by script (the AI cannot fly).
//  Rocket run: one straight pass with a rocket salvo at enemies around the target
//  (or spread over the area), then a climb on the way out.
//  Loiter: the helicopter's guns are measured when it arrives, and it flies the
//  pattern that keeps the most guns on the target:
//   - side guns (UH-1H doors): a pylon-turn orbit with the gun side toward the target;
//   - nose/tail guns (Mi-8 bow and ramp PKTs): repeated attack passes, nose gun and
//     rockets on the way in, ramp gun covering the target on the way out.
class ACR_HeliRun : ACR_SupportRun
{
 static const float SPEED_RUN = 55;
 static const float SPEED_LOITER = 36;
 static const float LOITER_RADIUS = 280;
 static const float LOITER_TIME_MS = 90000;
 static const float ALT_TRANSIT = 70;
 static const float ALT_LOITER = 55;
 static const float ALT_EGRESS = 150;  // climb away after the attack (out of small-arms reach)
 static const float RUN_TARGET_RADIUS = 120; // rocket run: enemies this close to the target are aimed at
 static const float RUN_SPREAD = 35;   // rocket run with no enemy seen: spread over the area

 IEntity m_Vehicle;
 string m_sFaction;
 ACR_StationedHeli m_Home;  // flies from and back to this helipad (null = off-map helicopter, deleted after)
 static const float LIFT_HEIGHT = 30;   // m: straight up off the pad before heading out
 static const float LIFT_RATE = 4;      // m/s
 protected bool m_bLifting;
 bool m_bReturning;
 bool m_bLanded;
 protected float m_fSpeed;
 static const float FLIGHT_ACCEL = 1.5; // m/s squared: about 37 s to cruise from a hover
 static const float FLIGHT_BRAKE = 3;
 protected float m_fNextTerrain;
 protected float m_fTerrainY;
 protected float m_fTerrainSpeed = SPEED_RUN;
 protected ref array<IEntity> m_aFlightIgnore = {};

 static float ApproachSpeed(float speed, float wanted, float dt)
 {
  return speed + Math.Clamp(wanted - speed, -FLIGHT_BRAKE * dt, FLIGHT_ACCEL * dt);
 }

 // Reserve 3 s for vertical-speed response and use only 5 of the available 8 m/s climb.
 // Slow down to buy climb time when a ridge cannot be cleared at the current speed.
 static float ClimbSafeSpeed(float distance, float rise)
 {
  if (rise <= 0) return SPEED_RUN;
  return Math.Clamp((distance - 25) / (rise / 5 + 3), 0, SPEED_RUN);
 }

 protected void IgnoreFlightOccupants()
 {
  m_aFlightIgnore.Clear();
  m_aFlightIgnore.Insert(m_Vehicle);
  SCR_BaseCompartmentManagerComponent flightSeats = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  array<BaseCompartmentSlot> occupied = {};
  if (flightSeats) flightSeats.GetCompartments(occupied);
  foreach (BaseCompartmentSlot seat : occupied)
   if (seat.GetOccupant()) m_aFlightIgnore.Insert(seat.GetOccupant());
 }

 // Soldiers never block the rotor envelope (Jacob 19 Oct: a lift hovered 1 m up for 70 s
 // because its squad stood under the tail box, so they never got the get-in order).
 protected bool RotorTraceFilter(notnull IEntity e, vector start = "0 0 0", vector dir = "0 0 0")
 {
  IEntity root = e;
  for (IEntity part = e; part; part = part.GetParent())
  {
   if (ChimeraCharacter.Cast(part)) return false;
   root = part;
  }
  return !LooseGear(root);
 }

 // Nor does loose gear: weapons and kit dropped by casualties under a landing spot held a lift
 // on the ground until cleanup removed them (Everon soak 23 Oct: 6 min, 500 ms/s). Only a small
 // free-standing weapon or item counts; vehicles carry item components in their hierarchy.
 static bool LooseGear(IEntity root)
 {
  if (!root || Vehicle.Cast(root)) return false;
  if (!root.FindComponent(WeaponComponent) && !root.FindComponent(InventoryItemComponent)) return false;
  vector mins, maxs;
  root.GetBounds(mins, maxs);
  return Math.Max(maxs[0] - mins[0], Math.Max(maxs[1] - mins[1], maxs[2] - mins[2])) < 2.5;
 }

 // Last movement check, including vertical takeoff and descent. Prediction can be
 // stale when another vehicle moves in; never advance the rotor envelope through it.
 // vertical: straight up or down over a landing spot, whose ground was checked when it was
 // picked (up to 2.5 m of rise): terrain is left out, or a slope under the tail stops it short.
 protected bool RotorMoveClear(vector from, vector to, bool vertical)
 {
  TraceFlags flags = TraceFlags.WORLD | TraceFlags.ENTS;
  if (vertical) flags = TraceFlags.ENTS;
  TraceBox rotor = new TraceBox();
  rotor.Mins = Vector(-12, 2.5, -12);
  rotor.Maxs = Vector(12, 10, 12);
  rotor.Start = from;
  rotor.End = to;
  rotor.ExcludeArray = m_aFlightIgnore;
  rotor.Flags = flags;
  BaseWorld world = GetGame().GetWorld();
  if (world.TraceMove(rotor, RotorTraceFilter) < 1) { m_BlockedBy = rotor.TraceEnt; return false; }
  // A movement trace can permit an overlap already present at its start. Check
  // the destination volume too, including when a moving obstacle entered it.
  rotor.Start = to;
  if (world.TracePosition(rotor, RotorTraceFilter) < 0) { m_BlockedBy = rotor.TraceEnt; return false; }
  // The long, narrow tail extends beyond the main disk. Enclose its rotated bounds
  // in a world-aligned box rather than using a full rotor-width square at tail height.
  TraceBox tail = new TraceBox();
  vector nose = Heading(m_fBodyYaw);
  vector tailCenter = nose * -11;
  float halfX = Math.AbsFloat(nose[0]) * 6 + Math.AbsFloat(nose[2]) * 3;
  float halfZ = Math.AbsFloat(nose[2]) * 6 + Math.AbsFloat(nose[0]) * 3;
  tail.Mins = Vector(-halfX, 1, -halfZ);
  tail.Maxs = Vector(halfX, 10, halfZ);
  tail.Start = from + tailCenter;
  tail.End = to + tailCenter;
  tail.ExcludeArray = m_aFlightIgnore;
  tail.Flags = flags;
  if (world.TraceMove(tail, RotorTraceFilter) < 1) { m_BlockedBy = tail.TraceEnt; return false; }
  tail.Start = tail.End;
  if (world.TracePosition(tail, RotorTraceFilter) >= 0) return true;
  m_BlockedBy = tail.TraceEnt;
  return false;
 }

 // A blocked helicopter holds still and retries a few times a second rather than repeating
 // the rotor traces every frame (Everon soak 23 Oct: a lift held down for 6 min cost ~8 ms
 // per frame). The blocker is logged so it can be fixed at its source.
 static const float GUARD_RETRY_MS = 250;
 static const float GUARD_LOG_MS = 10000;
 protected IEntity m_BlockedBy;
 protected float m_fGuardRetryAt;
 protected float m_fGuardLogAt;

 protected void GuardFlightMove(vector previous, bool vertical = false)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fGuardRetryAt)
  {
   m_Pos = previous;
   m_fClimb = 0;
   m_fSpeed = 0;
   return;
  }
  IgnoreFlightOccupants();
  m_BlockedBy = null;
  if (RotorMoveClear(previous, m_Pos, vertical)) return;
  // An upward escape may still be clear even when forward movement is blocked.
  vector escape = previous;
  escape[1] = Math.Max(previous[1], m_Pos[1]);
  if (escape[1] > previous[1] && RotorMoveClear(previous, escape, vertical)) m_Pos = escape;
  else
  {
   m_Pos = previous;
   m_fClimb = 0;
   m_fGuardRetryAt = now + GUARD_RETRY_MS;
   if (now >= m_fGuardLogAt)
   {
    m_fGuardLogAt = now + GUARD_LOG_MS;
    string blocker = "terrain";
    if (m_BlockedBy)
    {
     blocker = m_BlockedBy.ClassName();
     if (m_BlockedBy.GetPrefabData()) blocker = blocker + " " + m_BlockedBy.GetPrefabData().GetPrefabName();
    }
    PrintFormat("[ACR] SUPPORT %1 blocked at %2 by %3", LogName(), previous, blocker);
   }
  }
  m_fSpeed = 0;
 }

 protected float FlightClearance(vector forward, float above, float now)
 {
  if (now < m_fNextTerrain) return m_fTerrainY;
  m_fNextTerrain = now + 250;
  BaseWorld world = GetGame().GetWorld();
  vector side = Vector(forward[2], 0, -forward[0]);
  float horizon = Math.Max(400, m_fSpeed * 20 + m_fSpeed * m_fSpeed / (FLIGHT_BRAKE * 2));
  m_fTerrainSpeed = SPEED_RUN;
  m_fTerrainY = m_Pos[1] - 20;
  for (float ahead = 0; ahead <= horizon; ahead += 20)
  {
   for (int lane = -1; lane <= 1; lane++)
   {
    vector probe = m_Pos + forward * ahead + side * (lane * 18);
    float surface = Math.Max(0, world.GetSurfaceY(probe[0], probe[2]));
    float rise = surface + 35 - m_Pos[1];
    m_fTerrainSpeed = Math.Min(m_fTerrainSpeed, ClimbSafeSpeed(ahead, rise));
    float time = ahead / Math.Max(10, m_fSpeed);
    m_fTerrainY = Math.Max(m_fTerrainY, surface + above - Math.Max(0, time - 3) * 5);
   }
  }
  // Sweep the whole rotor/tail envelope along the stopping corridor; terrain samples
  // alone know nothing about trees, buildings or powerline pylons. Ignore our own hull.
  TraceBox sweep = new TraceBox();
  sweep.Mins = Vector(-18, 1, -18);
  sweep.Maxs = Vector(18, 10, 18);
  sweep.Start = m_Pos;
  float reach = Math.Max(40, m_fSpeed * m_fSpeed / (FLIGHT_BRAKE * 2) + m_fSpeed * 3 + 30);
  sweep.End = m_Pos + forward * reach;
  // Seated characters are not necessarily children of the airframe in trace queries.
  // Their gear must not be mistaken for an obstacle after boarding.
  IgnoreFlightOccupants();
  sweep.ExcludeArray = m_aFlightIgnore;
  sweep.Flags = TraceFlags.ENTS;
  float hit = world.TraceMove(sweep, null);
  if (hit < 1)
  {
   if (System.IsCLIParam("acrAirLiftContactTest")) PrintFormat("[ACR] LIFTCONTACT obstacle %1 at %2, fraction %3, position %4", sweep.TraceEnt, sweep.End, hit, m_Pos);
   m_fTerrainSpeed = Math.Min(m_fTerrainSpeed, Math.Max(0, (reach * hit - 30) / (m_fSpeed / FLIGHT_BRAKE + 3)));
   m_fTerrainY = Math.Max(m_fTerrainY, m_Pos[1] + 25);
  }
  return m_fTerrainY;
 }
 bool m_bLoiter;
 bool m_bDoorGuns;   // transport version: no rockets
 bool m_bAttack;     // a content pack's attack helicopter: longer on station, more rockets
 static const float ATTACK_LOITER_MS = 120000;
 static const int ATTACK_ROCKETS = 24;
 static const float DOOR_SIDE_SWAP_MS = 30000;
 float m_fNextSideSwap;
 float m_fLoiterMs = LOITER_TIME_MS;
 float m_fClimb;     // smoothed vertical speed (m/s)
 float m_fNextAltLog;
 bool m_bAimedAtEnemy;
 vector m_Target;
 vector m_Dir;
 vector m_Pos;
 float m_fYaw;       // direction of travel
 float m_fBodyYaw;   // where the nose points (differs in a pylon turn)
 float m_fRoll;
 ResourceName m_Rocket;
 int m_iRockets;
 float m_fNextRocket;
 float m_fDiveUntil;  // nose held down a little while firing rockets
 int m_iSalvoLeft;          // loiter: rockets still to fire in the current salvo
 vector m_SalvoAim;
 int m_iPhase;       // 0 inbound, 1 on station, 2 outbound
 float m_fOrbitEnd;
 vector m_ExitDir;
 float m_fStarted;
 bool m_bDestroyed;
 ref array<ref ACR_HeliGun> m_Guns = {};
 bool m_bAnalyzed;
 bool m_bPasses;     // attack passes instead of an orbit
 float m_fSideBearing = -90; // orbit: target bearing relative to the nose
 bool m_bPassOutbound;
 float m_fNextGunLog;
 float m_fNextEnemyScan;
 float m_fNextSideCheck;
 int m_iStack;        // height band (0-2) so our helicopters don't share an altitude
 static const float STACK_STEP = 30;        // m between height bands
 static const float SEPARATION = 160;       // m: closer than this (and within 40 m height), steer apart
 static const float OPPORTUNITY_RADIUS = 450; // m around the helicopter: gunners also engage enemies here

 bool SideCanFire(float bearing)
 {
  foreach (ACR_HeliGun gun : m_Guns)
   if (gun.CoversBearing(bearing) && ACR_Respawn.IsAlive(gun.Occupant()) && gun.Ammo() != 0) return true;
  return false;
 }
 ref array<IEntity> m_aEnemies = {};

 bool Start(ACR_Supports supports, ResourceName prefab, float now)
 {
  float startDistance = 2200;
  if (m_bLoiter) startDistance = 1500;
  m_Pos = SafeStart(m_Target - m_Dir * startDistance);
  vector toward = m_Target - m_Pos;
  toward[1] = 0;
  if (toward.Length() > 1) m_Dir = toward.Normalized();
  m_Pos[1] = SafeAltitude(m_Pos, m_Dir, ALT_TRANSIT + m_iStack * STACK_STEP);
  m_fYaw = Math.Atan2(m_Dir[0], m_Dir[2]) * Math.RAD2DEG;
  m_fBodyYaw = m_fYaw;
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return false;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(m_fYaw, 0, 0), parameters.Transform);
  parameters.Transform[3] = m_Pos;
  m_Vehicle = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
  if (!m_Vehicle) return false;
  supports.m_Vehicles.Insert(m_Vehicle);
  Physics physics = m_Vehicle.GetPhysics();
  if (physics) physics.EnableGravity(false);
  // A pack's airframe seats the side's own pilots and gunners (ACR_ContentSocket.Crewman reads the owner).
  if (m_bAttack) ACR_Vehicles.SetOwner(m_Vehicle, m_sFaction);
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  array<ECompartmentType> seats = {ECompartmentType.PILOT, ECompartmentType.TURRET};
  if (compartments) compartments.SpawnDefaultOccupants(seats);
  ResourceName selectedCrew;
  if (!m_bAttack) selectedCrew = supports.ConfiguredCrewPrefab(m_sFaction, !m_bDoorGuns);
  if (!selectedCrew.IsEmpty()) supports.Station().ApplyCrewPrefab(m_Vehicle, selectedCrew);
  BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(m_Vehicle.FindComponent(BaseVehicleControllerComponent));
  if (controller) controller.ForceStartEngine();
  // Arrives at full rotor speed, not spooling up from a standstill (Jacob, 2 Oct).
  Spool();
  m_fStarted = now;
  PrintFormat("[ACR] SUPPORT helicopter %1 inbound from %2 (%3 m out, clear of bases that aren't ours: %4)", prefab, m_Pos, (int)vector.DistanceXZ(m_Pos, m_Target), ClearOfEnemies(m_Pos));
  m_iRockets = 14;
  if (m_bLoiter) m_iRockets = 16;
  if (m_bAttack) m_iRockets = ATTACK_ROCKETS;
  if (m_bDoorGuns) m_iRockets = 0;
  return true;
 }

 static const float PARK_DRIFT = 5; // m from where it was set down before a helicopter is put back to lift

 // Takes off from its helipad (the helicopter and crew are already there).
 bool StartFromPad(ACR_Supports supports, ACR_StationedHeli home, float now)
 {
  if (!home || !home.m_Vehicle) return false;
  m_Home = home;
  m_Vehicle = home.m_Vehicle;
  m_Pos = m_Vehicle.GetOrigin();
  // Pushed since it was set down (Everon long match 31 Oct: 33 m, under a light tower, and the lift
  // held there for good, the side's gunship "out on a mission" with nothing flying): the lift is
  // only known to be clear from where it was set down, so it goes back there first. (That spot, not
  // its pad: a helicopter re-homed to another pad flies from where it stands.)
  if (home.m_vParked != vector.Zero && vector.DistanceXZ(m_Pos, home.m_vParked) > PARK_DRIFT)
  {
   PrintFormat("[ACR] SUPPORT %1 was %2 m from where it was set down, back there before it lifts", home.m_sName, (int)vector.DistanceXZ(m_Pos, home.m_vParked));
   m_Pos = home.m_vParked;
   m_Vehicle.SetOrigin(m_Pos);
  }
  m_Start = m_Pos;
  vector toward = m_Target - m_Pos;
  toward[1] = 0;
  if (toward.Length() > 1) m_Dir = toward.Normalized();
  m_fYaw = m_Vehicle.GetYawPitchRoll()[0];
  m_fBodyYaw = m_fYaw;
  m_bLifting = true;
  Physics physics = m_Vehicle.GetPhysics();
  if (physics) { physics.EnableGravity(false); physics.SetVelocity(vector.Zero); }
  supports.Station().ReplaceCrew(m_Vehicle, m_sFaction, m_Home.m_bGunship, m_Home.m_bAttack);
  BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(m_Vehicle.FindComponent(BaseVehicleControllerComponent));
  if (controller) controller.ForceStartEngine();
  Spool();
  m_fStarted = now;
  m_iRockets = 14;
  if (m_bLoiter) m_iRockets = 16;
  if (m_bAttack) m_iRockets = ATTACK_ROCKETS;
  if (m_bDoorGuns) m_iRockets = 0;
  PrintFormat("[ACR] SUPPORT %1 taking off from its helipad at %2, %3 m from the target", home.m_sName, m_Pos, (int)vector.DistanceXZ(m_Pos, m_Target));
  return true;
 }

 // Straight up off the pad, the nose swinging round toward the target.
 protected bool Lift(float dt)
 {
  vector previous = m_Pos;
  float ground = GetGame().GetWorld().GetSurfaceY(m_Pos[0], m_Pos[2]);
  float rise = LIFT_RATE;
  if (m_Pos[1] - ground < 3) rise = LIFT_RATE * 0.5;
  m_Pos[1] = m_Pos[1] + rise * dt;
  float wantYaw = Math.Atan2(m_Dir[0], m_Dir[2]) * Math.RAD2DEG;
  TurnToward(wantYaw, 15, dt);
  m_fBodyYaw = m_fYaw;
  GuardFlightMove(previous, true);
  vector velocity = vector.Zero;
  if (dt > 0) velocity = (m_Pos - previous) * (1 / dt);
  Fly(velocity, dt);
  if (m_Pos[1] - ground >= LIFT_HEIGHT)
  {
   m_bLifting = false;
   m_fClimb = rise;
   m_fSpeed = 0;
   m_fNextTerrain = 0;
   PrintFormat("[ACR] SUPPORT %1 off the ground (%2 m up), heading out", LogName(), (int)(m_Pos[1] - ground));
  }
  return true;
 }

 protected void Place(vector velocity, float pitch, float roll)
 {
  vector transform[4];
  Math3D.AnglesToMatrix(Vector(m_fBodyYaw, pitch, roll), transform);
  transform[3] = m_Pos;
  m_Vehicle.SetTransform(transform);
  Physics physics = m_Vehicle.GetPhysics();
  if (physics)
  {
   physics.SetVelocity(velocity);
   physics.SetAngularVelocity(vector.Zero);
  }
 }

 // Attitude from the motion itself (Jacob 20 Oct: forward flight twitched, turns had no bank):
 // nose down to speed up and at cruise, nose up to brake, banked by the turn's sideways
 // acceleration. Eased and rate-limited, so it never snaps from one frame to the next.
 static const float MAX_BANK = 28;
 static const float BANK_RATE = 20;   // deg/s
 static const float PITCH_RATE = 12;  // deg/s
 float m_fPitch;
 protected vector m_vLastVelocity;
 protected vector m_vAccel;           // smoothed horizontal acceleration
 protected float m_fYawRate;          // deg/s the track is turning

 protected void Fly(vector velocity, float dt, float pitchBias = 0)
 {
  if (dt > 0)
  {
   vector raw = (velocity - m_vLastVelocity) * (1 / dt);
   raw[1] = 0;
   m_vAccel = m_vAccel + (raw - m_vAccel) * Math.Min(1, dt * 3);
  }
  m_vLastVelocity = velocity;
  vector nose = Heading(m_fBodyYaw);
  vector right = Vector(nose[2], 0, -nose[0]);
  float cruise = Math.Clamp(vector.Dot(velocity, nose) * 0.08, -2, 5);
  float wantPitch = pitchBias - cruise - Math.Atan2(vector.Dot(m_vAccel, nose) * 0.6, 9.81) * Math.RAD2DEG;
  float wantRoll = -Math.Atan2(vector.Dot(m_vAccel, right), 9.81) * Math.RAD2DEG;
  m_fPitch = Ease(m_fPitch, Math.Clamp(wantPitch, -14, 12), PITCH_RATE, dt);
  m_fRoll = Ease(m_fRoll, Math.Clamp(wantRoll, -MAX_BANK, MAX_BANK), BANK_RATE, dt);
  Place(velocity, m_fPitch, m_fRoll);
 }

 static float Ease(float value, float wanted, float maxRate, float dt)
 {
  float step = (wanted - value) * Math.Min(1, dt * 2.5);
  return value + Math.Clamp(step, -maxRate * dt, maxRate * dt);
 }

 // Turns the track toward wantYaw; the turn rate builds up and eases off instead of
 // switching between full rate and none.
 protected void TurnToward(float wantYaw, float maxRate, float dt)
 {
  float delta = Wrap(wantYaw - m_fYaw);
  m_fYawRate += (Math.Clamp(delta * 1.5, -maxRate, maxRate) - m_fYawRate) * Math.Min(1, dt * 3);
  float turn = m_fYawRate * dt;
  if (turn * delta > 0 && Math.AbsFloat(turn) > Math.AbsFloat(delta)) turn = delta;
  m_fYaw = Wrap(m_fYaw + turn);
 }

 // Back to the pad: slows down on the way in, hovers over the pad and sets down.
 protected bool FlyHome(ACR_Supports supports, float dt, float now)
 {
  foreach (ACR_HeliGun idle : m_Guns) idle.StopFire();
  if (!FlyToSpot(supports, m_Home.m_Pad.Spot(), dt, now)) return true;
  Touchdown(supports);
  return false;
 }

 // Flies to a landing spot (a pad, or an air lift's landing zone): slows down on the way in,
 // hovers over it and sets down. True once it is down.
 protected bool FlyToSpot(ACR_Supports supports, vector pad, float dt, float now)
 {
  vector flat = pad - m_Pos;
  flat[1] = 0;
  float d = flat.Length();
  vector toPad = m_Dir;
  if (d > 0.01) toPad = flat.Normalized();
  float padYaw = Math.Atan2(toPad[0], toPad[2]) * Math.RAD2DEG;
  float delta = Wrap(padYaw - m_fYaw);
  if (d > 3) TurnToward(padYaw, 24, dt);
  m_fBodyYaw = Wrap(m_fBodyYaw + Math.Clamp(Wrap(m_fYaw - m_fBodyYaw), -40 * dt, 40 * dt));
  // Slower near the pad, and while still turning toward it (no circling round it).
  float facing = Math.Max(0.2, Math.Cos(Math.Min(Math.AbsFloat(delta), 90) * Math.DEG2RAD));
  float wanted = Math.Clamp(d * 0.22, 0, SPEED_RUN) * facing;
  vector forward = Heading(m_fYaw);
  float terrainY = m_Pos[1];
  if (d > 25)
  {
   terrainY = FlightClearance(forward, ALT_TRANSIT + m_iStack * STACK_STEP, now);
   wanted = Math.Min(wanted, m_fTerrainSpeed);
  }
  // Brake early enough to stop with a bounded deceleration.
  wanted = Math.Min(wanted, Math.Sqrt(FLIGHT_BRAKE * 2 * d));
  m_fSpeed = ApproachSpeed(m_fSpeed, wanted, dt);
  vector previous = m_Pos;
  vector move;
  // The last stretch is flown straight at the pad whatever the nose does (a hover taxi).
  if (d < 40) move = toPad * Math.Min(Math.Min(m_fSpeed, d * 0.5 + 0.3) * dt, d);
  else move = forward * (m_fSpeed * dt);
  m_Pos = m_Pos + move;
  // Height: transit height far out, then a glide down to a hover over the pad, then down.
  float ground = GetGame().GetWorld().GetSurfaceY(m_Pos[0], m_Pos[2]);
  float wantedY;
  if (d > 300) wantedY = terrainY;
  else
  {
   wantedY = Math.Max(pad[1] + Math.Clamp((d - 8) * 0.35, 12, ALT_TRANSIT), ground + 12);
   if (d > 25) wantedY = Math.Max(wantedY, terrainY);
  }
  float rate = Math.Clamp((wantedY - m_Pos[1]) * 0.6, -6, 8);
  if (d < 1)
  {
   wantedY = pad[1];
   rate = -Math.Clamp((m_Pos[1] - pad[1]) * 0.4, 1.2, 4);
  }
  m_fClimb += (rate - m_fClimb) * Math.Min(1, dt * 1.5);
  m_Pos[1] = previous[1] + m_fClimb * dt;
  if (d < 1) m_Pos[1] = Math.Max(pad[1], m_Pos[1]);
  // Hold horizontal movement if the next step would enter terrain while still climbing.
  if (d > 40 && m_Pos[1] < ground + 20)
  {
   m_Pos[0] = previous[0];
   m_Pos[2] = previous[2];
   m_fSpeed = 0;
  }
  if (d > 300)
  {
   vector away = Separation(supports);
   if (away != vector.Zero) m_Pos = m_Pos + away * (2 * dt);
  }
  GuardFlightMove(previous, d < 1);
  vector velocity = vector.Zero;
  if (dt > 0) velocity = (m_Pos - previous) * (1 / dt);
  Fly(velocity, dt);
  if (now >= m_fNextAltLog)
  {
   m_fNextAltLog = now + 5000;
   PrintFormat("[ACR] SUPPORT %1 coming in: %2 m from its landing spot, %3 m up, %4 m/s", LogName(), (int)d, (int)(m_Pos[1] - ground), (int)m_fSpeed);
  }
  if (d < 1 && m_Pos[1] <= pad[1] + 0.35) return true;
  // Over the pad, the last metres blocked (a truck parked beside the pad in the rotor sweep): it
  // hovered there for good and the support never came back (srv:helipad, 1 Nov). Centred on its
  // own pad the body clears what the sweep touches: after 10 s it sets down.
  bool lowOverPad = d < 1 && m_Pos[1] <= pad[1] + 3;
  if (!lowOverPad || now >= m_fGuardRetryAt) m_fLandBlockedSince = -1;
  else if (m_fLandBlockedSince < 0) m_fLandBlockedSince = now;
  if (lowOverPad && m_fLandBlockedSince >= 0 && now - m_fLandBlockedSince > 10000)
  {
   PrintFormat("[ACR] SUPPORT %1 sets down on its pad past an obstacle in the rotor sweep", LogName());
   return true;
  }
  return false;
 }
 protected float m_fLandBlockedSince = -1;

 // On the pad: physics back on, engine off, servicing starts.
 void Touchdown(ACR_Supports supports)
 {
  if (m_bLanded || !m_Vehicle || !m_Home) return;
  m_bLanded = true;
  vector pad = m_Home.m_Pad.Spot();
  m_Pos = pad;
  m_fBodyYaw = m_Vehicle.GetYawPitchRoll()[0];
  Place(vector.Zero, 0, 0);
  Physics physics = m_Vehicle.GetPhysics();
  if (physics) physics.EnableGravity(true);
  VehicleHelicopterSimulation rotors = VehicleHelicopterSimulation.Cast(m_Vehicle.FindComponent(VehicleHelicopterSimulation));
  if (rotors)
  {
   rotors.SetThrottle(0);
   rotors.EngineStop();
  }
  supports.Station().Landed(m_Home);
 }

 static const float SPOOL_MAX_SECONDS = 25;
 static const float ENEMY_CLEARANCE = 1200; // spool-up spot this far from bases that aren't ours (Jacob, 2 Oct)
 vector m_Start;

 // It sits ~20 s at its start point while the rotors spool up: not near anything that isn't
 // ours. Turn the start round the target until it is clear, else start at our nearest base.
 vector SafeStart(vector start)
 {
  vector away = start - m_Target;
  away[1] = 0;
  float distance = away.Length();
  if (distance < 1) return start;
  float yaw = Math.Atan2(away[0], away[2]);
  array<float> turns = {0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5};
  foreach (float turn : turns)
  {
   vector candidate = m_Target + Vector(Math.Sin(yaw + turn) * distance, 0, Math.Cos(yaw + turn) * distance);
   if (ACR_Manager.OnMap(candidate) && ClearOfEnemies(candidate)) { m_Start = candidate; return candidate; }
  }
  ACR_Base home;
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), m_Target);
   if (d > 200 && d < best) { best = d; home = base; }
  }
  if (home) start = home.GetOrigin();
  m_Start = start;
  return start;
 }

 bool ClearOfEnemies(vector point)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner != m_sFaction && vector.DistanceXZ(base.GetOrigin(), point) < ENEMY_CLEARANCE) return false;
  return true;
 }
 protected bool m_bSpooled;
 // Full throttle: the rotors run at flying speed (the script flies the airframe itself).
 void Spool()
 {
  VehicleHelicopterSimulation rotors = VehicleHelicopterSimulation.Cast(m_Vehicle.FindComponent(VehicleHelicopterSimulation));
  if (!rotors) return;
  if (!rotors.EngineIsOn()) rotors.EngineStart();
  rotors.SetThrottle(1);
 }

 // Terrain-following height: clear the ground (and sea) ahead.
 static float SafeAltitude(vector pos, vector forward, float above)
 {
  BaseWorld world = GetGame().GetWorld();
  float ground = 0;
  // Looks 400 m ahead so climbs start early instead of at the slope's foot.
  for (int i = 0; i < 6; i++)
  {
   vector probe = pos + forward * (i * 80);
   ground = Math.Max(ground, world.GetSurfaceY(probe[0], probe[2]));
  }
  return ground + above;
 }

 bool IsDestroyed()
 {
  DamageManagerComponent damage = DamageManagerComponent.Cast(m_Vehicle.FindComponent(DamageManagerComponent));
  return damage && damage.GetState() == EDamageState.DESTROYED;
 }

 // Script flies the helicopter, so the game's own loss of lift never applied: with
 // a rotor shot off it kept flying (live 1 Oct, Mi-8). The damage manager's movement
 // damage combines engine, gearbox and every rotor (a lost tail rotor counts too).
 static const float CRIPPLED_MOVEMENT_DAMAGE = 0.75;

 // Going down: gravity back and the engine off. With the engine left running, the game's own flight
 // model kept it in the air: crippled at 83 m it climbed to 232 m in 40 s (srv:helidown), and one
 // from a pad flew 1.8 km off until its wreck timer removed it in mid-air (srv:helipad, 31 Oct).
 protected void LetFall()
 {
  Physics wreck = m_Vehicle.GetPhysics();
  if (wreck) wreck.EnableGravity(true);
  VehicleHelicopterSimulation rotors = VehicleHelicopterSimulation.Cast(m_Vehicle.FindComponent(VehicleHelicopterSimulation));
  if (rotors)
  {
   rotors.SetThrottle(0);
   rotors.EngineStop();
  }
 }
 float MovementDamage()
 {
  DamageManagerComponent damage = DamageManagerComponent.Cast(m_Vehicle.FindComponent(DamageManagerComponent));
  if (!damage) return 0;
  return damage.GetMovementDamage();
 }

 static vector Heading(float yaw)
 {
  return Vector(Math.Sin(yaw * Math.DEG2RAD), 0, Math.Cos(yaw * Math.DEG2RAD));
 }

 static float Wrap(float angle)
 {
  while (angle > 180) angle -= 360;
  while (angle < -180) angle += 360;
  return angle;
 }

 // Find every crewed gun and measure which target bearings (relative to the
 // nose, target 60 m below at 280 m) it can reach, then pick the flight pattern.
 void AnalyzeGuns(ACR_Supports supports)
 {
  m_bAnalyzed = true;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  // Some helicopters (the Mi-8) have no default crew for their guns: seat soldiers there.
  AIGroup crewGroup;
  foreach (BaseCompartmentSlot seat : slots)
  {
   if (seat.GetOccupant())
   {
    AIControlComponent seated = AIControlComponent.Cast(seat.GetOccupant().FindComponent(AIControlComponent));
    if (seated && seated.GetControlAIAgent()) crewGroup = seated.GetControlAIAgent().GetParentGroup();
    if (crewGroup) break;
   }
  }
  ResourceName crewman = supports.CrewPrefab(m_sFaction, !m_bDoorGuns);
  foreach (BaseCompartmentSlot empty : slots)
  {
   if (empty.GetOccupant() || empty.GetType() != ECompartmentType.TURRET) continue;
   ResourceName gunner = crewman;
   if (m_bAttack)
   {
    ResourceName packed = ACR_ContentSocket.Get().Crewman(m_sFaction, m_Vehicle, ECompartmentType.TURRET);
    if (!packed.IsEmpty()) gunner = packed;
   }
   IEntity seatedCrew = empty.SpawnCharacterInCompartment(gunner, crewGroup);
   PrintFormat("[ACR] SUPPORT seated %1 in %2: %3", gunner, empty.GetCompartmentName(), seatedCrew != null);
  }
  foreach (BaseCompartmentSlot slot : slots)
  {
   if (slot.GetType() != ECompartmentType.TURRET) continue;
   IEntity turret = slot.GetOwner();
   ACR_HeliGun gun = new ACR_HeliGun();
   gun.m_Slot = slot;
   gun.m_Turret = turret;
   gun.m_TurretComponent = TurretComponent.Cast(turret.FindComponent(TurretComponent));
   gun.m_Controller = TurretControllerComponent.Cast(turret.FindComponent(TurretControllerComponent));
   gun.m_sName = slot.GetCompartmentName();
   if (!gun.m_TurretComponent || !gun.m_Controller) continue;
   // Rocket pods and fixed guns are pilot weapons (no turret slot); this is crewed guns only.
   for (int bin = 0; bin < 36; bin++)
   {
    vector point = m_Pos + Heading(m_fBodyYaw + bin * 10 - 180) * LOITER_RADIUS;
    point[1] = m_Pos[1] - 60;
    gun.m_aBins.Insert(gun.Covers(point));
   }
   m_Guns.Insert(gun);
   PrintFormat("[ACR] SUPPORT gun %1 (occupant=%2 ammo=%3) reaches bearings: %4", gun.m_sName, slot.GetOccupant() != null, gun.Ammo(), gun.Describe());
  }
  // Best side bearing for an orbit: most guns covering +-(60..120).
  int bestCount = 0;
  float bestBearing = -90;
  array<int> signs = {-1, 1};
  array<int> sideBearings = {90, 80, 100, 70, 110, 60, 120};
  foreach (int b : sideBearings)
  {
   foreach (int sign : signs)
   {
    int count = 0;
    foreach (ACR_HeliGun g : m_Guns) if (g.CoversBearing(sign * b)) count++;
    if (count > bestCount) { bestCount = count; bestBearing = sign * b; }
   }
  }
  int noseTail = 0;
  foreach (ACR_HeliGun g2 : m_Guns)
  {
   if (g2.CoversBearing(0) || g2.CoversBearing(10) || g2.CoversBearing(-10)) noseTail++;
   else if (g2.CoversBearing(180) || g2.CoversBearing(170) || g2.CoversBearing(-170)) noseTail++;
  }
  m_fSideBearing = bestBearing;
  // Door gunners always orbit (the Mi-8's side window guns barely got a look in on passes).
  m_bPasses = bestCount == 0 || (!m_bDoorGuns && noseTail > bestCount);
  PrintFormat("[ACR] SUPPORT gunship pattern: %1 (side guns %2 at %3 deg, nose/tail guns %4)", m_bPasses, bestCount, bestBearing, noseTail);
 }

 // Steering away from our other helicopters nearby (2 Oct live: two orbiting near
 // each other collided). Zero when none is close.
 vector Separation(ACR_Supports supports)
 {
  vector push = vector.Zero;
  array<ACR_HeliRun> others = {};
  supports.Helicopters(others);
  foreach (ACR_HeliRun other : others)
  {
   if (other == this || !other.m_Vehicle || other.m_bDestroyed) continue;
   vector apart = m_Pos - other.m_Pos;
   float height = Math.AbsFloat(apart[1]);
   apart[1] = 0;
   float d = apart.Length();
   if (d >= SEPARATION || height > 40) continue;
   if (d < 1) apart = Heading(m_fYaw + 90);
   push = push + apart.Normalized() * ((1 - d / SEPARATION) * 2.5);
  }
  return push;
 }

 override bool Frame(ACR_Supports supports, float dt, float now)
 {
  if (!m_Vehicle) return false;
  float movementDamage = MovementDamage();
  if (IsDestroyed() || movementDamage >= CRIPPLED_MOVEMENT_DAMAGE)
  {
   m_bDestroyed = true;
   LetFall();
   supports.m_Manager.NotifyFaction(m_sFaction, "Support helicopter is going down!");
   PrintFormat("[ACR] SUPPORT helicopter going down (destroyed=%1 movement damage=%2)", IsDestroyed(), movementDamage);
   if (m_Home) supports.Station().Lost(m_Home);
   return false;
  }
  float limit = 300000;
  if (m_Home) limit = 600000; // there and back (Finish puts it on its pad if it ran out)
  if (now - m_fStarted > limit) return false;
  if (!m_bAnalyzed && !m_bLifting && now - m_fStarted > 500) AnalyzeGuns(supports);
  Spool();
  if (StillSpooling(now)) return true;
  if (m_bLifting) return Lift(dt);
  if (m_bReturning) return FlyHome(supports, dt, now);
  return FlySupport(supports, dt, now);
 }

 // The game spools the rotors up over ~20 s and has no way to set their speed, so the
 // helicopter waits out of sight at its start point until they're at flying speed.
 protected bool StillSpooling(float now)
 {
  if (!m_bSpooled)
  {
   float rpm, target;
   VehicleHelicopterSimulation rotors = VehicleHelicopterSimulation.Cast(m_Vehicle.FindComponent(VehicleHelicopterSimulation));
   if (rotors && rotors.RotorCount() > 0)
   {
    rpm = rotors.RotorGetRPM(0);
    target = rotors.RotorGetRPMTarget(0);
   }
   if (target <= 0 || rpm >= target * 0.9 || now - m_fStarted > SPOOL_MAX_SECONDS * 1000)
   {
    m_bSpooled = true;
    PrintFormat("[ACR] SUPPORT helicopter spooled up after %1 s (rotor %2 of %3 rpm)", (int)((now - m_fStarted) / 1000), (int)rpm, (int)target);
   }
   else
   {
    Physics still = m_Vehicle.GetPhysics();
    if (still) { still.SetVelocity(vector.Zero); still.SetAngularVelocity(vector.Zero); }
    return true;
   }
  }
  return false;
 }

 // The support mission itself: rocket run, gunship or door gunners.
 protected bool FlySupport(ACR_Supports supports, float dt, float now)
 {

  vector flat = m_Target - m_Pos;
  flat[1] = 0;
  float distance = flat.Length();
  vector toTarget = flat.Normalized();
  float yawToTarget = Math.Atan2(toTarget[0], toTarget[2]) * Math.RAD2DEG;
  vector desired = m_Dir;
  float bodyOffset = 0; // nose = track + offset
  float speed = SPEED_RUN;
  float altitude = ALT_TRANSIT;
  if (m_bLoiter)
  {
   speed = SPEED_LOITER;
   if (m_iPhase == 0 && distance < LOITER_RADIUS + 60)
   {
    m_iPhase = 1;
    m_fOrbitEnd = now + m_fLoiterMs;
    m_fNextSideSwap = now + DOOR_SIDE_SWAP_MS;
    supports.m_Manager.NotifyFaction(m_sFaction, StationName() + " on station.");
    PrintFormat("[ACR] SUPPORT gunship on station at %1", m_Pos);
   }
   if (m_iPhase == 1 && now > m_fOrbitEnd)
   {
    m_iPhase = 2;
    m_ExitDir = -toTarget;
    supports.m_Manager.NotifyFaction(m_sFaction, StationName() + " leaving station.");
    if (m_Home) { m_bReturning = true; m_fSpeed = SPEED_LOITER; }
   }
   if (m_iPhase == 0) desired = toTarget;
   else if (m_iPhase == 1)
   {
    altitude = ALT_LOITER;
    // Gunner on the working side down or out of ammo: turn the other side in.
    if (!m_bPasses && now >= m_fNextSideCheck)
    {
     m_fNextSideCheck = now + 2000;
     // Door gunners: turn the other door in every 30 s, so both gunners get to fire
     // (the one on the outside of the orbit never could, 1 Oct live test).
     bool turnAbout = m_bDoorGuns && now >= m_fNextSideSwap && SideCanFire(-m_fSideBearing);
     if (turnAbout || (!SideCanFire(m_fSideBearing) && SideCanFire(-m_fSideBearing)))
     {
      m_fSideBearing = -m_fSideBearing;
      m_fNextSideSwap = now + DOOR_SIDE_SWAP_MS;
      PrintFormat("[ACR] SUPPORT gunship switching to its other side (bearing %1)", m_fSideBearing);
     }
    }
    if (m_bPasses)
    {
     // Attack passes: run in, overfly, extend 650 m, come round again.
     if (!m_bPassOutbound && distance < 90) m_bPassOutbound = true;
     if (m_bPassOutbound && distance > 650) m_bPassOutbound = false;
     if (m_bPassOutbound) desired = Heading(m_fYaw);
     else desired = toTarget;
    }
    else
    {
     // Pylon turn: circle so the target stays on the gun side.
     // Target on the left (negative bearing) = circle with the left side inward.
     float side = -1;
     if (m_fSideBearing < 0) side = 1;
     vector tangent = Vector(toTarget[2], 0, -toTarget[0]) * side;
     desired = (tangent + toTarget * ((distance - LOITER_RADIUS) / LOITER_RADIUS * 1.5)).Normalized();
     // Point the nose so the target sits exactly at the guns' best bearing.
     bodyOffset = Wrap(yawToTarget - m_fSideBearing - Math.Atan2(desired[0], desired[2]) * Math.RAD2DEG);
     bodyOffset = Math.Clamp(bodyOffset, -35, 35);
    }
   }
   else
   {
    desired = m_ExitDir;
    altitude = ALT_EGRESS;
   }
  }
  // Rocket run: once past the firing window, climb out.
  else if (vector.Dot(m_Target - m_Pos, m_Dir) < 250) altitude = ALT_EGRESS;
  altitude += m_iStack * STACK_STEP;
  vector away = Separation(supports);
  if (away != vector.Zero) desired = (desired + away).Normalized();

  // Track: turn toward the desired heading at a helicopter-like rate (Fly banks into it).
  TurnToward(Math.Atan2(desired[0], desired[2]) * Math.RAD2DEG, 24, dt);
  // Body: the nose follows the track plus the pylon-turn offset.
  float bodyDelta = Wrap(m_fYaw + bodyOffset - m_fBodyYaw);
  m_fBodyYaw = Wrap(m_fBodyYaw + Math.Clamp(bodyDelta, -40 * dt, 40 * dt));

  // Off a helipad it picks up speed instead of jumping to it.
  if (m_Home)
  {
   m_fSpeed += (speed - m_fSpeed) * Math.Min(1, dt * 0.4);
   speed = Math.Max(m_fSpeed, 3);
  }
  vector forward = Heading(m_fYaw);
  vector nose = Heading(m_fBodyYaw);
  vector previous = m_Pos;
  m_Pos = m_Pos + forward * (speed * dt);
  float wantedY = SafeAltitude(m_Pos, forward, altitude);
  // Vertical speed eases toward what the terrain asks for (it used to jump straight
  // to the limit, so the flight looked rigid over hills).
  float maxUp = 12;
  if (altitude >= ALT_EGRESS) maxUp = 16;
  float wantedClimb = Math.Clamp((wantedY - previous[1]) * 0.5, -10, maxUp);
  m_fClimb += (wantedClimb - m_fClimb) * Math.Min(1, dt * 1.2);
  m_Pos[1] = previous[1] + m_fClimb * dt;
  float floorY = SafeAltitude(m_Pos, forward, 20);
  if (m_Pos[1] < floorY) { m_Pos[1] = previous[1] + Math.Min(floorY - previous[1], 20 * dt); m_fClimb = Math.Max(m_fClimb, 0); }
  // Fly adds cruise and acceleration pitch; this is the climb and the rocket dive.
  float pitch = Math.Clamp(m_fClimb * 0.2, -2, 2);
  if (now < m_fDiveUntil) pitch -= 4;
  if (now >= m_fNextAltLog)
  {
   m_fNextAltLog = now + 5000;
   PrintFormat("[ACR] SUPPORT heli height %1 m above ground, climb %2 m/s, phase %3, %4 m from target", (int)(m_Pos[1] - GetGame().GetWorld().GetSurfaceY(m_Pos[0], m_Pos[2])), (int)m_fClimb, m_iPhase, (int)distance);
  }

  // Rockets: gun run fires one pass on the way in; loiter fires salvos at
  // enemies near the target whenever the nose points at them.
  if (m_iRockets > 0 && now >= m_fNextRocket)
  {
   vector aim;
   bool shoot;
   if (!m_bLoiter)
   {
    float along = vector.Dot(m_Target - m_Pos, m_Dir);
    shoot = along < 650 && along > 280;
    if (shoot)
    {
     // Spread the salvo over the enemies seen around the target; with none seen,
     // walk it over the whole area.
     if (now >= m_fNextEnemyScan)
     {
      m_fNextEnemyScan = now + 500;
      m_aEnemies.Clear();
      supports.FindEnemies(m_sFaction, m_Target, RUN_TARGET_RADIUS, m_aEnemies);
     }
     IEntity runTarget;
     if (!m_aEnemies.IsEmpty()) runTarget = m_aEnemies[m_iRockets % m_aEnemies.Count()];
     m_bAimedAtEnemy = runTarget != null;
     if (runTarget) aim = runTarget.GetOrigin() + Vector(Math.RandomFloat(-6, 6), 1, Math.RandomFloat(-6, 6));
     else aim = m_Target + Vector(Math.RandomFloat(-RUN_SPREAD, RUN_SPREAD), 3, Math.RandomFloat(-RUN_SPREAD, RUN_SPREAD));
    }
   }
   else if (m_iSalvoLeft > 0)
   {
    // A salvo, once started, is fired complete at the same spot (no odd pair of rockets).
    shoot = true;
    aim = m_SalvoAim + Vector(Math.RandomFloat(-8, 8), 0, Math.RandomFloat(-8, 8));
   }
   else if (m_iPhase == 1)
   {
    IEntity enemy = supports.FindEnemyNear(m_sFaction, m_Target, 220);
    vector enemyFlat;
    if (enemy) enemyFlat = enemy.GetOrigin() - m_Pos;
    enemyFlat[1] = 0;
    shoot = enemy != null && vector.Dot(enemyFlat.Normalized(), nose) > 0.9 && enemyFlat.Length() < 650 && enemyFlat.Length() > 150;
    if (enemy) aim = enemy.GetOrigin() + Vector(Math.RandomFloat(-6, 6), 2, Math.RandomFloat(-6, 6));
    if (shoot)
    {
     m_iSalvoLeft = Math.Min(4, m_iRockets);
     m_SalvoAim = enemy.GetOrigin() + Vector(0, 2, 0);
    }
   }
   if (shoot)
   {
    aim[1] = Math.Max(aim[1], GetGame().GetWorld().GetSurfaceY(aim[0], aim[2]) + 0.5);
    vector right = Vector(nose[2], 0, -nose[0]);
    float sideOffset = 1.6;
    if (m_iRockets % 2 == 0) sideOffset = -1.6;
    supports.FireProjectile(m_Rocket, m_Pos + nose * 6 + right * sideOffset - Vector(0, 1.2, 0), aim, forward * speed);
    m_iRockets--;
    PrintFormat("[ACR] SUPPORT heli rocket, %1 left, range %2, at an enemy %3", m_iRockets, vector.Distance(m_Pos, aim), m_bAimedAtEnemy);
    m_fNextRocket = now + 280;
    m_fDiveUntil = now + 1200;
    if (m_bLoiter)
    {
     m_iSalvoLeft--;
     if (m_iSalvoLeft <= 0) m_fNextRocket = now + 8000;
    }
   }
  }

  // Gunnery: engage enemies near the target once within reach.
  vector heliVelocity = forward * speed;
  if (m_bLoiter && m_iPhase != 2 && distance < 700)
  {
   if (now >= m_fNextEnemyScan)
   {
    m_fNextEnemyScan = now + 500;
    m_aEnemies.Clear();
    supports.FindEnemies(m_sFaction, m_Target, 320, m_aEnemies);
    // Targets of opportunity around the helicopter itself: the gunner on the outside
    // of the orbit faces away from the target and had nothing to shoot (2 Oct live).
    supports.FindEnemies(m_sFaction, m_Pos, OPPORTUNITY_RADIUS, m_aEnemies);
   }
   foreach (ACR_HeliGun shooter : m_Guns) shooter.Update(m_Vehicle, heliVelocity, m_Target, m_aEnemies, now);
  }
  else foreach (ACR_HeliGun idle : m_Guns) idle.StopFire();

  // Gun report (loiter): which guns can reach the nearest enemy, and rounds fired.
  if (m_bLoiter && m_iPhase == 1 && now >= m_fNextGunLog)
  {
   m_fNextGunLog = now + 5000;
   IEntity nearest = supports.FindEnemyNear(m_sFaction, m_Target, 250);
   string report = "";
   foreach (ACR_HeliGun gun : m_Guns)
   {
    int ammo = gun.Ammo();
    int fired = 0;
    if (gun.m_iLastAmmo >= 0 && ammo >= 0) fired = gun.m_iLastAmmo - ammo;
    gun.m_iLastAmmo = ammo;
    bool reach = nearest != null && gun.Covers(nearest.GetOrigin());
    report += string.Format(" | %1 manned=%2 target=%3 inArc=%4 fired=%5", gun.m_sName, ACR_Respawn.IsAlive(gun.Occupant()), gun.m_Target != null, reach, fired);
   }
   PrintFormat("[ACR] SUPPORT gunship guns%1", report);
  }

  vector velocity = forward * speed;
  if (dt > 0) velocity[1] = (m_Pos[1] - previous[1]) / dt;
  Fly(velocity, dt, pitch);

  // From a helipad: turn for home once clear of the target.
  if (m_Home && !m_bLoiter && vector.Dot(m_Target - m_Pos, m_Dir) < -600)
  {
   m_bReturning = true;
   m_fSpeed = speed;
   PrintFormat("[ACR] SUPPORT %1 returning to its pad", m_Home.m_sName);
  }
  if (m_Home) return true;
  if (!m_bLoiter) return vector.Dot(m_Target - m_Pos, m_Dir) > -2000;
  if (m_iPhase == 2) return distance < 2000;
  return true;
 }

 // Log lines: the pad helicopter's own name, else the kind.
 string LogName()
 {
  if (m_Home) return m_Home.m_sName;
  return StationName();
 }

 string StationName()
 {
  if (m_bDoorGuns) return "Door gunners";
  if (m_bAttack) return "Attack helicopter";
  return "Gunship";
 }

 override void Finish()
 {
  if (!m_Vehicle || m_bDestroyed) return;
  // A stationed helicopter is kept: if it never made it down (time ran out), it is put on its pad.
  if (m_Home)
  {
   if (!m_bLanded)
   {
    PrintFormat("[ACR] SUPPORT %1 ran out of time on its way back: set down on its pad", m_Home.m_sName);
    ACR_Manager manager = ACR_Manager.GetInstance();
    if (manager && manager.m_Supports) Touchdown(manager.m_Supports);
   }
   return;
  }
  array<IEntity> crew = {};
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (compartments)
  {
   array<BaseCompartmentSlot> slots = {};
   compartments.GetCompartments(slots);
   foreach (BaseCompartmentSlot slot : slots) if (slot.GetOccupant()) crew.Insert(slot.GetOccupant());
  }
  // Only AI goes with it: a player aboard (he climbed into a lift in the air or a seat nobody
  // emptied) is put out rather than deleted with his loadout.
  foreach (IEntity member : crew)
  {
   if (EntityUtils.IsPlayer(member))
   {
    ChimeraCharacter player = ChimeraCharacter.Cast(member);
    CompartmentAccessComponent access;
    if (player) access = player.GetCompartmentAccessComponent();
    if (access) access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
    PrintFormat("[ACR] SUPPORT helicopter left the area with a player aboard: put out");
    continue;
   }
   SCR_EntityHelper.DeleteEntityAndChildren(member);
  }
  SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);
  Print("[ACR] SUPPORT helicopter left the area");
 }
}
