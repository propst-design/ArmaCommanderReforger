// Covered-approach geometry, hidden staging points, and approach-line selection
// for ACR_AICommander. Extracted as a standalone helper to avoid modded-class
// load-order constraints; ACR_AICommander keeps thin forwarding wrappers for
// existing callers and tests.
class ACR_AIApproach
{
 // Where the objective's defenders look from: its centre and four points half way out, 2.2 m up.
 static void ObjectiveEyes(vector center, float radius, notnull array<vector> eyes)
 {
  eyes.Insert(EyeAt(center));
  for (int i = 0; i < 4; i++)
   eyes.Insert(EyeAt(center + Turned(Vector(0, 0, 1), i * 90) * (radius * 0.5)));
 }

 static vector EyeAt(vector point)
 {
  point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]) + 2.2;
  return point;
 }

 // The ground at 'point' is in sight of an eye (terrain only).
 static bool GroundSeen(notnull array<vector> eyes, vector point)
 {
  BaseWorld world = GetGame().GetWorld();
  vector body = point;
  body[1] = world.GetSurfaceY(point[0], point[2]) + 1.0;
  foreach (vector eye : eyes)
  {
   if (vector.DistanceXZ(eye, body) > ACR_AICommander.COVER_RANGE) continue;
   TraceParam trace = new TraceParam();
   trace.Start = eye;
   trace.End = body;
   trace.Flags = TraceFlags.WORLD;
   if (world.TraceMove(trace, null) >= 1) return true;
  }
  return false;
 }

 // Length and exposed length of the path from 'from' through 'legs', sampled every COVER_SAMPLE m.
 static float PathExposure(notnull array<vector> eyes, vector from, notnull array<vector> legs, out float exposed)
 {
  float length = 0;
  exposed = 0;
  vector a = from;
  foreach (vector b : legs)
  {
   float d = vector.DistanceXZ(a, b);
   int steps = (int)Math.Ceil(d / ACR_AICommander.COVER_SAMPLE);
   if (steps < 1) steps = 1;
   for (int i = 1; i <= steps; i++)
   {
    float t = i;
    t /= steps;
    if (GroundSeen(eyes, a + (b - a) * t)) exposed += d / steps;
   }
   length += d;
   a = b;
  }
  return length;
 }

 // Waypoints round 'center' from 'from' to 'stage', turning one way (sense 1 or -1), one every
 // COVER_ARC_STEP degrees. The radius runs from where the squad meets the arc (at most
 // COVER_SPIRAL_OUT past 'ring') to the staging point's, so the squad never walks in closer than that.
 static void ArcLegs(vector center, vector from, vector stage, float ring, int sense, notnull array<vector> legs)
 {
  vector a = from - center;
  a[1] = 0;
  vector b = stage - center;
  b[1] = 0;
  float startAngle = Math.Atan2(a[2], a[0]);
  float sweep = Math.Atan2(b[2], b[0]) - startAngle;
  if (sense > 0) { while (sweep <= 0) sweep += Math.PI2; }
  else { while (sweep >= 0) sweep -= Math.PI2; }
  float startRadius = Math.Clamp(a.Length(), ring, ring + ACR_AICommander.COVER_SPIRAL_OUT);
  float endRadius = b.Length();
  int steps = (int)Math.Ceil(Math.AbsFloat(sweep) * Math.RAD2DEG / ACR_AICommander.COVER_ARC_STEP);
  if (steps < 1) steps = 1;
  for (int i = 0; i < steps; i++)
  {
   float t = i;
   t /= steps;
   float angle = startAngle + sweep * t;
   float r = startRadius + (endRadius - startRadius) * t;
   vector point = center + Vector(Math.Cos(angle) * r, 0, Math.Sin(angle) * r);
   if (i == 0 && vector.DistanceXZ(point, from) < 30) continue;
   legs.Insert(point);
  }
  legs.Insert(stage);
 }

 // The strongpoint an attack of ours on 'target' goes round, or null.
 static ACR_Base BypassFor(ACR_AICommander cmd, ACR_Base target)
 {
  if (cmd.m_LaunchingAttack && cmd.m_LaunchingAttack.m_Target == target && cmd.m_LaunchingAttack.m_Bypass && cmd.m_LaunchingAttack.m_Bypass.m_sOwner != cmd.m_sFaction) return cmd.m_LaunchingAttack.m_Bypass;
  foreach (ACR_AIAttack attack : cmd.m_Attacks)
   if (attack.m_Target == target && attack.m_Bypass && attack.m_Bypass.m_sOwner != cmd.m_sFaction) return attack.m_Bypass;
  return null;
 }

 // A covered way for a foot squad to its staging point for 'target', when one beats the direct
 // line; sets cmd.m_fCoverDetour. False: walk the direct way (RouteTo).
 static bool CoveredApproach(ACR_AICommander cmd, ACR_Squad squad, ACR_Base target, vector stage)
 {
  int started = System.GetTickCount();
  vector from = cmd.PositionOf(squad);
  vector center = target.GetOrigin();
  float ring = target.m_fRadius + ACR_AICommander.STAGING_DISTANCE;
  array<vector> eyes = {};
  ObjectiveEyes(center, target.m_fRadius, eyes);
  // Going round a strongpoint (ACR_AIEnvelopment): its eyes count too, and a wide way round it is tried.
  ACR_Base bypass = BypassFor(cmd, target);
  if (bypass) ObjectiveEyes(bypass.GetOrigin(), bypass.m_fRadius, eyes);
  array<vector> direct = {stage};
  float directSeen;
  float directLength = PathExposure(eyes, from, direct, directSeen);
  float directCost = directLength + directSeen * ACR_AICommander.COVER_EXPOSED_WEIGHT;
  array<vector> best;
  float bestCost = directCost * (1 - ACR_AICommander.COVER_MIN_GAIN);
  float bestLength;
  float bestSeen;
  // Only when the squad isn't already inside the ring (a squad that close takes the short way).
  if (directSeen > 0 && vector.DistanceXZ(from, center) > ring * 0.9)
  {
   for (int sense = -1; sense <= 1; sense += 2)
   {
    array<vector> arc = {};
    ArcLegs(center, from, stage, ring, sense, arc);
    bool usable = true;
    vector prev = from;
    foreach (int i, vector leg : arc)
    {
     arc[i] = ACR_Manager.Ground(leg);
     if (BBB_Terrain.WaterAlong(prev, arc[i]) > 0 || (i < arc.Count() - 1 && NearOtherEnemyBase(cmd, arc[i], target))) { usable = false; break; }
     prev = arc[i];
    }
    if (!usable) continue;
    float seen;
    float length = PathExposure(eyes, from, arc, seen);
    if (length > directLength * ACR_AICommander.COVER_MAX_STRETCH + 300) continue;
    float cost = length + seen * ACR_AICommander.COVER_EXPOSED_WEIGHT;
    if (cost >= bestCost) continue;
    best = arc;
    bestCost = cost;
    bestLength = length;
    bestSeen = seen;
   }
  }
  // Wide of the strongpoint: one waypoint round it at ACR_AIEnvelopment.CLEAR, then the staging point.
  vector wide = vector.Zero;
  if (bypass) wide = ACR_AIEnvelopment.WideLeg(from, stage, bypass.GetOrigin(), bypass.m_fRadius);
  if (wide != vector.Zero)
  {
   array<vector> round = {ACR_Manager.Ground(wide), stage};
   bool dry = !BBB_Terrain.IsWaterAt(round[0]) && BBB_Terrain.WaterAlong(from, round[0]) <= 0 && !NearOtherEnemyBase(cmd, round[0], target);
   float roundSeen;
   float roundLength = PathExposure(eyes, from, round, roundSeen);
   float roundCost = roundLength + roundSeen * ACR_AICommander.COVER_EXPOSED_WEIGHT;
   if (dry && roundLength <= directLength * ACR_AICommander.COVER_MAX_STRETCH + 300 && roundCost < bestCost)
   {
    best = round;
    bestCost = roundCost;
    bestLength = roundLength;
    bestSeen = roundSeen;
   }
  }
  ACR_Perf.Lap("ai.coverRoute", started);
  if (!best) return false;
  cmd.m_fCoverDetour = bestLength - directLength;
  squad.WalkLegs(best, ACR_Squad.WP_MOVE, -1, "Moving", 0);
  cmd.Say(string.Format("[ACR] AI %1: %2 takes a covered way to its staging point for %3: %4 m, %5 m in sight (direct %6 m, %7 m in sight)", cmd.m_sFaction, squad.GetName(), target.m_sName, (int)bestLength, (int)bestSeen, (int)directLength, (int)directSeen));
  return true;
 }

 // A staging point near 'point' on the same approach line that the objective's ground can't see:
 // a little further round or further out, never closer than 150 m past the objective's edge.
 // 'others': the attack's other lines' staging points; a nudge never turns within
 // APPROACH_SEPARATION of one (#52).
 static vector HiddenStaging(ACR_AICommander cmd, ACR_Base target, vector point, array<vector> others = null)
 {
  vector center = target.GetOrigin();
  array<vector> eyes = {};
  ObjectiveEyes(center, target.m_fRadius, eyes);
  if (!GroundSeen(eyes, point)) return point;
  vector dir = point - center;
  dir[1] = 0;
  float ring = dir.Length();
  if (ring < 1) return point;
  dir.Normalize();
  array<float> turns = {0, -10, 10, -20, 20};
  array<float> outs = {0, 60, -40, 120};
  foreach (float outward : outs)
  {
   float r = ring + outward;
   if (r < target.m_fRadius + 150) continue;
   foreach (float turn : turns)
   {
    if (outward == 0 && turn == 0) continue;
    vector candidate = ACR_Manager.Ground(center + Turned(dir, turn) * r);
    if (BBB_Terrain.IsWaterAt(candidate) || NearOtherEnemyBase(cmd, candidate, target)) continue;
    if (others && !ApartFrom(center, candidate, others)) continue;
    if (!GroundSeen(eyes, candidate)) return candidate;
   }
  }
  return point;
 }

 static vector Turned(vector dir, float degrees)
 {
  float angle = degrees * Math.DEG2RAD;
  return Vector(dir[0] * Math.Cos(angle) - dir[2] * Math.Sin(angle), 0, dir[0] * Math.Sin(angle) + dir[2] * Math.Cos(angle));
 }

 // Degrees between two directions on the ground.
 static float TurnBetween(vector a, vector b)
 {
  a[1] = 0;
  b[1] = 0;
  if (a.LengthSq() < 0.001 || b.LengthSq() < 0.001) return 0;
  a.Normalize();
  b.Normalize();
  return Math.Acos(Math.Clamp(vector.Dot(a, b), -1, 1)) * Math.RAD2DEG;
 }

 // The approach line for a squad joining this attack (slot index, -1: none found, stage is
 // then the main staging point). Squads already on lines are in attack.m_aSlot.
 static int PickApproach(ACR_AICommander cmd, ACR_AIAttack attack, vector from, out vector stage)
 {
  ACR_Base target = attack.m_Target;
  stage = attack.m_Staging;
  if (!target) return -1;
  vector center = target.GetOrigin();
  float ring = target.m_fRadius + ACR_AICommander.STAGING_DISTANCE;
  vector ahead = attack.m_vApproach;
  vector own = from - center;
  own[1] = 0;
  if (own.LengthSq() > 1) own.Normalize();
  else own = ahead;
  if (ahead.LengthSq() < 0.001) ahead = own;
  if (ahead.LengthSq() < 0.001) ahead = Vector(0, 0, 1);
  array<int> order = {};
  array<float> keys = {};
  foreach (int slot, float turn : ACR_AICommander.s_aApproachTurns)
  {
   int used = 0;
   foreach (int taken : attack.m_aSlot) if (taken == slot) used++;
   float key = used * 1000 + TurnBetween(Turned(ahead, turn), own);
   // Going round a strongpoint: lines that form up in its sight and fire come last.
   if (attack.m_Bypass && vector.DistanceXZ(center + Turned(ahead, turn) * ring, attack.m_Bypass.GetOrigin()) < attack.m_Bypass.m_fRadius + ACR_AIEnvelopment.CLEAR) key += 500;
   int at = 0;
   while (at < keys.Count() && keys[at] <= key) at++;
   order.InsertAt(slot, at);
   keys.InsertAt(key, at);
  }
  foreach (int pick : order)
  {
   vector candidate;
   if (!cmd.StagingPoint(target, Turned(ahead, ACR_AICommander.s_aApproachTurns[pick]), ring, candidate)) continue;
   // The other lines' staging points (a squad sharing this line shares its point).
   array<vector> others = {};
   foreach (int i, int other : attack.m_aSlot)
   {
    if (other == pick || other < 0 || i >= attack.m_aStaging.Count()) continue;
    others.Insert(attack.m_aStaging[i]);
   }
   if (!ApartFrom(center, candidate, others)) continue;
   // Form up out of the objective's sight where the ground allows (Jacob 26 Oct, Provins),
   // without closing in on another line, the ones no squad has taken yet included (#52).
   if (!target.m_sOwner.IsEmpty())
   {
    array<vector> keepClear = {};
    keepClear.Copy(others);
    foreach (int free, float freeTurn : ACR_AICommander.s_aApproachTurns)
    {
     if (free != pick && !attack.m_aSlot.Contains(free)) keepClear.Insert(center + Turned(ahead, freeTurn) * ring);
    }
    candidate = HiddenStaging(cmd, target, candidate, keepClear);
   }
   stage = candidate;
   return pick;
  }
  return -1;
 }

 // 'point' is at least APPROACH_SEPARATION degrees round 'center' from each of 'others'.
 static bool ApartFrom(vector center, vector point, notnull array<vector> others)
 {
  foreach (vector other : others)
   if (TurnBetween(other - center, point - center) < ACR_AICommander.APPROACH_SEPARATION) return false;
  return true;
 }

 // Distinct approach lines in use (for the log).
 static int ApproachLines(ACR_AIAttack attack)
 {
  array<int> lines = {};
  foreach (int slot : attack.m_aSlot) if (slot >= 0 && !lines.Contains(slot)) lines.Insert(slot);
  return Math.Max(1, lines.Count());
 }

 static bool NearOtherEnemyBase(ACR_AICommander cmd, vector point, ACR_Base except)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base == except || base.m_sOwner == cmd.m_sFaction || base.m_sOwner.IsEmpty() || !cmd.Visible(base)) continue;
   if (vector.DistanceXZ(point, base.GetOrigin()) < base.m_fRadius + ACR_AICommander.STAGING_CLEARANCE) return true;
  }
  return false;
 }
}
