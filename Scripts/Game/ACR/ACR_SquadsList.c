// The Squads list on the calm map (playtest 30 Oct: finding one squad among many meant hunting the
// map for its icon). Docked on the left under INTEL: every squad under the player's command, one
// row each with its name, what kind it is, what it is doing, and its health or ammo when either is
// down; the selected ones marked. A row selects its squad (Shift adds it, as Shift + a marker
// click); Show also sends the map there (as the 2IC panel's Show); hovering a row rings where the
// squad stands. The search box above the rows keeps only the squads whose name or kind has what
// was typed in it ("2nd", "mortar"). The button in its title bar folds the list away to one line, and
// back; zoomed right out it folds by itself, as INTEL and the ACA panel do (that button opens it there).
// The title bar drags the list (ACR_PanelMover). A long list scrolls. (SCR_PlayerController in
// ACR_PlayerMapSquads fills it.)
class ACR_SquadsList : Managed
{
 static const ResourceName SEARCH_LAYOUT = "{67B4F07635959DC7}UI/layouts/WidgetLibrary/EditBox/WLib_EditBoxSearch.layout";
 static const float WIDTH = 420;
 static const float ROW_HEIGHT = 34;
 static const float SHOW_WIDTH = 64;
 static const float FOLD_WIDTH = 64;
 static const int NAME_CHARS = 22;  // a longer name is cut short to fit the row
 static bool s_bFolded;             // the player folded it (kept while the game runs)

 Widget m_Root;
 protected SCR_PlayerController m_Owner;
 protected TextWidget m_Title;
 protected SCR_ButtonTextComponent m_Fold;
 protected Widget m_Body;
 protected EditBoxWidget m_Search;
 protected SizeLayoutWidget m_ListBox;
 protected Widget m_List;
 protected TextWidget m_None;
 protected ref array<RplId> m_Ids = {};
 protected ref array<SCR_ButtonTextComponent> m_RowButtons = {};
 protected ref map<SCR_ButtonBaseComponent, int> m_Rows = new map<SCR_ButtonBaseComponent, int>();
 protected ref map<SCR_ButtonBaseComponent, int> m_Shows = new map<SCR_ButtonBaseComponent, int>();
 protected ref array<ref ACR_SquadsListHover> m_Hovers = {};
 protected string m_sHeader = "-";
 protected int m_iTotal;
 protected bool m_bFar;      // zoomed right out: folded unless opened there (m_bFarOpen)
 protected bool m_bFarOpen;

 void Create(Widget parent, SCR_PlayerController owner)
 {
  m_Owner = owner;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget column = ACR_TwoICStyle.PanelBox(parent, WIDTH, 8, 6, m_Root, ACR_MapDeclutter.PanelColor());
  FrameSlot.SetAnchorMin(m_Root, 0, 0);
  FrameSlot.SetAnchorMax(m_Root, 0, 0);
  FrameSlot.SetAlignment(m_Root, 0, 0);
  FrameSlot.SetSizeToContent(m_Root, true);
  m_Root.SetZOrder(1000);
  // The title and the fold button; the title bar over all but the button (RegisterPanel).
  Widget header = ACR_TwoICStyle.Row(column);
  AlignableSlot.SetHorizontalAlign(header, LayoutHorizontalAlign.Stretch);
  m_Title = ACR_TwoICStyle.Text(header, "SQUADS", ACR_TwoICStyle.White(), 16);
  if (m_Title)
  {
   ACR_TwoICStyle.Grow(m_Title);
   AlignableSlot.SetVerticalAlign(m_Title, LayoutVerticalAlign.Center);
   AlignableSlot.SetPadding(m_Title, 8, 0, 0, 0);
  }
  m_Fold = ACR_TwoICStyle.Button(header, "Hide", ACR_TwoICStyle.BUTTON_QUIET, FOLD_WIDTH, 15, 30);
  if (m_Fold) m_Fold.m_OnClicked.Insert(OnFold);
  m_Body = ACR_TwoICStyle.Column(column);
  AlignableSlot.SetHorizontalAlign(m_Body, LayoutHorizontalAlign.Stretch);
  // The search box (the game's own, as the server browser's).
  SizeLayoutWidget searchBox = ACR_TwoICStyle.Box(m_Body, WIDTH - 16, 44, null);
  if (searchBox)
  {
   AlignableSlot.SetPadding(searchBox, 0, 2, 0, 4);
   Widget search = ws.CreateWidgets(SEARCH_LAYOUT, searchBox);
   if (search)
   {
    AlignableSlot.SetHorizontalAlign(search, LayoutHorizontalAlign.Stretch);
    AlignableSlot.SetVerticalAlign(search, LayoutVerticalAlign.Stretch);
    m_Search = EditBoxWidget.Cast(search.FindAnyWidget("EditBox"));
    if (m_Search) m_Search.SetPlaceholderText("Find a squad");
   }
  }
  m_None = ACR_TwoICStyle.Text(m_Body, "", ACR_TwoICStyle.Dim(), 16);
  if (m_None) AlignableSlot.SetPadding(m_None, 8, 4, 8, 6);
  // The rows, clipped and scrolled when there are more than fit (as the 2IC panel's list).
  m_ListBox = ACR_TwoICStyle.Box(m_Body, WIDTH - 16, ROW_HEIGHT, null);
  Widget scroll = ws.CreateWidget(WidgetType.ScrollLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.CLIPCHILDREN, Color.FromInt(Color.WHITE), 0, m_ListBox);
  AlignableSlot.SetHorizontalAlign(scroll, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(scroll, LayoutVerticalAlign.Stretch);
  SizeLayoutWidget listWidth = ACR_TwoICStyle.Box(scroll, WIDTH - 16, 0, null);
  AlignableSlot.SetVerticalAlign(listWidth, LayoutVerticalAlign.Top);
  m_List = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, listWidth);
  AlignableSlot.SetHorizontalAlign(m_List, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(m_List, LayoutVerticalAlign.Top);
  m_Body.SetVisible(!Folded());
  ACR_PanelMover.Add(ACR_UiPrefs.SQUADS, m_Root, 0, 0, ACR_UiPrefs.FEED_HOME_X, ACR_UiPrefs.FEED_HOME_Y);
  ACR_PanelMover.Grip(ACR_UiPrefs.SQUADS, m_Root);
  ACR_PanelMover.TitleBar(ACR_UiPrefs.SQUADS, m_Root, 36, FOLD_WIDTH + 8);
 }

 bool Folded()
 {
  if (m_bFar) return !m_bFarOpen;
  return s_bFolded;
 }

 // Zoomed right out (INTEL folded to its bar): fold, as the panels do.
 void SetFar(bool far)
 {
  if (far == m_bFar) return;
  m_bFar = far;
  m_bFarOpen = false;
  Refold();
 }

 protected void Refold()
 {
  if (m_Body) m_Body.SetVisible(!Folded());
  m_sHeader = "-";
  m_Ids.Clear();
 }

 // What is typed in the search box, lower case ("" none).
 string Query()
 {
  if (!m_Search) return "";
  string text = m_Search.GetText();
  text = text.Trim();
  text.ToLower();
  return text;
 }

 // Typing in the search box: the map's keys (control groups 1-9, pings) must not act on it.
 bool Typing()
 {
  return m_Search && m_Search.IsInWriteMode();
 }

 // A row's label: "» 2nd Squad · mortar · moving   HP 60%" (» selected: the font has no ●).
 static string Label(string name, string kind, string doing, int health, int ammo, bool selected)
 {
  if (name.Length() > NAME_CHARS) name = name.Substring(0, NAME_CHARS - 3) + "...";
  string label = name;
  if (selected) label = "» " + label;
  if (!kind.IsEmpty()) label += " · " + kind;
  if (!doing.IsEmpty()) label += " · " + doing;
  // Only what is down, to keep the row short (the hover card has both).
  string down = "";
  if (health >= 0 && health < 100) down = string.Format("HP %1%%", health);
  if (ammo >= 0 && ammo < 100)
  {
   if (!down.IsEmpty()) down += " ";
   down += string.Format("ammo %1%%", ammo);
  }
  if (!down.IsEmpty()) label += "   " + down;
  return label;
 }

 // Whether a squad matches the search: its name or kind has the query ("" matches all).
 static bool Matches(string query, string name, string kind)
 {
  if (query.IsEmpty()) return true;
  string n = name;
  n.ToLower();
  string k = kind;
  k.ToLower();
  return n.Contains(query) || k.Contains(query);
 }

 // ids and labels: the rows to show (already searched); total: the squads before the search;
 // maxHeight: room for the rows (unscaled px).
 void SetRows(array<RplId> ids, array<string> labels, int total, float maxHeight)
 {
  m_iTotal = total;
  string header = string.Format("SQUADS  ·  %1", total);
  if (ids.Count() < total) header = string.Format("SQUADS  ·  %1 of %2", ids.Count(), total);
  if (header != m_sHeader && m_Title)
  {
   m_sHeader = header;
   m_Title.SetText(header);
  }
  if (m_Fold)
  {
   if (Folded()) m_Fold.SetText("Show");
   else m_Fold.SetText("Hide");
  }
  if (Folded()) return;
  bool same = ids.Count() == m_Ids.Count();
  if (same)
  {
   foreach (int i, RplId id : ids)
   {
    if (id != m_Ids[i]) same = false;
   }
  }
  if (!same) Rebuild(ids);
  foreach (int i, SCR_ButtonTextComponent row : m_RowButtons)
  {
   if (row && i < labels.Count()) row.SetText(labels[i]);
  }
  if (m_None)
  {
   string none = "";
   if (ids.IsEmpty() && total > 0) none = "No squad matches";
   else if (total == 0) none = "No squads under your command";
   m_None.SetVisible(!none.IsEmpty());
   m_None.SetText(none);
  }
  if (m_ListBox)
  {
   float height = Math.Clamp(ids.Count() * ROW_HEIGHT, 0, Math.Max(ROW_HEIGHT * 3, maxHeight));
   m_ListBox.SetVisible(!ids.IsEmpty());
   m_ListBox.SetHeightOverride(height);
  }
 }

 protected void Rebuild(array<RplId> ids)
 {
  if (m_Owner) m_Owner.ACR_SquadPickerHover(RplId.Invalid());
  m_Ids.Copy(ids);
  m_Rows.Clear();
  m_Shows.Clear();
  m_Hovers.Clear();
  m_RowButtons.Clear();
  if (!m_List) return;
  Widget child = m_List.GetChildren();
  while (child)
  {
   Widget next = child.GetSibling();
   child.RemoveFromHierarchy();
   child = next;
  }
  foreach (int i, RplId id : ids)
  {
   Widget line = ACR_TwoICStyle.Row(m_List);
   SCR_ButtonTextComponent row = ACR_TwoICStyle.Button(line, "", ACR_TwoICStyle.BUTTON_MENU, WIDTH - 16 - SHOW_WIDTH, 15, ROW_HEIGHT);
   if (row)
   {
    row.m_OnClicked.Insert(OnRow);
    m_Rows.Insert(row, i);
    Widget rowWidget = row.GetRootWidget();
    if (rowWidget)
    {
     ACR_SquadsListHover hover = new ACR_SquadsListHover();
     hover.m_List = this;
     hover.m_iIndex = i;
     rowWidget.AddHandler(hover);
     m_Hovers.Insert(hover);
    }
   }
   m_RowButtons.Insert(row);
   SCR_ButtonTextComponent show = ACR_TwoICStyle.Button(line, "Show", ACR_TwoICStyle.BUTTON_LINK, SHOW_WIDTH, 16, ROW_HEIGHT);
   if (show)
   {
    show.m_OnClicked.Insert(OnShow);
    m_Shows.Insert(show, i);
   }
  }
 }

 void Place(float x, float y)
 {
  ACR_PanelMover.Natural(ACR_UiPrefs.SQUADS, x, y);
 }

 void SetVisible(bool visible)
 {
  if (m_Root && m_Root.IsVisible() != visible) m_Root.SetVisible(visible);
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
  if (m_Owner) m_Owner.ACR_SquadPickerHover(RplId.Invalid());
  ACR_PanelMover.Remove(ACR_UiPrefs.SQUADS);
  if (m_Root) m_Root.RemoveFromHierarchy();
  m_Root = null;
 }

 void Hover(int i)
 {
  if (!m_Owner) return;
  if (i >= 0 && i < m_Ids.Count()) m_Owner.ACR_SquadPickerHover(m_Ids[i]);
  else m_Owner.ACR_SquadPickerHover(RplId.Invalid());
 }

 // Tests.
 int RowsForTest() { return m_Ids.Count(); }
 RplId IdForTest(int i)
 {
  if (i < 0 || i >= m_Ids.Count()) return RplId.Invalid();
  return m_Ids[i];
 }
 string LabelForTest(int i)
 {
  if (i < 0 || i >= m_RowButtons.Count() || !m_RowButtons[i]) return "";
  return m_RowButtons[i].GetText();
 }
 void SearchForTest(string text)
 {
  if (m_Search) m_Search.SetText(text);
 }
 void RowForTest(int i)
 {
  if (i >= 0 && i < m_RowButtons.Count()) OnRow(m_RowButtons[i]);
 }
 void ShowForTest(int i)
 {
  foreach (SCR_ButtonBaseComponent show, int k : m_Shows)
  {
   if (k == i) OnShow(show);
  }
 }
 bool FoldedForTest() { return Folded(); }

 protected void OnFold(SCR_ButtonBaseComponent button)
 {
  if (m_bFar) m_bFarOpen = !m_bFarOpen;
  else s_bFolded = !s_bFolded;
  Refold();
  if (m_Owner) m_Owner.ACR_MarkUiClick();
 }

 protected void OnRow(SCR_ButtonBaseComponent button)
 {
  int i;
  if (m_Owner && m_Rows.Find(button, i) && i < m_Ids.Count()) m_Owner.ACR_SquadsListPick(m_Ids[i]);
 }

 protected void OnShow(SCR_ButtonBaseComponent button)
 {
  int i;
  if (m_Owner && m_Shows.Find(button, i) && i < m_Ids.Count()) m_Owner.ACR_SquadsListShow(m_Ids[i]);
 }
}

// A row's hover: rings where its squad stands.
class ACR_SquadsListHover : ScriptedWidgetEventHandler
{
 ACR_SquadsList m_List;
 int m_iIndex;

 override bool OnMouseEnter(Widget w, int x, int y)
 {
  if (m_List) m_List.Hover(m_iIndex);
  return false;
 }

 override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
 {
  if (m_List) m_List.Hover(-1);
  return false;
 }
}
