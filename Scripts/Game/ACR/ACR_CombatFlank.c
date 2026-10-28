// Squad manoeuvre around fresh, personally observed infantry contacts. Native
// combat still chooses targets, cover and firing. No enemy world-position lookup.
class ACR_CombatFlank : Managed
{
 protected ref ACR_OrderSnapshot m_Original;
 protected vector m_Start, m_Threat, m_Flank;
 protected float m_fStillSince, m_fNextCheck, m_fNextFlank, m_fStarted, m_fNextRecovery;
 protected int m_iSerial, m_iLeg, m_iRecoveries;
 int m_iStarted;
 // The automatic flank of a stalled contact (Tick) is off (#54, Jacob 27 Oct): BBB's fire and
 // manoeuvre and squad tactics own how a squad in contact moves. It also never fired in play: a
 // squad shooting raises its own threat measure to 0.9-1.0, so Contact() always reported it
 // pinned (2 starts in 11 playtests). Begin() and an active manoeuvre still work; true restores it.
 static bool s_bAutomatic = false;
 bool Active() { return m_Original != null; }
 ACR_OrderSnapshot Original() { return m_Original; }
 void Cancel() { m_Original = null; m_fStillSince = 0; }

 static bool Eligible(ACR_Squad squad)
 {
  if (!squad || !squad.m_Group || squad.m_Commander != 0 || squad.HasPlayer() || squad.IsMounted()) return false;
  if (squad.m_Mode != ACR_EMoveMode.COMBAT || squad.CountAlive() < 3 || !squad.IsCombatUnit()) return false;
  ACR_EOrderState state = squad.OrderState();
  if (state != ACR_EOrderState.MOVE && state != ACR_EOrderState.ASSAULT) return false;
  if (squad.m_sOrderPrefab != ACR_Squad.WP_MOVE && squad.m_sOrderPrefab != ACR_Squad.WP_SEARCH_DESTROY) return false;
  return squad.StrengthRatio() >= 0.5 && (squad.m_iAmmoPct < 0 || squad.m_iAmmoPct >= 25);
 }

 // Gather last seen positions from the soldiers' sensors, never GetOrigin
 // on the target. Only a fresh enemy infantry report can trigger manoeuvre.
 static bool Contact(ACR_Squad squad, out vector position, out bool pinned)
 {
  pinned = false;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  float nearest = 350;
  bool found;
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return false;
  foreach (AIAgent agent : agents)
  {
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (!utility || !utility.m_PerceptionComponent) continue;
   if (utility.m_ThreatSystem && utility.m_ThreatSystem.GetThreatMeasure() >= 0.66) pinned = true;
   if (utility.m_CombatComponent && utility.m_CombatComponent.GetCombatMode() == EAIGroupCombatMode.HOLD_FIRE) { pinned = true; continue; }
   array<BaseTarget> targets = {};
   utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
   foreach (BaseTarget target : targets)
   {
    if (!target || target.GetTargetCategory() != ETargetCategory.ENEMY || target.GetUnitType() != EAIUnitType.UnitType_Infantry || target.GetTimeSinceSeen() > 5) continue;
    vector at = target.GetLastSeenPosition();
    float distance = vector.DistanceXZ(anchor.GetOrigin(), at);
    if (distance < 45 || distance >= nearest) continue;
    nearest = distance;
    position = at;
    found = true;
   }
  }
  return found;
 }

 // Don't send every squad out of its firing position at once. A neighbour
 // currently manoeuvring at the same contact owns this sector until it finishes.
 static bool SectorBusy(ACR_Squad squad, vector threat)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !squad.GetFaction()) return false;
  foreach (ACR_Squad other : manager.m_Squads)
  {
   if (other == squad || !other.GetFaction() || other.GetFaction() != squad.GetFaction()) continue;
   if (other.m_CombatFlank.Active() && vector.DistanceXZ(other.m_CombatFlank.m_Threat, threat) < 150) return true;
  }
  return false;
 }

 // A two-leg dogleg, perpendicular to the enemy bearing, rather than another
 // destination straight down the engagement lane. Mesh snaps are kept local.
 static bool Plan(ACR_Squad squad, vector threat, int side, out vector exit, out vector flank)
 {
  IEntity anchor = squad.GetAnchor();
  AIAgent leader = squad.m_Group.GetLeaderAgent();
  if (!anchor || !leader) return false;
  AIPathfindingComponent pathing = AIPathfindingComponent.Cast(leader.FindComponent(AIPathfindingComponent));
  if (!pathing) { if (System.IsCLIParam("acrFlankTest")) Print("[ACR] FLANKTEST no pathing"); return false; }
  vector start = anchor.GetOrigin();
  vector forward = threat - start;
  forward[1] = 0;
  float distance = forward.Length();
  if (distance < 45 || distance > 350) return false;
  forward.Normalize();
  float direction = side;
  vector lateral = Vector(-forward[2], 0, forward[0]) * direction;
  array<float> widths = {65, 45, 85};
  foreach (float width : widths)
  {
   vector a = ACR_Manager.Ground(start + lateral * 25);
   vector b = ACR_Manager.Ground(start + lateral * width + forward * Math.Min(60, distance * 0.25));
   NavmeshWorldComponent mesh = pathing.GetNavmeshComponent();
   if (mesh) { mesh.LoadTileIn(a); mesh.LoadTileIn(b); }
   vector onA, onB;
   if (!pathing.GetClosestPositionOnNavmesh(a, Vector(3, 3, 3), onA) || !pathing.GetClosestPositionOnNavmesh(b, Vector(3, 3, 3), onB)) { if (System.IsCLIParam("acrFlankTest")) PrintFormat("[ACR] FLANKTEST mesh rejects %1 / %2", a, b); continue; }
   if (BBB_Terrain.IsWaterAt(onA) || BBB_Terrain.IsWaterAt(onB) || BBB_Terrain.WaterAlong(start, onA, 5) > 0 || BBB_Terrain.WaterAlong(onA, onB, 5) > 0) continue;
   if (Math.AbsFloat(onB[1] - start[1]) > width * 0.35) continue;
   // Reject a blocked lateral departure. Navmesh will route the longer leg;
   // a local mesh point alone does not establish connectivity to it.
   TraceParam trace = new TraceParam();
   trace.Start = start + Vector(0, 0.8, 0);
   trace.End = onA + Vector(0, 0.8, 0);
   trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
   trace.Exclude = anchor;
   if (GetGame().GetWorld().TraceMove(trace, ACR_CombatFlank.StaticObstacle) < 0.99) { if (System.IsCLIParam("acrFlankTest")) PrintFormat("[ACR] FLANKTEST blocked exit %1 to %2", start, onA); continue; }
   exit = onA;
   flank = onB;
   return true;
  }
  return false;
 }

 static bool StaticObstacle(notnull IEntity entity)
 {
  return ChimeraCharacter.Cast(entity) == null;
 }

 bool Begin(ACR_Squad squad, vector threat, float now)
 {
  if (Active() || now < m_fNextFlank || !Eligible(squad) || SectorBusy(squad, threat)) return false;
  vector exit, flank;
  int side = 1;
  int identity = squad.m_Name.Length();
  if (identity > 0) identity = squad.m_Name.Get(identity - 1).ToAscii();
  if (identity % 2 == 0) side = -1;
  if (!Plan(squad, threat, side, exit, flank) && !Plan(squad, threat, -side, exit, flank)) return false;
  ref ACR_OrderSnapshot saved = squad.SnapshotOrder();
  // Only the short lateral departure outranks ordinary attack behaviour, so
  // they leave the firing lane. It expires in 12 s; the rest uses native combat.
  if (!squad.IssueWaypoint(ACR_Squad.WP_MOVE, exit, 8, true)) return false;
  m_Original = saved;
  m_iStarted++;
  m_iSerial = squad.m_iOrderSerial;
  m_Threat = threat;
  m_Flank = flank;
  m_fStarted = now;
  m_fNextFlank = now + 90000;
  m_iLeg = 0;
  m_iRecoveries = 0;
  m_fNextRecovery = now + 10000;
  PrintFormat("[ACR] FLANK %1: lateral %2, approach %3, objective %4", squad.GetName(), exit, flank, saved.m_Target);
  return true;
 }

 protected void Finish(ACR_Squad squad, string reason)
 {
  ref ACR_OrderSnapshot saved = m_Original;
  Cancel();
  squad.RestoreOrder(saved);
  PrintFormat("[ACR] FLANK %1: %2, resuming objective", squad.GetName(), reason);
 }

 void YieldToPlayer(ACR_Squad squad)
 {
  if (!Active()) return;
  // Only replace our own manoeuvre waypoint. A native player command wins.
  if (squad.m_Group && squad.m_Group.GetCurrentWaypoint() == squad.m_Waypoint) Finish(squad, "player joined");
  else Cancel();
 }

 bool Tick(ACR_Squad squad, float now)
 {
  if (Active())
  {
   if (squad.m_iOrderSerial != m_iSerial) { Cancel(); return false; }
   if (!Eligible(squad)) { Finish(squad, "no longer fit to manoeuvre"); return false; }
   vector fresh;
   bool pinned;
   ACR_CombatFlank.Contact(squad, fresh, pinned);
   if (pinned || now - m_fStarted > 60000) { Finish(squad, "pinned or timed out"); return true; }
   IEntity anchor = squad.GetAnchor();
   if (!anchor) { Finish(squad, "lost leader"); return true; }
   float gap = vector.DistanceXZ(anchor.GetOrigin(), squad.m_MoveTarget);
   if (m_iLeg == 0 && (gap < 10 || now - m_fStarted >= 12000))
   {
    m_iLeg = 1;
    squad.IssueWaypoint(ACR_Squad.WP_MOVE, m_Flank, 12);
   }
   else if (m_iLeg == 1 && gap < 15) Finish(squad, "on flank");
   else if (!squad.m_Group.GetCurrentWaypoint() && now >= m_fNextRecovery)
   {
    if (m_iRecoveries >= 3) Finish(squad, "unreachable flank");
    else
    {
     m_iRecoveries++;
     m_fNextRecovery = now + 10000;
     squad.IssueWaypoint(ACR_Squad.WP_MOVE, squad.m_MoveTarget, 12, m_iLeg == 0);
    }
   }
   return true;
  }
  if (!s_bAutomatic) { m_fStillSince = 0; return false; }
  if (now < m_fNextCheck) return false;
  m_fNextCheck = now + 3000;
  if (!Eligible(squad) || now < m_fNextFlank) { m_fStillSince = 0; return false; }
  IEntity body = squad.GetAnchor();
  if (!body || vector.DistanceXZ(body.GetOrigin(), squad.m_MoveTarget) < 60) { m_fStillSince = 0; return false; }
  vector threat;
  bool suppressed;
  if (!ACR_CombatFlank.Contact(squad, threat, suppressed) || suppressed) { m_fStillSince = 0; return false; }
  // Only flank contacts in the direction of the objective, not enemies behind us.
  vector goal = squad.m_MoveTarget - body.GetOrigin();
  vector enemy = threat - body.GetOrigin();
  goal[1] = 0; enemy[1] = 0;
  goal.Normalize(); enemy.Normalize();
  if (vector.Dot(goal, enemy) < 0.5) { m_fStillSince = 0; return false; }
  if (m_fStillSince == 0 || vector.DistanceXZ(body.GetOrigin(), m_Start) > 6)
  {
   m_Start = body.GetOrigin();
   m_fStillSince = now;
   return false;
  }
  if (now - m_fStillSince < 12000) return false;
  // A rejected terrain/coordination attempt waits rather than querying every tick.
  m_fNextCheck = now + 10000;
  return Begin(squad, threat, now);
 }
}
