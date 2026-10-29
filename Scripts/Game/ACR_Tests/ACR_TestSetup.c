// -acrSetupTest 1 (server, Arland, ~40 s): the setup phase (ACR_Setup). Runs from ACR_Mode's
// one-second timer, because the manager's tick is held while the phase lasts.
// Checks: nothing starts before Start (no AI, no layout, host options unread), rows are
// offered, a stranger can't change or start anything, options cycle on the server, and
// Start applies them (threat high reaches the game, starting units spawn).
class ACR_TestSetup
{
 static const int CHECKS = 25;
 protected static int s_iStartedAt = -1;

 static bool Wanted() { return System.IsCLIParam("acrSetupTest"); }

 protected static int Agents()
 {
  array<AIAgent> agents = {};
  if (GetGame().GetAIWorld()) GetGame().GetAIWorld().GetAIAgents(agents);
  return agents.Count();
 }

 static void Run(int second)
 {
  if (second == 6)
  {
   int agents = Agents();
   ACR_TestRegress.Check("setup: nothing starts before Start", ACR_Setup.Wanted() && !ACR_Setup.Started() && !ACR_Skirmish.Applied() && agents == 0, string.Format("wanted %1, started %2, layout applied %3, AI agents %4", ACR_Setup.Wanted(), ACR_Setup.Started(), ACR_Skirmish.Applied(), agents));
   string rows = ACR_Setup.Rows();
   ACR_TestRegress.Check("setup: the dialog rows list the options", rows.Contains("threat\tThreat: normal") && rows.Contains("ai\tAI commanders") && rows.Contains("shared\tShared command: on"), rows);
   // Tabs (25 Oct redesign): each tab fits one page of a 1080p screen (the list once ran off it,
   // hiding the map preview row), and the map preview sits on the first tab with the battlefield.
   array<int> tabRows = {};
   for (int t = 0; t < ACR_SetupDialog.TABS; t++) tabRows.Insert(0);
   array<string> lines = {};
   rows.Split("\n", lines, true);
   foreach (string line : lines)
   {
    array<string> parts = {};
    line.Split("\t", parts, false);
    if (parts.Count() >= 2) tabRows[ACR_SetupDialog.Tab(parts[0])] = tabRows[ACR_SetupDialog.Tab(parts[0])] + 1;
   }
   int perPage = ACR_SetupDialog.PageRows(1080);
   int largest;
   string counts;
   foreach (int tabCount : tabRows) { if (tabCount > largest) largest = tabCount; counts += tabCount.ToString() + " "; }
   float used = (perPage + 2) * ACR_SetupDialog.ROW_HEIGHT + ACR_SetupDialog.CHROME;
   ACR_TestRegress.Check("setup: every tab fits one page of a 1080p screen", perPage >= 10 && used <= 1080 && largest <= perPage, string.Format("rows per tab %1, %2 a page (%3 of 1080 used)", counts, perPage, used));
   // Every row fits its button at 1080p at every value it can show, and every line under the rows
   // (each company's and doctrine's, both sides) fits too (pn10b, 26 Oct: the company and doctrine
   // rows ran past theirs).
   int rowsChecked = 0;
   int tooLong = 0;
   string longestRow = "";
   string overLong = "";
   foreach (string rowLine : lines)
   {
    array<string> rowParts = {};
    rowLine.Split("\t", rowParts, false);
    if (rowParts.Count() < 2 || rowParts[0] == "map" || rowParts[0] == "undo") continue;
    array<string> shownTexts = {};
    if (rowParts[0].StartsWith("about")) shownTexts.Insert(rowParts[1]);
    else
    {
     array<string> shownValues = ACR_Setup.Values(rowParts[0]);
     shownValues.Insert(ACR_Setup.Current(rowParts[0]));
     foreach (string rowValue : shownValues) shownTexts.Insert(ACR_Setup.Label(rowParts[0]) + ": " + ACR_Setup.ValueName(rowParts[0], rowValue));
    }
    if (rowParts[0] == "aboutcompanywest")
    {
     array<string> companyKeys = {"", "mechanized", "weapons", "engineers", "recon"};
     foreach (string companyKey : companyKeys) shownTexts.Insert("Blue: " + ACR_DoctrineKits.CompanySummary(companyKey));
     foreach (ACR_AIDoctrine eachDoctrine : ACR_AIDoctrine.All()) shownTexts.Insert("Blue: " + eachDoctrine.m_sName + ": " + eachDoctrine.m_sBrief);
    }
    foreach (string rowText : shownTexts)
    {
     rowsChecked++;
     if (rowText.Length() > longestRow.Length()) longestRow = rowText;
     if (rowText.Length() > ACR_Setup.ROW_CHARS)
     {
      tooLong++;
      overLong += rowText + " | ";
     }
    }
   }
   ACR_TestRegress.Check("setup: every row fits its button at every value", rowsChecked > 50 && tooLong == 0, string.Format("%1 rows checked, %2 over %3 characters: %4longest '%5' (%6)", rowsChecked, tooLong, ACR_Setup.ROW_CHARS, overLong, longestRow, longestRow.Length()));
   ACR_TestRegress.Check("setup: the map preview is on the first tab with the battlefield", ACR_SetupDialog.Tab("map") == 0 && ACR_SetupDialog.Tab("seed") == 0 && ACR_SetupDialog.Tab("layout") == 0 && rows.Contains("map\t") && rows.IndexOf("map\t") < rows.IndexOf("pockets\t"), "");
   ACR_MissionHeader logisticsHeader = new ACR_MissionHeader();
   logisticsHeader.m_sAcrDeployment = "military";
   logisticsHeader.m_sAcrReinforcement = "deployment";
   logisticsHeader.m_sAcrFobServices = "free";
   logisticsHeader.m_iAcrDepotCapacity = 4000;
   logisticsHeader.m_iAcrServiceSupplyCost = 20;
   logisticsHeader.m_iAcrFobRPServiceCost = 5;
   logisticsHeader.m_iAcrFobRelay = 0;
   map<string, string> logisticsDefaults = new map<string, string>();
   logisticsHeader.Fill(logisticsDefaults);
   ACR_Options.UseHeaderForTest(logisticsDefaults);
   ACR_TestRegress.Check("setup: mission header can disable FOB relay requirement", !ACR_LogisticsRules.RequireRelay() && ACR_Setup.Current("fobrelay") == "0", "");
   ACR_Setup.Cycle("fobrelay");
   ACR_TestRegress.Check("setup: relay startup choice overrides mission header", ACR_LogisticsRules.RequireRelay() && ACR_Options.Source("acrFobRelay") == "screen" && ACR_Setup.Rows().Contains("fobrelay\t"), "");
   ACR_Options.Unset("acrFobRelay");
   ACR_TestRegress.Check("setup: logistics mission-header defaults resolve", ACR_LogisticsRules.DepotBonus() == 4000 && ACR_LogisticsRules.ServiceCost() == 20 && ACR_LogisticsRules.InstantServiceFee() == 5 && !ACR_LogisticsRules.SuppliedServices(), "");
   ACR_TestRegress.Check("setup: mission header maps deployment policies", ACR_LogisticsRules.Choice("acrDeployment", "") == "military" && ACR_LogisticsRules.Choice("acrReinforcement", "") == "deployment", "");
   ACR_Setup.Cycle("deployment");
   string deployment;
   ACR_Options.Get("acrDeployment", deployment);
   ACR_TestRegress.Check("setup: startup deployment choice overrides mission header", deployment == "all" && ACR_Options.Source("acrDeployment") == "screen", deployment);
   ACR_Options.Unset("acrDeployment");
   ACR_Setup.Cycle("fobservices");
   string services;
   ACR_Options.Get("acrFobServices", services);
   ACR_TestRegress.Check("setup: startup service choice overrides free header", services == "supplies" && ACR_LogisticsRules.SuppliedServices(), services);
   ACR_Options.Unset("acrFobServices");
   ACR_Options.UseHeaderForTest(null);
   string refused = ACR_Setup.Handle(9999, "threat");
   string refusedStart = ACR_Setup.Handle(9999, "start");
   ACR_TestRegress.Check("setup: a player who isn't host or admin can't change or start", refused.Contains("Only the host") && refusedStart.Contains("Only the host") && !ACR_Setup.Started(), refused);
   string first = ACR_Setup.Cycle("threat");
   string value;
   ACR_Options.Get("acrThreat", value);
   ACR_TestRegress.Check("setup: a click cycles threat normal -> high on the server", value == "high" && ACR_Options.Source("acrThreat") == "screen" && first.Contains("high"), first);
   ACR_Setup.Cycle("unitcap");
   string cap;
   ACR_Options.Get("acrUnitCap", cap);
   ACR_Setup.Cycle("startrp");
   ACR_Setup.Cycle("startrp");
   string rp;
   ACR_Options.Get("acrStartRP", rp);
   ACR_TestRegress.Check("setup: numeric options cycle from the scenario default", cap == "4" && rp == "100", string.Format("unit cap %1, start RP %2", cap, rp));
   // Any faction the world lists can play (13 Oct): Arland lists FIA; pick it for the red side.
   array<string> keys = {};
   ACR_FactionSelection.Candidates(keys);
   string listed;
   foreach (string k : keys) listed += k + " ";
   ACR_TestRegress.Check("setup: playable factions come from the world's faction manager", keys.Contains("US") && keys.Contains("USSR") && keys.Contains("FIA") && !keys.Contains("CIV"), listed);
   string red = ACR_Setup.Cycle("east");
   ACR_TestRegress.Check("setup: the red side cycles to FIA without applying yet", ACR_FactionSelection.East() == "FIA" && ACR_FactionSelection.West() == "US" && !ACR_FactionSelection.Applied() && red.Contains("FIA"), red);
   ACR_Setup.Start("the setup test");
   s_iStartedAt = second;
  }
  if (s_iStartedAt > 0 && second == s_iStartedAt + 8)
  {
   ACR_Manager manager = ACR_Manager.GetInstance();
   ACR_TestRegress.Check("setup: Start applies the chosen threat", ACR_Setup.Started() && ACR_Manager.ThreatOption() == 2, string.Format("threat %1", ACR_Manager.ThreatOption()));
   int rp = -1;
   if (manager && !ACR_FactionSettings.s_All.IsEmpty()) rp = manager.m_Bases.GetRP(ACR_FactionSettings.s_All[0].m_sFaction);
   ACR_TestRegress.Check("setup: buy lists and RP are built with the chosen start RP", rp >= 100 && rp < 200, string.Format("RP %1", rp));
   int agents = Agents();
   ACR_TestRegress.Check("setup: the layout is applied and starting units spawn after Start", ACR_Skirmish.Applied() && agents > 0, string.Format("AI agents %1, squads %2", agents, manager.m_Squads.Count()));
   FactionManager factions = GetGame().GetFactionManager();
   SCR_Faction fia = SCR_Faction.Cast(factions.GetFactionByKey("FIA"));
   SCR_Faction ussr = SCR_Faction.Cast(factions.GetFactionByKey("USSR"));
   ACR_TestRegress.Check("setup: Start makes FIA the red side", ACR_FactionSelection.Applied() && ACR_FactionSelection.East() == "FIA" && fia && fia.IsPlayable() && ussr && !ussr.IsPlayable(), "");
   int fiaBases, ussrBases;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner == "FIA") fiaBases++;
    if (base.m_sOwner == "USSR") ussrBases++;
   }
   ACR_TestRegress.Check("setup: the red side's bases belong to FIA", fiaBases > 0 && ussrBases == 0, string.Format("FIA %1, USSR %2", fiaBases, ussrBases));
   int buys = -1;
   if (manager) buys = manager.m_Bases.GetFaction("FIA").m_aRequisitions.Count();
   ACR_TestRegress.Check("setup: FIA gets settings and a buy list from its catalog", ACR_FactionSettings.Find("FIA") != null && buys > 0, string.Format("buy list %1", buys));
   int fiaSquads;
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == "FIA") fiaSquads++;
   ACR_TestRegress.Check("setup: the red side's starting units spawn as FIA", fiaSquads > 0, string.Format("FIA squads %1", fiaSquads));
   array<ref SCR_BasePlayerLoadout> loadouts = {};
   int officers = GetGame().GetLoadoutManager().GetPlayerLoadoutsByFaction(fia, loadouts);
   ACR_TestRegress.Check("setup: FIA has a command unit loadout", officers > 0, string.Format("%1 loadouts", officers));
   // AI reserve (Jacob 14 Oct): give US every neutral non-HQ base for a moment; the reserve must
   // pick a secure one behind the front, not the HQ.
   array<ACR_Base> lent = {};
   foreach (ACR_Base neutral : ACR_Base.s_All)
    if (neutral.m_sOwner.IsEmpty()) { neutral.m_sOwner = "US"; lent.Insert(neutral); }
   ACR_AICommander reserveAI = new ACR_AICommander(manager, "US");
   ACR_Base reserveAt = reserveAI.ReserveBase();
   string reserveText = "none";
   if (reserveAt) reserveText = string.Format("%1 (HQ %2, value %3, radius %4, %5 m from the front)", reserveAt.m_sName, reserveAI.IsHQ(reserveAt), reserveAt.m_iValue, reserveAt.m_fRadius, Math.Round(reserveAI.FrontDistance(reserveAt)));
   ACR_TestRegress.Check("setup: the AI reserve waits at a secure non-HQ base behind the front", reserveAt && !reserveAI.IsHQ(reserveAt) && reserveAI.ReserveFits(reserveAt), string.Format("%1 of %2 lent bases", reserveText, lent.Count()));
   foreach (ACR_Base back : lent) back.m_sOwner = "";
   ACR_TestRegress.Complete("setup", CHECKS);
  }
 }

 // -acrSetupUiTest 1 (GAME, Arland, ~40 s, on Jacob's screen): the host's dialog itself.
 static bool UiWanted() { return System.IsCLIParam("acrSetupUiTest"); }
 static void RunUi(int second)
 {
  if (second == 10)
  {
   ACR_SetupDialog dialog = ACR_SetupDialog.Cast(SCR_ConfigurableDialogUi.GetCurrentDialog());
   ACR_TestRegress.Check("setup UI: the host sees the setup dialog with the options", dialog != null && ACR_SetupClient.s_bAllowed && ACR_SetupClient.s_sRows.Contains("Threat: normal") && !ACR_Setup.Started(), string.Format("dialog %1, allowed %2", dialog != null, ACR_SetupClient.s_bAllowed));
   ACR_SetupClient.Request("threat");
  }
  if (second == 11)
  {
   ACR_SetupDialog tabbed = ACR_SetupDialog.Cast(SCR_ConfigurableDialogUi.GetCurrentDialog());
   if (tabbed) tabbed.ShowTab(1);
  }
  if (second == 12)
  {
   ACR_TestRegress.Check("setup UI: a click reaches the server and the dialog refreshes", ACR_SetupClient.s_sRows.Contains("Threat: high") && ACR_Manager.ThreatOption() == 2, ACR_SetupClient.s_sRows);
   ACR_SetupDialog shown = ACR_SetupDialog.Cast(SCR_ConfigurableDialogUi.GetCurrentDialog());
   ACR_TestRegress.Check("setup UI: the Command tab shows the AI rows, not the battlefield's", shown && shown.ShowsRow("ai") && shown.ShowsRow("doctrinewest") && !shown.ShowsRow("threat") && !shown.ShowsRow("map") && shown.ShowsRow("tab:0"), "");
  }
  if (second == 13) ACR_SetupClient.Request("start");
  if (second == 25)
  {
   ACR_SetupDialog open = ACR_SetupDialog.Cast(SCR_ConfigurableDialogUi.GetCurrentDialog());
   ACR_TestRegress.Check("setup UI: Start closes the dialog and the match begins", ACR_Setup.Started() && !ACR_SetupClient.s_bActive && open == null && ACR_Skirmish.Applied(), string.Format("started %1, dialog open %2", ACR_Setup.Started(), open != null));
   PlayerController pc = GetGame().GetPlayerController();
   ACR_TestRegress.Check("setup UI: the host spawns after Start", pc && pc.GetControlledEntity() != null, "");
   ACR_TestRegress.Complete("setupui", 5);
  }
 }

 // -acrSetupMapTest 1 (GAME, Kolguyev, ~75 s): the layout preview on the host's own map. The dialog
 // shows the map row; "Show the layout on the map" opens the map with one marker per planned base;
 // returning via the visible button and the keyboard handler brings the dialog back; reopening
 // must restore the controls, then Start builds the layout. Never force-close menus in this test.
 static bool MapWanted() { return System.IsCLIParam("acrSetupMapTest"); }
 protected static string s_sMapDeleted;
 protected static int s_iMapBeforeDelete;
 protected static vector s_vRememberedMapCenter;
 protected static float s_fRememberedMapZoom;
 protected static int s_iMapSeedBefore;
 protected static SCR_ButtonTextComponent s_MapSeedButton;

 static void RunMap(int second)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (second == 10)
  {
   ACR_SetupDialog dialog = ACR_SetupDialog.Cast(SCR_ConfigurableDialogUi.GetCurrentDialog());
   ACR_TestRegress.Check("setup map: the host sees the dialog with the map row", dialog != null && ACR_SetupClient.s_sRows.Contains("map\tShow and edit the layout on the map"), ACR_SetupClient.s_sRows);
  }
  if (second == 12) ACR_SetupClient.OpenPreviewMap();
  if (second == 18)
  {
   SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
   int markers = 0;
   if (pc) markers = pc.ACR_BaseMarkerCount();
   int planned = ACR_Setup.s_PreviewNames.Count();
   ACR_TestRegress.Check("setup map: the map opens without a body", mapEntity && mapEntity.IsOpen() && pc && !pc.GetControlledEntity(), "");
   ACR_TestRegress.Check("setup map: one marker per planned base", planned > 0 && markers == planned, string.Format("%1 markers, %2 planned", markers, planned));
   ACR_TestRegress.Check("setup map: base captions visible without hovering", pc && pc.ACR_TestBaseCaptionsVisible(), "actual MarkerText widgets");
   ACR_TestRegress.Check("setup map: base captions remain visible after mouse leave", pc && pc.ACR_TestBaseCaptionsVisible(true), "vanilla mouse-leave callback");
   ACR_TestRegress.Check("setup map: hover expands full details and leave restores compact text", pc && pc.ACR_TestBaseHover(), "actual enter/leave callbacks");
   ACR_TestRegress.Check("setup map: repeated creation replaces and positions the widget", pc && pc.ACR_TestBaseRecreate(), "old root detached, new root at world projection");
   ACR_TestRegress.Check("setup map: the dialog stays hidden while the map is open", SCR_ConfigurableDialogUi.GetCurrentDialog() == null, "");
   // New seed on the map: rolls the layout without going back to the dialog.
   s_iMapSeedBefore = ACR_Generator.Seed();
   SCR_MapMenuUI seedMenu = SCR_MapMenuUI.Cast(GetGame().GetMenuManager().GetTopMenu());
   Widget seedWidget;
   if (seedMenu) seedWidget = seedMenu.GetRootWidget().FindAnyWidget("ACR_SetupSeed");
   if (seedWidget) s_MapSeedButton = SCR_ButtonTextComponent.FindButtonTextComponent(seedWidget);
   if (s_MapSeedButton) s_MapSeedButton.m_OnClicked.Invoke(s_MapSeedButton);
  }
  if (second == 20)
  {
   int seedMarkers = 0;
   if (pc) seedMarkers = pc.ACR_BaseMarkerCount();
   int seedNow = ACR_Generator.Seed();
   ACR_TestRegress.Check("setup map: New seed button rolls a new layout and the map redraws it", s_MapSeedButton && seedNow != s_iMapSeedBefore && seedMarkers == ACR_Setup.s_PreviewNames.Count() && ACR_SetupClient.s_sRows.Contains(seedNow.ToString()), string.Format("seed %1 -> %2, %3 markers, %4 planned", s_iMapSeedBefore, seedNow, seedMarkers, ACR_Setup.s_PreviewNames.Count()));
   array<ref ACR_GenSite> layout = {};
   array<ACR_GenSite> hqs = {};
   ACR_Generator.Plan(ACR_Generator.Seed(), ACR_Skirmish.SizeOption(), layout, hqs);
   foreach (ACR_GenSite site : layout)
   {
    if (hqs.Contains(site)) continue;
    s_sMapDeleted = site.m_sName;
    break;
   }
   s_iMapBeforeDelete = ACR_Setup.s_PreviewNames.Count();
   if (pc) pc.ACR_ClickBase(s_sMapDeleted);
  }
  if (second == 22)
  {
   SCR_MapMenuUI editor = SCR_MapMenuUI.Cast(GetGame().GetMenuManager().GetTopMenu());
   Widget deleteWidget;
   if (editor) deleteWidget = editor.GetRootWidget().FindAnyWidget("ACR_SetupDelete");
   SCR_ButtonTextComponent deleteButton;
   if (deleteWidget) deleteButton = SCR_ButtonTextComponent.FindButtonTextComponent(deleteWidget);
   ACR_TestRegress.Check("setup map: visible delete control", deleteButton && deleteWidget.IsVisible(), "Delete selected CP");
   if (deleteButton) deleteButton.m_OnClicked.Invoke(deleteButton);
  }
  if (second == 26)
  {
   ACR_TestRegress.Check("setup map: delete button removes selected CP and its marker", !ACR_Setup.s_PreviewNames.Contains(s_sMapDeleted) && ACR_Setup.s_PreviewNames.Count() == s_iMapBeforeDelete - 1 && pc && pc.ACR_BaseMarkerCount() == s_iMapBeforeDelete - 1, s_sMapDeleted);
  }
  if (second == 28)
  {
   SCR_MapMenuUI menu = SCR_MapMenuUI.Cast(GetGame().GetMenuManager().GetTopMenu());
   Widget backButton;
   if (menu) backButton = menu.GetRootWidget().FindAnyWidget("ACR_SetupReturn");
   SCR_ButtonTextComponent button;
   if (backButton) button = SCR_ButtonTextComponent.FindButtonTextComponent(backButton);
   bool visible = backButton && backButton.IsVisible();
   bool context = GetGame().GetInputManager().IsContextActive("ACR_SetupMapContext");
   ACR_TestRegress.Check("setup map: visible return button and bodyless input context", button && visible && context, string.Format("button %1 visible %2 context %3", button != null, visible, context));
   if (button) button.m_OnClicked.Invoke(button);
  }
  if (second == 32)
  {
   SCR_MapEntity afterButton = SCR_MapEntity.GetMapInstance();
   ACR_TestRegress.Check("setup map: return button restores setup without starting", afterButton && !afterButton.IsOpen() && SCR_ConfigurableDialogUi.GetCurrentDialog() != null && !ACR_Setup.Started(), "");
   ACR_SetupClient.OpenPreviewMap();
  }
  if (second == 40)
  {
   SCR_MapMenuUI reopened = SCR_MapMenuUI.Cast(GetGame().GetMenuManager().GetTopMenu());
   ACR_TestRegress.Check("setup map: preview can reopen with its return controls", reopened && reopened.GetRootWidget().FindAnyWidget("ACR_SetupReturn") != null, "");
   ACR_TestRegress.Check("setup map: delete control restored on reopening", reopened && reopened.GetRootWidget().FindAnyWidget("ACR_SetupDelete") != null, "");
   ACR_TestRegress.Check("setup map: reopened preview restores visible base captions", pc && pc.ACR_TestBaseCaptionsVisible(), "");
   // Same callback registered for MenuBack/GadgetMap; hardware key delivery needs a live check.
   if (reopened) reopened.ACR_ReturnToSetup();
  }
  if (second == 45)
  {
   SCR_MapEntity closed = SCR_MapEntity.GetMapInstance();
   ACR_SetupDialog back = ACR_SetupDialog.Cast(SCR_ConfigurableDialogUi.GetCurrentDialog());
   ACR_TestRegress.Check("setup map: closing the map brings the dialog back", (!closed || !closed.IsOpen()) && back != null, string.Format("dialog %1", back != null));
   ACR_SetupClient.Request("start");
  }
  if (second == 60)
  {
   ACR_TestRegress.Check("setup map: Start builds the previewed layout and the host spawns", ACR_Generator.Built() && ACR_Base.s_All.Count() == ACR_Setup.s_PreviewNames.Count() && pc && pc.GetControlledEntity() != null, string.Format("%1 bases", ACR_Base.s_All.Count()));
   bool absent = true;
   foreach (ACR_Base built : ACR_Base.s_All) if (built.m_sName == s_sMapDeleted) absent = false;
   ACR_TestRegress.Check("setup map: deleted CP is absent from the started match", absent, s_sMapDeleted);
  }
  if (second == 63) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (second == 66)
  {
   SCR_MapEntity positioned = SCR_MapEntity.GetMapInstance();
   if (positioned && positioned.IsOpen() && pc && pc.GetControlledEntity())
   {
    vector body = pc.GetControlledEntity().GetOrigin();
    float mapX = 7000;
    if (body[0] > 5000) mapX = 3000;
    positioned.ZoomPanSmooth(0.8, mapX, 5000, 0);
   }
  }
  if (second == 68)
  {
   SCR_MapEntity remembered = SCR_MapEntity.GetMapInstance();
   if (remembered && remembered.IsOpen())
   {
    float rememberedX, rememberedZ;
    remembered.GetMapCenterWorldPosition(rememberedX, rememberedZ);
    s_vRememberedMapCenter = Vector(rememberedX, 0, rememberedZ);
    s_fRememberedMapZoom = remembered.GetCurrentZoom();
   }
   ACR_TestRegress.Check("setup map: gameplay base captions visible without hovering", pc && pc.ACR_TestBaseCaptionsVisible(), "after Start, actual MarkerText widgets");
   ACR_TestRegress.Check("setup map: gameplay hover expands current base details", pc && pc.ACR_TestBaseHover(), "");
   GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  }
  if (second == 70) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (second == 73)
  {
   SCR_MapEntity restored = SCR_MapEntity.GetMapInstance();
   float restoredX, restoredZ;
   if (restored) restored.GetMapCenterWorldPosition(restoredX, restoredZ);
   bool sameView = restored && restored.IsOpen() && s_fRememberedMapZoom > 0 && vector.DistanceXZ(s_vRememberedMapCenter, Vector(restoredX, 0, restoredZ)) < 5 && Math.AbsFloat(restored.GetCurrentZoom() - s_fRememberedMapZoom) < 0.001;
   ACR_TestRegress.Check("setup map: reopening gameplay map preserves pan and zoom", sameView, string.Format("centre %1 -> %2, zoom %3", s_vRememberedMapCenter, Vector(restoredX, 0, restoredZ), s_fRememberedMapZoom));
   ACR_TestRegress.Check("setup map: reopened gameplay restores visible base captions", pc && pc.ACR_TestBaseCaptionsVisible(true), "also after mouse leave");
   ACR_TestRegress.Complete("setupmap", 23);
  }
 }

 // -acrReserveTest 1 (server, Everon, ~1 min): where the AI keeps its reserve on a big map.
 // US is given every neutral base north of the split line (z > 6650); the reserve must sit at a
 // secure non-HQ base, not the Airbase HQ. Logs the five best candidates.
 // -acrSetupGenTest 1 (server, Kolguyev, ~40 s): the layout preview during setup (aca-f07).
 static const int GEN_CHECKS = 25;
 protected static ref array<string> s_PreviewNames = {};
 static bool GenWanted() { return System.IsCLIParam("acrSetupGenTest"); }
 static void RunGen(int second)
 {
  if (second == 6)
  {
   ACR_Setup.UpdatePreview();
   string rows = ACR_Setup.Rows();
   int first = ACR_Setup.s_PreviewNames.Count();
   ACR_TestRegress.Check("setup preview: a generated layout is planned before Start", !ACR_Setup.Started() && first >= 15 && ACR_Base.s_All.IsEmpty(), string.Format("%1 bases planned, %2 built", first, ACR_Base.s_All.Count()));
   ACR_TestRegress.Check("setup preview: rows offer the seed and the map", rows.Contains("seed\tLayout seed: ") && rows.Contains("map\tShow and edit the layout on the map"), rows);
   string seedBefore;
   ACR_Options.Get("acrSeed", seedBefore);
   string firstNames = "";
   foreach (string n : ACR_Setup.s_PreviewNames) firstNames += n + ";";
   ACR_Setup.Cycle("seed");
   ACR_Setup.Cycle("layout"); // gen:full -> gen:half
   ACR_Setup.UpdatePreview();
   int half = ACR_Setup.s_PreviewNames.Count();
   string seedAfter;
   ACR_Options.Get("acrSeed", seedAfter);
   ACR_TestRegress.Check("setup preview: a new seed and the half size re-plan it", seedAfter != seedBefore && half > 0 && half < first, string.Format("seed %1 -> %2, %3 -> %4 bases", seedBefore, seedAfter, first, half));
   EditorChecks();
   Moves();
   s_PreviewNames.Clear();
   s_PreviewNames.Copy(ACR_Setup.s_PreviewNames);
   ACR_Setup.Start("the setup preview test");
  }
  if (second == 14)
  {
   array<string> built = {};
   foreach (ACR_Base base : ACR_Base.s_All) built.Insert(base.m_sName);
   int same;
   foreach (string planned : s_PreviewNames) if (built.Contains(planned)) same++;
   ACR_TestRegress.Check("setup preview: Start builds exactly the previewed layout", ACR_Generator.Built() && same == s_PreviewNames.Count() && built.Count() == s_PreviewNames.Count(), string.Format("%1 of %2 previewed, %3 built", same, s_PreviewNames.Count(), built.Count()));
   int ownedHQs;
   foreach (ACR_Base hq : ACR_Base.s_All) if (hq.m_sOwner != "") ownedHQs++;
   ACR_TestRegress.Check("setup preview: two HQs owned after Start", ownedHQs == 2, string.Format("%1 owned", ownedHQs));
   int assignedRoles, deploySites, reinforcementSites, settlements;
   foreach (ACR_Base logistical : ACR_Base.s_All)
   {
    if (logistical.m_eLogisticsRole != ACR_EBaseRole.AUTHORED) assignedRoles++;
    if (ACR_LogisticsRules.CanDeploy(logistical)) deploySites++;
    if (ACR_LogisticsRules.CanReinforce(logistical)) reinforcementSites++;
    if (logistical.m_eLogisticsRole == ACR_EBaseRole.SETTLEMENT) settlements++;
   }
   ACR_TestRegress.Check("generated logistics: every built site has an explicit role", assignedRoles == built.Count(), assignedRoles.ToString());
   ACR_TestRegress.Check("generated logistics: settlements reinforce beyond military deployment sites", settlements > 0 && deploySites >= 2 && reinforcementSites > deploySites && deploySites < built.Count(), string.Format("deploy %1 reinforce %2 settlements %3 total %4", deploySites, reinforcementSites, settlements, built.Count()));
   ACR_Base moved;
   foreach (ACR_Base b : ACR_Base.s_All) if (b.m_sName == s_sMoved) moved = b;
   float off = -1;
   if (moved) off = vector.DistanceXZ(moved.GetOrigin(), s_vMovedTo);
   ACR_TestRegress.Check("layout editor: Start builds the moved base where it was put", moved && off >= 0 && off < 2, string.Format("%1 %2 m from the chosen spot", s_sMoved, (int)off));
   ACR_TestRegress.Check("editor: deleted CP stays absent after Start", !built.Contains(s_sDeleted), s_sDeleted);
   ACR_TestRegress.Check("editor: deletion requests after Start are refused", ACR_Setup.Handle(999, "delete:" + s_sMoved).Contains("already started"), "setup closed");
   ACR_TestRegress.Complete("setupgen", GEN_CHECKS);
  }
 }

 protected static string s_sMoved;
 protected static string s_sDeleted;
 protected static vector s_vMovedTo;

 // Layout editor (aca-f07 step 2): moves refused and accepted, undo, kept for Start.
 protected static void Moves()
 {
  int seed = ACR_Generator.Seed();
  int size = ACR_Skirmish.SizeOption();
  array<ref ACR_GenSite> layout = {};
  array<ACR_GenSite> hqs = {};
  ACR_Generator.Plan(seed, size, layout, hqs);
  ACR_GenSite site;
  ACR_GenSite neighbour;
  foreach (ACR_GenSite s : layout)
  {
   if (hqs.Contains(s)) continue;
   if (!site) site = s;
   else if (!neighbour) neighbour = s;
  }
  if (!site || !neighbour || hqs.Count() != 2) { ACR_TestRegress.Abort("setupgen", "no layout to edit"); return; }
  string stranger = ACR_Setup.HandleMove(999, site.m_sName, site.m_vPos[0] + 300, site.m_vPos[2]);
  string reason;
  bool crowd = ACR_Generator.Move(site.m_sName, neighbour.m_vPos[0] + 5, neighbour.m_vPos[2], reason);
  ACR_TestRegress.Check("layout editor: a move onto another base and a stranger's move are refused", !crowd && reason.Contains("Too close") && stranger.Contains("Only the host"), reason + " / " + stranger);
  // An HQ next to the other HQ.
  bool hqRefused = false;
  string hqReason = "";
  for (int i = 0; i < 16 && !hqRefused; i++)
  {
   float angle = Math.PI2 * i / 16.0;
   string r;
   if (!ACR_Generator.Move(hqs[0].m_sName, hqs[1].m_vPos[0] + Math.Cos(angle) * 600, hqs[1].m_vPos[2] + Math.Sin(angle) * 600, r) && r.Contains("HQs")) { hqRefused = true; hqReason = r; }
  }
  ACR_TestRegress.Check("layout editor: HQs can't be moved close together", hqRefused && ACR_Generator.MoveCount() == 0, hqReason);
  // A valid spot near the base.
  bool accepted = false;
  vector to;
  for (int ring = 150; ring <= 1500 && !accepted; ring += 150)
  {
   for (int j = 0; j < 8 && !accepted; j++)
   {
    float a = Math.PI2 * j / 8.0;
    to = Vector(site.m_vPos[0] + Math.Cos(a) * ring, 0, site.m_vPos[2] + Math.Sin(a) * ring);
    accepted = ACR_Generator.Move(site.m_sName, to[0], to[2], reason);
   }
  }
  ACR_Setup.UpdatePreview();
  int index = PreviewAt(to);
  float shown = -1;
  if (index >= 0) shown = vector.DistanceXZ(ACR_Setup.s_PreviewPos[index], to);
  ACR_TestRegress.Check("layout editor: a valid move is taken and shown in the preview", accepted && shown >= 0 && shown < 2 && ACR_Setup.Rows().Contains("undo\tUndo layout edits (1)"), string.Format("%1; preview %2 m off", reason, (int)shown));
  ACR_Generator.ClearMoves();
  ACR_Setup.UpdatePreview();
  index = ACR_Setup.s_PreviewNames.Find(site.m_sName);
  float back = -1;
  if (index >= 0) back = vector.DistanceXZ(ACR_Setup.s_PreviewPos[index], site.m_vPos);
  ACR_TestRegress.Check("layout editor: undo puts it back", ACR_Generator.MoveCount() == 0 && back >= 0 && back < 2 && !ACR_Setup.Rows().Contains("undo"), string.Format("%1 m from its place", (int)back));
  // Again, for Start.
  ACR_Generator.Move(site.m_sName, to[0], to[2], reason);
  ACR_Setup.UpdatePreview();
  int movedIndex = PreviewAt(to);
  if (movedIndex >= 0) s_sMoved = ACR_Setup.s_PreviewNames[movedIndex];
  s_vMovedTo = to;
  ACR_Generator.Plan(seed, size, layout, hqs);
  foreach (ACR_GenSite remove : layout)
  {
   if (hqs.Contains(remove) || remove.m_sName == s_sMoved) continue;
   s_sDeleted = remove.m_sName;
   break;
  }
  bool deleted = ACR_Generator.Delete(s_sDeleted, reason);
  ACR_Setup.UpdatePreview();
  ACR_TestRegress.Check("editor: final deletion is reflected before Start", deleted && !ACR_Setup.s_PreviewNames.Contains(s_sDeleted), reason);
 }

 // Locate by position because a move may change its display name.
 protected static int PreviewAt(vector at)
 {
  foreach (int i, vector pos : ACR_Setup.s_PreviewPos)
   if (vector.DistanceXZ(at, pos) < 2) return i;
  return -1;
 }

 protected static void EditorChecks()
 {
  int seed = ACR_Generator.Seed();
  int size = ACR_Skirmish.SizeOption();
  array<ref ACR_GenSite> layout = {};
  array<ACR_GenSite> hqs = {};
  ACR_Generator.Plan(seed, size, layout, hqs);
  ACR_GenSite source, target;
  foreach (ACR_GenSite site : layout)
  {
   if (hqs.Contains(site)) continue;
   if (!source) source = site;
   else if (!target) target = site;
  }
  if (!source || !target) { ACR_TestRegress.Abort("setupgen", "no editable CP pair"); return; }
  int count = layout.Count();
  string peak = ACR_Generator.NameAt(Vector(5064, 0, 6340), "landmark");
  ACR_TestRegress.Check("editor: Pobedy hill names moved CPs even though hills are not generated bases", peak == WidgetManager.Translate("#AR-MapLocation_Cain_PikPobedy"), peak);
  string reason;
  string stranger = ACR_Setup.Handle(999, "delete:" + target.m_sName);
  bool hq = ACR_Generator.Delete(hqs[0].m_sName, reason);
  ACR_TestRegress.Check("editor: unauthorized deletion and HQ deletion refused", stranger.Contains("Only the host") && !hq && reason.Contains("HQs") && ACR_Generator.MoveCount() == 0, reason);
  bool deleted = ACR_Generator.Delete(target.m_sName, reason);
  ACR_Setup.UpdatePreview();
  ACR_TestRegress.Check("editor: deleting a CP removes only that preview marker", deleted && ACR_Setup.s_PreviewNames.Count() == count - 1 && !ACR_Setup.s_PreviewNames.Contains(target.m_sName), reason);
  bool moved = ACR_Generator.Move(source.m_sName, target.m_vPos[0], target.m_vPos[2], reason);
  ACR_Setup.UpdatePreview();
  int index = PreviewAt(target.m_vPos);
  string renamed;
  if (index >= 0) renamed = ACR_Setup.s_PreviewNames[index];
  string expected = ACR_Generator.NameAt(target.m_vPos, source.m_sKind);
  ACR_TestRegress.Check("editor: moved CP takes destination place name", moved && renamed == expected && renamed != source.m_sName, renamed + " / " + expected);
  bool again = ACR_Generator.Move(renamed, target.m_vPos[0], target.m_vPos[2], reason);
  ACR_Setup.UpdatePreview();
  ACR_TestRegress.Check("editor: renamed CP can be moved again without duplicate edits", again && PreviewAt(target.m_vPos) >= 0 && ACR_Generator.MoveCount() == 2, reason);
  ACR_Options.Set("acrSeed", (seed + 1).ToString());
  ACR_Setup.UpdatePreview();
  ACR_TestRegress.Check("editor: edits do not leak to another seed", ACR_Generator.MoveCount() == 0, "another seed");
  ACR_Options.Set("acrSeed", seed.ToString());
  ACR_Setup.UpdatePreview();
  ACR_TestRegress.Check("editor: returning to the seed restores its edits", ACR_Generator.MoveCount() == 2 && PreviewAt(target.m_vPos) >= 0, "original seed");
  bool removedRenamed = ACR_Generator.Delete(renamed, reason);
  ACR_Setup.UpdatePreview();
  ACR_TestRegress.Check("editor: renamed CP can be deleted", removedRenamed && ACR_Setup.s_PreviewNames.Count() == count - 2 && !ACR_Setup.s_PreviewNames.Contains(renamed), reason);
  ACR_Generator.ClearMoves();
  ACR_Setup.UpdatePreview();
  ACR_TestRegress.Check("editor: undo restores deleted and moved CPs", ACR_Generator.MoveCount() == 0 && ACR_Setup.s_PreviewNames.Count() == count && ACR_Setup.s_PreviewNames.Contains(source.m_sName) && ACR_Setup.s_PreviewNames.Contains(target.m_sName), "all edits cleared");
  string high = SCR_PlayerController.ACR_BaseCaption("Depot", "Industry, 3 RP", true);
  string hidden = SCR_PlayerController.ACR_BaseCaption("Peak [1]", "Unclaimed | +1 RP | Hidden", false);
  ACR_TestRegress.Check("editor: compact captions retain value and HQ without full details", high == "[CP III] Depot" && hidden == "[CP I] Peak" && SCR_PlayerController.ACR_BaseCaption("HQ", "HQ (US), 2 RP", true) == "[HQ] HQ", high + " / " + hidden);
 }

 static bool ReserveWanted() { return System.IsCLIParam("acrReserveTest"); }
 static void RunReserve(int second)
 {
  if (second != 12) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  int lent;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner.IsEmpty() && base.GetOrigin()[2] > 6650) { base.m_sOwner = "US"; lent++; }
  ACR_AICommander ai = new ACR_AICommander(manager, "US");
  ACR_Base at = ai.ReserveBase();
  foreach (ACR_Base candidate : ACR_Base.s_All)
   if (ai.ReserveFits(candidate)) PrintFormat("[ACR] RESERVE candidate %1 value %2 radius %3 front %4 m", candidate.m_sName, candidate.m_iValue, candidate.m_fRadius, Math.Round(ai.FrontDistance(candidate)));
  string text = "none";
  if (at) text = string.Format("%1 (HQ %2, value %3, radius %4, %5 m from the front)", at.m_sName, ai.IsHQ(at), at.m_iValue, at.m_fRadius, Math.Round(ai.FrontDistance(at)));
  ACR_TestRegress.Check("reserve: Everon reserve at a secure non-HQ base", at && !ai.IsHQ(at) && ai.ReserveFits(at), string.Format("%1; %2 bases lent", text, lent));
  ACR_TestRegress.Complete("reserve", 1);
 }
}

// Inspect the actual native marker widget, not just the caption-producing string.
modded class SCR_PlayerController
{
 bool ACR_TestBaseHover()
 {
  if (m_ACR_BaseMarkers.IsEmpty()) return false;
  foreach (ACR_UnitMarker marker : m_ACR_BaseMarkers)
  {
   Widget root = marker.GetRootWidget();
   if (!root) return false;
   SCR_MapMarkerWidgetComponent component = marker.GetMarkerComponent();
   TextWidget text = TextWidget.Cast(root.FindAnyWidget("MarkerText"));
   if (!component || !text || marker.m_BaseHover == marker.m_BaseCaption) return false;
   component.OnMouseEnter(root, 0, 0);
   if (!text.IsVisible() || text.GetText() != marker.m_BaseHover) return false;
   // A changed feed while hovering must retain the expanded view.
   string saved = marker.m_BaseHover;
   marker.m_BaseHover += "\nupdated";
   marker.RefreshStyle();
   bool updated = text.GetText() == marker.m_BaseHover;
   marker.m_BaseHover = saved;
   marker.RefreshStyle();
   component.OnMouseLeave(root, null, 0, 0);
   if (!updated || text.GetText() != marker.m_BaseCaption || !text.IsVisible()) return false;
  }
  return true;
 }

 bool ACR_TestBaseRecreate()
 {
  if (m_ACR_BaseMarkers.IsEmpty()) return false;
  ACR_UnitMarker marker = m_ACR_BaseMarkers[0];
  Widget oldRoot = marker.GetRootWidget();
  if (!oldRoot) return false;
  marker.OnCreateMarker(true);
  Widget root = marker.GetRootWidget();
  if (!root || root == oldRoot || oldRoot.GetParent()) return false;
  int pos[2];
  marker.GetWorldPos(pos);
  marker.SetWorldPos(pos[0] + 120, pos[1] + 80);
  int screenX, screenY;
  SCR_MapEntity.GetMapInstance().WorldToScreen(pos[0] + 120, pos[1] + 80, screenX, screenY, true);
  vector actual = FrameSlot.GetPos(root);
  bool positioned = Math.AbsFloat(actual[0] - GetGame().GetWorkspace().DPIUnscale(screenX)) < 1 && Math.AbsFloat(actual[1] - GetGame().GetWorkspace().DPIUnscale(screenY)) < 1;
  marker.SetWorldPos(pos[0], pos[1]);
  return positioned;
 }

 bool ACR_TestBaseCaptionsVisible(bool mouseLeave = false)
 {
  if (m_ACR_BaseMarkers.IsEmpty()) return false;
  foreach (ACR_UnitMarker marker : m_ACR_BaseMarkers)
  {
   Widget root = marker.GetRootWidget();
   if (!root) return false;
   SCR_MapMarkerWidgetComponent component = SCR_MapMarkerWidgetComponent.Cast(root.FindHandler(SCR_MapMarkerWidgetComponent));
   if (!component) return false;
   if (mouseLeave) component.OnMouseLeave(root, null, 0, 0);
   TextWidget caption = TextWidget.Cast(root.FindAnyWidget("MarkerText"));
   // A base under the real mouse cursor (someone focused the game window) shows its full
   // details: that is the hover working, not a hidden caption (cc88map4, 28 Oct).
   string wanted = marker.m_BaseCaption;
   if (component.ACR_Hovered()) wanted = marker.m_BaseHover;
   if (!caption || !caption.IsVisible() || caption.GetText() != wanted)
   {
    string shown = "no caption";
    if (caption) shown = string.Format("visible %1 text '%2'", caption.IsVisible(), caption.GetText());
    PrintFormat("[ACR] TEST caption hidden: wanted '%1', %2, root visible %3", wanted, shown, root.IsVisible());
    return false;
   }
  }
  return true;
 }
}
