// Vanilla parks off-screen static markers in a "disabled" list and only revives them
// on pan end. A marker removed while parked stayed in that list and came back as a
// widget nobody updates (the "stuck marker"). ACR markers are never parked, and are
// removed from both lists.
modded class SCR_MapMarkerManagerComponent
{
 void ACR_RemoveMarker(SCR_MapMarkerBase marker)
 {
  if (!marker) return;
  marker.OnDelete();
  m_aStaticMarkers.RemoveItem(marker);
  m_aDisabledMarkers.RemoveItem(marker);
 }
}

// A marker is a draggable map element (vanilla MapMarkerBase.layout), and ACA rebuilds its unit,
// group, vehicle, contact and order markers as they change. One removed while the player holds the
// mouse on it left vanilla dragging a deleted widget: a NULL s_DraggedWidget exception every frame
// until the button was let go (pn11, 27 Oct: a purchase, then a new contact).
modded class SCR_MapToolInteractionUI
{
 override void Update(float timeSlice)
 {
  if (s_bIsDragging && !s_DraggedWidget) s_bIsDragging = false;
  super.Update(timeSlice);
 }
}

// Where a click on the map fell among markers' rectangles (x, y, w, h per 4 floats, native px).
class ACR_MapMarkerHit
{
 // The first rectangle holding x, y (its index, not the float's); -1 none.
 static int RectAt(notnull array<float> rects, float x, float y)
 {
  for (int r = 0; r + 3 < rects.Count(); r += 4)
  {
   if (x >= rects[r] && x <= rects[r] + rects[r + 2] && y >= rects[r + 1] && y <= rects[r + 1] + rects[r + 3]) return r / 4;
  }
  return -1;
 }
}

// Vanilla's SetTextVisible(true) enables hover text, then immediately hides the widget.
// Keep compact base captions visible and expand them to full details on hover.
// Other marker types retain their normal hover behaviour.
modded class SCR_MapMarkerWidgetComponent
{
 protected bool m_ACR_BaseHovered;

 // The icon, military symbol and caption as drawn (ACR_UnitMarker.ScreenRects).
 void ACR_ScreenRects(notnull array<float> rects)
 {
  ACR_AddRect(m_wMarkerIcon, rects);
  ACR_AddRect(m_wSymbolRoot, rects);
  ACR_AddRect(m_wMarkerText, rects);
 }

 protected void ACR_AddRect(Widget w, notnull array<float> rects)
 {
  if (!w || !w.IsVisible()) return;
  float x, y, width, height;
  w.GetScreenPos(x, y);
  w.GetScreenSize(width, height);
  if (width <= 0 || height <= 0) return;
  rects.Insert(x);
  rects.Insert(y);
  rects.Insert(width);
  rects.Insert(height);
 }
 protected void ACR_KeepBaseCaptionVisible()
 {
  ACR_UnitMarker marker = ACR_UnitMarker.Cast(m_MarkerObject);
  if (!marker || marker.m_BaseIndex < 0 || !m_wMarkerText) return;
  string text = marker.m_BaseCaption;
  // Not while the list of a "×N"'s squads is open: it would show behind the list (ACR_SquadPicker).
  // In a match the hover card shows the details (ACR_MapCard); the setup map keeps them here.
  if (m_ACR_BaseHovered && !ACR_SquadPicker.s_bOpen && ACR_SetupClient.IsActive()) text = marker.m_BaseHover;
  m_wMarkerText.SetText(text);
  m_wMarkerText.SetVisible(m_bShowText);
 }

 // A squad's or a base's marker under the cursor: its hover card (ACR_PlayerMapHover).
 protected void ACR_TellHover(bool on)
 {
  ACR_UnitMarker marker = ACR_UnitMarker.Cast(m_MarkerObject);
  if (!marker || marker.m_bContact || marker.m_iPingId >= 0 || marker.m_Original) return;
  if (marker.m_BaseIndex < 0 && !marker.m_GroupId.IsValid()) return;
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc) pc.ACR_HoverMarker(marker, on);
 }

 override bool OnMouseEnter(Widget w, int x, int y)
 {
  bool handled = super.OnMouseEnter(w, x, y);
  if (m_bIsEventListening) m_ACR_BaseHovered = true;
  ACR_KeepBaseCaptionVisible();
  ACR_TellHover(true);
  return handled;
 }

 override void SetTextVisible(bool state)
 {
  super.SetTextVisible(state);
  ACR_KeepBaseCaptionVisible();
 }

 override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
 {
  bool handled = super.OnMouseLeave(w, enterW, x, y);
  m_ACR_BaseHovered = false;
  ACR_KeepBaseCaptionVisible();
  ACR_TellHover(false);
  return handled;
 }
}
