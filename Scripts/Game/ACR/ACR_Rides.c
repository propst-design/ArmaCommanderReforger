// Rides: an idle transport (a crewed ground vehicle with no gun of its own, like
// a truck) picks a squad up where it stands, drives it to a drop-off point and
// lets it carry on from there on foot, then heads back to a safe base.
// Used by the AI commander for long trips and by players with the "Ride" order.
// A transport is only fetched from nearby: never more than MAX_PICKUP metres
// away, and never farther than half the trip itself.
enum ACR_ERideState
{
 TO_PICKUP,
 BOARDING,
 TO_DROP
}

class ACR_Ride
{
 ACR_Squad m_Rider;
 ACR_Squad m_Taxi;
 Vehicle m_Vehicle;
 string m_sFaction;
 vector m_Drop;
 // what the riders do once dropped (no prefab: take positions there)
 ResourceName m_sFinalPrefab;
 vector m_Final;
 float m_fFinalRadius;
 string m_sFinalOrder;
 int m_iFinalKind;
 int m_State;
 float m_fStateSince;
 float m_fDriveBudgetMs;
 int m_iRiderSerial;
 int m_iTaxiSerial;
 float m_fTaxiLostSince = -1;   // crew briefly out (seat swaps, a stumble): only give up after a while
 float m_fStoppedSince = -1;    // escort: when the carrier came to a standstill near the drop-off
 bool m_bEscort;                // an armed carrier (AI motorized assault): stays at the drop-off instead of going home
 vector m_Pickup;
 // A player in the riders (possession, 23 Oct; Jacob: taking over a riding squad cancelled the ride).
 bool m_bLedByPlayer;           // a player took over their leader: they follow him, the ride goes on
 bool m_bPlayerAboard;          // a player rider has been in the transport this leg
 float m_fUnloadSince = -1;     // dropped off with riders still aboard: the transport waits for them
}

class ACR_Rides : Managed
{
 static const float WASTED_SEAT_M = 30;        // m: penalty per empty seat when picking a transport
 static const float MAX_PICKUP = 800;          // m: farthest a transport is fetched from
 static const float PICKUP_SHARE = 0.5;        // ... and no farther than this share of the trip
 static const float MAX_PICKUP_AI = 1500;      // m: wider motor-pool fetch for long trips
 static const float PICKUP_SHARE_AI = 0.75;
 static const float MIN_TRIP = 400;            // m: shorter than this, just walk
 static const float PICKUP_REACHED = 35;       // m from the squad to stop and let them board
 static const float DROP_REACHED = 60;         // m from the drop-off point
 static const float PICKUP_TIMEOUT_MS = 240000;
 static const float BOARDING_TIMEOUT_MS = 75000;
 static const float STRAGGLER_WAIT_MS = 20000;  // then leave without riders this far away
 static const float STRAGGLER_DISTANCE = 60;
 // Leave once this share is aboard and the rest have had this long (1 Oct MP: two rides
 // gave up after 75 s with 7 and 8 of 9 aboard, the last ones stuck at the truck).
 static const float MOSTLY_ABOARD_SHARE = 0.75;
 static const float STUCK_BOARDING_MS = 20000; // then place the stragglers within 40 m straight in
 static const float MOSTLY_ABOARD_WAIT_MS = 30000;
 static const float ESCORT_DROP_NEAR = 250;    // m: an assault carrier stopped this close drops its riders
 static const float ESCORT_STOPPED_MS = 15000;
 static const float PLAYER_UNLOAD_MS = 60000;  // dropped off: the transport waits this long for a player to get out
 static const float RIDERS_UNLOAD_MS = 20000;  // ...and this long for AI riders (the dismount's sweeps take 9 s)

 protected ACR_Manager m_Manager;
 ref array<ref ACR_Ride> m_Rides = {};
 ref array<ref ACR_Ride> m_Unloading = {};    // rides over, transport waiting for its riders to get out
 ref map<string, float> m_NoTransportAt = new map<string, float>(); // faction -> last time a ride found no transport

 void ACR_Rides(ACR_Manager manager)
 {
  m_Manager = manager;
 }

 // ---------------------------------------------------------------- queries

 static string FactionOf(ACR_Squad squad)
 {
  if (!squad || !squad.GetFaction()) return "";
  return squad.GetFaction().GetFactionKey();
 }

 static int FreeSeats(IEntity vehicle)
 {
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
  int free;
  foreach (BaseCompartmentSlot seat : seats)
   if (!seat.GetOccupant()) free++;
  return free;
 }

 // Passenger seats, taken or not.
 static int CargoSeats(IEntity vehicle)
 {
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
  return seats.Count();
 }

 // A fighting vehicle that carries a squad (an APC or IFV: a LAV-25, a BTR-70), as the AI's assault
 // carriers (ACR_AICommander.PairCarriers): this many passenger seats or more.
 static const int CARRIER_SEATS = 4;

 ACR_Ride RideOf(ACR_Squad squad)
 {
  foreach (ACR_Ride ride : m_Rides)
   if (ride.m_Rider == squad || ride.m_Taxi == squad) return ride;
  return null;
 }

 bool Unloading(ACR_Squad taxi)
 {
  foreach (ACR_Ride ride : m_Unloading)
   if (ride.m_Taxi == taxi) return true;
  return false;
 }

 // A squad that can serve as a taxi: crews a drivable ground vehicle with no
 // manned gun, AI-driven, not busy with anything but holding or heading home.
 // carriersToo (a player's Ride order): an APC or IFV with its gun manned too (CARRIER_SEATS).
 bool IsFreeTransport(ACR_Squad squad, string faction, bool carriersToo = false)
 {
  return WhyNotTaxi(squad, faction, carriersToo).IsEmpty();
 }

 // "" = can serve as a taxi; otherwise why not (told to the player when a ride is refused).
 string WhyNotTaxi(ACR_Squad squad, string faction, bool carriersToo = false)
 {
  if (FactionOf(squad) != faction) return "other side";
  if (squad.HasPlayer()) return "led by a player";
  if (RideOf(squad) || Unloading(squad)) return "already giving a ride";
  IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
  if (!vehicle) return "no vehicle";
  // A fighting vehicle isn't a taxi for the AI's rides. A player's own Ride order may take one that
  // carries a squad (pn10b, 26 Oct: Fire Team 2's Ride was refused with an idle LAV-25 15 m away,
  // "has a manned gun", which read as the LAV having no seats).
  if (squad.HasGunner(vehicle) && (!carriersToo || CargoSeats(vehicle) < CARRIER_SEATS)) return "a fighting vehicle (manned gun)";
  if (ACR_Logistics.ServiceOf(vehicle) != 0) return "service vehicle"; // ammo/repair/fuel trucks have their own job
  if (!CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent))) return "not a ground vehicle";
  ChimeraCharacter driver = ACR_Squad.DriverOf(vehicle);
  if (!driver || !ACR_Respawn.IsAlive(driver)) return "no driver";
  if (ACR_Squad.FindController(driver) != 0) return "a player is driving";
  if (m_Manager.IsGunnerDriven(vehicle)) return "driven from the gun";
  // Idle, holding or defending somewhere, or heading home from an earlier ride.
  ACR_EOrderState state = squad.OrderState();
  if (state == ACR_EOrderState.HOLD || state == ACR_EOrderState.DEFEND || state == ACR_EOrderState.RETURN) return "";
  if (squad.m_Group != null && squad.m_Group.GetCurrentWaypoint() == null) return "";
  return "busy with an order";
 }

 // For a refused ride: the nearest of our crewed vehicles and why it can't come.
 protected string NearestTaxiNote(ACR_Squad rider, string faction, vector pickup, float limit, int seatsNeeded, bool carriersToo = false)
 {
  ACR_Squad nearest;
  float best = float.MAX;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad == rider || FactionOf(squad) != faction) continue;
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   if (!vehicle) continue;
   float d = vector.DistanceXZ(vehicle.GetOrigin(), pickup);
   if (d < best) { best = d; nearest = squad; }
  }
  if (!nearest) return " (we have no crewed vehicles)";
  string why = WhyNotTaxi(nearest, faction, carriersToo);
  if (why.IsEmpty() && best > limit) why = "too far";
  if (why.IsEmpty()) why = string.Format("only %1 free seats", FreeSeats(ACR_Transport.CrewedVehicle(nearest)));
  return string.Format(" (the nearest, %1, %2 m away: %3)", nearest.GetName(), (int)best, why);
 }

 // Nearest free transport with room for everyone, within the fetch limits ('except': taxis already
 // counted for others, ACR_AICommander.Reach).
 ACR_Squad FindTransport(string faction, vector pickup, float trip, int seatsNeeded, out float distance, bool wideFetch = false, array<ACR_Squad> except = null, bool carriersToo = false)
 {
  float maxFetch = MAX_PICKUP;
  float shareFetch = PICKUP_SHARE;
  if (wideFetch)
  {
   maxFetch = MAX_PICKUP_AI;
   shareFetch = PICKUP_SHARE_AI;
  }
  float limit = Math.Min(maxFetch, trip * shareFetch);
  ACR_Squad best;
  distance = float.MAX;
  float bestScore = float.MAX;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!IsFreeTransport(squad, faction, carriersToo)) continue;
   if (except && except.Contains(squad)) continue;
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   float d = vector.DistanceXZ(vehicle.GetOrigin(), pickup);
   if (d > limit) continue;
   int seats = FreeSeats(vehicle);
   if (seats < seatsNeeded) continue;
   // Right-sized first: every empty seat counts as WASTED_SEAT_M metres farther,
   // so two men take the jeep next door rather than a truck just as close.
   float score = d + (seats - seatsNeeded) * WASTED_SEAT_M;
   if (score >= bestScore) continue;
   bestScore = score;
   distance = d;
   best = squad;
  }
  return best;
 }

 // Recently needed a transport and found none (the AI commander buys one).
 bool ShortOfTransport(string faction)
 {
  if (!m_NoTransportAt.Contains(faction)) return false;
  return GetGame().GetWorld().GetWorldTime() - m_NoTransportAt.Get(faction) < 180000;
 }

 int CountTransports(string faction)
 {
  int count;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (FactionOf(squad) != faction) continue;
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   if (vehicle && !squad.HasGunner(vehicle) && FreeSeats(vehicle) >= 4) count++;
  }
  return count;
 }

 // ---------------------------------------------------------------- requests

 // Starts a ride, or returns null with the reason in why.
 // dropShort: how far short of the destination to get out. carriersToo: a player's Ride order, which
 // may take an APC or IFV (WhyNotTaxi).
 ACR_Ride Request(ACR_Squad rider, vector destination, float dropShort, ResourceName finalPrefab, float finalRadius, string finalOrder, int finalKind, out string why, bool wideFetch = false, bool carriersToo = false)
 {
  why = "";
  IEntity anchor = rider.GetAnchor();
  string faction = FactionOf(rider);
  if (!anchor || faction.IsEmpty()) { why = "not ready"; return null; }
  if (rider.IsMounted()) { why = "already mounted"; return null; }
  vector from = anchor.GetOrigin();
  float trip = vector.DistanceXZ(from, destination);
  if (trip < MIN_TRIP) { why = string.Format("too close to need a ride (under %1 m)", (int)MIN_TRIP); return null; }
  // After the trip check: a refused short hop kept sending the squad's waiting truck home. Its own
  // taxi has to be free again before FindTransport, so the rest still cancels first.
  CancelFor(rider);
  int seats = rider.CountAlive();
  float fetch;
  ACR_Squad taxi = FindTransport(faction, from, trip, seats, fetch, wideFetch, null, carriersToo);
  if (!taxi)
  {
   m_NoTransportAt.Set(faction, GetGame().GetWorld().GetWorldTime());
   float maxFetch = MAX_PICKUP;
   float shareFetch = PICKUP_SHARE;
   if (wideFetch)
   {
    maxFetch = MAX_PICKUP_AI;
    shareFetch = PICKUP_SHARE_AI;
   }
   float limit = Math.Min(maxFetch, trip * shareFetch);
   why = string.Format("no free transport with %1 seats within %2 m", seats, (int)limit) + NearestTaxiNote(rider, faction, from, limit, seats, carriersToo);
   return null;
  }
  // Only worth it if driving beats walking. Where the roads (which the game's own
  // drivers follow too) go a long way round, a truck took 3 minutes and 2 km for a
  // 600 m hop: then they walk.
  array<vector> roadLegs = {};
  int roadNodes, roadMillis;
  BBB_Roads.Route(from, destination, roadLegs, roadNodes, roadMillis);
  if (BBB_Roads.s_bLastDetour)
  {
   float driveSeconds = fetch / 4 + 45 + BBB_Roads.s_fLastLength / 4; // trucks average about 4 m/s door to door
   float walkSeconds = trip / 1.4;
   if (driveSeconds > walkSeconds * 0.8)
   {
    why = string.Format("no quick road there (%1 m by road for %2 m): walking is faster", (int)BBB_Roads.s_fLastLength, (int)trip);
    return null;
   }
  }
  return StartRide(rider, taxi, from, trip, fetch, destination, dropShort, finalPrefab, finalRadius, finalOrder, finalKind, false);
 }

 // AI motorized assault (5 Oct; Jacob: APCs carrying infantry into attacks): a ride in a given
 // armed carrier (gunner aboard, so never a taxi otherwise) that stays at the drop-off with
 // the riders instead of driving home. null when it can't carry them.
 ACR_Ride RequestCarrier(ACR_Squad rider, ACR_Squad carrier, vector destination, ResourceName finalPrefab, float finalRadius, string finalOrder, int finalKind, out string why)
 {
  why = "";
  IEntity anchor = rider.GetAnchor();
  IEntity vehicle = ACR_Transport.CrewedVehicle(carrier);
  string faction = FactionOf(rider);
  if (!anchor || !vehicle || faction.IsEmpty() || FactionOf(carrier) != faction) { why = "not ready"; return null; }
  if (rider.IsMounted() || RideOf(rider) || RideOf(carrier)) { why = "busy"; return null; }
  if (FreeSeats(vehicle) < rider.CountAlive()) { why = string.Format("%1 seats for %2 men", FreeSeats(vehicle), rider.CountAlive()); return null; }
  if (!CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent))) { why = "not a ground vehicle"; return null; }
  ChimeraCharacter driver = ACR_Squad.DriverOf(vehicle);
  if (!driver || !ACR_Respawn.IsAlive(driver) || ACR_Squad.FindController(driver) != 0 || m_Manager.IsGunnerDriven(vehicle)) { why = "no AI driver"; return null; }
  vector from = anchor.GetOrigin();
  float trip = vector.DistanceXZ(from, destination);
  if (trip < MIN_TRIP) { why = "too close"; return null; }
  float fetch = vector.DistanceXZ(vehicle.GetOrigin(), from);
  if (fetch > MAX_PICKUP) { why = "carrier too far"; return null; }
  // Same rule as other rides: not when the roads go so far round that walking is faster.
  array<vector> roadLegs = {};
  int roadNodes, roadMillis;
  BBB_Roads.Route(from, destination, roadLegs, roadNodes, roadMillis);
  if (BBB_Roads.s_bLastDetour)
  {
   float driveSeconds = fetch / 4 + 45 + BBB_Roads.s_fLastLength / 4;
   if (driveSeconds > trip / 1.4 * 0.8) { why = string.Format("no quick road (%1 m by road for %2 m)", (int)BBB_Roads.s_fLastLength, (int)trip); return null; }
  }
  CancelFor(rider);
  return StartRide(rider, carrier, from, trip, fetch, destination, 0, finalPrefab, finalRadius, finalOrder, finalKind, true);
 }

 protected ACR_Ride StartRide(ACR_Squad rider, ACR_Squad taxi, vector from, float trip, float fetch, vector destination, float dropShort, ResourceName finalPrefab, float finalRadius, string finalOrder, int finalKind, bool escort)
 {
  string faction = FactionOf(rider);
  ACR_Ride ride = new ACR_Ride();
  ride.m_bEscort = escort;
  ride.m_Rider = rider;
  ride.m_Taxi = taxi;
  ride.m_Vehicle = Vehicle.Cast(ACR_Transport.CrewedVehicle(taxi));
  ride.m_sFaction = faction;
  vector dir = destination - from;
  dir[1] = 0;
  dir.Normalize();
  ride.m_Drop = ACR_Manager.Ground(destination - dir * Math.Min(dropShort, trip * 0.5));
  // Pick up and drop off on roads when there is one nearby: the game's AI
  // drivers often can't reach off-road spots and abandon the vehicle.
  vector road;
  // (An assault carrier only within 120 m: its drop-off is the staging point short of the enemy.)
  float roadReach = 350;
  if (escort) roadReach = 120;
  if (BBB_Terrain.RoadPointNear(ride.m_Drop, roadReach, road)) ride.m_Drop = road;
  vector pickup = from;
  if (BBB_Terrain.RoadPointNear(from, 250, road)) pickup = road;
  ride.m_Pickup = pickup;
  ride.m_sFinalPrefab = finalPrefab;
  ride.m_Final = destination;
  ride.m_fFinalRadius = finalRadius;
  ride.m_sFinalOrder = finalOrder;
  ride.m_iFinalKind = finalKind;
  ride.m_fDriveBudgetMs = (trip / 4 + 150) * 1000; // generous: 4 m/s average plus slack
  RiderOrder(ride);
  taxi.SetMode(ACR_EMoveMode.FAST);
  taxi.RouteTo(pickup, ACR_Squad.WP_MOVE, -1, "Picking up " + rider.GetName(), 0);
  ride.m_iRiderSerial = rider.m_iOrderSerial;
  ride.m_iTaxiSerial = taxi.m_iOrderSerial;
  SetState(ride, ACR_ERideState.TO_PICKUP);
  m_Rides.Insert(ride);
  PrintFormat("[ACR] RIDE %1 -> %2: %3 fetched from %4 m, trip %5 m, pickup %6 drop %7", rider.GetName(), ACR_Manager.Grid(destination), taxi.GetName(), (int)fetch, (int)trip, pickup, ride.m_Drop);
  return ride;
 }

 // A new order for either squad ends its ride (riders already aboard get out).
 void CancelFor(ACR_Squad squad)
 {
  for (int i = m_Rides.Count() - 1; i >= 0; i--)
  {
   ACR_Ride ride = m_Rides[i];
   if (ride.m_Rider != squad && ride.m_Taxi != squad) continue;
   m_Rides.RemoveOrdered(i);
   if (ride.m_Taxi && ride.m_Taxi != squad) SendHome(ride);
  }
 }

 // ---------------------------------------------------------------- per second

 void Tick()
 {
  for (int i = m_Rides.Count() - 1; i >= 0; i--)
  {
   ACR_Ride ride = m_Rides[i];
   if (!Step(ride)) m_Rides.RemoveOrdered(i);
  }
  for (int u = m_Unloading.Count() - 1; u >= 0; u--)
  {
   ACR_Ride unload = m_Unloading[u];
   if (!WaitForRidersOut(unload)) m_Unloading.RemoveOrdered(u);
  }
 }

 // The riders' own order for where the ride stands (also when a player stops leading them).
 protected void RiderOrder(ACR_Ride ride)
 {
  ACR_Squad rider = ride.m_Rider;
  IEntity anchor = rider.GetAnchor();
  if (ride.m_State == ACR_ERideState.TO_DROP) rider.Halt("Riding", ACR_EOrderState.RIDE);
  else if (ride.m_State == ACR_ERideState.BOARDING && ride.m_Vehicle)
  {
   rider.MoveTo(ride.m_Vehicle.GetOrigin());
   rider.SetOrder("Boarding", ACR_EOrderState.RIDE);
  }
  else if (anchor && vector.DistanceXZ(anchor.GetOrigin(), ride.m_Pickup) > 5)
  {
   rider.MoveTo(ride.m_Pickup);
   rider.SetOrder("Waiting for ride", ACR_EOrderState.RIDE);
  }
  else rider.Halt("Waiting for ride", ACR_EOrderState.RIDE);
  ride.m_iRiderSerial = rider.m_iOrderSerial;
 }

 // Possessed riders: how many are in the transport, and how many alive outside it.
 protected int PlayersAboard(ACR_Ride ride, out int outside)
 {
  outside = 0;
  int inside;
  array<AIAgent> agents = {};
  ride.m_Rider.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body) || ACR_Squad.FindController(body) == 0) continue;
   if (ride.m_Vehicle && ACR_Transport.VehicleOf(body) == ride.m_Vehicle) inside++;
   else outside++;
  }
  return inside;
 }

 // Dropped off with riders aboard: the transport stays put until they are out (a player up to a
 // minute, AI riders up to RIDERS_UNLOAD_MS), then home. Before, it drove home with AI riders still
 // getting out, and an LAV's rear seats don't let them out while it moves (#23).
 protected bool WaitForRidersOut(ACR_Ride ride)
 {
  ACR_Squad taxi = ride.m_Taxi;
  if (!taxi || !ride.m_Vehicle || !m_Manager.m_Squads.Contains(taxi) || taxi.m_iOrderSerial != ride.m_iTaxiSerial) return false;
  bool riders = ride.m_Rider != null && m_Manager.m_Squads.Contains(ride.m_Rider);
  int outside, alive;
  bool player = riders && PlayersAboard(ride, outside) > 0;
  int aboard = 0;
  if (riders) aboard = Aboard(ride, alive);
  float waited = GetGame().GetWorld().GetWorldTime() - ride.m_fUnloadSince;
  if (player && waited < PLAYER_UNLOAD_MS) return true;
  if (aboard > 0 && !player && waited < RIDERS_UNLOAD_MS) return true;
  if (player) Log(ride, "a player is still aboard, the transport leaves with him");
  else if (aboard > 0) Log(ride, string.Format("%1 still aboard after %2 s, the transport leaves with them", aboard, (int)(waited / 1000)));
  else Log(ride, string.Format("riders out after %1 s, the transport goes", (int)(waited / 1000)));
  SendHome(ride);
  return false;
 }

 protected void SetState(ACR_Ride ride, int state)
 {
  ride.m_State = state;
  ride.m_fStateSince = GetGame().GetWorld().GetWorldTime();
 }

 protected int Aboard(ACR_Ride ride, out int alive)
 {
  alive = 0;
  int aboard;
  array<AIAgent> agents = {};
  ride.m_Rider.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body)) continue;
   alive++;
   if (ride.m_Vehicle && ACR_Transport.VehicleOf(body) == ride.m_Vehicle) aboard++;
  }
  return aboard;
 }

 // Riders not aboard, none of them anywhere near the transport (stuck at a spawn,
 // wandered off): the ride shouldn't wait for them.
 protected int Stragglers(ACR_Ride ride)
 {
  int distant;
  array<AIAgent> agents = {};
  ride.m_Rider.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body) || ACR_Transport.VehicleOf(body) == ride.m_Vehicle) continue;
   if (vector.DistanceXZ(body.GetOrigin(), ride.m_Vehicle.GetOrigin()) <= STRAGGLER_DISTANCE) return -1;
   distant++;
  }
  return distant;
 }

 // Returns false when the ride is over.
 protected bool Step(ACR_Ride ride)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  // (A player among the riders is fine: possessing one of them doesn't call the ride off.)
  bool riderOk = ride.m_Rider != null && m_Manager.m_Squads.Contains(ride.m_Rider) && ride.m_Rider.IsAlive();
  ChimeraCharacter driver = ACR_Squad.DriverOf(ride.m_Vehicle);
  bool taxiOk = ride.m_Taxi != null && ride.m_Vehicle != null && driver != null && m_Manager.m_Squads.Contains(ride.m_Taxi);
  if (taxiOk) taxiOk = ACR_Respawn.IsAlive(driver) && ACR_Transport.CrewedVehicle(ride.m_Taxi) == ride.m_Vehicle && !ride.m_Taxi.HasPlayer();
  if (!riderOk)
  {
   ReleaseTaxi(ride);
   return false;
  }
  int alive;
  int aboard = Aboard(ride, alive);
  if (taxiOk) ride.m_fTaxiLostSince = -1;
  else if (ride.m_Vehicle && ACR_Logistics.Health(ride.m_Vehicle) > 0 && ride.m_Taxi && m_Manager.m_Squads.Contains(ride.m_Taxi))
  {
   if (ride.m_fTaxiLostSince < 0) ride.m_fTaxiLostSince = now;
   // (45 s: the game's AI driver may hop out to open a gate the gate helper missed; 2 s if dead)
   float patience = 15000;
   if (driver && !ACR_Respawn.IsAlive(driver)) patience = 2000;
   else if (ride.m_Taxi.IsAlive() && !ride.m_Taxi.HasPlayer()) patience = 45000;
   if (now - ride.m_fTaxiLostSince < patience) return true;
  }
  if (!taxiOk)
  {
   Log(ride, string.Format("transport lost (driver %1 alive %2, crewing %3), continuing on foot", driver != null, (driver != null && ACR_Respawn.IsAlive(driver)), (ride.m_Taxi != null && ACR_Transport.CrewedVehicle(ride.m_Taxi) == ride.m_Vehicle)));
   // The crew still has our pickup/drop route: stop it, or the vehicle would
   // carry on empty once its driver gets back in.
   ReleaseTaxi(ride);
   Finish(ride, aboard > 0);
   return false;
  }
  // A player took over the riders' leader: they follow him now (not a new order), and he
  // rides along. When he lets go, they get the ride's own order again.
  ACR_Squad led = ride.m_Rider;
  if (led.m_iOrderSerial != ride.m_iRiderSerial && led.m_iOrderSerial == led.m_iCommandSerial)
  {
   ride.m_iRiderSerial = led.m_iOrderSerial;
   ride.m_bLedByPlayer = true;
   Log(ride, "a player leads the riders, the ride goes on");
  }
  else if (ride.m_bLedByPlayer && led.m_Commander == 0 && led.m_iOrderSerial == ride.m_iRiderSerial)
  {
   ride.m_bLedByPlayer = false;
   RiderOrder(ride);
   Log(ride, "the player let go of the riders, back on the ride's order");
  }
  // Someone else gave either squad a new order: the ride is off.
  if (ride.m_Rider.m_iOrderSerial != ride.m_iRiderSerial || ride.m_Taxi.m_iOrderSerial != ride.m_iTaxiSerial)
  {
   // (the transport crew bailing out also lands here, with the riders already told to get out)
   bool riderReordered = ride.m_Rider.m_iOrderSerial != ride.m_iRiderSerial && ride.m_Rider.OrderState() != ACR_EOrderState.DISMOUNT;
   if (riderReordered) Log(ride, "cancelled: the riders got a new order");
   else Log(ride, "cancelled: the transport crew was given other orders or bailed out");
   if (ride.m_Taxi.m_iOrderSerial == ride.m_iTaxiSerial) SendHome(ride);
   // Riders thrown out on the way (the transport got stuck or hit) still go on to their objective.
   ACR_Squad rider = ride.m_Rider;
   if (ride.m_State == ACR_ERideState.TO_DROP && rider.OrderState() == ACR_EOrderState.DISMOUNT)
    rider.ContinueAfterDismount(ride.m_sFinalPrefab, ride.m_Final, ride.m_fFinalRadius, ride.m_sFinalOrder, ride.m_iFinalKind);
   // Riders given a job while aboard (mines, a roadblock, a Fortify order): out, the job left to carry on on foot
   // (DismountAndContinue's Halt would end it, refunded).
   else if (aboard > 0 && riderReordered && rider.HasJob())
    rider.GetOutForJob();
   // Riders given a new order while aboard: out first, then carry it out on foot.
   else if (aboard > 0 && riderReordered && rider.m_sOrderPrefab != ACR_Squad.WP_GET_OUT)
    rider.DismountAndContinue(rider.m_sOrderPrefab, rider.m_MoveTarget, rider.m_fOrderRadius, rider.m_Order, rider.m_OrderKind);
   // The transport let them down, not a new order: carry on to the objective on foot
   // instead of standing around where the ride ended.
   else if (!riderReordered)
    Finish(ride, aboard > 0);
   return false;
  }
  float inState = now - ride.m_fStateSince;
  IEntity riderAnchor = ride.m_Rider.GetAnchor();
  switch (ride.m_State)
  {
   case ACR_ERideState.TO_PICKUP:
   {
    if (riderAnchor && vector.DistanceXZ(ride.m_Vehicle.GetOrigin(), riderAnchor.GetOrigin()) < PICKUP_REACHED)
    {
     ride.m_Taxi.Halt("Loading " + ride.m_Rider.GetName());
     ride.m_iTaxiSerial = ride.m_Taxi.m_iOrderSerial;
     SetState(ride, ACR_ERideState.BOARDING);
     // (A player leading them brings them over himself; seats are filled within 40 m.)
     if (!ride.m_bLedByPlayer) RiderOrder(ride);
     Log(ride, "transport arrived, boarding");
     return true;
    }
    if (inState > PICKUP_TIMEOUT_MS)
    {
     Log(ride, "transport never arrived, walking");
     SendHome(ride);
     Finish(ride, false);
     return false;
    }
    return true;
   }
   case ACR_ERideState.BOARDING:
   {
    if (alive > 0 && aboard < alive)
     ride.m_Rider.BoardCargo(ride.m_Vehicle, 40, inState > 8000, inState > STUCK_BOARDING_MS);
    int leftBehind = 0;
    if (alive > 0 && aboard > 0 && aboard < alive && inState > STRAGGLER_WAIT_MS) leftBehind = Stragglers(ride);
    bool mostly = alive > 0 && aboard < alive && inState > MOSTLY_ABOARD_WAIT_MS && MostlyAboard(aboard, alive);
    // Never off without a player rider: he climbs in himself.
    int playersOutside;
    PlayersAboard(ride, playersOutside);
    if (alive > 0 && playersOutside == 0 && (aboard >= alive || leftBehind > 0 || mostly))
    {
     if (leftBehind > 0) Log(ride, string.Format("leaving without %1 rider(s) more than %2 m away", leftBehind, STRAGGLER_DISTANCE));
     else if (mostly) Log(ride, string.Format("leaving with %1/%2 aboard", aboard, alive));
     // (A player leading them keeps them following him.)
     if (!ride.m_bLedByPlayer)
     {
      ride.m_Rider.Halt("Riding", ACR_EOrderState.RIDE);
      ride.m_iRiderSerial = ride.m_Rider.m_iOrderSerial;
     }
     ride.m_Taxi.RouteTo(ride.m_Drop, ACR_Squad.WP_MOVE, -1, "Driving " + ride.m_Rider.GetName(), 0);
     ride.m_iTaxiSerial = ride.m_Taxi.m_iOrderSerial;
     SetState(ride, ACR_ERideState.TO_DROP);
     Log(ride, "aboard, on the way");
     return true;
    }
    if (inState > BOARDING_TIMEOUT_MS)
    {
     if (playersOutside > 0) Log(ride, string.Format("the player never got in (%1/%2 aboard), off on foot", aboard, alive));
     else Log(ride, string.Format("boarding took too long (%1/%2 aboard), walking", aboard, alive));
     SendHome(ride);
     Finish(ride, aboard > 0);
     return false;
    }
    return true;
   }
   case ACR_ERideState.TO_DROP:
   {
    // A player rider who gets out on the way ends the ride there: his squad gets out with him.
    int playersOff;
    int playersOn = PlayersAboard(ride, playersOff);
    if (playersOn > 0) ride.m_bPlayerAboard = true;
    else if (ride.m_bPlayerAboard && playersOff > 0 && aboard > 0)
    {
     Log(ride, "a player got out on the way, everyone out here");
     Finish(ride, true);
     SendHome(ride);
     return false;
    }
    if (aboard == 0)
    {
     Log(ride, "riders are out early, continuing on foot");
     SendHome(ride);
     Finish(ride, false);
     return false;
    }
    float toDrop = vector.DistanceXZ(ride.m_Vehicle.GetOrigin(), ride.m_Drop);
    bool arrived = toDrop < DROP_REACHED;
    // An assault carrier that has come to a standstill near its drop-off (stuck on a fence or
    // a bend) lets its riders out there: close enough to form up.
    if (ride.m_bEscort && !arrived && toDrop < ESCORT_DROP_NEAR)
    {
     if (ride.m_Vehicle.GetPhysics() && ride.m_Vehicle.GetPhysics().GetVelocity().Length() < 0.5)
     {
      if (ride.m_fStoppedSince < 0) ride.m_fStoppedSince = now;
      else if (now - ride.m_fStoppedSince > ESCORT_STOPPED_MS) { arrived = true; Log(ride, string.Format("carrier stopped %1 m short, dropping here", (int)toDrop)); }
     }
     else ride.m_fStoppedSince = -1;
    }
    if (arrived || inState > ride.m_fDriveBudgetMs)
    {
     if (arrived) Log(ride, "dropped off");
     else Log(ride, "drive took too long, getting out here");
     Finish(ride, true);
     // Wait for the riders to get out rather than drive off with them (a player gets out himself).
     if (aboard > 0 && !ride.m_bEscort)
     {
      ride.m_Taxi.Halt("Unloading");
      ride.m_iTaxiSerial = ride.m_Taxi.m_iOrderSerial;
      ride.m_fUnloadSince = now;
      m_Unloading.Insert(ride);
      return false;
     }
     SendHome(ride);
     return false;
    }
    return true;
   }
  }
  return false;
 }

 // Riders carry out their final order (getting out first if aboard).
 protected void Finish(ACR_Ride ride, bool aboard)
 {
  ACR_Squad rider = ride.m_Rider;
  // Led by a player: he decides what comes next; they get out (if aboard) and follow him.
  if (rider.m_Commander != 0)
  {
   if (aboard) rider.DismountAndContinue("", ride.m_Final, ride.m_fFinalRadius, ride.m_sFinalOrder, ride.m_iFinalKind);
   else rider.FollowCommander();
   return;
  }
  if (aboard)
  {
   rider.DismountAndContinue(ride.m_sFinalPrefab, ride.m_Final, ride.m_fFinalRadius, ride.m_sFinalOrder, ride.m_iFinalKind);
   string faction = FactionOf(rider);
   if (!faction.IsEmpty()) m_Manager.NotifyFaction(faction, rider.GetName() + ": dropped off at " + ACR_Manager.Grid(ride.m_Vehicle.GetOrigin()));
   return;
  }
  if (ride.m_sFinalPrefab.IsEmpty())
  {
   IEntity anchor = rider.GetAnchor();
   if (anchor) rider.Defend(anchor.GetOrigin());
   return;
  }
  if (ride.m_sFinalPrefab == ACR_Squad.WP_DEFEND) rider.Defend(ride.m_Final);
  else if (ride.m_sFinalPrefab == ACR_Squad.WP_GARRISON) rider.Garrison(ride.m_Final);
  else if (ride.m_sFinalPrefab == ACR_Squad.WP_SEARCH_DESTROY) rider.Assault(ride.m_Final);
  else rider.MoveTo(ride.m_Final);
 }

 // Ride over: if the transport is still following this ride's orders, send it
 // home (or hold, when its crew is out of the vehicle).
 protected void ReleaseTaxi(ACR_Ride ride)
 {
  ACR_Squad taxi = ride.m_Taxi;
  if (!taxi || !m_Manager.m_Squads.Contains(taxi) || taxi.m_iOrderSerial != ride.m_iTaxiSerial) return;
  if (ride.m_Vehicle && ACR_Transport.CrewedVehicle(taxi) == ride.m_Vehicle) SendHome(ride);
  else taxi.Halt("Holding");
 }

 // Empty transport back to the nearest safe base of its side (or where it is).
 protected void SendHome(ACR_Ride ride)
 {
  ACR_Squad taxi = ride.m_Taxi;
  if (!taxi || !ride.m_Vehicle || !m_Manager.m_Squads.Contains(taxi)) return;
  // An assault carrier stays with its riders; the AI commander gives it its next order.
  if (ride.m_bEscort)
  {
   taxi.Halt("Holding");
   return;
  }
  vector here = ride.m_Vehicle.GetOrigin();
  ACR_Base home;
  float best = float.MAX;
  float bestScore = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != ride.m_sFaction || base.m_bContested) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), here);
   float score = d;
   if (ACR_LogisticsRules.CanDeploy(base)) score -= 250;
   foreach (ACR_Squad foot : m_Manager.m_Squads)
   {
    if (FactionOf(foot) != ride.m_sFaction || foot.IsMounted() || ACR_Transport.CrewedVehicle(foot) || foot.m_bMortar || foot.Aircraft()) continue;
    IEntity footAnchor = foot.GetAnchor();
    if (footAnchor && vector.DistanceXZ(footAnchor.GetOrigin(), base.GetOrigin()) <= base.m_fRadius + 150)
    {
     score -= 450;
     break;
    }
   }
   if (score < bestScore) { bestScore = score; best = d; home = base; }
  }
  if (!home || best < 80)
  {
   taxi.Halt("Holding");
   return;
  }
  // To a road point by the base, along the roads: the base centre is often somewhere the
  // game's driver can't reach, and he got out there and left the truck (7 Oct live).
  taxi.RouteTo(ACR_Manager.Ground(home.GetOrigin()), ACR_Squad.WP_MOVE, -1, "Returning", ACR_EOrderKind.MOVE);
  taxi.SetOrder("Returning", ACR_EOrderState.RETURN);
 }

 static bool MostlyAboard(int aboard, int alive)
 {
  // Compare as floats: with an int on the left, Enforce did the sum and the comparison in ints.
  float share = aboard;
  share /= alive;
  return alive > 0 && share >= MOSTLY_ABOARD_SHARE;
 }

 // (Either squad may already be gone: the taxi's crew can die with the ride under way.)
 protected void Log(ACR_Ride ride, string what)
 {
  string rider = "?", taxi = "?";
  if (ride.m_Rider) rider = ride.m_Rider.GetName();
  if (ride.m_Taxi) taxi = ride.m_Taxi.GetName();
  PrintFormat("[ACR] RIDE %1 (%2): %3", rider, taxi, what);
 }
}
