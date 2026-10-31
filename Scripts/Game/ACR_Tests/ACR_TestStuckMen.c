// srv:stuckmen (about 3 min): a squad on a long move with a third of its men unable to walk (their
// AI switched off, as a man jammed at a door). Checks: the first regroup waits for them; when it
// runs out the squad notes them and carries on with its order; it doesn't stop for them again, and
// its meeting point no longer counts them; a man who moves again counts again.
class ACR_TestStuckMen
{
 static const ResourceName SQUAD = "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
 static const string NAME = "Stuck Men Squad";
 protected static ref array<IEntity> s_aFrozen = {};
 protected static bool s_bSawRegroup;
 protected static bool s_bEnded;
 protected static int s_iEnded;
 protected static bool s_bStoppedAgain;
 static int s_iFollowers;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 10) Spawn(manager);
  ACR_Squad squad = Find(manager);
  if (!squad) return;
  if (second == 14) Order(squad);
  if (second <= 14) return;
  // A regroup only waits for men already out of touch: it holds until all the frozen men are.
  if (!s_bSawRegroup && FrozenInTouch(squad))
  {
   squad.HoldRegroupForTest(GetGame().GetWorld().GetWorldTime() + 3000);
   return;
  }
  if (squad.RegroupingForTest())
  {
   if (s_bEnded) s_bStoppedAgain = true;
   s_bSawRegroup = true;
   return;
  }
  if (s_bSawRegroup && !s_bEnded)
  {
   s_bEnded = true;
   s_iEnded = second;
   Ended(squad);
   return;
  }
  if (s_bEnded && second == s_iEnded + 30) After(squad);
  if (!s_bEnded && second == 150) ACR_TestRegress.Abort("stuckmen", "no regroup in 135 s");
 }

 protected static bool FrozenInTouch(ACR_Squad squad)
 {
  IEntity leader = squad.GetConsciousLeader();
  if (!leader) return false;
  foreach (IEntity body : s_aFrozen)
   if (body && vector.DistanceXZ(body.GetOrigin(), leader.GetOrigin()) <= ACR_Squad.STRAGGLER_DISTANCE + 2) return true;
  return false;
 }

 protected static ACR_Squad Find(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == NAME) return squad;
  return null;
 }

 protected static void Spawn(ACR_Manager manager)
 {
  string side = ACR_FactionSelection.West();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != side || base.m_bRetired) continue;
   vector spot = ACR_Manager.Ground(base.GetOrigin() + Vector(40, 0, 40));
   manager.m_sSpawningFaction = side;
   manager.SpawnUnit(NAME, SQUAD, "", false, "", spot, spot);
   manager.m_sSpawningFaction = "";
   return;
  }
  ACR_TestRegress.Abort("stuckmen", "no base to spawn at");
 }

 // A third of the men (not the leader) stop for good; the rest set off for the furthest base.
 protected static void Order(ACR_Squad squad)
 {
  IEntity leader = squad.GetLeader();
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  int wanted = Math.Ceil(agents.Count() / 3.0);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || body == leader || s_aFrozen.Count() >= wanted) continue;
   agent.DeactivateAI();
   s_aFrozen.Insert(body);
  }
  ACR_Base far;
  float best;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired) continue;
   float distance = vector.DistanceXZ(base.GetOrigin(), leader.GetOrigin());
   if (distance > best) { best = distance; far = base; }
  }
  bool moved = far && squad.MoveTo(ACR_Manager.Ground(far.GetOrigin()));
  PrintFormat("[ACR] STUCKMEN %1 of %2 men frozen, ordered %3 m away: %4", s_aFrozen.Count(), agents.Count(), (int)best, moved);
  if (!moved) ACR_TestRegress.Abort("stuckmen", "no move order");
 }

 protected static void Ended(ACR_Squad squad)
 {
  // A man in formation behind a frozen team leader can stop with him: he never moved either, so he is
  // noted too, and counts again once he moves.
  s_iFollowers = squad.StuckMen() - s_aFrozen.Count();
  ACR_TestRegress.Check("stuckmen: the first regroup waits for them, then notes who couldn't come", s_iFollowers >= 0 && s_iFollowers <= 1, string.Format("%1 noted of %2 frozen", squad.StuckMen(), s_aFrozen.Count()));
  ACR_TestRegress.Check("stuckmen: the squad carries on with its order", squad.MovementPendingForTest(), squad.m_Order);
  squad.HoldRegroupForTest(0);
 }

 protected static void After(ACR_Squad squad)
 {
  IEntity leader = squad.GetConsciousLeader();
  float gap = 0;
  foreach (IEntity body : s_aFrozen) if (body) gap = Math.Max(gap, vector.DistanceXZ(body.GetOrigin(), leader.GetOrigin()));
  ACR_TestRegress.Check("stuckmen: they are far behind and it doesn't stop for them again", gap > ACR_Squad.STRAGGLER_DISTANCE && !s_bStoppedAgain, string.Format("%1 m behind, stopped again %2", (int)gap, s_bStoppedAgain));
  ACR_TestRegress.Check("stuckmen: nobody else counts as a straggler", squad.Stragglers(ACR_Squad.STRAGGLER_DISTANCE) <= s_iFollowers, string.Format("%1, followers noted %2", squad.Stragglers(ACR_Squad.STRAGGLER_DISTANCE), s_iFollowers));
  vector center;
  bool has = squad.Centroid(center);
  ACR_TestRegress.Check("stuckmen: the meeting point doesn't count them", has && vector.DistanceXZ(center, leader.GetOrigin()) < gap * 0.25, string.Format("%1 m from the leader, they are %2 m", (int)vector.DistanceXZ(center, leader.GetOrigin()), (int)gap));
  // One of them gets free.
  int noted = squad.StuckMen();
  IEntity freed = s_aFrozen[0];
  freed.SetOrigin(ACR_Manager.Ground(freed.GetOrigin() + Vector(6, 0, 0)));
  freed.Update();
  ACR_TestRegress.Check("stuckmen: a man who moves counts again", squad.StuckMen() == noted - 1, string.Format("%1 noted, %2 before", squad.StuckMen(), noted));
  ACR_TestRegress.Complete("stuckmen", 6);
 }
}
