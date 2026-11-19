// AI commander: going round a strongpoint (Jacob, 19 Nov; Kamenoe: the USSR assaulted the US's
// strongest base head-on, lost 36 to 6 and ended with no combat-ready squad, while thinly held
// Soapograd beside it was the US's own main-effort base). The commander now also weighs going round:
// an enemy base is a strongpoint when the force it wants is more than the side can field, or it has
// just beaten an attack; a weakly held enemy base beside or behind it (within REACH) gets an
// envelopment bonus, most when taking it cuts the strongpoint off from the rest of its side's bases
// (Cut: a link graph of the bases the commander knows that side holds) or puts us in its rear (Rear).
// The bonus is a term in the existing scoring, not a second planner: attack target scores and
// allocator job values (ACR_AICommander.EnvelopBonus), lane scores (LaneScore), and the lane lets such
// a base in as a flank target. Approaches to it keep out of the strongpoint's sight and fire
// (LaunchAttack's main approach, PickApproach, CoveredApproach). Odds, doctrine pressure, truce
// and HQ rules are unchanged: the bonus only reorders what the commander may already attack.
// Pure functions of positions and numbers here, so ACR_TestLanes checks them without a world.
class ACR_AIEnvelopment
{
 static const float REACH = 2200;         // m: an enemy base this close to a strongpoint can go round it
 static const float LINK = 2500;          // m: bases of one side this close support each other
 static const float WEAK_SHARE = 0.5;     // a weak base: known defence at most this share of the strongpoint's
 static const float STRONG_SHARE = 0.75;  // a strongpoint wants more than this share of our combat strength
 static const int STRONG_MIN = 8;         // ... and is known to hold at least this many
 static const float REPULSED_MIN = 20;    // min a base that beat our attack counts as a strongpoint
 static const float GAIN = 1.5;           // bonus at a full cut or rear, Balanced doctrine
 static const float BASE_SHARE = 0.25;    // share of the gain any weak base beside a strongpoint gets
 static const float CLEAR = 700;          // m an approach keeps from the strongpoint's centre (its eyes see 800 m)
 static const int MAX_STRONG = 4;         // strongpoints weighed per update (the strongest first)

 // A strongpoint: wanting 'wanted' strength against our 'strength', 'known' defenders seen there, or
 // it just beat an attack of ours.
 static bool Strongpoint(float wanted, float strength, int known, bool repulsed)
 {
  if (repulsed) return true;
  if (known < STRONG_MIN) return false;
  return wanted > strength * STRONG_SHARE;
 }

 // Weakly held beside a strongpoint of 'strongDefence'.
 static bool Weak(int defence, int strongDefence)
 {
  return strongDefence * WEAK_SHARE >= defence;
 }

 // Share of the strongpoint's network (its side's bases it links to, directly or through others,
 // links within LINK) cut off from it once 'cand' falls, 'cand' itself not counted. A strongpoint
 // whose only link is 'cand' is cut off completely (1).
 static float Cut(vector strong, vector cand, notnull array<vector> friends)
 {
  array<vector> nodes = {};
  nodes.Insert(strong);
  nodes.Insert(cand);
  foreach (vector f : friends) nodes.Insert(f);
  int before = Reached(nodes, -1);
  if (before <= 1) return 0;   // cand isn't even linked to it
  if (before == 2) return 1;   // cand is its only link
  int after = Reached(nodes, 1);
  float lost = before - after - 1;
  return Math.Clamp(lost / (before - 2), 0, 1);
 }

 // Nodes reached from nodes[0] over links within LINK, leaving out node 'skip' (-1: none).
 protected static int Reached(notnull array<vector> nodes, int skip)
 {
  array<bool> seen = {};
  foreach (vector n : nodes) seen.Insert(false);
  array<int> queue = {0};
  seen[0] = true;
  int count = 1;
  while (!queue.IsEmpty())
  {
   int at = queue[0];
   queue.RemoveOrdered(0);
   foreach (int i, vector other : nodes)
   {
    if (seen[i] || i == skip || vector.DistanceXZ(nodes[at], other) > LINK) continue;
    seen[i] = true;
    count++;
    queue.Insert(i);
   }
  }
  return count;
 }

 // Share of the strongpoint's support lines (to each of its side's bases within 1.5 x LINK) that pass
 // by 'cand': it lies in the corridor between them (a third of the line's length either side).
 static float Astride(vector strong, vector cand, notnull array<vector> friends)
 {
  int lines, crossed;
  foreach (vector f : friends)
  {
   float length = vector.DistanceXZ(strong, f);
   if (length < 1 || length > LINK * 1.5) continue;
   lines++;
   if (ACR_AILanes.InLane(strong, f, length / 3, cand) && ACR_AILanes.Along(strong, f, cand) > 0) crossed++;
  }
  if (lines == 0) return 0;
  float share = crossed;
  return share / lines;
 }

 // How far into the strongpoint's rear 'cand' is, seen from our base 'front': 0 short of half way
 // along, 0.5 level with it, 1 a quarter of the way past it or more.
 static float Rear(vector front, vector strong, vector cand)
 {
  float length = vector.DistanceXZ(front, strong);
  if (length < 1) return 0;
  float along = ACR_AILanes.Along(front, strong, cand);
  return Math.Clamp((along - length * 0.75) / (length * 0.5), 0, 1);
 }

 // The score bonus (x value, / target score) for going round: 1 + gain x weight, the gain BASE_SHARE
 // for any weak base beside a strongpoint and full for a complete cut or the rear. 'weight': the
 // doctrine's (Firepower is patient: less).
 static float Bonus(float cut, float rear, float weight)
 {
  if (weight <= 0) return 1;
  float how = Math.Max(Math.Clamp(cut, 0, 1), Math.Clamp(rear, 0, 1));
  return 1 + GAIN * weight * (BASE_SHARE + (1 - BASE_SHARE) * how);
 }

 // Main approach for an attack on 'target' from 'from' going round 'strong': the side facing the
 // attackers, turned up to 90 degrees away from the strongpoint until its staging point (at 'ring')
 // is at least CLEAR from it, the smallest turn that does it (else the farthest).
 static vector ApproachAway(vector target, vector from, vector strong, float ring)
 {
  vector dir = from - target;
  dir[1] = 0;
  if (dir.LengthSq() < 1) dir = target - strong;
  dir[1] = 0;
  if (dir.LengthSq() < 0.001) return Vector(0, 0, 1);
  dir.Normalize();
  vector best = dir;
  float bestGap = vector.DistanceXZ(target + dir * ring, strong);
  if (bestGap >= CLEAR) return dir;
  array<float> turns = {30, -30, 60, -60, 90, -90};
  foreach (float turn : turns)
  {
   vector turned = ACR_AICommander.Turned(dir, turn);
   float gap = vector.DistanceXZ(target + turned * ring, strong);
   if (gap >= CLEAR) return turned;
   if (gap > bestGap)
   {
    bestGap = gap;
    best = turned;
   }
  }
  return best;
 }

 // A waypoint taking a walk from 'from' to 'to' round 'strong' at CLEAR (plus its radius), on the
 // side the straight line passes it; vector.Zero when the straight line keeps clear anyway.
 static vector WideLeg(vector from, vector to, vector strong, float radius)
 {
  float clear = radius + CLEAR;
  float length = vector.DistanceXZ(from, to);
  if (length < 1) return vector.Zero;
  float along = ACR_AILanes.Along(from, to, strong);
  if (along <= 0 || along >= length) return vector.Zero;
  if (ACR_AILanes.Off(from, to, strong) >= clear) return vector.Zero;
  vector dir = to - from;
  dir[1] = 0;
  dir.Normalize();
  vector closest = from + dir * along;
  vector side = closest - strong;
  side[1] = 0;
  if (side.LengthSq() < 1) side = Vector(-dir[2], 0, dir[0]);
  side.Normalize();
  return strong + side * clear;
 }
}
