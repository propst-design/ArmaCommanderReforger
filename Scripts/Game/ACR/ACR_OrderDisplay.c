// Client: everything drawn on the command map's canvas (base rings, support
// danger zones, order lines, status bars, selections, the selection box) plus
// the small name tags on other commanders' orders. Reads ACR_MapData; owns no
// game state. Pings are drawn by ACR_PingDisplay on the same canvas.
//
// Shared command: your own orders (and the AI's) are solid lines; another
// commander's are dashed in the same order colour, with his short name at the
// end. Squads another commander has selected get a thin white ring.
class ACR_OrderDisplay : Managed
{
 protected CanvasWidget m_Canvas;
 protected ref array<ref CanvasWidgetCommand> m_Commands = {};
 protected ref array<ref ACR_UnitMarker> m_TagMarkers = {};

 void SetCanvas(CanvasWidget canvas)
 {
  m_Canvas = canvas;
 }

 CanvasWidget GetCanvas() { return m_Canvas; }

 // Draw into someone else's command list (the HUD's ping layer).
 void UseCommands(array<ref CanvasWidgetCommand> commands)
 {
  m_Commands = commands;
 }

 // ---------------------------------------------------------------- colours
 static int OrderColor(int kind, float alpha)
 {
  Color color = new Color(0.30, 0.65, 1.00, alpha);                               // move: blue
  if (kind == ACR_EOrderKind.DEFEND) color = new Color(0.35, 0.90, 0.40, alpha);  // defend: green
  if (kind == ACR_EOrderKind.GET_IN) color = new Color(1.00, 0.85, 0.25, alpha);  // get in: yellow
  if (kind == ACR_EOrderKind.GET_OUT) color = new Color(1.00, 0.55, 0.20, alpha); // get out: orange
  if (kind == ACR_EOrderKind.HUNT) color = new Color(1.00, 0.25, 0.20, alpha);    // hunt: red
  return color.PackToInt();
 }

 // Friendly blue, enemy red, unclaimed yellow (the Arma 3 mod's colours).
 // During setup nobody has joined a faction yet: Blue = west, Red = east.
 static Color BaseColor(string owner, float alpha)
 {
  if (owner.IsEmpty()) return new Color(0.85, 0.72, 0.1, alpha);
  Faction mine = SCR_FactionManager.SGetLocalPlayerFaction();
  if (mine && mine.GetFactionKey() == owner) return new Color(0.2, 0.5, 1.0, alpha);
  if (!mine && ACR_SetupClient.IsActive())
  {
   string west, east;
   ACR_SetupClient.Sides(west, east);
   if (!west.IsEmpty() && owner == west) return new Color(0.2, 0.5, 1.0, alpha);
   if (!east.IsEmpty() && owner == east) return new Color(0.9, 0.15, 0.15, alpha);
  }
  // FIA is only ever a third party here (pockets are off when FIA plays a main side).
  if (owner == ACR_Pockets.FACTION) return new Color(0.2, 0.7, 0.25, alpha);
  return new Color(0.9, 0.15, 0.15, alpha);
 }

 // ---------------------------------------------------------------- frame
 // One SetDrawCommands a frame (review P9: the calm map and the 2IC each handed the list to the
 // canvas again after adding to it). Layers add their commands in DrawLayers.
 void Draw(SCR_MapEntity mapEntity, ACR_MapData data, bool boxing, vector boxStart)
 {
  if (!m_Canvas) return;
  m_Commands.Clear();
  DrawLayers(mapEntity, data, boxing, boxStart);
  m_Canvas.SetDrawCommands(m_Commands);
 }

 protected void DrawLayers(SCR_MapEntity mapEntity, ACR_MapData data, bool boxing, vector boxStart)
 {
  DrawBaseAreas(mapEntity, data);
  DrawZones(mapEntity, data);
  DrawMineShapes(mapEntity, data);
  DrawEmptyVehicles(mapEntity, data);
  DrawOrders(mapEntity, data);
  DrawOtherSelections(mapEntity, data);
  DrawStatusBars(mapEntity, data);
  DrawPending(mapEntity, data);
  DrawContactHeadings(mapEntity, data);
  if (boxing) DrawBox(mapEntity, boxStart);
  if (data.m_bAiming) DrawAim(mapEntity, data.m_vAimFrom, data.m_vAimTo);
  DrawMinePreview(mapEntity, data);
  ACR_PingDisplay.DrawOnMap(this, mapEntity, data);
 }

 void Add(CanvasWidgetCommand command)
 {
  m_Commands.Insert(command);
 }

 void Line(float x0, float y0, float x1, float y1, int color, float width)
 {
  LineDrawCommand line = new LineDrawCommand();
  line.m_iColor = color;
  line.m_fWidth = width;
  line.m_Vertices = {x0, y0, x1, y1};
  m_Commands.Insert(line);
 }

 // Dashes along a line; the dash length grows on long lines so a line never
 // takes more than ~60 draw commands.
 void DashedLine(float x0, float y0, float x1, float y1, int color, float width)
 {
  float dx = x1 - x0;
  float dy = y1 - y0;
  float length = Math.Sqrt(dx * dx + dy * dy);
  if (length < 1) return;
  float dash = Math.Max(9, length / 60);
  float gap = dash * 0.7;
  float step = dash + gap;
  float at = 0;
  while (at < length)
  {
   float end = Math.Min(at + dash, length);
   Line(x0 + dx * at / length, y0 + dy * at / length, x0 + dx * end / length, y0 + dy * end / length, color, width);
   at += step;
  }
 }

 void Ring(float x, float y, float radius, int color, float width, int segments = 24)
 {
  LineDrawCommand ring = new LineDrawCommand();
  ring.m_iColor = color;
  ring.m_fWidth = width;
  ring.m_bShouldEnclose = true;
  ring.m_Vertices = {};
  m_Canvas.TessellateCircle(Vector(x, y, 0), radius, segments, ring.m_Vertices);
  m_Commands.Insert(ring);
 }

 // Dashed circle around (x, y); scales dash count with screen radius so small and large rings read cleanly.
 void DashedRing(float x, float y, float radius, int color, float width, int dashes = 24)
 {
  if (radius < 4)
  {
   Ring(x, y, radius, color, width, 16);
   return;
  }
  int count = dashes;
  if (radius < 22) count = 12;
  else if (radius > 160) count = 36;
  float step = Math.PI2 / count;
  float arc = step * 0.56;
  for (int i = 0; i < count; i++)
  {
   float a0 = i * step;
   float a1 = a0 + arc;
   float am = a0 + arc * 0.5;
   LineDrawCommand seg = new LineDrawCommand();
   seg.m_iColor = color;
   seg.m_fWidth = width;
   seg.m_Vertices = {
    x + Math.Cos(a0) * radius, y + Math.Sin(a0) * radius,
    x + Math.Cos(am) * radius, y + Math.Sin(am) * radius,
    x + Math.Cos(a1) * radius, y + Math.Sin(a1) * radius
   };
   m_Commands.Insert(seg);
  }
 }

 void Dot(float x, float y, float radius, int color)
 {
  if (radius < 1) return;
  PolygonDrawCommand dot = new PolygonDrawCommand();
  dot.m_iColor = color;
  dot.m_Vertices = {};
  m_Canvas.TessellateCircle(Vector(x, y, 0), radius, 12, dot.m_Vertices);
  AddPolygon(dot);
 }

 // ---------------------------------------------------------------- filled shapes
 // The game triangulates every PolygonDrawCommand, every frame, and logs "DrawPolygon triangulation
 // failed, is the polygon degenerated?" for each one it can't. Live (27 Oct, Everon): 160,000 of those
 // lines, up to 30 a frame, while the map was zoomed in: the far squads' status bars and order dots
 // were tens of thousands of pixels off the canvas, where a shape a few pixels wide no longer
 // triangulates. Such shapes are skipped (nobody can see them), and so is one with fewer than three
 // distinct points, no area or a NaN. Anything on the canvas draws exactly as before.
 static const float POLYGON_MIN_AREA = 0.05; // square pixels
 static const float POLYGON_MARGIN = 2; // pixels past the canvas edge that still count as on it
 static const float POLYGON_FAR = 1000000000; // a coordinate past this (or NaN) is garbage

 // Whether the game can fill this polygon ({x0, y0, x1, y1, ...}) and it shows on a width x height
 // canvas (a size of 0 means not known: not checked).
 static bool PolygonDrawable(array<float> vertices, float width, float height)
 {
  if (!vertices || vertices.Count() % 2 != 0) return false;
  int points = vertices.Count() / 2;
  if (points < 3) return false;
  float x0 = vertices[0];
  float y0 = vertices[1];
  float minX = x0;
  float maxX = x0;
  float minY = y0;
  float maxY = y0;
  float twiceArea = 0;
  int distinct = 0;
  for (int p = 0; p < points; p++)
  {
   float x = vertices[p * 2];
   float y = vertices[p * 2 + 1];
   // NaN fails every comparison, so this rejects it too.
   if (!(Math.AbsFloat(x) < POLYGON_FAR) || !(Math.AbsFloat(y) < POLYGON_FAR)) return false;
   minX = Math.Min(minX, x);
   maxX = Math.Max(maxX, x);
   minY = Math.Min(minY, y);
   maxY = Math.Max(maxY, y);
   int q = (p + 1) % points;
   float nx = vertices[q * 2];
   float ny = vertices[q * 2 + 1];
   if (Math.AbsFloat(nx - x) > 0.01 || Math.AbsFloat(ny - y) > 0.01) distinct++;
   // Shoelace, relative to the first point so far-off coordinates keep their precision.
   twiceArea += (x - x0) * (ny - y0) - (nx - x0) * (y - y0);
  }
  if (distinct < 3 || Math.AbsFloat(twiceArea) < 2 * POLYGON_MIN_AREA) return false;
  if (width <= 0 || height <= 0) return true;
  if (maxX < -POLYGON_MARGIN || maxY < -POLYGON_MARGIN) return false;
  return minX <= width + POLYGON_MARGIN && minY <= height + POLYGON_MARGIN;
 }

 // The canvas's size in native pixels (the units of WorldToScreen and of the draw commands).
 protected void CanvasSize(out float width, out float height)
 {
  width = 0;
  height = 0;
  if (m_Canvas) m_Canvas.GetScreenSize(width, height);
  WorkspaceWidget workspace = GetGame().GetWorkspace();
  if (!workspace) return;
  width = Math.Max(width, workspace.GetWidth());
  height = Math.Max(height, workspace.GetHeight());
 }

 // A filled shape, unless the game couldn't fill it or it's off the canvas (PolygonDrawable).
 void AddPolygon(PolygonDrawCommand polygon)
 {
  if (!polygon) return;
  float width, height;
  CanvasSize(width, height);
  if (PolygonDrawable(polygon.m_Vertices, width, height)) m_Commands.Insert(polygon);
 }

 // ---------------------------------------------------------------- order lines
 protected void DrawOrders(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  foreach (int i, RplId groupId : data.m_OrderGroups)
  {
   int g = data.m_GroupIds.Find(groupId);
   if (g < 0 || g >= data.m_LeaderIds.Count()) continue;
   vector from = data.LeaderPos(g);
   if (from == vector.Zero) continue; // a new squad whose position hasn't come yet: no line from the map corner
   // Later entries of the same squad are the rest of its plan: chain them.
   bool chained = i > 0 && data.m_OrderGroups[i - 1] == groupId;
   if (chained) from = data.m_OrderTargets[i - 1];
   vector to = data.m_OrderTargets[i];
   int sx0, sy0, sx1, sy1;
   mapEntity.WorldToScreen(from[0], from[2], sx0, sy0, true);
   mapEntity.WorldToScreen(to[0], to[2], sx1, sy1, true);
   bool selected = data.IsSelected(groupId);
   float alpha = 0.75;
   if (selected) alpha = 1;
   int color = OrderColor(data.m_OrderKinds[i], alpha);
   float width = 2;
   if (selected) width = 3;
   int issuer = 0;
   if (i < data.m_OrderIssuers.Count()) issuer = data.m_OrderIssuers[i];
   bool theirs = issuer > 0 && issuer != data.m_iMyId;
   if (theirs) DashedLine(sx0, sy0, sx1, sy1, color, width);
   else Line(sx0, sy0, sx1, sy1, color, width);
   Dot(sx1, sy1, 6, color);
  }
 }

 // Tag markers ("Popp") at the end of other commanders' current orders.
 void UpdateTags(SCR_MapMarkerManagerComponent manager, ACR_MapData data)
 {
  array<vector> spots = {};
  array<string> labels = {};
  foreach (int i, RplId groupId : data.m_OrderGroups)
  {
   if (i > 0 && data.m_OrderGroups[i - 1] == groupId) continue; // only the current order
   int issuer = 0;
   if (i < data.m_OrderIssuers.Count()) issuer = data.m_OrderIssuers[i];
   if (issuer <= 0 || issuer == data.m_iMyId) continue;
   string tag = data.TagOf(issuer);
   if (tag.IsEmpty()) continue;
   spots.Insert(data.m_OrderTargets[i]);
   labels.Insert(tag);
  }
  if (m_TagMarkers.Count() != spots.Count())
  {
   foreach (ACR_UnitMarker old : m_TagMarkers) manager.ACR_RemoveMarker(old);
   m_TagMarkers.Clear();
   for (int m = 0; m < spots.Count(); m++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    marker.m_bContact = true; // clicks go through to the map
    marker.m_State = 5;       // an order tag before its widget is made (not an enemy pip)
    manager.ACR_AddSimpleMarker(marker, m_TagMarkers);
   }
  }
  foreach (int t, ACR_UnitMarker tagMarker : m_TagMarkers)
  {
   tagMarker.m_Label = labels[t];
   tagMarker.m_State = 5;
   vector pos = spots[t];
   tagMarker.SetWorldPos(pos[0], pos[2]);
   tagMarker.RefreshStyle();
  }
 }

 void ClearMarkers(SCR_MapMarkerManagerComponent manager)
 {
  if (manager) foreach (ACR_UnitMarker old : m_TagMarkers) manager.ACR_RemoveMarker(old);
  m_TagMarkers.Clear();
  ACR_PingDisplay.ClearMarkers(manager);
 }

 // ---------------------------------------------------------------- selections
 // Squads another commander has selected: a thin white ring.
 protected void DrawOtherSelections(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  Color white = new Color(1, 1, 1, 0.8);
  int color = white.PackToInt();
  foreach (int s, RplId groupId : data.m_SelGroups)
  {
   if (data.m_SelOwners[s] == data.m_iMyId) continue;
   int g = data.m_GroupIds.Find(groupId);
   if (g < 0 || g >= data.m_LeaderIds.Count()) continue;
   vector pos = data.LeaderPos(g);
   if (pos == vector.Zero) continue; // position not come yet
   int sx, sy;
   mapEntity.WorldToScreen(pos[0], pos[2], sx, sy, true);
   Ring(sx, sy, 17, color, 1.5);
  }
 }

 // ---------------------------------------------------------------- status bars
 // Two small bars under each friendly squad marker: health (green, amber, red)
 // and ammo (blue, red when low).
 protected void DrawStatusBars(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  foreach (int i, RplId groupId : data.m_StatusGroups)
  {
   if (i * 6 + 1 >= data.m_Status.Count()) break;
   int health = data.m_Status[i * 6];
   int ammo = data.m_Status[i * 6 + 1];
   if (health < 0) continue;
   int g = data.m_GroupIds.Find(groupId);
   if (g < 0 || g >= data.m_LeaderPositions.Count()) continue;
   vector pos = data.LeaderPos(g);
   if (pos == vector.Zero) continue; // position not come yet
   int sx, sy;
   mapEntity.WorldToScreen(pos[0], pos[2], sx, sy, true);
   float x = sx - 15;
   float y = sy + 16;
   Color healthColor = new Color(0.30, 0.85, 0.35, 0.95);
   if (health < 60) healthColor = new Color(1.00, 0.75, 0.20, 0.95);
   if (health < 30) healthColor = new Color(1.00, 0.25, 0.20, 0.95);
   Bar(x, y, 30, 4, health / 100.0, healthColor);
   if (ammo >= 0)
   {
    Color ammoColor = new Color(0.35, 0.70, 1.00, 0.95);
    if (ammo < 30) ammoColor = new Color(1.00, 0.25, 0.20, 0.95);
    Bar(x, y + 6, 30, 4, ammo / 100.0, ammoColor);
   }
  }
 }

 protected void Bar(float x, float y, float width, float height, float fill, Color color)
 {
  if (width < 2 || height < 2) return;
  fill = Math.Clamp(fill, 0, 1);
  Color dark = new Color(0, 0, 0, 0.6);
  int darkColor = dark.PackToInt();
  float x0 = x - 1;
  float y0 = y - 1;
  float x1 = x + width + 1;
  float y1 = y + height + 1;

  PolygonDrawCommand back1 = new PolygonDrawCommand();
  back1.m_iColor = darkColor;
  back1.m_Vertices = {x0, y0, x1, y0, x1, y1};
  AddPolygon(back1);

  PolygonDrawCommand back2 = new PolygonDrawCommand();
  back2.m_iColor = darkColor;
  back2.m_Vertices = {x0, y0, x1, y1, x0, y1};
  AddPolygon(back2);

  float fillWidth = width * fill;
  if (fillWidth < 1.0) return;
  float right = x + fillWidth;
  int barColor = color.PackToInt();

  PolygonDrawCommand bar1 = new PolygonDrawCommand();
  bar1.m_iColor = barColor;
  bar1.m_Vertices = {x, y, right, y, right, y + height};
  AddPolygon(bar1);

  PolygonDrawCommand bar2 = new PolygonDrawCommand();
  bar2.m_iColor = barColor;
  bar2.m_Vertices = {x, y, right, y + height, x, y + height};
  AddPolygon(bar2);
 }

 // ---------------------------------------------------------------- bases, zones
 // Capture radius circles; contested bases get a thicker outline.
 protected void DrawBaseAreas(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  foreach (int i, vector pos : data.m_BasePositions)
  {
   int cx, cy, ex, ey;
   mapEntity.WorldToScreen(pos[0], pos[2], cx, cy, true);
   mapEntity.WorldToScreen(pos[0] + data.m_BaseRadii[i], pos[2], ex, ey, true);
   float radius = Math.AbsFloat(ex - cx);
   if (radius < 3) continue;
   float width = 2;
   if ((data.m_BaseContested[i] & 1) != 0) width = 4;
   Ring(cx, cy, radius, BaseColor(data.m_BaseOwners[i], 0.9).PackToInt(), width, 48);
  }
 }

 // Zone classification for rings, center glyphs and hover cards:
 // 0 = range (~), 1 = civilian (%), 2 = FIA ambush (dangerous road), 3 = zombie area,
 // 4 = FIA hideout, 5 = FIA activity lead, 6 = friendly fire mission, 7 = other red zone.
 static int ZoneKind(string label)
 {
  if (label.StartsWith("~")) return 0;
  if (ACR_Intel.IsCivilianZone(label)) return 1;
  if (label == "FIA ambush") return 2;
  if (label == "Zombies") return 3;
  if (label == "FIA hideout") return 4;
  if (label == "FIA activity") return 5;
  if (label.Contains(" - ACTIVE") || label.EndsWith(" s")) return 6;
  return 7;
 }

 static bool ZoneHasCustomGlyph(string label)
 {
  int kind = ZoneKind(label);
  return kind >= 2 && kind <= 6;
 }

 // Crisp warning triangle (!) for dangerous roads, zombie areas and FIA intel leads.
 protected void DrawWarningTriangle(float cx, float cy, int fillColor)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float s = 1;
  if (ws) s = ws.DPIScale(1);
  int dark = Color.FromRGBA(18, 20, 24, 245).PackToInt();
  int white = Color.FromRGBA(250, 250, 250, 240).PackToInt();
  float rOut = 12 * s;
  float rMid = 10.6 * s;
  float rIn = 9 * s;
  PolygonDrawCommand outer = new PolygonDrawCommand();
  outer.m_iColor = dark;
  outer.m_Vertices = {cx, cy - rOut, cx + rOut * 0.96, cy + rOut * 0.72, cx - rOut * 0.96, cy + rOut * 0.72};
  AddPolygon(outer);
  PolygonDrawCommand rim = new PolygonDrawCommand();
  rim.m_iColor = white;
  rim.m_Vertices = {cx, cy - rMid, cx + rMid * 0.95, cy + rMid * 0.70, cx - rMid * 0.95, cy + rMid * 0.70};
  AddPolygon(rim);
  PolygonDrawCommand inner = new PolygonDrawCommand();
  inner.m_iColor = fillColor;
  inner.m_Vertices = {cx, cy - rIn, cx + rIn * 0.93, cy + rIn * 0.68, cx - rIn * 0.93, cy + rIn * 0.68};
  AddPolygon(inner);
  // Exclamation bar + dot inside the triangle.
  Line(cx, cy - 4.4 * s, cx, cy + 1.2 * s, dark, 2.2 * s);
  Dot(cx, cy + 4.1 * s, 1.45 * s, dark);
 }

 // Target crosshair glyph for active/incoming friendly fire missions.
 protected void DrawFireMissionGlyph(float cx, float cy)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float s = 1;
  if (ws) s = ws.DPIScale(1);
  int dark = Color.FromRGBA(18, 20, 24, 235).PackToInt();
  int red = Color.FromRGBA(255, 55, 45, 250).PackToInt();
  Ring(cx, cy, 8 * s, dark, 4.2 * s, 20);
  Ring(cx, cy, 8 * s, red, 2.2 * s, 20);
  Line(cx - 12 * s, cy, cx - 4.5 * s, cy, dark, 3.6 * s);
  Line(cx + 4.5 * s, cy, cx + 12 * s, cy, dark, 3.6 * s);
  Line(cx, cy - 12 * s, cx, cy - 4.5 * s, dark, 3.6 * s);
  Line(cx, cy + 4.5 * s, cx, cy + 12 * s, dark, 3.6 * s);
  Line(cx - 11.5 * s, cy, cx - 4.5 * s, cy, red, 2 * s);
  Line(cx + 4.5 * s, cy, cx + 11.5 * s, cy, red, 2 * s);
  Line(cx, cy - 11.5 * s, cx, cy - 4.5 * s, red, 2 * s);
  Line(cx, cy + 4.5 * s, cx, cy + 11.5 * s, red, 2 * s);
  Dot(cx, cy, 2 * s, red);
 }

 // Diamond glyph for confirmed FIA hideouts.
 protected void DrawHideoutGlyph(float cx, float cy)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float s = 1;
  if (ws) s = ws.DPIScale(1);
  int dark = Color.FromRGBA(18, 20, 24, 245).PackToInt();
  int white = Color.FromRGBA(250, 250, 250, 240).PackToInt();
  int fill = Color.FromRGBA(235, 82, 46, 250).PackToInt();
  float r0 = 10.5 * s;
  float r1 = 9 * s;
  float r2 = 7.5 * s;
  PolygonDrawCommand p0 = new PolygonDrawCommand();
  p0.m_iColor = dark;
  p0.m_Vertices = {cx, cy - r0, cx + r0, cy, cx, cy + r0, cx - r0, cy};
  AddPolygon(p0);
  PolygonDrawCommand p1 = new PolygonDrawCommand();
  p1.m_iColor = white;
  p1.m_Vertices = {cx, cy - r1, cx + r1, cy, cx, cy + r1, cx - r1, cy};
  AddPolygon(p1);
  PolygonDrawCommand p2 = new PolygonDrawCommand();
  p2.m_iColor = fill;
  p2.m_Vertices = {cx, cy - r2, cx + r2, cy, cx, cy + r2, cx - r2, cy};
  AddPolygon(p2);
  // Small dark roof/house mark inside.
  Line(cx - 3.5 * s, cy + 0.5 * s, cx, cy - 3.2 * s, dark, 2 * s);
  Line(cx, cy - 3.2 * s, cx + 3.5 * s, cy + 0.5 * s, dark, 2 * s);
  Line(cx - 2.4 * s, cy + 0.5 * s, cx - 2.4 * s, cy + 3.2 * s, dark, 1.8 * s);
  Line(cx + 2.4 * s, cy + 0.5 * s, cx + 2.4 * s, cy + 3.2 * s, dark, 1.8 * s);
 }

 // Hollow vehicle silhouette (chassis + cab + two wheels) for empty/abandoned vehicles.
 protected void DrawEmptyVehicles(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  if (!data || data.m_VehiclePositions.IsEmpty()) return;
  if ((data.m_bDeclOn && data.m_bDeclFar) || ACR_MapLayers.Off(ACR_MapLayers.VEHICLES)) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float s = 1;
  if (ws) s = ws.DPIScale(1);
  int dark = Color.FromRGBA(16, 20, 26, 240).PackToInt();
  int fill = Color.FromRGBA(24, 34, 46, 185).PackToInt();
  int edge = Color.FromRGBA(190, 224, 250, 250).PackToInt();
  foreach (vector pos : data.m_VehiclePositions)
  {
   int cx, cy;
   mapEntity.WorldToScreen(pos[0], pos[2], cx, cy, true);
   // Chassis + cab outline (hollow dark fill so it immediately reads as empty/uncrewed).
   float x0 = cx - 9 * s;
   float x1 = cx + 9 * s;
   float yTop = cy - 4.5 * s;
   float yMid = cy - 1 * s;
   float yBot = cy + 3 * s;
   PolygonDrawCommand back = new PolygonDrawCommand();
   back.m_iColor = dark;
   back.m_Vertices = {x0 - 1.4 * s, yMid - 1.2 * s, cx - 4.5 * s, yTop - 1.4 * s, cx + 4.5 * s, yTop - 1.4 * s, x1 + 1.4 * s, yMid - 1.2 * s, x1 + 1.4 * s, yBot + 1.4 * s, x0 - 1.4 * s, yBot + 1.4 * s};
   AddPolygon(back);
   PolygonDrawCommand body = new PolygonDrawCommand();
   body.m_iColor = fill;
   body.m_Vertices = {x0, yMid, cx - 4 * s, yTop, cx + 4 * s, yTop, x1, yMid, x1, yBot, x0, yBot};
   AddPolygon(body);
   LineDrawCommand outline = new LineDrawCommand();
   outline.m_iColor = edge;
   outline.m_fWidth = 1.9 * s;
   outline.m_bShouldEnclose = true;
   outline.m_Vertices = {x0, yMid, cx - 4 * s, yTop, cx + 4 * s, yTop, x1, yMid, x1, yBot, x0, yBot};
   m_Commands.Insert(outline);
   // Two hollow wheels underneath.
   float wy = yBot + 1.4 * s;
   float wxL = cx - 5.2 * s;
   float wxR = cx + 5.2 * s;
   Dot(wxL, wy, 3.2 * s, dark);
   Ring(wxL, wy, 2.3 * s, edge, 1.7 * s, 12);
   Dot(wxR, wy, 3.2 * s, dark);
   Ring(wxR, wy, 2.3 * s, edge, 1.7 * s, 12);
  }
 }

 protected void DrawZoneItem(SCR_MapEntity mapEntity, ACR_MapData data, int i)
 {
  vector pos = data.m_ZonePositions[i];
  int cx, cy, ex, ey;
  mapEntity.WorldToScreen(pos[0], pos[2], cx, cy, true);
  mapEntity.WorldToScreen(pos[0] + data.m_ZoneRadii[i], pos[2], ex, ey, true);
  float radius = Math.Max(Math.AbsFloat(ex - cx), 6);
  // A shaped minefield draws its outline instead (DrawMineShapes).
  if (ShapeAt(data, pos)) return;
  string label = "";
  if (i < data.m_ZoneLabels.Count()) label = data.m_ZoneLabels[i];
  int kind = ZoneKind(label);
  if (kind == 0)
  {
   Ring(cx, cy, radius, Color.FromRGBA(140, 204, 255, 140).PackToInt(), 2, 96);
   return;
  }
  if (kind == 1)
  {
   Ring(cx, cy, radius, Color.FromRGBA(184, 128, 255, 153).PackToInt(), 2, 64);
   return;
  }
  if (kind == 2)
  {
   // Dangerous road (FIA ambush): dashed amber-orange ring + warning triangle.
   DashedRing(cx, cy, radius, Color.FromRGBA(250, 128, 38, 238).PackToInt(), 2.6, 26);
   DrawWarningTriangle(cx, cy, Color.FromRGBA(248, 138, 38, 250).PackToInt());
   return;
  }
  if (kind == 3)
  {
   // Zombie danger area: dashed crimson-red ring + crimson warning triangle.
   DashedRing(cx, cy, radius, Color.FromRGBA(238, 62, 52, 238).PackToInt(), 2.6, 26);
   DrawWarningTriangle(cx, cy, Color.FromRGBA(235, 65, 55, 250).PackToInt());
   return;
  }
  if (kind == 4)
  {
   // Confirmed FIA hideout: solid red-orange ring + diamond house glyph.
   Ring(cx, cy, radius, Color.FromRGBA(238, 82, 46, 230).PackToInt(), 2.5, 48);
   DrawHideoutGlyph(cx, cy);
   return;
  }
  if (kind == 5)
  {
   // FIA activity (approximate intel lead): dashed amber-yellow ring + amber warning triangle.
   DashedRing(cx, cy, radius, Color.FromRGBA(242, 184, 46, 215).PackToInt(), 2.2, 24);
   DrawWarningTriangle(cx, cy, Color.FromRGBA(242, 184, 46, 245).PackToInt());
   return;
  }
  if (kind == 6)
  {
   // Friendly fire mission: solid bright red ring + crosshair glyph.
   Ring(cx, cy, radius, Color.FromRGBA(255, 38, 26, 242).PackToInt(), 3, 48);
   DrawFireMissionGlyph(cx, cy);
   return;
  }
  Ring(cx, cy, radius, Color.FromRGBA(255, 38, 26, 242).PackToInt(), 3, 48);
 }

 // Support danger areas, dangerous roads, hideouts, ranges and civilian zones.
 protected void DrawZones(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  foreach (int i, vector pos : data.m_ZonePositions) DrawZoneItem(mapEntity, data, i);
  // Subtle green leadership ring around the local commander's primary command unit.
  if (ACR_CommandPresence.On())
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
   IEntity main;
   if (pc) main = pc.GetMainEntity();
   if (main && ACR_Respawn.IsAlive(main))
   {
    vector coPos = main.GetOrigin();
    int ccx, ccy, cex, cey;
    mapEntity.WorldToScreen(coPos[0], coPos[2], ccx, ccy, true);
    mapEntity.WorldToScreen(coPos[0] + ACR_CommandPresence.COMMAND_RADIUS, coPos[2], cex, cey, true);
    float coRadius = Math.Max(Math.AbsFloat(cex - ccx), 6);
    Color coGreen = new Color(0.25, 0.95, 0.4, 0.45);
    Ring(ccx, ccy, coRadius, coGreen.PackToInt(), 1.5, 64);
   }
  }
 }

 // ---------------------------------------------------------------- shaped minefields (#200)
 // Whether a shaped minefield's middle is at 'pos' (its zone: the label stays, the circle doesn't).
 protected static bool ShapeAt(ACR_MapData data, vector pos)
 {
  int at = 0;
  foreach (int count : data.m_MineShapeCounts)
  {
   if (count < 1 || at + count > data.m_MineShapeCorners.Count()) return false;
   array<vector> corners = {};
   for (int k = 0; k < count; k++) corners.Insert(data.m_MineShapeCorners[at + k]);
   at += count;
   if (vector.DistanceXZ(ACR_MineShape.Middle(corners), pos) < 2) return true;
  }
  return false;
 }

 // Each shaped field: a faint red fill and a red outline.
 protected void DrawMineShapes(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  Color red = new Color(1, 0.15, 0.1, 0.95);
  Color fill = new Color(1, 0.15, 0.1, 0.16);
  int at = 0;
  foreach (int count : data.m_MineShapeCounts)
  {
   if (count < 3 || at + count > data.m_MineShapeCorners.Count()) break;
   array<float> screen = {};
   for (int k = 0; k < count; k++)
   {
    vector corner = data.m_MineShapeCorners[at + k];
    int sx, sy;
    mapEntity.WorldToScreen(corner[0], corner[2], sx, sy, true);
    screen.Insert(sx);
    screen.Insert(sy);
   }
   at += count;
   PolygonDrawCommand area = new PolygonDrawCommand();
   area.m_iColor = fill.PackToInt();
   area.m_Vertices = {};
   area.m_Vertices.Copy(screen);
   AddPolygon(area);
   LineDrawCommand edge = new LineDrawCommand();
   edge.m_iColor = red.PackToInt();
   edge.m_fWidth = 3;
   edge.m_bShouldEnclose = true;
   edge.m_Vertices = {};
   edge.m_Vertices.Copy(screen);
   m_Commands.Insert(edge);
  }
 }

 // The commander's minefield points so far: dots, the edges between them, dashed edges through the cursor
 // (where the next point would go) and a faint fill of what three or more already make; a ring on the last
 // point, the one to click again.
 protected void DrawMinePreview(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  int count = data.m_MineCorners.Count();
  if (count == 0) return;
  Color line = new Color(1, 0.55, 0.15, 0.95);
  Color fill = new Color(1, 0.55, 0.15, 0.18);
  array<float> screen = {};
  foreach (vector corner : data.m_MineCorners)
  {
   int sx, sy;
   mapEntity.WorldToScreen(corner[0], corner[2], sx, sy, true);
   screen.Insert(sx);
   screen.Insert(sy);
  }
  if (count >= 3)
  {
   array<vector> outline = {};
   outline.Copy(data.m_MineCorners);
   ACR_MineShape.Order(outline);
   PolygonDrawCommand area = new PolygonDrawCommand();
   area.m_iColor = fill.PackToInt();
   area.m_Vertices = {};
   foreach (vector o : outline)
   {
    int ox, oy;
    mapEntity.WorldToScreen(o[0], o[2], ox, oy, true);
    area.m_Vertices.Insert(ox);
    area.m_Vertices.Insert(oy);
   }
   AddPolygon(area);
  }
  for (int i = 0; i + 1 < count; i++) Line(screen[i * 2], screen[i * 2 + 1], screen[i * 2 + 2], screen[i * 2 + 3], line.PackToInt(), 3);
  int cx, cy;
  mapEntity.WorldToScreen(data.m_vMineCursor[0], data.m_vMineCursor[2], cx, cy, true);
  float lx = screen[count * 2 - 2];
  float ly = screen[count * 2 - 1];
  if (count < ACR_MineShape.MOST_CORNERS)
  {
   DashedLine(lx, ly, cx, cy, line.PackToInt(), 2);
   if (count >= 2) DashedLine(cx, cy, screen[0], screen[1], line.PackToInt(), 2);
  }
  for (int d = 0; d < count; d++) Dot(screen[d * 2], screen[d * 2 + 1], 4, line.PackToInt());
  Ring(lx, ly, 9, line.PackToInt(), 2, 16);
 }

 // ---------------------------------------------------------------- pending, box
 // Drawn faint the moment you click, until the server's order lines catch up.
 protected void DrawPending(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  int now = System.GetTickCount();
  Color faint = new Color(1, 1, 1, 0.45);
  for (int i = data.m_PendingTo.Count() - 1; i >= 0; i--)
  {
   if (now > data.m_PendingUntil[i])
   {
    data.m_PendingFrom.RemoveOrdered(i);
    data.m_PendingTo.RemoveOrdered(i);
    data.m_PendingUntil.RemoveOrdered(i);
    continue;
   }
   int x0, y0, x1, y1;
   mapEntity.WorldToScreen(data.m_PendingFrom[i][0], data.m_PendingFrom[i][2], x0, y0, true);
   mapEntity.WorldToScreen(data.m_PendingTo[i][0], data.m_PendingTo[i][2], x1, y1, true);
   Line(x0, y0, x1, y1, faint.PackToInt(), 2);
  }
 }

 static const float HEADING_FROM_PX = 16; // past the contact's symbol
 static const float HEADING_TO_PX = 42;

 // Spotted enemies seen moving: a red arrow from the symbol the way they were going.
 protected void DrawContactHeadings(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  foreach (int i, vector pos : data.m_ContactArrowPos)
  {
   float radians = data.m_ContactArrowHeading[i] * Math.DEG2RAD;
   vector ahead = pos + Vector(Math.Sin(radians), 0, Math.Cos(radians)) * 100;
   int x0, y0, x1, y1;
   mapEntity.WorldToScreen(pos[0], pos[2], x0, y0, true);
   mapEntity.WorldToScreen(ahead[0], ahead[2], x1, y1, true);
   float dx = x1 - x0;
   float dy = y1 - y0;
   float length = Math.Sqrt(dx * dx + dy * dy);
   if (length < 0.01) continue;
   dx = dx / length;
   dy = dy / length;
   float alpha = data.m_ContactArrowAlpha[i];
   Color shadeColor = new Color(0, 0, 0, 0.5 * alpha);
   Color redColor = new Color(1, 0.25, 0.2, alpha);
   float sx = x0 + dx * HEADING_FROM_PX;
   float sy = y0 + dy * HEADING_FROM_PX;
   float tx = x0 + dx * HEADING_TO_PX;
   float ty = y0 + dy * HEADING_TO_PX;
   float angle = Math.Atan2(dy, dx);
   for (int pass = 0; pass < 2; pass++)
   {
    int color = shadeColor.PackToInt();
    float width = 5;
    if (pass == 1)
    {
     color = redColor.PackToInt();
     width = 2.5;
    }
    Line(sx, sy, tx, ty, color, width);
    for (int side = -1; side <= 1; side += 2)
    {
     float back = angle + Math.PI + side * 0.55;
     Line(tx, ty, tx + Math.Cos(back) * 9, ty + Math.Sin(back) * 9, color, width);
    }
   }
  }
 }

 // Drag to aim: an arrow from where the order goes to where it will face.
 protected void DrawAim(SCR_MapEntity mapEntity, vector from, vector to)
 {
  int x0, y0, x1, y1;
  mapEntity.WorldToScreen(from[0], from[2], x0, y0, true);
  mapEntity.WorldToScreen(to[0], to[2], x1, y1, true);
  Color gold = new Color(1, 0.85, 0.2, 0.95);
  int color = gold.PackToInt();
  Line(x0, y0, x1, y1, color, 3);
  float angle = Math.Atan2(y1 - y0, x1 - x0);
  for (int side = -1; side <= 1; side += 2)
  {
   float back = angle + Math.PI + side * 0.5;
   Line(x1, y1, x1 + Math.Cos(back) * 14, y1 + Math.Sin(back) * 14, color, 3);
  }
 }

 protected void DrawBox(SCR_MapEntity mapEntity, vector start)
 {
  float wx, wz;
  mapEntity.GetMapCursorWorldPosition(wx, wz);
  int x0, y0, x1, y1;
  mapEntity.WorldToScreen(start[0], start[2], x0, y0, true);
  mapEntity.WorldToScreen(wx, wz, x1, y1, true);
  LineDrawCommand box = new LineDrawCommand();
  Color white = new Color(1, 1, 1, 0.9);
  box.m_iColor = white.PackToInt();
  box.m_fWidth = 1.5;
  box.m_bShouldEnclose = true;
  box.m_Vertices = {x0, y0, x1, y0, x1, y1, x0, y1};
  m_Commands.Insert(box);
 }
}
