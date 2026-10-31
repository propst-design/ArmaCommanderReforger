// Air lift support (board aca-f22, 14 Oct). Jacob: an off-map transport helicopter, on every map
// and generated layout, for players and (later) the AI commander, which pays half (it can't fly
// bought helicopters). The commander selects a squad and right-clicks where it should land:
// one or two helicopters (enough cargo seats) fly in, land on clear ground near the squad, the
// squad boards, they fly to a clear spot near the clicked point, the squad gets out and holds
// there, and the helicopters leave the way they came.
//
// Stages: INBOUND (spool up off-map, fly to the pickup spots, squad walks there) -> BOARDING
// (native walk-and-board tasks reserve cargo seats on both carriers; stalled passengers
// retain bounded placement recovery, and the lift
// leaves with whoever is aboard after BOARD_WAIT_MS) -> TRANSIT -> UNLOADING (Get out; anyone
// still aboard after UNLOAD_WAIT_MS is put out) -> LEAVING. While a squad has an air lift, map
// orders to it are refused and the AI commander leaves it alone (ACR_AirLift.Busy).

// Open, level ground for a helicopter: a swept obstacle volume covering the rotor disk
// and tail, plus a survey of the ground beneath the whole footprint.
class ACR_LandingZone
{
 static const float CLEAR_RADIUS = 18;
 static const float MAX_RISE = 2.5;   // m between the highest and lowest ground in the footprint
 static const float SPACING = 40;     // m between two helicopters' spots
 static const float RING_STEP = 20;
 static int s_iChecks;                // spots examined (benchmark)
 static int s_iRejTree, s_iRejRay, s_iRejGround, s_iRejBox; // benchmark: where spots were refused

 // Up to count spots, nearest the centre first. Returns how many were found.
 // Searches that found nothing, remembered for a while (Jacob 18 Oct: a failed search over flat
 // forest took up to 4.7 s and the AI commander repeated it for every long move).
 static ref map<string, float> s_Failed = new map<string, float>();
 static const float FAILED_MEMORY_MS = 120000;
 static void Reset() { s_Failed.Clear(); }

 static const int SEARCH_BUDGET_MS = 120; // a search gives up after this (it runs inside one server tick)

 // A spot another air lift still uses (its pickups and drops until it is done). Two lifts sent
 // to one area picked spots 17 m apart (Everon soak 23 Oct), rotor disks overlapping.
 static bool InUse(vector spot)
 {
  foreach (ACR_AirLift lift : ACR_AirLift.s_Active)
  {
   if (!lift || lift.m_bDone) continue;
   foreach (vector pickup : lift.m_Pickups) if (vector.DistanceXZ(pickup, spot) < SPACING) return true;
   foreach (vector drop : lift.m_Drops) if (vector.DistanceXZ(drop, spot) < SPACING) return true;
  }
  return false;
 }

 static int Find(vector center, float maxRadius, int count, notnull array<vector> spots)
 {
  spots.Clear();
  int started = System.GetTickCount();
  float now = GetGame().GetWorld().GetWorldTime();
  string key = string.Format("%1:%2:%3:%4", (int)(center[0] / 10), (int)(center[2] / 10), (int)maxRadius, count);
  float failedAt;
  if (s_Failed.Find(key, failedAt) && now - failedAt < FAILED_MEMORY_MS) return 0;
  for (float ring = 0; ring <= maxRadius && spots.Count() < count && System.GetTickCount() - started <= SEARCH_BUDGET_MS; ring += RING_STEP)
  {
   int steps = 1;
   if (ring > 0)
   {
    float around = ring * Math.PI2 / 25.0;
    steps = Math.Max(8, (int)around);
   }
   for (int i = 0; i < steps && spots.Count() < count; i++)
   {
    float angle = Math.PI2 * i / steps;
    vector spot = Vector(center[0] + Math.Cos(angle) * ring, 0, center[2] + Math.Sin(angle) * ring);
    bool apart = !InUse(spot);
    foreach (vector taken : spots) if (vector.DistanceXZ(taken, spot) < SPACING) apart = false;
    if (System.GetTickCount() - started > SEARCH_BUDGET_MS) break;
    if (!apart || !Clear(spot)) continue;
    spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
    spots.Insert(spot);
   }
  }
  if (spots.IsEmpty())
  {
   if (s_Failed.Count() > 500) s_Failed.Clear();
   s_Failed.Set(key, now);
  }
  return spots.Count();
 }

 // One more spot at least SPACING from those already in spots, within maxRadius of center, with
 // its own time budget (Jacob 20 Oct: a 9-man squad got one helicopter because one end of the
 // lift had a single clear spot in the first search). True when one was added.
 static bool Another(vector center, float maxRadius, notnull array<vector> spots)
 {
  int started = System.GetTickCount();
  for (float ring = 0; ring <= maxRadius && System.GetTickCount() - started <= SEARCH_BUDGET_MS; ring += RING_STEP)
  {
   int steps = 1;
   if (ring > 0) steps = Math.Max(8, (int)(ring * Math.PI2 / 25.0));
   for (int i = 0; i < steps; i++)
   {
    if (System.GetTickCount() - started > SEARCH_BUDGET_MS) return false;
    float angle = Math.PI2 * i / steps;
    vector spot = Vector(center[0] + Math.Cos(angle) * ring, 0, center[2] + Math.Sin(angle) * ring);
    bool apart = !InUse(spot);
    foreach (vector taken : spots) if (vector.DistanceXZ(taken, spot) < SPACING) apart = false;
    if (!apart || !Clear(spot)) continue;
    spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
    spots.Insert(spot);
    return true;
   }
  }
  return false;
 }

 // Sample the ground separately from obstacles. A continuous box sweep catches narrow
 // trunks, poles and overhanging branches that the old spaced pencil rays missed.
 protected static vector s_vTreeProbe;
 protected static bool s_bTreeHit;
 protected static bool BlockingTree(IEntity e)
 {
  if (!Tree.Cast(e)) return true;
  if (vector.DistanceXZ(e.GetOrigin(), s_vTreeProbe) > CLEAR_RADIUS - 1) return true;
  vector mins, maxs;
  e.GetBounds(mins, maxs);
  if (maxs[1] - mins[1] < 3) return true;
#ifdef ENABLE_DESTRUCTION
  SCR_DestructibleTreeV2 tree = SCR_DestructibleTreeV2.Cast(e);
  if (tree && tree.GetIsDestroyed()) return true;
#endif
  s_bTreeHit = true;
  return false;
 }

 static bool Clear(vector spot)
 {
  s_iChecks++;
  if (!ACR_Manager.OnMap(spot)) return false;
  BaseWorld world = GetGame().GetWorld();
  float floorY = world.GetSurfaceY(spot[0], spot[2]);
  if (floorY < BBB_Terrain.SEA_LEVEL) return false;
  // Cheapest rejection in woods: a standing tree inside the footprint (its trunk would stop
  // the swept box below). The query stops at the first one.
  s_vTreeProbe = spot;
  s_bTreeHit = false;
  world.QueryEntitiesBySphere(Vector(spot[0], floorY + 2, spot[2]), CLEAR_RADIUS, BlockingTree, null, EQueryEntitiesFlags.STATIC);
  if (s_bTreeHit) { s_iRejTree++; return false; }
  // Cheap first: thin vertical rays inside the swept volume. Anything they hit, the box would
  // hit too, and in a forest one of them nearly always finds a tree before the costly sweep
  // (Jacob 18 Oct: failed searches over flat forest took up to 4.7 s).
  TraceParam ray = new TraceParam();
  ray.Flags = TraceFlags.ENTS;
  for (int r = 0; r < 17; r++)
  {
   float px = spot[0];
   float pz = spot[2];
   if (r > 0)
   {
    float ringR = 9;
    int k = r - 1;
    if (k >= 8) k -= 8;
    float a = Math.PI2 * k / 8.0;
    if (r > 8) { ringR = CLEAR_RADIUS - 1; a += 0.39; }
    px += Math.Cos(a) * ringR;
    pz += Math.Sin(a) * ringR;
   }
   ray.Start = Vector(px, floorY + 52, pz);
   ray.End = Vector(px, floorY + 0.4, pz);
   if (world.TraceMove(ray, null) < 1) { s_iRejRay++; return false; }
  }
  float low = 100000;
  float high = -100000;
  for (float dx = -CLEAR_RADIUS; dx <= CLEAR_RADIUS; dx += 2)
  {
   for (float dz = -CLEAR_RADIUS; dz <= CLEAR_RADIUS; dz += 2)
   {
    if (dx * dx + dz * dz > CLEAR_RADIUS * CLEAR_RADIUS) continue;
    float ground = world.GetSurfaceY(spot[0] + dx, spot[2] + dz);
    if (ground < BBB_Terrain.SEA_LEVEL) { s_iRejGround++; return false; }
    low = Math.Min(low, ground);
    high = Math.Max(high, ground);
    if (high - low > MAX_RISE) { s_iRejGround++; return false; }
   }
  }
  TraceBox clearance = new TraceBox();
  clearance.Mins = Vector(-CLEAR_RADIUS, 0.4, -CLEAR_RADIUS);
  clearance.Maxs = Vector(CLEAR_RADIUS, 12, CLEAR_RADIUS);
  clearance.Start = Vector(spot[0], floorY + 40, spot[2]);
  clearance.End = Vector(spot[0], floorY, spot[2]);
  clearance.Flags = TraceFlags.ENTS;
  bool clear = world.TraceMove(clearance, null) >= 1;
  if (!clear) s_iRejBox++;
  return clear;
 }
}

// One air lift helicopter: the support helicopter's flying, landing at a spot and leaving.
class ACR_LiftHeli : ACR_HeliRun
{
 vector m_Spot;      // where it is landing (pickup, then drop-off)
 bool m_bDown;       // on the ground at m_Spot, rotors turning
 bool m_bLeaving;
 bool m_bAway;       // gone off the map
 protected float m_fLeftAt;
 static int s_iTestSeats;  // tests: cargo seats per helicopter (0 = all), to make a squad need two

 override string StationName() { return "Air lift"; }

 // One frame. False once it is going down or has flown off.
 bool Step(ACR_Supports supports, float dt, float now)
 {
  if (!m_Vehicle || m_bDestroyed || m_bAway) return false;
  if (IsDestroyed() || MovementDamage() >= CRIPPLED_MOVEMENT_DAMAGE)
  {
   m_bDestroyed = true;
   Physics wreck = m_Vehicle.GetPhysics();
   if (wreck) wreck.EnableGravity(true);
   PrintFormat("[ACR] AIRLIFT helicopter going down (destroyed=%1 movement damage=%2)", IsDestroyed(), MovementDamage());
   return false;
  }
  Spool();
  if (StillSpooling(now)) return true;
  if (m_bLifting) return Lift(dt);
  if (m_bLeaving) return FlyAway(dt, now);
  if (m_bHold) return HoldStep(dt, now);
  if (m_bDown)
  {
   Place(vector.Zero, 0, 0);
   return true;
  }
  if (FlyToSpot(supports, m_Spot, dt, now))
  {
   m_bDown = true;
   m_Pos = m_Spot;
   Place(vector.Zero, 0, 0);
   PrintFormat("[ACR] AIRLIFT helicopter down at %1", m_Spot);
  }
  return true;
 }

 bool Spooled() { return m_bSpooled; }

 // Off-map lift helicopters spawn at transit height: fly in with the rotors spooling
 // instead of hanging still in the air for ~20 s first (aca-airlift-wave).
 void SkipSpoolWait() { m_bSpooled = true; }

 // Sequential lifts (one landing spot for two helicopters): wait airborne short of the
 // spot until the other helicopter has cleared it.
 static const float HOLD_DISTANCE = 220;
 bool m_bHold;
 vector m_HoldAt;
 int m_iLeg;          // sequential lifts: 0 to pickup, 1 loading, 2 to drop, 3 unloading, 4 gone
 float m_fLegAt;

 void Hold(vector spot)
 {
  if (m_bHold) return;
  vector back = m_Start - spot;
  back[1] = 0;
  if (back.Length() < 1) back = m_Dir * -1;
  back[1] = 0;
  if (back.Length() < 0.01) back = "0 0 1";
  m_HoldAt = spot + back.Normalized() * HOLD_DISTANCE;
  m_bHold = true;
  PrintFormat("[ACR] AIRLIFT helicopter holding %1 m short of %2 for its turn", (int)HOLD_DISTANCE, spot);
 }

 void Release()
 {
  if (!m_bHold) return;
  m_bHold = false;
  m_fNextTerrain = 0;
  PrintFormat("[ACR] AIRLIFT helicopter cleared to land at %1", m_Spot);
 }

 // Fly to the holding point at transit height and hover there.
 protected bool HoldStep(float dt, float now)
 {
  vector flat = m_HoldAt - m_Pos;
  flat[1] = 0;
  float d = flat.Length();
  if (d > 20)
  {
   vector dir = flat.Normalized();
   TurnToward(Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG, 24, dt);
  }
  m_fBodyYaw = m_fYaw;
  vector forward = Heading(m_fYaw);
  float wantedY = FlightClearance(forward, ALT_TRANSIT + m_iStack * STACK_STEP, now);
  float wanted = Math.Min(Math.Min(SPEED_RUN, m_fTerrainSpeed), Math.Sqrt(FLIGHT_BRAKE * 2 * d));
  if (d <= 20) wanted = 0;
  m_fSpeed = ApproachSpeed(m_fSpeed, wanted, dt);
  vector previous = m_Pos;
  m_Pos = m_Pos + forward * (m_fSpeed * dt);
  float rate = Math.Clamp((wantedY - m_Pos[1]) * 0.6, -6, 8);
  m_fClimb += (rate - m_fClimb) * Math.Min(1, dt * 1.5);
  m_Pos[1] = previous[1] + m_fClimb * dt;
  float ground = GetGame().GetWorld().GetSurfaceY(m_Pos[0], m_Pos[2]);
  if (m_Pos[1] < ground + 20)
  {
   m_Pos[0] = previous[0];
   m_Pos[2] = previous[2];
   m_fSpeed = 0;
  }
  GuardFlightMove(previous);
  vector velocity = vector.Zero;
  if (dt > 0) velocity = (m_Pos - previous) * (1 / dt);
  Fly(velocity, dt);
  return true;
 }

 // Off the ground, then on to the next spot (or away).
 void TakeOff(vector next, bool leaving)
 {
  m_bDown = false;
  m_bLeaving = leaving;
  m_Spot = next;
  vector toward = next - m_Pos;
  if (leaving) toward = m_Start - m_Pos;
  toward[1] = 0;
  if (toward.Length() > 1) m_Dir = toward.Normalized();
  m_bLifting = true;
  m_fLeftAt = GetGame().GetWorld().GetWorldTime();
 }

 // Back the way it came: off the map (or 90 s out), then gone.
 protected bool FlyAway(float dt, float now)
 {
  vector flat = m_Start - m_Pos;
  flat[1] = 0;
  float d = flat.Length();
  if (d < 150 || now - m_fLeftAt > 90000)
  {
   m_bAway = true;
   return false;
  }
  vector dir = flat.Normalized();
  float want = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
  TurnToward(want, 24, dt);
  m_fBodyYaw = m_fYaw;
  vector forward = Heading(m_fYaw);
  float wantedY = FlightClearance(forward, ALT_TRANSIT + m_iStack * STACK_STEP, now);
  m_fSpeed = ApproachSpeed(m_fSpeed, Math.Min(SPEED_RUN, m_fTerrainSpeed), dt);
  vector previous = m_Pos;
  m_Pos = m_Pos + forward * (m_fSpeed * dt);
  float rate = Math.Clamp((wantedY - m_Pos[1]) * 0.6, -6, 8);
  m_fClimb += (rate - m_fClimb) * Math.Min(1, dt * 1.5);
  m_Pos[1] = previous[1] + m_fClimb * dt;
  float ground = GetGame().GetWorld().GetSurfaceY(m_Pos[0], m_Pos[2]);
  if (m_Pos[1] < ground + 20)
  {
   m_Pos[0] = previous[0];
   m_Pos[2] = previous[2];
   m_fSpeed = 0;
  }
  GuardFlightMove(previous);
  vector velocity = vector.Zero;
  if (dt > 0) velocity = (m_Pos - previous) * (1 / dt);
  Fly(velocity, dt);
  return true;
 }

 int FreeSeats(out BaseCompartmentSlot first, array<BaseCompartmentSlot> claimed = null)
 {
  first = null;
  array<BaseCompartmentSlot> seats = {};
  FreeSeatList(seats, claimed);
  if (!seats.IsEmpty()) first = seats[0];
  return seats.Count();
 }

 // Free cargo seats, leaving out any already given to someone this second (claimed).
 void FreeSeatList(notnull array<BaseCompartmentSlot> result, array<BaseCompartmentSlot> claimed = null)
 {
  result.Clear();
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
  int used;
  foreach (BaseCompartmentSlot seat : seats)
  {
   if (seat.GetOccupant() || seat.IsReserved()) { used++; continue; }
   if (claimed && claimed.Contains(seat)) { used++; continue; }
   result.Insert(seat);
  }
  if (s_iTestSeats > 0) while (result.Count() > 0 && result.Count() + used > s_iTestSeats) result.Remove(result.Count() - 1);
 }

 int CargoSeats()
 {
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
  if (s_iTestSeats > 0) return Math.Min(s_iTestSeats, seats.Count());
  return seats.Count();
 }
}

// A cargo reservation stays owned until native occupancy confirms boarding.
// Native GetInVehicle supplies the walk to the assigned carrier, including the
// overflow carrier beyond the squad's shared rendezvous waypoint.
class ACR_LiftBoarding
{
 ChimeraCharacter m_Character;
 ACR_LiftHeli m_Heli;
 BaseCompartmentSlot m_Seat;
 SCR_AIUtilityComponent m_Utility;
 ref SCR_AIGetInVehicle m_Action;
 float m_fRequestedAt;
 float m_fProgressAt;
 float m_fBestDistance;
 vector m_LastPosition;

 void Release()
 {
  bool seated = m_Seat && m_Seat.GetOccupant() == m_Character;
  if (m_Action && m_Utility)
  {
   EAIActionState state = m_Action.GetActionState();
   if (state != EAIActionState.COMPLETED && state != EAIActionState.FAILED)
   {
    if (!seated && m_Character && ACR_Squad.FindController(m_Character) == 0 && m_Seat && m_Seat.IsReservedBy(m_Character))
    {
     CompartmentAccessComponent access = m_Character.GetCompartmentAccessComponent();
     if (access && access.IsGettingIn()) access.InterruptVehicleActionQueue(true, true, true);
    }
    // Native Fail() can queue a dismount or eject an already seated passenger.
    // Finish only our behavior; never clear someone else's newer reservation.
    if (!m_Seat || !m_Seat.IsReservedBy(m_Character)) m_Action.m_CompartmentToGetIn.m_Value = null;
    m_Action.Complete();
   }
  }
  if (m_Seat && m_Character && m_Seat.IsReservedBy(m_Character)) m_Seat.SetReserved(null);
  m_Action = null;
 }
}

class ACR_AirLift : ACR_SupportRun
{
 static const int INBOUND = 0;
 static const int BOARDING = 1;
 static const int TRANSIT = 2;
 static const int UNLOADING = 3;
 static const int LEAVING = 4;
 static const float MIN_DISTANCE = 400;     // m: closer than this, walking is about as quick
 static const float PICKUP_SEARCH = 250;    // m round the squad for a clear landing spot
 static const float DROP_SEARCH = 200;      // m round the clicked point
 static const float SECOND_SPOT_SEARCH = 300; // m round the first spot when a second helicopter has none
 static const float NEAR_RANGE = 40;        // m: stalled native boarding may use near recovery
 static const float FORCE_RANGE = 400;      // m: farther stalled passengers may use delayed recovery
 static float NEAR_BOARD_MS = 10000;
 static float FORCE_BOARD_MS = 60000;
 static float SEQUENTIAL_FORCE_MS = 15000;  // later chalks of a sequential lift
 static float BOARD_WAIT_MS = 120000;       // then it leaves with whoever is aboard
 static float UNLOAD_WAIT_MS = 25000;       // then anyone still aboard is put out
 static const float TIME_LIMIT_MS = 900000;

 static ref array<ACR_AirLift> s_Active = {};   // weak: a finished lift drops out by itself

 // Strategic multi-squad air assault wave (aca-airlift-wave): allow up to 3 squads to launch
 // in the same wave within 15 seconds of the wave's first squad before triggering cooldown.
 static float s_fWaveUntil;
 static string s_sWaveFaction;
 static int s_iWaveCount;
 static const int MAX_WAVE_SQUADS = 3;

 static void Reset()
 {
  s_Active.Clear();
  s_fWaveUntil = 0;
  s_sWaveFaction = "";
  s_iWaveCount = 0;
 }
 static const float WAVE_WINDOW_MS = 15000;

 static bool WaveSlotAvailable(string factionKey, float now)
 {
  return s_sWaveFaction == factionKey && now <= s_fWaveUntil && s_iWaveCount < MAX_WAVE_SQUADS;
 }

 ACR_Squad m_Squad;
 string m_sFaction;
 string m_sSquadName;
 bool m_bByAI;
 int m_iStage;
 ref array<ref ACR_LiftHeli> m_Helis = {};
 protected ref array<ref ACR_LiftBoarding> m_Boarding = {};
 ref array<vector> m_Pickups = {};
 ref array<vector> m_Drops = {};
 ResourceName m_sFinalPrefab;
 vector m_FinalTarget;
 float m_fFinalRadius;
 string m_sFinalOrder;
 int m_iFinalKind;
 protected float m_fStarted;
 protected float m_fStageAt;
 protected float m_fNextStep;
 protected bool m_bToldDown;
 protected float m_fOutAt = -1;
 // Only one landing spot at either end for a squad that needs two helicopters: they take
 // turns on the same spots (aca-airlift-wave; Jacob 21 Oct: 9-man squads in 6-seat Hueys
 // were refused or walked 300 m through woods to a second spot).
 bool m_bSequential;
 protected int m_iChalksOut;      // sequential: helicopters that have left the pickup with troops
 static bool s_bTestOneSpot;      // tests: never look for a second spot

 // A squad with an air lift on the way, boarding, in the air or getting out.
 static bool Busy(ACR_Squad squad)
 {
  if (!squad) return false;
  foreach (ACR_AirLift lift : s_Active)
   if (lift && !lift.m_bDone && lift.m_Squad == squad && lift.m_iStage < LEAVING) return true;
  return false;
 }

 static ACR_AirLift For(ACR_Squad squad)
 {
  foreach (ACR_AirLift lift : s_Active)
   if (lift && !lift.m_bDone && lift.m_Squad == squad) return lift;
  return null;
 }

 static int Alive(ACR_Squad squad)
 {
  int alive;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents) if (agent && ACR_Respawn.IsAlive(agent.GetControlledEntity())) alive++;
  return alive;
 }

 // The support call (from ACR_Supports.Call). Nothing is spent unless helicopters can come.
 static string Call(ACR_Supports supports, string factionKey, ACR_SupportEntry entry, vector target, bool byAI, ACR_Squad squad, ResourceName finalPrefab = "", vector finalTarget = vector.Zero, float finalRadius = -1, string finalOrder = "", int finalKind = 0)
 {
  for (int i = s_Active.Count() - 1; i >= 0; i--) if (!s_Active[i]) s_Active.Remove(i);
  if (!squad || !squad.m_Group) return "Select the squad to fly first, then call the air lift";
  string name = squad.GetName();
  if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != factionKey) return "That squad belongs to another faction";
  if (Busy(squad)) return name + " already has an air lift";
  if (squad.HasPlayer()) return name + ": an air lift can't carry a squad a player is in";
  if (squad.IsMounted() || ACR_Transport.CrewedVehicle(squad)) return name + " is in a vehicle: order it out first";
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return name + " is not ready";
  if (!ACR_Manager.OnMap(target)) return "That landing zone is off the map";
  float distance = vector.DistanceXZ(anchor.GetOrigin(), target);
  if (distance < MIN_DISTANCE) return string.Format("%1 is only %2 m away: walk it (air lifts from %3 m)", name, (int)distance, (int)MIN_DISTANCE);
  array<vector> pickups = {};
  if (ACR_LandingZone.Find(anchor.GetOrigin(), PICKUP_SEARCH, 2, pickups) == 0) return string.Format("No clear landing spot within %1 m of %2 (trees, buildings, slopes or water)", (int)PICKUP_SEARCH, name);
  array<vector> drops = {};
  if (ACR_LandingZone.Find(target, DROP_SEARCH, 2, drops) == 0) return string.Format("No clear landing spot within %1 m of that point (trees, buildings, slopes or water)", (int)DROP_SEARCH);
  int cost = entry.CostFor(byAI);
  if (!supports.m_Manager.m_Bases.Spend(factionKey, cost)) return string.Format("Not enough RP for %1 (%2 needed)", entry.m_sName, cost);
  float now = GetGame().GetWorld().GetWorldTime();
  bool inWave = WaveSlotAvailable(factionKey, now);
  if (!inWave)
  {
   s_sWaveFaction = factionKey;
   s_fWaveUntil = now + WAVE_WINDOW_MS;
   s_iWaveCount = 1;
  }
  else
  {
   s_iWaveCount++;
  }
  entry.m_fReadyAt = now + entry.m_iCooldown * 1000;
  ACR_AirLift lift = new ACR_AirLift();
  lift.m_Squad = squad;
  lift.m_sFaction = factionKey;
  lift.m_sSquadName = name;
  lift.m_bByAI = byAI;
  lift.m_sFinalPrefab = finalPrefab;
  lift.m_FinalTarget = finalTarget;
  lift.m_fFinalRadius = finalRadius;
  lift.m_sFinalOrder = finalOrder;
  lift.m_iFinalKind = finalKind;
  lift.m_Pickups.Copy(pickups);
  lift.m_Drops.Copy(drops);
  if (!lift.Begin(supports, now))
  {
   supports.m_Manager.m_Bases.GetFaction(factionKey).m_iRP += cost;
   if (inWave) s_iWaveCount--;
   else { s_fWaveUntil = 0; entry.m_fReadyAt = now; }
   return "No helicopter available";
  }
  supports.AddRun(lift);
  s_Active.Insert(lift);
  // They walk to a point beside the landing spot, not under the rotors.
  vector meet = pickups[0];
  vector side = anchor.GetOrigin() - pickups[0];
  side[1] = 0;
  if (side.Length() > 1) meet = ACR_Manager.Ground(pickups[0] + side.Normalized() * 15);
  squad.MoveTo(meet);
  squad.SetOrder("Air lift: to the landing zone", ACR_EOrderState.MOVE);
  ACR_LiftHeli first = lift.m_Helis[0];
  // Include gentle acceleration, braking and the final vertical descent (no spool-up wait:
  // the helicopters arrive already flying).
  float seconds = vector.DistanceXZ(first.m_Start, pickups[0]) / ACR_HeliRun.SPEED_RUN;
  seconds += ACR_HeliRun.SPEED_RUN / (ACR_HeliRun.FLIGHT_ACCEL * 2) + ACR_HeliRun.SPEED_RUN / (ACR_HeliRun.FLIGHT_BRAKE * 2);
  int eta = (int)(seconds + ACR_HeliRun.ALT_TRANSIT / ACR_HeliRun.LIFT_RATE);
  string message = string.Format("%1 for %2: %3 helicopter(s) landing at %4, ETA about %5 s.", entry.m_sName, name, lift.m_Helis.Count(), ACR_Manager.Grid(pickups[0]), (int)(Math.Round(eta / 15.0) * 15));
  PrintFormat("[ACR] AIRLIFT %1 %2: %3 helicopter(s), %4 soldiers, pickup %5, drop %6 (%7 m), cost %8%9", factionKey, name, lift.m_Helis.Count(), Alive(squad), pickups[0], drops[0], (int)vector.DistanceXZ(pickups[0], drops[0]), cost, AIText(byAI));
  if (!byAI) supports.m_Manager.NotifyFaction(factionKey, message);
  return message;
 }

 protected static string AIText(bool byAI)
 {
  if (byAI) return " (AI commander)";
  return "";
 }

 // Spawns the helicopters off the map: a second one when the first hasn't seats for everyone
 // (and both ends have room for two).
 bool Begin(ACR_Supports supports, float now)
 {
  m_fStarted = now;
  m_fStageAt = now;
  ResourceName prefab = supports.TransportHelicopterFor(m_sFaction);
  if (!SpawnHeli(supports, prefab, 0, now)) return false;
  int seats = m_Helis[0].CargoSeats();
  if (seats < Alive(m_Squad))
  {
   // A second helicopter needs its own spot at both ends: look wider round the first one.
   if (!s_bTestOneSpot)
   {
    if (m_Pickups.Count() < 2) ACR_LandingZone.Another(m_Pickups[0], SECOND_SPOT_SEARCH, m_Pickups);
    if (m_Drops.Count() < 2) ACR_LandingZone.Another(m_Drops[0], SECOND_SPOT_SEARCH, m_Drops);
   }
   if (m_Pickups.Count() > 1 && m_Drops.Count() > 1) SpawnHeli(supports, prefab, 1, now);
   else
   {
    // No room for two: the second one waits airborne and lands on the same spots in turn.
    m_bSequential = true;
    SpawnHeli(supports, prefab, 1, now, true);
    PrintFormat("[ACR] AIRLIFT %1: one landing spot (pickup spots %2, drop spots %3): helicopters take turns", m_sSquadName, m_Pickups.Count(), m_Drops.Count());
   }
   if (m_Helis.Count() < 2) PrintFormat("[ACR] AIRLIFT %1: no second helicopter (pickup spots %2, drop spots %3)", m_sSquadName, m_Pickups.Count(), m_Drops.Count());
  }
  PrintFormat("[ACR] AIRLIFT %1: %2 cargo seats per helicopter, %3 helicopter(s)", m_sSquadName, seats, m_Helis.Count());
  return true;
 }

 protected bool SpawnHeli(ACR_Supports supports, ResourceName prefab, int index, float now, bool sharedSpot = false)
 {
  int spotIndex = index;
  if (sharedSpot) spotIndex = 0;
  ACR_LiftHeli heli = new ACR_LiftHeli();
  heli.m_sFaction = m_sFaction;
  heli.m_bDoorGuns = true; // no rockets
  heli.m_Target = m_Pickups[spotIndex];
  heli.m_Spot = m_Pickups[spotIndex];
  heli.m_Dir = supports.ApproachDirection(m_sFaction, m_Pickups[spotIndex]);
  heli.m_iStack = index;
  if (!heli.Start(supports, prefab, now)) return false;
  heli.SkipSpoolWait();
  if (sharedSpot) heli.Hold(heli.m_Spot);
  m_Helis.Insert(heli);
  return true;
 }

 override bool Frame(ACR_Supports supports, float dt, float now)
 {
  if (now - m_fStarted > TIME_LIMIT_MS)
  {
   PrintFormat("[ACR] AIRLIFT %1: out of time in stage %2", m_sSquadName, m_iStage);
   return false;
  }
  int flying;
  foreach (ACR_LiftHeli heli : m_Helis)
  {
   if (heli.Step(supports, dt, now)) { flying++; continue; }
   if (heli.m_bDestroyed && !m_bToldDown)
   {
    m_bToldDown = true;
    Tell(supports, "Air lift helicopter is going down!");
   }
  }
  if (flying == 0) return false;
  if (now < m_fNextStep) return true;
  m_fNextStep = now + 1000;
  bool squadGone = !m_Squad || !m_Squad.m_Group || Alive(m_Squad) == 0;
  if (m_bSequential && m_iStage < UNLOADING)
  {
   if (squadGone) Leave(supports, now, "the squad is gone");
   else SequentialStep(supports, now);
   return true;
  }
  switch (m_iStage)
  {
   case INBOUND:
   {
    if (squadGone) { Leave(supports, now, "the squad is gone"); break; }
    if (!AllDown()) break;
    Stage(BOARDING, now);
    Tell(supports, string.Format("Air lift landed for %1 at %2.", m_sSquadName, ACR_Manager.Grid(m_Pickups[0])));
    break;
   }
   case BOARDING:
   {
    if (squadGone) { Leave(supports, now, "the squad is gone"); break; }
    int aboard = Board(now);
    int alive = Alive(m_Squad);
    if (aboard >= alive || (now - m_fStageAt > BOARD_WAIT_MS && aboard > 0))
    {
     if (aboard < alive) PrintFormat("[ACR] AIRLIFT %1: leaving with %2 of %3 aboard", m_sSquadName, aboard, alive);
     ClearBoarding();
     m_Squad.Halt("Air lift: in the air", ACR_EOrderState.HOLD);
     foreach (int i, ACR_LiftHeli up : m_Helis)
     {
      if (up.m_bDestroyed || up.m_bAway) continue;
      if (Passengers(up) == 0) up.TakeOff(up.m_Start, true);
      else up.TakeOff(m_Drops[Math.Min(i, m_Drops.Count() - 1)], false);
     }
     Stage(TRANSIT, now);
     PrintFormat("[ACR] AIRLIFT %1: %2 of %3 aboard, taking off", m_sSquadName, aboard, alive);
    }
    else if (now - m_fStageAt > BOARD_WAIT_MS)
    {
     m_Squad.Halt("Holding");
     Leave(supports, now, "nobody reached the landing zone");
     Tell(supports, string.Format("Air lift left: %1 never reached the landing zone.", m_sSquadName));
    }
    break;
   }
   case TRANSIT:
   {
    bool allDown = true;
    int carrying;
    foreach (ACR_LiftHeli carrier : m_Helis)
    {
     if (carrier.m_bDestroyed || carrier.m_bAway || carrier.m_bLeaving) continue;
     carrying++;
     if (!carrier.m_bDown) allDown = false;
    }
    if (carrying == 0) return false;
    if (!allDown) break;
    Stage(UNLOADING, now);
    if (m_Squad && m_Squad.m_Group)
    {
     if (!m_sFinalPrefab.IsEmpty())
     {
      vector dest = m_FinalTarget;
      if (dest == vector.Zero) dest = m_Drops[0];
      m_Squad.ContinueAfterDismount(m_sFinalPrefab, dest, m_fFinalRadius, m_sFinalOrder, m_iFinalKind);
     }
     m_Squad.GetOut();
    }
    PrintFormat("[ACR] AIRLIFT %1: down at the drop-off, getting out", m_sSquadName);
    break;
   }
   case UNLOADING:
   {
    int still = Aboard();
    if (still > 0 && now - m_fStageAt > UNLOAD_WAIT_MS) PutOut();
    if (still > 0 && now - m_fStageAt <= UNLOAD_WAIT_MS + 5000) break;
    // Out (or left 5 s to get clear of a forced exit): hold the landing zone, then go.
    if (m_Squad && m_Squad.m_Group && Alive(m_Squad) > 0)
    {
     if (!m_sFinalPrefab.IsEmpty())
     {
      vector finalDest = m_FinalTarget;
      if (finalDest == vector.Zero) finalDest = m_Drops[0];
      m_Squad.RestoreOrder(ACR_OrderSnapshot.Make(m_sFinalPrefab, finalDest, m_fFinalRadius, m_sFinalOrder, ACR_Squad.StateOfLabel(m_sFinalOrder), m_iFinalKind));
     }
     else
     {
      m_Squad.Defend(m_Drops[0]);
     }
    }
    Tell(supports, string.Format("%1 landed at %2.", m_sSquadName, ACR_Manager.Grid(m_Drops[0])));
    Leave(supports, now, "done");
    break;
   }
  }
  return true;
 }

 protected void Stage(int stage, float now)
 {
  m_iStage = stage;
  m_fStageAt = now;
 }

 protected void Tell(ACR_Supports supports, string message)
 {
  PrintFormat("[ACR] AIRLIFT %1", message);
  if (!m_bByAI) supports.m_Manager.NotifyFaction(m_sFaction, message);
 }

 protected void Leave(ACR_Supports supports, float now, string why)
 {
  PrintFormat("[ACR] AIRLIFT %1: helicopters leaving (%2)", m_sSquadName, why);
  foreach (ACR_LiftHeli heli : m_Helis)
   if (!heli.m_bDestroyed && !heli.m_bAway && !heli.m_bLeaving) heli.TakeOff(heli.m_Start, true);
  ClearBoarding();
  Stage(LEAVING, now);
 }

 protected bool AllDown()
 {
  int live;
  foreach (ACR_LiftHeli heli : m_Helis)
  {
   if (heli.m_bDestroyed || heli.m_bAway) continue;
   live++;
   if (!heli.m_bDown) return false;
  }
  return live > 0;
 }

 protected bool Loading(ACR_LiftHeli heli)
 {
  return heli && heli.m_Vehicle && !heli.m_bDestroyed && !heli.m_bAway && heli.m_bDown
   && !heli.m_bLeaving && (!m_bSequential || heli.m_iLeg == 1);
 }

 protected void ClearBoarding(ACR_LiftHeli only = null)
 {
  for (int i = m_Boarding.Count() - 1; i >= 0; i--)
  {
   ACR_LiftBoarding job = m_Boarding[i];
   if (only && job.m_Heli != only) continue;
   job.Release();
   m_Boarding.Remove(i);
  }
 }

 // Native get-in tasks walk each soldier to a uniquely reserved cargo seat.
 // Keep the existing placement recovery only for passengers who stop progressing.
 protected int Board(float now)
 {
  float waited = now - m_fStageAt;
  array<AIAgent> agents = {};
  m_Squad.GetAgents(agents);
  array<ChimeraCharacter> eligible = {};
  foreach (AIAgent member : agents)
  {
   if (!member) continue;
   ChimeraCharacter body = ChimeraCharacter.Cast(member.GetControlledEntity());
   if (body && ACR_Respawn.IsAlive(body) && ACR_Squad.FindController(body) == 0) eligible.Insert(body);
  }
  for (int prune = m_Boarding.Count() - 1; prune >= 0; prune--)
  {
   ACR_LiftBoarding pending = m_Boarding[prune];
   bool occupied = pending.m_Seat && pending.m_Seat.GetOccupant() == pending.m_Character;
   bool invalid = !eligible.Contains(pending.m_Character) || !Loading(pending.m_Heli)
    || !pending.m_Utility || !pending.m_Seat || (!occupied && pending.m_Seat.GetOccupant() != null)
    || (!occupied && pending.m_Seat.IsReserved() && !pending.m_Seat.IsReservedBy(pending.m_Character));
   if (occupied || invalid)
   {
    pending.Release();
    m_Boarding.Remove(prune);
   }
  }
  foreach (ChimeraCharacter character : eligible)
  {
   IEntity inVehicle = ACR_Transport.VehicleOf(character);
   if (inVehicle && Ours(inVehicle)) continue;
   if (inVehicle || character.GetCharacterController().IsUnconscious()) continue;
   CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
   if (!access) continue;
   ACR_LiftBoarding job;
   foreach (ACR_LiftBoarding existing : m_Boarding)
    if (existing.m_Character == character) { job = existing; break; }
   if (job)
   {
    float distance = vector.DistanceXZ(character.GetOrigin(), job.m_Heli.m_Vehicle.GetOrigin());
    // A valid path may first lead away from the carrier around an obstacle.
    // Actual foot movement counts as progress as well as closing the distance.
    if (distance < job.m_fBestDistance - 1 || vector.DistanceSqXZ(character.GetOrigin(), job.m_LastPosition) > 1)
    {
     job.m_fBestDistance = Math.Min(job.m_fBestDistance, distance);
     job.m_LastPosition = character.GetOrigin();
     job.m_fProgressAt = now;
    }
    EAIActionState actionState = job.m_Action.GetActionState();
    bool ended = actionState == EAIActionState.COMPLETED || actionState == EAIActionState.FAILED;
    float forceMs = FORCE_BOARD_MS;
    if (m_bSequential && m_iChalksOut > 0) forceMs = Math.Min(forceMs, SEQUENTIAL_FORCE_MS);
    bool stalled = now - job.m_fProgressAt > NEAR_BOARD_MS;
    bool recover = stalled && ((distance <= NEAR_RANGE && waited > NEAR_BOARD_MS) || (distance <= FORCE_RANGE && waited > forceMs));
    if (recover)
    {
     ACR_LiftHeli assigned = job.m_Heli;
     BaseCompartmentSlot assignedSeat = job.m_Seat;
     job.Release();
     m_Boarding.RemoveItem(job);
     if (!assignedSeat.GetOccupant() && !assignedSeat.IsReserved())
     {
      bool placed = access.GetInVehicle(assigned.m_Vehicle, assignedSeat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
      PrintFormat("[ACR] AIRLIFT recovery %1: stalled %2 s, %3 m to assigned carrier -> %4", m_sSquadName, (int)((now - job.m_fProgressAt) / 1000), (int)distance, placed);
     }
     continue;
    }
    if (ended && now - job.m_fRequestedAt >= 5000 && !access.IsGettingIn())
    {
     // Retain the progress clock across a native-path retry so repeated failures
     // cannot reset the bounded recovery forever.
     job.m_Seat.SetReserved(character);
     job.m_Action = new SCR_AIGetInVehicle(job.m_Utility, null, job.m_Heli.m_Vehicle, job.m_Seat, EAICompartmentType.Cargo,
      SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE, SCR_AIActionBase.PRIORITY_LEVEL_NORMAL);
     job.m_Utility.AddAction(job.m_Action);
     job.m_fRequestedAt = now;
    }
    continue;
   }
   // Do not replace another system's in-progress get-in animation.
   if (access.IsGettingIn()) continue;
   ACR_LiftHeli best;
   BaseCompartmentSlot seat;
   float bestDistance = float.MAX;
   foreach (ACR_LiftHeli heli : m_Helis)
   {
    if (!Loading(heli)) continue;
    BaseCompartmentSlot free;
    if (heli.FreeSeats(free, null) == 0) continue;
    float candidateDistance = vector.DistanceXZ(character.GetOrigin(), heli.m_Vehicle.GetOrigin());
    if (candidateDistance < bestDistance) { bestDistance = candidateDistance; best = heli; seat = free; }
   }
   if (!best) continue;
   AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
   if (!control || !control.GetControlAIAgent()) continue;
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(control.GetControlAIAgent().FindComponent(SCR_AIUtilityComponent));
   if (!utility) continue;
   // Respect an existing native get-in task; never pile up competing behaviors.
   if (utility.FindActionOfType(SCR_AIGetInVehicle)) continue;
   seat.SetReserved(character);
   SCR_AIGetInVehicle getIn = new SCR_AIGetInVehicle(utility, null, best.m_Vehicle, seat, EAICompartmentType.Cargo,
    SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE, SCR_AIActionBase.PRIORITY_LEVEL_NORMAL);
   utility.AddAction(getIn);
   ACR_LiftBoarding next = new ACR_LiftBoarding();
   next.m_Character = character;
   next.m_Heli = best;
   next.m_Seat = seat;
   next.m_Utility = utility;
   next.m_Action = getIn;
   next.m_fRequestedAt = now;
   next.m_fProgressAt = now;
   next.m_fBestDistance = bestDistance;
   next.m_LastPosition = character.GetOrigin();
   m_Boarding.Insert(next);
   PrintFormat("[ACR] AIRLIFT walk to board %1: carrier %2, seat %3, %4 m", m_sSquadName, m_Helis.Find(best) + 1, seat.GetCompartmentName(), (int)bestDistance);
  }
  // Count passengers physically aboard, including any newly player-controlled member.
  return Aboard();
 }

 protected bool Ours(IEntity vehicle)
 {
  foreach (ACR_LiftHeli heli : m_Helis) if (heli.m_Vehicle == vehicle) return true;
  return false;
 }

 // Squad members in a helicopter's cargo seats.
 protected int Passengers(ACR_LiftHeli heli)
 {
  int count;
  if (!m_Squad) return 0;
  array<AIAgent> agents = {};
  m_Squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity character = agent.GetControlledEntity();
   if (character && ACR_Respawn.IsAlive(character) && ACR_Transport.VehicleOf(character) == heli.m_Vehicle) count++;
  }
  return count;
 }

 protected int Aboard()
 {
  int count;
  foreach (ACR_LiftHeli heli : m_Helis) if (heli.m_Vehicle) count += Passengers(heli);
  return count;
 }

 protected void GetOutHeli(ACR_LiftHeli heli)
 {
  if (!heli || !heli.m_Vehicle) return;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(heli.m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
  foreach (BaseCompartmentSlot seat : seats)
  {
   ChimeraCharacter passenger = ChimeraCharacter.Cast(seat.GetOccupant());
   if (!passenger) continue;
   CompartmentAccessComponent access = passenger.GetCompartmentAccessComponent();
   if (!access || access.IsGettingOut()) continue;
   access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
  }
 }

 protected void PutOutHeli(ACR_LiftHeli heli)
 {
  if (!heli || !heli.m_Vehicle) return;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(heli.m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
  foreach (BaseCompartmentSlot seat : seats)
  {
   ChimeraCharacter passenger = ChimeraCharacter.Cast(seat.GetOccupant());
   if (!passenger) continue;
   CompartmentAccessComponent access = passenger.GetCompartmentAccessComponent();
   if (!access || access.IsGettingOut()) continue;
   bool ok = access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   PrintFormat("[ACR] AIRLIFT %1: put a passenger out -> %2", m_sSquadName, ok);
  }
 }

 // Whoever is still in a cargo seat (any group) is put out on the ground beside the helicopter.
 protected void PutOut()
 {
  foreach (ACR_LiftHeli heli : m_Helis) PutOutHeli(heli);
 }

 // Two helicopters taking turns on a single landing spot (aca-airlift-wave).
 protected void SequentialStep(ACR_Supports supports, float now)
 {
  if (m_Helis.Count() < 2)
  {
   m_bSequential = false;
   return;
  }
  ACR_LiftHeli h0 = m_Helis[0];
  ACR_LiftHeli h1 = m_Helis[1];
  int alive = Alive(m_Squad);

  // Handle destruction of either helicopter gracefully
  if (h0.m_bDestroyed && h0.m_iLeg < 4)
  {
   h0.m_iLeg = 4;
   if (h1 && h1.m_bHold)
   {
    if (m_iChalksOut > 0)
    {
     h1.m_Spot = m_Drops[0];
     h1.m_Target = m_Drops[0];
    }
    else
    {
     h1.m_Spot = m_Pickups[0];
     h1.m_Target = m_Pickups[0];
    }
    h1.Release();
   }
  }
  if (h1.m_bDestroyed && h1.m_iLeg < 4)
  {
   h1.m_iLeg = 4;
  }

  // Chalk 0: loading at pickup
  if (h0.m_iLeg == 0 && h0.m_bDown)
  {
   h0.m_iLeg = 1;
   h0.m_fLegAt = now;
   Stage(BOARDING, now);
   Tell(supports, string.Format("Air lift chalk 1 landed for %1 at %2.", m_sSquadName, ACR_Manager.Grid(m_Pickups[0])));
  }
  if (h0.m_iLeg == 1)
  {
   int aboard0 = Board(now);
   float waited0 = now - h0.m_fLegAt;
   if (Passengers(h0) >= h0.CargoSeats() || aboard0 >= alive || (waited0 > BOARD_WAIT_MS && Passengers(h0) > 0))
   {
    ClearBoarding(h0);
    h0.TakeOff(m_Drops[0], false);
    h0.m_iLeg = 2;
    h0.m_fLegAt = now;
    m_iChalksOut++;
    PrintFormat("[ACR] AIRLIFT %1: chalk 1 taking off with %2 passengers", m_sSquadName, Passengers(h0));
    if (h1 && !h1.m_bDestroyed && !h1.m_bAway)
    {
     h1.m_Spot = m_Pickups[0];
     h1.m_Target = m_Pickups[0];
     h1.Release();
    }
   }
   else if (waited0 > BOARD_WAIT_MS)
   {
    m_Squad.Halt("Holding");
    Leave(supports, now, "nobody reached the landing zone");
    Tell(supports, string.Format("Air lift left: %1 never reached the landing zone.", m_sSquadName));
    return;
   }
  }

  // Chalk 1: loading at pickup
  if (h1.m_iLeg == 0 && !h1.m_bHold && h1.m_bDown)
  {
   h1.m_iLeg = 1;
   h1.m_fLegAt = now;
   Tell(supports, string.Format("Air lift chalk 2 landed for %1 at %2.", m_sSquadName, ACR_Manager.Grid(m_Pickups[0])));
  }
  if (h1.m_iLeg == 1)
  {
   int aboard1 = Board(now);
   float waited1 = now - h1.m_fLegAt;
   if (Passengers(h1) >= h1.CargoSeats() || aboard1 >= alive || (waited1 > BOARD_WAIT_MS && Passengers(h1) > 0))
   {
    ClearBoarding(h1);
    h1.TakeOff(m_Drops[0], false);
    h1.m_iLeg = 2;
    h1.m_fLegAt = now;
    m_iChalksOut++;
    PrintFormat("[ACR] AIRLIFT %1: chalk 2 taking off with %2 passengers", m_sSquadName, Passengers(h1));
   }
   else if (waited1 > BOARD_WAIT_MS)
   {
    ClearBoarding(h1);
    h1.TakeOff(h1.m_Start, true);
    h1.m_iLeg = 4;
   }
  }

  // Chalk 0: arrival and unloading at drop spot
  if (h0.m_iLeg == 2 && h0.m_bDown)
  {
   h0.m_iLeg = 3;
   h0.m_fLegAt = now;
   GetOutHeli(h0);
   PrintFormat("[ACR] AIRLIFT %1: chalk 1 down at drop-off, unloading", m_sSquadName);
  }
  if (h0.m_iLeg == 3)
  {
   int still0 = Passengers(h0);
   if (still0 > 0 && now - h0.m_fLegAt > UNLOAD_WAIT_MS) PutOutHeli(h0);
   if (still0 == 0 || now - h0.m_fLegAt > UNLOAD_WAIT_MS + 5000)
   {
    h0.TakeOff(h0.m_Start, true);
    h0.m_iLeg = 4;
    PrintFormat("[ACR] AIRLIFT %1: chalk 1 departed drop-off", m_sSquadName);
    if (h1 && h1.m_bHold)
    {
     h1.m_Spot = m_Drops[0];
     h1.m_Target = m_Drops[0];
     h1.Release();
    }
   }
  }

  // Chalk 1: approach and unloading at drop spot
  if (h1.m_iLeg == 2)
  {
   if (h0.m_iLeg < 4 && !h1.m_bHold)
   {
    float distToDrop = vector.DistanceXZ(h1.m_Pos, m_Drops[0]);
    if (distToDrop < ACR_LiftHeli.HOLD_DISTANCE + 50)
    {
     h1.Hold(m_Drops[0]);
    }
   }
   if (!h1.m_bHold && h1.m_bDown)
   {
    h1.m_iLeg = 3;
    h1.m_fLegAt = now;
    GetOutHeli(h1);
    PrintFormat("[ACR] AIRLIFT %1: chalk 2 down at drop-off, unloading", m_sSquadName);
   }
  }
  if (h1.m_iLeg == 3)
  {
   int still1 = Passengers(h1);
   if (still1 > 0 && now - h1.m_fLegAt > UNLOAD_WAIT_MS) PutOutHeli(h1);
   if (still1 == 0 || now - h1.m_fLegAt > UNLOAD_WAIT_MS + 5000)
   {
    h1.TakeOff(h1.m_Start, true);
    h1.m_iLeg = 4;
    PrintFormat("[ACR] AIRLIFT %1: chalk 2 departed drop-off", m_sSquadName);
   }
  }

  // Both chalks completed drop-off
  if (h0.m_iLeg == 4 && h1.m_iLeg == 4)
  {
   Stage(UNLOADING, now);
   if (m_Squad && m_Squad.m_Group && Alive(m_Squad) > 0)
   {
    if (!m_sFinalPrefab.IsEmpty())
    {
     vector finalDest = m_FinalTarget;
     if (finalDest == vector.Zero) finalDest = m_Drops[0];
     m_Squad.RestoreOrder(ACR_OrderSnapshot.Make(m_sFinalPrefab, finalDest, m_fFinalRadius, m_sFinalOrder, ACR_Squad.StateOfLabel(m_sFinalOrder), m_iFinalKind));
    }
    else
    {
     m_Squad.Defend(m_Drops[0]);
    }
   }
   Tell(supports, string.Format("%1 landed at %2.", m_sSquadName, ACR_Manager.Grid(m_Drops[0])));
   Leave(supports, now, "done");
  }
 }

 override void Finish()
 {
  m_bDone = true;
  ClearBoarding();
  foreach (ACR_LiftHeli heli : m_Helis)
  {
   if (!heli.m_Vehicle || heli.m_bDestroyed) continue;
   if (Passengers(heli) > 0) PrintFormat("[ACR] AIRLIFT %1: %2 still aboard when it ended", m_sSquadName, Passengers(heli));
   heli.Finish();
  }
  PrintFormat("[ACR] AIRLIFT %1 finished (stage %2)", m_sSquadName, m_iStage);
 }
}
