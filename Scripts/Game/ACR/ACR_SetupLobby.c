// The setup lobby (0.7.0, Jacob 19 Nov): the match setup as a lobby screen instead of the classic
// dialog of "Label: value" buttons (ACR_SetupDialog, kept behind -acrSetupClassic 1 for one release).
// Left: the tabs as a list, Match first. Centre: the tab's rows as "Label < value >" spin boxes, a
// dot (and an orange label) on rows that differ from the server's default, rows that only matter
// under another one indented beneath it and hidden while it is off. Right: a sketch of the planned
// layout, the "US vs USSR, ..." summary and what the focused row does. Footer: preset picker, Reset
// tab, Start. Players who can't change the setup see the same screen read-only.
// The rows come from the server exactly as before (ACR_Setup.Rows, one "key\tLabel: value" line
// each), now with the row's lobby tab, parent and flags after them (ACR_Setup.Line); every change
// is still a request the server checks (ACR_Setup.Handle), so presets, -acr options and the
// srv:setup tests see no difference. The widgets are the game's own widget library (spin box, edit
// box, text button), so mouse, keyboard and gamepad focus work as in the game's settings menus.
// Every colour, heading font and text style comes from the theme in use (ACR_SetupTheme: vanilla
// command blue, the Cold War pack's green terminal, the modern stack's tablet); none is picked here.

// One row as the lobby reads it. Also used by the server-side test (ACR_TestSetup.RunLobby).
class ACR_SetupLobbyRow
{
 string m_sKey;
 string m_sText;   // what the classic dialog shows
 string m_sLabel;  // the part before ": " (value rows)
 string m_sValue;  // the part after it
 int m_iTab;
 string m_sParent;
 bool m_bChanged;
 bool m_bHidden;
 string m_sKind;   // value, seed, action, about, preset, meta
}

class ACR_SetupLobbyModel
{
 // The server's row lines, parsed. Lines without the lobby fields (the presets' own) go to the
 // Presets tab when their key says so, else to Match.
 static void Parse(string rows, notnull array<ref ACR_SetupLobbyRow> into)
 {
  into.Clear();
  array<string> lines = {};
  rows.Split("\n", lines, true);
  foreach (string line : lines)
  {
   array<string> parts = {};
   line.Split("\t", parts, false);
   if (parts.Count() < 2) continue;
   ACR_SetupLobbyRow row = new ACR_SetupLobbyRow();
   row.m_sKey = parts[0];
   row.m_sText = parts[1];
   if (parts.Count() >= 5)
   {
    row.m_iTab = parts[2].ToInt();
    row.m_sParent = parts[3];
    row.m_bChanged = parts[4].Contains("c");
    row.m_bHidden = parts[4].Contains("h");
   }
   else if (row.m_sKey.StartsWith("preset")) row.m_iTab = ACR_Setup.LOBBY_PRESETS;
   row.m_sKind = Kind(row.m_sKey, row.m_sText);
   if (row.m_sKind == "value" || row.m_sKind == "seed")
   {
    int colon = row.m_sText.IndexOf(": ");
    row.m_sLabel = row.m_sText.Substring(0, colon);
    row.m_sValue = row.m_sText.Substring(colon + 2, row.m_sText.Length() - colon - 2);
   }
   into.Insert(row);
  }
 }

 static string Kind(string key, string text)
 {
  if (key.StartsWith("lobby")) return "meta";
  if (key.StartsWith("preset")) return "preset";
  if (key.StartsWith("about")) return "about";
  if (key == "map" || key == "undo" || key == "deckeditor") return "action";
  if (text.IndexOf(": ") < 0) return "action";
  if (key == "seed") return "seed";
  return "value";
 }

 static ACR_SetupLobbyRow Find(notnull array<ref ACR_SetupLobbyRow> rows, string key)
 {
  foreach (ACR_SetupLobbyRow row : rows) if (row.m_sKey == key) return row;
  return null;
 }

 // The rows a tab shows, in the server's order: not hidden, and not under a row that isn't shown.
 static void Shown(notnull array<ref ACR_SetupLobbyRow> rows, int tab, notnull array<ACR_SetupLobbyRow> into)
 {
  into.Clear();
  foreach (ACR_SetupLobbyRow row : rows)
  {
   if (row.m_iTab != tab || row.m_bHidden || row.m_sKind == "meta") continue;
   if (!row.m_sParent.IsEmpty())
   {
    ACR_SetupLobbyRow parent = Find(rows, row.m_sParent);
    if (parent && parent.m_bHidden) continue;
   }
   into.Insert(row);
  }
 }

 // Rows of a tab that differ from the server's default (hidden ones too: they still count).
 static int ChangedOn(notnull array<ref ACR_SetupLobbyRow> rows, int tab)
 {
  int count = 0;
  foreach (ACR_SetupLobbyRow row : rows) if (row.m_iTab == tab && row.m_bChanged) count++;
  return count;
 }

 // "US vs USSR, generated, whole map, seed 123".
 static string Summary(notnull array<ref ACR_SetupLobbyRow> rows)
 {
  string west, east, layout;
  ACR_SetupLobbyRow row = Find(rows, "west");
  if (row) west = row.m_sValue;
  row = Find(rows, "east");
  if (row) east = row.m_sValue;
  row = Find(rows, "layout");
  if (row) layout = row.m_sValue;
  string summary = west + " vs " + east + ", " + layout;
  row = Find(rows, "seed");
  if (row && !row.m_sValue.StartsWith("random")) summary += ", seed " + row.m_sValue;
  return summary;
 }

 // "Preset in use: X" from the presets' note line ("" when the server sent none).
 static string PresetInUse(notnull array<ref ACR_SetupLobbyRow> rows)
 {
  ACR_SetupLobbyRow note = Find(rows, "presetnote");
  if (!note) return "";
  return note.m_sText;
 }

 // The preset loaded now ("" for none), from the note line.
 static string LoadedPreset(notnull array<ref ACR_SetupLobbyRow> rows)
 {
  string note = PresetInUse(rows);
  int colon = note.IndexOf(": ");
  if (colon < 0) return "";
  string name = note.Substring(colon + 2, note.Length() - colon - 2);
  if (name.StartsWith("none")) return "";
  return name;
 }

 // The right panel's preset line (20 Nov: it said "Preset in use: none" while the footer picker
 // showed "Default", which reads as a contradiction). "Loaded" here, "Preset to load" on the picker.
 static string LoadedLine(notnull array<ref ACR_SetupLobbyRow> rows)
 {
  if (PresetInUse(rows).IsEmpty()) return "";
  string name = LoadedPreset(rows);
  if (name.IsEmpty()) return "Loaded preset: none (the scenario's own setup)";
  return "Loaded preset: " + name;
 }

 // A base of the planned layout is an HQ: generated layouts say "HQ (US), 2 RP", the world's own
 // bases "HQ (US), 2 RP" too since 20 Nov (ACR_Setup.UpdatePreview), the live map "... | HQ".
 static bool IsHQDetail(string detail)
 {
  return detail.StartsWith("HQ") || detail.Contains("| HQ");
 }

 // The map sketch's square for a base: HQs biggest, hidden sites smallest.
 static int MarkerSize(string detail)
 {
  if (IsHQDetail(detail)) return 18;
  if (detail.Contains("Hidden")) return 6;
  return 10;
 }

 // The footer's controls, left to right (the keys line sits under them on its own line).
 static void FooterKeys(bool allowed, notnull array<string> into)
 {
  into.Clear();
  if (!allowed)
  {
   into.Insert("footerdecks");
   return;
  }
  into.Insert("footerpreset");
  into.Insert("footerload");
  into.Insert("footerreset");
  into.Insert("footerstart");
 }

 // What the right panel says for a key: a row, a tab, a footer control or the Presets tab.
 static string HelpFor(notnull array<ref ACR_SetupLobbyRow> rows, string key)
 {
  if (key.StartsWith("tab:"))
  {
   int tab = key.Substring(4, key.Length() - 4).ToInt();
   int changed = ChangedOn(rows, tab);
   string tabHelp = ACR_Setup.LobbyTabName(tab) + ": " + TabAbout(tab);
   if (changed > 0) tabHelp += string.Format("\n%1 changed from this server's default.", changed);
   return tabHelp;
  }
  if (key == "footerpreset" || key == "footerload") return "Load a saved setup: pick it in Preset to load, then press Load preset. The Presets tab saves, renames and shares them.";
  if (key == "footerreset") return "Puts every row of this tab back to this server's default (a loaded preset still applies).";
  if (key == "footerstart") return "Starts the match with this setup. Space on a keyboard, Y on a gamepad.";
  if (key == "footerdecks") return "Build a deck and hand it in while the host sets up the match.";
  if (key.StartsWith("preset")) return "Kept on the server in profile/ACA/SetupPresets. A server loads one with -acrPreset <name>; launch options still win over a preset.";
  string help = ACR_Setup.Help(key);
  ACR_SetupLobbyRow row = Find(rows, key);
  if (help.IsEmpty() && row) help = row.m_sText;
  if (help.IsEmpty()) return "Point at a row to see what it does.";
  if (row && row.m_sKind == "value") help = row.m_sLabel + ": " + row.m_sValue + "\n" + help;
  // A company or doctrine row: what the chosen one does.
  ACR_SetupLobbyRow about = Find(rows, "about" + key);
  if (about) help += "\n\n" + about.m_sText;
  if (row && row.m_bChanged) help += "\n(changed from this server's default)";
  return help;
 }

 // The plain tooltip a device skin shows on a control whose label is themed ("SITREP", "Execute",
 // "Area of ops"): its plain name and what it does, from the same help as the right panel.
 static string Tooltip(notnull array<ref ACR_SetupLobbyRow> rows, string key)
 {
  if (key.StartsWith("tab:"))
  {
   int tabIndex = key.Substring(4, key.Length() - 4).ToInt();
   return ACR_Setup.LobbyTabName(tabIndex) + ": " + TabAbout(tabIndex);
  }
  if (key == "footerstart") return "Start the match: starts it with this setup (Space, or Y on a gamepad).";
  if (key.StartsWith("footer") || key.StartsWith("preset")) return HelpFor(rows, key);
  ACR_SetupLobbyRow row = Find(rows, key);
  string name = key;
  if (row && !row.m_sLabel.IsEmpty()) name = row.m_sLabel;
  else if (row) name = row.m_sText;
  string help = ACR_Setup.Help(key);
  help.Replace("\n", " ");
  if (help.IsEmpty()) return name;
  return name + ": " + help;
 }

 // How many monospace characters fit across a width (the character's width as measured, plus a margin).
 static int TipChars(float width, float charWidth)
 {
  if (charWidth <= 0) return 12;
  return Math.Max(12, Math.Floor(width / (charWidth * 1.03)));
 }

 // Text broken at spaces into lines of at most perLine characters (a longer word is cut).
 static string Wrap(string text, int perLine, out int lines)
 {
  array<string> words = {};
  text.Split(" ", words, true);
  string result = "";
  string current = "";
  lines = 1;
  foreach (string listed : words)
  {
   string word = listed;
   while (word.Length() > perLine)
   {
    if (!current.IsEmpty())
    {
     result += current + "\n";
     lines++;
     current = "";
    }
    result += word.Substring(0, perLine) + "\n";
    lines++;
    word = word.Substring(perLine, word.Length() - perLine);
   }
   if (current.IsEmpty()) current = word;
   else if (current.Length() + 1 + word.Length() <= perLine) current += " " + word;
   else
   {
    result += current + "\n";
    lines++;
    current = word;
   }
  }
  return result + current;
 }

 static string TabAbout(int tab)
 {
  if (tab == 0) return "where the match is fought, how hard the enemy pushes, and who else is out there: FIA and zombies.";
  if (tab == 1) return "the two sides, which ones an AI commands, how the match opens and the starting RP.";
  if (tab == 2) return "the AI commanders, their doctrines and squad caps, minefields and fortifying, and how squads and guns behave.";
  if (tab == 3) return "money, companies and decks, deployment, FOB services and supply.";
  if (tab == 4) return "the 2IC, how players share command, the command map and the mission intro.";
  if (tab == 5) return "features that ship switched off until they have been played.";
  return "save the whole setup under a name, load it, share it.";
 }

 // The two sides' faction keys (the map sketch colours bases by them).
 static void Sides(notnull array<ref ACR_SetupLobbyRow> rows, out string west, out string east)
 {
  west = "";
  east = "";
  ACR_SetupLobbyRow sides = Find(rows, "lobbysides");
  if (!sides) return;
  array<string> keys = {};
  sides.m_sText.Split(",", keys, false);
  if (keys.Count() == 2)
  {
   west = keys[0];
   east = keys[1];
  }
 }
}

// Which control the right panel describes: the focused one (keyboard, gamepad, or the mouse, which
// focuses what it points at), else the open tab. Mouse-over alone no longer moves the help (20 Nov:
// the pointer resting on the Economy tab kept its text up while Starting RP had the focus).
class ACR_SetupLobbyHelp
{
 string m_sFocused;

 void Focus(string key) { m_sFocused = key; }

 void Blur(string key)
 {
  if (m_sFocused == key) m_sFocused = "";
 }

 string Key(int tab)
 {
  if (!m_sFocused.IsEmpty()) return m_sFocused;
  return "tab:" + tab.ToString();
 }
}

// Which control's tooltip shows (device skins): the last control the pointer entered or the focus
// reached; leaving it, or its focus moving away, hides the tooltip, and so does a tab change. The
// pointer resting on empty space shows none, keyboard and gamepad focus show the focused control's
// (20 Nov: it stayed up after the pointer left everything, falling back to the focus).
class ACR_SetupLobbyTipState
{
 string m_sShown;

 void Hover(string key, bool on)
 {
  if (on) m_sShown = key;
  else if (m_sShown == key) m_sShown = "";
 }

 void Focus(string key, bool on)
 {
  if (on) m_sShown = key;
  else if (m_sShown == key) m_sShown = "";
 }

 void Clear() { m_sShown = ""; }

 string Shown() { return m_sShown; }
}

// Focus on a control: the right panel says what it does (spin boxes have no invoker for it, BBB's
// settings tab does the same).
class ACR_SetupLobbyHandler : ScriptedWidgetEventHandler
{
 ACR_SetupLobby m_Lobby;
 string m_sKey;

 override bool OnFocus(Widget w, int x, int y)
 {
  if (m_Lobby)
  {
   m_Lobby.Focused(m_sKey, true);
   m_Lobby.Tip(m_sKey, w, true, false);
  }
  return false;
 }

 // The pointer: only the tooltip (the right panel follows the focus).
 override bool OnMouseEnter(Widget w, int x, int y)
 {
  if (m_Lobby) m_Lobby.Tip(m_sKey, w, true, true);
  return false;
 }

 override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
 {
  if (m_Lobby) m_Lobby.Tip(m_sKey, w, false, true);
  return false;
 }

 override bool OnFocusLost(Widget w, int x, int y)
 {
  if (m_Lobby)
  {
   m_Lobby.Focused(m_sKey, false);
   m_Lobby.Tip(m_sKey, w, false, false);
  }
  return false;
 }

 void OnLeft()
 {
  if (m_Lobby) m_Lobby.Step(m_sKey, -1);
 }

 void OnRight()
 {
  if (m_Lobby) m_Lobby.Step(m_sKey, 1);
 }
}

class ACR_SetupLobby : SCR_ConfigurableDialogUi
{
 static const string TAG = "ACR_SetupLobby";
 protected static const ResourceName LAYOUT_SPIN = "{C9DF0E6590F6C388}UI/layouts/WidgetLibrary/SpinBox/WLib_SpinBox.layout";
 protected static const ResourceName LAYOUT_BUTTON = "{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout";
 protected static const ResourceName LAYOUT_TEXT = "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout";
 protected static const ResourceName LAYOUT_EDIT = "{0022F0B45ADBC5AC}UI/layouts/WidgetLibrary/EditBox/WLib_EditBox.layout";
 protected static const float INDENT = 36;
 protected static const float TIP_WIDTH = 420;
 static const float ARROW_SIZE = 28;
 static const float DEVICE_MAP = 340;
 protected static const float TIP_PAD = 9;
 protected static const int TIP_FONT = 14;
 protected static const string REPORT = "Found a bug? Please report it: discord.gg/WK8xGSztB (issues channel; no GitHub account needed).";

 protected ref ACR_SetupTheme m_Theme;
 protected Widget m_wTabs;
 protected Widget m_wRows;
 protected TextWidget m_wPageTitle;
 protected TextWidget m_wDecor;
 protected Widget m_wMapDots;
 protected Widget m_wFooter;
 protected TextWidget m_wSubtitle;
 protected TextWidget m_wTabTitle;
 protected TextWidget m_wSummary;
 protected TextWidget m_wHelp;
 protected TextWidget m_wStatus;
 protected TextWidget m_wKeys;
 // The device skins' tooltip: what a themed control does, in plain words, on hover and on focus.
 protected Widget m_wTip;
 protected TextWidget m_wTipText;
 protected ref ACR_SetupLobbyTipState m_TipState = new ACR_SetupLobbyTipState();
 protected Widget m_wTipShown;
 protected ref map<string, string> m_RowLetters = new map<string, string>();
 // The terminal's monospace copies of the library controls' own texts (their font can't be set
 // from script): a button's label over its own (kept, invisible, so the button keeps its size), a
 // row's label and value in place of its own (hidden).
 protected ref map<SCR_ButtonTextComponent, TextWidget> m_MonoButtons = new map<SCR_ButtonTextComponent, TextWidget>();
 protected ref map<string, TextWidget> m_MonoLabels = new map<string, TextWidget>();
 protected ref map<string, TextWidget> m_MonoValues = new map<string, TextWidget>();
 protected ref ACR_SetupLobbyHelp m_Help = new ACR_SetupLobbyHelp();

 protected ref array<ref ACR_SetupLobbyRow> m_aRows = {};
 protected ref array<ref ACR_SetupLobbyHandler> m_aRowHandlers = {};
 protected ref array<ref ACR_SetupLobbyHandler> m_aFixedHandlers = {};
 protected ref map<string, Widget> m_RowWidgets = new map<string, Widget>();
 protected ref map<string, SCR_SpinBoxComponent> m_Spins = new map<string, SCR_SpinBoxComponent>();
 protected ref map<SCR_ButtonBaseComponent, string> m_Buttons = new map<SCR_ButtonBaseComponent, string>();
 protected ref map<int, SCR_ButtonTextComponent> m_TabButtons = new map<int, SCR_ButtonTextComponent>();
 protected int m_iTab;
 protected string m_sShape;     // the rows the centre column was built from
 protected string m_sFocus;     // the row the help describes
 protected string m_sInfo;
 protected bool m_bAllowed;
 protected bool m_bBuilt;
 protected bool m_bListening;
 protected string m_sMapSig;
 protected SCR_EditBoxComponent m_SeedBox;
 protected string m_sSeedShown;
 // Footer
 protected SCR_SpinBoxComponent m_FooterPreset;
 protected ref array<string> m_aFooterPresets = {};
 protected string m_sFooterPick;
 protected string m_sFooterSig;
 protected bool m_bFilling;
 // Presets tab
 protected SCR_EditBoxComponent m_PresetName;
 protected string m_sPresetTyped;
 protected string m_sPresetSelected;
 protected ref array<string> m_aPresetNames = {};

 override protected void OnMenuOpen(SCR_ConfigurableDialogUiPreset preset)
 {
  Widget root = GetRootWidget();
  if (!root) return;
  m_wTabs = root.FindAnyWidget("Tabs");
  m_wRows = root.FindAnyWidget("Rows");
  m_wMapDots = root.FindAnyWidget("MapDots");
  m_wFooter = root.FindAnyWidget("FooterLeft");
  m_wSubtitle = TextWidget.Cast(root.FindAnyWidget("Subtitle"));
  m_wTabTitle = TextWidget.Cast(root.FindAnyWidget("TabTitle"));
  m_wSummary = TextWidget.Cast(root.FindAnyWidget("Summary"));
  m_wHelp = TextWidget.Cast(root.FindAnyWidget("Help"));
  m_wStatus = TextWidget.Cast(root.FindAnyWidget("Status"));
  m_wPageTitle = TextWidget.Cast(root.FindAnyWidget("Title"));
  m_wDecor = TextWidget.Cast(root.FindAnyWidget("Decor"));
  m_iTab = ACR_Setup.LOBBY_FIRST;
  ApplyTheme(root);
  // Shoulder buttons (Q / E) page through the tabs, as in the game's own tabbed menus.
  InputManager input = GetGame().GetInputManager();
  input.AddActionListener("MenuTabLeft", EActionTrigger.DOWN, OnTabLeft);
  input.AddActionListener("MenuTabRight", EActionTrigger.DOWN, OnTabRight);
  m_bListening = true;
 }

 override void OnMenuClose()
 {
  if (m_bListening)
  {
   InputManager input = GetGame().GetInputManager();
   input.RemoveActionListener("MenuTabLeft", EActionTrigger.DOWN, OnTabLeft);
   input.RemoveActionListener("MenuTabRight", EActionTrigger.DOWN, OnTabRight);
   m_bListening = false;
  }
 }

 // The skin: colours of the page's own widgets, the heading font (a heading layout replaces the
 // layout's title and tab heading), the decoration line.
 protected void ApplyTheme(Widget root)
 {
  m_Theme = ACR_SetupTheme.Current();
  Widget backdrop = root.FindAnyWidget("Backdrop");
  if (backdrop) backdrop.SetColor(m_Theme.m_Backdrop);
  Widget mapBack = root.FindAnyWidget("MapBack");
  if (mapBack) mapBack.SetColor(m_Theme.m_Panel);
  if (!m_Theme.m_sHeadingLayout.IsEmpty())
  {
   m_wPageTitle = Heading(root.FindAnyWidget("TitleHolder"), m_wPageTitle);
   m_wTabTitle = Heading(root.FindAnyWidget("TabTitleHolder"), m_wTabTitle);
  }
  Colour(m_wPageTitle, m_Theme.m_Heading);
  Colour(m_wTabTitle, m_Theme.m_Heading);
  Colour(m_wSubtitle, m_Theme.m_Subtitle);
  Colour(m_wSummary, m_Theme.m_Heading);
  Colour(m_wHelp, m_Theme.m_Help);
  Colour(m_wStatus, m_Theme.m_Status);
  if (m_wPageTitle) m_wPageTitle.SetText(m_Theme.Text("AC: Actual - match setup"));
  if (m_wDecor)
  {
   m_wDecor.SetVisible(!m_Theme.m_sDecor.IsEmpty());
   m_wDecor.SetText(m_Theme.m_sDecor);
   m_wDecor.SetColor(m_Theme.m_Decor);
  }
  Structure(root, m_Theme);
  if (!m_Theme.m_bDevice) return;
  // Denser and calmer type on a device; the terminal's body text in its monospace font too.
  if (m_Theme.m_bMonoBody)
  {
   Widget side = root.FindAnyWidget("Side");
   m_wSubtitle = Mono(root.FindAnyWidget("TitleHolder"), m_wSubtitle);
   m_wSummary = Mono(side, m_wSummary);
   m_wHelp = Mono(side, m_wHelp);
   m_wStatus = Mono(side, m_wStatus);
  }
  bool monoHeading = m_Theme.m_sHeadingLayout == ACR_SetupTheme.HEADING_MONO;
  bool monoBody = m_Theme.m_bMonoBody;
  Size(m_wPageTitle, 22, monoHeading);
  Size(m_wTabTitle, 18, monoHeading);
  Size(m_wSubtitle, 15, monoBody);
  Size(m_wSummary, 17, monoBody);
  Size(m_wHelp, 15, monoBody);
  Size(m_wStatus, 14, monoBody);
  BuildTip(root);
 }

 // The device skins' structure (tablet, terminal): the world hidden behind an opaque screen in a
 // bezel, a status bar and 1 px rules, a thin line beside the tab rail. Static, so the server's test
 // can build the layout and check it. Vanilla: nothing changes.
 static void Structure(Widget root, ACR_SetupTheme theme)
 {
  if (!root || !theme || !theme.m_bDevice) return;
  Widget backdrop = root.FindAnyWidget("Backdrop");
  if (backdrop) backdrop.SetColor(theme.m_Bezel);
  Reveal(root.FindAnyWidget("ScreenEdge"), theme.m_Line);
  Reveal(root.FindAnyWidget("Screen"), theme.m_Screen);
  Widget page = root.FindAnyWidget("Page");
  if (page)
  {
   FrameSlot.SetAnchorMin(page, 0.045, 0.06);
   FrameSlot.SetAnchorMax(page, 0.955, 0.94);
  }
  Reveal(root.FindAnyWidget("StatusBar"), null);
  Reveal(root.FindAnyWidget("StatusBack"), theme.m_StatusBack);
  StatusText(root.FindAnyWidget("StatusLeft"), theme.m_sStatusLeft, theme);
  StatusText(root.FindAnyWidget("StatusRight"), theme.m_sStatusRight, theme);
  Rule(root.FindAnyWidget("TopRule"), theme.m_Line);
  Rule(root.FindAnyWidget("FooterRule"), theme.m_Line);
  if (theme.m_bRail) Rule(root.FindAnyWidget("RailLine"), theme.m_Line);
  // A smaller map sketch: with the status bar and rules the page ran past the screen at 1105 px and
  // cut the footer's keys line (20 Nov).
  SizeLayoutWidget mapSize = SizeLayoutWidget.Cast(root.FindAnyWidget("MapSize"));
  if (mapSize)
  {
   mapSize.SetWidthOverride(DEVICE_MAP);
   mapSize.SetHeightOverride(DEVICE_MAP);
  }
 }

 protected static void Reveal(Widget w, Color color)
 {
  if (!w) return;
  w.SetVisible(true);
  if (color) w.SetColor(color);
 }

 // A 1 px line: stretched along its layout, one pixel across.
 protected static void Rule(Widget w, Color color)
 {
  ImageWidget line = ImageWidget.Cast(w);
  if (!line) return;
  Reveal(line, color);
  line.SetSize(1, 1);
 }

 // The status bar's words in the monospace font (once: a holder keeps its text).
 protected static void StatusText(Widget holder, string text, ACR_SetupTheme theme)
 {
  if (!holder || text.IsEmpty() || holder.GetChildren()) return;
  TextWidget words = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(ACR_SetupTheme.HEADING_MONO, holder));
  if (!words) return;
  AlignableSlot.SetPadding(words, 0, 0, 0, 0);
  words.SetText(text);
  MonoFont(words, 14);
  words.SetColor(theme.m_Decor);
 }

 // A text in the monospace font in place of the layout's own (hidden), appended to the holder.
 protected TextWidget Mono(Widget holder, TextWidget old)
 {
  if (!holder || !old) return old;
  TextWidget mono = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(ACR_SetupTheme.HEADING_MONO, holder));
  if (!mono) return old;
  mono.SetColor(old.GetColor());
  mono.SetText(old.GetText());
  AlignableSlot.SetPadding(mono, 0, 4, 0, 8);
  old.SetVisible(false);
  return mono;
 }

 protected static void Size(TextWidget w, int size, bool mono = false)
 {
  if (mono) MonoFont(w, size);
  else if (w) w.SetExactFontSize(size);
 }

 // A heading in the skin's heading font, in place of the layout's own (hidden).
 protected TextWidget Heading(Widget holder, TextWidget old)
 {
  if (!holder) return old;
  TextWidget heading = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(m_Theme.m_sHeadingLayout, holder));
  if (!heading) return old;
  if (old)
  {
   heading.SetText(old.GetText());
   old.SetVisible(false);
  }
  return heading;
 }

 protected static void Colour(Widget w, Color color)
 {
  if (w) w.SetColor(color);
 }

 // The layout found its parts (false: the lobby can't draw, ACR_SetupClient falls back to classic).
 bool Ready()
 {
  return m_wTabs && m_wRows && m_wFooter;
 }

 int CurrentTab() { return m_iTab; }

 // A row on screen now (tests).
 bool ShowsRow(string key) { return m_RowWidgets.Contains(key); }

 // The server sent new rows (or the screen opened): rebuild what changed shape, update the rest.
 void Fill(string rows, string info, bool allowed)
 {
  if (!Ready()) return;
  if (!m_Theme) m_Theme = ACR_SetupTheme.Current();
  bool allowedChanged = allowed != m_bAllowed || !m_bBuilt;
  m_bAllowed = allowed;
  m_sInfo = info;
  ACR_SetupLobbyModel.Parse(rows, m_aRows);
  FillTabs();
  string shape = RowsShape();
  if (shape != m_sShape || allowedChanged) BuildRows(shape);
  else UpdateRows();
  FillFooter(allowedChanged);
  FillSide();
  SCR_InputButtonComponent start = FindButton(BUTTON_CONFIRM);
  if (start)
  {
   // The dialog's own Start is now only the key badge (Space, Y on a pad) right of the footer's
   // Start match button, which mouse, keyboard and gamepad focus can press (20 Nov: it was text
   // beside a badge, unlike the buttons next to it). The badge keeps the key working.
   start.SetVisible(m_bAllowed, false);
   start.SetLabel("");
  }
  if (!m_bBuilt)
  {
   m_bBuilt = true;
   // Focus the first tab, so a gamepad can start moving at once.
   SCR_ButtonTextComponent first = m_TabButtons.Get(0);
   if (first) GetGame().GetWorkspace().SetFocusedWidget(first.GetRootWidget());
  }
 }

 // ---------------------------------------------------------------- tabs
 protected void FillTabs()
 {
  if (m_TabButtons.IsEmpty())
  {
   for (int tab = 0; tab < ACR_Setup.LOBBY_TABS; tab++)
   {
    SCR_ButtonTextComponent button = AddButton(m_wTabs, "tab:" + tab.ToString(), TabText(tab), 0, m_aFixedHandlers);
    if (button) m_TabButtons.Set(tab, button);
    // The rail: text-only tabs flush on the rail, a thin divider under each.
    if (!m_Theme.m_bRail) continue;
    if (button) AlignableSlot.SetPadding(button.GetRootWidget(), 0, 0, 0, 0);
    ImageWidget divider = ImageWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, m_Theme.m_Line, 0, m_wTabs));
    if (!divider) continue;
    AlignableSlot.SetHorizontalAlign(divider, LayoutHorizontalAlign.Stretch);
    divider.SetSize(1, 1);
    divider.SetOpacity(0.5);
   }
  }
  foreach (int index, SCR_ButtonTextComponent tabButton : m_TabButtons)
  {
   string name = m_Theme.TabLabel(index);
   if (name.IsEmpty()) name = TabText(index);
   if (index != ACR_Setup.LOBBY_PRESETS && ACR_SetupLobbyModel.ChangedOn(m_aRows, index) > 0) name += "  " + m_Theme.m_sChanged;
   ButtonText(tabButton, m_Theme.Text(name));
   Paint(tabButton, index == m_iTab, m_Theme.m_bRail);
  }
 }

 // "2.  Match": the paragraph number as a prefix, then the plain name.
 protected static string TabText(int tab)
 {
  return ACR_Setup.LobbyTabPrefix(tab) + "  " + ACR_Setup.LobbyTabName(tab);
 }

 void ShowTab(int tab)
 {
  if (tab < 0) tab = ACR_Setup.LOBBY_TABS - 1;
  if (tab >= ACR_Setup.LOBBY_TABS) tab = 0;
  m_iTab = tab;
  FillTabs();
  BuildRows(RowsShape());
  FillFooter(false);
  FillSide();
 }

 protected void OnTabLeft() { ShowTab(m_iTab - 1); }
 protected void OnTabRight() { ShowTab(m_iTab + 1); }

 // ---------------------------------------------------------------- the centre column
 // Which rows the tab shows and of what kind: the column is rebuilt only when this changes, so a
 // value change keeps the focus where it is.
 protected string RowsShape()
 {
  string shape = m_iTab.ToString();
  array<ACR_SetupLobbyRow> shown = {};
  ACR_SetupLobbyModel.Shown(m_aRows, m_iTab, shown);
  foreach (ACR_SetupLobbyRow row : shown) shape += "|" + row.m_sKey + ":" + row.m_sKind;
  if (m_iTab == ACR_Setup.LOBBY_PRESETS) shape += "|" + m_sPresetSelected;
  if (m_iTab == ACR_Setup.LOBBY_FIRST) shape += "|" + SideText(true) + "|" + SideText(false);
  return shape;
 }

 protected void BuildRows(string shape)
 {
  m_sShape = shape;
  if (m_PresetName) m_sPresetTyped = m_PresetName.GetValue();
  m_PresetName = null;
  m_SeedBox = null;
  m_sSeedShown = "";
  while (m_wRows.GetChildren()) m_wRows.GetChildren().RemoveFromHierarchy();
  m_RowWidgets.Clear();
  m_Spins.Clear();
  m_MonoLabels.Clear();
  m_MonoValues.Clear();
  m_aRowHandlers.Clear();
  // Buttons of the tabs and the footer stay; the rows' go.
  array<SCR_ButtonBaseComponent> gone = {};
  foreach (SCR_ButtonBaseComponent button, string key : m_Buttons) if (!key.StartsWith("tab:") && !key.StartsWith("footer")) gone.Insert(button);
  foreach (SCR_ButtonBaseComponent old : gone)
  {
   m_Buttons.Remove(old);
   m_MonoButtons.Remove(SCR_ButtonTextComponent.Cast(old));
  }
  string heading = m_Theme.TabHeading(m_iTab);
  if (heading.IsEmpty()) heading = TabText(m_iTab);
  if (m_wTabTitle) m_wTabTitle.SetText(m_Theme.Text(heading));
  // The rows go (or the tab changed): no tooltip may stay or point at them.
  HideTip();
  if (m_iTab == ACR_Setup.LOBBY_PRESETS)
  {
   BuildPresets();
   Refocus();
   return;
  }
  if (m_iTab == ACR_Setup.LOBBY_FIRST) BuildSides();
  array<ACR_SetupLobbyRow> shown = {};
  ACR_SetupLobbyModel.Shown(m_aRows, m_iTab, shown);
  // The terminal letters its rows as an order's sub-paragraphs: A., B., ...
  m_RowLetters.Clear();
  string letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  int lettered = 0;
  foreach (ACR_SetupLobbyRow shownRow : shown)
  {
   if (shownRow.m_sKind != "value" || lettered >= letters.Length()) continue;
   m_RowLetters.Set(shownRow.m_sKey, letters.Substring(lettered, 1) + ". ");
   lettered++;
  }
  foreach (ACR_SetupLobbyRow row : shown)
  {
   float indent = 0;
   if (!row.m_sParent.IsEmpty()) indent = INDENT;
   if (row.m_sKind == "value") AddSpinRow(row, indent);
   else if (row.m_sKind == "seed") AddSeedRow(row, indent);
   else if (row.m_sKind == "action")
   {
    SCR_ButtonTextComponent action = AddButton(m_wRows, row.m_sKey, row.m_sText, indent, m_aRowHandlers);
    // Players who can't change the setup may still open their own deck builder.
    if (action && !m_bAllowed && row.m_sKey != "deckeditor") action.SetEnabled(false, false);
   }
   else if (row.m_sKind == "about")
   {
    TextWidget about = AddText(m_wRows, row.m_sText, indent + 8);
    if (about) about.SetColor(m_Theme.m_TextDim);
   }
  }
  if (shown.IsEmpty()) AddText(m_wRows, "Nothing to set here with this setup.", 8);
  Refocus();
 }

 // Values and dots in place (same rows as before).
 protected void UpdateRows()
 {
  foreach (string key, SCR_SpinBoxComponent spin : m_Spins)
  {
   ACR_SetupLobbyRow row = ACR_SetupLobbyModel.Find(m_aRows, key);
   if (row) SetSpin(spin, row);
  }
  ACR_SetupLobbyRow seed = ACR_SetupLobbyModel.Find(m_aRows, "seed");
  if (seed && m_SeedBox) SetSeed(seed);
  foreach (SCR_ButtonBaseComponent button, string buttonKey : m_Buttons)
  {
   ACR_SetupLobbyRow action = ACR_SetupLobbyModel.Find(m_aRows, buttonKey);
   SCR_ButtonTextComponent textButton = SCR_ButtonTextComponent.Cast(button);
   if (action && textButton && action.m_sKind == "action") ButtonText(textButton, m_Theme.Text(action.m_sText));
  }
  if (m_sFocus != "") Describe(m_sFocus);
 }

 protected void Refocus()
 {
  if (m_sFocus.IsEmpty()) return;
  Widget again = m_RowWidgets.Get(m_sFocus);
  if (again) GetGame().GetWorkspace().SetFocusedWidget(again);
 }

 // "Label < value >": the game's spin box with the row's value in both of its items, so either arrow
 // is live; an arrow asks the server for the value before or after (ACR_Setup.Cycle) and the next
 // rows bring the new one.
 protected void AddSpinRow(ACR_SetupLobbyRow row, float indent)
 {
  Widget w = GetGame().GetWorkspace().CreateWidgets(LAYOUT_SPIN, m_wRows);
  if (!w) return;
  AlignableSlot.SetPadding(w, 4 + indent, 2, 4, 2);
  if (m_Theme.m_bCompact) AlignableSlot.SetPadding(w, 4 + indent, 0, 4, 0);
  SpinLook(w, m_Theme);
  SCR_SpinBoxComponent spin = SCR_SpinBoxComponent.Cast(w.FindHandler(SCR_SpinBoxComponent));
  if (!spin) return;
  spin.SetCycleMode(true);
  Widget hints = w.FindAnyWidget("HintBar");
  if (hints) hints.SetVisible(false);
  SetSpin(spin, row);
  ACR_SetupLobbyHandler handler = Watch(w, row.m_sKey, m_aRowHandlers);
  spin.GetOnLeftArrowClick().Insert(handler.OnLeft);
  spin.GetOnRightArrowClick().Insert(handler.OnRight);
  spin.SetEnabled(m_bAllowed, false);
  m_RowWidgets.Set(row.m_sKey, w);
  m_Spins.Set(row.m_sKey, spin);
 }

 // A device skin's row (the widget library's spin box, its parts by name): a thin row with no
 // filled bar, the label and value in smaller type, the arrows as light marks round the value.
 // The control itself is untouched, so mouse, keys and pad work as before.
 static void SpinLook(Widget w, ACR_SetupTheme theme)
 {
  if (!w || !theme || !theme.m_bCompact) return;
  Compact(w, FitHeight(16));
  Color clear = new Color(0, 0, 0, 0);
  Widget back = w.FindAnyWidget("Background");
  if (back) back.SetColor(clear);
  Widget highlight = w.FindAnyWidget("LineHighlight");
  // The row's hover and focus band: a dark fill, not the bright rule colour (20 Nov: green on green).
  if (highlight) highlight.SetColor(theme.m_ButtonHover);
  array<string> arrows = {"ButtonLeft", "ButtonRight"};
  foreach (string arrowName : arrows)
  {
   Widget arrow = w.FindAnyWidget(arrowName);
   if (!arrow) continue;
   SizeLayoutWidget arrowSize = SizeLayoutWidget.Cast(arrow.FindAnyWidget("SizeLayout"));
   if (arrowSize)
   {
    // 20 Nov: the arrows came out tiny in the thin rows.
    arrowSize.EnableWidthOverride(true);
    arrowSize.SetWidthOverride(ARROW_SIZE);
    arrowSize.EnableHeightOverride(true);
    arrowSize.SetHeightOverride(ARROW_SIZE);
   }
   Widget arrowBack = arrow.FindAnyWidget("BackgroundImage");
   if (arrowBack) arrowBack.SetColor(clear);
   Widget mark = arrow.FindAnyWidget("Panel");
   if (mark) mark.SetColor(theme.m_Decor);
  }
  TextWidget value = TextWidget.Cast(w.FindAnyWidget("SelectionText"));
  if (value)
  {
   value.SetExactFontSize(16);
   value.SetColor(theme.m_ButtonText);
  }
  TextWidget label = TextWidget.Cast(w.FindAnyWidget("Text"));
  if (label) label.SetExactFontSize(16);
  // The value in a thin box (the mock-ups): the spin box's arrows and value moved into an overlay
  // over a 1 px frame. The parts keep their own handlers, so the control works as before.
  Widget content = w.FindAnyWidget("ContentLayout");
  Widget line = w.FindAnyWidget("HorizontalLayout");
  if (!content || !line || content.GetParent() != line) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget box = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, line);
  if (!box) return;
  LayoutSlot.SetSizeMode(box, LayoutSizeMode.Fill);
  AlignableSlot.SetVerticalAlign(box, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(box, 8, 3, 0, 3);
  Widget edge = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, theme.m_Line, 0, box);
  if (edge)
  {
   AlignableSlot.SetHorizontalAlign(edge, LayoutHorizontalAlign.Stretch);
   AlignableSlot.SetVerticalAlign(edge, LayoutVerticalAlign.Stretch);
  }
  Widget inside = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, theme.m_Screen, 0, box);
  if (inside)
  {
   AlignableSlot.SetHorizontalAlign(inside, LayoutHorizontalAlign.Stretch);
   AlignableSlot.SetVerticalAlign(inside, LayoutVerticalAlign.Stretch);
   AlignableSlot.SetPadding(inside, 1, 1, 1, 1);
  }
  box.AddChild(content);
  AlignableSlot.SetHorizontalAlign(content, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(content, LayoutVerticalAlign.Center);
 }

 // ---------------------------------------------------------------- tooltip (device skins)
 // The tooltip: a framed box sized to its text, which is wrapped here (the font is monospace, so a
 // line's width is its length): 20 Nov, its text ran out of a box sized by the layout.
 protected void BuildTip(Widget root)
 {
  if (!root || m_wTip) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_wTip = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, Color.FromInt(Color.WHITE), 100, root);
  if (!m_wTip) return;
  FrameSlot.SetAnchorMin(m_wTip, 0, 0);
  FrameSlot.SetAnchorMax(m_wTip, 0, 0);
  FrameSlot.SetAlignment(m_wTip, 0, 0);
  FrameSlot.SetSizeToContent(m_wTip, false);
  Widget edge = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, m_Theme.m_Line, 0, m_wTip);
  if (edge)
  {
   AlignableSlot.SetHorizontalAlign(edge, LayoutHorizontalAlign.Stretch);
   AlignableSlot.SetVerticalAlign(edge, LayoutVerticalAlign.Stretch);
  }
  Widget inside = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, m_Theme.m_StatusBack, 0, m_wTip);
  if (inside)
  {
   AlignableSlot.SetHorizontalAlign(inside, LayoutHorizontalAlign.Stretch);
   AlignableSlot.SetVerticalAlign(inside, LayoutVerticalAlign.Stretch);
   AlignableSlot.SetPadding(inside, 1, 1, 1, 1);
  }
  m_wTipText = TextWidget.Cast(ws.CreateWidgets(ACR_SetupTheme.HEADING_MONO, m_wTip));
  if (!m_wTipText) return;
  AlignableSlot.SetHorizontalAlign(m_wTipText, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(m_wTipText, LayoutVerticalAlign.Stretch);
  AlignableSlot.SetPadding(m_wTipText, TIP_PAD, TIP_PAD, TIP_PAD, TIP_PAD);
  MonoFont(m_wTipText, TIP_FONT);
  m_wTipText.SetColor(m_Theme.m_Help);
 }

 // A control got or lost the pointer (hover) or the focus: the tooltip follows the pointer while
 // it rests on a control, else the focus.
 void Tip(string key, Widget w, bool on, bool hover)
 {
  if (!m_wTip) return;
  if (hover) m_TipState.Hover(key, on);
  else m_TipState.Focus(key, on);
  if (on && m_TipState.Shown() == key) m_wTipShown = w;
  if (m_TipState.Shown().IsEmpty() || !m_wTipShown)
  {
   m_wTip.SetVisible(false);
   return;
  }
  ShowTip(m_TipState.Shown(), m_wTipShown);
 }

 protected void HideTip()
 {
  m_TipState.Clear();
  m_wTipShown = null;
  if (m_wTip) m_wTip.SetVisible(false);
 }

 protected void ShowTip(string key, Widget w)
 {
  string text = ACR_SetupLobbyModel.Tooltip(m_aRows, key);
  if (text.IsEmpty() || !m_wTipText)
  {
   m_wTip.SetVisible(false);
   return;
  }
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float x, y, wide, high;
  w.GetScreenPos(x, y);
  w.GetScreenSize(wide, high);
  x = ws.DPIUnscale(x);
  y = ws.DPIUnscale(y);
  wide = ws.DPIUnscale(wide);
  high = ws.DPIUnscale(high);
  float screenWide = ws.DPIUnscale(ws.GetWidth());
  float screenHigh = ws.DPIUnscale(ws.GetHeight());
  // Where: a tab's in the rail's own empty space under the tabs (beside them it covered the Match
  // tab's side panels), a row's under the row, a footer button's above it. Its width: the rail's, or
  // up to TIP_WIDTH, kept on the screen.
  float boxWide = TIP_WIDTH;
  float left = x;
  float top = y + high + 6;
  bool above = key.StartsWith("footer");
  if (key.StartsWith("tab:") && m_wTabs)
  {
   float railX, railY, railW, railH;
   m_wTabs.GetScreenPos(railX, railY);
   m_wTabs.GetScreenSize(railW, railH);
   left = ws.DPIUnscale(railX);
   boxWide = Math.Max(ws.DPIUnscale(railW) - 16, 200);
   top = y + high + 6;
   SCR_ButtonTextComponent last = m_TabButtons.Get(ACR_Setup.LOBBY_TABS - 1);
   if (last)
   {
    float lastX, lastY, lastW, lastH;
    last.GetRootWidget().GetScreenPos(lastX, lastY);
    last.GetRootWidget().GetScreenSize(lastW, lastH);
    top = ws.DPIUnscale(lastY + lastH) + 12;
   }
  }
  float charWidth, lineHeight;
  MonoMetrics(TIP_FONT, charWidth, lineHeight);
  int perLine = ACR_SetupLobbyModel.TipChars(boxWide - 2 * TIP_PAD, charWidth);
  int lines = 1;
  string wrapped = ACR_SetupLobbyModel.Wrap(text, perLine, lines);
  m_wTipText.SetText(wrapped);
  float boxHigh = lines * lineHeight + 2 * TIP_PAD + 2;
  float measuredW, measuredH;
  m_wTipText.GetTextSize(measuredW, measuredH);
  if (measuredH > 0) boxHigh = Math.Max(boxHigh, ws.DPIUnscale(measuredH) + 2 * TIP_PAD + 2);
  if (above) top = y - boxHigh - 6;
  left = Math.Max(4, Math.Min(left, screenWide - boxWide - 4));
  if (top + boxHigh > screenHigh - 4) top = y - boxHigh - 6;
  top = Math.Max(4, top);
  FrameSlot.SetPos(m_wTip, left, top);
  FrameSlot.SetSize(m_wTip, boxWide, boxHigh);
  m_wTip.SetVisible(true);
 }

 // A button's words, and the terminal's monospace copy over them.
 protected void ButtonText(SCR_ButtonTextComponent button, string text)
 {
  button.SetText(text);
  if (!m_Theme.m_bMonoBody) return;
  TextWidget mono = m_MonoButtons.Get(button);
  TextWidget own = button.GetTextWidget();
  if (!mono)
  {
   Widget overlay = button.GetRootWidget().FindAnyWidget("Overlay");
   if (!overlay || !own) return;
   mono = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(ACR_SetupTheme.HEADING_MONO, overlay));
   if (!mono) return;
   AlignableSlot.SetVerticalAlign(mono, LayoutVerticalAlign.Center);
   AlignableSlot.SetHorizontalAlign(mono, LayoutHorizontalAlign.Center);
   AlignableSlot.SetPadding(mono, 12, 0, 12, 0);
   MonoFont(mono, 14);
   m_MonoButtons.Set(button, mono);
  }
  mono.SetText(text);
  if (own)
  {
   mono.SetColor(own.GetColor());
   own.SetOpacity(0);
  }
 }

 // The terminal: a row's label and value in monospace, in place of the spin box's own.
 protected void MonoRow(string key, Widget spin, string label, string value, Color labelColor)
 {
  if (!m_Theme.m_bMonoBody || !spin) return;
  TextWidget monoLabel = m_MonoLabels.Get(key);
  if (!monoLabel)
  {
   Widget labelRow = spin.FindAnyWidget("LabelRoot");
   Widget labelFrame = spin.FindAnyWidget("Frame0");
   if (labelRow && labelFrame)
   {
    labelFrame.SetVisible(false);
    monoLabel = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(ACR_SetupTheme.HEADING_MONO, labelRow));
    if (monoLabel)
    {
     LayoutSlot.SetSizeMode(monoLabel, LayoutSizeMode.Fill);
     AlignableSlot.SetVerticalAlign(monoLabel, LayoutVerticalAlign.Center);
     AlignableSlot.SetPadding(monoLabel, 8, 0, 4, 0);
     MonoFont(monoLabel, 15);
     m_MonoLabels.Set(key, monoLabel);
    }
   }
  }
  if (monoLabel)
  {
   monoLabel.SetText(label);
   monoLabel.SetColor(labelColor);
  }
  TextWidget monoValue = m_MonoValues.Get(key);
  if (!monoValue)
  {
   Widget own = spin.FindAnyWidget("SelectionText");
   if (own && own.GetParent())
   {
    own.SetVisible(false);
    monoValue = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(ACR_SetupTheme.HEADING_MONO, own.GetParent()));
    if (monoValue)
    {
     AlignableSlot.SetHorizontalAlign(monoValue, LayoutHorizontalAlign.Center);
     AlignableSlot.SetVerticalAlign(monoValue, LayoutVerticalAlign.Center);
     AlignableSlot.SetPadding(monoValue, 6, 0, 6, 0);
     MonoFont(monoValue, 15);
     monoValue.SetColor(m_Theme.m_ButtonText);
     m_MonoValues.Set(key, monoValue);
    }
   }
  }
  if (monoValue) monoValue.SetText(value);
 }

 static void HideItemBars(Widget spin)
 {
  if (!spin) return;
  Widget bars = spin.FindAnyWidget("SizeLayout0");
  if (bars) bars.SetVisible(false);
  Widget hints = spin.FindAnyWidget("HintBar");
  if (hints) hints.SetVisible(false);
 }

 protected void SetSpin(SCR_SpinBoxComponent spin, ACR_SetupLobbyRow row)
 {
  m_bFilling = true;
  string value = m_Theme.Text(row.m_sValue);
  if (spin.GetNumItems() != 2 || spin.GetItemName(0) != value)
  {
   spin.ClearAll();
   spin.AddItem(value);
   spin.AddItem(value);
   spin.SetCurrentItem(0, false, false, false);
  }
  // The spin box's item bars (one per item; ours are two copies of the value) drew through the
  // value on the tablet (20 Nov); adding items can bring them back, so they go after every fill.
  HideItemBars(spin.GetRootWidget());
  string plain = m_Theme.RowLabel(row.m_sKey);
  if (plain.IsEmpty()) plain = row.m_sLabel;
  string letter = "";
  if (m_Theme.m_bLettered) m_RowLetters.Find(row.m_sKey, letter);
  string label = m_Theme.Text(letter + plain);
  if (row.m_bChanged) label = m_Theme.m_sChanged + label;
  if (!m_Theme.m_sCursor.IsEmpty() && row.m_sKey == m_sFocus) label = m_Theme.m_sCursor + label;
  spin.SetLabel(label);
  Color labelColor = m_Theme.m_Text;
  if (row.m_bChanged) labelColor = m_Theme.m_Changed;
  TextWidget labelWidget = spin.GetLabel();
  if (labelWidget) labelWidget.SetColor(labelColor);
  MonoRow(row.m_sKey, spin.GetRootWidget(), label, value, labelColor);
  m_bFilling = false;
 }

 // The seed: typed in (Enter or leaving the box sends it), or a new random one.
 protected void AddSeedRow(ACR_SetupLobbyRow row, float indent)
 {
  Widget line = GetGame().GetWorkspace().CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_wRows);
  AlignableSlot.SetPadding(line, indent, 0, 0, 0);
  Widget edit = GetGame().GetWorkspace().CreateWidgets(LAYOUT_EDIT, line);
  if (edit)
  {
   AlignableSlot.SetPadding(edit, 4, 2, 4, 2);
   LayoutSlot.SetSizeMode(edit, LayoutSizeMode.Fill);
   m_SeedBox = SCR_EditBoxComponent.Cast(edit.FindHandler(SCR_EditBoxComponent));
   if (m_SeedBox)
   {
    m_SeedBox.SetLabel(m_Theme.Text(row.m_sLabel));
    m_SeedBox.SetPlaceholderText("random");
    m_SeedBox.m_OnWriteModeLeave.Insert(OnSeedTyped);
    m_SeedBox.SetEnabled(m_bAllowed, false);
    SetSeed(row);
   }
   Watch(edit, row.m_sKey, m_aRowHandlers);
   m_RowWidgets.Set(row.m_sKey, edit);
  }
  SCR_ButtonTextComponent roll = AddButton(line, "seed", "New seed", 0, m_aRowHandlers);
  if (roll && !m_bAllowed) roll.SetEnabled(false, false);
 }

 protected void SetSeed(ACR_SetupLobbyRow row)
 {
  string value = row.m_sValue;
  if (value.StartsWith("random")) value = "";
  if (value == m_sSeedShown) return;
  m_sSeedShown = value;
  m_SeedBox.SetValue(value);
 }

 protected void OnSeedTyped(string text)
 {
  if (!m_bAllowed || text == m_sSeedShown || text.IsEmpty()) return;
  ACR_SetupClient.Request("seedset:" + text);
 }

 // A row's arrow (from its handler).
 void Step(string key, int step)
 {
  if (m_bFilling || !m_bAllowed) return;
  m_sFocus = key;
  if (step < 0) ACR_SetupClient.Request("prev:" + key);
  else ACR_SetupClient.Request(key);
 }

 // ---------------------------------------------------------------- Presets tab (#143)
 protected void BuildPresets()
 {
  m_aPresetNames.Clear();
  array<string> shown = {};
  foreach (ACR_SetupLobbyRow row : m_aRows)
  {
   if (!row.m_sKey.StartsWith("presetrow:")) continue;
   m_aPresetNames.Insert(row.m_sKey.Substring(10, row.m_sKey.Length() - 10));
   shown.Insert(row.m_sText);
  }
  if (!m_aPresetNames.Contains(m_sPresetSelected)) m_sPresetSelected = "";
  string loadedLine = ACR_SetupLobbyModel.LoadedLine(m_aRows);
  if (!loadedLine.IsEmpty()) AddText(m_wRows, m_Theme.Text(loadedLine), 8);
  if (!m_bAllowed)
  {
   foreach (string listed : shown) AddText(m_wRows, listed, 24);
   return;
  }
  Widget edit = GetGame().GetWorkspace().CreateWidgets(LAYOUT_EDIT, m_wRows);
  if (edit)
  {
   AlignableSlot.SetPadding(edit, 4, 2, 4, 2);
   m_PresetName = SCR_EditBoxComponent.Cast(edit.FindHandler(SCR_EditBoxComponent));
   if (m_PresetName)
   {
    m_PresetName.SetLabel("Name");
    m_PresetName.SetValue(m_sPresetTyped);
   }
   Watch(edit, "presetname", m_aRowHandlers);
  }
  Widget top = GetGame().GetWorkspace().CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_wRows);
  AddButton(top, "presetsave", "Save the setup under this name", 0, m_aRowHandlers, true);
  AddButton(top, "presetimport", "Paste from the clipboard", 0, m_aRowHandlers, true);
  foreach (int i, string name : m_aPresetNames)
  {
   string text = shown[i];
   if (name == m_sPresetSelected) text = "> " + text + " <";
   AddButton(m_wRows, "presetpick:" + i.ToString(), text, 0, m_aRowHandlers);
  }
  if (m_sPresetSelected.IsEmpty())
  {
   AddText(m_wRows, "Pick a preset to load, rename, delete or copy it,\nor to load it at the start of every match.", 8);
   return;
  }
  Widget acts = GetGame().GetWorkspace().CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_wRows);
  AddButton(acts, "presetload", "Load", 0, m_aRowHandlers, true);
  AddButton(acts, "presetrename", "Rename to the name above", 0, m_aRowHandlers, true);
  AddButton(acts, "presetdelete", "Delete", 0, m_aRowHandlers, true);
  Widget more = GetGame().GetWorkspace().CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_wRows);
  bool every = shown[m_aPresetNames.Find(m_sPresetSelected)].Contains(", loads every match");
  string everyText = "Load at every match: off";
  if (every) everyText = "Load at every match: on";
  AddButton(more, "presetauto", everyText, 0, m_aRowHandlers, true);
  AddButton(more, "presetexport", "Copy to the clipboard", 0, m_aRowHandlers, true);
 }

 protected string PresetTyped()
 {
  if (m_PresetName) m_sPresetTyped = m_PresetName.GetValue();
  return m_sPresetTyped;
 }

 // A Presets tab button: true when it was one (the same requests as the classic dialog's).
 protected bool OnPresetButton(string key)
 {
  if (key.StartsWith("presetpick:"))
  {
   int index = key.Substring(11, key.Length() - 11).ToInt();
   if (index >= 0 && index < m_aPresetNames.Count())
   {
    m_sPresetSelected = m_aPresetNames[index];
    m_sPresetTyped = m_sPresetSelected;
    if (m_PresetName) m_PresetName.SetValue(m_sPresetTyped);
   }
   BuildRows(RowsShape());
   return true;
  }
  if (key == "presetsave") { ACR_SetupClient.Request("preset:save:" + PresetTyped()); return true; }
  if (key == "presetimport") { ACR_SetupClient.Request("preset:import:" + System.ImportFromClipboard()); return true; }
  if (!key.StartsWith("preset") || m_sPresetSelected.IsEmpty()) return key.StartsWith("preset");
  if (key == "presetload") ACR_SetupClient.Request("preset:load:" + m_sPresetSelected);
  else if (key == "presetdelete") ACR_SetupClient.Request("preset:delete:" + m_sPresetSelected);
  else if (key == "presetauto") ACR_SetupClient.Request("preset:auto:" + m_sPresetSelected);
  else if (key == "presetexport") ACR_SetupClient.Request("preset:export:" + m_sPresetSelected);
  else if (key == "presetrename")
  {
   string to = PresetTyped();
   ACR_SetupClient.Request("preset:rename:" + m_sPresetSelected + "\t" + to);
   m_sPresetSelected = ACR_SetupPresets.CleanName(to);
  }
  return true;
 }

 // ---------------------------------------------------------------- footer
 // Host: preset picker + Load, Reset tab. Everyone else: their deck builder (when decks are on).
 protected void FillFooter(bool rebuild)
 {
  if (rebuild)
  {
   while (m_wFooter.GetChildren()) m_wFooter.GetChildren().RemoveFromHierarchy();
   array<SCR_ButtonBaseComponent> gone = {};
   foreach (SCR_ButtonBaseComponent button, string key : m_Buttons) if (key.StartsWith("footer")) gone.Insert(button);
   foreach (SCR_ButtonBaseComponent old : gone)
   {
    m_Buttons.Remove(old);
    m_MonoButtons.Remove(SCR_ButtonTextComponent.Cast(old));
   }
   m_MonoLabels.Remove("footerpreset");
   m_MonoValues.Remove("footerpreset");
   m_FooterPreset = null;
   m_sFooterSig = "";
   // The skin's keys line (real bindings only) on its own line under the footer: beside the
   // buttons it squeezed them together at 1080p and on the Cold War terminal (20 Nov).
   KeysLine();
   array<string> keys = {};
   ACR_SetupLobbyModel.FooterKeys(m_bAllowed, keys);
   foreach (string part : keys)
   {
    if (part == "footerpreset")
    {
     Widget spin = GetGame().GetWorkspace().CreateWidgets(LAYOUT_SPIN, m_wFooter);
     if (!spin) continue;
     LayoutSlot.SetSizeMode(spin, LayoutSizeMode.Fill);
     AlignableSlot.SetPadding(spin, 0, 0, 8, 0);
     SpinLook(spin, m_Theme);
     m_FooterPreset = SCR_SpinBoxComponent.Cast(spin.FindHandler(SCR_SpinBoxComponent));
     if (m_FooterPreset)
     {
      m_FooterPreset.SetLabel(m_Theme.Text("Preset to load"));
      m_FooterPreset.SetCycleMode(true);
      m_FooterPreset.m_OnChanged.Insert(OnFooterPreset);
      Widget hints = spin.FindAnyWidget("HintBar");
      if (hints) hints.SetVisible(false);
     }
     Watch(spin, part, m_aFixedHandlers);
     continue;
    }
    SCR_ButtonTextComponent made = AddButton(m_wFooter, part, FooterText(part), 0, m_aFixedHandlers);
    if (!made) continue;
    // Room between the footer's buttons (they touched with the rows' 2 px).
    AlignableSlot.SetPadding(made.GetRootWidget(), 12, 2, 0, 2);
    AlignableSlot.SetVerticalAlign(made.GetRootWidget(), LayoutVerticalAlign.Center);
    // Room round the words inside the button (20 Nov: Load preset and Reset tab filled their boxes
    // edge to edge); the button grows with its label.
    TextWidget words = made.GetTextWidget();
    if (words) AlignableSlot.SetPadding(words, 16, 4, 16, 4);
    // Start stands out in the skin's strong colour; its key badge sits right of it.
    if (part != "footerstart") continue;
    PaintStart(made);
    // On a device, Start stands apart from the secondary controls.
    if (m_Theme.m_bDevice) AlignableSlot.SetPadding(made.GetRootWidget(), 40, 2, 0, 2);
   }
  }
  foreach (SCR_ButtonBaseComponent footerButton, string footerKey : m_Buttons)
  {
   if (footerKey == "footerreset") footerButton.SetVisible(m_iTab != ACR_Setup.LOBBY_PRESETS, false);
   if (footerKey == "footerdecks") footerButton.SetVisible(ACR_SetupLobbyModel.Find(m_aRows, "deckeditor") != null, false);
  }
  if (!m_FooterPreset) return;
  // The presets the server lists; the one in use is picked when it is among them.
  array<string> names = {};
  foreach (ACR_SetupLobbyRow row : m_aRows) if (row.m_sKey.StartsWith("presetrow:")) names.Insert(row.m_sKey.Substring(10, row.m_sKey.Length() - 10));
  string inUse = ACR_SetupLobbyModel.PresetInUse(m_aRows);
  string sig = inUse;
  foreach (string listed : names) sig += "|" + listed;
  if (sig == m_sFooterSig) return;
  m_sFooterSig = sig;
  m_bFilling = true;
  m_aFooterPresets.Copy(names);
  m_FooterPreset.ClearAll();
  int pick = 0;
  string loaded = ACR_SetupLobbyModel.LoadedPreset(m_aRows);
  foreach (int i, string name : names)
  {
   // The loaded one says so, so the picker and the right panel's "Loaded preset" agree.
   if (name == loaded)
   {
    m_FooterPreset.AddItem(m_Theme.Text(name + " (loaded)"));
    pick = i;
   }
   else m_FooterPreset.AddItem(m_Theme.Text(name));
  }
  if (names.IsEmpty()) m_FooterPreset.AddItem("(no presets)");
  m_FooterPreset.SetCurrentItem(pick, false, false, false);
  HideItemBars(m_FooterPreset.GetRootWidget());
  if (pick < names.Count()) m_sFooterPick = names[pick];
  else m_sFooterPick = "";
  MonoRow("footerpreset", m_FooterPreset.GetRootWidget(), m_Theme.Text("Preset to load"), m_FooterPreset.GetItemName(pick), m_Theme.m_Text);
  m_bFilling = false;
 }

 protected string FooterText(string key)
 {
  if (key == "footerload") return "Load preset";
  if (key == "footerreset") return "Reset tab";
  if (key == "footerstart") return m_Theme.StartLabel();
  return "Build my deck";
 }

 // The skin's keys line, once, under the footer row.
 protected void KeysLine()
 {
  if (m_wKeys || m_Theme.m_sKeys.IsEmpty()) return;
  Widget footerRow = m_wFooter.GetParent();
  if (!footerRow || !footerRow.GetParent()) return;
  if (m_Theme.m_bDevice) m_wKeys = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(ACR_SetupTheme.HEADING_MONO, footerRow.GetParent()));
  else m_wKeys = AddText(footerRow.GetParent(), m_Theme.m_sKeys, 0);
  if (!m_wKeys) return;
  m_wKeys.SetText(m_Theme.m_sKeys);
  if (m_Theme.m_bDevice) MonoFont(m_wKeys, 13);
  m_wKeys.SetColor(m_Theme.m_Decor);
  AlignableSlot.SetPadding(m_wKeys, 0, 4, 0, 0);
 }

 protected void OnFooterPreset(SCR_SpinBoxComponent spin, int index)
 {
  if (m_bFilling || index < 0 || index >= m_aFooterPresets.Count()) return;
  m_sFooterPick = m_aFooterPresets[index];
  MonoRow("footerpreset", spin.GetRootWidget(), m_Theme.Text("Preset to load"), spin.GetItemName(index), m_Theme.m_Text);
 }

 // ---------------------------------------------------------------- right panel
 protected void FillSide()
 {
  string west, east;
  ACR_SetupLobbyModel.Sides(m_aRows, west, east);
  if (m_wSummary)
  {
   string summary = ACR_SetupLobbyModel.Summary(m_aRows);
   string loaded = ACR_SetupLobbyModel.LoadedLine(m_aRows);
   if (!loaded.IsEmpty()) summary += "\n" + m_Theme.Text(loaded);
   m_wSummary.SetText(summary);
  }
  if (m_wSubtitle)
  {
   if (m_bAllowed) m_wSubtitle.SetText("Pick a tab, change a row with its arrows (or left and right), then press " + m_Theme.StartLabel() + ". A dot marks a change from this server's default.");
   else m_wSubtitle.SetText("Waiting for the host or an admin to set up the match and press Start. You can look; only they can change it.");
  }
  if (m_wStatus)
  {
   string status = m_sInfo;
   if (!status.IsEmpty()) status += "\n\n";
   m_wStatus.SetText(status + REPORT);
  }
  ShowHelp();
  DrawMap(west, east);
 }

 // A control took or lost the focus (its handler): the help follows the focus, the open tab's
 // description when nothing has it.
 void Focused(string key, bool has)
 {
  if (has) m_Help.Focus(key);
  else m_Help.Blur(key);
  // A row keeps its place for Refocus and the cursor even while a tab or footer control has focus.
  if (has && !key.StartsWith("tab:") && !key.StartsWith("footer") && !key.StartsWith("preset")) Describe(key);
  else ShowHelp();
 }

 // A row is the one being worked on (focus, or one of its arrows clicked): the skin's cursor moves
 // to it and the right panel describes it.
 void Describe(string key)
 {
  string before = m_sFocus;
  m_sFocus = key;
  if (!m_Theme.m_sCursor.IsEmpty() && before != key)
  {
   Relabel(before);
   Relabel(key);
  }
  ShowHelp();
 }

 // The right panel: what the focused control does.
 protected void ShowHelp()
 {
  if (m_wHelp) m_wHelp.SetText(ACR_SetupLobbyModel.HelpFor(m_aRows, m_Help.Key(m_iTab)));
 }

 // What the right panel describes now (tests).
 string HelpKey() { return m_Help.Key(m_iTab); }

 protected void Relabel(string key)
 {
  SCR_SpinBoxComponent spin = m_Spins.Get(key);
  ACR_SetupLobbyRow row = ACR_SetupLobbyModel.Find(m_aRows, key);
  if (spin && row) SetSpin(spin, row);
 }

 // A sketch of the planned layout from the base preview the server sends the host (ACR_Setup.SendPreview):
 // squares in the sides' colours (bigger for HQs), grey for neutral bases, green for other factions.
 protected void DrawMap(string west, string east)
 {
  if (!m_wMapDots) return;
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_MapData data;
  if (pc) data = pc.ACR_GetData();
  int count = 0;
  if (data) count = data.m_BasePositions.Count();
  string sig = string.Format("%1|%2|%3|%4", count, west, east, m_bAllowed);
  if (data && count > 0) sig += "|" + data.m_BaseNames[0] + data.m_BasePositions[0].ToString() + data.m_BaseNames[count - 1] + data.m_BaseOwners[count - 1] + data.m_BaseOwners[0];
  if (data && count > 0 && data.m_BaseDetails.Count() == count) sig += "|" + data.m_BaseDetails[0] + data.m_BaseDetails[count - 1];
  if (sig == m_sMapSig) return;
  m_sMapSig = sig;
  while (m_wMapDots.GetChildren()) m_wMapDots.GetChildren().RemoveFromHierarchy();
  if (count == 0 || !GetGame().GetWorld())
  {
   string empty = "The planned layout shows here.";
   if (!m_bAllowed) empty = "The host sees the planned layout here.";
   TextWidget note = AddText(m_wMapDots, empty, 0);
   if (note)
   {
    note.SetColor(m_Theme.m_TextDim);
    FrameSlot.SetAnchorMin(note, 0.5, 0.5);
    FrameSlot.SetAnchorMax(note, 0.5, 0.5);
    FrameSlot.SetAlignment(note, 0.5, 0.5);
    FrameSlot.SetSizeToContent(note, true);
   }
   return;
  }
  vector mins, maxs;
  GetGame().GetWorld().GetBoundBox(mins, maxs);
  float width = Math.Max(maxs[0] - mins[0], 1);
  float depth = Math.Max(maxs[2] - mins[2], 1);
  bool names = count <= 24;
  // Two passes: ordinary bases first, then the HQs on top with an outline, so a neighbour's square
  // never hides one (20 Nov: the red HQ sat against a base's square at the same size).
  array<int> order = {};
  array<int> hqs = {};
  for (int b = 0; b < count; b++)
  {
   if (b < data.m_BaseDetails.Count() && ACR_SetupLobbyModel.IsHQDetail(data.m_BaseDetails[b])) hqs.Insert(b);
   else order.Insert(b);
  }
  order.InsertAll(hqs);
  foreach (int i : order)
  {
   string detail = "";
   if (i < data.m_BaseDetails.Count()) detail = data.m_BaseDetails[i];
   bool hq = ACR_SetupLobbyModel.IsHQDetail(detail);
   vector pos = data.m_BasePositions[i];
   float x = 0.04 + 0.92 * Math.Clamp((pos[0] - mins[0]) / width, 0, 1);
   float y = 0.04 + 0.92 * (1 - Math.Clamp((pos[2] - mins[2]) / depth, 0, 1));
   string owner = data.m_BaseOwners[i];
   // HQs biggest, hidden sites smaller; the colours are the side panels' (the quick start split
   // shows as owners).
   float size = ACR_SetupLobbyModel.MarkerSize(detail);
   if (hq) Square(x, y, size + 6, m_Theme.m_Heading);
   Widget dot = Square(x, y, size, m_Theme.Owner(owner, west, east));
   if (!dot) continue;
   if (!names) continue;
   TextWidget label = AddText(m_wMapDots, data.m_BaseNames[i], 0);
   if (!label) continue;
   label.SetExactFontSize(13);
   label.SetColor(m_Theme.m_TextDim);
   FrameSlot.SetAnchorMin(label, x, y);
   FrameSlot.SetAnchorMax(label, x, y);
   FrameSlot.SetAlignment(label, 0, 0.5);
   FrameSlot.SetPos(label, size * 0.5 + 3, 0);
   FrameSlot.SetSizeToContent(label, true);
  }
 }

 // A square on the map sketch, centred on (x, y) of the map's frame.
 protected Widget Square(float x, float y, float size, Color color)
 {
  Widget dot = GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, color, 0, m_wMapDots);
  if (!dot) return null;
  FrameSlot.SetAnchorMin(dot, x, y);
  FrameSlot.SetAnchorMax(dot, x, y);
  FrameSlot.SetAlignment(dot, 0.5, 0.5);
  FrameSlot.SetPos(dot, 0, 0);
  FrameSlot.SetSize(dot, size, size);
  return dot;
 }

 // ---------------------------------------------------------------- widgets
 protected ACR_SetupLobbyHandler Watch(Widget w, string key, notnull array<ref ACR_SetupLobbyHandler> keep)
 {
  ACR_SetupLobbyHandler handler = new ACR_SetupLobbyHandler();
  handler.m_Lobby = this;
  handler.m_sKey = key;
  w.AddHandler(handler);
  keep.Insert(handler);
  return handler;
 }

 protected TextWidget AddText(Widget parent, string text, float indent)
 {
  ResourceName layout = LAYOUT_TEXT;
  if (m_Theme.m_bMonoBody && parent != m_wMapDots) layout = ACR_SetupTheme.HEADING_MONO;
  TextWidget widget = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(layout, parent));
  if (!widget) return null;
  if (layout == ACR_SetupTheme.HEADING_MONO) MonoFont(widget, 15);
  if (parent != m_wMapDots) AlignableSlot.SetPadding(widget, indent, 4, 4, 4);
  widget.SetText(text);
  widget.SetColor(m_Theme.m_Text);
  return widget;
 }

 protected SCR_ButtonTextComponent AddButton(Widget parent, string key, string text, float indent, notnull array<ref ACR_SetupLobbyHandler> keep, bool fill = false)
 {
  Widget widget = GetGame().GetWorkspace().CreateWidgets(LAYOUT_BUTTON, parent);
  if (!widget) return null;
  AlignableSlot.SetPadding(widget, 2 + indent, 2, 2, 2);
  if (fill) LayoutSlot.SetSizeMode(widget, LayoutSizeMode.Fill);
  SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(widget);
  if (!button) return null;
  ButtonText(button, m_Theme.Text(text));
  Paint(button, false);
  Widget border = widget.FindAnyWidget("Border");
  if (border) border.SetColor(m_Theme.m_ButtonBorder);
  button.m_OnClicked.Insert(OnButton);
  m_Buttons.Set(button, key);
  Watch(widget, key, keep);
  if (!key.StartsWith("tab:") && !key.StartsWith("footer")) m_RowWidgets.Set(key, widget);
  return button;
 }

 // The skin's button colours (the vanilla hover is white behind white text); the open tab stands out.
 // On a device skin (tablet, terminal): tabs are text-only on the rail, other buttons thin
 // outlines; the open tab and Start are the only filled ones. The focus border stays on, so
 // keyboard and gamepad players always see where they are.
 protected void Paint(SCR_ButtonBaseComponent button, bool selected, bool tab = false)
 {
  Color normal = m_Theme.m_Button;
  Color hover = m_Theme.m_ButtonHover;
  Color press = m_Theme.m_ButtonPress;
  Color text = m_Theme.m_ButtonText;
  if (selected)
  {
   // The open tab keeps its fill under the pointer: the terminal's dark label needs its light ground.
   normal = m_Theme.m_ButtonSelected;
   hover = normal;
   press = normal;
   text = m_Theme.m_ButtonSelectedText;
  }
  Fill(button, normal, hover, press, text);
  if (!m_Theme.m_bDevice) return;
  SCR_ButtonTextComponent textButton = SCR_ButtonTextComponent.Cast(button);
  if (!textButton) return;
  if (textButton.m_wBorder)
  {
   textButton.m_wBorder.SetColor(m_Theme.m_Line);
   // Rail tabs show the border only on focus; other buttons are thin outlines.
   if (tab) textButton.m_wBorder.SetOpacity(0);
   else textButton.m_wBorder.SetOpacity(1);
  }
  textButton.m_bShowBorderOnHover = false;
  Compact(textButton.GetRootWidget(), FitHeight(16));
  TextWidget label = textButton.GetTextWidget();
  if (!label) return;
  label.SetExactFontSize(16);
  if (tab) AlignableSlot.SetHorizontalAlign(label, LayoutHorizontalAlign.Left);
 }

 // Start: the one filled button of the footer.
 protected void PaintStart(SCR_ButtonBaseComponent button)
 {
  Fill(button, m_Theme.m_Start, m_Theme.m_StartHover, m_Theme.m_StartHover, m_Theme.m_StartText);
  SCR_ButtonTextComponent textButton = SCR_ButtonTextComponent.Cast(button);
  if (m_Theme.m_bDevice && textButton && textButton.m_wBorder) textButton.m_wBorder.SetColor(m_Theme.m_StartHover);
 }

 protected void Fill(SCR_ButtonBaseComponent button, Color normal, Color hover, Color press, Color text)
 {
  button.SetBackgroundColors(normal, hover, press, press, press);
  if (button.m_wBackground) button.m_wBackground.SetColor(normal);
  SCR_ButtonTextComponent textButton = SCR_ButtonTextComponent.Cast(button);
  if (!textButton || !text) return;
  TextWidget label = textButton.GetTextWidget();
  if (label) label.SetColor(text);
  textButton.m_TextDefault = text;
  textButton.m_TextToggled = text;
 }

 // A control's height for its font: the text's own height (descenders included: 25 px at 16 pt,
 // measured) and room above and below (20 Nov: rows of 34 px and buttons of 32 cut the descenders).
 static float FitHeight(int fontSize)
 {
  return Math.Ceil(fontSize * 1.6) + 16;
 }

 // The monospace font draws far bigger than its size says (measured 20 Nov: at 15 its line is
 // 37.5 px, the body font's 25 px at 16), which clipped and spilled the terminal's text. Its sizes go
 // through this scale, measured once against the body font (bare static: initializer budget).
 protected static float s_fMonoScale;
 static float MonoScale()
 {
  if (s_fMonoScale > 0) return s_fMonoScale;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  if (!ws) return 1;
  TextWidget mono = TextWidget.Cast(ws.CreateWidgets(ACR_SetupTheme.HEADING_MONO));
  TextWidget body = TextWidget.Cast(ws.CreateWidgets(LAYOUT_TEXT));
  float monoW, monoH, bodyW, bodyH;
  if (mono && body)
  {
   mono.SetExactFontSize(20);
   body.SetExactFontSize(20);
   mono.SetText("Mg");
   body.SetText("Mg");
   mono.GetTextSize(monoW, monoH);
   body.GetTextSize(bodyW, bodyH);
  }
  if (mono) mono.RemoveFromHierarchy();
  if (body) body.RemoveFromHierarchy();
  if (monoH > 0 && bodyH > 0) s_fMonoScale = bodyH / monoH;
  else return 1;
  return s_fMonoScale;
 }

 // A monospace text at the size it should look.
 static void MonoFont(TextWidget w, int visual)
 {
  if (w) w.SetExactFontSize(Math.Max(6, Math.Round(visual * MonoScale())));
 }

 // A monospace character's width and a line's height at a look size, in layout units (the tooltip
 // wraps and sizes itself from these).
 static void MonoMetrics(int visual, out float charWidth, out float lineHeight)
 {
  charWidth = visual * 0.62;
  lineHeight = visual * 1.6;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  if (!ws) return;
  TextWidget probe = TextWidget.Cast(ws.CreateWidgets(ACR_SetupTheme.HEADING_MONO));
  if (!probe) return;
  MonoFont(probe, visual);
  float w, h;
  probe.SetText("MMMMMMMMMM");
  probe.GetTextSize(w, h);
  if (w > 0) charWidth = ws.DPIUnscale(w) / 10;
  if (h > 0) lineHeight = ws.DPIUnscale(h);
  probe.RemoveFromHierarchy();
 }

 // A widget library control's own height (its "SizeLayout" child), for the denser device skins.
 protected static void Compact(Widget control, float height)
 {
  if (!control) return;
  SizeLayoutWidget size = SizeLayoutWidget.Cast(control.FindAnyWidget("SizeLayout"));
  if (!size) return;
  size.EnableHeightOverride(true);
  size.SetHeightOverride(height);
 }

 // ---------------------------------------------------------------- Match: the two sides
 // The two chosen factions facing each other, Blue left and Red right in the side colours, each
 // with whether an AI commands it and its doctrine and company.
 protected void BuildSides()
 {
  Widget line = GetGame().GetWorkspace().CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_wRows);
  AlignableSlot.SetPadding(line, 4, 0, 4, 10);
  SidePanel(line, true);
  TextWidget vs = AddText(line, m_Theme.Text("vs"), 0);
  if (vs)
  {
   vs.SetColor(m_Theme.m_Heading);
   AlignableSlot.SetVerticalAlign(vs, LayoutVerticalAlign.Center);
   AlignableSlot.SetPadding(vs, 10, 0, 10, 0);
  }
  SidePanel(line, false);
 }

 protected void SidePanel(Widget parent, bool west)
 {
  Widget panel = GetGame().GetWorkspace().CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent);
  LayoutSlot.SetSizeMode(panel, LayoutSizeMode.Fill);
  Color fill = m_Theme.m_EastPanel;
  Color edge = m_Theme.m_East;
  if (west)
  {
   fill = m_Theme.m_WestPanel;
   edge = m_Theme.m_West;
  }
  Widget back = GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, fill, 0, panel);
  AlignableSlot.SetHorizontalAlign(back, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(back, LayoutVerticalAlign.Stretch);
  ImageWidget stripe = ImageWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, edge, 0, panel));
  if (stripe)
  {
   AlignableSlot.SetVerticalAlign(stripe, LayoutVerticalAlign.Stretch);
   AlignableSlot.SetHorizontalAlign(stripe, LayoutHorizontalAlign.Left);
   stripe.SetSize(6, 6);
  }
  Widget column = GetGame().GetWorkspace().CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, panel);
  AlignableSlot.SetPadding(column, 16, 8, 8, 8);
  AlignableSlot.SetHorizontalAlign(column, LayoutHorizontalAlign.Stretch);
  array<string> lines = {};
  SideText(west).Split("\n", lines, false);
  foreach (int i, string text : lines)
  {
   TextWidget widget = AddText(column, text, 0);
   if (!widget) continue;
   AlignableSlot.SetPadding(widget, 0, 1, 0, 1);
   if (i == 0)
   {
    widget.SetColor(edge);
    if (m_Theme.m_bMonoBody) MonoFont(widget, 20);
    else widget.SetExactFontSize(22);
   }
  }
 }

 // "United States\nBlue side\nAI commander: on\nDoctrine: ...\nCompany: ..." from the rows.
 protected string SideText(bool west)
 {
  string side = "east";
  string colour = "Red side";
  if (west)
  {
   side = "west";
   colour = "Blue side";
  }
  ACR_SetupLobbyRow name = ACR_SetupLobbyModel.Find(m_aRows, side);
  string faction = "?";
  if (name) faction = name.m_sValue;
  string text = m_Theme.Text(faction) + "\n" + m_Theme.Text(colour);
  text += "\n" + m_Theme.Text("AI commander: " + AICommands(faction));
  ACR_SetupLobbyRow doctrine = ACR_SetupLobbyModel.Find(m_aRows, "doctrine" + side);
  if (doctrine) text += "\n" + m_Theme.Text("Doctrine: " + doctrine.m_sValue);
  ACR_SetupLobbyRow company = ACR_SetupLobbyModel.Find(m_aRows, "company" + side);
  if (company) text += "\n" + m_Theme.Text("Company: " + company.m_sValue);
  return text;
 }

 // The AI commanders row read for one side: "on", "off" or "scenario's choice".
 protected string AICommands(string faction)
 {
  ACR_SetupLobbyRow ai = ACR_SetupLobbyModel.Find(m_aRows, "ai");
  if (!ai) return "scenario's choice";
  if (ai.m_sValue == "both") return "on";
  if (ai.m_sValue == "none") return "off";
  if (ai.m_sValue.EndsWith(" only"))
  {
   if (ai.m_sValue == faction + " only") return "on";
   return "off";
  }
  return ai.m_sValue;
 }

 protected void OnButton(SCR_ButtonBaseComponent button)
 {
  string key;
  if (!m_Buttons.Find(button, key)) return;
  if (key.StartsWith("tab:")) { ShowTab(key.Substring(4, key.Length() - 4).ToInt()); return; }
  if (key == "map") { ACR_SetupClient.OpenPreviewMap(); return; }
  if (key == "deckeditor" || key == "footerdecks") { ACR_DeckClient.OpenFromSetup(); return; }
  if (!m_bAllowed) return;
  if (key == "footerload")
  {
   if (!m_sFooterPick.IsEmpty()) ACR_SetupClient.Request("preset:load:" + m_sFooterPick);
   return;
  }
  if (key == "footerreset") { ACR_SetupClient.Request("resettab:" + m_iTab.ToString()); return; }
  if (key == "footerstart") { OnConfirm(); return; }
  if (OnPresetButton(key)) return;
  ACR_SetupClient.Request(key);
 }

 // Start (Space, or Y on a pad: not Enter/A, which press the focused row): the server ends the phase.
 override protected void OnConfirm()
 {
  if (m_bAllowed) ACR_SetupClient.Request("start");
 }

 override protected void OnCancel()
 {
 }
}
