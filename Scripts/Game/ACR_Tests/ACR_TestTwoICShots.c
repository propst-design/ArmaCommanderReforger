// -acrTwoICShotsTest 1 (a game client, not srv; ~1.5 min): the 2IC's screens as the player sees them,
// captured from outside at each "TWOICSHOTS ready" line (scratchpad shot-capture.ps1) for a look against the #14 sketch ("2IC sketch -
// 1 Map view" to "4 FOB sites"): the map with the card, the channel and the [2IC] tags; the panel;
// the one Alt+right-click menu (ACA's pings, then the asks) with the channel on Requests; Establish FOB's numbered sites and the
// FOB here card; then the suggestion's Show (the map pans to it). The 2IC on, no AI commander, three more US squads, Bravo taken back by the player,
// a request of each kind and a FOB site suggestion.
// The declutter sketch boards (1 Map view, 2 Placing a FOB, 3 Suggestion before and after, 4 Zoomed
// out, 2IC hidden): 2ic_1_map (the three-line card, a pinned "Needs you" line, merged reports, the
// orange corners), 2ic_1b_full_card (the card on hover), 2ic_4_fob (only the banner and the
// one-line sites), 2ic_4b_site_card (a site's card), 2ic_6_suggest_before, 2ic_7_suggest_after
// (Truck sent, the dashed ring), 2ic_8_zoomed_out (the pill), 2ic_9_hidden_badge (the count).
// VERDICT: the 15 checks beside the shots (the shots themselves are for a look, not graded).
class ACR_TestTwoICShots
{
 static const int CHECKS = 15;
 static const ResourceName RIFLE_SQUAD = "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 protected static bool s_bSuggested;
 protected static bool s_bAsked;
 protected static int s_iFobShot;
 protected static string s_sTruck = "";
 protected static int s_iPings = -1;
 protected static int s_iSentId;
 protected static RplId s_ShowId;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_ShowId = RplId.Invalid();
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (tick == 2)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Options.Set("acr2IC", "1");
   ACR_Options.Set("acrAIBuildShare", "15");
   vector rally = h.m_Range.m_Rally;
   SCR_AIGroup first = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(RIFLE_SQUAD, ACR_Manager.Ground(rally + Vector(-60, 0, -80))));
   if (first) first.SetCustomName("1st Squad", 0);
   SCR_AIGroup second = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(RIFLE_SQUAD, ACR_Manager.Ground(rally + Vector(90, 0, -140))));
   if (second) second.SetCustomName("2nd Squad", 0);
   SCR_AIGroup third = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(FIRE_TEAM, ACR_Manager.Ground(rally + Vector(-120, 0, 60))));
   if (third) third.SetCustomName("Light Fire Team 1", 0);
   return;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (tick > 150 && s_iFobShot == 0) Finish("timed out");
  if (!pc || !alpha || !bravo || tick < 12) return;
  ACR_AICommander ai = manager.FobAICommanderFor("US");
  // Bravo is the player's (taken back), the rest the 2IC's.
  if (tick == 12)
  {
   ACR_SquadAI.Release(bravo, false);
   bravo.m_bClaimed = true;
   ai.m_fBuildBudget = 48;
  }
  // Requests: hold our nearest base, support the player at Alpha, take the nearest enemy base.
  if (tick == 14 && !s_bAsked)
  {
   s_bAsked = true;
   int me = pc.GetPlayerId();
   string name = GetGame().GetPlayerManager().GetPlayerName(me);
   ACR_Base own = Nearest(alpha.GetAnchor().GetOrigin(), true);
   ACR_Base enemy = Nearest(alpha.GetAnchor().GetOrigin(), false);
   if (own) PrintFormat("[ACR] TWOICSHOTS hold: %1", ACR_TwoICRequests.AskFor(manager, "US", me, name, ACR_ETwoICAsk.HOLD, own.GetOrigin()));
   PrintFormat("[ACR] TWOICSHOTS support: %1", ACR_TwoICRequests.AskFor(manager, "US", me, name, ACR_ETwoICAsk.SUPPORT, alpha.GetAnchor().GetOrigin()));
   if (enemy) PrintFormat("[ACR] TWOICSHOTS take: %1", ACR_TwoICRequests.AskFor(manager, "US", me, name, ACR_ETwoICAsk.TAKE, enemy.GetOrigin()));
  }
  // A construction truck for Establish FOB, bought as the AI commander buys one, kept as the player's;
  // after board 1, whose pinned line is the sketch's: no truck yet, "Buy a truck".
  if (tick == 32) BuyTruck(manager, Nearest(alpha.GetAnchor().GetOrigin(), true));
  if (tick >= 33 && tick < 47 && s_sTruck.IsEmpty())
  {
   foreach (ACR_Squad any : manager.m_Squads)
   {
    if (!any || !any.GetFaction() || any.GetFaction().GetFactionKey() != "US" || !any.ConstructionTruck()) continue;
    ACR_SquadAI.Release(any, true);
    any.m_bClaimed = true;
    s_sTruck = any.GetName();
    PrintFormat("[ACR] TWOICSHOTS truck: %1 (can establish %2)", s_sTruck, any.CanEstablishFob());
   }
  }
  // A FOB site suggestion behind our nearest base, once its sites are scored.
  if (tick >= 14 && tick < 40 && !s_bSuggested && ai)
  {
   ACR_Base front = Nearest(alpha.GetAnchor().GetOrigin(), true);
   array<ref ACR_FobSite> sites = {};
   if (front && ai.TwoICFobSites(front, -1, sites))
   {
    array<ACR_FobSite> best = {};
    ACR_TwoICSuggest.Top(sites, ACR_TwoICSuggest.SITES_SHOWN, best);
    if (!best.IsEmpty()) ACR_TwoICSuggest.Suggest("US", front, best[0]);
    s_bSuggested = true;
    PrintFormat("[ACR] TWOICSHOTS suggestion behind %1: %2 sites", front.m_sName, sites.Count());
   }
  }
  // Each screen holds 4 s after its "ready" line (the shot is the last frame drawn).
  if (tick == 20) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (tick == 23)
  {
   // The 2IC button, clicked as the player would; without it (no tool bar) straight on.
   SCR_MapToolEntry entry = SCR_MapToolMenuUI.s_ACR_TwoICEntry;
   if (entry && !pc.ACR_TwoICShown()) entry.m_OnClick.Invoke();
   if (!pc.ACR_TwoICShown()) pc.ACR_TestTwoICShown(true);
  }
  if (tick == 27)
  {
   SCR_MapToolEntry button = SCR_MapToolMenuUI.s_ACR_TwoICEntry;
   bool hidden = button && button.m_ButtonComp && button.m_ButtonComp.m_wImage && button.m_ButtonComp.m_wImage.GetOpacity() == 0;
   bool lit = button && button.IsEntryActive();
   int drawn = ACR_OrderDisplay.s_iTwoICIconDrawn;
   ACR_TestRegress.Check("twoicshots: the 2IC button shows the ACA icon in colour (its own glyph hidden), lit while the 2IC is shown", hidden && lit && drawn > 0, string.Format("button %1, glyph hidden %2, lit %3, icon drawn in %4 frames", button != null, hidden, lit, drawn));
  }
  // Board 1: two reports in a row (they merge), and a line that needs the player (the suggestion
  // with no truck, else a request it can't do yet).
  if (tick == 24) ACR_TwoIC.Report("US", "Took on 2nd Squad");
  if (tick == 26)
  {
   ACR_TwoIC.Report("US", "Took on Light Fire Team 1");
   if (!FailSuggestion(true)) CantYet(pc, "Lamentin");
  }
  if (tick == 28)
  {
   Shot("2ic_1_map");
   ACR_TestRegress.Check("twoicshots: the card is three lines; the channel pins what needs the player on top, each open request one line, at most 6 rows", pc.ACR_TestTwoICForm() == ACR_TwoICView.FORM_COMPACT && pc.ACR_TestPinned() >= 1 && pc.ACR_TestRows() <= ACR_TwoICCompact.ROW_CAP && pc.ACR_TestOneLine() >= 1, string.Format("form %1, pinned %2, one-line %3 of %4 rows, +%5 more; zoom %6", pc.ACR_TestTwoICForm(), pc.ACR_TestPinned(), pc.ACR_TestOneLine(), pc.ACR_TestRows(), pc.ACR_TestMore(), pc.ACR_TestZoom()));
   bool buyEnabled;
   string buyLabel;
   bool buy = pc.ACR_TestBuyShown(buyEnabled, buyLabel);
   ACR_TestRegress.Check("twoicshots: with no truck, the pinned line offers \"Buy a truck at <base> (N RP)\"", buy && buyLabel.StartsWith("Buy a truck at ") && buyEnabled, string.Format("shown %1, enabled %2, '%3' (the suggestion was there: %4)", buy, buyEnabled, buyLabel, ACR_TwoICSuggest.LiveFor("US") != null));
   ACR_TestRegress.Check("twoicshots: the card's root is ACR_TwoICCard, top left", pc.ACR_TestCardNamed(), "");
  }
  if (tick == 29)
  {
   FailSuggestion(false);
   pc.ACR_TestTwoICHover(true);
  }
  if (tick == 31)
  {
   Shot("2ic_1b_full_card");
   ACR_TestRegress.Check("twoicshots: on hover the full card opens", pc.ACR_TestTwoICForm() == ACR_TwoICView.FORM_FULL, string.Format("form %1", pc.ACR_TestTwoICForm()));
  }
  if (tick == 32) pc.ACR_TestTwoICHover(false);
  if (tick == 32) pc.ACR_TwoICTogglePanel();
  if (tick == 35) Shot("2ic_2_panel");
  // Show on 2nd Squad's row (playtest 30 Oct): the panel closes, the squad is selected, the map on it.
  if (tick == 36)
  {
   ACR_Squad shown = h.NamedSquad(manager, "2nd Squad");
   if (shown) s_ShowId = SCR_EntityHelper.EntityToRplId(shown.m_Group);
   if (shown) pc.ACR_TwoICShowSquad(s_ShowId);
  }
  if (tick == 38)
  {
   SCR_MapEntity showMap = SCR_MapEntity.GetMapInstance();
   ACR_Squad shownSquad = h.NamedSquad(manager, "2nd Squad");
   float off = -1;
   if (showMap && shownSquad && shownSquad.GetAnchor())
   {
    WorkspaceWidget showWs = GetGame().GetWorkspace();
    float cx, cz;
    showMap.ScreenToWorld(showWs.GetWidth() / 2, showWs.GetHeight() / 2, cx, cz);
    off = vector.DistanceXZ(Vector(cx, 0, cz), shownSquad.GetAnchor().GetOrigin());
   }
   ACR_TestRegress.Check("twoicshots: the panel's Show closes it, selects the squad and centres the map on it", !pc.ACR_TwoICPanelOpen() && s_ShowId.IsValid() && pc.ACR_GetSelectedGroup() == s_ShowId && off >= 0 && off < 150, string.Format("panel open %1, selected %2, map centre %3 m from it", pc.ACR_TwoICPanelOpen(), pc.ACR_GetSelectedGroup() == s_ShowId, (int)off));
  }
  if (tick == 38) Shot("2ic_2b_show");
  if (tick == 39 && pc.ACR_TwoICPanelOpen()) pc.ACR_TwoICTogglePanel();
  if (tick == 40)
  {
   ACR_Base enemy = Nearest(alpha.GetAnchor().GetOrigin(), false);
   vector at = alpha.GetAnchor().GetOrigin() + Vector(200, 0, 100);
   if (enemy) at = enemy.GetOrigin();
   pc.ACR_TestTwoICFilter(ACR_ETwoICFilter.REQUESTS);
   pc.ACR_TestOpenPicker(at);
  }
  if (tick == 43) Shot("2ic_3_requests");
  // One menu (sketch 3): ACA's pings on top of the asks, its own picker hidden; a ping from it.
  if (tick == 45)
  {
   ACR_TestRegress.Check("twoicshots: Alt+right-click opens one menu, ACA's pings above the asks", pc.ACR_TestOneMenu(), "");
   s_iPings = pc.ACR_TestPingCount();
   pc.ACR_TwoICPing(ACR_EPing.LOOK);
  }
  if (tick == 49) ACR_TestRegress.Check("twoicshots: a ping from that menu is placed", s_iPings >= 0 && pc.ACR_TestPingCount() > s_iPings, string.Format("pings %1 -> %2", s_iPings, pc.ACR_TestPingCount()));
  // Establish FOB as a player arms it: his command unit selected (it may establish), then armed.
  if (tick == 47)
  {
   pc.ACR_TestClosePicker();
   pc.ACR_TestTwoICFilter(ACR_ETwoICFilter.ALL);
   pc.ACR_TestSelect(s_sTruck);
  }
  if (tick == 48) pc.ACR_TestArmEstablish();
  // The 2IC's numbered sites come once scored (a few seconds to half a minute).
  if (tick >= 50 && s_iFobShot == 0 && (pc.ACR_TestSitesShown() || tick >= 80))
  {
   s_iFobShot = tick;
   PrintFormat("[ACR] TWOICSHOTS sites shown %1 after %2 s", pc.ACR_TestSitesShown(), tick - 47);
   Shot("2ic_4_fob");
   string look;
   bool placing = pc.ACR_TestPlacingLook(look);
   ACR_TestRegress.Check("twoicshots: placing a FOB only the banner and the sites show (ACA's panel, INTEL and the channel hidden, the card a pill)", placing, look);
  }
  // Board 2: the first site's card, as under the cursor.
  if (s_iFobShot > 0 && tick == s_iFobShot + 1) pc.ACR_TestHoverSite(0);
  if (s_iFobShot > 0 && tick == s_iFobShot + 3)
  {
   Shot("2ic_4b_site_card");
   ACR_TestRegress.Check("twoicshots: the site under the cursor opens its card beside it", !pc.ACR_TestSitesShown() || pc.ACR_TestCardSite() == 0, string.Format("card site %1", pc.ACR_TestCardSite()));
  }
  // The channel's "Show" on the suggestion pans the map to its marker; placing ends (ACA's panel
  // and INTEL come back).
  if (s_iFobShot > 0 && tick == s_iFobShot + 4)
  {
   pc.ACR_TestHoverSite(-1);
   pc.ACR_TwoICCancelEstablish();
   pc.ACR_TestClosePicker();
   pc.ACR_TwoICShowSuggestion();
  }
  if (s_iFobShot > 0 && tick == s_iFobShot + 6)
  {
   SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
   float x, z;
   if (mapEntity) mapEntity.GetMapCenterWorldPosition(x, z);
   vector ring;
   bool has = pc.ACR_TestSuggestionRing(ring);
   float off = vector.DistanceXZ(ring, Vector(x, 0, z));
   ACR_TestRegress.Check("twoicshots: the suggestion's Show pans the map to its marker", has && off < 30, string.Format("ring %1, map centre %2 (%3 m off)", has, Vector(x, 0, z), (int)off));
   string back;
   bool restored = pc.ACR_TestRestored(back);
   ACR_TestRegress.Check("twoicshots: after placing, ACA's panel, INTEL and the channel come back", restored, back);
  }
  // Board 3: the suggestion's card, then Send truck: "Truck sent", the dashed ring and its label.
  if (s_iFobShot > 0 && tick == s_iFobShot + 7) Shot("2ic_6_suggest_before");
  if (s_iFobShot > 0 && tick == s_iFobShot + 8)
  {
   ACR_TwoICSuggestion live = ACR_TwoICSuggest.LiveFor("US");
   if (live)
   {
    s_iSentId = live.m_iId;
    pc.ACR_TwoICAct(live.m_iId, ACR_TwoICSuggest.ACT_ACCEPT);
   }
   PrintFormat("[ACR] TWOICSHOTS send truck on line %1", s_iSentId);
  }
  if (s_iFobShot > 0 && tick == s_iFobShot + 12)
  {
   Shot("2ic_7_suggest_after");
   ACR_TwoICLine line = ACR_TwoIC.Find("US", s_iSentId);
   string status = "-";
   if (line) status = line.m_sStatus + " / " + line.m_sAnswer;
   vector sentAt;
   bool sentRing = pc.ACR_TestSentRing(sentAt);
   ACR_TestRegress.Check("twoicshots: after Send truck the line reads Truck sent, and the site keeps a dashed ring", line && line.m_sStatus == "Truck sent" && sentRing, string.Format("'%1'; sent ring %2", status, sentRing));
  }
  // Board 4: zoomed out (the card a pill), then the 2IC hidden with two lines that need the player
  // (the 2IC button's count).
  if (s_iFobShot > 0 && tick == s_iFobShot + 13) pc.ACR_TestZoomOut();
  if (s_iFobShot > 0 && tick == s_iFobShot + 15)
  {
   Shot("2ic_8_zoomed_out");
   ACR_TestRegress.Check("twoicshots: zoomed out the card is a one-line pill", pc.ACR_TestTwoICForm() == ACR_TwoICView.FORM_PILL, string.Format("form %1, zoom %2", pc.ACR_TestTwoICForm(), pc.ACR_TestZoom()));
   CantYet(pc, "Lamentin");
   CantYet(pc, "Provins");
   SCR_MapToolEntry toggle = SCR_MapToolMenuUI.s_ACR_TwoICEntry;
   if (toggle && pc.ACR_TwoICShown()) toggle.m_OnClick.Invoke();
   if (pc.ACR_TwoICShown()) pc.ACR_TestTwoICShown(false);
  }
  if (s_iFobShot > 0 && tick == s_iFobShot + 16) pc.ACR_TestCentreOnBases();
  if (s_iFobShot > 0 && tick == s_iFobShot + 18)
  {
   Shot("2ic_9_hidden_badge");
   int badgeDrawn = ACR_OrderDisplay.s_iTwoICBadgeDrawn;
   ACR_TestRegress.Check("twoicshots: the 2IC hidden, its button counts the lines that need the player", !pc.ACR_TwoICShown() && pc.ACR_TestBadge() >= 2 && badgeDrawn > 0, string.Format("shown %1, count %2, drawn in %3 frames", pc.ACR_TwoICShown(), pc.ACR_TestBadge(), badgeDrawn));
   Finish("");
  }
 }

 // The live suggestion as if Send truck had found no truck (failed) or open again; false: none.
 protected static bool FailSuggestion(bool failed)
 {
  ACR_TwoICSuggestion live = ACR_TwoICSuggest.LiveFor("US");
  if (!live) return false;
  string answer = live.m_sWhy;
  if (failed) answer += ACR_TwoICCompact.TRUCK_FAIL + "no construction truck for the FOB site (requisition one with its 500 supplies, then Send truck again)";
  ACR_TwoIC.SetLine("US", live.m_iId, "Suggested", answer, ACR_TwoICSuggest.ACT_ACCEPT | ACR_TwoICRequests.ACT_CANCEL, 0);
  return true;
 }

 // A request line the 2IC can't do yet (only its look: no request behind it).
 protected static void CantYet(SCR_PlayerController pc, string place)
 {
  int me = pc.GetPlayerId();
  string name = GetGame().GetPlayerManager().GetPlayerName(me);
  int id = ACR_TwoIC.Say("US", ACR_ETwoICLine.REQUEST, string.Format("%1: hold %2", name, place), "Can't yet");
  ACR_TwoIC.SetLine("US", id, "Can't yet", "2IC: can't yet: every squad is busy", ACR_TwoICRequests.ACT_KEEP | ACR_TwoICRequests.ACT_CANCEL, me);
 }

 protected static void BuyTruck(ACR_Manager manager, ACR_Base at)
 {
  ACR_FactionData data = manager.m_Bases.GetFaction("US");
  if (!data || !at) return;
  int pick = -1;
  foreach (int e, ACR_RequisitionEntry offer : data.m_aRequisitions)
  {
   if (offer.m_iRemaining <= 0 || offer.m_sVehicle.IsEmpty()) continue;
   if ((ACR_Logistics.ServiceOfName(offer.m_sVehicle) & ACR_EService.CONSTRUCTION) == 0) continue;
   if (pick < 0 || offer.m_iCost < data.m_aRequisitions[pick].m_iCost) pick = e;
  }
  if (pick < 0)
  {
   Print("[ACR] TWOICSHOTS no construction truck in the US list");
   return;
  }
  data.m_iRP += data.m_aRequisitions[pick].m_iCost;
  PrintFormat("[ACR] TWOICSHOTS bought: %1", manager.PurchaseAt("US", pick, at));
 }

 protected static ACR_Base Nearest(vector pos, bool ours)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   if (ours != (base.m_sOwner == "US")) continue;
   if (!ours && base.m_sOwner.IsEmpty()) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), pos);
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = base;
  }
  return best;
 }

 // In the engine, to $profile:acr-shots/<name>.bmp (the last frame drawn, so a tick after the state
 // to show). Its folder is made first: a write that fails puts the shot on the clipboard instead,
 // so with no folder the shot is skipped and logged.
 static void Shot(string name)
 {
  FileIO.MakeDirectory("$profile:acr-shots");
  if (!FileIO.FileExists("$profile:acr-shots"))
  {
   PrintFormat("[ACR] TWOICSHOTS %1 skipped: no shots folder", name);
   return;
  }
  System.MakeScreenshot("$profile:acr-shots/" + name);
  PrintFormat("[ACR] TWOICSHOTS ready %1", name);
 }

 protected static void Finish(string why)
 {
  if (why.IsEmpty())
  {
   ACR_TestRegress.Complete("twoicshots", CHECKS);
   return;
  }
  PrintFormat("[ACR] TWOICSHOTS %1", why);
  ACR_TestRegress.Abort("twoicshots", why);
 }
}

// Test-only hooks on the 2IC's client state (not in the player package).
modded class SCR_PlayerController
{
 void ACR_TestTwoICShown(bool shown)
 {
  m_ACR_TwoICShown = shown;
 }

 void ACR_TestTwoICFilter(int filter)
 {
  if (m_ACR_TwoICView) m_ACR_TwoICView.SetFilter(filter);
 }

 void ACR_TestOpenPicker(vector pos)
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (mapEntity) ACR_OpenPicker(mapEntity, pos);
 }

 void ACR_TestClosePicker()
 {
  ACR_ClosePicker();
 }

 bool ACR_TestOneMenu()
 {
  return m_ACR_TwoICView && m_ACR_TwoICView.AskShown() && m_ACR_Panel && !m_ACR_Panel.PickerShown();
 }

 int ACR_TestPingCount()
 {
  return m_ACR_Data.m_PingIds.Count();
 }

 bool ACR_TestSuggestionRing(out vector at)
 {
  foreach (int r, int kind : m_ACR_Data.m_TwoICRingKinds)
  {
   if (kind != ACR_TwoICSuggest.RING_KIND || r >= m_ACR_Data.m_TwoICRings.Count()) continue;
   at = m_ACR_Data.m_TwoICRings[r];
   return true;
  }
  return false;
 }

 bool ACR_TestSitesShown()
 {
  return !m_ACR_Data.m_TwoICSites.IsEmpty();
 }

 // Selects the squad whose name starts so in the map data (as a click on it would).
 void ACR_TestSelect(string prefix)
 {
  int index = -1;
  foreach (int i, string name : m_ACR_Data.m_GroupNames)
  {
   if (name.StartsWith(prefix)) index = i;
  }
  if (index < 0 || index >= m_ACR_Data.m_GroupIds.Count())
  {
   string names = "";
   foreach (string known : m_ACR_Data.m_GroupNames) names += known + " | ";
   PrintFormat("[ACR] TWOICSHOTS no map data for %1: %2", prefix, names);
   return;
  }
  ACR_SelectGroup(m_ACR_Data.m_GroupIds[index]);
 }

 void ACR_TestArmEstablish()
 {
  m_ACR_CursorMode = ACR_ECursorMode.ESTABLISH_FOB;
 }

 // ---------------------------------------------------------------- the declutter boards
 void ACR_TestTwoICHover(bool hover)
 {
  if (m_ACR_TwoICView) m_ACR_TwoICView.SetHoverForTest(hover);
 }

 int ACR_TestTwoICForm()
 {
  if (!m_ACR_TwoICView) return -1;
  return m_ACR_TwoICView.CardForm();
 }

 int ACR_TestPinned()
 {
  if (!m_ACR_TwoICView) return -1;
  return m_ACR_TwoICView.ShownPinned();
 }

 int ACR_TestOneLine()
 {
  if (!m_ACR_TwoICView) return -1;
  return m_ACR_TwoICView.ShownOneLine();
 }

 int ACR_TestMore()
 {
  if (!m_ACR_TwoICView) return -1;
  return m_ACR_TwoICView.ShownMore();
 }

 int ACR_TestRows()
 {
  if (!m_ACR_TwoICView) return -1;
  return m_ACR_TwoICView.ShownRows();
 }

 void ACR_TestHoverSite(int site)
 {
  m_ACR_TwoICForcedSite = site;
 }

 int ACR_TestCardSite()
 {
  if (!m_ACR_TwoICView) return -1;
  return m_ACR_TwoICView.CardSite();
 }

 // Placing a FOB: the card a pill, the channel, ACA's panel and its intel feed hidden.
 bool ACR_TestPlacingLook(out string detail)
 {
  if (!m_ACR_TwoICView || !m_ACR_Panel) return false;
  bool panel = m_ACR_Panel.m_Root && !m_ACR_Panel.m_Root.IsVisible();
  bool feed = !m_ACR_Panel.ACR_FeedShown();
  bool channel = !m_ACR_TwoICView.ChannelShown();
  bool pill = m_ACR_TwoICView.CardForm() == ACR_TwoICView.FORM_PILL;
  detail = string.Format("ACA panel hidden %1, INTEL hidden %2, channel hidden %3, card a pill %4 (establishing %5)", panel, feed, channel, pill, m_ACR_TwoICEstablishing);
  return panel && feed && channel && pill;
 }

 bool ACR_TestRestored(out string detail)
 {
  if (!m_ACR_TwoICView || !m_ACR_Panel) return false;
  bool panel = m_ACR_Panel.m_Root && m_ACR_Panel.m_Root.IsVisible();
  bool channel = m_ACR_TwoICView.ChannelShown();
  bool card = m_ACR_TwoICView.CardForm() != ACR_TwoICView.FORM_PILL || ACR_TwoICZoomedOut();
  detail = string.Format("ACA panel shown %1, INTEL hidden by the 2IC %2, channel shown %3, card form %4", panel, m_ACR_Panel.ACR_TwoICPlacingHidden(), channel, m_ACR_TwoICView.CardForm());
  return panel && !m_ACR_Panel.ACR_TwoICPlacingHidden() && channel && card;
 }

 bool ACR_TestSentRing(out vector at)
 {
  foreach (int r, int kind : m_ACR_Data.m_TwoICRingKinds)
  {
   if (kind != ACR_TwoICSuggest.RING_SENT || r >= m_ACR_Data.m_TwoICRings.Count()) continue;
   at = m_ACR_Data.m_TwoICRings[r];
   return true;
  }
  return false;
 }

 // Zoomed all the way out (boards 8 and 9), then centred on our bases (ZoomOut centres the world,
 // and pn12f's badge shot was mostly off the map).
 void ACR_TestZoomOut()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return;
  mapEntity.ZoomOut();
  ACR_TestCentreOnBases();
 }

 // The map at its zoom, centred on the mean of the US bases.
 void ACR_TestCentreOnBases()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return;
  vector sum;
  int count;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != "US") continue;
   sum = sum + base.GetOrigin();
   count++;
  }
  if (count == 0) return;
  vector mean = sum * (1.0 / count);
  mapEntity.ZoomPanSmooth(mapEntity.GetCurrentZoom(), mean[0], mean[2], 0.1);
  PrintFormat("[ACR] TWOICSHOTS map centred on %1 US bases at %2 (zoom %3)", count, mean, mapEntity.GetCurrentZoom());
 }

 string ACR_TestZoom()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return "-";
  return string.Format("%1 (min %2, zoomed out %3)", mapEntity.GetCurrentZoom(), mapEntity.GetMinZoom(), ACR_TwoICZoomedOut());
 }

 int ACR_TestBadge()
 {
  return m_ACR_Data.m_iTwoICBadge;
 }

 bool ACR_TestBuyShown(out bool enabled, out string label)
 {
  enabled = false;
  label = "";
  if (!m_ACR_TwoICView) return false;
  return m_ACR_TwoICView.BuyShown(enabled, label);
 }

 // The card's root, by its name, under the map's root.
 bool ACR_TestCardNamed()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.GetMapMenuRoot()) return false;
  Widget card = mapEntity.GetMapMenuRoot().FindAnyWidget(ACR_TwoICView.CARD_NAME);
  return card && card == m_ACR_TwoICView.m_CardRoot;
 }
}
