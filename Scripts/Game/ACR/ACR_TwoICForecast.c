// The defence forecast as a 2IC advisement (Jacob, 4 Nov, defence decision 1; -acrDefenceForecast 2).
// On a side a player commands, with the 2IC on, a firm forecast (ACR_DefenceForecast: enough expected,
// due within lead time; the same picture the AI commander defends on) becomes a "2IC suggests" line:
// what is expected, from where, when and on what intel, and the answer: the squad the 2IC would send
// to hold the base's edge on that side. The way in and that spot show on the map while the line is open.
// Accept (the commander): a 2IC Hold request for that spot, as if they had asked for it themselves (the
// 2IC's own squad choice, its refusals, the Release button). Dismiss: not again for that base and way
// for DISMISSED_MS, unless the way swings by more than CHANGED_ANGLE. The enemy never sees it.
class ACR_TwoICAdvice : Managed
{
 int m_iId;           // its channel line
 string m_sFaction;
 ACR_Base m_Base;
 float m_fBearing;    // degrees from the base toward the way in
 vector m_vHold;      // the spot the answer holds: the base's edge on that side
 vector m_vWay;       // a point out on the way in, for the map
 string m_sSquad;     // the squad the 2IC would send ("" none free)
}

class ACR_TwoICForecast
{
 static const float LOOK_MS = 15000;          // the forecast refreshed this often (a strategic tick)
 static const float DISMISSED_MS = 600000;    // a dismissed base and way: not again for 10 min ...
 static const float CHANGED_ANGLE = 60;       // ... unless the way in swings this far
 static const float WAY_OUT = 300;            // m past the base's radius: the way-in marker
 protected static ref array<ref ACR_TwoICAdvice> s_Live;
 protected static ref map<string, float> s_NextLook;
 protected static ref map<string, float> s_DismissedUntil; // faction|base -> until
 protected static ref map<string, float> s_DismissedWay;   // faction|base -> bearing
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_Live = {};
  s_NextLook = new map<string, float>();
  s_DismissedUntil = new map<string, float>();
  s_DismissedWay = new map<string, float>();
 }

 // Each second for a side the 2IC is on for (ACR_Manager, after its FOB suggestions).
 static void Tick(ACR_Manager manager, string faction, ACR_AICommander ai, bool running)
 {
  if (!running || !ai || !ACR_DefenceForecast.Acts())
  {
   EndAll(faction, "the 2IC isn't advising on the defence now");
   return;
  }
  float now = GetGame().GetWorld().GetWorldTime();
  float next;
  if (s_NextLook.Find(faction, next) && now < next) return;
  s_NextLook.Set(faction, now + LOOK_MS);
  if (!ai.ForecastForTwoIC()) return;
  Review(faction, ai, now);
 }

 // Open lines kept, updated or ended; firm bases without one get one.
 static void Review(string faction, ACR_AICommander ai, float now)
 {
  ACR_DefenceForecast forecast = ai.Forecast();
  if (!forecast) return;
  for (int i = s_Live.Count() - 1; i >= 0; i--)
  {
   ACR_TwoICAdvice live = s_Live[i];
   if (live.m_sFaction != faction) continue;
   if (!live.m_Base || live.m_Base.m_sOwner != faction) End(live, "Over", "the base isn't ours any more");
   else if (forecast.Demand(live.m_Base) <= 0) End(live, "Withdrawn", "no longer expected: the contacts turned away or weren't seen again");
  }
  foreach (ACR_BaseForecast entry : forecast.m_aBases)
  {
   if (forecast.Demand(entry.m_Base) <= 0 || LiveFor(faction, entry.m_Base)) continue;
   ACR_ForecastWay lead = entry.LeadWay();
   if (!lead || Dismissed(faction, entry.m_Base, lead.m_fBearing, now)) continue;
   // That edge is held on a live request already: pn44 (7 Nov) advised the same base again 15 s
   // after each Accept, and three Accepts made three Hold requests there. Another way in still is.
   if (ACR_TwoICRequests.HoldingAt(faction, entry.m_Base, lead.m_fBearing, CHANGED_ANGLE)) continue;
   Advise(faction, ai, entry, lead);
  }
 }

 static ACR_TwoICAdvice Advise(string faction, ACR_AICommander ai, ACR_BaseForecast entry, ACR_ForecastWay lead)
 {
  ACR_Base base = entry.m_Base;
  ACR_TwoICAdvice a = new ACR_TwoICAdvice();
  a.m_sFaction = faction;
  a.m_Base = base;
  a.m_fBearing = lead.m_fBearing;
  vector dir = ACR_DefenceScreen.Toward(lead.m_fBearing);
  a.m_vHold = ACR_Manager.Ground(base.GetOrigin() + dir * base.m_fRadius);
  a.m_vWay = ACR_Manager.Ground(base.GetOrigin() + dir * (base.m_fRadius + WAY_OUT));
  ACR_Squad squad = ai.TwoICDefender(a.m_vHold);
  if (squad) a.m_sSquad = squad.GetName();
  string title = string.Format("2IC suggests: defend %1, an attack is expected from the %2", base.m_sName, ACR_DefenceForecast.CompassOf(lead.m_fBearing));
  a.m_iId = ACR_TwoIC.Say(faction, ACR_ETwoICLine.SUGGESTION, title, "Suggested");
  if (a.m_iId == 0) return null;
  ACR_TwoIC.SetLine(faction, a.m_iId, "Suggested", Why(entry, lead, a), ACR_TwoICSuggest.ACT_ACCEPT | ACR_TwoICRequests.ACT_CANCEL, 0);
  s_Live.Insert(a);
  PrintFormat("[ACR] 2IC %1: advises defending %2 (%3)", faction, base.m_sName, entry.Line());
  return a;
 }

 // "~7 in ~4 min, on 2 contacts (mounted); Squad 3 would hold its NE edge, 900 m away"
 static string Why(ACR_BaseForecast entry, ACR_ForecastWay lead, ACR_TwoICAdvice a)
 {
  string text = string.Format("~%1 in ~%2 min, on %3 contacts", Math.Round(entry.m_fExpected), Math.Max(1, Math.Round(entry.m_fEta / 60)), entry.m_iContacts);
  if (lead.m_bMounted) text += " (mounted)";
  if (entry.m_bMassing) text += ", massing";
  string edge = ACR_DefenceForecast.CompassOf(a.m_fBearing);
  if (a.m_sSquad.IsEmpty()) return text + string.Format("; Accept asks the 2IC to hold its %1 edge (no squad free right now: it keeps trying)", edge);
  return text + string.Format("; %1 would hold its %2 edge", a.m_sSquad, edge);
 }

 // ---------------------------------------------------------------- the buttons
 static bool Owns(int id)
 {
  return Find(id) != null;
 }

 static string Act(SCR_PlayerController pc, int id, int action)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!Replication.IsServer() || !manager || !pc) return "Server unavailable";
  ACR_TwoICAdvice a = Find(id);
  if (!a) return "That suggestion is over";
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction || faction.GetFactionKey() != a.m_sFaction) return "Not your side's suggestion";
  if (!manager.IsCommander(pc.GetPlayerId())) return "Take command to answer the 2IC's suggestions";
  return ActOn(manager, a, action, pc.GetPlayerId(), GetGame().GetPlayerManager().GetPlayerName(pc.GetPlayerId()));
 }

 static string ActOn(ACR_Manager manager, ACR_TwoICAdvice a, int action, int playerId, string playerName)
 {
  if (action == ACR_TwoICSuggest.ACT_ACCEPT)
  {
   // The commander's own order: a Hold request at the edge, answered as any request is.
   string answer = ACR_TwoICRequests.AskFor(manager, a.m_sFaction, playerId, playerName, ACR_ETwoICAsk.HOLD, a.m_vHold);
   End(a, "Accepted", answer);
   return answer;
  }
  string key = a.m_sFaction + "|" + a.m_Base.m_sName;
  s_DismissedUntil.Set(key, GetGame().GetWorld().GetWorldTime() + DISMISSED_MS);
  s_DismissedWay.Set(key, a.m_fBearing);
  End(a, "Dismissed", "not suggested again for 10 min unless it comes from another side");
  return "Dismissed";
 }

 // ---------------------------------------------------------------- the map
 static void AddRings(string faction, notnull array<vector> positions, notnull array<string> labels, notnull array<int> kinds)
 {
  foreach (ACR_TwoICAdvice a : s_Live)
  {
   if (a.m_sFaction != faction) continue;
   string from = ACR_DefenceForecast.CompassOf(a.m_fBearing);
   positions.Insert(a.m_vWay);
   labels.Insert(string.Format("2IC: attack expected on %1 this way (from the %2)", a.m_Base.m_sName, from));
   kinds.Insert(ACR_TwoICSuggest.RING_KIND);
   positions.Insert(a.m_vHold);
   labels.Insert(string.Format("2IC suggests: hold the %1 edge of %2", from, a.m_Base.m_sName));
   kinds.Insert(ACR_TwoICSuggest.RING_KIND);
  }
 }

 static string RingsSig(string faction)
 {
  string sig = "";
  foreach (ACR_TwoICAdvice a : s_Live)
   if (a.m_sFaction == faction) sig += string.Format("|d%1", a.m_iId);
  return sig;
 }

 // ---------------------------------------------------------------- helpers
 protected static void End(ACR_TwoICAdvice a, string status, string answer)
 {
  ACR_TwoIC.SetLine(a.m_sFaction, a.m_iId, status, answer, 0, 0);
  s_Live.RemoveItem(a);
 }

 protected static void EndAll(string faction, string why)
 {
  for (int i = s_Live.Count() - 1; i >= 0; i--)
   if (s_Live[i].m_sFaction == faction) End(s_Live[i], "Withdrawn", why);
 }

 static ACR_TwoICAdvice Find(int id)
 {
  foreach (ACR_TwoICAdvice a : s_Live)
   if (a.m_iId == id) return a;
  return null;
 }

 static ACR_TwoICAdvice LiveFor(string faction, ACR_Base base)
 {
  foreach (ACR_TwoICAdvice a : s_Live)
   if (a.m_sFaction == faction && a.m_Base == base) return a;
  return null;
 }

 protected static bool Dismissed(string faction, ACR_Base base, float bearing, float now)
 {
  string key = faction + "|" + base.m_sName;
  float until;
  if (!s_DismissedUntil.Find(key, until) || now >= until) return false;
  return CHANGED_ANGLE >= ACR_DefenceForecast.AngleBetween(bearing, s_DismissedWay.Get(key));
 }

 // Tests: a fresh look on the next tick, and nothing open.
 static void ResetForTest()
 {
  s_Live.Clear();
  s_NextLook.Clear();
  s_DismissedUntil.Clear();
  s_DismissedWay.Clear();
 }
}
