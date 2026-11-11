// AI commander lanes (ACR_AILanes, Jacob 4 Nov), run from srv:regress at tick 4: the corridor
// geometry, the lane and target scores, when a review changes lane, the doctrines' lane settings,
// the host switch, and a commander on the test range picking a lane it can push down.
class ACR_TestLanes
{
 static void Run(ACR_Manager manager)
 {
  CheckGeometry();
  CheckScores();
  CheckDoctrines();
  // Lanes are on by default (#232); set them on anyway so a host flag can't change what this checks.
  ACR_Options.Set("acrAIStrategy", "1");
  CheckCommander(manager);
  ACR_Options.Unset("acrAIStrategy");
 }

 static void CheckGeometry()
 {
  vector from = "0 0 0";
  vector to = "0 0 3000";
  float half = 700;
  bool along = Math.AbsFloat(ACR_AILanes.Along(from, to, "400 0 1200") - 1200) < 0.5 && ACR_AILanes.Along(from, to, "0 0 -300") < 0;
  bool off = Math.AbsFloat(ACR_AILanes.Off(from, to, "400 0 1200") - 400) < 0.5 && Math.AbsFloat(ACR_AILanes.Off(from, to, "-650 0 2000") - 650) < 0.5;
  ACR_TestRegress.Check("lanes: distance along and off the axis", along && off, "");

  bool inside = ACR_AILanes.InLane(from, to, half, "300 0 1500") && ACR_AILanes.InLane(from, to, half, "0 0 3500");
  bool outside = !ACR_AILanes.InLane(from, to, half, "900 0 1500") && !ACR_AILanes.InLane(from, to, half, "0 0 4000") && !ACR_AILanes.InLane(from, to, half, "0 0 -900");
  ACR_TestRegress.Check("lanes: a corridor between its ends, half a width of slack past each", inside && outside, "");

  bool flank = ACR_AILanes.OnFlank(from, to, half, "1000 0 1500") && ACR_AILanes.OnFlank(from, to, half, "-1400 0 800");
  bool notFlank = !ACR_AILanes.OnFlank(from, to, half, "300 0 1500") && !ACR_AILanes.OnFlank(from, to, half, "2000 0 1500") && !ACR_AILanes.OnFlank(from, to, half, "1000 0 5000");
  ACR_TestRegress.Check("lanes: flanks run beside the corridor, not in it or far off", flank && notFlank, "");
 }

 static void CheckScores()
 {
  bool gain = ACR_AILanes.Gain(true, 3) == 3 && ACR_AILanes.Gain(false, 3) == ACR_AILanes.TACTICAL_GAIN && ACR_AILanes.Gain(true, 0) == ACR_AILanes.TACTICAL_GAIN;
  ACR_TestRegress.Check("lanes: income counts its value, a tactical point a little", gain, "");

  bool richer = ACR_AILanes.Score(6, 4, 3000) > ACR_AILanes.Score(3, 4, 3000);
  bool cheaper = ACR_AILanes.Score(4, 2, 3000) > ACR_AILanes.Score(4, 12, 3000);
  bool shorter = ACR_AILanes.Score(4, 4, 2000) > ACR_AILanes.Score(4, 4, 5000);
  ACR_TestRegress.Check("lanes: more income, less defence and less length score higher", richer && cheaper && shorter && ACR_AILanes.Score(0, 0, 1000) == 0, "");

  bool keeps = !ACR_AILanes.ShouldSwitch(true, 0, ACR_AILanes.HOLD_MIN + 1, 1, 1.2) && !ACR_AILanes.ShouldSwitch(true, 0, 1, 1, 5);
  bool changes = ACR_AILanes.ShouldSwitch(true, 0, ACR_AILanes.HOLD_MIN + 1, 1, 1.5) && ACR_AILanes.ShouldSwitch(false, 0, 0, 0, 0.1) && ACR_AILanes.ShouldSwitch(true, ACR_AILanes.SETBACKS, 0, 5, 1);
  ACR_TestRegress.Check("lanes: kept while held or not clearly beaten, changed when broken, stalled or beaten", keeps && changes, "");

  float tactical = ACR_AILanes.TargetFactor(true, false, false, 0);
  float town = ACR_AILanes.TargetFactor(true, false, true, 3);
  float flank = ACR_AILanes.TargetFactor(false, true, true, 3);
  float away = ACR_AILanes.TargetFactor(false, false, true, 3);
  bool order = town > 0 && town < tactical && tactical < 1 && flank > 1 && away < 0;
  ACR_TestRegress.Check("lanes: targets in the lane first (income before tactical), flanks later, others left alone", order, string.Format("town %1, tactical %2, flank %3, away %4", town, tactical, flank, away));
 }

 static void CheckDoctrines()
 {
  bool used = ACR_AIDoctrine.Find("balanced").m_bLanes && ACR_AIDoctrine.Find("blitz").m_bLanes && ACR_AIDoctrine.Find("fortress").m_bLanes && ACR_AIDoctrine.Find("firepower").m_bLanes;
  bool raider = !ACR_AIDoctrine.Find("raider").m_bLanes;
  bool fronts = ACR_AIDoctrine.Find("balanced").m_iLaneFronts == 1 && ACR_AIDoctrine.Find("fortress").m_iLaneFronts == 0;
  bool works = ACR_AIDoctrine.Find("fortress").m_fFlankWorks > ACR_AIDoctrine.Find("balanced").m_fFlankWorks;
  bool rush = ACR_AIDoctrine.Find("blitz").m_fRushMinutes < ACR_AIDoctrine.Find("balanced").m_fRushMinutes;
  ACR_TestRegress.Check("lanes: doctrines fight down a lane (Raider raids everywhere), Fortress guards flanks hardest", used && raider && fronts && works && rush, "");

  // On by default from 0.5.13 (#232); -acrAIStrategy 0 or the classic commander turns them off, and the
  // part's own flag wins over the classic switch.
  bool onByDefault = ACR_AILanes.On();
  ACR_Options.Set("acrAIStrategy", "0");
  bool off = !ACR_AILanes.On();
  ACR_Options.Unset("acrAIStrategy");
  ACR_Options.Set("acrAICommander", "classic");
  bool classicOff = !ACR_AILanes.On();
  ACR_Options.Set("acrAIStrategy", "1");
  bool flagWins = ACR_AILanes.On();
  ACR_Options.Unset("acrAIStrategy");
  ACR_Options.Unset("acrAICommander");
  ACR_TestRegress.Check("lanes: on by default, -acrAIStrategy 0 or -acrAICommander classic turns them off", onByDefault && off && classicOff && flagWins && ACR_AILanes.On(), string.Format("default %1, flag off %2, classic off %3, flag over classic %4", onByDefault, off, classicOff, flagWins));
 }

 // On the test range: once the rush is over, a commander picks a lane from a base it holds toward
 // one it doesn't (or none when nothing is in sight), and an income base is never a flank strongpoint.
 static void CheckCommander(ACR_Manager manager)
 {
  string side = ACR_FactionSelection.West();
  ACR_AICommander ai = new ACR_AICommander(manager, side);
  ai.SetDoctrineForTest(ACR_AIDoctrine.Find("balanced"));
  ai.SetRushOverForTest(true);
  ai.UpdateLaneForTest();
  ACR_AILane lane = ai.Lane();
  // Pairs a lane could join: a base of ours, uncontested, and one in sight we don't hold, far enough.
  int pairs;
  foreach (ACR_Base own : ACR_Base.s_All)
  {
   if (own.m_bRetired || own.m_sOwner != side || own.m_bContested) continue;
   foreach (ACR_Base other : ACR_Base.s_All)
   {
    if (other.m_bRetired || other.m_bPocket || other.m_sOwner == side || !other.IsVisibleTo(side)) continue;
    if (vector.DistanceXZ(own.GetOrigin(), other.GetOrigin()) >= ACR_AILanes.MIN_LENGTH) pairs++;
   }
  }
  bool ok;
  string detail = "no lane";
  if (lane)
  {
   ok = lane.Valid(side) && lane.Contains(lane.m_Objective.GetOrigin()) && lane.Contains(lane.m_From.GetOrigin()) && ai.LaneActive();
   detail = string.Format("%1 -> %2, score %3", lane.m_From.m_sName, lane.m_Objective.m_sName, lane.m_fScore);
  }
  else ok = pairs == 0;
  ACR_TestRegress.Check("lanes: a commander picks a lane from its own base toward one it doesn't hold", ok, string.Format("%1 (%2 possible lanes)", detail, pairs));

  bool incomeNeverFlank = true;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_bIncome && ai.FlankStrongpointForTest(base)) incomeNeverFlank = false;
  ACR_TestRegress.Check("lanes: only tactical points become flank strongpoints", incomeNeverFlank, "");

  // A lane with nothing in it to attack doesn't hold attacks elsewhere back (soak 4 Nov: lanes
  // toward empty bases kept both sides from attacking all match); one with an enemy base in it does.
  // Narrow lanes from a base of ours to an empty base, then to an enemy one.
  ACR_Base ownStart, empty, enemy;
  foreach (ACR_Base pick : ACR_Base.s_All)
  {
   if (pick.m_bRetired || pick.m_bPocket) continue;
   if (!ownStart && pick.m_sOwner == side && !pick.m_bContested) ownStart = pick;
   else if (!empty && pick.OwnerSeenBy(side).IsEmpty() && pick.IsVisibleTo(side)) empty = pick;
   else if (!enemy && !pick.OwnerSeenBy(side).IsEmpty() && pick.m_sOwner != side && !ACR_Insurgency.Friendly(pick.OwnerSeenBy(side), side) && pick.IsVisibleTo(side)) enemy = pick;
  }
  ai.SetDoctrineForTest(ACR_AIDoctrine.Find("balanced"));
  bool openEmpty = true;
  bool closedEnemy = true;
  string found = string.Format("own %1, empty %2, enemy %3", ownStart != null, empty != null, enemy != null);
  if (ownStart && empty)
  {
   ACR_AILane toEmpty = new ACR_AILane();
   toEmpty.m_From = ownStart;
   toEmpty.m_Objective = empty;
   toEmpty.m_fHalfWidth = 1;
   ai.SetLaneForTest(toEmpty);
   openEmpty = ai.LaneOpenForTest();
  }
  if (ownStart && enemy)
  {
   ACR_AILane toEnemy = new ACR_AILane();
   toEnemy.m_From = ownStart;
   toEnemy.m_Objective = enemy;
   toEnemy.m_fHalfWidth = 1;
   ai.SetLaneForTest(toEnemy);
   closedEnemy = !ai.LaneOpenForTest();
  }
  ai.SetLaneForTest(null);
  ACR_TestRegress.Check("lanes: a lane with no enemy base in it lets attacks go elsewhere, one with an enemy base holds them", openEmpty && closedEnemy, found);

  // Raider has no lane.
  ai.SetDoctrineForTest(ACR_AIDoctrine.Find("raider"));
  ai.UpdateLaneForTest();
  ACR_TestRegress.Check("lanes: a Raider commander plans base by base", !ai.Lane() && !ai.LaneActive(), "");
 }
}
