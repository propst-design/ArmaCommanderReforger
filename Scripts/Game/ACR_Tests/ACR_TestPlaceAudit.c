// -acrPlaceAuditTest 1 (server-capable, about 1 min; any map): how good each town's plan is, not only whether
// it breaks a rule. Every long piece is looked at from behind the bags: how far it sees out, whether it stands
// boxed in a yard, whether it faces the threat, whether it stands on hard ground or hugs a wall. Every way into
// the town is checked for a line across it and a gun on it. One AUDIT row per town and one AUDIT TOTAL row, the
// worst pieces as AUDITBAD rows and every town as PLANMAP rows for a drawing. It fails on nothing by itself: it
// has floors (FLOOR_*) that go up as the planner gets better.
class ACR_TestPlaceAudit
{
 static const float LOOK = 80;          // m looked out from a piece
 static const float SHORT = 25;         // m: a piece that sees less than this watches a fence, not an approach
 static const float BOX = 25;           // ... boxed: front and back both
 static const float GATE_NEAR = 30;     // m from a way in to a piece for it to be held
 static const float HUG = 3;            // m from a wall, running along it: pointless beside something as good
 static const float ALMOST = 14;        // m between two lines' ends that nearly meet: a hole, not a gap on purpose
 // What the planner has to reach, in percent (of long pieces, or of ways in).
 static const float FLOOR_SEEING = 75;
 static const float FLOOR_BOXED = 10;
 static const float FLOOR_AWAY = 15;
 // (Places follow the ways he walks now, so a road in with a gun on it but no line across it isn't a hole.)
 static const float FLOOR_GATES = 75;
 static const float FLOOR_GUN_GATES = 50;
 static const float FLOOR_RING_CLOSED = 45;
 // These three fell on 3 Nov (50 / 100 / 40 before): the yardstick's walk (ACR_KillGround.Solve) put no fire on the
 // cells of his previous ways, only round them, so it steered him into the bags' fire and overstated every plan by
 // about 30 m on the softest way. With the honest walk the same plans read 22 m / 92-95 m / 66-70% (two runs).
 static const float FLOOR_SOFTEST = 20;  // m under fire on his softest way in, on average over the towns
 static const float FLOOR_MEAN_WAY = 90;  // ... and on his average way
 static const float MOST_OPEN_TOWNS = 70; // % of towns where his softest way has under 30 m of fire
 static const string AUDIT_SEED = "20261103"; // the match seed every run draws its picks from

 protected static int s_iNext;
 protected static int s_iTowns;
 protected static int s_iPieces, s_iSeeing, s_iShort, s_iBoxed, s_iAway, s_iHard, s_iHug, s_iPlugs, s_iDeadAsked, s_iDeadGround;
 protected static int s_iGates, s_iGatesHeld, s_iGatesGun, s_iNests, s_iNestsOnGate, s_iAlmost;
 protected static float s_fLookSum;
 protected static int s_iRingAsked, s_iRingClosed;
 protected static float s_fSoftSum, s_fMeanSum;
 protected static int s_iSoftTowns, s_iSoftOpen;
 protected static ref array<ACR_Base> s_aBases;
 static void ACR_InitStatics()
 {
  s_aBases = {};
 }
 protected static string s_sSide;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings aside : ACR_FactionSettings.s_All) aside.m_bAICommander = false;
  if (System.IsCLIParam("acrPlaceAuditOldTest")) ACR_Options.Set("acrKillGround", "0");
  // One seed for every run: the styles and picks are drawn from the match seed (ACR_Variety), and without this the
  // same code moved about 7 m and 7% between runs, 13 of 50 towns drawing another style (3 Nov).
  ACR_Options.Set("acrSeed", AUDIT_SEED);
  if (second < 5) return;
  if (s_aBases.IsEmpty() && s_iNext == 0)
  {
   s_sSide = ACR_FactionSelection.West();
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base && !base.m_bRetired && !base.m_bHidden && !base.m_bBuilt) s_aBases.Insert(base);
  }
  for (int i = 0; i < 2 && s_iNext < s_aBases.Count(); i++)
  {
   Town(s_aBases[s_iNext]);
   s_iNext++;
  }
  if (s_iNext == s_aBases.Count() && s_iTowns > 0)
  {
   Totals();
   s_iTowns = 0;
  }
 }

 // Every town as text, for working on placement away from the game: the ground on a 6 m grid and all that stands.
 static const float DUMP_CELL = 6;
 static const float DUMP_OUT = 140;
 protected static ref map<int, int> s_mDumpTrees;
 protected static float s_fDumpX, s_fDumpZ;
 protected static int s_iDumpN;

 protected static bool DumpTree(IEntity e)
 {
  if (!Tree.Cast(e)) return true;
  vector at = e.GetOrigin();
  int ix = (at[0] - s_fDumpX) / DUMP_CELL;
  int iz = (at[2] - s_fDumpZ) / DUMP_CELL;
  if (ix < 0 || iz < 0 || ix >= s_iDumpN || iz >= s_iDumpN) return true;
  int key = iz * s_iDumpN + ix;
  int have;
  s_mDumpTrees.Find(key, have);
  s_mDumpTrees.Set(key, have + 1);
  return true;
 }

 protected static void Dump(ACR_Base base, ACR_BaseWork work, ACR_BaseSurvey survey, array<vector> dirs, array<float> weights)
 {
  string town = base.m_sName;
  vector center = base.GetOrigin();
  float half = survey.m_fRadius + DUMP_OUT;
  s_iDumpN = half * 2 / DUMP_CELL;
  s_fDumpX = center[0] - half;
  s_fDumpZ = center[2] - half;
  PrintFormat("TOWN|%1|S|%2|%3|%4|%5|%6|%7|%8", town, ACR_BaseStyle.Word(work.m_Style.m_iKind), center[0], center[2], survey.m_fRadius, s_fDumpX, s_fDumpZ, s_iDumpN);
  foreach (int d, vector dir : dirs) PrintFormat("TOWN|%1|T|%2|%3|%4", town, dir[0], dir[2], weights[d]);
  string edge = "";
  foreach (float e : survey.m_aEdge) edge += string.Format("%1,", (int)e);
  PrintFormat("TOWN|%1|E|%2", town, edge);
  string threat = "";
  foreach (float t : survey.m_aThreat) threat += string.Format("%1,", t.ToString(-1, 2));
  PrintFormat("TOWN|%1|X|%2", town, threat);
  foreach (IEntity b : survey.m_aBuildings)
  {
   if (!b) continue;
   vector mins, maxs;
   b.GetBounds(mins, maxs);
   vector c1 = b.CoordToParent(Vector(mins[0], 0, mins[2]));
   vector c2 = b.CoordToParent(Vector(maxs[0], 0, mins[2]));
   vector c3 = b.CoordToParent(Vector(maxs[0], 0, maxs[2]));
   vector c4 = b.CoordToParent(Vector(mins[0], 0, maxs[2]));
   PrintFormat("TOWN|%1|H|%2|%3|%4|%5|%6|%7|%8|%9", town, c1[0], c1[2], c2[0], c2[2], c3[0], c3[2], c4[0], c4[2]);
  }
  foreach (ACR_BaseDefenceSeg seg : survey.m_aSegs) PrintFormat("TOWN|%1|R|%2|%3|%4|%5", town, seg.m_vA[0], seg.m_vA[2], seg.m_vB[0], seg.m_vB[2]);
  for (int w = 0; w + 1 < survey.m_aWallEnds.Count(); w += 2)
  {
   vector wa = survey.m_aWallEnds[w];
   vector wb = survey.m_aWallEnds[w + 1];
   string wallName = survey.m_aWallNames[w / 2];
   int wallSlash = wallName.LastIndexOf("/");
   if (wallSlash >= 0) wallName = wallName.Substring(wallSlash + 1, wallName.Length() - wallSlash - 1);
   PrintFormat("TOWN|%1|W|%2|%3|%4|%5|%6|%7", town, wa[0], wa[2], wb[0], wb[2], survey.m_aWallTall[w / 2].ToString(-1, 1), wallName);
  }
  foreach (vector rock : survey.m_aRocks) PrintFormat("TOWN|%1|K|%2|%3", town, rock[0], rock[2]);
  foreach (ACR_BaseGate gate : survey.m_aGates) PrintFormat("TOWN|%1|A|%2|%3|%4|%5|%6", town, gate.m_vPos[0], gate.m_vPos[2], gate.m_vOut[0], gate.m_vOut[2], survey.m_aThreat[gate.m_iSector].ToString(-1, 2));
  foreach (int li, ACR_BaseLine line : work.m_aLines)
   foreach (ACR_LinePiece piece : line.m_aPieces)
    PrintFormat("TOWN|%1|L|%2|%3|%4|%5|%6|%7", town, piece.m_vFrom[0], piece.m_vFrom[2], piece.m_vTo[0], piece.m_vTo[2], line.m_iTier, li);
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (site.m_eKind == ACR_EBaseSite.PLANNED) continue;
   PrintFormat("TOWN|%1|G|%2|%3|%4|%5|%6", town, ACR_BaseWorks.SiteWords(site), site.m_vPos[0], site.m_vPos[2], site.m_fYaw, site.m_iTier);
  }
  foreach (vector ringAt : ACR_BaseRing.s_aLastPath) PrintFormat("TOWN|%1|P|%2|%3", town, ringAt[0], ringAt[2]);
  ACR_BaseRing.s_aLastPath.Clear();
  s_mDumpTrees = new map<int, int>();
  GetGame().GetWorld().QueryEntitiesBySphere(center, half * 1.45, DumpTree, null, EQueryEntitiesFlags.STATIC);
  BaseWorld world = GetGame().GetWorld();
  for (int iz = 0; iz < s_iDumpN; iz++)
  {
   string row = "";
   string water = "";
   string trees = "";
   string hard = "";
   for (int ix = 0; ix < s_iDumpN; ix++)
   {
    float x = s_fDumpX + (ix + 0.5) * DUMP_CELL;
    float z = s_fDumpZ + (iz + 0.5) * DUMP_CELL;
    vector at = Vector(x, 0, z);
    at[1] = world.GetSurfaceY(x, z);
    row += string.Format("%1,", (int)(at[1] * 10));
    if (BBB_Terrain.IsWaterAt(at)) water += "1";
    else water += "0";
    int count;
    s_mDumpTrees.Find(iz * s_iDumpN + ix, count);
    trees += string.Format("%1", Math.Min(count, 9));
    if (ACR_Fortify.HardSurface(at)) hard += "1";
    else hard += "0";
    if ((ix + 1) % 40 == 0 || ix == s_iDumpN - 1)
    {
     PrintFormat("TOWN|%1|Z|%2|%3|%4|%5|%6|%7", town, iz, ix / 40, row, water, trees, hard);
     row = "";
     hard = "";
     water = "";
     trees = "";
    }
   }
  }
 }

 protected static int Percent(int part, int whole)
 {
  if (whole <= 0) return 100;
  return part * 100 / whole;
 }

 protected static void Town(ACR_Base base)
 {
  string faction = base.m_sOwner;
  if (faction.IsEmpty()) faction = s_sSide;
  ACR_BaseWorks.Reset();
  ACR_BaseRing.s_bMeasureForTest = true;
  ACR_BaseRing.s_iLastAsked = 0;
  ACR_BaseRing.s_iLastClosed = 0;
  ACR_BaseWork work = ACR_BaseWorks.WorkFor(base, faction);
  ACR_BaseSurvey survey = ACR_BaseWorks.s_LastSurvey;
  if (!work || !survey) return;
  s_iRingAsked += ACR_BaseRing.s_iLastAsked;
  s_iRingClosed += ACR_BaseRing.s_iLastClosed;
  s_iTowns++;
  array<vector> dirs = {};
  array<float> weights = {};
  ACR_BaseDefences.Threats(base, faction, dirs, weights);
  Dump(base, work, survey, dirs, weights);
  ACR_KillGround yardstick = ACR_KillGround.Measure(work, survey);
  float softest;
  int ways;
  yardstick.Softest(softest, ways);
  float meanFire = yardstick.MeanFire();
  yardstick.DumpWays(base.m_sName);
  s_fSoftSum += softest;
  s_fMeanSum += meanFire;
  s_iSoftTowns++;
  if (softest < 30) s_iSoftOpen++;
  PrintFormat("AUDIT KILL %1 (%2): the softest way in %3 m under fire, on average %4 m, %5 ms to plan", base.m_sName, ACR_BaseStyle.Word(work.m_Style.m_iKind), (int)softest, (int)meanFire, ACR_KillGround.s_iLastMs);
  int pieces, seeing, shortView, boxed, away, hard, hug, pieceIndex, deadAsked, deadGround;
  float lookSum;
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   foreach (ACR_LinePiece piece : line.m_aPieces)
   {
    // (A redoubt's sides look out along its flank: they aren't what it's for.)
    if (!piece.m_bLong || piece.m_bCurve) continue;
    // (A plug watches the road through it: what its bags see past the houses isn't what it's for.)
    if (line.m_bPlug)
    {
     s_iPlugs++;
     continue;
    }
    pieces++;
    float bearing = ACR_BaseDefences.Bearing(piece.m_vFacing);
    vector eye = piece.m_vPos - piece.m_vFacing * 0.4;
    eye[1] = ACR_BaseLiner.Ground(eye) + ACR_BaseSurvey.UP_BAGS;
    float look = Math.Max(ACR_BaseSurvey.Reach(eye, bearing, LOOK), Math.Max(ACR_BaseSurvey.Reach(eye, bearing - 18, LOOK), ACR_BaseSurvey.Reach(eye, bearing + 18, LOOK)));
    lookSum += look;
    vector rear = piece.m_vPos + piece.m_vFacing * 0.6;
    rear[1] = ACR_BaseLiner.Ground(rear) + 1.0;
    float back = ACR_BaseSurvey.Reach(rear, bearing + 180, LOOK);
    string bad = "";
    pieceIndex++;
    if (pieceIndex % 3 == 0)
    {
     deadAsked++;
     if (ACR_BaseLines.DeadFront(piece.m_vPos, piece.m_vFacing) >= 2) deadGround++;
    }
    if (look >= SHORT) seeing++;
    else
    {
     shortView++;
     bad += " short";
    }
    if (look < BOX && back < BOX)
    {
     boxed++;
     bad += " boxed";
     if (line.m_bGap) bad += " gap";
     else if (!line.m_aLanes.IsEmpty()) bad += " road";
    }
    if (!piece.m_bCurve && ACR_BaseDefences.ThreatDot(dirs, weights, piece.m_vFacing) < -0.2 && work.m_Style.m_iKind != ACR_BaseStyle.RING)
    {
     away++;
     bad += " away";
    }
    if (ACR_Fortify.HardSurface(piece.m_vPos))
    {
     hard++;
     bad += " hard";
    }
    if (Hugs(survey, piece))
    {
     hug++;
     bad += " hug";
    }
    if (!bad.IsEmpty()) PrintFormat("AUDITBAD|%1|%2|%3|%4|%5", base.m_sName, piece.m_vPos[0], piece.m_vPos[2], (int)look, bad);
   }
  }
  // The ways in.
  int gates, held, gunned;
  foreach (ACR_BaseGate gate : survey.m_aGates)
  {
   if (survey.m_aThreat[gate.m_iSector] < ACR_BaseSurvey.MIN_SPOT_THREAT) continue;
   gates++;
   bool isHeld = ACR_BaseLines.GateHeld(work, gate);
   bool isGunned = false;
   int geometry = 0;
   int nestsThere = 0;
   foreach (ACR_BaseSite site : work.m_aSites)
   {
    if (site.m_eKind != ACR_EBaseSite.NEST && site.m_eKind != ACR_EBaseSite.BUNKER) continue;
    nestsThere++;
    if (ACR_BaseLines.GunCoversGate(site.m_vPos, site.m_fYaw, gate, false)) geometry++;
    if (ACR_BaseLines.GunCoversGate(site.m_vPos, site.m_fYaw, gate, true)) isGunned = true;
   }
   if (!isGunned) PrintFormat("AUDITGUN|%1|%2|%3|guns %4|pointing %5", base.m_sName, gate.m_vPos[0], gate.m_vPos[2], nestsThere, geometry);
   if (isHeld) held++;
   if (isGunned) gunned++;
   if (!isHeld) PrintFormat("AUDITGATE|%1|%2|%3|open", base.m_sName, gate.m_vPos[0], gate.m_vPos[2]);
  }
  // Lines' ends that nearly meet.
  int almost;
  foreach (int a, ACR_BaseLine one : work.m_aLines)
  {
   if (one.m_aPieces.IsEmpty()) continue;
   for (int b = a + 1; b < work.m_aLines.Count(); b++)
   {
    ACR_BaseLine two = work.m_aLines[b];
    if (two.m_aPieces.IsEmpty()) continue;
    array<vector> ends = {one.m_aPieces[0].m_vFrom, one.m_aPieces[one.m_aPieces.Count() - 1].m_vTo};
    array<vector> others = {two.m_aPieces[0].m_vFrom, two.m_aPieces[two.m_aPieces.Count() - 1].m_vTo};
    foreach (vector e : ends)
     foreach (vector o : others)
      if (vector.DistanceXZ(e, o) < ALMOST) almost++;
   }
  }
  int nests;
  foreach (ACR_BaseSite gun : work.m_aSites) if (gun.m_eKind == ACR_EBaseSite.NEST) nests++;
  s_iPieces += pieces;
  s_iDeadAsked += deadAsked;
  s_iDeadGround += deadGround;
  s_iSeeing += seeing;
  s_iShort += shortView;
  s_iBoxed += boxed;
  s_iAway += away;
  s_iHard += hard;
  s_iHug += hug;
  s_iGates += gates;
  s_iGatesHeld += held;
  s_iGatesGun += gunned;
  s_iNests += nests;
  s_iAlmost += almost;
  s_fLookSum += lookSum;
  int mean = 0;
  if (pieces > 0) mean = lookSum / pieces;
  PrintFormat("AUDIT|%1|%2|pieces %3|lines %4|nests %5", base.m_sName, ACR_BaseStyle.Word(work.m_Style.m_iKind), pieces, work.m_aLines.Count(), nests);
  PrintFormat("AUDIT2|%1|sees %2%%|mean %3 m|short %4|boxed %5|away %6|hard %7|hug %8", base.m_sName, Percent(seeing, pieces), mean, shortView, boxed, away, hard, hug);
  PrintFormat("AUDIT3|%1|gates %2|held %3|gunned %4|almost %5|ms %6", base.m_sName, gates, held, gunned, almost, ACR_BaseWorks.s_iLastPlanMs);
 }

 // Runs along a wall or fence, close to it.
 protected static bool Hugs(ACR_BaseSurvey survey, ACR_LinePiece piece)
 {
  vector along = ACR_BaseDefences.FlatDir(piece.m_vFrom, piece.m_vTo);
  int count = survey.m_aWallEnds.Count();
  for (int i = 0; i + 1 < count; i += 2)
  {
   vector a = survey.m_aWallEnds[i];
   if (vector.DistanceXZ(a, piece.m_vPos) > HUG + 15) continue;
   ACR_BaseDefenceSeg seg = new ACR_BaseDefenceSeg();
   seg.m_vA = a;
   seg.m_vB = survey.m_aWallEnds[i + 1];
   vector q;
   if (ACR_BaseDefences.SegDistance(seg, piece.m_vPos, q) >= HUG) continue;
   if (Math.AbsFloat(ACR_BaseDefences.DotXZ(ACR_BaseDefences.FlatDir(seg.m_vA, seg.m_vB), along)) > 0.85) return true;
  }
  return false;
 }

 protected static void Totals()
 {
  int mean = 0;
  if (s_iPieces > 0) mean = s_fLookSum / s_iPieces;
  PrintFormat("AUDIT TOTAL pieces %1, sees %2%%, mean %3 m, short %4, boxed %5, away %6, hard %7, hug %8, in plugs %9", s_iPieces, Percent(s_iSeeing, s_iPieces), mean, s_iShort, s_iBoxed, s_iAway, s_iHard, s_iHug, s_iPlugs);
  PrintFormat("AUDIT TOTAL dead ground close in front of %1%% of pieces", Percent(s_iDeadGround, s_iDeadAsked));
  PrintFormat("AUDIT TOTAL gates %1, held %2%%, gunned %3%%, nests %4, almost meeting %5", s_iGates, Percent(s_iGatesHeld, s_iGates), Percent(s_iGatesGun, s_iGates), s_iNests, s_iAlmost);
  ACR_TestRegress.Check("placeaudit: most pieces see out", Percent(s_iSeeing, s_iPieces) >= FLOOR_SEEING, string.Format("%1%%", Percent(s_iSeeing, s_iPieces))); // int-math ok
  ACR_TestRegress.Check("placeaudit: few pieces boxed in", Percent(s_iBoxed, s_iPieces) <= FLOOR_BOXED, string.Format("%1%%", Percent(s_iBoxed, s_iPieces)));
  ACR_TestRegress.Check("placeaudit: few pieces face away from the threat", Percent(s_iAway, s_iPieces) <= FLOOR_AWAY, string.Format("%1%%", Percent(s_iAway, s_iPieces)));
  ACR_TestRegress.Check("placeaudit: the ways in are held", Percent(s_iGatesHeld, s_iGates) >= FLOOR_GATES, string.Format("%1%%", Percent(s_iGatesHeld, s_iGates))); // int-math ok
  ACR_TestRegress.Check("placeaudit: guns on the ways in", Percent(s_iGatesGun, s_iGates) >= FLOOR_GUN_GATES, string.Format("%1%%", Percent(s_iGatesGun, s_iGates))); // int-math ok
  if (s_iSoftTowns > 0) PrintFormat("AUDIT TOTAL his softest way in is on average %1 m under fire, his average way %2 m; %3 of %4 towns under 30 m", (int)(s_fSoftSum / s_iSoftTowns), (int)(s_fMeanSum / s_iSoftTowns), s_iSoftOpen, s_iSoftTowns);
  PrintFormat("AUDIT TOTAL threatened way round closed %1%% (%2 of %3 cells)", Percent(s_iRingClosed, s_iRingAsked), s_iRingClosed, s_iRingAsked);
  ACR_TestRegress.Check("placeaudit: the threatened way round is closed", Percent(s_iRingClosed, s_iRingAsked) >= FLOOR_RING_CLOSED, string.Format("%1%%", Percent(s_iRingClosed, s_iRingAsked))); // int-math ok
  if (s_iSoftTowns > 0 && !System.IsCLIParam("acrPlaceAuditOldTest"))
  {
   ACR_TestRegress.Check("placeaudit: he can't walk in unshot", s_fSoftSum / s_iSoftTowns >= FLOOR_SOFTEST, string.Format("%1 m on the softest way, floor %2", (int)(s_fSoftSum / s_iSoftTowns), (int)FLOOR_SOFTEST));
   ACR_TestRegress.Check("placeaudit: his ways in are fired on", s_fMeanSum / s_iSoftTowns >= FLOOR_MEAN_WAY, string.Format("%1 m on average, floor %2", (int)(s_fMeanSum / s_iSoftTowns), (int)FLOOR_MEAN_WAY));
   ACR_TestRegress.Check("placeaudit: few towns left open", Percent(s_iSoftOpen, s_iSoftTowns) <= MOST_OPEN_TOWNS, string.Format("%1%% under 30 m", Percent(s_iSoftOpen, s_iSoftTowns)));
  }
  ACR_TestRegress.Complete("placeaudit", 6);
 }
}
