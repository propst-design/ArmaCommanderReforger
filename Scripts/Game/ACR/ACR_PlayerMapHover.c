// The command map's hover card (ACR_MapCard) and strength warnings (playtest 30 Oct).
// - The card: the marker under the cursor (SCR_MapMarkerWidgetComponent.ACR_TellHover) - a squad's
//   name and kind, what it is doing and where to, its health, ammo and fuel, who gave its order and
//   when; a base's details. Not while the list of a "×N"'s squads is open.
// - Warnings: the health and ammo bars under a squad's icon show only near, and only for a squad
//   with its own icon. A squad that is hurt (HP under WARN_HP) or short of ammo (under WARN_AMMO)
//   now also gets a dot at its icon's top right, zoomed out too, and an icon that stands for several
//   squads ("×N") or a base's count gets the worst of theirs: orange, red under WARN_BAD.
// (Named to load after ACR_PlayerMapGroups.c and ACR_PlayerMapDeclutter.c, whose methods it uses.)
modded class SCR_PlayerController
{
 protected ref ACR_MapCard m_ACR_Card;
 protected ACR_UnitMarker m_ACR_HoverMarker;

 void ACR_HoverMarker(ACR_UnitMarker marker, bool on)
 {
  if (on) m_ACR_HoverMarker = marker;
  else if (m_ACR_HoverMarker == marker) m_ACR_HoverMarker = null;
 }

 override protected void ACR_HoverBase(ACR_UnitMarker marker, bool on)
 {
  ACR_HoverMarker(marker, on);
 }

 override protected void ACR_UpdatePicker()
 {
  super.ACR_UpdatePicker();
  ACR_UpdateCard();
 }

 override void ACR_UpdateSetupMap()
 {
  super.ACR_UpdateSetupMap();
  ACR_UpdateCard();
 }

 override protected void ACR_CloseSetupMap()
 {
  m_ACR_HoverMarker = null;
  if (m_ACR_Card) m_ACR_Card.Destroy();
  m_ACR_Card = null;
  super.ACR_CloseSetupMap();
 }

 // The map's click (ACR_OnMapClick) and closing (ACR_ClosePanel) close the pickers: the card too.
 override protected void ACR_ClosePicker()
 {
  if (m_ACR_Card) m_ACR_Card.Destroy();
  m_ACR_Card = null;
  super.ACR_ClosePicker();
 }

 protected void ACR_UpdateCard()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  ACR_UnitMarker marker = m_ACR_HoverMarker;
  bool setupOk = !ACR_SetupClient.IsActive() || (marker && marker.m_BaseIndex >= 0 && ACR_MapDeclutter.BasesOn());
  bool want = marker && mapEntity && mapEntity.IsOpen() && mapEntity.GetMapMenuRoot() && m_ACR_Data && !ACR_SquadPickerOpen() && setupOk;
  if (!want)
  {
   if (m_ACR_Card) m_ACR_Card.Hide();
   return;
  }
  string title;
  array<string> lines = {};
  if (marker.m_BaseIndex >= 0) ACR_BaseCard(marker, title, lines);
  else if (marker.m_iVehicleIndex >= 0)
  {
   if (!ACR_VehicleCard(marker.m_iVehicleIndex, title, lines))
   {
    if (m_ACR_Card) m_ACR_Card.Hide();
    return;
   }
  }
  else if (marker.m_bZone)
  {
   if (!ACR_ZoneCard(marker, title, lines))
   {
    if (m_ACR_Card) m_ACR_Card.Hide();
    return;
   }
  }
  else if (!ACR_SquadCard(marker.m_GroupId, title, lines))
  {
   if (m_ACR_Card) m_ACR_Card.Hide();
   return;
  }
  if (!m_ACR_Card || !m_ACR_Card.m_Root)
  {
   m_ACR_Card = new ACR_MapCard();
   m_ACR_Card.Create(mapEntity.GetMapMenuRoot());
  }
  int mx, my;
  WidgetManager.GetMousePos(mx, my);
  m_ACR_Card.Show(title, lines, mx, my);
 }

 // "2nd Squad · infantry"; "Moving to Crossroads · 420 m"; "HP 80% · ammo 35%"; "Ordered by Yung · 2 min ago".
 protected bool ACR_SquadCard(RplId groupId, out string title, notnull array<string> lines)
 {
  int g = m_ACR_Data.m_GroupIds.Find(groupId);
  if (g < 0) return false;
  string name = "Squad";
  if (g < m_ACR_Data.m_GroupNames.Count()) name = m_ACR_Data.m_GroupNames[g];
  title = name;
  string kind = ACR_SquadPicker.TypeWord(ACR_GroupIcons(groupId));
  if (!kind.IsEmpty()) title += "  ·  " + kind;
  vector at = m_ACR_Data.LeaderPos(g);
  // What it is doing and where to.
  int o = m_ACR_Data.m_OrderGroups.Find(groupId);
  string doing = "No order: holding where it is";
  if (o >= 0 && o < m_ACR_Data.m_OrderKinds.Count() && o < m_ACR_Data.m_OrderTargets.Count())
  {
   vector target = m_ACR_Data.m_OrderTargets[o];
   int distance = vector.DistanceXZ(at, target);
   string place = ACR_PlaceNear(target);
   switch (m_ACR_Data.m_OrderKinds[o])
   {
    case ACR_EOrderKind.DEFEND: doing = "Defending " + place; break;
    case ACR_EOrderKind.GET_IN: doing = "Getting in, " + place; break;
    case ACR_EOrderKind.GET_OUT: doing = "Getting out, " + place; break;
    case ACR_EOrderKind.HUNT: doing = "Hunting, " + place; break;
    default: doing = "Moving to " + place; break;
   }
   if (distance >= 30) doing += string.Format("  ·  %1 m", distance);
  }
  if (g < m_ACR_Data.m_DeclMark.Count() && m_ACR_Data.m_DeclMark[g] == ACR_ESquadMark.FIGHTING) doing += "  ·  in contact";
  lines.Insert(doing);
  // Health, ammo, fuel.
  int s = m_ACR_Data.m_StatusGroups.Find(groupId);
  if (s >= 0 && s * 6 + 2 < m_ACR_Data.m_Status.Count())
  {
   string strength = ACR_SquadPicker.Strength(m_ACR_Data.m_Status[s * 6], m_ACR_Data.m_Status[s * 6 + 1]);
   int fuel = m_ACR_Data.m_Status[s * 6 + 2];
   if (fuel >= 0) strength += string.Format("  ·  fuel %1%%", fuel);
   lines.Insert(strength);
  }
  // Who gave the order, and when.
  int age;
  int by = m_ACR_Data.OrderedBy(groupId, age);
  string who = "the AI commander";
  if (by > 0) who = m_ACR_Data.TagOf(by);
  if (o >= 0 && !who.IsEmpty())
  {
   string when = "";
   if (age >= 0 && age < 60) when = string.Format("  ·  %1 s ago", age);
   else if (age >= 60) when = string.Format("  ·  %1 min ago", age / 60);
   lines.Insert("Ordered by " + who + when);
  }
  return true;
 }

 // A base's details, one a line, as its caption grew to on hover before.
 protected void ACR_BaseCard(ACR_UnitMarker marker, out string title, notnull array<string> lines)
 {
  title = ACR_MapDeclutter.ShortName(marker.m_Label);
  // What its count stands for, first: the details below may run out of lines.
  int b = marker.m_BaseIndex;
  if (b >= 0 && b < m_ACR_Data.m_DeclChipOwn.Count() && m_ACR_Data.m_bDeclOn && m_ACR_Data.m_DeclChipOwn[b] + m_ACR_Data.m_DeclChipEnemy[b] > 0)
   lines.Insert(ACR_MapDeclutter.ChipText(m_ACR_Data.m_DeclChipOwn[b], m_ACR_Data.m_DeclChipEnemy[b]) + " here  ·  click for the list");
  array<string> parts = {};
  marker.m_BaseHover.Split("\n", parts, true);
  foreach (int i, string part : parts)
  {
   if (i == 0 || lines.Count() >= ACR_MapCard.LINES) continue;
   lines.Insert(part);
  }
 }

 // An empty / abandoned vehicle's hover card: vehicle type, assigned crew (if any), sighting age,
 // and how to recover it.
 protected bool ACR_VehicleCard(int v, out string title, notnull array<string> lines)
 {
  if (v < 0 || v >= m_ACR_VehicleNames.Count()) return false;
  string raw = m_ACR_VehicleNames[v];
  string crew = "";
  int comma = raw.IndexOf(",");
  if (comma >= 0)
  {
   crew = raw.Substring(comma + 1, raw.Length() - comma - 1);
   crew.TrimInPlace();
   raw = raw.Substring(0, comma);
  }
  if (raw.StartsWith("#")) raw = WidgetManager.Translate(raw);
  title = raw + "  ·  Empty vehicle";
  if (crew.StartsWith("crew: ")) lines.Insert("Assigned crew: " + crew.Substring(6, crew.Length() - 6));
  else if (!crew.IsEmpty()) lines.Insert(crew);
  else lines.Insert("Abandoned  ·  no squad assigned");
  int age = 0;
  if (v < m_ACR_VehicleAges.Count()) age = m_ACR_VehicleAges[v] + ACR_MapData.Since(m_ACR_VehiclesAt);
  if (age <= 20) lines.Insert("In sight now");
  else if (age < 60) lines.Insert(string.Format("Last seen %1 s ago", age));
  else lines.Insert(string.Format("Last seen %1 min ago", age / 60));
  lines.Insert("Select a squad and right-click nearby to board");
  return true;
 }

 // A zone marker's hover card: explains what the ring means and what the commander can do about it.
 protected bool ACR_ZoneCard(ACR_UnitMarker marker, out string title, notnull array<string> lines)
 {
  string raw = marker.m_sZoneRaw;
  if (raw.IsEmpty()) return false;
  int kind = ACR_OrderDisplay.ZoneKind(raw);
  if (kind == 2)
  {
   title = "FIA ambush  ·  Dangerous road";
   lines.Insert("Friendly losses to FIA along this road");
   lines.Insert("Supply runs avoid this area until cleared or expired");
   lines.Insert("Send a combat squad to sweep and clear the road");
   return true;
  }
  if (kind == 3)
  {
   title = "Zombie area  ·  Danger zone";
   lines.Insert("Infected spotted or engaged in this area");
   lines.Insert("Supply runs avoid this area until cleared or expired");
   lines.Insert("Sweep with a combat squad to clear");
   return true;
  }
  if (kind == 4)
  {
   title = "FIA hideout";
   lines.Insert("Confirmed insurgent cell base");
   lines.Insert("Destroy it to stop ambushes in this sector");
   return true;
  }
  if (kind == 5)
  {
   title = "FIA activity  ·  Intel lead";
   lines.Insert("Approximate area of an insurgent hideout");
   lines.Insert("More intel or contacts will narrow the ring");
   return true;
  }
  if (kind == 6)
  {
   title = raw;
   lines.Insert("Friendly fire mission danger area");
   lines.Insert("Keep friendly squads clear of the ring");
   return true;
  }
  string clean = raw;
  if (kind == 0 || kind == 1) clean = raw.Substring(1, raw.Length() - 1);
  title = clean;
  if (kind == 0) lines.Insert("Tactical range / coverage ring");
  else if (kind == 1) lines.Insert("Recent civilian presence reported here");
  else lines.Insert("Active map zone");
  return true;
 }

 // The base nearest 'pos' within PLACE_NEAR m ("Crossroads"), else "the map point".
 static const float PLACE_NEAR = 700;
 protected string ACR_PlaceNear(vector pos)
 {
  string best = "the map point";
  float bestDistance = PLACE_NEAR;
  foreach (int b, vector base : m_ACR_Data.m_BasePositions)
  {
   float d = vector.DistanceXZ(base, pos);
   if (d >= bestDistance || b >= m_ACR_Data.m_BaseNames.Count()) continue;
   bestDistance = d;
   best = ACR_MapDeclutter.ShortName(m_ACR_Data.m_BaseNames[b]);
  }
  return best;
 }

 // Tests.
 bool ACR_CardShownForTest() { return m_ACR_Card && m_ACR_Card.Shown(); }
 bool ACR_HoverSquadForTest(RplId groupId)
 {
  foreach (ACR_UnitMarker marker : m_ACR_GroupMarkers)
  {
   if (marker && marker.m_GroupId == groupId)
   {
    ACR_HoverMarker(marker, true);
    return true;
   }
  }
  return false;
 }
 void ACR_HoverClearForTest() { m_ACR_HoverMarker = null; }
}

modded class ACR_OrderDisplay
{
 static const int WARN_HP = 60;
 static const int WARN_AMMO = 35;
 static const int WARN_BAD = 30;   // HP or ammo under this: red
 static int s_iWarnDots;           // dots drawn (tests)

 override protected void DrawStatusBars(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  super.DrawStatusBars(mapEntity, data);
  if (data.m_bDeclOn) DrawWarnings(mapEntity, data);
 }

 // 0 fine, 1 orange, 2 red.
 static int WarnLevel(int health, int ammo)
 {
  if ((health >= 0 && health < WARN_BAD) || (ammo >= 0 && ammo < WARN_BAD)) return 2;
  if ((health >= 0 && health < WARN_HP) || (ammo >= 0 && ammo < WARN_AMMO)) return 1;
  return 0;
 }

 // A dot at the top right of each icon shown whose squad, or a squad it stands for, is in trouble.
 protected void DrawWarnings(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  // The worst level per icon shown (the "×N" icon's group index), near only for clusters (lone
  // squads' bars show near); and per base's count, drawn on the count.
  map<int, int> worst = new map<int, int>();
  map<int, int> worstChip = new map<int, int>();
  foreach (int i, RplId groupId : data.m_StatusGroups)
  {
   if (i * 6 + 1 >= data.m_Status.Count()) break;
   int level = WarnLevel(data.m_Status[i * 6], data.m_Status[i * 6 + 1]);
   if (level == 0) continue;
   int g = data.m_GroupIds.Find(groupId);
   if (g < 0 || data.DeclHidden(g)) continue;
   if (g < data.m_DeclBaseOf.Count() && data.m_DeclBaseOf[g] >= 0 && data.ShownByOther(g))
   {
    int chipHad;
    if (!worstChip.Find(data.m_DeclBaseOf[g], chipHad) || chipHad < level) worstChip.Set(data.m_DeclBaseOf[g], level);
    continue;
   }
   int shown = g;
   if (data.ShownByOther(g)) shown = ACR_ShownBy(data, g);
   if (shown < 0) continue;
   // A lone squad near has its bars: no dot needed.
   if (shown == g && !data.m_bDeclFar && (g >= data.m_DeclCount.Count() || data.m_DeclCount[g] < 2)) continue;
   int had;
   if (worst.Find(shown, had) && had >= level) continue;
   worst.Set(shown, level);
  }
  int orange = Color.FromRGBA(255, 170, 40, 255).PackToInt();
  int red = Color.FromRGBA(240, 60, 50, 255).PackToInt();
  int edge = Color.FromRGBA(20, 22, 26, 255).PackToInt();
  foreach (int s, int lvl : worst)
  {
   if (s >= data.m_LeaderPositions.Count()) continue;
   vector pos = data.LeaderPos(s);
   int sx, sy;
   mapEntity.WorldToScreen(pos[0], pos[2], sx, sy, true);
   float x = sx + Px(13);
   float y = sy - Px(13);
   int fill = orange;
   if (lvl >= 2) fill = red;
   Dot(x, y, Px(6), edge);
   Dot(x, y, Px(4.5), fill);
   s_iWarnDots++;
  }
  // On a base's count: its top corner away from the dot.
  foreach (int b, int chipLevel : worstChip)
  {
   if (b * 4 + 3 >= data.m_DeclChipRect.Count() || b * 2 >= data.m_DeclDots.Count()) continue;
   float x0 = data.m_DeclChipRect[b * 4];
   float x1 = data.m_DeclChipRect[b * 4 + 2];
   float corner = x1;
   if ((x0 + x1) * 0.5 < data.m_DeclDots[b * 2]) corner = x0;
   int chipFill = orange;
   if (chipLevel >= 2) chipFill = red;
   Dot(corner, data.m_DeclChipRect[b * 4 + 1], Px(6), edge);
   Dot(corner, data.m_DeclChipRect[b * 4 + 1], Px(4.5), chipFill);
   s_iWarnDots++;
  }
 }

 // The group whose icon stands for squad g in its pile, -1 none.
 protected int ACR_ShownBy(ACR_MapData data, int g)
 {
  if (g >= data.m_DeclPile.Count()) return -1;
  return data.m_DeclPile[g];
 }
}
