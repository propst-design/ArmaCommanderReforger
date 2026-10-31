// Garrison order: infantry move into buildings and hold the windows and doorways.
//
// How: each building is surveyed once (Survey: floors from downward traces,
// indoors = a ceiling of that building overhead, on the soldier navmesh, and near an outer
// wall with a clear view out). A squad gets one hidden "post" per soldier at the best spots
// of the clicked building, spilling into neighbours within SPILL_RADIUS. A post is a
// server-local entity carrying the game's sentinel smart action (Prefabs/ACR/
// ACR_GarrisonPost_*.et, crouched or standing by window height, looking out). The order
// itself is a Defend waypoint (ACR_Waypoint_Garrison.et) whose preset searches the tag
// "ACRGarrison"; the modded SCR_AIFindSmartActions below hands the group exactly its own
// posts, and the game's defend activity sends one soldier to each. Soldiers without a post
// take the usual sector cover round the waypoint.
class ACR_GarrisonSpot : Managed
{
 vector m_Pos;    // floor point
 vector m_Look;   // horizontal unit vector out of the window
 bool m_bStand;   // the view is only clear standing (high sill)
 bool m_bInterior; // no window: a place inside for soldiers the windows can't take
 float m_fScore;
}

class ACR_Garrison
{
 static const ResourceName POST_CROUCH = "{AC230926A00100A0}Prefabs/ACR/ACR_GarrisonPost_Crouch.et";
 static const ResourceName POST_STAND = "{AC230926A00100A1}Prefabs/ACR/ACR_GarrisonPost_Stand.et";
 static const string TAG = "ACRGarrison";
 static float SPILL_RADIUS = 40;      // neighbours taken when one building isn't enough
 static float PICK_RADIUS = 60;       // buildings looked at round the clicked point
 static const float SPACING = 2.2;    // between posts (any squad's)
 static const float INTERIOR_SPACING = 3.0; // between places inside (no window)
 static const float GRID = 1.25;      // survey step
 static const float OPENING = 1.6;    // a post is this close to the outside
 static const float VIEW = 8;         // clear view out through the opening

 protected static ref map<SCR_AIGroup, ref array<IEntity>> s_Posts = new map<SCR_AIGroup, ref array<IEntity>>();
 protected static ref map<IEntity, ref array<ref ACR_GarrisonSpot>> s_Survey = new map<IEntity, ref array<ref ACR_GarrisonSpot>>();
 protected static ref array<IEntity> s_aFound = {};
 protected static IEntity s_Building; // the building a ceiling trace must hit
 static int s_iNoHead, s_iNoRoof, s_iNoMesh, s_iNoView, s_iNoWall; // survey rejections (log)
 static bool s_bDebugViews; // tests: print what blocks views
 protected static int s_iDebugViews;
 protected static IEntity s_DebugHit;
 protected static float s_fDebugDist;

 // ---------------------------------------------------------------- posts per group

 static bool HasPosts(SCR_AIGroup group)
 {
  if (!group) return false;
  array<IEntity> posts = s_Posts.Get(group);
  return posts && !posts.IsEmpty();
 }

 static void PostsOf(SCR_AIGroup group, notnull array<AISmartActionComponent> result)
 {
  result.Clear();
  if (!group) return;
  array<IEntity> posts = s_Posts.Get(group);
  if (!posts) return;
  foreach (IEntity post : posts)
  {
   if (!post) continue;
   AISmartActionComponent action = AISmartActionComponent.Cast(post.FindComponent(AISmartActionComponent));
   if (action) result.Insert(action);
  }
 }

 static void PostEntities(SCR_AIGroup group, notnull array<IEntity> result)
 {
  result.Clear();
  if (!group) return;
  array<IEntity> posts = s_Posts.Get(group);
  if (posts) result.Copy(posts);
 }

 // Every post the group holds goes (new order, squad gone). Deleted a moment later so the
 // soldiers' smart-action behaviours end first.
 static void Release(SCR_AIGroup group)
 {
  if (!group) return;
  array<IEntity> posts = s_Posts.Get(group);
  if (!posts) return;
  s_Posts.Remove(group);
  group.ReleaseSmartActions();
  foreach (IEntity post : posts)
  {
   if (post) GetGame().GetCallqueue().CallLater(DeletePost, 3000, false, post);
  }
  PrintFormat("[ACR] GARRISON %1: %2 posts released", group.GetCustomName(), posts.Count());
 }

 protected static void DeletePost(IEntity post)
 {
  if (post) SCR_EntityHelper.DeleteEntityAndChildren(post);
 }

 // Posts of groups that are gone (every second from the manager).
 static void Sweep()
 {
  for (int i = s_Posts.Count() - 1; i >= 0; i--)
  {
   SCR_AIGroup group = s_Posts.GetKey(i);
   if (group && group.GetAgentsCount() > 0)
   {
    WatchPosts(group, s_Posts.GetElement(i));
    continue;
   }
   array<IEntity> posts = s_Posts.GetElement(i);
   s_Posts.RemoveElement(i);
   if (!posts) continue;
   foreach (IEntity post : posts) DeletePost(post);
  }
 }

 static int PostCount()
 {
  int n;
  foreach (SCR_AIGroup group, array<IEntity> posts : s_Posts) n += posts.Count();
  return n;
 }

 // ---------------------------------------------------------------- assigning

 // Posts for 'wanted' soldiers of 'group' in the building at/near 'point' and its
 // neighbours. Returns how many were placed; center/radius describe the waypoint that
 // covers them. 0 = no usable building there; -1 = the navmesh there is still loading
 // (only with waitForMesh: try again in a second).
 static int Assign(SCR_AIGroup group, IEntity navAgent, vector point, int wanted, out vector center, out float radius, bool waitForMesh = true)
 {
  if (!group) return 0;
  Release(group);
  s_Replans.Remove(group);
  point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
  array<IEntity> buildings = {};
  BuildingsNear(point, buildings);
  if (buildings.IsEmpty()) return 0;
  IEntity first = buildings[0];
  array<ref ACR_GarrisonSpot> chosen = {};
  if (!PickSpots(buildings, navAgent, wanted, waitForMesh, SPILL_RADIUS, chosen)) return -1;
  if (chosen.IsEmpty()) return 0;
  array<IEntity> posts = {};
  foreach (ACR_GarrisonSpot spot : chosen)
  {
   IEntity post = SpawnPost(spot);
   if (post) posts.Insert(post);
  }
  if (posts.IsEmpty()) return 0;
  s_Posts.Set(group, posts);
  // The waypoint goes on a post (on the navmesh, ground floor, nearest the building's
  // middle), not the building's origin: a group that can't walk to its waypoint gets
  // it completed by the game and goes Idle (4 Oct live: the office at Barracks, half the
  // squad in the hallway, the rest milling about outside).
  vector middle = first.GetOrigin();
  float ground = GetGame().GetWorld().GetSurfaceY(middle[0], middle[2]);
  float best = float.MAX;
  foreach (IEntity candidate : posts)
  {
   vector at = candidate.GetOrigin();
   float cost = vector.DistanceXZ(at, middle) + 3 * Math.AbsFloat(at[1] - ground);
   if (cost < best) { best = cost; center = at; }
  }
  // Arrival radius; once the soldiers are going to their posts WatchPosts tightens it
  // (TightRadius): the game's combat moves stay inside a defend waypoint's radius and
  // head back in from outside it, so that keeps the garrison in its buildings in a fight.
  radius = 15;
  float tight = 6;
  foreach (IEntity post : posts)
  {
   radius = Math.Max(radius, vector.DistanceXZ(center, post.GetOrigin()) + 5);
   tight = Math.Max(tight, vector.DistanceXZ(center, post.GetOrigin()) + 2);
  }
  s_Tight.Set(group, tight);
  PrintFormat("[ACR] GARRISON %1: %2 posts for %3 soldiers in %4 (+neighbours), radius %5", group.GetCustomName(), posts.Count(), wanted, Describe(first), radius);
  return posts.Count();
 }

 // Posts at the given spots for a group (another order's own posts: an ambush's, ACR_Ambush), each
 // crouched and looking along its m_Look, registered as a garrison's so the game's defend activity
 // sends one soldier to each under a garrison waypoint at 'center'. The waypoint's radius for them.
 static int PostsAt(SCR_AIGroup group, notnull array<ref ACR_GarrisonSpot> spots, vector center, out float radius)
 {
  radius = 15;
  if (!group) return 0;
  Release(group);
  s_Replans.Remove(group);
  array<IEntity> posts = {};
  foreach (ACR_GarrisonSpot spot : spots)
  {
   IEntity post = SpawnPost(spot);
   if (post) posts.Insert(post);
  }
  if (posts.IsEmpty()) return 0;
  s_Posts.Set(group, posts);
  float tight = 6;
  foreach (IEntity placed : posts)
  {
   radius = Math.Max(radius, vector.DistanceXZ(center, placed.GetOrigin()) + 5);
   tight = Math.Max(tight, vector.DistanceXZ(center, placed.GetOrigin()) + 2);
  }
  s_Tight.Set(group, tight);
  return posts.Count();
 }

 // The best free spots, the first building first, then neighbours within SPILL_RADIUS of
 // it. false = a building's navmesh is still loading (waitForMesh).
 protected static bool PickSpots(array<IEntity> buildings, IEntity navAgent, int wanted, bool waitForMesh, float spill, notnull array<ref ACR_GarrisonSpot> chosen)
 {
  IEntity first = buildings[0];
  array<vector> taken = {};
  foreach (SCR_AIGroup other, array<IEntity> otherPosts : s_Posts)
  {
   foreach (IEntity p : otherPosts) if (p) taken.Insert(p.GetOrigin());
  }
  foreach (IEntity building : buildings)
  {
   if (chosen.Count() >= wanted) break;
   if (building != first && vector.DistanceXZ(building.GetOrigin(), first.GetOrigin()) > spill) continue;
   array<ref ACR_GarrisonSpot> spots = Survey(building, navAgent, waitForMesh);
   if (!spots) return false;
   foreach (ACR_GarrisonSpot spot : spots)
   {
    if (chosen.Count() >= wanted) break;
    float spacing = SPACING;
    if (spot.m_bInterior) spacing = INTERIOR_SPACING;
    if (TooClose(spot.m_Pos, taken, spacing) || IsBad(spot.m_Pos)) continue;
    chosen.Insert(spot);
    taken.Insert(spot.m_Pos);
   }
  }
  return true;
 }

 // ---------------------------------------------------------------- unreachable posts
 // The navmesh check can't tell a spot nobody can walk to (a tower top reached by
 // ladder, a room behind a wall). Once the squad is there (leader inside the waypoint
 // radius, the game's defend activity running or finished), a post that stays unclaimed,
 // or whose soldier stops making headway short of it, for STUCK_LIMIT s is dropped and
 // never offered again, a replacement is placed, and the order is issued again so the
 // game hands out the posts afresh.
 static int STUCK_LIMIT = 18;
 static const int MAX_REPLANS = 6;
 protected static ref map<IEntity, int> s_Stuck = new map<IEntity, int>();
 protected static ref map<IEntity, vector> s_LastPos = new map<IEntity, vector>();
 protected static ref map<SCR_AIGroup, int> s_Replans = new map<SCR_AIGroup, int>();
 protected static ref map<SCR_AIGroup, float> s_Tight = new map<SCR_AIGroup, float>();
 protected static ref array<vector> s_aBad = {};
 static int s_iDropped; // tests

 protected static bool IsBad(vector at)
 {
  foreach (vector bad : s_aBad)
  {
   if (vector.DistanceXZ(at, bad) < 1.0 && Math.AbsFloat(at[1] - bad[1]) < 1.5) return true;
  }
  return false;
 }

 static bool PathEndsShort(AIAgent agent, vector goal)
 {
  AIBaseMovementComponent movement = agent.GetMovementComponent();
  if (!movement) return false;
  array<vector> path = {};
  movement.GetCurrentPath(path);
  if (path.IsEmpty()) return false;
  vector end = path[path.Count() - 1];
  return vector.DistanceXZ(end, goal) > 1.5 || Math.AbsFloat(end[1] - goal[1]) > 1.5;
 }

 protected static void WatchPosts(SCR_AIGroup group, array<IEntity> posts)
 {
  SCR_DefendWaypoint waypoint = SCR_DefendWaypoint.Cast(group.GetCurrentWaypoint());
  if (!waypoint || !waypoint.GetPrefabData() || !waypoint.GetPrefabData().GetPrefabName().Contains("ACR_Waypoint_Garrison")) return;
  AIAgent leader = group.GetLeaderAgent();
  if (!leader || !leader.GetControlledEntity()) return;
  if (vector.DistanceXZ(leader.GetControlledEntity().GetOrigin(), waypoint.GetOrigin()) > waypoint.GetCompletionRadius() + 10) return;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  if (!utility) return;
  AIActionBase activity = utility.GetCurrentAction();
  if (SCR_AIMoveActivity.Cast(activity)) { TakeInStragglers(group, waypoint); return; }
  if (!SCR_AIDefendActivity.Cast(activity) && !SCR_AIIdleActivity.Cast(activity)) return; // still on the way, or fighting
  // In a fight soldiers leave their posts to shoot (the game's attack outranks them):
  // that's no sign a post can't be reached, so nothing is counted then.
  if (utility.m_Perception)
  {
   foreach (IEntity enemy : utility.m_Perception.m_aTargetEntities)
   {
    if (enemy && ACR_Respawn.IsAlive(enemy) && vector.DistanceXZ(enemy.GetOrigin(), waypoint.GetOrigin()) < 400)
    {
     foreach (IEntity calm : posts) { s_Stuck.Remove(calm); s_LastPos.Remove(calm); }
     return;
    }
   }
  }
  float tight;
  if (SCR_AIDefendActivity.Cast(activity) && s_Tight.Find(group, tight) && waypoint.GetCompletionRadius() > tight)
  {
   waypoint.SetCompletionRadius(tight);
   PrintFormat("[ACR] GARRISON %1: in position, holding within %2 m", group.GetCustomName(), tight);
  }
  array<IEntity> dropped = {};
  foreach (IEntity post : posts)
  {
   if (!post) continue;
   AISmartActionComponent action = AISmartActionComponent.Cast(post.FindComponent(AISmartActionComponent));
   if (!action) continue;
   int stuck;
   s_Stuck.Find(post, stuck);
   AIAgent user = action.GetUser();
   if (!user || !user.GetControlledEntity())
   {
    if (action.IsActionAccessible()) stuck++;
    else stuck = 0; // reserved: someone is setting off for it
    s_LastPos.Remove(post);
   }
   else
   {
    // short of the post and standing still (a stair climb winds about, so headway
    // is judged by the soldier moving at all)
    // Short of the post and standing still (a stair climb winds about, so headway is
    // judged by the soldier moving at all). Standing still on a path that ends short of
    // the post (the pathfinder knows there is no way there) counts 3x.
    vector at = user.GetControlledEntity().GetOrigin();
    float gap = vector.Distance(at, post.GetOrigin());
    vector was;
    bool known = s_LastPos.Find(post, was);
    if (gap > 1.5 && known && vector.Distance(at, was) < 0.25)
    {
     stuck++;
     if (PathEndsShort(user, post.GetOrigin())) stuck += 2;
    }
    else stuck = 0;
    s_LastPos.Set(post, at);
   }
   s_Stuck.Set(post, stuck);
   if (stuck >= STUCK_LIMIT) dropped.Insert(post);
  }
  if (dropped.IsEmpty()) return;
  int replans;
  s_Replans.Find(group, replans);
  if (replans >= MAX_REPLANS) return;
  s_Replans.Set(group, replans + 1);
  array<AIAgent> stranded = {}; // soldiers whose post was dropped get the replacements first
  foreach (IEntity post : dropped)
  {
   AISmartActionComponent droppedAction = AISmartActionComponent.Cast(post.FindComponent(AISmartActionComponent));
   if (droppedAction && droppedAction.GetUser()) stranded.Insert(droppedAction.GetUser());
   s_aBad.Insert(post.GetOrigin());
   s_Stuck.Remove(post);
   s_LastPos.Remove(post);
   posts.RemoveItem(post);
   DeletePost(post);
   s_iDropped++;
  }
  array<IEntity> buildings = {};
  BuildingsNear(waypoint.GetOrigin(), buildings);
  array<ref ACR_GarrisonSpot> chosen = {};
  if (!buildings.IsEmpty()) PickSpots(buildings, leader, dropped.Count(), false, SPILL_RADIUS * 2, chosen);
  int handed;
  foreach (ACR_GarrisonSpot spot : chosen)
  {
   IEntity replacement = SpawnPost(spot);
   if (!replacement) continue;
   posts.Insert(replacement);
   AIAgent preferred;
   if (!stranded.IsEmpty())
   {
    preferred = stranded[0];
    stranded.RemoveOrdered(0);
   }
   if (HandOut(group, waypoint, utility, replacement, preferred)) handed++;
  }
  PrintFormat("[ACR] GARRISON %1: %2 posts nobody could reach dropped, %3 replacements (%4 handed out)", group.GetCustomName(), dropped.Count(), chosen.Count(), handed);
  // The defend activity is over (everyone's post failed at once): start it afresh.
  if (!SCR_AIDefendActivity.Cast(activity)) waypoint.GetOnWaypointPropertiesChanged().Invoke();
 }

 // The game hands out the posts only once every soldier is inside the waypoint's radius: one man
 // jammed on the way (a fence, a doorway) kept the whole squad milling about outside with no posts
 // (srv:garrisonoffice, 30 Oct: one man 25 m out for 40 s, 3 of 9 on posts). A man outside it who
 // hasn't moved for STRAGGLER_LIMIT s is taken in by widening the radius to him; WatchPosts
 // tightens it again once the defend activity runs.
 static const int STRAGGLER_LIMIT = 8;
 static const float STRAGGLER_REACH = 60;
 protected static ref map<IEntity, vector> s_StragglerAt = new map<IEntity, vector>();
 protected static ref map<IEntity, int> s_StragglerFor = new map<IEntity, int>();
 static int s_iWidened; // tests

 protected static void TakeInStragglers(SCR_AIGroup group, SCR_DefendWaypoint waypoint)
 {
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  float radius = waypoint.GetCompletionRadius();
  float wanted = radius;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   float gap = vector.DistanceXZ(body.GetOrigin(), waypoint.GetOrigin());
   if (gap <= radius)
   {
    s_StragglerAt.Remove(body);
    s_StragglerFor.Remove(body);
    continue;
   }
   vector was;
   int still;
   if (s_StragglerAt.Find(body, was) && vector.Distance(body.GetOrigin(), was) < 0.5)
   {
    s_StragglerFor.Find(body, still);
    still++;
   }
   else s_StragglerAt.Set(body, body.GetOrigin());
   s_StragglerFor.Set(body, still);
   if (still >= STRAGGLER_LIMIT && gap < STRAGGLER_REACH) wanted = Math.Max(wanted, gap + 2);
  }
  if (wanted <= radius) return;
  waypoint.SetCompletionRadius(wanted);
  s_iWidened++;
  PrintFormat("[ACR] GARRISON %1: a man stuck %2 m out, radius widened to take him in", group.GetCustomName(), Math.Round(wanted - 2));
 }

 // A new post to one soldier who has none, the way the game's defend activity hands
 // them out (so the others keep theirs).
 protected static bool HandOut(SCR_AIGroup group, SCR_DefendWaypoint waypoint, SCR_AIGroupUtilityComponent utility, IEntity post, AIAgent preferred = null)
 {
  SCR_AIDefendActivity activity = SCR_AIDefendActivity.Cast(utility.GetCurrentAction());
  SCR_AISmartActionComponent action = SCR_AISmartActionComponent.Cast(post.FindComponent(SCR_AISmartActionComponent));
  if (!activity || !action || !utility.m_Mailbox) return false;
  array<IEntity> held = {};
  PostEntities(group, held);
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  AIAgent free = preferred;
  if (free && !free.GetControlledEntity()) free = null;
  for (int i = 0; i < agents.Count() && !free; i++)
  {
   AIAgent agent = agents[i];
   if (!agent || !agent.GetControlledEntity()) continue;
   bool busy;
   foreach (IEntity other : held)
   {
    AISmartActionComponent otherAction = AISmartActionComponent.Cast(other.FindComponent(AISmartActionComponent));
    if (otherAction && otherAction.GetUser() == agent) { busy = true; break; }
   }
   SCR_ChimeraAIAgent chimera = SCR_ChimeraAIAgent.Cast(agent);
   if (chimera && chimera.m_UtilityComponent && SCR_AIPerformActionBehavior.Cast(chimera.m_UtilityComponent.GetCurrentBehavior())) busy = true;
   if (!busy) free = agent;
  }
  if (!free) return false;
  action.ReserveAction(free);
  SCR_AIMessage_PerformAction message = SCR_AIMessage_PerformAction.Create(null, "", action, true, waypoint.GetPriorityLevel(), waypoint, activity);
  utility.m_Mailbox.RequestBroadcast(message, free);
  return true;
 }

 protected static bool TooClose(vector at, array<vector> taken, float spacing = SPACING)
 {
  foreach (vector t : taken)
  {
   if (vector.DistanceXZ(at, t) < spacing && Math.AbsFloat(at[1] - t[1]) < 2) return true;
  }
  return false;
 }

 protected static IEntity SpawnPost(ACR_GarrisonSpot spot)
 {
  ResourceName prefab = POST_CROUCH;
  if (spot.m_bStand) prefab = POST_STAND;
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid())
  {
   PrintFormat("[ACR] GARRISON post prefab missing: %1", prefab);
   return null;
  }
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  vector angles = spot.m_Look.VectorToAngles();
  angles[1] = 0;
  angles[2] = 0;
  Math3D.AnglesToMatrix(angles, params.Transform);
  params.Transform[3] = spot.m_Pos;
  return GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), params);
 }

 static string Describe(IEntity building)
 {
  if (!building) return "nothing";
  string name = building.ClassName();
  if (building.GetPrefabData())
  {
   name = building.GetPrefabData().GetPrefabName();
   int slash = name.LastIndexOf("/");
   if (slash >= 0) name = name.Substring(slash + 1, name.Length() - slash - 1);
  }
  return string.Format("%1 at %2", name, building.GetOrigin());
 }

 // ---------------------------------------------------------------- bases

 // A base's own garrison: posts in the buildings within 'area' of the centre, nearest
 // first (no spill limit). Surveys without waiting for the navmesh (Prepare loads it
 // while enemies approach). Same returns as Assign.
 static int AssignArea(SCR_AIGroup group, IEntity navAgent, vector center, float area, int wanted)
 {
  if (!group) return 0;
  Release(group);
  s_Replans.Remove(group);
  array<IEntity> buildings = {};
  BuildingsWithin(center, area, buildings);
  if (buildings.IsEmpty()) return 0;
  array<ref ACR_GarrisonSpot> chosen = {};
  PickSpots(buildings, navAgent, wanted, false, area * 2, chosen);
  array<IEntity> posts = {};
  foreach (ACR_GarrisonSpot spot : chosen)
  {
   IEntity post = SpawnPost(spot);
   if (post) posts.Insert(post);
  }
  if (posts.IsEmpty()) return 0;
  s_Posts.Set(group, posts);
  PrintFormat("[ACR] GARRISON %1: %2 posts for %3 soldiers in %4 buildings within %5 m", group.GetCustomName(), posts.Count(), wanted, buildings.Count(), area);
  return posts.Count();
 }

 // Enemies approaching a base: load its navmesh and survey its buildings ahead of the
 // garrison deploying (cheap once cached).
 static void Prepare(vector center, float area)
 {
  IEntity agent = AnyAgent();
  if (!agent) return;
  AIPathfindingComponent pathing = AIPathfindingComponent.Cast(agent.FindComponent(AIPathfindingComponent));
  if (!pathing || !pathing.GetNavmeshComponent()) return;
  array<IEntity> buildings = {};
  BuildingsWithin(center, area, buildings);
  foreach (IEntity building : buildings)
  {
   if (s_Survey.Contains(building)) continue;
   Survey(building, agent, true); // loads the tile first; surveys once it is in
  }
 }

 // The usable building nearest the point within 'area' (where AI squads garrison a base).
 static bool AnchorNear(vector point, float area, out vector at)
 {
  array<IEntity> buildings = {};
  BuildingsWithin(point, area, buildings);
  if (buildings.IsEmpty()) return false;
  at = buildings[0].GetOrigin();
  return true;
 }

 // Any AI agent: the navmesh is the world's, any soldier's pathfinding reads it.
 static IEntity AnyAgent()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return null;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad && squad.m_Group && squad.m_Group.GetLeaderAgent()) return squad.m_Group.GetLeaderAgent();
  }
  return null;
 }

 // ---------------------------------------------------------------- buildings

 // Usable buildings within 'area' of the point, nearest (box distance) first.
 static void BuildingsWithin(vector point, float area, notnull array<IEntity> result)
 {
  s_aFound.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(point, area + 15, CollectBuilding, null, EQueryEntitiesFlags.STATIC);
  array<IEntity> found = {};
  found.Copy(s_aFound);
  s_aFound.Clear();
  while (!found.IsEmpty())
  {
   int pick = -1;
   float nearest = float.MAX;
   foreach (int i, IEntity b : found)
   {
    float d = DistanceToBox(b, point);
    if (d < nearest) { nearest = d; pick = i; }
   }
   if (nearest > area) break;
   result.Insert(found[pick]);
   found.RemoveOrdered(pick);
  }
 }

 // Intact buildings round the point, the one the point is in (or nearest to) first,
 // then by distance from that one.
 static void BuildingsNear(vector point, notnull array<IEntity> result)
 {
  s_aFound.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(point, PICK_RADIUS, CollectBuilding, null, EQueryEntitiesFlags.STATIC);
  array<IEntity> found = {};
  found.Copy(s_aFound);
  s_aFound.Clear();
  if (found.IsEmpty()) return;
  IEntity first;
  float best = float.MAX;
  foreach (IEntity b : found)
  {
   float d = DistanceToBox(b, point);
   if (d < best) { best = d; first = b; }
  }
  if (best > 25) return; // clicked in the open
  result.Insert(first);
  found.RemoveItem(first);
  while (!found.IsEmpty())
  {
   int pick = -1;
   float nearest = float.MAX;
   foreach (int i, IEntity b : found)
   {
    float d = vector.DistanceXZ(b.GetOrigin(), first.GetOrigin());
    if (d < nearest) { nearest = d; pick = i; }
   }
   result.Insert(found[pick]);
   found.RemoveOrdered(pick);
  }
 }

 protected static bool CollectBuilding(IEntity e)
 {
  if (!SCR_DestructibleBuildingEntity.Cast(e)) return true;
  if (e.GetParent()) return true;
  vector mins, maxs;
  e.GetBounds(mins, maxs);
  vector size = maxs - mins;
  if (size[0] < 3.5 || size[2] < 3.5 || size[1] < 2.5) return true;
  s_aFound.Insert(e);
  return true;
 }

 // Horizontal distance from a point to the building's box (0 inside).
 static float DistanceToBox(IEntity b, vector point)
 {
  vector mins, maxs;
  b.GetBounds(mins, maxs);
  vector local = b.CoordToLocal(point);
  float dx = Math.Max(0, Math.Max(mins[0] - local[0], local[0] - maxs[0]));
  float dz = Math.Max(0, Math.Max(mins[2] - local[2], local[2] - maxs[2]));
  return Math.Sqrt(dx * dx + dz * dz);
 }

 // ---------------------------------------------------------------- survey

 // Window and doorway spots of one building, best first (cached). null = its navmesh
 // tile is still loading (waitForMesh); without waitForMesh such a building is surveyed
 // without the navmesh check and not cached.
 static array<ref ACR_GarrisonSpot> Survey(IEntity building, IEntity navAgent, bool waitForMesh = true)
 {
  array<ref ACR_GarrisonSpot> spots = s_Survey.Get(building);
  if (spots) return spots;
  AIPathfindingComponent pathing;
  if (navAgent) pathing = AIPathfindingComponent.Cast(navAgent.FindComponent(AIPathfindingComponent));
  bool cache = true;
  NavmeshWorldComponent mesh;
  if (pathing) mesh = pathing.GetNavmeshComponent();
  if (mesh && !mesh.IsTileLoaded(building.GetOrigin()))
  {
   mesh.LoadTileIn(building.GetOrigin());
   if (waitForMesh) return null;
   pathing = null;
   cache = false;
  }
  spots = {};
  float started = System.GetTickCount();
  s_iNoHead = 0; s_iNoRoof = 0; s_iNoMesh = 0; s_iNoView = 0; s_iNoWall = 0; s_iDebugViews = 0;
  int candidates;
  vector mins, maxs;
  building.GetBounds(mins, maxs);
  BaseWorld world = GetGame().GetWorld();
  for (float x = mins[0] + 0.6; x <= maxs[0] - 0.6; x += GRID)
  {
   for (float z = mins[2] + 0.6; z <= maxs[2] - 0.6; z += GRID)
   {
    vector top = building.CoordToParent(Vector(x, maxs[1] + 0.5, z));
    float bottom = building.CoordToParent(Vector(x, mins[1] - 0.5, z))[1];
    bottom = Math.Max(bottom, world.GetSurfaceY(top[0], top[2]) - 0.5);
    vector from = top;
    for (int floor = 0; floor < 8; floor++)
    {
     vector to = from;
     to[1] = bottom;
     if (to[1] >= from[1]) break;
     float hitY;
     if (!TraceDown(from, to, hitY)) break;
     vector standAt = Vector(from[0], hitY, from[2]);
     from[1] = hitY - 0.3;
     candidates++;
     ACR_GarrisonSpot spot = Rate(building, standAt, mins, maxs, pathing);
     if (spot) spots.Insert(spot);
    }
   }
  }
  // best first
  array<ref ACR_GarrisonSpot> sorted = {};
  while (!spots.IsEmpty())
  {
   int bestIndex;
   float bestScore = -1;
   foreach (int i, ACR_GarrisonSpot s : spots)
   {
    if (s.m_fScore > bestScore) { bestScore = s.m_fScore; bestIndex = i; }
   }
   sorted.Insert(spots[bestIndex]);
   spots.RemoveOrdered(bestIndex);
  }
  if (cache) s_Survey.Set(building, sorted);
  int inside;
  foreach (ACR_GarrisonSpot counted : sorted) if (counted.m_bInterior) inside++;
  string why = string.Format("no head room %1, no roof %2, off navmesh %3, not at a wall %4, no view %5", s_iNoHead, s_iNoRoof, s_iNoMesh, s_iNoWall, s_iNoView);
  PrintFormat("[ACR] GARRISON survey %1: %2 spots (%3 inside, no window) of %4 floor points (%5 ms; %6)", Describe(building), sorted.Count(), inside, candidates, System.GetTickCount() - started, why);
  return sorted;
 }

 // A floor point is a post if it is indoors (ceiling of this building overhead), has
 // head room, is on the navmesh, and looks out through a nearby opening.
 protected static ACR_GarrisonSpot Rate(IEntity building, vector at, vector mins, vector maxs, AIPathfindingComponent pathing)
 {
  if (!Clear(at + Vector(0, 0.15, 0), at + Vector(0, 1.85, 0))) { s_iNoHead++; return null; }
  s_Building = building;
  bool roofed = HitsBuilding(at + Vector(0, 1.85, 0), at + Vector(0, 8, 0));
  s_Building = null;
  if (!roofed) { s_iNoRoof++; return null; }
  if (pathing)
  {
   vector onMesh;
   bool found = pathing.GetClosestPositionOnNavmesh(at, Vector(0.6, 1.0, 0.6), onMesh);
   if (!found || vector.DistanceXZ(onMesh, at) > 0.7 || Math.AbsFloat(onMesh[1] - at[1]) > 0.8)
   {
    s_iNoMesh++;
    return null;
   }
  }
  // Directions where the outside (no roof of this building overhead) is within
  // OPENING m: the spot is at an outer wall there. A window or doorway = a clear view
  // through it for VIEW m.
  array<vector> outward = {};
  s_Building = building;
  for (int d = 0; d < 16; d++)
  {
   float angle = d * Math.PI2 / 16;
   vector dir = Vector(Math.Cos(angle), 0, Math.Sin(angle));
   vector probe = at + dir * OPENING;
   if (!HitsBuilding(probe + Vector(0, 1.85, 0), probe + Vector(0, 8, 0))) outward.Insert(dir);
  }
  s_Building = null;
  if (outward.IsEmpty()) { s_iNoWall++; return Interior(building, at); }
  for (int stance = 0; stance < 2; stance++)
  {
   float eye = 1.0;
   if (stance == 1) eye = 1.55;
   vector lookSum;
   int open;
   foreach (vector dir : outward)
   {
    vector start = at + Vector(0, eye, 0);
    if (Clear(start, start + dir * VIEW)) { open++; lookSum = lookSum + dir; }
    else if (s_bDebugViews && s_iDebugViews < 12)
    {
     s_iDebugViews++;
     PrintFormat("[ACR] GARRISON   view from %1 dir %2 eye %3 blocked by %4 at %5 m", at, dir, eye, Describe(s_DebugHit), s_fDebugDist);
    }
   }
   if (open == 0) continue;
   ACR_GarrisonSpot spot = new ACR_GarrisonSpot();
   spot.m_Pos = at;
   spot.m_Look = lookSum.Normalized();
   spot.m_bStand = stance == 1;
   spot.m_fScore = Math.Min(open, 5) + (at[1] - building.GetOrigin()[1]) * 0.1;
   if (spot.m_bStand) spot.m_fScore -= 0.5;
   return spot;
  }
  s_iNoView++;
  return Interior(building, at);
 }

 // Indoors on the navmesh but no view out: kept as a place inside the building for the
 // soldiers the windows can't take (better than the game's sector cover round the
 // waypoint, which put them outside or into walls: 4 Oct live, "a little dancy dance").
 protected static ACR_GarrisonSpot Interior(IEntity building, vector at)
 {
  ACR_GarrisonSpot spot = new ACR_GarrisonSpot();
  spot.m_Pos = at;
  vector away = at - building.GetOrigin();
  away[1] = 0;
  if (away.Length() < 0.1) away = building.GetTransformAxis(2);
  spot.m_Look = away.Normalized();
  spot.m_bInterior = true;
  spot.m_fScore = 0.1;
  return spot;
 }

 protected static bool TraceDown(vector from, vector to, out float hitY)
 {
  TraceParam trace = new TraceParam();
  trace.Start = from;
  trace.End = to;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float fraction = GetGame().GetWorld().TraceMove(trace, SolidFilter);
  if (fraction >= 1) return false;
  hitY = from[1] + (to[1] - from[1]) * fraction;
  return true;
 }

 static bool ClearLine(vector from, vector to) { return Clear(from, to); }

 protected static bool Clear(vector from, vector to)
 {
  TraceParam trace = new TraceParam();
  trace.Start = from;
  trace.End = to;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float fraction = GetGame().GetWorld().TraceMove(trace, SolidFilter);
  if (s_bDebugViews) { s_DebugHit = trace.TraceEnt; s_fDebugDist = vector.Distance(from, to) * fraction; }
  return fraction >= 1;
 }

 protected static bool HitsBuilding(vector from, vector to)
 {
  TraceParam trace = new TraceParam();
  trace.Start = from;
  trace.End = to;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float fraction = GetGame().GetWorld().TraceMove(trace, SolidFilter);
  if (fraction >= 1 || !trace.TraceEnt) return false;
  IEntity root = trace.TraceEnt.GetRootParent();
  return trace.TraceEnt == s_Building || root == s_Building;
 }

 // People, glass, doors and our own posts don't block a view or a floor.
 static bool SolidFilter(notnull IEntity e)
 {
  if (ChimeraCharacter.Cast(e)) return false;
  if (!e.GetPrefabData()) return true;
  string name = e.GetPrefabData().GetPrefabName();
  if (name.Contains("/Windows/") || name.Contains("Glass") || name.Contains("/Doors/") || name.Contains("ACR_GarrisonPost")) return false;
  return true;
 }
}

// The garrison waypoint's smart-action search: a group with posts gets exactly its own,
// and nobody ever searches for our tag (another squad's posts).
modded class SCR_AIFindSmartActions
{
 override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
 {
  array<string> tags = {};
  GetVariableIn(PORT_SA_TAGS, tags);
  bool ours = tags && tags.Contains(ACR_Garrison.TAG);
  if (!ours && !ACR_Garrison.HasPosts(m_groupOwner)) return super.EOnTaskSimulate(owner, dt);
  array<AISmartActionComponent> posts = {};
  ACR_Garrison.PostsOf(m_groupOwner, posts);
  if (!posts.IsEmpty())
  {
   SetVariableOut(PORT_SA_NUMBER, posts.Count());
   m_groupOwner.AllocateSmartActions(posts);
  }
  SetVariableOut(PORT_SA_FOUND, !posts.IsEmpty());
  return ENodeResult.SUCCESS;
 }
}
