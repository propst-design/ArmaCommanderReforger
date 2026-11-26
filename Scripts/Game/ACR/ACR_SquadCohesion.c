// Squad cohesion, straggler tracking, regrouping, and movement diagnostics; extracted from ACR_Squad.c.
class ACR_SquadCohesion
{
 static int OrdersSince(ACR_Squad sq, float since)
 {
  int count;
  foreach (float at : sq.m_aRecentOrders) if (at >= since) count++;
  return count;
 }

 // Its movement state in a few words: waypoint recovery, regrouping, gave up.
 static string ThinkMovement(ACR_Squad sq)
 {
  string state;
  if (sq.m_bRegrouping) state += " regrouping";
  if (sq.m_bGarrisonPending) state += " garrison pending";
  if (sq.m_iWaypointRecoveries > 0) state += string.Format(" recovery %1/4", sq.m_iWaypointRecoveries);
  if (sq.GaveUpUnreachable()) state += " GAVE UP (unreachable)";
  else if (sq.MovementStillPending()) state += string.Format(" moving, %1 m to go", (int)DistanceToOrder(sq));
  if (sq.m_Group && !sq.m_Group.GetCurrentWaypoint() && sq.MovementStillPending()) state += " NO WAYPOINT";
  return state.Trim();
 }

 static float DistanceToOrder(ACR_Squad sq)
 {
  IEntity anchor = sq.GetAnchor();
  if (!anchor) return 0;
  return vector.DistanceXZ(anchor.GetOrigin(), sq.m_MoveTarget);
 }

 static bool Stuck(ACR_Squad sq, IEntity body)
 {
  vector at;
  if (!sq.m_mStuck.Find(body, at)) return false;
  if (vector.DistanceXZ(body.GetOrigin(), at) < ACR_Squad.STUCK_MOVED) return true;
  sq.m_mStuck.Remove(body);
  return false;
 }

 // Living members awake and on foot the squad has stopped waiting for.
 static int StuckMen(ACR_Squad sq)
 {
  int count;
  array<AIAgent> agents = {};
  sq.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Squad.IsConscious(body) && !ACR_Transport.VehicleOf(body) && Stuck(sq, body)) count++;
  }
  return count;
 }

 static void HoldRegroupForTest(ACR_Squad sq, float until) { sq.m_fNextRegroup = until; sq.m_bRegrouping = false; }

 static void StartRegroupForTest(ACR_Squad sq) { sq.m_bRegrouping = true; }

 static bool RegroupingForTest(ACR_Squad sq) { return sq.m_bRegrouping; }

 static float NextRegroupForTest(ACR_Squad sq) { return sq.m_fNextRegroup; }

 static bool MovementPendingForTest(ACR_Squad sq) { return sq.MovementStillPending(); }

 static bool OrderSettled(float now, float orderedAt) { return now - orderedAt >= ACR_Squad.ORDER_SETTLE_MS; }

 // Middle of the living members on foot (where a strung-out squad meets up).
 static bool Centroid(ACR_Squad sq, out vector center)
 {
  array<AIAgent> agents = {};
  sq.GetAgents(agents);
  vector sum;
  int n;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Squad.IsConscious(body) || ACR_Transport.VehicleOf(body) || Stuck(sq, body)) continue;
   sum = sum + body.GetOrigin();
   n++;
  }
  if (n == 0) return false;
  center = ACR_Manager.Ground(sum * (1.0 / n));
  return true;
 }

 // Living members on foot farther than this from the leader.
 static int Stragglers(ACR_Squad sq, float radius)
 {
  IEntity leader = sq.GetConsciousLeader();
  if (!leader) return 0;
  int count;
  array<AIAgent> agents = {};
  sq.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || body == leader || !ACR_Squad.IsConscious(body) || ACR_Transport.VehicleOf(body) || Stuck(sq, body)) continue;
   if (vector.DistanceXZ(body.GetOrigin(), leader.GetOrigin()) > radius) count++;
  }
  return count;
 }

 static void Cohesion(ACR_Squad sq, float now, bool contact)
 {
  if (sq.m_bRegrouping)
  {
   bool closed = Stragglers(sq, ACR_Squad.REGROUPED_DISTANCE) == 0;
   if (!closed && !contact && now - sq.m_fRegroupSince < ACR_Squad.REGROUP_TIMEOUT_MS) return;
   sq.m_bRegrouping = false;
   sq.m_fNextRegroup = now + ACR_Squad.REGROUP_COOLDOWN_MS;
   if (!closed) sq.m_fNextRegroup = now + ACR_Squad.REGROUP_FAILED_COOLDOWN_MS;
   IEntity waited = sq.GetConsciousLeader();
   if (!closed && !contact && waited) sq.NoteStuck(waited);
   sq.RestoreOrder(sq.m_Regroup);
   PrintFormat("[ACR] TACTICS %1: regrouped (%2 s), moving on", sq.m_Name, (int)((now - sq.m_fRegroupSince) / 1000));
   return;
  }
  if (contact || now < sq.m_fNextRegroup || !OrderSettled(now, sq.m_fOrderedAt) || !sq.m_Group || sq.PlayerSteers() || sq.IsMounted()) return;
  bool movingOrder = sq.m_sOrderPrefab == ACR_Squad.WP_MOVE || sq.m_sOrderPrefab == ACR_Squad.WP_DEFEND || sq.m_sOrderPrefab == ACR_Squad.WP_SEARCH_DESTROY || sq.m_sOrderPrefab == ACR_Squad.WP_GARRISON;
  if (!movingOrder) return;
  array<AIWaypoint> waypoints = {};
  sq.m_Group.GetWaypoints(waypoints);
  IEntity leader = sq.GetConsciousLeader();
  if (waypoints.IsEmpty() || !leader || vector.DistanceXZ(leader.GetOrigin(), sq.m_MoveTarget) < 200) return;
  // The knocked-out can't walk to a regroup point: count only those awake.
  int alive = sq.CountConscious() - StuckMen(sq);
  if (alive < 3) return;
  int behind = Stragglers(sq, ACR_Squad.STRAGGLER_DISTANCE);
  if (behind * 3 < alive) return;
  NoteRegroupStart(sq, leader);
  sq.m_Regroup = sq.SnapshotOrder();
  sq.ClearOrders();
  // Everyone heads for the middle of the squad: the front waits and walks
  // back a little, the stragglers catch up, instead of the leader standing
  // still while the tail wanders.
  vector gather;
  if (Centroid(sq, gather)) sq.IssueWaypoint(ACR_Squad.WP_MOVE, gather, 10);
  sq.SetOrder("Regrouping", ACR_EOrderState.REGROUP);
  sq.m_bRegrouping = true;
  sq.m_fRegroupSince = now;
  PrintFormat("[ACR] TACTICS %1: %2 of %3 strung out, regrouping", sq.m_Name, behind, alive);
  LogStragglers(sq, leader);
 }

 static void NoteRegroupStart(ACR_Squad sq, IEntity leader)
 {
  sq.m_mRegroupFrom.Clear();
  array<AIAgent> agents = {};
  sq.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || body == leader || !ACR_Squad.IsConscious(body) || ACR_Transport.VehicleOf(body) || Stuck(sq, body)) continue;
   if (vector.DistanceXZ(body.GetOrigin(), leader.GetOrigin()) > ACR_Squad.STRAGGLER_DISTANCE) sq.m_mRegroupFrom.Set(body, body.GetOrigin());
  }
 }

 // Who is behind and what he is doing (21 Oct live, Kolguyev: the same two or three men every
 // regroup, the heavily loaded ones by eye). One line per straggler, plus the leader for scale.
 static void LogStragglers(ACR_Squad sq, IEntity leader)
 {
  PrintFormat("[ACR] STRAGGLER %1 leader %2: load %3 kg", sq.m_Name, StragglerKind(leader), Math.Round(LoadOf(leader)));
  array<AIAgent> agents = {};
  sq.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || body == leader || !ACR_Squad.IsConscious(body) || ACR_Transport.VehicleOf(body) || Stuck(sq, body)) continue;
   float distance = vector.DistanceXZ(body.GetOrigin(), leader.GetOrigin());
   if (distance <= ACR_Squad.STRAGGLER_DISTANCE) continue;
   string doing = "idle";
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (utility && utility.GetCurrentBehavior()) doing = utility.GetCurrentBehavior().ClassName();
   string stance = "?";
   ChimeraCharacter character = ChimeraCharacter.Cast(body);
   if (character && character.GetCharacterController()) stance = typename.EnumToString(ECharacterStance, character.GetCharacterController().GetStance());
   PrintFormat("[ACR] STRAGGLER %1 %2: %3 m behind, load %4 kg, %5, %6, climb %7 m", sq.m_Name, StragglerKind(body), Math.Round(distance), Math.Round(LoadOf(body)), doing, stance, Math.Round(leader.GetOrigin()[1] - body.GetOrigin()[1]));
  }
 }

 static float LoadOf(IEntity body)
 {
  if (!body) return 0;
  SCR_CharacterInventoryStorageComponent storage = SCR_CharacterInventoryStorageComponent.Cast(body.FindComponent(SCR_CharacterInventoryStorageComponent));
  if (!storage) return 0;
  return storage.GetTotalWeight();
 }

 static string StragglerKind(IEntity body)
 {
  if (!body || !body.GetPrefabData()) return "?";
  string name = body.GetPrefabData().GetPrefabName();
  int slash = name.LastIndexOf("/");
  if (slash >= 0) name = name.Substring(slash + 1, name.Length() - slash - 1);
  name.Replace(".et", "");
  name.Replace("Character_", "");
  return name;
 }
}
