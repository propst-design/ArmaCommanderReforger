// AI commander, logistics: service jobs, ammo runs, dangerous roads, evacuations, pallets and the truce.
//
// Second layer of ACR_AICommander (see ACR_AICommanderBuild.c for how the layers load).
modded class ACR_AICommander
{
 void ManageLogisticsForTest() { ManageLogistics(); }

 // Why a drive-out job should end now (JOB_ON: carry on). Pure, for srv:regress.
 static int ServiceJobEnd(bool hasTruck, float stock, float sentAt, float movedAt, float now)
 {
  if (!hasTruck) return JOB_NO_TRUCK;
  if (stock < 1) return JOB_EMPTY;
  if (now - movedAt > SERVICE_JOB_STILL_MS) return JOB_STILL;
  if (now - sentAt > SERVICE_JOB_MAX_MS) return JOB_LONG;
  return JOB_ON;
 }

 protected void EndServiceJob(ACR_Squad crew)
 {
  // Its stuck-hold retry (ACR_Squad.HoldRetryWatch) would re-drive the dead job to the old meeting
  // point minutes later, untracked and unchecked (review 0.5.13 #9).
  if (crew) crew.DropHoldRetry();
  m_ServiceJobs.Remove(crew);
  m_ServiceJobSince.Remove(crew);
  m_ServiceJobMoved.Remove(crew);
 }

 protected ACR_Squad CrewOf(IEntity vehicle)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
   if (ACR_Transport.CrewedVehicle(squad) == vehicle) return squad;
  return null;
 }

 protected int NeedsOf(ACR_Squad squad)
 {
  int need;
  // Ammo runs on (ManageAmmoRuns): the squad's own line and its dry men; else the old fixed line.
  if (ACR_AmmoRuns.On())
  {
   if (AmmoLow(squad)) need |= ACR_EService.AMMO;
  }
  else if (squad.m_iAmmoPct >= 0 && squad.m_iAmmoPct < LOW_AMMO) need |= ACR_EService.AMMO;
  if (squad.m_iAvgHpPct < LOW_HEALTH) need |= ACR_EService.MEDICAL;
  IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
  if (vehicle)
  {
   if (ACR_Logistics.Health(vehicle) * 100 < LOW_HEALTH) need |= ACR_EService.REPAIR;
   float fuel = ACR_Logistics.Fuel(vehicle);
   if (fuel >= 0 && fuel * 100 < LOW_FUEL) need |= ACR_EService.FUEL;
  }
  return need;
 }

 protected void ManageLogistics()
 {
  ACR_Logistics logistics = m_Manager.m_Logistics;
  m_iNeeds = 0;
  if (!logistics) return;
  float nowMs = GetGame().GetWorld().GetWorldTime();
  // Finished jobs: the served squad no longer needs it, or either side is gone.
  array<ACR_Squad> done = {};
  foreach (ACR_Squad jobCrew, ACR_Squad jobClient : m_ServiceJobs)
  {
   if (!jobCrew || !jobClient || !m_Manager.m_Squads.Contains(jobCrew) || !m_Manager.m_Squads.Contains(jobClient))
   {
    done.Insert(jobCrew);
    continue;
   }
   if (AmmoJob(jobCrew, jobClient)) continue; // an ammo run lets its truck go itself (TickTruckRun)
   if (NeedsOf(jobClient) == 0 || jobCrew.OrderState() == ACR_EOrderState.HOLD)
   {
    done.Insert(jobCrew);
    continue;
   }
   // ...or it isn't going to happen (#242 audit): see SERVICE_JOB_STILL_MS.
   IEntity jobTruck = ACR_Transport.CrewedVehicle(jobCrew);
   float stock = 0;
   if (jobTruck)
   {
    stock = logistics.StockOf(jobTruck);
    Physics jobPhysics = jobTruck.GetPhysics();
    if (!m_ServiceJobMoved.Contains(jobCrew) || (jobPhysics && jobPhysics.GetVelocity().Length() > ACR_Logistics.SERVICE_MAX_SPEED)) m_ServiceJobMoved.Set(jobCrew, nowMs);
   }
   float sentAt = nowMs;
   if (m_ServiceJobSince.Contains(jobCrew)) sentAt = m_ServiceJobSince.Get(jobCrew);
   else m_ServiceJobSince.Set(jobCrew, nowMs);
   float movedAt = nowMs;
   if (m_ServiceJobMoved.Contains(jobCrew)) movedAt = m_ServiceJobMoved.Get(jobCrew);
   int end = ServiceJobEnd(jobTruck != null, stock, sentAt, movedAt, nowMs);
   if (end == JOB_ON) continue;
   done.Insert(jobCrew);
   m_ServiceSnub.Set(jobClient, nowMs + SERVICE_SNUB_MS);
   string whyEnd = "its truck is gone";
   if (end == JOB_EMPTY) whyEnd = "the truck is empty";
   else if (end == JOB_STILL) whyEnd = "stopped without serving them";
   else if (end == JOB_LONG) whyEnd = "it took too long";
   Say(string.Format("[ACR] AI %1: %2 gives up resupplying %3 (%4)", m_sFaction, jobCrew.GetName(), jobClient.GetName(), whyEnd));
  }
  foreach (ACR_Squad finished : done) EndServiceJob(finished);
  array<ACR_Squad> snubGone = {};
  foreach (ACR_Squad snubbed, float snubUntil : m_ServiceSnub)
   if (!snubbed || !m_Manager.m_Squads.Contains(snubbed) || nowMs >= snubUntil) snubGone.Insert(snubbed);
  foreach (ACR_Squad unsnubbed : snubGone) m_ServiceSnub.Remove(unsnubbed);
  ManageAmmoRuns(logistics, nowMs);
  array<ACR_Squad> walkers = {};
  foreach (ACR_Squad walker, float giveUp : m_ResupplyGiveUp) walkers.Insert(walker);
  foreach (ACR_Squad walker : walkers)
  {
   if (!walker || !m_Manager.m_Squads.Contains(walker) || walker.m_iAITask != ACR_EAITask.RESUPPLY) { m_ResupplyGiveUp.Remove(walker); continue; }
   if (nowMs < m_ResupplyGiveUp.Get(walker)) continue;
   m_ResupplyGiveUp.Remove(walker);
   walker.SetAITask(ACR_EAITask.FREE);
   Say(string.Format("[ACR] AI %1: %2 never reached a service truck, back to duty", m_sFaction, walker.GetName()));
  }
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
  {
   int need = NeedsOf(squad);
   // The ammo of the squads ammo runs may take is theirs (ManageAmmoRuns); the trucks here see to the rest.
   bool ammoRun = m_AmmoRuns.Contains(squad);
   if (ACR_AmmoRuns.On() && (ammoRun || AmmoRunCandidate(squad)))
   {
    m_iNeeds |= need & ACR_EService.AMMO; // still buys a truck when the side has none
    need &= ~ACR_EService.AMMO;
   }
   // Walking back for replacements (ReinforceDepleted) is a resupply with no service need.
   if (squad.m_iAITask == ACR_EAITask.RESUPPLY && need == 0 && !m_Refitting.Contains(squad) && !ammoRun)
   {
    squad.SetAITask(ACR_EAITask.FREE);
    Say(string.Format("[ACR] AI %1: %2 resupplied", m_sFaction, squad.GetName()));
   }
   if (need == 0 || InAssault(squad) || ACR_Logistics.ServiceOf(ACR_Transport.CrewedVehicle(squad)) != 0) continue;
   m_iNeeds |= need;
   // Not a squad giving or taking a ride: it finishes the trip first (6 Nov live: UAZ 1, low on
   // fuel 1.2 km short of Soapograd, turned back for the fuel truck with its riders aboard).
   // A tank that actually runs dry ends the ride on its own (transport lost, on foot).
   if (m_Manager.m_Rides && (m_Manager.m_Rides.RideOf(squad) || m_Manager.m_Rides.Unloading(squad))) continue;
   array<int> services = {ACR_EService.AMMO, ACR_EService.REPAIR, ACR_EService.FUEL, ACR_EService.MEDICAL};
   foreach (int service : services)
   {
    if ((need & service) == 0) continue;
    array<IEntity> stations = {};
    logistics.Stations(m_sFaction, service, stations);
    IEntity station;
    float best = SERVICE_REACH;
    foreach (IEntity candidate : stations)
    {
     float d = vector.DistanceXZ(candidate.GetOrigin(), PositionOf(squad));
     if (d < best) { best = d; station = candidate; }
    }
    if (!station || best < ACR_Logistics.SERVICE_RANGE) continue; // none, or already being served
    ACR_Squad crew = CrewOf(station);
    bool holding = squad.m_iAITask == ACR_EAITask.DEFEND || squad.m_iAITask == ACR_EAITask.POSTED || !squad.m_Group.GetCurrentWaypoint();
    bool crewFree = crew && !crew.PlayerSteers() && Ours(crew) && !m_ServiceJobs.Contains(crew) && !StuckLately(crew);
    string what = ACR_Logistics.ServiceName(service);
    if (crewFree && !m_ServiceSnub.Contains(squad) && (holding || squad.IsMounted() == false && best > 800) && !DangerOnTheWay(crew, PositionOf(squad)))
    {
     // Bring the truck to them.
     crew.SetMode(ACR_EMoveMode.FAST);
     crew.RouteTo(ACR_Manager.Ground(PositionOf(squad)), ACR_Squad.WP_MOVE, -1, "Resupplying " + squad.GetName(), 0);
     m_ServiceJobs.Set(crew, squad);
     m_ServiceJobSince.Set(crew, nowMs);
     m_ServiceJobMoved.Set(crew, nowMs);
     m_ParkSentAt.Remove(crew); // back from the job, it goes straight to the park
     Say(string.Format("[ACR] AI %1: %2 drives out to %3 (%4)", m_sFaction, crew.GetName(), squad.GetName(), what));
    }
    else if (squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED)
    {
     // Go back to the truck.
     squad.SetAITask(ACR_EAITask.RESUPPLY);
     m_ResupplyGiveUp.Set(squad, nowMs + REFIT_GIVE_UP_MS);
     squad.SetMode(ACR_EMoveMode.FAST);
     Travel(squad, ACR_Manager.Ground(station.GetOrigin()), false, 0);
     Say(string.Format("[ACR] AI %1: %2 falls back to the %3 truck", m_sFaction, squad.GetName(), what));
    }
    break;
   }
  }
  // Idle service vehicles: restock if low, otherwise wait at the supply point's truck park.
  ACR_Base supplyPoint = SupplyPoint();
  vector park = TruckPark(supplyPoint);
  array<ACR_Squad> parkGone = {};
  foreach (ACR_Squad parkCrew, float parkSent : m_ParkSentAt)
   if (!parkCrew || !m_Manager.m_Squads.Contains(parkCrew)) parkGone.Insert(parkCrew);
  foreach (ACR_Squad gone : parkGone) { m_ParkSentAt.Remove(gone); m_ParkSentTo.Remove(gone); }
  foreach (ACR_Squad idleCrew : mine)
  {
   IEntity serviceVehicle = ACR_Transport.CrewedVehicle(idleCrew);
   int serviceMask = ACR_Logistics.ServiceOf(serviceVehicle) & ~ACR_EService.CONSTRUCTION;
   if (!serviceVehicle || serviceMask == 0 || idleCrew.PlayerSteers() || m_ServiceJobs.Contains(idleCrew)) continue;
   if (idleCrew.OrderState() == ACR_EOrderState.BUILD_FOB || idleCrew.m_iAITask == ACR_EAITask.BUILD_FOB) continue;
   if (IsAssistingFobUpgrade(idleCrew) || StuckLately(idleCrew)) continue;
   ACR_Base goal = supplyPoint;
   bool restock = logistics.StockPercent(serviceVehicle) < 25;
   if (restock) goal = HomeBase();
   if (!goal) continue;
   // Restocking needs the ring (ACR_Logistics.Restock); waiting only needs a spot it can drive to.
   vector spot = goal.GetOrigin();
   if (!restock && goal == supplyPoint) spot = park;
   vector at = serviceVehicle.GetOrigin();
   if (vector.DistanceXZ(at, goal.GetOrigin()) < goal.m_fRadius) continue;
   if (!restock && vector.DistanceXZ(at, spot) < PARK_ARRIVE) continue;
   if (idleCrew.OrderState() == ACR_EOrderState.MOVE && (vector.DistanceXZ(idleCrew.RouteDestination(), goal.GetOrigin()) < goal.m_fRadius + 80 || vector.DistanceXZ(idleCrew.RouteDestination(), spot) < PARK_ARRIVE + 80)) continue;
   // Sent here lately and stopped short (a spot its driver can't reach): it waits where it
   // stopped, and tries again after PARK_RETRY_MS.
   if (m_ParkSentAt.Contains(idleCrew) && nowMs - m_ParkSentAt.Get(idleCrew) < PARK_RETRY_MS && vector.DistanceXZ(m_ParkSentTo.Get(idleCrew), spot) < PARK_ARRIVE) continue;
   if (DangerOnTheWay(idleCrew, spot)) continue;
   idleCrew.RouteTo(ACR_Manager.Ground(spot), ACR_Squad.WP_MOVE, -1, "Moving", 0);
   m_ParkSentAt.Set(idleCrew, nowMs);
   m_ParkSentTo.Set(idleCrew, spot);
   Say(string.Format("[ACR] AI %1: %2 to %3 (%4)", m_sFaction, idleCrew.GetName(), goal.m_sName, ACR_Logistics.ServiceName(serviceMask)));
  }
 }

 ACR_AmmoRun AmmoRunOf(ACR_Squad squad)
 {
  ACR_AmmoRun run;
  if (squad) m_AmmoRuns.Find(squad, run);
  return run;
 }

 // The ammo run holding this truck (TickTruckRun lets it go, not ManageLogistics).
 protected bool AmmoJob(ACR_Squad crew, ACR_Squad client)
 {
  ACR_AmmoRun run = AmmoRunOf(client);
  return run && run.m_Crew == crew;
 }

 // Its line: the doctrine's, varied per squad (seeded).
 protected float AmmoLine(ACR_Squad squad)
 {
  return ACR_AmmoRuns.Line(m_Doctrine.m_fAmmoLow, string.Format("ammo %1 %2", m_sFaction, squad.GetName()));
 }

 protected bool AmmoLow(ACR_Squad squad)
 {
  return ACR_AmmoRuns.Low(squad.m_iAmmoPct, squad.m_iDryPct, AmmoLine(squad));
 }

 protected bool AmmoHolder(ACR_Squad squad)
 {
  return squad.m_iAITask == ACR_EAITask.DEFEND || squad.m_iAITask == ACR_EAITask.FORTIFY;
 }

 // Who may go on a run: free, posted, defending or holding a position, no player, not riding,
 // not in an assault, not walking back for replacements (that refill restocks it too).
 protected bool AmmoRunCandidate(ACR_Squad squad)
 {
  if (!squad || squad.PlayerSteers() || squad.m_bMortar || squad.Aircraft() || !Ours(squad)) return false;
  if (squad == m_FobBuilder || squad == m_FobEscort || OnRequest(squad) || InAssault(squad) || m_Refitting.Contains(squad)) return false;
  IEntity crewed = ACR_Transport.CrewedVehicle(squad);
  if (crewed && ACR_Logistics.ServiceOf(crewed) != 0) return false;
  if (m_Manager.m_Rides && (m_Manager.m_Rides.RideOf(squad) || m_Manager.m_Rides.Unloading(squad))) return false;
  int task = squad.m_iAITask;
  return task == ACR_EAITask.FREE || task == ACR_EAITask.POSTED || task == ACR_EAITask.DEFEND || task == ACR_EAITask.FORTIFY;
 }

 // A truck crew free to drive out.
 protected bool AmmoCrewFree(ACR_Squad crew)
 {
  if (!crew || crew.PlayerSteers() || !Ours(crew) || m_ServiceJobs.Contains(crew) || StuckLately(crew)) return false;
  if (crew.OrderState() == ACR_EOrderState.BUILD_FOB || crew.m_iAITask == ACR_EAITask.BUILD_FOB) return false;
  return !IsAssistingFobUpgrade(crew);
 }

 // Why a truck can't be sent to 'meet' for this squad ("" = it can).
 protected string TruckUnsafe(ACR_Squad squad, vector meet)
 {
  if (squad.KnownEnemies(AMMO_CONTACT_M) > 0) return "in contact";
  array<vector> seen = {};
  SeenEnemiesNear(meet, AMMO_SEEN_M, seen);
  if (!seen.IsEmpty()) return "enemies seen near";
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != m_sFaction) continue;
   if (vector.DistanceXZ(meet, base.GetOrigin()) > base.m_fRadius + 150) continue;
   if (base.m_bContested || ThreatTo(base) > 0) return base.m_sName + " under attack";
  }
  ACR_Base enemy = NearestEnemyBase(meet);
  if (enemy && vector.DistanceXZ(meet, enemy.GetOrigin()) < AMMO_FRONT_M) return "at the door of " + enemy.m_sName;
  return "";
 }

 protected float FrontRiskAt(vector point)
 {
  ACR_Base enemy = NearestEnemyBase(point);
  if (!enemy) return 0;
  return ACR_AmmoRuns.FrontRisk(vector.DistanceXZ(point, enemy.GetOrigin()));
 }

 protected bool AmmoSkips(ACR_Squad squad, ACR_Base base, float now)
 {
  map<ACR_Base, float> skips;
  float at;
  if (!base || !m_AmmoSkips.Find(squad, skips) || !skips.Find(base, at)) return false;
  return now - at < AMMO_SKIP_MS;
 }

 protected void SkipAmmoBase(ACR_Squad squad, ACR_Base base, float now)
 {
  if (!squad || !base) return;
  map<ACR_Base, float> skips;
  if (!m_AmmoSkips.Find(squad, skips))
  {
   skips = new map<ACR_Base, float>();
   m_AmmoSkips.Insert(squad, skips);
  }
  // Long-past skips go (and bases that no longer exist), so the map stays small.
  array<ACR_Base> old = {};
  foreach (ACR_Base was, float at : skips)
   if (!was || now - at >= AMMO_SKIP_MS) old.Insert(was);
  foreach (ACR_Base gone : old) skips.Remove(gone);
  skips.Set(base, now);
 }

 protected void ManageAmmoRuns(ACR_Logistics logistics, float now)
 {
  if (!ACR_AmmoRuns.On())
  {
   if (!m_AmmoRuns.IsEmpty()) EndAllAmmoRuns("ammo runs turned off");
   return;
  }
  PruneAmmo();
  array<ACR_Squad> running = {};
  foreach (ACR_Squad runner, ACR_AmmoRun r : m_AmmoRuns) running.Insert(runner);
  foreach (ACR_Squad squad : running) TickAmmoRun(squad, logistics, now);
  // New runs, the lowest first.
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  int combat;
  foreach (ACR_Squad member : mine)
   if (member.IsCombatUnit()) combat++;
  int cap = ACR_Variety.Share(combat, ACR_AmmoRuns.RUN_SHARE, "ammo runs " + m_sFaction);
  if (cap < 1) cap = 1;
  array<ACR_Squad> low = {};
  foreach (ACR_Squad candidate : mine)
  {
   if (m_AmmoRuns.Contains(candidate) || !AmmoRunCandidate(candidate) || !AmmoLow(candidate)) continue;
   float lookAt;
   if (m_AmmoLookAt.Find(candidate, lookAt) && now < lookAt) continue;
   float toppedAt;
   int toppedPct;
   if (m_AmmoToppedAt.Find(candidate, toppedAt) && m_AmmoToppedPct.Find(candidate, toppedPct) && ACR_AmmoRuns.ToppedHolds(now, toppedAt, toppedPct, candidate.m_iAmmoPct)) continue;
   low.Insert(candidate);
  }
  for (int i = 1; i < low.Count(); i++)
  {
   ACR_Squad current = low[i];
   int j = i - 1;
   while (j >= 0)
   {
    if (low[j].m_iAmmoPct <= current.m_iAmmoPct) break;
    low[j + 1] = low[j];
    j--;
   }
   low[j + 1] = current;
  }
  foreach (ACR_Squad squad : low)
  {
   bool dry = squad.m_iAmmoPct < OUT_OF_AMMO;
   if (!dry && m_AmmoRuns.Count() >= cap) continue;
   StartAmmoRun(squad, logistics, now);
  }
 }

 // The choice for one squad low on ammo: the truck, a base, or carrying on.
 protected void StartAmmoRun(ACR_Squad squad, ACR_Logistics logistics, float now)
 {
  vector at = PositionOf(squad);
  bool holder = AmmoHolder(squad);
  bool dry = squad.m_iAmmoPct < OUT_OF_AMMO;
  float line = AmmoLine(squad);
  float shortfall = ACR_AmmoRuns.Shortfall(squad.m_iAmmoPct, squad.m_iDryPct, line);
  int alive = squad.CountAlive();
  if (alive < 1) alive = 1;
  // The truck: the cheapest free ammo or arsenal truck within SERVICE_REACH of a meeting point on a road near it.
  vector meet = at;
  vector road;
  if (BBB_Terrain.RoadPointNear(at, AMMO_MEET_ROAD_M, road)) meet = road;
  string danger = TruckUnsafe(squad, meet);
  float noTruckUntil;
  if (danger.IsEmpty() && m_AmmoNoTruckUntil.Find(squad, noTruckUntil) && now < noTruckUntil) danger = "its last truck failed it";
  ACR_Squad crew;
  IEntity truck;
  float truckCost = -1;
  if (danger.IsEmpty())
  {
   array<IEntity> stations = {};
   logistics.Stations(m_sFaction, ACR_EService.AMMO, stations);
   foreach (IEntity station : stations)
   {
    if (logistics.IsPoint(station)) continue;
    ACR_Squad stationCrew = CrewOf(station);
    if (!AmmoCrewFree(stationCrew)) continue;
    float drive = vector.DistanceXZ(station.GetOrigin(), meet);
    if (drive > SERVICE_REACH || DangerOnTheWay(stationCrew, meet)) continue;
    float cost = ACR_AmmoRuns.TruckCost(drive, vector.DistanceXZ(at, meet), FrontRiskAt(meet), logistics.StockOf(station) / alive);
    if (truckCost < 0 || cost < truckCost) { truckCost = cost; crew = stationCrew; truck = station; }
   }
  }
  // The bases. A defender stays with its defence unless it is shot dry (the truck may still come).
  array<ACR_Base> bases = {};
  array<float> baseCosts = {};
  if (squad.m_iAITask != ACR_EAITask.DEFEND || dry)
  {
   array<ResourceName> missing = {};
   squad.GetMissing(missing);
   bool wounded = squad.m_iAvgHpPct < LOW_HEALTH;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (!RearmBase(base) || AmmoSkips(squad, base, now)) continue;
    float dist = vector.DistanceXZ(at, base.GetOrigin());
    if (dist > AMMO_BASE_REACH) continue;
    float front = FrontRiskAt(base.GetOrigin());
    if (DefenceThreat(base) > 0) front = 1; // an attack on it is forecast
    bool ride = m_Manager.m_Rides && dist > m_Doctrine.m_fRideDistance;
    bases.Insert(base);
    baseCosts.Insert(ACR_AmmoRuns.BaseCost(Math.Max(0, dist - base.m_fRadius), ride, holder, missing.Count(), wounded, front));
   }
  }
  // Option refs: -2 carry on, -1 the truck, 0.. a base.
  array<float> costs = {};
  array<int> refs = {};
  costs.Insert(ACR_AmmoRuns.WaitCost(shortfall));
  refs.Insert(-2);
  if (crew)
  {
   costs.Insert(truckCost);
   refs.Insert(-1);
  }
  foreach (int b, float baseCost : baseCosts)
  {
   costs.Insert(baseCost);
   refs.Insert(b);
  }
  int minute = (int)(now / 60000);
  int pick = ACR_AmmoRuns.Pick(costs, string.Format("ammo run %1 %2 %3", m_sFaction, squad.GetName(), minute));
  string noTruck = "";
  if (!danger.IsEmpty()) noTruck = " (no truck: " + danger + ")";
  else if (!crew) noTruck = " (no truck free)";
  if (pick < 0 || refs[pick] == -2)
  {
   m_AmmoLookAt.Set(squad, now + AMMO_LOOK_MS);
   Say(string.Format("[ACR] AI %1: %2 low on ammo (%3 pct, line %4) carries on for now%5", m_sFaction, squad.GetName(), squad.m_iAmmoPct, (int)line, noTruck));
   return;
  }
  ACR_AmmoRun run = new ACR_AmmoRun();
  run.m_fStartedAt = now;
  run.m_iFromTask = squad.m_iAITask;
  run.m_iStartPct = squad.m_iAmmoPct;
  if (holder) NoteLeftPost(squad);
  squad.SetAITask(ACR_EAITask.RESUPPLY);
  squad.SetMode(ACR_EMoveMode.FAST);
  m_AmmoLookAt.Remove(squad);
  if (refs[pick] == -1)
  {
   run.m_Crew = crew;
   run.m_vMeet = meet;
   run.m_fGiveUpAt = now + AMMO_TRUCK_MS;
   m_AmmoRuns.Set(squad, run);
   m_ServiceJobs.Set(crew, squad);
   crew.SetMode(ACR_EMoveMode.FAST);
   crew.RouteTo(ACR_Manager.Ground(meet), ACR_Squad.WP_MOVE, -1, "Resupplying " + squad.GetName(), 0);
   if (vector.DistanceXZ(at, meet) > 15) squad.RouteTo(ACR_Manager.Ground(meet), ACR_Squad.WP_MOVE, -1, "Meeting the ammo truck", 0);
   else squad.Halt("Waiting for the ammo truck");
   Say(string.Format("[ACR] AI %1: %2 low on ammo (%3 pct, line %4) calls the ammo truck: %5 meets it at %6, %7 m out", m_sFaction, squad.GetName(), squad.m_iAmmoPct, (int)line, crew.GetName(), ACR_Manager.Grid(meet), (int)vector.DistanceXZ(truck.GetOrigin(), meet)));
   return;
  }
  ACR_Base target = bases[refs[pick]];
  run.m_Base = target;
  run.m_vMeet = target.GetOrigin();
  run.m_fGiveUpAt = now + REFIT_GIVE_UP_MS;
  m_AmmoRuns.Set(squad, run);
  float away = vector.DistanceXZ(at, target.GetOrigin());
  Say(string.Format("[ACR] AI %1: %2 low on ammo (%3 pct, line %4) goes back to %5 to rearm, %6 m%7", m_sFaction, squad.GetName(), squad.m_iAmmoPct, (int)line, target.m_sName, (int)away, noTruck));
  if (away > target.m_fRadius) Travel(squad, ACR_Manager.Ground(target.GetOrigin()), false, 0);
  else TickBaseRun(squad, run, logistics, now);
 }

 protected void TickAmmoRun(ACR_Squad squad, ACR_Logistics logistics, float now)
 {
  ACR_AmmoRun run = AmmoRunOf(squad);
  if (!run) return;
  if (!m_Manager.m_Squads.Contains(squad) || squad.CountAlive() <= 0 || squad.PlayerSteers() || !Ours(squad))
  {
   EndAmmoRun(squad, "", false, now);
   return;
  }
  // Taken for something else meanwhile (a defence, a player's order): the run is over.
  if (squad.m_iAITask != ACR_EAITask.RESUPPLY)
  {
   EndAmmoRun(squad, "taken for another job", false, now);
   return;
  }
  if (ACR_AmmoRuns.Done(squad.m_iAmmoPct, squad.m_iDryPct))
  {
   EndAmmoRun(squad, string.Format("rearmed (%1 -> %2 pct)", run.m_iStartPct, squad.m_iAmmoPct), false, now);
   return;
  }
  if (run.m_Crew) TickTruckRun(squad, run, logistics, now);
  else TickBaseRun(squad, run, logistics, now);
 }

 protected void TickTruckRun(ACR_Squad squad, ACR_AmmoRun run, ACR_Logistics logistics, float now)
 {
  ACR_Squad crew = run.m_Crew;
  IEntity truck;
  if (crew) truck = ACR_Transport.CrewedVehicle(crew);
  ACR_Squad serving;
  bool booked = crew && m_ServiceJobs.Find(crew, serving) && serving == squad;
  string lost = "";
  if (!crew || !truck || !m_Manager.m_Squads.Contains(crew) || ACR_Logistics.Health(truck) <= 0) lost = "the truck was lost";
  else if (!booked || crew.PlayerSteers()) lost = "the truck was taken off the job";
  else if (StuckLately(crew) || crew.GaveUpUnreachable()) lost = "the truck can't get there";
  else if (run.m_fArrivedAt <= 0 && now >= run.m_fGiveUpAt) lost = "the truck never came";
  else if (logistics.StockOf(truck) < 1) lost = "the truck ran dry";
  else lost = TruckUnsafe(squad, run.m_vMeet);
  if (!lost.IsEmpty())
  {
   // The truck goes back to the supply point (ManageLogistics); the squad looks again at once,
   // trucks left out a while: a base, or carrying on.
   ReleaseAmmoTruck(crew, squad);
   m_AmmoRuns.Remove(squad);
   m_AmmoNoTruckUntil.Set(squad, now + AMMO_LOOK_MS);
   squad.SetAITask(ACR_EAITask.FREE);
   Say(string.Format("[ACR] AI %1: %2's ammo truck called off (%3)", m_sFaction, squad.GetName(), lost));
   if (AmmoRunCandidate(squad) && AmmoLow(squad)) StartAmmoRun(squad, logistics, now);
   return;
  }
  Physics physics = truck.GetPhysics();
  bool stopped = !physics || physics.GetVelocity().Length() <= ACR_Logistics.SERVICE_MAX_SPEED;
  if (run.m_fArrivedAt <= 0)
  {
   if (vector.DistanceXZ(truck.GetOrigin(), run.m_vMeet) > AMMO_MEET_M || !stopped) return;
   run.m_fArrivedAt = now;
  }
  // There: the men gather at the truck, and it hands out to all of them, not only at its tailgate.
  if (!run.m_bGathered)
  {
   run.m_bGathered = true;
   if (vector.DistanceXZ(PositionOf(squad), truck.GetOrigin()) > 12) squad.RouteTo(ACR_Manager.Ground(truck.GetOrigin()), ACR_Squad.WP_MOVE, -1, "Rearming at the ammo truck", 0);
  }
  logistics.ServeSquadAt(truck, squad, AMMO_SERVE_M);
  if (now - run.m_fArrivedAt < AMMO_SERVE_MS) return;
  EndAmmoRun(squad, string.Format("rearmed at the ammo truck as far as it could (%1 -> %2 pct)", run.m_iStartPct, squad.m_iAmmoPct), squad.m_iAmmoPct < ACR_AmmoRuns.DONE_PCT, now);
 }

 protected void TickBaseRun(ACR_Squad squad, ACR_AmmoRun run, ACR_Logistics logistics, float now)
 {
  ACR_Base base = run.m_Base;
  bool inside = base && vector.DistanceXZ(PositionOf(squad), base.GetOrigin()) <= base.m_fRadius;
  string lost = "";
  if (!RearmBase(base)) lost = "it can't restock it now";
  else if (!inside && (now >= run.m_fGiveUpAt || squad.GaveUpUnreachable())) lost = "it can't get there";
  if (!lost.IsEmpty())
  {
   SkipAmmoBase(squad, base, now);
   m_AmmoRuns.Remove(squad);
   squad.SetAITask(ACR_EAITask.FREE);
   string name = "?";
   if (base) name = base.m_sName;
   Say(string.Format("[ACR] AI %1: %2's walk back to %3 for ammo called off (%4)", m_sFaction, squad.GetName(), name, lost));
   if (AmmoRunCandidate(squad) && AmmoLow(squad)) StartAmmoRun(squad, logistics, now);
   return;
  }
  if (!inside) return; // on its way (Travel: on foot, or a ride)
  if (run.m_fArrivedAt <= 0) run.m_fArrivedAt = now;
  if (run.m_fServedAt > 0)
  {
   if (now - run.m_fServedAt < AMMO_SETTLE_MS) return;
   EndAmmoRun(squad, string.Format("restocked at %1 as far as it could (%2 -> %3 pct)", base.m_sName, run.m_iStartPct, squad.m_iAmmoPct), squad.m_iAmmoPct < ACR_AmmoRuns.DONE_PCT, now);
   return;
  }
  // No restock with enemies near (as the commander's Reinforce, Jacob 4 Oct): it waits, then fights.
  if (m_Manager.m_Requisition.InContact(squad) != "")
  {
   if (run.m_fContactSince <= 0) run.m_fContactSince = now;
   if (now - run.m_fContactSince < AMMO_CONTACT_MS) return;
   m_AmmoLookAt.Set(squad, now + AMMO_LOOK_MS);
   EndAmmoRun(squad, string.Format("enemies at %1, it fights on", base.m_sName), false, now);
   return;
  }
  run.m_fContactSince = 0;
  // The stragglers come in first (only the men inside the circle are restocked).
  if (MenInside(squad, base) < squad.CountAlive() && now - run.m_fArrivedAt < AMMO_GATHER_MS) return;
  int served = m_Manager.m_Requisition.RestockAmmo(squad, base);
  int vehicles = ServiceAmmoVehicles(squad, base);
  run.m_fServedAt = now;
  Say(string.Format("[ACR] AI %1: %2 restocked at %3: %4 men, %5 vehicles", m_sFaction, squad.GetName(), base.m_sName, served, vehicles));
 }

 protected int MenInside(ACR_Squad squad, ACR_Base base)
 {
  int inside;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   if (soldier && ACR_Respawn.IsAlive(soldier) && vector.DistanceXZ(soldier.GetOrigin(), base.GetOrigin()) <= base.m_fRadius) inside++;
  }
  return inside;
 }

 // Its vehicles inside the base: rearmed and repaired at the Reinforce price (ACR_Requisition.ServiceCost).
 protected int ServiceAmmoVehicles(ACR_Squad squad, ACR_Base base)
 {
  if (!ACR_LogisticsRules.CanDeploy(base)) return 0;
  array<IEntity> vehicles = {};
  m_Manager.m_Requisition.SquadVehiclesIn(squad, base, vehicles);
  if (vehicles.IsEmpty()) return 0;
  int cost;
  foreach (IEntity vehicle : vehicles) cost += ACR_Requisition.ServiceCost(vehicle);
  if (cost > 0)
  {
   int before = m_Manager.m_Bases.GetRP(m_sFaction);
   if (Purse() - BuildKeep() < cost || !m_Manager.m_Bases.Spend(m_sFaction, cost)) return 0;
   m_Manager.LogSpend(null, m_sFaction, before, "rearmed and repaired " + squad.GetName());
  }
  return m_Manager.m_Requisition.ServiceVehicles(vehicles);
 }

 protected void ReleaseAmmoTruck(ACR_Squad crew, ACR_Squad client)
 {
  ACR_Squad serving;
  if (crew && m_ServiceJobs.Find(crew, serving) && serving == client) EndServiceJob(crew);
 }

 // topped: the refill couldn't bring it back to DONE_PCT; no new run for a while (ToppedHolds).
 protected void EndAmmoRun(ACR_Squad squad, string why, bool topped, float now)
 {
  ACR_AmmoRun run = AmmoRunOf(squad);
  m_AmmoRuns.Remove(squad);
  if (run && run.m_Crew) ReleaseAmmoTruck(run.m_Crew, squad);
  if (!squad) return;
  if (topped)
  {
   m_AmmoToppedAt.Set(squad, now);
   m_AmmoToppedPct.Set(squad, squad.m_iAmmoPct);
  }
  if (squad.m_iAITask == ACR_EAITask.RESUPPLY && !m_Refitting.Contains(squad)) squad.SetAITask(ACR_EAITask.FREE);
  if (!why.IsEmpty()) Say(string.Format("[ACR] AI %1: %2 %3", m_sFaction, squad.GetName(), why));
 }

 protected void EndAllAmmoRuns(string why)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_Squad> running = {};
  foreach (ACR_Squad runner, ACR_AmmoRun r : m_AmmoRuns) running.Insert(runner);
  foreach (ACR_Squad squad : running) EndAmmoRun(squad, why, false, now);
  m_AmmoRuns.Clear();
 }

 protected void ForgetAmmo(ACR_Squad squad)
 {
  ACR_AmmoRun run = AmmoRunOf(squad);
  if (run && run.m_Crew) ReleaseAmmoTruck(run.m_Crew, squad);
  m_AmmoRuns.Remove(squad);
  m_AmmoLookAt.Remove(squad);
  m_AmmoNoTruckUntil.Remove(squad);
  m_AmmoToppedAt.Remove(squad);
  m_AmmoToppedPct.Remove(squad);
  m_AmmoSkips.Remove(squad);
 }

 // Squads gone from the match (wiped, deleted) leave nothing behind in the ammo maps (review 0.5.13).
 protected void PruneAmmo()
 {
  array<ACR_Squad> gone = {};
  foreach (ACR_Squad a, ACR_AmmoRun r : m_AmmoRuns) if (!a || !m_Manager.m_Squads.Contains(a)) gone.Insert(a);
  foreach (ACR_Squad b, float t1 : m_AmmoLookAt) if (!b || !m_Manager.m_Squads.Contains(b)) gone.Insert(b);
  foreach (ACR_Squad c, float t2 : m_AmmoNoTruckUntil) if (!c || !m_Manager.m_Squads.Contains(c)) gone.Insert(c);
  foreach (ACR_Squad d, float t3 : m_AmmoToppedAt) if (!d || !m_Manager.m_Squads.Contains(d)) gone.Insert(d);
  foreach (ACR_Squad e, int p : m_AmmoToppedPct) if (!e || !m_Manager.m_Squads.Contains(e)) gone.Insert(e);
  foreach (ACR_Squad f, map<ACR_Base, float> s : m_AmmoSkips) if (!f || !m_Manager.m_Squads.Contains(f)) gone.Insert(f);
  foreach (ACR_Squad squad : gone) ForgetAmmo(squad);
 }

 // Tests.
 float AmmoLineForTest(ACR_Squad squad) { return AmmoLine(squad); }

 string TruckUnsafeForTest(ACR_Squad squad, vector meet) { return TruckUnsafe(squad, meet); }

 void AttackPoolForTest(array<ACR_Squad> result) { MySquads(result, true); DropAmmoShort(result); }

 bool AmmoShortForAttackForTest(ACR_Squad squad) { return AmmoShortForAttack(squad); }

 void ForgetAmmoForTest(ACR_Squad squad)
 {
  ForgetAmmo(squad);
  if (squad && squad.m_iAITask == ACR_EAITask.RESUPPLY) squad.SetAITask(ACR_EAITask.FREE);
 }

 protected vector TruckPark(ACR_Base base)
 {
  if (!base) return vector.Zero;
  if (base == m_ParkBase) return m_vPark;
  m_ParkBase = base;
  vector centre = base.GetOrigin();
  vector away = vector.Zero;
  float nearest = float.MAX;
  foreach (ACR_Base other : ACR_Base.s_All)
  {
   if (other.m_sOwner == m_sFaction || !Visible(other)) continue;
   float d = vector.DistanceXZ(centre, other.GetOrigin());
   if (d >= nearest || d < 1) continue;
   nearest = d;
   away = Vector(centre[0] - other.GetOrigin()[0], 0, centre[2] - other.GetOrigin()[2]).Normalized();
  }
  if (!ACR_Logistics.ParkSpot(centre, base.m_fRadius, away, m_vPark)) m_vPark = centre;
  Say(string.Format("[ACR] AI %1: service trucks wait at %2, %3 m from its centre", m_sFaction, base.m_sName, Math.Round(vector.DistanceXZ(m_vPark, centre))));
  return m_vPark;
 }

 protected bool DangerOnTheWay(ACR_Squad crew, vector destination)
 {
  ACR_DangerRoad road = ACR_Insurgency.OnTheWay(m_sFaction, PositionOf(crew), destination);
  if (!road) return false;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_fDangerSaidAt > 60000)
  {
   m_fDangerSaidAt = now;
   Say(string.Format("[ACR] AI %1: %2 waits, %3 on the way at %4", m_sFaction, crew.GetName(), ACR_Insurgency.Label(road), ACR_Manager.Grid(road.m_vPos)));
  }
  return true;
 }

 protected void ManageDangerRoads()
 {
  if (ACR_Insurgency.s_aRoads.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_DangerRoad> cleared = {};
  // One zombie area cleared at a time: the dead mustn't pull the army off the war.
  bool zombieSweep;
  foreach (ACR_DangerRoad sweeping : ACR_Insurgency.s_aRoads) if (sweeping.m_bZombies && sweeping.m_sSide == m_sFaction && sweeping.m_Sweeper) zombieSweep = true;
  // Hideouts holding our men first (insurgency part 4): the free squads go there before roads.
  array<ACR_DangerRoad> order = {};
  foreach (ACR_DangerRoad rescue : ACR_Insurgency.s_aRoads) if (rescue.m_Hideout && ACR_FIAPrisoners.HeldAt(rescue.m_Hideout, m_sFaction) > 0) order.Insert(rescue);
  foreach (ACR_DangerRoad other : ACR_Insurgency.s_aRoads) if (!order.Contains(other)) order.Insert(other);
  foreach (ACR_DangerRoad road : order)
  {
   if (road.m_sSide != m_sFaction) continue;
   int held;
   if (road.m_Hideout) held = ACR_FIAPrisoners.HeldAt(road.m_Hideout, m_sFaction);
   ACR_Squad sweeper = road.m_Sweeper;
   if (sweeper && (!m_Manager.m_Squads.Contains(sweeper) || sweeper.CountAlive() <= 0 || sweeper.PlayerSteers() || sweeper.m_iAITask != ACR_EAITask.SWEEP || sweeper.GaveUpUnreachable() || now - road.m_fSweepSince > SWEEP_MAX_MS))
   {
    if (sweeper.m_iAITask == ACR_EAITask.SWEEP) sweeper.SetAITask(ACR_EAITask.FREE);
    road.m_Sweeper = null;
    road.m_fQuietSince = 0;
    sweeper = null;
   }
   if (!sweeper)
   {
    int most = 2;
    if (road.m_Hideout) most = 3 + Math.Min(held, RESCUE_SWEEPS);
    if (road.m_iSweeps >= most) continue;
    // Nobody goes into a zombie town at night: the area stays marked (supply waits) until day.
    if (road.m_bZombies && (ACR_Outbreak.Night() || zombieSweep)) continue;
    float reach = SWEEP_REACH;
    if (held > 0) reach = SWEEP_REACH * RESCUE_REACH;
    sweeper = FindSweeper(road.m_vPos, reach);
    if (!sweeper) continue;
    if (road.m_bZombies) zombieSweep = true;
    road.m_Sweeper = sweeper;
    road.m_fSweepSince = now;
    road.m_iSweeps++;
    sweeper.SetAITask(ACR_EAITask.SWEEP);
    sweeper.SetMode(ACR_EMoveMode.COMBAT);
    Travel(sweeper, road.m_vPos, false, 0);
    string what = "the road at %1 (FIA ambush)";
    if (road.m_Hideout) what = "the FIA hideout at %1";
    if (held > 0) what = "the FIA hideout at %1 (" + held.ToString() + " of our men held there)";
    if (road.m_bZombies) what = "the zombie area at %1";
    Say(string.Format("[ACR] AI %1: %2 to clear %3", m_sFaction, sweeper.GetName(), string.Format(what, ACR_Manager.Grid(road.m_vPos))));
    continue;
   }
   if (vector.DistanceXZ(PositionOf(sweeper), road.m_vPos) > 80 || sweeper.KnownEnemies(300) > 0)
   {
    road.m_fQuietSince = 0;
    continue;
   }
   if (road.m_fQuietSince <= 0) road.m_fQuietSince = now;
   if (now - road.m_fQuietSince < ACR_Insurgency.CLEAR_QUIET_MS) continue;
   // A hideout is only clear once it is destroyed: a quiet sweeper there is freed, and another
   // may be sent.
   if (!road.m_Hideout)
   {
    cleared.Insert(road);
    continue;
   }
   sweeper.SetAITask(ACR_EAITask.FREE);
   road.m_Sweeper = null;
   road.m_fQuietSince = 0;
  }
  foreach (ACR_DangerRoad done : cleared) ACR_Insurgency.Clear(done, done.m_Sweeper.GetName() + " found nobody there");
 }

 // Who goes for a pallet at 'at' (see the note above ManagePallets), or null.
 protected ACR_Squad PalletRunner(vector at)
 {
  ACR_Squad runner = FindSweeper(at);
  if (runner) return runner;
  float best = PALLET_DIVERT;
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (!attack.m_bGrab || !attack.m_Target) continue;
   foreach (ACR_Squad grabber : attack.m_Squads)
   {
    if (!grabber || !Ours(grabber) || grabber.PlayerSteers() || !grabber.GetAnchor() || ACR_AirLift.Busy(grabber)) continue;
    if (grabber.CountAlive() < 2 || IsSupportTeam(grabber) || !grabber.IsCombatUnit() || grabber == m_ReserveSquad) continue;
    if (grabber.m_iAITask != ACR_EAITask.ATTACK || grabber.KnownEnemies(300) > 0) continue;
    vector from = PositionOf(grabber);
    if (vector.DistanceXZ(from, attack.m_Target.GetOrigin()) < PALLET_GRAB_NEAR) continue;
    float d = vector.DistanceXZ(from, at);
    if (d >= best) continue;
    best = d;
    runner = grabber;
   }
  }
  if (runner) return runner;
  if (ACR_Containment.ZombiesNear(at, PALLET_TRUCK_ZOMBIES)) return null;
  best = EVAC_REACH;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (m_bHandedOnly && !squad.m_bAICommanded) continue;
   if (!m_Manager.m_Rides.IsFreeTransport(squad, m_sFaction) || StuckLately(squad)) continue;
   if (squad.m_iAITask != ACR_EAITask.FREE && squad.m_iAITask != ACR_EAITask.POSTED) continue;
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   if (!vehicle) continue;
   float d = vector.DistanceXZ(vehicle.GetOrigin(), at);
   if (d >= best) continue;
   best = d;
   runner = squad;
  }
  return runner;
 }

 protected ACR_Squad FindSweeper(vector at, float reach = -1)
 {
  if (reach < 0) reach = SWEEP_REACH;
  array<ACR_Squad> free = {};
  MySquads(free, true);
  SortByDistance(free, at);
  foreach (ACR_Squad squad : free)
  {
   if (squad.CountAlive() < 2 || IsSupportTeam(squad) || !squad.IsCombatUnit() || squad == m_ReserveSquad) continue;
   if (vector.DistanceXZ(PositionOf(squad), at) > reach) return null;
   return squad;
  }
  return null;
 }

 void ManageDangerRoadsForTest() { ManageDangerRoads(); }

 protected void ManageEvacs()
 {
  if (!ACR_Containment.On()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  bool ours = !m_bTwoICAll || ACR_Evac.TwoICExtracts(m_sFaction);
  if (m_EvacTaxi)
  {
   IEntity vehicle = ACR_Transport.CrewedVehicle(m_EvacTaxi);
   bool going = ours && vehicle && m_Manager.m_Squads.Contains(m_EvacTaxi) && m_EvacTaxi.CountAlive() > 0 && !m_EvacTaxi.PlayerSteers() && m_EvacTaxi.m_iAITask == ACR_EAITask.RESCUE && now - m_fEvacSince <= EVAC_MAX_MS;
   string done = "";
   if (!going) done = "called off";
   else done = EvacStep(vehicle, now);
   if (done.IsEmpty()) return;
   Say(string.Format("[ACR] AI %1: evac by %2 ends (%3)", m_sFaction, m_EvacTaxi.GetName(), done));
   if (m_EvacTaxi.m_iAITask == ACR_EAITask.RESCUE) m_EvacTaxi.SetAITask(ACR_EAITask.FREE);
   m_EvacTaxi = null;
   m_EvacPoint = null;
  }
  if (!ours || !m_Manager.m_Rides) return;
  if (ResumeEvac(now)) return;
  // An XP of ours with people waiting, else the town with the most people we can reach.
  ACR_EvacPoint pick;
  vector goal;
  ACR_Squad taxi;
  for (int x = 0; x < ACR_Evac.Count(); x++)
  {
   ACR_EvacPoint open = ACR_Evac.At(x);
   if (open.m_sSide != m_sFaction || ACR_Evac.OnFoot(open) == 0) continue;
   ACR_Squad near = EvacTaxi(open.m_vPos);
   if (!near) continue;
   pick = open;
   taxi = near;
   goal = open.m_vPos;
   break;
  }
  int most = 0;
  int town = -1;
  if (!pick && ACR_Evac.CountFor(m_sFaction) < ACR_Evac.MOST_PER_SIDE)
  {
   for (int i = 0; i < ACR_Containment.SiteCount(); i++)
   {
    ACR_SurvivorSite site = ACR_Containment.SiteAt(i);
    if (site.m_iCount <= most) continue;
    ACR_Squad candidate = EvacTaxi(site.m_vPos);
    if (!candidate) continue;
    most = site.m_iCount;
    town = i;
    taxi = candidate;
    goal = ACR_Evac.OutsideBases(site.m_vPos);
   }
  }
  if (!pick && town < 0) return;
  if (!pick)
  {
   string why = ACR_Evac.Place(m_sFaction, goal, pick);
   if (!why.IsEmpty() || !pick)
   {
    Say(string.Format("[ACR] AI %1: no evac at %2: %3", m_sFaction, ACR_Manager.Grid(goal), why));
    return;
   }
  }
  m_EvacPoint = pick;
  m_EvacTaxi = taxi;
  m_iEvacPhase = 0;
  m_fEvacSince = now;
  m_fEvacPhaseAt = now;
  taxi.SetAITask(ACR_EAITask.RESCUE);
  taxi.SetMode(ACR_EMoveMode.FAST);
  Travel(taxi, pick.m_vPos, false, 0);
  Say(string.Format("[ACR] AI %1: %2 drives to the evac point for %3 (%4)", m_sFaction, taxi.GetName(), pick.m_sTown, ACR_Manager.Grid(pick.m_vPos)));
 }

 // Civilians still aboard one of our free trucks after its evac ended (the base out of reach, the
 // time up, called off): their XP has nobody on foot, so ManageEvacs never picked it again and they
 // rode wherever the truck went next, the XP holding one of the side's slots. Drive them to a base.
 // A truck that just gave up (StuckLately) is left alone for a while, as service jobs are.
 protected bool ResumeEvac(float now)
 {
  for (int x = 0; x < ACR_Evac.Count(); x++)
  {
   ACR_EvacPoint open = ACR_Evac.At(x);
   if (open.m_sSide != m_sFaction || ACR_Evac.OnFoot(open) > 0 || ACR_Evac.Aboard(open) == 0) continue;
   foreach (ACR_Squad squad : m_Manager.m_Squads)
   {
    if (m_bHandedOnly && !squad.m_bAICommanded) continue;
    if (!m_Manager.m_Rides.IsFreeTransport(squad, m_sFaction) || StuckLately(squad)) continue;
    IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
    if (!vehicle || ACR_Evac.Aboard(open, vehicle) == 0) continue;
    m_EvacPoint = open; // EvacBase skips a base beside this XP
    ACR_Base base = EvacBase(vehicle.GetOrigin());
    if (!base)
    {
     m_EvacPoint = null;
     continue;
    }
    m_EvacTaxi = squad;
    m_iEvacPhase = 2;
    m_fEvacSince = now;
    m_fEvacPhaseAt = now;
    squad.SetAITask(ACR_EAITask.RESCUE);
    squad.SetMode(ACR_EMoveMode.FAST);
    Travel(squad, base.GetOrigin(), false, 0);
    Say(string.Format("[ACR] AI %1: %2 takes the %3 civilians still aboard to %4", m_sFaction, squad.GetName(), ACR_Evac.Aboard(open, vehicle), base.m_sName));
    return true;
   }
  }
  return false;
 }

 // The free transport of ours nearest 'at' within EVAC_REACH, or null.

 protected ACR_Squad EvacTaxi(vector at)
 {
  ACR_Squad best;
  float bestDistance = EVAC_REACH;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (m_bHandedOnly && !squad.m_bAICommanded) continue;
   if (!m_Manager.m_Rides.IsFreeTransport(squad, m_sFaction) || StuckLately(squad)) continue;
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   if (!vehicle || ACR_Rides.FreeSeats(vehicle) < 2) continue;
   float d = vector.DistanceXZ(vehicle.GetOrigin(), at);
   if (d < bestDistance) { bestDistance = d; best = squad; }
  }
  return best;
 }

 // One step of the evac under way: "" while it goes on, else why it ended.
 protected string EvacStep(IEntity vehicle, float now)
 {
  int aboard;
  if (m_EvacPoint) aboard = ACR_Evac.Aboard(m_EvacPoint, vehicle);
  if (m_iEvacPhase == 0)
  {
   if (!ACR_Evac.Has(m_EvacPoint)) return "the evac point is gone";
   if (m_EvacTaxi.GaveUpUnreachable()) return "can't reach the evac point";
   if (vector.DistanceXZ(vehicle.GetOrigin(), m_EvacPoint.m_vPos) > EVAC_AT_XP) return "";
   m_iEvacPhase = 1;
   m_fEvacPhaseAt = now;
   m_EvacTaxi.Halt("Loading civilians");
   return "";
  }
  if (m_iEvacPhase == 1)
  {
   int onFoot;
   if (ACR_Evac.Has(m_EvacPoint)) onFoot = ACR_Evac.OnFoot(m_EvacPoint);
   bool leave = aboard > 0 && (ACR_Rides.FreeSeats(vehicle) == 0 || onFoot == 0 || now - m_fEvacPhaseAt > EVAC_LOAD_MS);
   if (!leave)
   {
    if (aboard == 0 && (onFoot == 0 || now - m_fEvacPhaseAt > EVAC_LOAD_MS * 2)) return "nobody to load";
    return "";
   }
   ACR_Base base = EvacBase(vehicle.GetOrigin());
   if (!base) return "no base of ours clear of the dead";
   m_iEvacPhase = 2;
   m_fEvacPhaseAt = now;
   Travel(m_EvacTaxi, base.GetOrigin(), false, 0);
   Say(string.Format("[ACR] AI %1: %2 drives %3 civilians to %4", m_sFaction, m_EvacTaxi.GetName(), aboard, base.m_sName));
   return "";
  }
  if (aboard == 0) return "delivered";
  if (m_EvacTaxi.GaveUpUnreachable()) return "can't reach the base";
  return "";
 }

 // Our base nearest 'at' with no zombie in it (ACR_Evac.DeliveryBase pays there), or null.
 protected ACR_Base EvacBase(vector at)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || (base.m_bBuilt && base.m_bHqLost) || base.m_sOwner != m_sFaction) continue;
   if (ACR_Containment.ZombiesNear(base.GetOrigin(), Math.Max(base.m_fRadius, 60))) continue;
   // Not one beside the evac point (a FOB built or a town taken there since): ACR_Evac.Deliver
   // pays nothing there, and the truck sat in it with its civilians until the evac timed out.
   if (m_EvacPoint && vector.DistanceXZ(base.GetOrigin(), m_EvacPoint.m_vPos) <= Math.Max(base.m_fRadius, 60) + ACR_Evac.XP_RADIUS) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), at);
   if (d < bestDistance) { bestDistance = d; best = base; }
  }
  return best;
 }

 void ManageEvacsForTest() { ManageEvacs(); }

 protected void ManagePallets()
 {
  if (!ACR_Containment.On()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  bool ours = !m_bTwoICAll || ACR_Evac.TwoICExtracts(m_sFaction);
  if (m_PalletSquad)
  {
   bool going = ours && ACR_Airdrop.Has(m_PalletTarget) && ACR_Airdrop.MayTake(m_PalletTarget, m_sFaction) && m_Manager.m_Squads.Contains(m_PalletSquad) && m_PalletSquad.CountAlive() > 0 && !m_PalletSquad.PlayerSteers() && m_PalletSquad.m_iAITask == ACR_EAITask.PALLET && !m_PalletSquad.GaveUpUnreachable() && now - m_fPalletSince <= SWEEP_MAX_MS;
   if (going) return;
   if (m_PalletSquad.m_iAITask == ACR_EAITask.PALLET) m_PalletSquad.SetAITask(ACR_EAITask.FREE);
   m_PalletSquad = null;
   m_PalletTarget = null;
  }
  if (!ours) return;
  ACR_Pallet pick;
  ACR_Squad squad;
  float best = float.MAX;
  for (int i = 0; i < ACR_Airdrop.Count(); i++)
  {
   ACR_Pallet pallet = ACR_Airdrop.At(i);
   if (!ACR_Airdrop.MayTake(pallet, m_sFaction)) continue;
   ACR_Squad near = PalletRunner(pallet.m_vPos);
   if (!near) continue;
   float d = vector.DistanceXZ(PositionOf(near), pallet.m_vPos);
   if (pallet.m_sFor != m_sFaction) d += 500; // ours first
   if (d >= best) continue;
   best = d;
   pick = pallet;
   squad = near;
  }
  if (!pick) return;
  string how = "";
  if (OnGrab(squad))
  {
   if (squad.m_AITarget) how = string.Format(" (off its grab of %1)", squad.m_AITarget.m_sName);
   LeaveAttacks(squad); // the grab ends without a "failed" report (m_bGivenAway); it may be planned again
   squad.m_AITarget = null;
  }
  else if (!squad.IsCombatUnit()) how = " (a truck)";
  m_PalletTarget = pick;
  m_PalletSquad = squad;
  m_fPalletSince = now;
  squad.SetAITask(ACR_EAITask.PALLET);
  if (squad.IsCombatUnit()) squad.SetMode(ACR_EMoveMode.COMBAT);
  else squad.SetMode(ACR_EMoveMode.FAST);
  Travel(squad, pick.m_vPos, false, 0);
  string whose = "our";
  if (pick.m_sFor != m_sFaction) whose = "the enemy's";
  Say(string.Format("[ACR] AI %1: %2 goes for %3 supply pallet at %4%5", m_sFaction, squad.GetName(), whose, ACR_Manager.Grid(pick.m_vPos), how));
 }

 void ManagePalletsForTest() { ManagePallets(); }

 // Truce (ACR_Truce, TvTvZ): an AI commander in command of its side accepts an offer, or offers one, when
 // the dead press its bases or it is losing; it refuses otherwise. It breaks a truce that has held
 // ACR_Truce.MIN_HOLD_MS once the dead are under control and it is ahead. The 2IC leaves the truce to
 // the commanding player.
 protected void ManageTruce()
 {
  if (m_bTwoICAll || m_bHandedOnly || !ACR_Truce.Possible()) return;
  bool pressed = DeadPress();
  int standing = Standing();
  int state = ACR_Truce.State();
  if (state == ACR_ETruce.OFFERED && ACR_Truce.By() != m_sFaction)
  {
   string reply;
   if (pressed || standing < 0) reply = ACR_Truce.Offer(m_sFaction);
   else reply = ACR_Truce.Refuse(m_sFaction);
   Say(string.Format("[ACR] AI %1: truce offer answered (dead pressing %2, standing %3): %4", m_sFaction, pressed, standing, reply));
   return;
  }
  if (state == ACR_ETruce.NONE && pressed && standing <= 0)
  {
   string offered = ACR_Truce.Offer(m_sFaction);
   if (offered == "Truce offered") Say(string.Format("[ACR] AI %1: offers a truce (dead pressing, standing %2)", m_sFaction, standing));
   return;
  }
  if (state == ACR_ETruce.HOLDS && ACR_Truce.HeldMs() >= ACR_Truce.MIN_HOLD_MS && !pressed && standing > 0 && ACR_Containment.LevelForTest() <= 1)
  {
   ACR_Truce.Break(m_sFaction);
   Say(string.Format("[ACR] AI %1: breaks the truce (dead under control, standing %2)", m_sFaction, standing));
  }
 }

 // The dead press us: zombies in or near a base of ours, or the outbreak out of control (level 3+).
 protected bool DeadPress()
 {
  if (ACR_Containment.On() && ACR_Containment.LevelForTest() >= 3) return true;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != m_sFaction) continue;
   if (ACR_Containment.ZombiesNear(base.GetOrigin(), base.m_fRadius + 300)) return true;
  }
  return false;
 }

 // Bases held against the other living side: +1 ahead (two more), -1 losing (two fewer), 0 even.
 protected int Standing()
 {
  int ours;
  int theirs;
  array<string> sides = ACR_Containment.Sides();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner.IsEmpty() || !sides.Contains(base.m_sOwner)) continue;
   if (base.m_sOwner == m_sFaction) ours++;
   else theirs++;
  }
  if (ours >= theirs + 2) return 1;
  if (ours + 2 <= theirs) return -1;
  return 0;
 }

 void ManageTruceForTest() { ManageTruce(); }

 bool TruceWantsForTest() { return DeadPress() || Standing() < 0; }

 protected bool StuckLately(ACR_Squad crew)
 {
  if (!crew) return false;
  float now = GetGame().GetWorld().GetWorldTime();
  return now - crew.m_fStuckAt < STUCK_SERVICE_WAIT_MS || now - crew.m_fGaveUpAt < STUCK_SERVICE_WAIT_MS;
 }

 bool StuckLatelyForTest(ACR_Squad crew) { return StuckLately(crew); }

 protected bool IsAssistingFobUpgrade(ACR_Squad squad)
 {
  if (!squad) return false;
  if (squad.m_Order == "Assisting FOB upgrade construction" && squad.m_Group && squad.m_Group.GetCurrentWaypoint()) return true;
  vector pos = PositionOf(squad);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || !base.m_bBuilt || base.m_sOwner != m_sFaction || !base.m_aFobSlots) continue;
   if (vector.DistanceXZ(pos, base.GetOrigin()) > base.m_fRadius) continue;
   if (ACR_FobSlots.AnyBuilding(base)) return true;
  }
  return false;
 }

 // Our base nearest the front that isn't threatened: where service vehicles wait.
 protected ACR_Base SupplyPoint()
 {
  ACR_Base best;
  float bestFront = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || base.m_bContested || ThreatTo(base) > 0) continue;
   float front = float.MAX;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction && Visible(other)) front = Math.Min(front, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (front < 500) continue; // too close to the enemy to park trucks
   float score = front;
   if (base == m_SupplyPoint) score -= 450; // stay put unless another safe base is much closer to the front
   if (score < bestFront) { bestFront = score; best = base; }
  }
  if (!best) best = HomeBase();
  m_SupplyPoint = best;
  return best;
 }
}
