// -acrPanelShotsTest 1 (a game client, not srv; ~5 min): the movable panels and the key reminders, with
// shots. Real mouse input comes from outside: each "[ACR] PANELSHOTS DO" line asks for a drag or click
// in screen pixels of the game's window. Checks: the strip shows off the map; every movable panel moves
// when dragged by what the player would press, its spot is saved; a button still clicks and turns the
// strip off and on; a click on an INTEL line still jumps the map; a folded panel moves by its bar; a
// double click puts each back. A hint: it shows on the map, drawn over the panels, none is under it, a
// button still clicks meanwhile, and each panel is back where it was after, the player's spot unchanged
// (the playtest's: x -6, y 9); an armed order's note stands clear of the panel. Everything goes through a test file (UseTestFileForTest): the player's
// own prefs are never read after the start, written or deleted, even if this test is cut short.
class ACR_TestPanelShots
{
 protected static const int FIRST_DRAG = 34;
 protected static const int DRAG_TICKS = 7;
 protected static const int BACK_TICKS = 5;
 protected static vector s_Before;

 // The panels to drag, and where to (their top left corner, reference pixels on a 1920 x 1080 screen).
 protected static string Id(int i)
 {
  array<string> ids = {ACR_UiPrefs.PANEL, ACR_UiPrefs.FEED, ACR_UiPrefs.SQUADS, ACR_UiPrefs.CARD, ACR_UiPrefs.CHANNEL, ACR_UiPrefs.LAYERS, ACR_UiPrefs.DEBUG_BOX};
  if (i < ids.Count()) return ids[i];
  return "";
 }
 protected static vector Target(int i)
 {
  array<vector> spots = {Vector(1180, 60, 0), Vector(520, 60, 0), Vector(720, 300, 0), Vector(110, 300, 0), Vector(110, 620, 0), Vector(700, 1000, 0), Vector(1250, 860, 0)};
  return spots[i];
 }
 protected static const int PANELS = 7;

 static void Run(ACR_TestHarness h, int tick)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (tick == 2)
  {
   // The 2IC's card and channel need the 2IC on; the Squads list needs a squad to command.
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Options.Set("acr2IC", "1");
  }
  if (!pc || tick < 4) return;
  if (tick == 4)
  {
   ACR_UiPrefs.UseTestFileForTest();
   ACR_Manager manager = ACR_Manager.GetInstance();
   ACR_Intel intel = null;
   if (manager) intel = manager.IntelFor("US");
   if (intel && h.m_Range) intel.AddFeed(GetGame().GetWorld().GetWorldTime(), "Alpha 1: enemy rifle squad near the test range", h.m_Range.m_Rally + Vector(300, 0, 300));
  }
  WorkspaceWidget ws = GetGame().GetWorkspace();
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  ACR_CommandPanel panel = pc.ACR_CommandPanelForTest();
  if (tick == 20)
  {
   ACR_TestRegress.Check("panelshots: the key strip shows off the map", ACR_KeyHints.ShownForTest(), "");
   Shot("panel_1_strip");
  }
  if (tick == 22) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (tick == 24 && mapEntity) mapEntity.ZoomPanSmooth(ws.GetWidth() / 2400.0, h.m_Range.m_Rally[0], h.m_Range.m_Rally[2], 0.1);
  if (tick == 28)
  {
   pc.ACR_TestTwoICShown(true);
   if (!pc.ACR_IsCommanderForHints()) pc.ACR_OnPanelAction(ACR_EPanelAction.TOGGLE_COMMAND);
  }
  if (tick == 32)
  {
   Shot("panel_2_home");
   for (int s = 0; s < PANELS; s++) ACR_TestRegress.Check("panelshots: shows at the start: " + Id(s), ACR_PanelMover.ShowsForTest(Id(s)), "");
  }

  // Each panel in turn: a drag by what the player would press, and it stands there.
  int dragAt = tick - FIRST_DRAG;
  if (dragAt >= 0 && dragAt < PANELS * DRAG_TICKS)
  {
   int i = dragAt / DRAG_TICKS;
   if (dragAt % DRAG_TICKS == 0)
   {
    s_Before = ACR_PanelMover.TopLeftForTest(Id(i));
    vector target = Target(i);
    // A real drag, so a panel that doesn't show now fails below: nothing moves.
    Ask("drag", ACR_PanelMover.GripForTest(Id(i)), ws.DPIScale(target[0] - s_Before[0]), ws.DPIScale(target[1] - s_Before[1]));
   }
   if (dragAt % DRAG_TICKS == DRAG_TICKS - 1)
   {
    vector now = ACR_PanelMover.TopLeftForTest(Id(i));
    vector want = Target(i);
    bool there = ACR_UiPrefs.Moved(Id(i)) && Math.AbsFloat(now[0] - want[0]) < 30 && Math.AbsFloat(now[1] - want[1]) < 30;
    ACR_TestRegress.Check("panelshots: a drag moves it: " + Id(i), there, string.Format("%1 -> %2, wanted %3", s_Before, now, want));
    FileHandle file = FileIO.OpenFile(ACR_UiPrefs.TEST_PREFS, FileMode.READ);
    bool saved;
    string line;
    while (file && file.ReadLine(line) >= 0)
    {
     if (line.StartsWith("place." + Id(i) + "=")) saved = true;
    }
    if (file) file.Close();
    ACR_TestRegress.Check("panelshots: its spot is saved: " + Id(i), saved, "");
   }
  }
  int after = FIRST_DRAG + PANELS * DRAG_TICKS;

  // The moved panels: their buttons and INTEL's lines still take their clicks.
  if (tick == after)
  {
   Shot("panel_3_moved");
   if (panel) Ask("click", panel.KeyHintsButtonForTest(), 0, 0);
  }
  if (tick == after + 6)
  {
   ACR_TestRegress.Check("panelshots: the button still clicks: reminders off", !ACR_UiPrefs.s_bKeyHints, "");
   if (panel) Ask("click", panel.KeyHintsButtonForTest(), 0, 0);
  }
  if (tick == after + 12)
  {
   ACR_TestRegress.Check("panelshots: and on again", ACR_UiPrefs.s_bKeyHints, "");
   if (panel && panel.FeedLineForTest() != vector.Zero) Ask("click", panel.FeedLineForTest(), 0, 0);
  }
  if (tick == after + 18)
  {
   ACR_TestRegress.Check("panelshots: a click on an INTEL line still jumps the map", pc.ACR_FeedJumpedForTest(), "");
   if (panel) ACR_TestRegress.Check("panelshots: INTEL's title bar stops short of its first line", panel.FeedTitleClearForTest(), "");
   pc.ACR_TwoICTogglePanel();
  }

  // The 2IC panel, by its header.
  if (tick == after + 21)
  {
   s_Before = ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.TWOIC_PANEL);
   Ask("drag", ACR_PanelMover.GripForTest(ACR_UiPrefs.TWOIC_PANEL), ws.DPIScale(-120), ws.DPIScale(80));
  }
  if (tick == after + 27)
  {
   vector moved = ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.TWOIC_PANEL);
   ACR_TestRegress.Check("panelshots: a drag moves it: the 2IC panel", ACR_UiPrefs.Moved(ACR_UiPrefs.TWOIC_PANEL) && moved != s_Before, string.Format("%1 -> %2", s_Before, moved));
   Shot("panel_4_twoic_moved");
   Ask("double", ACR_PanelMover.GripForTest(ACR_UiPrefs.TWOIC_PANEL), 0, 0);
  }
  if (tick == after + 32)
  {
   ACR_TestRegress.Check("panelshots: a double click puts it back: the 2IC panel", !ACR_UiPrefs.Moved(ACR_UiPrefs.TWOIC_PANEL), "");
   pc.ACR_TwoICTogglePanel();
   // Folded: zoomed right out each panel is a bar.
   if (mapEntity) mapEntity.ZoomPanSmooth(mapEntity.GetMinZoom(), h.m_Range.m_Rally[0], h.m_Range.m_Rally[2], 0.1);
  }
  if (tick == after + 37)
  {
   ACR_TestRegress.Check("panelshots: zoomed out INTEL is its bar", panel && panel.DeclIntelCollapsed(), "");
   s_Before = ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.FEED);
   Ask("drag", ACR_PanelMover.GripForTest(ACR_UiPrefs.FEED), ws.DPIScale(60), ws.DPIScale(60));
  }
  if (tick == after + 43)
  {
   vector foldedNow = ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.FEED);
   ACR_TestRegress.Check("panelshots: a drag on INTEL's bar moves it", foldedNow != s_Before, string.Format("%1 -> %2", s_Before, foldedNow));
   Shot("panel_5_folded");
   if (mapEntity) mapEntity.ZoomPanSmooth(ws.GetWidth() / 2400.0, h.m_Range.m_Rally[0], h.m_Range.m_Rally[2], 0.1);
  }

  // A double click on each puts it back.
  int back = tick - (after + 47);
  if (back >= 0 && back < PANELS * BACK_TICKS)
  {
   int k = back / BACK_TICKS;
   if (back % BACK_TICKS == 0) Ask("double", ACR_PanelMover.GripForTest(Id(k)), 0, 0);
   if (back % BACK_TICKS == BACK_TICKS - 1) ACR_TestRegress.Check("panelshots: a double click puts it back: " + Id(k), !ACR_UiPrefs.Moved(Id(k)), "");
  }
  int done = after + 47 + PANELS * BACK_TICKS;
  if (tick == done) Shot("panel_6_back");
  RunHints(pc, panel, tick - done - 1);
  if (tick == done + 56) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  if (tick == done + 62)
  {
   ACR_TestRegress.Check("panelshots: the strip is back after the map", ACR_KeyHints.ShownForTest(), "");
   FileIO.DeleteFile(ACR_UiPrefs.TEST_PREFS);
   ACR_TestRegress.Complete("panelshots", 58);
  }
 }

 // The hint and the note. at: seconds from its start.
 protected static void RunHints(SCR_PlayerController pc, ACR_CommandPanel panel, int at)
 {
  // At home, nothing selected.
  if (at == 0) ShowHint("Alpha 1: fortifying. An order's hint, long enough to read as the real ones do, and to need more than one line of the box.");
  if (at == 3)
  {
   float hx, hy, hw, hh;
   ACR_TestRegress.Check("panelshots: a hint shows on the map", ACR_PanelMover.HintRect(hx, hy, hw, hh), ACR_PanelMover.HintPathForTest());
   ACR_TestRegress.Check("panelshots: the panel stays at home for a hint", ACR_PanelMover.ShownX(ACR_UiPrefs.PANEL) == ACR_UiPrefs.PANEL_HOME_X && ACR_PanelMover.ShownY(ACR_UiPrefs.PANEL) == ACR_UiPrefs.PANEL_HOME_Y, string.Format("%1, %2", ACR_PanelMover.ShownX(ACR_UiPrefs.PANEL), ACR_PanelMover.ShownY(ACR_UiPrefs.PANEL)));
   ACR_TestRegress.Check("panelshots: the hint stands beside the panel, not under it", ACR_PanelMover.ClearOfHintForTest(ACR_UiPrefs.PANEL), string.Format("%1", ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.PANEL)));
   ACR_TestRegress.Check("panelshots: the hint's layout is lifted over the panels", ACR_PanelMover.HintLiftedForTest(), ACR_PanelMover.HintPathForTest());
   Shot("panel_7_hint_home");
   SCR_HintManagerComponent.HideHint();
  }
  if (at == 8)
  {
   ACR_TestRegress.Check("panelshots: the hint's column is back in its place after", ACR_PanelMover.HintShiftForTest() == 0, string.Format("%1", ACR_PanelMover.HintShiftForTest()));
   ACR_TestRegress.Check("panelshots: the panel is still at home after the hint", ACR_PanelMover.ShownX(ACR_UiPrefs.PANEL) == ACR_UiPrefs.PANEL_HOME_X && ACR_PanelMover.ShownY(ACR_UiPrefs.PANEL) == ACR_UiPrefs.PANEL_HOME_Y, string.Format("%1, %2", ACR_PanelMover.ShownX(ACR_UiPrefs.PANEL), ACR_PanelMover.ShownY(ACR_UiPrefs.PANEL)));
   // The spot from the playtest: dragged to the top right, over the hint.
   ACR_UiPrefs.SetSpot(ACR_UiPrefs.PANEL, -6, 9);
   ShowHint("Alpha 1: fortifying. The same, with the panel on the player's own spot right over the hint box.");
  }
  if (at == 12)
  {
   float spotX, spotY;
   ACR_UiPrefs.Spot(ACR_UiPrefs.PANEL, spotX, spotY);
   ACR_TestRegress.Check("panelshots: a moved panel stays on its spot for a hint", ACR_PanelMover.ShownX(ACR_UiPrefs.PANEL) == -6 && ACR_PanelMover.ShownY(ACR_UiPrefs.PANEL) == 9, string.Format("%1, %2", ACR_PanelMover.ShownX(ACR_UiPrefs.PANEL), ACR_PanelMover.ShownY(ACR_UiPrefs.PANEL)));
   ACR_TestRegress.Check("panelshots: the hint stands beside a moved panel", ACR_PanelMover.ClearOfHintForTest(ACR_UiPrefs.PANEL), string.Format("%1", ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.PANEL)));
   ACR_TestRegress.Check("panelshots: and the player's spot is unchanged", spotX == -6 && spotY == 9, string.Format("%1, %2", spotX, spotY));
   Shot("panel_8_hint_moved");
   s_bKeysBefore = ACR_UiPrefs.s_bKeyHints;
   if (panel) Ask("click", panel.KeyHintsButtonForTest(), 0, 0);
  }
  if (at == 17)
  {
   ACR_TestRegress.Check("panelshots: a button still clicks while a hint shows", ACR_UiPrefs.s_bKeyHints != s_bKeysBefore, "");
   ACR_UiPrefs.s_bKeyHints = s_bKeysBefore;
   if (panel) panel.RefreshKeyHints();
   SCR_HintManagerComponent.HideHint();
  }
  if (at == 22)
  {
   ACR_TestRegress.Check("panelshots: the moved panel returns to the player's spot", ACR_PanelMover.ShownX(ACR_UiPrefs.PANEL) == -6 && ACR_PanelMover.ShownY(ACR_UiPrefs.PANEL) == 9, string.Format("%1, %2", ACR_PanelMover.ShownX(ACR_UiPrefs.PANEL), ACR_PanelMover.ShownY(ACR_UiPrefs.PANEL)));
   Shot("panel_9_returned");
   ACR_UiPrefs.Forget(ACR_UiPrefs.PANEL);
   // Any panel: INTEL put where the hint comes.
   ACR_UiPrefs.SetSpot(ACR_UiPrefs.FEED, 1500, 100);
   ShowHint("Alpha 1: fortifying. The same, with INTEL put where the hint comes.");
  }
  if (at == 26)
  {
   ACR_TestRegress.Check("panelshots: the hint stands beside INTEL too", ACR_PanelMover.ClearOfHintForTest(ACR_UiPrefs.FEED), string.Format("%1", ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.FEED)));
   SCR_HintManagerComponent.HideHint();
  }
  if (at == 31)
  {
   ACR_TestRegress.Check("panelshots: and INTEL stays there", ACR_PanelMover.ShownX(ACR_UiPrefs.FEED) == 1500 && ACR_PanelMover.ShownY(ACR_UiPrefs.FEED) == 100, string.Format("%1, %2", ACR_PanelMover.ShownX(ACR_UiPrefs.FEED), ACR_PanelMover.ShownY(ACR_UiPrefs.FEED)));
   ACR_UiPrefs.Forget(ACR_UiPrefs.FEED);
   // An armed order: its words beside the panel.
   ACR_SquadsList list = pc.ACR_SquadsListForTest();
   if (list && list.RowsForTest() > 0) pc.ACR_SquadsListPick(list.IdForTest(0));
  }
  if (at == 34) pc.ACR_OnPanelAction(ACR_EPanelAction.DEFEND);
  if (at == 37)
  {
   if (panel)
   {
    ACR_TestRegress.Check("panelshots: an armed order's words show", panel.NoteShownForTest(), "");
    ACR_TestRegress.Check("panelshots: beside the panel, not behind it", panel.NoteClearForTest(), "");
   }
   Shot("panel_10_armed_note");
   pc.ACR_OnPanelAction(ACR_EPanelAction.DEFEND);
  }
  if (at == 40 && panel) ACR_TestRegress.Check("panelshots: cancelled, the words go", !panel.NoteShownForTest(), "");
  // Radio lines, as a fight sends them: INTEL at home hid them.
  if (at == 41)
  {
   SCR_ChatPanelManager chat = SCR_ChatPanelManager.GetInstance();
   if (chat)
   {
    for (int line = 1; line <= 5; line++) chat.OnNewMessage(string.Format("[Radio] Alpha 1: contact, %1 infantry, 150 m E of Farm.", line));
   }
  }
  if (at == 43)
  {
   float home = ACR_PanelMover.ChatLeftForTest();
   ACR_TestRegress.Check("panelshots: radio lines stand clear of the panels at home", home > 0, string.Format("%1", home));
   Shot("panel_11_radio");
   // Where the lines went: they move on again.
   ACR_UiPrefs.SetSpot(ACR_UiPrefs.FEED, home, 20);
  }
  if (at == 46)
  {
   float moved = ACR_PanelMover.ChatLeftForTest();
   ACR_TestRegress.Check("panelshots: and of a panel put where they went", moved >= 0, string.Format("%1", moved));
   Shot("panel_12_radio_moved");
   ACR_UiPrefs.Forget(ACR_UiPrefs.FEED);
   // A player's row of panels across the top (pn25 playtest): no room left beside them for the lines.
   ACR_UiPrefs.SetSpot(ACR_UiPrefs.FEED, 76, 0);
   ACR_UiPrefs.SetSpot(ACR_UiPrefs.SQUADS, 445, 0);
   ACR_UiPrefs.SetSpot(ACR_UiPrefs.CARD, 900, 0);
   SCR_ChatPanelManager rowChat = SCR_ChatPanelManager.GetInstance();
   if (rowChat)
   {
    for (int rowLine = 1; rowLine <= 5; rowLine++) rowChat.OnNewMessage(string.Format("[Radio] Alpha 1: contact, %1 infantry, 150 m E of Farm.", rowLine));
   }
  }
  if (at == 49)
  {
   float row = ACR_PanelMover.ChatLeftForTest();
   ACR_TestRegress.Check("panelshots: radio lines stand clear of a row of panels too", row >= 0, string.Format("%1", row));
   Shot("panel_13_radio_row");
   ShowHint("AC: Actual: you are already in your command unit. The same, with a row of panels across the top.");
  }
  if (at == 52)
  {
   ACR_TestRegress.Check("panelshots: the hint stands beside the panel with a row across the top", ACR_PanelMover.ClearOfHintForTest(ACR_UiPrefs.PANEL) && ACR_PanelMover.ClearOfHintForTest(ACR_UiPrefs.CARD), ACR_PanelMover.HintPathForTest());
   Shot("panel_14_hint_row");
   SCR_HintManagerComponent.HideHint();
   ACR_UiPrefs.Forget(ACR_UiPrefs.FEED);
   ACR_UiPrefs.Forget(ACR_UiPrefs.SQUADS);
   ACR_UiPrefs.Forget(ACR_UiPrefs.CARD);
  }
 }

 protected static bool s_bKeysBefore;

 protected static void ShowHint(string text)
 {
  SCR_HintManagerComponent.ShowCustomHint(text, "AC: Actual", 30, true);
 }

 // The middle of the rectangle, and how far to drag from it.
 protected static void Ask(string what, vector rect, float dx, float dy)
 {
  PrintFormat("[ACR] PANELSHOTS DO %1 %2 %3 %4 %5", what, (int)rect[0], (int)rect[1], (int)dx, (int)dy);
 }

 protected static void Shot(string name)
 {
  FileIO.MakeDirectory("$profile:acr-shots");
  if (!FileIO.FileExists("$profile:acr-shots")) return;
  System.MakeScreenshot("$profile:acr-shots/" + name);
  PrintFormat("[ACR] PANELSHOTS ready %1", name);
 }
}
