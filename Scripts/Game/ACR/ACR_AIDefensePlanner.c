// AI commander defence assignment, counter-battery support, and defence-in-depth; extracted from ACR_AICommander.c.
class ACR_AIDefensePlanner
{
 static void AssignDefenders(ACR_AICommander cmd)
 {
  // Release defenders of bases that are no longer threatened (or no longer ours).
  // (only our own: with both sides on AI, the other commander's defenders
  // were set free here every tick, since their base isn't ours)
  foreach (ACR_Squad squad : cmd.m_Manager.m_Squads)
  {
   if (squad.m_iAITask != ACR_EAITask.DEFEND || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != cmd.m_sFaction) continue;
   if (squad == cmd.m_FobEscort && cmd.HasActiveFobProject()) continue;
   if (cmd.OnRequest(squad)) continue; // a player's request to the 2IC (#14 v1)
   ACR_Base held = squad.m_AITarget;
   if (!held || held.m_sOwner != cmd.m_sFaction || cmd.DefenceThreat(held) == 0)
   {
    if (held && cmd.m_Manager.HasPrimedDemolition(held)) continue;
    squad.SetAITask(ACR_EAITask.FREE);
    continue;
   }
   // A defence that is being overrun: the badly hurt get out rather than die in place.
   bool overrun = cmd.ThreatTo(held) > cmd.DefenceOf(held) * 2;
   if (overrun && squad.StrengthRatio() < cmd.HoldRatio() && squad.KnownEnemies(300) > 0)
    cmd.FallBack(squad, "defence of " + held.m_sName + " overrun", held);
   else if (squad.m_iAmmoPct >= 0 && squad.m_iAmmoPct < ACR_AICommander.OUT_OF_AMMO && squad.KnownEnemies(300) > 0)
    cmd.FallBack(squad, "out of ammo", held, true);
  }
  array<ACR_Squad> free = {};
  cmd.MySquads(free, true, true); // a threatened base calls squads on their way to a posting too
  if (cmd.m_ReserveSquad) free.RemoveItem(cmd.m_ReserveSquad);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != cmd.m_sFaction) continue;
   int threat = cmd.DefenceThreat(base);
   if (threat == 0) continue;
   string threatText = cmd.ThreatText(base);
   int defence = cmd.DefenceOf(base);
   if (cmd.ThreatTo(base) > defence * ACR_AICommander.CONCEDE_RATIO * cmd.m_Doctrine.m_fConcedeRatio && defence > 0) continue; // lost cause (the enemies there now, not a forecast)
   float needed = threat * ACR_AICommander.DEFENSE_RATIO * cmd.m_Doctrine.m_fDefenseRatio - defence;
   // Defending ahead of a forecast attack, the walk takes minutes: squads already on their way count, or a
   // base with nobody there yet called one more every tick (forecastdefend: four teams for an expected 3).
   if (cmd.ThreatTo(base) == 0) needed -= cmd.EnRouteTo(base);
   if (needed <= 0) continue;
   // Outnumbered there (the attackers outweigh all we have within DEFENSE_DISTANCE, its garrison
   // included): the squads holding our roadblocks and positions nearby go too, nearest first with
   // the free squads and before the reserve, a Fortress's included (review of #16: on a weak side,
   // holders that stay lock its few squads into static positions). Else they go only as a last
   // resort, below.
   bool outnumbered = threat > defence;
   array<ACR_Squad> pool = {};
   foreach (ACR_Squad spare : free) pool.Insert(spare);
   if (outnumbered) cmd.HoldersNear(base.GetOrigin(), ACR_AICommander.DEFENSE_DISTANCE * 3, pool);
   cmd.SortByDistance(pool, base.GetOrigin());
   int k = 0;
   while (k < pool.Count() && needed > 0)
   {
    ACR_Squad squad = pool[k];
    if (vector.DistanceXZ(cmd.PositionOf(squad), base.GetOrigin()) > ACR_AICommander.DEFENSE_DISTANCE * 3) { k++; continue; }
    if (ACR_AICommander.IsSupportTeam(squad) && free.Count() > 1) { k++; continue; }
    if (cmd.KeepsWorksManned(squad, base, outnumbered)) { k++; continue; }
    bool holding = ACR_AICommander.IsHolder(squad);
    if (holding) cmd.NoteLeftPost(squad);
    squad.SetMode(ACR_EMoveMode.FAST);
    cmd.Travel(squad, base.GetOrigin(), true, 0);
    squad.SetAITask(ACR_EAITask.DEFEND);
    squad.m_AITarget = base;
    needed -= squad.Strength();
    pool.RemoveOrdered(k);
    free.RemoveItem(squad);
    if (holding)
     cmd.Say(string.Format("[ACR] AI %1: %2 leaves what it holds to defend %3 (%4, outnumbered: defence %5)", cmd.m_sFaction, squad.GetName(), base.m_sName, threatText, defence));
    else
     cmd.Say(string.Format("[ACR] AI %1: %2 defends %3 (%4)", cmd.m_sFaction, squad.GetName(), base.m_sName, threatText));
   }
   // Only a reserve still posted: one sent into an attack (or a sweep, a resupply) meanwhile kept
   // m_ReserveSquad, and was then also committed here and pulled between the two jobs.
   if (needed > 0 && cmd.m_ReserveSquad && cmd.m_ReserveSquad.m_iAITask == ACR_EAITask.POSTED && !cmd.m_ReserveSquad.PlayerSteers() && cmd.m_ReserveSquad.CountAlive() > 0 && cmd.m_ReserveSquad.KnownEnemies(250) == 0 && !cmd.KeepsWorksManned(cmd.m_ReserveSquad, base, outnumbered))
   {
    ACR_Squad qrf = cmd.m_ReserveSquad;
    cmd.m_ReserveSquad = null;
    qrf.SetMode(ACR_EMoveMode.FAST);
    cmd.Travel(qrf, base.GetOrigin(), true, 0);
    qrf.SetAITask(ACR_EAITask.DEFEND);
    qrf.m_AITarget = base;
    needed -= qrf.Strength();
    cmd.Say(string.Format("[ACR] AI %1: QRF reserve %2 committed to defend %3 (%4)", cmd.m_sFaction, qrf.GetName(), base.m_sName, threatText));
   }
   // Still short: the builders holding our roadblocks and positions nearby, as a last resort
   // (never a Fortress's while it isn't outnumbered there: its defences stay manned).
   if (needed <= 0 || cmd.m_Doctrine.m_bHoldersStay) continue;
   array<ACR_Squad> holders = {};
   cmd.HoldersNear(base.GetOrigin(), ACR_AICommander.DEFENSE_DISTANCE * 3, holders);
   foreach (ACR_Squad holder : holders)
   {
    if (needed <= 0) break;
    if (cmd.KeepsWorksManned(holder, base, outnumbered)) continue;
    cmd.NoteLeftPost(holder);
    holder.SetMode(ACR_EMoveMode.FAST);
    cmd.Travel(holder, base.GetOrigin(), true, 0);
    holder.SetAITask(ACR_EAITask.DEFEND);
    holder.m_AITarget = base;
    needed -= holder.Strength();
    cmd.Say(string.Format("[ACR] AI %1: %2 leaves what it holds to defend %3 (%4)", cmd.m_sFaction, holder.GetName(), base.m_sName, threatText));
   }
  }
  DefendInDepth(cmd);
  // Defensive fires: shell the attackers of a base that is losing the fight (a rich side: one
  // that is merely in a fight, down to half its defence at the RP cap).
  float fireFactor = cmd.FireFactor();
  foreach (ACR_Base held : ACR_Base.s_All)
  {
   if (held.m_sOwner != cmd.m_sFaction || fireFactor * cmd.ThreatTo(held) <= cmd.DefenceOf(held)) continue;
   vector seenAt;
   ACR_Squad attacker = cmd.EnemyNear(held.GetOrigin(), ACR_AICommander.THREAT_DISTANCE + held.m_fRadius, seenAt);
   if (!attacker) continue;
   int airThreat = ACR_AICommander.Lowered(cmd.m_Doctrine.m_iDefensiveAirThreat, fireFactor);
   if (cmd.ThreatTo(held) >= airThreat && cmd.CallSupport(ACR_ESupportType.HELI_GUN_RUN, seenAt, "gun run at " + held.m_sName)) break;
   if (cmd.ThreatTo(held) >= airThreat && cmd.CallSupport(ACR_ESupportType.ATTACK_HELI, seenAt, "attack helicopter at " + held.m_sName)) break;
   if (cmd.ThreatTo(held) >= airThreat && cmd.CallSupport(ACR_ESupportType.ROCKET_STRIKE, seenAt, "rocket strike at " + held.m_sName)) break;
   if (cmd.CallSupport(ACR_ESupportType.MORTAR_HE_LARGE, seenAt, "defensive fire at " + held.m_sName)) break;
   if (cmd.CallSupport(ACR_ESupportType.MORTAR_HE_SMALL, seenAt, "defensive fire at " + held.m_sName)) break;
   // No mortar in reach: the howitzer battery (slow and dear, but it reaches anywhere).
   if (cmd.CallSupport(ACR_ESupportType.ARTILLERY_BATTERY, seenAt, "battery fire at " + held.m_sName)) break;
  }

  // Roadblock interdiction fires: shell enemies spotted approaching forward checkpoints
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (!rb || rb.m_sFaction != cmd.m_sFaction) continue;
   vector enemySeen;
   ACR_Squad hostile = cmd.EnemyNear(rb.m_vPos, 220, enemySeen);
   if (hostile && vector.DistanceXZ(rb.m_vPos, enemySeen) >= 60.0)
   {
    if (cmd.CallSupport(ACR_ESupportType.MORTAR_HE_LARGE, enemySeen, "roadblock interdiction")) break;
    if (cmd.CallSupport(ACR_ESupportType.MORTAR_HE_SMALL, enemySeen, "roadblock interdiction")) break;
   }
  }
  CounterBatterySupport(cmd, fireFactor);
 }

 // Counter-battery the side pays for (ACR_CounterBattery): an enemy mortar pinned down (its ring
 // FIX_TIGHT or less) that our own fire-at-will teams left alone for CB_PAID_WAIT_MS (none in reach,
 // or all busy) gets our mortar support, or else the howitzer battery when the side can spare it:
 // rich, or a doctrine that fires more (Firepower always; Fortress from about 670 RP, the others
 // from about 800-900). CallSupport keeps it to the side's own RP and away from our own men.
 static void CounterBatterySupport(ACR_AICommander cmd, float fireFactor)
 {
  ACR_Intel intel = cmd.m_Manager.IntelFor(cmd.m_sFaction);
  float now = GetGame().GetWorld().GetWorldTime();
  ACR_CBFix fix = ACR_CounterBattery.Target(intel, now);
  if (!fix || fix.m_fRadius > ACR_CounterBattery.FIX_TIGHT || fix.m_fPinnedAt <= 0) return;
  if (now - fix.m_fPinnedAt < ACR_CounterBattery.CB_PAID_WAIT_MS) return;
  string why = "counter-battery on the enemy mortar " + intel.Place(fix.m_vCentre);
  bool fired = cmd.CallSupport(ACR_ESupportType.MORTAR_HE_SMALL, fix.m_vCentre, why);
  float lands = ACR_CounterBattery.MORTAR_LANDS_MS;
  float eager = fireFactor / Math.Max(0.1, cmd.m_Doctrine.m_fFireMissionGap);
  if (!fired && eager >= ACR_CounterBattery.CB_BATTERY_EAGER)
  {
   fired = cmd.CallSupport(ACR_ESupportType.ARTILLERY_BATTERY, fix.m_vCentre, "battery " + why);
   lands = ACR_CounterBattery.BATTERY_LANDS_MS;
  }
  if (!fired) return;
  ACR_CounterBattery.s_iPaidFires++;
  ACR_CounterBattery.Shelled(fix, fix.m_vCentre, vector.Zero, now + lands);
 }

 static void CounterBatterySupportForTest(ACR_AICommander cmd, float fireFactor) { CounterBatterySupport(cmd, fireFactor); }

 // Where the known attackers of a base are (strength-weighted mean of where they were last
 // seen within the threat ring). False: none seen.
 static bool ThreatFrom(ACR_AICommander cmd, ACR_Base base, out vector at)
 {
  vector center = base.GetOrigin();
  vector sum;
  float weight;
  foreach (ACR_Squad squad : cmd.m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == base.m_sOwner) continue;
   vector known;
   if (!cmd.KnownPosition(squad, known) || vector.DistanceXZ(known, center) > ACR_AICommander.THREAT_DISTANCE + base.m_fRadius) continue;
   float w = Math.Max(1, squad.Strength());
   sum = sum + known * w;
   weight += w;
  }
  if (weight <= 0) return false;
  at = sum * (1.0 / weight);
  return true;
 }

 static bool ForwardSpotOf(ACR_AICommander cmd, ACR_Squad squad, out vector spot)
 {
  return cmd.m_ForwardAt.Find(squad, spot);
 }

 static void RecallForward(ACR_AICommander cmd, ACR_Squad squad, ACR_Base base, string why)
 {
  cmd.m_ForwardAt.Remove(squad);
  if (!base) return;
  squad.SetMode(ACR_EMoveMode.FAST);
  cmd.Travel(squad, base.GetOrigin(), true, 0);
  cmd.Say(string.Format("[ACR] AI %1: %2 falls back from the forward line to %3 (%4)", cmd.m_sFaction, squad.GetName(), base.m_sName, why));
 }

 static void DefendInDepth(ACR_AICommander cmd)
 {
  // Forget squads that no longer defend (freed, fallen back, taken over, gone).
  array<ACR_Squad> stale = {};
  foreach (ACR_Squad held, vector heldAt : cmd.m_ForwardAt)
   if (!held || !cmd.m_Manager.m_Squads.Contains(held) || held.PlayerSteers() || held.m_iAITask != ACR_EAITask.DEFEND || !held.m_AITarget) stale.Insert(held);
  foreach (ACR_Squad gone : stale) cmd.m_ForwardAt.Remove(gone);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != cmd.m_sFaction || cmd.DefenceThreat(base) == 0) continue;
   vector center = base.GetOrigin();
   array<ACR_Squad> defenders = {};
   foreach (ACR_Squad squad : cmd.m_Manager.m_Squads)
   {
    if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != cmd.m_sFaction || squad.PlayerSteers() || !squad.GetAnchor()) continue;
    if (cmd.m_bHandedOnly && !squad.m_bAICommanded) continue;
    if (squad == cmd.m_FobEscort || squad == cmd.m_ReserveSquad || cmd.IsScreen(squad) || squad.m_bMortar || squad.Aircraft() || ACR_AirLift.Busy(squad)) continue;
    bool defending = squad.m_iAITask == ACR_EAITask.DEFEND && squad.m_AITarget == base;
    // Troops posted at a threatened base are its defence now.
    bool posted = squad.m_iAITask == ACR_EAITask.POSTED && squad.IsCombatUnit() && !ACR_AICommander.IsSupportTeam(squad) && vector.DistanceXZ(cmd.PositionOf(squad), center) <= base.m_fRadius + 150;
    if (!defending && !posted) continue;
    if (posted)
    {
     squad.SetAITask(ACR_EAITask.DEFEND);
     squad.m_AITarget = base;
    }
    defenders.Insert(squad);
   }
   vector enemyAt;
   bool seen = ThreatFrom(cmd, base, enemyAt);
   float enemyDistance = 0;
   if (seen) enemyDistance = vector.DistanceXZ(enemyAt, center);
   float depth = 0;
   if (seen) depth = Math.Clamp(Math.Max(enemyDistance * 0.5, base.m_fRadius + 40), ACR_AICommander.FORWARD_MIN, ACR_AICommander.FORWARD_MAX);
   bool lineHolds = seen && enemyDistance - depth >= ACR_AICommander.FORWARD_ENEMY_GAP;
   // How many go forward: the doctrine's share (half standard), the base garrison counting as
   // one more squad at the point; at least one squad (or the garrison) stays as the reserve.
   int extra = 0;
   if (base.m_iGarrisonLeft > 0) extra = 1;
   int forward = (int)(cmd.m_Doctrine.m_fForwardShare * (defenders.Count() + extra));
   if (extra == 0) forward = Math.Min(forward, defenders.Count() - 1);
   if (!lineHolds) forward = 0;
   // Already forward and fit first (no shuffling), then the nearest to the line.
   vector dir = enemyAt - center;
   dir[1] = 0;
   if (dir.LengthSq() > 1) dir.Normalize();
   else dir = Vector(0, 0, 1);
   vector line = center + dir * depth;
   array<ACR_Squad> picked = {};
   foreach (ACR_Squad was : defenders)
    if (picked.Count() < forward && cmd.m_ForwardAt.Contains(was) && was.StrengthRatio() >= 0.5) picked.Insert(was);
   cmd.SortByDistance(defenders, line);
   foreach (ACR_Squad near : defenders)
    if (picked.Count() < forward && !picked.Contains(near) && near.StrengthRatio() >= 0.5) picked.Insert(near);
   foreach (ACR_Squad member : defenders)
   {
    if (picked.Contains(member) || !cmd.m_ForwardAt.Contains(member)) continue;
    string why = "the enemy is too close";
    if (member.StrengthRatio() < 0.5) why = "badly hurt";
    else if (lineHolds) why = "needed in reserve";
    RecallForward(cmd, member, base, why);
   }
   vector across = Vector(-dir[2], 0, dir[0]);
   foreach (int i, ACR_Squad goer : picked)
   {
    float offset = (i - (picked.Count() - 1) * 0.5) * ACR_AICommander.FORWARD_SPREAD;
    vector spot = ACR_Manager.Ground(line + across * offset);
    if (BBB_Terrain.IsWaterAt(spot)) spot = ACR_Manager.Ground(line);
    if (BBB_Terrain.IsWaterAt(spot)) continue;
    vector had;
    if (cmd.m_ForwardAt.Find(goer, had) && vector.DistanceXZ(had, spot) < ACR_AICommander.FORWARD_REISSUE) continue;
    cmd.m_ForwardAt.Set(goer, spot);
    if (vector.DistanceXZ(cmd.PositionOf(goer), spot) > 220 && goer.KnownEnemies(350) == 0) goer.SetMode(ACR_EMoveMode.FAST);
    else goer.SetMode(ACR_EMoveMode.COMBAT);
    cmd.Travel(goer, spot, true, 0);
    cmd.Say(string.Format("[ACR] AI %1: %2 moves out to a forward line %3 m from %4 towards the attack (enemy seen %5 m out, %6 stay in reserve)", cmd.m_sFaction, goer.GetName(), Math.Round(depth), base.m_sName, Math.Round(enemyDistance), defenders.Count() - picked.Count()));
   }
  }
 }
}
