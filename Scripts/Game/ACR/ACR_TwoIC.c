// The second-in-command's map UI (#14 v1, -acr2IC 1, off by default): what a side's players see of
// its always-on 2IC (ACR_SquadAI.Adopt, ACR_AICommander.TickHanded). The server keeps each side's
// 2IC channel (its reports, later the answers to requests and its suggestions; never chat or the
// intel feed) and builds the 2IC card each second: the squads and FOBs it runs, its reserve (the
// build budget) and what it is doing. ACR_MapFeed sends both to the side's players when they
// change; the map shows them only while a player has the 2IC button of the map's tool bar on
// (ACR_TwoICView).
enum ACR_ETwoICLine
{
 REPORT,     // what it did
 REQUEST,    // a player's request and its answer
 SUGGESTION  // "2IC suggests"
}

class ACR_TwoICLine : Managed
{
 int m_iId;
 int m_iKind;
 string m_sText;
 string m_sStatus;   // a request's status (Coming / Can't yet / Holding), else empty
 string m_sAnswer;   // the 2IC's answer to a request
 int m_iActions;     // its buttons (ACR_TwoICRequests.ACT_*)
 int m_iPlayer;      // the player who asked (he and the commander get the buttons)
 float m_fAt;        // world time (ms)
}

// One side's 2IC card. Numbers go out as one int array (Pack), in this order.
class ACR_TwoICCard : Managed
{
 bool m_bRunning;     // the 2IC runs the side's squads (no AI commander has the side)
 int m_iSquads;       // squads it runs
 int m_iSquadsAll;    // squads of the side
 int m_iFobs;         // FOBs it manages
 int m_iFobsAll;      // built FOBs of the side
 int m_iReserve;      // build budget (RP)
 int m_iReserveMax;
 int m_iShare;        // % of income set aside for it, 0 = off
 string m_sDoing;

 void Pack(notnull array<int> numbers)
 {
  numbers.Clear();
  int running;
  if (m_bRunning) running = 1;
  numbers.Insert(running);
  numbers.Insert(m_iSquads);
  numbers.Insert(m_iSquadsAll);
  numbers.Insert(m_iFobs);
  numbers.Insert(m_iFobsAll);
  numbers.Insert(m_iReserve);
  numbers.Insert(m_iReserveMax);
  numbers.Insert(m_iShare);
 }

 string Sig()
 {
  return string.Format("%1|%2/%3|%4/%5|%6/%7|%8|", m_bRunning, m_iSquads, m_iSquadsAll, m_iFobs, m_iFobsAll, m_iReserve, m_iReserveMax, m_iShare) + m_sDoing;
 }
}

// One side's 2IC panel: a row per squad (where, doing, run by) and per built FOB, the share of
// income set aside for its reserve, and whether it is a small force.
class ACR_TwoICPanelData : Managed
{
 ref array<RplId> m_SquadIds = {};
 ref array<string> m_SquadNames = {};
 ref array<string> m_Wheres = {};
 ref array<string> m_Doings = {};
 ref array<string> m_RunBys = {};
 ref array<int> m_SquadFlags = {};   // ACR_TwoIC.ROW_*
 ref array<string> m_FobLabels = {};
 ref array<int> m_FobFlags = {};     // +1: the 2IC manages it
 int m_iShare;
 string m_sSmall;

 string Sig()
 {
  string sig = string.Format("%1|%2|", m_iShare, m_sSmall);
  foreach (int i, string name : m_SquadNames) sig += string.Format("%1;%2;%3;%4;%5|", name, m_Wheres[i], m_Doings[i], m_RunBys[i], m_SquadFlags[i]);
  foreach (int f, string label : m_FobLabels) sig += string.Format("%1;%2|", label, m_FobFlags[f]);
  return sig;
 }
}

class ACR_TwoIC
{
 static const int CHANNEL_LINES = 12; // kept per side
 static const int SMALL_FORCE = 4;    // fewer squads: a small force (grabs only, half holds)
 static const int SHARE_MAX = 30;     // the panel's reserve share, as the setting's (ACR_AICommander.BuildShare)
 static const int ROW_2IC = 1;        // the 2IC runs it
 static const int ROW_LOCKED = 2;     // a player leads it: his
 static const int ROW_CAN_HAND = 4;   // it may be handed to the 2IC
 protected static ref map<string, int> s_Share = new map<string, int>();
 protected static ref array<int> s_PanelOpen = {};
 protected static ref array<int> s_PanelFresh = {};
 protected static ref map<string, ref ACR_TwoICPanelData> s_Panels = new map<string, ref ACR_TwoICPanelData>();
 protected static ref map<string, float> s_PanelAt = new map<string, float>();
 protected static ref map<string, ref array<ref ACR_TwoICLine>> s_Channel = new map<string, ref array<ref ACR_TwoICLine>>();
 protected static ref map<string, int> s_Revision = new map<string, int>();
 protected static int s_iNextId = 1;
 protected static ref map<string, ref ACR_TwoICCard> s_Cards = new map<string, ref ACR_TwoICCard>();
 protected static ref map<string, float> s_CardAt = new map<string, float>();

 static bool On() { return ACR_SquadAI.TwoIC(); }

 // The 2IC runs this side: on, and a player commands it, or it has no AI commander at all (its
 // squads would stand idle). A side the AI commander leads never has one, its start-up hold
 // included: pn10 (26 Oct) had the 2IC take on Red's squads and suggest a FOB in the 45 s before
 // Red's AI commander took command.
 static bool Running(ACR_Manager manager, string faction)
 {
  if (!On() || !manager) return false;
  return ACR_SquadAI.Runs(manager, faction);
 }

 // ---------------------------------------------------------------- the channel
 // A line in the side's 2IC channel; its id (0: the 2IC is off).
 static int Say(string faction, int kind, string text, string status = "")
 {
  if (!On() || faction.IsEmpty() || text.IsEmpty()) return 0;
  array<ref ACR_TwoICLine> lines = Lines(faction);
  ACR_TwoICLine line = new ACR_TwoICLine();
  line.m_iId = s_iNextId;
  s_iNextId++;
  line.m_iKind = kind;
  line.m_sText = text;
  line.m_sStatus = status;
  line.m_fAt = GetGame().GetWorld().GetWorldTime();
  lines.Insert(line);
  while (lines.Count() > CHANNEL_LINES) lines.RemoveOrdered(0);
  Bump(faction);
  PrintFormat("[ACR] 2IC %1 channel: %2 %3", faction, text, status);
  return line.m_iId;
 }

 static void Report(string faction, string text)
 {
  Say(faction, ACR_ETwoICLine.REPORT, text);
 }

 // A line's new status (a request answered, a suggestion taken up); false when it is gone.
 static bool SetStatus(string faction, int id, string status)
 {
  ACR_TwoICLine line = Find(faction, id);
  if (!line) return false;
  if (line.m_sStatus == status) return true;
  line.m_sStatus = status;
  Bump(faction);
  return true;
 }

 // A request's line: its status, the 2IC's answer and the buttons.
 static bool SetLine(string faction, int id, string status, string answer, int actions, int playerId)
 {
  ACR_TwoICLine line = Find(faction, id);
  if (!line) return false;
  if (line.m_sStatus == status && line.m_sAnswer == answer && line.m_iActions == actions && line.m_iPlayer == playerId) return true;
  line.m_sStatus = status;
  line.m_sAnswer = answer;
  line.m_iActions = actions;
  line.m_iPlayer = playerId;
  Bump(faction);
  return true;
 }

 static ACR_TwoICLine Find(string faction, int id)
 {
  array<ref ACR_TwoICLine> lines;
  if (!s_Channel.Find(faction, lines)) return null;
  foreach (ACR_TwoICLine line : lines)
   if (line.m_iId == id) return line;
  return null;
 }

 static int Count(string faction)
 {
  array<ref ACR_TwoICLine> lines;
  if (!s_Channel.Find(faction, lines)) return 0;
  return lines.Count();
 }

 // The channel for the feed: ages in seconds at send time (the client counts on); flags: the
 // buttons + 16 x the player who asked.
 static void Channel(string faction, notnull array<int> ids, notnull array<int> kinds, notnull array<string> texts, notnull array<string> answers, notnull array<string> statuses, notnull array<int> ages, notnull array<int> flags)
 {
  ids.Clear();
  kinds.Clear();
  texts.Clear();
  answers.Clear();
  statuses.Clear();
  ages.Clear();
  flags.Clear();
  array<ref ACR_TwoICLine> lines;
  if (!s_Channel.Find(faction, lines)) return;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_TwoICLine line : lines)
  {
   ids.Insert(line.m_iId);
   kinds.Insert(line.m_iKind);
   texts.Insert(line.m_sText);
   answers.Insert(line.m_sAnswer);
   statuses.Insert(line.m_sStatus);
   ages.Insert((int)((now - line.m_fAt) / 1000));
   flags.Insert(line.m_iActions + 16 * line.m_iPlayer);
  }
 }

 // Changes with every new line or status (not with the ages).
 static string ChannelSig(string faction)
 {
  return string.Format("%1|%2", faction, s_Revision.Get(faction));
 }

 protected static array<ref ACR_TwoICLine> Lines(string faction)
 {
  array<ref ACR_TwoICLine> lines;
  if (!s_Channel.Find(faction, lines))
  {
   lines = new array<ref ACR_TwoICLine>();
   s_Channel.Insert(faction, lines);
  }
  return lines;
 }

 protected static void Bump(string faction)
 {
  s_Revision.Set(faction, s_Revision.Get(faction) + 1);
 }

 // ---------------------------------------------------------------- the card
 // The side's card, built at most once a second.
 static ACR_TwoICCard CardFor(ACR_Manager manager, string faction)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  ACR_TwoICCard card;
  float at;
  if (s_Cards.Find(faction, card) && s_CardAt.Find(faction, at) && now - at < 1000) return card;
  card = Build(manager, faction);
  s_Cards.Set(faction, card);
  s_CardAt.Set(faction, now);
  return card;
 }

 static ACR_TwoICCard Build(ACR_Manager manager, string faction)
 {
  ACR_TwoICCard card = new ACR_TwoICCard();
  if (!manager) return card;
  card.m_bRunning = Running(manager, faction);
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction) continue;
   card.m_iSquadsAll++;
   if (squad.m_bAICommanded) card.m_iSquads++;
  }
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_sOwner != faction || !base.m_bBuilt || base.m_bRetired || !base.m_FobSupply) continue;
   card.m_iFobsAll++;
   if (ACR_FobAI.Handed(base)) card.m_iFobs++;
  }
  ACR_AICommander ai = manager.FobAICommanderFor(faction);
  card.m_iReserveMax = ACR_AICommander.BUILD_BUDGET_MAX;
  if (ai)
  {
   card.m_iReserve = ai.BuildBudget();
   card.m_iShare = (int)ai.EffectiveShare();
   if (card.m_bRunning) card.m_sDoing = ai.TwoICDoing();
  }
  if (!card.m_bRunning) card.m_sDoing = "the AI commander has the side: the 2IC stands by";
  return card;
 }

 // ---------------------------------------------------------------- the panel
 // The reserve share the commander set in the panel for his side; false: the setting's.
 static bool ShareFor(string faction, out int share)
 {
  share = 0;
  if (!On() || !s_Share.Find(faction, share)) return false;
  return true;
 }

 // The panel's slider (server): the commander's side keeps 'share' % of its income for the 2IC.
 static string SetShare(SCR_PlayerController pc, int share)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!Replication.IsServer() || !manager || !pc) return "Server unavailable";
  if (!On()) return "The 2IC is off for this match";
  if (!manager.IsCommander(pc.GetPlayerId())) return "Take command to set the 2IC's reserve";
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction) return "You have no faction";
  return SetShareFor(faction.GetFactionKey(), share, pc.GetPlayerId());
 }

 static string SetShareFor(string faction, int share, int playerId = 0)
 {
  share = Math.ClampInt(share, 0, SHARE_MAX);
  s_Share.Set(faction, share);
  PrintFormat("[ACR] 2IC %1: reserve share %2%% (player %3)", faction, share, playerId);
  if (share == 0) return "2IC reserve: off (it builds nothing)";
  return string.Format("2IC reserve: %1%% of income", share);
 }

 static bool SmallForce(string faction)
 {
  return ACR_SquadAI.CountFor(faction) < SMALL_FORCE;
 }

 static string SmallText(string faction)
 {
  int squads = ACR_SquadAI.CountFor(faction);
  if (squads < SMALL_FORCE) return string.Format("Small force: %1 squads (under %2). It only takes empty bases and holds with at most half of them.", squads, SMALL_FORCE);
  return string.Format("Not a small force: %1 squads (under %2 it only takes empty bases).", squads, SMALL_FORCE);
 }

 // A player opens or closes the panel: rows go to him only while it is open, the first at once.
 static void SetPanelOpen(int playerId, bool open)
 {
  if (open)
  {
   if (!s_PanelOpen.Contains(playerId)) s_PanelOpen.Insert(playerId);
   if (!s_PanelFresh.Contains(playerId)) s_PanelFresh.Insert(playerId);
  }
  else s_PanelOpen.RemoveItem(playerId);
 }

 static bool PanelOpen(int playerId)
 {
  return s_PanelOpen.Contains(playerId);
 }

 // True once after the panel opened (the feed sends it then, changed or not).
 static bool PanelFresh(int playerId)
 {
  if (!s_PanelFresh.Contains(playerId)) return false;
  s_PanelFresh.RemoveItem(playerId);
  return true;
 }

 // The side's panel, built at most once a second.
 static ACR_TwoICPanelData PanelFor(ACR_Manager manager, string faction)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  ACR_TwoICPanelData panel;
  float at;
  if (s_Panels.Find(faction, panel) && s_PanelAt.Find(faction, at) && now - at < 1000) return panel;
  panel = BuildPanel(manager, faction);
  s_Panels.Set(faction, panel);
  s_PanelAt.Set(faction, now);
  return panel;
 }

 static ACR_TwoICPanelData BuildPanel(ACR_Manager manager, string faction)
 {
  ACR_TwoICPanelData panel = new ACR_TwoICPanelData();
  if (!manager) return panel;
  ACR_AICommander ai = manager.FobAICommanderFor(faction);
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.m_Group || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction) continue;
   panel.m_SquadIds.Insert(SCR_EntityHelper.EntityToRplId(squad.m_Group));
   panel.m_SquadNames.Insert(squad.GetName());
   panel.m_Wheres.Insert(Where(squad));
   int flags = 0;
   string why = "";
   if (ACR_SquadAI.CanHand(squad, why)) flags |= ROW_CAN_HAND;
   if (squad.m_bAICommanded)
   {
    flags |= ROW_2IC;
    string doing = "";
    if (ai) doing = ai.TaskText(squad);
    panel.m_Doings.Insert(doing);
    panel.m_RunBys.Insert("2IC");
   }
   else
   {
    panel.m_Doings.Insert(squad.m_Order);
    int leader = ACR_Squad.FindController(squad.GetLeader());
    if (squad.HasPlayer()) flags |= ROW_LOCKED;
    if (leader > 0) panel.m_RunBys.Insert(GetGame().GetPlayerManager().GetPlayerName(leader));
    else if (!why.IsEmpty()) panel.m_RunBys.Insert("commander (" + why + ")");
    else panel.m_RunBys.Insert("commander");
   }
   panel.m_SquadFlags.Insert(flags);
  }
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_sOwner != faction || !base.m_bBuilt || base.m_bRetired || !base.m_FobSupply) continue;
   panel.m_FobLabels.Insert(base.GetLabel());
   int fobFlags = 0;
   if (ACR_FobAI.Handed(base)) fobFlags = 1;
   panel.m_FobFlags.Insert(fobFlags);
  }
  if (ai) panel.m_iShare = ai.SideShare();
  panel.m_sSmall = SmallText(faction);
  return panel;
 }

 // "at Morton", "300 m from Morton" (to the nearest 100 m), "grid 123-456".
 static string Where(ACR_Squad squad)
 {
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return "unknown";
  vector pos = anchor.GetOrigin();
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
  if (!best) return string.Format("grid %1-%2", (int)(pos[0] / 10), (int)(pos[2] / 10));
  if (bestDistance <= best.m_fRadius) return "at " + best.m_sName;
  int rounded = (int)Math.Max(100, Math.Round(bestDistance / 100) * 100);
  return string.Format("%1 m from %2", rounded, best.m_sName);
 }

 static void ResetForTest() { Reset(); }

 // Everything per match (ACR_Manager.Stop at the end of a mission).
 static void Reset()
 {
  ACR_TwoICRequests.ResetForTest();
  ACR_TwoICSuggest.ResetForTest();
  s_Share.Clear();
  s_PanelOpen.Clear();
  s_PanelFresh.Clear();
  s_Panels.Clear();
  s_PanelAt.Clear();
  s_Channel.Clear();
  s_Revision.Clear();
  s_Cards.Clear();
  s_CardAt.Clear();
 }
}
