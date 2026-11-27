// AI commander attack planning, launching, reinforcement, and assault update loop; extracted from ACR_AICommander.c.
class ACR_AIAttackPlanner
{
 // The strength of the free squads that can get to 'target' (Reach), each taxi counted once.
 static int ReachableStrength(ACR_AICommander cmd, array<ACR_Squad> free, vector target, float ring = 0)
 {
  int strength;
  array<ACR_Squad> taxis = {};
  bool liftTaken = false;
  foreach (ACR_Squad squad : free)
  {
   float seconds;
   int how = cmd.Reach(squad, target, taxis, liftTaken, seconds, ring);
   if (how == ACR_AICommander.REACH_NONE) continue;
   if (how == ACR_AICommander.REACH_AIR_LIFT) liftTaken = true;
   strength += squad.Strength();
  }
  return strength;
 }

 // How long an attack's forming up waits: STAGING_TIMEOUT, or its slowest squad's way there and
 // a minute, at most STAGE_TRAVEL_MAX.
 static float StageSeconds(float travel)
 {
  return Math.Max(ACR_AICommander.STAGING_TIMEOUT, Math.Min(ACR_AICommander.STAGE_TRAVEL_MAX, travel + 60));
 }

 static int ReachForTest(ACR_AICommander cmd, ACR_Squad squad, vector target, notnull array<ACR_Squad> taxis, bool liftTaken, out float seconds) { return cmd.Reach(squad, target, taxis, liftTaken, seconds); }

 static int ReachableStrengthForTest(ACR_AICommander cmd, array<ACR_Squad> free, vector target) { return ReachableStrength(cmd, free, target); }

 static bool IsSupportTeam(ACR_Squad squad)
 {
  if (!squad || squad.FirePower() == 0) return true;
  if (squad.m_Group && squad.m_Group.m_eACRFormation != ACR_EFormation.NONE) return ACR_InfantryCatalog.Specialist(squad.m_Group.m_eACRFormation);
  string name = squad.GetName();
  name.ToLower();
  array<string> words = {"observer", "recon", "scout", "spotter", "medic", "sniper", "sapper", "engineer", "logistic", "supply", "radio", "hq"};
  foreach (string word : words)
   if (name.Contains(word)) return true;
  return false;
 }

 static float NearestFreeDistance(ACR_AICommander cmd, array<ACR_Squad> free, vector point)
 {
  float best = float.MAX;
  foreach (ACR_Squad squad : free)
   best = Math.Min(best, vector.DistanceXZ(cmd.PositionOf(squad), point));
  return best;
 }

 // Strength an attack on a held base wants (it is reinforced up to 1.25x this).
 static float AttackNeed(ACR_AICommander cmd, ACR_Base target)
 {
  if (target.m_bBuilt) return Math.Max(8, cmd.DefenceOf(target) * 0.9 + target.m_iGarrisonLeft * 0.7);
  return Math.Max(10, cmd.DefenceOf(target) * ACR_AICommander.ATTACK_RATIO * cmd.m_Doctrine.m_fAttackRatio * 0.65) + target.m_iGarrisonLeft;
 }

 static void ReinforceAttacks(ACR_AICommander cmd, array<ACR_Squad> free)
 {
  if (free.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_AIAttack attack : cmd.m_Attacks)
  {
   if (free.IsEmpty()) break;
   ACR_Base target = attack.m_Target;
   if (!target || attack.m_bGrab || target.m_sOwner == cmd.m_sFaction || target.m_bRetired) continue;
   if (attack.m_SapperDemo && attack.m_SapperDemo.InProgress()) continue;
   int strength;
   foreach (ACR_Squad member : attack.m_Squads) strength += member.FieldStrength();
   float need = AttackNeed(cmd, target);
   if (cmd.InLaneOrFlank(target)) need *= ACR_AICommander.LANE_MASS; // the main effort is filled up further
   if (attack.m_bAssaulting && attack.m_fAssaultStart > 0 && now - attack.m_fAssaultStart > 90000) need *= 1.35;
   if (need * 1.15 <= strength) continue; // float first: an int on the left compares in ints
   cmd.SortByDistance(free, target.GetOrigin());
   int k = 0;
   while (k < free.Count() && need * 1.25 > strength)
   {
    ACR_Squad squad = free[k];
    float dist = vector.DistanceXZ(cmd.PositionOf(squad), target.GetOrigin());
    float maxDist = ACR_AICommander.REINFORCE_ATTACK_DISTANCE;
    if (squad.IsMounted()) maxDist = ACR_AICommander.REINFORCE_ATTACK_MOUNTED;
    // Once the assault is going in, only squads close enough to arrive with it join: no
    // waves walking in one after another (Jacob 20 Oct, Etoupe).
    if (attack.m_bAssaulting) maxDist = Math.Min(maxDist, target.m_fRadius + ACR_AICommander.STAGING_DISTANCE + ACR_AICommander.ASSAULT_JOIN_EXTRA);
    if (dist > ACR_AICommander.REINFORCE_ATTACK_MOUNTED) break;
    // Observers, medics, snipers and other support teams don't join an assault.
    if (dist > maxDist || IsSupportTeam(squad)) { k++; continue; }
    vector stage = JoinAttack(cmd, attack, squad, now);
    strength += squad.FieldStrength();
    if (strength > attack.m_iStartStrength) attack.m_iStartStrength = strength;
    free.RemoveOrdered(k);
    cmd.Say(string.Format("[ACR] AI %1: %2 reinforces the attack on %3 (%4/%5), staging at grid %6", cmd.m_sFaction, squad.GetName(), target.m_sName, strength, (int)need, ACR_Manager.Grid(stage)));
   }
  }
 }

 // Adds a squad to an attack already under way (ReinforceAttacks, the allocator): its approach line
 // and staging point, the forming up waiting for it (within limits), its orders. Returns the stage.
 static vector JoinAttack(ACR_AICommander cmd, ACR_AIAttack attack, ACR_Squad squad, float now)
 {
  ACR_Base target = attack.m_Target;
  vector stage;
  int slot = cmd.PickApproach(attack, cmd.PositionOf(squad), stage);
  attack.Add(squad, stage, slot);
  // Still forming up: the assault waits for it (within limits) so they go in together.
  if (!attack.m_bAssaulting)
  {
   float speed = 2;
   if (squad.IsMounted()) speed = 8;
   float arrive = now + (vector.DistanceXZ(cmd.PositionOf(squad), stage) / speed + 30) * 1000;
   attack.m_fStageDeadline = Math.Max(attack.m_fStageDeadline, Math.Min(arrive, attack.m_fStageLimit));
  }
  squad.SetAITask(ACR_EAITask.ATTACK);
  squad.m_AITarget = target;
  if (attack.m_bAssaulting)
  {
   squad.SetMode(ACR_EMoveMode.COMBAT);
   squad.Assault(target.GetOrigin());
  }
  else
  {
   squad.SetMode(ACR_EMoveMode.FAST);
   cmd.Travel(squad, stage, false, 0, target);
   // A covered way round is longer: the forming up waits for it (within limits).
   if (cmd.m_fCoverDetour > 0)
   {
    float covered = now + ((vector.DistanceXZ(cmd.PositionOf(squad), stage) + cmd.m_fCoverDetour) / ACR_AICommander.COVER_WALK_SPEED + 30) * 1000;
    attack.m_fStageDeadline = Math.Max(attack.m_fStageDeadline, Math.Min(covered, attack.m_fStageLimit));
   }
  }
  return stage;
 }

 static bool PlanBudgetSpent(ACR_AICommander cmd)
 {
  return System.GetTickCount() - cmd.m_iTickStarted > ACR_AICommander.PLAN_BUDGET_MS;
 }

 static void AssignAttacks(ACR_AICommander cmd)
 {
  ACR_FactionSettings settings = cmd.Settings();
  int maxAttacks = 1;
  if (settings) maxAttacks = settings.m_iAIAttacks;
  bool counterDue = cmd.m_CounterTarget && cmd.m_CounterTarget.m_sOwner != cmd.m_sFaction && GetGame().GetWorld().GetWorldTime() < cmd.m_fCounterUntil && !cmd.IsAttacking(cmd.m_CounterTarget);
  array<ACR_Squad> free = {};
  cmd.MySquads(free, true);
  cmd.DropAmmoShort(free); // shot nearly dry: it rearms first (ManageAmmoRuns), or it would fall back at once
  // Keep a reserve once the force is big enough, at a secure base behind the front (not the
  // HQ unless nothing else fits: Jacob 14 Oct, reserves at the HQ took too long to respond).
  array<ACR_Squad> all = {};
  cmd.MySquads(all, false);
  int minAllForReserve = 5;
  if (ACR_Manager.ThreatOption() == 0) minAllForReserve = 6;
  else if (ACR_Manager.ThreatOption() == 2) minAllForReserve = 4;
  minAllForReserve = (int)Math.Max(3, minAllForReserve + cmd.m_Doctrine.m_iReserveSquads);
  if (free.Count() >= 2 && (cmd.KeepsReserve() || (all.Count() >= minAllForReserve && cmd.Pressure() >= 0.9)))
  {
   ACR_Base home = cmd.ReserveBase();
   // A reserve still on its way there stays the reserve (committed, it isn't in 'free' meanwhile).
   if (home && !cmd.CommittedTo(cmd.m_ReserveSquad, home.GetOrigin(), home.m_fRadius + 150))
   {
    cmd.SortByDistance(free, home.GetOrigin());
    ACR_Squad reserve;
    if (cmd.m_ReserveSquad && free.Contains(cmd.m_ReserveSquad) && vector.DistanceXZ(cmd.PositionOf(cmd.m_ReserveSquad), home.GetOrigin()) <= ACR_AICommander.RESERVE_MAX_PULL && !IsSupportTeam(cmd.m_ReserveSquad))
     reserve = cmd.m_ReserveSquad;
    if (!reserve)
    {
     foreach (ACR_Squad candidateReserve : free)
     {
      if (vector.DistanceXZ(cmd.PositionOf(candidateReserve), home.GetOrigin()) > ACR_AICommander.RESERVE_MAX_PULL) break;
      if (!IsSupportTeam(candidateReserve)) { reserve = candidateReserve; break; }
     }
    }
    if (!reserve && vector.DistanceXZ(cmd.PositionOf(free[0]), home.GetOrigin()) <= ACR_AICommander.RESERVE_MAX_PULL)
     reserve = free[0];
    if (reserve)
    {
     cmd.m_ReserveSquad = reserve;
     free.RemoveItem(reserve);
     bool enRoute = reserve.m_iAITask == ACR_EAITask.POSTED && (ACR_AirLift.Busy(reserve) || ACR_AICommander.ReserveMoving(reserve.OrderState()) || (reserve.m_Group && reserve.m_Group.GetCurrentWaypoint() && vector.DistanceXZ(reserve.m_MoveTarget, home.GetOrigin()) <= home.m_fRadius + 150));
     if (reserve.m_iAITask != ACR_EAITask.POSTED || (!enRoute && vector.DistanceXZ(cmd.PositionOf(reserve), home.GetOrigin()) > 150))
      cmd.HoldReserve(reserve, home);
    }
   }
  }
  if (free.IsEmpty()) return;

  // Empty bases: several quick grabs at once, each by whichever free squad gets
  // there first (vehicles count as three times faster than walking).
  int grabs = 0;
  int attacks = 0;
  // With a lane (ACR_AILanes): grabs in it and on its flanks, and one at a time close behind our own.
  bool laneActive = cmd.LaneActive();
  int rearGrabs = 0;
  foreach (ACR_AIAttack running : cmd.m_Attacks)
  {
   if (running.m_bGrab) grabs++;
   else attacks++;
   if (laneActive && running.m_bGrab && !cmd.InLaneOrFlank(running.m_Target)) rearGrabs++;
  }
  // Big maps (Everon soak 5 Oct: 3 grabs at once left each side with 7 of 56 bases after
  // 18 min): one grab per 6 empty bases in sight, 3 to 8.
  int unclaimed;
  foreach (ACR_Base open : ACR_Base.s_All) if (cmd.SeenOwner(open).IsEmpty() && cmd.Visible(open)) unclaimed++;
  int maxGrabs = Math.Clamp(unclaimed / 6, Math.Max(1, ACR_AICommander.MAX_GRABS + cmd.m_Doctrine.m_iGrabs), Math.Max(1, ACR_AICommander.MAX_GRABS_BIG + cmd.m_Doctrine.m_iGrabs));
  while (grabs < maxGrabs && !free.IsEmpty() && !PlanBudgetSpent(cmd))
  {
   ACR_Base empty;
   ACR_Squad runner;
   float bestTime = float.MAX;
   bool bestByTruck;
   foreach (ACR_Base candidate : ACR_Base.s_All)
   {
    if (!cmd.SeenOwner(candidate).IsEmpty() || !cmd.Visible(candidate) || cmd.IsAttacking(candidate) || cmd.CoolingDown(candidate)) continue;
    if (laneActive && !cmd.InLaneOrFlank(candidate) && !cmd.OffLaneGrabAllowed(candidate, rearGrabs)) continue;
    float incomeBonus = cmd.IncomeGrabBonus(candidate);
    foreach (ACR_Squad squad : free)
    {
     // A medic or other support team stays out of it, as out of attacks (#34).
     if (IsSupportTeam(squad)) continue;
     if (cmd.GrabBarred(squad, candidate)) continue; // stalled there lately
     // On foot within GRAB_WALK, in its own vehicle anywhere, farther by a free truck (GrabTime).
     bool byTruck;
     float time = cmd.GrabTimeOf(squad, candidate.GetOrigin(), bestTime * incomeBonus, byTruck);
     if (time < 0) continue;
     time = time / incomeBonus; // income first while short of it
     if (time < bestTime) { bestTime = time; empty = candidate; runner = squad; bestByTruck = byTruck; }
    }
   }
   if (!empty) break;
   cmd.StartGrab(runner, empty);
   free.RemoveItem(runner);
   grabs++;
   if (laneActive && !cmd.InLaneOrFlank(empty)) rearGrabs++;
   string how = "";
   if (bestByTruck) how = ", by truck";
   cmd.Say(string.Format("[ACR] AI %1: %2 grabs empty %3 (about %4 s away%5)", cmd.m_sFaction, runner.GetName(), empty.m_sName, (int)bestTime, how));
   cmd.TwoICReport(string.Format("Taking %1 with %2 (about %3 s away)", empty.m_sName, runner.GetName(), (int)bestTime));
  }
  // A small 2IC force (#14 v1) only grabs empty bases.
  if (cmd.SmallForce())
  {
   float smallNow = GetGame().GetWorld().GetWorldTime();
   if (smallNow >= cmd.m_fNextSmallLog)
   {
    cmd.m_fNextSmallLog = smallNow + 60000;
    PrintFormat("[ACR] 2IC %1: a small force (%2 squads): grabs of empty bases only", cmd.m_sFaction, ACR_SquadAI.CountFor(cmd.m_sFaction));
   }
   return;
  }
  if (PlanBudgetSpent(cmd)) return;
  ReinforceAttacks(cmd, free);
  if (free.IsEmpty() || PlanBudgetSpent(cmd)) return;
  // Plenty of idle troops: open more fronts. On large maps with many bases and idle armies,
  // allow up to 6 extra attacks so a dominant side doesn't sit idle.
  int maxExtraAttacks = cmd.m_Doctrine.m_iExtraFronts;
  if (ACR_Base.s_All.Count() > 24) maxExtraAttacks = maxExtraAttacks * 2;
  // A lane concentrates the force: one main attack and the doctrine's supporting ones, the rest of
  // the idle troops filling them up and posted forward in the lane (PostIdle).
  if (laneActive) maxExtraAttacks = cmd.m_Doctrine.m_iLaneFronts;
  maxAttacks += Math.Min(maxExtraAttacks, free.Count() / cmd.m_Doctrine.m_iSquadsPerFront);
  // "Take this base" (a player's request to the 2IC, #14 v1): the next attack goes for it.
  ACR_Base requested = cmd.RequestedTarget();
  if (requested && cmd.IsAttacking(requested)) requested = null;
  if (attacks >= maxAttacks && !counterDue && !requested) return;

  // Target: a counterattack on a base just lost, otherwise the enemy base with
  // the lowest (defence x ratio) scaled by distance -- the best one we can
  // actually muster enough for, rather than waiting forever on one we can't.
  float pressure = cmd.Pressure();
  int available;
  foreach (ACR_Squad counted : free) available += counted.Strength();
  ACR_Base target;
  ACR_Base wanted;
  float required = 1;
  float wantedRequired;
  int wantedReach;
  vector from = cmd.PositionOf(free[0]);
  array<ACR_Base> candidates = {};
  array<float> scores = {};
  // A counterattack or a player's "take this base" goes first, but no longer alone: when the AI
  // couldn't muster enough for it, every other attack waited, and each base it lost restarted
  // the counter window (taking a cheap rear base every few minutes stalled the AI; audit 31 Oct).
  // Over the attack cap only the priority target is open, as before.
  ACR_Base priority = null;
  if (counterDue) priority = cmd.m_CounterTarget;
  else if (requested) priority = requested;
  // Every other candidate passes HostileOwner: a counter window or request on a base now held by
  // the truce partner sent a full attack that could neither fire nor capture.
  if (priority && ACR_Insurgency.Friendly(priority.m_sOwner, cmd.m_sFaction))
  {
   priority = null;
   if (attacks >= maxAttacks) return; // it was the only target open over the cap
  }
  if (priority)
  {
   candidates.Insert(priority);
   scores.Insert(-1);
  }
  bool laneOpen = cmd.LaneOpenToOthers();
  if (!priority || attacks < maxAttacks)
  {
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base == priority) continue;
    if (base.m_sOwner == cmd.m_sFaction || !cmd.HostileOwner(base) || !cmd.Visible(base) || cmd.IsAttacking(base) || cmd.CoolingDown(base)) continue;
    if (!cmd.HqTargetable(base)) continue;
    float cost = Math.Max(10, cmd.DefenceOf(base) * ACR_AICommander.ATTACK_RATIO * cmd.m_Doctrine.m_fAttackRatio);
    float dist = NearestFreeDistance(cmd, free, base.GetOrigin());
    // FIA pockets: cleared when in the way (near our own bases), after the real enemy.
    if (base.m_bPocket && cmd.NearestOwnDistance(base.GetOrigin()) > ACR_AICommander.POCKET_IN_THE_WAY) continue;
    // The lane: in it sooner (income first), its flanks a little later, elsewhere not while it has work.
    float laneFactor = cmd.LaneTargetFactor(base);
    if (laneFactor < 0 && !laneOpen) continue;
    float score = cost * (dist / 1000 + 0.1);
    if (laneFactor > 0) score *= laneFactor;
    score /= cmd.EnvelopBonus(base) * cmd.IncomeGrabBonus(base); // going round a strongpoint, cheap income (ACR_AIEnvelopment, ACR_AIIncome)
    if (base.m_bPocket) score *= cmd.m_Doctrine.m_fPocketScore;
    if (base.m_bContested || (cmd.DefenceOf(base) <= ACR_AICommander.UNKNOWN_DEFENCE + 2 && dist < 1200))
     score *= 0.65 * cmd.m_Doctrine.m_fWeakTarget;
    if (base.m_bBuilt)
    {
     score *= 0.35 * cmd.m_Doctrine.m_fFobTarget;
     float frontDist = float.MAX;
     foreach (ACR_Base ownBase : ACR_Base.s_All)
     {
      if (ownBase && !ownBase.m_bRetired && ownBase.m_sOwner == cmd.m_sFaction)
      {
       float d = vector.DistanceXZ(base.GetOrigin(), ownBase.GetOrigin());
       if (d < frontDist) frontDist = d;
      }
     }
     if (frontDist < 1200) score *= 0.6;
    }
    int at = 0;
    while (at < scores.Count() && scores[at] <= score) at++;
    candidates.InsertAt(base, at);
    scores.InsertAt(score, at);
   }
  }
  foreach (ACR_Base candidate : candidates)
  {
   if (candidate != priority && PlanBudgetSpent(cmd)) break; // reach checks are the dear part
   float need = 1;
   if (!candidate.m_sOwner.IsEmpty()) need = AttackNeed(cmd, candidate);
   if (counterDue && candidate == cmd.m_CounterTarget) need = need * cmd.m_Doctrine.m_fCounterNeed; // strike before they dig in
   need = need * pressure;
   if (ACR_Manager.ThreatOption() == 2) need = need * 0.85; // High: launch a viable force sooner
   // Only the squads that can get there count (#27): a target beyond their reach is skipped.
   int reachable = ReachableStrength(cmd, free, candidate.GetOrigin(), ACR_AICommander.AttackRing(candidate));
   if (!wanted) { wanted = candidate; wantedRequired = need; wantedReach = reachable; }
   if (need <= reachable) { target = candidate; required = need; break; }
  }
  // Waiting on the lane: nothing in it it could muster for counts toward a starved lane.
  if (laneActive && target && cmd.InLaneOrFlank(target)) cmd.NoteLaneMuster(true);
  else if (laneActive && !target && wanted && cmd.InLaneOrFlank(wanted)) cmd.NoteLaneMuster(false);
  if (!target)
  {
   float now = GetGame().GetWorld().GetWorldTime();
   if (wanted && now >= cmd.m_fNextWeakLog)
   {
    cmd.m_fNextWeakLog = now + 60000;
    cmd.Say(string.Format("[ACR] AI %1: not strong enough for any target (best: %2, %3/%4 in reach of %5 free, pressure x%6)", cmd.m_sFaction, wanted.m_sName, wantedReach, (int)wantedRequired, available, pressure));
   }
   return; // Requisition() will add troops
  }
  cmd.SortByDistance(free, target.GetOrigin());
  from = cmd.PositionOf(free[0]);
  if (target.m_bBuilt)
  {
   for (int s = free.Count() - 1; s >= 0; s--)
   {
    if (ACR_SapperDemo.IsSapperSquad(free[s]))
    {
     ACR_Squad sapperSquad = free[s];
     free.RemoveOrdered(s);
     free.InsertAt(sapperSquad, 0);
     break;
    }
   }
  }
  ACR_AIAttack attack = NewAttack(cmd, target, counterDue && target == cmd.m_CounterTarget);
  int strength;
  // With troops to spare, send a margin above the bare minimum.
  float margin = 1;
  if (free.Count() > 4) margin = 1 + Math.Min(0.5, (free.Count() - 4) * 0.05);
  // Commit the force together (Jacob 20 Oct, Etoupe: two squads went in and six followed in
  // waves): past the bare minimum, every fighting squad in reach joins up to the strength the
  // attack would otherwise be reinforced to.
  float goal = Math.Max(required * margin, AttackNeed(cmd, target) * 1.15);
  if (target.m_sOwner.IsEmpty()) goal = required * margin;
  // The main effort goes in heavy: an attack in the lane fills up further.
  else if (cmd.InLaneOrFlank(target)) goal = goal * ACR_AICommander.LANE_MASS;
  int remainingPool = available;
  array<ACR_Squad> taxis = {};
  bool liftTaken = false;
  foreach (ACR_Squad squad : free)
  {
   int sqStrength = squad.Strength();
   remainingPool -= sqStrength;
   // Recon teams watch from overwatch; they never go in, so they don't make up an attack.
   if (!target.m_sOwner.IsEmpty() && ACR_AICommander.IsReconTeam(squad)) continue;
   if (squad.StrengthRatio() < 0.55 && required <= strength + remainingPool) continue;
   float distToTarget = vector.DistanceXZ(cmd.PositionOf(squad), target.GetOrigin());
   if (required * margin <= strength)
   {
    if (goal <= strength) break;
    float reach = ACR_AICommander.REINFORCE_ATTACK_DISTANCE;
    if (squad.IsMounted()) reach = ACR_AICommander.REINFORCE_ATTACK_MOUNTED;
    if (distToTarget > reach || IsSupportTeam(squad)) continue;
   }
   // A squad that can't get there stays out (no taxi to be had, no air lift); one that can sets
   // how long the forming up waits.
   float travel;
   int how = cmd.Reach(squad, target.GetOrigin(), taxis, liftTaken, travel, ACR_AICommander.AttackRing(target));
   if (how == ACR_AICommander.REACH_NONE) continue;
   if (how == ACR_AICommander.REACH_AIR_LIFT) liftTaken = true;
   attack.m_fTravelSeconds = Math.Max(attack.m_fTravelSeconds, travel);
   attack.m_Squads.Insert(squad);
   strength += sqStrength;
  }
  if (required > strength)
  {
   cmd.Say(string.Format("[ACR] AI %1: not strong enough for %2 (%3/%4)", cmd.m_sFaction, target.m_sName, strength, required));
   return; // Requisition() will add troops
  }
  if (!target.m_sOwner.IsEmpty() && ACR_AICommander.TakersIn(attack) == 0)
  {
   float noTakersAt = GetGame().GetWorld().GetWorldTime();
   if (noTakersAt >= cmd.m_fNextWeakLog)
   {
    cmd.m_fNextWeakLog = noTakersAt + 60000;
    cmd.Say(string.Format("[ACR] AI %1: not attacking %2: no infantry in reach to take it (only crews, recon or support teams)", cmd.m_sFaction, target.m_sName));
   }
   return;
  }
  LaunchAttack(cmd, attack, from, strength, required, pressure);
 }

 // A new attack on 'target' (AssignAttacks, the allocator); a built FOB is a sapper raid.
 static ACR_AIAttack NewAttack(ACR_AICommander cmd, ACR_Base target, bool counter)
 {
  ACR_AIAttack attack = new ACR_AIAttack();
  attack.m_Target = target;
  attack.m_bCounter = counter;
  attack.m_Bypass = cmd.EnvelopOf(target); // going round it: approaches keep out of its sight (LaunchAttack)
  if (target.m_bBuilt)
  {
   attack.m_bFobRaid = true;
   attack.m_SapperDemo = new ACR_SapperDemo(cmd.m_Manager, cmd, attack);
   cmd.Say(string.Format("[ACR] AI %1: targeted enemy forward base %2 for sapper demolition", cmd.m_sFaction, target.m_sName));
  }
  return attack;
 }

 // Sends the chosen squads off: staging points (pincers), base of fire, orders.
 static void LaunchAttack(ACR_AICommander cmd, ACR_AIAttack attack, vector from, int strength, float required, float pressure, bool dropWalkers = true)
 {
  ACR_Base target = attack.m_Target;
  // What it sets out with, as UpdateAttacks counts it later (FieldStrength): a squad leaving a
  // roadblock's gun for this attack would otherwise count as 5 lost at once.
  int fielded;
  foreach (ACR_Squad starter : attack.m_Squads) if (starter) fielded += starter.FieldStrength();
  attack.m_iStartStrength = fielded;
  attack.m_fLaunchedAt = GetGame().GetWorld().GetWorldTime();
  // Staging: the main approach from the attackers' side; with several squads
  // they spread out and come in from different angles (pincers).
  vector dir = from - target.GetOrigin();
  dir[1] = 0;
  dir.Normalize();
  float ring = target.m_fRadius + ACR_AICommander.STAGING_DISTANCE;
  // Going round a strongpoint: the main approach turns away from it, out of its sight and fire.
  if (attack.m_Bypass && attack.m_Bypass.m_sOwner != cmd.m_sFaction) dir = ACR_AIEnvelopment.ApproachAway(target.GetOrigin(), from, attack.m_Bypass.GetOrigin(), ring);
  attack.m_vApproach = dir;
  attack.m_Staging = ACR_Manager.Ground(target.GetOrigin() + dir * ring);
  // Staging points on dry land with a dry run in (no forming up in a bay).
  vector dryMain;
  if (cmd.StagingPoint(target, dir, ring, dryMain)) attack.m_Staging = dryMain;
  attack.m_aStaging.Clear();
  attack.m_aSlot.Clear();
  // Assign foot squads first so they take the direct lines (0, +-60) and mounted units swing wide (+-115).
  for (int pass = 0; pass < 2; pass++)
  {
   foreach (int idx, ACR_Squad member : attack.m_Squads)
   {
    bool mounted = member.IsMounted() || ACR_Transport.CrewedVehicle(member) != null;
    if ((pass == 0 && mounted) || (pass == 1 && !mounted)) continue;
    while (attack.m_aStaging.Count() <= idx)
    {
     attack.m_aStaging.Insert(attack.m_Staging);
     attack.m_aSlot.Insert(-1);
    }
    vector stage;
    int slot = cmd.PickApproach(attack, cmd.PositionOf(member), stage);
    attack.m_aStaging[idx] = stage;
    attack.m_aSlot[idx] = slot;
   }
  }
  bool empty = target.m_sOwner.IsEmpty();
  // With two or more squads on a held base, one becomes the base of fire
  // while the others manoeuvre.
  // Picked by firepower (a gun vehicle or the biggest rifle squad), never a support team.
  if (!empty && attack.m_Squads.Count() >= 2)
  {
   int bestFire = 2;
   foreach (ACR_Squad fireCandidate : attack.m_Squads)
   {
    if (ACR_SapperDemo.IsSapperSquad(fireCandidate) || IsSupportTeam(fireCandidate)) continue;
    int firepower = fireCandidate.FirePower();
    if (firepower > bestFire) { bestFire = firepower; attack.m_Suppressor = fireCandidate; }
   }
  }
  attack.m_fStageDeadline = GetGame().GetWorld().GetWorldTime() + StageSeconds(attack.m_fTravelSeconds) * 1000;
  attack.m_fStageLimit = attack.m_fStageDeadline + ACR_AICommander.STAGING_TIMEOUT * ACR_AICommander.STAGE_STRETCH * 1000;
  // Motorized assault: an armed carrier (APC) in the attack takes an infantry squad to the
  // staging point and stays there with it as base of fire.
  array<ACR_Squad> carried = {};
  if (!empty && cmd.m_Manager.m_Rides) PairCarriers(cmd, attack, carried);
  float plannedDeadline = attack.m_fStageDeadline;
  cmd.m_LaunchingAttack = attack;
  foreach (ACR_Squad squad : attack.m_Squads)
  {
   squad.SetAITask(ACR_EAITask.ATTACK);
   squad.m_AITarget = target;
   if (carried.Contains(squad)) continue; // the ride has its orders
   squad.SetMode(ACR_EMoveMode.FAST);
   // Nobody to fight at an empty base: go straight in.
   if (empty) cmd.Travel(squad, target.GetOrigin(), false, 100);
   else
   {
    cmd.Travel(squad, attack.StagingOf(squad), false, 0, target);
    // The travel estimate was the straight way: a covered way round adds its extra length.
    if (cmd.m_fCoverDetour > 0) attack.m_fStageDeadline = Math.Max(attack.m_fStageDeadline, Math.Min(attack.m_fStageLimit, plannedDeadline + cmd.m_fCoverDetour / ACR_AICommander.COVER_WALK_SPEED * 1000));
   }
  }
  cmd.m_LaunchingAttack = null;
  // A squad sent on foot beyond a walk (the ride or air lift its plan counted on wasn't there when
  // Travel asked) leaves the attack now, not six minutes later as "never formed up" (match 6 Nov,
  // Chernarus: Holding Squad 1 walked 6.6 km for an attack planned on a 240 s lift, twice). The
  // attack goes on without it while it keeps the strength it wanted and someone to take the base.
  // An admin's forced attack keeps everyone.
  if (!empty && dropWalkers)
  {
   for (int w = attack.m_Squads.Count() - 1; w >= 0; w--)
   {
    ACR_Squad walker = attack.m_Squads[w];
    if (!walker || carried.Contains(walker)) continue;
    float walk = vector.DistanceXZ(cmd.PositionOf(walker), attack.StagingOf(walker));
    if (!cmd.OnFootFar(walker, walk)) continue;
    cmd.Say(string.Format("[ACR] AI %1: %2 leaves the attack on %3: on foot for %4 m, no ride or air lift to be had", cmd.m_sFaction, walker.GetName(), target.m_sName, (int)walk));
    strength -= walker.Strength();
    attack.m_iStartStrength -= walker.FieldStrength();
    attack.Remove(w);
    walker.SetAITask(ACR_EAITask.FREE);
   }
   if (attack.m_Squads.IsEmpty() || required > strength || ACR_AICommander.TakersIn(attack) == 0)
   {
    float droppedAt = GetGame().GetWorld().GetWorldTime();
    cmd.Say(string.Format("[ACR] AI %1: attack on %2 called off at launch: its squads can't get there in time (%3/%4 strength left)", cmd.m_sFaction, target.m_sName, strength, (int)required));
    cmd.m_StalledUntil.Set(target, droppedAt + ACR_AICommander.ALLOC_DROP_COOLDOWN * 1000);
    foreach (ACR_Squad released : attack.m_Squads) released.SetAITask(ACR_EAITask.FREE);
    return;
   }
  }
  attack.m_bAssaulting = empty;
  if (!empty && ACR_AttackGround.On()) cmd.StartGround(attack);
  cmd.m_Attacks.Insert(attack);
  cmd.m_fLastOffensive = GetGame().GetWorld().GetWorldTime();
  if (attack.m_Bypass && attack.m_Bypass.m_sOwner != cmd.m_sFaction) cmd.Say(string.Format("[ACR] AI %1: %2 via %3", cmd.m_sFaction, cmd.EnvelopText(target), target.m_sName));
  if (pressure < 1) cmd.Say(string.Format("[ACR] AI %1: taking the initiative against %2 (accepting x%3 odds after a quiet spell)", cmd.m_sFaction, target.m_sName, pressure));
  string kind = "attacking";
  if (attack.m_bCounter) { kind = "COUNTERATTACKING"; cmd.m_CounterTarget = null; }
  cmd.Say(string.Format("[ACR] AI %1: %2 %3 with %4 squad(s), strength %5/%6", cmd.m_sFaction, kind, target.m_sName, attack.m_Squads.Count(), strength, required));
  if (attack.m_bCounter) cmd.TwoICReport(string.Format("Counterattacking %1 with %2", target.m_sName, ACR_AICommander.SquadList(attack.m_Squads)));
  else cmd.TwoICReport(string.Format("Attacking %1 with %2", target.m_sName, ACR_AICommander.SquadList(attack.m_Squads)));
  // "Take this base" (a player's request to the 2IC): its attack is out.
  if (cmd.m_RequestedTarget && target == cmd.m_RequestedTarget)
  {
   cmd.m_RequestedTarget = null;
   ACR_TwoICRequest asked = ACR_TwoICRequests.Find(cmd.m_iRequestedBy);
   if (asked) asked.Set(ACR_ETwoICState.COMING, string.Format("attacking %1 with %2", target.m_sName, ACR_AICommander.SquadList(attack.m_Squads)));
   PrintFormat("[ACR] 2IC %1: attacking %2 as asked (request %3)", cmd.m_sFaction, target.m_sName, cmd.m_iRequestedBy);
  }
  if (empty) return;
  string stagingAt;
  foreach (ACR_Squad staged : attack.m_Squads)
   stagingAt += string.Format(" %1 grid %2,", staged.GetName(), ACR_Manager.Grid(attack.StagingOf(staged)));
  cmd.Say(string.Format("[ACR] AI %1: staging for %2 at%3", cmd.m_sFaction, target.m_sName, stagingAt));
 }

 static void PairCarriers(ACR_AICommander cmd, ACR_AIAttack attack, array<ACR_Squad> carried)
 {
  foreach (int c, ACR_Squad carrier : attack.m_Squads)
  {
   IEntity vehicle = ACR_Transport.CrewedVehicle(carrier);
   if (!vehicle || carried.Contains(carrier) || !carrier.HasGunner(vehicle)) continue;
   int seats = ACR_Rides.FreeSeats(vehicle);
   if (seats < 4) continue;
   ACR_Squad best;
   float bestDistance = ACR_AICommander.CARRIER_PICKUP;
   foreach (ACR_Squad rider : attack.m_Squads)
   {
    if (rider == carrier || carried.Contains(rider) || rider.IsMounted() || ACR_Transport.CrewedVehicle(rider)) continue;
    if (rider.CountAlive() > seats) continue;
    float d = vector.DistanceXZ(cmd.PositionOf(rider), vehicle.GetOrigin());
    if (d < bestDistance) { bestDistance = d; best = rider; }
   }
   if (!best) continue;
   vector stage = attack.StagingOf(best);
   ACR_AICommander.s_iCarrierTries++;
   string why;
   if (!cmd.m_Manager.m_Rides.RequestCarrier(best, carrier, stage, ACR_Squad.WP_MOVE, -1, "Moving", 0, why))
   {
    cmd.Say(string.Format("[ACR] AI %1: %2 can't carry %3 (%4)", cmd.m_sFaction, carrier.GetName(), best.GetName(), why));
    continue;
   }
   if (c < attack.m_aStaging.Count()) attack.m_aStaging[c] = stage;
   int riderAt = attack.m_Squads.Find(best);
   if (c < attack.m_aSlot.Count() && riderAt >= 0 && riderAt < attack.m_aSlot.Count()) attack.m_aSlot[c] = attack.m_aSlot[riderAt];
   carried.Insert(best);
   carried.Insert(carrier);
   cmd.Say(string.Format("[ACR] AI %1: %2 carries %3 to the staging point for %4", cmd.m_sFaction, carrier.GetName(), best.GetName(), attack.m_Target.m_sName));
  }
 }

 // Debug menu: attack now with every free squad, ignoring odds, attack limits and
 // cooldowns. target null = the nearest enemy-held base (an empty one if none).
 static string ForceAttack(ACR_AICommander cmd, ACR_Base target)
 {
  if (!cmd.m_bActive) return string.Format("%1 AI commander isn't in command (a player commands it, or it's held or off)", cmd.m_sFaction);
  cmd.m_RetreatingUntil.Clear();
  array<ACR_Squad> free = {};
  cmd.MySquads(free, true, true);
  if (free.IsEmpty()) return string.Format("%1 AI has no free squads to attack with", cmd.m_sFaction);
  vector from = cmd.PositionOf(free[0]);
  if (!target)
  {
   float best = float.MAX;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner == cmd.m_sFaction) continue;
    float d = vector.DistanceXZ(base.GetOrigin(), from);
    if (base.m_sOwner.IsEmpty()) d += 1000000; // enemy bases first
    if (d < best) { best = d; target = base; }
   }
  }
  if (!target) return "No base to attack";
  if (target.m_sOwner == cmd.m_sFaction) return string.Format("%1 already holds %2", cmd.m_sFaction, target.m_sName);
  cmd.m_StalledUntil.Remove(target);
  cmd.SortByDistance(free, target.GetOrigin());
  ACR_AIAttack attack = new ACR_AIAttack();
  attack.m_Target = target;
  int strength;
  foreach (ACR_Squad squad : free)
  {
   attack.m_Squads.Insert(squad);
   strength += squad.Strength();
  }
  cmd.Say(string.Format("[ACR] AI %1: forced attack (debug menu)", cmd.m_sFaction));
  LaunchAttack(cmd, attack, from, strength, strength, 1, false);
  return string.Format("%1 AI attacking %2 with %3 squad(s)", cmd.m_sFaction, target.m_sName, free.Count());
 }

 static void NoteBBBFight(ACR_AICommander cmd, ACR_AIAttack attack)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  float elapsed = 0;
  if (attack.m_fBBBCheckAt > 0) elapsed = (now - attack.m_fBBBCheckAt) / 1000;
  attack.m_fBBBCheckAt = now;
  if (!attack.m_bAssaulting || attack.m_fAssaultStart <= 0 || attack.m_bGrab) return;
  bool fighting = false;
  ACR_Squad needy;
  BBB_TacticReport needyReport;
  foreach (ACR_Squad squad : attack.m_Squads)
  {
   if (!squad || !squad.m_Group) continue;
   BBB_TacticReport report = BBB_Tactic.Report(squad.m_Group);
   if (!report) continue;
   if (report.m_bGaining || report.m_bNeedsSupport) fighting = true;
   if (!needy && report.m_bNeedsSupport && report.m_fThreatAge <= ACR_AICommander.BBB_THREAT_FRESH_S && report.m_vThreat.LengthSq() > 0) { needy = squad; needyReport = report; }
  }
  // A long gap (paused game, first look) doesn't count.
  if (fighting && elapsed > 0 && elapsed < 30) attack.m_fBBBFightS = Math.Min(ACR_AICommander.BBB_FIGHT_MAX_S, attack.m_fBBBFightS + elapsed);
  if (!needy || now < attack.m_fBBBSupportAt) return;
  string why = string.Format("%1 held on %2 (%3)", needy.GetName(), attack.m_Target.m_sName, needyReport.m_sWhy);
  if (cmd.CallSupport(ACR_ESupportType.MORTAR_HE_LARGE, needyReport.m_vThreat, why) || cmd.CallSupport(ACR_ESupportType.MORTAR_HE_SMALL, needyReport.m_vThreat, why))
   attack.m_fBBBSupportAt = now + ACR_AICommander.BBB_SUPPORT_EVERY_S * 1000;
 }

 // Every couple of minutes, send attackers that stopped short of the base in again.
 static void PushAttackers(ACR_AICommander cmd, ACR_AIAttack attack)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (attack.m_bGrab || now < attack.m_fNextPush) return;
  // The others fell: the base of fire is the last squad that can take the base, so it goes in
  // instead of covering an assault that isn't coming.
  if (attack.m_Suppressor && ACR_AICommander.TakersIn(attack, attack.m_Suppressor) == 0 && ACR_AICommander.CanTakeBase(attack.m_Suppressor, attack.m_bFobRaid))
  {
   cmd.Say(string.Format("[ACR] AI %1: base of fire %2 goes in on %3 (no other squad left to take it)", cmd.m_sFaction, attack.m_Suppressor.GetName(), attack.m_Target.m_sName));
   attack.m_Suppressor = null;
   attack.m_fNextPush = 1; // re-issue below now
  }
  if (attack.m_fNextPush > 0)
  {
   foreach (ACR_Squad squad : attack.m_Squads)
   {
    if (squad == attack.m_Suppressor || squad.m_CombatFlank.Active() || ACR_AirLift.Busy(squad)) continue;
    IEntity veh = ACR_Transport.CrewedVehicle(squad);
    if (veh && squad.HasGunner(veh)) continue;
    if (vector.DistanceXZ(cmd.PositionOf(squad), attack.m_Target.GetOrigin()) < attack.m_Target.m_fRadius * 0.6) continue;
    squad.SetMode(ACR_EMoveMode.COMBAT);
    squad.Assault(attack.m_Target.GetOrigin());
   }
  }
  attack.m_fNextPush = now + ACR_AICommander.PUSH_INTERVAL * 1000;
 }

 static void UpdateAttacks(ACR_AICommander cmd)
 {
  for (int i = cmd.m_Attacks.Count() - 1; i >= 0; i--)
  {
   ACR_AIAttack attack = cmd.m_Attacks[i];
   for (int s = attack.m_Squads.Count() - 1; s >= 0; s--)
   {
    ACR_Squad squad = attack.m_Squads[s];
    if (!squad || !cmd.m_Manager.m_Squads.Contains(squad) || squad.PlayerSteers())
    {
     // A player took it: free it, or it stayed tasked ATTACK and the AI never used it again.
     if (squad && squad.m_iAITask == ACR_EAITask.ATTACK) squad.SetAITask(ACR_EAITask.FREE);
     attack.Remove(s);
     continue;
    }
    // Another job took it (base works borrow an attacker out of contact, WorksWorkerFurther):
    // it is that job's now. Left in the attack, PushAttackers kept ordering it back into the
    // assault and it still counted towards the attack's strength.
    if (squad.m_iAITask != ACR_EAITask.ATTACK)
    {
     if (attack.m_Target) cmd.Say(string.Format("[ACR] AI %1: %2 leaves the attack on %3 (now %4)", cmd.m_sFaction, squad.GetName(), attack.m_Target.m_sName, typename.EnumToString(ACR_EAITask, squad.m_iAITask)));
     ACR_AICommander.LeaveAttack(attack, squad); // not a loss: its strength leaves with it
     continue;
    }
    // Lost its gunner (or down to one man): no use in an attack.
    if (!squad.IsCombatUnit())
    {
     cmd.FallBack(squad, "can no longer fight");
     attack.Remove(s);
     continue;
    }
    // Badly hurt and still under fire: pull out instead of dying in place.
    if (attack.m_bAssaulting && squad.StrengthRatio() < cmd.RetreatRatio() && squad.KnownEnemies(300) > 0)
    {
     cmd.FallBack(squad, "heavy casualties");
     attack.Remove(s);
     continue;
    }
    // Shot dry: pull back to rearm instead of charging with empty rifles.
    if (squad.m_iAmmoPct >= 0 && squad.m_iAmmoPct < ACR_AICommander.OUT_OF_AMMO)
    {
     cmd.FallBack(squad, "out of ammo", null, true);
     attack.Remove(s);
    }
   }
   ACR_Base target = attack.m_Target;
   bool won = target && (target.m_sOwner == cmd.m_sFaction || target.m_bRetired);
   if (attack.m_bGrab && target && !cmd.SeenOwner(target).IsEmpty() && !won)
   {
    cmd.Say(string.Format("[ACR] AI %1: %2 was taken first, grab cancelled", cmd.m_sFaction, target.m_sName));
    foreach (ACR_Squad late : attack.m_Squads) late.SetAITask(ACR_EAITask.FREE);
    cmd.m_Attacks.RemoveOrdered(i);
    continue;
   }
   int strength;
   foreach (ACR_Squad member : attack.m_Squads) strength += member.FieldStrength();
   bool fusePrimed = attack.m_SapperDemo && attack.m_SapperDemo.InProgress();
   NoteBBBFight(cmd, attack);
   // Every squad went to another job (a player's request, the allocator): not a loss, not a stall.
   bool handedOff = attack.m_bGivenAway && attack.m_Squads.IsEmpty() && !won;
   bool broken = !fusePrimed && !handedOff && ACR_AICommander.AttackBroken(strength, attack.m_iStartStrength, cmd.m_Doctrine.m_fAbortRatio);
   bool stalled = !fusePrimed && !handedOff && !won && target && cmd.Stalled(attack);
   // Nobody left who can walk in (the infantry died or fell back): vehicles and support teams
   // alone never take it, so stop now instead of waiting out the time limit.
   bool noTakers = !fusePrimed && !won && target && !attack.m_bGrab && !target.m_sOwner.IsEmpty() && !attack.m_Squads.IsEmpty() && ACR_AICommander.TakersIn(attack) == 0;
   // A truce made (or a side befriended) after the launch: its squads can neither fire nor capture
   // there, and going in called preparation strikes on the partner's base, which broke the truce.
   bool truce = !fusePrimed && !won && target && !target.m_sOwner.IsEmpty() && ACR_Insurgency.Friendly(target.m_sOwner, cmd.m_sFaction);
   if (!target || won || (attack.m_Squads.IsEmpty() && !fusePrimed) || broken || stalled || noTakers || truce)
   {
    string outcome = "failed";
    if (won) outcome = "won";
    if (truce) outcome = "called off (held by a side we have a truce with)";
    if (handedOff) outcome = "ended (its squads went to other jobs)";
    if (noTakers) outcome = "called off (no infantry left to take it)";
    if (broken)
    {
     outcome = "called off (too many losses)";
     cmd.NoteRepulsed(target);
     // Not straight back at the base that just beat it (Raider breaks off at half strength
     // and weighs weak bases low, so it used to re-launch at the same one).
     if (target) cmd.m_StalledUntil.Set(target, GetGame().GetWorld().GetWorldTime() + ACR_AICommander.STALLED_COOLDOWN * 1000);
    }
    if (stalled)
    {
     // Where the attackers got to (night soak 26 Oct: 46 of 118 grabs and attacks stalled).
     float closest = -1;
     string closestOrder;
     foreach (ACR_Squad stuck : attack.m_Squads)
     {
      if (!stuck) continue;
      float gap = vector.DistanceXZ(cmd.PositionOf(stuck), target.GetOrigin());
      if (closest < 0 || gap < closest) { closest = gap; closestOrder = stuck.m_Order; }
     }
     outcome = string.Format("called off (stalled; nearest squad %1 m out, '%2'; BBB fighting %3 s)", (int)closest, closestOrder, (int)attack.m_fBBBFightS);
     if (!attack.m_bGrab) cmd.NoteRepulsed(target);
     float stallCooldown = ACR_AICommander.STALLED_COOLDOWN;
     if (attack.m_bGrab)
     {
      // Another squad tries soon: not these (NoteGrabStalled), nor a crewed vehicle after vehicles stopped short.
      cmd.NoteGrabStalled(attack);
      stallCooldown = ACR_AICommander.GRAB_RETRY_S;
      outcome += string.Format("; another squad may try in %1 s", (int)ACR_AICommander.GRAB_RETRY_S);
     }
     cmd.m_StalledUntil.Set(target, GetGame().GetWorld().GetWorldTime() + stallCooldown * 1000);
    }
    if (target) cmd.Say(string.Format("[ACR] AI %1: attack on %2 %3", cmd.m_sFaction, target.m_sName, outcome));
    string why = "it failed";
    if (broken) why = "too many losses";
    else if (stalled) why = "it stalled";
    else if (truce) why = "we have a truce with its holder";
    // Its squads went to a player's request (2IC Support me / Hold this) or another job: not a failure.
    bool givenAway = handedOff;
    if (!givenAway && !truce) cmd.NoteLaneOutcome(attack, won);
    if (target && won) cmd.TwoICReport(string.Format("Took %1", target.m_sName));
    else if (target && !givenAway && attack.m_bGrab) cmd.TwoICReport(string.Format("Couldn't take %1: %2", target.m_sName, why));
    else if (target && !givenAway) cmd.TwoICReport(string.Format("Called off the attack on %1: %2", target.m_sName, why));
    foreach (ACR_Squad squad : attack.m_Squads)
    {
     if (broken) cmd.FallBack(squad, "attack called off");
     else squad.SetAITask(ACR_EAITask.FREE);
    }
    cmd.m_Attacks.RemoveOrdered(i);
    continue;
   }
   if (attack.m_bAssaulting)
   {
    bool demoBusy = false;
    if (attack.m_bFobRaid && attack.m_SapperDemo)
    {
     float now = GetGame().GetWorld().GetWorldTime();
     attack.m_SapperDemo.Tick(now);
     ACR_ESapperDemoState demoState = attack.m_SapperDemo.State();
     // Planting or fused: hold the attackers back. After the blast the attack takes the FOB.
     if (demoState == ACR_ESapperDemoState.PLANTING || demoState == ACR_ESapperDemoState.WIRING || demoState == ACR_ESapperDemoState.FALLBACK)
     {
      demoBusy = true;
     }
    }
    if (demoBusy) continue;
    cmd.TickGround(attack);
    PushAttackers(cmd, attack);
    cmd.TickFireBase(attack);
    continue;
   }
   cmd.TickGround(attack);
   bool anyAirLift = false;
   bool gathered = true;
   foreach (ACR_Squad squad : attack.m_Squads)
   {
    if (ACR_AirLift.Busy(squad)) { anyAirLift = true; gathered = false; continue; }
    if (vector.DistanceXZ(cmd.PositionOf(squad), attack.StagingOf(squad)) > 120) gathered = false;
   }
   float stageNow = GetGame().GetWorld().GetWorldTime();
   // Out of time with nobody who can take the base anywhere near their staging point (a slow
   // drive by road, held up on the way): going in would be a lone squad far out, so call it off;
   // the base waits out the stall cooldown and the next plan picks squads that can get there.
   if (!gathered && !anyAirLift && stageNow > attack.m_fStageDeadline)
   {
    float nearestTaker = -1;
    foreach (ACR_Squad formingUp : attack.m_Squads)
    {
     if (!ACR_AICommander.CanTakeBase(formingUp, attack.m_bFobRaid)) continue;
     float lateGap = vector.DistanceXZ(cmd.PositionOf(formingUp), attack.StagingOf(formingUp));
     if (nearestTaker < 0 || lateGap < nearestTaker) nearestTaker = lateGap;
    }
    if (nearestTaker > ACR_AICommander.ASSAULT_JOIN_EXTRA)
    {
     cmd.Say(string.Format("[ACR] AI %1: attack on %2 called off (never formed up; nearest squad %3 m from its staging point)", cmd.m_sFaction, target.m_sName, (int)nearestTaker));
     cmd.m_StalledUntil.Set(target, stageNow + ACR_AICommander.STALLED_COOLDOWN * 1000);
     foreach (ACR_Squad released : attack.m_Squads) released.SetAITask(ACR_EAITask.FREE);
     cmd.m_Attacks.RemoveOrdered(i);
     continue;
    }
   }
   if (gathered || (!anyAirLift && stageNow > attack.m_fStageDeadline))
   {
    attack.m_bAssaulting = true;
    // The base of fire goes in too when it is the only squad left that can take the base.
    if (attack.m_Suppressor && ACR_AICommander.TakersIn(attack, attack.m_Suppressor) == 0) attack.m_Suppressor = null;
    attack.m_fAssaultStart = GetGame().GetWorld().GetWorldTime();
    // Preparation: shell the objective with heavy assets against fortified FOBs
    bool heavyPrep = target.m_bBuilt || cmd.DefenceOf(target) >= ACR_AICommander.Lowered(cmd.m_Doctrine.m_iHeavyPrep, cmd.FireFactor());
    if (!heavyPrep || !cmd.CallSupport(ACR_ESupportType.BOMB_RUN, target.GetOrigin(), "airstrike preparation on " + target.m_sName))
     if (!heavyPrep || !cmd.CallSupport(ACR_ESupportType.ROCKET_STRIKE, target.GetOrigin(), "rocket strike preparation on " + target.m_sName))
      if (!heavyPrep || !cmd.CallSupport(ACR_ESupportType.HELI_GUN_RUN, target.GetOrigin(), "gun run preparation on " + target.m_sName))
       if (!cmd.CallSupport(ACR_ESupportType.MORTAR_HE_LARGE, target.GetOrigin(), "preparation fire on " + target.m_sName))
        if (!cmd.CallSupport(ACR_ESupportType.MORTAR_HE_SMALL, target.GetOrigin(), "preparation fire on " + target.m_sName))
         cmd.CallSupport(ACR_ESupportType.ARTILLERY_BATTERY, target.GetOrigin(), "battery preparation fire on " + target.m_sName);
    vector screen = attack.m_Staging + (target.GetOrigin() - attack.m_Staging) * 0.55;
    cmd.CallSupport(ACR_ESupportType.MORTAR_SMOKE, screen, "smoke screen for " + target.m_sName);
    // Reserve gunship support for an assault, not only the rare counterattack.
    // CallSupport still checks cooldown, range/pad readiness and friendly distance.
    // A content pack's attack helicopter (ACR_ContentSocket) goes first when the side has one.
    if (!cmd.CallSupport(ACR_ESupportType.ATTACK_HELI, target.GetOrigin(), "attack helicopter for the assault on " + target.m_sName))
     if (!cmd.CallSupport(ACR_ESupportType.HELI_LOITER, target.GetOrigin(), "gunship for the assault on " + target.m_sName))
      cmd.CallSupport(ACR_ESupportType.HELI_DOOR_GUNS, target.GetOrigin(), "door-gunner heli for the assault on " + target.m_sName);
    foreach (ACR_Squad squad : attack.m_Squads)
    {
     squad.SetMode(ACR_EMoveMode.COMBAT);
     if (squad != attack.m_Suppressor)
     {
      IEntity vSquad = ACR_Transport.CrewedVehicle(squad);
      if (vSquad && squad.HasGunner(vSquad))
      {
       vector vSpot;
       if (cmd.OverwatchSpot(target, cmd.PositionOf(squad), vSpot)) squad.Defend(vSpot);
       else squad.Assault(target.GetOrigin());
      }
      else
      {
       squad.Assault(target.GetOrigin());
      }
     }
    }
    if (attack.m_Suppressor) cmd.UpdateFireBase(attack);
    string suppressor = "none";
    if (attack.m_Suppressor) suppressor = attack.m_Suppressor.GetName();
    cmd.Say(string.Format("[ACR] AI %1: assaulting %2 with %3 squad(s) on %4 approach line(s), base of fire: %5", cmd.m_sFaction, target.m_sName, attack.m_Squads.Count(), ACR_AICommander.ApproachLines(attack), suppressor));
   }
  }
 }
}
