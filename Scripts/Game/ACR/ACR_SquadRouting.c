// Squad routing, field-leg pathing, waypoint queues, patrol, and unreachable recovery; extracted from ACR_Squad.c.
class ACR_SquadRouting
{
 // Where the current order ends up: the last leg of a route, the order carried on after a
 // dismount, else the order's own point (ACR_AICommander's trips).
 static vector Heading(ACR_Squad sq)
 {
  if (!sq.m_aRoute.IsEmpty()) return sq.m_aRoute[sq.m_aRoute.Count() - 1];
  if (sq.m_bResumeOnFoot && sq.m_Resume) return sq.m_Resume.m_Target;
  return sq.m_MoveTarget;
 }

 // #86 (29 Oct): one waypoint per vehicle trip is the default, as vanilla's own scenarios drive
 // (each road hop was a waypoint end and a fresh engine path: brake, plan again); -acrNoRoadHops 0
 // brings the road hops back. The hops are the fallback: a failed move (OnGroupMoveFailed) and a
 // stuck vehicle (DriveWatch's Reroute) go on over the roads from where they stand.
 static bool NoRoadHops()
 {
  string value;
  if (System.GetCLIParam("acrNoRoadHops", value)) return value != "0";
  return true;
 }

 // Covered fall-back: on by default from 0.6.0 (#232; off in 0.5.9-0.5.12): -acrCoveredFallBack 0 or the
 // classic commander (ACR_AIRework) turns it off.
 static bool CoveredFallBackOn() { return ACR_LogisticsRules.Choice("acrCoveredFallBack", ACR_AIRework.Default("1")) == "1"; }

 // The order just given is the AI commander's fall-back to this point (call after issuing it).
 static void MarkFallBack(ACR_Squad sq, vector to)
 {
  sq.m_vFallBackTo = to;
  sq.m_iFallBackSerial = sq.m_iOrderSerial;
  sq.m_fFallBackCheckAt = 0;
  sq.m_bFallBackPushed = false;
 }

 static bool FallingBack(ACR_Squad sq, out vector to)
 {
  if (sq.m_iFallBackSerial < 0 || sq.m_iFallBackSerial != sq.m_iOrderSerial) return false;
  to = sq.m_vFallBackTo;
  return true;
 }

 // A covered fall-back that isn't getting back (BBB's bounds stalled, or never started because the
 // squad sees nobody while the enemy still fires): every 30 s in contact, under 10 m gained toward
 // the spot forces the order and the squad runs for it (Jacob, 4 Nov: a screen must not lie prone in
 // the open). True while that run stands: the order is not re-issued at combat level in contact.
 static bool WatchFallBack(ACR_Squad sq, float now, bool contact)
 {
  vector to;
  if (!FallingBack(sq, to) || !BBB_Morale.OrderedOn()) return false;
  if (sq.m_bFallBackPushed) return true;
  IEntity anchor = sq.GetAnchor();
  if (!anchor || !contact || sq.IsMounted() || !sq.m_Waypoint)
  {
   sq.m_fFallBackCheckAt = 0;
   return false;
  }
  float dist = vector.DistanceXZ(anchor.GetOrigin(), to);
  if (sq.m_fFallBackCheckAt <= 0 || now < sq.m_fFallBackCheckAt - ACR_Squad.FALLBACK_CHECK_MS)
  {
   sq.m_fFallBackCheckAt = now + ACR_Squad.FALLBACK_CHECK_MS;
   sq.m_fFallBackDist = dist;
   return false;
  }
  if (now < sq.m_fFallBackCheckAt) return false;
  float gained = sq.m_fFallBackDist - dist;
  sq.m_fFallBackCheckAt = now + ACR_Squad.FALLBACK_CHECK_MS;
  sq.m_fFallBackDist = dist;
  if (gained >= ACR_Squad.FALLBACK_MIN_GAIN_M || dist <= 30) return false;
  sq.m_bFallBackPushed = true;
  sq.IssueWaypoint(sq.m_sOrderPrefab, sq.m_MoveTarget, sq.m_fOrderRadius, true);
  PrintFormat("[ACR] TACTICS %1: falling back under fire, %2 m gained in 30 s: running for it (%3 m to go)", sq.m_Name, Math.Round(gained), Math.Round(dist));
  return true;
 }

 // exactEnd (player orders): road hops on the way, but the last leg is the clicked point.
 static bool RouteTo(ACR_Squad sq, vector destination, ResourceName prefab, float radius, string order, int kind, bool exactEnd = false)
 {
  // Vehicle crews: aim for the nearest road point, which AI drivers can reach.
  vector road;
  vector clicked = destination;
  IEntity driven = ACR_Transport.CrewedVehicle(sq);
  // A roadblock gunner is infantry for this order: the new order takes him off the gun.
  // Planned as a crew, the road point nearest the target can be the roadblock itself.
  bool onPost = driven != null && driven == sq.m_RoadblockTurret;
  if (onPost) driven = null;
  if (driven && BBB_Terrain.RoadPointNear(destination, 250, road)) destination = road;
  IEntity anchor = sq.GetAnchor();
  array<vector> legs = {};
  // ...and on longer trips, along the roads in short hops only with -acrNoRoadHops 0 (by default
  // one waypoint for the whole trip: the game's driver plans it, NoRoadHops).
  if (driven && anchor && vector.DistanceXZ(anchor.GetOrigin(), destination) > 400 && !NoRoadHops())
  {
   int nodes, millis;
   if (BBB_Roads.Route(driven.GetOrigin(), destination, legs, nodes, millis) && legs.Count() > 1)
   {
    PrintFormat("[ACR] ROAD %1: %2 hops over %3 road points (%4 ms)", sq.m_Name, legs.Count(), nodes, millis);
    if (exactEnd && vector.DistanceXZ(legs[legs.Count() - 1], clicked) > 15) legs.Insert(clicked);
    if (System.IsCLIParam("acrRideTest")) foreach (vector hop : legs) PrintFormat("[ACR] ROAD   hop %1", hop);
    StartRoute(sq, legs, prefab, radius, order, kind);
    return true;
   }
   legs.Clear();
  }
  if (exactEnd) destination = clicked;
  bool landRoute;
  if (anchor && (onPost || !sq.IsMounted()))
  {
   int landStarted = System.GetTickCount();
   landRoute = BBB_Terrain.LandRoute(anchor.GetOrigin(), destination, legs);
   ACR_Perf.Lap("route.land", landStarted);
  }
  // AI foot squads outside friendly ground keep off the roads (FieldLegs).
  if (!landRoute && anchor && !driven && !sq.IsMounted() && !sq.PlayerSteers() && FieldLegs(sq, anchor.GetOrigin(), destination, legs))
  {
   PrintFormat("[ACR] ROUTE %1: keeping off the roads outside friendly ground, %2 legs", sq.m_Name, legs.Count());
   StartRoute(sq, legs, prefab, radius, order, kind);
   return true;
  }
  if (!landRoute)
  {
   if (prefab == ACR_Squad.WP_GARRISON) { sq.Garrison(destination); return true; }
   if (prefab == ACR_Squad.WP_DEFEND) return sq.Defend(destination);
   if (prefab == ACR_Squad.WP_SEARCH_DESTROY) return sq.Assault(destination);
   // A crewed vehicle's one-waypoint trip ends within 22 m (vanilla's resupply trucks use the base
   // radius), so it doesn't creep and jockey for the prefab's 5 m at the end.
   if (driven && NoRoadHops()) return sq.MoveTo(destination, 22);
   return sq.MoveTo(destination);
  }
  PrintFormat("[ACR] ROUTE %1: straight line crosses water, going round in %2 legs", sq.m_Name, legs.Count());
  StartRoute(sq, legs, prefab, radius, order, kind);
  return true;
 }

 static bool FieldLegs(ACR_Squad sq, vector from, vector to, notnull array<vector> legs)
 {
  legs.Clear();
  if (ACR_LogisticsRules.Choice("acrFieldRoutes", "1") == "0" || !sq.GetFaction()) return false;
  float length = vector.DistanceXZ(from, to);
  if (length < ACR_Squad.FIELD_STEP * 2 + ACR_Squad.FIELD_APPROACH || BBB_Terrain.WaterAlong(from, to) > 0) return false;
  int started = System.GetTickCount();
  string side = sq.GetFaction().GetFactionKey();
  vector dir = to - from;
  dir[1] = 0;
  dir.Normalize();
  vector across = Vector(-dir[2], 0, dir[0]);
  // Deep water the line crosses, and the bridge over each crossing.
  array<float> crossFrom = {};
  array<float> crossTo = {};
  array<vector> bridgeNear = {};
  array<vector> bridgeFar = {};
  array<vector> approachNear = {};
  array<vector> approachFar = {};
  bool inWater;
  float wetFrom;
  for (float wetAt = 0; wetAt <= length; wetAt += ACR_Squad.FIELD_WATER_STEP)
  {
   bool deep = BBB_Terrain.IsDeepWaterAt(from + dir * wetAt);
   if (deep && !inWater)
   {
    inWater = true;
    wetFrom = wetAt;
   }
   else if (!deep && inWater)
   {
    inWater = false;
    vector nearEnd, farEnd, nearApproach, farApproach;
    if (!BBB_Terrain.BridgeNear(from + dir * ((wetFrom + wetAt) * 0.5), from, ACR_Squad.FIELD_BRIDGE_SEARCH, nearEnd, farEnd, nearApproach, farApproach))
    {
     PrintFormat("[ACR] ROUTE %1: deep water %2 m along and no bridge within %3 m, walking direct", sq.m_Name, Math.Round(wetFrom), ACR_Squad.FIELD_BRIDGE_SEARCH);
     ACR_Perf.Lap("route.field", started);
     return false;
    }
    // The same bridge again (a viaduct over two channels, a river bending under it): one crossing.
    int last = bridgeNear.Count() - 1;
    if (last >= 0 && (vector.DistanceXZ(bridgeNear[last], nearEnd) < 20 || vector.DistanceXZ(bridgeNear[last], farEnd) < 20))
    {
     crossTo[last] = wetAt + ACR_Squad.FIELD_BRIDGE_KEEP;
     continue;
    }
    crossFrom.Insert(wetFrom - ACR_Squad.FIELD_BRIDGE_KEEP);
    crossTo.Insert(wetAt + ACR_Squad.FIELD_BRIDGE_KEEP);
    bridgeNear.Insert(nearEnd);
    bridgeFar.Insert(farEnd);
    approachNear.Insert(nearApproach);
    approachFar.Insert(farApproach);
    PrintFormat("[ACR] ROUTE %1: deep water %2 m along, over the bridge %3 to %4", sq.m_Name, Math.Round(wetFrom), nearEnd, farEnd);
   }
  }
  if (inWater) { ACR_Perf.Lap("route.field", started); return false; } // the destination is in the water
  float lean = 0; // the side the last moved leg went to (0: none yet)
  bool moved = !bridgeNear.IsEmpty();
  vector previous = from;
  int next;
  bool bridged; // crossing 'next' has its bridge legs
  int fixedLegs; // legs up to the last bridge's way off: never dropped
  array<int> decks = {}; // legs that start a bridge deck (the step to the next one is over water)
  for (float along = ACR_Squad.FIELD_STEP; along < length - ACR_Squad.FIELD_APPROACH; along += ACR_Squad.FIELD_STEP)
  {
   bool crossing;
   while (next < crossFrom.Count() && along >= crossFrom[next])
   {
    if (!bridged)
    {
     OntoBridge(legs, from, fixedLegs, decks, approachNear[next], bridgeNear[next], bridgeFar[next], approachFar[next]);
     fixedLegs = legs.Count();
     previous = legs[legs.Count() - 1];
     bridged = true;
    }
    if (along <= crossTo[next])
    {
     crossing = true;
     break;
    }
    next++;
    bridged = false;
   }
   if (crossing) continue;
   vector point = ACR_Manager.Ground(from + dir * along);
   vector leg = point;
   if ((ACR_Squad.s_bFieldAnywhere || !FriendlyGround(point, side)) && ACR_Minefields.RoadClearance(point) < ACR_Squad.FIELD_CLEAR)
   {
    bool ok;
    for (float shift = ACR_Squad.FIELD_SHIFT; shift <= ACR_Squad.FIELD_SHIFT_MAX && !ok; shift += ACR_Squad.FIELD_SHIFT)
    {
     for (int pass = 0; pass < 2 && !ok; pass++)
     {
      float sense = 1;
      if (lean < 0) sense = -1;
      if (pass == 1) sense = -sense;
      vector candidate = ACR_Manager.Ground(point + across * (shift * sense));
      if (ACR_Minefields.RoadClearance(candidate) < ACR_Squad.FIELD_CLEAR || BBB_Terrain.WaterDepthAt(candidate) > 0) continue;
      if (BBB_Terrain.DeepWaterAlong(previous, candidate) > 0) continue;
      leg = candidate;
      lean = sense;
      moved = true;
      ok = true;
     }
    }
   }
   if (BBB_Terrain.IsDeepWaterAt(leg) || BBB_Terrain.DeepWaterAlong(previous, leg) > 0) continue;
   legs.Insert(leg);
   previous = leg;
  }
  // Crossings past the last leg (in the approach): their bridges still.
  while (next < crossFrom.Count())
  {
   if (!bridged)
   {
    OntoBridge(legs, from, fixedLegs, decks, approachNear[next], bridgeNear[next], bridgeFar[next], approachFar[next]);
    fixedLegs = legs.Count();
   }
   bridged = false;
   next++;
  }
  if (!moved) { ACR_Perf.Lap("route.field", started); legs.Clear(); return false; }
  legs.Insert(to);
  // The whole route dry-footed, bridge decks apart (a river may bend back across the way between
  // legs the line never crossed): else walk direct, as before.
  vector walked = from;
  foreach (int n, vector step : legs)
  {
   if (!decks.Contains(n - 1) && BBB_Terrain.DeepWaterAlong(walked, step) > 0)
   {
    PrintFormat("[ACR] ROUTE %1: deep water between field legs %2 and %3, walking direct", sq.m_Name, walked, step);
    ACR_Perf.Lap("route.field", started);
    legs.Clear();
    return false;
   }
   walked = step;
  }
  ACR_Perf.Lap("route.field", started);
  return true;
 }

 // Over a bridge along its road: a leg or two since the last bridge (fixedLegs) that would leave
 // deep water between them and the way on is dropped when the leg before reaches it dry (a river
 // bending round the abutments), then the way on, both ends (recorded in decks) and the way off.
 static void OntoBridge(array<vector> legs, vector from, int fixedLegs, array<int> decks, vector nearApproach, vector nearEnd, vector farEnd, vector farApproach)
 {
  for (int drop = 0; drop < 2 && legs.Count() > fixedLegs; drop++)
  {
   if (BBB_Terrain.DeepWaterAlong(legs[legs.Count() - 1], nearApproach) <= 0) break;
   vector before = from;
   if (legs.Count() >= 2) before = legs[legs.Count() - 2];
   if (BBB_Terrain.DeepWaterAlong(before, nearApproach) > 0) break;
   legs.Remove(legs.Count() - 1);
  }
  if (vector.DistanceXZ(nearApproach, nearEnd) > 1) legs.Insert(nearApproach);
  legs.Insert(nearEnd);
  decks.Insert(legs.Count() - 1);
  legs.Insert(farEnd);
  if (vector.DistanceXZ(farApproach, farEnd) > 1) legs.Insert(farApproach);
 }

 // Nearest held base is ours or a friend's.
 static bool FriendlyGround(vector point, string side)
 {
  ACR_Base nearest;
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_sOwner.IsEmpty()) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), point);
   if (d < best) { best = d; nearest = base; }
  }
  return nearest && ACR_Insurgency.Friendly(nearest.m_sOwner, side);
 }

 static void StartRoute(ACR_Squad sq, array<vector> legs, ResourceName prefab, float radius, string order, int kind)
 {
  sq.NewOrder();
  sq.m_bRegrouping = false;
  ClearPlans(sq);
  foreach (vector leg : legs) sq.m_aRoute.Insert(leg);
  vector finalDestination = vector.Zero;
  if (!legs.IsEmpty()) finalDestination = legs[legs.Count() - 1];
  sq.m_Route = ACR_OrderSnapshot.Make(prefab, finalDestination, radius, order, ACR_Squad.StateOfLabel(order), kind);
  NextLeg(sq);
 }

 // Walks a route the AI commander planned (a covered approach), leg by leg like a land route.
 // A crewed vehicle drives it as one trip to its end (#86 again, 20 Nov, Jacob: one waypoint per trip):
 // on Reforger 1.9 every waypoint change ends the group's move and the driver plans again from a stop,
 // whatever is queued behind it. A recorded supply route or a covered approach is the game's own road
 // plan to the last point; the road hops stay the fallback for a drive that fails (OnGroupMoveFailed,
 // DriveWatch's Reroute). -acrNoRoadHops 0 walks the legs as before.
 static void WalkLegs(ACR_Squad sq, array<vector> legs, ResourceName prefab, float radius, string order, int kind)
 {
  if (!legs || legs.IsEmpty()) return;
  if (legs.Count() > 1 && NoRoadHops() && sq.IsMounted() && ACR_Transport.CrewedVehicle(sq))
  {
   PrintFormat("[ACR] ROUTE %1: %2 legs driven as one trip to the last", sq.m_Name, legs.Count());
   RouteTo(sq, legs[legs.Count() - 1], prefab, radius, order, kind);
   return;
  }
  StartRoute(sq, legs, prefab, radius, order, kind);
 }

 // m_Route outlives its route (NextLeg restores the final order from it), so it only counts while
 // legs remain: after a later plain MoveTo it named an old route's end (audit 31 Oct).
 static vector RouteDestination(ACR_Squad sq)
 {
  if (!sq.m_aRoute.IsEmpty() && sq.m_Route && sq.m_Route.m_Target != vector.Zero) return sq.m_Route.m_Target;
  if (!sq.m_aRoute.IsEmpty()) return sq.m_aRoute[sq.m_aRoute.Count() - 1];
  return sq.m_MoveTarget;
 }

 static bool HasActiveRoute(ACR_Squad sq)
 {
  return sq.m_Route != null || !sq.m_aRoute.IsEmpty();
 }

 static void NextLeg(ACR_Squad sq)
 {
  sq.m_iWaypointRecoveries = 0;
  sq.m_fNextWaypointRecovery = 0;
  SkipStraightHops(sq);
  vector leg = sq.m_aRoute[0];
  sq.m_aRoute.RemoveOrdered(0);
  if (sq.m_aRoute.IsEmpty()) sq.RestoreOrderAt(sq.m_Route, sq.m_Route.m_sPrefab, leg, sq.m_Route.m_fRadius);
  else sq.RestoreOrderAt(sq.m_Route, ACR_Squad.WP_MOVE, leg, 25);
 }

 static bool RoadFlow()
 {
  return !ACR_Squad.s_bRoadFlowOff && ACR_LogisticsRules.Choice("acrRoadFlow", "1") != "0";
 }

 static void SkipStraightHops(ACR_Squad sq)
 {
  if (sq.m_aRoute.Count() < 2 || !sq.IsMounted() || !RoadFlow()) return;
  IEntity driven = ACR_Transport.CrewedVehicle(sq);
  if (!driven) return;
  vector from = driven.GetOrigin();
  // Handing over short of the current hop (not in m_aRoute any more): it must be on the line too.
  bool viaCurrent = sq.m_MoveTarget != vector.Zero && vector.DistanceXZ(from, sq.m_MoveTarget) < 200;
  int aim = 0;
  for (int k = 1; k < sq.m_aRoute.Count(); k++)
  {
   if (vector.DistanceXZ(from, sq.m_aRoute[k]) > ACR_Squad.ROAD_AIM_M) break;
   bool straight = !viaCurrent || ACR_AICommander.DistanceToSegment(sq.m_MoveTarget, from, sq.m_aRoute[k]) <= ACR_Squad.ROAD_STRAIGHT_M;
   for (int j = 0; j < k && straight; j++)
    straight = ACR_AICommander.DistanceToSegment(sq.m_aRoute[j], from, sq.m_aRoute[k]) <= ACR_Squad.ROAD_STRAIGHT_M;
   if (!straight) break;
   aim = k;
  }
  if (aim > 0) PrintFormat("[ACR] ROAD %1: straight road, skipping %2 hops (%3 left)", sq.m_Name, aim, sq.m_aRoute.Count() - aim);
  for (int skip = 0; skip < aim; skip++) sq.m_aRoute.RemoveOrdered(0);
 }

 // The order's own waypoint, or the road hop queued behind it (the group may be on it
 // for up to a squad tick before FlowHops makes it the order's waypoint).
 static bool IsOrderWaypoint(ACR_Squad sq, AIWaypoint waypoint)
 {
  return waypoint && (waypoint == sq.m_Waypoint || waypoint == sq.m_QueuedHop);
 }

 static void DropQueuedHop(ACR_Squad sq)
 {
  if (!sq.m_QueuedHop) return;
  if (sq.m_Group) sq.m_Group.RemoveWaypoint(sq.m_QueuedHop);
  ACR_Squad.DeleteWaypointSoon(sq.m_QueuedHop);
  sq.m_QueuedHop = null;
 }

 // True when it looked after the route this tick.
 static bool FlowHops(ACR_Squad sq, IEntity driven, float handover)
 {
  if (!RoadFlow()) return false;
  AIWaypoint current = sq.m_Group.GetCurrentWaypoint();
  if (sq.m_QueuedHop)
  {
   // The group moved on to it: it is the order's waypoint now.
   if (current == sq.m_QueuedHop)
   {
    ACR_Squad.DeleteWaypointSoon(sq.m_Waypoint);
    sq.m_Waypoint = sq.m_QueuedHop;
    sq.m_MoveTarget = sq.m_QueuedHop.GetOrigin();
    sq.m_QueuedHop = null;
    if (!sq.m_aRoute.IsEmpty()) sq.m_aRoute.RemoveOrdered(0);
    sq.m_iWaypointRecoveries = 0;
    sq.m_fNextWaypointRecovery = 0;
    return true;
   }
   if (current == sq.m_Waypoint && current) return true;
   // Something else replaced the waypoints (a player, a recovery): the queued hop is stale.
   DropQueuedHop(sq);
   return false;
  }
  if (sq.m_aRoute.Count() < 2 || !current || current != sq.m_Waypoint || sq.m_sOrderPrefab != ACR_Squad.WP_MOVE) return false;
  if (vector.DistanceXZ(driven.GetOrigin(), sq.m_MoveTarget) > handover + 80) return true;
  SkipStraightHops(sq);
  if (sq.m_aRoute.Count() < 2) return false;
  Resource resource = Resource.Load(ACR_Squad.WP_MOVE);
  if (!resource.IsValid()) return false;
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  params.Transform[3] = sq.m_aRoute[0];
  AIWaypoint hop = AIWaypoint.Cast(GetGame().SpawnEntityPrefabLocal(resource, null, params));
  if (!hop) return false;
  hop.SetCompletionRadius(25);
  SCR_AIWaypoint scripted = SCR_AIWaypoint.Cast(hop);
  if (scripted && sq.m_bForced) scripted.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
  sq.m_Group.AddWaypoint(hop);
  sq.m_QueuedHop = hop;
  return true;
 }

 // How near the current hop a crewed vehicle gets the next one: far enough out that the
 // driver hasn't started braking yet, with a squad tick (1 s) to spare.
 static float RoadHandover(ACR_Squad sq, IEntity driven)
 {
  if (!RoadFlow() || !driven.GetPhysics()) return 70;
  return Math.Max(70, driven.GetPhysics().GetVelocity().Length() * ACR_Squad.ROAD_HANDOVER_S);
 }

 static void ClearPlans(ACR_Squad sq)
 {
  sq.m_aRoute.Clear();
  sq.m_aQueuePos.Clear();
  sq.m_aQueueKind.Clear();
  sq.m_bPatrol = false; // a new order ends a patrol
  sq.m_iEscortPlayerId = 0;
  sq.m_vEscortAnchor = vector.Zero;
 }

 // Patrol (map panel): the squad walks its queued points (Shift + right-click) in a loop, the
 // point it is moving to now included; Defend and Garrison points are walked through. With one
 // point only it walks between where it stands and that point. A squad that isn't moving
 // (holding where it arrived, say) sets off at once. A new order ends it, as does toggling it
 // again: then it finishes the current leg only (the loop's points are dropped, #65 review: they
 // were all still queued, so it walked one more lap).
 static bool TogglePatrol(ACR_Squad sq, out string refusal)
 {
  refusal = "";
  if (sq.m_bPatrol)
  {
   sq.m_bPatrol = false;
   sq.m_aQueuePos.Clear();
   sq.m_aQueueKind.Clear();
   return true;
  }
  bool moving = sq.OrderState() == ACR_EOrderState.MOVE && sq.m_Group && sq.m_Group.GetCurrentWaypoint() && sq.m_MoveTarget != vector.Zero && !sq.m_aQueuePos.Contains(sq.m_MoveTarget);
  int points = sq.m_aQueuePos.Count();
  if (moving) points++;
  IEntity anchor = sq.GetAnchor();
  if (points < 1 || !anchor)
  {
   refusal = "give it a move order or Shift + right-click a point first";
   return false;
  }
  // One point: there and back, from where it stands.
  if (points < 2 && sq.m_aQueuePos.Count() < ACR_Squad.MAX_QUEUED)
  {
   sq.m_aQueuePos.Insert(anchor.GetOrigin());
   sq.m_aQueueKind.Insert(0);
  }
  // Past MAX_QUEUED if need be: the point it is heading for is never left out of the loop.
  if (moving)
  {
   sq.m_aQueuePos.Insert(sq.m_MoveTarget);
   sq.m_aQueueKind.Insert(0);
  }
  sq.m_bPatrol = true;
  PrintFormat("[ACR] SQUAD %1: patrolling %2 points (moving %3)", sq.m_Name, sq.m_aQueuePos.Count(), moving);
  if (!moving) NextQueued(sq);
  return true;
 }

 // Add an order after the current one (or carry it out now if there's nothing to do).
 // Waiting where it ended up ("Holding position": arrived, dismounted, a build done): its hold
 // waypoint never completes, so nothing queued behind it would ever start.
 static bool HoldingInPlace(ACR_Squad sq) { return sq.m_Order == "Holding position"; }

 // Jacob (15 Nov): an order queued behind a hold-type order (Defend, Garrison, a post: its Defend
 // waypoint never completes) would never start, so it is refused, not run at once. Behind the last
 // queued order, the route's end order, else the current one. A patrol walks every point as a move.
 static bool QueueBehindHold(ACR_Squad sq)
 {
  if (!sq.m_Group) return false;
  if (!sq.m_aQueuePos.IsEmpty())
  {
   if (sq.m_bPatrol) return false;
   int last = sq.m_aQueueKind[sq.m_aQueueKind.Count() - 1];
   return last == 1 || last == 2;
  }
  if (!sq.m_aRoute.IsEmpty()) return sq.m_Route && sq.m_Route.m_iKind == ACR_EOrderKind.DEFEND;
  // Set aside for a while (a regroup, getting away in a vehicle, a combat flank): the order put back
  // afterwards is what a queue would wait behind, so a Defend there refuses it too.
  ACR_OrderSnapshot aside;
  if (sq.m_CombatFlank.Active()) aside = sq.m_CombatFlank.Original();
  else if (sq.m_bRegrouping) aside = sq.m_Regroup;
  else if (sq.m_Evade) aside = sq.m_Evade;
  if (aside) return aside.m_eState == ACR_EOrderState.DEFEND;
  return sq.m_Group.GetCurrentWaypoint() != null && !HoldingInPlace(sq) && sq.OrderState() == ACR_EOrderState.DEFEND;
 }

 static bool Queue(ACR_Squad sq, vector position, bool defend, bool garrison = false)
 {
  if (!sq.m_Group || QueueBehindHold(sq)) return false;
  bool busy = (sq.m_Group.GetCurrentWaypoint() != null && !HoldingInPlace(sq)) || !sq.m_aRoute.IsEmpty() || !sq.m_aQueuePos.IsEmpty();
  if (!busy)
  {
   if (garrison) { sq.Garrison(position); return true; }
   if (defend) return sq.Defend(position);
   return sq.MoveTo(position);
  }
  if (sq.m_aQueuePos.Count() >= ACR_Squad.MAX_QUEUED) return false;
  sq.m_aQueuePos.Insert(position);
  int kind = 0;
  if (defend) kind = 1;
  if (garrison) kind = 2;
  sq.m_aQueueKind.Insert(kind);
  return true;
 }

 // Every point still ahead of the squad, in order: route legs, then queued orders.
 static void GetPlannedPoints(ACR_Squad sq, notnull array<vector> points, notnull array<int> kinds)
 {
  if (sq.m_CombatFlank.Active())
  {
   points.Insert(sq.m_CombatFlank.Original().m_Target);
   kinds.Insert(sq.m_CombatFlank.Original().m_iKind);
  }
  int routeKind;
  if (sq.m_Route) routeKind = sq.m_Route.m_iKind;
  foreach (vector leg : sq.m_aRoute)
  {
   points.Insert(leg);
   kinds.Insert(routeKind);
  }
  foreach (int i, vector queued : sq.m_aQueuePos)
  {
   points.Insert(queued);
   int queuedKind = sq.m_aQueueKind[i];
   if (queuedKind == 2) queuedKind = ACR_EOrderKind.DEFEND;
   kinds.Insert(queuedKind);
  }
 }

 static void FollowQueue(ACR_Squad sq)
 {
  // Not while hunting: the queue waits for the hunt to end (EndHunt), or the next sighting's new
  // Search & Destroy order replaced the queued one it had started.
  if (sq.m_aQueuePos.IsEmpty() || !sq.m_aRoute.IsEmpty() || sq.m_bRegrouping || sq.m_bHunting || !sq.m_Group) return;

  if (sq.m_Group.GetCurrentWaypoint()) return;
  // An engine-completed waypoint is not proof that the squad reached it.
  if (MovementStillPending(sq)) return;
  NextQueued(sq);
 }

 // The next queued order now (a patrol puts the point back at the end).
 static void NextQueued(ACR_Squad sq)
 {
  if (sq.m_aQueuePos.IsEmpty() || !sq.m_Group) return;
  vector next = sq.m_aQueuePos[0];
  int kind = sq.m_aQueueKind[0];
  sq.m_aQueuePos.RemoveOrdered(0);
  sq.m_aQueueKind.RemoveOrdered(0);
  // A patrol: the point goes back at the end, and every point is walked through.
  if (sq.m_bPatrol)
  {
   sq.m_aQueuePos.Insert(next);
   sq.m_aQueueKind.Insert(0);
   kind = 0;
  }
  sq.m_iWaypointRecoveries = 0;
  sq.m_fNextWaypointRecovery = 0;
  if (kind == 2)
  {
   sq.PlaceGarrison(next);
   kind = ACR_EOrderKind.DEFEND;
  }
  else if (kind == 1)
  {
   sq.IssueWaypoint(ACR_Squad.WP_DEFEND, next, 15);
   sq.SetOrder("Defending", ACR_EOrderState.DEFEND);
  }
  else
  {
   sq.IssueWaypoint(ACR_Squad.WP_MOVE, next);
   if (sq.m_bPatrol) sq.SetOrder("Patrolling", ACR_EOrderState.MOVE);
   else sq.SetOrder("Moving", ACR_EOrderState.MOVE);
  }
  sq.m_OrderKind = kind;
 }

 static void FollowRoute(ACR_Squad sq)
 {
  FollowQueue(sq);
  // Getting away (Evade / DriveOut) keeps the route for EvadeTick's RestoreOrder, but the escape
  // waypoint is not a leg: handing over near it sent the vehicle down the next hop, back into fire.
  if (sq.m_aRoute.IsEmpty() || sq.m_bRegrouping || !sq.m_Group || sq.m_Evade) return;
  array<AIWaypoint> waypoints = {};
  sq.m_Group.GetWaypoints(waypoints);
  IEntity leader = sq.GetLeader();
  // Vehicles get the next road hop well before reaching this one: the AI driver
  // brakes as it nears the end of a waypoint, so handing over at 30 m made it stop
  // for a second at every hop (1 Oct live test).
  float handover = 30;
  IEntity driven;
  if (sq.IsMounted()) driven = ACR_Transport.CrewedVehicle(sq);
  if (driven) handover = RoadHandover(sq, driven);
  if (driven && FlowHops(sq, driven, handover)) return;
  if (!waypoints.IsEmpty())
  {
   if (leader && vector.DistanceXZ(leader.GetOrigin(), sq.m_MoveTarget) < handover) NextLeg(sq);
   return;
  }
  if (!leader) return;
  float toLeg = vector.DistanceXZ(leader.GetOrigin(), sq.m_MoveTarget);
  if (toLeg < handover)
  {
   NextLeg(sq);
   return;
  }
  // Recovery runs after route/queue handling, including the final mounted leg.
 }

 static bool MovementStillPending(ACR_Squad sq)
 {
  ACR_EOrderState state = sq.OrderState();
  if (state != ACR_EOrderState.MOVE && state != ACR_EOrderState.DEFEND && state != ACR_EOrderState.ASSAULT && state != ACR_EOrderState.RETURN) return false;
  if (sq.m_sOrderPrefab != ACR_Squad.WP_MOVE && sq.m_sOrderPrefab != ACR_Squad.WP_DEFEND && sq.m_sOrderPrefab != ACR_Squad.WP_SEARCH_DESTROY && sq.m_sOrderPrefab != ACR_Squad.WP_GARRISON) return false;
  IEntity anchor = sq.GetAnchor();
  if (!anchor) return true;
  float arrival = Math.Max(60, sq.m_fOrderRadius);
  if (sq.IsMounted()) arrival = Math.Max(70, arrival);
  if (!sq.m_aRoute.IsEmpty()) arrival = 30;
  if (!sq.m_aRoute.IsEmpty() && sq.IsMounted() && ACR_Transport.CrewedVehicle(sq)) arrival = 70;
  return vector.DistanceXZ(anchor.GetOrigin(), sq.m_MoveTarget) >= arrival;
 }

 static void RecoverMovementWaypoint(ACR_Squad sq, float now)
 {
  if (!sq.m_Group || sq.m_bRegrouping || sq.m_bGarrisonPending || sq.m_Group.GetCurrentWaypoint() || !MovementStillPending(sq)) return;
  if (now < sq.m_fNextWaypointRecovery || sq.m_iWaypointRecoveries >= 4) return;
  sq.m_fNextWaypointRecovery = now + 10000;
  sq.m_iWaypointRecoveries++;
  // Jacob 18 Oct: orders were dropped after four identical reissues to a spot the
  // engine could not path to. The first retry repeats the order (a transient drop);
  // later ones aim for the nearest walkable point round it, searching wider each time.
  // The start can be the problem as much as the target: a soldier stranded off the navmesh.
  if (!sq.IsMounted()) sq.StepOntoNavmesh(0);
  vector target = sq.m_MoveTarget;
  string how = "same spot";
  if (sq.m_iWaypointRecoveries >= 2)
  {
   vector reachable;
   bool found = ReachableNear(sq, sq.m_MoveTarget, 5 + 15 * (sq.m_iWaypointRecoveries - 2), reachable);
   // #30: the spot itself is walkable, so the way in from this side is the problem.
   if ((!found || vector.DistanceXZ(reachable, sq.m_MoveTarget) < 3) && DetourRound(sq)) return;
   if (found)
   {
    how = string.Format("nearest walkable point, %1 m off", (int)vector.DistanceXZ(reachable, sq.m_MoveTarget));
    target = reachable;
    sq.m_MoveTarget = reachable;
   }
  }
  sq.IssueWaypoint(sq.m_sOrderPrefab, target, sq.m_fOrderRadius, sq.m_bForced);
  PrintFormat("[ACR] MOVE %1: waypoint disappeared before arrival; recovery %2/4 (%3), group vehicles %4", sq.m_Name, sq.m_iWaypointRecoveries, how, GroupVehicleCount(sq));
  if (sq.m_iWaypointRecoveries >= 4) sq.m_bRecoveryLast = true;
 }

 static bool DetourRound(ACR_Squad sq)
 {
  if (sq.IsMounted()) return false;
  IEntity anchor = sq.GetAnchor();
  if (!anchor) return false;
  bool onDetour = sq.m_iDetourSerial == sq.m_iOrderSerial && vector.DistanceXZ(sq.m_MoveTarget, sq.m_vDetourGoal) > 1;
  if (sq.m_iDetourSerial != sq.m_iOrderSerial)
  {
   vector away = anchor.GetOrigin() - sq.m_MoveTarget;
   away[1] = 0;
   if (away.LengthSq() < 1) return false;
   away.Normalize();
   sq.m_iDetourSerial = sq.m_iOrderSerial;
   sq.m_iDetours = 0;
   sq.m_vDetourGoal = sq.m_MoveTarget;
   sq.m_vDetourAway = away;
  }
  if (sq.m_iDetours >= 2) return false;
  vector goal = sq.m_vDetourGoal;
  vector side = Vector(sq.m_vDetourAway[2], 0, -sq.m_vDetourAway[0]);
  if (sq.m_iDetours == 1) side = side * -1;
  vector back, flank;
  if (!ReachableNear(sq, goal + side * ACR_Squad.DETOUR_M, 30, flank)) return false;
  bool backOut = vector.DistanceXZ(anchor.GetOrigin(), goal) < ACR_Squad.DETOUR_M - 30 && ReachableNear(sq, goal + sq.m_vDetourAway * ACR_Squad.DETOUR_M, 30, back);
  // The legs of the last detour still ahead go; the goal and the order's own route after it stay.
  if (onDetour)
  {
   while (!sq.m_aRoute.IsEmpty() && vector.DistanceXZ(sq.m_aRoute[0], goal) > 1) sq.m_aRoute.RemoveOrdered(0);
  }
  else
  {
   if (sq.m_aRoute.IsEmpty()) sq.m_Route = ACR_OrderSnapshot.Make(sq.m_sOrderPrefab, goal, sq.m_fOrderRadius, sq.m_Order, sq.OrderState(), sq.m_OrderKind);
   sq.m_aRoute.InsertAt(goal, 0);
  }
  sq.m_aRoute.InsertAt(flank, 0);
  if (backOut) sq.m_aRoute.InsertAt(back, 0);
  sq.m_iDetours++;
  string how = "";
  if (backOut) how = string.Format("back out to %1, then ", ACR_Manager.Grid(back));
  PrintFormat("[ACR] MOVE %1: can't get in to %2 from this side, going round (%3%4, detour %5/2)", sq.m_Name, ACR_Manager.Grid(goal), how, ACR_Manager.Grid(flank), sq.m_iDetours);
  // The commander who gave the order sees why it walks away from the target.
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && sq.m_iOrderedBy > 0) manager.Notify(sq.m_iOrderedBy, string.Format("%1 can't get in to %2 from this side, going round", sq.m_Name, ACR_Manager.Grid(goal)));
  NextLeg(sq);
  return true;
 }

 static void ReportUnreachable(ACR_Squad sq)
 {
  if (!sq.m_bRecoveryLast || !sq.m_Group || sq.m_Group.GetCurrentWaypoint() || !MovementStillPending(sq)) return;
  if (GetGame().GetWorld().GetWorldTime() < sq.m_fNextWaypointRecovery) return;
  sq.m_bRecoveryLast = false;
  string message = string.Format("%1 can't reach %2, holding", sq.m_Name, ACR_Manager.Grid(sq.m_MoveTarget));
  // Where the squad stood too: a squad stuck in a pocket fails every move from there, a target
  // in one fails every squad (AI soak 7 Nov: the logs couldn't tell the two apart).
  IEntity anchor = sq.GetAnchor();
  vector from = sq.m_MoveTarget;
  if (anchor) from = anchor.GetOrigin();
  int off = vector.DistanceXZ(from, sq.m_MoveTarget);
  PrintFormat("[ACR] MOVE %1: gave up after 4 recoveries at %2 (the squad at %3, %4 m away)", sq.m_Name, sq.m_MoveTarget, from, off);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && sq.m_iOrderedBy > 0) manager.Notify(sq.m_iOrderedBy, message);
  sq.m_iGaveUpMoveSerial = sq.m_iOrderSerial;
  sq.m_fGaveUpAt = GetGame().GetWorld().GetWorldTime();
  sq.Halt("Holding (unreachable)");
  sq.m_iUnreachableSerial = sq.m_iOrderSerial;
 }

 static bool GaveUpUnreachable(ACR_Squad sq) { return sq.m_iUnreachableSerial == sq.m_iOrderSerial; }

 // A move order given since this serial (this one included) was given up as unreachable.
 static bool GaveUpSince(ACR_Squad sq, int serial) { return sq.m_iGaveUpMoveSerial >= serial; }

 // srv:mortargiveup: as ReportUnreachable, without waiting out four recoveries.
 static void GiveUpForTest(ACR_Squad sq)
 {
  sq.m_iGaveUpMoveSerial = sq.m_iOrderSerial;
  sq.m_fGaveUpAt = GetGame().GetWorld().GetWorldTime();
  sq.Halt("Holding (unreachable)");
  sq.m_iUnreachableSerial = sq.m_iOrderSerial;
 }

 // Its last move order still stands, though the engine may have dropped the waypoint: the
 // squad's own recovery owns it, and a fresh order would reset that recovery.
 static bool MovePending(ACR_Squad sq) { return MovementStillPending(sq) && !GaveUpUnreachable(sq); }

 // Vehicles the game's group AI counts as its own (it plans moves with them).
 static int GroupVehicleCount(ACR_Squad sq)
 {
  if (!sq.m_Group) return 0;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(sq.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  if (!utility || !utility.m_VehicleMgr) return 0;
  return utility.m_VehicleMgr.GetVehiclesCount();
 }

 // The nearest point on the soldiers' navmesh or road within radius of 'at'.
 static bool ReachableNear(ACR_Squad sq, vector at, float radius, out vector result)
 {
  if (sq.IsMounted())
  {
   vector roadPoint;
   if (BBB_Terrain.RoadPointNear(at, radius, roadPoint))
   {
    result = roadPoint;
    return true;
   }
   return false;
  }
  array<AIAgent> agents = {};
  sq.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   AIPathfindingComponent pathing = AIPathfindingComponent.Cast(agent.FindComponent(AIPathfindingComponent));
   if (!pathing) continue;
   NavmeshWorldComponent mesh = pathing.GetNavmeshComponent();
   if (mesh && !mesh.IsTileLoaded(at)) mesh.LoadTileIn(at);
   return pathing.GetClosestPositionOnNavmesh(at, Vector(radius, 10, radius), result);
  }
  return false;
 }
}
