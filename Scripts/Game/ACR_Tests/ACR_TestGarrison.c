// -acrGarrisonTest 1 (server-capable, ~2.5 min): the Garrison order. Alpha garrisons the
// nearest building with room for the whole squad; every soldier should reach a post
// (window or doorway) inside it. Then a move order releases and removes the posts.
class ACR_TestGarrison
{
 static ref array<IEntity> s_aBuildings = {};
 static ref array<IEntity> s_aHouses = {};
 static IEntity s_Building;
 static int s_iWanted;
 static int s_iBestOnPost;
 static int s_iInsideUnderFire, s_iAliveUnderFire;
 static bool s_bContact;
 static SCR_AIGroup s_Enemy;
 static SCR_AIGroup s_Group;  // the squad garrisoning
 static bool s_bOffice;       // -acrGarrisonOfficeTest
 static bool s_bStuck;        // -acrGarrisonStuckTest: the office, one soldier frozen on the way in (#128)
 static AIAgent s_Frozen;

 static ResourceName RifleSquadOf(ACR_Manager manager, string key)
 {
  foreach (ACR_RequisitionEntry entry : manager.m_Bases.GetFaction(key).m_aRequisitions)
   if (entry.m_sVehicle.IsEmpty() && entry.m_sName.Contains("Rifle Squad")) return entry.m_sGroup;
  return "";
 }

 static bool Collect(IEntity e)
 {
  if (SCR_DestructibleBuildingEntity.Cast(e) && !e.GetParent()) s_aBuildings.Insert(e);
  return true;
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  if (!alpha || !alpha.GetAnchor()) return;
  if (tick == 3)
  {
   // navmesh tiles near the range, to see how long they take to stream in
   NavmeshWorldComponent mesh = GetGame().GetAIWorld().GetNavmeshWorldComponent("Soldiers");
   vector c = alpha.GetAnchor().GetOrigin();
   PrintFormat("[ACR] GARRISON TEST tile at alpha loaded %1, 150 m north %2", mesh.IsTileLoaded(c), mesh.IsTileLoaded(c + Vector(0, 0, 130)));
   for (int tx = -2; tx <= 2; tx++) for (int tz = -2; tz <= 2; tz++) mesh.LoadTileIn(c + Vector(tx * 60, 0, tz * 60));
  }
  if (tick == 3)
  {
   // survey sample: houses within 1.5 km (tiles requested now, surveyed at tick 4)
   s_aHouses.Clear();
   s_aBuildings.Clear();
   GetGame().GetWorld().QueryEntitiesBySphere(alpha.GetAnchor().GetOrigin(), 1500, Collect, null, EQueryEntitiesFlags.STATIC);
   NavmeshWorldComponent meshH = GetGame().GetAIWorld().GetNavmeshWorldComponent("Soldiers");
   foreach (IEntity hb : s_aBuildings)
   {
    if (s_aHouses.Count() >= 40) break;
    if (!hb.GetPrefabData() || !hb.GetPrefabData().GetPrefabName().Contains("Structures/Houses/")) continue;
    s_aHouses.Insert(hb);
    meshH.LoadTileIn(hb.GetOrigin());
   }
   PrintFormat("[ACR] GARRISON TEST %1 buildings within 1.5 km", s_aBuildings.Count());
   for (int k = 0; k < s_aBuildings.Count() && k < 400; k += 10) PrintFormat("[ACR] GARRISON TEST   %1", ACR_Garrison.Describe(s_aBuildings[k]));
   s_aBuildings.Clear();
  }
  if (tick == 4)
  {
   ACR_Garrison.s_bDebugViews = true;
   int total, withFour;
   foreach (IEntity house : s_aHouses)
   {
    array<ref ACR_GarrisonSpot> houseSpots = ACR_Garrison.Survey(house, h.m_Range.m_Group.GetLeaderAgent());
    if (!houseSpots) continue;
    int n = houseSpots.Count();
    total += n;
    if (n >= 4) withFour++;
   }
   ACR_Garrison.s_bDebugViews = false;
   PrintFormat("[ACR] GARRISON TEST house sample: %1 houses, %2 spots, %3 with room for 4", s_aHouses.Count(), total, withFour);
   NavmeshWorldComponent mesh2 = GetGame().GetAIWorld().GetNavmeshWorldComponent("Soldiers");
   vector c2 = alpha.GetAnchor().GetOrigin();
   PrintFormat("[ACR] GARRISON TEST after 1 s: 150 m north loaded %1, requested %2", mesh2.IsTileLoaded(c2 + Vector(0, 0, 130)), mesh2.IsTileRequested(c2 + Vector(0, 0, 130)));
  }
  // -acrGarrisonOfficeTest: Jacob's 4 Oct case, a 9-man rifle squad and the two-storey
  // office at Barracks (Office_E_01_tutorial, 1370/2350).
  if (tick == 2 && s_bOffice)
  {
   // no enemy base garrisons waking up near the office (Outpost is 170 m off)
   foreach (ACR_Base quiet : ACR_Base.s_All) if (quiet.m_sOwner != "US") quiet.m_iGarrisonLeft = 0;
  }
  // ...and no Soviet squads about: this run measures moving in, not a firefight
  if (tick == 4 && s_bOffice)
  {
   int removed;
   foreach (ACR_Squad red : manager.m_Squads)
   {
    if (!red || !red.GetFaction() || red.GetFaction().GetFactionKey() == "US") continue;
    array<AIAgent> reds = {};
    red.GetAgents(reds);
    foreach (AIAgent r : reds) if (r.GetControlledEntity()) { SCR_EntityHelper.DeleteEntityAndChildren(r.GetControlledEntity()); removed++; }
   }
   PrintFormat("[ACR] GARRISON TEST office: %1 Soviet soldiers removed", removed);
  }
  if (tick == 5 && s_bOffice)
  {
   array<IEntity> near = {};
   ACR_Garrison.BuildingsWithin(Vector(1370, 0, 2350), 30, near);
   if (!near.IsEmpty()) s_Building = near[0];
   ResourceName rifles = RifleSquadOf(manager, "US");
   if (s_Building && rifles != "")
   {
    vector spawnAt = ACR_Manager.Ground(s_Building.GetOrigin() + Vector(-30, 0, 30)); // from the garages, as in the live run
    s_Group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(rifles, spawnAt));
   }
   PrintFormat("[ACR] GARRISON TEST office %1, rifle squad %2", ACR_Garrison.Describe(s_Building), s_Group != null);
   ACR_TestRegress.Check("garrison: the office and a rifle squad", s_Building && s_Group);
   if (s_Building)
   {
    ACR_Garrison.s_bDebugViews = true;
    array<ref ACR_GarrisonSpot> officeSpots = ACR_Garrison.Survey(s_Building, h.m_Range.m_Group.GetLeaderAgent(), false);
    ACR_Garrison.s_bDebugViews = false;
    foreach (ACR_GarrisonSpot os : officeSpots) PrintFormat("[ACR] GARRISON TEST   spot %1 look %2 stand %3 inside %4", os.m_Pos, os.m_Look, os.m_bStand, os.m_bInterior);
   }
   if (!s_Building || !s_Group) ACR_TestRegress.Abort("garrison", "no building or group");
  }
  if (tick == 5 && !s_bOffice)
  {
   s_Group = h.m_Range.m_Group;
   vector from = alpha.GetAnchor().GetOrigin();
   s_iWanted = alpha.CountAlive();
   s_aBuildings.Clear();
   GetGame().GetWorld().QueryEntitiesBySphere(from, 300, Collect, null, EQueryEntitiesFlags.STATIC);
   // nearest first
   array<IEntity> sorted = {};
   while (!s_aBuildings.IsEmpty())
   {
    int pick;
    float best = float.MAX;
    foreach (int i, IEntity b : s_aBuildings)
    {
     float d = vector.DistanceXZ(b.GetOrigin(), from);
     if (d < best) { best = d; pick = i; }
    }
    sorted.Insert(s_aBuildings[pick]);
    s_aBuildings.RemoveOrdered(pick);
   }
   // A house (where the order is meant to be used): the nearest of the surveyed sample
   // with room for the whole squad twice over.
   int surveyed;
   float nearestHouse = float.MAX;
   foreach (IEntity house : s_aHouses)
   {
    array<ref ACR_GarrisonSpot> spots = ACR_Garrison.Survey(house, h.m_Range.m_Group.GetLeaderAgent());
    surveyed++;
    if (!spots || spots.Count() < s_iWanted * 2) continue;
    float d = vector.DistanceXZ(house.GetOrigin(), from);
    if (d < nearestHouse) { nearestHouse = d; s_Building = house; }
   }
   if (s_Building)
   {
    foreach (ACR_GarrisonSpot sp : ACR_Garrison.Survey(s_Building, null)) PrintFormat("[ACR] GARRISON TEST   spot %1 look %2 stand %3 score %4", sp.m_Pos, sp.m_Look, sp.m_bStand, sp.m_fScore);
   }
   if (!s_Building) { PrintFormat("[ACR] GARRISON TEST no building among %1", sorted.Count()); ACR_TestRegress.Check("garrison: a building with room for the squad", false); ACR_TestRegress.Complete("garrison"); return; }
   PrintFormat("[ACR] GARRISON TEST %1 soldiers, %2 buildings in 300 m, surveyed %3, chose %4 (%5 m away)", s_iWanted, sorted.Count(), surveyed, ACR_Garrison.Describe(s_Building), vector.DistanceXZ(from, s_Building.GetOrigin()));
   ACR_TestRegress.Check("garrison: a building with room for the squad", s_Building != null);
   if (!s_Building) { ACR_TestRegress.Abort("garrison", "no building"); return; }
   // the walk there is ordinary movement: start the squad 30 m off to keep the test short
   vector off = s_Building.GetOrigin() + (from - s_Building.GetOrigin()).Normalized() * 30;
   array<AIAgent> movers = {};
   alpha.GetAgents(movers);
   foreach (int m, AIAgent mover : movers)
   {
    BaseGameEntity body = BaseGameEntity.Cast(mover.GetControlledEntity());
    if (!body) continue;
    vector mat[4];
    Math3D.MatrixIdentity4(mat);
    mat[3] = ACR_Manager.Ground(off + Vector(m * 1.5, 0, 0));
    body.Teleport(mat);
   }
   bool placed = alpha.Garrison(s_Building.GetOrigin());
   PrintFormat("[ACR] GARRISON TEST order -> %1, label '%2', state %3", placed, alpha.m_Order, typename.EnumToString(ACR_EOrderState, alpha.OrderState()));
   array<IEntity> posts = {};
   ACR_Garrison.PostEntities(h.m_Range.m_Group, posts);
   ACR_TestRegress.Check("garrison: one post per soldier", placed && posts.Count() == s_iWanted, string.Format("%1 posts, %2 soldiers", posts.Count(), s_iWanted));
   foreach (IEntity post : posts) PrintFormat("[ACR] GARRISON TEST   post %1 facing %2", post.GetOrigin(), post.GetTransformAxis(2));
  }
  if (!s_Building || !s_Group) return;
  ACR_Squad sq = manager.FindSquadByGroup(s_Group);
  if (!sq) return;
  if (s_bOffice && tick == 12)
  {
   s_iWanted = sq.CountAlive();
   bool placedOffice = sq.Garrison(s_Building.GetOrigin());
   array<IEntity> officePosts = {};
   ACR_Garrison.PostEntities(s_Group, officePosts);
   PrintFormat("[ACR] GARRISON TEST office: %1 soldiers, order -> %2, %3 posts", s_iWanted, placedOffice, officePosts.Count());
   ACR_TestRegress.Check("garrison: one post per soldier", officePosts.Count() == s_iWanted, string.Format("%1 posts, %2 soldiers", officePosts.Count(), s_iWanted));
   foreach (IEntity op : officePosts)
   {
    PrintFormat("[ACR] GARRISON TEST   post %1 facing %2 (%3)", op.GetOrigin(), op.GetTransformAxis(2), op.GetPrefabData().GetPrefabName());
   }
  }
  // -acrGarrisonStuckTest: the man farthest from the office stops dead on the way in (as one jammed
  // at a fence does); the game hands out no posts until everyone is inside the waypoint's radius.
  if (s_bStuck && tick == 14)
  {
   array<AIAgent> walkers = {};
   sq.GetAgents(walkers);
   float farthest = -1;
   foreach (AIAgent walker : walkers)
   {
    if (walker == s_Group.GetLeaderAgent() || !walker.GetControlledEntity()) continue;
    float away = vector.DistanceXZ(walker.GetControlledEntity().GetOrigin(), s_Building.GetOrigin());
    if (away > farthest) { farthest = away; s_Frozen = walker; }
   }
   if (s_Frozen) s_Frozen.DeactivateAI();
   PrintFormat("[ACR] GARRISON TEST stuck: froze a man %1 m from the office", Math.Round(farthest));
  }
  if (s_bStuck && tick == 60)
  {
   array<IEntity> handed = {};
   ACR_Garrison.PostEntities(s_Group, handed);
   int taken;
   foreach (IEntity hp : handed)
   {
    AISmartActionComponent ha = AISmartActionComponent.Cast(hp.FindComponent(AISmartActionComponent));
    if (ha && ha.GetUser()) taken++;
   }
   ACR_TestRegress.Check("garrison: a stuck man doesn't hold the posts back", ACR_Garrison.s_iWidened > 0 && taken >= s_iWanted - 1, string.Format("radius widened %1 times, %2 of %3 posts taken", ACR_Garrison.s_iWidened, taken, s_iWanted));
   if (s_Frozen) s_Frozen.ActivateAI();
  }
  // Under fire: a Soviet fire team 140 m out where the first post looks. Do they stay in?
  if (tick == 70)
  {
   array<IEntity> lookPosts = {};
   ACR_Garrison.PostEntities(s_Group, lookPosts);
   // a spot 70-130 m out that a post can see
   vector enemyAt = ACR_Manager.Ground(s_Building.GetOrigin() + Vector(110, 0, 0));
   bool seen;
   foreach (IEntity lp : lookPosts)
   {
    for (int dist = 70; dist <= 130 && !seen; dist += 20)
    {
     for (int turn = -2; turn <= 2 && !seen; turn++)
     {
      vector dir = lp.GetTransformAxis(2);
      float a = turn * 0.25;
      dir = Vector(dir[0] * Math.Cos(a) - dir[2] * Math.Sin(a), 0, dir[0] * Math.Sin(a) + dir[2] * Math.Cos(a));
      float reach = dist;
      vector cand = ACR_Manager.Ground(lp.GetOrigin() + dir * reach);
      if (ACR_Garrison.ClearLine(lp.GetOrigin() + Vector(0, 1.2, 0), cand + Vector(0, 1.5, 0))) { enemyAt = cand; seen = true; }
     }
    }
    if (seen) break;
   }
   PrintFormat("[ACR] GARRISON TEST enemy spot seen from a post: %1", seen);
   SCR_AIGroup enemy = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, "USSR"), enemyAt));
   PrintFormat("[ACR] GARRISON TEST enemy fire team at %1 (%2 m): %3", enemyAt, vector.Distance(enemyAt, s_Building.GetOrigin()), enemy != null);
   s_Enemy = enemy;
  }
  // ...and it walks at the house, so there is a fight to hold through.
  if (tick == 73 && s_Enemy)
  {
   ACR_Squad attackers = manager.FindSquadByGroup(s_Enemy);
   if (attackers) attackers.MoveTo(ACR_Manager.Ground(s_Building.GetOrigin()));
   PrintFormat("[ACR] GARRISON TEST enemy moving in: %1", attackers != null);
  }
  if (tick > 5 && tick <= 125 && tick % 10 == 0)
  {
   array<IEntity> posts = {};
   ACR_Garrison.PostEntities(s_Group, posts);
   array<AIAgent> agents = {};
   sq.GetAgents(agents);
   int onPost, inside, used;
   string postStates = "";
   foreach (IEntity post : posts)
   {
    AISmartActionComponent action = AISmartActionComponent.Cast(post.FindComponent(AISmartActionComponent));
    if (action && action.GetUser()) used++;
    if (action)
    {
     string pathEnd = "-";
     if (action.GetUser()) pathEnd = ACR_Garrison.PathEndsShort(action.GetUser(), post.GetOrigin()).ToString();
     postStates += string.Format(" [y %1 free %2 user %3 short %4]", post.GetOrigin()[1], action.IsActionAccessible(), action.GetUser() != null, pathEnd);
    }
   }
   SCR_AIGroupUtilityComponent gu = SCR_AIGroupUtilityComponent.Cast(s_Group.FindComponent(SCR_AIGroupUtilityComponent));
   string activity = "none";
   if (gu && gu.GetCurrentAction()) activity = gu.GetCurrentAction().ClassName();
   array<AISmartActionComponent> allocated = {};
   s_Group.GetAllocatedSmartActions(allocated);
   array<AIWaypoint> wps = {};
   s_Group.GetWaypoints(wps);
   string current = "none";
   AIWaypoint cw = s_Group.GetCurrentWaypoint();
   if (cw && cw.GetPrefabData()) current = string.Format("%1 r %2 at %3", cw.GetPrefabData().GetPrefabName(), cw.GetCompletionRadius(), cw.GetOrigin());
   PrintFormat("[ACR] GARRISON TEST t=%1: group activity %2, %3 allocated, %4 waypoints, current %5, posts%6", tick, activity, allocated.Count(), wps.Count(), current, postStates);
   string where = "";
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (!body) continue;
    float nearest = float.MAX;
    foreach (IEntity post : posts) nearest = Math.Min(nearest, vector.Distance(body.GetOrigin(), post.GetOrigin()));
    if (nearest < 1.5) onPost++;
    if (ACR_Garrison.DistanceToBox(s_Building, body.GetOrigin()) < 0.5) inside++;
    string doing = "?";
    SCR_ChimeraAIAgent chimera = SCR_ChimeraAIAgent.Cast(agent);
    if (chimera && chimera.m_UtilityComponent && chimera.m_UtilityComponent.GetCurrentBehavior()) doing = chimera.m_UtilityComponent.GetCurrentBehavior().ClassName();
    where += string.Format(" %1 m (%2, y %3)", Math.Round(nearest * 10) / 10, doing, body.GetOrigin()[1]);
   }
   if (tick < 70) s_iBestOnPost = Math.Max(s_iBestOnPost, onPost);
   if (tick == 120)
   {
    // holding = still at the building (inside the tightened waypoint radius + 3 m)
    AIWaypoint held = s_Group.GetCurrentWaypoint();
    s_iInsideUnderFire = 0;
    foreach (AIAgent holder : agents)
    {
     if (held && holder.GetControlledEntity() && vector.DistanceXZ(holder.GetControlledEntity().GetOrigin(), held.GetOrigin()) < held.GetCompletionRadius() + 3) s_iInsideUnderFire++;
    }
    s_iAliveUnderFire = sq.CountAlive();
   }
   if (tick > 70 && sq.KnownEnemies(350) > 0) s_bContact = true;
   float rounds;
   foreach (AIAgent counted : agents) if (counted.GetControlledEntity()) rounds += ACR_Logistics.SoldierMagazines(counted.GetControlledEntity());
   PrintFormat("[ACR] GARRISON TEST t=%1: %2/%3 on a post, %4 inside the first building, %5 posts in use, order '%6', enemies known %7, magazines %8, post distances:%9", tick, onPost, agents.Count(), inside, used, sq.m_Order, sq.KnownEnemies(350), rounds, where);
  }
  if (tick == 126)
  {
   ACR_TestRegress.Check("garrison: soldiers reach their posts", s_iBestOnPost >= Math.Ceil(s_iWanted * 0.75), string.Format("best %1 of %2", s_iBestOnPost, s_iWanted));
   ACR_TestRegress.Check("garrison: label stays Garrisoning", sq.m_Order == "Garrisoning", sq.m_Order);
   if (s_bContact) ACR_TestRegress.Check("garrison: they hold the building in a fight", s_iInsideUnderFire >= Math.Ceil(s_iAliveUnderFire * 0.75), string.Format("%1 of %2 alive still at the building", s_iInsideUnderFire, s_iAliveUnderFire));
   else Print("[ACR] GARRISON TEST no contact with the enemy fire team: hold check skipped");
   sq.MoveTo(ACR_Manager.Ground(sq.m_Home));
   ACR_TestRegress.Check("garrison: a new order releases the posts", !ACR_Garrison.HasPosts(s_Group));
  }
  if (tick == 131)
  {
   ACR_TestRegress.Check("garrison: released posts are deleted", ACR_Garrison.PostCount() == 0, string.Format("%1 left", ACR_Garrison.PostCount()));
   PrintFormat("[ACR] GARRISON TEST %1 unreachable posts were dropped and replaced", ACR_Garrison.s_iDropped);
   // vanilla comparison: a plain Defend replaced by a move (script error check)
   sq.Defend(sq.GetAnchor().GetOrigin());
  }
  if (tick == 146) sq.MoveTo(ACR_Manager.Ground(sq.m_Home));
  if (tick == 150) ACR_TestRegress.Complete("garrison");
 }
}
