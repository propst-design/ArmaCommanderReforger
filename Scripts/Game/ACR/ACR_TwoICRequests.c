// "Ask the 2IC" (#14 v1, step 4): a player's request from the map's picker (Alt+right-click):
// Support me here, Hold this, Take this base. The 2IC answers each second, ahead of its own plans
// (ACR_AICommander.AnswerTwoIC), and the answer lands in the 2IC channel with a status: Coming
// (who, from where), Can't yet (why; the player keeps it queued or cancels it), Holding (until he
// releases it), Supporting (10 min), then Done. Live requests get a ring on the map. The player
// who asked and the commander may cancel or release a request.
enum ACR_ETwoICAsk
{
 SUPPORT,
 HOLD,
 TAKE
}

enum ACR_ETwoICState
{
 WAITING,     // asked, not answered yet
 COMING,      // a squad is on its way (or the attack is)
 CANT,        // can't yet: why in the answer
 HOLDING,     // a squad holds the spot until released
 SUPPORTING,  // a squad supports there until SUPPORT_MS runs out
 DONE,
 CANCELLED
}

class ACR_TwoICRequest : Managed
{
 int m_iId;            // its channel line's id
 int m_iKind;
 string m_sFaction;
 int m_iPlayer;
 string m_sPlayer;
 vector m_vPos;
 string m_sPlace;      // "Military Hill", "grid 123-456"
 ACR_Base m_Base;      // TAKE
 ACR_Squad m_Squad;    // the squad on it (support, hold, a grab)
 string m_sSquad;
 int m_iState = ACR_ETwoICState.WAITING;
 string m_sAnswer;
 bool m_bQueued;       // can't yet, kept queued: tried again every RETRY_MS
 float m_fNextTry;
 float m_fUntil;       // supporting: runs out; can't yet, not queued: dropped
 float m_fAskedAt;

 void Set(int state, string answer)
 {
  m_iState = state;
  m_sAnswer = answer;
 }

 bool Live()
 {
  return m_iState != ACR_ETwoICState.DONE && m_iState != ACR_ETwoICState.CANCELLED;
 }
}

class ACR_TwoICRequests
{
 static const int ACT_KEEP = 1;       // the line's buttons
 static const int ACT_CANCEL = 2;
 static const int ACT_RELEASE = 4;
 static const int MAX_PER_PLAYER = 3; // live requests at once
 static const float TAKE_RADIUS = 150;  // m round a base's circle: "Take this base" means that base
 static const float RETRY_MS = 20000;
 static const float DROP_MS = 120000;   // a "can't yet" nobody keeps queued is dropped
 protected static ref array<ref ACR_TwoICRequest> s_Live = {};
 protected static ref map<int, float> s_mLastAsk = new map<int, float>();

 // ---------------------------------------------------------------- asking
 // A player's request (server).
 static string Ask(SCR_PlayerController pc, int kind, vector pos)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!Replication.IsServer() || !manager || !pc) return "Server unavailable";
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction) return "You have no faction";
  // What a client sends is checked here (audit 31 Oct): a known kind, a point on the map, and
  // one ask every 2 s per player (asking and cancelling in a loop flooded the side's channel).
  if (kind < ACR_ETwoICAsk.SUPPORT || kind > ACR_ETwoICAsk.TAKE) return "Unknown request";
  if (!ACR_Manager.OnMap(pos)) return "That point is off the map";
  float now = GetGame().GetWorld().GetWorldTime();
  float last;
  if (s_mLastAsk.Find(pc.GetPlayerId(), last) && now - last < 2000) return "One request at a time: wait a moment";
  s_mLastAsk.Set(pc.GetPlayerId(), now);
  return AskFor(manager, faction.GetFactionKey(), pc.GetPlayerId(), GetGame().GetPlayerManager().GetPlayerName(pc.GetPlayerId()), kind, pos);
 }

 static string AskFor(ACR_Manager manager, string faction, int playerId, string playerName, int kind, vector pos)
 {
  if (!ACR_TwoIC.Running(manager, faction)) return "The 2IC isn't running your side (the AI commander has it)";
  int mine;
  foreach (ACR_TwoICRequest live : s_Live)
   if (live.m_iPlayer == playerId && live.m_sFaction == faction) mine++;
  if (mine >= MAX_PER_PLAYER) return string.Format("You have %1 requests with the 2IC already: cancel one in the 2IC channel", mine);
  ACR_TwoICRequest r = new ACR_TwoICRequest();
  r.m_iKind = kind;
  r.m_sFaction = faction;
  r.m_iPlayer = playerId;
  r.m_sPlayer = playerName;
  r.m_vPos = pos;
  r.m_sPlace = Place(pos);
  r.m_fAskedAt = GetGame().GetWorld().GetWorldTime();
  string text;
  if (kind == ACR_ETwoICAsk.TAKE)
  {
   r.m_Base = BaseAt(pos);
   if (!r.m_Base) return "There's no base there to take";
   if (r.m_Base.m_sOwner == faction) return r.m_Base.m_sName + " is ours already";
   r.m_vPos = r.m_Base.GetOrigin();
   r.m_sPlace = r.m_Base.m_sName;
   text = string.Format("%1: take %2", playerName, r.m_sPlace);
  }
  else if (kind == ACR_ETwoICAsk.HOLD) text = string.Format("%1: hold %2", playerName, r.m_sPlace);
  else text = string.Format("%1: support me at %2", playerName, r.m_sPlace);
  r.m_iId = ACR_TwoIC.Say(faction, ACR_ETwoICLine.REQUEST, text, "Asked");
  if (r.m_iId == 0) return "The 2IC is off for this match";
  ACR_TwoIC.SetLine(faction, r.m_iId, "Asked", "", ACT_CANCEL, playerId);
  s_Live.Insert(r);
  PrintFormat("[ACR] 2IC %1: request %2 (%3) from player %4 at %5", faction, r.m_iId, typename.EnumToString(ACR_ETwoICAsk, kind), playerId, r.m_sPlace);
  return "Asked the 2IC: its answer comes in the 2IC channel";
 }

 // The line's buttons (server): Keep queued, Cancel, Release. The player who asked or the commander.
 static string Act(SCR_PlayerController pc, int id, int action)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!Replication.IsServer() || !manager || !pc) return "Server unavailable";
  if (ACR_TwoICSuggest.Owns(id)) return ACR_TwoICSuggest.Act(pc, id, action);
  ACR_TwoICRequest r = Find(id);
  if (!r) return "That request is over";
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction || faction.GetFactionKey() != r.m_sFaction) return "Not your side's request";
  if (r.m_iPlayer != pc.GetPlayerId() && !manager.IsCommander(pc.GetPlayerId())) return "Only the player who asked or the commander can change it";
  return ActOn(manager, r, action);
 }

 static string ActOn(ACR_Manager manager, ACR_TwoICRequest r, int action)
 {
  ACR_AICommander ai = manager.FobAICommanderFor(r.m_sFaction);
  if (action == ACT_KEEP)
  {
   if (r.m_iState != ACR_ETwoICState.CANT) return "Only a request it can't do yet is kept queued";
   r.m_bQueued = true;
   r.m_fNextTry = GetGame().GetWorld().GetWorldTime() + RETRY_MS;
   Show(r);
   return "Kept queued: the 2IC tries again every 20 s";
  }
  string released = ReleasedText(r);
  if (ai) ai.EndTwoICRequest(r);
  if (action == ACT_RELEASE) r.Set(ACR_ETwoICState.DONE, released);
  else r.Set(ACR_ETwoICState.CANCELLED, "cancelled");
  Show(r);
  s_Live.RemoveItem(r);
  PrintFormat("[ACR] 2IC %1: request %2 %3", r.m_sFaction, r.m_iId, typename.EnumToString(ACR_ETwoICState, r.m_iState));
  if (action == ACT_RELEASE) return "Released";
  return "Cancelled";
 }

 protected static string ReleasedText(ACR_TwoICRequest r)
 {
  if (r.m_sSquad.IsEmpty()) return "released";
  return string.Format("released: %1 is free again", r.m_sSquad);
 }

 // ---------------------------------------------------------------- answering
 // Each second for a side (ACR_Manager): running, the 2IC answers its live requests ahead of its
 // own plans; not running (the AI commander took the side), they end.
 static void Tick(string faction, ACR_AICommander ai, bool running)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = s_Live.Count() - 1; i >= 0; i--)
  {
   ACR_TwoICRequest r = s_Live[i];
   if (r.m_sFaction != faction) continue;
   if (!running || !ai)
   {
    if (ai) ai.EndTwoICRequest(r);
    r.Set(ACR_ETwoICState.CANCELLED, "the AI commander has the side now");
   }
   else
   {
    ai.AnswerTwoIC(r);
    if (r.m_iState == ACR_ETwoICState.CANT && !r.m_bQueued && r.m_fUntil > 0 && now >= r.m_fUntil)
    {
     ai.EndTwoICRequest(r);
     r.Set(ACR_ETwoICState.DONE, "dropped (not kept queued): " + r.m_sAnswer);
    }
   }
   Show(r);
   if (!r.Live()) s_Live.RemoveOrdered(i);
  }
 }

 // Its channel line: status, answer and buttons.
 static void Show(ACR_TwoICRequest r)
 {
  int actions;
  string status = StatusText(r);
  int state = r.m_iState;
  if (state == ACR_ETwoICState.WAITING || state == ACR_ETwoICState.COMING) actions = ACT_CANCEL;
  else if (state == ACR_ETwoICState.CANT)
  {
   actions = ACT_CANCEL;
   if (!r.m_bQueued) actions |= ACT_KEEP;
  }
  else if (state == ACR_ETwoICState.HOLDING || state == ACR_ETwoICState.SUPPORTING) actions = ACT_RELEASE;
  string answer = "";
  if (!r.m_sAnswer.IsEmpty()) answer = "2IC: " + r.m_sAnswer;
  ACR_TwoIC.SetLine(r.m_sFaction, r.m_iId, status, answer, actions, r.m_iPlayer);
 }

 static string StatusText(ACR_TwoICRequest r)
 {
  int state = r.m_iState;
  if (state == ACR_ETwoICState.COMING)
  {
   int eta = ArrivalMinutes(r);
   if (eta > 0) return string.Format("Coming, %1 min", eta);
   return "Coming";
  }
  if (state == ACR_ETwoICState.CANT)
  {
   if (r.m_bQueued) return "Queued";
   return "Can't yet";
  }
  if (state == ACR_ETwoICState.HOLDING) return "Holding";
  if (state == ACR_ETwoICState.SUPPORTING) return "Supporting";
  if (state == ACR_ETwoICState.DONE) return "Done";
  if (state == ACR_ETwoICState.CANCELLED) return "Cancelled";
  return "Asked";
 }

 // About how many minutes the squad on it needs to get there (on foot 1.4 m/s, mounted 8).
 static int ArrivalMinutes(ACR_TwoICRequest r)
 {
  if (!r.m_Squad || !r.m_Squad.GetAnchor()) return 0;
  vector at = r.m_Squad.GetAnchor().GetOrigin();
  float speed = 1.4;
  if (r.m_Squad.IsMounted()) speed = 8;
  return (int)Math.Max(1, Math.Round(vector.DistanceXZ(at, r.m_vPos) / speed / 60));
 }

 // ---------------------------------------------------------------- the map's rings
 // Live requests of a side: where, and a short label ("Hold: Alpha 1, until you release it").
 static void Rings(string faction, notnull array<vector> positions, notnull array<string> labels, notnull array<int> kinds)
 {
  positions.Clear();
  labels.Clear();
  kinds.Clear();
  foreach (ACR_TwoICRequest r : s_Live)
  {
   if (r.m_sFaction != faction || !r.Live()) continue;
   positions.Insert(r.m_vPos);
   labels.Insert(RingLabel(r));
   kinds.Insert(r.m_iKind);
  }
  ACR_TwoICSuggest.AddRings(faction, positions, labels, kinds);
 }

 static string RingsSig(string faction)
 {
  string sig = faction;
  foreach (ACR_TwoICRequest r : s_Live)
   if (r.m_sFaction == faction) sig += string.Format("|%1;%2;%3", r.m_iId, r.m_iState, RingLabel(r));
  return sig + ACR_TwoICSuggest.RingsSig(faction);
 }

 static string RingLabel(ACR_TwoICRequest r)
 {
  string kind = "Support";
  if (r.m_iKind == ACR_ETwoICAsk.HOLD) kind = "Hold";
  else if (r.m_iKind == ACR_ETwoICAsk.TAKE) kind = "Take";
  string who = r.m_sSquad;
  if (who.IsEmpty()) who = r.m_sPlace;
  int state = r.m_iState;
  if (state == ACR_ETwoICState.HOLDING) return string.Format("%1: %2, until you release it", kind, who);
  if (state == ACR_ETwoICState.SUPPORTING) return string.Format("%1: %2", kind, who);
  if (state == ACR_ETwoICState.COMING)
  {
   int eta = ArrivalMinutes(r);
   if (eta > 0) return string.Format("%1: %2, %3 min", kind, who, eta);
   return string.Format("%1: %2, coming", kind, who);
  }
  if (state == ACR_ETwoICState.CANT) return string.Format("%1: %2 (%3)", kind, r.m_sPlace, StatusText(r));
  return string.Format("%1: %2 (asked)", kind, r.m_sPlace);
 }

 // ---------------------------------------------------------------- helpers
 static ACR_TwoICRequest Find(int id)
 {
  foreach (ACR_TwoICRequest r : s_Live)
   if (r.m_iId == id) return r;
  return null;
 }

 // The newest live request (tests).
 static ACR_TwoICRequest Last()
 {
  if (s_Live.IsEmpty()) return null;
  return s_Live[s_Live.Count() - 1];
 }

 static int LiveCount(string faction)
 {
  int count;
  foreach (ACR_TwoICRequest r : s_Live)
   if (r.m_sFaction == faction) count++;
  return count;
 }

 // The base a "Take this base" means: the nearest whose circle, and TAKE_RADIUS round it, holds pos.
 static ACR_Base BaseAt(vector pos)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), pos);
   if (d > base.m_fRadius + TAKE_RADIUS || d >= bestDistance) continue;
   bestDistance = d;
   best = base;
  }
  return best;
 }

 // "Military Hill" (within a base's circle), "near Military Hill" (within 500 m), else the grid.
 static string Place(vector pos)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), pos);
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = base;
  }
  if (best && bestDistance <= best.m_fRadius) return best.m_sName;
  if (best && bestDistance <= 500) return "near " + best.m_sName;
  return string.Format("grid %1-%2", (int)(pos[0] / 100), (int)(pos[2] / 100));
 }

 static void ResetForTest()
 {
  s_Live.Clear();
  s_mLastAsk.Clear();
 }
}
