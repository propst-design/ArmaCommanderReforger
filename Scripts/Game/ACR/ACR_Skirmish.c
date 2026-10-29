// Skirmish size (5 Oct; Jacob: the whole island is too big as the only option). A world can
// carry ACR_Skirmish layouts: a half of the map or a small flashpoint of about five bases
// round one hotspot, each with its own HQ per side. The host picks the size (-acrSize
// full|half|flashpoint) and optionally one layout by name (-acrLayout, else one of that size
// at random). On the server's first frame the chosen layout:
//  - retires every base not in it (off the map and out of every base list),
//  - makes its HQs the sides' owned bases (garrison as the old HQ's); other in-play bases
//    that started owned turn neutral,
//  - moves each side's starting units and player spawn points to its new HQ,
//  - drops helipads of retired bases; a side left without a pad gets the off-map helicopters.
// Full size (or no layout of that size in the world) plays the world as placed.
enum ACR_ESkirmishSize
{
 HALF,
 FLASHPOINT
}

class ACR_SkirmishClass : GenericEntityClass {}
class ACR_Skirmish : GenericEntity
{
 [Attribute("", UIWidgets.EditBox, "Layout name, picked with -acrLayout (spaces may be written as _)")]
 string m_sName;
 [Attribute("1", UIWidgets.ComboBox, "Skirmish size this layout is for", "", ParamEnumArray.FromEnum(ACR_ESkirmishSize))]
 ACR_ESkirmishSize m_eSize;
 [Attribute("", UIWidgets.EditBox, "Bases in play, by map name, separated by commas (the HQs are added automatically)")]
 string m_sBases;
 [Attribute("", UIWidgets.EditBox, "HQ per side as faction=base, separated by semicolons, e.g. US=Airbase; USSR=Power Plant")]
 string m_sHQs;

 static ref array<ACR_Skirmish> s_All = {};

 // Server state of the applied layout.
 protected static bool s_bApplied;
 static ACR_Skirmish s_Active;
 protected static ref array<string> s_PadSides = {};

 void ACR_Skirmish(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT);
 }
 override void EOnInit(IEntity owner)
 {
  if (GetGame().InPlayMode()) s_All.Insert(this);
 }
 void ~ACR_Skirmish()
 {
  if (s_All) s_All.RemoveItem(this);
  if (s_Active == this) s_Active = null;
 }

 static void Reset()
 {
  s_bApplied = false;
  s_Active = null;
  s_PadSides.Clear();
 }

 static bool Active() { return s_Active != null; }

 // Helicopter supports of a side fly from its helipads, unless a layout left it without one.
 static bool SideHasPads(string factionKey)
 {
  if (!s_Active && !ACR_Generator.Built()) return true;
  return s_PadSides.Contains(factionKey);
 }

 static string SizeName(ACR_ESkirmishSize size)
 {
  if (size == ACR_ESkirmishSize.HALF) return "half";
  return "flashpoint";
 }

 // -acrSize full|half|flashpoint: -1 = full (default), else the ACR_ESkirmishSize.
 static int SizeOption()
 {
  string value;
  if (!ACR_Options.Get("acrSize", value)) return -1;
  value.ToLower();
  if (value == "half") return ACR_ESkirmishSize.HALF;
  if (value == "flashpoint" || value == "small" || value == "flash") return ACR_ESkirmishSize.FLASHPOINT;
  return -1;
 }

 static string Simple(string text)
 {
  string result = text;
  result.Replace("_", "");
  result.Replace(" ", "");
  result.Replace("-", "");
  result.ToLower();
  return result;
 }

 // Layouts are authored with US/USSR labels, while modded games use selected faction keys.
 protected static string SelectedSide(string authoredSide)
 {
  if (authoredSide == "US") return ACR_FactionSelection.West();
  if (authoredSide == "USSR") return ACR_FactionSelection.East();
  return authoredSide;
 }

 // The layout the host asked for, or null for the whole world.
 static ACR_Skirmish Choose()
 {
  string wanted;
  ACR_Options.Get("acrLayout", wanted);
  string testLayout = ACR_TestHooks.Get().SkirmishLayout();
  if (testLayout != "") wanted = testLayout;
  wanted = Simple(wanted);
  if (wanted != "" && wanted != "random")
  {
   foreach (ACR_Skirmish layout : s_All)
    if (Simple(layout.m_sName) == wanted) return layout;
   if (wanted != "generated") PrintFormat("[ACR] SKIRMISH no layout named '%1' in this world", wanted);
  }
  int size = SizeOption();
  if (size < 0) return null;
  array<ACR_Skirmish> options = {};
  foreach (ACR_Skirmish candidate : s_All) if (candidate.m_eSize == size) options.Insert(candidate);
  if (options.IsEmpty())
  {
   PrintFormat("[ACR] SKIRMISH no %1 layout in this world: playing the whole map", SizeName(size));
   return null;
  }
  return options.GetRandomElement();
 }

 static ACR_Base FindBase(string name)
 {
  string wanted = Simple(name);
  if (wanted == "") return null;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (Simple(base.m_sName) == wanted) return base;
  return null;
 }

 // Server, first frame (all world entities registered, before any tick), or earlier from the
 // auto-spawn when a hosting player spawns before that first frame (ACR_PlayerSide).
 static bool Applied() { return s_bApplied; }

 static void ApplyOnce()
 {
  if (s_bApplied) return;
  s_bApplied = true;
  ACR_FactionSelection.Apply();
  // A generated layout (aca-f07) replaces the hand-built one, at any size.
  bool generated = false;
  if (ACR_Generator.Wanted())
  {
   ACR_Generator.Build();
   generated = ACR_Generator.Built() && !ACR_Generator.s_Sites.IsEmpty();
  }
  if (!generated)
  {
   ACR_Skirmish layout = Choose();
   if (layout) layout.Apply();
  }
  ACR_QuickStart.Apply();
  ACR_Pockets.Apply();
  ACR_Guerrillas.Apply();
  ACR_Insurgency.Apply();
 }

 void Apply()
 {
  // HQs named by the layout.
  map<string, ACR_Base> hqs = new map<string, ACR_Base>();
  array<string> pairs = {};
  m_sHQs.Split(";", pairs, true);
  foreach (string pair : pairs)
  {
   array<string> parts = {};
   pair.Split("=", parts, false);
   if (parts.Count() != 2) continue;
   string faction = parts[0];
   faction.Replace(" ", "");
   if (faction == "US") faction = ACR_FactionSelection.West();
   else if (faction == "USSR") faction = ACR_FactionSelection.East();
   ACR_Base named = FindBase(parts[1]);
   if (!named) { PrintFormat("[ACR] SKIRMISH %1: HQ base '%2' not found", m_sName, parts[1]); continue; }
   hqs.Set(faction, named);
  }
  if (hqs.Count() < 2)
  {
   PrintFormat("[ACR] SKIRMISH %1 needs an HQ for each side (%2 found): playing the whole map", m_sName, hqs.Count());
   return;
  }
  // Bases in play.
  array<ACR_Base> inPlay = {};
  foreach (string side0, ACR_Base hq0 : hqs) inPlay.Insert(hq0);
  array<string> names = {};
  m_sBases.Split(",", names, true);
  foreach (string name : names)
  {
   ACR_Base base = FindBase(name);
   if (!base) { PrintFormat("[ACR] SKIRMISH %1: base '%2' not found", m_sName, name); continue; }
   if (!inPlay.Contains(base)) inPlay.Insert(base);
  }
  // Each side's HQ as placed (its first owned base nearest its spawn point), for moving its
  // starting units and spawn points, and its garrison size.
  map<string, ACR_Base> oldHQs = new map<string, ACR_Base>();
  foreach (string side, ACR_Base hq2 : hqs) oldHQs.Set(side, OldHQ(side));
  // Starting units belong to the side whose placed HQ is nearest.
  foreach (ACR_StartingUnit unit : ACR_StartingUnit.s_All)
  {
   string owner = NearestOwner(unit.GetOrigin());
   ACR_Base newHQ = hqs.Get(owner);
   if (newHQ && newHQ != oldHQs.Get(owner)) unit.m_MovedTo = newHQ;
  }
  // Spawn points follow their side's HQ.
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
  {
   if (!point || ACR_SpawnPoints.IsBasePoint(point)) continue;
   string side = SelectedSide(point.GetFactionKey());
   ACR_Base target = hqs.Get(side);
   if (!target) continue;
   point.SetFactionKey(side);
   vector at = ACR_Manager.Ground(target.GetOrigin() + Vector(6, 0, 6));
   vector free;
   if (ACR_BaseSystem.ClearGround(free, at, 20, 1.5)) at = free;
   point.SetOrigin(at);
   PrintFormat("[ACR] SKIRMISH spawn point %1 moved to %2 %3", side, target.m_sName, at);
  }
  // Owners: HQs to their side, everything else in play neutral; the rest retired.
  array<ACR_Base> all = {};
  all.Copy(ACR_Base.s_All);
  int retired;
  foreach (ACR_Base base2 : all)
  {
   if (!inPlay.Contains(base2)) { base2.Retire("not in skirmish " + m_sName); retired++; continue; }
   string hqSide = "";
   foreach (string side2, ACR_Base hq3 : hqs) if (hq3 == base2) hqSide = side2;
   if (hqSide != "")
   {
    ACR_Base old = oldHQs.Get(hqSide);
    base2.m_sOwner = hqSide;
    base2.m_fScore = 100;
    base2.m_sCapturer = "";
    base2.m_bAllowSpawn = true;
    base2.m_bHidden = false;
    base2.m_eLogisticsRole = ACR_EBaseRole.HQ;
    if (old && old.m_iGarrison > base2.m_iGarrison) base2.m_iGarrison = old.m_iGarrison;
    base2.m_iGarrisonLeft = base2.m_iGarrison;
   }
   else if (base2.m_sOwner != "")
   {
    base2.m_sOwner = "";
    base2.m_fScore = 0;
    base2.m_iGarrisonLeft = base2.m_iGarrison;
   }
  }
  // Helipads of retired bases are gone; sides with a pad left keep pad helicopters.
  foreach (ACR_Helipad pad : ACR_Helipad.s_All)
  {
   if (pad.m_sBase != "" && !FindBase(pad.m_sBase)) pad.Drop();
   else if (pad.m_sBase == "" && !inPlay.Contains(pad.Base())) pad.Drop();
  }
  RecountPads();
  s_Active = this;
  string hqText = "";
  foreach (string side3, ACR_Base hq4 : hqs) hqText += string.Format(" %1=%2", side3, hq4.m_sName);
  PrintFormat("[ACR] SKIRMISH %1 (%2): %3 bases in play, %4 retired; HQs%5; helipad sides %6", m_sName, SizeName(m_eSize), ACR_Base.s_All.Count(), retired, hqText, s_PadSides.Count());
 }

 // Which sides have a helipad in play (set once the layout is applied; tests call it again).
 static void RecountPads()
 {
  s_PadSides.Clear();
  foreach (ACR_Helipad pad : ACR_Helipad.s_All)
  {
   string padSide = pad.Faction();
   if (padSide != "" && !s_PadSides.Contains(padSide)) s_PadSides.Insert(padSide);
  }
 }

 // A side's HQ as placed: its owned base nearest to its spawn point (else its first owned base).
 protected static ACR_Base OldHQ(string side)
 {
  vector home;
  bool found;
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
   if (point && !ACR_SpawnPoints.IsBasePoint(point) && SelectedSide(point.GetFactionKey()) == side) { home = point.GetOrigin(); found = true; break; }
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != side) continue;
   float distance = 0;
   if (found) distance = vector.DistanceXZ(base.GetOrigin(), home);
   if (distance < bestDistance) { bestDistance = distance; best = base; }
  }
  return best;
 }

 protected static string NearestOwner(vector position)
 {
  string owner = "";
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == "") continue;
   float distance = vector.DistanceXZ(base.GetOrigin(), position);
   if (distance < bestDistance) { bestDistance = distance; owner = base.m_sOwner; }
  }
  return owner;
 }
}
