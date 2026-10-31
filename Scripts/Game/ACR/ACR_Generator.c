// Scenario generator (board aca-f07, 14 Oct). Builds bases, HQs and starting units from the map's
// own named places, by the rules of Jacob's hand-built Everon layout:
//   city r90 / town r75 / village r60 / settlement r45: nominal value 1
//   military r60: value 3; industry r50: value 2; airport r80: value 3
//   home-region/nearest objectives retain income; a spaced strategic selection also earns RP;
//   remaining objectives (including hidden landmarks) are tactical, not passive income.
//   HQs: value 2, radius >= 70, garrison 6, owned; other bases start neutral with no garrison.
// Thin crowded objectives; reject rear/HQ-blocked approaches using cached land connectivity.
// Seed later income/tactical destinations while protecting HQs, openings and economic budgets.
// Bases remain on their map places (moved onto dry ground when the place is in water).
// Runs instead of a hand-built layout when the host picks "Generated" (acrLayout generated) or
// the world has no bases (e.g. ACR_Kolguyev). Same seed + size + world = same layout.
// Sizes: full map; half (a seeded rotated band or compact region); flashpoint (about seven
// sites within 1.8 km of one town, HQs inside it at opposite ends).
class ACR_GenSite
{
 string m_sId; // original surveyed name; edits survive display-name changes
 string m_sName;
 string m_sKind;     // city town village settlement military airport industry landmark
 vector m_vPos;
 float m_fRadius;
 float m_fFullRadius; // the kind's own radius, before shrinking to fit
 int m_iValue;
 bool m_bIncome;
 int m_iOpeningSide = -1; // protected opening income, otherwise -1
 int m_iRank;        // which of two overlapping sites stays
 string m_sKey;      // the map's location key, e.g. Military, Harbour, Cain_Kamensk
 string m_sPrefab;

 ACR_GenSite Copy()
 {
  ACR_GenSite copy = new ACR_GenSite();
  copy.m_sId = m_sId;
  copy.m_sName = m_sName;
  copy.m_sKind = m_sKind;
  copy.m_vPos = m_vPos;
  copy.m_fRadius = m_fRadius;
  copy.m_fFullRadius = m_fFullRadius;
  copy.m_iValue = m_iValue;
  copy.m_bIncome = m_bIncome;
  copy.m_iOpeningSide = m_iOpeningSide;
  copy.m_iRank = m_iRank;
  copy.m_sKey = m_sKey;
  copy.m_sPrefab = m_sPrefab;
  return copy;
 }

 // Shown in the setup preview: "Military site, 3 RP".
 string Describe()
 {
  string kind = m_sKind;
  if (kind == "military") kind = "Military site";
  else if (kind == "industry") kind = "Industry";
  else if (kind == "airport") kind = "Airport";
  else if (kind == "landmark") kind = "Hidden landmark";
  else kind = "Settlement";
  if (!m_bIncome) return kind + ", tactical (no income)";
  return string.Format("%1, %2 RP", kind, m_iValue);
 }
}

class ACR_Generator
{
 protected static bool s_bBuilt;
 static int s_iSeed;
 static ref array<ref ACR_GenSite> s_Sites = {};   // the layout in play (after Build)
 protected static ref array<IEntity> s_Found = {};

 protected static ref array<ref ACR_GenSite> s_Surveyed;  // the map's sites, read once
 protected static ref ACR_GenAccess s_Access;
 protected static ref array<ref ACR_GenSite> s_NamePlaces = {}; // named hills need not become CPs

 // Layout editor (aca-f07): bases the host moved on the setup map, by stable site ID. They belong
 // to one seed and size; another seed or size ignores them.
 static ref map<string, vector> s_Moves = new map<string, vector>();
 protected static ref array<string> s_Deleted = {};
 protected static int s_iMovesSeed;
 protected static int s_iMovesSize;
 static int s_iMovesVersion;

 static void Reset()
 {
  s_bBuilt = false;
  s_iSeed = 0;
  s_Sites.Clear();
  s_Surveyed = null;
  s_Access = null;
  s_NamePlaces.Clear();
  s_Moves.Clear();
  s_Deleted.Clear();
  s_iMovesVersion = 0;
 }

 static bool Built() { return s_bBuilt; }

 // The host asked for a generated layout, or the world has no bases of its own.
 static bool Wanted()
 {
  string layout;
  if (ACR_Options.Get("acrLayout", layout))
  {
   layout.ToLower();
   if (layout == "generated") return true;
  }
  return !HasOwnBases();
 }

 // The world was laid out by hand (Arland, Everon); else only generated layouts exist.
 static bool HasOwnBases()
 {
  foreach (ACR_Base base : ACR_Base.s_All) if (!base.m_bBuilt) return true;
  return false;
 }

 // The seed in use: the host's (acrSeed), else a random one, remembered so the log and the
 // setup screen can show it.
 static int Seed()
 {
  string text;
  int seed = 0;
  if (ACR_Options.Get("acrSeed", text)) seed = text.ToInt();
  if (seed <= 0)
  {
   seed = Math.RandomInt(1, 100000);
   if (seed < 0) seed = -seed;
   if (seed == 0) seed = 1;
   ACR_Options.Set("acrSeed", seed.ToString());
  }
  return seed;
 }

 // ---------------------------------------------------------------- reading the map
 protected static int s_iHouses;

 protected static bool CountHouse(IEntity e)
 {
  if (!e) return true;
  if (SCR_DestructibleBuildingEntity.Cast(e)) { s_iHouses++; return true; }
  EntityPrefabData data = e.GetPrefabData();
  if (data && data.GetPrefabName().Contains("/Buildings/")) s_iHouses++;
  return true;
 }

 // Buildings within a radius (a named place with none is a field or a pass, not a settlement).
 static int Houses(vector at, float radius)
 {
  s_iHouses = 0;
  GetGame().GetWorld().QueryEntitiesBySphere(at, radius, CountHouse);
  return s_iHouses;
 }

 protected static bool Collect(IEntity e)
 {
  if (e && e.FindComponent(MapDescriptorComponent)) s_Found.Insert(e);
  return true;
 }

 // Every usable named place of the world, classified and named.
 static void Survey(notnull array<ref ACR_GenSite> sites)
 {
  sites.Clear();
  s_NamePlaces.Clear();
  BaseWorld world = GetGame().GetWorld();
  vector mins, maxs;
  world.GetBoundBox(mins, maxs);
  s_Found.Clear();
  world.QueryEntitiesByAABB(Vector(mins[0], -500, mins[2]), Vector(maxs[0], 3000, maxs[2]), Collect);
  array<ref ACR_GenSite> pending = {};
  foreach (IEntity e : s_Found)
  {
   MapDescriptorComponent d = MapDescriptorComponent.Cast(e.FindComponent(MapDescriptorComponent));
   if (!d) continue;
   int type = d.GetBaseType();
   MapItem item = d.Item();
   if (!item) continue;
   string raw = item.GetDisplayName();
   ACR_GenSite site = new ACR_GenSite();
   site.m_vPos = e.GetOrigin();
   site.m_sKey = raw;
   site.m_sKey.Replace("#AR-MapLocation_", "");
   site.m_sKey.Replace("#AR-Campaign_MapLocation_", "");
   if (e.GetPrefabData())
   {
    string extension;
    site.m_sPrefab = FilePath.StripExtension(FilePath.StripPath(e.GetPrefabData().GetPrefabName()), extension);
   }
   string shown = raw;
   if (raw.StartsWith("#")) shown = WidgetManager.Translate(raw);
   // Third-party maps (29 Oct, Kunar): names with stray tabs/line breaks, and "?" placeholders.
   shown.Replace("\n", " ");
   shown.Replace("\r", " ");
   shown.Replace("\t", " ");
   shown = shown.Trim();
   if (shown != "" && !shown.StartsWith("#") && !HasLetter(shown)) continue;
   if (type == EMapDescriptorType.MDT_NAME_HILL && shown != "" && !shown.StartsWith("#"))
   {
    ACR_GenSite hill = new ACR_GenSite();
    hill.m_vPos = site.m_vPos;
    hill.m_sName = shown;
    s_NamePlaces.Insert(hill);
   }
   if (!Classify(site, type, shown)) continue;
   pending.Insert(site);
  }
  // A stable order, so the same seed always picks the same sites.
  while (!pending.IsEmpty())
  {
   int best = 0;
   for (int i = 1; i < pending.Count(); i++)
   {
    if (pending[i].m_vPos[0] < pending[best].m_vPos[0]) best = i;
    else if (pending[i].m_vPos[0] == pending[best].m_vPos[0] && pending[i].m_vPos[2] < pending[best].m_vPos[2]) best = i;
   }
   sites.Insert(pending[best]);
   pending.RemoveOrdered(best);
  }
  NameIndustry(sites);
  foreach (ACR_GenSite identified : sites) identified.m_sId = identified.m_sName;
 }

 static bool HasLetter(string text)
 {
  for (int i = 0; i < text.Length(); i++)
  {
   int c = text.Get(i).ToAscii();
   if ((c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c > 127 || c < 0) return true;
  }
  return false;
 }

 // Kind, radius, value and (for settlements and landmarks) the name. False = not a base.
 protected static bool Classify(ACR_GenSite site, int type, string shown)
 {
  string key = site.m_sKey;
  if (type == EMapDescriptorType.MDT_NAME_CITY) return Set(site, "city", 90, 1, 3, shown);
  if (type == EMapDescriptorType.MDT_NAME_TOWN) return Set(site, "town", 75, 1, 3, shown);
  if (type == EMapDescriptorType.MDT_NAME_VILLAGE) return Set(site, "village", 60, 1, 2, shown);
  if (type == EMapDescriptorType.MDT_NAME_SETTLEMENT) return Set(site, "settlement", 45, 1, 1, shown);
  // Plain labels ("farm", "harbor" as a local name) are not places of their own.
  if (shown != "" && !shown.StartsWith("#"))
  {
   int first = shown.Get(0).ToAscii();
   if (first >= 97 && first <= 122 && type != EMapDescriptorType.MDT_NAME_GENERIC) return false;
  }
  if (type == EMapDescriptorType.MDT_NAME_LOCAL)
  {
   if (key.Contains("Mine")) return Set(site, "industry", 50, 2, 4, shown);
   if (shown == "" || shown.StartsWith("#")) return false;
   return Set(site, "landmark", 40, 1, 0, shown);
  }
  if (type != EMapDescriptorType.MDT_NAME_GENERIC) return false;
  if (key == "Military") return Set(site, "military", 60, 3, 5, "");
  if (key == "Airport") return Set(site, "airport", 80, 3, 4, "");
  if (key == "Harbour" || key == "Factory" || key == "Industrial" || key == "PowerPlant" || key == "Sawmill" || key.Contains("Quarry")) return Set(site, "industry", 50, 2, 4, "");
  if (key == "Landfill") return Set(site, "landmark", 40, 1, 0, shown);
  if (key == "Farm") return false;
  // Any other named place (Cain_Langonnet, Cain_Villa...) is a settlement when houses stand
  // there; fields, passes and heaths (Everon's Ramtop Meadows, Saint-Pierre's Pass) are not.
  if (shown == "" || shown.StartsWith("#")) return false;
  if (Houses(site.m_vPos, 120) < 6) return false;
  return Set(site, "settlement", 45, 1, 1, shown);
 }

 protected static bool Set(ACR_GenSite site, string kind, float radius, int value, int rank, string name)
 {
  site.m_sKind = kind;
  site.m_fRadius = radius;
  site.m_fFullRadius = radius;
  site.m_iValue = value;
  site.m_iRank = rank;
  site.m_sName = name;
  return true;
 }

 // Industry and military sites are named after the place they belong to: the prefab's own
 // place name when it has one (Langonnet_Military_01 -> "Langonnet Base"), else the nearest
 // settlement ("Chotain Base").
 protected static void NameIndustry(array<ref ACR_GenSite> sites)
 {
  array<string> used = {};
  foreach (ACR_GenSite named : sites) if (named.m_sName != "") used.Insert(named.m_sName);
  foreach (ACR_GenSite site : sites)
  {
   if (site.m_sName != "") continue;
   string suffix = Suffix(site);
   string place = PrefabPlace(site.m_sPrefab);
   if (place == "" && site.m_sKind != "airport") place = NearestSettlement(sites, site.m_vPos);
   string name = (place + " " + suffix).Trim();
   int n = 2;
   string tryName = name;
   while (used.Contains(tryName)) { tryName = string.Format("%1 %2", name, n); n++; }
   site.m_sName = tryName;
   used.Insert(tryName);
  }
 }

 protected static string Suffix(ACR_GenSite site)
 {
  string key = site.m_sKey;
  if (site.m_sKind == "military") return "Base";
  if (site.m_sKind == "airport") return "Airport";
  if (key == "Harbour") return "Harbor";
  if (key == "PowerPlant") return "Power Plant";
  if (key.Contains("Quarry")) return "Quarry";
  return key;
 }

 // "BareRidge_Military_01" -> "Bare Ridge"; "" for generic prefabs (LocationUrbanGeneric_...).
 protected static string PrefabPlace(string prefab)
 {
  if (prefab == "" || prefab.StartsWith("Location")) return "";
  array<string> parts = {};
  prefab.Split("_", parts, true);
  if (parts.Count() < 2) return "";
  string word = parts[0];
  string spaced;
  for (int i = 0; i < word.Length(); i++)
  {
   string c = word.Get(i);
   int code = c.ToAscii();
   if (i > 0 && code >= 65 && code <= 90)
   {
    int before = word.Get(i - 1).ToAscii();
    if (before >= 97 && before <= 122) spaced += " ";
   }
   spaced += c;
  }
  return spaced;
 }

 protected static string NearestSettlement(array<ref ACR_GenSite> sites, vector at)
 {
  string best = "";
  float bestDistance = float.MAX;
  foreach (ACR_GenSite site : sites)
  {
   if (site.m_sKind != "city" && site.m_sKind != "town" && site.m_sKind != "village" && site.m_sKind != "settlement") continue;
   float distance = vector.DistanceXZ(site.m_vPos, at);
   if (distance < bestDistance) { bestDistance = distance; best = site.m_sName; }
  }
  return best;
 }

 // ---------------------------------------------------------------- choosing the layout
 // The layout for a seed and size, without touching the world (tests call this to compare).
 // size: -1 full, else ACR_ESkirmishSize. hqs[0] = west HQ, hqs[1] = east HQ.
 // A seed whose draw can't make a playable layout (29 Oct: one flashpoint seed in eight on
 // Gulfcoast Islands' sparse islands) tries a few derived draws, still deterministically. A
 // failed plan leaves a generated world with no bases, so this beats reporting failure.
 // Host edits stay keyed to the seed the host chose. Seeds that plan first time are unchanged.
 static bool Plan(int seed, int size, notnull array<ref ACR_GenSite> layout, notnull array<ACR_GenSite> hqs, array<ref ACR_GenSite> incomeCandidates = null)
 {
  for (int retry = 0; retry < 6; retry++)
   if (PlanDraw(seed, seed + retry * 7919, size, layout, hqs, incomeCandidates)) return true;
  return false;
 }

 protected static bool PlanDraw(int seed, int draw, int size, notnull array<ref ACR_GenSite> layout, notnull array<ACR_GenSite> hqs, array<ref ACR_GenSite> incomeCandidates)
 {
  layout.Clear();
  hqs.Clear();
  if (incomeCandidates) incomeCandidates.Clear();
  RandomGenerator rng = new RandomGenerator();
  rng.SetSeed(draw);
  // The map's places are read once (about a second), then copied for each plan.
  if (!s_Surveyed)
  {
   s_Surveyed = {};
   Survey(s_Surveyed);
  }
  array<ref ACR_GenSite> sites = {};
  foreach (ACR_GenSite surveyed : s_Surveyed) sites.Insert(surveyed.Copy());
  // Dry ground, and no two capture circles too close: the higher-ranked site stays.
  array<ref ACR_GenSite> kept = {};
  foreach (ACR_GenSite site : sites)
  {
   if (!Dry(site)) continue;
   kept.Insert(site);
  }
  array<ref ACR_GenSite> spaced = {};
  for (int rank = 5; rank >= 0; rank--)
  {
   foreach (ACR_GenSite candidate : kept)
   {
    if (candidate.m_iRank != rank || candidate.m_sKind == "landmark") continue;
    if (Fit(spaced, candidate)) spaced.Insert(candidate);
   }
  }
  // A few hidden landmarks, spread out: about one for every ten other bases.
  array<ACR_GenSite> marks = {};
  foreach (ACR_GenSite mark : kept) if (mark.m_sKind == "landmark") marks.Insert(mark);
  float others = spaced.Count();
  int wantMarks = Math.Round(others / 10);
  if (wantMarks < 2) wantMarks = 2;
  int tries = 0;
  int added = 0;
  while (added < wantMarks && !marks.IsEmpty() && tries < 200)
  {
   tries++;
   int pick = Math.Floor(rng.RandFloat01() * marks.Count());
   if (pick >= marks.Count()) pick = marks.Count() - 1;
   ACR_GenSite chosen = marks[pick];
   marks.Remove(pick);
   if (!Fit(spaced, chosen)) continue;
   bool farFromMarks = true;
   foreach (ACR_GenSite other : spaced) if (other.m_sKind == "landmark" && vector.DistanceXZ(other.m_vPos, chosen.m_vPos) < 600) farFromMarks = false;
   if (!farFromMarks) continue;
   spaced.Insert(chosen);
   added++;
  }
  // Flashpoints retry bounded local regions when access would leave fewer than five sites.
  bool flash = size == ACR_ESkirmishSize.FLASHPOINT;
  array<ACR_GenSite> pool = {}, hqPool = {};
  int attempts = 1;
  // Halves too (29 Oct, Gulfcoast Islands): a band can cut an archipelago into unreachable
  // islands. Retries only follow a failed draw, so maps that plan first time are unchanged.
  if (flash || size == ACR_ESkirmishSize.HALF) attempts = 8;
  bool selected = false;
  for (int attempt = 0; attempt < attempts; attempt++)
  {
   pool.Clear(); hqPool.Clear();
   foreach (ACR_GenSite s : spaced) pool.Insert(s);
   if (size == ACR_ESkirmishSize.HALF) pool = Half(pool, rng);
   else if (flash) pool = Flashpoint(pool, rng);
   if (pool.Count() < 3) continue;
   ACR_GenSite west, east;
   PickHQs(pool, flash, rng, west, east);
   if (!west || !east) continue;
   hqPool.Insert(west); hqPool.Insert(east);
   FairObjectives(pool, hqPool, flash);
   int minimum = 3;
   if (flash) minimum = 5;
   if (pool.Count() < minimum) continue;
   selected = true;
   break;
  }
  if (!selected) return false;
  if (rng.RandFloat01() < 0.5) { ACR_GenSite swap = hqPool[0]; hqPool[0] = hqPool[1]; hqPool[1] = swap; }
  hqs.Copy(hqPool);
  ThinObjectives(pool, hqs, flash);
  if (pool.Count() < 3) return false;
  foreach (ACR_GenSite p : pool) layout.Insert(p);
  // Establish the original income budget before omitting any optional tactical sites.
  AssignEconomy(layout, hqs, flash, draw);
  // Optional owned snapshot of the actual economic candidate pool, before tactical omission.
  if (incomeCandidates) foreach (ACR_GenSite economic : layout) incomeCandidates.Insert(economic.Copy());
  int economySize = layout.Count();
  SelectContested(layout, hqs, flash, rng);
  int omitted = economySize - layout.Count();
  bool edited = seed == s_iMovesSeed && size == s_iMovesSize && (!s_Deleted.IsEmpty() || !s_Moves.IsEmpty());
  if (seed == s_iMovesSeed && size == s_iMovesSize)
   for (int removed = layout.Count() - 1; removed >= 0; removed--)
    if (s_Deleted.Contains(layout[removed].m_sId) && !hqs.Contains(layout[removed])) layout.RemoveOrdered(removed);
  ApplyMoves(seed, size, layout);
  // Host edits remain authoritative; moving an HQ relocates its protected opening income.
  // Automatically omitted tactical sites must not lower the economic target on a re-plan.
  if (edited) AssignEconomy(layout, hqs, flash, draw, layout.Count() + omitted);
  // HQs get a 70 m circle where there is room for it.
  foreach (ACR_GenSite hq : hqs)
  {
   float allowed = 70;
   foreach (ACR_GenSite near : layout)
   {
    if (near == hq) continue;
    float room = vector.DistanceXZ(near.m_vPos, hq.m_vPos) - near.m_fRadius - 10;
    if (room < allowed) allowed = room;
   }
   if (allowed > hq.m_fRadius) hq.m_fRadius = allowed;
  }
  return true;
 }

 // Keep the HQs and the two closest opening objectives per side before thinning the rest.
 // Opening pairs may be 250 m apart; other objectives need 350 m. A flashpoint is deliberately
 // compact already (5-7 sites), so it retains its existing circle-spacing rule.
 protected static void ThinObjectives(array<ACR_GenSite> pool, array<ACR_GenSite> hqs, bool flash)
 {
  if (flash) return;
  array<ref ACR_GenSite> kept = {};
  foreach (ACR_GenSite hq : hqs) kept.Insert(hq);
  if (!flash)
  {
   for (int side = 0; side < 2; side++)
    for (int opening = 0; opening < 2; opening++)
    {
     ACR_GenSite closest;
     float nearest = float.MAX;
     foreach (ACR_GenSite candidate : pool)
     {
      if (kept.Contains(candidate) || candidate.m_sKind == "landmark") continue;
      float away = vector.DistanceXZ(candidate.m_vPos, hqs[side].m_vPos);
      if (away > vector.DistanceXZ(candidate.m_vPos, hqs[1 - side].m_vPos)) continue;
      bool room = true;
      foreach (ACR_GenSite prior : kept) if (vector.DistanceXZ(candidate.m_vPos, prior.m_vPos) < 250) room = false;
      if (room && away < nearest) { nearest = away; closest = candidate; }
     }
     if (closest) { closest.m_iOpeningSide = side; kept.Insert(closest); }
    }
  }
  float spacing = 350;
  for (int rank = 5; rank >= 0; rank--)
   foreach (ACR_GenSite site : pool)
   {
    if (site.m_iRank != rank || kept.Contains(site)) continue;
    bool fits = true;
    foreach (ACR_GenSite other : kept) if (vector.DistanceXZ(site.m_vPos, other.m_vPos) < spacing) fits = false;
    if (fits && Fit(kept, site)) kept.Insert(site);
   }
  pool.Clear();
  foreach (ACR_GenSite retained : kept) pool.Insert(retained);
 }

 // Avoid objectives directly behind an HQ, while retaining reachable flank objectives.
 // A whole half-plane cut would wrongly remove towns beside, rather than behind, an HQ.
 static bool BetweenHQs(vector at, vector west, vector east)
 {
  vector axis = east - west;
  axis[1] = 0;
  float length = axis.Length();
  if (length < 1) return false;
  float along = vector.Dot(at - west, axis / length);
  vector offset = at - west;
  offset[1] = 0;
  float lateral = (offset - axis / length * along).Length();
  if (along < -200 && -along > 0.75 * lateral) return false;
  if (along > length + 200 && along - length > 0.75 * lateral) return false;
  return true;
 }

 protected static void FairObjectives(array<ACR_GenSite> pool, array<ACR_GenSite> hqs, bool flash)
 {
  if (!s_Access)
  {
   s_Access = new ACR_GenAccess();
   s_Access.Build(s_Surveyed);
  }
  array<int> westReach = {}, eastReach = {};
  float clearance = ACR_GenAccess.HQ_CLEARANCE;
  if (flash) clearance = 100;
  s_Access.Reach(hqs[0].m_vPos, hqs[1].m_vPos, westReach, clearance);
  s_Access.Reach(hqs[1].m_vPos, hqs[0].m_vPos, eastReach, clearance);
  for (int i = pool.Count() - 1; i >= 0; i--)
  {
   ACR_GenSite site = pool[i];
   if (hqs.Contains(site)) continue;
   if (!BetweenHQs(site.m_vPos, hqs[0].m_vPos, hqs[1].m_vPos) || !s_Access.CanReach(site.m_vPos, westReach, hqs[1].m_vPos, clearance) || !s_Access.CanReach(site.m_vPos, eastReach, hqs[0].m_vPos, clearance)) pool.RemoveOrdered(i);
  }
 }

 // Among otherwise suitable, widely separated HQ pairs, prefer the one that strands the
 // fewest objectives. This avoids fixing a bad HQ choice by unnecessarily deleting a region.
 protected static int AccessibleCount(array<ACR_GenSite> pool, ACR_GenSite west, ACR_GenSite east)
 {
  if (!s_Access)
  {
   s_Access = new ACR_GenAccess();
   s_Access.Build(s_Surveyed);
  }
  array<int> westReach = {}, eastReach = {};
  s_Access.Reach(west.m_vPos, east.m_vPos, westReach);
  s_Access.Reach(east.m_vPos, west.m_vPos, eastReach);
  int count = 2;
  foreach (ACR_GenSite site : pool)
  {
   if (site == west || site == east) continue;
   if (BetweenHQs(site.m_vPos, west.m_vPos, east.m_vPos) && s_Access.CanReach(site.m_vPos, westReach, east.m_vPos) && s_Access.CanReach(site.m_vPos, eastReach, west.m_vPos)) count++;
  }
  return count;
 }

 // Home regions and four nearest ordinary objectives per side keep their full income. This
 // protects the opening; later RP comes from a spaced selection of strategic sites instead
 // of every town/landmark. HQ/basic income, starting RP and reinforcement prices are untouched.
 static void AssignEconomy(array<ref ACR_GenSite> layout, array<ACR_GenSite> hqs, bool flash, int seed = 0, int plannedCount = -1)
 {
  foreach (ACR_GenSite site : layout) { site.m_bIncome = false; site.m_iOpeningSide = -1; }
  foreach (ACR_GenSite hq : hqs) { hq.m_bIncome = true; hq.m_iValue = 2; }
  int starters = 4;
  if (flash) starters = 2;
  for (int side = 0; side < 2; side++)
   for (int opening = 0; opening < starters; opening++)
   {
    ACR_GenSite closest;
    float distance = float.MAX;
    foreach (ACR_GenSite candidate : layout)
    {
     if (candidate.m_bIncome || candidate.m_sKind == "landmark") continue;
     float away = vector.DistanceXZ(candidate.m_vPos, hqs[side].m_vPos);
     if (away > vector.DistanceXZ(candidate.m_vPos, hqs[1 - side].m_vPos)) continue;
     if (away < distance) { distance = away; closest = candidate; }
    }
    if (closest) { closest.m_bIncome = true; closest.m_iOpeningSide = side; }
   }
  // A dense home region must not lose early funding simply because it has a fifth nearby
  // objective. These sites remain valuable later, too: there is no timed income nerf.
  if (!flash)
   foreach (ACR_GenSite nearby : layout)
   {
    if (nearby.m_bIncome || nearby.m_sKind == "landmark") continue;
    float westDistance = vector.DistanceXZ(nearby.m_vPos, hqs[0].m_vPos);
    float eastDistance = vector.DistanceXZ(nearby.m_vPos, hqs[1].m_vPos);
    if (Math.Min(westDistance, eastDistance) > 2000) continue;
    nearby.m_bIncome = true;
    nearby.m_iOpeningSide = 0;
    if (eastDistance < westDistance) nearby.m_iOpeningSide = 1;
   }
  int earning;
  foreach (ACR_GenSite counted : layout) if (counted.m_bIncome) earning++;
  if (plannedCount < 0) plannedCount = layout.Count();
  int target = Math.Max(earning, 2 + Math.Ceil(0.5 * (plannedCount - 2)));
  float separation = 800;
  if (flash) separation = 300;
  while (earning < target)
  {
   ACR_GenSite best;
   float score = -1;
   foreach (ACR_GenSite option : layout)
   {
    if (option.m_bIncome || option.m_sKind == "landmark") continue;
    float nearest = float.MAX;
    foreach (ACR_GenSite income : layout) if (income.m_bIncome) nearest = Math.Min(nearest, vector.DistanceXZ(option.m_vPos, income.m_vPos));
    if (nearest < separation) continue;
    float value = option.m_iRank * 400 + Math.Min(nearest, 2500);
    if (value > score) { score = value; best = option; }
   }
   if (!best) break;
   best.m_bIncome = true;
   earning++;
  }
  if (seed != 0) VaryIncome(layout, hqs, flash, seed);
 }

 // Choose the later income portfolio together: one-site swaps cannot escape choices
 // blocked by another earning site. Keep exact count/RP and protected openings; site values
 // stay unchanged. Deterministic work cap, no wall-clock cutoff that could change a seed.
 protected static void VaryIncome(array<ref ACR_GenSite> layout, array<ACR_GenSite> hqs, bool flash, int seed, int maxSteps = 4096)
 {
  array<ACR_GenSite> anchors = {}, original = {}, candidates = {};
  int value = 0;
  foreach (ACR_GenSite site : layout)
  {
   if (!site.m_bIncome) continue;
   if (hqs.Contains(site) || site.m_iOpeningSide >= 0) anchors.Insert(site);
   else { original.Insert(site); value += site.m_iValue; }
  }
  if (original.IsEmpty()) return;
  float spacing = 800;
  if (flash) spacing = 300;
  foreach (ACR_GenSite option : layout)
  {
   if (hqs.Contains(option) || option.m_iOpeningSide >= 0 || option.m_sKind == "landmark" || option.m_iValue < 1 || option.m_iValue > 3) continue;
   bool fits = true;
   foreach (ACR_GenSite anchor : anchors)
    if (vector.DistanceXZ(option.m_vPos, anchor.m_vPos) < spacing) fits = false;
   if (fits) candidates.Insert(option);
  }
  array<ACR_GenSite> chosen = {};
  array<int> budget = {maxSteps};
  array<ref array<ACR_GenSite>> portfolios = {};
  bool complete = EnumerateIncome(candidates, original.Count(), value, spacing, chosen, portfolios, budget);
  if (complete && !portfolios.IsEmpty())
  {
   // A complete small pool gets an even seed cycle: consecutive seeds cannot repeatedly
   // land on the same portfolio. No partial catalogue is used as though it were complete.
   int choice = seed % portfolios.Count();
   if (choice < 0) choice += portfolios.Count();
   chosen.Copy(portfolios[choice]);
  }
  else
  {
   // Large pools retain seeded geographical variety rather than using a biased catalogue
   // prefix. Each phase has <=4096 branches; failure changes no income flags.
   RandomGenerator rng = new RandomGenerator(); rng.SetSeed(seed);
   for (int i = candidates.Count() - 1; i > 0; i--)
   {
    int pick = Math.Min(i, Math.Floor(rng.RandFloat01() * (i + 1)));
    ACR_GenSite swap = candidates[i]; candidates[i] = candidates[pick]; candidates[pick] = swap;
   }
   chosen.Clear(); budget[0] = maxSteps;
   if (!ChooseIncome(candidates, 0, original.Count(), value, spacing, chosen, budget)) return;
  }
  // Atomic replacement: exhaustion/failure leaves the original flags completely intact.
  foreach (ACR_GenSite previous : original) previous.m_bIncome = false;
  foreach (ACR_GenSite selected : chosen) selected.m_bIncome = true;
 }

 // Bound the remaining RP by the actual available values before branching. The
 // candidates entering this routine already fit every previously selected site.
 protected static bool IncomeBudgetFits(array<ACR_GenSite> candidates, int slots, int value)
 {
  if (candidates.Count() < slots || value < slots || value > 3 * slots) return false;
  array<int> counts = {0, 0, 0, 0};
  foreach (ACR_GenSite site : candidates) counts[site.m_iValue] = counts[site.m_iValue] + 1;
  int remaining = slots, minimum = 0, maximum = 0;
  for (int low = 1; low <= 3; low++)
  {
   int takeLow = Math.Min(remaining, counts[low]);
   minimum += takeLow * low; remaining -= takeLow;
  }
  remaining = slots;
  for (int high = 3; high >= 1; high--)
  {
   int takeHigh = Math.Min(remaining, counts[high]);
   maximum += takeHigh * high; remaining -= takeHigh;
  }
  return value >= minimum && value <= maximum;
 }

 protected static bool EnumerateIncome(array<ACR_GenSite> candidates, int slots, int value, float spacing, array<ACR_GenSite> chosen, array<ref array<ACR_GenSite>> portfolios, array<int> budget)
 {
  if (slots == 0)
  {
   if (value != 0) return true;
   if (portfolios.Count() >= 128) return false;
   array<ACR_GenSite> snapshot = {}; snapshot.Copy(chosen);
   portfolios.Insert(snapshot);
   return true;
  }
  if (!IncomeBudgetFits(candidates, slots, value)) return true;
  for (int i = 0; i <= candidates.Count() - slots; i++)
  {
   if (budget[0] <= 0) return false;
   budget[0] = budget[0] - 1;
   ACR_GenSite option = candidates[i];
   array<ACR_GenSite> next = {};
   for (int j = i + 1; j < candidates.Count(); j++)
    if (vector.DistanceXZ(option.m_vPos, candidates[j].m_vPos) >= spacing) next.Insert(candidates[j]);
   chosen.Insert(option);
   bool complete = EnumerateIncome(next, slots - 1, value - option.m_iValue, spacing, chosen, portfolios, budget);
   chosen.RemoveOrdered(chosen.Count() - 1);
   if (!complete) return false;
  }
  return true;
 }

 protected static bool ChooseIncome(array<ACR_GenSite> candidates, int start, int slots, int value, float spacing, array<ACR_GenSite> chosen, array<int> budget)
 {
  if (slots == 0) return value == 0;
  if (value < slots || value > 3 * slots || candidates.Count() - start < slots) return false;
  for (int i = start; i <= candidates.Count() - slots; i++)
  {
   if (budget[0] <= 0) return false;
   budget[0] = budget[0] - 1;
   ACR_GenSite option = candidates[i];
   if (option.m_iValue > value) continue;
   bool fits = true;
   foreach (ACR_GenSite prior : chosen)
    if (vector.DistanceXZ(option.m_vPos, prior.m_vPos) < spacing) fits = false;
   if (!fits) continue;
   chosen.Insert(option);
   if (ChooseIncome(candidates, i + 1, slots - 1, value - option.m_iValue, spacing, chosen, budget)) return true;
   chosen.RemoveOrdered(chosen.Count() - 1);
  }
  return false;
 }

 // Keep every income site and protected opening. Vary the remaining tactical presence;
 // flashpoints instead select 5-7 sites from their accessible local pool, protecting the
 // two nearest ordinary opening objectives per side, then assigning their normal economy.
 protected static void SelectContested(array<ref ACR_GenSite> layout, array<ACR_GenSite> hqs, bool flash, RandomGenerator rng)
 {
  array<ref ACR_GenSite> selected = {};
  array<ACR_GenSite> optional = {};
  foreach (ACR_GenSite site : layout)
  {
   bool protect = site.m_bIncome || site.m_iOpeningSide >= 0 || hqs.Contains(site);
   if (flash) protect = site.m_iOpeningSide >= 0 || hqs.Contains(site);
   if (protect || (!flash && site.m_sKind == "landmark")) selected.Insert(site);
   else optional.Insert(site);
  }
  int target = Math.Max(6, layout.Count() - Math.Ceil((0.2 + 0.2 * rng.RandFloat01()) * optional.Count()));
  if (!flash && !optional.IsEmpty()) target = Math.Max(target, selected.Count() + 1);
  if (flash) target = Math.Max(selected.Count(), 5 + Math.Floor(3 * rng.RandFloat01()));
  target = Math.Min(target, layout.Count());
  // A seeded direction and kind preference shift which flank and facilities feature.
  float angle = Math.PI2 * rng.RandFloat01();
  vector axis = Vector(Math.Cos(angle), 0, Math.Sin(angle));
  vector middle = 0.5 * (hqs[0].m_vPos + hqs[1].m_vPos);
  int kind = Math.Min(2, Math.Floor(3 * rng.RandFloat01()));
  array<float> scores = {};
  foreach (ACR_GenSite candidate : optional)
  {
   float preference = 0;
   if (kind == 0 && (candidate.m_sKind == "city" || candidate.m_sKind == "town" || candidate.m_sKind == "village" || candidate.m_sKind == "settlement")) preference = 0.35;
   if (kind == 1 && candidate.m_sKind == "industry") preference = 0.35;
   if (kind == 2 && (candidate.m_sKind == "military" || candidate.m_sKind == "airport")) preference = 0.35;
   float flank = Math.Clamp(vector.Dot(candidate.m_vPos - middle, axis) / 3000, -0.25, 0.25);
   scores.Insert(rng.RandFloat01() + preference + flank);
  }
  while (selected.Count() < target && !optional.IsEmpty())
  {
   int best = 0;
   for (int i = 1; i < optional.Count(); i++) if (scores[i] > scores[best]) best = i;
   selected.Insert(optional[best]);
   optional.RemoveOrdered(best); scores.RemoveOrdered(best);
  }
  layout.Clear();
  foreach (ACR_GenSite chosen : selected) layout.Insert(chosen);
  if (flash) AssignEconomy(layout, hqs, true);
 }

 // ---------------------------------------------------------------- layout editor
 // The host's moves, applied after the sites and HQs are chosen (so a move never changes which
 // places are in play). A moved base takes its kind's full circle, shrunk to fit.
 protected static void ApplyMoves(int seed, int size, array<ref ACR_GenSite> layout)
 {
  if (s_Moves.IsEmpty() || seed != s_iMovesSeed || size != s_iMovesSize) return;
  foreach (ACR_GenSite site : layout)
  {
   vector to;
   if (!s_Moves.Find(site.m_sId, to)) continue;
   site.m_vPos = to;
   float allowed = site.m_fFullRadius;
   if (allowed <= 0) allowed = site.m_fRadius;
   foreach (ACR_GenSite other : layout)
   {
    if (other == site) continue;
    float room = vector.DistanceXZ(other.m_vPos, site.m_vPos) - other.m_fRadius - 10;
    if (room < allowed) allowed = room;
   }
   if (allowed < 35) allowed = 35; // checked when moved; a later move can only take the room it found
   site.m_fRadius = allowed;
  }
  foreach (ACR_GenSite renamed : layout)
  {
   if (!s_Moves.Contains(renamed.m_sId)) continue;
   string label = NameAt(renamed.m_vPos, renamed.m_sKind);
   string unique = label;
   int number = 2;
   bool duplicate = true;
   while (duplicate)
   {
    duplicate = false;
    foreach (ACR_GenSite named : layout) if (named != renamed && named.m_sName == unique) duplicate = true;
    if (duplicate) { unique = label + " " + number.ToString(); number++; }
   }
   renamed.m_sName = unique;
  }
 }

 // Include landmarks, even those not selected for this layout (e.g. Pobedy Peak).
 static string NameAt(vector at, string kind)
 {
  string name = "Control point";
  float distance = float.MAX;
  foreach (ACR_GenSite place : s_Surveyed)
  {
   float away = vector.DistanceXZ(at, place.m_vPos);
   if (away < distance) { distance = away; name = place.m_sName; }
  }
  foreach (ACR_GenSite hill : s_NamePlaces)
  {
   float hillDistance = vector.DistanceXZ(at, hill.m_vPos);
   if (hillDistance < distance) { distance = hillDistance; name = hill.m_sName; }
  }
  if (kind == "military" && !name.Contains("Base")) name += " Base";
  else if (kind == "airport" && !name.Contains("Airfield")) name += " Airfield";
  else if (kind == "industry") name += " Industry";
  return name;
 }

 // Edited sites (moves and deletions) for the current seed and size.
 static int MoveCount()
 {
  string text;
  int seed = 0;
  if (ACR_Options.Get("acrSeed", text)) seed = text.ToInt();
  if (seed != s_iMovesSeed || ACR_Skirmish.SizeOption() != s_iMovesSize) return 0;
  return s_Moves.Count() + s_Deleted.Count();
 }

 static void ClearMoves()
 {
  s_Moves.Clear();
  s_Deleted.Clear();
  s_iMovesVersion++;
 }

 static bool Delete(string name, out string reason)
 {
  array<ref ACR_GenSite> layout = {};
  array<ACR_GenSite> hqs = {};
  int seed = Seed();
  int size = ACR_Skirmish.SizeOption();
  if (!Plan(seed, size, layout, hqs)) { reason = "There is no generated layout to edit"; return false; }
  foreach (ACR_GenSite site : layout)
  {
   if (site.m_sName != name) continue;
   if (hqs.Contains(site)) { reason = "HQs cannot be deleted; move them instead"; return false; }
   if (layout.Count() <= 3) { reason = "Keep at least one capture point and the two HQs"; return false; }
   if (seed != s_iMovesSeed || size != s_iMovesSize) ClearMoves();
   s_iMovesSeed = seed;
   s_iMovesSize = size;
   s_Deleted.Insert(site.m_sId);
   s_Moves.Remove(site.m_sId);
   s_iMovesVersion++;
   reason = name + " deleted. Undo layout edits restores it";
   return true;
  }
  reason = "No base called " + name;
  return false;
 }

 // The host moved a base on the setup map (server). Refused with a reason when the spot is off
 // the map, in water, crowds another base's capture circle, or brings the HQs too close.
 static bool Move(string name, float x, float z, out string reason)
 {
  int seed = Seed();
  int size = ACR_Skirmish.SizeOption();
  if (seed != s_iMovesSeed || size != s_iMovesSize)
  {
   s_Moves.Clear();
   s_Deleted.Clear();
   s_iMovesSeed = seed;
   s_iMovesSize = size;
  }
  array<ref ACR_GenSite> layout = {};
  array<ACR_GenSite> hqs = {};
  if (!Plan(seed, size, layout, hqs)) { reason = "There is no generated layout to edit"; return false; }
  ACR_GenSite site;
  foreach (ACR_GenSite candidate : layout) if (candidate.m_sName == name) site = candidate;
  if (!site) { reason = "No base called " + name; return false; }
  vector pos = Vector(x, GetGame().GetWorld().GetSurfaceY(x, z), z);
  if (!ACR_Manager.OnMap(pos)) { reason = "That spot is off the map"; return false; }
  if (BBB_Terrain.IsWater(x, z)) { reason = "That spot is in water"; return false; }
  foreach (ACR_GenSite other : layout)
  {
   if (other == site) continue;
   float room = vector.DistanceXZ(other.m_vPos, pos) - other.m_fRadius - 10;
   if (room < 35) { reason = string.Format("Too close to %1: capture circles need room", other.m_sName); return false; }
  }
  if (hqs.Count() == 2 && (site == hqs[0] || site == hqs[1]))
  {
   ACR_GenSite otherHQ = hqs[0];
   if (site == hqs[0]) otherHQ = hqs[1];
   float minimum = 1000;
   if (size == ACR_ESkirmishSize.FLASHPOINT) minimum = 300;
   if (vector.DistanceXZ(otherHQ.m_vPos, pos) < minimum) { reason = string.Format("The two HQs must stay at least %1 m apart", (int)minimum); return false; }
  }
  float moved = vector.DistanceXZ(site.m_vPos, pos);
  s_Moves.Set(site.m_sId, pos);
  s_iMovesVersion++;
  reason = string.Format("%1 moved %2 m", name, (int)moved);
  PrintFormat("[ACR] GEN MOVE %1 -> %2 %3 (seed %4)", name, (int)x, (int)z, seed);
  return true;
 }

 // The place, or dry ground within 100 m of it.
 protected static bool Dry(ACR_GenSite site)
 {
  if (!BBB_Terrain.IsWater(site.m_vPos[0], site.m_vPos[2])) return true;
  for (int ring = 20; ring <= 100; ring += 20)
  {
   for (int step = 0; step < 12; step++)
   {
    float angle = Math.PI2 * step / 12.0;
    float x = site.m_vPos[0] + Math.Cos(angle) * ring;
    float z = site.m_vPos[2] + Math.Sin(angle) * ring;
    if (BBB_Terrain.IsWater(x, z)) continue;
    site.m_vPos = Vector(x, site.m_vPos[1], z);
    return true;
   }
  }
  return false;
 }

 // Fits a site among those placed: its capture circle shrinks (to 35 m at least) to stay 10 m
 // clear of every placed circle, like Saint-Philippe and its harbour on Everon. False = no room.
 protected static bool Fit(array<ref ACR_GenSite> placed, ACR_GenSite site)
 {
  float allowed = site.m_fRadius;
  foreach (ACR_GenSite other : placed)
  {
   float room = vector.DistanceXZ(other.m_vPos, site.m_vPos) - other.m_fRadius - 10;
   if (room < allowed) allowed = room;
  }
  if (allowed < 35) return false;
  site.m_fRadius = allowed;
  return true;
 }

 // Roughly half the ordinary sites in a seeded region: either a rotated, sliding band
 // (including edge halves) or the nearest sites round a seeded place. Landmarks follow the
 // boundary but do not define it, so a hidden-site roll cannot distort the battlefield.
 protected static array<ACR_GenSite> Half(array<ACR_GenSite> pool, RandomGenerator rng)
 {
  array<ACR_GenSite> ordinary = {};
  foreach (ACR_GenSite site : pool) if (site.m_sKind != "landmark") ordinary.Insert(site);
  array<ACR_GenSite> result = {};
  if (ordinary.Count() < 3) return result;
  float angle = Math.PI2 * rng.RandFloat01();
  vector axis = Vector(Math.Cos(angle), 0, Math.Sin(angle));
  int anchorIndex = Math.Min(ordinary.Count() - 1, Math.Floor(rng.RandFloat01() * ordinary.Count()));
  vector anchor = ordinary[anchorIndex].m_vPos;
  bool compact = rng.RandFloat01() < 0.35;
  int count = Math.Max(3, Math.Round((0.45 + 0.1 * rng.RandFloat01()) * ordinary.Count()));
  count = Math.Min(count, ordinary.Count());
  array<float> sorted = {};
  foreach (ACR_GenSite point : ordinary)
  {
   float metric = vector.Dot(point.m_vPos, axis);
   if (compact) metric = vector.DistanceXZ(point.m_vPos, anchor);
   sorted.Insert(metric);
  }
  sorted.Sort();
  int start = 0;
  if (!compact) start = Math.Min(ordinary.Count() - count, Math.Floor(rng.RandFloat01() * (ordinary.Count() - count + 1)));
  float low = -float.MAX;
  if (start > 0) low = 0.5 * (sorted[start - 1] + sorted[start]);
  float high = float.MAX;
  if (start + count < sorted.Count()) high = 0.5 * (sorted[start + count - 1] + sorted[start + count]);
  foreach (ACR_GenSite included : pool)
  {
   float position = vector.Dot(included.m_vPos, axis);
   if (compact) position = vector.DistanceXZ(included.m_vPos, anchor);
   if (position >= low && position <= high) result.Insert(included);
  }
  return result;
 }

 // An accessible local candidate pool; final selection keeps 5-7 actual objectives.
 // Radius and centre vary; sparse neighbourhoods fall back to the original 1.8 km envelope.
 protected static array<ACR_GenSite> Flashpoint(array<ACR_GenSite> pool, RandomGenerator rng)
 {
  array<ACR_GenSite> centres = {};
  foreach (ACR_GenSite site : pool) if (site.m_sKind == "city" || site.m_sKind == "town" || site.m_sKind == "village") centres.Insert(site);
  if (centres.IsEmpty()) foreach (ACR_GenSite ordinary : pool) if (ordinary.m_sKind != "landmark") centres.Insert(ordinary);
  array<ACR_GenSite> result = {};
  for (int attempt = 0; attempt < 20 && !centres.IsEmpty(); attempt++)
  {
   int pick = Math.Min(centres.Count() - 1, Math.Floor(rng.RandFloat01() * centres.Count()));
   ACR_GenSite centre = centres[pick];
   centres.Remove(pick);
   float radius = 1250 + 550 * rng.RandFloat01();
   for (int envelope = 0; envelope < 2; envelope++)
   {
    result.Clear();
    foreach (ACR_GenSite nearby : pool)
     if (vector.DistanceXZ(nearby.m_vPos, centre.m_vPos) <= radius) result.Insert(nearby);
    if (result.Count() >= 5) return result;
    radius = 1800;
   }
  }
  return result;
 }

 protected static void Farthest(array<ACR_GenSite> pool, out ACR_GenSite a, out ACR_GenSite b)
 {
  float best = -1;
  for (int i = 0; i < pool.Count(); i++)
   for (int j = i + 1; j < pool.Count(); j++)
   {
    float d = vector.DistanceXZ(pool[i].m_vPos, pool[j].m_vPos);
    if (d > best) { best = d; a = pool[i]; b = pool[j]; }
   }
 }

 // HQ pairs retain the 80% separation floor. Keep the original three best geometric
 // pairs as quality anchors and sample up to nine alternatives with at most two more rear
 // objectives. Only pairs within one accessible objective of the best checked pair compete.
 // Actual objectives still pass FairObjectives and ThinObjectives, regardless of the pair.
 protected static void PickHQs(array<ACR_GenSite> pool, bool flashpoint, RandomGenerator rng, out ACR_GenSite west, out ACR_GenSite east, float spanShare = 0.8)
 {
  array<ACR_GenSite> candidates = {};
  if (!flashpoint)
   foreach (ACR_GenSite site : pool) if (site.m_sKind == "military" || site.m_sKind == "airport") candidates.Insert(site);
  if (candidates.Count() < 2)
  {
   candidates.Clear();
   foreach (ACR_GenSite open : pool) if (open.m_sKind != "landmark") candidates.Insert(open);
  }
  // Never promote hidden landmarks, even on a sparse map.
  if (candidates.Count() < 2) return;
  if (flashpoint) { Farthest(candidates, west, east); return; }
  ACR_GenSite spanA, spanB;
  Farthest(candidates, spanA, spanB);
  if (!spanA || !spanB) return;
  float minimumSpan = spanShare * vector.DistanceXZ(spanA.m_vPos, spanB.m_vPos);
  array<int> allA = {}, allB = {}, rearCounts = {};
  array<float> distances = {};
  int bestRear = int.MAX;
  for (int i = 0; i < candidates.Count(); i++)
   for (int j = i + 1; j < candidates.Count(); j++)
   {
    float distance = vector.DistanceXZ(candidates[i].m_vPos, candidates[j].m_vPos);
    if (distance < minimumSpan) continue;
    int rear = 0;
    foreach (ACR_GenSite objective : pool)
     if (objective.m_sKind != "landmark" && !BetweenHQs(objective.m_vPos, candidates[i].m_vPos, candidates[j].m_vPos)) rear++;
    bestRear = Math.Min(bestRear, rear);
    allA.Insert(i); allB.Insert(j); rearCounts.Insert(rear); distances.Insert(distance);
   }
  array<int> shortlist = {};
  // The farthest three with the fewest rear objectives are always evaluated.
  for (int anchor = 0; anchor < 3; anchor++)
  {
   int farthest = -1;
   float span = -1;
   foreach (int p, float apart : distances)
    if (rearCounts[p] == bestRear && !shortlist.Contains(p) && apart > span) { span = apart; farthest = p; }
   if (farthest >= 0) shortlist.Insert(farthest);
  }
  array<int> alternatives = {};
  foreach (int option, int behind : rearCounts)
   if (behind <= bestRear + 2 && !shortlist.Contains(option)) alternatives.Insert(option);
  // Uniform sample without replacement; bounded graph work even when every town can be HQ.
  for (int extra = 0; extra < 9 && !alternatives.IsEmpty(); extra++)
  {
   int sampled = Math.Min(alternatives.Count() - 1, Math.Floor(rng.RandFloat01() * alternatives.Count()));
   shortlist.Insert(alternatives[sampled]);
   alternatives.Remove(sampled);
  }
  array<int> counts = {};
  int bestCount = -1;
  foreach (int pair : shortlist)
  {
   int retained = AccessibleCount(pool, candidates[allA[pair]], candidates[allB[pair]]);
   counts.Insert(retained);
   bestCount = Math.Max(bestCount, retained);
  }
  array<int> suitable = {};
  foreach (int checked, int total : counts)
   if (total >= bestCount - 1 && total >= 3) suitable.Insert(shortlist[checked]);
  // Archipelagos (29 Oct, Gulfcoast Islands): every widely separated pair can sit on islands
  // with no dry route between them. Only then accept closer pairs; other maps keep 80%.
  if (suitable.IsEmpty())
  {
   if (spanShare > 0.3) PickHQs(pool, flashpoint, rng, west, east, spanShare - 0.2);
   return;
  }
  int pick = Math.Min(suitable.Count() - 1, Math.Floor(rng.RandFloat01() * suitable.Count()));
  int choice = suitable[pick];
  west = candidates[allA[choice]];
  east = candidates[allB[choice]];
 }

 // ---------------------------------------------------------------- building it
 // Server, from ACR_Skirmish.ApplyOnce (after the faction pair is applied, before any tick).
 static void Build()
 {
  if (s_bBuilt) return;
  s_bBuilt = true;
  s_iSeed = Seed();
  int size = ACR_Skirmish.SizeOption();
  array<ref ACR_GenSite> layout = {};
  array<ACR_GenSite> hqs = {};
  if (!Plan(s_iSeed, size, layout, hqs))
  {
   Print("[ACR] GENERATOR failed: playing the world as placed");
   return;
  }
  string westSide = ACR_FactionSelection.West();
  string eastSide = ACR_FactionSelection.East();
  // Hand-built bases and pads go; their starting units follow their side to the new HQ.
  map<ACR_StartingUnit, string> unitSides = new map<ACR_StartingUnit, string>();
  foreach (ACR_StartingUnit unit : ACR_StartingUnit.s_All) unitSides.Set(unit, NearestOwner(unit.GetOrigin()));
  array<ACR_Base> old = {};
  old.Copy(ACR_Base.s_All);
  foreach (ACR_Base gone : old) if (!gone.m_bBuilt) gone.Retire("generated layout");
  foreach (ACR_Helipad pad : ACR_Helipad.s_All) pad.Drop();
  ACR_Skirmish.RecountPads();
  // The new bases.
  ACR_Base westHQ, eastHQ;
  foreach (ACR_GenSite site : layout)
  {
   string owner = "";
   if (site == hqs[0]) owner = westSide;
   else if (site == hqs[1]) owner = eastSide;
   ACR_Base base = MakeBase(site, owner);
   if (site == hqs[0]) westHQ = base;
   if (site == hqs[1]) eastHQ = base;
   s_Sites.Insert(site);
  }
  // Starting units: the world's own, else a default force per side, at each side's HQ.
  if (ACR_StartingUnit.s_All.IsEmpty())
  {
   DefaultForce(westHQ, true);
   DefaultForce(eastHQ, false);
  }
  else
  {
   foreach (ACR_StartingUnit u, string side : unitSides)
   {
    if (side == eastSide) u.m_MovedTo = eastHQ;
    else u.m_MovedTo = westHQ;
    if (u.m_MovedTo) u.SetOrigin(u.m_MovedTo.GetOrigin());
   }
  }
  // Player spawn points go to their side's HQ.
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
  {
   if (!point || ACR_SpawnPoints.IsBasePoint(point)) continue;
   string key = point.GetFactionKey();
   if (key == "US") key = westSide;
   else if (key == "USSR") key = eastSide;
   ACR_Base target = westHQ;
   if (key == eastSide) target = eastHQ;
   if (!target) continue;
   point.SetFactionKey(key);
   vector at = ACR_Manager.Ground(target.GetOrigin() + Vector(6, 0, 6));
   vector free;
   if (ACR_BaseSystem.ClearGround(free, at, 20, 1.5)) at = free;
   point.SetOrigin(at);
  }
  string sizeName = "full";
  if (size >= 0) sizeName = ACR_Skirmish.SizeName(size);
  PrintFormat("[ACR] GENERATED seed %1 size %2: %3 bases; HQs %4=%5 %6=%7, %8 m apart", s_iSeed, sizeName, layout.Count(), westSide, hqs[0].m_sName, eastSide, hqs[1].m_sName, (int)vector.DistanceXZ(hqs[0].m_vPos, hqs[1].m_vPos));
  foreach (ACR_GenSite s : layout) PrintFormat("[ACR] GEN BASE %1 | %2 | %3 %4 | v%5 r%6 | income %7", s.m_sName, s.m_sKind, (int)s.m_vPos[0], (int)s.m_vPos[2], s.m_iValue, (int)s.m_fRadius, s.m_bIncome);
 }

 protected static ACR_Base MakeBase(ACR_GenSite site, string owner)
 {
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  parameters.Transform[3] = ACR_Manager.Ground(site.m_vPos);
  ACR_Base base = ACR_Base.Cast(GetGame().SpawnEntity(ACR_Base, GetGame().GetWorld(), parameters));
  base.m_sName = site.m_sName;
  base.m_iValue = site.m_iValue;
  base.m_fRadius = site.m_fRadius;
  base.m_bIncome = site.m_bIncome;
  base.m_bHidden = site.m_sKind == "landmark";
  base.m_bAllowSpawn = true;
  base.m_bAllowReinforce = true;
  base.m_eLogisticsRole = ACR_EBaseRole.OBJECTIVE;
  if (site.m_sKind == "military" || site.m_sKind == "airport") base.m_eLogisticsRole = ACR_EBaseRole.MILITARY;
  if (site.m_sKind == "city" || site.m_sKind == "town" || site.m_sKind == "village" || site.m_sKind == "settlement") base.m_eLogisticsRole = ACR_EBaseRole.SETTLEMENT;
  base.m_iGarrison = 0;
  base.m_sOwner = owner;
  base.m_sCapturer = "";
  base.m_fScore = 0;
  if (owner != "")
  {
   base.m_eLogisticsRole = ACR_EBaseRole.HQ;
   base.m_bIncome = true;
   base.m_iValue = 2;
   base.m_iGarrison = 6;
   base.m_bHidden = false;
   base.m_fScore = 100;
  }
  base.m_iGarrisonLeft = base.m_iGarrison;
  return base;
 }

 // Two rifle squads, a weapons team, a truck and a car (as on Everon), in the side's colours
 // through the faction swaps.
 protected static void DefaultForce(ACR_Base hq, bool west)
 {
  if (!hq) return;
  if (west)
  {
   Unit(hq, "1st Squad", "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et", "");
   Unit(hq, "2nd Squad", "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et", "");
   Unit(hq, "Weapons Team", "{958039B857396B7B}Prefabs/Groups/BLUFOR/Group_US_MachineGunTeam.et", "");
   Unit(hq, "Truck 1", "", "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et");
   Unit(hq, "Humvee 1", "", "{4A71F755A4513227}Prefabs/Vehicles/Wheeled/M998/M1025.et");
   return;
  }
  Unit(hq, "Soviet 1st Squad", "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et", "");
  Unit(hq, "Soviet 2nd Squad", "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et", "");
  Unit(hq, "Soviet Weapons Team", "{A2F75E45C66B1C0A}Prefabs/Groups/OPFOR/Group_USSR_MachineGunTeam.et", "");
  Unit(hq, "Ural 1", "", "{16C1F16C9B053801}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et");
  Unit(hq, "UAZ 1", "", "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et");
 }

 protected static void Unit(ACR_Base hq, string name, ResourceName group, ResourceName vehicle)
 {
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  parameters.Transform[3] = hq.GetOrigin();
  ACR_StartingUnit unit = ACR_StartingUnit.Cast(GetGame().SpawnEntity(ACR_StartingUnit, GetGame().GetWorld(), parameters));
  unit.m_sName = name;
  unit.m_sGroup = group;
  unit.m_sVehicle = vehicle;
  unit.m_bCrewed = true;
  unit.m_MovedTo = hq;
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
