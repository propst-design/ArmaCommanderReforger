// AI commander requisition, reinforcement, refit tracking, and unit-class helpers; extracted from ACR_AICommander.c.
class ACR_AIRequisition
{
 static int FullSizeOf(ACR_AICommander cmd, ACR_Squad squad)
 {
  if (!squad) return 0;
  array<ResourceName> missing = {};
  squad.GetMissing(missing);
  return squad.CountAlive() + missing.Count();
 }

 static bool IsSmallTeamEntry(ACR_RequisitionEntry entry)
 {
  if (!entry || !entry.m_sVehicle.IsEmpty() || entry.m_bMortarTeam) return false;
  if (entry.m_aSoldiers) return entry.m_aSoldiers.Count() <= 3;
  int baseCost = entry.m_iBaseCost;
  if (baseCost <= 0) baseCost = entry.m_iCost;
  if (baseCost <= 9) return true;
  string name = entry.m_sName;
  name.ToLower();
  if (name.Contains("squad") || name.Contains("section") || name.Contains("platoon") || name.Contains("fire team") || name.Contains("fireteam")) return false;
  return name.Contains("team");
 }

 // A walk back to refill given up on the way to 'base' (null: not known): that base is skipped
 // for REFIT_SKIP_MS (RefitGaveUpOn). Returns how long the squad waits before its next try.
 static float NoteRefitGaveUp(ACR_AICommander cmd, ACR_Squad squad, ACR_Base base, float now)
 {
  if (base)
  {
   cmd.m_RefitGaveUpOn.Set(squad, base);
   cmd.m_RefitGaveUpAt.Set(squad, now);
  }
  int giveUps = RefitGiveUps(cmd, squad) + 1;
  cmd.m_RefitGiveUps.Set(squad, giveUps);
  if (giveUps > ACR_AICommander.REFIT_RETRY_MAX) giveUps = ACR_AICommander.REFIT_RETRY_MAX;
  return ACR_AICommander.REINFORCE_RETRY_MS * giveUps;
 }

 static int RefitGiveUps(ACR_AICommander cmd, ACR_Squad squad)
 {
  int giveUps;
  if (!cmd.m_RefitGiveUps.Find(squad, giveUps)) return 0;
  return giveUps;
 }

 static bool RefitGaveUpOn(ACR_AICommander cmd, ACR_Squad squad, ACR_Base base, float now)
 {
  ACR_Base given;
  if (!base || !cmd.m_RefitGaveUpOn.Find(squad, given) || given != base) return false;
  float at;
  if (!cmd.m_RefitGaveUpAt.Find(squad, at)) return false;
  return now - at < ACR_AICommander.REFIT_SKIP_MS;
 }

 static void ForgetRefit(ACR_AICommander cmd, ACR_Squad squad)
 {
  cmd.m_RefitWalkTo.Remove(squad);
  cmd.m_RefitGaveUpOn.Remove(squad);
  cmd.m_RefitGaveUpAt.Remove(squad);
  cmd.m_RefitGiveUps.Remove(squad);
 }

 // Refill depleted infantry squads out of contact (especially 1-man remnants that
 // dropped out of combat status) instead of leaving them idle and buying 2-man teams.
 static bool ReinforceDepleted(ACR_AICommander cmd, ACR_FactionData data, int keep)
 {
  if (!data || !cmd.m_Manager.m_Requisition) return false;
  // At the soldier cap only the refill is skipped: the walks still end (prune, give-ups, arrival), or
  // squads walking back sat on RESUPPLY, out of every defence and attack, until the side lost men (review, 12 Nov).
  bool capped = cmd.m_Manager.m_Requisition.OverSoldierCap(cmd.m_sFaction);
  array<ACR_Squad> all = {};
  cmd.MySquads(all, false);
  array<ACR_Squad> gone = {};
  foreach (ACR_Squad walker, float until : cmd.m_Refitting)
   if (!walker || !all.Contains(walker)) gone.Insert(walker);
  foreach (ACR_Squad lost : gone) cmd.m_Refitting.Remove(lost);
  ACR_Squad best;
  int bestScore = -1;
  int bestCost = 0;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_Squad squad : all)
  {
   if (!squad || squad.PlayerSteers() || squad.m_bMortar || squad.Aircraft() || ACR_Transport.CrewedVehicle(squad)) continue;
   if (cmd.InAssault(squad)) continue;
   float retryAt;
   if (cmd.m_ReinforceRetryAt.Find(squad, retryAt))
   {
    if (now < retryAt) continue;
    cmd.m_ReinforceRetryAt.Remove(squad);
   }
   array<ResourceName> missing = {};
   squad.GetMissing(missing);
   if (missing.IsEmpty())
   {
    cmd.m_Refitting.Remove(squad);
    continue;
   }
   // Builders stay on their FOB or roadblock (ManageFobBuild / ManageRoadblocks own them).
   if (squad.m_iAITask == ACR_EAITask.BUILD_FOB || squad.m_iAITask == ACR_EAITask.ROADBLOCK) continue;
   bool remnant = !squad.IsCombatUnit();
   float giveUpAt;
   bool refitting = cmd.m_Refitting.Find(squad, giveUpAt);
   if (!remnant && !refitting && squad.m_iAITask != ACR_EAITask.FREE && squad.m_iAITask != ACR_EAITask.POSTED && squad.m_iAITask != ACR_EAITask.DEFEND && squad.m_iAITask != ACR_EAITask.RESUPPLY) continue;
   if (!remnant && squad.StrengthRatio() > 0.65) continue;
   // A base whose circle it is in now, else the nearest one out of the fighting.
   ACR_Base inside;
   ACR_Base refit;
   float insideDist = float.MAX;
   float refitDist = float.MAX;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner != cmd.m_sFaction || !ACR_LogisticsRules.CanReinforce(base) || base.m_bContested) continue;
    float d = vector.DistanceXZ(cmd.PositionOf(squad), base.GetOrigin());
    if (d <= base.m_fRadius && d < insideDist) { insideDist = d; inside = base; }
    if (cmd.ThreatTo(base) == 0 && d < refitDist && !RefitGaveUpOn(cmd, squad, base, now)) { refitDist = d; refit = base; }
   }
   // Only inside the circle: there the replacements come from this base and the squad
   // gets its ammo too. Badly hurt idle squads walk back out of the fighting to refill
   // (posted ones too: their post's waypoint used to keep them at the front for good);
   // others wait until they pass a base.
   if (!inside)
   {
    if (cmd.m_AmmoRuns.Contains(squad)) continue; // walking back to rearm: that base refills it too
    // Re-sending the walk every tick reset the squad's waypoint recovery, so an unreachable
    // walk never ended until the 15-min limit (Everon 26 Oct: 200 dropped waypoints).
    if (refitting && (now >= giveUpAt || squad.GaveUpUnreachable()))
    {
     cmd.m_Refitting.Remove(squad);
     if (squad.m_iAITask == ACR_EAITask.RESUPPLY) squad.SetAITask(ACR_EAITask.FREE);
     ACR_Base walkedTo;
     cmd.m_RefitWalkTo.Find(squad, walkedTo);
     cmd.m_RefitWalkTo.Remove(squad);
     float retryMs = NoteRefitGaveUp(cmd, squad, walkedTo, now);
     cmd.m_ReinforceRetryAt.Set(squad, now + retryMs);
     string baseName = "?";
     if (walkedTo) baseName = walkedTo.m_sName;
     cmd.Say(string.Format("[ACR] AI %1: %2 gave up walking back to reinforce at %3 (%4 in a row); next try in %5 s, elsewhere", cmd.m_sFaction, squad.GetName(), baseName, RefitGiveUps(cmd, squad), (int)(retryMs / 1000)));
     continue;
    }
    if (refitting && squad.m_Group && (squad.m_Group.GetCurrentWaypoint() || squad.MovePending())) continue;
    // Don't cut short a walk to a service vehicle (ManageLogistics).
    if (!refitting && squad.m_iAITask == ACR_EAITask.RESUPPLY && squad.m_Group && squad.m_Group.GetCurrentWaypoint()) continue;
    if (capped)
    {
     // No walk is started or sent again while no replacements can come: one waiting for a new waypoint ends.
     if (refitting)
     {
      cmd.m_Refitting.Remove(squad);
      cmd.m_RefitWalkTo.Remove(squad);
      if (squad.m_iAITask == ACR_EAITask.RESUPPLY) squad.SetAITask(ACR_EAITask.FREE);
     }
     continue;
    }
    bool idle = squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED || squad.m_iAITask == ACR_EAITask.RESUPPLY;
    if (!refit || !(refitting || remnant || (idle && squad.StrengthRatio() < cmd.m_Doctrine.m_fRefitRatio))) continue;
    if (!refitting) cmd.m_Refitting.Set(squad, now + ACR_AICommander.REFIT_GIVE_UP_MS);
    squad.SetAITask(ACR_EAITask.RESUPPLY);
    squad.SetMode(ACR_EMoveMode.FAST);
    cmd.Travel(squad, ACR_Manager.Ground(refit.GetOrigin()), false, 0);
    cmd.m_RefitWalkTo.Set(squad, refit);
    cmd.Say(string.Format("[ACR] AI %1: depleted squad %2 (%3% strength) moving to %4 to reinforce", cmd.m_sFaction, squad.GetName(), (int)(squad.StrengthRatio() * 100), refit.m_sName));
    continue;
   }
   cmd.m_RefitGiveUps.Remove(squad); // in a base's circle: a walk that got there ends the run of give-ups
   cmd.m_RefitWalkTo.Remove(squad);
   // Arrived: the walk is over. Left on RESUPPLY while contact, a blocked relay or no RP held the
   // refill, it sat out of every defence of the base it stood in, for good (audit 31 Oct); free,
   // it is still refilled here once that clears.
   if (refitting)
   {
    cmd.m_Refitting.Remove(squad);
    if (squad.m_iAITask == ACR_EAITask.RESUPPLY) squad.SetAITask(ACR_EAITask.FREE);
   }
   if (capped) continue;
   if (cmd.m_Manager.m_Requisition.InContact(squad) != "") continue;
   if (ACR_LogisticsRules.RelayBlocked(ACR_FobSupply.Nearby(cmd.m_sFaction, cmd.PositionOf(squad)))) continue;
   // Same price ReinforceSquad checks: replacements plus a FOB's instant restock fee.
   int cost = missing.Count() * ACR_Requisition.REINFORCE_COST_PER_SOLDIER;
   ACR_Base depot = cmd.m_Manager.m_Requisition.DeployBaseAt(cmd.m_sFaction, cmd.PositionOf(squad));
   if (depot) cost += cmd.m_Manager.m_Requisition.InstantRestockCost(squad, depot);
   if (cmd.Purse(data) - keep < cost)
   {
    if (!cmd.m_RefillWaiting) cmd.m_RefillWaiting = squad;
    continue;
   }
   int score = missing.Count() * 10;
   if (remnant) score += 100;
   if (score > bestScore) { bestScore = score; best = squad; bestCost = cost; }
  }
  if (!best) return false;
  // ReinforceSquad charges only for soldiers who actually joined.
  int rpBefore = cmd.m_Manager.m_Bases.GetRP(cmd.m_sFaction);
  string outcome = cmd.m_Manager.m_Requisition.ReinforceSquad(null, best);
  if (cmd.m_Manager.m_Bases.GetRP(cmd.m_sFaction) >= rpBefore)
  {
   // Nothing joined: don't pick the same squad again every tick.
   cmd.m_ReinforceRetryAt.Set(best, now + ACR_AICommander.REINFORCE_RETRY_MS);
   cmd.Say(string.Format("[ACR] AI %1 reinforce failed, retry in %2 s: %3", cmd.m_sFaction, (int)(ACR_AICommander.REINFORCE_RETRY_MS / 1000), outcome));
   return false;
  }
  if (best.m_iAITask == ACR_EAITask.RESUPPLY) best.SetAITask(ACR_EAITask.FREE);
  cmd.m_Refitting.Remove(best);
  cmd.Say(string.Format("[ACR] AI %1 reinforce: %2", cmd.m_sFaction, outcome));
  return true;
 }

 static void Requisition(ACR_AICommander cmd)
 {
  if (ACR_DoctrineKits.Pending(cmd.m_sFaction)) return; // its starting kit first
  ACR_FactionSettings settings = cmd.Settings();
  int cap = 8;
  if (settings) cap = settings.m_iAIUnitCap;
  // Money piling up: field a few more squads rather than sit on it.
  int banked = cmd.Purse();
  int maxCapBonus = 4;
  int myBases = 0;
  foreach (ACR_Base b : ACR_Base.s_All) if (b.m_sOwner == cmd.m_sFaction) myBases++;
  if (ACR_Base.s_All.Count() > 24 && myBases > ACR_Base.s_All.Count() / 2 && banked >= 400) maxCapBonus = 8;
  if (banked > 150) cap += Math.Min(maxCapBonus, (banked - 100) / 50);
  ACR_FactionData data = cmd.m_Manager.m_Bases.GetFaction(cmd.m_sFaction);
  if (!data) return;
  // Refills come first and are treated as urgent troops: they may spend the fire-support
  // reserve (only the engineering budget is kept), and while a squad at a base waits for the
  // RP to refill, no new units are bought. Everon 26 Oct: refills kept the whole 75 RP
  // reserve the AI rarely had, so in nearly 3 h RU refilled 3 squads and bought 21 units
  // while 4-7 squads per side sat at bases with 1-3 men.
  cmd.m_RefillWaiting = null;
  ReinforceDepleted(cmd, data, cmd.BuildKeep());
  if (cmd.m_RefillWaiting)
  {
   float refillNow = GetGame().GetWorld().GetWorldTime();
   if (refillNow >= cmd.m_fNextRefillLog)
   {
    cmd.m_fNextRefillLog = refillNow + 60000;
    cmd.Say(string.Format("[ACR] AI %1: holding purchases until %2 can be refilled (%3 RP)", cmd.m_sFaction, cmd.m_RefillWaiting.GetName(), cmd.Purse(data)));
   }
   return;
  }
  array<ACR_Squad> mine = {};
  cmd.MySquads(mine, false);
  // Only fighting units count against the cap (trucks and lone drivers don't).
  int combatLine = 0;
  float capUsed = 0;
  int supportTeams = 0;
  foreach (ACR_Squad existing : mine)
  {
   if (IsSpecialistSquad(existing)) supportTeams++;
   else if (existing.IsCombatUnit())
   {
    combatLine++;
    capUsed += CapSlots(cmd, existing);
   }
  }
  // Deploy at the forward-most safe deployment base (military/airfield/FOB with
  // ThreatTo == 0 and at least FOB_DEPLOY_MIN_FRONT from any non-owned base);
  // fall back to the furthest rear base when all forward bases are under threat.
  ACR_Base spawn;
  ACR_Base forwardSpawn;
  float best = -1;
  float bestForward = 100000;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != cmd.m_sFaction || !ACR_LogisticsRules.CanDeploy(base) || base.m_bContested) continue;
   // distance to the nearest non-owned base = how far behind the lines it is
   float nearestFront = 100000;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != cmd.m_sFaction) nearestFront = Math.Min(nearestFront, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (cmd.ThreatTo(base) > 0) nearestFront = 0;
   else if (nearestFront >= ACR_AICommander.FOB_DEPLOY_MIN_FRONT)
   {
    float forwardScore = nearestFront;
    if (base.m_bBuilt) forwardScore -= 150;
    if (forwardScore < bestForward) { bestForward = forwardScore; forwardSpawn = base; }
   }
   if (nearestFront > best) { best = nearestFront; spawn = base; }
  }
  if (forwardSpawn) spawn = forwardSpawn;
  if (!spawn) return;
  // A service truck for a need nobody can meet (squads low on ammo, a wrecked
  // BRDM...), one of each kind at a time. Trucks don't count against the cap, so a side at its
  // cap still replaces a lost one (7 Nov: the cap check came first, and a full side that lost its
  // only fuel truck went without until it lost a squad).
  if (cmd.m_Manager.m_Logistics)
  {
   array<int> services = {ACR_EService.AMMO, ACR_EService.REPAIR, ACR_EService.FUEL, ACR_EService.MEDICAL};
   foreach (int service : services)
   {
    if ((cmd.m_iNeeds & service) == 0 || cmd.m_Manager.m_Logistics.CountStations(cmd.m_sFaction, service) > 0) continue;
    int pick = -1;
    foreach (int e, ACR_RequisitionEntry offer : data.m_aRequisitions)
    {
     if (!ACR_Requisition.Available(data, offer) || offer.m_iCost > cmd.Purse(data) || offer.m_sVehicle.IsEmpty()) continue;
     if ((ACR_Logistics.ServiceOfName(offer.m_sVehicle) & service) == 0) continue;
     if (pick < 0 || offer.m_iCost < data.m_aRequisitions[pick].m_iCost) pick = e;
    }
    if (pick < 0) continue;
    cmd.Say(string.Format("[ACR] AI %1 requisition (%2 support): %3", cmd.m_sFaction, ACR_Logistics.ServiceName(service), cmd.m_Manager.PurchaseAt(cmd.m_sFaction, pick, spawn)));
    return;
   }
  }
  if (capUsed >= cap) return;
  // Squads keep having to walk far with no truck to fetch them: buy a transport first.
  int maxTransports = 4;
  if (ACR_Manager.ThreatOption() == 2) maxTransports = 5;
  maxTransports += cmd.m_Doctrine.m_iTransports;
  // A small party (a team, a squad's remnant) found no ride: a jeep, right-sized and cheap.
  bool urgent = cmd.UrgentTroops(combatLine, cap);
  bool lightShort = cmd.m_Manager.m_Rides && cmd.m_Manager.m_Rides.ShortOfLightRide(cmd.m_sFaction);
  if (lightShort && BuyLightRide(cmd, data, spawn, urgent)) return;
  if (cmd.m_Manager.m_Rides && ((lightShort && !urgent) || cmd.m_Manager.m_Rides.ShortOfTruck(cmd.m_sFaction)) && cmd.m_Manager.m_Rides.CountTransports(cmd.m_sFaction) < maxTransports)
  {
   // The cheapest truck; a rich side (#28) takes an armed one (an APC) when it has one.
   bool armedRide = cmd.FireFactor() > 1;
   int truck = -1;
   foreach (int t, ACR_RequisitionEntry candidate : data.m_aRequisitions)
   {
    if (!candidate.m_bTransport || ACR_AICommander.IsServiceVehicle(candidate) || candidate.m_iRemaining <= 0 || candidate.m_iCost > cmd.Purse(data)) continue;
    if (truck < 0) { truck = t; continue; }
    ACR_RequisitionEntry held = data.m_aRequisitions[truck];
    if (armedRide && candidate.m_bArmed != held.m_bArmed) { if (candidate.m_bArmed) truck = t; continue; }
    if (candidate.m_iCost < held.m_iCost) truck = t;
   }
   if (truck >= 0)
   {
    cmd.Say(string.Format("[ACR] AI %1 requisition (transport): %2", cmd.m_sFaction, cmd.m_Manager.PurchaseAt(cmd.m_sFaction, truck, spawn)));
    cmd.m_Manager.m_Rides.m_NoTransportAt.Remove(cmd.m_sFaction);
    cmd.m_Manager.m_Rides.m_NoLightAt.Remove(cmd.m_sFaction);
    return;
   }
  }
  // Buy up to two units per tick. The pick is weighted by price, so real fighting
  // power (rifle squads, MG teams, armed vehicles) wins over a swarm of cheap
  // two-man teams; specialist teams (engineers, sappers, medics, logistics,
  // recon) are rare and never doubled up.
  // Keep a fire-support budget unless troops are needed more.
  int keep = 0;
  bool highThreat = ACR_Manager.ThreatOption() == 2;
  if (highThreat)
  {
   // High intensity should field its fire support even while the infantry pool
   // is still building. These checks remain stock/RP/pad gated and buy at most
   // one support unit per strategy tick.
   keep = cmd.SupportBudget();
   if (cmd.BuyMortarTeam(spawn, data)) return;
   if (cmd.BuyHelicopter(data)) return;
  }
  else if (!urgent || cmd.m_Doctrine.m_bSupportsFirst)
  {
   // Firepower still buys its guns when short of troops, but doesn't sit on mission money then.
   if (!urgent) keep = cmd.SupportBudget();
   if (cmd.BuyMortarTeam(spawn, data)) return;
   if (cmd.BuyHelicopter(data)) return;
  }
  // The engineering budget's next order (-acrAIBuildShare) isn't for units, unless troops are short:
  // a poor side's budget can be all it holds (Chernarus 6 Nov: 16 of 16 RP).
  if (!urgent) keep += cmd.BuildKeep();
  int smallTeams, lineSquads, vehicles;
  foreach (ACR_Squad existing : mine)
  {
   if (ACR_Transport.CrewedVehicle(existing)) vehicles++;
   else if (FullSizeOf(cmd, existing) <= 3) smallTeams++;
   else lineSquads++;
  }
  bool hasLineOffer = false;
  foreach (ACR_RequisitionEntry checkOffer : data.m_aRequisitions)
  {
   if (ACR_Requisition.Available(data, checkOffer) && checkOffer.m_sVehicle.IsEmpty() && !checkOffer.m_bMortarTeam && !IsSmallTeamEntry(checkOffer) && !IsSpecialistTeam(checkOffer))
   {
    hasLineOffer = true;
    break;
   }
  }
  int budget = cmd.Purse(data) - keep;
  // Save up for a pricier unit only with a working force in the field; short of
  // troops, or when troops are urgent, buy what is affordable now.
  int saveMax = Math.Max(budget, 85);
  if (combatLine < ACR_AICommander.SAVE_MIN_UNITS || (urgent && !highThreat)) saveMax = budget;
  if (cmd.m_iSaveForEntry >= 0 && cmd.m_iSaveForEntry < data.m_aRequisitions.Count())
  {
   ACR_RequisitionEntry savedEntry = data.m_aRequisitions[cmd.m_iSaveForEntry];
   if (!ACR_Requisition.Available(data, savedEntry) || savedEntry.m_iCost > saveMax || (combatLine < 2 && savedEntry.m_iCost > budget) || (savedEntry.m_eFormation == ACR_EFormation.SF && HasSpecialForces(cmd)))
    cmd.m_iSaveForEntry = -1;
   else if (savedEntry.m_iCost > budget)
    return;
  }
  else cmd.m_iSaveForEntry = -1;

  bool boughtSF;
  float richFactor = cmd.FireFactor();
  // The cap counts fighting units only (as above): trucks, ambulances, crewless
  // vehicles and 1-man remnants filled it on Everon and the AI sat on 1000 RP.
  for (int n = 0; n < 2 && capUsed < cap; n++)
  {
   budget = cmd.Purse(data) - keep;
   int pick = -1;
   if (cmd.m_iSaveForEntry >= 0 && cmd.m_iSaveForEntry < data.m_aRequisitions.Count() && ACR_Requisition.Available(data, data.m_aRequisitions[cmd.m_iSaveForEntry]) && data.m_aRequisitions[cmd.m_iSaveForEntry].m_iCost <= budget)
   {
    pick = cmd.m_iSaveForEntry;
    cmd.m_iSaveForEntry = -1;
    cmd.m_iLastSavedLog = -1;
   }
   else
   {
    array<int> options = {};
    array<float> weights = {};
    float total;
    foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
    {
     if (!ACR_Requisition.Available(data, entry) || entry.m_iCost > saveMax) continue;
     if (entry.m_eFormation == ACR_EFormation.SF && (boughtSF || HasSpecialForces(cmd))) continue;
     if (combatLine < 2 && entry.m_iCost > budget) continue;
     if (supportTeams >= 2 && IsSpecialistTeam(entry)) continue;
     if (entry.m_bMortarTeam) continue; // bought on its own (BuyMortarTeam)
     // AI can't fly: a bought helicopter would just sit at base. Gunships come as supports instead.
     if (entry.m_bAircraft || entry.m_sVehicle.Contains("/Helicopters/") || entry.m_sVehicle.Contains("/Planes/")) continue;
     // Nor service vehicles (bought on need) or unarmed cars (nothing to fight with).
     if (ACR_AICommander.IsServiceVehicle(entry)) continue;
     if (!entry.m_sVehicle.IsEmpty() && !entry.m_bArmed) continue;
     bool isSapper = IsSapperEntry(entry);
     bool smallTeam = IsSmallTeamEntry(entry);
     if (smallTeam && !(isSapper && HasEnemyFob(cmd)) && hasLineOffer && smallTeams >= 2 && smallTeams * 2 >= lineSquads)
     {
      // Raiders run more small teams, up to two per line squad.
      if (!cmd.m_Doctrine.m_bSmallTeamsFree || (smallTeams >= 3 && smallTeams >= lineSquads * 2)) continue;
     }
     // Weigh by the base price: a unit whose stepped price has climbed isn't worth more.
     float weight = ACR_InfantryCatalog.AIWeight(entry);
     if (entry.m_sVehicle.IsEmpty() && !smallTeam && !IsSpecialistTeam(entry)) weight *= 1.8;
     else if (smallTeam && !(isSapper && HasEnemyFob(cmd))) weight *= 0.35;
     if (smallTeam) weight *= cmd.m_Doctrine.m_fSmallTeamWeight;
     if (!entry.m_sVehicle.IsEmpty()) weight *= cmd.m_Doctrine.m_fVehicleWeight;
     if (highThreat && !entry.m_sVehicle.IsEmpty()) weight *= 1.5;
     if (highThreat && cmd.m_sFaction == "RHS_AFRF" && IsRUArmorEntry(cmd, entry)) weight *= 2;
     if (!entry.m_sVehicle.IsEmpty() && vehicles == 0 && combatLine >= 3) weight *= 1.6;
     if (isSapper && HasEnemyFob(cmd))
     {
      if (!OwnsLike(cmd, entry)) weight *= 3.0;
     }
     else if (IsSpecialistTeam(entry))
     {
      if (OwnsLike(cmd, entry)) continue;
      weight *= 0.3;
      if (entry.m_bObserverTeam) weight *= cmd.m_Doctrine.m_fObserverWeight;
     }
     // Keep infantry the backbone (less so for a rich side, which buys quality: #28).
     if (!entry.m_sVehicle.IsEmpty()) weight *= Math.Min(1, 0.5 * richFactor);
     weight *= QualityWeight(entry, richFactor);
     // A low-threat war is mostly fought on foot.
     if (!entry.m_sVehicle.IsEmpty() && ACR_Manager.ThreatOption() == 0) weight *= 0.4;
     // Price-weighting favours dear units; halve those not yet affordable so saving stays the exception.
     if (entry.m_iCost > budget) weight *= 0.5;
     options.Insert(i);
     weights.Insert(weight);
     total += weight;
    }
    if (options.IsEmpty()) return;
    float roll = Math.RandomFloat(0, total);
    pick = options[options.Count() - 1];
    foreach (int k, float w : weights)
    {
     roll -= w;
     if (roll <= 0) { pick = options[k]; break; }
    }
   }
   ACR_RequisitionEntry chosen = data.m_aRequisitions[pick];
   if (chosen.m_iCost > cmd.Purse(data) - keep)
   {
    cmd.m_iSaveForEntry = pick;
    if (cmd.m_iLastSavedLog != pick)
    {
     cmd.m_iLastSavedLog = pick;
     cmd.Say(string.Format("[ACR] AI %1 saving RP for %2 (%3/%4 RP)", cmd.m_sFaction, chosen.m_sName, Math.Max(0, cmd.Purse(data) - keep), chosen.m_iCost));
    }
    return;
   }
   int stockBefore = chosen.m_iRemaining;
   string result = cmd.m_Manager.PurchaseAt(cmd.m_sFaction, pick, spawn);
   if (chosen.m_eFormation == ACR_EFormation.SF && chosen.m_iRemaining < stockBefore) boughtSF = true;
   cmd.Say(string.Format("[ACR] AI %1 requisition: %2", cmd.m_sFaction, result));
   if (!chosen.m_sVehicle.IsEmpty()) vehicles++;
   else if (IsSmallTeamEntry(chosen)) smallTeams++;
   else lineSquads++;
   if (cmd.m_Doctrine.m_bSmallTeamsFree && IsSmallTeamEntry(chosen)) capUsed += 0.5;
   else capUsed += 1;
  }
 }

 // Quality over quantity (#28 step 3): a rich side (factor over 1) weighs units by their base price
 // once more, scaled to the factor: at the RP cap a 76 RP APC weighs three times, a 12 RP team half.
 static float QualityWeight(ACR_RequisitionEntry entry, float factor)
 {
  if (factor <= 1) return 1;
  float price = entry.m_iBaseCost;
  if (price <= 0) price = entry.m_iCost;
  if (price <= 0) return 1;
  return Math.Pow(price / 25.0, factor - 1);
 }

 // An unarmed car a team can ride in: not a truck, carrier, aircraft, armour or service vehicle, and
 // no group of its own aboard. Catalog and content-socket entries alike (neither flags jeeps).
 static bool IsLightRide(ACR_RequisitionEntry entry)
 {
  if (!entry || entry.m_sVehicle.IsEmpty() || !entry.m_sGroup.IsEmpty() || !entry.m_sPassengers.IsEmpty()) return false;
  if (entry.m_bArmed || entry.m_bTransport || entry.m_bCarrier || entry.m_bSquadCarrier || entry.m_bAircraft || entry.m_bRare) return false;
  if (entry.m_bMortarTeam || entry.m_bObserverTeam || entry.m_iSupportHeli != 0) return false;
  if (entry.m_sVehicle.Contains("/Helicopters/") || entry.m_sVehicle.Contains("/Planes/") || entry.m_sVehicle.Contains("/Tracked/")) return false;
  return !ACR_AICommander.IsServiceVehicle(entry);
 }

 // Our crewed light rides: a light-ride prefab (whoever sits in it, so one that spawns with a man in
 // a hatch can't be bought again and again), or any other unarmed taxi with under 4 passenger seats.
 static int CountLightRides(ACR_AICommander cmd, ACR_FactionData data)
 {
  array<ResourceName> prefabs = {};
  foreach (ACR_RequisitionEntry entry : data.m_aRequisitions)
   if (IsLightRide(entry)) prefabs.Insert(entry.m_sVehicle);
  int count;
  foreach (ACR_Squad squad : cmd.m_Manager.m_Squads)
  {
   if (ACR_Rides.FactionOf(squad) != cmd.m_sFaction) continue;
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   if (!vehicle) continue;
   if (vehicle.GetPrefabData() && prefabs.Contains(vehicle.GetPrefabData().GetPrefabName())) { count++; continue; }
   if (squad.HasGunner(vehicle) || ACR_Logistics.ServiceOf(vehicle) != 0) continue;
   if (!CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent))) continue;
   if (ACR_Rides.CargoSeats(vehicle) < 4) count++;
  }
  return count;
 }

 // The cheapest light ride on offer, when the side has fewer than its share. urgent (short of
 // troops): only if the purse still buys the cheapest squad on foot afterwards, so a jeep never
 // takes the RP the line needs. True: bought one.
 static bool BuyLightRide(ACR_AICommander cmd, ACR_FactionData data, ACR_Base spawn, bool urgent)
 {
  int maxLight = ACR_AICommander.MAX_LIGHT_RIDES;
  if (ACR_Manager.ThreatOption() == 2) maxLight++;
  int have = CountLightRides(cmd, data);
  if (have >= maxLight) return false;
  int purse = cmd.Purse(data);
  int pick = -1;
  int cheapestFoot = -1;
  foreach (int e, ACR_RequisitionEntry offer : data.m_aRequisitions)
  {
   if (!ACR_Requisition.Available(data, offer)) continue;
   if (offer.m_sVehicle.IsEmpty() && !offer.m_sGroup.IsEmpty() && !offer.m_bMortarTeam && !offer.m_bObserverTeam && !IsSpecialistTeam(offer))
   {
    if (cheapestFoot < 0 || offer.m_iCost < cheapestFoot) cheapestFoot = offer.m_iCost;
    continue;
   }
   if (!IsLightRide(offer) || offer.m_iCost > purse) continue;
   if (pick < 0 || offer.m_iCost < data.m_aRequisitions[pick].m_iCost) pick = e;
  }
  if (pick < 0) return false;
  if (urgent && cheapestFoot > 0 && purse - data.m_aRequisitions[pick].m_iCost < cheapestFoot) return false;
  cmd.Say(string.Format("[ACR] AI %1 requisition (light ride, %2 of %3): %4", cmd.m_sFaction, have + 1, maxLight, cmd.m_Manager.PurchaseAt(cmd.m_sFaction, pick, spawn)));
  cmd.m_Manager.m_Rides.m_NoLightAt.Remove(cmd.m_sFaction);
  return true;
 }

 static float CapSlots(ACR_AICommander cmd, ACR_Squad squad)
 {
  if (squad.m_bLeftovers) return 0; // men a player dismissed from other squads: not a unit it bought
  if (cmd.m_Doctrine.m_bSmallTeamsFree && !ACR_Transport.CrewedVehicle(squad) && FullSizeOf(cmd, squad) <= 3) return 0.5;
  return 1;
 }

 static void RequisitionForTest(ACR_AICommander cmd, int needs) { cmd.m_iNeeds = needs; Requisition(cmd); }

 static bool IsRUArmorEntry(ACR_AICommander cmd, ACR_RequisitionEntry entry)
 {
  if (!entry || entry.m_sVehicle.IsEmpty()) return false;
  string vehicle = FilePath.StripExtension(FilePath.StripPath(entry.m_sVehicle));
  vehicle.ToLower();
  return vehicle.Contains("btr70") || vehicle.Contains("brdm2");
 }

 static bool IsSapperEntry(ACR_RequisitionEntry entry)
 {
  if (!entry) return false;
  if (entry.m_eFormation != ACR_EFormation.NONE) return entry.m_eFormation == ACR_EFormation.ENGINEER;
  string name = entry.m_sName;
  name.ToLower();
  if (name.Contains("sapper") || name.Contains("engineer")) return true;
  string grp = entry.m_sGroup;
  grp.ToLower();
  if (grp.Contains("sapper") || grp.Contains("engineer")) return true;
  return false;
 }

 static bool HasEnemyFob(ACR_AICommander cmd)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner == cmd.m_sFaction || base.m_sOwner.IsEmpty()) continue;
   if (base.m_bBuilt && cmd.Visible(base)) return true;
  }
  return false;
 }

 static bool IsSpecialistSquad(ACR_Squad squad)
 {
  if (!squad || squad.IsMounted() || ACR_Transport.CrewedVehicle(squad) || squad.m_bMortar || squad.Aircraft()) return false;
  if (squad.m_Group && squad.m_Group.m_eACRFormation != ACR_EFormation.NONE) return ACR_InfantryCatalog.Specialist(squad.m_Group.m_eACRFormation);
  string name = squad.GetName();
  name.ToLower();
  array<string> words = {"engineer", "sapper", "medic", "logistic", "supply", "recon", "sniper", "hq", "radio", "observer", "scout", "spotter"};
  foreach (string word : words)
   if (name.Contains(word)) return true;
  return false;
 }

 static bool IsSpecialistTeam(ACR_RequisitionEntry entry)
 {
  if (!entry) return false;
  if (entry.m_eFormation != ACR_EFormation.NONE) return ACR_InfantryCatalog.Specialist(entry.m_eFormation);
  if (entry.m_bObserverTeam) return true;
  string name = entry.m_sName;
  name.ToLower();
  array<string> words = {"engineer", "sapper", "medic", "logistic", "supply", "recon", "sniper", "hq", "radio", "observer", "scout", "spotter"};
  foreach (string word : words)
   if (name.Contains(word)) return true;
  return false;
 }

 static bool HasSpecialForces(ACR_AICommander cmd)
 {
  // Count all live SF formations, including player-held or currently airlifted
  // ones; those exclusions belong to order assignment, not purchase scarcity.
  foreach (ACR_Squad squad : cmd.m_Manager.m_Squads)
   if (squad && squad.m_Group && squad.GetFaction() && squad.GetFaction().GetFactionKey() == cmd.m_sFaction && squad.CountAlive() > 0 && squad.m_Group.m_eACRFormation == ACR_EFormation.SF) return true;
  return false;
 }

 // Already have a unit bought from this entry (names are numbered per purchase).
 static bool OwnsLike(ACR_AICommander cmd, ACR_RequisitionEntry entry)
 {
  array<ACR_Squad> mine = {};
  cmd.MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
   if (squad.GetName().StartsWith(entry.m_sName)) return true;
  return false;
 }
}
