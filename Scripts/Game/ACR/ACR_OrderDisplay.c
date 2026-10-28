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
 static Color BaseColor(string owner, float alpha)
 {
  if (owner.IsEmpty()) return new Color(0.85, 0.72, 0.1, alpha);
  Faction mine = SCR_FactionManager.SGetLocalPlayerFaction();
  if (mine && mine.GetFactionKey() == owner) return new Color(0.2, 0.5, 1.0, alpha);
  // FIA is only ever a third party here (pockets are off when FIA plays a main side).
  if (owner == ACR_Pockets.FACTION) return new Color(0.2, 0.7, 0.25, alpha);
  return new Color(0.9, 0.15, 0.15, alpha);
 }

 // ---------------------------------------------------------------- frame
 void Draw(SCR_MapEntity mapEntity, ACR_MapData data, bool boxing, vector boxStart)
 {
  if (!m_Canvas) return;
  m_Commands.Clear();
  DrawBaseAreas(mapEntity, data);
  DrawZones(mapEntity, data);
  DrawOrders(mapEntity, data);
  DrawOtherSelections(mapEntity, data);
  DrawStatusBars(mapEntity, data);
  DrawPending(mapEntity, data);
  if (boxing) DrawBox(mapEntity, boxStart);
  if (data.m_bAiming) DrawAim(mapEntity, data.m_vAimFrom, data.m_vAimTo);
  ACR_PingDisplay.DrawOnMap(this, mapEntity, data);
  m_Canvas.SetDrawCommands(m_Commands);
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
    marker.SetType(SCR_EMapMarkerType.SIMPLE);
    marker.SetMarkerConfigID(1);
    marker.SetCanBeRemovedByOwner(false);
    m_TagMarkers.Insert(marker);
    manager.InsertStaticMarker(marker, true);
    marker.SetMarkerOwnerID(-1);
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
   if (data.m_BaseContested[i] == 1) width = 4;
   Ring(cx, cy, radius, BaseColor(data.m_BaseOwners[i], 0.9).PackToInt(), width, 48);
  }
 }

 // Support danger areas (friendly fire missions): red rings.
 protected void DrawZones(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  Color red = new Color(1, 0.15, 0.1, 0.95);
  Color reach = new Color(0.55, 0.8, 1, 0.55);
  foreach (int i, vector pos : data.m_ZonePositions)
  {
   int cx, cy, ex, ey;
   mapEntity.WorldToScreen(pos[0], pos[2], cx, cy, true);
   mapEntity.WorldToScreen(pos[0] + data.m_ZoneRadii[i], pos[2], ex, ey, true);
   float radius = Math.Max(Math.AbsFloat(ex - cx), 6);
   // A mortar team's range ("~" label): thin and pale.
   if (i < data.m_ZoneLabels.Count() && data.m_ZoneLabels[i].StartsWith("~"))
   {
    Ring(cx, cy, radius, reach.PackToInt(), 2, 96);
    continue;
   }
   Ring(cx, cy, radius, red.PackToInt(), 3, 48);
  }
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
