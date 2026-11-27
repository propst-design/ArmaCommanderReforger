// Defensive line geometry and line-walking solver (ACR_LinePiece, ACR_BaseLine, ACR_BaseLiner); split from ACR_BaseLines.c.

class ACR_LinePiece : Managed
{
 vector m_vPos;        // its middle, on the ground
 float m_fYaw;
 ResourceName m_sPrefab;
 vector m_vFrom;       // its ends, in the order along the line
 vector m_vTo;
 vector m_vFacing;     // the line's firing side here
 int m_iBreak;         // what follows it: 0 the next piece, 1 a walk-through gap, 2 a road
 bool m_bCurve;        // part of an end curving back
 bool m_bLong;
 float m_fLook = -1;   // m it sees out from behind its bags, up to ACR_BaseLiner.LOOK_FAR; -1 not looked
}

class ACR_BaseLine : Managed
{
 int m_iTier;
 int m_iSector;
 ref array<ref ACR_LinePiece> m_aPieces = {};  // in order, once it's chosen
 ref array<ref ACR_LinePiece> m_aLeft = {};    // the two ways from the seed, before that
 ref array<ref ACR_LinePiece> m_aRight = {};
 string m_sEndA;                               // what each end stands on: building, wall, rock, slope, road, work, line, curve
 string m_sEndB;
 ref array<vector> m_aLanes = {};              // where a road goes through it, and which way is out along it
 ref array<vector> m_aLaneOut = {};
 vector m_vMiddle;
 vector m_vFacing;
 float m_fLength;
 float m_fView;
 float m_fSeeing = 1;                          // its share of long pieces that see past SEES_FAR
 bool m_bGap;                                  // closes a gap in a wall
 int m_iSeam;                                  // what stands between its two walks' first pieces (BREAK_*)
 float m_fLookMean;                            // m its pieces see out, on average (Fit's trace, LOOK_FAR at most)
 bool m_bPlug;                                 // straight across a way in, hemmed in: what it watches is the road
 float m_fRise;                                // m its middle stands over the ground round it (negative: a hollow)
 int m_iDeadFront;                             // of the spots 20-65 m in front of its middle, those it can't see
 bool m_bRing;                                  // a stretch of the line that closes the town (ACR_BaseRing)
 int m_iSites;

 int Count() { return m_aLeft.Count() + m_aRight.Count(); }

 // Both ends stand on a wall, a building or another work: it's closing a gap, whatever its length. (Bags
 // between two rocks or down to a road aren't.)
 bool Closes()
 {
  return Solid(m_sEndA) && Solid(m_sEndB);
 }

 protected bool Solid(string word)
 {
  return word == "wall" || word == "building" || word == "work" || word == "line";
 }

 int Anchors()
 {
  int anchors;
  if (!m_sEndA.IsEmpty() && m_sEndA != "curve") anchors++;
  if (!m_sEndB.IsEmpty() && m_sEndB != "curve") anchors++;
  return anchors;
 }
}

// Walks one candidate line.
class ACR_BaseLiner : Managed
{
 static const int FIT_OK = 0;
 static const int FIT_ROAD = 1;
 static const int FIT_BUILDING = 2;
 static const int FIT_SOLID = 3;
 static const int FIT_SLOPE = 4;
 static const int FIT_DOOR = 5;
 static const int FIT_VIEW = 6;
 static const int FIT_WATER = 7;
 static const int FIT_BEHIND = 8;
 static const int FIT_PAVED = 9;
 static const int FIT_BOXED = 10;
 static const float ROOM = 1.6;             // m clear behind the bags: a man stands and fires there
 static const float VIEW = 15;              // m clear on a piece's firing side, straight out or a little to either side
 static const float LOOK_FAR = 45;          // m a piece is looked out from, and what's kept of it
 static const float SEES_FAR = 25;          // m: seeing less it watches a fence, not an approach
 static const float BOX = 25;               // m: less than this in front and behind is a yard, not a place for a line
 static const int MAX_BLIND = 2;            // pieces in a row that may go without (a tree in front of one)
 static const float GENTLE = 5.5;           // degrees at a joint while nothing's in the way
 static const float MAX_RISE = 1.0;         // m along a long piece (its ground tilt tops at 12 degrees: a little buried)
 static const float MAX_LEAN = 0.7;         // m across it
 static const float ROAD_KEEP = 0.6;        // m from a road's edge to a piece
 static const float WIDE_PAVED = 18;        // m of concrete ahead that make it a runway or an apron, not a yard
 static const float BAY_TURN = 25;          // degrees: a fire bay steps a long piece's width in or out
 static const float RING_PAST = 15;         // m a stretch of the ring looks on past what's in its way
 static const float RING_PASSED = 11;       // m from a point it's heading for that it may have gone past
 static const float RING_PAST_ROAD = 27;    // ... and past a road it meets at a slant
 static const float RING_TURN = 14;         // degrees at a joint on its way there: a wide curve, its joints closed
 int m_iReturns;       // long pieces an open end bends back with after its round piece: a redoubt's sides
 int m_iBayEvery;      // long pieces between fire bays, 0 none
 float m_fBaySide = 1; // 1 out towards the enemy, -1 back
 protected int m_iSinceBay;
 protected int m_iBayStep;
 static int s_iBaysForTest;

 ACR_BaseSurvey m_Survey;
 ACR_BaseWork m_Work;
 ref array<ref ACR_BaseDefenceSeg> m_aRoads;
 ref array<IEntity> m_aHouses;
 float m_fOffset;      // m past the town's edge
 float m_fWobble;      // m the offset swings by along the line
 float m_fPhase;
 float m_fSeed;        // the bearing it started from
 float m_fSpan;        // degrees it may run from there
 bool m_bTarget;       // straight for a point
 bool m_bOpenOnly;     // ... and still not through a yard: a funnel's arm, not a gap being closed
 bool m_bRing;         // a stretch of the ring: it has to get there, so what it sees doesn't stop it (the plan looked)
 ref array<vector> m_aWay; // ... through these, m_vTarget the one it's heading for
 int m_iWay;
 vector m_vTarget;
 string m_sTargetWord;
 int m_iStop;          // the last walk: what ended it (a FIT_ code), -1 its length or span
 protected bool m_bBlindStep;
 protected float m_fLastLook = -1; // what the last Fit saw out
 protected int m_iBlind;
 protected int m_iSeeing;

 static vector FacingOf(vector heading, int sign)
 {
  if (sign > 0) return Vector(-heading[2], 0, heading[0]);
  return Vector(heading[2], 0, -heading[0]);
 }

 static vector Turned(vector heading, float degrees)
 {
  float rad = degrees * Math.DEG2RAD;
  float c = Math.Cos(rad);
  float s = Math.Sin(rad);
  return Vector(heading[0] * c + heading[2] * s, 0, heading[2] * c - heading[0] * s);
 }

 // Away from the firing side by 'degrees'.
 static vector TurnedBack(vector heading, int sign, float degrees)
 {
  vector back = FacingOf(heading, sign) * -1;
  float rad = degrees * Math.DEG2RAD;
  return heading * Math.Cos(rad) + back * Math.Sin(rad);
 }

 static float Ground(vector p)
 {
  return GetGame().GetWorld().GetSurfaceY(p[0], p[2]);
 }

 static string FitWord(int fit)
 {
  switch (fit)
  {
   case FIT_ROAD: return "road";
   case FIT_BUILDING: return "building";
   case FIT_SOLID: return "solid";
   case FIT_SLOPE: return "slope";
   case FIT_DOOR: return "door";
   case FIT_VIEW: return "view";
   case FIT_WATER: return "water";
   case FIT_BEHIND: return "behind";
   case FIT_PAVED: return "paved";
   case FIT_BOXED: return "boxed";
  }
  return "length";
 }

 static const float OTHER_BAGS = 3;     // m from another line's bags
 static const float TIES_IN = 7;        // ... except where it goes on from one
 vector m_vTiesIn;

 void ACR_BaseLiner(ACR_BaseSurvey survey, ACR_BaseWork work, float seedBearing)
 {
  m_Survey = survey;
  m_Work = work;
  m_fSeed = seedBearing;
  m_fSpan = 75;
  survey.ThisWay(ACR_BaseSurvey.Sector(seedBearing), m_aRoads, m_aHouses);
 }

 // Whether a piece of this length fits going 'heading' from 'tip': why not, else FIT_OK.
 // loose: an end piece or a piece that closes on an anchor (no view asked, nearer to things).
 // noView: the rest as usual, no view asked.
 int Fit(vector tip, vector heading, float length, int sign, bool loose, bool noView = false)
 {
  m_fLastLook = -1;
  vector to = tip + heading * length;
  vector mid = tip + heading * (length * 0.5);
  float tipY = Ground(tip);
  float toY = Ground(to);
  mid[1] = Ground(mid);
  if (BBB_Terrain.IsWaterAt(mid) || BBB_Terrain.IsWaterAt(to)) return FIT_WATER;
  float keep = ROAD_KEEP;
  if (loose) keep = 0.3;
  if (ACR_BaseDefences.RoadEdgeGap(m_aRoads, mid) < keep || ACR_BaseDefences.RoadEdgeGap(m_aRoads, to) < keep) return FIT_ROAD;
  if (WidePaved(to, heading)) return FIT_PAVED;
  // A square or a yard's concrete: a line goes round it, unless it's closing a gap across it.
  if (PlacementOn() && !m_Survey.m_bLoose && !m_bTarget && !loose && ACR_Fortify.HardSurface(mid) && ACR_Fortify.HardSurface(to)) return FIT_PAVED;
  if (Math.AbsFloat(toY - tipY) > MAX_RISE * length / ACR_BaseLines.LONG_STEP) return FIT_SLOPE;
  vector facing = FacingOf(heading, sign);
  vector front = mid + facing * 0.6;
  vector rear = mid - facing * 0.6;
  if (Math.AbsFloat(Ground(front) - Ground(rear)) > MAX_LEAN) return FIT_SLOPE;
  float clear = 1;
  if (loose) clear = 0.3;
  if (ACR_BaseDefences.BuildingGap(m_aHouses, mid, clear + 1) < clear || ACR_BaseDefences.BuildingGap(m_aHouses, to, clear + 1) < clear) return FIT_BUILDING;
  if (m_Survey.DoorGap(mid) < ACR_BaseSurvey.WALL_DOOR) return FIT_DOOR;
  if (!ACR_BaseDefences.WorldClear(mid, ACR_BaseDefences.Bearing(facing), Vector(length * 0.5 + 0.05, 0, 0.61), null, false, true)) return FIT_SOLID;
  // Not across or along a line already laid.
  if (m_bRing && m_Work && vector.DistanceXZ(mid, m_vTiesIn) > TIES_IN && m_Work.PieceWithin(mid, OTHER_BAGS)) return FIT_SOLID;
  if (loose) return FIT_OK;
  if (!RoomBehind(mid, facing)) return FIT_BEHIND;
  if (noView) return FIT_OK;
  // The trace starts 1.2 m on from here: right in front of the bags, so a fence a metre off counts.
  vector muzzle = mid - facing * 0.4;
  muzzle[1] = Ground(muzzle) + ACR_BaseSurvey.UP_BAGS;
  float look = ACR_BaseDefences.Bearing(facing);
  float seen = ACR_BaseSurvey.Reach(muzzle, look, LOOK_FAR);
  if (seen < SEES_FAR)
  {
   seen = Math.Max(seen, ACR_BaseSurvey.Reach(muzzle, look - 18, LOOK_FAR));
   if (seen < SEES_FAR) seen = Math.Max(seen, ACR_BaseSurvey.Reach(muzzle, look + 18, LOOK_FAR));
  }
  m_fLastLook = seen;
  if (seen < VIEW - 0.5 && !m_bRing) return FIT_VIEW;
  // Boxed in: a short view in front and a short one behind. Closing a gap is what a line in a yard is for.
  if (PlacementOn() && !m_Survey.m_bLoose && seen < BOX && (!m_bTarget || m_bOpenOnly))
  {
   vector back = mid + facing * 0.6;
   back[1] = Ground(back) + 1.0;
   if (ACR_BaseSurvey.Reach(back, look + 180, BOX + 1.2) < BOX) return FIT_BOXED;
  }
  return FIT_OK;
 }

 // -acrBasePlacement 0: the first version's looser placement.
 static bool PlacementOn() { return ACR_LogisticsRules.Choice("acrBasePlacement", "1") != "0"; }

 // A runway, a taxiway or an apron ahead: no road on the network tells of these, and a line across one is
 // a wall dropped on the concrete (pn23 playtest, Airport). A yard or a track narrower than this goes
 // under the bags like any ground.
 static bool WidePaved(vector at, vector heading)
 {
  if (!ACR_Fortify.HardSurface(at)) return false;
  return ACR_Fortify.HardSurface(at + heading * (WIDE_PAVED * 0.5)) && ACR_Fortify.HardSurface(at + heading * WIDE_PAVED);
 }

 // Whether a man can stand behind bags here: no fence or wall hard at their back. The trace starts 1.2 m
 // on from where it's given, so this starts it at their rear face.
 static bool RoomBehind(vector middle, vector facing)
 {
  vector from = middle + facing * 0.6;
  from[1] = Ground(from) + 1.0;
  return ACR_BaseSurvey.Reach(from, ACR_BaseDefences.Bearing(facing) + 180, ROOM + 1.2) >= ROOM + 1.1;
 }

 // Where the line wants to go next: on along the town's edge at its offset, or at its target.
 protected vector Wanted(vector tip, int sign)
 {
  if (m_bTarget) return ACR_BaseDefences.FlatDir(tip, m_vTarget);
  vector center = m_Survey.m_vCenter;
  float bearing = ACR_BaseDefences.Bearing(tip - center);
  float radius = vector.DistanceXZ(tip, center);
  float step = (ACR_BaseLines.LONG_STEP * 1.5 / Math.Max(radius, 15)) * Math.RAD2DEG * sign;
  float ahead = bearing + step;
  float reach = m_Survey.EdgeSmooth(ahead) + m_fOffset + m_fWobble * Math.Sin(ahead * 0.11 + m_fPhase);
  vector target = center + ACR_BaseSurvey.Dir(ahead) * reach;
  return ACR_BaseDefences.FlatDir(tip, target);
 }

 protected float TurnBetween(vector from, vector to)
 {
  return ACR_BaseSurvey.Normal(ACR_BaseDefences.Bearing(to) - ACR_BaseDefences.Bearing(from) + 180) - 180;
 }

 // A fire bay: a piece angled off, one along, one angled back, one along again, so a stretch stands 1.4 m
 // off the rest and a man sees along the line. No room for its next piece: the line goes on as usual and
 // turns back by itself. -acrBaseBays 0 turns them off.
 static bool BaysOn()
 {
  return ACR_LogisticsRules.Choice("acrBaseBays", "1") != "0";
 }

 protected bool BayStep(vector tip, vector heading, int sign, out vector next)
 {
  if (m_iBayStep == 0)
  {
   if ((m_bTarget && !m_bRing) || m_iBayEvery <= 0 || m_iSinceBay < m_iBayEvery) return false;
   m_iBayStep = 1;
   s_iBaysForTest++;
  }
  // Towards the firing side is against Turned's sense when sign is 1.
  float turn = -sign * BAY_TURN * m_fBaySide;
  if (m_iBayStep == 2 || m_iBayStep == 3) turn = -turn;
  vector way = Turned(heading, turn);
  if (Fit(tip, way, ACR_BaseLines.LONG_STEP, sign, false) != FIT_OK)
  {
   m_iBayStep = 0;
   m_iSinceBay = 0;
   return false;
  }
  next = way;
  m_iBayStep++;
  if (m_iBayStep > 4)
  {
   m_iBayStep = 0;
   m_iSinceBay = 0;
  }
  return true;
 }

 // The next long piece: the turn towards where it wants to go, then wider and wider either way, to step
 // round a tree or a rock (the joint opens a little on the outside at the widest). FIT_OK with 'next' set,
 // or why it can't go straight on.
 protected int NextHeading(vector tip, vector heading, int sign, out vector next)
 {
  m_bBlindStep = false;
  if (BayStep(tip, heading, sign, next)) return FIT_OK;
  float wanted = Math.Clamp(TurnBetween(heading, Wanted(tip, sign)), -GENTLE, GENTLE);
  // Straight for a target it can be asked to turn hard.
  if (m_bTarget) wanted = Math.Clamp(TurnBetween(heading, Wanted(tip, sign)), -26, 26);
  if (m_bRing) wanted = Math.Clamp(wanted, -RING_TURN, RING_TURN);
  array<float> turns = {wanted};
  array<float> more = {-GENTLE, GENTLE, 0, -11, 11, -18, 18, -26, 26, -34, 34};
  foreach (float turn : more)
   if (Math.AbsFloat(turn - wanted) > 2 && (!m_bRing || Math.AbsFloat(turn) <= 26)) turns.Insert(turn);
  int first = FIT_OK;
  foreach (int i, float tried : turns)
  {
   vector way = Turned(heading, tried);
   int fit = Fit(tip, way, ACR_BaseLines.LONG_STEP, sign, false);
   if (fit == FIT_OK)
   {
    next = way;
    return FIT_OK;
   }
   if (i == 0) first = fit;
   // A road straight ahead is the end of it, not something to wriggle along.
   if ((fit == FIT_ROAD || fit == FIT_PAVED) && Math.AbsFloat(tried) < 6) return fit;
  }
  // Nowhere it can both stand and see, but somewhere it can stand: a piece or two blind.
  if (m_iBlind < MAX_BLIND)
  {
   for (int k = 0; k < turns.Count() && k < 3; k++)
   {
    vector blind = Turned(heading, turns[k]);
    if (Fit(tip, blind, ACR_BaseLines.LONG_STEP, sign, false, true) != FIT_OK) continue;
    next = blind;
    m_bBlindStep = true;
    return FIT_OK;
   }
  }
  return first;
 }

 protected ACR_LinePiece Piece(vector from, vector to, vector middle, float yaw, ResourceName prefab, vector facing)
 {
  ACR_LinePiece piece = new ACR_LinePiece();
  piece.m_vFrom = from;
  piece.m_vTo = to;
  piece.m_vPos = middle;
  piece.m_vPos[1] = Ground(middle);
  piece.m_fYaw = yaw;
  piece.m_sPrefab = prefab;
  piece.m_vFacing = facing;
  piece.m_bLong = ACR_BaseDefences.IsLong(prefab);
  return piece;
 }

 // One way from 'tip', up to 'most' long pieces. How it ended, "" if in the open. m_iStop says why.
 string Walk(vector tip, vector heading, int sign, int most, notnull array<ref ACR_LinePiece> run, notnull array<vector> lanes, notnull array<vector> laneOut, ACR_BaseDefenceKit kit)
 {
  bool rounded;
  int made;
  m_iBlind = 0;
  m_iSeeing = 0;
  m_iStop = -1;
  m_iSinceBay = 0;
  m_iBayStep = 0;
  while (made < most)
  {
   if (m_bTarget && (vector.DistanceXZ(tip, m_vTarget) < ACR_BaseLines.LONG_STEP || (m_bRing && WentPast(tip, heading))))
   {
    if (NextWay()) continue;
    return Closed(tip, heading, sign, run, kit, m_sTargetWord);
   }
   if (!m_bTarget && OffSeed(tip) > m_fSpan) return "";
   vector next;
   int why = NextHeading(tip, heading, sign, next);
   if (why == FIT_OK)
   {
    // Blind pieces come back only after two that see: it can't stay nearly blind by turns.
    if (m_bBlindStep)
    {
     m_iBlind++;
     m_iSeeing = 0;
    }
    else
    {
     m_iSeeing++;
     if (m_iSeeing >= 2) m_iBlind = 0;
    }
    m_iSinceBay++;
    vector to = tip + next * ACR_BaseLines.LONG_STEP;
    vector facing = FacingOf(next, sign);
    ACR_LinePiece walked = Piece(tip, to, tip + next * (ACR_BaseLines.LONG_STEP * 0.5), ACR_BaseDefences.Bearing(facing), kit.m_sLong, facing);
    walked.m_fLook = m_fLastLook;
    run.Insert(walked);
    tip = to;
    heading = next;
    made++;
    continue;
   }
   m_iStop = why;
   if (why == FIT_ROAD && !run.IsEmpty())
   {
    vector across;
    vector lane;
    if (Across(tip, heading, across, lane))
    {
     AddLane(lane, lanes, laneOut);
     if (made < most && Fit(across, heading, ACR_BaseLines.LONG_STEP, sign, false, true) == FIT_OK)
     {
      run[run.Count() - 1].m_iBreak = ACR_BaseLines.BREAK_ROAD;
      tip = across;
      continue;
     }
     vector pastRoad, pastRoadWay;
     if (m_bRing && made < most && PastIt(across, sign, pastRoad, pastRoadWay))
     {
      run[run.Count() - 1].m_iBreak = ACR_BaseLines.BREAK_ROAD;
      tip = pastRoad;
      heading = pastRoadWay;
      continue;
     }
    }
    else
    {
     AddLane(tip + heading * 3, lanes, laneOut);
     vector pastAlong, pastAlongWay;
     if (m_bRing && made < most && !NearLastWay(tip) && PastIt(tip, sign, pastAlong, pastAlongWay, RING_PAST_ROAD))
     {
      run[run.Count() - 1].m_iBreak = ACR_BaseLines.BREAK_ROAD;
      tip = pastAlong;
      heading = pastAlongWay;
      continue;
     }
    }
    return Closed(tip, heading, sign, run, kit, "road");
   }
   // A stretch of the ring goes on past a tree or a shed, and a gap there is one men can walk through. Not past
   // what it's closing on.
   vector past, pastWay;
   if (m_bRing && !run.IsEmpty() && !NearLastWay(tip) && PastIt(tip, sign, past, pastWay))
   {
    run[run.Count() - 1].m_iBreak = ACR_BaseLines.BREAK_WALK;
    tip = past;
    heading = pastWay;
    continue;
   }
   // Round what's in the way, once a way: a round piece bends the line back.
   if (!rounded && why != FIT_SLOPE && why != FIT_ROAD && why != FIT_PAVED && !run.IsEmpty())
   {
    rounded = true;
    vector after;
    if (RoundPiece(tip, heading, sign, run, kit, after))
    {
     tip = run[run.Count() - 1].m_vTo;
     heading = after;
     made++;
     continue;
    }
   }
   return Ended(tip, heading, sign, why, run, kit);
  }
  return "";
 }

 // Close to the point it's heading for and not facing it: a piece turns only so far, and it would circle it.
 protected bool WentPast(vector tip, vector heading)
 {
  if (vector.DistanceXZ(tip, m_vTarget) > RING_PASSED) return false;
  return ACR_BaseDefences.DotXZ(heading, ACR_BaseDefences.FlatDir(tip, m_vTarget)) < 0.4;
 }

 // Every road and building round the town, not those one sector's way: a stretch of the ring ends up further round
 // than that list knows.
 void AllRoads()
 {
  m_aRoads = m_Survey.m_aSegs;
  m_aHouses = m_Survey.m_aBuildings;
 }

 // On to the next point of a stretch of the ring, if there is one.
 protected bool NextWay()
 {
  if (!m_aWay || m_iWay + 1 >= m_aWay.Count()) return false;
  m_iWay++;
  m_vTarget = m_aWay[m_iWay];
  return true;
 }

 protected bool NearLastWay(vector tip)
 {
  if (!m_aWay || m_aWay.IsEmpty() || m_sTargetWord.IsEmpty() || m_sTargetWord == "line") return false;
  return vector.DistanceXZ(tip, m_aWay[m_aWay.Count() - 1]) < RING_PASSED;
 }

 // The first spot on the way on where a piece fits again.
 bool PastIt(vector tip, int sign, out vector beyond, out vector way, float reach = RING_PAST)
 {
  way = Wanted(tip, sign);
  for (float d = 3; d <= reach; d += 3)
  {
   vector tried = tip + way * d;
   if (Fit(tried, way, ACR_BaseLines.LONG_STEP, sign, false) != FIT_OK) continue;
   beyond = tried;
   return true;
  }
  return false;
 }

 protected float OffSeed(vector tip)
 {
  return Math.AbsFloat(ACR_BaseSurvey.Normal(ACR_BaseDefences.Bearing(tip - m_Survey.m_vCenter) - m_fSeed + 180) - 180);
 }

 // Over the road ahead: the first spot on its far side, and the lane's middle. False if it runs along.
 protected bool Across(vector tip, vector heading, out vector across, out vector lane)
 {
  bool onRoad;
  float entered;
  for (float d = 1; d <= 30; d += 1)
  {
   vector p = tip + heading * d;
   float gap = ACR_BaseDefences.RoadEdgeGap(m_aRoads, p);
   if (!onRoad)
   {
    if (gap < 0.3)
    {
     onRoad = true;
     entered = d;
    }
    else if (d > 8) return false;
    continue;
   }
   if (gap >= 0.8)
   {
    across = p;
    lane = tip + heading * ((entered + d) * 0.5);
    return true;
   }
  }
  return false;
 }

 protected void AddLane(vector lane, notnull array<vector> lanes, notnull array<vector> laneOut)
 {
  foreach (vector have : lanes) if (vector.DistanceXZ(have, lane) < 12) return;
  vector onRoad;
  float dist;
  int seg = ACR_BaseDefences.NearestSeg(m_aRoads, lane, onRoad, dist);
  vector out_way = ACR_BaseDefences.FlatDir(m_Survey.m_vCenter, lane);
  if (seg >= 0)
  {
   ACR_BaseDefenceSeg road = m_aRoads[seg];
   vector along = ACR_BaseDefences.FlatDir(road.m_vA, road.m_vB);
   if (ACR_BaseDefences.DotXZ(along, out_way) < 0) along = along * -1;
   out_way = along;
   lane = onRoad;
  }
  lanes.Insert(lane);
  laneOut.Insert(out_way);
 }

 // A round piece at the tip: its chord turned back 20 degrees, its far tip 30 more. Its tips stand
 // ROUND_HALF either side of its middle and half a metre behind it. The heading it leaves on.
 protected bool RoundPiece(vector tip, vector heading, int sign, notnull array<ref ACR_LinePiece> run, ACR_BaseDefenceKit kit, out vector after)
 {
  vector chord = TurnedBack(heading, sign, 20);
  if (Fit(tip, chord, ACR_BaseDefences.ROUND_HALF * 2, sign, true) != FIT_OK) return false;
  vector facing = FacingOf(chord, sign);
  vector to = tip + chord * (ACR_BaseDefences.ROUND_HALF * 2);
  vector middle = tip + chord * ACR_BaseDefences.ROUND_HALF + facing * 0.5;
  run.Insert(Piece(tip, to, middle, ACR_BaseDefences.Bearing(facing), kit.m_sRound, facing));
  after = TurnedBack(chord, sign, 30);
  return true;
 }

 // Stopped by something: is it something to end on? Closes up to it if so; "" if it's open ground.
 protected string Ended(vector tip, vector heading, int sign, int why, notnull array<ref ACR_LinePiece> run, ACR_BaseDefenceKit kit)
 {
  string word = m_Survey.AnchorNear(tip + heading * 1.5, 4.5);
  if (word.IsEmpty() && why == FIT_SLOPE) word = "slope";
  if (word.IsEmpty()) return "";
  return Closed(tip, heading, sign, run, kit, word);
 }

 // The last short piece up to what it ends on, if there's room for one.
 protected string Closed(vector tip, vector heading, int sign, notnull array<ref ACR_LinePiece> run, ACR_BaseDefenceKit kit, string word)
 {
  vector facing = FacingOf(heading, sign);
  if (Fit(tip, heading, ACR_BaseLines.SHORT_STEP, sign, true) == FIT_OK)
  {
   vector to = tip + heading * ACR_BaseLines.SHORT_STEP;
   run.Insert(Piece(tip, to, tip + heading * (ACR_BaseLines.SHORT_STEP * 0.5), ACR_BaseDefences.Bearing(facing), kit.m_sShort, facing));
  }
  return word;
 }

 // An open end goes on to the nearest thing it can end on, within 30 m and not behind it. Its word, or ""
 // and the pieces it tried come off.
 string Extend(notnull array<ref ACR_LinePiece> run, int sign, int most, ACR_BaseDefenceKit kit, notnull array<vector> lanes, notnull array<vector> laneOut)
 {
  if (run.IsEmpty() || most <= 0) return "";
  ACR_LinePiece last = run[run.Count() - 1];
  vector tip = last.m_vTo;
  vector heading = ACR_BaseDefences.FlatDir(last.m_vFrom, last.m_vTo);
  vector target;
  string word;
  if (!m_Survey.NearestAnchor(tip, heading, m_Work, ACR_BaseLines.EXTEND_REACH, target, word)) return "";
  int had = run.Count();
  m_bTarget = true;
  m_vTarget = target;
  m_sTargetWord = word;
  string got = Walk(tip, heading, sign, most, run, lanes, laneOut, kit);
  m_bTarget = false;
  if (!got.IsEmpty() && !MostlyBlind(run, had)) return got;
  while (run.Count() > had) run.Remove(run.Count() - 1);
  return "";
 }

 // Of the pieces from 'from' on, do most of at least three see too little to be worth laying?
 protected bool MostlyBlind(notnull array<ref ACR_LinePiece> run, int from)
 {
  if (!PlacementOn() || m_Survey.m_bLoose) return false;
  int longs, blind;
  for (int i = from; i < run.Count(); i++)
  {
   if (!run[i].m_bLong) continue;
   longs++;
   if (run[i].m_fLook >= 0 && run[i].m_fLook < SEES_FAR) blind++;
  }
  return longs >= 3 && blind * 2 > longs;
 }

 // The open end of a line curves back: a round piece and an end piece with its taper trailing, or just
 // the end piece. Pieces come off the end until that fits. With no pieces this way at all, from the seed.
 // False if none of it will.
 bool CurveBack(notnull array<ref ACR_LinePiece> run, int sign, ACR_BaseDefenceKit kit, vector seedTip, vector seedHeading)
 {
  while (true)
  {
   // (A piece or two left past a break by what came off isn't worth curving.)
   ACR_BaseLines.LoneTail(run);
   vector tip = seedTip;
   vector heading = seedHeading;
   if (!run.IsEmpty())
   {
    ACR_LinePiece last = run[run.Count() - 1];
    last.m_iBreak = 0;
    tip = last.m_vTo;
    heading = ACR_BaseDefences.FlatDir(last.m_vFrom, last.m_vTo);
   }
   vector after;
   if (RoundPiece(tip, heading, sign, run, kit, after))
   {
    ACR_LinePiece bend = run[run.Count() - 1];
    bend.m_bCurve = true;
    vector endAt = bend.m_vTo;
    int sides = ReturnPieces(sign, run, kit, endAt, after);
    if (AddEnd(endAt, after, sign, run, kit)) return true;
    for (int off = 0; off <= sides; off++) run.Remove(run.Count() - 1);
   }
   vector sharp = TurnedBack(heading, sign, 50);
   if (AddEnd(tip, sharp, sign, run, kit)) return true;
   if (run.IsEmpty()) return false;
   run.Remove(run.Count() - 1);
  }
  return false;
 }

 // Where a road goes through a line, the bags turn back along it either side: a gate, not two ends that
 // happen to stop at a road. Back the way the line's men stand, one piece each, a short or a long one.
 void GateShoulders(ACR_BaseLine line, ACR_BaseDefenceKit kit, ACR_BaseWork work)
 {
  array<ref ACR_LinePiece> pieces = line.m_aPieces;
  RandomGenerator rng = ACR_Variety.For(string.Format("base gate %1 %2", work.m_Base.m_sName, work.m_aLines.Count()));
  for (int i = pieces.Count() - 2; i >= 0; i--)
  {
   ACR_LinePiece before = pieces[i];
   if (before.m_iBreak != ACR_BaseLines.BREAK_ROAD) continue;
   ACR_LinePiece after = pieces[i + 1];
   float length = ACR_BaseLines.SHORT_STEP;
   ResourceName prefab = kit.m_sShort;
   if (rng.RandFloat01() < 0.5)
   {
    length = ACR_BaseLines.LONG_STEP;
    prefab = kit.m_sLong;
   }
   vector inward = (before.m_vFacing + after.m_vFacing) * -0.5;
   inward[1] = 0;
   inward.Normalize();
   // Each side's facing: towards the road.
   vector toAfter = ACR_BaseDefences.FlatDir(before.m_vTo, after.m_vFrom);
   if (line.m_bRing)
   {
    vector square = Vector(inward[2], 0, -inward[0]);
    if (ACR_BaseDefences.DotXZ(square, toAfter) < 0) square = square * -1;
    toAfter = square;
   }
   if (Fit(after.m_vFrom, inward, length, 1, true, true) == FIT_OK)
   {
    vector afterEnd = after.m_vFrom + inward * length;
    ACR_LinePiece shoulder = Piece(afterEnd, after.m_vFrom, after.m_vFrom + inward * (length * 0.5), ACR_BaseDefences.Bearing(toAfter * -1), prefab, toAfter * -1);
    shoulder.m_bCurve = true;
    pieces.InsertAt(shoulder, i + 1);
   }
   if (Fit(before.m_vTo, inward, length, 1, true, true) == FIT_OK)
   {
    vector beforeEnd = before.m_vTo + inward * length;
    ACR_LinePiece other = Piece(before.m_vTo, beforeEnd, before.m_vTo + inward * (length * 0.5), ACR_BaseDefences.Bearing(toAfter), prefab, toAfter);
    other.m_bCurve = true;
    other.m_iBreak = ACR_BaseLines.BREAK_ROAD;
    before.m_iBreak = 0;
    pieces.InsertAt(other, i + 1);
   }
  }
 }

 // A redoubt's sides: long pieces bending further back, as far as the ground lets them. How many went in.
 protected int ReturnPieces(int sign, notnull array<ref ACR_LinePiece> run, ACR_BaseDefenceKit kit, inout vector tip, inout vector heading)
 {
  int made;
  while (made < m_iReturns)
  {
   vector way = TurnedBack(heading, sign, 16);
   if (Fit(tip, way, ACR_BaseLines.LONG_STEP, sign, true) != FIT_OK) break;
   vector to = tip + way * ACR_BaseLines.LONG_STEP;
   vector facing = FacingOf(way, sign);
   ACR_LinePiece side = Piece(tip, to, tip + way * (ACR_BaseLines.LONG_STEP * 0.5), ACR_BaseDefences.Bearing(facing), kit.m_sLong, facing);
   side.m_bCurve = true;
   run.Insert(side);
   tip = to;
   heading = way;
   made++;
  }
  return made;
 }

 protected bool AddEnd(vector tip, vector heading, int sign, notnull array<ref ACR_LinePiece> run, ACR_BaseDefenceKit kit)
 {
  if (Fit(tip, heading, ACR_BaseLines.END_STEP, sign, true) != FIT_OK) return false;
  vector to = tip + heading * ACR_BaseLines.END_STEP;
  // The taper is at the end piece's local +X (ACR_BaseDefences.EndFacing): it trails away from the line.
  vector facing = ACR_BaseDefences.EndFacing(heading);
  ACR_LinePiece piece = Piece(tip, to, tip + heading * (ACR_BaseLines.END_STEP * 0.5), ACR_BaseDefences.Bearing(facing), kit.m_sEnd, FacingOf(heading, sign));
  piece.m_bCurve = true;
  run.Insert(piece);
  return true;
 }
}
