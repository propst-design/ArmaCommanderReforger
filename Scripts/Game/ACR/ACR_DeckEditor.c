// Deck builder (#106), client side: the player's decks in the profile (ACR_DeckStore), the
// catalogue the server sends (ACR_DeckView), and the editor panel on the map (ACR_DeckEditor):
// the catalogue on the left, the deck on the right, the points on top; custom cards made from the
// faction's own soldiers; share codes through the clipboard; Hand in sends the deck to the server.
// F3 opens it on the map, and during match setup for everyone (the host also has a setup row).

// The player's decks, one share code per line in the profile.
class ACR_DeckStore
{
 static const string FILE = "$profile:ACA_Decks.txt";
 protected static ref array<ref ACR_DeckData> s_Decks;

 static array<ref ACR_DeckData> All()
 {
  if (s_Decks) return s_Decks;
  s_Decks = {};
  if (!FileIO.FileExists(FILE)) return s_Decks;
  FileHandle file = FileIO.OpenFile(FILE, FileMode.READ);
  if (!file) return s_Decks;
  string line;
  while (file.ReadLine(line) >= 0)
  {
   ACR_DeckData deck = ACR_DeckData.FromText(line);
   if (deck) s_Decks.Insert(deck);
  }
  file.Close();
  return s_Decks;
 }

 static void Save()
 {
  FileHandle file = FileIO.OpenFile(FILE, FileMode.WRITE);
  if (!file) return;
  foreach (ACR_DeckData deck : All()) file.WriteLine(deck.ToText());
  file.Close();
 }

 static void Of(string faction, notnull array<ACR_DeckData> result)
 {
  result.Clear();
  foreach (ACR_DeckData deck : All()) if (deck.m_sFaction == faction) result.Insert(deck);
 }

 // A name not yet taken for this faction: "Deck 2", "Desert Marines (2)".
 static string FreeName(string faction, string wanted)
 {
  string name = ACR_DeckRules.CleanName(wanted);
  if (name.IsEmpty()) name = "Deck";
  if (!Taken(faction, name)) return name;
  for (int i = 2; i < 100; i++)
  {
   string next = ACR_DeckRules.CleanName(name + " " + i.ToString());
   if (!Taken(faction, next)) return next;
  }
  return name;
 }

 protected static bool Taken(string faction, string name)
 {
  foreach (ACR_DeckData deck : All()) if (deck.m_sFaction == faction && deck.m_sName == name) return true;
  return false;
 }
}

// One faction's catalogue as the server sent it.
class ACR_DeckView
{
 string m_sFaction;
 int m_iSide;     // 0 Blue, 1 Red
 int m_iBudget;
 string m_sApplied;
 ref array<string> m_aNames = {};
 ref array<int> m_aRoles = {};
 ref array<int> m_aPoints = {};
 ref array<int> m_aPrices = {};
 ref array<int> m_aFlags = {};
 ref array<string> m_aInfos = {};
 ref array<string> m_aSoldierIds = {};
 ref array<string> m_aSoldierNames = {};
 ref array<int> m_aSoldierRP = {};
 ref array<int> m_aSoldierFlags = {};

 int Index(string name) { return m_aNames.Find(name); }
 bool Has(int i, int flag) { return i >= 0 && i < m_aFlags.Count() && (m_aFlags[i] & flag) != 0; }

 int CustomPrice(ACR_CustomCard card, out bool sf)
 {
  sf = false;
  int price;
  foreach (string id : card.m_aSoldiers)
  {
   int s = m_aSoldierIds.Find(id);
   if (s < 0) continue;
   price += m_aSoldierRP[s];
   if ((m_aSoldierFlags[s] & ACR_DeckFlag.SF) != 0) sf = true;
  }
  int ride = Index(card.m_sTransport);
  if (ride >= 0) price += m_aPrices[ride];
  if (price < 6) price = 6;
  return price;
 }

 int CustomPoints(ACR_CustomCard card)
 {
  bool sf;
  int price = CustomPrice(card, sf);
  return ACR_DeckRules.PointsOfPrice(price, sf);
 }

 int Points(ACR_DeckData deck)
 {
  int total;
  foreach (string card : deck.m_aCards)
  {
   int i = Index(card);
   if (i >= 0 && !Has(i, ACR_DeckFlag.LOCKED)) total += m_aPoints[i];
  }
  foreach (ACR_CustomCard custom : deck.m_aCustom) total += CustomPoints(custom);
  return total;
 }

 // A new deck: the standard one (today's list).
 void Standard(ACR_DeckData deck)
 {
  deck.m_aCards.Clear();
  foreach (int i, string name : m_aNames)
   if (Has(i, ACR_DeckFlag.STANDARD) && !Has(i, ACR_DeckFlag.LOCKED)) deck.m_aCards.Insert(name);
 }
}

class ACR_DeckClient
{
 protected static ref array<ref ACR_DeckView> s_Views;
 protected static ref ACR_DeckEditor s_Editor;
 protected static bool s_bWantOpen;
 static string s_sResult;   // the server's answer to the last hand-in

 static ACR_DeckView View(int side)
 {
  if (!s_Views) return null;
  foreach (ACR_DeckView view : s_Views) if (view.m_iSide == side) return view;
  return null;
 }

 static ACR_DeckView Store(int side, string faction)
 {
  if (!s_Views) s_Views = {};
  ACR_DeckView view = View(side);
  if (!view)
  {
   view = new ACR_DeckView();
   view.m_iSide = side;
   s_Views.Insert(view);
  }
  if (view.m_sFaction != faction)
  {
   view.m_sFaction = faction;
   view.m_aSoldierIds.Clear();
  }
  return view;
 }

 static bool IsOpen() { return s_Editor != null; }

 // Every frame on clients (ACR_Manager.OnFrame).
 static void Frame()
 {
  InputManager input = GetGame().GetInputManager();
  if (input && ACR_SetupClient.IsActive()) input.ActivateContext("ACR_DeckContext");
  if (input && input.GetActionTriggered("ACR_DeckEditor")) Toggle();
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  bool mapOpen = mapEntity && mapEntity.IsOpen() && mapEntity.GetMapMenuRoot();
  if (s_bWantOpen && mapOpen)
  {
   s_bWantOpen = false;
   s_Editor = new ACR_DeckEditor();
   s_Editor.Create(mapEntity.GetMapMenuRoot());
   Ask();
  }
  if (s_Editor && !mapOpen) Close();
 }

 static void Toggle()
 {
  if (s_Editor || s_bWantOpen) { Close(); return; }
  Open();
 }

 static void OpenFromSetup() { Open(); }

 static void Open()
 {
  if (s_Editor) return;
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.IsOpen()) ACR_SetupClient.OpenMap();
  s_bWantOpen = true;
 }

 static void Close()
 {
  s_bWantOpen = false;
  if (s_Editor) s_Editor.Destroy();
  s_Editor = null;
 }

 static void Ask()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc) pc.ACR_DeckAsk();
 }

 static void HandIn(ACR_DeckData deck)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc && deck) pc.ACR_DeckHandIn(deck.ToText());
 }

 static void Received()
 {
  if (s_Editor) s_Editor.Refresh();
 }

 static void Result(string text)
 {
  s_sResult = text;
  if (s_Editor) s_Editor.SetStatus(text);
  else SCR_HintManagerComponent.ShowCustomHint(text, "AC: Actual", 8, true);
 }

 static void Reset()
 {
  Close();
  s_Views = null;
  s_sResult = "";
 }
}

// The editor panel.
class ACR_DeckEditor
{
 static const float WIDTH = 1480;
 static const float CATALOGUE_WIDTH = 820;
 static const float LIST_HEIGHT = 560;
 static const float COL_POINTS = 70;
 static const float COL_INFO = 250;
 static const float COL_BUTTON = 96;
 static const int NAME_SIZE = 18;

 protected Widget m_Root;
 protected TextWidget m_Title;
 protected TextWidget m_Points;
 protected TextWidget m_Status;
 protected TextWidget m_Applied;
 protected Widget m_Tabs;
 protected Widget m_Left;
 protected Widget m_Right;
 protected TextWidget m_LeftTitle;
 protected EditBoxWidget m_NameEdit;
 protected EditBoxWidget m_CardEdit;
 protected Widget m_CardEditBox;
 protected ref map<SCR_ButtonBaseComponent, string> m_Fixed = new map<SCR_ButtonBaseComponent, string>();
 protected ref map<SCR_ButtonBaseComponent, string> m_Rows = new map<SCR_ButtonBaseComponent, string>();
 protected int m_iSide;
 protected int m_iDeck;
 protected ref ACR_CustomCard m_Custom;  // the custom card being made or changed, else null
 protected string m_sCustomWas;          // its name before (empty for a new one)
 protected bool m_bDeleteArmed;

 void Create(Widget parent)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget column = ACR_TwoICStyle.PanelBox(parent, WIDTH, 0, 0, m_Root, ACR_TwoICStyle.Rgb(16, 19, 23, 0.99));
  FrameSlot.SetAnchorMin(m_Root, 0.5, 0.5);
  FrameSlot.SetAnchorMax(m_Root, 0.5, 0.5);
  FrameSlot.SetAlignment(m_Root, 0.5, 0.5);
  FrameSlot.SetPos(m_Root, 0, 10);
  FrameSlot.SetSizeToContent(m_Root, true);
  m_Root.SetZOrder(1150);

  // Header: title, the two sides, the points, Close.
  Widget header = ACR_TwoICStyle.Row(column);
  AlignableSlot.SetPadding(header, 26, 16, 26, 12);
  SizeLayoutWidget dot = ACR_TwoICStyle.Box(header, 16, 16, ACR_TwoICStyle.Accent());
  AlignableSlot.SetVerticalAlign(dot, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(dot, 0, 0, 14, 0);
  TextWidget title = ACR_TwoICStyle.Text(header, "DECKS", ACR_TwoICStyle.White(), 32, true);
  AlignableSlot.SetVerticalAlign(title, LayoutVerticalAlign.Center);
  m_Tabs = ACR_TwoICStyle.Row(header);
  AlignableSlot.SetVerticalAlign(m_Tabs, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(m_Tabs, 24, 0, 0, 0);
  m_Points = ACR_TwoICStyle.Text(header, "", ACR_TwoICStyle.White(), 24, true);
  AlignableSlot.SetVerticalAlign(m_Points, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(m_Points, 24, 0, 0, 0);
  ACR_TwoICStyle.Grow(m_Points);
  SCR_ButtonTextComponent close = ACR_TwoICStyle.Button(header, "X", ACR_TwoICStyle.BUTTON_GHOST, 50, 22, 50);
  Fixed(close, "close");

  // The deck bar: < name >, rename, new, copy, delete; codes; hand in.
  Widget bar = ACR_TwoICStyle.Row(column);
  AlignableSlot.SetPadding(bar, 26, 0, 26, 10);
  Fixed(ACR_TwoICStyle.Button(bar, "<", ACR_TwoICStyle.BUTTON_GHOST, 44, 20, 44), "prev");
  SizeLayoutWidget nameBox = ACR_TwoICStyle.Box(bar, 330, 44, null);
  Widget nameWidget = ws.CreateWidgets(ACR_SquadsList.SEARCH_LAYOUT, nameBox);
  if (nameWidget)
  {
   AlignableSlot.SetHorizontalAlign(nameWidget, LayoutHorizontalAlign.Stretch);
   AlignableSlot.SetVerticalAlign(nameWidget, LayoutVerticalAlign.Stretch);
   m_NameEdit = EditBoxWidget.Cast(nameWidget.FindAnyWidget("EditBox"));
   if (m_NameEdit) m_NameEdit.SetPlaceholderText("Deck name");
  }
  Fixed(ACR_TwoICStyle.Button(bar, ">", ACR_TwoICStyle.BUTTON_GHOST, 44, 20, 44), "next");
  Fixed(ACR_TwoICStyle.Button(bar, "Rename", ACR_TwoICStyle.BUTTON_OUTLINE, 110, 17, 44), "rename");
  Fixed(ACR_TwoICStyle.Button(bar, "New", ACR_TwoICStyle.BUTTON_OUTLINE, 80, 17, 44), "new");
  Fixed(ACR_TwoICStyle.Button(bar, "Copy", ACR_TwoICStyle.BUTTON_OUTLINE, 80, 17, 44), "copy");
  Fixed(ACR_TwoICStyle.Button(bar, "Delete", ACR_TwoICStyle.BUTTON_OUTLINE, 96, 17, 44), "delete");
  ACR_TwoICStyle.Grow(ACR_TwoICStyle.Box(bar, 10, 1, null));
  Fixed(ACR_TwoICStyle.Button(bar, "Copy code", ACR_TwoICStyle.BUTTON_QUIET, 120, 17, 44), "export");
  Fixed(ACR_TwoICStyle.Button(bar, "Paste code", ACR_TwoICStyle.BUTTON_QUIET, 120, 17, 44), "import");
  Fixed(ACR_TwoICStyle.Button(bar, "Hand in", ACR_TwoICStyle.BUTTON_FILL, 130, 18, 44), "handin");
  ACR_TwoICStyle.Box(column, 0, 1, ACR_TwoICStyle.Track());

  // Body: the catalogue (or the custom card being made) and the deck.
  Widget body = ACR_TwoICStyle.Row(column);
  SizeLayoutWidget leftBox = ACR_TwoICStyle.Box(body, CATALOGUE_WIDTH, 0, null);
  Widget leftColumn = ACR_TwoICStyle.Column(leftBox);
  AlignableSlot.SetPadding(leftColumn, 26, 14, 20, 14);
  m_LeftTitle = ACR_TwoICStyle.Text(leftColumn, "CATALOGUE", ACR_TwoICStyle.White(), 24, true);
  // The custom card's name (shown while making one).
  m_CardEditBox = ACR_TwoICStyle.Box(leftColumn, CATALOGUE_WIDTH - 60, 44, null);
  Widget cardWidget = ws.CreateWidgets(ACR_SquadsList.SEARCH_LAYOUT, m_CardEditBox);
  if (cardWidget)
  {
   AlignableSlot.SetHorizontalAlign(cardWidget, LayoutHorizontalAlign.Stretch);
   AlignableSlot.SetVerticalAlign(cardWidget, LayoutVerticalAlign.Stretch);
   m_CardEdit = EditBoxWidget.Cast(cardWidget.FindAnyWidget("EditBox"));
   if (m_CardEdit) m_CardEdit.SetPlaceholderText("Card name");
  }
  m_Left = List(leftColumn, CATALOGUE_WIDTH - 60);
  ACR_TwoICStyle.Box(body, 1, 0, ACR_TwoICStyle.Track());
  Widget rightColumn = ACR_TwoICStyle.Column(body);
  ACR_TwoICStyle.Grow(rightColumn);
  AlignableSlot.SetPadding(rightColumn, 20, 14, 26, 14);
  Widget deckHead = ACR_TwoICStyle.Row(rightColumn);
  m_Title = ACR_TwoICStyle.Text(deckHead, "DECK", ACR_TwoICStyle.White(), 24, true);
  ACR_TwoICStyle.Grow(m_Title);
  Fixed(ACR_TwoICStyle.Button(deckHead, "New custom card", ACR_TwoICStyle.BUTTON_OUTLINE, 190, 17, 40), "custom:new");
  m_Right = List(rightColumn, WIDTH - CATALOGUE_WIDTH - 60);
  ACR_TwoICStyle.Box(column, 0, 1, ACR_TwoICStyle.Track());

  // Footer: what the server said, what the sides play.
  Widget footer = ACR_TwoICStyle.Column(column);
  AlignableSlot.SetPadding(footer, 26, 10, 26, 14);
  m_Status = ACR_TwoICStyle.Text(footer, "", ACR_TwoICStyle.Accent(), 17);
  m_Applied = ACR_TwoICStyle.Text(footer, "", ACR_TwoICStyle.Grey(), 16);
  ACR_PanelMover.Add("deck", m_Root, 0.5, 0.5, 0, 10);
  ACR_PanelMover.TitleBar("deck", m_Root, 76, 70);

  m_iSide = 0;
  Faction mine = SCR_FactionManager.SGetLocalPlayerFaction();
  if (mine && mine.GetFactionKey() == ACR_FactionSelection.East()) m_iSide = 1;
  SetStatus(ACR_DeckClient.s_sResult);
  Refresh();
 }

 void Destroy()
 {
  ACR_PanelMover.Remove("deck");
  if (m_Root) m_Root.RemoveFromHierarchy();
  m_Root = null;
  m_Fixed.Clear();
  m_Rows.Clear();
 }

 protected Widget List(Widget parent, float width)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  SizeLayoutWidget listBox = ACR_TwoICStyle.Box(parent, width, LIST_HEIGHT, null);
  AlignableSlot.SetPadding(listBox, 0, 8, 0, 0);
  Widget scroll = ws.CreateWidget(WidgetType.ScrollLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.CLIPCHILDREN, Color.FromInt(Color.WHITE), 0, listBox);
  AlignableSlot.SetHorizontalAlign(scroll, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(scroll, LayoutVerticalAlign.Stretch);
  SizeLayoutWidget listWidth = ACR_TwoICStyle.Box(scroll, width - 16, 0, null);
  AlignableSlot.SetVerticalAlign(listWidth, LayoutVerticalAlign.Top);
  Widget list = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, listWidth);
  AlignableSlot.SetHorizontalAlign(list, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(list, LayoutVerticalAlign.Top);
  return list;
 }

 protected void Fixed(SCR_ButtonTextComponent button, string action)
 {
  if (!button) return;
  button.m_OnClicked.Insert(OnFixed);
  m_Fixed.Set(button, action);
 }

 protected SCR_ButtonTextComponent RowButton(Widget parent, string text, int look, string action, float width = COL_BUTTON)
 {
  SCR_ButtonTextComponent button = ACR_TwoICStyle.Button(parent, text, look, width, 16, 36);
  if (!button) return null;
  if (action.IsEmpty()) { button.SetEnabled(false, false); return button; }
  button.m_OnClicked.Insert(OnRow);
  m_Rows.Set(button, action);
  return button;
 }

 void SetStatus(string text)
 {
  if (m_Status) m_Status.SetText(text);
 }

 // ---------------------------------------------------------------- state
 protected ACR_DeckView View() { return ACR_DeckClient.View(m_iSide); }

 protected ACR_DeckData Deck()
 {
  ACR_DeckView view = View();
  if (!view || view.m_sFaction.IsEmpty()) return null;
  array<ACR_DeckData> decks = {};
  ACR_DeckStore.Of(view.m_sFaction, decks);
  if (decks.IsEmpty())
  {
   // The first look at a faction: its standard deck to start from.
   if (view.m_aNames.IsEmpty()) return null;
   ACR_DeckData first = new ACR_DeckData();
   first.m_sFaction = view.m_sFaction;
   first.m_sName = "Standard";
   view.Standard(first);
   ACR_DeckStore.All().Insert(first);
   ACR_DeckStore.Save();
   decks.Insert(first);
  }
  if (m_iDeck >= decks.Count()) m_iDeck = decks.Count() - 1;
  if (m_iDeck < 0) m_iDeck = 0;
  return decks[m_iDeck];
 }

 protected void Changed()
 {
  ACR_DeckStore.Save();
  Refresh();
 }

 // ---------------------------------------------------------------- drawing
 void Refresh()
 {
  if (!m_Root) return;
  Clear(m_Tabs);
  Clear(m_Left);
  Clear(m_Right);
  // Tab buttons live in m_Fixed under "side:": drop the old ones.
  array<SCR_ButtonBaseComponent> old = {};
  foreach (SCR_ButtonBaseComponent b, string a : m_Fixed) if (a.StartsWith("side:")) old.Insert(b);
  foreach (SCR_ButtonBaseComponent gone : old) m_Fixed.Remove(gone);
  m_Rows.Clear();
  for (int side = 0; side < 2; side++)
  {
   ACR_DeckView tabView = ACR_DeckClient.View(side);
   string label = "Blue";
   if (side == 1) label = "Red";
   if (tabView && !tabView.m_sFaction.IsEmpty()) label += ": " + ACR_FactionSelection.DisplayName(tabView.m_sFaction);
   int look = ACR_TwoICStyle.BUTTON_OUTLINE;
   if (side == m_iSide) look = ACR_TwoICStyle.BUTTON_FILL;
   Fixed(ACR_TwoICStyle.Button(m_Tabs, label, look, 230, 17, 42), "side:" + side.ToString());
  }
  ACR_DeckView view = View();
  ACR_DeckData deck = Deck();
  bool making = m_Custom != null;
  m_CardEditBox.SetVisible(making);
  if (!view || !deck)
  {
   m_Points.SetText("");
   m_Title.SetText("DECK");
   m_LeftTitle.SetText("CATALOGUE");
   ACR_TwoICStyle.Text(m_Left, "Asking the server for the catalogue...", ACR_TwoICStyle.Grey(), 18);
   return;
  }
  int points = view.Points(deck);
  string budget = ACR_DeckRules.BudgetName(view.m_iBudget);
  Color pointsColor = ACR_TwoICStyle.White();
  if (view.m_iBudget > 0 && points > view.m_iBudget) pointsColor = ACR_TwoICStyle.Rgb(235, 110, 90);
  m_Points.SetText(string.Format("%1 / %2 points", points, budget));
  m_Points.SetColor(pointsColor);
  array<ACR_DeckData> decks = {};
  ACR_DeckStore.Of(view.m_sFaction, decks);
  m_Title.SetText(string.Format("DECK %1 OF %2", m_iDeck + 1, decks.Count()));
  if (m_NameEdit && GetGame().GetWorkspace().GetFocusedWidget() != m_NameEdit) m_NameEdit.SetText(deck.m_sName);
  m_Applied.SetText(view.m_sApplied);
  if (making) DrawCustom(view, deck);
  else DrawCatalogue(view, deck);
  DrawDeck(view, deck);
 }

 protected void DrawCatalogue(ACR_DeckView view, ACR_DeckData deck)
 {
  m_LeftTitle.SetText("CATALOGUE");
  for (int role = 0; role < ACR_DeckRules.RoleCount(); role++)
  {
   bool any;
   foreach (int i, string name : view.m_aNames)
   {
    if (view.m_aRoles[i] != role) continue;
    if (!any) Heading(m_Left, ACR_DeckRules.RoleName(role));
    any = true;
    Widget row = ACR_TwoICStyle.Row(m_Left, 2);
    string shown = name;
    if (view.Has(i, ACR_DeckFlag.EXTRA)) shown += ACR_TwoICStyle.Colored("  new", ACR_TwoICStyle.Accent());
    TextWidget nameText = ACR_TwoICStyle.Text(row, shown, ACR_TwoICStyle.White(), NAME_SIZE);
    AlignableSlot.SetVerticalAlign(nameText, LayoutVerticalAlign.Center);
    ACR_TwoICStyle.Grow(nameText);
    Cell(row, view.m_aInfos[i], COL_INFO, ACR_TwoICStyle.Grey());
    Cell(row, PointsText(view.m_aPoints[i], view.Has(i, ACR_DeckFlag.LOCKED)), COL_POINTS, ACR_TwoICStyle.White());
    if (view.Has(i, ACR_DeckFlag.LOCKED)) RowButton(row, "Always", ACR_TwoICStyle.BUTTON_OFF, "");
    else if (deck.m_aCards.Contains(name)) RowButton(row, "In deck", ACR_TwoICStyle.BUTTON_OFF, "");
    else RowButton(row, "Add", ACR_TwoICStyle.BUTTON_OUTLINE, "add:" + i.ToString());
   }
  }
  // Custom cards from this faction's other decks: add a copy.
  array<ACR_DeckData> decks = {};
  ACR_DeckStore.Of(view.m_sFaction, decks);
  array<string> seen = {};
  foreach (ACR_CustomCard own : deck.m_aCustom) seen.Insert(own.m_sName);
  bool heading;
  foreach (int d, ACR_DeckData other : decks)
  {
   foreach (int c, ACR_CustomCard custom : other.m_aCustom)
   {
    if (seen.Contains(custom.m_sName)) continue;
    seen.Insert(custom.m_sName);
    if (!heading) Heading(m_Left, "Your custom cards (other decks)");
    heading = true;
    Widget customRow = ACR_TwoICStyle.Row(m_Left, 2);
    TextWidget customName = ACR_TwoICStyle.Text(customRow, custom.m_sName, ACR_TwoICStyle.Blue(), NAME_SIZE);
    AlignableSlot.SetVerticalAlign(customName, LayoutVerticalAlign.Center);
    ACR_TwoICStyle.Grow(customName);
    Cell(customRow, CustomInfo(view, custom), COL_INFO, ACR_TwoICStyle.Grey());
    Cell(customRow, PointsText(view.CustomPoints(custom), false), COL_POINTS, ACR_TwoICStyle.White());
    RowButton(customRow, "Add", ACR_TwoICStyle.BUTTON_OUTLINE, string.Format("addcustom:%1:%2", d, c));
   }
  }
 }

 protected void DrawDeck(ACR_DeckView view, ACR_DeckData deck)
 {
  for (int role = 0; role < ACR_DeckRules.RoleCount(); role++)
  {
   bool any;
   foreach (int i, string name : view.m_aNames)
   {
    if (view.m_aRoles[i] != role) continue;
    bool locked = view.Has(i, ACR_DeckFlag.LOCKED);
    if (!locked && !deck.m_aCards.Contains(name)) continue;
    if (!any) Heading(m_Right, ACR_DeckRules.RoleName(role));
    any = true;
    Widget row = ACR_TwoICStyle.Row(m_Right, 2);
    TextWidget nameText = ACR_TwoICStyle.Text(row, name, ACR_TwoICStyle.White(), NAME_SIZE);
    AlignableSlot.SetVerticalAlign(nameText, LayoutVerticalAlign.Center);
    ACR_TwoICStyle.Grow(nameText);
    Cell(row, PointsText(view.m_aPoints[i], locked), COL_POINTS, ACR_TwoICStyle.White());
    if (locked) RowButton(row, "Always", ACR_TwoICStyle.BUTTON_OFF, "");
    else RowButton(row, "Remove", ACR_TwoICStyle.BUTTON_GHOST, "remove:" + name);
   }
  }
  // Cards this match's mods don't have (kept in the deck, left out in the match).
  bool missingHead;
  foreach (string card : deck.m_aCards)
  {
   if (view.Index(card) >= 0) continue;
   if (!missingHead) Heading(m_Right, "Not in this match's mods");
   missingHead = true;
   Widget missingRow = ACR_TwoICStyle.Row(m_Right, 2);
   TextWidget missingName = ACR_TwoICStyle.Text(missingRow, card, ACR_TwoICStyle.Dim(), NAME_SIZE);
   ACR_TwoICStyle.Grow(missingName);
   RowButton(missingRow, "Remove", ACR_TwoICStyle.BUTTON_GHOST, "remove:" + card);
  }
  if (!deck.m_aCustom.IsEmpty()) Heading(m_Right, "Custom cards");
  foreach (int c, ACR_CustomCard custom : deck.m_aCustom)
  {
   Widget customRow = ACR_TwoICStyle.Row(m_Right, 2);
   TextWidget customName = ACR_TwoICStyle.Text(customRow, custom.m_sName, ACR_TwoICStyle.Blue(), NAME_SIZE);
   AlignableSlot.SetVerticalAlign(customName, LayoutVerticalAlign.Center);
   ACR_TwoICStyle.Grow(customName);
   Cell(customRow, PointsText(view.CustomPoints(custom), false), COL_POINTS, ACR_TwoICStyle.White());
   RowButton(customRow, "Edit", ACR_TwoICStyle.BUTTON_LINK, "edit:" + c.ToString(), 70);
   RowButton(customRow, "Remove", ACR_TwoICStyle.BUTTON_GHOST, "dropcustom:" + c.ToString());
  }
 }

 // Making or changing a custom card: its men, its ride, its price.
 protected void DrawCustom(ACR_DeckView view, ACR_DeckData deck)
 {
  bool sf;
  int price = view.CustomPrice(m_Custom, sf);
  int points = ACR_DeckRules.PointsOfPrice(price, sf);
  string head = "NEW CUSTOM CARD";
  if (!m_sCustomWas.IsEmpty()) head = "CUSTOM CARD";
  m_LeftTitle.SetText(string.Format("%1  ·  %2 men, %3 RP, %4 points", head, m_Custom.m_aSoldiers.Count(), price, points));
  Widget actions = ACR_TwoICStyle.Row(m_Left, 6);
  RowButton(actions, "Save card", ACR_TwoICStyle.BUTTON_FILL, "cardsave", 150);
  RowButton(actions, "Cancel", ACR_TwoICStyle.BUTTON_OUTLINE, "cardcancel", 110);
  ACR_TwoICStyle.Grow(ACR_TwoICStyle.Text(actions, "", ACR_TwoICStyle.Grey(), 16));
  Heading(m_Left, string.Format("Soldiers (%1 to %2 men; special forces make it a rare SF card)", ACR_DeckRules.CUSTOM_MIN_MEN, ACR_DeckRules.CUSTOM_MAX_MEN));
  if (view.m_aSoldierIds.IsEmpty()) ACR_TwoICStyle.Text(m_Left, "This faction has no soldiers to pick from here.", ACR_TwoICStyle.Grey(), 17);
  foreach (int s, string id : view.m_aSoldierIds)
  {
   int count;
   foreach (string man : m_Custom.m_aSoldiers) if (man == id) count++;
   Widget row = ACR_TwoICStyle.Row(m_Left, 2);
   Color color = ACR_TwoICStyle.White();
   if ((view.m_aSoldierFlags[s] & ACR_DeckFlag.SF) != 0) color = ACR_TwoICStyle.Accent();
   TextWidget nameText = ACR_TwoICStyle.Text(row, view.m_aSoldierNames[s], color, NAME_SIZE);
   AlignableSlot.SetVerticalAlign(nameText, LayoutVerticalAlign.Center);
   ACR_TwoICStyle.Grow(nameText);
   Cell(row, view.m_aSoldierRP[s].ToString() + " RP", 90, ACR_TwoICStyle.Grey());
   RowButton(row, "-", ACR_TwoICStyle.BUTTON_GHOST, "less:" + s.ToString(), 50);
   Cell(row, count.ToString(), 40, ACR_TwoICStyle.White());
   RowButton(row, "+", ACR_TwoICStyle.BUTTON_GHOST, "more:" + s.ToString(), 50);
  }
  Heading(m_Left, "Ride (optional): the team gets in when it arrives");
  Widget ride = ACR_TwoICStyle.Row(m_Left, 2);
  string rideName = "On foot";
  if (!m_Custom.m_sTransport.IsEmpty()) rideName = m_Custom.m_sTransport;
  TextWidget rideText = ACR_TwoICStyle.Text(ride, rideName, ACR_TwoICStyle.White(), NAME_SIZE);
  AlignableSlot.SetVerticalAlign(rideText, LayoutVerticalAlign.Center);
  ACR_TwoICStyle.Grow(rideText);
  RowButton(ride, "Change", ACR_TwoICStyle.BUTTON_OUTLINE, "ride", 110);
 }

 protected void Heading(Widget list, string text)
 {
  TextWidget heading = ACR_TwoICStyle.Text(list, text, ACR_TwoICStyle.Grey(), 16, true);
  if (heading) AlignableSlot.SetPadding(heading, 0, 12, 0, 2);
 }

 protected void Cell(Widget row, string text, float width, Color color)
 {
  SizeLayoutWidget box = ACR_TwoICStyle.Box(row, width, 0, null);
  AlignableSlot.SetVerticalAlign(box, LayoutVerticalAlign.Center);
  TextWidget cell = ACR_TwoICStyle.Text(box, text, color, 16);
  if (cell) AlignableSlot.SetVerticalAlign(cell, LayoutVerticalAlign.Center);
 }

 protected string PointsText(int points, bool locked)
 {
  if (locked) return "free";
  return points.ToString() + " pts";
 }

 protected string CustomInfo(ACR_DeckView view, ACR_CustomCard card)
 {
  bool sf;
  int price = view.CustomPrice(card, sf);
  string info = card.m_aSoldiers.Count().ToString() + " men";
  if (!card.m_sTransport.IsEmpty()) info += " + ride";
  return info + ", " + price.ToString() + " RP";
 }

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

 // ---------------------------------------------------------------- buttons
 protected void OnFixed(SCR_ButtonBaseComponent button)
 {
  string action;
  if (!m_Fixed.Find(button, action)) return;
  Act(action);
 }

 protected void OnRow(SCR_ButtonBaseComponent button)
 {
  string action;
  if (!m_Rows.Find(button, action)) return;
  // Rebuilding the lists destroys this button: act after its click has finished.
  GetGame().GetCallqueue().CallLater(Act, 0, false, action);
 }

 protected void Act(string action)
 {
  if (!m_Root) return;
  if (action != "delete") m_bDeleteArmed = false;
  if (action == "close") { GetGame().GetCallqueue().CallLater(ACR_DeckClient.Close, 0, false); return; }
  if (action.StartsWith("side:"))
  {
   m_iSide = action.Substring(5, action.Length() - 5).ToInt();
   m_iDeck = 0;
   m_Custom = null;
   // The host may have changed the sides or the deck rows since: ask again.
   ACR_DeckClient.Ask();
   GetGame().GetCallqueue().CallLater(Refresh, 0, false);
   return;
  }
  ACR_DeckView view = View();
  ACR_DeckData deck = Deck();
  if (!view || !deck) return;
  array<ACR_DeckData> decks = {};
  ACR_DeckStore.Of(view.m_sFaction, decks);
  if (action == "prev" || action == "next")
  {
   int step = 1;
   if (action == "prev") step = -1;
   m_iDeck = m_iDeck + step;
   if (m_iDeck < 0) m_iDeck = decks.Count() - 1;
   if (m_iDeck >= decks.Count()) m_iDeck = 0;
   m_Custom = null;
   Refresh();
   return;
  }
  if (action == "rename")
  {
   string wanted = "";
   if (m_NameEdit) wanted = m_NameEdit.GetText();
   wanted = ACR_DeckRules.CleanName(wanted);
   if (wanted.IsEmpty() || wanted == deck.m_sName) { SetStatus("Type a new name in the box, then Rename."); return; }
   deck.m_sName = ACR_DeckStore.FreeName(view.m_sFaction, wanted);
   SetStatus("Renamed to " + deck.m_sName);
   Changed();
   return;
  }
  if (action == "new" || action == "copy")
  {
   ACR_DeckData made = new ACR_DeckData();
   made.m_sFaction = view.m_sFaction;
   if (action == "copy")
   {
    made = deck.Copy();
    made.m_sName = ACR_DeckStore.FreeName(view.m_sFaction, deck.m_sName);
   }
   else
   {
    made.m_sName = ACR_DeckStore.FreeName(view.m_sFaction, "Deck");
    view.Standard(made);
   }
   ACR_DeckStore.All().Insert(made);
   m_iDeck = decks.Count();
   m_Custom = null;
   SetStatus(made.m_sName + ": a new deck from the standard one. Remove and add cards to fit the points.");
   if (action == "copy") SetStatus("Copied as " + made.m_sName);
   Changed();
   return;
  }
  if (action == "delete")
  {
   if (!m_bDeleteArmed) { m_bDeleteArmed = true; SetStatus("Press Delete again to delete " + deck.m_sName + " for good."); return; }
   m_bDeleteArmed = false;
   ACR_DeckStore.All().RemoveItem(deck);
   SetStatus(deck.m_sName + " deleted");
   m_Custom = null;
   Changed();
   return;
  }
  if (action == "export")
  {
   System.ExportToClipboard(deck.ToText());
   SetStatus("The deck's code is on the clipboard: paste it to a friend, they press Paste code.");
   return;
  }
  if (action == "import")
  {
   ACR_DeckData pasted = ACR_DeckData.FromText(System.ImportFromClipboard());
   if (!pasted) { SetStatus("The clipboard has no deck code (it starts with ACADECK1)."); return; }
   pasted.m_sName = ACR_DeckStore.FreeName(pasted.m_sFaction, pasted.m_sName);
   ACR_DeckStore.All().Insert(pasted);
   if (pasted.m_sFaction == view.m_sFaction)
   {
    array<ACR_DeckData> after = {};
    ACR_DeckStore.Of(view.m_sFaction, after);
    m_iDeck = after.Find(pasted);
    SetStatus("Pasted " + pasted.m_sName);
   }
   else SetStatus(string.Format("Pasted %1, a %2 deck: it shows when %2 plays.", pasted.m_sName, pasted.m_sFaction));
   Changed();
   return;
  }
  if (action == "handin")
  {
   // Decks are taken only during match setup (ACR_Decks.Accept): say so here, without asking the server.
   if (!ACR_SetupClient.IsActive())
   {
    SetStatus("Decks are handed in during match setup: this match has started. Copy code shares it instead.");
    return;
   }
   if (view.m_iBudget > 0 && view.Points(deck) > view.m_iBudget) SetStatus("Over the points: the server drops the dearest cards. Handing in...");
   else SetStatus("Handing in " + deck.m_sName + "...");
   ACR_DeckClient.HandIn(deck);
   return;
  }
  if (action.StartsWith("add:"))
  {
   int index = action.Substring(4, action.Length() - 4).ToInt();
   if (index < 0 || index >= view.m_aNames.Count()) return;
   if (deck.m_aCards.Count() >= ACR_DeckRules.MAX_CARDS) { SetStatus("A deck holds at most " + ACR_DeckRules.MAX_CARDS.ToString() + " cards."); return; }
   if (!deck.m_aCards.Contains(view.m_aNames[index])) deck.m_aCards.Insert(view.m_aNames[index]);
   Changed();
   return;
  }
  if (action.StartsWith("remove:"))
  {
   deck.m_aCards.RemoveItemOrdered(action.Substring(7, action.Length() - 7));
   Changed();
   return;
  }
  if (action.StartsWith("addcustom:"))
  {
   array<string> parts = {};
   action.Split(":", parts, false);
   if (parts.Count() < 3) return;
   int d = parts[1].ToInt();
   int c = parts[2].ToInt();
   if (d < 0 || d >= decks.Count() || c < 0 || c >= decks[d].m_aCustom.Count()) return;
   if (deck.m_aCustom.Count() >= ACR_DeckRules.MAX_CUSTOM) { SetStatus("A deck holds at most " + ACR_DeckRules.MAX_CUSTOM.ToString() + " custom cards."); return; }
   deck.m_aCustom.Insert(decks[d].m_aCustom[c].Copy());
   Changed();
   return;
  }
  if (action.StartsWith("dropcustom:"))
  {
   int drop = action.Substring(11, action.Length() - 11).ToInt();
   if (drop >= 0 && drop < deck.m_aCustom.Count()) deck.m_aCustom.RemoveOrdered(drop);
   Changed();
   return;
  }
  if (action == "custom:new")
  {
   if (deck.m_aCustom.Count() >= ACR_DeckRules.MAX_CUSTOM) { SetStatus("A deck holds at most " + ACR_DeckRules.MAX_CUSTOM.ToString() + " custom cards."); return; }
   m_Custom = new ACR_CustomCard();
   m_sCustomWas = "";
   if (m_CardEdit) m_CardEdit.SetText("");
   SetStatus("Pick the men with + and -, name the card, then Save card.");
   Refresh();
   return;
  }
  if (action.StartsWith("edit:"))
  {
   int edit = action.Substring(5, action.Length() - 5).ToInt();
   if (edit < 0 || edit >= deck.m_aCustom.Count()) return;
   m_Custom = deck.m_aCustom[edit].Copy();
   m_sCustomWas = m_Custom.m_sName;
   if (m_CardEdit) m_CardEdit.SetText(m_Custom.m_sName);
   Refresh();
   return;
  }
  if (!m_Custom) return;
  if (action == "cardcancel") { m_Custom = null; Refresh(); return; }
  if (action.StartsWith("more:") || action.StartsWith("less:"))
  {
   int s = action.Substring(5, action.Length() - 5).ToInt();
   if (s < 0 || s >= view.m_aSoldierIds.Count()) return;
   string id = view.m_aSoldierIds[s];
   if (action.StartsWith("more:"))
   {
    if (m_Custom.m_aSoldiers.Count() >= ACR_DeckRules.CUSTOM_MAX_MEN) { SetStatus("A team has at most " + ACR_DeckRules.CUSTOM_MAX_MEN.ToString() + " men."); return; }
    m_Custom.m_aSoldiers.Insert(id);
   }
   else
   {
    int at = m_Custom.m_aSoldiers.Find(id);
    if (at >= 0) m_Custom.m_aSoldiers.RemoveOrdered(at);
   }
   SortMen(view);
   Refresh();
   return;
  }
  if (action == "ride")
  {
   // On foot, then each transport in the catalogue in turn.
   array<string> rides = {""};
   foreach (int r, string rideName : view.m_aNames) if (view.Has(r, ACR_DeckFlag.TRANSPORT)) rides.Insert(rideName);
   int now = rides.Find(m_Custom.m_sTransport);
   m_Custom.m_sTransport = rides[(now + 1) % rides.Count()];
   Refresh();
   return;
  }
  if (action == "cardsave")
  {
   string typed = "";
   if (m_CardEdit) typed = m_CardEdit.GetText();
   string name = ACR_DeckRules.CleanName(typed);
   if (name.IsEmpty()) { SetStatus("Name the card first (the box at the top)."); return; }
   if (view.Index(name) >= 0) { SetStatus(name + " is a catalogue card's name: pick another."); return; }
   if (m_Custom.m_aSoldiers.Count() < ACR_DeckRules.CUSTOM_MIN_MEN) { SetStatus("A team needs at least " + ACR_DeckRules.CUSTOM_MIN_MEN.ToString() + " men."); return; }
   ACR_CustomCard same = deck.FindCustom(name);
   if (same && name != m_sCustomWas) { SetStatus("This deck has a custom card called " + name + " already."); return; }
   m_Custom.m_sName = name;
   ACR_CustomCard was = deck.FindCustom(m_sCustomWas);
   if (was && !m_sCustomWas.IsEmpty())
   {
    int place = deck.m_aCustom.Find(was);
    deck.m_aCustom.Set(place, m_Custom);
   }
   else deck.m_aCustom.Insert(m_Custom);
   SetStatus(string.Format("%1 saved: %2 points.", name, view.CustomPoints(m_Custom)));
   m_Custom = null;
   Changed();
   return;
  }
 }

 // The men in the catalogue's order (leaders first, as the soldier list shows them).
 protected void SortMen(ACR_DeckView view)
 {
  array<string> sorted = {};
  foreach (string id : view.m_aSoldierIds)
   foreach (string man : m_Custom.m_aSoldiers) if (man == id) sorted.Insert(man);
  foreach (string other : m_Custom.m_aSoldiers) if (view.m_aSoldierIds.Find(other) < 0) sorted.Insert(other);
  m_Custom.m_aSoldiers.Copy(sorted);
 }
}

// The editor's requests and the server's answers.
modded class SCR_PlayerController
{
 protected float m_fACR_DeckAskAt = -100000;
 protected float m_fACR_DeckHandInAt = -100000;

 void ACR_DeckAsk()
 {
  Rpc(ACR_DeckAskServer);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_DeckAskServer()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_fACR_DeckAskAt < 1000) return;
  m_fACR_DeckAskAt = now;
  for (int side = 0; side < 2; side++)
  {
   string faction = ACR_FactionSelection.West();
   if (side == 1) faction = ACR_FactionSelection.East();
   array<string> names = {};
   array<int> roles = {};
   array<int> points = {};
   array<int> prices = {};
   array<int> flags = {};
   array<string> infos = {};
   array<string> soldierIds = {};
   array<string> soldierNames = {};
   array<int> soldierRP = {};
   array<int> soldierFlags = {};
   ACR_Decks.Catalogue(faction, names, roles, points, prices, flags, infos, soldierIds, soldierNames, soldierRP, soldierFlags);
   string applied = ACR_Decks.Applied(faction);
   if (applied.IsEmpty()) applied = "plays " + ACR_Decks.RowName(faction, ACR_Decks.Chosen(faction));
   Rpc(ACR_DeckHeaderOwner, side, faction, ACR_DeckRules.Budget(), applied);
   Rpc(ACR_DeckCardsOwner, side, names, roles, points, prices, flags, infos);
   Rpc(ACR_DeckSoldiersOwner, side, faction, soldierIds, soldierNames, soldierRP, soldierFlags);
  }
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_DeckHeaderOwner(int side, string faction, int budget, string applied)
 {
  ACR_DeckView view = ACR_DeckClient.Store(side, faction);
  view.m_iBudget = budget;
  string sideName = "Blue";
  if (side == 1) sideName = "Red";
  view.m_sApplied = sideName + " (" + faction + ") " + applied;
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_DeckCardsOwner(int side, array<string> names, array<int> roles, array<int> points, array<int> prices, array<int> flags, array<string> infos)
 {
  ACR_DeckView view = ACR_DeckClient.View(side);
  if (!view) return;
  view.m_aNames.Copy(names);
  view.m_aRoles.Copy(roles);
  view.m_aPoints.Copy(points);
  view.m_aPrices.Copy(prices);
  view.m_aFlags.Copy(flags);
  view.m_aInfos.Copy(infos);
  ACR_DeckClient.Received();
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_DeckSoldiersOwner(int side, string faction, array<string> ids, array<string> names, array<int> rp, array<int> flags)
 {
  ACR_DeckView view = ACR_DeckClient.Store(side, faction);
  view.m_aSoldierIds.Copy(ids);
  view.m_aSoldierNames.Copy(names);
  view.m_aSoldierRP.Copy(rp);
  view.m_aSoldierFlags.Copy(flags);
  ACR_DeckClient.Received();
 }

 void ACR_DeckHandIn(string text)
 {
  Rpc(ACR_DeckHandInServer, text);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_DeckHandInServer(string text)
 {
  // Its own limiter (review of 0.5.13): a hand-in reads the catalogue and sends it back, so a
  // client sending them fast could hitch the server. After Start it's refused before any of that.
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_fACR_DeckHandInAt < 2000) { Rpc(ACR_DeckResultOwner, "Too fast: wait a moment and hand it in again"); return; }
  m_fACR_DeckHandInAt = now;
  if (ACR_Setup.Started()) { Rpc(ACR_DeckResultOwner, "The match has started: hand a deck in during match setup"); return; }
  string result = ACR_Decks.Submit(GetPlayerId(), text);
  PrintFormat("[ACR] DECK player %1: %2", GetPlayerId(), result);
  Rpc(ACR_DeckResultOwner, result);
  // The deck rows and what each side plays changed: send the catalogue's header again.
  m_fACR_DeckAskAt = -100000;
  ACR_DeckAskServer();
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_DeckResultOwner(string result)
 {
  ACR_DeckClient.Result(result);
 }
}
