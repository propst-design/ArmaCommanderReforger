// AI commander, engineering: FOB establishment and upgrades, roadblocks, minefields, fortify and base works,
// holders back to their posts, and why nobody builds.
//
// Split out of ACR_AICommander.c (12 Nov 2026). Enforce has no partial classes, so each part is a modded
// layer of the same class. Layers apply in file-name order and a layer can call only itself, earlier
// layers and the base, never a later one (base code calling a later layer fails to compile):
// ACR_AICommander.c < ACR_AICommanderBuild.c < ACR_AICommanderLogistics.c < ACR_AICommanderTwoIC.c.
// Fields, constants and anything an earlier part calls stay in ACR_AICommander.c.
modded class ACR_AICommander
{
 ACR_Base NearestDeployBase(ACR_Base fromBase)
 {
  if (!fromBase) return null;
  ACR_Base best;
  float bestDist = float.MAX;
  foreach (ACR_Base candidate : ACR_Base.s_All)
  {
   if (candidate.m_sOwner != m_sFaction || candidate.m_bRetired || !ACR_LogisticsRules.CanDeploy(candidate)) continue;
   float d = vector.DistanceXZ(fromBase.GetOrigin(), candidate.GetOrigin());
   if (d < bestDist)
   {
    bestDist = d;
    best = candidate;
   }
  }
  return best;
 }

 ACR_Base NeedsFob()
 {
  array<ACR_Base> frontBases = {};
  FrontlineBases(frontBases);
  if (frontBases.IsEmpty()) return null;

  foreach (ACR_Base front : frontBases)
  {
   if (!front) continue;
   float nearestDeploy = float.MAX;
   foreach (ACR_Base candidate : ACR_Base.s_All)
   {
    if (candidate.m_sOwner != m_sFaction || candidate.m_bRetired || !ACR_LogisticsRules.CanDeploy(candidate)) continue;
    float d = vector.DistanceXZ(front.GetOrigin(), candidate.GetOrigin());
    if (d < nearestDeploy) nearestDeploy = d;
   }

   if (nearestDeploy >= m_Doctrine.m_fFobFront)
   {
    bool hasCloseFob = false;
    foreach (ACR_Base existingFob : ACR_Base.s_All)
    {
     if (existingFob.m_sOwner != m_sFaction || !existingFob.m_bBuilt || existingFob.m_bRetired) continue;
     if (vector.DistanceXZ(front.GetOrigin(), existingFob.GetOrigin()) < FOB_EXISTING_PROXIMITY)
     {
      hasCloseFob = true;
      break;
     }
    }
    if (!hasCloseFob) return front;
   }
  }
  return null;
 }

 // A FOB site behind 'front' (ACR_FobSiting: the old offsets, scored), the whole search at once.
 bool FindFobSite(ACR_Base front, out vector site)
 {
  bool pending;
  return FindFobSiteStep(front, site, -1, pending);
 }

 bool FindFobSiteStep(ACR_Base front, out vector site, int most, out bool pending)
 {
  site = vector.Zero;
  pending = false;
  m_sFobSiteNone = "";
  if (!front) return false;
  ACR_Base deployBase = NearestDeployBase(front);
  vector dir = Vector(0, 0, -1);
  if (deployBase)
  {
   vector toDeploy = deployBase.GetOrigin() - front.GetOrigin();
   toDeploy[1] = 0;
   if (toDeploy.LengthSq() > 0.001)
   {
    toDeploy.Normalize();
    dir = toDeploy;
   }
  }
  array<ref ACR_FobSite> sites = {};
  string none;
  if (!ACR_FobSiting.Step(m_sFaction, front, dir, NearestEnemyBaseTo(front), most, sites, none))
  {
   pending = true;
   return false;
  }
  Avoided(vector.Zero); // drops the expired avoided sites
  ACR_FobSite best = ACR_FobSiting.Best(sites, m_aAvoidSites, string.Format("%1 %2 %3", m_sFaction, front.m_sName, m_aAvoidSites.Count()));
  if (!best)
  {
   m_sFobSiteNone = none;
   if (none.IsEmpty()) m_sFobSiteNone = string.Format("all %1 clear sites abandoned recently", sites.Count());
   return false;
  }
  site = best.m_vPos;
  NoteBuild("fob site " + front.m_sName, string.Format("[ACR] AI %1: FOB site behind %2 at %3: of %4 clear sites, %5", m_sFaction, front.m_sName, site, sites.Count(), ACR_FobSiting.Breakdown(best)));
  return true;
 }

 // The nearest base held by a side we fight, as far as we know (vector.Zero: none). Not the truce
 // partner's or an allied FIA pocket's, nor a hidden pocket we haven't found (NearestEnemyBase's rule).
 protected vector NearestEnemyBaseTo(ACR_Base from)
 {
  vector best = vector.Zero;
  float bestDistance = float.MAX;
  foreach (ACR_Base b : ACR_Base.s_All)
  {
   if (!b || b.m_bRetired || b.m_sOwner == m_sFaction || !HostileOwner(b) || !Visible(b)) continue;
   float d = vector.DistanceXZ(b.GetOrigin(), from.GetOrigin());
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = b.GetOrigin();
  }
  return best;
 }

 ACR_Squad FindConstructionSquad()
 {
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
  {
   if (!squad || squad.PlayerSteers() || squad.CountAlive() <= 0 || squad == m_FobEscort) continue;
   if (squad.m_iAITask == ACR_EAITask.ATTACK || squad.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
   // Not a squad giving or taking a ride (1 Nov live: a Humvee carrying medics was sent to build).
   if (m_Manager.m_Rides && m_Manager.m_Rides.RideOf(squad)) continue;
   IEntity truck = squad.ConstructionTruck();
   if (!truck || ACR_Logistics.Health(truck) <= 0) continue;
   if (m_Manager.m_Logistics && m_Manager.m_Logistics.StockOf(truck) >= ACR_FobBuild.LOADS_RESERVED)
    return squad;
  }
  return null;
 }

 // Any of our construction trucks still alive with a full 500-supply load.
 bool HasLoadedConstructionTruck()
 {
  array<IEntity> vehicles = {};
  ACR_Vehicles.All(vehicles);
  foreach (IEntity vehicle : vehicles)
  {
   if (!vehicle || ACR_Logistics.Health(vehicle) <= 0 || (ACR_Logistics.ServiceOf(vehicle) & ACR_EService.CONSTRUCTION) == 0) continue;
   FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
   if (!aff || !aff.GetAffiliatedFaction() || aff.GetAffiliatedFaction().GetFactionKey() != m_sFaction) continue;
   if (m_Manager.m_Logistics && m_Manager.m_Logistics.StockOf(vehicle) >= ACR_FobBuild.LOADS_RESERVED) return true;
  }
  return false;
 }

 bool BuyConstructionTruck(ACR_Base spawn)
 {
  if (!spawn) return false;
  // One purchase at a time: a truck already out there (crew still forming up, or parked) waits.
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_fFobTruckBoughtAt < FOB_TRUCK_RETRY_MS || HasLoadedConstructionTruck()) return false;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data || !data.m_aRequisitions) return false;

  int pick = -1;
  foreach (int e, ACR_RequisitionEntry offer : data.m_aRequisitions)
  {
   if (!ACR_Requisition.Available(data, offer) || offer.m_iCost > Purse(data) || offer.m_sVehicle.IsEmpty()) continue;
   if ((ACR_Logistics.ServiceOfName(offer.m_sVehicle) & ACR_EService.CONSTRUCTION) == 0) continue;
   if (pick < 0 || offer.m_iCost < data.m_aRequisitions[pick].m_iCost) pick = e;
  }
  if (pick < 0) return false;

  int cost = data.m_aRequisitions[pick].m_iCost;
  if (Purse(data) < cost + FOB_RP_COMBAT_RESERVE) return false;

  // PurchaseAt answers refusals too ("under attack", "Not enough RP"): only RP spent is a truck.
  int before = data.m_iRP;
  string bought = m_Manager.PurchaseAt(m_sFaction, pick, spawn);
  Say(string.Format("[ACR] AI %1 requisition (FOB construction truck): %2", m_sFaction, bought));
  bool paid = data.m_iRP < before || cost <= 0; // a free truck (mission config) can't be told apart: as before
  if (paid) m_fFobTruckBoughtAt = now;
  return paid;
 }

 ACR_Squad FindEscortSquad(ACR_Squad builder)
 {
  array<ACR_Squad> free = {};
  MySquads(free, true);
  if (builder) SortByDistance(free, PositionOf(builder));
  foreach (ACR_Squad squad : free)
  {
   if (!squad || squad == builder || squad.PlayerSteers() || squad.CountAlive() <= 0 || IsSupportTeam(squad)) continue;
   if (squad.IsCombatUnit()) return squad;
  }
  return null;
 }

 void ManageFobBuild()
 {
  if (HasActiveFobProject()) return;

  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextFobEval) return;
  m_fNextFobEval = now + FOB_EVAL_INTERVAL_MS;

  // Rebuild a destroyed HQ before planning anything new.
  vector site;
  ACR_Base front = HqLostFob();
  if (front) site = front.GetOrigin();
  else
  {
   front = NeedsFob();
   if (!front) return;
   bool pending;
   if (!FindFobSiteStep(front, site, ACR_FobSiting.VALIDATE_PER_STEP, pending))
   {
    // Site searches are expensive (an HQ and 15 spots per candidate): a few candidates a step,
    // the next step soon; with none clear, back off.
    if (pending)
    {
     m_fNextFobEval = now + FOB_SITE_STEP_MS;
     return;
    }
    m_fNextFobEval = now + FOB_SITE_RETRY_MS;
    Say(string.Format("[ACR] AI %1: no clear FOB site near %2 (%3), next look in 5 minutes", m_sFaction, front.m_sName, m_sFobSiteNone));
    return;
   }
  }

  ACR_Squad builder = FindConstructionSquad();
  if (!builder)
  {
   ACR_Base spawn = NearestDeployBase(front);
   if (!spawn) spawn = HomeBase();
   BuyConstructionTruck(spawn);
   return;
  }

  builder.SetAITask(ACR_EAITask.BUILD_FOB);
  if (!builder.EstablishFob(site))
  {
   builder.SetAITask(ACR_EAITask.FREE);
   return;
  }

  m_FobBuilder = builder;
  m_bFobProject = true;
  m_vFobTargetSite = site;

  ACR_Squad escort = FindEscortSquad(builder);
  if (escort)
  {
   m_FobEscort = escort;
   escort.SetAITask(ACR_EAITask.DEFEND);
   escort.m_AITarget = null; // it guards the site, not an old base (freed by HasActiveFobProject)
   escort.SetMode(ACR_EMoveMode.FAST);
   // (Travel: a far escort rides out rather than walking behind the truck.)
   Travel(escort, site, true, 0);
   Say(string.Format("[ACR] AI %1: %2 assigned as escort for FOB build at %3", m_sFaction, escort.GetName(), site));
  }

  Say(string.Format("[ACR] AI %1: dispatched %2 to establish forward FOB near %3 at %4", m_sFaction, builder.GetName(), front.m_sName, site));
 }

 // Our own FOB whose HQ is gone, quiet enough to rebuild.
 ACR_Base HqLostFob()
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || !base.m_bBuilt || base.m_bRetired || !base.m_bHqLost || base.m_sOwner != m_sFaction) continue;
   if (base.m_bContested || ACR_FobBuild.IsContested(base.GetOrigin(), m_sFaction) || Avoided(base.GetOrigin())) continue;
   return base;
  }
  return null;
 }

 bool Avoided(vector site)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = m_aAvoidSites.Count() - 1; i >= 0; i--)
  {
   if (now >= m_aAvoidUntil[i])
   {
    m_aAvoidSites.RemoveOrdered(i);
    m_aAvoidUntil.RemoveOrdered(i);
    continue;
   }
   if (vector.DistanceXZ(m_aAvoidSites[i], site) < FOB_SITE_AVOID_RADIUS) return true;
  }
  return false;
 }

 // A demolition charge on one of our FOBs: the nearest squad runs to defuse it.
 bool RespondToCharge(vector chargePos)
 {
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  ACR_Squad best;
  float bestDist = 500;
  foreach (ACR_Squad squad : mine)
  {
   if (!squad || squad.CountAlive() <= 0 || squad.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
   // Not one busy elsewhere: an attacker stayed counted in its attack, a builder's paid job
   // was cancelled by the move, a player's squad is his.
   int task = squad.m_iAITask;
   if (task != ACR_EAITask.FREE && task != ACR_EAITask.POSTED && task != ACR_EAITask.DEFEND) continue;
   if (squad.HasJob() || squad.PlayerSteers()) continue;
   float d = vector.DistanceXZ(PositionOf(squad), chargePos);
   if (d < bestDist)
   {
    bestDist = d;
    best = squad;
   }
  }
  if (!best) return false;
  ACR_Base nearestBase;
  float nearestBaseDist = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction) continue;
   float dist = vector.DistanceXZ(base.GetOrigin(), chargePos);
   if (dist < nearestBaseDist) { nearestBaseDist = dist; nearestBase = base; }
  }
  if (m_ReserveSquad == best) m_ReserveSquad = null;
  best.SetAITask(ACR_EAITask.DEFEND);
  best.m_AITarget = nearestBase;
  best.SetMode(ACR_EMoveMode.FAST);
  best.MoveTo(chargePos);
  Say(string.Format("[ACR] AI %1: %2 sent to defuse a demolition charge at %3 (%4 m)", m_sFaction, best.GetName(), chargePos, (int)bestDist));
  return true;
 }

 // handedOnly: only the FOBs a player handed to the AI (ACR_FobAI), with no squads sent to help.
 void ManageFobUpgrades(bool handedOnly = false)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextFobUpgradeEval) return;
  m_fNextFobUpgradeEval = now + FOB_UPGRADE_EVAL_INTERVAL_MS;

  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data) return;

  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || !base.m_bBuilt || base.m_bHqLost || base.m_sOwner != m_sFaction || !base.m_FobSupply) continue;
   if (base.m_bContested || ACR_FobBuild.IsContested(base.GetOrigin(), m_sFaction)) continue;
   if (handedOnly && !ACR_FobAI.Handed(base)) continue;

   // 1. Supply replenishment check
   OrderFobSupplies(base, data);

   // 2. Upgrade check & engineer acceleration
   if (!base.m_aFobSlots) ACR_FobSlots.InitSlots(base);
   if (ACR_FobSlots.AnyBuilding(base))
   {
    if (!handedOnly) AssistFobConstruction(base);
   }
   else
   {
    int stock = ACR_FobSupply.Stock(base.m_FobSupply);
    OrderFobUpgrade(base, stock);
   }
  }
 }

 // A player commands the side (or it has no AI commander): the FOBs handed to the AI (FOB
 // panel, ACR_FobAI) still get its supply orders and upgrades (ACR_Manager.TickAICommanders).
 void ManageHandedFobs()
 {
  ManageFobUpgrades(true);
 }

 bool OrderFobSupplies(ACR_Base base, ACR_FactionData data)
 {
  if (!base || !base.m_FobSupply || !data) return false;
  if (base.m_iSupplyDelivery > 0) return false;
  if (base.m_bContested || ACR_FobBuild.IsContested(base.GetOrigin(), m_sFaction)) return false;

  int stock = ACR_FobSupply.Stock(base.m_FobSupply);
  int capacity = ACR_FobSupply.CapacityOf(base);
  if (capacity - stock < ACR_FobSupply.DELIVERY_AMOUNT) return false;

  int minNeeded = 500;
  int nextTier = 0;
  int nextSlot = BestUpgradeSlotFor(base, nextTier);
  if (nextSlot >= 0) minNeeded = Math.Max(500, ACR_FobSlots.SlotCost(nextSlot, nextTier));
  if (stock >= minNeeded) return false;

  if (Purse(data) < ACR_FobSupply.DELIVERY_COST + FOB_RP_COMBAT_RESERVE) return false;

  string result = ACR_FobSupply.Order(m_Manager, m_sFaction, base);
  Say(string.Format("[ACR] AI %1 ordered supplies for %2: %3", m_sFaction, base.m_sName, result));
  return base.m_iSupplyDelivery > 0;
 }

 // The AI's FOB build order: the base buildings, Defences I, the tier-1 buildings,
 // Defences II, the helipad, then Defences III. Blocked spots are skipped.
 int BestUpgradeSlotFor(ACR_Base base, out int targetTier)
 {
  targetTier = 0;
  if (!base) return -1;
  if (!base.m_aFobSlots) ACR_FobSlots.InitSlots(base);

  array<int> order = {
   ACR_FobSlots.SLOT_RELAY, 0,
   ACR_FobSlots.SLOT_BARRACKS, 0,
   ACR_FobSlots.SLOT_REPAIR, 0,
   ACR_FobSlots.SLOT_FUEL, 0,
   ACR_FobSlots.SLOT_HOSPITAL, 0,
   ACR_FobSlots.SLOT_DEPOT, 0,
   ACR_FobSlots.SLOT_AMMO, 0,
   ACR_FobSlots.SLOT_DEFENCES, 0,
   ACR_FobSlots.SLOT_BARRACKS, 1,
   ACR_FobSlots.SLOT_REPAIR, 1,
   ACR_FobSlots.SLOT_DEFENCES, 1,
   ACR_FobSlots.SLOT_HELIPAD, 0,
   ACR_FobSlots.SLOT_DEFENCES, 2
  };

  for (int i = 0; i + 1 < order.Count(); i += 2)
  {
   int slotIdx = order[i];
   int tier = order[i + 1];
   ACR_FobSlot slot = ACR_FobSlots.SlotAt(base, slotIdx);
   if (!slot || slot.m_bBlocked) continue; // its spot is blocked: skip it
   bool mine = ACR_FobSlots.IsBuiltForOwner(slot, m_sFaction);
   bool empty = slot.m_eState == ACR_EFobSlotState.EMPTY || (slot.m_eState == ACR_EFobSlotState.BUILT && !mine);
   if (tier == 0 && empty)
   {
    targetTier = 0;
    return slotIdx;
   }
   if (tier > 0 && mine && slot.m_iTier == tier - 1)
   {
    targetTier = tier;
    return slotIdx;
   }
  }
  return -1;
 }

 bool OrderFobUpgrade(ACR_Base base, int stock)
 {
  if (!base) return false;
  if (!base.m_aFobSlots) ACR_FobSlots.InitSlots(base);

  int targetTier = 0;
  int pickSlot = BestUpgradeSlotFor(base, targetTier);
  if (pickSlot < 0) return false;

  int cost = ACR_FobSlots.SlotCost(pickSlot, targetTier);
  if (stock < cost) return false;

  string refusal;
  string outcome = ACR_FobSlots.OrderUpgradeBase(m_Manager, m_sFaction, base, pickSlot, refusal);
  if (!outcome.IsEmpty())
  {
   Say(string.Format("[ACR] AI %1: ordered FOB %2 upgrade %3 (tier %4)", m_sFaction, base.m_sName, ACR_FobSlots.SlotName(pickSlot, targetTier), targetTier));
   return true;
  }
  return false;
 }

 ACR_Squad AssistFobConstruction(ACR_Base base)
 {
  if (!base || ACR_FobSlots.HasEngineer(base)) return null;

  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
  {
   if (!squad || squad.PlayerSteers() || squad.CountAlive() <= 0) continue;
   // Only squads with nothing else to do: sappers laying mines, roadblock and Fortify holders,
   // sweepers, refits, rides out to build and the reserve were re-routed every 15 s, their paid
   // jobs cancelled (audit 31 Oct; RespondToCharge has the same rule).
   if (squad.m_iAITask != ACR_EAITask.FREE && squad.m_iAITask != ACR_EAITask.POSTED) continue;
   if (squad == m_ReserveSquad || IsScreen(squad) || squad.HasJob() || PendingBuildOf(squad)) continue;
   IEntity truck = squad.ConstructionTruck();
   bool hasTruck = truck && ACR_Logistics.Health(truck) > 0;
   if (!hasTruck && !ACR_SapperDemo.IsSapperSquad(squad)) continue;

   float dist = vector.DistanceXZ(PositionOf(squad), base.GetOrigin());
   if (dist <= 500.0)
   {
    squad.SetMode(ACR_EMoveMode.FAST);
    squad.RouteTo(base.GetOrigin(), ACR_Squad.WP_MOVE, -1, "Assisting FOB upgrade construction", 0, true);
    Say(string.Format("[ACR] AI %1: dispatched %2 to assist upgrade construction at %3", m_sFaction, squad.GetName(), base.m_sName));
    return squad;
   }
  }
  return null;
 }

 ACR_Squad FobBuilder() { return m_FobBuilder; }

 ACR_Base GetHomeBase() { return HomeBase(); }

 // Construction temporarily owns an AI squad. A finished checkpoint keeps its builder holding
 // it while KeepsHolding (the defences manned, playtest 25 Oct: the AI's roadblock and position
 // stood empty); otherwise it is an idle post again that attacks and defence may recall. Also
 // repairs completed ROADBLOCK tasks left by the earlier implementation.
 protected void ReleaseRoadblockBuilders()
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad || squad.m_iAITask != ACR_EAITask.ROADBLOCK) continue;
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (squad.PlayerSteers()) continue;
   if (squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || PendingBuildOf(squad)) continue;
   if (squad.m_ActiveRoadblock && KeepsHolding(squad)) continue;
   squad.SetAITask(ACR_EAITask.POSTED);
   Say(string.Format("[ACR] AI %1: %2 available for reassignment after roadblock construction", m_sFaction, squad.GetName()));
  }
 }

 void ReleaseBuildersForTest() { ReleaseRoadblockBuilders(); }

 // A builder stays on what it built while it holds it (its order), is fit to fight, and fewer
 // than the doctrine's m_iHolders other squads of ours fit to hold have held theirs longer:
 // those aren't sent on attacks, and defend a base when outnumbered there or as a last resort
 // (AssignDefenders; a Fortress's only when outnumbered). Counting all the other holders instead released every one of them when a
 // doctrine switch lowered the cap below their number (Fortress 4 -> Firepower 2: each saw 3).
 protected bool KeepsHolding(ACR_Squad squad)
 {
  if (!FitToHold(squad)) return false;
  int ahead;
  bool passed;
  foreach (ACR_Squad other : m_Manager.m_Squads)
  {
   if (other == squad)
   {
    passed = true;
    continue;
   }
   if (!other || !other.GetFaction() || other.GetFaction().GetFactionKey() != m_sFaction || !IsHolder(other) || !FitToHold(other)) continue;
   if (other.m_fOrderedAt < squad.m_fOrderedAt || (other.m_fOrderedAt == squad.m_fOrderedAt && !passed)) ahead++;
  }
  return ahead < HoldersCap();
 }

 protected bool FitToHold(ACR_Squad squad)
 {
  return squad.OrderState() == ACR_EOrderState.DEFEND && squad.StrengthRatio() >= RetreatRatio();
 }

 // Engineering may spend 'cost' now: out of the build budget while the share is on (it is set
 // aside already), else with 'reserve' RP to spare as well.
 bool CanBuild(int cost, int reserve)
 {
  // The lazy commander's 2IC never builds into the player's keep, budget or not.
  if (m_bTwoICAll && Purse() < cost) return false;
  // The budget can outrun the purse within a tick, now that units may spend past BUILD_FENCE.
  if (SideShare() > 0) return BuildBudget() >= cost && Purse() >= cost;
  // The 2IC (squads handed to the AI) builds only out of the budget, never from the side's RP,
  // which is the commanding player's (#14): with the share off it builds nothing.
  if (m_bHandedOnly && !m_bTwoICAll) return false;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  return data && Purse(data) >= cost + reserve;
 }

 protected void FrontBases(notnull array<ACR_Base> bases, notnull array<vector> enemies, int most)
 {
  bases.Clear();
  enemies.Clear();
  array<float> gaps = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != m_sFaction || base.m_bContested) continue;
   float nearest = float.MAX;
   vector enemyAt;
   foreach (ACR_Base other : ACR_Base.s_All)
   {
    // The enemy as this commander knows it (NearestEnemyBase's rule): roadblocks, mines and positions
    // faced the truce partner, an allied FIA pocket, or a pocket nobody on this side had found yet.
    if (!other || other.m_bRetired || other.m_sOwner == m_sFaction || !HostileOwner(other) || !Visible(other)) continue;

    float d = vector.DistanceXZ(base.GetOrigin(), other.GetOrigin());
    if (d >= nearest) continue;
    nearest = d;
    enemyAt = other.GetOrigin();
   }
   if (nearest == float.MAX) continue;
   int at = 0;
   while (at < gaps.Count() && gaps[at] <= nearest) at++;
   gaps.InsertAt(nearest, at);
   bases.InsertAt(base, at);
   enemies.InsertAt(enemyAt, at);
  }
  while (bases.Count() > most)
  {
   bases.Remove(bases.Count() - 1);
   enemies.Remove(enemies.Count() - 1);
  }
 }

 void FrontBasesOf(notnull array<ACR_Base> bases, notnull array<vector> enemies, int most) { FrontBases(bases, enemies, most); }

 void FrontBasesForTest(notnull array<ACR_Base> bases, notnull array<vector> enemies) { FrontBases(bases, enemies, FRONT_BASES); }

 // Manage roadblocks / observation posts (aca-f23). Every 15 s, one build at a time, while it
 // holds fewer than its doctrine's m_iRoadblocks and engineering can pay (CanBuild): at the first
 // of our front bases (FrontBases) with no roadblock of ours within 400 m, else at the first with
 // a second road towards the enemy that has none, a free squad within 450 m of it (else the
 // nearest within FAR_BUILDER) establishes one at the best road site 200-450 m out towards the
 // nearest enemy base (ACR_RoadblockSiting: sight down the road, no junction or dead end near,
 // chokepoints; playtest 25 Oct: the first road point found put two at a road's end and a
 // T-crossing).
 void ManageRoadblocks()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextRoadblockEval) return;
  m_fNextRoadblockEval = now + 15000;
  if (!CanBuild(ACR_Roadblocks.ROADBLOCK_COST, 25))
  {
   NoteBuild("roadblock pay", string.Format("[ACR] AI %1: no roadblock for now: engineering can't pay %2 RP", m_sFaction, ACR_Roadblocks.ROADBLOCK_COST));
   return;
  }

  // Dug-up checkpoints go first: their entries counted against the cap, and only a siting call
  // (never reached once the cap was met) pruned them, so the side never built another.
  ACR_Roadblocks.PruneRoadblocks();
  int activeCount = 0;
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (rb && rb.m_Entity && rb.m_sFaction == m_sFaction) activeCount++;
  }
  if (activeCount >= m_Doctrine.m_iRoadblocks) return;
  foreach (ACR_Squad builder : m_Manager.m_Squads)
  {
   if (builder && builder.m_iAITask == ACR_EAITask.ROADBLOCK && builder.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK && builder.GetFaction() && builder.GetFaction().GetFactionKey() == m_sFaction) return;
  }
  if (PendingBuilds(ACR_EAITask.ROADBLOCK) > 0) return;

  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return;
  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  FrontBases(fronts, enemies, FRONT_BASES);
  array<ACR_Squad> squads = {};
  MySquads(squads, true);
  // First the bases with no roadblock of ours near; then a second road into one that has.
  for (int pass = 0; pass < 2; pass++)
  {
   foreach (int f, ACR_Base frontlineBase : fronts)
   {
    bool covered = false;
    foreach (ACR_Roadblock rb2 : ACR_Roadblocks.s_aRoadblocks)
    {
     if (rb2 && rb2.m_sFaction == m_sFaction && vector.DistanceXZ(rb2.m_vPos, frontlineBase.GetOrigin()) < 400.0) covered = true;
    }
    if (covered != (pass == 1)) continue;
    // Scoring a base's road sites traces sight lines: it waits for a tick with budget left.
    if (PlanBudgetSpent())
    {
     NoteBuild("roadblock budget", string.Format("[ACR] AI %1: roadblock siting at %2 waits for a tick with planning budget left", m_sFaction, frontlineBase.m_sName));
     return;
    }
    vector enemyBasePos = enemies[f];
    vector basePos = frontlineBase.GetOrigin();
    int considered;
    string none;
    ACR_RoadSite site = ACR_RoadblockSiting.Best(basePos, enemyBasePos, m_sFaction, pass == 1, considered, none);
    if (!site)
    {
     if (pass == 0) NoteBuild("roadblock site " + frontlineBase.m_sName, string.Format("[ACR] AI %1: no roadblock site at %2: %3", m_sFaction, frontlineBase.m_sName, none));
     continue;
    }
    vector targetRoadPos = site.m_vPos;
    float targetYaw = site.m_fYaw;

    // An idle or posted squad within 450 m of this base, else the nearest within FAR_BUILDER.
    ACR_Squad candidate = null;
    ACR_Squad farther = null;
    float fartherDistance = FAR_BUILDER;
    foreach (ACR_Squad squad : squads)
    {
     if (!squad || squad.PlayerSteers() || squad.CountAlive() <= 0 || squad.IsVehicle() || IsSupportTeam(squad)) continue;
     if (squad == m_ReserveSquad || IsScreen(squad) || squad.m_Commander != 0) continue;
     if (squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || squad.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
     if (squad.m_iAITask != ACR_EAITask.POSTED && squad.m_iAITask != ACR_EAITask.FREE) continue;
     if (DueForRefit(squad)) continue;
     float d = vector.DistanceXZ(PositionOf(squad), basePos);
     if (d < 450.0)
     {
      candidate = squad;
      break;
     }
     if (d >= fartherDistance) continue;
     fartherDistance = d;
     farther = squad;
    }
    if (!candidate) candidate = farther;
    if (!candidate)
    {
     NoteBuild("roadblock squad " + frontlineBase.m_sName, string.Format("[ACR] AI %1: a roadblock site at %2 waits for a free squad within %3 m %4", m_sFaction, frontlineBase.m_sName, FAR_BUILDER, NoBuilderWhy(basePos, BUILDER_ROADBLOCK)));
     continue;
    }
    if (RideToBuild(candidate, ACR_EAITask.ROADBLOCK, frontlineBase, enemyBasePos, targetRoadPos, targetYaw))
    {
     Say(string.Format("[ACR] AI %1: %2 rides out to a roadblock site on the approach to %3 at %4: of %5 road sites, %6", m_sFaction, candidate.GetName(), frontlineBase.m_sName, targetRoadPos, considered, ACR_RoadblockSiting.Breakdown(site)));
     return;
    }
    if (!candidate.EstablishRoadblock(targetRoadPos, targetYaw, ACR_ERoadblockType.MEDIUM, false))
    {
     NoteBuild("roadblock order " + frontlineBase.m_sName, string.Format("[ACR] AI %1: %2 can't take the roadblock order at %3", m_sFaction, candidate.GetName(), frontlineBase.m_sName));
     continue;
    }
    candidate.SetAITask(ACR_EAITask.ROADBLOCK);
    string which = "";
    if (pass == 1) which = " (a second road)";
    Say(string.Format("[ACR] AI %1: dispatched %2%3 to establish roadblock on approach to %4 at %5%6: of %7 road sites, %8", m_sFaction, candidate.GetName(), FromFar(candidate, basePos, 450), frontlineBase.m_sName, targetRoadPos, which, considered, ACR_RoadblockSiting.Breakdown(site)));
    return;
   }
  }
 }

 protected void NoteBuild(string key, string line)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  float last;
  if (m_mBuildNotes.Find(key, last) && now - last < NOTE_S * 1000) return;
  m_mBuildNotes.Set(key, now);
  Say(line);
 }

 protected string FromFar(ACR_Squad candidate, vector pos, float near)
 {
  float d = vector.DistanceXZ(PositionOf(candidate), pos);
  if (d <= near) return "";
  return string.Format(" (none nearer: from %1 m away)", (int)d);
 }

 protected bool RideToBuild(ACR_Squad squad, int task, ACR_Base base, vector enemy, vector site, float yaw)
 {
  if (!m_Manager.m_Rides || squad.IsMounted()) return false;
  float distance = vector.DistanceXZ(PositionOf(squad), site);
  if (distance <= m_Doctrine.m_fRideDistance) return false;
  string why;
  if (!m_Manager.m_Rides.Request(squad, site, 0, ACR_Squad.WP_MOVE, -1, "Riding out to build", 0, why, true, false, m_bHandedOnly))
  {
   NoteBuild("ride " + squad.GetName(), string.Format("[ACR] AI %1: no ride for %2 to build at %3, %4 m out (%5): it walks", m_sFaction, squad.GetName(), base.m_sName, (int)distance, why));
   return false;
  }
  ACR_PendingBuild pending = new ACR_PendingBuild();
  pending.m_Squad = squad;
  pending.m_iTask = task;
  pending.m_Base = base;
  pending.m_vEnemy = enemy;
  pending.m_vSite = site;
  pending.m_fYaw = yaw;
  pending.m_fDeadline = GetGame().GetWorld().GetWorldTime() + distance / ACR_Minefields.TRAVEL_SPEED * 1000 + ACR_Squad.FOB_GIVE_UP_MS;
  m_aPendingBuilds.Insert(pending);
  squad.SetAITask(task);
  if (task != ACR_EAITask.ROADBLOCK) Say(string.Format("[ACR] AI %1: %2 rides out to %3 to %4 there (%5 m)", m_sFaction, squad.GetName(), base.m_sName, BuildName(task), (int)distance));
  return true;
 }

 protected static string BuildName(int task)
 {
  if (task == ACR_EAITask.ROADBLOCK) return "build a roadblock";
  if (task == ACR_EAITask.MINES) return "lay a minefield";
  return "dig in";
 }

 protected ACR_PendingBuild PendingBuildOf(ACR_Squad squad)
 {
  foreach (ACR_PendingBuild pending : m_aPendingBuilds)
  {
   if (pending.m_Squad == squad) return pending;
  }
  return null;
 }

 protected int PendingBuilds(int task)
 {
  int count;
  foreach (ACR_PendingBuild pending : m_aPendingBuilds)
  {
   if (pending.m_iTask == task) count++;
  }
  return count;
 }

 // Squads riding out to build: once out of the truck (the ride over, or fallen through and it
 // walked on), the job as if it stood there; given up past the deadline, or when the squad was
 // re-tasked, taken over or its base lost.
 protected void ManagePendingBuilds()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = m_aPendingBuilds.Count() - 1; i >= 0; i--)
  {
   ACR_PendingBuild pending = m_aPendingBuilds[i];
   ACR_Squad squad = pending.m_Squad;
   string what = BuildName(pending.m_iTask);
   bool gone = !squad || !m_Manager.m_Squads.Contains(squad) || squad.CountAlive() <= 0 || squad.PlayerSteers() || squad.m_iAITask != pending.m_iTask;
   if (gone || !pending.m_Base || pending.m_Base.m_sOwner != m_sFaction || now > pending.m_fDeadline)
   {
    m_aPendingBuilds.Remove(i);
    if (gone) continue;
    squad.SetAITask(ACR_EAITask.POSTED);
    Say(string.Format("[ACR] AI %1: %2 no longer rides out to %3 (base lost or past its deadline)", m_sFaction, squad.GetName(), what));
    continue;
   }
   if (m_Manager.m_Rides.RideOf(squad) || squad.IsMounted()) continue;
   m_aPendingBuilds.Remove(i);
   string from = string.Format(" (out of its ride, %1 m from %2)", (int)vector.DistanceXZ(PositionOf(squad), pending.m_Base.GetOrigin()), pending.m_Base.m_sName);
   bool ordered;
   bool paid;
   if (pending.m_iTask == ACR_EAITask.ROADBLOCK)
   {
    paid = CanBuild(ACR_Roadblocks.ROADBLOCK_COST, 25);
    ordered = paid && squad.EstablishRoadblock(pending.m_vSite, pending.m_fYaw, ACR_ERoadblockType.MEDIUM, false);
    if (ordered) Say(string.Format("[ACR] AI %1: %2%3 establishing the roadblock at %4", m_sFaction, squad.GetName(), from, pending.m_vSite));
   }
   else if (pending.m_iTask == ACR_EAITask.MINES)
   {
    paid = CanBuild(ACR_Minefields.Cost(ACR_EMineKind.MIXED), 30);
    ordered = paid && LayMines(squad, pending.m_Base, pending.m_vEnemy, from);
   }
   else if (pending.m_bWorks)
   {
    paid = CanBuild(ACR_Fortify.Cost(1), 30);
    string refusal;
    ordered = paid && ACR_BaseWorks.Order(squad, pending.m_Base, refusal);
    if (ordered)
    {
     squad.SetAITask(ACR_EAITask.FORTIFY);
     Say(string.Format("[ACR] AI %1: %2%3 fortifying the town at %4", m_sFaction, squad.GetName(), from, pending.m_Base.m_sName));
    }
   }
   else
   {
    paid = CanBuild(ACR_Fortify.Cost(1), 30);
    ordered = paid && DigIn(squad, pending.m_Base, pending.m_vEnemy, from);
   }
   if (ordered) continue;
   squad.SetAITask(ACR_EAITask.POSTED);
   string why = "engineering can't pay now";
   if (paid) why = "the order was refused there";
   Say(string.Format("[ACR] AI %1: %2 out of its ride, but can't %3 now (%4)", m_sFaction, squad.GetName(), what, why));
  }
 }

 void ManagePendingBuildsForTest() { ManagePendingBuilds(); }

 bool PendingBuildForTest(ACR_Squad squad) { return PendingBuildOf(squad) != null; }

 // Minefields on the approaches (-acrMinesAI 1; off by default). Every 30 s, one field at a
 // time, while it has fewer than its doctrine's m_iMinefields down and engineering can pay: at the
 // first of our front bases with no field of ours within 300 m, a free infantry squad within 600 m
 // of it (sappers first; else the nearest within FAR_BUILDER) lays one at the best scored spot
 // (ACR_MineSiting: on the road ahead of our roadblock or in front of our position where they cover
 // it, on the enemy's road route, in dead ground; the old ring 150-270 m out last), never within
 // DEFENCE_STANDOFF of our own defences: mixed on a road, AP mines on open ground.
 void ManageMinefields()
 {
  foreach (ACR_Squad layer : m_Manager.m_Squads)
  {
   if (!layer || layer.m_iAITask != ACR_EAITask.MINES || layer.m_MinePlan || PendingBuildOf(layer)) continue;
   if (!layer.GetFaction() || layer.GetFaction().GetFactionKey() != m_sFaction) continue;
   layer.SetAITask(ACR_EAITask.POSTED);
   Say(string.Format("[ACR] AI %1: %2 available for reassignment after laying mines", m_sFaction, layer.GetName()));
  }
  // Planning a field queries the roads and terrain: it waits for a tick with budget left.
  if (!ACR_Minefields.AIUses()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextMinefieldEval) return;
  if (PlanBudgetSpent())
  {
   NoteBuild("mines budget", string.Format("[ACR] AI %1: minefield planning waits for a tick with planning budget left", m_sFaction));
   return;
  }
  m_fNextMinefieldEval = now + 30000;
  if (ACR_Minefields.CountFields(m_sFaction) >= m_Doctrine.m_iMinefields) return;
  int cost = ACR_Minefields.Cost(ACR_EMineKind.MIXED);
  if (!CanBuild(cost, 30))
  {
   NoteBuild("mines pay", string.Format("[ACR] AI %1: no minefield for now: engineering can't pay %2 RP", m_sFaction, cost));
   return;
  }
  // One field at a time: any squad of ours on its way to lay one. (The free squads alone leave
  // out the one already laying, so two were sent to the same spot, playtest 25 Oct.)
  foreach (ACR_Squad laying : m_Manager.m_Squads)
  {
   if (laying && laying.m_MinePlan && laying.GetFaction() && laying.GetFaction().GetFactionKey() == m_sFaction) return;
  }
  if (PendingBuilds(ACR_EAITask.MINES) > 0) return;

  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  FrontBases(fronts, enemies, FRONT_BASES);
  array<ACR_Squad> squads = {};
  MySquads(squads, true);
  foreach (int f, ACR_Base front : fronts)
  {
   if (ACR_Minefields.FieldNear(m_sFaction, front.GetOrigin(), 300)) continue;
   // A squad that can lay mines within 600 m of the base (sappers first), else the nearest
   // within FAR_BUILDER.
   ACR_Squad candidate = null;
   ACR_Squad farther = null;
   float fartherDistance = FAR_BUILDER;
   foreach (ACR_Squad squad : squads)
   {
    if (!squad || squad.PlayerSteers() || squad == m_ReserveSquad || IsScreen(squad) || IsSupportTeam(squad) || !ACR_Minefields.CanLay(squad)) continue;
    if (squad.m_iAITask != ACR_EAITask.POSTED && squad.m_iAITask != ACR_EAITask.FREE) continue;
    if (squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || squad.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
    if (DueForRefit(squad)) continue;
    float d = vector.DistanceXZ(PositionOf(squad), front.GetOrigin());
    if (d > 600)
    {
     if (d >= fartherDistance) continue;
     fartherDistance = d;
     farther = squad;
     continue;
    }
    if (!candidate || (ACR_SapperDemo.IsSapperSquad(squad) && !ACR_SapperDemo.IsSapperSquad(candidate))) candidate = squad;
   }
   if (!candidate) candidate = farther;
   if (!candidate)
   {
    NoteBuild("mines squad " + front.m_sName, string.Format("[ACR] AI %1: a minefield at %2 waits for a free squad that can lay mines within %3 m %4", m_sFaction, front.m_sName, FAR_BUILDER, NoBuilderWhy(front.GetOrigin(), BUILDER_MINES)));
    continue;
   }

   if (RideToBuild(candidate, ACR_EAITask.MINES, front, enemies[f], front.GetOrigin(), 0)) return;
   if (LayMines(candidate, front, enemies[f], FromFar(candidate, front.GetOrigin(), 600))) return;
  }
 }

 // Lays a field round 'front' with 'candidate' at the best scored spot it may lay at
 // (ACR_MineSiting: covered by our roadblock or position, on the enemy's road, dead ground; the old
 // ring last); true if ordered, else notes why not.
 protected bool LayMines(ACR_Squad candidate, ACR_Base front, vector enemyPos, string from)
 {
  array<ref ACR_MineSite> sites = {};
  int crowded;
  ACR_MineSiting.Candidates(this, m_sFaction, front.GetOrigin(), enemyPos, sites, crowded);
  int refused = 0;
  string refusal = "";
  foreach (ACR_MineSite site : sites)
  {
   // The plan snaps the field onto a road through the spot, which can bring it inside the
   // standoff the spot passed: judge where it would really lie (Plan charges nothing). A shape (#200,
   // ACR_MineSiting.ShapeFor) is judged by every corner too; one that doesn't fit lays the square.
   array<vector> corners = {};
   string shape = ACR_MineSiting.ShapeFor(site, enemyPos, corners);
   ACR_MinePlan probe = ACR_Minefields.Plan(candidate, site.m_eKind, site.m_vPos, refusal, vector.Zero, corners);
   if (!corners.IsEmpty() && (!probe || !ClearOfOwnDefences(probe.m_vSite) || !OutlineClear(probe)))
   {
    corners.Clear();
    shape = "square";
    probe = ACR_Minefields.Plan(candidate, site.m_eKind, site.m_vPos, refusal);
   }
   if (probe && !ClearOfOwnDefences(probe.m_vSite))
   {
    refused++;
    refusal = "the road through it is too near our own defences";
    continue;
   }
   if (!ACR_Minefields.Order(candidate, site.m_eKind, site.m_vPos, refusal, vector.Zero, corners))
   {
    refused++;
    continue;
   }
   candidate.SetAITask(ACR_EAITask.MINES);
   Say(string.Format("[ACR] AI %1: %2 laid as a %3", m_sFaction, ACR_Minefields.Name(site.m_eKind), shape));
   Say(string.Format("[ACR] AI %1: %2%3 laying %4 on the approach to %5 at %6: of %7 spots, %8", m_sFaction, candidate.GetName(), from, ACR_Minefields.Name(site.m_eKind), front.m_sName, site.m_vPos, sites.Count(), ACR_MineSiting.Breakdown(site)));
   return true;
  }
  string why = string.Format("%1 spots, %2 more too near our own defences or bases", sites.Count(), crowded);
  if (refused > 0) why += string.Format(", %1 refused (last: %2)", refused, refusal);
  NoteBuild("mines site " + front.m_sName, string.Format("[ACR] AI %1: no minefield site at %2: %3", m_sFaction, front.m_sName, why));
  return false;
 }

 // Every corner of a shaped field's outline clear of our defences as its middle must be (#200).
 protected bool OutlineClear(ACR_MinePlan plan)
 {
  foreach (vector corner : plan.m_aOutline)
  {
   if (!ClearOfOwnDefences(corner)) return false;
  }
  return true;
 }

 bool ClearOfOwnDefences(vector pos)
 {
  if (ACR_Fortify.NearestOwn(m_sFaction, pos, DEFENCE_STANDOFF)) return false;
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (rb && rb.m_sFaction == m_sFaction && vector.DistanceXZ(rb.m_vPos, pos) < DEFENCE_STANDOFF) return false;
  }
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (squad.m_FortifyPlan && vector.DistanceXZ(squad.m_FortifyPlan.m_vSite, pos) < DEFENCE_STANDOFF) return false;
   if (squad.RoadblockPending() && vector.DistanceXZ(squad.m_vRoadblockSite, pos) < DEFENCE_STANDOFF) return false;
  }
  foreach (ACR_PendingBuild pending : m_aPendingBuilds)
  {
   if (pending.m_iTask == ACR_EAITask.ROADBLOCK && vector.DistanceXZ(pending.m_vSite, pos) < DEFENCE_STANDOFF) return false;
  }
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base && !base.m_bRetired && base.m_sOwner == m_sFaction && vector.DistanceXZ(base.GetOrigin(), pos) < base.m_fRadius + 40) return false;
  }
  return true;
 }

 // Fortified positions on the approaches (-acrFortifyAI 1; off by default). Every 60 s, one
 // order at a time: first man an empty position at a front base (ManPositions, free); then, while
 // engineering can pay, upgrade our lowest-tier position in use below tier III (nearest the front
 // first), worked by the squad holding it or the nearest free one within FAR_BUILDER; else, with
 // fewer than its doctrine's m_iFortifyPositions in use (PositionsInUse), dig the nearest free
 // infantry squad (within FAR_BUILDER) in at the best scored spot out from the first of our front
 // bases with no position of ours within 300 m, facing the enemy (DigIn, ACR_PositionSiting), never
 // within DEFENCE_STANDOFF of our own fields. A position whose upgrade was refused is left alone
 // for a while (ACR_Fortify.HoldUpgrades). The squad then holds it while KeepsHolding, else it is
 // posted again.
 void ManageFortify()
 {
  foreach (ACR_Squad digger : m_Manager.m_Squads)
  {
   if (!digger || digger.m_iAITask != ACR_EAITask.FORTIFY || digger.m_FortifyPlan || PendingBuildOf(digger)) continue;
   if (!digger.GetFaction() || digger.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (digger.m_Fortification && KeepsHolding(digger)) continue;
   digger.SetAITask(ACR_EAITask.POSTED);
  }
  // Planning a position queries the terrain round it: it waits for a tick with budget left.
  if (!ACR_Fortify.AIUses()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextFortifyEval) return;
  if (PlanBudgetSpent())
  {
   NoteBuild("fortify budget", string.Format("[ACR] AI %1: fortify planning waits for a tick with planning budget left", m_sFaction));
   return;
  }
  m_fNextFortifyEval = now + 60000;
  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  FrontBases(fronts, enemies, FRONT_BASES);
  if (fronts.IsEmpty()) return;
  // An empty position first: manning it costs nothing.
  if (ManPositions(fronts)) return;
  foreach (ACR_Squad busy : m_Manager.m_Squads)
  {
   if (busy && busy.m_FortifyPlan && busy.GetFaction() && busy.GetFaction().GetFactionKey() == m_sFaction) return;
  }
  if (PendingBuilds(ACR_EAITask.FORTIFY) > 0) return;
  // Tactical points on the lane's flanks are works sites too, front or not: anti-flank strongpoints.
  array<ACR_Base> worksSites = {};
  worksSites.Copy(fronts);
  if (LaneActive())
  {
   foreach (ACR_Base flank : ACR_Base.s_All)
   {
    if (flank.m_bRetired || flank.m_sOwner != m_sFaction || flank.m_bIncome || flank.m_bContested) continue;
    if (m_Lane.Flanks(flank.GetOrigin()) && !worksSites.Contains(flank)) worksSites.Insert(flank);
   }
  }
  if (ManageBaseWorks(worksSites)) return;
  string refusal;

  ACR_Fortification target;
  foreach (ACR_Fortification fort : ACR_Fortify.s_Positions)
  {
   if (fort.m_sFaction != m_sFaction || fort.m_iTier >= ACR_Fortify.MAX_TIER || ACR_Fortify.BeingBuilt(fort) || ACR_Fortify.UpgradesHeld(fort)) continue;
   if (!InUse(fort, fronts)) continue; // left behind by the front, empty: not worth RP
   if (target && fort.m_iTier > target.m_iTier) continue;
   if (target && fort.m_iTier == target.m_iTier && vector.DistanceXZ(fort.m_vPos, fronts[0].GetOrigin()) >= vector.DistanceXZ(target.m_vPos, fronts[0].GetOrigin())) continue;
   target = fort;
  }
  if (target)
  {
   int upgradeCost = ACR_Fortify.Cost(target.m_iTier + 1);
   ACR_Squad worker = HolderOf(target);
   if (!worker) worker = FortifyWorkerNear(target.m_vPos);
   if (!CanBuild(upgradeCost, 30))
   {
    NoteBuild("fortify upgrade pay", string.Format("[ACR] AI %1: no upgrade of the position at %2 for now: engineering can't pay %3 RP", m_sFaction, target.m_vPos, upgradeCost));
   }
   else if (!worker)
   {
    NoteBuild("fortify upgrade squad", string.Format("[ACR] AI %1: upgrading the position at %2 waits for a free squad within %3 m %4", m_sFaction, target.m_vPos, FAR_BUILDER, NoBuilderWhy(target.m_vPos, BUILDER_FORTIFY)));
   }
   else if (ACR_Fortify.Order(worker, target.m_vPos, refusal))
   {
    worker.SetAITask(ACR_EAITask.FORTIFY);
    Say(string.Format("[ACR] AI %1: %2%3 upgrading the position at %4 to %5", m_sFaction, worker.GetName(), FromFar(worker, target.m_vPos, FORTIFY_NEAR), target.m_vPos, ACR_Fortify.TierName(target.m_iTier + 1)));
    return;
   }
   else
   {
    NoteBuild("fortify upgrade order", string.Format("[ACR] AI %1: no upgrade of the position at %2: %3", m_sFaction, target.m_vPos, refusal));
   }
  }

  if (PositionsInUse(fronts) >= m_Doctrine.m_iFortifyPositions) return;
  if (!CanBuild(ACR_Fortify.Cost(1), 30))
  {
   NoteBuild("fortify pay", string.Format("[ACR] AI %1: no new position for now: engineering can't pay %2 RP", m_sFaction, ACR_Fortify.Cost(1)));
   return;
  }
  foreach (int f, ACR_Base front : fronts)
  {
   if (ACR_Fortify.NearestOwn(m_sFaction, front.GetOrigin(), 300)) continue;
   ACR_Squad candidate = FortifyWorkerNear(front.GetOrigin());
   if (!candidate)
   {
    NoteBuild("fortify squad " + front.m_sName, string.Format("[ACR] AI %1: a position at %2 waits for a free squad within %3 m %4", m_sFaction, front.m_sName, FAR_BUILDER, NoBuilderWhy(front.GetOrigin(), BUILDER_FORTIFY)));
    continue;
   }
   if (RideToBuild(candidate, ACR_EAITask.FORTIFY, front, enemies[f], front.GetOrigin(), 0)) return;
   if (DigIn(candidate, front, enemies[f], FromFar(candidate, front.GetOrigin(), FORTIFY_NEAR))) return;
  }
 }

 protected bool ManageBaseWorks(array<ACR_Base> fronts)
 {
  if (!ACR_BaseWorks.Enabled() || !m_Doctrine || m_Doctrine.m_iFortifyPositions <= 0) return false;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data) return false;
  array<ACR_Base> open = {};
  array<float> scores = {};
  foreach (ACR_Base front : fronts)
  {
   // What it already built, shot away: back first, and it asks no bank.
   bool repair = ACR_BaseWorks.NeedsRepair(front);
   int next = ACR_BaseWorks.TierOf(front) + 1;
   if (front.m_bBuilt) continue;
   if (!repair && (next > ACR_Fortify.MAX_TIER || Purse(data) < next * WORKS_BANK_STEP || !CanBuild(ACR_Fortify.Cost(next), 30))) continue;
   if (repair && !CanBuild(ACR_Fortify.Cost(1), 30)) continue;
   open.Insert(front);
   float score = ThreatTo(front) + 1 - DefenceOf(front) * 0.1;
   if (repair) score += 1;
   if (FlankStrongpoint(front)) score += m_Doctrine.m_fFlankWorks;
   scores.Insert(score);
  }
  if (open.IsEmpty()) return false;
  ACR_Base base = open[ACR_Variety.Pick(scores, string.Format("base works %1 %2", m_sFaction, (int)GetGame().GetWorld().GetWorldTime()))];
  if (FlankStrongpoint(base)) Say(string.Format("[ACR] AI %1: %2 guards the flank of the push toward %3", m_sFaction, base.m_sName, m_Lane.m_Objective.m_sName));
  ACR_Squad worker = FortifyWorkerNear(base.GetOrigin());
  if (!worker) worker = WorksWorkerFurther(base.GetOrigin());
  // Far off it rides, as to a position. No truck: it walks only if that isn't the rest of the match.
  if (worker && RideToBuild(worker, ACR_EAITask.FORTIFY, base, base.GetOrigin(), base.GetOrigin(), 0))
  {
   ACR_PendingBuild riding = PendingBuildOf(worker);
   if (riding) riding.m_bWorks = true;
   return true;
  }
  if (worker && vector.DistanceXZ(PositionOf(worker), base.GetOrigin()) > WORKS_WALK) worker = null;
  if (!worker)
  {
   NoteBuild("works squad " + base.m_sName, string.Format("[ACR] AI %1: works at %2 wait for a free squad within %3 m %4", m_sFaction, base.m_sName, FAR_BUILDER, NoBuilderWhy(base.GetOrigin(), BUILDER_FORTIFY)));
   return false;
  }
  string refusal;
  if (!ACR_BaseWorks.Order(worker, base, refusal))
  {
   NoteBuild("works " + base.m_sName, string.Format("[ACR] AI %1: no works at %2: %3", m_sFaction, base.m_sName, refusal));
   return false;
  }
  worker.SetAITask(ACR_EAITask.FORTIFY);
  Say(string.Format("[ACR] AI %1: %2 fortifying the town at %3", m_sFaction, worker.GetName(), base.m_sName));
  return true;
 }

 // A tactical point (no income) of ours on the lane's flank: it gets works first, to hold the flank.
 protected bool FlankStrongpoint(ACR_Base base)
 {
  return base && LaneActive() && !base.m_bIncome && base.m_sOwner == m_sFaction && m_Lane.Flanks(base.GetOrigin());
 }

 bool FlankStrongpointForTest(ACR_Base base) { return FlankStrongpoint(base); }

 protected ACR_Squad WorksWorkerFurther(vector pos)
 {
  array<ACR_Squad> squads = {};
  MySquads(squads, false);
  ACR_Squad free;
  ACR_Squad attacker;
  float freeBest = WORKS_FAR;
  float attackBest = FAR_BUILDER;
  int attacking;
  foreach (ACR_Squad squad : squads)
  {
   if (!squad || squad.m_iAITask != ACR_EAITask.ATTACK) continue;
   attacking++;
  }
  foreach (ACR_Squad other : squads)
  {
   if (!other || other.PlayerSteers() || other == m_ReserveSquad || IsScreen(other) || IsSupportTeam(other) || !ACR_Fortify.CanFortify(other) || DueForRefit(other)) continue;
   if (other.m_MinePlan || other.m_ActiveRoadblock || other.m_Fortification || other.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || other.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
   float d = vector.DistanceXZ(PositionOf(other), pos);
   bool isFree = (other.m_iAITask == ACR_EAITask.FREE || other.m_iAITask == ACR_EAITask.POSTED) && other.IsCombatUnit() && !IsRetreating(other);
   if (isFree && d < freeBest)
   {
    freeBest = d;
    free = other;
   }
   if (other.m_iAITask == ACR_EAITask.ATTACK && attacking >= WORKS_ATTACK_SPARE && d < attackBest && other.KnownEnemies(60, true) == 0)
   {
    attackBest = d;
    attacker = other;
   }
  }
  if (free) return free;
  return attacker;
 }

 protected bool ManPositions(array<ACR_Base> fronts)
 {
  int holding;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad && squad.GetFaction() && squad.GetFaction().GetFactionKey() == m_sFaction && IsHolder(squad) && FitToHold(squad)) holding++;
  }
  if (holding >= HoldersCap()) return false;
  foreach (ACR_Fortification fort : ACR_Fortify.s_Positions)
  {
   if (fort.m_sFaction != m_sFaction || ManningSquad(fort) != null || ACR_Fortify.BeingBuilt(fort) || !AtFront(fort, fronts)) continue;
   ACR_Squad worker = FortifyWorkerNear(fort.m_vPos);
   if (!worker)
   {
    NoteBuild("man " + fort.m_iId.ToString(), string.Format("[ACR] AI %1: the empty position at %2 waits for a free squad within %3 m %4", m_sFaction, fort.m_vPos, FAR_BUILDER, NoBuilderWhy(fort.m_vPos, BUILDER_FORTIFY)));
    continue;
   }
   if (!worker.ManFortification(fort)) continue;
   worker.SetAITask(ACR_EAITask.FORTIFY);
   Say(string.Format("[ACR] AI %1: %2%3 mans the empty %4 at %5", m_sFaction, worker.GetName(), FromFar(worker, fort.m_vPos, FORTIFY_NEAR), ACR_Fortify.TierName(fort.m_iTier), fort.m_vPos));
   return true;
  }
  return false;
 }

 protected bool AtFront(ACR_Fortification fort, array<ACR_Base> fronts)
 {
  foreach (ACR_Base front : fronts)
  {
   if (vector.DistanceXZ(fort.m_vPos, front.GetOrigin()) <= FRONT_POSITION_REACH) return true;
  }
  return false;
 }

 // Manned, or at a front base (to be manned): an empty one the front has left behind is not.
 protected bool InUse(ACR_Fortification fort, array<ACR_Base> fronts)
 {
  return ManningSquad(fort) != null || AtFront(fort, fronts);
 }

 // Our positions that count towards the doctrine's m_iFortifyPositions: those in use. Empty ones
 // the front left behind no longer stop new ones at the front.
 protected int PositionsInUse(array<ACR_Base> fronts)
 {
  int count;
  foreach (ACR_Fortification fort : ACR_Fortify.s_Positions)
  {
   if (fort.m_sFaction == m_sFaction && InUse(fort, fronts)) count++;
  }
  return count;
 }

 int PositionsInUseForTest()
 {
  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  FrontBases(fronts, enemies, FRONT_BASES);
  return PositionsInUse(fronts);
 }

 // Digs 'candidate' in at the best scored spot out from 'front' towards 'enemyPos' that takes it
 // (ACR_PositionSiting: field of fire, slight high ground, no dead ground close in front, cover
 // behind, its upgrades fit, overlooking our roadblock or field; the old line spots too), clear of
 // our fields; true if ordered, else notes why not.
 protected bool DigIn(ACR_Squad candidate, ACR_Base front, vector enemyPos, string from)
 {
  array<ref ACR_PositionSite> sites = {};
  int nearFields;
  ACR_PositionSiting.Candidates(candidate, m_sFaction, front.GetOrigin(), enemyPos, sites, nearFields);
  int refused = 0;
  string refusal = "";
  foreach (ACR_PositionSite site : sites)
  {
   if (!ACR_Fortify.Order(candidate, site.m_vPos, refusal, site.AimAt(enemyPos)))
   {
    refused++;
    continue;
   }
   candidate.SetAITask(ACR_EAITask.FORTIFY);
   // Better spots the order refused, so a low score can be told from poor ground.
   string skipped = "";
   if (refused > 0) skipped = string.Format("; %1 better spots refused (last: %2)", refused, refusal);
   Say(string.Format("[ACR] AI %1: %2%3 digging in on the approach to %4 at %5: of %6 spots, %7%8", m_sFaction, candidate.GetName(), from, front.m_sName, site.m_vPos, sites.Count(), ACR_PositionSiting.Breakdown(site), skipped));
   return true;
  }
  string why = string.Format("%1 spots, %2 more near our own fields", sites.Count(), nearFields);
  if (refused > 0) why += string.Format(", %1 refused (last: %2)", refused, refusal);
  NoteBuild("fortify site " + front.m_sName, string.Format("[ACR] AI %1: no position site at %2: %3", m_sFaction, front.m_sName, why));
  return false;
 }

 // The squad of ours holding this position, if any.
 protected ACR_Squad HolderOf(ACR_Fortification fort)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad && squad.m_Fortification == fort && IsHolder(squad) && !squad.PlayerSteers()) return squad;
  }
  return null;
 }

 // RETURN_MS varied by up to 15% per squad (ACR_Variety, seeded from the match seed): holders
 // don't all stop looking for their posts on the same tick.
 protected float ReturnLimit(ACR_Squad squad)
 {
  return ACR_Variety.Jitter(RETURN_MS, 0.15, string.Format("return %1 %2", m_sFaction, squad.GetName()));
 }

 protected void ReturnToPosts()
 {
  if (m_LeftPost.IsEmpty() && m_LeftRoadblock.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_Squad> done = {};
  foreach (ACR_Squad squad, ACR_Fortification fort : m_LeftPost)
  {
   float at = 0;
   m_LeftPostAt.Find(squad, at);
   if (!squad || !fort || !m_Manager.m_Squads.Contains(squad) || squad.CountAlive() <= 0 || !ACR_Fortify.s_Positions.Contains(fort) || fort.m_sFaction != m_sFaction || now - at > ReturnLimit(squad))
   {
    done.Insert(squad);
    continue;
   }
   // Still defending, or taken by something else since: it waits.
   if (squad.m_iAITask != ACR_EAITask.FREE && squad.m_iAITask != ACR_EAITask.POSTED) continue;
   done.Insert(squad);
   if (squad.PlayerSteers() || !Ours(squad) || ManningSquad(fort) != null) continue;
   if (Holding() >= m_Doctrine.m_iHolders) continue;
   if (!squad.ManFortification(fort)) continue;
   squad.SetAITask(ACR_EAITask.FORTIFY);
   PrintFormat("[ACR] AI %1: %2 goes back to hold the %3 at %4 it left to defend", m_sFaction, squad.GetName(), ACR_Fortify.TierName(fort.m_iTier), fort.m_vPos);
  }
  foreach (ACR_Squad left, ACR_Roadblock rb : m_LeftRoadblock)
  {
   float leftAt = 0;
   m_LeftPostAt.Find(left, leftAt);
   if (!left || !rb || !m_Manager.m_Squads.Contains(left) || left.CountAlive() <= 0 || !ACR_Roadblocks.s_aRoadblocks.Contains(rb) || !rb.m_Entity || rb.m_sFaction != m_sFaction || now - leftAt > ReturnLimit(left))
   {
    done.Insert(left);
    continue;
   }
   if (left.m_iAITask != ACR_EAITask.FREE && left.m_iAITask != ACR_EAITask.POSTED) continue;
   done.Insert(left);
   if (left.PlayerSteers() || !Ours(left) || RoadblockManningSquad(rb) != null) continue;
   if (Holding() >= m_Doctrine.m_iHolders) continue;
   if (!left.ManRoadblock(rb)) continue;
   left.SetAITask(ACR_EAITask.ROADBLOCK);
   PrintFormat("[ACR] AI %1: %2 goes back to hold the %3 at %4 it left to defend", m_sFaction, left.GetName(), ACR_Roadblocks.Name(rb.m_iType), rb.m_vPos);
  }
  foreach (ACR_Squad gone : done)
  {
   m_LeftPost.Remove(gone);
   m_LeftRoadblock.Remove(gone);
   m_LeftPostAt.Remove(gone);
  }
 }

 // The squad (AI or player) holding a roadblock, on its way back to one included.
 protected ACR_Squad RoadblockManningSquad(ACR_Roadblock rb)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad && squad.m_ActiveRoadblock == rb && squad.CountAlive() > 0) return squad;
  }
  return null;
 }

 // The squads of ours holding what we built (and fit to).
 protected int Holding()
 {
  int holding;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad && squad.GetFaction() && squad.GetFaction().GetFactionKey() == m_sFaction && IsHolder(squad) && FitToHold(squad)) holding++;
  }
  return holding;
 }

 void ReturnToPostsForTest() { ReturnToPosts(); }

 void NoteLeftPostForTest(ACR_Squad squad) { NoteLeftPost(squad); }

 // Why no squad was found for a build at pos: what the side's free squads are, and how many of the
 // rest are busy. The pn9 playtest (26 Oct) logged "waits for a free squad" while the status line
 // said "free 3": the three were unarmed trucks. E.g. "(free: 3 unarmed vehicles; busy: 5
 // defending, 2 attacking)".
 protected string NoBuilderWhy(vector pos, int kind)
 {
  array<ACR_Squad> free = {};
  MySquads(free, true);
  int unarmed, crews, support, reserve, refitting, jobs, unfit, far, inReach, players;
  float nearest = -1;
  foreach (ACR_Squad squad : free)
  {
   if (!squad) continue;
   if (squad.PlayerSteers()) players++;
   else if (squad == m_ReserveSquad) reserve++;
   else if (IsSupportTeam(squad)) support++;
   else if (squad.IsVehicle() && !squad.IsCombatUnit()) unarmed++;
   else if (squad.IsVehicle() && kind != BUILDER_MINES) crews++;
   else if (DueForRefit(squad)) refitting++;
   else if (squad.m_MinePlan || squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || squad.OrderState() == ACR_EOrderState.BUILD_FOB || squad.m_ActiveRoadblock || squad.m_Fortification) jobs++;
   else if ((kind == BUILDER_FORTIFY && !ACR_Fortify.CanFortify(squad)) || (kind == BUILDER_MINES && !ACR_Minefields.CanLay(squad))) unfit++;
   else
   {
    float d = vector.DistanceXZ(PositionOf(squad), pos);
    if (d < FAR_BUILDER)
    {
     inReach++;
     continue;
    }
    far++;
    if (nearest < 0 || d < nearest) nearest = d;
   }
  }
  string parts = "";
  if (unarmed > 0) parts += ", " + Several(unarmed, "unarmed vehicle", "unarmed vehicles");
  if (crews > 0) parts += ", " + Several(crews, "vehicle crew", "vehicle crews");
  if (support > 0) parts += ", " + Several(support, "support team", "support teams");
  if (reserve > 0) parts += ", the reserve";
  if (refitting > 0) parts += string.Format(", %1 due to refit", refitting);
  if (jobs > 0) parts += ", " + Several(jobs, "on another job", "on other jobs");
  if (unfit > 0 && kind == BUILDER_MINES) parts += string.Format(", %1 that can't lay mines", unfit);
  else if (unfit > 0) parts += string.Format(", %1 that can't dig in", unfit);
  if (inReach > 0) parts += string.Format(", %1 in reach but not free for this job", inReach);
  if (far > 0) parts += string.Format(", %1 further out (nearest %2 m)", far, (int)nearest);
  if (players > 0) parts += ", " + Several(players, "led by a player", "led by players");
  int defending, attacking, other;
  foreach (ACR_Squad busy : m_Manager.m_Squads)
  {
   if (!busy || !busy.GetFaction() || busy.GetFaction().GetFactionKey() != m_sFaction || free.Contains(busy)) continue;
   if (busy.m_iAITask == ACR_EAITask.DEFEND) defending++;
   else if (busy.m_iAITask == ACR_EAITask.ATTACK) attacking++;
   else other++;
  }
  string why = "(none free";
  if (!parts.IsEmpty()) why = "(free: " + parts.Substring(2, parts.Length() - 2);
  why += string.Format("; busy: %1 defending, %2 attacking, %3 other)", defending, attacking, other);
  return why;
 }

 // "1 support team", "2 support teams".
 protected static string Several(int n, string one, string many)
 {
  if (n == 1) return "1 " + one;
  return n.ToString() + " " + many;
 }

 string NoBuilderWhyForTest(vector pos, int kind) { return NoBuilderWhy(pos, kind); }

 protected ACR_Squad FortifyWorkerNear(vector pos)
 {
  ACR_Squad candidate;
  float candidateDistance = FAR_BUILDER;
  array<ACR_Squad> squads = {};
  MySquads(squads, true);
  foreach (ACR_Squad squad : squads)
  {
   if (!squad || squad.PlayerSteers() || squad == m_ReserveSquad || IsSupportTeam(squad) || !ACR_Fortify.CanFortify(squad)) continue;
   if (squad.m_MinePlan || squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || squad.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
   if (squad.m_ActiveRoadblock || squad.m_Fortification) continue;
   if (DueForRefit(squad)) continue;
   float d = vector.DistanceXZ(PositionOf(squad), pos);
   if (d >= candidateDistance) continue;
   candidateDistance = d;
   candidate = squad;
  }
  return candidate;
 }

 bool CanBuildHandedForTest(int cost, int reserve)
 {
  m_bHandedOnly = true;
  bool ok = CanBuild(cost, reserve);
  m_bHandedOnly = false;
  return ok;
 }
}
