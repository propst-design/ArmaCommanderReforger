// -acrSetupLobbyTest 1 (server, Arland, ~30 s): the setup lobby (0.7.0, ACR_SetupLobby).
// The server half of the new screen, checkable without a player: every row line carries its lobby
// tab, parent and flags; rows sit on the agreed tabs; a change sets the "changed" dot and going back
// clears it; the left arrow steps back; rows under a switched-off parent are hidden; the seed can be
// typed; Reset tab drops only that tab's screen choices; strangers can't use the new requests; every
// option row has help for the right panel; and the lobby's layout and dialog preset load (the layout
// is also built where the server has a workspace). What it can't see: how the screen looks.
class ACR_TestSetupLobby
{
 static const int CHECKS = 23;
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

 // 15b, by itself (the check's function has a 64-local limit).
 protected static string Devices(WorkspaceWidget workspace)
 {
  string missing;
  // 15b. The device skins' structure: an opaque screen in a bezel, the status bar with its words,
  // the rules and the rail line; vanilla keeps the see-through page.
  array<string> skinNames = {"vanilla", "tablet", "crt"};
  foreach (string skinKey : skinNames)
  {
   ACR_SetupTheme look = ACR_SetupTheme.Named(skinKey);
   Widget themed = workspace.CreateWidgets(LAYOUT);
   if (!themed) { missing += skinKey + ":build "; continue; }
   ACR_SetupLobby.Structure(themed, look);
   bool device = look.m_bDevice;
   Widget screen = themed.FindAnyWidget("Screen");
   Widget bar = themed.FindAnyWidget("StatusBar");
   Widget leftWords = themed.FindAnyWidget("StatusLeft");
   bool built = screen && screen.IsVisible() && bar && bar.IsVisible() && themed.FindAnyWidget("TopRule").IsVisible() && themed.FindAnyWidget("FooterRule").IsVisible() && leftWords && TextWidget.Cast(leftWords.GetChildren()) && themed.FindAnyWidget("Backdrop").GetColor().A() >= 0.999;
   bool hidden = screen && !screen.IsVisible() && bar && !bar.IsVisible() && !themed.FindAnyWidget("RailLine").IsVisible();
   if (device && !built) missing += skinKey + ":device ";
   if (!device && !hidden) missing += skinKey + ":plain ";
   if (device && look.m_bRail && !themed.FindAnyWidget("RailLine").IsVisible()) missing += skinKey + ":rail ";
   themed.RemoveFromHierarchy();
  }
  if (!ACR_SetupTheme.Tablet().m_bDevice || !ACR_SetupTheme.Crt().m_bDevice || ACR_SetupTheme.Vanilla().m_bDevice) missing += "devices ";
  return missing;
 }

 protected static void Tooltips(notnull array<ref ACR_SetupLobbyRow> parsed)
 {
  array<string> footer = {};
  // 21. Themed wording (Jacob, 20 Nov): the tablet and terminal may say SITREP, EXEC, Execute, Area
  // of ops... but every themed tab, row and button has a plain tooltip; vanilla keeps plain labels.
  string tipWrong;
  array<string> devices = {"tablet", "crt"};
  foreach (string deviceName : devices)
  {
   ACR_SetupTheme skin = ACR_SetupTheme.Named(deviceName);
   if (skin.StartLabel() != "Execute") tipWrong += deviceName + ":execute ";
   for (int t = 0; t < ACR_Setup.LOBBY_TABS; t++)
   {
    string tip = ACR_SetupLobbyModel.Tooltip(parsed, "tab:" + t.ToString());
    if (skin.TabLabel(t).IsEmpty() || !tip.StartsWith(ACR_Setup.LobbyTabName(t) + ": ") || tip.Length() < 20) tipWrong += string.Format("%1:tab%2 ", deviceName, t);
   }
   foreach (ACR_SetupLobbyRow tipRow : parsed)
   {
    if (tipRow.m_sKind != "value" && tipRow.m_sKind != "seed" && tipRow.m_sKind != "action") continue;
    string rowTip = ACR_SetupLobbyModel.Tooltip(parsed, tipRow.m_sKey);
    if (rowTip.IsEmpty() || rowTip.Contains("\n")) tipWrong += deviceName + ":" + tipRow.m_sKey + " ";
    // A renamed row's tooltip names it plainly.
    else if (!skin.RowLabel(tipRow.m_sKey).IsEmpty() && !rowTip.StartsWith(tipRow.m_sLabel + ": ")) tipWrong += deviceName + ":plain:" + tipRow.m_sKey + " ";
   }
   array<string> buttons = {};
   ACR_SetupLobbyModel.FooterKeys(true, buttons);
   ACR_SetupLobbyModel.FooterKeys(false, footer);
   buttons.InsertAll(footer);
   foreach (string buttonKey : buttons) if (ACR_SetupLobbyModel.Tooltip(parsed, buttonKey).Length() < 20) tipWrong += deviceName + ":" + buttonKey + " ";
  }
  ACR_SetupTheme plainSkin = ACR_SetupTheme.Vanilla();
  if (!plainSkin.TabLabel(1).IsEmpty() || !plainSkin.RowLabel("layout").IsEmpty() || plainSkin.StartLabel() != "Start match" || !plainSkin.TabHeading(1).IsEmpty()) tipWrong += "vanilla:themed ";
  if (!ACR_SetupLobbyModel.Tooltip(parsed, "footerstart").StartsWith("Start the match")) tipWrong += "execute:plain ";
  ACR_TestRegress.Check("lobby: themed tabs, rows and buttons all have a plain tooltip; vanilla stays plain", tipWrong.IsEmpty(), tipWrong);
 }

 protected static void MapSketch()
 {
  // 17. The map sketch: the planned layout names both HQs (the world's own bases too), drawn at 18
  // against 10 for a base and 6 for a hidden site (20 Nov: every HQ came out at 10).
  ACR_Setup.UpdatePreview();
  array<string> details = {};
  ACR_Setup.PreviewDetails(details);
  int hqMarks = 0;
  int bigOthers = 0;
  foreach (string detail : details)
  {
   int size = ACR_SetupLobbyModel.MarkerSize(detail);
   if (size == 18) hqMarks++;
   else if (size > 10) bigOthers++;
  }
  bool sizesOk = ACR_SetupLobbyModel.MarkerSize("HQ (US), 2 RP") == 18 && ACR_SetupLobbyModel.MarkerSize("Control point, 1 RP | HQ") == 18 && ACR_SetupLobbyModel.MarkerSize("Control point, 1 RP") == 10 && ACR_SetupLobbyModel.MarkerSize("Control point, no income | Hidden") == 6;
  string firstDetail = "none";
  if (!details.IsEmpty()) firstDetail = details[0];
  ACR_TestRegress.Check("lobby: the map sketch draws both HQs bigger than the bases", hqMarks == 2 && bigOthers == 0 && sizesOk, string.Format("%1 HQs of %2 bases (%3, layout %4); sizes ok %5", hqMarks, details.Count(), firstDetail, ACR_Setup.Current("layout"), sizesOk));

 }

 // 22 (20 Nov, item 8): text fits its control. The server lays nothing out (sizes read 0), but it
 // measures text: a row's and a button's label (descenders included) against the height the device
 // skins give them, and the tooltip's wrapped text against its box.
 protected static void TextFits()
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  if (!ws)
  {
   ACR_TestRegress.Skip("lobby: labels fit their rows and buttons; the tooltip's text fits its box", "no workspace on this server");
   return;
  }
  string wrong;
  Widget root = ws.CreateWidgets(LAYOUT);
  Widget rows = root.FindAnyWidget("Rows");
  float tw, th;
  array<string> skins = {"tablet", "crt"};
  foreach (string skinName : skins)
  {
   ACR_SetupTheme theme = ACR_SetupTheme.Named(skinName);
   // A row: the library's label at the row's font, and the monospace copy the terminal shows.
   Widget spin = ws.CreateWidgets(SPIN, rows);
   ACR_SetupLobby.SpinLook(spin, theme);
   SizeLayoutWidget rowSize = SizeLayoutWidget.Cast(spin.FindAnyWidget("SizeLayout"));
   TextWidget label = TextWidget.Cast(spin.FindAnyWidget("Text"));
   if (label)
   {
    label.SetText(theme.Text("Blue force gjpqy"));
    label.GetTextSize(tw, th);
    if (th <= 0 || th + 12 > ACR_SetupLobby.FitHeight(16)) wrong += string.Format("%1:row text %2 in %3 ", skinName, th, ACR_SetupLobby.FitHeight(16));
   }
   if (!rowSize) wrong += skinName + ":rowsize ";
   TextWidget mono = TextWidget.Cast(ws.CreateWidgets(ACR_SetupTheme.HEADING_MONO, rows));
   ACR_SetupLobby.MonoFont(mono, 15);
   mono.SetText("A. BLUE FORCE gjpqy");
   mono.GetTextSize(tw, th);
   if (th <= 0 || th + 12 > ACR_SetupLobby.FitHeight(16)) wrong += string.Format("%1:mono text %2 ", skinName, th);
   // A button: "Execute" at the device font, in the height it gets.
   Widget button = ws.CreateWidgets("{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout", rows);
   SCR_ButtonTextComponent text = SCR_ButtonTextComponent.FindButtonTextComponent(button);
   if (text && text.GetTextWidget())
   {
    text.SetText(theme.StartLabel() + " gjpqy");
    text.GetTextWidget().SetExactFontSize(16);
    text.GetTextWidget().GetTextSize(tw, th);
    if (th <= 0 || th + 12 > ACR_SetupLobby.FitHeight(16)) wrong += string.Format("%1:button text %2 ", skinName, th);
   }
  }
  // The tooltip: wrapped lines no longer than the width allows, and the box tall and wide enough for
  // the text as measured, for the longest tooltip there is.
  array<ref ACR_SetupLobbyRow> parsed = {};
  ACR_SetupLobbyModel.Parse(ACR_Setup.Rows(), parsed);
  string longest = ACR_SetupLobbyModel.Tooltip(parsed, "tab:0");
  foreach (ACR_SetupLobbyRow row : parsed)
  {
   string tip = ACR_SetupLobbyModel.Tooltip(parsed, row.m_sKey);
   if (tip.Length() > longest.Length()) longest = tip;
  }
  float charWidth, lineHeight, linePitch;
  ACR_SetupLobby.MonoMetrics(14, charWidth, lineHeight, linePitch);
  int perLine = ACR_SetupLobbyModel.TipChars(420 - 18, charWidth);
  int lines;
  string wrapped = ACR_SetupLobbyModel.Wrap(longest, perLine, lines);
  array<string> pieces = {};
  wrapped.Split("\n", pieces, false);
  foreach (string piece : pieces) if (piece.Length() > perLine) wrong += "tip:line ";
  TextWidget tipText = TextWidget.Cast(ws.CreateWidgets(ACR_SetupTheme.HEADING_MONO, rows));
  ACR_SetupLobby.MonoFont(tipText, 14);
  tipText.SetText(wrapped);
  tipText.GetTextSize(tw, th);
  float boxHigh = lineHeight + (lines - 1) * linePitch + 2;
  if (tw <= 0 || tw > 420 - 18 + 1 || th > boxHigh + 1) wrong += string.Format("tip:box text %1x%2 box %3x%4 lines %5 scale %6 ", tw, th, 420 - 18, boxHigh, lines, ACR_SetupLobby.MonoScale());
  // The tooltip shows and hides on the right events (the state the screen uses).
  ACR_SetupLobbyTipState state = new ACR_SetupLobbyTipState();
  state.Hover("tab:1", true);
  state.Focus("tab:1", true);
  bool shown = state.Shown() == "tab:1";
  state.Hover("tab:1", false);
  bool leftEmpty = state.Shown() == "";
  state.Hover("west", true);
  state.Focus("tab:1", false);
  bool swapped = state.Shown() == "west";
  state.Hover("east", true);
  state.Hover("west", false);
  bool clean = state.Shown() == "east";
  state.Hover("east", false);
  state.Focus("ai", true);
  bool keyboard = state.Shown() == "ai";
  state.Clear();
  if (!shown || !leftEmpty || !swapped || !clean || !keyboard || state.Shown() != "") wrong += string.Format("tip:state %1%2%3%4%5 ", shown, leftEmpty, swapped, clean, keyboard);
  root.RemoveFromHierarchy();
  ACR_TestRegress.Check("lobby: labels fit their rows and buttons; the tooltip's text fits its box", wrong.IsEmpty(), wrong);
 }

 // 23 (20 Nov): every tooltip's text, as the screen wraps it, fits its box as the server measures it:
 // every tab, row and footer button, in the box widths a tooltip gets (the rail's and the full one),
 // as written and in capitals, with room to spare so the text widget never wraps a line again.
 protected static void TipsFit()
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  if (!ws)
  {
   ACR_TestRegress.Skip("lobby: every tooltip's text fits its box", "no workspace on this server");
   return;
  }
  array<ref ACR_SetupLobbyRow> parsed = {};
  ACR_SetupLobbyModel.Parse(ACR_Setup.Rows(), parsed);
  array<string> keys = {};
  for (int t = 0; t < ACR_Setup.LOBBY_TABS; t++) keys.Insert("tab:" + t.ToString());
  foreach (ACR_SetupLobbyRow row : parsed) if (row.m_sKind == "value" || row.m_sKind == "seed" || row.m_sKind == "action") keys.Insert(row.m_sKey);
  array<string> footer = {};
  ACR_SetupLobbyModel.FooterKeys(true, footer);
  keys.InsertAll(footer);
  keys.Insert("footerdecks");
  keys.Insert("presetsave");
  TextWidget probe = TextWidget.Cast(ws.CreateWidgets(ACR_SetupTheme.HEADING_MONO));
  ACR_SetupLobby.MonoFont(probe, ACR_SetupLobby.TIP_FONT);
  array<float> widths = {ACR_SetupLobby.TIP_WIDTH, 220};
  string over;
  int checked = 0;
  float worst = 0;
  foreach (string key : keys)
  {
   string text = ACR_SetupLobbyModel.Tooltip(parsed, key);
   for (int caps = 0; caps < 2; caps++)
   {
    if (caps == 1) text.ToUpper();
    foreach (float boxWide : widths)
    {
     string wrapped;
     int lines;
     float boxHigh = ACR_SetupLobby.TipBox(text, boxWide, wrapped, lines);
     probe.SetText(wrapped);
     float tw, th;
     probe.GetTextSize(tw, th);
     tw = ws.DPIUnscale(tw);
     th = ws.DPIUnscale(th);
     float roomW = boxWide - 2 * ACR_SetupLobby.TIP_PAD;
     float roomH = boxHigh - 2 * ACR_SetupLobby.TIP_PAD - 2;
     checked++;
     worst = Math.Max(worst, th / Math.Max(roomH, 1));
     // 5 % to spare across: a screen whose scale differs a little must not wrap a line again.
     if (tw * 1.05 > roomW || th > roomH + 0.5) over += string.Format("%1%2@%3 (%4x%5 in %6x%7, %8 lines) ", key, caps, boxWide, tw, th, roomW, roomH, lines);
    }
   }
  }
  probe.RemoveFromHierarchy();
  ACR_TestRegress.Check("lobby: every tooltip's text fits its box", over.IsEmpty() && checked > 100, string.Format("%1 checked, worst height %2; over: %3", checked, worst, over));
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
  if (Field("shared", 2) != "4" || Field("declutter", 2) != "4" || Field("intro", 2) != "4" || Field("twoic", 2) != "4") wrong += "command ";
  if (Field("pullover", 2) != "2" || Field("fieldsoffire", 2) != "2" || Field("periodjets", 2) != "2") wrong += "promoted ";
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
   array<string> names = {"Tabs", "Rows", "RowsScroll", "MapDots", "MapBack", "Backdrop", "FooterLeft", "ButtonsRight", "Title", "TitleHolder", "Decor", "Subtitle", "TabTitle", "TabTitleHolder", "Summary", "Help", "Status", "Screen", "ScreenEdge", "StatusBar", "StatusBack", "StatusLeft", "StatusRight", "TopRule", "FooterRule", "RailLine"};
   foreach (string name : names) if (!root || !root.FindAnyWidget(name)) missing += name + " ";
   bool spinOk = false;
   if (root && root.FindAnyWidget("Rows"))
   {
    Widget spin = workspace.CreateWidgets(SPIN, root.FindAnyWidget("Rows"));
    spinOk = spin && spin.FindHandler(SCR_SpinBoxComponent) != null;
    // The parts of the widget library's spin box the device skins restyle by name (20 Nov).
    if (spin)
    {
     array<string> parts = {"SizeLayout", "Background", "LineHighlight", "ButtonLeft", "ButtonRight", "SelectionText", "BackgroundImage", "Panel"};
     foreach (string part : parts) if (!spin.FindAnyWidget(part)) missing += "spin:" + part + " ";
    }
    // The CRT skin's heading font layout builds too.
    Widget mono = workspace.CreateWidgets(ACR_SetupTheme.HEADING_MONO, root.FindAnyWidget("TitleHolder"));
    spinOk = spinOk && TextWidget.Cast(mono) != null;
    // A device skin's row: the value and its arrows moved into a thin bordered box (an overlay).
    Widget boxed = workspace.CreateWidgets(SPIN, root.FindAnyWidget("Rows"));
    if (boxed)
    {
     ACR_SetupLobby.SpinLook(boxed, ACR_SetupTheme.Tablet());
     Widget content = boxed.FindAnyWidget("ContentLayout");
     if (!content || !OverlayWidget.Cast(content.GetParent()) || !boxed.FindHandler(SCR_SpinBoxComponent)) missing += "valuebox ";
    }
   }
   if (root) root.RemoveFromHierarchy();
   missing += Devices(workspace);
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
   // Button labels readable in every state, the open tab and Start too: WCAG AA 4.5:1 (20 Nov:
   // 1.6-3.5:1 on the tablet).
   if (!theme.m_ButtonText || !theme.m_ButtonSelectedText || !theme.m_Start || !theme.m_StartHover || !theme.m_StartText || !theme.m_Bezel || !theme.m_Screen || !theme.m_Line || !theme.m_StatusBack) themeWrong += skinName + ":device ";
   else if (theme.m_Button && theme.m_ButtonHover && theme.m_ButtonPress && theme.m_ButtonSelected)
   {
    float worst = Math.Min(Math.Min(theme.LabelContrast(theme.m_Button, theme.m_ButtonText), theme.LabelContrast(theme.m_ButtonHover, theme.m_ButtonText)), Math.Min(theme.LabelContrast(theme.m_ButtonPress, theme.m_ButtonText), theme.LabelContrast(theme.m_ButtonSelected, theme.m_ButtonSelectedText)));
    worst = Math.Min(worst, Math.Min(theme.LabelContrast(theme.m_Start, theme.m_StartText), theme.LabelContrast(theme.m_StartHover, theme.m_StartText)));
    if (worst < 4.5) themeWrong += string.Format("%1:contrast %2 ", skinName, worst);
   }
  }
  if (ACR_SetupTheme.Crt().Text("Starting RP") != "STARTING RP" || ACR_SetupTheme.Vanilla().Text("Starting RP") != "Starting RP") themeWrong += "case ";
  if (ACR_SetupTheme.Chosen() != "vanilla") themeWrong += "chosen:" + ACR_SetupTheme.Chosen() + " ";
  // The skin in use is the one the socket's era picks now (no stale cache after a reset).
  ACR_SetupTheme.Reset();
  if (ACR_SetupTheme.Current().m_sName != ACR_SetupTheme.Chosen()) themeWrong += "cached:" + ACR_SetupTheme.Current().m_sName + " ";
  if (ACR_Setup.LobbyTabPrefix(0) != "1." || ACR_Setup.LobbyTabPrefix(1) != "2." || ACR_Setup.LobbyTabPrefix(5) != "A." || ACR_Setup.LobbyTabPrefix(6) != "B.") themeWrong += "prefix ";
  ACR_TestRegress.Check("lobby: three skins with every colour set, vanilla by default, plain tab names", themeWrong.IsEmpty(), themeWrong);

  MapSketch();

  // 18. The right panel follows the focus: the focused row's help, not a tab's the pointer rests
  // on; the open tab's description only when nothing has the focus (20 Nov: Starting RP focused
  // showed "Economy and logistics: ...").
  ACR_SetupLobbyHelp focus = new ACR_SetupLobbyHelp();
  focus.Focus("startrp");
  string rowHelp = ACR_SetupLobbyModel.HelpFor(parsed, focus.Key(1));
  focus.Blur("tab:3");
  bool kept = focus.Key(1) == "startrp";
  focus.Blur("startrp");
  string tabHelp = ACR_SetupLobbyModel.HelpFor(parsed, focus.Key(1));
  bool helpOk = rowHelp.Contains("Requisition points") && rowHelp.StartsWith("Starting RP: ") && !rowHelp.Contains("Economy") && kept && tabHelp.StartsWith("Match: ") && !ACR_SetupLobbyModel.HelpFor(parsed, "footerstart").IsEmpty();
  ACR_TestRegress.Check("lobby: the help shows the focused row, the tab only when nothing is focused", helpOk, rowHelp + " | " + tabHelp);

  // 19. The footer: picker, Load preset, Reset tab, then Start match as a real button (its key
  // badge right of it); a player who can't change the setup gets their deck builder.
  array<string> footer = {};
  ACR_SetupLobbyModel.FooterKeys(true, footer);
  string hostFooter = "";
  foreach (string footerKey : footer) hostFooter += footerKey + " ";
  ACR_SetupLobbyModel.FooterKeys(false, footer);
  string guestFooter = "";
  foreach (string guestKey : footer) guestFooter += guestKey + " ";
  ACR_TestRegress.Check("lobby: footer order picker, load, reset, start; guests get the deck builder", hostFooter == "footerpreset footerload footerreset footerstart " && guestFooter == "footerdecks ", hostFooter + "| " + guestFooter);

  // 20. Preset wording: the right panel says which preset is loaded, the picker which one Load
  // preset would load; "none" stays none (the server's note line is unchanged for the classic dialog).
  array<ref ACR_SetupLobbyRow> fake = {};
  ACR_SetupLobbyModel.Parse("presetnote\tPreset in use: Large battle\npresetrow:Large battle\tLarge battle (built-in)", fake);
  string loadedLine = ACR_SetupLobbyModel.LoadedLine(fake);
  string noneLine = ACR_SetupLobbyModel.LoadedLine(parsed);
  bool presetWords = ACR_SetupLobbyModel.LoadedPreset(fake) == "Large battle" && loadedLine == "Loaded preset: Large battle" && ACR_SetupLobbyModel.LoadedPreset(parsed) == "" && noneLine == "Loaded preset: none (the scenario's own setup)";
  ACR_TestRegress.Check("lobby: the loaded preset is named apart from the picker's choice", presetWords, loadedLine + " | " + noneLine);
  Tooltips(parsed);
  TextFits();
  TipsFit();
  ACR_TestRegress.Complete("setuplobby", CHECKS);
 }
}
