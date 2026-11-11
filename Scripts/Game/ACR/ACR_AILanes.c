// AI commander strategy: an axis of advance (Jacob, 4 Nov). Before this the commander treated every
// base it could see as its own problem: grabs of empty bases went to whichever one a free squad
// reached first, and idle troops opened up to six more fronts, so most fights on a big map were one
// squad meeting one squad. Now, after a short land-rush of the empty bases near it
// (m_fRushMinutes), the commander picks a lane: a corridor from one of its front bases toward an
// objective on the far side, scored by the income along it against the defence it knows of there
// and its length. It then concentrates on it:
//   - attacks go to bases in the lane or on its flanks; others wait unless the lane is starved;
//   - one main attack plus the doctrine's m_iLaneFronts supporting attacks, not the extra fronts;
//   - empty bases are grabbed in the lane, on its flanks, and close behind its own bases only;
//   - idle squads post at the front bases in the lane, and the reserve sits behind it;
//   - tactical points (no income) on the lane's flanks come first for base works: anti-flank
//     strongpoints that let the push stay narrow.
// It changes lane when the objective falls, the start base is lost, the lane stalls (attacks in it
// called off, or nothing in it it can muster for), or once held a while, a clearly better lane
// opens. Defence and counterattacks come first, as before. The lane is told to its own side's 2IC
// channel and nowhere else: the enemy works it out from its intel, as a player would.
// On by default from 0.5.13 (#232; experimental and off in 0.5.9-0.5.12): -acrAIStrategy 0 or
// -acrAICommander classic (ACR_AIRework) turns it off; off is the commander from before. The geometry and scores here are pure
// functions of positions, so ACR_TestLanes can check them without a world.
class ACR_AILane : Managed
{
 ACR_Base m_From;        // own base the push starts from
 ACR_Base m_Objective;   // where it is going
 float m_fHalfWidth;     // m either side of the axis
 float m_fScore;
 float m_fSince = -1;    // world ms it was chosen
 int m_iSetbacks;        // attacks in it called off (a win takes one back)
 float m_fStarvedSince = -1; // world ms since it couldn't muster for its best target

 bool Valid(string faction)
 {
  if (!m_From || !m_Objective || m_From.m_bRetired || m_Objective.m_bRetired) return false;
  return m_From.m_sOwner == faction && m_Objective.m_sOwner != faction;
 }

 bool Contains(vector point) { return ACR_AILanes.InLane(m_From.GetOrigin(), m_Objective.GetOrigin(), m_fHalfWidth, point); }
 bool Flanks(vector point) { return ACR_AILanes.OnFlank(m_From.GetOrigin(), m_Objective.GetOrigin(), m_fHalfWidth, point); }
 bool Same(ACR_AILane other) { return other && other.m_From == m_From && other.m_Objective == m_Objective; }
}

class ACR_AILanes
{
 static const float MIN_LENGTH = 800;     // m: a lane shorter than this is just the next attack
 static const float FLANK_BAND = 2.2;     // the flanks run out to this x the half width
 static const float TACTICAL_GAIN = 0.5;  // a point without income is still ground worth a little
 static const float HQ_GAIN = 3;          // an enemy HQ as the objective
 static const float COST_SCALE = 10;      // known defenders that halve a lane's score
 static const float LENGTH_SCALE = 3000;  // m that halve a lane's score
 static const float KEEP_BONUS = 1.3;     // a new lane must beat the current one by this much
 static const float REVIEW_S = 120;       // s between lane reviews
 static const float HOLD_MIN = 8;         // min a lane is held before a better one can take over
 static const int SETBACKS = 2;           // attacks called off in it before it changes lane
 static const float STARVE_S = 360;       // s it waits to muster for a lane target before that counts as a setback
 static const float FAILED_MIN = 15;      // min a stalled objective's lanes score half
 static const float FAILED_FACTOR = 0.5;
 static const float REAR_GRAB = 1000;     // m: off-lane empty bases this close behind its own still get grabbed
 static const float FLANK_TARGET = 1.5;   // x attack score of an enemy base on the flank
 static const float INCOME_TARGET = 0.15; // each RP of income in the lane lowers its score by this share
 static const float POST_PULL = 2000;     // m an idle squad counts a front base in the lane as closer
 static const float RESERVE_PULL = 600;   // reserve score of an own base in or behind the lane

 // On unless the host turns it off (-acrAIStrategy 0, or the classic commander: ACR_AIRework).
 static bool On() { return ACR_LogisticsRules.Choice("acrAIStrategy", ACR_AIRework.Default("1")) == "1"; }

 // Metres along the axis from 'from' toward 'to' (negative behind 'from').
 static float Along(vector from, vector to, vector point)
 {
  float dx = to[0] - from[0];
  float dz = to[2] - from[2];
  float length = Math.Sqrt(dx * dx + dz * dz);
  if (length < 1) return 0;
  return ((point[0] - from[0]) * dx + (point[2] - from[2]) * dz) / length;
 }

 // Metres sideways off the axis line, either side.
 static float Off(vector from, vector to, vector point)
 {
  float dx = to[0] - from[0];
  float dz = to[2] - from[2];
  float length = Math.Sqrt(dx * dx + dz * dz);
  if (length < 1) return vector.DistanceXZ(from, point);
  return Math.AbsFloat((point[0] - from[0]) * dz - (point[2] - from[2]) * dx) / length;
 }

 // Within the corridor: between its ends (half a width of slack past each) and its half width off the axis.
 static bool InLane(vector from, vector to, float halfWidth, vector point)
 {
  if (!Between(from, to, halfWidth, point)) return false;
  return Off(from, to, point) <= halfWidth;
 }

 // Beside the corridor: between its ends, past its edge but within FLANK_BAND half widths.
 static bool OnFlank(vector from, vector to, float halfWidth, vector point)
 {
  if (!Between(from, to, halfWidth, point)) return false;
  float off = Off(from, to, point);
  return off > halfWidth && off <= halfWidth * FLANK_BAND;
 }

 protected static bool Between(vector from, vector to, float halfWidth, vector point)
 {
  float along = Along(from, to, point);
  return along >= -halfWidth && along <= vector.DistanceXZ(from, to) + halfWidth;
 }

 // What a base in the lane is worth taking: its income, or a little for a tactical point.
 static float Gain(bool income, int value)
 {
  if (income && value > 0) return value;
  return TACTICAL_GAIN;
 }

 // A lane's score: what it wins over the defence it knows of and how far it goes.
 static float Score(float gain, float cost, float length)
 {
  if (gain <= 0) return 0;
  return gain / (1 + cost / COST_SCALE) / (1 + length / LENGTH_SCALE);
 }

 // Whether a review should change lane: always for a broken or stalled one, else only for a clearly
 // better one after the current one has been held HOLD_MIN.
 static bool ShouldSwitch(bool valid, int setbacks, float heldMin, float currentScore, float bestScore)
 {
  if (!valid || setbacks >= SETBACKS) return true;
  if (heldMin < HOLD_MIN) return false;
  return bestScore > currentScore * KEEP_BONUS;
 }

 // An attack target's score multiplier for the lane (lower = sooner): in it, cheaper the more income
 // it brings; on its flanks a little dearer; elsewhere -1 (left alone while the lane has work).
 static float TargetFactor(bool inLane, bool onFlank, bool income, int value)
 {
  if (inLane)
  {
   if (!income || value <= 0) return 0.5;
   return 0.5 / (1 + value * INCOME_TARGET);
  }
  if (onFlank) return FLANK_TARGET;
  return -1;
 }
}
