// The 2IC panel (#14 v1), from the 2IC card's button or its key (F11, map only), laid out as the #14
// sketch's panel (ACR_TwoICStyle): a header (the dot, "2IC", what it runs, Close); on the left every
// squad of the side as a table (squad, where, doing, run by) with Take back (outlined) or Hand to 2IC
// (the accent), "You lead it" for the player's own, and Show (the map pans to it, selected); on the right the FOBs it may manage (a tick for
// each), the share of income set aside for its reserve (its build budget) with a slider and bar,
// whether it is a small force, and the type slot (later). The server sends the rows only while the
// panel is open (ACR_TwoIC.PanelFor); the buttons go through the squad panel's and FOB panel's own
// requests, so the server checks them as it does those.
class ACR_TwoICPanel : Managed
{
 static const float WIDTH = 1560;        // the sketch's 1180 px at 1440
 static const float SIDE_WIDTH = 470;
 static const float LIST_HEIGHT = 600;
 static const float COL_NAME = 270;
 static const int NAME_CHARS = 26;     // a squad's name longer than this is cut short to fit COL_NAME
 static const float COL_WHERE = 200;
 static const float COL_RUN = 100;
 static const float COL_ACTION = 210;
 static const float COL_SHOW = 90;      // Show: the panel closes and the map pans to the squad (playtest 30 Oct)
 static const float LIST_WIDTH = 1037;   // WIDTH - SIDE_WIDTH - the left column's padding - the divider
 static const float SIDE_BAR = 430;
 static const float SHARE_TRACK = 300;

 Widget m_Root;
 protected SCR_PlayerController m_Owner;
 protected Widget m_SquadList;
 protected Widget m_FobList;
 protected TextWidget m_Summary;
 protected TextWidget m_FobsNone;
 protected TextWidget m_ShareValue;
 protected TextWidget m_ReserveText;
 protected SizeLayoutWidget m_BarFill;
 protected SizeLayoutWidget m_BarRest;
 protected SizeLayoutWidget m_ShareFill;
 protected SizeLayoutWidget m_ShareRest;
 protected Widget m_SmallChip;
 protected TextWidget m_SmallChipText;
 protected TextWidget m_SmallText;
 protected TextWidget m_CommandHint;
 protected ref map<SCR_ButtonBaseComponent, int> m_SquadButtons = new map<SCR_ButtonBaseComponent, int>();
 protected ref map<SCR_ButtonBaseComponent, int> m_FobButtons = new map<SCR_ButtonBaseComponent, int>();
 protected ref map<SCR_ButtonBaseComponent, int> m_ShowButtons = new map<SCR_ButtonBaseComponent, int>();
 protected ref map<SCR_ButtonBaseComponent, int> m_FobShowButtons = new map<SCR_ButtonBaseComponent, int>();
 protected ref array<RplId> m_SquadIds = {};
 protected ref array<string> m_FobLabels = {};
 protected string m_sShown = "-";
 protected int m_iShareShown = -1;

 void Create(Widget parent, SCR_PlayerController owner)
 {
  m_Owner = owner;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget column = ACR_TwoICStyle.PanelBox(parent, WIDTH, 0, 0, m_Root, ACR_TwoICStyle.Rgb(16, 19, 23, 0.99));
  FrameSlot.SetAnchorMin(m_Root, 0.5, 0.5);
  FrameSlot.SetAnchorMax(m_Root, 0.5, 0.5);
  FrameSlot.SetAlignment(m_Root, 0.5, 0.5);
  FrameSlot.SetPos(m_Root, 0, 20);
  FrameSlot.SetSizeToContent(m_Root, true);
  m_Root.SetZOrder(1100);
  // Held by the header, all but its Close (ACR_PanelMover): the panel's body is its lists and buttons.
  ACR_PanelMover.Add(ACR_UiPrefs.TWOIC_PANEL, m_Root, 0.5, 0.5, 0, 20);
  ACR_PanelMover.TitleBar(ACR_UiPrefs.TWOIC_PANEL, m_Root, 86, 90);

  // The header: the dot, "2IC", what it runs; Close.
  Widget header = ACR_TwoICStyle.Row(column);
  AlignableSlot.SetPadding(header, 26, 18, 26, 18);
  SizeLayoutWidget dot = ACR_TwoICStyle.Box(header, 16, 16, ACR_TwoICStyle.Accent());
  AlignableSlot.SetVerticalAlign(dot, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(dot, 0, 0, 14, 0);
  TextWidget title = ACR_TwoICStyle.Text(header, "2IC", ACR_TwoICStyle.White(), 34, true);
  AlignableSlot.SetVerticalAlign(title, LayoutVerticalAlign.Center);
  m_Summary = ACR_TwoICStyle.Text(header, "", ACR_TwoICStyle.Grey(), 20);
  AlignableSlot.SetVerticalAlign(m_Summary, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(m_Summary, 18, 4, 0, 0);
  ACR_TwoICStyle.Grow(m_Summary);
  SCR_ButtonTextComponent close = ACR_TwoICStyle.Button(header, "X", ACR_TwoICStyle.BUTTON_GHOST, 50, 22, 50);
  if (close) close.m_OnClicked.Insert(OnClose);
  ACR_TwoICStyle.Box(column, 0, 1, ACR_TwoICStyle.Track());

  Widget body = ACR_TwoICStyle.Row(column);
  // Left: the squads.
  Widget left = ACR_TwoICStyle.Column(body);
  ACR_TwoICStyle.Grow(left);
  AlignableSlot.SetPadding(left, 26, 20, 26, 20);
  Widget squadsHead = ACR_TwoICStyle.Row(left);
  TextWidget squads = ACR_TwoICStyle.Text(squadsHead, "SQUADS", ACR_TwoICStyle.White(), 26, true);
  AlignableSlot.SetVerticalAlign(squads, LayoutVerticalAlign.Center);
  m_CommandHint = ACR_TwoICStyle.Text(squadsHead, "", ACR_TwoICStyle.Grey(), 17);
  AlignableSlot.SetVerticalAlign(m_CommandHint, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(m_CommandHint, 16, 4, 0, 0);
  ACR_TwoICStyle.Grow(m_CommandHint);
  Widget columns = ACR_TwoICStyle.Row(left, 14);
  Header(columns, "SQUAD", COL_NAME);
  Header(columns, "WHERE", COL_WHERE);
  ACR_TwoICStyle.Grow(Header(columns, "DOING", 0));
  Header(columns, "RUN BY", COL_RUN);
  Header(columns, "", COL_SHOW);
  Header(columns, "", COL_ACTION);
  ACR_TwoICStyle.Box(left, 0, 1, ACR_TwoICStyle.Track());
  SizeLayoutWidget listBox = ACR_TwoICStyle.Box(left, 0, LIST_HEIGHT, null);
  // Clip, or a long list draws over the whole panel (as the requisition list).
  Widget scroll = ws.CreateWidget(WidgetType.ScrollLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.CLIPCHILDREN, Color.FromInt(Color.WHITE), 0, listBox);
  // The rows from the top of the box (it centred a short list).
  AlignableSlot.SetHorizontalAlign(scroll, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(scroll, LayoutVerticalAlign.Stretch);
  // A scroll layout gives its child any width: the rows' Fill columns need the list's width.
  SizeLayoutWidget listWidth = ACR_TwoICStyle.Box(scroll, LIST_WIDTH, 0, null);
  AlignableSlot.SetVerticalAlign(listWidth, LayoutVerticalAlign.Top);
  m_SquadList = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, listWidth);
  AlignableSlot.SetHorizontalAlign(m_SquadList, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(m_SquadList, LayoutVerticalAlign.Top);

  ACR_TwoICStyle.Box(body, 1, 0, ACR_TwoICStyle.Track());

  // Right: FOBs, reserve, small force, type.
  SizeLayoutWidget side = ACR_TwoICStyle.Box(body, SIDE_WIDTH, 0, null);
  AlignableSlot.SetVerticalAlign(side, LayoutVerticalAlign.Top);
  Widget right = ACR_TwoICStyle.Column(side);
  AlignableSlot.SetPadding(right, 20, 20, 20, 20);
  Heading(right, "FOBS", 0);
  m_FobList = ACR_TwoICStyle.Column(right, 8);
  m_FobsNone = ACR_TwoICStyle.Text(right, "No FOBs built yet", ACR_TwoICStyle.Grey(), 17);

  Heading(right, "RESERVE", 26);
  Widget shareRow = ACR_TwoICStyle.Row(right, 8);
  ACR_TwoICStyle.Grow(ACR_TwoICStyle.Text(shareRow, "Share of income set aside", ACR_TwoICStyle.White(), 19));
  m_ShareValue = ACR_TwoICStyle.Text(shareRow, "", ACR_TwoICStyle.White(), 19, true);
  // The sketch's slider: a track with the share filled in the accent and a knob, and - / + in 5%
  // steps either side.
  Widget control = ACR_TwoICStyle.Row(right, 8);
  SCR_ButtonTextComponent down = ACR_TwoICStyle.Button(control, "-", ACR_TwoICStyle.BUTTON_GHOST, 44, 22, 38);
  if (down) down.m_OnClicked.Insert(OnShareDown);
  Widget track = ACR_TwoICStyle.Row(control);
  AlignableSlot.SetVerticalAlign(track, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(track, 14, 0, 14, 0);
  m_ShareFill = ACR_TwoICStyle.Box(track, SHARE_TRACK / 2, 6, ACR_TwoICStyle.Accent());
  AlignableSlot.SetVerticalAlign(m_ShareFill, LayoutVerticalAlign.Center);
  SizeLayoutWidget knob = ACR_TwoICStyle.Box(track, 18, 18, ACR_TwoICStyle.Accent());
  AlignableSlot.SetVerticalAlign(knob, LayoutVerticalAlign.Center);
  m_ShareRest = ACR_TwoICStyle.Box(track, SHARE_TRACK / 2, 6, ACR_TwoICStyle.Rgb(201, 198, 191, 0.5));
  AlignableSlot.SetVerticalAlign(m_ShareRest, LayoutVerticalAlign.Center);
  SCR_ButtonTextComponent up = ACR_TwoICStyle.Button(control, "+", ACR_TwoICStyle.BUTTON_GHOST, 44, 22, 38);
  if (up) up.m_OnClicked.Insert(OnShareUp);
  Widget bar = ACR_TwoICStyle.Row(right, 10);
  m_BarFill = ACR_TwoICStyle.Box(bar, SIDE_BAR / 2, 8, ACR_TwoICStyle.Accent());
  m_BarRest = ACR_TwoICStyle.Box(bar, SIDE_BAR / 2, 8, ACR_TwoICStyle.Track());
  m_ReserveText = ACR_TwoICStyle.Text(right, "", ACR_TwoICStyle.Rgb(201, 198, 191), 17);
  AlignableSlot.SetPadding(m_ReserveText, 0, 10, 0, 0);

  Widget smallHead = ACR_TwoICStyle.Row(right, 26);
  TextWidget small = ACR_TwoICStyle.Text(smallHead, "SMALL FORCE", ACR_TwoICStyle.White(), 26, true);
  AlignableSlot.SetVerticalAlign(small, LayoutVerticalAlign.Center);
  m_SmallChipText = ACR_TwoICStyle.Chip(smallHead, m_SmallChip);
  AlignableSlot.SetVerticalAlign(m_SmallChip, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(m_SmallChip, 12, 0, 0, 0);
  m_SmallText = ACR_TwoICStyle.Text(right, "Under 4 squads the 2IC only grabs empty bases, holds with at most half of its squads, and answers your requests first.", ACR_TwoICStyle.Rgb(201, 198, 191), 17);
  AlignableSlot.SetPadding(m_SmallText, 0, 8, 0, 0);

  Heading(right, "TYPE", 26);
  SCR_ButtonTextComponent type = ACR_TwoICStyle.Button(right, "Standard", ACR_TwoICStyle.BUTTON_OUTLINE, 0, 19, 50);
  if (type)
  {
   AlignableSlot.SetPadding(type.GetRootWidget().GetParent(), 0, 8, 0, 0);
   type.SetEnabled(false, false);
   TextWidget typeText = type.GetTextWidget();
   if (typeText)
   {
    typeText.SetColor(ACR_TwoICStyle.Grey());
    AlignableSlot.SetHorizontalAlign(typeText, LayoutHorizontalAlign.Left);
   }
  }
 }

 void Destroy()
 {
  ACR_PanelMover.Remove(ACR_UiPrefs.TWOIC_PANEL);
  if (m_Root) m_Root.RemoveFromHierarchy();
  m_Root = null;
  m_SquadButtons.Clear();
  m_FobButtons.Clear();
 }

 bool Contains(Widget widget)
 {
  while (widget)
  {
   if (m_Root && widget == m_Root) return true;
   widget = widget.GetParent();
  }
  return false;
 }

 // The rows, rebuilt only when they changed. myName: the player's own (his squad reads "You").
 void SetData(array<RplId> ids, array<string> names, array<string> wheres, array<string> doings, array<string> runBys, array<int> flags, array<string> fobs, array<int> fobFlags, int share, string small, int reserve, int reserveMax, bool commander, string myName)
 {
  if (!m_Root || !ids || !names || names.Count() != ids.Count()) return;
  string key = string.Format("%1|%2|%3|%4|%5|", share, small, reserve, commander, myName);
  foreach (int k, string n : names) key += string.Format("%1;%2;%3;%4;%5|", n, wheres[k], doings[k], runBys[k], flags[k]);
  foreach (int f, string label : fobs) key += string.Format("%1;%2|", label, fobFlags[f]);
  if (key == m_sShown) return;
  m_sShown = key;
  if (commander) m_CommandHint.SetText("Yours while you lead it or after you give it an order, until you hand it back. Everything else is the 2IC's.");
  else m_CommandHint.SetText("Take command to hand squads and FOBs to the 2IC or take them back.");

  Clear(m_SquadList);
  m_SquadButtons.Clear();
  m_ShowButtons.Clear();
  m_SquadIds.Copy(ids);
  int run;
  foreach (int i, string name : names)
  {
   bool twoIC = (flags[i] & ACR_TwoIC.ROW_2IC) != 0;
   bool locked = (flags[i] & ACR_TwoIC.ROW_LOCKED) != 0;
   bool canHand = (flags[i] & ACR_TwoIC.ROW_CAN_HAND) != 0;
   if (twoIC) run++;
   SCR_ButtonTextComponent button = AddSquad(i, name, wheres[i], doings[i], runBys[i], myName, twoIC, locked, commander && (twoIC || canHand));
   if (!button) continue;
   button.m_OnClicked.Insert(OnSquad);
   m_SquadButtons.Insert(button, i);
  }

  Clear(m_FobList);
  m_FobButtons.Clear();
  m_FobShowButtons.Clear();
  m_FobLabels.Copy(fobs);
  int managed;
  foreach (int j, string fob : fobs)
  {
   bool his = (fobFlags[j] & 1) != 0;
   if (his) managed++;
   SCR_ButtonTextComponent fobButton = AddFob(j, fob, his, commander);
   if (!fobButton) continue;
   fobButton.m_OnClicked.Insert(OnFob);
   m_FobButtons.Insert(fobButton, j);
  }
  m_FobsNone.SetVisible(fobs.IsEmpty());

  string fobText = "manages no FOB";
  if (managed == 1) fobText = "manages 1 FOB";
  else if (managed > 1) fobText = string.Format("manages %1 FOBs", managed);
  string reserveText = "reserve off";
  if (share > 0) reserveText = string.Format("reserve %1 / %2 RP", reserve, reserveMax);
  m_Summary.SetText(string.Format("Runs %1 of %2 squads  ·  %3  ·  %4", run, names.Count(), fobText, reserveText));

  m_iShareShown = share;
  float part = Math.Clamp(share / (float)ACR_TwoIC.SHARE_MAX, 0, 1);
  m_ShareFill.SetWidthOverride(Math.Max(1, SHARE_TRACK * part));
  m_ShareRest.SetWidthOverride(Math.Max(1, SHARE_TRACK * (1 - part)));
  m_ShareValue.SetText(string.Format("%1%%", share));
  float filled = 0;
  if (share > 0 && reserveMax > 0) filled = Math.Clamp(reserve / (float)reserveMax, 0, 1);
  m_BarFill.SetWidthOverride(Math.Max(1, SIDE_BAR * filled));
  m_BarRest.SetWidthOverride(Math.Max(1, SIDE_BAR * (1 - filled)));
  if (share > 0) m_ReserveText.SetText(string.Format("%1 of %2 RP saved. The 2IC builds roadblocks, minefields and positions only from this, never from your RP.", reserve, reserveMax));
  else m_ReserveText.SetText("Off: the 2IC builds nothing (it never spends your RP).");
  // "Not a small force: 9 squads ..." / "A small force: 3 squads ...": the chip says which.
  string chip = small;
  int colon = small.IndexOf(":");
  if (colon > 0) chip = small.Substring(0, colon);
  if (chip.StartsWith("Not")) chip = "Off";
  else if (chip.StartsWith("A small")) chip = "On";
  int count = 0;
  foreach (int c : flags) count++;
  ACR_TwoICStyle.SetChip(m_SmallChip, m_SmallChipText, string.Format("%1  ·  %2 squads", chip, count));
  if (m_SmallChipText)
  {
   m_SmallChipText.SetColor(ACR_TwoICStyle.Rgb(201, 198, 191));
   Widget chipFill = m_SmallChip.GetChildren();
   if (chipFill) chipFill.SetColor(ACR_TwoICStyle.Track());
  }
 }

 // ---------------------------------------------------------------- clicks
 protected void OnClose(SCR_ButtonBaseComponent button)
 {
  if (m_Owner) m_Owner.ACR_TwoICTogglePanel();
 }

 protected void OnSquad(SCR_ButtonBaseComponent button)
 {
  int i;
  if (m_Owner && m_SquadButtons.Find(button, i) && i < m_SquadIds.Count()) m_Owner.ACR_Request(ACR_ERequest.ORDER_SQUAD_AI, m_SquadIds[i]);
 }

 protected void OnShow(SCR_ButtonBaseComponent button)
 {
  int i;
  if (m_Owner && m_ShowButtons.Find(button, i) && i < m_SquadIds.Count()) m_Owner.ACR_TwoICShowSquad(m_SquadIds[i]);
 }

 protected void OnShowFob(SCR_ButtonBaseComponent button)
 {
  int j;
  if (m_Owner && m_FobShowButtons.Find(button, j) && j < m_FobLabels.Count()) m_Owner.ACR_TwoICShowFob(m_FobLabels[j]);
 }

 protected void OnFob(SCR_ButtonBaseComponent button)
 {
  int j;
  if (m_Owner && m_FobButtons.Find(button, j) && j < m_FobLabels.Count()) m_Owner.ACR_TwoICFob(m_FobLabels[j]);
 }

 protected void OnShareDown(SCR_ButtonBaseComponent button)
 {
  if (m_Owner && m_iShareShown >= 0) m_Owner.ACR_TwoICShare(Math.Max(0, m_iShareShown - 5));
 }

 protected void OnShareUp(SCR_ButtonBaseComponent button)
 {
  if (m_Owner && m_iShareShown >= 0) m_Owner.ACR_TwoICShare(Math.Min(ACR_TwoIC.SHARE_MAX, m_iShareShown + 5));
 }

 // ---------------------------------------------------------------- building
 protected void Clear(Widget list)
 {
  if (!list) return;
  Widget child = list.GetChildren();
  while (child)
  {
   Widget next = child.GetSibling();
   child.RemoveFromHierarchy();
   child = next;
  }
 }

 protected TextWidget Heading(Widget parent, string text, float gapTop)
 {
  TextWidget heading = ACR_TwoICStyle.Text(parent, text, ACR_TwoICStyle.White(), 26, true);
  if (heading && gapTop > 0) AlignableSlot.SetPadding(heading, 0, gapTop, 0, 0);
  return heading;
 }

 // A column header (the sketch's small condensed grey caps); width 0: as the row gives.
 protected Widget Header(Widget row, string text, float width)
 {
  Widget cell = ACR_TwoICStyle.Box(row, width, 0, null);
  TextWidget label = ACR_TwoICStyle.Text(cell, text, ACR_TwoICStyle.Grey(), 16, true);
  if (label) AlignableSlot.SetHorizontalAlign(label, LayoutHorizontalAlign.Left);
  return cell;
 }

 protected Widget Cell(Widget row, string text, float width, Color color, bool strong = false)
 {
  Widget cell = ACR_TwoICStyle.Box(row, width, 0, null);
  AlignableSlot.SetVerticalAlign(cell, LayoutVerticalAlign.Center);
  TextWidget label = ACR_TwoICStyle.Text(cell, text, color, 19, strong);
  if (label) label.SetExactFontSize(19);
  if (label)
  {
   AlignableSlot.SetPadding(label, 0, 0, 12, 0);
   AlignableSlot.SetHorizontalAlign(label, LayoutHorizontalAlign.Left);
   AlignableSlot.SetVerticalAlign(label, LayoutVerticalAlign.Center);
  }
  return cell;
 }

 // A squad's row: name, where, doing, run by (2IC in the accent, You in blue), Show, and its button.
 protected SCR_ButtonTextComponent AddSquad(int index, string name, string where, string doing, string runBy, string myName, bool twoIC, bool locked, bool enabled)
 {
  Widget cells = ACR_TwoICStyle.Row(m_SquadList);
  AlignableSlot.SetHorizontalAlign(cells, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetPadding(cells, 0, 7, 0, 7);
  Color dim = ACR_TwoICStyle.Rgb(201, 198, 191);
  // A long name ("M923A1 Construction Truck 1") ran into the WHERE column: cut short.
  if (name.Length() > NAME_CHARS) name = name.Substring(0, NAME_CHARS - 3) + "...";
  Cell(cells, name, COL_NAME, ACR_TwoICStyle.White(), true);
  Cell(cells, where, COL_WHERE, dim);
  if (doing.IsEmpty()) doing = "-";
  // One case down the column: the server's lines mix "holding" and "Holding" (playtest 30 Oct).
  string firstLetter = doing.Substring(0, 1);
  firstLetter.ToLower();
  doing = firstLetter + doing.Substring(1, doing.Length() - 1);
  ACR_TwoICStyle.Grow(Cell(cells, doing, 0, dim));
  string by = "2IC";
  Color byColor = ACR_TwoICStyle.Accent();
  if (!twoIC)
  {
   by = "You";
   if (!runBy.IsEmpty() && runBy != myName && runBy != "commander") by = runBy;
   byColor = ACR_TwoICStyle.Blue();
  }
  Cell(cells, by, COL_RUN, byColor, true);
  // Show: any squad, whoever runs it (the commander's own and a player's too).
  Widget showCell = ACR_TwoICStyle.Box(cells, COL_SHOW, 0, null);
  AlignableSlot.SetVerticalAlign(showCell, LayoutVerticalAlign.Center);
  SCR_ButtonTextComponent show = ACR_TwoICStyle.Button(showCell, "Show", ACR_TwoICStyle.BUTTON_GHOST, COL_SHOW - 10, 17, 44);
  if (show)
  {
   show.m_OnClicked.Insert(OnShow);
   m_ShowButtons.Insert(show, index);
  }
  Widget action = ACR_TwoICStyle.Box(cells, COL_ACTION, 0, null);
  AlignableSlot.SetVerticalAlign(action, LayoutVerticalAlign.Center);
  ACR_TwoICStyle.Box(m_SquadList, 0, 1, ACR_TwoICStyle.Line());
  if (locked && !twoIC)
  {
   ACR_TwoICStyle.Text(action, "You lead it", ACR_TwoICStyle.Grey(), 16);
   return null;
  }
  int look = ACR_TwoICStyle.BUTTON_FILL;
  string label = "Hand to 2IC";
  if (twoIC)
  {
   look = ACR_TwoICStyle.BUTTON_OUTLINE;
   label = "Take back";
  }
  SCR_ButtonTextComponent button = ACR_TwoICStyle.Button(action, label, look, COL_ACTION - 10, 17, 44);
  if (button) button.SetEnabled(enabled, false);
  return button;
 }

 // A FOB's row: a tick box (the accent when the 2IC manages it), its name, and who has it.
 protected SCR_ButtonTextComponent AddFob(int index, string fob, bool his, bool commander)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget holder = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_FobList);
  AlignableSlot.SetPadding(holder, 0, 0, 0, 8);
  ACR_TwoICStyle.Fill(holder, ACR_TwoICStyle.Block());
  SizeLayoutWidget pad = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, holder));
  AlignableSlot.SetPadding(pad, 12, 8, 12, 8);
  Widget row = ACR_TwoICStyle.Row(pad);
  int look = ACR_TwoICStyle.BUTTON_GHOST;
  string tick = "";
  if (his)
  {
   look = ACR_TwoICStyle.BUTTON_FILL;
   tick = "✓";
  }
  SCR_ButtonTextComponent box = ACR_TwoICStyle.Button(row, tick, look, 40, 20, 40);
  if (box) box.SetEnabled(commander, false);
  TextWidget name = ACR_TwoICStyle.Text(row, fob, ACR_TwoICStyle.White(), 19);
  AlignableSlot.SetVerticalAlign(name, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(name, 12, 0, 0, 0);
  ACR_TwoICStyle.Grow(name);
  string note = "yours";
  if (his) note = "2IC upgrades and defends";
  TextWidget whose = ACR_TwoICStyle.Text(row, note, ACR_TwoICStyle.Grey(), 16);
  AlignableSlot.SetVerticalAlign(whose, LayoutVerticalAlign.Center);
  SCR_ButtonTextComponent show = ACR_TwoICStyle.Button(row, "Show", ACR_TwoICStyle.BUTTON_GHOST, 76, 16, 40);
  if (show)
  {
   show.m_OnClicked.Insert(OnShowFob);
   m_FobShowButtons.Insert(show, index);
  }
  return box;
 }
}
