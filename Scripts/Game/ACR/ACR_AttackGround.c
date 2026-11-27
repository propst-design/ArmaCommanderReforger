// The ground picture of one attack (Jacob, 4 Nov 2026: no set distances, the commander reads the
// ground). Built once when an attack on a held base launches and again when intel moves the enemy,
// a little per frame. It only describes the ground; nothing acts on it yet (design: docs/handoff/
// ai-allocator-design.md, "Execution: an attack is one fight", step 1). -acrAttackGround 1 builds
// it, logs it and shows it in AC:A Think; 2 also draws it in the world where the server renders
// (solo or a listen host).
//
// Enemy eyes: the objective's eye points plus every enemy position this side has seen near it.
// Samples: points on spokes fanned out from the objective toward the attackers' side. A sample is
// dead ground when no eye can see a crouching man there (terrain, buildings, walls and rocks block
// sight; trees and bushes are concealment, not cover, and don't count). Seen samples can be shot
// from the eyes and can shoot back.
// Per spoke, walking in from the outside: the line of departure is the last dead sample before
// the first seen one; dead samples further in are covered points (bounds); a seen sample with dead
// ground just behind it is a firing position (a crest a squad can fire from and drop back behind).
class ACR_GroundSample
{
 vector m_vPos;
 int m_iSpoke;
 int m_iStep;
 int m_iSeenBy;       // eyes that can see a crouching man here (-1: not traced yet)
 bool m_bWater;
}

class ACR_GroundSpoke
{
 float m_fAngle;        // radians, world XZ (x = cos, z = sin)
 int m_iFirst;          // index of its innermost sample in m_aSamples
 int m_iCount;
 int m_iDeparture = -1; // sample index of its line of departure (-1: exposed from the outer end, or no break)
 int m_iBreak = -1;     // first seen sample walking in (-1: dead all the way in)
 int m_iCovered;        // dead samples inside the break: covered points to bound to
}

class ACR_AttackGround
{
 // Sampling resolution (how finely the ground is read, not how the attack moves).
 static const float SAMPLE_STEP = 25;      // m between samples along a spoke
 static const float SPOKE_STEP = 10;       // degrees between spokes
 static const float SECTOR = 120;          // degrees either side of the main approach
 static const float EYE_RANGE = 800;       // m: beyond this an eye isn't counted (rifle and MG reach)
 static const float BODY = 1.0;            // m above ground: a crouching man
 static const float EYE_HEIGHT = 1.7;      // m above ground for a seen enemy's eyes
 static const int MAX_ENEMY_EYES = 8;
 static const int TRACES_PER_PUMP = 160;   // sight traces per frame step while building
 static const int MAX_FIRING = 8;

 ACR_Base m_Target;
 string m_sFaction;
 vector m_vCenter;
 vector m_vApproach;
 float m_fInner;
 float m_fExtent;
 ref array<vector> m_aEyes = {};
 int m_iObjectiveEyes;
 ref array<ref ACR_GroundSample> m_aSamples = {};
 ref array<ref ACR_GroundSpoke> m_aSpokes = {};
 ref array<int> m_aFiring = {};            // sample indices, best first
 ref array<float> m_aFiringScore = {};
 int m_iNext;                              // next sample to trace
 int m_iNextEye;                           // next eye of that sample
 int m_iSeenAcc;
 bool m_bDone;
 int m_iTraces;
 int m_iBuildMs;
 float m_fBuiltAt;
 int m_iBuilds;
 ref array<ref Shape> m_aShapes;

 // Grounds being built, pumped from the call queue (static-init budget: filled in ACR_InitStatics).
 protected static ref array<ACR_AttackGround> s_Building;
 protected static bool s_bPumping;
 static void ACR_InitStatics()
 {
  s_Building = new array<ACR_AttackGround>();
  s_bPumping = false;
 }

 // 0 off, 1 build + log + Think, 2 also draw.
 static int Mode() { return Math.ClampInt(ACR_LogisticsRules.Choice("acrAttackGround", "0").ToInt(), 0, 2); }
 static bool On() { return Mode() > 0; }

 void ACR_AttackGround(ACR_Base target, string faction, vector approach, float extent)
 {
  m_Target = target;
  m_sFaction = faction;
  m_vCenter = ACR_Manager.Ground(target.GetOrigin());
  approach[1] = 0;
  if (approach.LengthSq() < 0.01) approach = Vector(0, 0, 1);
  approach.Normalize();
  m_vApproach = approach;
  m_fInner = Math.Max(target.m_fRadius, SAMPLE_STEP);
  m_fExtent = Math.Clamp(extent, m_fInner + 8 * SAMPLE_STEP, m_fInner + EYE_RANGE);
  BuildSamples();
 }

 void ~ACR_AttackGround()
 {
  if (s_Building) s_Building.RemoveItem(this);
 }

 protected void BuildSamples()
 {
  m_aSamples.Clear();
  m_aSpokes.Clear();
  float mid = Math.Atan2(m_vApproach[2], m_vApproach[0]);
  int half = (int)(SECTOR / SPOKE_STEP);
  int steps = (int)Math.Floor((m_fExtent - m_fInner) / SAMPLE_STEP) + 1;
  for (int s = -half; s <= half; s++)
  {
   ACR_GroundSpoke spoke = new ACR_GroundSpoke();
   spoke.m_fAngle = mid + s * SPOKE_STEP * Math.DEG2RAD;
   spoke.m_iFirst = m_aSamples.Count();
   vector dir = Vector(Math.Cos(spoke.m_fAngle), 0, Math.Sin(spoke.m_fAngle));
   for (int k = 0; k < steps; k++)
   {
    ACR_GroundSample sample = new ACR_GroundSample();
    sample.m_vPos = ACR_Manager.Ground(m_vCenter + dir * (m_fInner + k * SAMPLE_STEP));
    sample.m_iSpoke = m_aSpokes.Count();
    sample.m_iStep = k;
    sample.m_iSeenBy = -1;
    sample.m_bWater = BBB_Terrain.IsWaterAt(sample.m_vPos);
    m_aSamples.Insert(sample);
   }
   spoke.m_iCount = steps;
   m_aSpokes.Insert(spoke);
  }
 }

 // The eyes: the objective's own, then the seen enemies (nearest the objective first). A changed
 // set (an enemy seen somewhere no eye was) restarts the build. True when it restarted.
 bool SetEyes(notnull array<vector> enemies)
 {
  array<vector> eyes = {};
  ACR_AICommander.ObjectiveEyes(m_vCenter, m_Target.m_fRadius, eyes);
  int objective = eyes.Count();
  array<vector> sorted = {};
  foreach (vector e : enemies) sorted.Insert(e);
  for (int a = 0; a < sorted.Count() && a < MAX_ENEMY_EYES; a++)
  {
   int best = a;
   for (int b = a + 1; b < sorted.Count(); b++)
    if (vector.DistanceXZ(sorted[b], m_vCenter) < vector.DistanceXZ(sorted[best], m_vCenter)) best = b;
   if (best != a) sorted.SwapItems(a, best);
   vector eye = ACR_Manager.Ground(sorted[a]);
   eye[1] = eye[1] + EYE_HEIGHT;
   eyes.Insert(eye);
  }
  bool changed = eyes.Count() != m_aEyes.Count();
  if (!changed)
  {
   foreach (vector next : eyes)
   {
    bool near = false;
    foreach (vector old : m_aEyes)
     if (vector.DistanceXZ(old, next) < SAMPLE_STEP * 2) near = true;
    if (!near) changed = true;
   }
  }
  if (!changed && m_iBuilds > 0) return false;
  m_aEyes = eyes;
  m_iObjectiveEyes = objective;
  Restart();
  return true;
 }

 protected void Restart()
 {
  foreach (ACR_GroundSample sample : m_aSamples) sample.m_iSeenBy = -1;
  m_iNext = 0;
  m_iNextEye = 0;
  m_iSeenAcc = 0;
  m_bDone = false;
  m_iTraces = 0;
  m_iBuildMs = 0;
  m_aFiring.Clear();
  m_aFiringScore.Clear();
  if (!s_Building.Contains(this)) s_Building.Insert(this);
  if (!s_bPumping)
  {
   s_bPumping = true;
   GetGame().GetCallqueue().CallLater(Pump, 50, true);
  }
 }

 // Spends a few traces on every ground being built; stops itself when nothing is left.
 static void Pump()
 {
  for (int i = s_Building.Count() - 1; i >= 0; i--)
  {
   ACR_AttackGround ground = s_Building[i];
   if (!ground || ground.Step(TRACES_PER_PUMP)) s_Building.Remove(i);
  }
  if (s_Building.IsEmpty())
  {
   s_bPumping = false;
   GetGame().GetCallqueue().Remove(Pump);
  }
 }

 // Builds it all now (tests).
 void BuildNow()
 {
  if (s_Building) s_Building.RemoveItem(this);
  bool done = false;
  while (!done) done = Step(100000);
 }

 // Traces up to 'budget' sample-eye pairs. True when the picture is complete.
 bool Step(int budget)
 {
  if (m_bDone) return true;
  int started = System.GetTickCount();
  BaseWorld world = GetGame().GetWorld();
  int used = 0;
  while (m_iNext < m_aSamples.Count() && used < budget)
  {
   ACR_GroundSample sample = m_aSamples[m_iNext];
   if (m_iNextEye < m_aEyes.Count())
   {
    vector eye = m_aEyes[m_iNextEye];
    m_iNextEye++;
    if (vector.DistanceXZ(eye, sample.m_vPos) > EYE_RANGE) continue;
    used++;
    if (Sees(world, eye, sample.m_vPos + Vector(0, BODY, 0))) m_iSeenAcc++;
    continue;
   }
   sample.m_iSeenBy = m_iSeenAcc;
   m_iSeenAcc = 0;
   m_iNextEye = 0;
   m_iNext++;
  }
  m_iTraces += used;
  m_iBuildMs += System.GetTickCount() - started;
  ACR_Perf.Lap("ai.ground", started);
  if (m_iNext < m_aSamples.Count()) return false;
  Derive();
  m_bDone = true;
  m_iBuilds++;
  m_fBuiltAt = GetGame().GetWorld().GetWorldTime();
  if (Mode() >= 2) Draw();
  return true;
 }

 // Sight from 'eye' to 'body': terrain, buildings, walls and rocks block it; people, vehicles,
 // trees and bushes don't (they hide a man, they don't stop a bullet).
 static bool Sees(BaseWorld world, vector eye, vector body)
 {
  TraceParam trace = new TraceParam();
  trace.Start = eye;
  trace.End = body;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  return world.TraceMove(trace, SightFilter) >= 1;
 }

 protected static bool SightFilter(notnull IEntity e)
 {
  if (ChimeraCharacter.Cast(e) || Vehicle.Cast(e) || Tree.Cast(e)) return false;
  return ACR_Garrison.SolidFilter(e);
 }

 // Lines of departure, breaks, covered points and firing positions from the traced samples.
 protected void Derive()
 {
  foreach (ACR_GroundSpoke spoke : m_aSpokes)
  {
   spoke.m_iDeparture = -1;
   spoke.m_iBreak = -1;
   spoke.m_iCovered = 0;
   // Walk in from the outer end.
   for (int k = spoke.m_iCount - 1; k >= 0; k--)
   {
    ACR_GroundSample sample = m_aSamples[spoke.m_iFirst + k];
    if (sample.m_bWater) continue;
    bool dead = sample.m_iSeenBy == 0;
    if (spoke.m_iBreak < 0)
    {
     if (dead) { spoke.m_iDeparture = spoke.m_iFirst + k; continue; }
     spoke.m_iBreak = spoke.m_iFirst + k;
     continue;
    }
    if (dead) spoke.m_iCovered++;
   }
   // Firing positions: seen samples with dead ground just outside them (the next sample out).
   for (int f = 0; f + 1 < spoke.m_iCount; f++)
   {
    ACR_GroundSample firing = m_aSamples[spoke.m_iFirst + f];
    ACR_GroundSample behind = m_aSamples[spoke.m_iFirst + f + 1];
    if (firing.m_bWater || firing.m_iSeenBy <= 0 || behind.m_bWater || behind.m_iSeenBy != 0) continue;
    AddFiring(spoke.m_iFirst + f, FiringScore(firing));
   }
  }
 }

 // How good a crest is to fire from: how many eyes it sees, and height over the objective.
 protected float FiringScore(ACR_GroundSample sample)
 {
  float height = sample.m_vPos[1] - m_vCenter[1];
  return sample.m_iSeenBy + Math.Clamp(height / 10.0, -1.0, 3.0);
 }

 protected void AddFiring(int index, float score)
 {
  int at = m_aFiring.Count();
  for (int i = 0; i < m_aFiring.Count(); i++)
   if (score > m_aFiringScore[i]) { at = i; break; }
  if (at >= MAX_FIRING) return;
  m_aFiring.InsertAt(index, at);
  m_aFiringScore.InsertAt(score, at);
  if (m_aFiring.Count() > MAX_FIRING)
  {
   m_aFiring.Remove(MAX_FIRING);
   m_aFiringScore.Remove(MAX_FIRING);
  }
 }

 // ------------------------------------------------------------------ reading it

 bool IsDone() { return m_bDone; }

 int DeadCount()
 {
  int dead;
  foreach (ACR_GroundSample sample : m_aSamples)
   if (!sample.m_bWater && sample.m_iSeenBy == 0) dead++;
  return dead;
 }

 int LandCount()
 {
  int land;
  foreach (ACR_GroundSample sample : m_aSamples)
   if (!sample.m_bWater) land++;
  return land;
 }

 // The spoke nearest the bearing from the objective to 'point'.
 ACR_GroundSpoke SpokeToward(vector point)
 {
  vector d = point - m_vCenter;
  float angle = Math.Atan2(d[2], d[0]);
  ACR_GroundSpoke best;
  float bestGap = 999;
  foreach (ACR_GroundSpoke spoke : m_aSpokes)
  {
   float gap = Math.AbsFloat(angle - spoke.m_fAngle);
   while (gap > Math.PI) gap = Math.AbsFloat(gap - Math.PI2);
   if (gap < bestGap) { bestGap = gap; best = spoke; }
  }
  return best;
 }

 // Where a squad coming from 'point' crosses its line of departure. False: exposed from the outer
 // end of that spoke, or dead ground all the way in.
 bool DepartureToward(vector point, out vector departure)
 {
  ACR_GroundSpoke spoke = SpokeToward(point);
  if (!spoke || spoke.m_iDeparture < 0 || spoke.m_iBreak < 0) return false;
  departure = m_aSamples[spoke.m_iDeparture].m_vPos;
  return true;
 }

 // Where a squad coming from 'point' first enters enemy sight (the exposure break: ideal smoke point).
 bool BreakToward(vector point, out vector breakPos)
 {
  ACR_GroundSpoke spoke = SpokeToward(point);
  if (!spoke || spoke.m_iBreak < 0) return false;
  breakPos = m_aSamples[spoke.m_iBreak].m_vPos;
  return true;
 }

 // Best scored crest firing position within [minRange, maxRange] of the objective and reachable from 'from'.
 bool BestFiringToward(vector from, float minRange, float maxRange, out vector spot)
 {
  if (!m_bDone || m_aFiring.IsEmpty()) return false;
  float bestMetric = -999999;
  bool found = false;
  foreach (int idx, int sampleIdx : m_aFiring)
  {
   ACR_GroundSample s = m_aSamples[sampleIdx];
   if (!s || s.m_bWater) continue;
   float distObj = vector.DistanceXZ(s.m_vPos, m_vCenter);
   if (distObj < minRange || distObj > maxRange) continue;
   float walk = vector.DistanceXZ(s.m_vPos, from);
   if (walk > maxRange + 250) continue;
   float metric = m_aFiringScore[idx] * 40.0 - walk * 0.15;
   if (!found || metric > bestMetric)
   {
    bestMetric = metric;
    spot = s.m_vPos;
    found = true;
   }
  }
  return found;
 }

 // One line for Think and the log.
 string Summary(notnull array<vector> approaches)
 {
  if (!m_bDone) return string.Format("ground %1: reading (%2/%3 points)", m_Target.m_sName, m_iNext, m_aSamples.Count());
  int land = LandCount();
  int dead = DeadCount();
  int share = 0;
  if (land > 0) share = Math.Round(100.0 * dead / land);
  string lines;
  array<int> said = {};
  foreach (vector from : approaches)
  {
   ACR_GroundSpoke spoke = SpokeToward(from);
   if (!spoke) continue;
   int index = m_aSpokes.Find(spoke);
   if (said.Contains(index)) continue;
   said.Insert(index);
   int bearing = Math.Round(Math.Atan2(Math.Cos(spoke.m_fAngle), Math.Sin(spoke.m_fAngle)) * Math.RAD2DEG);
   if (bearing < 0) bearing += 360;
   if (spoke.m_iBreak < 0) lines += string.Format(" | from %1 deg: covered all the way in", bearing);
   else if (spoke.m_iDeparture < 0) lines += string.Format(" | from %1 deg: in sight from %2 m out", bearing, Math.Round(vector.DistanceXZ(m_aSamples[spoke.m_iBreak].m_vPos, m_vCenter)));
   else lines += string.Format(" | from %1 deg: departure %2 m out at %3, %4 covered points further in", bearing, Math.Round(vector.DistanceXZ(m_aSamples[spoke.m_iDeparture].m_vPos, m_vCenter)), ACR_Manager.Grid(m_aSamples[spoke.m_iDeparture].m_vPos), spoke.m_iCovered);
  }
  string fire = "none";
  if (!m_aFiring.IsEmpty())
  {
   ACR_GroundSample top = m_aSamples[m_aFiring[0]];
   fire = string.Format("%1 (best %2: sees %3 eyes, %4 m above)", m_aFiring.Count(), ACR_Manager.Grid(top.m_vPos), top.m_iSeenBy, Math.Round(top.m_vPos[1] - m_vCenter[1]));
  }
  return string.Format("ground %1: %2 eyes (%3 seen enemies), dead ground %4 percent of %5 points, firing positions %6%7 [%8 traces, %9 ms]", m_Target.m_sName, m_aEyes.Count(), m_aEyes.Count() - m_iObjectiveEyes, share, land, fire, lines, m_iTraces, m_iBuildMs);
 }

 // ------------------------------------------------------------------ drawing (mode 2)

 // Green: dead ground. Red: seen. Blue: firing positions. Yellow: lines of departure. White: eyes.
 void Draw()
 {
  m_aShapes = new array<ref Shape>();
  ShapeFlags flags = ShapeFlags.NOOUTLINE | ShapeFlags.TRANSP;
  foreach (ACR_GroundSample sample : m_aSamples)
  {
   if (sample.m_bWater) continue;
   int colour = ARGB(128, 255, 48, 48);
   if (sample.m_iSeenBy == 0) colour = ARGB(128, 48, 255, 48);
   m_aShapes.Insert(Shape.CreateSphere(colour, flags, sample.m_vPos + Vector(0, 0.5, 0), 1.2));
  }
  foreach (int f : m_aFiring)
   m_aShapes.Insert(Shape.CreateSphere(ARGB(192, 48, 112, 255), flags | ShapeFlags.NOZBUFFER, m_aSamples[f].m_vPos + Vector(0, 2, 0), 3));
  foreach (ACR_GroundSpoke spoke : m_aSpokes)
   if (spoke.m_iDeparture >= 0 && spoke.m_iBreak >= 0)
    m_aShapes.Insert(Shape.CreateSphere(ARGB(192, 255, 224, 48), flags | ShapeFlags.NOZBUFFER, m_aSamples[spoke.m_iDeparture].m_vPos + Vector(0, 2, 0), 3));
  foreach (vector eye : m_aEyes)
   m_aShapes.Insert(Shape.CreateSphere(ARGB(192, 255, 255, 255), flags | ShapeFlags.NOZBUFFER, eye, 2));
 }
}
