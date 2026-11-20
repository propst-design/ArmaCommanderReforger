// -acrLightRidesTest 1 (server, about 3 min; Everon): the AI commander buys jeeps for rides (Jacob,
// 20 Nov: "I don't see the AI buying jeeps or HMMWVs for rides at all"; it only ever bought trucks:
// ACR_AICommander.BuyLightRide). A US commander with RP whose small party found no ride
// (ACR_Rides.NoTransportFor, as a refused ride records it) buys a light car and no truck; the car
// arrives crewed and counts as a light ride; and a small party rides in it. On Everon the US 2IC
// (on by default) takes the new jeep for one of its own teams within seconds (first run, 20 Nov: the
// Weapons Team, 2 km); if nobody has by 75 s, a two-man team beside it asks for a ride 700 m.
class ACR_TestLightRides
{
 static const int CHECKS = 3;
 protected static ref ACR_AICommander s_AI;
 protected static int s_iEntry;
 protected static ACR_Squad s_Jeep, s_Team;
 protected static bool s_bDone;
 protected static string s_sSeen;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || s_bDone) return;
  if (second == 1)
  {
   s_iEntry = -1;
   foreach (ACR_FactionSettings aside : ACR_FactionSettings.s_All) aside.m_bAICommander = false;
  }
  if (second == 12) Buy(manager);
  if (second > 12 && second < 60 && !s_Jeep) FindJeep(manager);
  if (second == 60) Arrived(manager);
  if (second > 60 && second < 190 && s_Jeep) Watch(manager);
  if (second == 75 && s_Jeep && !manager.m_Rides.RideOf(s_Jeep)) OwnTeam(manager);
  if (second > 75 && s_Team && !manager.m_Rides.RideOf(s_Team) && s_sSeen.IsEmpty()) AskRide(manager);
  if (second == 190) Finish(false, "");
 }

 protected static int TrucksBought(ACR_FactionData data)
 {
  int count;
  foreach (ACR_RequisitionEntry entry : data.m_aRequisitions)
   if (entry.m_bTransport) count += entry.m_iBought;
  return count;
 }

 protected static void Buy(ACR_Manager manager)
 {
  ACR_FactionSettings us = ACR_FactionSettings.Find("US");
  ACR_FactionData data = manager.m_Bases.GetFaction("US");
  if (!us || !data || !manager.m_Rides)
  {
   ACR_TestRegress.Abort("lightrides", "no US settings, faction data or rides");
   s_bDone = true;
   return;
  }
  string offers = "";
  foreach (ACR_RequisitionEntry offer : data.m_aRequisitions)
   if (ACR_AICommander.IsLightRide(offer)) offers += string.Format("%1 (%2 RP); ", offer.m_sName, offer.m_iCost);
  PrintFormat("[ACR] LIGHTRIDES US light rides on offer: %1", offers);
  int capWas = us.m_iAIUnitCap;
  us.m_iAIUnitCap = 40; // room for anything; the squad cap isn't what this is about
  data.m_iRP = 600;
  array<int> boughtBefore = {};
  foreach (ACR_RequisitionEntry before : data.m_aRequisitions) boughtBefore.Insert(before.m_iBought);
  int trucks = TrucksBought(data);
  s_AI = new ACR_AICommander(manager, "US");
  int lightBefore = s_AI.CountLightRides(data);
  manager.m_Rides.NoTransportFor("US", 2); // a two-man team's ride was refused: no free transport
  s_AI.RequisitionForTest(0);
  us.m_iAIUnitCap = capWas;
  foreach (int i, ACR_RequisitionEntry after : data.m_aRequisitions)
   if (i < boughtBefore.Count() && after.m_iBought > boughtBefore[i] && ACR_AICommander.IsLightRide(after)) s_iEntry = i;
  string bought = "nothing";
  if (s_iEntry >= 0) bought = data.m_aRequisitions[s_iEntry].m_sName;
  ACR_TestRegress.Check("light rides: an AI commander with RP short of a ride for a small party buys a jeep, not a truck", s_iEntry >= 0 && TrucksBought(data) == trucks, string.Format("bought %1 (light rides before %2); trucks bought %3 -> %4; offers: %5", bought, lightBefore, trucks, TrucksBought(data), offers));
  if (s_iEntry < 0) Finish(false, "no jeep bought");
 }

 protected static void FindJeep(ACR_Manager manager)
 {
  if (s_iEntry < 0) return;
  ResourceName prefab = manager.m_Bases.GetFaction("US").m_aRequisitions[s_iEntry].m_sVehicle;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   if (!vehicle || ACR_Rides.FactionOf(squad) != "US" || !vehicle.GetPrefabData()) continue;
   if (vehicle.GetPrefabData().GetPrefabName() == prefab) { s_Jeep = squad; return; }
  }
 }

 protected static void Arrived(ACR_Manager manager)
 {
  string note = "never arrived";
  int light;
  if (s_Jeep)
  {
   light = s_AI.CountLightRides(manager.m_Bases.GetFaction("US"));
   string taxi = manager.m_Rides.WhyNotTaxi(s_Jeep, "US");
   if (taxi.IsEmpty()) taxi = "free";
   note = string.Format("%1, %2 passenger seats, as a taxi: %3; light rides now %4", s_Jeep.GetName(), ACR_Rides.CargoSeats(ACR_Transport.CrewedVehicle(s_Jeep)), taxi, light);
  }
  ACR_TestRegress.Check("light rides: the jeep arrives crewed and counts as a light ride", s_Jeep != null && light >= 1, note);
  if (!s_Jeep) Finish(false, "no jeep");
 }

 // Nobody took the jeep: a two-man team (the first small US team on offer) beside it.
 protected static void OwnTeam(ACR_Manager manager)
 {
  ResourceName group;
  foreach (ACR_RequisitionEntry entry : manager.m_Bases.GetFaction("US").m_aRequisitions)
   if (group.IsEmpty() && ACR_AICommander.IsSmallTeamEntry(entry) && !entry.m_sGroup.IsEmpty()) group = entry.m_sGroup;
  if (group.IsEmpty()) { Finish(false, "the jeep sat idle, and no small US team on offer to ride it"); return; }
  vector at = ACR_Transport.CrewedVehicle(s_Jeep).GetOrigin();
  manager.SpawnUnit("Jeep Riders", group, "", false, "", vector.Zero, ACR_Manager.Ground(at + Vector(20, 0, 12)));
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName().StartsWith("Jeep Riders")) s_Team = squad;
 }

 protected static void AskRide(ACR_Manager manager)
 {
  if (!s_Team)
  {
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName().StartsWith("Jeep Riders")) s_Team = squad;
   if (!s_Team) return;
  }
  if (!s_Team.GetAnchor()) return;
  vector dest = Destination(s_Team.GetAnchor().GetOrigin());
  if (dest == vector.Zero) { Finish(false, "no road destination 600-1000 m from the jeep"); return; }
  string why;
  ACR_Ride ride = manager.m_Rides.Request(s_Team, dest, 0, ACR_Squad.WP_MOVE, -1, "Moving", 0, why, true);
  string taxi = "none";
  if (ride && ride.m_Taxi) taxi = ride.m_Taxi.GetName();
  PrintFormat("[ACR] LIGHTRIDES %1 (%2 men) asks for a ride %3 m: taxi %4 (%5)", s_Team.GetName(), s_Team.CountAlive(), (int)vector.DistanceXZ(s_Team.GetAnchor().GetOrigin(), dest), taxi, why);
  if (!ride) Finish(false, "the team's ride was refused: " + why);
 }

 // Whoever rides the jeep: done once a party is aboard it as passengers.
 protected static void Watch(ACR_Manager manager)
 {
  ACR_Ride ride = manager.m_Rides.RideOf(s_Jeep);
  if (!ride || ride.m_Taxi != s_Jeep || !ride.m_Rider) return;
  ACR_Squad rider = ride.m_Rider;
  if (s_sSeen.IsEmpty())
  {
   s_sSeen = string.Format("%1 (%2 men)", rider.GetName(), rider.CountAlive());
   PrintFormat("[ACR] LIGHTRIDES %1 gets a ride in %2", s_sSeen, s_Jeep.GetName());
  }
  if (Aboard(rider, ACR_Transport.CrewedVehicle(s_Jeep)))
   Finish(true, string.Format("%1 aboard %2", s_sSeen, s_Jeep.GetName()));
 }

 protected static void Finish(bool aboard, string note)
 {
  if (s_bDone) return;
  s_bDone = true;
  if (!aboard && note.IsEmpty())
  {
   note = "nobody aboard by 190 s";
   if (!s_sSeen.IsEmpty()) note = s_sSeen + " got the ride but never sat in the jeep";
  }
  ACR_TestRegress.Check("light rides: a small party rides in the jeep the AI bought", aboard, note);
  ACR_TestRegress.Complete("lightrides", CHECKS);
 }

 // Every living man of 'squad' is in 'vehicle' (the rides' own test: ACR_Rides.Aboard).
 protected static bool Aboard(ACR_Squad squad, IEntity vehicle)
 {
  if (!vehicle) return false;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  int seated;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body)) continue;
   if (ACR_Transport.VehicleOf(body) != vehicle) return false;
   seated++;
  }
  return seated > 0;
 }

 // A road point 600-1000 m away that a car reaches without a long detour.
 protected static vector Destination(vector from)
 {
  for (int k = 0; k < 72; k++)
  {
   int slot = k % 24;
   int ring = k / 24;
   float angle = Math.PI2 * slot / 24;
   float reach = 700 + ring * 150;
   vector probe = from + Vector(Math.Cos(angle) * reach, 0, Math.Sin(angle) * reach);
   vector road;
   if (!BBB_Terrain.RoadPointNear(probe, 120, road)) continue;
   if (vector.DistanceXZ(from, road) < 600) continue;
   array<vector> legs = {};
   int nodes, millis;
   BBB_Roads.Route(from, road, legs, nodes, millis);
   if (BBB_Roads.s_bLastDetour || BBB_Roads.s_fLastLength > vector.DistanceXZ(from, road) * 1.6) continue;
   return road;
  }
  return vector.Zero;
 }
}
