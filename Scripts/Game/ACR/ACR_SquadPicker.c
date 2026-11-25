// The units behind a base's count or a "×N" on the calm map (playtest 30 Oct: "when many units are in
// the same place we get the 2x, 3x ... it's still hard to choose the right unit"). Before, each click
// on the icon selected the next squad of the pile in turn (ACR_CycleOverlap), with no name until it
// was picked. Now a click opens this list beside it: the base, then one numbered row per squad with
// its name, what kind it is (its symbol: infantry, motorized, armour ...), what it is doing (moving,
// fighting, holding: the icon's corner mark), how far out it is when outside the base's ring, and its
// health and ammo, the selected ones marked, the enemy seen there, and Select all. A row, or its
// number key (1-9), selects that squad (Shift adds it to the selection, as Shift + a marker click
// does); Select all selects them all. Hovering a row rings where that squad really stands. It stays
// clear of the panels and on the screen, scrolling a list too long for it, and closes on a pick, a
// click elsewhere on the map, or when the map closes (SCR_PlayerController.ACR_OpenSquadList in
// ACR_PlayerMapDeclutter). While it is open a base marker under the cursor keeps its short caption
// (ACR_MapMarkers).
class ACR_SquadPicker : Managed
{
 static const float WIDTH = 540;
 static const float ROW_HEIGHT = 38;
 static const float ENEMY_HEIGHT = 26;
 static const int NAME_CHARS = 22;  // a longer name is cut short to fit the row
 static const int KEYS = 9;         // rows 1-9 have a number key
 static bool s_bOpen;

 Widget m_Root;
 protected SCR_PlayerController m_Owner;
 protected ref array<RplId> m_Ids = {};
 protected ref map<SCR_ButtonBaseComponent, int> m_Rows = new map<SCR_ButtonBaseComponent, int>();
 protected ref array<ref ACR_SquadPickerHover> m_Hovers = {};
 protected float m_fX0;  // what it opened beside (native px)
 protected float m_fX1;
 protected float m_fY;
 protected float m_fSide = 60; // how far from that (unscaled px)
 protected float m_fHeight; // what its rows and the rest add up to (unscaled px)
 protected ScrollLayoutWidget m_Scroll;

 // What a squad's symbol says it is, in a word.
 static string TypeWord(int icons)
 {
  if (icons & EMilitarySymbolIcon.ROTARY_WING) return "helicopter";
  if (icons & EMilitarySymbolIcon.FIXED_WING) return "aircraft";
  if (icons & EMilitarySymbolIcon.ARMOR) return "armour";
  if (icons & EMilitarySymbolIcon.SUPPLY) return "supply";
  if (icons & EMilitarySymbolIcon.MEDICAL) return "medical";
  if (icons & EMilitarySymbolIcon.MAINTENANCE) return "repair";
  if (icons & EMilitarySymbolIcon.MORTAR) return "mortar";
  if (icons & EMilitarySymbolIcon.ARTILLERY) return "artillery";
  if (icons & EMilitarySymbolIcon.MOTORIZED) return "motorized";
  if (icons & EMilitarySymbolIcon.RECON) return "recon";
  if (icons & EMilitarySymbolIcon.SNIPER) return "sniper";
  if (icons & EMilitarySymbolIcon.ANTITANK) return "AT";
  if (icons & EMilitarySymbolIcon.MACHINEGUN) return "MG";
  if (icons & EMilitarySymbolIcon.INFANTRY) return "infantry";
  return "";
 }

 // health, ammo: percent, -1 unknown. "HP 80% · ammo 35%".
 static string Strength(int health, int ammo)
 {
  string text = "";
  if (health >= 0) text = string.Format("HP %1%%", health);
  if (ammo >= 0)
  {
   if (!text.IsEmpty()) text += "  ·  ";
   text += string.Format("ammo %1%%", ammo);
  }
  return text;
 }

 // x, y: the cursor in native pixels. enemies: a line per enemy seen there, or null.
 void Create(Widget parent, SCR_PlayerController owner, string title, array<RplId> ids, array<string> names, array<string> types, array<string> doings, array<string> strengths, array<bool> selected, float x, float y, array<string> enemies = null)
 {
  m_Owner = owner;
  m_Ids.Copy(ids);
  m_fX0 = x;
  m_fX1 = x;
  m_fY = y;
  s_bOpen = true;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget column = ACR_TwoICStyle.PanelBox(parent, WIDTH - 16, 8, 8, m_Root, ACR_TwoICStyle.Rgb(16, 19, 23, 0.97));
  Widget catchCursor = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, Color.FromRGBA(0, 0, 0, 3), -1, m_Root);
  if (catchCursor)
  {
   AlignableSlot.SetHorizontalAlign(catchCursor, LayoutHorizontalAlign.Stretch);
   AlignableSlot.SetVerticalAlign(catchCursor, LayoutVerticalAlign.Stretch);
  }
  FrameSlot.SetAnchorMin(m_Root, 0, 0);
  FrameSlot.SetAnchorMax(m_Root, 0, 0);
  FrameSlot.SetSizeToContent(m_Root, true);
  m_Root.SetZOrder(1050);
  TextWidget head = ACR_TwoICStyle.Text(column, title, ACR_TwoICStyle.Grey(), 16, true);
  if (head) AlignableSlot.SetPadding(head, 6, 2, 6, 6);
  // The rows, clipped and scrolled when there are more than fit (as the Squads list's).
  int enemyLines = 0;
  if (enemies && !enemies.IsEmpty()) enemyLines = enemies.Count() + 1;
  float wanted = names.Count() * ROW_HEIGHT + enemyLines * ENEMY_HEIGHT;
  float others = 16 + 30;
  if (ids.Count() > 1) others += ROW_HEIGHT + 4;
  float rowsHeight = Math.Clamp(wanted, ROW_HEIGHT, Math.Max(ROW_HEIGHT * 2, ws.DPIUnscale(ws.GetHeight()) - 16 - others));
  m_fHeight = rowsHeight + others;
  SizeLayoutWidget listBox = ACR_TwoICStyle.Box(column, WIDTH - 16, rowsHeight, null);
  Widget scroll = ws.CreateWidget(WidgetType.ScrollLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.CLIPCHILDREN, Color.FromInt(Color.WHITE), 0, listBox);
  AlignableSlot.SetHorizontalAlign(scroll, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(scroll, LayoutVerticalAlign.Stretch);
  m_Scroll = ScrollLayoutWidget.Cast(scroll);
  SizeLayoutWidget listWidth = ACR_TwoICStyle.Box(scroll, WIDTH - 16, 0, null);
  AlignableSlot.SetVerticalAlign(listWidth, LayoutVerticalAlign.Top);
  Widget list = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, listWidth);
  AlignableSlot.SetHorizontalAlign(list, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(list, LayoutVerticalAlign.Top);
  foreach (int i, string name : names)
  {
   if (name.Length() > NAME_CHARS) name = name.Substring(0, NAME_CHARS - 3) + "...";
   // Past KEYS no number: there is no key for it. (As wide as a digit and two spaces, near enough.)
   string key = "    ";
   if (i < KEYS) key = (i + 1).ToString() + "  ";
   string mark = "";
   if (i < selected.Count() && selected[i]) mark = "» "; // (the font has no dot)
   string label = key + mark + name;
   if (i < types.Count() && !types[i].IsEmpty()) label += "  ·  " + types[i];
   if (i < doings.Count() && !doings[i].IsEmpty()) label += "  ·  " + doings[i];
   if (i < strengths.Count() && !strengths[i].IsEmpty()) label += "    " + strengths[i];
   SCR_ButtonTextComponent row = ACR_TwoICStyle.Button(list, label, ACR_TwoICStyle.BUTTON_MENU, WIDTH - 16, 16, ROW_HEIGHT);
   if (!row) continue;
   row.m_OnClicked.Insert(OnRow);
   m_Rows.Insert(row, i);
   Widget rowWidget = row.GetRootWidget();
   if (rowWidget)
   {
    ACR_SquadPickerHover hover = new ACR_SquadPickerHover();
    hover.m_Picker = this;
    hover.m_iIndex = i;
    rowWidget.AddHandler(hover);
    m_Hovers.Insert(hover);
   }
  }
  if (enemyLines > 0)
  {
   TextWidget enemyHead = ACR_TwoICStyle.Text(list, "ENEMY SEEN", ACR_TwoICStyle.Grey(), 15);
   if (enemyHead) AlignableSlot.SetPadding(enemyHead, 6, 4, 6, 2);
   foreach (string enemy : enemies)
   {
    TextWidget enemyLine = ACR_TwoICStyle.Text(list, enemy, ACR_MapDeclutter.Rgb(240, 130, 120), 16);
    if (enemyLine) AlignableSlot.SetPadding(enemyLine, 6, 2, 6, 2);
   }
  }
  if (ids.Count() > 1)
  {
   SCR_ButtonTextComponent all = ACR_TwoICStyle.Button(column, string.Format("Select all %1", ids.Count()), ACR_TwoICStyle.BUTTON_OUTLINE, WIDTH - 16, 16, ROW_HEIGHT);
   if (all) all.m_OnClicked.Insert(OnAll);
  }
  array<float> none = {};
  Place(none);
 }

 // What it opens beside, when not the cursor: a base's count (native px).
 void AnchorTo(float x0, float x1, float y, float side)
 {
  m_fX0 = x0;
  m_fX1 = x1;
  m_fY = y;
  m_fSide = side;
 }

 // Beside what it was opened from, clear of the clicked icon or count, of keepClear (the panels:
 // rectangles in unscaled px) and on the screen. Each frame while open: a base's click also opens the
 // ACA panel, a frame later.
 void Place(array<float> keepClear)
 {
  if (!m_Root) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float h = m_fHeight;
  float sw, sh;
  m_Root.GetScreenSize(sw, sh);
  if (sh > 0) h = ws.DPIUnscale(sh);
  vector at = ACR_MapDeclutter.PlaceBox(ws.DPIUnscale(m_fX0), ws.DPIUnscale(m_fX1), ws.DPIUnscale(m_fY), WIDTH, h, m_fSide, 30, ws.DPIUnscale(ws.GetWidth()), ws.DPIUnscale(ws.GetHeight()), 8, keepClear);
  FrameSlot.SetPos(m_Root, at[0], at[1]);
 }

 bool Contains(Widget widget)
 {
  while (widget)
  {
   if (widget == m_Root) return true;
   widget = widget.GetParent();
  }
  return false;
 }

 void Destroy()
 {
  s_bOpen = false;
  if (m_Owner) m_Owner.ACR_SquadPickerHover(RplId.Invalid());
  if (m_Root) m_Root.RemoveFromHierarchy();
  m_Root = null;
 }

 // Tests: row i on screen (px).
 protected bool RowRectForTest(int i, out float x, out float y, out float w, out float h)
 {
  foreach (SCR_ButtonBaseComponent button, int row : m_Rows)
  {
   if (row != i || !button.GetRootWidget()) continue;
   button.GetRootWidget().GetScreenPos(x, y);
   button.GetRootWidget().GetScreenSize(w, h);
   return true;
  }
  return false;
 }

 // Tests: the middle of row i on screen (px).
 bool RowPointForTest(int i, out float x, out float y)
 {
  float w, h;
  if (!RowRectForTest(i, x, y, w, h)) return false;
  x += w * 0.5;
  y += h * 0.5;
  return true;
 }

 // Tests: row i wholly inside what the list shows now.
 bool RowInViewForTest(int i)
 {
  float x, y, w, h;
  if (!m_Scroll || !RowRectForTest(i, x, y, w, h)) return false;
  float sx, sy, sw, sh;
  m_Scroll.GetScreenPos(sx, sy);
  m_Scroll.GetScreenSize(sw, sh);
  return y >= sy - 1 && y + h <= sy + sh + 1;
 }

 // Tests: how far down the rows are scrolled (0 the top, 1 the end), and straight to the end.
 float ScrollForTest()
 {
  float x, y;
  if (m_Scroll) m_Scroll.GetSliderPos(x, y);
  return y;
 }

 void ScrollEndForTest()
 {
  if (m_Scroll) m_Scroll.SetSliderPos(0, 1);
 }

 RplId IdAt(int i)
 {
  if (i < 0 || i >= m_Ids.Count()) return RplId.Invalid();
  return m_Ids[i];
 }

 // A number key: its row.
 void ChooseIndex(int i)
 {
  if (m_Owner && i >= 0 && i < m_Ids.Count()) m_Owner.ACR_SquadPickerChoose(m_Ids[i]);
 }

 void Hover(int i)
 {
  if (!m_Owner) return;
  if (i >= 0 && i < m_Ids.Count()) m_Owner.ACR_SquadPickerHover(m_Ids[i]);
  else m_Owner.ACR_SquadPickerHover(RplId.Invalid());
 }

 protected void OnRow(SCR_ButtonBaseComponent button)
 {
  int i;
  if (m_Rows.Find(button, i)) ChooseIndex(i);
 }

 protected void OnAll(SCR_ButtonBaseComponent button)
 {
  if (m_Owner) m_Owner.ACR_SquadPickerAll(m_Ids);
 }
}

// A row's hover: rings where its squad stands.
class ACR_SquadPickerHover : ScriptedWidgetEventHandler
{
 ACR_SquadPicker m_Picker;
 int m_iIndex;

 override bool OnMouseEnter(Widget w, int x, int y)
 {
  if (m_Picker) m_Picker.Hover(m_iIndex);
  return false;
 }

 override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
 {
  if (m_Picker) m_Picker.Hover(-1);
  return false;
 }
}
