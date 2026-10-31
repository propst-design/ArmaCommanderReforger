// The one way to move a panel on the command map. A panel registers its root (Add) with where it
// stands by itself (its natural spot, which may follow something: Natural) and how it is held: its
// title bar (TitleBar), or its own face for one that has no bar to spare (Grip, Body). A drag keeps
// the spot in the player's prefs (ACR_UiPrefs), a double click forgets it, and each frame (Tick) every
// panel goes where it should stand, kept on the screen. A press on a button, a text box or a
// scrolling list is theirs, never a drag.
//
// The game's hints: on the map the hint box sits in the map menu's own HUD layout (MapMenu.layout,
// HudManagerLayout > Group_Right > Slot_Hints, top right, z 0), a sibling of these panels (z 1000), so a
// panel over it hid it. While a hint shows its layout is lifted over them all, and its column steps left of
// any panel in the way (ClearHint). The panels stay put: one that moved for a hint landed on the next panel
// (pn25 playtest).

// Where the game's hint box is while it shows.
modded class SCR_HintUIComponent
{
 protected static ref array<SCR_HintUIComponent> s_aACR_Hints = {};

 override void HandlerAttached(Widget w)
 {
  super.HandlerAttached(w);
  if (!s_aACR_Hints.Contains(this)) s_aACR_Hints.Insert(this);
 }

 override void HandlerDeattached(Widget w)
 {
  s_aACR_Hints.RemoveItem(this);
  super.HandlerDeattached(w);
 }

 // A hint box on screen (shown, not faded out), null none; its rectangle (px).
 static Widget ACR_Shown(out float x, out float y, out float w, out float h)
 {
  foreach (SCR_HintUIComponent hint : s_aACR_Hints)
  {
   if (!hint || !hint.m_Widget || !hint.m_Widget.IsVisibleInHierarchy() || hint.m_Widget.GetOpacity() < 0.2) continue;
   hint.m_Widget.GetScreenPos(x, y);
   hint.m_Widget.GetScreenSize(w, h);
   if (w > 0 && h > 0) return hint.m_Widget;
  }
  return null;
 }
}

class ACR_PanelDragHandler : ScriptedWidgetEventHandler
{
 string m_sId;
 ImageWidget m_Lit; // a title bar's fill: lit under the cursor, so the player sees it can be held

 override bool OnMouseButtonDown(Widget w, int x, int y, int button)
 {
  if (button != 0) return false;
  return ACR_PanelMover.DragStart(m_sId);
 }

 override bool OnMouseButtonUp(Widget w, int x, int y, int button)
 {
  if (button == 0) ACR_PanelMover.DragEnd();
  return false;
 }

 override bool OnDoubleClick(Widget w, int x, int y, int button)
 {
  if (button != 0 || ACR_PanelMover.OverControl()) return false;
  ACR_PanelMover.PlaceHome(m_sId);
  return true;
 }

 override bool OnMouseEnter(Widget w, int x, int y)
 {
  if (m_Lit) m_Lit.SetColor(ACR_PanelMover.BarColor(true));
  return false;
 }

 override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
 {
  if (m_Lit) m_Lit.SetColor(ACR_PanelMover.BarColor(false));
  return false;
 }
}

class ACR_PanelSlot : Managed
{
 string m_sId;
 ref array<Widget> m_aBoxes = {};      // the root first, then what stands where it does (the folded bar)
 float m_fAnchorX;                     // 0 left, 0.5 middle, 1 right: the root's anchor and alignment
 float m_fAnchorY;
 float m_fNaturalX;                    // in the anchor's terms
 float m_fNaturalY;
 float m_fShownX;
 float m_fShownY;
 SizeLayoutWidget m_TitleBar;
 float m_fTitleBarHeight;
 ref array<ref ACR_PanelDragHandler> m_aHandlers = {};

 // What shows of it now (null: nothing).
 Widget Box()
 {
  foreach (Widget box : m_aBoxes)
  {
   if (box && box.IsVisible()) return box;
  }
  return null;
 }

 void SetPos(float x, float y)
 {
  m_fShownX = x;
  m_fShownY = y;
  foreach (Widget box : m_aBoxes)
  {
   if (box) FrameSlot.SetPos(box, x, y);
  }
 }
}

class ACR_PanelMover
{
 protected static ref array<ref ACR_PanelSlot> s_aSlots = {};
 protected static ACR_PanelSlot s_Drag;
 protected static bool s_bDragMoved;
 protected static int s_iDragFromX;
 protected static int s_iDragFromY;
 protected static float s_fDragStartX;
 protected static float s_fDragStartY;
 protected static const float DRAG_START = 4; // reference pixels before a press counts as a drag
 protected static const float HANDLE_WIDTH = 44;
 protected static const float HANDLE_HEIGHT = 3;
 protected static const int HINT_Z = 1200;    // over every panel of ours, the 2IC's (1100) too
 protected static bool s_bHint;               // a hint shows: its rectangle (unscaled px)
 protected static float s_fHintX;
 protected static float s_fHintY;
 protected static float s_fHintW;
 protected static float s_fHintH;
 protected static Widget s_HintLayer;         // what is lifted for it, and its own z order
 protected static int s_iHintLayerZ;
 protected static const float HINT_COLUMN_LEFT = 1320; // Group_Right's own left offset in the layout
 protected static Widget s_HintColumn;        // Group_Right, and how far left of its place it stands
 protected static float s_fHintShift;

 static Color BarColor(bool lit)
 {
  if (lit) return new Color(1, 1, 1, 0.18);
  return new Color(1, 1, 1, 0.05);
 }

 // ---------------------------------------------------------------- what a panel registers
 protected static ACR_PanelSlot Slot(string id)
 {
  foreach (ACR_PanelSlot slot : s_aSlots)
  {
   if (slot.m_sId == id) return slot;
  }
  return null;
 }

 // A panel rebuilt (the UI size's buttons) takes its old entry's place.
 static void Add(string id, Widget root, float anchorX, float anchorY, float naturalX, float naturalY)
 {
  if (!root) return;
  ACR_UiPrefs.Load();
  Remove(id);
  ACR_PanelSlot slot = new ACR_PanelSlot();
  slot.m_sId = id;
  slot.m_aBoxes.Insert(root);
  slot.m_fAnchorX = anchorX;
  slot.m_fAnchorY = anchorY;
  slot.m_fNaturalX = naturalX;
  slot.m_fNaturalY = naturalY;
  s_aSlots.Insert(slot);
  Place(slot);
 }

 // Something else that stands where the panel stands (only one of them shows at a time).
 static void AddBox(string id, Widget box)
 {
  ACR_PanelSlot slot = Slot(id);
  if (slot && box) slot.m_aBoxes.Insert(box);
 }

 static void Remove(string id)
 {
  ACR_PanelSlot slot = Slot(id);
  if (!slot) return;
  if (s_Drag == slot) s_Drag = null;
  s_aSlots.RemoveItem(slot);
  if (s_aSlots.IsEmpty())
  {
   LiftHint(null);
   ShiftHintColumn(0);
  }
 }

 // Where it stands by itself: followed each frame if it follows something.
 static void Natural(string id, float x, float y)
 {
  ACR_PanelSlot slot = Slot(id);
  if (!slot) return;
  slot.m_fNaturalX = x;
  slot.m_fNaturalY = y;
 }

 protected static ACR_PanelDragHandler NewHandler(ACR_PanelSlot slot)
 {
  ACR_PanelDragHandler handler = new ACR_PanelDragHandler();
  handler.m_sId = slot.m_sId;
  slot.m_aHandlers.Insert(handler);
  return handler;
 }

 // A press on this widget, or on what it holds that takes no press itself, drags the panel.
 static void Grip(string id, Widget widget)
 {
  ACR_PanelSlot slot = Slot(id);
  if (slot && widget) widget.AddHandler(NewHandler(slot));
 }

 // A panel held by its whole face, in an overlay: Grip, a clear layer under its content for where the
 // content lets the cursor through (a PanelBox's fill ignores it), and the handle.
 static void Body(string id, Widget overlay)
 {
  if (!overlay) return;
  Grip(id, overlay);
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget clear = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, Color.FromRGBA(0, 0, 0, 3), 0, overlay);
  if (clear)
  {
   AlignableSlot.SetHorizontalAlign(clear, LayoutHorizontalAlign.Stretch);
   AlignableSlot.SetVerticalAlign(clear, LayoutVerticalAlign.Stretch);
  }
  Handle(overlay);
 }

 // A title bar: a lit band along the top of an overlay, over what is there (height 0: all of it), so
 // nothing it holds may be a button: rightMargin leaves room for them. A press anywhere on it drags.
 static void TitleBar(string id, Widget overlay, float height, float rightMargin = 0)
 {
  ACR_PanelSlot slot = Slot(id);
  if (!slot || !overlay) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  SizeLayoutWidget bar = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 5, overlay));
  if (!bar) return;
  AlignableSlot.SetHorizontalAlign(bar, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(bar, LayoutVerticalAlign.Stretch);
  if (height > 0)
  {
   bar.EnableHeightOverride(true);
   bar.SetHeightOverride(height);
   AlignableSlot.SetVerticalAlign(bar, LayoutVerticalAlign.Top);
  }
  AlignableSlot.SetPadding(bar, 0, 0, rightMargin, 0);
  ImageWidget fill = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, BarColor(false), 0, bar));
  if (!fill) return;
  AlignableSlot.SetHorizontalAlign(fill, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(fill, LayoutVerticalAlign.Stretch);
  ACR_PanelDragHandler handler = NewHandler(slot);
  handler.m_Lit = fill;
  fill.AddHandler(handler);
  if (!slot.m_TitleBar) slot.m_TitleBar = bar;
  Handle(overlay);
 }

 // The handle: a short bar at the top middle, the sign that this panel can be held.
 static void Handle(Widget overlay)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  SizeLayoutWidget handle = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 6, overlay));
  if (!handle) return;
  handle.EnableWidthOverride(true);
  handle.SetWidthOverride(HANDLE_WIDTH);
  handle.EnableHeightOverride(true);
  handle.SetHeightOverride(HANDLE_HEIGHT);
  AlignableSlot.SetHorizontalAlign(handle, LayoutHorizontalAlign.Center);
  AlignableSlot.SetVerticalAlign(handle, LayoutVerticalAlign.Top);
  AlignableSlot.SetPadding(handle, 0, 4, 0, 0);
  Widget mark = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, new Color(1, 1, 1, 0.5), 0, handle);
  if (!mark) return;
  AlignableSlot.SetHorizontalAlign(mark, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(mark, LayoutVerticalAlign.Stretch);
 }

 // The title bar reaches down to this widget, the first below it (a font size changes where that is,
 // and INTEL's bar must stop short of its first line: a click there jumps the map).
 static void FitTitleBar(string id, Widget below)
 {
  ACR_PanelSlot slot = Slot(id);
  if (!slot || !slot.m_TitleBar || !below || slot.m_aBoxes.IsEmpty() || slot.m_aBoxes[0] != slot.Box()) return;
  float rootX, rootY, belowX, belowY;
  slot.m_aBoxes[0].GetScreenPos(rootX, rootY);
  below.GetScreenPos(belowX, belowY);
  float height = GetGame().GetWorkspace().DPIUnscale(belowY - rootY);
  if (height < 16 || height == slot.m_fTitleBarHeight) return;
  slot.m_fTitleBarHeight = height;
  slot.m_TitleBar.SetHeightOverride(height);
 }

 // ---------------------------------------------------------------- the drag
 // A button, a text box or a scrolling list under the cursor.
 static bool OverControl()
 {
  Widget widget = WidgetManager.GetWidgetUnderCursor();
  while (widget)
  {
   if (ButtonWidget.Cast(widget) || EditBoxWidget.Cast(widget) || ScrollLayoutWidget.Cast(widget)) return true;
   widget = widget.GetParent();
  }
  return false;
 }

 static bool DragStart(string id)
 {
  ACR_PanelSlot slot = Slot(id);
  if (!slot || OverControl()) return false;
  WidgetManager.GetMousePos(s_iDragFromX, s_iDragFromY);
  s_Drag = slot;
  s_bDragMoved = false;
  // From where it shows, which may not be its saved spot.
  s_fDragStartX = slot.m_fShownX;
  s_fDragStartY = slot.m_fShownY;
  return true;
 }

 protected static void TickDrag()
 {
  if (!s_Drag) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  int x, y;
  WidgetManager.GetMousePos(x, y);
  float dx = ws.DPIUnscale(x - s_iDragFromX);
  float dy = ws.DPIUnscale(y - s_iDragFromY);
  if (!s_bDragMoved && Math.AbsFloat(dx) + Math.AbsFloat(dy) < DRAG_START) return;
  s_bDragMoved = true;
  float width, height;
  Widget box = s_Drag.Box();
  if (box) box.GetScreenSize(width, height);
  width = ws.DPIUnscale(width);
  height = ws.DPIUnscale(height);
  float screenW = ws.DPIUnscale(ws.GetWidth());
  float screenH = ws.DPIUnscale(ws.GetHeight());
  ACR_UiPrefs.SetSpot(s_Drag.m_sId, ACR_UiPrefs.ClampX(s_fDragStartX + dx, s_Drag.m_fAnchorX, screenW, width), ACR_UiPrefs.ClampY(s_fDragStartY + dy, s_Drag.m_fAnchorY, screenH, height));
 }

 static void DragEnd()
 {
  if (!s_Drag) return;
  ACR_PanelSlot slot = s_Drag;
  s_Drag = null;
  if (!s_bDragMoved) return;
  // Where it was seen to stop (a tall panel stops above its dragged spot).
  ACR_UiPrefs.SetSpot(slot.m_sId, slot.m_fShownX, slot.m_fShownY);
  ACR_UiPrefs.Save();
 }

 static void PlaceHome(string id)
 {
  ACR_PanelSlot slot = Slot(id);
  if (s_Drag == slot) s_Drag = null;
  ACR_UiPrefs.Forget(id);
  ACR_UiPrefs.Save();
  if (slot) Place(slot);
 }

 // ---------------------------------------------------------------- each frame
 protected static void Place(ACR_PanelSlot slot)
 {
  float x, y;
  if (!ACR_UiPrefs.Spot(slot.m_sId, x, y))
  {
   x = slot.m_fNaturalX;
   y = slot.m_fNaturalY;
  }
  Widget box = slot.Box();
  if (box)
  {
   WorkspaceWidget ws = GetGame().GetWorkspace();
   float width, height;
   box.GetScreenSize(width, height);
   width = ws.DPIUnscale(width);
   height = ws.DPIUnscale(height);
   // Whole on this screen, not written back: it returns to its spot on the screen it was put on,
   // and a panel grown past the bottom (a squad selected) stands higher only while it is that tall.
   float screenW = ws.DPIUnscale(ws.GetWidth());
   float screenH = ws.DPIUnscale(ws.GetHeight());
   x = ACR_UiPrefs.ClampX(x, slot.m_fAnchorX, screenW, width);
   y = ACR_UiPrefs.FitY(y, slot.m_fAnchorY, screenH, height);
  }
  slot.SetPos(x, y);
 }

 // The hint's layout over the panels while one shows, as it was after. Only the layout's frames are
 // told to ignore the cursor, on their way up from the hint, which keeps its own.
 protected static void LiftHint(Widget hint)
 {
  Widget layer = null;
  Widget menu = null;
  if (hint && !s_aSlots.IsEmpty() && !s_aSlots[0].m_aBoxes.IsEmpty() && s_aSlots[0].m_aBoxes[0]) menu = s_aSlots[0].m_aBoxes[0].GetParent();
  if (menu)
  {
   layer = hint;
   while (layer && layer.GetParent() != menu) layer = layer.GetParent();
  }
  if (layer == s_HintLayer) return;
  if (s_HintLayer) s_HintLayer.SetZOrder(s_iHintLayerZ);
  s_HintLayer = layer;
  if (!layer) return;
  s_iHintLayerZ = layer.GetZOrder();
  layer.SetZOrder(HINT_Z);
  Widget frame = hint.GetParent();
  while (frame && frame != menu)
  {
   frame.SetFlags(WidgetFlags.IGNORE_CURSOR);
   frame = frame.GetParent();
  }
 }

 // The hint's column stands left of whatever panel is in the way, worked out from the screen's edge each
 // frame, so it comes back when the hint goes. A panel being dragged doesn't count until it's put down.
 protected static void ClearHint(Widget hint)
 {
  Widget column = hint;
  while (column && column.GetName() != "Group_Right") column = column.GetParent();
  if (!column)
  {
   ShiftHintColumn(0);
   return;
  }
  // A map opened again has a new column, in its own place.
  if (column != s_HintColumn) s_fHintShift = 0;
  s_HintColumn = column;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float columnX, columnY, columnW, columnH;
  column.GetScreenPos(columnX, columnY);
  column.GetScreenSize(columnW, columnH);
  // What the box keeps from the column's right edge: both read now, so the layout's lag behind a shift doesn't count.
  float margin = ws.DPIUnscale(columnX + columnW) - (s_fHintX + s_fHintW);
  float home = ws.DPIUnscale(ws.GetWidth()) - margin - s_fHintW;
  float left = home;
  // Until it stops moving: stepping past one panel can land on another.
  bool moved = true;
  for (int pass = 0; moved && pass <= s_aSlots.Count(); pass++)
  {
   moved = false;
   foreach (ACR_PanelSlot slot : s_aSlots)
   {
    Widget box = slot.Box();
    if (!box || slot == s_Drag) continue;
    float x, y, w, h;
    box.GetScreenPos(x, y);
    box.GetScreenSize(w, h);
    x = ws.DPIUnscale(x);
    y = ws.DPIUnscale(y);
    w = ws.DPIUnscale(w);
    h = ws.DPIUnscale(h);
    if (w <= 0 || h <= 0 || !ACR_UiPrefs.Overlap(left, s_fHintY, s_fHintW, s_fHintH, x, y, w, h)) continue;
    left = x - ACR_UiPrefs.STEP_GAP - s_fHintW;
    moved = true;
   }
  }
  ShiftHintColumn(Math.Max(home - Math.Max(left, 0), 0));
 }

 protected static void ShiftHintColumn(float shift)
 {
  if (!s_HintColumn || Math.AbsFloat(shift - s_fHintShift) < 1) return;
  s_fHintShift = shift;
  FrameSlot.SetOffsets(s_HintColumn, HINT_COLUMN_LEFT - shift, 0, shift, 0);
 }

 // A hint's rectangle while one shows (unscaled px).
 static bool HintRect(out float x, out float y, out float w, out float h)
 {
  x = s_fHintX;
  y = s_fHintY;
  w = s_fHintW;
  h = s_fHintH;
  return s_bHint;
 }

 // The game's chat and radio lines: Slot_Chat in the map menu's own HUD layout (Group_Left, top left, 600 wide),
 // under these panels, so INTEL at home hid them (pn23 playtest). The group stands beside whatever panel is
 // in the way. Worked out from the screen's edge each frame, so it comes back when a panel moves off.
 protected static const float CHAT_WIDTH = 480;  // what its lines take (the longest seen, 452); its slot is 600 wide
 protected static const float CHAT_RIGHT = 1320; // the group's own right offset in the layout
 protected static const float CHAT_GAP = 12;
 protected static const float CHAT_TOOLBAR = 84; // the map's own tool buttons, on the left below the top row
 protected static const float CHAT_HEIGHT = 170; // what five lines take: they run on past their slot, which may measure less
 protected static float s_fChatLeft;
 protected static float s_fChatTop;

 protected static Widget ChatSlot()
 {
  if (s_aSlots.IsEmpty() || s_aSlots[0].m_aBoxes.IsEmpty() || !s_aSlots[0].m_aBoxes[0]) return null;
  Widget menu = s_aSlots[0].m_aBoxes[0].GetParent();
  if (!menu) return null;
  return menu.FindAnyWidget("Slot_Chat");
 }

 protected static void ClearChat()
 {
  Widget chat = ChatSlot();
  if (!chat || !chat.GetParent()) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float chatW, chatH;
  chat.GetScreenSize(chatW, chatH);
  float height = Math.Max(ws.DPIUnscale(chatH), CHAT_HEIGHT);
  float screenW = ws.DPIUnscale(ws.GetWidth());
  float screenH = ws.DPIUnscale(ws.GetHeight());
  // Along the top if there's room past the panels; if they fill the row, under the first panels that leave
  // room (pn25 playtest: four panels across the top left the lines under the command panel).
  array<float> tops = {0};
  foreach (ACR_PanelSlot under : s_aSlots)
  {
   Widget box = under.Box();
   if (!box) continue;
   float boxX, boxY, boxW, boxH;
   box.GetScreenPos(boxX, boxY);
   box.GetScreenSize(boxW, boxH);
   if (boxW > 0 && boxH > 0) tops.Insert(ws.DPIUnscale(boxY + boxH) + CHAT_GAP);
  }
  tops.Sort();
  float left = screenW - CHAT_WIDTH;
  float top = 0;
  foreach (float tried : tops)
  {
   if (tried + height > screenH) continue;
   float from = 0;
   if (tried > 0) from = CHAT_TOOLBAR;
   float fits = ChatFits(from, tried, height);
   if (fits + CHAT_WIDTH > screenW) continue;
   left = fits;
   top = tried;
   break;
  }
  if (Math.AbsFloat(left - s_fChatLeft) < 1 && Math.AbsFloat(top - s_fChatTop) < 1) return;
  s_fChatLeft = left;
  s_fChatTop = top;
  FrameSlot.SetOffsets(chat.GetParent(), left, top, CHAT_RIGHT - left, 0);
 }

 // The lines' left edge at this height: past every panel in their way.
 protected static float ChatFits(float left, float top, float height)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  // Until it stops moving: stepping past one panel can land on another.
  bool moved = true;
  for (int pass = 0; moved && pass <= s_aSlots.Count(); pass++)
  {
   moved = false;
   foreach (ACR_PanelSlot slot : s_aSlots)
   {
    Widget box = slot.Box();
    if (!box) continue;
    float x, y, w, h;
    box.GetScreenPos(x, y);
    box.GetScreenSize(w, h);
    x = ws.DPIUnscale(x);
    y = ws.DPIUnscale(y);
    w = ws.DPIUnscale(w);
    h = ws.DPIUnscale(h);
    if (w <= 0 || h <= 0 || !ACR_UiPrefs.Overlap(left, top, CHAT_WIDTH, height, x, y, w, h)) continue;
    left = x + w + CHAT_GAP;
    moved = true;
   }
  }
  return left;
 }

 // Every frame the map is open (ACR_CommandPanel.TickDrag).
 static void Tick()
 {
  TickDrag();
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float x, y, w, h;
  Widget hint = SCR_HintUIComponent.ACR_Shown(x, y, w, h);
  s_bHint = hint != null;
  s_fHintX = ws.DPIUnscale(x);
  s_fHintY = ws.DPIUnscale(y);
  s_fHintW = ws.DPIUnscale(w);
  s_fHintH = ws.DPIUnscale(h);
  LiftHint(hint);
  foreach (ACR_PanelSlot slot : s_aSlots) Place(slot);
  ClearHint(hint);
  ClearChat();
 }

 // Where it shows now, in its anchor's terms.
 static float ShownX(string id)
 {
  ACR_PanelSlot slot = Slot(id);
  if (slot) return slot.m_fShownX;
  return 0;
 }
 static float ShownY(string id)
 {
  ACR_PanelSlot slot = Slot(id);
  if (slot) return slot.m_fShownY;
  return 0;
 }

 // ---------------------------------------------------------------- tests
 static bool ShowsForTest(string id)
 {
  ACR_PanelSlot slot = Slot(id);
  return slot && slot.Box();
 }

 // What to press (screen pixels, x and y): the middle of the title bar, or a corner of the face.
 static vector GripForTest(string id)
 {
  ACR_PanelSlot slot = Slot(id);
  if (!slot || !slot.Box()) return vector.Zero;
  float x, y, width, height;
  Widget box = slot.Box();
  if (slot.m_TitleBar && box == slot.m_aBoxes[0])
  {
   slot.m_TitleBar.GetScreenPos(x, y);
   slot.m_TitleBar.GetScreenSize(width, height);
   return Vector(x + width * 0.5, y + height * 0.5, 0);
  }
  box.GetScreenPos(x, y);
  float inset = GetGame().GetWorkspace().DPIScale(6);
  return Vector(x + inset, y + inset, 0);
 }

 // Nothing of it is under the hint (it doesn't show, or no hint does, counts as clear).
 static bool ClearOfHintForTest(string id)
 {
  ACR_PanelSlot slot = Slot(id);
  if (!slot || !slot.Box() || !s_bHint) return true;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float x, y, w, h;
  slot.Box().GetScreenPos(x, y);
  slot.Box().GetScreenSize(w, h);
  return !ACR_UiPrefs.Overlap(ws.DPIUnscale(x), ws.DPIUnscale(y), ws.DPIUnscale(w), ws.DPIUnscale(h), s_fHintX, s_fHintY, s_fHintW, s_fHintH);
 }

 static bool HintLiftedForTest() { return s_HintLayer && s_HintLayer.GetZOrder() == HINT_Z; }
 static float HintShiftForTest() { return s_fHintShift; }

 // The chat lines' column clear of every panel: where it stands, -1 not clear, -2 no chat slot.
 static float ChatLeftForTest()
 {
  Widget chat = ChatSlot();
  if (!chat) return -2;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float chatX, top, chatW, chatH;
  chat.GetScreenPos(chatX, top);
  chat.GetScreenSize(chatW, chatH);
  float left = ws.DPIUnscale(chatX);
  foreach (ACR_PanelSlot slot : s_aSlots)
  {
   Widget box = slot.Box();
   if (!box) continue;
   float x, y, w, h;
   box.GetScreenPos(x, y);
   box.GetScreenSize(w, h);
   if (w > 0 && h > 0 && ACR_UiPrefs.Overlap(left, ws.DPIUnscale(top), CHAT_WIDTH, Math.Max(ws.DPIUnscale(chatH), CHAT_HEIGHT), ws.DPIUnscale(x), ws.DPIUnscale(y), ws.DPIUnscale(w), ws.DPIUnscale(h))) return -1;
  }
  return left;
 }

 // Where the hint box hangs, for the log: its parents' names up to the top.
 static string HintPathForTest()
 {
  float x, y, w, h;
  Widget widget = SCR_HintUIComponent.ACR_Shown(x, y, w, h);
  string path = string.Format("%1,%2 %3x%4:", s_fHintX, s_fHintY, s_fHintW, s_fHintH);
  while (widget)
  {
   path += " " + widget.GetName();
   widget = widget.GetParent();
  }
  return path;
 }

 // Its box's top left corner on the screen, in reference pixels.
 static vector TopLeftForTest(string id)
 {
  ACR_PanelSlot slot = Slot(id);
  if (!slot || !slot.Box()) return vector.Zero;
  float x, y;
  slot.Box().GetScreenPos(x, y);
  WorkspaceWidget ws = GetGame().GetWorkspace();
  return Vector(ws.DPIUnscale(x), ws.DPIUnscale(y), 0);
 }
}
