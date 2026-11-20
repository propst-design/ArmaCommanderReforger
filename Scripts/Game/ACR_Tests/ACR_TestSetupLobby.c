// -acrSetupLobbyTest 1 (server, Arland, ~30 s): the setup lobby (0.7.0, ACR_SetupLobby).
// The server half of the new screen, checkable without a player: every row line carries its lobby
// tab, parent and flags; rows sit on the agreed tabs; a change sets the "changed" dot and going back
// clears it; the left arrow steps back; rows under a switched-off parent are hidden; the seed can be
// typed; Reset tab drops only that tab's screen choices; strangers can't use the new requests; every
// option row has help for the right panel; and the lobby's layout and dialog preset load (the layout
// is also built where the server has a workspace). What it can't see: how the screen looks.
class ACR_TestSetupLobby
{
 static const int CHECKS = 16;
 protected static const ResourceName LAYOUT = "{AC230926A00100F6}UI/layouts/ACR/ACR_SetupLobby.layout";
 protected static const ResourceName SPIN = "{C9DF0E6590F6C388}UI/layouts/WidgetLibrary/SpinBox/WLib_SpinBox.layout";

 static bool Wanted() { return System.IsCLIParam("acrSetupLobbyTest"); }

 // The row line of a key, split at tabs (empty when the key has no line).
 protected static array<string> Fields(string rows, string key)
 {
  array<string> lines = {};
  rows.Split("\n", lines, true);
  foreach (string line : lines)
  {
   if (!line.StartsWith(key + "\t")) continue;
   array<string> parts = {};
   line.Split("\t", parts, false);
   return parts;
  }
  return {};
 }

 protected static string Field(string key, int index)
 {
  array<string> parts = Fields(ACR_Setup.Rows(), key);
  if (index >= parts.Count()) return "<none>";
  return parts[index];
 }

 protected static bool Flag(string key, string flag)
 {
  return Field(key, 4).Contains(flag);
 }

 protected static bool ShownOn(int tab, string key)
 {
  array<ref ACR_SetupLobbyRow> rows = {};
  ACR_SetupLobbyModel.Parse(ACR_Setup.Rows(), rows);
  array<ACR_SetupLobbyRow> shown = {};
  ACR_SetupLobbyModel.Shown(rows, tab, shown);
  foreach (ACR_SetupLobbyRow row : shown) if (row.m_sKey == key) return true;
  return false;
 }

 static void Run(int second)
 {
  if (second != 6) return;
  ACR_Options.ClearSet();
  string rows = ACR_Setup.Rows();

  // 1. The protocol: five fields on every row line but the presets' own; the sides' keys for the map.
  array<string> lines = {};
  rows.Split("\n", lines, true);
  int lobbyLines = 0;
  string shortLines;
  foreach (string line : lines)
  {
   array<string> parts = {};
   line.Split("\t", parts, false);
   if (parts.IsEmpty() || parts[0].StartsWith("preset") || parts[0].StartsWith("lobby")) continue;
   if (parts.Count() == 5) lobbyLines++;
   else shortLines += parts[0] + " ";
  }
  ACR_TestRegress.Check("lobby: every row line carries its tab, parent and flags", lobbyLines > 40 && shortLines.IsEmpty() && rows.Contains("lobbysides\tUS,USSR"), string.Format("%1 lines; short: %2", lobbyLines, shortLines));

  // 2. The agreed tabs, read from the lines themselves.
  string wrong;
  array<string> match = {"west", "east", "quickstart", "ai", "startrp"};
  foreach (string key : match) if (Field(key, 2) != "1") wrong += key + " ";
  if (Field("layout", 2) != "0" || Field("threat", 2) != "0" || Field("guerrillas", 2) != "0" || Field("pockets", 2) != "0") wrong += "battlefield ";
  if (Field("aicommander", 2) != "2" || Field("doctrinewest", 2) != "2" || Field("unitcap", 2) != "2" || Field("mines", 2) != "2" || Field("fortify", 2) != "2") wrong += "forces ";
  if (Field("decks", 2) != "3" || Field("deployment", 2) != "3" || Field("pricing", 2) != "3") wrong += "economy ";
  if (Field("shared", 2) != "4" || Field("declutter", 2) != "4" || Field("intro", 2) != "4") wrong += "command ";
  if (Field("supplyruns", 2) != "5") wrong += "experimental ";
  if (ACR_Setup.LOBBY_FIRST != 1 || ACR_Setup.LobbyTabName(ACR_Setup.LOBBY_FIRST) != "Match") wrong += "first ";
  if (ACR_SetupLobbyModel.Kind("presetrow:Default", "Default (built-in)") != "preset" || ACR_Setup.LobbyTab("presetrow:Default") != ACR_Setup.LOBBY_PRESETS) wrong += "presets ";
  ACR_TestRegress.Check("lobby: rows sit on the agreed tabs (opens on Match)", wrong.IsEmpty(), wrong);

  // 3. No tab is empty, and only the Match rows land on Match (an unsorted row would fall there).
  array<ref ACR_SetupLobbyRow> parsed = {};
  ACR_SetupLobbyModel.Parse(rows, parsed);
  string empty, stray;
  for (int tab = 0; tab < ACR_Setup.LOBBY_PRESETS; tab++)
  {
   array<ACR_SetupLobbyRow> shown = {};
   ACR_SetupLobbyModel.Shown(parsed, tab, shown);
   if (shown.IsEmpty()) empty += ACR_Setup.LobbyTabName(tab) + " ";
  }
  array<string> matchKeys = {"west", "east", "quickstart", "ai", "startrp"};
  array<string> allKeys = ACR_Setup.RowKeys();
  foreach (string rowKey : allKeys) if (ACR_Setup.LobbyTab(rowKey) == ACR_Setup.LOBBY_FIRST && !matchKeys.Contains(rowKey)) stray += rowKey + " ";
  ACR_TestRegress.Check("lobby: every tab has rows and no row falls onto Match unsorted", empty.IsEmpty() && stray.IsEmpty(), "empty: " + empty + "| stray: " + stray);

  // 4. Dependent rows name the row they sit under.
  bool parents = Field("fiaallegiance", 3) == "guerrillas" && Field("minesai", 3) == "mines" && Field("baseworks", 3) == "fortify" && Field("servicecost", 3) == "fobservices" && Field("deckpoints", 3) == "decks" && Field("aboutdoctrinewest", 3) == "doctrinewest" && Field("threat", 3) == "";
  ACR_TestRegress.Check("lobby: dependent rows name their parent", parents, string.Format("fia %1, minesai %2, baseworks %3, servicecost %4, deckpoints %5", Field("fiaallegiance", 3), Field("minesai", 3), Field("baseworks", 3), Field("servicecost", 3), Field("deckpoints", 3)));

  // 5. The changed dot: a change marks the row (and its tab); the left arrow back clears it.
  bool cleanAtStart = !Flag("threat", "c");
  ACR_Setup.Act(0, "threat");
  bool marked = Flag("threat", "c") && ACR_Setup.Current("threat") == "high";
  ACR_SetupLobbyModel.Parse(ACR_Setup.Rows(), parsed);
  bool tabMarked = ACR_SetupLobbyModel.ChangedOn(parsed, ACR_Setup.LobbyTab("threat")) > 0;
  string back = ACR_Setup.Act(0, "prev:threat");
  bool cleared = !Flag("threat", "c") && ACR_Setup.Current("threat") == "normal";
  ACR_TestRegress.Check("lobby: a change marks the row and its tab, going back clears it", cleanAtStart && marked && tabMarked && cleared, string.Format("clean %1, marked %2, tab %3, back '%4' cleared %5", cleanAtStart, marked, tabMarked, back, cleared));

  // 6. The left arrow steps back through the values and wraps.
  ACR_Setup.Act(0, "prev:unitcap");
  string capLast = ACR_Setup.Current("unitcap");
  ACR_Setup.Act(0, "prev:unitcap");
  string capBefore = ACR_Setup.Current("unitcap");
  ACR_TestRegress.Check("lobby: the left arrow steps back and wraps", capLast == "24" && capBefore == "16", string.Format("%1 then %2", capLast, capBefore));

  // 7. Minefield orders off: its two rows are flagged hidden and the model leaves them out.
  ACR_Options.ClearSet();
  ACR_Setup.SetValue("mines", "0");
  bool hiddenOff = Flag("minesai", "h") && Flag("minesreal", "h") && !ShownOn(2, "minesai") && ShownOn(2, "mines");
  ACR_Setup.SetValue("mines", "1");
  bool shownOn = !Flag("minesai", "h") && ShownOn(2, "minesai") && ShownOn(2, "minesreal");
  ACR_TestRegress.Check("lobby: rows under a switched-off parent hide, and come back with it", hiddenOff && shownOn, string.Format("off hidden %1, on shown %2", hiddenOff, shownOn));

  // 8. The FIA rows matter with either kind of FIA: hidden only with pockets and guerrillas both off.
  ACR_Setup.SetValue("guerrillas", "off");
  bool pocketsKeep = ShownOn(0, "fiaallegiance");
  ACR_Setup.SetValue("pockets", "off");
  bool bothOff = !ShownOn(0, "fiaallegiance") && !ShownOn(0, "fiahidden") && ShownOn(0, "guerrillas");
  ACR_Setup.SetValue("guerrillas", "few");
  bool back2 = ShownOn(0, "fialeaning");
  ACR_TestRegress.Check("lobby: FIA rows hide only with pockets and guerrillas both off", pocketsKeep && bothOff && back2, string.Format("pockets keep %1, both off %2, back %3", pocketsKeep, bothOff, back2));

  // 9. The seed: typed, nonsense refused, never marked changed.
  ACR_Options.ClearSet();
  ACR_Setup.SetValue("layout", "gen:full");
  string typed = ACR_Setup.Act(0, "seedset:4321");
  string nonsense = ACR_Setup.Act(0, "seedset:abc");
  bool seedOk = ACR_Setup.Current("seed") == "4321" && typed.Contains("4321") && nonsense.Contains("whole number") && !Flag("seed", "c") && Field("seed", 3) == "layout";
  ACR_TestRegress.Check("lobby: the seed can be typed, nonsense is refused", seedOk, typed + " | " + nonsense);

  // 10. Reset tab: Match's screen choices go, the other tabs' stay.
  ACR_Options.ClearSet();
  ACR_Setup.Act(0, "threat");
  ACR_Setup.Act(0, "startrp");
  ACR_Setup.Act(0, "ai");
  ACR_Setup.Act(0, "mines");
  string reset = ACR_Setup.Act(0, "resettab:1");
  ACR_SetupLobbyModel.Parse(ACR_Setup.Rows(), parsed);
  bool resetOk = ACR_Options.Source("acrStartRP") != "screen" && ACR_Options.Source("acrAI") != "screen" && ACR_Options.Source("acrThreat") == "screen" && ACR_Options.Source("acrMines") == "screen" && ACR_SetupLobbyModel.ChangedOn(parsed, 1) == 0 && ACR_SetupLobbyModel.ChangedOn(parsed, 0) > 0 && ACR_SetupLobbyModel.ChangedOn(parsed, 2) > 0;
  ACR_TestRegress.Check("lobby: Reset tab drops only that tab's choices", resetOk, reset);
  ACR_Options.ClearSet();

  // 11. The new requests are host-only like the old ones.
  string strangerPrev = ACR_Setup.Handle(9999, "prev:threat");
  string strangerReset = ACR_Setup.Handle(9999, "resettab:1");
  string strangerSeed = ACR_Setup.Handle(9999, "seedset:5");
  ACR_TestRegress.Check("lobby: a stranger can't step, reset or type a seed", strangerPrev.Contains("Only the host") && strangerReset.Contains("Only the host") && strangerSeed.Contains("Only the host") && ACR_Setup.Current("threat") == "normal", strangerPrev);

  // 12. The lobby's summary reads the same rows as the classic dialog's; the classic tabs are untouched.
  rows = ACR_Setup.Rows();
  ACR_SetupLobbyModel.Parse(rows, parsed);
  string summary = ACR_SetupLobbyModel.Summary(parsed);
  string classic = ACR_SetupDialog.Summary(rows);
  ACR_TestRegress.Check("lobby: summary matches the classic dialog's; classic tabs unchanged", summary == classic && summary.Contains(" vs ") && ACR_SetupDialog.Tab("threat") == 0 && ACR_SetupDialog.Tab("aicommander") == 1, summary + " | " + classic);

  // 13. The right panel has help for every option row, two lines at most.
  string noHelp;
  int helped = 0;
  foreach (ACR_SetupLobbyRow row : parsed)
  {
   if (row.m_sKind != "value" && row.m_sKind != "seed" && row.m_sKind != "action") continue;
   string help = ACR_Setup.Help(row.m_sKey);
   array<string> helpLines = {};
   help.Split("\n", helpLines, false);
   if (help.IsEmpty() || helpLines.Count() > 2) noHelp += row.m_sKey + " ";
   else helped++;
  }
  ACR_TestRegress.Check("lobby: every row has help for the right panel (two lines at most)", noHelp.IsEmpty() && helped > 40, string.Format("%1 helped; missing or long: %2", helped, noHelp));

  // 14. The dialog preset: our layout, and Start on its own action (not Enter/A, which press rows).
  Resource container = BaseContainerTools.LoadContainer(ACR_SetupDialog.PRESETS);
  SCR_ConfigurableDialogUiPreset preset;
  if (container && container.IsValid())
  {
   SCR_ConfigurableDialogUiPresets presets = SCR_ConfigurableDialogUiPresets.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
   if (presets) preset = presets.FindPreset(ACR_SetupLobby.TAG);
  }
  bool presetOk = preset && preset.m_sLayout == LAYOUT && preset.m_aButtons && preset.m_aButtons.Count() == 1 && preset.m_aButtons[0].m_sTag == SCR_ConfigurableDialogUi.BUTTON_CONFIRM && preset.m_aButtons[0].m_sActionName == "MenuDeploy";
  string presetText = "no preset";
  if (preset) presetText = preset.m_sLayout;
  ACR_TestRegress.Check("lobby: the dialog preset uses the lobby layout and Start on MenuDeploy", presetOk, presetText);

  // 15. The layout loads, and builds its named parts where this server has a workspace.
  Resource layout = Resource.Load(LAYOUT);
  bool loaded = layout && layout.IsValid();
  WorkspaceWidget workspace = GetGame().GetWorkspace();
  if (!workspace)
  {
   if (loaded) ACR_TestRegress.Skip("lobby: the layout builds its named parts", "the layout loads; no workspace on this server to build it");
   else ACR_TestRegress.Check("lobby: the layout builds its named parts", false, "the layout resource didn't load");
  }
  else
  {
   Widget root = workspace.CreateWidgets(LAYOUT);
   string missing;
   array<string> names = {"Tabs", "Rows", "RowsScroll", "MapDots", "MapBack", "Backdrop", "FooterLeft", "ButtonsRight", "Title", "TitleHolder", "Decor", "Subtitle", "TabTitle", "TabTitleHolder", "Summary", "Help", "Status"};
   foreach (string name : names) if (!root || !root.FindAnyWidget(name)) missing += name + " ";
   bool spinOk = false;
   if (root && root.FindAnyWidget("Rows"))
   {
    Widget spin = workspace.CreateWidgets(SPIN, root.FindAnyWidget("Rows"));
    spinOk = spin && spin.FindHandler(SCR_SpinBoxComponent) != null;
    // The CRT skin's heading font layout builds too.
    Widget mono = workspace.CreateWidgets(ACR_SetupTheme.HEADING_MONO, root.FindAnyWidget("TitleHolder"));
    spinOk = spinOk && TextWidget.Cast(mono) != null;
   }
   if (root) root.RemoveFromHierarchy();
   ACR_TestRegress.Check("lobby: the layout builds its named parts", loaded && root && missing.IsEmpty() && spinOk, string.Format("loaded %1, root %2, missing: %3, spin %4", loaded, root != null, missing, spinOk));
  }
  // 16. The theme table: three skins, every colour set, plain words kept (upper case only in the CRT),
  // vanilla on a server without an era pack, tab names led by the paragraph number.
  string themeWrong;
  array<string> skins = {"vanilla", "crt", "tablet"};
  foreach (string skinName : skins)
  {
   ACR_SetupTheme theme = ACR_SetupTheme.Named(skinName);
   if (!theme || theme.m_sName != skinName) { themeWrong += skinName + ":name "; continue; }
   if (!theme.m_Backdrop || !theme.m_Panel || !theme.m_Heading || !theme.m_Decor || !theme.m_Subtitle || !theme.m_Text || !theme.m_TextDim || !theme.m_Help || !theme.m_Status || !theme.m_Changed) themeWrong += skinName + ":text ";
   if (!theme.m_Button || !theme.m_ButtonHover || !theme.m_ButtonPress || !theme.m_ButtonSelected || !theme.m_ButtonBorder) themeWrong += skinName + ":buttons ";
   if (!theme.m_West || !theme.m_East || !theme.m_Neutral || !theme.m_Other || !theme.m_WestPanel || !theme.m_EastPanel) themeWrong += skinName + ":sides ";
   if (theme.m_sChanged.IsEmpty()) themeWrong += skinName + ":mark ";
   if (!theme.m_sHeadingLayout.IsEmpty() && !Resource.Load(theme.m_sHeadingLayout).IsValid()) themeWrong += skinName + ":font ";
  }
  if (ACR_SetupTheme.Crt().Text("Starting RP") != "STARTING RP" || ACR_SetupTheme.Vanilla().Text("Starting RP") != "Starting RP") themeWrong += "case ";
  if (ACR_SetupTheme.Chosen() != "vanilla") themeWrong += "chosen:" + ACR_SetupTheme.Chosen() + " ";
  if (ACR_Setup.LobbyTabPrefix(0) != "1." || ACR_Setup.LobbyTabPrefix(1) != "2." || ACR_Setup.LobbyTabPrefix(5) != "A." || ACR_Setup.LobbyTabPrefix(6) != "B.") themeWrong += "prefix ";
  ACR_TestRegress.Check("lobby: three skins with every colour set, vanilla by default, plain tab names", themeWrong.IsEmpty(), themeWrong);
  ACR_TestRegress.Complete("setuplobby", CHECKS);
 }
}
