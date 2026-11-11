// Where the next attack on us will come from (Jacob, 4 Nov 2026: "better defence logic based on intel
// and potential attack vectors"). Step 1 of the defence rework (design: docs/handoff/ai-allocator-
// design.md, "Defence: read the attack before it lands"): a forecast, built only from what the side
// knows, logged and shown in AC:A Think so it can be judged before anything acts on it.
// -acrDefenceForecast 1 logs it, 2 (the default from 0.5.13, #232) also acts on it; 0 or -acrAICommander
// classic turns it off.
//
// Today the commander only counts enemies already inside THREAT_DISTANCE of a base. The forecast looks
// further out, per base of ours:
//   - every enemy contact the side knows of (ACR_Intel.m_Contacts) is projected along its heading (the
//     map's arrow) at its kind's speed; a base inside the cone round that heading, and soon along it,
//     is a likely target. A contact standing still threatens the bases it could reach, weakly. A part
//     of each contact's weight goes to "none of ours": it may be going elsewhere;
//   - contacts gathered close together and standing within reach of a base are massing for it;
//   - the lanes the enemy would push down if it reasoned as our own commander does (ACR_AILanes, from
//     the enemy bases we can see to ours, against the defence we have there) make bases in them likelier;
//   - the enemy doctrine we have confirmed (ACR_DoctrineIntel.Believed(), never the truth) leans it:
//     Blitz comes mounted, a Raider goes for the weak, a Fortress seldom comes at all;
//   - the ways bases were lost before are remembered for a while.
// Per base: the expected attacking strength, when it would arrive, and the ways in (one per direction
// it comes from, so two thrusts stay two, where today's mean of sightings merges them into one).
// It also scores itself: when a base comes under threat it logs whether that was forecast, from which
// way and how long before ("forecast check"), and a forecast that fades with no attack ("lapsed").
// The geometry and weights are pure functions of their inputs (Compute), so tests run them without a world.
class ACR_ForecastWay : Managed
{
 float m_fBearing;     // degrees (0 = north) from the base toward where it comes from
 float m_fStrength;    // expected strength by this way
 float m_fEta = -1;    // s until the first of it arrives (-1: remembered, not seen)
 int m_iContacts;
 bool m_bMounted;
 bool m_bRemembered;   // a way a base was lost by before
 float m_fX, m_fZ;     // weighted direction sum while merging
}

class ACR_BaseForecast : Managed
{
 ACR_Base m_Base;
 float m_fExpected;    // expected attacking strength
 float m_fEta = -1;    // s until the bulk of it could arrive
 float m_fTopP;        // the likeliest single contact's chance of coming here
 int m_iContacts;
 float m_fLane;        // 0-1: how much the enemy's likely lanes run through it
 bool m_bMassing;
 ref array<ref ACR_ForecastWay> m_aWays = {};

 // "Szilis: ~7 (P .62) in ~4 min; NE 5 (2, mounted), S 2 | lane .4, massing"
 string Line()
 {
  string text = string.Format("%1: ~%2", m_Base.m_sName, Math.Round(m_fExpected));
  text += string.Format(" (P %1)", ACR_DefenceForecast.Two(m_fTopP));
  if (m_fEta >= 0) text += string.Format(" in ~%1 min", Math.Max(1, Math.Round(m_fEta / 60)));
  string ways;
  foreach (ACR_ForecastWay way : m_aWays)
  {
   if (!ways.IsEmpty()) ways += ", ";
   ways += ACR_DefenceForecast.CompassOf(way.m_fBearing);
   if (way.m_bRemembered) { ways += " (lost that way before)"; continue; }
   ways += string.Format(" %1 (%2", Math.Round(way.m_fStrength), way.m_iContacts);
   if (way.m_bMounted) ways += ", mounted";
   ways += ")";
  }
  if (!ways.IsEmpty()) text += "; " + ways;
  string notes;
  if (m_fLane > 0.05) notes = "lane " + ACR_DefenceForecast.Two(m_fLane);
  if (m_bMassing)
  {
   if (!notes.IsEmpty()) notes += ", ";
   notes += "massing";
  }
  if (!notes.IsEmpty()) text += " | " + notes;
  return text;
 }

 ACR_ForecastWay LeadWay()
 {
  ACR_ForecastWay best;
  foreach (ACR_ForecastWay way : m_aWays)
   if (!way.m_bRemembered && (!best || way.m_fStrength > best.m_fStrength)) best = way;
  return best;
 }
}

// One enemy contact as the forecast sees it (from an ACR_Contact, or made up by a test).
class ACR_ForecastContact : Managed
{
 vector m_vPos;
 float m_fHeading = -1; // degrees, -1: none
 float m_fAgeS;
 float m_fStrength;
 bool m_bMounted;
 float m_fSpeed = -1;   // m/s it was seen moving at (-1: unknown, the kind's speed is used)
}

class ACR_DefenceForecast : Managed
{
 // Weights of the model (how strongly each piece of intel counts; not distances on the ground).
 static const float CONE_POWER = 4;         // cos^4 of the angle off its heading: half weight ~33 degrees off
 static const float STANDING = 0.3;         // a contact with no heading, toward any base it could reach
 static const float ETA_SCALE_S = 300;      // a base 5 min further along its way counts half
 static const float NONE_WEIGHT = 0.5;      // the share of a contact going for none of ours
 static const float HORIZON_S = 900;        // bases further than 15 min from a contact aren't forecast from it
 static const float FOOT_SPEED = 1.2;       // m/s, as ACR_AICommander.WALK_SPEED
 static const float MOUNTED_SPEED = 6;      // m/s, as ACR_AICommander.MOUNTED_SPEED
 static const float SPEED_LEAST = 0.5;      // m/s: an observed speed is used between these
 static const float SPEED_MOST = 25;
 static const float LANE_PRIOR = 0.6;       // a base on the enemy's likeliest lane counts this much more
 static const float MASS_GAP = 150;         // m: standing contacts this close together are one gathering
 static const float MASS_BOOST = 1.5;       // ... and a gathering counts this much more toward its nearest base
 static const float WAY_MERGE = 40;         // degrees: contributions this close in bearing are one way
 static const float LOSS_WEIGHT = 2;        // strength a remembered way shows with when fresh
 static const float LOSS_MEMORY_MS = 1800000; // 30 min until a way we lost a base by is forgotten
 static const float BLITZ_MOUNTED = 1.3;    // Blitz: its mounted contacts count this much more
 static const float RAIDER_WEAK = 0.5;      // Raider: a weakly held base counts this much more
 static const int RAIDER_WEAK_DEFENCE = 6;  // ... held by less than this
 static const float FORTRESS_FACTOR = 0.7;  // Fortress: it seldom comes out
 static const float NOTE = 2;               // expected strength from which a forecast is logged and checked
 static const float LOG_MS = 60000;         // a base's forecast logged again at most this often, unless it changed a lot
 static const float CHECK_ANGLE = 60;       // degrees: the threat came "by the forecast way" within this
 static const int THINK_BASES = 5;

 string m_sFaction;
 ref array<ref ACR_BaseForecast> m_aBases = {};
 // Self-check: when a base's forecast first reached NOTE (cleared when it lapses), what it last
 // said, and whether the base was threatened last tick.
 protected ref map<ACR_Base, float> m_NotedSince = new map<ACR_Base, float>();
 protected ref map<ACR_Base, float> m_LoggedAt = new map<ACR_Base, float>();
 protected ref map<ACR_Base, float> m_LoggedExpected = new map<ACR_Base, float>();
 protected ref map<ACR_Base, float> m_LoggedBearing = new map<ACR_Base, float>();
 protected ref map<ACR_Base, bool> m_Threatened = new map<ACR_Base, bool>();
 protected ref map<ACR_Base, float> m_ThreatBearing = new map<ACR_Base, float>();
 protected ref map<ACR_Base, bool> m_Came = new map<ACR_Base, bool>(); // an attack came while it was noted
 // Ways bases were lost by: bearing and world time.
 protected ref map<ACR_Base, float> m_LostBearing = new map<ACR_Base, float>();
 protected ref map<ACR_Base, float> m_LostAt = new map<ACR_Base, float>();
 // The enemy lanes' weight per base of ours, refreshed every ACR_AILanes.REVIEW_S.
 protected ref map<ACR_Base, float> m_LaneShare = new map<ACR_Base, float>();
 protected float m_fLaneAt = -1;
 protected float m_fSummaryAt = -1;
 static const float SUMMARY_MS = 300000;    // a summary line this often, for reading soaks
 int m_iHits, m_iWrongWay, m_iUnforecast, m_iLapsed;
 int m_iAtDoor; // of the unforecast: nobody had seen them before they were at the base
 ref array<string> m_aSaid = {}; // lines for the commander's log, taken by it each tick

 // The commander acting on it (-acrDefenceForecast 2, defence step 2): a firm forecast (enough
 // expected, due within lead time) is defended ahead of the attack; it stays firm until it has
 // clearly faded (a feint's projection decays, and its defenders are released by the same count).
 // A fire team of 4 coming straight on reads ~2.5 (part of every contact is kept for 'going elsewhere'):
 // at 3 it never firmed up (forecastdefend, 4 Nov). One man walking in stays under it.
 static const float FIRM = 2;               // expected strength from which a base is defended ahead
 static const float FIRM_KEEP = 1;          // ... and stays defended until it falls below this
 static const float LEAD_S = 360;           // s: defended ahead when the attack is due within this
 static const float LEAD_KEEP_S = 600;      // ... and stays defended while it is due within this
 static const int HEDGE_TARGETS = 3;        // likeliest targets the reserve hedges between
 protected ref map<ACR_Base, bool> m_Firm = new map<ACR_Base, bool>();

 void ACR_DefenceForecast(string faction) { m_sFaction = faction; }

 // 0 off (default), 1 forecast + log + Think, 2 the AI commander also acts on it.
 // 2 by default (#232), 0 with the classic commander (ACR_AIRework).
 static bool On() { return ACR_LogisticsRules.Choice("acrDefenceForecast", ACR_AIRework.Default("2")) != "0"; }
 static bool Acts() { return ACR_LogisticsRules.Choice("acrDefenceForecast", ACR_AIRework.Default("2")).ToInt() >= 2; }

 // Firm: worth defending ahead now. A base firm last tick keeps it on the lower bars.
 static bool IsFirm(ACR_BaseForecast entry, bool wasFirm)
 {
  if (!entry || entry.m_fEta < 0) return false;
  if (wasFirm) return entry.m_fExpected >= FIRM_KEEP && LEAD_KEEP_S >= entry.m_fEta;
  return entry.m_fExpected >= FIRM && LEAD_S >= entry.m_fEta;
 }

 // Strength a base of ours should be defended against ahead of the attack (0: not firm).
 int Demand(ACR_Base base)
 {
  bool firm;
  if (!m_Firm.Find(base, firm) || !firm) return 0;
  ACR_BaseForecast entry = Of(base);
  if (!entry) return 0;
  return (int)Math.Max(1, Math.Round(entry.m_fExpected));
 }

 // Our bases an attack is forecast on (expected at least NOTE), likeliest first, at most 'most'.
 void Watched(notnull array<ACR_Base> into, int most)
 {
  into.Clear();
  foreach (ACR_BaseForecast entry : m_aBases)
  {
   if (into.Count() >= most || entry.m_fExpected < NOTE) break;
   into.Insert(entry.m_Base);
  }
 }

 // Of 'candidates', the one whose farthest target is nearest: a reserve there reaches the worst case
 // soonest, so a show of force at one base moves it toward both, not into one. 'keep' (-1: none) stays
 // unless another is better by more than 'margin' m. -1: no candidates or no targets.
 static int Hedge(notnull array<vector> candidates, notnull array<vector> targets, int keep, float margin)
 {
  if (targets.IsEmpty()) return -1;
  int best = -1;
  float bestWorst = float.MAX;
  float keptWorst = float.MAX;
  foreach (int c, vector at : candidates)
  {
   float worst = 0;
   foreach (vector target : targets) worst = Math.Max(worst, vector.DistanceXZ(at, target));
   if (c == keep) keptWorst = worst;
   if (worst < bestWorst)
   {
    bestWorst = worst;
    best = c;
   }
  }
  if (keep >= 0 && keptWorst <= bestWorst + margin) return keep;
  return best;
 }

 static string Two(float value) { return "." + Math.ClampInt(Math.Round(value * 100), 0, 99).ToString(2); }

 static string CompassOf(float bearing)
 {
  array<string> names = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  float b = bearing;
  while (b < 0) b += 360;
  int index = (int)Math.Round(b / 45.0);
  if (index >= 8) index = 0;
  return names[index];
 }

 // Smallest difference between two bearings, 0-180.
 static float AngleBetween(float a, float b)
 {
  float d = Math.AbsFloat(a - b);
  while (d > 360) d -= 360;
  if (d > 180) d = 360 - d;
  return d;
 }

 // How likely a contact heading 'heading' (-1: none) is to be going for something at 'bearing' from it.
 static float DirectionWeight(float heading, float bearing)
 {
  if (heading < 0) return STANDING;
  float c = Math.Cos(AngleBetween(heading, bearing) * Math.DEG2RAD);
  if (c <= 0) return 0;
  return Math.Pow(c, CONE_POWER);
 }

 // Seconds for a contact to reach a base 'distance' metres off (past the base's ring), allowing for
 // how far it has moved toward it since it was seen.
 static float Eta(ACR_ForecastContact contact, float distance, float bearing)
 {
  // The speed it was seen moving at, else its kind's (a squad on a move order runs at ~3 m/s,
  // three times the long-route foot average: forecastattack, 4 Nov).
  float speed = FOOT_SPEED;
  if (contact.m_bMounted) speed = MOUNTED_SPEED;
  if (contact.m_fSpeed > 0) speed = Math.Clamp(contact.m_fSpeed, SPEED_LEAST, SPEED_MOST);
  float moved = 0;
  if (contact.m_fHeading >= 0) moved = contact.m_fAgeS * speed * Math.Max(0, Math.Cos(AngleBetween(contact.m_fHeading, bearing) * Math.DEG2RAD));
  return Math.Max(0, distance - moved) / speed;
 }

 // The forecast for 'bases' (ours, with their defence) from 'contacts'. 'laneShare' and 'massing'
 // may be null. 'doctrine' is the enemy doctrine we believe ("" none). Pure: no world reads.
 static void Compute(notnull array<ACR_Base> bases, notnull array<int> defence, notnull array<ref ACR_ForecastContact> contacts, map<ACR_Base, float> laneShare, string doctrine, notnull array<ref ACR_BaseForecast> result)
 {
  result.Clear();
  foreach (ACR_Base seedBase : bases)
  {
   ACR_BaseForecast seedEntry = new ACR_BaseForecast();
   seedEntry.m_Base = seedBase;
   float lane;
   if (laneShare) laneShare.Find(seedBase, lane);
   seedEntry.m_fLane = lane;
   result.Insert(seedEntry);
  }
  array<bool> massing = {};
  Massing(contacts, massing);
  // Earliest arrival of the bulk: weighted ETAs per base.
  array<float> etaSum = {};
  array<float> etaWeight = {};
  foreach (ACR_BaseForecast seeded : result) { etaSum.Insert(0); etaWeight.Insert(0); }
  foreach (int c, ACR_ForecastContact contact : contacts)
  {
   if (contact.m_fStrength <= 0) continue;
   float strength = contact.m_fStrength;
   if (doctrine == "blitz" && contact.m_bMounted) strength *= BLITZ_MOUNTED;
   if (doctrine == "fortress") strength *= FORTRESS_FACTOR;
   // How much each base draws this contact.
   array<float> pull = {};
   array<float> etas = {};
   array<float> bearings = {};
   float total = NONE_WEIGHT;
   int nearest = -1;
   float nearestEta = -1;
   foreach (int b, ACR_Base candidate : bases)
   {
    vector center = candidate.GetOrigin();
    float distance = Math.Max(0, vector.DistanceXZ(contact.m_vPos, center) - candidate.m_fRadius);
    float toBase = ACR_Intel.Bearing(contact.m_vPos, center);
    float eta = Eta(contact, distance, toBase);
    float w = 0;
    if (eta <= HORIZON_S)
    {
     w = DirectionWeight(contact.m_fHeading, toBase) / (1 + eta / ETA_SCALE_S);
     w *= 1 + LANE_PRIOR * result[b].m_fLane;
     if (doctrine == "raider" && defence[b] < RAIDER_WEAK_DEFENCE) w *= 1 + RAIDER_WEAK;
     if (nearest < 0 || eta < nearestEta) { nearest = b; nearestEta = eta; }
    }
    pull.Insert(w);
    etas.Insert(eta);
    bearings.Insert(ACR_Intel.Bearing(center, contact.m_vPos));
   }
   if (massing[c] && nearest >= 0)
   {
    pull[nearest] = pull[nearest] * MASS_BOOST;
    result[nearest].m_bMassing = true;
   }
   foreach (float p : pull) total += p;
   foreach (int k, ACR_BaseForecast drawn : result)
   {
    if (pull[k] <= 0) continue;
    float chance = pull[k] / total;
    float expected = strength * chance;
    drawn.m_fExpected += expected;
    drawn.m_iContacts++;
    drawn.m_fTopP = Math.Max(drawn.m_fTopP, chance);
    etaSum[k] = etaSum[k] + etas[k] * expected;
    etaWeight[k] = etaWeight[k] + expected;
    AddToWay(drawn, bearings[k], expected, etas[k], contact.m_bMounted, false);
   }
  }
  foreach (int e, ACR_BaseForecast done : result)
  {
   if (etaWeight[e] > 0) done.m_fEta = etaSum[e] / etaWeight[e];
   SortWays(done);
  }
 }

 // Standing contacts within MASS_GAP of another standing contact: a gathering.
 static void Massing(notnull array<ref ACR_ForecastContact> contacts, notnull array<bool> massing)
 {
  massing.Clear();
  foreach (ACR_ForecastContact a : contacts)
  {
   bool gathered = false;
   if (a.m_fHeading < 0)
   {
    foreach (ACR_ForecastContact b : contacts)
     if (b != a && b.m_fHeading < 0 && vector.DistanceXZ(a.m_vPos, b.m_vPos) <= MASS_GAP) gathered = true;
   }
   massing.Insert(gathered);
  }
 }

 static void AddToWay(ACR_BaseForecast entry, float bearing, float strength, float eta, bool mounted, bool remembered)
 {
  ACR_ForecastWay way;
  foreach (ACR_ForecastWay had : entry.m_aWays)
  {
   if (had.m_bRemembered != remembered || AngleBetween(had.m_fBearing, bearing) > WAY_MERGE) continue;
   way = had;
   break;
  }
  if (!way)
  {
   way = new ACR_ForecastWay();
   way.m_bRemembered = remembered;
   entry.m_aWays.Insert(way);
  }
  float rad = bearing * Math.DEG2RAD;
  float w = Math.Max(strength, 0.01);
  way.m_fX = way.m_fX + Math.Sin(rad) * w;
  way.m_fZ = way.m_fZ + Math.Cos(rad) * w;
  way.m_fBearing = Math.Atan2(way.m_fX, way.m_fZ) * Math.RAD2DEG;
  if (way.m_fBearing < 0) way.m_fBearing = way.m_fBearing + 360;
  way.m_fStrength = way.m_fStrength + strength;
  if (!remembered)
  {
   way.m_iContacts++;
   if (mounted) way.m_bMounted = true;
   if (way.m_fEta < 0 || eta < way.m_fEta) way.m_fEta = eta;
  }
 }

 static void SortWays(ACR_BaseForecast entry)
 {
  for (int i = 1; i < entry.m_aWays.Count(); i++)
  {
   for (int j = i; j > 0 && entry.m_aWays[j].m_fStrength > entry.m_aWays[j - 1].m_fStrength; j--)
    entry.m_aWays.SwapItems(j, j - 1);
  }
 }

 // The enemy's likely lanes, as our own commander would pick them from its side: from each enemy
 // base we can see to each base of ours at least ACR_AILanes.MIN_LENGTH away, what it would win
 // (income of our bases in the lane) against what it would meet (our defence there). Each base of
 // ours gets the share of lane score running through it, the most-run base 1.
 static void LaneShares(notnull array<ACR_Base> ours, notnull array<int> defence, notnull array<ACR_Base> enemy, float halfWidth, notnull map<ACR_Base, float> shares)
 {
  shares.Clear();
  array<float> sum = {};
  foreach (ACR_Base o : ours) sum.Insert(0);
  foreach (ACR_Base from : enemy)
  {
   vector a = from.GetOrigin();
   foreach (ACR_Base to : ours)
   {
    vector b = to.GetOrigin();
    float length = vector.DistanceXZ(a, b);
    if (length < ACR_AILanes.MIN_LENGTH) continue;
    float gain = 0;
    float cost = 0;
    array<int> inLane = {};
    foreach (int i, ACR_Base passed : ours)
    {
     if (!ACR_AILanes.InLane(a, b, halfWidth, passed.GetOrigin())) continue;
     gain += ACR_AILanes.Gain(passed.m_bIncome, passed.m_iValue);
     cost += defence[i];
     inLane.Insert(i);
    }
    float score = ACR_AILanes.Score(gain, cost, length);
    foreach (int k : inLane) sum[k] = sum[k] + score;
   }
  }
  float most = 0;
  foreach (float s : sum) most = Math.Max(most, s);
  if (most <= 0) return;
  foreach (int n, ACR_Base share : ours) shares.Set(share, sum[n] / most);
 }

 static ACR_ForecastContact FromContact(ACR_Contact contact, float now)
 {
  ACR_ForecastContact made = new ACR_ForecastContact();
  made.m_vPos = contact.m_Pos;
  made.m_fHeading = contact.m_fHeading;
  made.m_fSpeed = contact.m_fSpeed;
  made.m_fAgeS = Math.Max(0, (now - contact.m_fSeen) / 1000);
  made.m_bMounted = contact.m_iKind == ACR_Intel.KIND_VEHICLE || contact.m_iKind == ACR_Intel.KIND_ARMED || contact.m_iKind == ACR_Intel.KIND_ARMOUR;
  // As the commander's threat count weighs a squad (its strength); a group we don't manage by the men seen.
  if (contact.m_Squad) made.m_fStrength = contact.m_Squad.Strength();
  else made.m_fStrength = contact.m_iCount;
  // Older sightings count less, down to half by the time intel forgets them.
  made.m_fStrength = made.m_fStrength * (1 - 0.5 * Math.Clamp(made.m_fAgeS * 1000 / ACR_Intel.MEMORY_MS, 0, 1));
  return made;
 }

 // One strategic tick. 'contacts' from this side's intel (aircraft left out: they don't take bases);
 // 'ours'/'defence' our bases and the commander's defence count; 'enemy' the enemy bases we can see.
 void Update(notnull array<ref ACR_Contact> contacts, notnull array<ACR_Base> ours, notnull array<int> defence, notnull array<ACR_Base> enemy, float laneHalfWidth, string doctrine, float now)
 {
  if (m_fLaneAt < 0 || now - m_fLaneAt >= ACR_AILanes.REVIEW_S * 1000)
  {
   m_fLaneAt = now;
   LaneShares(ours, defence, enemy, laneHalfWidth, m_LaneShare);
  }
  array<ref ACR_ForecastContact> projected = {};
  foreach (ACR_Contact contact : contacts)
  {
   if (!contact || contact.m_iKind == ACR_Intel.KIND_AIR) continue;
   projected.Insert(FromContact(contact, now));
  }
  Compute(ours, defence, projected, m_LaneShare, doctrine, m_aBases);
  if (m_fSummaryAt < 0 || now - m_fSummaryAt >= SUMMARY_MS)
  {
   m_fSummaryAt = now;
   int headed;
   foreach (ACR_ForecastContact moving : projected) if (moving.m_fHeading >= 0) headed++;
   string top = "nothing expected";
   if (!m_aBases.IsEmpty() && m_aBases[0].m_fExpected > 0) top = m_aBases[0].Line();
   m_aSaid.Insert(string.Format("forecast summary: %1 contacts (%2 moving), %3 bases of ours, enemy bases seen %4, doctrine believed '%5'; checks %6 hit, %7 wrong way, %8 not forecast, %9 lapsed; top: ", projected.Count(), headed, ours.Count(), enemy.Count(), doctrine, m_iHits, m_iWrongWay, m_iUnforecast, m_iLapsed) + top + string.Format(" (%1 not forecast were first seen at the base)", m_iAtDoor));
  }
  // Remembered ways.
  foreach (ACR_BaseForecast entry : m_aBases)
  {
   float lostAt;
   if (!m_LostAt.Find(entry.m_Base, lostAt)) continue;
   float age = now - lostAt;
   if (age >= LOSS_MEMORY_MS) continue;
   AddToWay(entry, m_LostBearing.Get(entry.m_Base), LOSS_WEIGHT * (1 - age / LOSS_MEMORY_MS), -1, false, true);
  }
  // Strongest first.
  for (int i = 1; i < m_aBases.Count(); i++)
   for (int j = i; j > 0 && m_aBases[j].m_fExpected > m_aBases[j - 1].m_fExpected; j--)
    m_aBases.SwapItems(j, j - 1);
  foreach (ACR_BaseForecast noted : m_aBases) LogChange(noted, now);
  UpdateFirm();
  // Lapsed: a base noted before whose forecast fell away with no attack.
  array<ACR_Base> lapsed = {};
  foreach (ACR_Base base, float since : m_NotedSince)
  {
   ACR_BaseForecast still = Of(base);
   if (still && 0.5 * NOTE <= still.m_fExpected) continue;
   lapsed.Insert(base);
  }
  foreach (ACR_Base gone : lapsed)
  {
   m_NotedSince.Remove(gone);
   m_LoggedAt.Remove(gone);
   bool came;
   m_Came.Find(gone, came);
   m_Came.Remove(gone);
   bool attacked;
   m_Threatened.Find(gone, attacked);
   if (came || attacked || !gone) continue;
   m_iLapsed++;
   m_aSaid.Insert(string.Format("forecast lapsed at %1: no attack came", gone.m_sName));
  }
 }

 // Which bases are firm now; said when one turns firm or fades.
 protected void UpdateFirm()
 {
  map<ACR_Base, bool> was = new map<ACR_Base, bool>();
  was.Copy(m_Firm);
  m_Firm.Clear();
  foreach (ACR_BaseForecast entry : m_aBases)
  {
   bool before;
   was.Find(entry.m_Base, before);
   bool firm = IsFirm(entry, before);
   if (firm) m_Firm.Set(entry.m_Base, true);
   if (firm && !before) m_aSaid.Insert(string.Format("forecast firm at %1, defending ahead: %2", entry.m_Base.m_sName, entry.Line()));
   if (!firm && before) m_aSaid.Insert(string.Format("forecast at %1 no longer firm (~%2 expected)", entry.m_Base.m_sName, Math.Round(entry.m_fExpected)));
  }
 }

 // Tests: firm as a strategic tick would leave it.
 void UpdateFirmForTest() { UpdateFirm(); }

 ACR_BaseForecast Of(ACR_Base base)
 {
  foreach (ACR_BaseForecast entry : m_aBases)
   if (entry.m_Base == base) return entry;
  return null;
 }

 protected void LogChange(ACR_BaseForecast entry, float now)
 {
  if (entry.m_fExpected < NOTE) return;
  ACR_Base base = entry.m_Base;
  if (!m_NotedSince.Contains(base)) m_NotedSince.Set(base, now);
  ACR_ForecastWay lead = entry.LeadWay();
  float bearing = -1;
  if (lead) bearing = lead.m_fBearing;
  float at, expected, had;
  bool logged = m_LoggedAt.Find(base, at);
  m_LoggedExpected.Find(base, expected);
  m_LoggedBearing.Find(base, had);
  bool changed = !logged || entry.m_fExpected > expected * 1.5 || entry.m_fExpected < expected * 0.67;
  if (logged && bearing >= 0 && had >= 0 && AngleBetween(bearing, had) > WAY_MERGE) changed = true;
  if (!changed || (logged && now - at < LOG_MS)) return;
  m_LoggedAt.Set(base, now);
  m_LoggedExpected.Set(base, entry.m_fExpected);
  m_LoggedBearing.Set(base, bearing);
  m_aSaid.Insert("forecast " + entry.Line());
 }

 // Each strategic tick after the commander's threat count: whether the base is threatened now and
 // from where (ThreatFrom). A base newly threatened is checked against what was forecast.
 void NoteThreat(ACR_Base base, int threat, bool seen, vector from, float now)
 {
  bool was;
  m_Threatened.Find(base, was);
  bool now_ = threat > 0;
  m_Threatened.Set(base, now_);
  if (!now_ || !seen) return;
  float bearing = ACR_Intel.Bearing(base.GetOrigin(), from);
  m_ThreatBearing.Set(base, bearing);
  if (was) return;
  m_Came.Set(base, true);
  // A man or two isn't an attack: not checked.
  if (NOTE > threat) return;
  float since;
  ACR_BaseForecast entry = Of(base); // last tick's forecast
  if (!m_NotedSince.Find(base, since) || !entry)
  {
   m_iUnforecast++;
   string why = "first seen at the base (no sighting before it)";
   if (entry && entry.m_fExpected > 0.05) why = string.Format("seen before, but only %1 expected (logged from %2)", entry.m_fExpected.ToString(), NOTE);
   else m_iAtDoor++;
   m_aSaid.Insert(string.Format("forecast check %1: threat %2 from %3, not forecast: %4", base.m_sName, threat, CompassOf(bearing), why));
   return;
  }
  ACR_ForecastWay lead = entry.LeadWay();
  bool rightWay = false;
  foreach (ACR_ForecastWay way : entry.m_aWays)
   if (!way.m_bRemembered && AngleBetween(way.m_fBearing, bearing) <= CHECK_ANGLE) rightWay = true;
  string leadText = "?";
  if (lead) leadText = CompassOf(lead.m_fBearing);
  string verdict = "hit";
  if (rightWay) m_iHits++;
  else
  {
   m_iWrongWay++;
   verdict = "wrong way";
  }
  m_aSaid.Insert(string.Format("forecast check %1: threat %2 from %3; forecast ~%4 mainly from %5, %6 s ahead: %7", base.m_sName, threat, CompassOf(bearing), Math.Round(entry.m_fExpected), leadText, Math.Round((now - since) / 1000), verdict));
 }

 // Tests: log (and so note) a base's forecast as a strategic tick would.
 void NoteForTest(ACR_Base base, float now)
 {
  ACR_BaseForecast entry = Of(base);
  if (entry) LogChange(entry, now);
 }

 // A base of ours just lost: remember the way it was taken from (its last threat bearing).
 void NoteLost(ACR_Base base, float now)
 {
  float bearing;
  if (!m_ThreatBearing.Find(base, bearing)) return;
  m_LostBearing.Set(base, bearing);
  m_LostAt.Set(base, now);
  m_NotedSince.Remove(base);
  m_LoggedAt.Remove(base);
  m_Came.Remove(base);
 }

 void Think(notnull array<string> lines)
 {
  lines.Insert(string.Format("Defence forecast (checks: %1 hit, %2 wrong way, %3 not forecast (%4 first seen at the base), %5 lapsed):", m_iHits, m_iWrongWay, m_iUnforecast, m_iAtDoor, m_iLapsed));
  int shown;
  foreach (ACR_BaseForecast entry : m_aBases)
  {
   if (shown >= THINK_BASES || (entry.m_fExpected < 0.5 && entry.m_aWays.IsEmpty())) break;
   lines.Insert("  " + entry.Line());
   shown++;
  }
  if (shown == 0) lines.Insert("  nothing expected");
 }
}
