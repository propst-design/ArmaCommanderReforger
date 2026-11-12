// The defence forecast (ACR_DefenceForecast, -acrDefenceForecast; Jacob 4 Nov), run from srv:regress:
// the option, and the model on made-up contacts round two real bases of the test world: a heading
// picks its base, two thrusts stay two ways, a far walker is beyond the horizon, standing contacts
// gathered close are massing, believed doctrines lean it, enemy lanes weigh the bases they run
// through, and the self-check tells a forecast attack from an unforecast one.
class ACR_TestForecast
{
 static void Run(ACR_Manager manager)
 {
  CheckOption();
  ACR_Base a, b;
  if (!TwoBases(a, b))
  {
   ACR_TestRegress.Check("forecast: two bases on the test world", false, "fewer than two bases");
   return;
  }
  CheckHeading(a, b);
  CheckTwoWays(a, b);
  CheckHorizon(a, b);
  CheckMassing(a, b);
  CheckDoctrine(a, b);
  CheckLanes(a, b);
  CheckSelfCheck(a, b);
  // Step 2: acting on it.
  CheckFirm(a);
  CheckDemand(a, b);
  CheckHedge();
  CheckScreenSpot(a, b);
 }

 static ACR_BaseForecast Entry(ACR_Base base, float expected, float eta)
 {
  ACR_BaseForecast entry = new ACR_BaseForecast();
  entry.m_Base = base;
  entry.m_fExpected = expected;
  entry.m_fEta = eta;
  return entry;
 }

 static void CheckFirm(ACR_Base a)
 {
  bool strongSoon = ACR_DefenceForecast.IsFirm(Entry(a, 4, 200), false);
  bool weak = ACR_DefenceForecast.IsFirm(Entry(a, 1.5, 200), false);
  bool late = ACR_DefenceForecast.IsFirm(Entry(a, 4, 500), false);
  bool remembered = ACR_DefenceForecast.IsFirm(Entry(a, 4, -1), false);
  // Once firm it holds on the lower bars (no flicker at the edge), and lets go once clearly faded.
  bool keepsWeak = ACR_DefenceForecast.IsFirm(Entry(a, 1.5, 200), true);
  bool keepsLate = ACR_DefenceForecast.IsFirm(Entry(a, 4, 500), true);
  bool fades = ACR_DefenceForecast.IsFirm(Entry(a, 0.5, 200), true);
  bool ok = strongSoon && !weak && !late && !remembered && keepsWeak && keepsLate && !fades;
  ACR_TestRegress.Check("forecast: firm when strong and due within lead time, held on lower bars, let go once faded", ok, string.Format("strong soon %1, weak %2, late %3, remembered %4, keeps weak %5, keeps late %6, faded %7", strongSoon, weak, late, remembered, keepsWeak, keepsLate, fades));
 }

 static void CheckDemand(ACR_Base a, ACR_Base b)
 {
  ACR_DefenceForecast forecast = new ACR_DefenceForecast("TEST");
  vector at = Beside(a, b, 600, 1);
  array<ACR_Base> ours = {};
  ours.Insert(a);
  ours.Insert(b);
  array<int> defence = {0, 0};
  array<ref ACR_ForecastContact> contacts = {};
  ACR_ForecastContact coming = MakeContact(at, ACR_Intel.Bearing(at, a.GetOrigin()), 10, true);
  contacts.Insert(coming);
  ACR_DefenceForecast.Compute(ours, defence, contacts, null, "", forecast.m_aBases);
  forecast.UpdateFirmForTest();
  int toA = forecast.Demand(a);
  int toB = forecast.Demand(b);
  // It turns away: nothing left to defend against.
  coming.m_fHeading = ACR_Intel.Bearing(a.GetOrigin(), at);
  ACR_DefenceForecast.Compute(ours, defence, contacts, null, "", forecast.m_aBases);
  forecast.UpdateFirmForTest();
  int after = forecast.Demand(a);
  ACR_TestRegress.Check("forecast: a firm forecast asks for defenders at its base only, and none once it turns away", toA > 0 && toB == 0 && after == 0, string.Format("A %1, B %2, A after it turned %3", toA, toB, after));
 }

 static void CheckHedge()
 {
  array<vector> spots = {Vector(0, 0, 0), Vector(1000, 0, 0), Vector(3000, 0, 0)};
  array<vector> two = {Vector(0, 0, 1500), Vector(2000, 0, 1500)};
  int between = ACR_DefenceForecast.Hedge(spots, two, -1, 300);
  // The current base stays while it's nearly as good; the reserve moves only for a clear gain.
  array<vector> near = {Vector(200, 0, 1500), Vector(1200, 0, 1500)}; // farthest: 1921 m from 0, 1700 m from 1
  int kept = ACR_DefenceForecast.Hedge(spots, near, 0, 300);
  int moved = ACR_DefenceForecast.Hedge(spots, near, 2, 300);
  array<vector> none = {};
  int nothing = ACR_DefenceForecast.Hedge(spots, none, -1, 300);
  ACR_TestRegress.Check("forecast: the reserve hedges where the farthest likely target is nearest, and doesn't move for a small gain", between == 1 && kept == 0 && moved == 1 && nothing == -1, string.Format("between %1, kept %2, moved %3, none %4", between, kept, moved, nothing));
 }

 static void CheckScreenSpot(ACR_Base a, ACR_Base b)
 {
  float way = ACR_Intel.Bearing(a.GetOrigin(), b.GetOrigin());
  array<ACR_Base> enemy = {};
  vector spot;
  int sees;
  bool found = ACR_DefenceScreen.Spot(a, way, enemy, spot, sees);
  float gap = vector.DistanceXZ(spot, a.GetOrigin()) - a.m_fRadius;
  float off = ACR_DefenceForecast.AngleBetween(ACR_Intel.Bearing(a.GetOrigin(), spot), way);
  bool placed = found && sees > 0 && gap >= ACR_DefenceScreen.OUT_LEAST - 5 && ACR_DefenceScreen.OUT_MOST + 5 >= gap && ACR_DefenceScreen.TURN_MOST + 1 >= off;
  ACR_TestRegress.Check("forecast: a screen spot is found outside the base, toward the way, seeing some of it", placed, string.Format("%1: found %2, %3 m out, %4 deg off the way, sees %5/%6", a.m_sName, found, Math.Round(gap), Math.Round(off), sees, ACR_DefenceScreen.SAMPLES));
 }

 static void CheckOption()
 {
  // Acting on it by default from 0.6.0 (#232); 1 only logs; 0 or the classic commander turns it off.
  bool actsByDefault = ACR_DefenceForecast.On() && ACR_DefenceForecast.Acts();
  ACR_Options.Set("acrDefenceForecast", "1");
  bool logs = ACR_DefenceForecast.On() && !ACR_DefenceForecast.Acts();
  ACR_Options.Set("acrDefenceForecast", "0");
  bool off = !ACR_DefenceForecast.On();
  ACR_Options.Unset("acrDefenceForecast");
  ACR_Options.Set("acrAICommander", "classic");
  bool classicOff = !ACR_DefenceForecast.On() && !ACR_Squad.CoveredFallBackOn() && ACR_AICommander.AllocatorMode() == 0;
  ACR_Options.Unset("acrAICommander");
  bool restored = ACR_DefenceForecast.Acts() && ACR_Squad.CoveredFallBackOn() && ACR_AICommander.AllocatorMode() == 2;
  ACR_TestRegress.Check("forecast: acts by default, 1 logs, 0 or -acrAICommander classic (also allocator, fall-back) turns it off", actsByDefault && logs && off && classicOff && restored, string.Format("default %1, log %2, off %3, classic off %4, back %5", actsByDefault, logs, off, classicOff, restored));
 }

 // Two live bases at least 800 m apart (a lane can run between them).
 static bool TwoBases(out ACR_Base a, out ACR_Base b)
 {
  foreach (ACR_Base first : ACR_Base.s_All)
  {
   if (!first || first.m_bRetired) continue;
   foreach (ACR_Base second : ACR_Base.s_All)
   {
    if (!second || second == first || second.m_bRetired) continue;
    if (vector.DistanceXZ(first.GetOrigin(), second.GetOrigin()) < 800) continue;
    a = first;
    b = second;
    return true;
   }
  }
  return false;
 }

 // A point 'metres' out from base 'at', square to the line between the two bases.
 static vector Beside(ACR_Base at, ACR_Base other, float metres, float side)
 {
  vector along = other.GetOrigin() - at.GetOrigin();
  along[1] = 0;
  along.Normalize();
  vector across = Vector(-along[2], 0, along[0]);
  return at.GetOrigin() + across * (metres * side);
 }

 static ACR_ForecastContact MakeContact(vector pos, float heading, float strength, bool mounted)
 {
  ACR_ForecastContact made = new ACR_ForecastContact();
  made.m_vPos = pos;
  made.m_fHeading = heading;
  made.m_fStrength = strength;
  made.m_bMounted = mounted;
  return made;
 }

 static float Expected(ACR_Base a, ACR_Base b, notnull array<ref ACR_ForecastContact> contacts, string doctrine, ACR_Base which, out ACR_BaseForecast entry)
 {
  array<ACR_Base> bases = {};
  bases.Insert(a);
  bases.Insert(b);
  array<int> defence = {10, 10};
  array<ref ACR_BaseForecast> result = {};
  ACR_DefenceForecast.Compute(bases, defence, contacts, null, doctrine, result);
  foreach (ACR_BaseForecast found : result)
  {
   if (found.m_Base != which) continue;
   entry = found;
   return found.m_fExpected;
  }
  return -1;
 }

 static void CheckHeading(ACR_Base a, ACR_Base b)
 {
  vector at = Beside(a, b, 600, 1);
  array<ref ACR_ForecastContact> contacts = {};
  contacts.Insert(MakeContact(at, ACR_Intel.Bearing(at, a.GetOrigin()), 8, true));
  ACR_BaseForecast ea, eb;
  float toA = Expected(a, b, contacts, "", a, ea);
  float toB = Expected(a, b, contacts, "", b, eb);
  ACR_TestRegress.Check("forecast: a contact heading for a base forecasts that one, not the other", toA > 1 && toA > toB * 3, string.Format("A %1, B %2", toA, toB));
 }

 static void CheckTwoWays(ACR_Base a, ACR_Base b)
 {
  vector north = Beside(a, b, 500, 1);
  vector south = Beside(a, b, 500, -1);
  array<ref ACR_ForecastContact> contacts = {};
  contacts.Insert(MakeContact(north, ACR_Intel.Bearing(north, a.GetOrigin()), 6, false));
  contacts.Insert(MakeContact(south, ACR_Intel.Bearing(south, a.GetOrigin()), 6, false));
  ACR_BaseForecast entry;
  Expected(a, b, contacts, "", a, entry);
  bool two = entry && entry.m_aWays.Count() == 2;
  bool apart = two && ACR_DefenceForecast.AngleBetween(entry.m_aWays[0].m_fBearing, entry.m_aWays[1].m_fBearing) > 150;
  string got = "none";
  if (entry) got = entry.Line();
  ACR_TestRegress.Check("forecast: attacks from two sides stay two ways in, not one mean", two && apart, got);
 }

 static void CheckHorizon(ACR_Base a, ACR_Base b)
 {
  // On foot 1.2 m/s: 1500 m past the ring is ~21 min, past the 15 min horizon.
  vector far = Beside(a, b, a.m_fRadius + 1500, 1);
  array<ref ACR_ForecastContact> contacts = {};
  contacts.Insert(MakeContact(far, ACR_Intel.Bearing(far, a.GetOrigin()), 8, false));
  ACR_BaseForecast entry;
  float onFoot = Expected(a, b, contacts, "", a, entry);
  contacts[0].m_bMounted = true;
  float mounted = Expected(a, b, contacts, "", a, entry);
  ACR_TestRegress.Check("forecast: a walker 20 min out isn't forecast yet; the same mounted is", onFoot <= 0 && mounted > 0, string.Format("foot %1, mounted %2", onFoot, mounted));
 }

 static void CheckMassing(ACR_Base a, ACR_Base b)
 {
  vector at = Beside(a, b, 450, 1);
  array<ref ACR_ForecastContact> one = {};
  one.Insert(MakeContact(at, -1, 6, false));
  ACR_BaseForecast entry;
  float alone = Expected(a, b, one, "", a, entry);
  array<ref ACR_ForecastContact> two = {};
  two.Insert(MakeContact(at, -1, 6, false));
  two.Insert(MakeContact(at + Vector(60, 0, 0), -1, 6, false));
  float gathered = Expected(a, b, two, "", a, entry);
  bool flagged = entry && entry.m_bMassing;
  ACR_TestRegress.Check("forecast: standing contacts gathered close mass on the nearest base", flagged && gathered > alone * 2, string.Format("alone %1, gathered %2", alone, gathered));
 }

 static void CheckDoctrine(ACR_Base a, ACR_Base b)
 {
  vector at = Beside(a, b, 700, 1);
  array<ref ACR_ForecastContact> contacts = {};
  contacts.Insert(MakeContact(at, ACR_Intel.Bearing(at, a.GetOrigin()), 8, true));
  ACR_BaseForecast entry;
  float plain = Expected(a, b, contacts, "", a, entry);
  float blitz = Expected(a, b, contacts, "blitz", a, entry);
  float fortress = Expected(a, b, contacts, "fortress", a, entry);
  ACR_TestRegress.Check("forecast: a believed Blitz comes harder mounted, a Fortress seldom", blitz > plain && fortress < plain, string.Format("plain %1, blitz %2, fortress %3", plain, blitz, fortress));
 }

 // Any three live bases where one, taken as the enemy's, is a lane's length from one of the other two.
 static bool Triple(out ACR_Base enemyBase, out ACR_Base x, out ACR_Base y)
 {
  foreach (ACR_Base e : ACR_Base.s_All)
  {
   if (!e || e.m_bRetired) continue;
   foreach (ACR_Base p : ACR_Base.s_All)
   {
    if (!p || p == e || p.m_bRetired || vector.DistanceXZ(e.GetOrigin(), p.GetOrigin()) < ACR_AILanes.MIN_LENGTH) continue;
    foreach (ACR_Base q : ACR_Base.s_All)
    {
     if (!q || q == e || q == p || q.m_bRetired) continue;
     enemyBase = e;
     x = p;
     y = q;
     return true;
    }
   }
  }
  return false;
 }

 static void CheckLanes(ACR_Base unusedA, ACR_Base unusedB)
 {
  ACR_Base enemyBase, a, b;
  if (!Triple(enemyBase, a, b))
  {
   ACR_TestRegress.Check("forecast: enemy lanes weigh the bases they run through", false, "no three bases on the test world");
   return;
  }
  array<ACR_Base> ours = {};
  ours.Insert(a);
  ours.Insert(b);
  array<int> defence = {0, 0};
  array<ACR_Base> enemy = {};
  enemy.Insert(enemyBase);
  map<ACR_Base, float> shares = new map<ACR_Base, float>();
  ACR_DefenceForecast.LaneShares(ours, defence, enemy, 700, shares);
  float sa, sb;
  shares.Find(a, sa);
  shares.Find(b, sb);
  float most = Math.Max(sa, sb);
  // With the same contact, the more-run base draws more of it once the lanes count.
  vector mid = (a.GetOrigin() + b.GetOrigin()) * 0.5;
  array<ref ACR_ForecastContact> contacts = {};
  contacts.Insert(MakeContact(mid, -1, 8, true));
  array<ref ACR_BaseForecast> without = {};
  array<ref ACR_BaseForecast> leaned = {};
  ACR_DefenceForecast.Compute(ours, defence, contacts, null, "", without);
  ACR_DefenceForecast.Compute(ours, defence, contacts, shares, "", leaned);
  int top = 0;
  if (sb > sa) top = 1;
  bool leans = most > 0.99 && leaned[top].m_fExpected > without[top].m_fExpected;
  ACR_TestRegress.Check("forecast: enemy lanes weigh the bases they run through", leans, string.Format("shares %1 %2 / %3 %4 (enemy %5)", a.m_sName, sa, b.m_sName, sb, enemyBase.m_sName));
 }

 static void CheckSelfCheck(ACR_Base a, ACR_Base b)
 {
  ACR_DefenceForecast forecast = new ACR_DefenceForecast("TEST");
  // Unforecast: a threat out of nothing.
  forecast.NoteThreat(a, 6, true, Beside(a, b, 300, 1), 1000);
  bool unforecast = forecast.m_iUnforecast == 1 && forecast.m_iAtDoor == 1;
  forecast.NoteThreat(a, 0, false, vector.Zero, 2000);
  // Forecast from one side, then the threat comes from that side: a hit.
  vector at = Beside(a, b, 600, -1);
  array<ACR_Base> ours = {};
  ours.Insert(a);
  ours.Insert(b);
  array<int> defence = {10, 10};
  array<ref ACR_ForecastContact> contacts = {};
  contacts.Insert(MakeContact(at, ACR_Intel.Bearing(at, a.GetOrigin()), 10, true));
  ACR_DefenceForecast.Compute(ours, defence, contacts, null, "", forecast.m_aBases);
  forecast.NoteForTest(a, 4000);
  forecast.NoteThreat(a, 8, true, Beside(a, b, 250, -1), 60000);
  bool hit = forecast.m_iHits == 1;
  ACR_TestRegress.Check("forecast: the self-check tells a forecast attack from one nobody saw coming", unforecast && hit, string.Format("unforecast %1, hits %2, wrong way %3", forecast.m_iUnforecast, forecast.m_iHits, forecast.m_iWrongWay));
 }
}
