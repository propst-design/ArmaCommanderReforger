// Map unit markers, marker click handler, and Ctrl-drag box-select handler; split from ACR_PlayerController.c.

// Map markers for soldiers and group leaders. Clicking one requests possession.
class ACR_UnitMarker : SCR_MapMarkerBase
{
 RplId m_UnitId;
 string m_Label;
 string m_BaseCaption; // display only; requests retain the exact label
 string m_BaseHover;
 int m_State;          // 0 AI, 1 player-controlled, 2 dead
 bool m_Original;      // the player's command unit
 bool m_bCommandUnit;  // persistent marker for a fellow commander, not a possession target
 bool m_Selected;
 RplId m_GroupId;      // squad this marker belongs to
 int m_BaseIndex = -1; // >= 0: this marker is a base
 bool m_bContact;      // spotted enemy: clicks go through to the map
 bool m_bZone;         // zone label marker (danger road, fire mission, hideout, etc.)
 bool m_bKeepCaption;  // keep marker text visible without hover (zones, empty vehicles)
 bool m_bHideIcon;     // hide vanilla simple dot (canvas draws a custom glyph instead)
 string m_sZoneRaw;    // unstripped zone label for hover card classification
 int m_iVehicleIndex = -1; // >= 0: empty/abandoned vehicle index
 bool m_bSuspected;    // spotted enemy not seen for a while (ACR_CONTACT_SUSPECTED)
 float m_fAlpha = 1;   // spotted enemy: fades as the sighting ages
 int m_iContactShape = -1; // spotted enemy: icons and dimension it was prepared with
 int m_iPingId = -1;   // >= 0: this marker is a ping's label
 int m_iSymbolKey;     // changes when m_GroupSymbol is set differently
 protected Widget m_wPip; // ground-level reticle pip for spotted contacts
 protected string m_sStyled = "-"; // what RefreshStyle last applied
 ref ACR_MarkerClick m_Click;
 ref SCR_MilitarySymbol m_GroupSymbol;
 override void OnCreateMarker(bool skipProfanityFilter = false)
 {
  // Map-open and local insertion can both create this marker. Remove the old
  // widget before replacing its reference, or its caption stays at the origin.
  if (m_wRoot) m_wRoot.RemoveFromHierarchy();
  m_wRoot = null;
  m_MarkerWidgetComp = null;
  // Always update: units move into view without the map being panned.
  m_bTestVisibleFrame = false;
  m_sStyled = "-";
  m_wPip = null; // it went with the old root
  super.OnCreateMarker(true);
  if (!m_wRoot) return;
  m_Click = new ACR_MarkerClick();
  m_Click.m_Marker = this;
  m_wRoot.AddHandler(m_Click);
  OnUpdate();
  RefreshStyle();
 }
 override void SetWorldPos(int posX, int posY)
 {
  super.SetWorldPos(posX, posY);
  // Position immediately; persistent captions must never wait at (0, 0).
  if (m_wRoot && m_MapEntity) OnUpdate();
 }
 void ACR_SetColor(Color color)
 {
  if (m_MarkerWidgetComp) m_MarkerWidgetComp.SetColor(color);
 }
 // A marker with no click of its own: a click on it means the base whose name or icon is under it
 // (ACR_MarkerClick). A zone's label (a town's "Civilians (n)" ring is centred on the town, so its
 // label covered the town's name: a click on the name did nothing), a spotted enemy or zombie
 // contact, our empty vehicle, a fellow commander's unit, an order's or the 2IC's tag. Bases, pings,
 // our own command unit and squads' and soldiers' markers (ofSquad) keep their clicks.
 // ofSquad comes from the controller's marker lists: m_GroupId can't tell, an unset RplId is valid (0).
 bool PassesClickToBase(bool ofSquad)
 {
  if (m_BaseIndex >= 0 || m_iPingId >= 0 || m_Original) return false;
  return m_bContact || m_bCommandUnit || !ofSquad;
 }
 // The marker's icon, symbol and caption on screen (native px; x, y, w, h each), for a click's hit test.
 void ScreenRects(notnull array<float> rects)
 {
  if (m_MarkerWidgetComp && m_wRoot && m_wRoot.IsVisible()) m_MarkerWidgetComp.ACR_ScreenRects(rects);
 }
 // Called every map frame: the widget is only touched when something changed.
 void RefreshStyle()
 {
  if (!m_MarkerWidgetComp) return;
  string shown = m_Label;
  if (m_BaseIndex >= 0) shown = m_BaseCaption;
  int alphaStep = Math.Round(m_fAlpha * 20);
  bool dark = m_bKeepCaption && ACR_MapDeclutter.Dark();
  string key = string.Format("%1|%2|%3|%4|%5|%6|%7|%8", shown, m_State, m_Original, m_Selected, m_iSymbolKey, m_BaseHover, alphaStep, m_bSuspected);
  key += string.Format("|%1|%2|%3", m_bKeepCaption, m_bHideIcon, dark);
  if (key == m_sStyled) return;
  m_sStyled = key;
  if (m_BaseIndex >= 0 || m_bZone || m_iVehicleIndex >= 0) m_MarkerWidgetComp.SetEventListening(true);
  m_MarkerWidgetComp.SetText(shown, true);
  m_MarkerWidgetComp.SetTextVisible(true);
  ImageWidget icon = m_MarkerWidgetComp.ACR_Icon();
  if (icon && m_bHideIcon) icon.SetOpacity(0);
  else if (icon && m_BaseIndex < 0 && icon.GetOpacity() != 1) icon.SetOpacity(1);
  Color color = new Color(0.1, 0.65, 1, 1);
  if (m_State == 1) color = new Color(1, 0.75, 0.1, 1);
  if (m_State == 2) color = new Color(0.5, 0.5, 0.5, 1);
  if (m_State == 3) color = new Color(1, 0.25, 0.2, 1);   // spotted enemy
  if (m_State == 4) color = new Color(0.65, 0.8, 0.95, 1); // our empty vehicle
  if (m_State == 5) color = new Color(0.92, 0.92, 0.92, 1); // another commander's tag on an order
  if (m_State == 6) color = new Color(1, 1, 1, 1);          // ping label (coloured by kind afterwards)
  if (m_Original) color = new Color(0.2, 1, 0.3, 1);
  if (m_Selected) color = new Color(1, 1, 1, 1);
  if (m_fAlpha < 1) color = new Color(color.R(), color.G(), color.B(), color.A() * m_fAlpha);
  m_MarkerWidgetComp.SetColor(color);
  if (m_GroupSymbol) m_MarkerWidgetComp.UpdateMilitarySymbol(m_GroupSymbol);
  // A spotted enemy's reticle; not our empty vehicles (state 4), order tags (state 5), or zones,
  // which use m_bContact for click-through
  if (m_bContact && !m_bZone && m_State != 4 && m_State != 5 && m_wRoot && !m_wPip)
  {
   WorkspaceWidget ws = GetGame().GetWorkspace();
   if (ws)
   {
    m_wPip = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, new Color(1, 0.22, 0.18, 0.9), 99, m_wRoot);
    FrameSlot.SetAnchorMin(m_wPip, 0.5, 0.5);
    FrameSlot.SetAnchorMax(m_wPip, 0.5, 0.5);
    FrameSlot.SetAlignment(m_wPip, 0.5, 0.5);
    FrameSlot.SetSize(m_wPip, 6, 6);
   }
  }
  if (m_wPip)
  {
   float pipAlpha = 0.95 * m_fAlpha;
   if (m_bSuspected) pipAlpha = Math.Min(pipAlpha, 0.55);
   m_wPip.SetColor(new Color(1, 0.22, 0.18, pipAlpha));
  }
 }
 // Left-click: select this marker's squad (Arma Commander style). The command
 // unit marker instead returns you to your command unit.
 void Select()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc) return;
  pc.ACR_MarkUiClick();
  if (m_iPingId >= 0)
  {
   pc.ACR_ClickPing(m_iPingId);
   return;
  }
  // Deploying: a click on a squad standing on a base counts as a click on the base.
  if (m_BaseIndex < 0 && pc.ACR_DeployPending())
  {
   int pos[2];
   GetWorldPos(pos);
   pc.ACR_ClickBaseAt(pos[0], pos[1]);
   return;
  }
  // Deploying on a base marker: the unit appears where the cursor is.
  if (m_BaseIndex >= 0 && pc.ACR_DeployPending())
  {
   SCR_MapEntity markerMap = SCR_MapEntity.GetMapInstance();
   float cx, cz;
   if (markerMap)
   {
    markerMap.GetMapCursorWorldPosition(cx, cz);
    if (pc.m_ACR_MusterMode) pc.ACR_ClickBaseAt(cx, cz);
    else pc.ACR_ClickBase(m_Label, Vector(cx, 0, cz), true);
    return;
   }
  }
  if (m_Original) pc.ACR_Request(ACR_ERequest.RETURN);
  else if (m_BaseIndex >= 0) pc.ACR_ClickBase(m_Label);
  else pc.ACR_PickGroup(this);
 }
}
class ACR_MarkerClick : ScriptedWidgetEventHandler
{
 ACR_UnitMarker m_Marker;
 override bool OnMouseButtonDown(Widget w, int x, int y, int button)
 {
  if (button != 0 || !m_Marker) return false;
  // Markers drawn over a base's name (zone labels, zombie contacts) let the click reach the base.
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc && pc.ACR_PassesClickToBase(m_Marker) && pc.ACR_ClickBaseUnderCursor()) return true;
  if (m_Marker.m_bContact) return false;
  m_Marker.Select();
  return true;
 }
}

// Ctrl + drag on the command map (see ACR_BoxDown / ACR_BoxUp).
class ACR_BoxHandler : ScriptedWidgetEventHandler
{
 SCR_PlayerController m_Owner;
 override bool OnMouseButtonDown(Widget w, int x, int y, int button)
 {
  if (button != 0 || !m_Owner) return false;
  m_Owner.ACR_BoxDown(x, y);
  return true;
 }
 override bool OnMouseButtonUp(Widget w, int x, int y, int button)
 {
  if (button != 0 || !m_Owner) return false;
  m_Owner.ACR_BoxUp(x, y);
  return true;
 }
}

