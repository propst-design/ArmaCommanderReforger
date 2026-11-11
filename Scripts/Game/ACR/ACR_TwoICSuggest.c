// "2IC suggests" (#14 v1, step 5) and its help with Establish FOB (step 6).
// Suggests: every LOOK_MS the 2IC looks for a front base that needs a FOB (ACR_AICommander.NeedsFob)
// and scores sites behind it as the AI commander does (ACR_FobSiting, a few a second); the best is a
// line in the 2IC channel and a marker on the map, with the reasons. Accept (the commander): the
// side's nearest construction truck with its 500 supplies goes to establish the FOB there, as on a
// player's order (the 2IC buys nothing); Dismiss: that front isn't suggested again for 15 min.
// Establish FOB: with the truck's order armed, the 2IC's best SITES_SHOWN sites behind the front
// nearest it, numbered, with their reasons; and always the "FOB here" card under the cursor: the
// HQ, the upgrade and defence spots that fit there and what blocks the rest (FobHere).
class ACR_TwoICSuggestion : Managed
{
 int m_iId;           // its channel line
 string m_sFaction;
 string m_sFront;
 vector m_vPos;
 string m_sWhy;
}

// A FOB site a suggestion's Send truck sent a truck to: a dashed ring and "FOB site · truck on the
// way · about 4 min" on the map (the declutter sketch 3) until the FOB stands or the truck gives up.
class ACR_TwoICSent : Managed
{
 string m_sFaction;
 vector m_vPos;
 ACR_Squad m_Truck;
}

class ACR_TwoICSiteJob : Managed
{
 int m_iPlayer;
 string m_sFaction;
 ACR_Base m_Front;
 float m_fStarted;
}

class ACR_TwoICSuggest
{
 static const int ACT_ACCEPT = 8;            // a suggestion's buttons: Accept, and Dismiss (ACT_CANCEL)
 static const float LOOK_MS = 30000;
 static const float DISMISSED_MS = 900000;
 static const float BUILT_NEAR = 150;        // m: a FOB built this close takes the suggestion up
 static const int SITES_SHOWN = 3;
 static const float SITE_JOB_MS = 60000;     // an Establish FOB search gives up after this
 static const float FOB_HERE_MS = 250;       // a player's cursor checks, at most this often
 protected static ref array<ref ACR_TwoICSuggestion> s_Live;
 protected static ref map<string, float> s_NextLook;
 protected static ref map<string, float> s_Dismissed;   // faction|front -> until
 protected static ref map<string, string> s_Searching; // faction -> front
 protected static ref array<ref ACR_TwoICSiteJob> s_Jobs;
 protected static ref map<int, float> s_FobHereAt;
 protected static ref array<ref ACR_TwoICSent> s_Sent;
 static const int RING_SENT = 4;             // a sent truck's site (after RING_KIND)
 static const float TRUCK_SPEED = 8;         // m/s, for "about 4 min"
 static const float TRUCK_THERE = 60;        // m: the truck is at the site, building
 static const float TRUCK_BOUGHT_MS = 30000; // a bought truck whose deploy can't be tracked: another waits this long
 static const float TRUCK_DEPLOY_MS = 60000; // a tracked one that never deploys stops blocking after this
 protected static ref map<string, float> s_TruckBoughtAt;
 protected static ref map<string, string> s_TruckBought; // faction -> the bought unit's name
 protected static ref map<int, int> s_SitesSent;        // player -> sites of his last Establish FOB (tests)
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_Live = {};
  s_NextLook = new map<string, float>();
  s_Dismissed = new map<string, float>();
  s_Searching = new map<string, string>();
  s_Jobs = {};
  s_FobHereAt = new map<int, float>();
  s_Sent = {};
  s_TruckBoughtAt = new map<string, float>();
  s_TruckBought = new map<string, string>();
  s_SitesSent = new map<int, int>();
 }

 // ---------------------------------------------------------------- suggesting
 // Each second for a side the 2IC is on for (ACR_Manager, after the requests). running: the 2IC runs
 // the side (its Establish FOB sites, its sent trucks' rings); suggest: it may suggest new FOB
 // sites too, which it doesn't while it runs the whole side and builds its own (ManageFobBuild).
 // pn44 (7 Nov): one flag did both, so with the whole-side 2IC the sites a player's armed Establish
 // FOB asks for were dropped the next second and never came.
 static void Tick(ACR_Manager manager, string faction, ACR_AICommander ai, bool running, bool suggest = true)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = s_Live.Count() - 1; i >= 0; i--)
  {
   ACR_TwoICSuggestion live = s_Live[i];
   if (live.m_sFaction != faction) continue;
   if (!running) End(live, "Withdrawn", "the AI commander has the side now");
   else if (BuiltNear(faction, live.m_vPos)) End(live, "Done", "a FOB stands there now");
   else TruckReady(manager, live);
  }
  TickSent(faction);
  TickJobs(manager, faction, ai, running);
  if (!running || !suggest || !ai || HasLive(faction)) return;
  string front;
  if (!s_Searching.Find(faction, front))
  {
   float next;
   if (s_NextLook.Find(faction, next) && now < next) return;
   s_NextLook.Set(faction, now + LOOK_MS);
   ACR_Base needs = ai.NeedsFob();
   if (!needs || Dismissed(faction, needs.m_sName) || BuildingBehind(faction, needs, ai)) return;
   front = needs.m_sName;
   s_Searching.Set(faction, front);
  }
  ACR_Base frontBase = BaseNamed(front);
  if (!frontBase || frontBase.m_sOwner != faction)
  {
   s_Searching.Remove(faction);
   return;
  }
  array<ref ACR_FobSite> sites = {};
  if (!ai.TwoICFobSites(frontBase, ACR_FobSiting.VALIDATE_PER_STEP, sites)) return;
  s_Searching.Remove(faction);
  for (int t = sites.Count() - 1; t >= 0; t--)
   if (Taken(faction, sites[t].m_vPos)) sites.RemoveOrdered(t);
  array<ACR_FobSite> best = {};
  Top(sites, 1, best);
  if (best.IsEmpty()) return;
  Suggest(faction, frontBase, best[0]);
 }

 static ACR_TwoICSuggestion Suggest(string faction, ACR_Base front, ACR_FobSite site)
 {
  ACR_TwoICSuggestion s = new ACR_TwoICSuggestion();
  s.m_sFaction = faction;
  s.m_sFront = front.m_sName;
  s.m_vPos = site.m_vPos;
  s.m_sWhy = ShortWhy(site);
  s.m_iId = ACR_TwoIC.Say(faction, ACR_ETwoICLine.SUGGESTION, string.Format("2IC suggests: a FOB behind %1", front.m_sName), "Suggested");
  if (s.m_iId == 0) return null;
  ACR_TwoIC.SetLine(faction, s.m_iId, "Suggested", s.m_sWhy, ACT_ACCEPT | ACR_TwoICRequests.ACT_CANCEL, 0);
  s_Live.Insert(s);
  PrintFormat("[ACR] 2IC %1: suggests a FOB behind %2 at %3: %4", faction, front.m_sName, site.m_vPos, ACR_FobSiting.Breakdown(site));
  return s;
 }

 // The line's buttons (server): Accept or Dismiss, the commander only.
 static bool Owns(int id)
 {
  return Find(id) != null;
 }

 static string Act(SCR_PlayerController pc, int id, int action)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!Replication.IsServer() || !manager || !pc) return "Server unavailable";
  ACR_TwoICSuggestion s = Find(id);
  if (!s) return "That suggestion is over";
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction || faction.GetFactionKey() != s.m_sFaction) return "Not your side's suggestion";
  if (!manager.IsCommander(pc.GetPlayerId())) return "Take command to answer the 2IC's suggestions";
  return ActOn(manager, s, action);
 }

 static string ActOn(ACR_Manager manager, ACR_TwoICSuggestion s, int action)
 {
  if (action == ACT_ACCEPT)
  {
   string why;
   ACR_Squad truck = SendTruck(manager, s, why);
   if (!truck)
   {
    ACR_TwoIC.SetLine(s.m_sFaction, s.m_iId, "Suggested", s.m_sWhy + ACR_TwoICCompact.TRUCK_FAIL + why, ACT_ACCEPT | ACR_TwoICRequests.ACT_CANCEL, 0);
    return "Can't send a truck: " + why;
   }
   // The declutter sketch 3: the chip reads "Truck sent", the answer says which truck, and the
   // site keeps a dashed ring with the truck's time to it until the FOB stands.
   End(s, "Truck sent", string.Format("%1 is on its way to build it", truck.GetName()));
   ACR_TwoICSent sent = new ACR_TwoICSent();
   sent.m_sFaction = s.m_sFaction;
   sent.m_vPos = truck.FobSite();
   if (sent.m_vPos == vector.Zero) sent.m_vPos = s.m_vPos;
   sent.m_Truck = truck;
   s_Sent.Insert(sent);
   return truck.GetName() + ": establishing the FOB the 2IC suggested";
  }
  s_Dismissed.Set(s.m_sFaction + "|" + s.m_sFront, GetGame().GetWorld().GetWorldTime() + DISMISSED_MS);
  End(s, "Dismissed", "not suggested again for 15 min");
  return "Dismissed";
 }

 // The side's nearest construction truck with its loads (no player leading it) establishes the FOB.
 static ACR_Squad SendTruck(ACR_Manager manager, ACR_TwoICSuggestion s, out string why)
 {
  why = NO_TRUCK;
  ACR_Squad best;
  float bestDistance = float.MAX;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!TruckFor(squad, s.m_sFaction)) continue;
   IEntity anchor = squad.GetAnchor();
   float d = vector.DistanceXZ(anchor.GetOrigin(), s.m_vPos);
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = squad;
  }
  if (!best) return null;
  vector site;
  string siteWhy;
  if (!ACR_FobBuild.ValidateSite(s.m_vPos, site, siteWhy, s.m_sFaction))
  {
   why = "the site isn't clear any more (" + siteWhy + ")";
   return null;
  }
  if (!best.EstablishFob(site))
  {
   why = best.GetName() + " can't take the order";
   return null;
  }
  if (best.m_bAICommanded) best.SetAITask(ACR_EAITask.BUILD_FOB);
  PrintFormat("[ACR] 2IC %1: %2 establishes the suggested FOB at %3", s.m_sFaction, best.GetName(), site);
  return best;
 }

 // A squad Send truck can take: the side's, no player in it, a construction truck with its loads.
 static bool TruckFor(ACR_Squad squad, string faction)
 {
  if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction || squad.HasPlayer() || !squad.CanEstablishFob()) return false;
  return squad.GetAnchor() != null;
 }

 static bool HasTruck(ACR_Manager manager, string faction)
 {
  if (!manager) return false;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (TruckFor(squad, faction)) return true;
  return false;
 }

 // A suggestion whose Send truck found no truck: once a truck can take it (bought, or back from a
 // job), its line says so and offers Send truck again; the 2IC still sends nothing by itself.
 protected static void TruckReady(ACR_Manager manager, ACR_TwoICSuggestion s)
 {
  ACR_TwoICLine line = ACR_TwoIC.Find(s.m_sFaction, s.m_iId);
  if (!line) return;
  int state = ACR_TwoICCompact.TruckState(line.m_sAnswer);
  if (state != ACR_TwoICCompact.TRUCK_BUY && state != ACR_TwoICCompact.TRUCK_SENDABLE) return;
  bool ready = HasTruck(manager, s.m_sFaction);
  int actions = ACT_ACCEPT | ACR_TwoICRequests.ACT_CANCEL;
  if (ready && state == ACR_TwoICCompact.TRUCK_BUY) ACR_TwoIC.SetLine(s.m_sFaction, s.m_iId, "Suggested", s.m_sWhy + ACR_TwoICCompact.TRUCK_READY + "a construction truck can take it", actions, 0);
  else if (!ready && state == ACR_TwoICCompact.TRUCK_SENDABLE) ACR_TwoIC.SetLine(s.m_sFaction, s.m_iId, "Suggested", s.m_sWhy + ACR_TwoICCompact.TRUCK_FAIL + NO_TRUCK, actions, 0);
 }

 static const string NO_TRUCK = "no construction truck for the FOB site (requisition one with its 500 supplies, then Send truck again)";

 // ---------------------------------------------------------------- Buy a truck
 // The pinned no-truck line's "Buy a truck" is the player's own requisition, never the 2IC's: the
 // Requisition menu's cheapest construction truck, at the side's base nearest the FOB site that can
 // deploy one (a shorter drive; the HQ only without a site). entry, cost, base: what it would buy
 // where; "" when it can be bought now, else why not (the line greys the button with it).
 static string TruckOffer(ACR_Manager manager, string faction, vector near, out int entry, out int cost, out ACR_Base base)
 {
  entry = -1;
  cost = -1;
  base = null;
  if (!manager || !manager.m_Bases) return "server unavailable";
  ACR_FactionData data = manager.m_Bases.GetFaction(faction);
  if (!data || !data.m_aRequisitions) return "no requisition list";
  bool listed = false;
  foreach (int e, ACR_RequisitionEntry offer : data.m_aRequisitions)
  {
   if (offer.m_sVehicle.IsEmpty() || (ACR_Logistics.ServiceOfName(offer.m_sVehicle) & ACR_EService.CONSTRUCTION) == 0) continue;
   listed = true;
   if (offer.m_iRemaining <= 0) continue;
   if (entry < 0 || offer.m_iCost < data.m_aRequisitions[entry].m_iCost) entry = e;
  }
  if (entry < 0)
  {
   if (listed) return "no construction truck left to requisition";
   return "your side has no construction truck to requisition";
  }
  cost = data.m_aRequisitions[entry].m_iCost;
  base = TruckBase(faction, near);
  if (!base) return "no base can deploy one";
  if (TruckOnItsWay(manager, faction)) return "a truck is on its way to deploy";
  if (data.m_iRP < cost) return string.Format("not enough RP (%1 needed, %2 left)", cost, data.m_iRP);
  if (manager.m_Requisition && manager.m_Requisition.OverSoldierCap(faction)) return string.Format("too many soldiers in the field (limit %1)", manager.m_iSoldierCap);
  return "";
 }

 // The side's base nearest the FOB site 'near' that can deploy one now (the menu's checks: its
 // own, deploys new units, a relay where it needs one, not under attack); without a site
 // (vector.Zero) its HQ, else any.
 static ACR_Base TruckBase(string faction, vector near)
 {
  ACR_Base best;
  ACR_Base hq;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != faction || base.m_bContested) continue;
   if (!ACR_LogisticsRules.CanDeploy(base) || ACR_LogisticsRules.RelayBlocked(base)) continue;
   if (!hq && ACR_LogisticsRules.IsHQ(base)) hq = base;
   float d = vector.DistanceXZ(base.GetOrigin(), near);
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = base;
  }
  if (near == vector.Zero && hq) return hq;
  return best;
 }

 // A truck bought with "Buy a truck" that hasn't deployed yet (no squad of its name, which its
 // crew takes, nor TRUCK_DEPLOY_MS gone): another waits. Untracked (no name): TRUCK_BOUGHT_MS.
 static bool TruckOnItsWay(ACR_Manager manager, string faction)
 {
  float at;
  if (!s_TruckBoughtAt.Find(faction, at)) return false;
  float since = GetGame().GetWorld().GetWorldTime() - at;
  string name = s_TruckBought.Get(faction);
  bool waiting;
  if (name.IsEmpty() || !manager) waiting = since < TRUCK_BOUGHT_MS;
  else waiting = since < TRUCK_DEPLOY_MS && !SquadNamed(manager, faction, name);
  if (waiting) return true;
  s_TruckBoughtAt.Remove(faction);
  s_TruckBought.Remove(faction);
  return false;
 }

 static bool SquadNamed(ACR_Manager manager, string faction, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad.GetName() == name && squad.GetFaction() && squad.GetFaction().GetFactionKey() == faction) return true;
  return false;
 }

 // The truck "Buy a truck" bought last for the side, while it hasn't deployed ("" none; tests).
 static string TruckBought(string faction)
 {
  return s_TruckBought.Get(faction);
 }

 // A player's "Buy a truck" (server): the Requisition menu's own purchase (ACR_Manager.Purchase:
 // the commander only, its checks, the RP and the spend log) of the offer's truck at the offer's
 // base. pc null (tests): ACR_Manager.PurchaseAt, the purchase under the menu's.
 static string BuyTruck(ACR_Manager manager, SCR_PlayerController pc, string faction)
 {
  ACR_TwoICSuggestion s = LiveFor(faction);
  vector near = vector.Zero;
  if (s) near = s.m_vPos;
  int entry, cost;
  ACR_Base base;
  string why = TruckOffer(manager, faction, near, entry, cost, base);
  if (!why.IsEmpty()) return "Can't buy a truck: " + why;
  int before = manager.m_Bases.GetRP(faction);
  string bought;
  if (pc) bought = manager.Purchase(pc, entry, base.GetLabel());
  else bought = manager.PurchaseAt(faction, entry, base);
  if (manager.m_Bases.GetRP(faction) < before)
  {
   // Its crew squad takes the purchase's name (ACR_Requisition.PurchaseAt): the guard lifts when
   // that squad is there.
   ACR_RequisitionEntry truck = manager.m_Bases.GetFaction(faction).m_aRequisitions[entry];
   s_TruckBoughtAt.Set(faction, GetGame().GetWorld().GetWorldTime());
   s_TruckBought.Set(faction, string.Format("%1 %2", truck.m_sName, truck.m_iNamed));
  }
  PrintFormat("[ACR] 2IC %1: Buy a truck (%2 RP at %3, player %4): %5", faction, cost, base.m_sName, pc != null, bought);
  return bought;
 }

 // The feed's offer for the side's pinned no-truck line: cost, the base it deploys at, and why not
 // ("" it can be bought); its signature ("-" no such line).
 static string OfferFor(ACR_Manager manager, string faction, out int cost, out string baseName, out string why)
 {
  cost = -1;
  baseName = "";
  why = "";
  ACR_TwoICSuggestion s = LiveFor(faction);
  if (!s) return "-";
  ACR_TwoICLine line = ACR_TwoIC.Find(faction, s.m_iId);
  if (!line || ACR_TwoICCompact.TruckState(line.m_sAnswer) != ACR_TwoICCompact.TRUCK_BUY) return "-";
  int entry;
  ACR_Base base;
  why = TruckOffer(manager, faction, s.m_vPos, entry, cost, base);
  if (base) baseName = base.m_sName;
  return string.Format("%1|%2|%3", cost, baseName, why);
 }

 protected static void End(ACR_TwoICSuggestion s, string status, string answer)
 {
  ACR_TwoIC.SetLine(s.m_sFaction, s.m_iId, status, answer, 0, 0);
  s_Live.RemoveItem(s);
 }

 // The suggestion card's line (the declutter sketch 3): "6/8 upgrades · 7/7 defences · hidden ·
 // 443 m back".
 static string ShortWhy(ACR_FobSite s)
 {
  return ACR_TwoICCompact.Short(s.m_iSlots, s.m_iDefences, s.m_iHidden, s.m_iEyes, (int)s.m_fBack, true);
 }

 // "all 8 upgrade and 7 defence spots clear, hidden from 3 of 4 enemy eyes, a road 20 m off,
 // 480 m behind the front".
 static string Reasons(ACR_FobSite s)
 {
  string text;
  if (s.m_iSlots >= ACR_FobSlots.SLOT_COUNT && s.m_iDefences >= ACR_FobDefences.SPOT_COUNT) text = "every upgrade and defence spot clear";
  else text = string.Format("%1/%2 upgrade and %3/%4 defence spots clear", s.m_iSlots, ACR_FobSlots.SLOT_COUNT, s.m_iDefences, ACR_FobDefences.SPOT_COUNT);
  if (s.m_iEyes > 0) text += string.Format(", hidden from %1 of %2 enemy lookouts", s.m_iHidden, s.m_iEyes);
  else text += ", out of the enemy's sight";
  if (s.m_fRoad >= 0 && s.m_fRoad <= ACR_FobSiting.ROAD_FAR) text += string.Format(", a road %1 m off", (int)s.m_fRoad);
  else text += ", no road close";
  text += string.Format(", %1 m behind the front", (int)s.m_fBack);
  if (s.m_iCover > 0) text += string.Format(", covered by %1 of our positions", s.m_iCover);
  if (s.m_bOnRoute) text += ", but on the enemy's road in";
  if (s.m_bTown) text += ", but in a town";
  return text;
 }

 // ---------------------------------------------------------------- Establish FOB: numbered sites
 // A player armed Establish FOB with this truck squad: the 2IC's best sites behind the front base
 // nearest it, sent to him when scored (ACR_PlayerController.ACR_SendTwoICSites).
 // at: where the unit that establishes it is (a construction truck or the player's command unit,
 // which is no ACA squad: pn12's look test found the sites never came for it).
 static string AskSites(ACR_Manager manager, int playerId, string faction, vector at)
 {
  if (!ACR_TwoIC.Running(manager, faction)) return "";
  ACR_AICommander ai = manager.FobAICommanderFor(faction);
  if (!ai || at == vector.Zero) return "";
  ACR_Base front = ai.NeedsFob();
  if (!front) front = NearestOwn(faction, at);
  if (!front) return "";
  for (int i = s_Jobs.Count() - 1; i >= 0; i--)
   if (s_Jobs[i].m_iPlayer == playerId) s_Jobs.RemoveOrdered(i);
  ACR_TwoICSiteJob job = new ACR_TwoICSiteJob();
  job.m_iPlayer = playerId;
  job.m_sFaction = faction;
  job.m_Front = front;
  job.m_fStarted = GetGame().GetWorld().GetWorldTime();
  s_Jobs.Insert(job);
  return front.m_sName;
 }

 protected static void TickJobs(ACR_Manager manager, string faction, ACR_AICommander ai, bool running)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = s_Jobs.Count() - 1; i >= 0; i--)
  {
   ACR_TwoICSiteJob job = s_Jobs[i];
   if (job.m_sFaction != faction) continue;
   if (!running || !ai || !job.m_Front || now - job.m_fStarted > SITE_JOB_MS)
   {
    s_Jobs.RemoveOrdered(i);
    continue;
   }
   array<ref ACR_FobSite> sites = {};
   if (!ai.TwoICFobSites(job.m_Front, ACR_FobSiting.VALIDATE_PER_STEP, sites)) continue;
   s_Jobs.RemoveOrdered(i);
   array<vector> positions = {};
   array<string> labels = {};
   array<int> facts = {};
   SitesFor(sites, job.m_Front.m_sName, positions, labels, facts);
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(job.m_iPlayer));
   if (pc) pc.ACR_SendTwoICSites(positions, labels, facts);
   s_SitesSent.Set(job.m_iPlayer, positions.Count());
   PrintFormat("[ACR] 2IC %1: %2 sites behind %3 for player %4 (sent %5)", faction, positions.Count(), job.m_Front.m_sName, job.m_iPlayer, pc != null);
  }
 }

 // The best SITES_SHOWN, numbered: "1: every upgrade ... (behind Morton)"; facts: each one's
 // ACR_TwoICCompact.FACTS numbers, for its one-line label and its card (the declutter sketch 2).
 static void SitesFor(array<ref ACR_FobSite> sites, string front, notnull array<vector> positions, notnull array<string> labels, array<int> facts = null)
 {
  array<ACR_FobSite> best = {};
  Top(sites, SITES_SHOWN, best);
  foreach (int n, ACR_FobSite site : best)
  {
   positions.Insert(site.m_vPos);
   labels.Insert(string.Format("%1: %2", n + 1, Reasons(site)));
   if (facts) ACR_TwoICCompact.Facts(site, facts);
  }
 }

 static bool JobFor(int playerId)
 {
  foreach (ACR_TwoICSiteJob job : s_Jobs)
   if (job.m_iPlayer == playerId) return true;
  return false;
 }

 // ---------------------------------------------------------------- FOB here
 // The cursor card while Establish FOB is armed: the HQ, the upgrade and defence spots that fit
 // and what blocks the rest; then whether a FOB can be ordered there. "" while asked too often.
 static string FobHere(int playerId, vector pos)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  float last;
  if (s_FobHereAt.Find(playerId, last) && now - last < FOB_HERE_MS) return "";
  s_FobHereAt.Set(playerId, now);
  return FobHereText(pos);
 }

 static string FobHereText(vector pos)
 {
  pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
  string reason;
  int unusedSlots, unusedDefences;
  bool ok = ACR_FobBuild.CheckSpotCount(pos, reason, unusedSlots, null, unusedDefences);
  string hqWhy;
  bool hq = ACR_FobBuild.FootprintClear(pos, ACR_FobBuild.HQ_RADIUS, null, hqWhy);
  int slots = 0;
  string slotBlocks = "";
  for (int s = 0; s < ACR_FobSlots.SLOT_COUNT; s++)
  {
   string slotWhy = "";
   if (ACR_FobSlots.SlotSpotClear(pos, s, 0, null, slotWhy)) slots++;
   else if (slotBlocks.Length() < 90) slotBlocks += string.Format("; %1: %2", ACR_FobSlots.SlotName(s), slotWhy);
  }
  int defences = 0;
  string defenceBlocks = "";
  for (int d = 0; d < ACR_FobDefences.SPOT_COUNT; d++)
  {
   string defWhy = "";
   if (ACR_FobDefences.DefenceSpotClear(pos, d, defWhy)) defences++;
   else if (defenceBlocks.Length() < 90) defenceBlocks += string.Format("; spot %1: %2", d + 1, defWhy);
  }
  string text = "FOB HERE\n";
  if (hq) text += "HQ fits\n";
  else text += "HQ: " + hqWhy + "\n";
  text += string.Format("Upgrade spots %1/%2%3\n", slots, ACR_FobSlots.SLOT_COUNT, slotBlocks);
  text += string.Format("Defence spots %1/%2%3\n", defences, ACR_FobDefences.SPOT_COUNT, defenceBlocks);
  if (ok) text += "Can be ordered here";
  else text += "Can't be ordered here: " + reason + " (the truck looks up to 50 m round for a clear spot)";
  return text;
 }

 // ---------------------------------------------------------------- the map's markers
 static void AddRings(string faction, notnull array<vector> positions, notnull array<string> labels, notnull array<int> kinds)
 {
  foreach (ACR_TwoICSuggestion s : s_Live)
  {
   if (s.m_sFaction != faction) continue;
   positions.Insert(s.m_vPos);
   labels.Insert(string.Format("2IC suggests: FOB site behind %1", s.m_sFront));
   kinds.Insert(ACR_TwoICSuggest.RING_KIND);
  }
  foreach (ACR_TwoICSent sent : s_Sent)
  {
   if (sent.m_sFaction != faction) continue;
   positions.Insert(sent.m_vPos);
   labels.Insert(SentLabel(SentMinutes(sent)));
   kinds.Insert(RING_SENT);
  }
 }

 // A sent truck's site, as long as the truck is on that build and no FOB stands there yet.
 protected static void TickSent(string faction)
 {
  for (int i = s_Sent.Count() - 1; i >= 0; i--)
  {
   ACR_TwoICSent sent = s_Sent[i];
   if (sent.m_sFaction != faction) continue;
   bool building = sent.m_Truck && sent.m_Truck.OrderState() == ACR_EOrderState.BUILD_FOB;
   if (building) building = vector.DistanceXZ(sent.m_Truck.FobSite(), sent.m_vPos) <= ACR_FobBuild.FOB_SPACING;
   if (!building || BuiltNear(faction, sent.m_vPos)) s_Sent.RemoveOrdered(i);
  }
 }

 // The truck's minutes to the site (at TRUCK_SPEED), 0 once it is there.
 static int SentMinutes(ACR_TwoICSent sent)
 {
  if (!sent || !sent.m_Truck || !sent.m_Truck.GetAnchor()) return 0;
  float d = vector.DistanceXZ(sent.m_Truck.GetAnchor().GetOrigin(), sent.m_vPos);
  if (d <= TRUCK_THERE) return 0;
  return (int)Math.Max(1, Math.Round(d / TRUCK_SPEED / 60));
 }

 // "FOB site · truck on the way · about 4 min"; 0: "FOB site · truck there, building".
 static string SentLabel(int minutes)
 {
  if (minutes <= 0) return "FOB site  ·  truck there, building";
  return string.Format("FOB site  ·  truck on the way  ·  about %1 min", minutes);
 }

 static int SentCount(string faction)
 {
  int count;
  foreach (ACR_TwoICSent sent : s_Sent)
   if (sent.m_sFaction == faction) count++;
  return count;
 }

 // The side's first sent site (tests).
 static bool SentAt(string faction, out vector pos)
 {
  foreach (ACR_TwoICSent sent : s_Sent)
  {
   if (sent.m_sFaction != faction) continue;
   pos = sent.m_vPos;
   return true;
  }
  return false;
 }

 static const int RING_KIND = 3; // after ACR_ETwoICAsk's three

 static string RingsSig(string faction)
 {
  string sig = "";
  foreach (ACR_TwoICSuggestion s : s_Live)
   if (s.m_sFaction == faction) sig += string.Format("|s%1", s.m_iId);
  foreach (ACR_TwoICSent sent : s_Sent)
   if (sent.m_sFaction == faction) sig += string.Format("|t%1", SentMinutes(sent));
  return sig;
 }

 // ---------------------------------------------------------------- helpers
 static ACR_TwoICSuggestion Find(int id)
 {
  foreach (ACR_TwoICSuggestion s : s_Live)
   if (s.m_iId == id) return s;
  return null;
 }

 static bool HasLive(string faction)
 {
  foreach (ACR_TwoICSuggestion s : s_Live)
   if (s.m_sFaction == faction) return true;
  return false;
 }

 static ACR_TwoICSuggestion LiveFor(string faction)
 {
  foreach (ACR_TwoICSuggestion s : s_Live)
   if (s.m_sFaction == faction) return s;
  return null;
 }

 protected static bool Dismissed(string faction, string front)
 {
  float until;
  return s_Dismissed.Find(faction + "|" + front, until) && GetGame().GetWorld().GetWorldTime() < until;
 }

 // A site with a FOB of the side, or with one of its squads on the way to build there (a truck
 // sent, by Accept or anyone), within FOB spacing: not suggested. pn12d (27 Oct): the same site was
 // suggested again 0.3 s after its Accept had sent the truck.
 static bool Taken(string faction, vector pos)
 {
  if (BuiltNear(faction, pos)) return true;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction || squad.OrderState() != ACR_EOrderState.BUILD_FOB) continue;
   if (vector.DistanceXZ(squad.FobSite(), pos) <= ACR_FobBuild.FOB_SPACING) return true;
  }
  return false;
 }

 // One of the side's FOBs is already on its way behind this front (within the doctrine's FOB
 // distance of it): the front isn't suggested another until that one is built or given up.
 static bool BuildingBehind(string faction, ACR_Base front, ACR_AICommander ai)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !front || !ai || !ai.Doctrine()) return false;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction || squad.OrderState() != ACR_EOrderState.BUILD_FOB) continue;
   if (vector.DistanceXZ(squad.FobSite(), front.GetOrigin()) <= ai.Doctrine().m_fFobFront) return true;
  }
  return false;
 }

 protected static bool BuiltNear(string faction, vector pos)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && base.m_bBuilt && !base.m_bRetired && base.m_sOwner == faction && vector.DistanceXZ(base.GetOrigin(), pos) <= BUILT_NEAR) return true;
  return false;
 }

 protected static ACR_Base BaseNamed(string name)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && !base.m_bRetired && base.m_sName == name) return base;
  return null;
 }

 protected static ACR_Base NearestOwn(string faction, vector pos)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_bBuilt || base.m_sOwner != faction) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), pos);
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = base;
  }
  return best;
 }

 // The 'count' best by score (the old order breaks ties), best first.
 static void Top(array<ref ACR_FobSite> sites, int count, notnull array<ACR_FobSite> best)
 {
  best.Clear();
  foreach (ACR_FobSite site : sites)
  {
   int at = 0;
   while (at < best.Count() && (best[at].m_fScore > site.m_fScore || (best[at].m_fScore == site.m_fScore && best[at].m_iOrder < site.m_iOrder))) at++;
   if (at < count) best.InsertAt(site, at);
   if (best.Count() > count) best.RemoveOrdered(count);
  }
 }

 static void LookNowForTest(string faction)
 {
  s_NextLook.Remove(faction);
  s_Searching.Remove(faction);
 }

 static void ResetForTest()
 {
  s_Live.Clear();
  s_NextLook.Clear();
  s_Dismissed.Clear();
  s_Searching.Clear();
  s_Jobs.Clear();
  s_FobHereAt.Clear();
  s_Sent.Clear();
  s_TruckBoughtAt.Clear();
  s_TruckBought.Clear();
  s_SitesSent.Clear();
 }

 // The sites the player's last Establish FOB got (-1: none came yet).
 static int SitesSentForTest(int playerId)
 {
  int count;
  if (s_SitesSent.Find(playerId, count)) return count;
  return -1;
 }
}
