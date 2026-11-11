// The calm map's Squads list (ACR_SquadsList) and layer switches (ACR_MapLayers): built when the
// map opens, kept up to date while it is open, gone when it closes. Neither shows on the setup
// map or with the calm map off (-acrMapDeclutter 0).
// (Named to load after ACR_PlayerMapDeclutter.c, ACR_PlayerMapGroups.c and ACR_PlayerMapHover.c,
// whose methods it uses.)
modded class ACR_CommandPanel
{
 // The bottom of what is docked top left (INTEL, or its bar when folded), unscaled px; 0 none.
 float ACR_LeftBottom()
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float bottom = 0;
  array<Widget> boxes = {m_FeedRoot, m_DeclIntelBar};
  foreach (Widget box : boxes)
  {
   if (!box || !box.IsVisible()) continue;
   float x, y, w, h;
   box.GetScreenPos(x, y);
   box.GetScreenSize(w, h);
   bottom = Math.Max(bottom, ws.DPIUnscale(y + h));
  }
  return bottom;
 }

 // The left and right edges and the bottom of the ACA panel (or its bar when folded), unscaled px;
 // false when neither shows.
 bool ACR_RightBox(out float left, out float right, out float bottom)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  bool any;
  left = ws.DPIUnscale(ws.GetWidth());
  right = 0;
  bottom = 0;
  array<Widget> boxes = {m_Root, m_DeclAcaBar};
  foreach (Widget box : boxes)
  {
   if (!box || !box.IsVisible()) continue;
   float x, y, w, h;
   box.GetScreenPos(x, y);
   box.GetScreenSize(w, h);
   if (w <= 0 || h <= 0) continue;
   any = true;
   left = Math.Min(left, ws.DPIUnscale(x));
   right = Math.Max(right, ws.DPIUnscale(x + w));
   bottom = Math.Max(bottom, ws.DPIUnscale(y + h));
  }
  return any;
 }
}

modded class SCR_PlayerController
{
 protected ref ACR_SquadsList m_ACR_SquadsList;
 protected ref ACR_MapLayerBar m_ACR_LayerBar;
 protected int m_iACR_SquadsListAt;
 static const int SQUADS_LIST_MS = 250;

 // A squad whose kind's layer is off, unless it is selected.
 override protected bool ACR_LayerHides(int g)
 {
  if (!m_ACR_Data || g < 0 || g >= m_ACR_Data.m_GroupIds.Count()) return false;
  RplId id = m_ACR_Data.m_GroupIds[g];
  if (!ACR_MapLayers.Off(ACR_MapLayers.KindOf(ACR_GroupIcons(id)))) return false;
  return !m_ACR_Data.IsSelected(id);
 }

 override void ACR_UpdateMap()
 {
  super.ACR_UpdateMap();
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.IsOpen() || !m_ACR_Panel || !m_ACR_Data) return;
  bool want = ACR_MapDeclutter.On() && !ACR_SetupClient.IsActive() && mapEntity.GetMapMenuRoot();
  m_ACR_Data.m_bDeclZonesOff = want && ACR_MapLayers.Off(ACR_MapLayers.ZONES);
  ACR_UpdateLayerBar(mapEntity, want);
  ACR_ApplyLayers(want);
  ACR_UpdateSquadsList(mapEntity, want);
 }

 override protected void ACR_ClosePanel()
 {
  if (m_ACR_SquadsList) m_ACR_SquadsList.Destroy();
  m_ACR_SquadsList = null;
  if (m_ACR_LayerBar) m_ACR_LayerBar.Destroy();
  m_ACR_LayerBar = null;
  super.ACR_ClosePanel();
 }

 override protected void ACR_PanelsToKeepClear(notnull array<float> rects)
 {
  super.ACR_PanelsToKeepClear(rects);
  if (m_ACR_SquadsList) ACR_CommandPanel.DeclAddRect(m_ACR_SquadsList.m_Root, rects);
  if (m_ACR_LayerBar) ACR_CommandPanel.DeclAddRect(m_ACR_LayerBar.m_Root, rects);
  ACR_CommandPanel.DeclAddRect(m_wDeclLegend, rects);
 }

 override protected bool ACR_OverOwnUI(Widget widget)
 {
  if (m_ACR_SquadsList && m_ACR_SquadsList.Contains(widget)) return true;
  if (m_ACR_LayerBar && m_ACR_LayerBar.Contains(widget)) return true;
  return super.ACR_OverOwnUI(widget);
 }

 override bool ACR_HandleMapRightClick()
 {
  Widget under = WidgetManager.GetWidgetUnderCursor();
  if ((m_ACR_SquadsList && m_ACR_SquadsList.Contains(under)) || (m_ACR_LayerBar && m_ACR_LayerBar.Contains(under))) return true;
  return super.ACR_HandleMapRightClick();
 }

 // ---------------------------------------------------------------- layers
 protected void ACR_UpdateLayerBar(SCR_MapEntity mapEntity, bool want)
 {
  if (!want)
  {
   if (m_ACR_LayerBar) m_ACR_LayerBar.SetVisible(false);
   return;
  }
  if (!m_ACR_LayerBar || !m_ACR_LayerBar.m_Root)
  {
   m_ACR_LayerBar = new ACR_MapLayerBar();
   m_ACR_LayerBar.Create(mapEntity.GetMapMenuRoot(), this);
  }
  m_ACR_LayerBar.SetVisible(true);
  m_ACR_LayerBar.Refresh();
  ACR_PlaceCorner();
 }

 // The bottom right corner: the legend (its marks) at the bottom, the layer switches above it. A
 // tall ACA panel (a squad selected: its orders, roadblocks, mines, Fortify) reaches down into that
 // corner (pn14: the switches sat over its hint line and hid the legend), so then both stand left
 // of it instead. Layer switches the player has put elsewhere (ACR_PanelMover) take the legend along.
 static const float CORNER_MARGIN = 24;
 static const float CORNER_GAP = 12;
 protected void ACR_PlaceCorner()
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float screenW = ws.DPIUnscale(ws.GetWidth());
  float screenH = ws.DPIUnscale(ws.GetHeight());
  float legendW, legendH, barW, barH;
  bool legend = m_wDeclLegend && m_wDeclLegend.IsVisible();
  if (legend)
  {
   m_wDeclLegend.GetScreenSize(legendW, legendH);
   legendW = ws.DPIUnscale(legendW);
   legendH = ws.DPIUnscale(legendH);
  }
  if (m_ACR_LayerBar && m_ACR_LayerBar.m_Root)
  {
   m_ACR_LayerBar.m_Root.GetScreenSize(barW, barH);
   barW = ws.DPIUnscale(barW);
   barH = ws.DPIUnscale(barH);
  }
  if (m_ACR_LayerBar && ACR_UiPrefs.Moved(ACR_UiPrefs.LAYERS))
  {
   float shownX = ACR_PanelMover.ShownX(ACR_UiPrefs.LAYERS);
   float shownY = ACR_PanelMover.ShownY(ACR_UiPrefs.LAYERS);
   // Under the switches, or over them when that runs off the screen.
   float legendY = shownY + 6 + legendH;
   if (legendY > -6) legendY = shownY - barH - 6;
   if (legend) FrameSlot.SetPos(m_wDeclLegend, shownX, legendY);
   return;
  }
  float stackTop = screenH - CORNER_MARGIN - legendH - 6 - barH;
  float right = CORNER_MARGIN;
  float panelLeft, panelRight, panelBottom;
  float stackLeft = screenW - CORNER_MARGIN - Math.Max(legendW, barW);
  if (m_ACR_Panel && m_ACR_Panel.ACR_RightBox(panelLeft, panelRight, panelBottom) && panelBottom > stackTop - CORNER_GAP && panelRight > stackLeft && panelLeft < screenW - CORNER_MARGIN)
   right = screenW - panelLeft + CORNER_GAP;
  m_fACR_CornerRight = right;
  if (legend) FrameSlot.SetPos(m_wDeclLegend, -right, -CORNER_MARGIN);
  float bottom = CORNER_MARGIN;
  if (legend) bottom += legendH + 6;
  if (m_ACR_LayerBar) m_ACR_LayerBar.Place(bottom, right);
 }
 protected float m_fACR_CornerRight;

 // What the canvas and the squads' markers don't do themselves (ACR_LayerHides, m_bDeclZonesOff):
 // the contacts', the empty vehicles' and the mortar ranges' markers go off the map (ACR_Park: the
 // game shows a hidden marker's widget again each frame). Their own updates put them back each
 // frame, so a layer switched back on shows them at once.
 protected void ACR_ApplyLayers(bool want)
 {
  if (!want) return;
  if (ACR_MapLayers.Off(ACR_MapLayers.CONTACTS))
  {
   foreach (ACR_UnitMarker contact : m_ACR_ContactMarkers) ACR_Park(contact);
  }
  if (ACR_MapLayers.Off(ACR_MapLayers.ZONES))
  {
   foreach (int z, ACR_UnitMarker zone : m_ACR_ZoneMarkers)
   {
    if (z < m_ACR_Data.m_ZoneLabels.Count() && (m_ACR_Data.m_ZoneLabels[z].StartsWith("~") || ACR_Intel.IsCivilianZone(m_ACR_Data.m_ZoneLabels[z]))) ACR_Park(zone);
   }
  }
  if (ACR_MapLayers.Off(ACR_MapLayers.VEHICLES))
  {
   foreach (ACR_UnitMarker vehicle : m_ACR_VehicleMarkers) ACR_Park(vehicle);
  }
 }

 // ---------------------------------------------------------------- the Squads list
 protected void ACR_UpdateSquadsList(SCR_MapEntity mapEntity, bool want)
 {
  if (!want)
  {
   if (m_ACR_SquadsList) m_ACR_SquadsList.SetVisible(false);
   return;
  }
  if (!m_ACR_SquadsList || !m_ACR_SquadsList.m_Root)
  {
   m_ACR_SquadsList = new ACR_SquadsList();
   m_ACR_SquadsList.Create(mapEntity.GetMapMenuRoot(), this);
   m_iACR_SquadsListAt = 0;
  }
  // While the search box is being typed in, the map's keys stay out of it.
  if (m_ACR_SquadsList.Typing())
  {
   InputManager input = GetGame().GetInputManager();
   if (input) input.ActivateContext("ACR_SquadSearchContext");
  }
  // Under INTEL, wherever that stands, until the player puts the list somewhere of its own (a
  // double click on its title brings it back under INTEL). Clear of a debug box below it.
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float top = Math.Max(96, m_ACR_Panel.ACR_LeftBottom() + 10);
  m_ACR_SquadsList.Place(ACR_PanelMover.ShownX(ACR_UiPrefs.FEED), top);
  float spotX, spotY;
  if (ACR_UiPrefs.Spot(ACR_UiPrefs.SQUADS, spotX, spotY)) top = spotY;
  float floor = ws.DPIUnscale(ws.GetHeight()) - 24;
  Widget debugBox = m_ACR_Panel.DebugRoot();
  if (debugBox && debugBox.IsVisible())
  {
   float dx, dy;
   debugBox.GetScreenPos(dx, dy);
   if (ws.DPIUnscale(dy) > top) floor = Math.Min(floor, ws.DPIUnscale(dy) - 10);
  }
  m_ACR_SquadsList.SetFar(m_ACR_Panel.DeclIntelCollapsed());
  int now = System.GetTickCount();
  if (now - m_iACR_SquadsListAt < SQUADS_LIST_MS) return;
  m_iACR_SquadsListAt = now;
  // Our squads, in the order the map knows them.
  string query = m_ACR_SquadsList.Query();
  array<RplId> ids = {};
  array<string> labels = {};
  int total;
  foreach (int g, RplId id : m_ACR_Data.m_GroupIds)
  {
   if (g >= m_ACR_Data.m_Commandable.Count() || m_ACR_Data.m_Commandable[g] != 1) continue;
   total++;
   string name = "Squad";
   if (g < m_ACR_Data.m_GroupNames.Count()) name = m_ACR_Data.m_GroupNames[g];
   string kind = ACR_SquadPicker.TypeWord(ACR_GroupIcons(id));
   if (!ACR_SquadsList.Matches(query, name, kind)) continue;
   string doing = "";
   if (g < m_ACR_Data.m_DeclMark.Count() && m_ACR_Data.m_DeclMark[g] >= 0) doing = ACR_MapDeclutter.MarkName(m_ACR_Data.m_DeclMark[g]);
   if (m_ACR_Data.DeclHidden(g)) doing = "hidden";
   int health = -1;
   int ammo = -1;
   int s = m_ACR_Data.m_StatusGroups.Find(id);
   if (s >= 0 && s * 6 + 1 < m_ACR_Data.m_Status.Count())
   {
    health = m_ACR_Data.m_Status[s * 6];
    ammo = m_ACR_Data.m_Status[s * 6 + 1];
   }
   ids.Insert(id);
   labels.Insert(ACR_SquadsList.Label(name, kind, doing, health, ammo, m_ACR_Data.IsSelected(id)));
  }
  // The header, the search box and a margin above the rows (unscaled px).
  float room = floor - top - 34 - 50 - 20;
  m_ACR_SquadsList.SetRows(ids, labels, total, room);
  m_ACR_SquadsList.SetVisible(total > 0);
 }

 // A row: that squad is selected (Shift adds it).
 void ACR_SquadsListPick(RplId id)
 {
  ACR_MarkUiClick();
  ACR_CloseSquadPicker();
  ACR_SelectGroup(id);
  m_iACR_SquadsListAt = 0;
 }

 // Show: selected, and the map goes there (the 2IC panel's Show).
 void ACR_SquadsListShow(RplId id)
 {
  if (!m_ACR_Data) return;
  int g = m_ACR_Data.m_GroupIds.Find(id);
  if (g < 0) return;
  ACR_SquadsListPick(id);
  ACR_FocusMap(m_ACR_Data.LeaderPos(g));
 }

 // Tests.
 float ACR_CornerRightForTest() { return m_fACR_CornerRight; }
 // The layer switches and the legend clear of the ACA panel (none overlaps it).
 bool ACR_CornerClearForTest()
 {
  if (!m_ACR_Panel || !m_ACR_Panel.m_Root || !m_ACR_Panel.m_Root.IsVisible()) return true;
  array<Widget> boxes = {m_wDeclLegend};
  if (m_ACR_LayerBar) boxes.Insert(m_ACR_LayerBar.m_Root);
  float px, py, pw, ph;
  m_ACR_Panel.m_Root.GetScreenPos(px, py);
  m_ACR_Panel.m_Root.GetScreenSize(pw, ph);
  foreach (Widget box : boxes)
  {
   if (!box || !box.IsVisible()) continue;
   float x, y, w, h;
   box.GetScreenPos(x, y);
   box.GetScreenSize(w, h);
   if (x < px + pw && x + w > px && y < py + ph && y + h > py) return false;
  }
  return true;
 }
 ACR_SquadsList ACR_SquadsListForTest() { return m_ACR_SquadsList; }
 ACR_MapLayerBar ACR_LayerBarForTest() { return m_ACR_LayerBar; }
 void ACR_SquadsListRefreshForTest() { m_iACR_SquadsListAt = 0; }
 bool ACR_GroupMarkerShownForTest(RplId groupId)
 {
  foreach (ACR_UnitMarker marker : m_ACR_GroupMarkers)
  {
   if (marker && marker.m_GroupId == groupId) return ACR_OnScreenForTest(marker);
  }
  return false;
 }
 // Markers on the map that aren't where their unit is: squads', the command unit's, our empty
 // vehicles', contacts' (a marker in a base's count is off the map); standing: those that are.
 int ACR_FalseMarkersForTest(out int standing, out string detail)
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  standing = 0;
  detail = "";
  int wrong;
  foreach (int g, ACR_UnitMarker squad : m_ACR_GroupMarkers)
  {
   if (!squad || !ACR_OnScreenForTest(squad) || g >= m_ACR_Data.m_LeaderPositions.Count()) continue;
   standing++;
   if (ACR_MarkerAtForTest(mapEntity, squad, m_ACR_Data.m_LeaderPositions[g])) continue;
   wrong++;
   detail += m_ACR_Data.m_GroupNames[g] + " ";
  }
  foreach (ACR_UnitMarker marker : m_ACR_Markers)
  {
   if (!marker || !marker.m_Original || !ACR_OnScreenForTest(marker)) continue;
   standing++;
   if (ACR_MarkerAtForTest(mapEntity, marker, m_ACR_OriginalPos)) continue;
   wrong++;
   detail += "command-unit ";
  }
  foreach (int v, ACR_UnitMarker vehicle : m_ACR_VehicleMarkers)
  {
   if (!vehicle || !ACR_OnScreenForTest(vehicle) || v >= m_ACR_VehiclePositions.Count()) continue;
   standing++;
   if (ACR_MarkerAtForTest(mapEntity, vehicle, m_ACR_VehiclePositions[v])) continue;
   wrong++;
   detail += "vehicle ";
  }
  foreach (int c, ACR_UnitMarker contact : m_ACR_ContactMarkers)
  {
   if (!contact || !ACR_OnScreenForTest(contact) || c >= m_ACR_ContactPositions.Count()) continue;
   standing++;
   if (ACR_MarkerAtForTest(mapEntity, contact, m_ACR_ContactPositions[c])) continue;
   wrong++;
   detail += "contact ";
  }
  return wrong;
 }

 // The marker is at the unit, to a packet's worth of a unit on the move (15 m), and its widget is
 // where the marker says, to a few pixels.
 protected bool ACR_MarkerAtForTest(SCR_MapEntity mapEntity, ACR_UnitMarker marker, vector real)
 {
  float x, y;
  if (!mapEntity || !ACR_DeclPoint(marker, x, y)) return false;
  int at[2];
  marker.GetWorldPos(at);
  if (vector.DistanceXZ(Vector(at[0], 0, at[1]), real) > 15) return false;
  int sx, sy;
  mapEntity.WorldToScreen(at[0], at[1], sx, sy, true);
  float reach = GetGame().GetWorkspace().DPIScale(5);
  return Math.AbsFloat(sx - x) <= reach && Math.AbsFloat(sy - y) <= reach;
 }

 // Base b's count against a recount, and what it says.
 bool ACR_ChipCountRightForTest()
 {
  int total;
  foreach (int b, int own : m_ACR_Data.m_DeclChipOwn)
  {
   int members;
   foreach (int at : m_ACR_Data.m_DeclBaseOf) if (at == b) members++;
   if (members != own) return false;
   total += own;
  }
  foreach (int g, int into : m_ACR_Data.m_DeclBaseOf)
  {
   // In a count and not selected: no icon, and out of it: an icon or a pile that shows it.
   bool selected = g < m_ACR_Data.m_GroupIds.Count() && m_ACR_Data.IsSelected(m_ACR_Data.m_GroupIds[g]);
   if (into >= 0 && !selected && m_ACR_Data.m_DeclRep[g]) return false;
  }
  return true;
 }

 int ACR_ChipTotalForTest()
 {
  int total;
  foreach (int own : m_ACR_Data.m_DeclChipOwn) total += own;
  return total;
 }

 int ACR_ChipEnemyForTest()
 {
  int total;
  foreach (int enemy : m_ACR_Data.m_DeclChipEnemy) total += enemy;
  return total;
 }

 string ACR_ChipTextForTest()
 {
  foreach (int b, int own : m_ACR_Data.m_DeclChipOwn)
  {
   if (own + m_ACR_Data.m_DeclChipEnemy[b] > 0) return ACR_MapDeclutter.ChipText(own, m_ACR_Data.m_DeclChipEnemy[b]);
  }
  return "";
 }

 // The list open and clear of every panel, on the screen.
 bool ACR_SquadListClearForTest()
 {
  if (!ACR_SquadPickerOpen()) return false;
  array<float> rects = {};
  ACR_PanelsToKeepClear(rects);
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float x, y, w, h;
  m_ACR_SquadPicker.m_Root.GetScreenPos(x, y);
  m_ACR_SquadPicker.m_Root.GetScreenSize(w, h);
  if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > ws.GetWidth() || y + h > ws.GetHeight()) return false;
  return ACR_MapDeclutter.FirstHit(ws.DPIUnscale(x), ws.DPIUnscale(y), ws.DPIUnscale(x + w), ws.DPIUnscale(y + h), rects) < 0;
 }

 int ACR_ListedBaseForTest() { return m_iACR_ListBase; }

 int ACR_FirstChipBaseForTest()
 {
  foreach (int b, int own : m_ACR_Data.m_DeclChipOwn)
  {
   if (own + m_ACR_Data.m_DeclChipEnemy[b] > 0) return b;
  }
  return -1;
 }

 // Where a click on the first base's count would land (screen px).
 bool ACR_ChipPointForTest(out float x, out float y)
 {
  int b = ACR_FirstChipBaseForTest();
  if (b < 0 || b * 4 + 3 >= m_ACR_Data.m_DeclChipRect.Count()) return false;
  x = (m_ACR_Data.m_DeclChipRect[b * 4] + m_ACR_Data.m_DeclChipRect[b * 4 + 2]) * 0.5;
  y = (m_ACR_Data.m_DeclChipRect[b * 4 + 1] + m_ACR_Data.m_DeclChipRect[b * 4 + 3]) * 0.5;
  return true;
 }

 bool ACR_DotPointForTest(int b, out float x, out float y)
 {
  if (b < 0 || b * 2 + 1 >= m_ACR_Data.m_DeclDots.Count()) return false;
  x = m_ACR_Data.m_DeclDots[b * 2];
  y = m_ACR_Data.m_DeclDots[b * 2 + 1];
  return true;
 }

 bool ACR_ListRowPointForTest(int i, out float x, out float y)
 {
  return m_ACR_SquadPicker && m_ACR_SquadPicker.RowPointForTest(i, x, y);
 }

 // A list far too long for the screen: our squads, over and over (only squads we command ever get
 // into a list).
 void ACR_BigListForTest()
 {
  ACR_CloseSquadPicker();
  array<int> ours = {};
  foreach (int g, int commandable : m_ACR_Data.m_Commandable)
  {
   if (commandable == 1) ours.Insert(g);
  }
  if (ours.IsEmpty()) return;
  array<int> members = {};
  for (int i = 0; i < 40; i++) members.Insert(ours[i % ours.Count()]);
  ACR_OpenSquadList(members, "40 UNITS HERE", -1, null);
 }

 bool ACR_ListRowInViewForTest(int i) { return m_ACR_SquadPicker && m_ACR_SquadPicker.RowInViewForTest(i); }

 float ACR_ListScrollForTest()
 {
  if (!m_ACR_SquadPicker) return -1;
  return m_ACR_SquadPicker.ScrollForTest();
 }

 void ACR_ListScrollEndForTest()
 {
  if (m_ACR_SquadPicker) m_ACR_SquadPicker.ScrollEndForTest();
 }

 // The map's own click (as where the cursor is); a click just made doesn't count against it.
 void ACR_MapClickForTest()
 {
  m_ACR_LastUiClick = 0;
  ACR_ClosePicker();
 }

 // A contact at spot, as the server would send it (a test's: it is gone with the next packet unless
 // the test sends it again).
 void ACR_ContactForTest(vector spot)
 {
  m_ACR_ContactPositions.Insert(spot);
  m_ACR_ContactKinds.Insert(0);
  m_ACR_ContactCounts.Insert(4);
  m_ACR_ContactAges.Insert(5);
  m_ACR_ContactDetails.Insert(-900 - m_ACR_ContactPositions.Count());
  m_ACR_ContactDetails.Insert(0);
  m_ACR_ContactDetails.Insert(-1);
 }

 void ACR_ContactsClearForTest()
 {
  m_ACR_ContactPositions.Clear();
  m_ACR_ContactKinds.Clear();
  m_ACR_ContactCounts.Clear();
  m_ACR_ContactAges.Clear();
  m_ACR_ContactDetails.Clear();
 }

 // Shown and on the screen (a parked marker's widget is still "visible", off the map).
 static bool ACR_OnScreenForTest(ACR_UnitMarker marker)
 {
  Widget root = marker.GetRootWidget();
  if (!root || !root.IsVisible()) return false;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float x, y;
  root.GetScreenPos(x, y);
  return x > -200 && y > -200 && x < ws.GetWidth() + 200 && y < ws.GetHeight() + 200;
 }
 int ACR_LayerKindForTest(RplId groupId) { return ACR_MapLayers.KindOf(ACR_GroupIcons(groupId)); }
}

// Zones off: the mortar teams' range rings and the civilian rings go; a fire mission's danger area stays.
modded class ACR_OrderDisplay
{
 override protected void DrawZones(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  if (!data.m_bDeclZonesOff)
  {
   super.DrawZones(mapEntity, data);
   return;
  }
  Color red = new Color(1, 0.15, 0.1, 0.95);
  foreach (int i, vector pos : data.m_ZonePositions)
  {
   if (i < data.m_ZoneLabels.Count() && (data.m_ZoneLabels[i].StartsWith("~") || ACR_Intel.IsCivilianZone(data.m_ZoneLabels[i]))) continue;
   int cx, cy, ex, ey;
   mapEntity.WorldToScreen(pos[0], pos[2], cx, cy, true);
   mapEntity.WorldToScreen(pos[0] + data.m_ZoneRadii[i], pos[2], ex, ey, true);
   float radius = Math.Max(Math.AbsFloat(ex - cx), 6);
   Ring(cx, cy, radius, red.PackToInt(), 3, 48);
  }
 }
}
