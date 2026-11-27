// AI commander, counter-insurgency jobs (0.8.0 #244 item 5): what the AI army does with town support,
// using only what a player of the army sees (known tiers, its own intel rings) and the orders and
// purchases a player would use.
//  - Town posts (ManageCoinPosts, before grabs and attacks take the free squads): up to
//    ACR_Coin.TownPosts towns get a squad (TOWN_POST, Travel there and defend), the worst first: the
//    known tier, then the town's value, then nearness to our bases. Unknown towns rank as Quiet; a
//    Loyal town needs nobody; an FIA-held town is an attack target, not a post. A post is released
//    once its town is Loyal, so the squad moves on to the next.
//  - Searching a ring (ManageCoinPosts): one squad (SEARCH) walks to the middle of the tightest FIA
//    intel ring the side has (ACR_FIAIntel leads), in Combat mode; once there and quiet for
//    SEARCH_QUIET_MS it is freed and that ring rests SEARCH_AGAIN_MS. Finding the hideout itself is
//    the existing rule (men near it when it fights): the existing sweep takes over then.
//  - Aid runs (ManageCoin): a free supply truck with a paid load drives to the town that gains most
//    (the highest support among towns we hold or post, Quiet or worse, not on aid cooldown) and hands
//    out aid there with the Unload order's own call (ACR_TownSupport.Aid). Garrison and convoy sends
//    a squad with it (it becomes that town's post). No truck: one is bought as for a FOB.
//  - Reserve: no COIN job takes the reserve squad or the last ACR_Coin.ReserveKeep free squads, and in
//    COIN the reserve is kept whatever the force size or pressure (ACR_AICommander.KeepsReserve): with
//    no enemy army the old rule (5+ squads, recent fighting) almost never held one back.
//  - Fire: CallSupport weighs the town first (ACR_TownSupport.FireRestraint).
//
// Layer after ACR_AICommanderLogistics.c, before ACR_AICommanderTwoIC.c (which calls it): file-name order,
// see ACR_AICommanderBuild.c.
modded class ACR_AICommander
{
 static const float COIN_REACH = 8000;          // m: squads this far from a job may take it
 static const float SEARCH_ARRIVED = 150;
 static const float SEARCH_QUIET_MS = 120000;
 static const float SEARCH_MAX_MS = 900000;
 static const float SEARCH_AGAIN_MS = 600000;
 static const float AID_MAX_MS = 720000;

 protected bool CoinOn()
 {
  return ACR_TownSupport.On() && ACR_Coin.IsArmy(m_sFaction) && (!m_bHandedOnly || m_bTwoICAll);
 }

 // A free squad for a COIN job near 'at', leaving the reserve and ReserveKeep free squads alone.
 protected ACR_Squad CoinSquad(vector at)
 {
  array<ACR_Squad> free = {};
  MySquads(free, true);
  int spare = free.Count() - ACR_Coin.ReserveKeep(m_sFaction);
  if (free.Contains(m_ReserveSquad)) spare--;
  if (spare <= 0) return null;
  return FindSweeper(at, COIN_REACH);
 }

 // ---------------------------------------------------------------- posts and searches

 protected void ManageCoinPosts()
 {
  if (!CoinOn()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  CoinReserve();
  PruneTownPosts();
  int wanted = ACR_Coin.TownPosts(m_sFaction);
  for (int i = 0; i < wanted && m_TownPosts.Count() < wanted; i++)
  {
   ACR_Base town = WorstTown();
   if (!town) break;
   ACR_Squad squad = CoinSquad(town.GetOrigin());
   if (!squad) break;
   PostToTown(squad, town, "to hold");
  }
  ManageSearch(now);
 }

 // The reaction force first: a free line squad nearest the reserve base is held there before the
 // posts, the search and the allocator's grabs share out the rest (the allocator scored grabs above
 // its reserve job and sent every squad grabbing: coin-srvcoinai-j).
 protected void CoinReserve()
 {
  if (m_ReserveSquad && m_Manager.m_Squads.Contains(m_ReserveSquad) && m_ReserveSquad.CountAlive() > 0 && m_ReserveSquad.m_iAITask == ACR_EAITask.POSTED) return;
  array<ACR_Squad> free = {};
  MySquads(free, true);
  if (free.Count() < 2) return;
  ACR_Base home = ReserveBase();
  if (!home) home = HomeBase();
  if (!home) return;
  SortByDistance(free, home.GetOrigin());
  foreach (ACR_Squad squad : free)
  {
   if (IsSupportTeam(squad) || squad.CountAlive() < 2) continue;
   HoldReserve(squad, home);
   return;
  }
 }

 protected void PostToTown(ACR_Squad squad, ACR_Base town, string why)
 {
  m_TownPosts.Set(squad, town);
  squad.SetAITask(ACR_EAITask.TOWN_POST);
  squad.m_AITarget = town;
  Travel(squad, town.GetOrigin(), true, 0);
  int tier = ACR_TownSupport.KnownTier(town, m_sFaction);
  Say(string.Format("[ACR] AI %1: %2 %3 %4 (%5, %6)", m_sFaction, squad.GetName(), why, town.m_sName, ACR_TownSupport.TierName(tier), ACR_Coin.PostureName(ACR_Coin.Posture(m_sFaction))));
 }

 protected void PruneTownPosts()
 {
  array<ACR_Squad> gone = {};
  foreach (ACR_Squad squad, ACR_Base town : m_TownPosts)
  {
   bool keep = squad && m_Manager.m_Squads.Contains(squad) && squad.CountAlive() > 0 && !squad.PlayerSteers() && squad.m_iAITask == ACR_EAITask.TOWN_POST && Ours(squad);
   keep = keep && town && ACR_TownSupport.IsTown(town) && town.m_sOwner != ACR_InsurgentSide.Key();
   // Won over: on to the next town.
   if (keep && town.m_iSupportTier == ACR_TownSupport.LOYAL)
   {
    Say(string.Format("[ACR] AI %1: %2 is Loyal, %3 moves on", m_sFaction, town.m_sName, squad.GetName()));
    keep = false;
   }
   if (keep) continue;
   gone.Insert(squad);
   if (squad && squad.m_iAITask == ACR_EAITask.TOWN_POST) squad.SetAITask(ACR_EAITask.FREE);
  }
  foreach (ACR_Squad left : gone) m_TownPosts.Remove(left);
 }

 // The town that needs a post most (null: none).
 ACR_Base WorstTown()
 {
  ACR_Base best;
  float bestScore = -1000000;
  foreach (ACR_Base town : ACR_Base.s_All)
  {
   if (!ACR_TownSupport.IsTown(town) || town.m_fSupport < 0 || town.m_sOwner == ACR_InsurgentSide.Key() || town.m_bPocket) continue;
   bool posted;
   foreach (ACR_Squad squad, ACR_Base at : m_TownPosts) if (at == town) posted = true;
   if (posted) continue;
   int tier = ACR_TownSupport.KnownTier(town, m_sFaction);
   if (tier < 0) tier = ACR_TownSupport.QUIET;
   if (tier == ACR_TownSupport.LOYAL) continue;
   float score = tier * 100 + town.m_iValue * 10 - NearestOwnDistance(town.GetOrigin()) / 200;
   if (score <= bestScore) continue;
   bestScore = score;
   best = town;
  }
  return best;
 }

 protected void ManageSearch(float now)
 {
  if (m_Searcher)
  {
   bool going = m_Manager.m_Squads.Contains(m_Searcher) && m_Searcher.CountAlive() > 0 && !m_Searcher.PlayerSteers() && m_Searcher.m_iAITask == ACR_EAITask.SEARCH && now - m_fSearchSince < SEARCH_MAX_MS;
   string done = "";
   if (!going) done = "called off";
   else if (vector.DistanceXZ(PositionOf(m_Searcher), m_vSearchAt) > SEARCH_ARRIVED || m_Searcher.KnownEnemies(300) > 0) m_fSearchQuietSince = 0;
   else if (m_fSearchQuietSince <= 0) m_fSearchQuietSince = now;
   else if (now - m_fSearchQuietSince >= SEARCH_QUIET_MS) done = "searched, nothing found";
   if (done.IsEmpty()) return;
   Say(string.Format("[ACR] AI %1: search at %2 by %3 ends (%4)", m_sFaction, ACR_Manager.Grid(m_vSearchAt), m_Searcher.GetName(), done));
   m_SearchedAt.Set(ACR_Manager.Grid(m_vSearchAt), now);
   if (m_Searcher.m_iAITask == ACR_EAITask.SEARCH) m_Searcher.SetAITask(ACR_EAITask.FREE);
   m_Searcher = null;
   return;
  }
  ACR_FIALead lead = SearchLead(now);
  if (!lead) return;
  ACR_Squad squad = CoinSquad(lead.m_vCentre);
  if (!squad) return;
  m_Searcher = squad;
  m_vSearchAt = lead.m_vCentre;
  m_fSearchSince = now;
  m_fSearchQuietSince = 0;
  squad.SetAITask(ACR_EAITask.SEARCH);
  squad.SetMode(ACR_EMoveMode.COMBAT);
  Travel(squad, lead.m_vCentre, false, 0);
  Say(string.Format("[ACR] AI %1: %2 to search the FIA ring at %3 (%4 m, %5 leads)", m_sFaction, squad.GetName(), ACR_Manager.Grid(lead.m_vCentre), (int)lead.m_fRadius, lead.m_iLeads));
 }

 // The side's tightest ring not searched lately (null: none).
 ACR_FIALead SearchLead(float now)
 {
  ACR_FIALead best;
  foreach (ACR_FIALead lead : ACR_FIAIntel.s_aLeads)
  {
   if (lead.m_sSide != m_sFaction) continue;
   float at;
   if (m_SearchedAt.Find(ACR_Manager.Grid(lead.m_vCentre), at) && now - at < SEARCH_AGAIN_MS) continue;
   if (!best || lead.m_fRadius < best.m_fRadius) best = lead;
  }
  return best;
 }

 // ---------------------------------------------------------------- aid runs

 protected void ManageCoin()
 {
  if (!CoinOn()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (m_AidRun)
  {
   TickAidRun(now);
   return;
  }
  ACR_Base town = AidTown();
  if (!town) return;
  ACR_Squad truck = AidTruckSquad(town.GetOrigin());
  if (!truck)
  {
   // No truck with a paid load free: buy one as for a FOB (its own rules: one at a time, RP kept).
   if (!HasConstructionTruck()) BuyConstructionTruck(HomeBase());
   return;
  }
  // Garrison and convoy: a squad goes with it and stays as the town's post.
  if (ACR_Coin.Posture(m_sFaction) == ACR_Coin.CONVOY && !PostedIn(town))
  {
   ACR_Squad escort = CoinSquad(PositionOf(truck));
   if (!escort) return;
   PostToTown(escort, town, "escorts aid to");
  }
  m_AidRun = truck;
  m_AidTown = town;
  m_fAidSince = now;
  truck.SetAITask(ACR_EAITask.AID);
  truck.m_AITarget = town;
  Travel(truck, town.GetOrigin(), false, 0);
  Say(string.Format("[ACR] AI %1: %2 takes aid to %3 (%4, support %5)", m_sFaction, truck.GetName(), town.m_sName, ACR_TownSupport.TierName(ACR_TownSupport.KnownTier(town, m_sFaction)), Math.Round(town.m_fSupport)));
 }

 protected void TickAidRun(float now)
 {
  string done = "";
  bool going = m_Manager.m_Squads.Contains(m_AidRun) && m_AidRun.CountAlive() > 0 && !m_AidRun.PlayerSteers() && m_AidRun.m_iAITask == ACR_EAITask.AID && m_AidTown;
  if (!going) done = "called off";
  else if (now - m_fAidSince > AID_MAX_MS) done = "took too long";
  else if (ACR_TownSupport.TownAt(PositionOf(m_AidRun)) == m_AidTown)
  {
   bool delivered;
   string said = ACR_TownSupport.Aid(m_AidRun, delivered);
   // Still rolling in: try again next turn.
   if (!delivered && said.Contains("stop the truck")) return;
   done = said;
  }
  if (done.IsEmpty()) return;
  Say(string.Format("[ACR] AI %1: aid run by %2 ends: %3", m_sFaction, m_AidRun.GetName(), done));
  if (m_AidRun.m_iAITask == ACR_EAITask.AID) m_AidRun.SetAITask(ACR_EAITask.FREE);
  m_AidRun = null;
  m_AidTown = null;
 }

 protected bool PostedIn(ACR_Base town)
 {
  foreach (ACR_Squad squad, ACR_Base at : m_TownPosts) if (at == town) return true;
  return false;
 }

 // The town an aid run gains most in (null: none): held by us or with our men in it, Quiet or worse as we know it,
 // no FIA owner, not on aid cooldown; the highest support first.
 ACR_Base AidTown()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  ACR_Base best;
  foreach (ACR_Base town : ACR_Base.s_All)
  {
   if (!ACR_TownSupport.IsTown(town) || town.m_fSupport < 0 || town.m_sOwner == ACR_InsurgentSide.Key()) continue;
   // Only where aid would be taken now: held, or our men already there (a post still on its way
   // would see the truck refused: "isn't ours").
   if (town.m_sOwner != m_sFaction && m_Manager.m_Bases.MenWithin(town.GetOrigin(), Math.Max(town.m_fRadius, ACR_TownSupport.RADIUS_MIN), m_sFaction) < ACR_TownSupport.PRESENCE_MIN) continue;
   if (ACR_TownSupport.KnownTier(town, m_sFaction) < ACR_TownSupport.QUIET) continue;
   if (town.m_fAidAt > 0 && now - town.m_fAidAt < ACR_TownSupport.AID_GAP_MS) continue;
   if (!best || town.m_fSupport > best.m_fSupport) best = town;
  }
  return best;
 }

 // Our free supply truck with a paid load for aid, nearest 'at' (null: none).
 protected ACR_Squad AidTruckSquad(vector at)
 {
  ACR_Squad best;
  float bestDistance = float.MAX;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction || !Ours(squad) || squad.PlayerSteers() || !squad.GetAnchor()) continue;
   if (squad.m_iAITask != ACR_EAITask.FREE && squad.m_iAITask != ACR_EAITask.POSTED) continue;
   IEntity truck = ACR_TownSupport.AidTruck(squad);
   if (!truck || m_Manager.m_Logistics.PaidSupplies(truck) < ACR_TownSupport.AID_SUPPLIES) continue;
   float d = vector.DistanceXZ(PositionOf(squad), at);
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = squad;
  }
  return best;
 }

 // Tests.
 int TownPostCountForTest() { return m_TownPosts.Count(); }
 bool PostedInForTest(ACR_Base town) { return PostedIn(town); }
 ACR_Squad SearcherForTest() { return m_Searcher; }
 vector SearchAtForTest() { return m_vSearchAt; }
 ACR_Squad ReserveSquadForTest() { return m_ReserveSquad; }
 int FreeSquadsCountForTest() { array<ACR_Squad> free = {}; MySquads(free, true); return free.Count(); }
}
