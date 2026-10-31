// Widgets we put straight on the workspace (HUD ping icons and labels, the
// capture bar, the ping-key hint) outlive the mission: the workspace stays
// when the world unloads, so they stayed on screen in the main menu. Every such
// widget is registered here and removed when the mode stops (ACR_Manager.Stop).
class ACR_Hud
{
 protected static ref array<Widget> s_Widgets = {};
 protected static Widget s_Root;

 // A fullscreen frame gives positioned HUD children frame slots and keeps
 // mission cleanup in one owner.
 static Widget Root()
 {
  if (s_Root) return s_Root;
  s_Root = CreateLayer(800);
  return s_Root;
 }

 // Separate fullscreen layers preserve top-level stacking for interactive
 // cameras and death fades instead of trapping them under the ordinary HUD.
 static Widget CreateLayer(int order, bool interactive = false)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  if (!ws) return null;
  int flags = WidgetFlags.VISIBLE;
  if (!interactive) flags = flags | WidgetFlags.IGNORE_CURSOR;
  Widget layer = Track(ws.CreateWidget(WidgetType.FrameWidgetTypeID, flags, Color.FromInt(Color.WHITE), 0, ws));
  if (layer)
  {
   // Workspace-created frame roots have FrameWidgetSlot (verified in-client).
   // AlignableSlot calls leave the default 100x30 reference-pixel rectangle.
   FrameSlot.SetAnchorMin(layer, 0, 0);
   FrameSlot.SetAnchorMax(layer, 1, 1);
   FrameSlot.SetOffsets(layer, 0, 0, 0, 0);
   layer.SetZOrder(order);
  }
  return layer;
 }

 static void Remove(Widget widget)
 {
  if (!widget) return;
  s_Widgets.RemoveItem(widget);
  widget.RemoveFromHierarchy();
 }

 static int s_iCleared; // for the tests

 static Widget Track(Widget widget)
 {
  if (widget && !s_Widgets.Contains(widget)) s_Widgets.Insert(widget);
  return widget;
 }

 static void Clear()
 {
  foreach (Widget widget : s_Widgets)
  {
   if (!widget) continue;
   widget.RemoveFromHierarchy();
   s_iCleared++;
  }
  s_Widgets.Clear();
  s_Root = null;
  ACR_PingDisplay.Forget();
  ACR_KeyHints.Forget();
 }
}
