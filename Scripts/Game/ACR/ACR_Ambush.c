// Ambush, as part of Defend (#12, Jacob: tie the ambush to the Defend order): a Defend clicked on
// a road (DefendOnRoad) sends the squad to a spot by it to lie in wait there, prone and holding
// its fire, until enemies (on foot or in vehicles) enter the kill zone, come too close
// or fire on it. Then it opens fire and holds the spot. The kill zone is the nearest stretch of
// road (within ROAD_RANGE of the spot), else the ground towards the nearest threat the side
// knows of (ACR_Intel.ThreatPoint). Ordered on the road itself, the road there is the kill zone
// and the squad lies in wait off it (HidingSpot). Its own side sees the kill zone on the map as
// a pale ring. In place, each soldier takes his own spot (Spots): in sight of the kill zone,
// SPACING apart, on both sides of a road where the cover is, a crouched post looking at the kill
// zone (ACR_Garrison's posts under a garrison waypoint: the game's defend activity sends one man to
// each). Dragged (drag to aim, DefendAimed), the posts are where the drag began, facing where it
// ended: a drag to a road ambushes that road, one anywhere else is a Defend on posts facing that way.
enum ACR_EAmbushPhase
{
 MOVING,      // on the way to the spot (returns fire only)
 POSITIONING, // there: each man on his way to his own spot (holding fire)
 WAITING,     // on their spots: prone, holding fire
 SPRUNG       // fired: fights from the spot
}

class ACR_AmbushPlan : Managed
{
 vector m_vSite;     // where the squad lies in wait
 vector m_vKillZone; // the kill zone's centre
 ACR_EAmbushPhase m_ePhase;
 float m_fOrderedAt;
 int m_iAlive;       // living members once in place (a loss springs it)
 bool m_bProne;      // the prone setting is on the group
 string m_sSprung;   // what sprang it
 float m_fSprungAt;
 int m_iAliveAtSpring;
 bool m_bFaded;      // insurgent band (#97 item 9): broke off after springing ambush
 float m_fPositioningAt;
 float m_fSpotsLoggedAt;
 ref array<IEntity> m_aMen = {};    // each man ...
 ref array<vector> m_aSpots = {};   // ... and his spot (Spots)
 ref array<int> m_aSides = {};      // 1 the site's side of the road, -1 across it
 bool m_bDefend;     // a dragged Defend off a road (DefendAimed): posts facing the drag, firing at will
 bool m_bDragged;    // dragged (DefendAimed): the posts stay on the pressed side of the road, by the pressed spot
 float m_fSetTimeout;      // SET_TIMEOUT_MS and POSITION_TIMEOUT_MS for this ambush (jittered:
 float m_fPositionTimeout; // ACR_Variety, so squads don't all give up on the same second)
 float m_fBestDist = 1000000; // the leader's nearest yet to the site on the way (Stalled) ...
 float m_fProgressAt;         // ... since then
 int m_iReissues;             // the way's waypoint issued again this many times
 vector m_vPostsAt;           // the posts' garrison waypoint (Spots) ...
 float m_fPostsRadius;        // ... and its radius (0: none)
 int m_iPostReissues;         // issued again this many times (Tick: the game dropped it)
}

class ACR_Ambush
{
 static const float ON_ROAD = 4;              // m past a road's edge a Defend click counts as on it (DefendOnRoad)
 static const float ROAD_RANGE = 60;          // the spot may be this far from the road it watches
 static const float OFF_ROAD = 20;            // a spot on the road and no better hiding spot: this far off it
 static const float HIDE_MIN = 15;            // a hiding spot this far from the road's edge ...
 static const float HIDE_MAX = 40;            // ... to this far
 static const float EYE = 1.0;                // risen to a crouch to fire: the line of sight from here ...
 static const float TARGET = 1.5;             // ... to a man's chest over the kill zone
 static const float COVER_RADIUS = 6;         // trees, bushes and rocks this close hide the spot
 static const float THREAT_REACH = 40;        // no road: the kill zone this far towards the threat
 static const float KILL_RADIUS = 30;         // enemies seen inside spring it
 static const float CLOSE_RANGE = 25;         // ...as do enemies seen this close to the spot
 static const float SET_RADIUS = 12;          // the leader this close to the spot: in place
 static const float SET_TIMEOUT_MS = 120000;  // in place anyway after this long on the way
 static const float STALL_MS = 15000;         // the leader no closer for this long on the way: stalled (Stalled)
 static const float NEAR_ENOUGH = 50;         // stalled twice this close to the spot: in place
 static const float SEEN_S = 3;               // a sighting (or side report) this fresh counts
 static const float SPACING = 9;              // m between two men's spots at least (the user: wider than 6)
 static const float WALL_RADIUS = 4;          // a wall or rock this close covers the spot (counts double)
 static const float STAGGER = 4;              // m along the road a spot across it keeps from ours
 static const float SPOT_REACH = 2.5;         // a man this close to his spot is on it
 static const float POSITION_TIMEOUT_MS = 45000; // on their spots anyway after this long
 static const float AIM_ROAD = 20;            // m: a dragged Defend ending this close to a road ambushes that road
 static const float AIM_REACH = 150;          // m: a dragged kill zone no farther from the spot than this
 static const float FACE_REACH = 60;          // m: a dragged Defend off a road looks at a point this far out
 static const float DRAG_SPREAD = 30;         // m: a dragged ambush's posts this close to the first one
 static const float PLAIN_SPREAD = 60;        // m: a plain one's, room for both sides (pn12d: 100-170 m, a 154 m waypoint)
 static const float DRAG_STEP = 8;            // m: from the pressed spot towards the road in these steps
 static const float HEIGHT_WEIGHT = 0.6;      // a spot's score for each m above the kill zone (below: less)

 // Defend clicked on a road: that stretch of road is the kill zone, and the squad lies in wait
 // off it (Plan, HidingSpot). False when the click isn't on a road or no ambush can be laid there
 // (a vehicle crew, say; 'why' says which): the caller gives a plain Defend.
 static bool DefendOnRoad(ACR_Squad squad, vector point, out vector site, out vector killZone, out string why)
 {
  vector road, dir;
  float width;
  if (!ACR_Minefields.Snap(point, ROAD_RANGE, road, dir, width) || vector.DistanceXZ(road, point) > width * 0.5 + ON_ROAD)
  {
   why = "not on a road";
   return false;
  }
  if (!CanOrder(squad, why) || !Plan(squad, point, site, killZone, why)) return false;
  if (squad.Ambush(site, killZone)) return true;
  why = "could not lay an ambush there";
  return false;
 }

 // Defend dragged (drag to aim): the squad takes posts where the drag began, facing where it
 // ended. Ended on or by a road (AIM_ROAD), it is an ambush with that road as the kill zone; ended
 // anywhere else, a Defend on posts facing that way, firing at will (ambush false). A start on a
 // road moves off it (HidingSpot). False (with why) for a vehicle crew or a mounted squad, or with
 // no posts to be had: the caller gives a plain Defend.
 static bool DefendAimed(ACR_Squad squad, vector point, vector aim, out vector site, out vector killZone, out bool ambush, out string why)
 {
  if (!CanOrder(squad, why)) return false;
  vector toward = aim - point;
  toward[1] = 0;
  float reach = toward.Length();
  if (reach < 1)
  {
   why = "no facing";
   return false;
  }
  toward.Normalize();
  vector end = aim;
  if (reach > AIM_REACH) end = point + toward * AIM_REACH;
  vector road, dir;
  float width;
  ambush = ACR_Minefields.Snap(end, AIM_ROAD, road, dir, width) && vector.DistanceXZ(road, end) <= width * 0.5 + AIM_ROAD;
  if (ambush) killZone = road;
  else killZone = ACR_Manager.Ground(point + toward * Math.Min(reach, FACE_REACH));
  site = ACR_Manager.Ground(point);
  vector here, hereDir;
  float hereWidth;
  if (ACR_Minefields.Snap(point, ROAD_RANGE, here, hereDir, hereWidth) && vector.DistanceXZ(here, point) < hereWidth * 0.5 + ON_ROAD)
  {
   string hideWhy;
   if (!HidingSpot(squad, here, hereDir, hereWidth, killZone, site, hideWhy)) site = ACR_Manager.Ground(here + PreferredSide(squad, here, hereDir) * (hereWidth * 0.5 + OFF_ROAD));
   PrintFormat("[ACR] AMBUSH %1: dragged from the road, taking posts at %2 (%3)", squad.GetName(), site, hideWhy);
  }
  if (squad.Ambush(site, killZone, !ambush))
  {
   if (squad.m_Ambush) squad.m_Ambush.m_bDragged = true;
   return true;
  }
  why = "could not take up posts there";
  return false;
 }

 static bool CanOrder(ACR_Squad squad, out string refusal)
 {
  refusal = "";
  if (squad.IsVehicle())
  {
   refusal = "a vehicle crew can't lie in ambush: use Defend";
   return false;
  }
  if (squad.IsMounted())
  {
   refusal = "an ambush is laid on foot: Get out first";
   return false;
  }
  return true;
 }

 // The spot and the kill zone for an ambush ordered at point. A point on the road makes the
 // road there the kill zone and puts the squad off it (HidingSpot; with nowhere better, OFF_ROAD
 // beside it on PreferredSide). false (with why) with no road near and no known threat to face.
 static bool Plan(ACR_Squad squad, vector point, out vector site, out vector killZone, out string refusal)
 {
  refusal = "";
  site = point;
  vector road, dir;
  float width;
  if (ACR_Minefields.Snap(point, ROAD_RANGE, road, dir, width))
  {
   killZone = road;
   if (vector.DistanceXZ(point, road) < width * 0.5 + 6)
   {
    string hideWhy;
    if (!HidingSpot(squad, road, dir, width, killZone, site, hideWhy)) site = ACR_Manager.Ground(road + PreferredSide(squad, road, dir) * (width * 0.5 + OFF_ROAD));
    PrintFormat("[ACR] AMBUSH %1: on the road, lying in wait at %2 (%3)", squad.GetName(), site, hideWhy);
   }
   return true;
  }
  string faction;
  if (squad.GetFaction()) faction = squad.GetFaction().GetFactionKey();
  vector threat = ACR_Intel.ThreatPoint(faction, point);
  if (threat == vector.Zero)
  {
   refusal = string.Format("no road within %1 m and no known enemy to face: pick a spot near a road", ROAD_RANGE);
   return false;
  }
  vector toward = threat - point;
  toward[1] = 0;
  float reach = Math.Min(THREAT_REACH, toward.Length());
  toward.Normalize();
  killZone = ACR_Manager.Ground(point + toward * reach);
  return true;
 }

 // Where to lie in wait beside the road at 'road' (direction dir, width): HIDE_MIN to HIDE_MAX
 // off its edge, along it a little either way, PreferredSide first. A place must be off every
 // road, out of water, on the soldiers' navmesh and in sight of the kill zone; trees, bushes and
 // rocks round it and higher ground count for it, and about 20 m out is best (the user saw a
 // squad lie on the road, playtest 25 Oct). false with no such place (detail: what was found).
 static bool HidingSpot(ACR_Squad squad, vector road, vector dir, float width, vector killZone, out vector site, out string detail)
 {
  vector side = PreferredSide(squad, road, dir);
  array<float> sides = {1, -1};
  array<float> outs = {20, 15, 25, 30, 35, 40};
  array<float> alongs = {0, -8, 8};
  float best = -1000;
  bool found;
  int tried, offRoad, onMesh, seen;
  foreach (float s : sides)
  {
   foreach (float o : outs)
   {
    foreach (float a : alongs)
    {
     vector candidate = ACR_Manager.Ground(road + side * (s * (width * 0.5 + o)) + dir * a);
     tried++;
     if (BBB_Terrain.IsWaterAt(candidate) || ACR_Minefields.RoadClearance(candidate) < HIDE_MIN - 3) continue;
     offRoad++;
     if (!OnMesh(squad, candidate)) continue;
     onMesh++;
     if (!SeesKillZone(candidate, killZone)) continue;
     seen++;
     float rise = Math.Clamp(candidate[1] - killZone[1], 0, 4);
     float score = Cover(candidate) + rise * 0.5 - Math.AbsFloat(o - 20) * 0.05;
     if (s > 0) score += 2;
     if (score <= best) continue;
     best = score;
     site = candidate;
     found = true;
    }
   }
  }
  detail = string.Format("%1 places: %2 off the road, %3 on the navmesh, %4 in sight of the kill zone (last blocked by %5)", tried, offRoad, onMesh, seen, s_sBlockedBy);
  return found;
 }

 // The side of the road (a unit vector across it) to lie in wait on: away from the threat the
 // side knows of when that is off to one side of the road, else the side the squad comes from.
 static vector PreferredSide(ACR_Squad squad, vector road, vector dir)
 {
  vector side = Vector(dir[2], 0, -dir[0]);
  string faction;
  if (squad.GetFaction()) faction = squad.GetFaction().GetFactionKey();
  vector threat = ACR_Intel.ThreatPoint(faction, road);
  if (threat != vector.Zero)
  {
   vector toThreat = threat - road;
   toThreat[1] = 0;
   float across = vector.Dot(toThreat, side);
   if (toThreat.Length() > 1 && Math.AbsFloat(across) > toThreat.Length() * 0.3)
   {
    if (across > 0) side = side * -1;
    return side;
   }
  }
  IEntity anchor = squad.GetAnchor();
  if (anchor && vector.Dot(anchor.GetOrigin() - road, side) < 0) side = side * -1;
  return side;
 }

 // On the soldiers' navmesh (a tile not loaded yet is asked for and not held against it).
 static bool OnMesh(ACR_Squad squad, vector at)
 {
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   AIPathfindingComponent pathing = AIPathfindingComponent.Cast(agent.FindComponent(AIPathfindingComponent));
   if (!pathing) continue;
   NavmeshWorldComponent mesh = pathing.GetNavmeshComponent();
   if (!mesh) return true;
   if (!mesh.IsTileLoaded(at))
   {
    mesh.LoadTileIn(at);
    return true;
   }
   vector onMesh;
   return pathing.GetClosestPositionOnNavmesh(at, Vector(1.5, 3, 1.5), onMesh);
  }
  return true;
 }

 // A line of sight from EYE over the spot to TARGET over the kill zone's ground: no ground,
 // tree, building or other thing in between (people and vehicles aside).
 static string s_sBlockedBy = "nothing";
 static bool SeesKillZone(vector spot, vector killZone)
 {
  return SeesKillZoneAt(spot, killZone, EYE);
 }

 static bool SeesKillZoneAt(vector spot, vector killZone, float eye)
 {
  TraceParam trace = new TraceParam();
  trace.Start = ACR_Manager.Ground(spot) + Vector(0, eye, 0);
  trace.End = ACR_Manager.Ground(killZone) + Vector(0, TARGET, 0);
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float hit = GetGame().GetWorld().TraceMove(trace, SightFilter);
  if (hit >= 1) return true;
  s_sBlockedBy = "the ground";
  if (trace.TraceEnt && trace.TraceEnt.GetPrefabData()) s_sBlockedBy = FilePath.StripPath(trace.TraceEnt.GetPrefabData().GetPrefabName());
  else if (trace.TraceEnt) s_sBlockedBy = trace.TraceEnt.ClassName();
  s_sBlockedBy += string.Format(" %1 m out", (int)(hit * vector.Distance(trace.Start, trace.End)));
  return false;
 }

 protected static bool SightFilter(notnull IEntity e)
 {
  return !ChimeraCharacter.Cast(e) && !Vehicle.Cast(e);
 }

 // Cover at the spot (7 at most): trees and bushes within COVER_RADIUS count 1, walls and rocks
 // within WALL_RADIUS count 2 (the user, pn11: a stone wall ran along the far side of the road,
 // and there were rocks, which a man lies behind).
 protected static int s_iCover;
 protected static vector s_vCoverAt;
 static int Cover(vector spot)
 {
  s_iCover = 0;
  s_vCoverAt = spot;
  GetGame().GetWorld().QueryEntitiesBySphere(spot, COVER_RADIUS, CountCover, null, EQueryEntitiesFlags.STATIC);
  return Math.Min(s_iCover, 7);
 }

 protected static bool CountCover(IEntity e)
 {
  if (!e) return true;
  if (Tree.Cast(e) && !e.GetParent())
  {
   s_iCover++;
   return s_iCover < 7;
  }
  if (!e.GetPrefabData()) return true;
  string name = e.GetPrefabData().GetPrefabName();
  name.ToLower();
  if (!name.Contains("/rocks/") && !name.Contains("/walls/") && !name.Contains("wall_")) return true;
  if (vector.DistanceXZ(e.GetOrigin(), s_vCoverAt) <= WALL_RADIUS || name.Contains("/walls/")) s_iCover += 2;
  return s_iCover < 7;
 }

 // defend: a dragged Defend off a road (DefendAimed): the posts without the waiting.
 static ACR_AmbushPlan Start(ACR_Squad squad, vector site, vector killZone, bool defend = false)
 {
  ACR_AmbushPlan plan = new ACR_AmbushPlan();
  plan.m_vSite = site;
  plan.m_vKillZone = killZone;
  plan.m_bDefend = defend;
  string key = string.Format("ambush %1 %2", squad.GetName(), squad.m_iOrderSerial);
  plan.m_fSetTimeout = ACR_Variety.Jitter(SET_TIMEOUT_MS, 0.15, key + " set");
  plan.m_fPositionTimeout = ACR_Variety.Jitter(POSITION_TIMEOUT_MS, 0.15, key + " spots");
  plan.m_ePhase = ACR_EAmbushPhase.MOVING;
  plan.m_fOrderedAt = GetGame().GetWorld().GetWorldTime();
  plan.m_fProgressAt = plan.m_fOrderedAt;
  string what = "kill zone";
  if (defend) what = "a Defend facing";
  PrintFormat("[ACR] AMBUSH %1: moving to %2, %3 %4 (%5 m from the spot)", squad.GetName(), site, what, killZone, (int)vector.DistanceXZ(site, killZone));
  return plan;
 }

 // On the way but the leader no closer for STALL_MS, or the group's waypoint gone (srv:ambushdrag,
 // 27 Oct: ordered again from its ambush posts, a squad stood 30 m off its new site for the whole
 // two minutes, the men following a leader who never set off; the game completes a waypoint when
 // a group move fails, as ACR_Squad's garrison reissue says): the waypoint is issued again, twice;
 // stalled after that within NEAR_ENOUGH, it is in place (each man walks to his own post from there).
 protected static bool Stalled(ACR_Squad squad, ACR_AmbushPlan plan, float now, float dist)
 {
  bool dropped = squad.m_Group && !squad.m_Group.GetCurrentWaypoint();
  if (dist < plan.m_fBestDist - 2)
  {
   plan.m_fBestDist = dist;
   plan.m_fProgressAt = now;
   if (!dropped) return false;
  }
  if (!dropped && now - plan.m_fProgressAt < STALL_MS) return false;
  plan.m_fProgressAt = now;
  if (plan.m_iReissues < 2)
  {
   plan.m_iReissues++;
   squad.IssueWaypoint(ACR_Squad.WP_DEFEND, plan.m_vSite, SET_RADIUS);
   string why = string.Format("no closer for %1 s", (int)(STALL_MS / 1000));
   if (dropped) why = "the game dropped its waypoint";
   PrintFormat("[ACR] AMBUSH %1: %2, %3 m from the spot: issued again (%4/2)", squad.GetName(), why, (int)dist, plan.m_iReissues);
   return false;
  }
  if (dist > NEAR_ENOUGH) return false;
  PrintFormat("[ACR] AMBUSH %1: still stalled %2 m from the spot: in place from here", squad.GetName(), (int)dist);
  return true;
 }

 // The squad's order text for a phase: "Ambush: ..." or, for a dragged Defend, "Defend: ...".
 static string OrderText(ACR_AmbushPlan plan, string phase)
 {
  if (plan && plan.m_bDefend) return "Defend: " + phase;
  return "Ambush: " + phase;
 }

 // Every tactics tick while the squad has an ambush.
 static void Tick(ACR_Squad squad, float now)
 {
  ACR_AmbushPlan plan = squad.m_Ambush;
  if (!plan) return;
  // Regrouping, or the order set aside: the squad moves as usual until it is back on it.
  bool own = squad.m_Order.StartsWith("Ambush") || (plan.m_bDefend && squad.m_Order.StartsWith("Defend:"));
  Apply(squad, plan, own);
  if (!own) return;
  // Insurgent band ambush fade (#97 item 9): after a short fight (60 s) or >=30% losses,
  // break contact and head back to the nearest hideout unless the commander gives a new order.
  if (plan.m_ePhase == ACR_EAmbushPhase.SPRUNG && !plan.m_bFaded && !plan.m_bDefend && squad.GetFaction() && ACR_InsurgentSide.IsInsurgent(squad.GetFaction().GetFactionKey()))
  {
   int alive = squad.CountAlive();
   bool tookLosses = plan.m_iAliveAtSpring > 0 && alive <= Math.Floor(plan.m_iAliveAtSpring * 0.7);
   if (alive > 0 && (now - plan.m_fSprungAt >= 60000 || tookLosses))
   {
    plan.m_bFaded = true;
    IEntity anchor = squad.GetAnchor();
    if (anchor)
    {
     ACR_DefenderPost home = ACR_HideoutHomes.NearestHome(anchor.GetOrigin(), true);
     if (home)
     {
      squad.RouteTo(home.m_vPos, ACR_Squad.WP_MOVE, -1, "Fading to hideout", ACR_EOrderKind.MOVE);
      ACR_Manager mgr = ACR_Manager.GetInstance();
      if (mgr) mgr.Command().Feed(squad.GetFaction().GetFactionKey(), string.Format("%1 fading back to %2", squad.GetName(), home.m_sName), anchor.GetOrigin());
      PrintFormat("[ACR] AMBUSH FADE %1 heading to %2", squad.GetName(), home.m_sName);
     }
    }
   }
   return;
  }
  if (plan.m_ePhase == ACR_EAmbushPhase.MOVING)
  {
   string onTheWay;
   if (!plan.m_bDefend) onTheWay = Trigger(squad, plan, false);
   if (!onTheWay.IsEmpty())
   {
    Spring(squad, plan, onTheWay);
    return;
   }
   IEntity leader = squad.GetLeader();
   bool there = leader && vector.DistanceXZ(leader.GetOrigin(), plan.m_vSite) <= SET_RADIUS;
   bool stalled = !there && leader && Stalled(squad, plan, now, vector.DistanceXZ(leader.GetOrigin(), plan.m_vSite));
   if (!there && !stalled && now - plan.m_fOrderedAt < plan.m_fSetTimeout) return;
   // There: each man to his own spot (pn10b, 26 Oct: the whole squad lay at one site).
   plan.m_ePhase = ACR_EAmbushPhase.POSITIONING;
   plan.m_fPositioningAt = now;
   plan.m_iAlive = squad.CountAlive();
   squad.SetOrder(OrderText(plan, "taking positions"), ACR_EOrderState.DEFEND);
   Spots(squad, plan);
   Apply(squad, plan, true);
   PrintFormat("[ACR] AMBUSH %1: in place (%2), %3 taking their spots", squad.GetName(), there, plan.m_iAlive);
   return;
  }
  if (plan.m_ePhase == ACR_EAmbushPhase.POSITIONING)
  {
   string onSpots;
   if (!plan.m_bDefend) onSpots = Trigger(squad, plan, true);
   if (!onSpots.IsEmpty())
   {
    Spring(squad, plan, onSpots);
    return;
   }
   int on = OnSpots(plan);
   // The game completed the posts' waypoint (srv:ambushroad, 27 Oct: 1 of 4 men reached his spot,
   // the rest fell back into formation behind him): again, as a garrison's is (ACR_Squad).
   if (on < plan.m_aSpots.Count() && plan.m_fPostsRadius > 0 && plan.m_iPostReissues < 2 && squad.m_Group && !squad.m_Group.GetCurrentWaypoint())
   {
    plan.m_iPostReissues++;
    squad.IssueWaypoint(ACR_Squad.WP_GARRISON, plan.m_vPostsAt, plan.m_fPostsRadius);
    PrintFormat("[ACR] AMBUSH %1: the game dropped the posts' waypoint with %2 of %3 on their spots: issued again (%4/2)", squad.GetName(), on, plan.m_aSpots.Count(), plan.m_iPostReissues);
   }
   if (now - plan.m_fSpotsLoggedAt >= 10000)
   {
    plan.m_fSpotsLoggedAt = now;
    LogSpots(squad, plan);
   }
   if (on < plan.m_aSpots.Count() && now - plan.m_fPositioningAt < plan.m_fPositionTimeout) return;
   if (plan.m_bDefend)
   {
    // A dragged Defend doesn't wait to spring: it fights from its posts.
    plan.m_ePhase = ACR_EAmbushPhase.SPRUNG;
    plan.m_sSprung = "a Defend";
    squad.SetOrder("Defend: on its posts", ACR_EOrderState.DEFEND);
    Apply(squad, plan, true);
    PrintFormat("[ACR] AMBUSH %1: %2 of %3 posts taken after %4 s, defending", squad.GetName(), on, plan.m_aSpots.Count(), (int)((now - plan.m_fPositioningAt) / 1000));
    return;
   }
   Hold(plan);
   plan.m_ePhase = ACR_EAmbushPhase.WAITING;
   squad.SetOrder("Ambush: waiting", ACR_EOrderState.DEFEND);
   Apply(squad, plan, true);
   PrintFormat("[ACR] AMBUSH %1: %2 of %3 spots taken after %4 s, waiting", squad.GetName(), on, plan.m_aSpots.Count(), (int)((now - plan.m_fPositioningAt) / 1000));
   return;
  }
  if (plan.m_ePhase != ACR_EAmbushPhase.WAITING) return;
  Watch(plan);
  string why = Trigger(squad, plan, true);
  if (!why.IsEmpty()) Spring(squad, plan, why);
 }

 // What springs the ambush now ("" for nothing): a loss, the group under fire, a hostile its
 // soldiers detect in the kill zone or close by, or one the side reports in the kill zone (a
 // spotter radioing it in; prone in the grass they may not see it themselves). inPlace: the
 // kill zone, the reports and the losses count.
 static string Trigger(ACR_Squad squad, ACR_AmbushPlan plan, bool inPlace)
 {
  if (inPlace && squad.CountAlive() < plan.m_iAlive) return "took a loss";
  if (!squad.m_Group) return "";
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(squad.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  if (utility && utility.m_Perception)
  {
   foreach (SCR_AIGroupTargetCluster cluster : utility.m_Perception.m_aTargetClusters)
    if (cluster.m_State && cluster.m_State.m_iCountEndangering != 0 && cluster.m_State.m_iCountAlive != 0) return "under fire";
  }
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   SCR_AIUtilityComponent unit = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (!unit || !unit.m_PerceptionComponent) continue;
   if (unit.m_ThreatSystem && unit.m_ThreatSystem.GetThreatMeasure() >= 0.66) return "under fire";
   if (!inPlace) continue;
   array<BaseTarget> targets = {};
   unit.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
   array<BaseTarget> detected = {};
   unit.m_PerceptionComponent.GetTargetsList(detected, ETargetCategory.DETECTED);
   targets.InsertAll(detected);
   foreach (BaseTarget target : targets)
   {
    if (!target || target.GetTimeSinceDetected() > SEEN_S || !Hostile(squad, target.GetTargetEntity())) continue;
    vector at = target.GetLastDetectedPosition();
    if (vector.DistanceXZ(at, plan.m_vKillZone) <= KILL_RADIUS) return "enemy in the kill zone";
    if (vector.DistanceXZ(at, plan.m_vSite) <= CLOSE_RANGE) return "enemy too close";
   }
  }
  if (!inPlace || !squad.GetFaction()) return "";
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(squad.GetFaction().GetFactionKey());
  if (!intel) return "";
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_Contact contact : intel.m_Contacts)
   if (now - contact.m_fSeen <= SEEN_S * 1000 && vector.DistanceXZ(contact.m_Pos, plan.m_vKillZone) <= KILL_RADIUS) return "reported in the kill zone";
  return "";
 }

 // A living soldier or vehicle of another side (not a civilian or an empty wreck).
 protected static bool Hostile(ACR_Squad squad, IEntity entity)
 {
  if (!entity || !ACR_Respawn.IsAlive(entity) || !squad.GetFaction()) return false;
  string key = ACR_Intel.FactionKeyOf(entity);
  // Friendly, not equal: allied FIA walking through doesn't spring it.
  return !key.IsEmpty() && !ACR_Insurgency.Friendly(key, squad.GetFaction().GetFactionKey());
 }

 // ---------------------------------------------------------------- each man's spot
 // The user (pn10b, 26 Oct): the squad "should ... pick the best positions possible and also should
 // not be bunched up and should not all be on the same side of the road if possible". Each living
 // AI soldier gets a spot of his own (PickSpots) and a post there (ACR_Garrison.PostsAt).
 static void Spots(ACR_Squad squad, ACR_AmbushPlan plan)
 {
  plan.m_aMen.Clear();
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man && ACR_Respawn.IsAlive(man) && ACR_Squad.FindController(man) == 0) plan.m_aMen.Insert(man);
  }
  string detail;
  PickSpots(squad, plan.m_vSite, plan.m_vKillZone, plan.m_aMen.Count(), plan.m_aSpots, plan.m_aSides, detail, string.Format("%1 %2", squad.GetName(), squad.m_iOrderSerial), plan.m_bDragged);
  int across;
  foreach (int side : plan.m_aSides) if (side < 0) across++;
  PrintFormat("[ACR] AMBUSH %1: spots for %2 men, %3 across the road (%4)", squad.GetName(), plan.m_aMen.Count(), across, detail);
  // A crouched post on each spot, looking at the kill zone, under a garrison waypoint over them all
  // (on the spot nearest their middle): the game's defend activity sends one man to each, as for a
  // garrison. Moves of our own lost to the waypoint's formation and positions within seconds
  // (srv:ambushroad, 27 Oct), as they did in the pn11 playtest.
  if (plan.m_aSpots.IsEmpty()) return;
  vector middle = vector.Zero;
  foreach (vector spot : plan.m_aSpots) middle = middle + spot;
  middle = middle * (1.0 / plan.m_aSpots.Count());
  vector at = plan.m_aSpots[0];
  array<ref ACR_GarrisonSpot> posts = {};
  foreach (vector each : plan.m_aSpots)
  {
   if (vector.DistanceXZ(each, middle) < vector.DistanceXZ(at, middle)) at = each;
   ACR_GarrisonSpot post = new ACR_GarrisonSpot();
   post.m_Pos = each;
   vector look = plan.m_vKillZone - each;
   look[1] = 0;
   look.Normalize();
   post.m_Look = look;
   posts.Insert(post);
  }
  float radius;
  int placed = ACR_Garrison.PostsAt(squad.m_Group, posts, at, radius);
  if (placed > 0)
  {
   squad.IssueWaypoint(ACR_Squad.WP_GARRISON, at, radius);
   plan.m_vPostsAt = at;
   plan.m_fPostsRadius = radius;
  }
  PrintFormat("[ACR] AMBUSH %1: %2 posts, the waypoint at %3, radius %4 m", squad.GetName(), placed, at, (int)radius);
 }

 // Spots with a living man of the squad on them (the defend activity decides who takes which).
 static int OnSpots(ACR_AmbushPlan plan)
 {
  int on;
  foreach (vector spot : plan.m_aSpots)
  {
   foreach (IEntity man : plan.m_aMen)
   {
    if (man && ACR_Respawn.IsAlive(man) && vector.DistanceXZ(man.GetOrigin(), spot) <= SPOT_REACH)
    {
     on++;
     break;
    }
   }
  }
  return on;
 }

 // Where each man is and what he is doing while they take their spots (the pn11 playtest: "0 of 4
 // on their spots after 45 s" for a player's squad, which srv:ambush never showed).
 static void LogSpots(ACR_Squad squad, ACR_AmbushPlan plan)
 {
  foreach (int i, vector spot : plan.m_aSpots)
  {
   IEntity nearest;
   foreach (IEntity man : plan.m_aMen) if (man && ACR_Respawn.IsAlive(man) && (!nearest || vector.DistanceXZ(man.GetOrigin(), spot) < vector.DistanceXZ(nearest.GetOrigin(), spot))) nearest = man;
   if (!nearest) continue;
   string doing = "none";
   SCR_AIUtilityComponent utility = ACR_OrganEasterEgg.Utility(nearest);
   if (utility && utility.GetCurrentBehavior()) doing = utility.GetCurrentBehavior().ClassName();
   PrintFormat("[ACR] AMBUSH %1: spot %2 (side %3): nearest man %4 m off, doing %5 (a group waypoint: %6)", squad.GetName(), i, plan.m_aSides[i], (int)vector.DistanceXZ(nearest.GetOrigin(), spot), doing, squad.m_Group && squad.m_Group.GetCurrentWaypoint());
  }
 }

 // On their spots: the posts keep them there, crouched and looking at the kill zone.
 static void Hold(ACR_AmbushPlan plan)
 {
 }

 // While they wait, each looks at the kill zone (below any enemy he sees: PRIO_UNKNOWN_TARGET).
 static void Watch(ACR_AmbushPlan plan)
 {
  foreach (IEntity man : plan.m_aMen)
  {
   SCR_AIUtilityComponent utility = ACR_OrganEasterEgg.Utility(man);
   if (utility && utility.m_LookAction && ACR_Respawn.IsAlive(man)) utility.m_LookAction.LookAt(plan.m_vKillZone + Vector(0, 1, 0), SCR_AILookAction.PRIO_UNKNOWN_TARGET, 3.0);
  }
 }

 // 'count' spots for an ambush at site watching killZone, best first: in sight of the kill zone
 // (SeesKillZone), on the navmesh, out of water; trees, bushes and rocks round it and higher ground
 // count for it, and each is SPACING from the others. By a road (the kill zone on it): 15-35 m off
 // either edge, along it 24 m either way, and split across both sides where both have spots in
 // sight, the far side's STAGGER along the road from ours (not straight across: our own fire).
 // Elsewhere a line across the line of sight at the site. Short of places, the rest go round the
 // site, SPACING apart. sides: 1 the site's side, -1 across the road.
 // Dynamism (ACR_Variety): each spot is picked among the near-best places, so two ambushes on one road
 // differ; 'key' says which ambush (the squad and its order).
 // A spot's height over the kill zone, for its score: higher ground first, lower ground less
 // (pn12d, 27 Oct: all three men went across the road to its lower side, where they couldn't see it).
 static float Height(vector spot, vector killZone)
 {
  return Math.Clamp(spot[1] - killZone[1], -4, 6) * HEIGHT_WEIGHT;
 }

 // pinned (a dragged ambush): every spot on the pressed side of the road, by the pressed spot (the
 // site), stepping towards the road where nothing there sees the kill zone, never across it, and
 // within DRAG_SPREAD of one another (pn12d: posts 100-170 m apart, the waypoint 154 m wide).
 static int PickSpots(ACR_Squad squad, vector site, vector killZone, int count, notnull array<vector> spots, notnull array<int> sides, out string detail, string key = "", bool pinned = false)
 {
  spots.Clear();
  sides.Clear();
  array<vector> places = {};
  array<float> scores = {};
  array<int> placeSides = {};
  array<float> placeAlongs = {};
  vector road, dir;
  float width;
  bool byRoad = ACR_Minefields.Snap(killZone, 12, road, dir, width) && vector.DistanceXZ(road, killZone) < width * 0.5 + ON_ROAD;
  int tried;
  string pinnedWhy;
  vector pinAway = vector.Zero;
  if (byRoad && pinned)
  {
   vector away = Vector(dir[2], 0, -dir[0]);
   if (vector.Dot(site - road, away) < 0) away = away * -1;
   pinAway = away;
   float siteOff = vector.Dot(site - road, away) - width * 0.5;
   float nearest = HIDE_MIN - 3;
   float reach = Math.Max(siteOff - nearest, 0);
   array<vector> closest = {};
   array<float> closestAlongs = {};
   for (float d = 0; d <= reach + 0.01; d += DRAG_STEP)
   {
    bool lastRow = d + DRAG_STEP > reach + 0.01;
    for (float a = -18; a <= 18; a += 6)
    {
     vector candidate = ACR_Manager.Ground(site + dir * a - away * d);
     tried++;
     if (BBB_Terrain.IsWaterAt(candidate) || ACR_Minefields.RoadClearance(candidate) < nearest || vector.Dot(candidate - road, away) < width * 0.5 || !OnMesh(squad, candidate)) continue;
     if (lastRow)
     {
      closest.Insert(candidate);
      closestAlongs.Insert(a);
     }
     if (!SeesKillZone(candidate, killZone)) continue;
     places.Insert(candidate);
     scores.Insert(Cover(candidate) + Height(candidate, killZone) - d * 0.03 - Math.AbsFloat(a) * 0.02);
     placeSides.Insert(1);
     placeAlongs.Insert(a);
    }
   }
   // Nothing on our side sees the kill zone: the row nearest the road on our side anyway.
   if (places.IsEmpty())
   {
    pinnedWhy = "; none in sight on the pressed side: the row nearest the road there";
    foreach (int c, vector near : closest)
    {
     places.Insert(near);
     scores.Insert(Cover(near) + Height(near, killZone) - Math.AbsFloat(closestAlongs[c]) * 0.02);
     placeSides.Insert(1);
     placeAlongs.Insert(closestAlongs[c]);
    }
   }
  }
  else if (byRoad)
  {
   vector across = Vector(dir[2], 0, -dir[0]);
   if (vector.Dot(site - road, across) < 0) across = across * -1;
   array<float> roadSides = {1, -1};
   foreach (float s : roadSides)
   {
    for (float o = 15; o <= 35; o += 5)
    {
     for (float a = -24; a <= 24; a += 6)
     {
      vector candidate = ACR_Manager.Ground(road + across * (s * (width * 0.5 + o)) + dir * a);
      tried++;
      if (BBB_Terrain.IsWaterAt(candidate) || ACR_Minefields.RoadClearance(candidate) < HIDE_MIN - 3 || !OnMesh(squad, candidate) || !SeesKillZone(candidate, killZone)) continue;
      places.Insert(candidate);
      scores.Insert(Cover(candidate) + Height(candidate, killZone) - Math.AbsFloat(o - 20) * 0.05 - Math.AbsFloat(a) * 0.02);
      placeSides.Insert((int)s);
      placeAlongs.Insert(a);
     }
    }
   }
  }
  else
  {
   vector toward = killZone - site;
   toward[1] = 0;
   toward.Normalize();
   vector line = Vector(toward[2], 0, -toward[0]);
   array<float> depths = {0, -6, 6};
   foreach (float depth : depths)
   {
    for (float b = -24; b <= 24; b += 6)
    {
     vector spot = ACR_Manager.Ground(site + line * b + toward * depth);
     tried++;
     if (BBB_Terrain.IsWaterAt(spot) || ACR_Minefields.RoadClearance(spot) < 3 || !OnMesh(squad, spot) || !SeesKillZone(spot, killZone)) continue;
     places.Insert(spot);
     scores.Insert(Cover(spot) + Height(spot, killZone) - Math.AbsFloat(b) * 0.02);
     placeSides.Insert(1);
     placeAlongs.Insert(b);
    }
   }
  }
  int nearPlaces, farPlaces;
  float nearCover, farCover;
  foreach (int k2, int placeSide : placeSides)
  {
   float worth = Math.Max(scores[k2], 0) + 0.5;
   if (placeSide > 0)
   {
    nearPlaces++;
    nearCover += worth;
   }
   else
   {
    farPlaces++;
    farCover += worth;
   }
  }
  // Across the road by where the cover is (each side's places in sight, by their cover and height),
  // not half and half: a share of the men, rounded at random (ACR_Variety.Share), at least one man
  // on our side.
  int wantFar;
  if (byRoad && farPlaces > 0 && nearPlaces > 0) wantFar = Math.ClampInt(ACR_Variety.Share(count, farCover / (nearCover + farCover), key + " split"), 0, count - 1);
  array<float> ourAlongs = {};
  for (int pick = 0; pick < count; pick++)
  {
   int far = 0;
   foreach (int taken : sides) if (taken < 0) far++;
   int want = 1;
   if (far < wantFar && (pick % 2 == 1 || pick - far >= count - wantFar)) want = -1;
   string pickKey = string.Format("%1 spot %2", key, pick);
   float spread = PLAIN_SPREAD;
   if (pinned) spread = DRAG_SPREAD;
   int best = BestPlace(places, scores, placeSides, placeAlongs, spots, ourAlongs, want, pickKey, spread);
   if (best < 0 && !pinned) best = BestPlace(places, scores, placeSides, placeAlongs, spots, ourAlongs, -want, pickKey, spread);
   if (best < 0) break;
   spots.Insert(places[best]);
   sides.Insert(placeSides[best]);
   if (placeSides[best] > 0) ourAlongs.Insert(placeAlongs[best]);
  }
  // Short of places: round the spots already picked (the site when none), SPACING apart.
  int placed = spots.Count();
  vector round = site;
  if (placed > 0)
  {
   round = vector.Zero;
   foreach (vector p : spots) round = round + p;
   round = round * (1.0 / placed);
  }
  for (int k = 0; spots.Count() < count && k < 32; k++)
  {
   float angle = Math.PI2 * k / 8;
   int ring = 1 + k / 8;
   vector ringSpot = ACR_Manager.Ground(round + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * (SPACING * ring));
   if (BBB_Terrain.IsWaterAt(ringSpot)) continue;
   // Dragged: never across the road, and as close together as the picked ones.
   if (pinned && byRoad && vector.Dot(ringSpot - road, pinAway) < width * 0.5 + 3) continue;
   if (placed > 0 && pinned && vector.DistanceXZ(ringSpot, spots[0]) > DRAG_SPREAD + SPACING) continue;
   bool clear = true;
   foreach (vector other : spots) if (vector.DistanceXZ(other, ringSpot) < SPACING) clear = false;
   if (!clear) continue;
   spots.Insert(ringSpot);
   sides.Insert(1);
  }
  detail = string.Format("%1 places tried, %2 in sight of the kill zone (%3 on our side, %4 across, cover %5 / %6), %7 placed there, %8 round them; by a road %9", tried, places.Count(), nearPlaces, farPlaces, (int)nearCover, (int)farCover, placed, spots.Count() - placed, byRoad);
  if (pinned) detail += ", dragged: the pressed side only" + pinnedWhy;
  return spots.Count();
 }

 // A place on 'side' that is SPACING from every spot taken, picked among the near-best
 // (ACR_Variety); across the road, STAGGER along it from ours scores more. -1 for none.
 protected static int BestPlace(array<vector> places, array<float> scores, array<int> placeSides, array<float> placeAlongs, array<vector> taken, array<float> ourAlongs, int side, string key, float spread = 0)
 {
  array<int> open = {};
  array<float> openScores = {};
  foreach (int i, vector place : places)
  {
   if (placeSides[i] != side) continue;
   bool clear = true;
   foreach (vector spot : taken) if (vector.DistanceXZ(spot, place) < SPACING) clear = false;
   if (spread > 0 && !taken.IsEmpty() && vector.DistanceXZ(taken[0], place) > spread) clear = false;
   if (!clear) continue;
   float score = scores[i];
   if (side < 0)
   {
    foreach (float along : ourAlongs) if (Math.AbsFloat(along - placeAlongs[i]) < STAGGER) score -= 2;
   }
   open.Insert(i);
   openScores.Insert(score);
  }
  int chosen = ACR_Variety.Pick(openScores, key);
  if (chosen < 0) return -1;
  return open[chosen];
 }

 static void Spring(ACR_Squad squad, ACR_AmbushPlan plan, string why)
 {
  plan.m_ePhase = ACR_EAmbushPhase.SPRUNG;
  plan.m_sSprung = why;
  plan.m_fSprungAt = GetGame().GetWorld().GetWorldTime();
  plan.m_iAliveAtSpring = squad.CountAlive();
  squad.SetOrder("Ambush: sprung", ACR_EOrderState.DEFEND);
  Apply(squad, plan, true);
  PrintFormat("[ACR] AMBUSH %1: sprung (%2)", squad.GetName(), why);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !squad.GetFaction()) return;
  ACR_Intel intel = manager.IntelFor(squad.GetFaction().GetFactionKey());
  if (intel) intel.AddFeed(GetGame().GetWorld().GetWorldTime(), string.Format("%1 sprang its ambush (%2)", squad.GetName(), why), plan.m_vKillZone);
 }

 // The combat mode and stance for the phase. own: on its ambush order (not regrouping), else
 // the squad's usual combat mode and no stance limit.
 static void Apply(ACR_Squad squad, ACR_AmbushPlan plan, bool own)
 {
  if (!squad.m_Group) return;
  EAIGroupCombatMode mode = EAIGroupCombatMode.FIRE_AT_WILL;
  if (squad.m_Mode == ACR_EMoveMode.SILENT) mode = EAIGroupCombatMode.RETURN_FIRE;
  bool lying = own && !plan.m_bDefend;
  if (lying && plan.m_ePhase == ACR_EAmbushPhase.MOVING) mode = EAIGroupCombatMode.RETURN_FIRE;
  if (lying && (plan.m_ePhase == ACR_EAmbushPhase.WAITING || plan.m_ePhase == ACR_EAmbushPhase.POSITIONING)) mode = EAIGroupCombatMode.HOLD_FIRE;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(squad.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  if (utility && utility.GetCombatModeExternal() != mode) utility.SetCombatMode(mode);
  // Down (crouched on its posts) while waiting; m_bProne says so.
  plan.m_bProne = own && plan.m_ePhase == ACR_EAmbushPhase.WAITING;
 }

 // A new order ends the ambush: the usual combat mode, no stance limit.
 static void End(ACR_Squad squad)
 {
  ACR_AmbushPlan plan = squad.m_Ambush;
  if (!plan) return;
  if (squad.m_Group) ACR_Garrison.Release(squad.m_Group);
  squad.m_Ambush = null;
  Apply(squad, plan, false);
  PrintFormat("[ACR] AMBUSH %1: ended (%2)", squad.GetName(), typename.EnumToString(ACR_EAmbushPhase, plan.m_ePhase));
 }

 // The side's own ambushes: a pale ring round each kill zone ("~": not a danger area).
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.m_Ambush || squad.m_Ambush.m_bDefend || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction) continue;
   positions.Insert(squad.m_Ambush.m_vKillZone);
   radii.Insert(KILL_RADIUS);
   labels.Insert("~Ambush " + squad.GetName());
  }
 }
}
