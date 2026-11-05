// The dark map's own labels (the moon button: ACR_MapDeclutter.Dark). The map's picture is one
// image per terrain (its map entity's "Satellite background image", e.g. EveronRasterized.edds),
// and script can't recolour it: the props configs only colour what the map draws from data, and
// on Everon turning every one of them dark changed nothing on screen (30 Oct). So the dark map
// stays a film over the picture, and what the film buried, the map's own place names (dark text
// on a dark map), is drawn again above it in light text, the map's own hidden meanwhile
// (MapDescriptorProps.SetTextVisible): cities and seas at any zoom; towns, villages, islands and
// lakes from mid zoom; settlements, local names, hills, ridges, valleys and streams close in. A
// name within NAME_CLEAR m of a base gives way while the calm map draws that base's name, which then
// stands for the place. Zoomed out the calm map names only some bases and leaves the rest bare dots,
// and then the place's own name shows (#203: on West Zagoria, with a base in nearly every town,
// leaving every such name out took almost every town's name off the map). (The grid's numbers stay
// readable through the film.)
// (Named to load after ACR_PlayerMapDeclutter.c, whose methods it uses.)
modded class SCR_PlayerController
{
 static const float NAME_CLEAR = 300;   // m from a base: its name stands for the place
 static const float MID_SPAN = 7000;    // m across the screen: towns and villages from here in
 static const float NEAR_SPAN = 2500;   // m across the screen: local names from here in

 protected Widget m_wACR_DarkLabels;
 protected ref array<TextWidget> m_aACR_PlaceTexts = {};
 protected ref array<vector> m_aACR_PlacePos = {};
 protected ref array<int> m_aACR_PlaceRank = {};
 protected ref array<int> m_aACR_PlaceBase = {}; // the base within NAME_CLEAR of each (the nearest), -1 none
 protected int m_iACR_PlaceBases = -1;  // the base count m_aACR_PlaceBase was found for
 protected int m_iACR_PlacesShown;      // tests
 protected int m_iACR_PlacesForBases;   // tests: names shown for a base drawn as a bare dot
 protected bool m_bACR_NamesHidden;

 override void ACR_UpdateMap()
 {
  super.ACR_UpdateMap();
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.IsOpen() || !m_ACR_Panel || !m_ACR_Data) return;
  ACR_DarkLabels(mapEntity, ACR_MapDeclutter.On() && ACR_MapDeclutter.Dark());
 }

 override protected void ACR_ClosePanel()
 {
  if (m_wACR_DarkLabels) m_wACR_DarkLabels.RemoveFromHierarchy();
  m_wACR_DarkLabels = null;
  m_aACR_PlaceTexts.Clear();
  m_aACR_PlacePos.Clear();
  m_aACR_PlaceRank.Clear();
  m_aACR_PlaceBase.Clear();
  m_iACR_PlaceBases = -1;
  m_bACR_NamesHidden = false;
  super.ACR_ClosePanel();
 }

 protected void ACR_DarkLabels(SCR_MapEntity mapEntity, bool want)
 {
  if (!want)
  {
   if (m_wACR_DarkLabels && m_wACR_DarkLabels.IsVisible()) m_wACR_DarkLabels.SetVisible(false);
   ACR_HideMapNames(mapEntity, false);
   m_iACR_PlacesShown = 0;
   m_iACR_PlacesForBases = 0;
   return;
  }
  if (!m_wACR_DarkLabels && !ACR_NewDarkLabels(mapEntity)) return;
  if (!m_wACR_DarkLabels.IsVisible()) m_wACR_DarkLabels.SetVisible(true);
  ACR_HideMapNames(mapEntity, true);
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float zoom = mapEntity.GetCurrentZoom();
  float span = 100000;
  if (zoom > 0) span = ws.GetWidth() / zoom;
  int maxRank = 0;
  if (span < MID_SPAN) maxRank = 1;
  if (span < NEAR_SPAN) maxRank = 2;
  int w = ws.GetWidth();
  int h = ws.GetHeight();
  // Each name's base, found again whenever the bases change (a FOB built or lost).
  array<vector> bases = m_ACR_Data.m_BasePositions;
  if (m_iACR_PlaceBases != bases.Count())
  {
   m_iACR_PlaceBases = bases.Count();
   m_aACR_PlaceBase.Clear();
   foreach (vector at : m_aACR_PlacePos)
   {
    m_aACR_PlaceBase.Insert(ACR_MapDeclutter.NearestBase(at, bases, NAME_CLEAR));
   }
  }
  m_iACR_PlacesShown = 0;
  m_iACR_PlacesForBases = 0;
  foreach (int i, TextWidget text : m_aACR_PlaceTexts)
  {
   if (!text) continue;
   int base = -1;
   if (i < m_aACR_PlaceBase.Count()) base = m_aACR_PlaceBase[i];
   // Near a base, only while the calm map draws no name for it (its layout's m_DeclNames, this frame).
   bool show = m_aACR_PlaceRank[i] <= maxRank && ACR_MapDeclutter.PlaceNameShows(base, m_ACR_Data.m_DeclNames);
   int sx, sy;
   if (show)
   {
    vector pos = m_aACR_PlacePos[i];
    mapEntity.WorldToScreen(pos[0], pos[2], sx, sy, true);
    show = sx > -100 && sy > -40 && sx < w + 100 && sy < h + 40;
   }
   if (text.IsVisible() != show) text.SetVisible(show);
   if (!show) continue;
   FrameSlot.SetPos(text, ws.DPIUnscale(sx), ws.DPIUnscale(sy));
   m_iACR_PlacesShown++;
   if (base >= 0) m_iACR_PlacesForBases++;
  }
 }

 // The labels' layer: in the map's frame over the dark film (ACR_DeclTint, -900), under the
 // markers; the names read once from the map's data.
 protected bool ACR_NewDarkLabels(SCR_MapEntity mapEntity)
 {
  Widget root = mapEntity.GetMapMenuRoot();
  Widget frame;
  if (root) frame = root.FindAnyWidget("MapFrame");
  if (!frame) return false;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_wACR_DarkLabels = ws.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), -800, frame);
  if (!m_wACR_DarkLabels) return false;
  FrameSlot.SetAnchorMin(m_wACR_DarkLabels, 0, 0);
  FrameSlot.SetAnchorMax(m_wACR_DarkLabels, 1, 1);
  FrameSlot.SetOffsets(m_wACR_DarkLabels, 0, 0, 0, 0);
  m_wACR_DarkLabels.SetZOrder(-800);
  array<int> types = ACR_NameTypes();
  foreach (int type : types)
  {
   array<MapItem> items = {};
   mapEntity.GetByType(items, type);
   foreach (MapItem item : items)
   {
    if (!item) continue;
    string name = item.GetDisplayName();
    if (name.IsEmpty()) continue;
    vector pos = item.GetPos();
    TextWidget text = ACR_PlaceText(name, type);
    if (!text) continue;
    m_aACR_PlaceTexts.Insert(text);
    m_aACR_PlacePos.Insert(pos);
    m_aACR_PlaceRank.Insert(ACR_PlaceRank(type));
   }
  }
  return true;
 }

 // 0 at any zoom, 1 from mid zoom, 2 close in.
 protected static int ACR_PlaceRank(int type)
 {
  if (type == EMapDescriptorType.MDT_NAME_CITY || type == EMapDescriptorType.MDT_NAME_SEA_MAJOR) return 0;
  if (type == EMapDescriptorType.MDT_NAME_TOWN || type == EMapDescriptorType.MDT_NAME_VILLAGE || type == EMapDescriptorType.MDT_NAME_ISLAND || type == EMapDescriptorType.MDT_NAME_WATER_MAJOR || type == EMapDescriptorType.MDT_NAME_SEA_MINOR) return 1;
  return 2;
 }

 // Places in near-white, water in blue, the lie of the land (hills, ridges, valleys) in a warm grey;
 // bigger for the bigger places. The name may be a string-table key: SetTextFormat translates it.
 protected TextWidget ACR_PlaceText(string name, int type)
 {
  Color color = ACR_TwoICStyle.Rgb(226, 229, 233, 0.92);
  int size = 16;
  bool heading = false;
  if (type == EMapDescriptorType.MDT_NAME_SEA_MAJOR || type == EMapDescriptorType.MDT_NAME_SEA_MINOR || type == EMapDescriptorType.MDT_NAME_WATER_MAJOR || type == EMapDescriptorType.MDT_NAME_WATER_MINOR)
   color = ACR_TwoICStyle.Rgb(122, 172, 226, 0.9);
  if (type == EMapDescriptorType.MDT_NAME_HILL || type == EMapDescriptorType.MDT_NAME_RIDGE || type == EMapDescriptorType.MDT_NAME_VALLEY)
   color = ACR_TwoICStyle.Rgb(196, 184, 156, 0.85);
  if (type == EMapDescriptorType.MDT_NAME_CITY || type == EMapDescriptorType.MDT_NAME_SEA_MAJOR)
  {
   size = 22;
   heading = true;
  }
  else if (type == EMapDescriptorType.MDT_NAME_TOWN || type == EMapDescriptorType.MDT_NAME_VILLAGE)
  {
   size = 18;
   heading = true;
  }
  TextWidget text = ACR_TwoICStyle.Text(m_wACR_DarkLabels, "", color, size, heading);
  if (!text) return null;
  text.SetTextFormat(name);
  text.SetFlags(WidgetFlags.IGNORE_CURSOR);
  text.SetTextWrapping(false);
  FrameSlot.SetAlignment(text, 0.5, 0.5);
  FrameSlot.SetSizeToContent(text, true);
  text.SetVisible(false);
  return text;
 }

 // The map's own place names off while ours show (and on again), on every layer.
 protected void ACR_HideMapNames(SCR_MapEntity mapEntity, bool hide)
 {
  if (hide == m_bACR_NamesHidden) return;
  m_bACR_NamesHidden = hide;
  array<int> types = ACR_NameTypes();
  for (int i = 0; i < mapEntity.LayerCount(); i++)
  {
   MapLayer layer = mapEntity.GetLayer(i);
   if (!layer) continue;
   foreach (int type : types)
   {
    MapDescriptorProps props = layer.GetPropsFor(EFactionMapID.UNKNOWN, type);
    if (props) props.SetTextVisible(!hide);
   }
  }
 }

 protected static array<int> ACR_NameTypes()
 {
  return {EMapDescriptorType.MDT_NAME_CITY, EMapDescriptorType.MDT_NAME_SEA_MAJOR, EMapDescriptorType.MDT_NAME_TOWN, EMapDescriptorType.MDT_NAME_VILLAGE, EMapDescriptorType.MDT_NAME_ISLAND, EMapDescriptorType.MDT_NAME_WATER_MAJOR, EMapDescriptorType.MDT_NAME_SEA_MINOR, EMapDescriptorType.MDT_NAME_SETTLEMENT, EMapDescriptorType.MDT_NAME_LOCAL, EMapDescriptorType.MDT_NAME_HILL, EMapDescriptorType.MDT_NAME_RIDGE, EMapDescriptorType.MDT_NAME_VALLEY, EMapDescriptorType.MDT_NAME_WATER_MINOR, EMapDescriptorType.MDT_NAME_GENERIC};
 }

 // Tests.
 int ACR_DarkPlacesShownForTest() { return m_iACR_PlacesShown; }
 int ACR_DarkPlacesForBasesForTest() { return m_iACR_PlacesForBases; }
 bool ACR_MapNamesHiddenForTest() { return m_bACR_NamesHidden; }
}
