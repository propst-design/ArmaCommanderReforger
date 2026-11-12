// Zombies (#107): Project Outbreak's undead as a presence in the world, not a side, the way
// Insurgency (#89) treats FIA: no commander, no RP, hostile to both sides. ACA takes no dependency
// on Outbreak: it only looks for Outbreak's faction ("Z", which Outbreak adds to the stock faction
// manager our worlds use) and spawns the characters in that faction's catalog by prefab. Without
// Outbreak loaded there is no Z faction: no setup row, no nests, nothing changes.
// Outbreak's own AI (Creature Melee AI) picks targets by sight and sound and ignores waypoints, and
// Outbreak makes Z hostile to every other faction itself, so ACA only decides where and how many.
//  - Nests: a reserve of zombies at a site (an ACR_Defenders post, like an FIA cell) that pours out
//    when a soldier of either side comes within Range() and goes back in once nobody is near any
//    of them. Sites: some of the open (unheld) bases away from the starting bases, so a side must
//    clear a town before it can take it, and buildings in the country between them. A nest left
//    alone grows back to its size; one whose zombies are all killed is cleared.
//  - Spread: a full nest now and then sends half its zombies as a horde to another town 0.8-3 km
//    away. A horde is a nest on the move: while nobody is near it walks (its position, nobody to
//    see), near soldiers it pours out where it is, and back in it walks on. At the town it joins
//    the nest there or starts one, so cleared towns can be taken back by the dead. Soldiers the
//    zombies kill join the nearest nest or horde. A cap keeps the total in check.
//  - Night (the world's own night hours): nests wake from further out, grow back three times as
//    fast and spread three times as often, hordes walk twice as fast, more specials come out, and
//    at nightfall nests near a held base (not an HQ) send a raid at it. A raid comes out inside the
//    base, so its garrison fights; with nobody to fight it nests there until someone comes.
//  - Counterattacks (Poppi via Jacob, 7 Nov; Benny's Warfare QRF): a side that takes a town or
//    clears a nest has the dead come for it. Up to COUNTER_NESTS settled nests within COUNTER_REACH
//    (packed up, COUNTER_MIN or more) send a horde there, by day one nest a third of its zombies, at
//    night two nests half; it walks COUNTER_PACE times as fast. At a held base it comes out inside
//    like a raid (the garrison fights); at an empty place it nests unless someone is there. The side
//    is warned. One per place per COUNTER_GAP_MS.
//  - Specials (Outbreak's Giants and Nightingales, Special Zombies & Creatures' Mutant, known by
//    their components): one at most per spawn by day, two at night.
//  - Bases: zombies never hold, take or find a base. Inside one they block it: a held base is
//    "under attack" and can't secure itself, an open base's capture stops until they are dead.
//  - Seen like any enemy (ACR_Intel), named as zombies. A side that sees a pack (3 or more) or
//    loses a man to them marks a zombie area on its map (ACR_Insurgency.MarkZombies): its AI
//    commander sends a squad to clear it by day, never at night, and holds supply runs past it.
//  - ACA never runs a Z group as a squad (Outbreak's own spawns and the risen dead stay Outbreak's),
//    Z is never offered as a side, and nests are garrison groups, so BBB's squad behaviours don't
//    reach them.
// Option -acrZombies off|few|many (setup row "Zombies", shown only with Outbreak loaded; off by
// default). Never on flashpoints.
class ACR_ZombieNest : Managed
{
 int m_iSize;          // grows back to this many
 float m_fGrowAt;      // the next one back (0: not growing)
 string m_sBase;       // the base it sits in ("": out in the country)
 int m_iRisen;         // men the zombies killed while out: they join at pack-up
 float m_fHordeAt;     // a settled nest sends a horde from then
 bool m_bRoaming;      // a horde on its way to m_vGoal
 vector m_vGoal;
 string m_sGoal;
 ACR_Base m_Raid;      // a night raid on this held base (null: none)
 bool m_bCounter;      // a counterattack (walks faster)
 bool m_bGroundZero;   // where a contained outbreak started (ACR_Containment): refills and spreads fast
}

class ACR_Outbreak
{
 static const string FACTION = "Z";
 static const float NEST_RANGE = 300;      // a soldier this close: the nest pours out ...
 static const float NIGHT_RANGE = 450;     // ... at night
 static const float SPREAD = 40;           // m round the site they come out over (the town, not a clump)
 static const float STAY_RANGE = 350;      // out while a soldier is this close to any of them
 static const float GROW_MS = 300000;      // a nest left alone gets one back this often (night: a third)
 static const float HQ_CLEAR = 1200;       // m from a starting base (whole map)
 static const float BASE_CLEAR = 100;      // a country nest: m outside every base's circle
 static const float SPACING = 500;         // m between two nests
 static const int MAX_LOOKS = 40;          // building searches for country nests
 static const int NEST_MAX = 50;           // a nest never grows past this many (ground zero: ACR_Containment.GZ_MAX)
 static const float HORDE_MS = 1500000;    // a full nest sends a horde about this often (night: a third)
 static const int HORDE_MIN = 15;          // a nest needs this many to send one
 static const float WALK = 1.0;            // m/s a horde walks (night: twice)
 static const float GOAL_MIN = 800;        // a horde's next town is this far ...
 static const float GOAL_MAX = 3000;       // ... at most
 static const float RAID_REACH = 2500;     // nightfall: nests this close to a held base raid it
 static const float ZONE_RADIUS = 300;     // a side's zombie area
 static const int PACK = 3;                // zombies seen at once that mark an area
 static const float COUNTER_REACH = 2000;  // m: nests this close to a town just taken or cleared ...
 static const int COUNTER_NESTS = 2;       // ... at most this many (night; one by day) ...
 static const int COUNTER_MIN = 15;        // ... holding this many send a counterattack
 static const float COUNTER_PACE = 3;      // a counterattack walks this many times as fast
 static const float COUNTER_GAP_MS = 900000; // one per place per 15 min
 static float s_fWalkScaleForTest = 1;
 static int s_iCounters;                   // counterattacks sent (tests)

 protected static bool s_bOn;
 protected static int s_iCap;
 protected static bool s_bNight;
 protected static int s_iSeconds;
 protected static ref array<vector> s_aStarts;
 protected static ref array<ResourceName> s_aSpecial;
 protected static ref array<ResourceName> s_aPlain;
 protected static ref array<vector> s_aCounterAt;
 protected static ref array<float> s_aCounterTime;
 static void ACR_InitStatics()
 {
  s_aCounterAt = {};
  s_aCounterTime = {};
  s_aStarts = {};
  s_aSpecial = {};
  s_aPlain = {};
 }

 // Outbreak loaded: the world's faction manager carries Z.
 static bool Loaded()
 {
  if (!GetGame() || !GetGame().GetFactionManager()) return false;
  return GetGame().GetFactionManager().GetFactionByKey(FACTION) != null;
 }

 static bool On() { return s_bOn; }

 static bool IsZombies(string factionKey)
 {
  return factionKey == FACTION;
 }

 static bool IsZombie(IEntity body)
 {
  return body && IsZombies(ACR_Intel.FactionKeyOf(body));
 }

 static string Describe(int count)
 {
  if (count == 1) return "1 zombie";
  return count.ToString() + " zombies";
 }

 // off / few / many: base nests as a share of the open bases, country nests, zombies per nest.
 static string LevelOption()
 {
  string value = ACR_TestHooks.Get().Zombies();
  if (value == "" && !ACR_Options.Get("acrZombies", value)) value = "";
  value.ToLower();
  if (value == "few" || value == "many") return value;
  if (ACR_TvZ.On() || ACR_Containment.Asked()) return "few"; // TvZ and containment need zombies
  return "off";
 }

 // The world's own night hours (cached each second by Tick).
 static bool Night() { return s_bNight; }

 protected static bool ReadNight()
 {
  ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
  if (!world || !world.GetTimeAndWeatherManager()) return false;
  TimeAndWeatherManagerEntity time = world.GetTimeAndWeatherManager();
  return time.IsNightHour(time.GetTimeOfTheDay());
 }

 static float Range()
 {
  if (s_bNight) return NIGHT_RANGE;
  return NEST_RANGE;
 }

 // Server, after the guerrillas (ACR_Skirmish.ApplyOnce).
 static void Apply()
 {
  s_bOn = false;
  ACR_Containment.Reset();
  ACR_Truce.Reset();
  s_aCounterAt.Clear();
  s_aCounterTime.Clear();
  s_iCounters = 0;
  s_aStarts.Clear();
  s_iSeconds = 0;
  string level = LevelOption();
  if (level == "off") return;
  int size = ACR_Skirmish.SizeOption();
  if (ACR_Skirmish.s_Active) size = ACR_Skirmish.s_Active.m_eSize;
  if (size == ACR_ESkirmishSize.FLASHPOINT) return;
  if (!Loaded())
  {
   PrintFormat("[ACR] ZOMBIES none: no %1 faction in this world (Project Outbreak isn't loaded)", FACTION);
   return;
  }
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases) return;
  array<ResourceName> pool = manager.m_Bases.GetFaction(FACTION).m_aGarrison;
  if (pool.IsEmpty())
  {
   PrintFormat("[ACR] ZOMBIES none: the %1 faction lists no characters", FACTION);
   return;
  }
  Sort(pool);
  float share = 0.15;
  int country = 3;
  int minSize = 15;
  int maxSize = 25;
  s_iCap = 300;
  if (level == "many") { share = 0.3; country = 6; minSize = 25; maxSize = 40; s_iCap = 500; }
  float clearance = HQ_CLEAR;
  // TvZ: zombies are the only enemy (ACR_TvZ). The absent side's bases are already open, so the
  // starting bases below (clearance, horde goals) are the living side's alone.
  bool tvz = ACR_TvZ.On();
  if (tvz) { share = ACR_TvZ.Share(level); country = ACR_TvZ.Country(country); s_iCap = ACR_TvZ.Cap(s_iCap); }
  if (size == ACR_ESkirmishSize.HALF) { country = Math.Ceil(country * 0.5); clearance = 900; s_iCap = s_iCap / 2; }
  array<ACR_Base> open = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_bHidden) continue;
   if (!base.m_sOwner.IsEmpty() && !base.m_bPocket) s_aStarts.Insert(base.GetOrigin());
   else if (base.m_sOwner.IsEmpty() && !base.m_bBuilt) open.Insert(base);
  }
  array<ACR_Base> candidates = {};
  foreach (ACR_Base site : open) if (Clear(site.GetOrigin(), clearance)) candidates.Insert(site);
  int atBases = Math.Ceil(candidates.Count() * share);
  int placed;
  string names = "";
  // TvZ: the absent side's HQ is the hive, the biggest nest on the map.
  ACR_Base hive;
  ACR_DefenderPost hivePost;
  if (tvz) hive = ACR_TvZ.Hive();
  if (hive && candidates.Contains(hive))
  {
   candidates.RemoveItem(hive);
   hivePost = AddNest(hive.GetOrigin(), Math.Min(NEST_MAX, maxSize * 2), hive.m_sName);
   names += string.Format(" %1(%2, hive)", hivePost.m_sName, hivePost.m_iReserve);
   placed++;
  }
  // Containment (ACR_Containment): one ground zero (the hive in TvZ), nothing else at the start.
  if (ACR_Containment.Asked())
  {
   ACR_Base zeroTown = hive;
   ACR_DefenderPost zero = hivePost;
   if (!zero)
   {
    zeroTown = PickGroundZero(candidates);
    if (zeroTown) zero = AddNest(zeroTown.GetOrigin(), ACR_Containment.GZ_START, zeroTown.m_sName);
   }
   if (zero)
   {
    names += string.Format(" %1(%2, ground zero)", zeroTown.m_sName, zero.m_iReserve);
    if (!hivePost) placed++;
    atBases = placed;
    country = 0;
    ACR_Containment.Start(zero, zeroTown);
   }
  }
  while (placed < atBases && !candidates.IsEmpty())
  {
   int i = Math.RandomInt(0, candidates.Count());
   ACR_Base pick = candidates[i];
   candidates.Remove(i);
   if (Crowded(pick.GetOrigin(), null)) continue;
   // The cap holds from the start: no nest the cap can't hold at its smallest size.
   if (ACR_Outbreak.Room() < minSize) break;
   ACR_DefenderPost post = AddNest(pick.GetOrigin(), Math.Min(Math.RandomIntInclusive(minSize, maxSize), ACR_Outbreak.Room()), pick.m_sName);
   names += string.Format(" %1(%2)", post.m_sName, post.m_iReserve);
   placed++;
  }
  // Out in the country: buildings round the open bases, outside every base.
  int wild;
  int looks;
  while (wild < country && looks < MAX_LOOKS && !open.IsEmpty() && ACR_Outbreak.Room() >= minSize)
  {
   looks++;
   ACR_Base near = open.GetRandomElement();
   float angle = Math.RandomFloat(0, Math.PI2);
   float reach = near.m_fRadius + Math.RandomFloat(300, 800);
   vector around = near.GetOrigin() + Vector(Math.Cos(angle) * reach, 0, Math.Sin(angle) * reach);
   array<IEntity> buildings = {};
   ACR_Garrison.BuildingsWithin(around, 150, buildings);
   foreach (IEntity building : buildings)
   {
    vector at = building.GetOrigin();
    if (!Clear(at, clearance) || InsideBase(at) || Crowded(at, null) || BBB_Terrain.IsWaterAt(at)) continue;
    ACR_DefenderPost nest = AddNest(at, Math.Min(Math.RandomIntInclusive(minSize, maxSize), ACR_Outbreak.Room()), "");
    names += string.Format(" %1(%2)", nest.m_sName, nest.m_iReserve);
    wild++;
    break;
   }
  }
  s_bOn = placed + wild > 0;
  s_bNight = ReadNight();
  PrintFormat("[ACR] ZOMBIES %1: %2 nests in open bases, %3 in the country, %4 kinds of zombie (%5 special), cap %6, night %7:%8", level, placed, wild, pool.Count(), s_aSpecial.Count(), s_iCap, s_bNight, names);
  string kinds = "";
  foreach (ResourceName kind : pool) kinds += " " + Short(kind);
  PrintFormat("[ACR] ZOMBIES kinds:%1; special:%2", kinds, Specials());
 }

 protected static string Short(ResourceName prefab)
 {
  return FilePath.StripExtension(FilePath.StripPath(prefab));
 }

 static string Specials()
 {
  string names = "";
  foreach (ResourceName kind : s_aSpecial) names += " " + Short(kind);
  return names;
 }

 // Every enabled character in the Z faction's catalog (Outbreak, and add-ons that extend it): the
 // nests' garrison pool (ACR_FactionCatalog.BuildGarrisonPool).
 static void BuildPool(notnull array<ResourceName> pool)
 {
  pool.Clear();
  SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(FACTION));
  if (!faction) return;
  SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER, false);
  if (!catalog) return;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  foreach (SCR_EntityCatalogEntry entry : entries)
   if (entry.IsEnabled() && !entry.GetPrefab().IsEmpty() && !pool.Contains(entry.GetPrefab())) pool.Insert(entry.GetPrefab());
 }

 // Specials apart from the plain dead.
 protected static void Sort(notnull array<ResourceName> pool)
 {
  s_aSpecial.Clear();
  s_aPlain.Clear();
  foreach (ResourceName prefab : pool)
  {
   if (IsSpecial(prefab)) s_aSpecial.Insert(prefab);
   else s_aPlain.Insert(prefab);
  }
  if (s_aPlain.IsEmpty()) s_aPlain.Copy(pool);
 }

 // A Giant, Nightingale or Mutant: the creature's own component on the prefab or one it inherits.
 static bool IsSpecial(ResourceName prefab)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource || !resource.IsValid()) return false;
  BaseContainer container = resource.GetResource().ToBaseContainer();
  while (container)
  {
   BaseContainerList components = container.GetObjectArray("components");
   if (components)
   {
    for (int i = 0; i < components.Count(); i++)
    {
     string type = components.Get(i).GetClassName();
     if (type == "Z_GiantStatureComponent" || type == "Z_NightingaleComponent" || type == "POSZAC_MutantComponent") return true;
    }
   }
   container = container.GetAncestor();
  }
  return false;
 }

 // Who comes out: plain dead, now and then a special (one at most by day, two at night).
 static void Picks(int count, notnull array<ResourceName> picks)
 {
  picks.Clear();
  int limit = 1;
  if (s_bNight) limit = 2;
  int specials;
  for (int i = 0; i < count; i++)
  {
   if (!s_aSpecial.IsEmpty() && specials < limit && Math.RandomFloat01() < 0.15)
   {
    picks.Insert(s_aSpecial.GetRandomElement());
    specials++;
    continue;
   }
   if (!s_aPlain.IsEmpty()) picks.Insert(s_aPlain.GetRandomElement());
  }
 }

 static ACR_DefenderPost AddNest(vector at, int size, string baseName)
 {
  ACR_DefenderPost post = ACR_Defenders.Add(FACTION, at, "", 40);
  post.m_iReserve = size;
  post.m_fSpawnRange = NEST_RANGE;
  ACR_ZombieNest nest = new ACR_ZombieNest();
  nest.m_iSize = size;
  nest.m_sBase = baseName;
  nest.m_fHordeAt = GetGame().GetWorld().GetWorldTime() + HordeGap();
  post.m_Nest = nest;
  Name(post);
  ACR_NestDressing.Add(at, size);
  return post;
 }

 protected static void Name(ACR_DefenderPost post)
 {
  ACR_ZombieNest nest = post.m_Nest;
  if (nest.m_bRoaming) post.m_sName = string.Format("Zombie horde for %1", nest.m_sGoal);
  else if (!nest.m_sBase.IsEmpty()) post.m_sName = string.Format("Zombies at %1", nest.m_sBase);
  else post.m_sName = string.Format("Zombies %1", ACR_Manager.Grid(post.m_vPos));
 }

 // Where a nest or horde is, for messages and as the goal of the hordes it draws (never its label,
 // #241): the town it holds, the town a horde has reached, the town it's in, else a grid.
 static string Place(ACR_DefenderPost post)
 {
  ACR_ZombieNest nest = post.m_Nest;
  if (nest && !nest.m_bRoaming && !nest.m_sBase.IsEmpty()) return nest.m_sBase;
  if (nest && nest.m_bRoaming && !nest.m_sGoal.IsEmpty() && vector.DistanceXZ(post.m_vPos, nest.m_vGoal) < 400) return nest.m_sGoal;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (!base.m_bRetired && !base.m_bHidden && vector.DistanceXZ(post.m_vPos, base.GetOrigin()) < base.m_fRadius + 200) return base.m_sName;
  return ACR_Manager.Grid(post.m_vPos);
 }

 // A nest's next horde: ground zero sends them sooner (ACR_Containment).
 // Containment's stage speeds every nest up (ACR_Containment.Pace, 1 otherwise).
 protected static float NestHordeGap(ACR_ZombieNest nest)
 {
  if (!nest.m_bGroundZero) return HordeGap() / ACR_Containment.Pace();
  float gap = ACR_Containment.GZ_HORDE_MS;
  if (s_bNight) gap = gap / 3;
  return gap * Math.RandomFloat(0.7, 1.3) / ACR_Containment.Pace();
 }

 // Whether a settled nest has enough to send a horde, and how many go (0: not ready).
 // Containment (#240): ground zero from GZ_HORDE_MIN, other nests from twice HORDE_FLOOR, and a
 // horde is never under HORDE_FLOOR, so the spread doesn't halve town by town. Otherwise a full
 // nest of HORDE_MIN or more sends half.
 static int HordeCount(ACR_DefenderPost post)
 {
  ACR_ZombieNest nest = post.m_Nest;
  int least = ACR_Containment.HORDE_FLOOR;
  if (nest.m_bGroundZero)
  {
   if (post.m_iReserve < ACR_Containment.GZ_HORDE_MIN) return 0;
   return Math.Max(least, post.m_iReserve / 2);
  }
  if (ACR_Containment.On())
  {
   if (post.m_iReserve < least * 2) return 0;
   return Math.Max(least, post.m_iReserve / 2);
  }
  if (post.m_iReserve < HORDE_MIN || post.m_iReserve < nest.m_iSize) return 0;
  return post.m_iReserve / 2;
 }

 protected static float HordeGap()
 {
  float gap = HORDE_MS;
  if (s_bNight) gap = HORDE_MS / 3;
  if (ACR_TvZ.On()) gap *= ACR_TvZ.HORDE_SCALE;
  return gap * Math.RandomFloat(0.7, 1.3);
 }

 protected static bool Clear(vector at, float clearance)
 {
  foreach (vector start : s_aStarts) if (vector.DistanceXZ(at, start) < clearance) return false;
  return true;
 }

 // Ground zero (ACR_Containment): among the open towns far enough from the starting bases, one of
 // the three furthest from the nearest of them (null: none).
 protected static ACR_Base PickGroundZero(notnull array<ACR_Base> candidates)
 {
  array<ACR_Base> picks = {};
  array<float> spans = {};
  foreach (ACR_Base base : candidates)
  {
   float nearest = 100000;
   foreach (vector start : s_aStarts) nearest = Math.Min(nearest, vector.DistanceXZ(base.GetOrigin(), start));
   int at = 0;
   while (at < spans.Count() && spans[at] >= nearest) at++;
   picks.InsertAt(base, at);
   spans.InsertAt(nearest, at);
  }
  if (picks.IsEmpty()) return null;
  int top = picks.Count();
  if (top > 3) top = 3;
  return picks[Math.RandomInt(0, top)];
 }

 // The most a nest holds: ground zero has its own, bigger ceiling (ACR_Containment.GZ_MAX).
 static int MaxOf(ACR_ZombieNest nest)
 {
  if (nest && nest.m_bGroundZero) return ACR_Containment.GZ_MAX;
  return NEST_MAX;
 }

 // Zombies the cap still allows.
 static int Room()
 {
  return Math.Max(0, s_iCap - Total());
 }

 protected static bool InsideBase(vector at)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (!base.m_bRetired && vector.DistanceXZ(at, base.GetOrigin()) < base.m_fRadius + BASE_CLEAR) return true;
  return false;
 }

 protected static bool Crowded(vector at, ACR_DefenderPost except)
 {
  return NestNear(at, SPACING, except) != null;
 }

 // A settled nest within range of at (null: none).
 protected static ACR_DefenderPost NestNear(vector at, float range, ACR_DefenderPost except)
 {
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
   if (post != except && post.m_Nest && !post.m_Nest.m_bRoaming && !post.m_bGone && vector.DistanceXZ(post.m_vPos, at) < range) return post;
  return null;
 }

 // Every zombie ACA keeps: in reserve, out, and the dead waiting to join.
 static int Total()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  int total;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Nest || post.m_bGone) continue;
   total += post.m_iReserve + post.m_Nest.m_iRisen;
   if (post.m_Group && manager && manager.m_Bases) total += manager.m_Bases.GuardsAlive(post.m_Group);
  }
  return total;
 }

 // ---------------------------------------------------------------- every second (ACR_Manager.Tick)

 static void Tick()
 {
  if (!s_bOn) return;
  s_iSeconds++;
  if (s_iSeconds % 10 != 0) return;
  bool night = ReadNight();
  if (night == s_bNight) return;
  s_bNight = night;
  if (night) Nightfall();
  else PrintFormat("[ACR] ZOMBIES day breaks");
 }

 // Nests near a held base (not an HQ, except in TvZ) send half their zombies at it, one raid per base.
 static void Nightfall()
 {
  int raids;
  array<ACR_Base> raided = {};
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   ACR_ZombieNest nest = post.m_Nest;
   if (!nest || nest.m_bRoaming || post.m_bGone || post.m_Group || post.m_iReserve < 6 || Math.RandomFloat01() > 0.6) continue;
   ACR_Base target;
   float best = RAID_REACH;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_bRetired || base.m_sOwner.IsEmpty() || base.m_bPocket || (ACR_LogisticsRules.IsHQ(base) && !ACR_TvZ.On()) || raided.Contains(base)) continue;
    float d = vector.DistanceXZ(base.GetOrigin(), post.m_vPos);
    if (d < best) { best = d; target = base; }
   }
   if (!target) continue;
   raided.Insert(target);
   if (SendHorde(post, post.m_iReserve / 2, target.GetOrigin(), target.m_sName, target)) raids++;
  }
  PrintFormat("[ACR] ZOMBIES nightfall: %1 raids on held bases", raids);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  array<string> sides = {ACR_FactionSelection.West(), ACR_FactionSelection.East()};
  foreach (string side : sides)
  {
   ACR_Intel intel = manager.IntelFor(side);
   if (intel) intel.AddFeed(GetGame().GetWorld().GetWorldTime(), "Night falls. The dead are restless");
  }
 }

 // A horde of 'count' out of 'from' towards 'goal'; false if there aren't enough.
 static bool SendHorde(ACR_DefenderPost from, int count, vector goal, string goalName, ACR_Base raid = null)
 {
  if (count < 4 || from.m_iReserve < count) return false;
  from.m_iReserve -= count;
  if (from.m_iReserve < from.m_Nest.m_iSize && from.m_Nest.m_fGrowAt <= 0) from.m_Nest.m_fGrowAt = GetGame().GetWorld().GetWorldTime() + NestGrowGap(from.m_Nest);
  ACR_DefenderPost horde = ACR_Defenders.Add(FACTION, from.m_vPos, "", 40);
  horde.m_iReserve = count;
  horde.m_fSpawnRange = NEST_RANGE;
  ACR_ZombieNest nest = new ACR_ZombieNest();
  nest.m_iSize = count;
  nest.m_bRoaming = true;
  nest.m_vGoal = goal;
  nest.m_sGoal = goalName;
  nest.m_Raid = raid;
  horde.m_Nest = nest;
  Name(horde);
  string kind = "horde";
  if (raid) kind = "raid";
  PrintFormat("[ACR] ZOMBIES %1 of %2 from %3 to %4 (%5 m)", kind, count, from.m_sName, goalName, (int)vector.DistanceXZ(from.m_vPos, goal));
  // Containment: people are holed up in a town the dead are coming for, unless they already nest there.
  if (!raid && ACR_Containment.On() && !NestNear(goal, 200, null)) ACR_Containment.AddSurvivors(goal, goalName);
  return true;
 }

 // A town 0.8-3 km from 'from' for a horde: open bases away from the starting bases, ones with no
 // nest likelier (the dead spread). False with none.
 static bool PickGoal(vector from, out vector goal, out string goalName)
 {
  array<ACR_Base> picks = {};
  array<float> weights = {};
  float total;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_bHidden || !base.m_sOwner.IsEmpty()) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), from);
   if (d < GOAL_MIN || d > GOAL_MAX || !Clear(base.GetOrigin(), HQ_CLEAR * 0.75)) continue;
   float weight = 3;
   if (NestNear(base.GetOrigin(), 200, null)) weight = 1;
   picks.Insert(base);
   weights.Insert(weight);
   total += weight;
  }
  if (picks.IsEmpty()) return false;
  float roll = Math.RandomFloat(0, total);
  foreach (int i, ACR_Base pick : picks)
  {
   roll -= weights[i];
   if (roll > 0 && i < picks.Count() - 1) continue;
   goal = pick.GetOrigin();
   goalName = pick.m_sName;
   return true;
  }
  return false;
 }

 protected static float NestGrowGap(ACR_ZombieNest nest)
 {
  float gap = GrowGap();
  if (nest.m_bGroundZero) gap = ACR_Containment.GZ_GROW_MS;
  else if (ACR_Containment.On()) gap = ACR_Containment.NEST_GROW_MS;
  if (s_bNight && (nest.m_bGroundZero || ACR_Containment.On())) gap = gap / 3;
  return gap / ACR_Containment.Pace();
 }

 protected static float GrowGap()
 {
  if (s_bNight) return GROW_MS / 3;
  return GROW_MS;
 }

 // ---------------------------------------------------------------- ACR_Defenders hooks

 // Every 5 s while packed up: grow back, send a horde, walk. True: come out now (a raid at its base).
 static bool TickPost(ACR_Manager manager, ACR_DefenderPost post)
 {
  ACR_ZombieNest nest = post.m_Nest;
  float now = GetGame().GetWorld().GetWorldTime();
  if (nest.m_bRoaming) return Walk(post);
  if (nest.m_fGrowAt > 0 && now >= nest.m_fGrowAt && post.m_iReserve > 0)
  {
   nest.m_fGrowAt = 0;
   if (Total() < s_iCap) post.m_iReserve++;
   if (post.m_iReserve < nest.m_iSize) nest.m_fGrowAt = now + NestGrowGap(nest);
  }
  if (now >= nest.m_fHordeAt)
  {
   vector goal;
   string goalName;
   int count = HordeCount(post);
   bool sent = count > 0 && PickGoal(post.m_vPos, goal, goalName) && SendHorde(post, count, goal, goalName);
   nest.m_fHordeAt = now + NestHordeGap(nest);
   // Containment (#240): a nest not ready yet looks again soon instead of waiting a whole gap.
   if (!sent && ACR_Containment.On()) nest.m_fHordeAt = now + ACR_Containment.RETRY_MS;
  }
  return false;
 }

 // A horde walks on towards its town; there, it joins the nest or starts one.
 protected static bool Walk(ACR_DefenderPost post)
 {
  ACR_ZombieNest nest = post.m_Nest;
  if (nest.m_Raid && (nest.m_Raid.m_sOwner.IsEmpty() || nest.m_Raid.m_bRetired)) nest.m_Raid = null;
  vector to = nest.m_vGoal - post.m_vPos;
  to[1] = 0;
  float left = to.Length();
  float step = WALK * 5 * s_fWalkScaleForTest;
  if (s_bNight) step *= 2;
  if (nest.m_bCounter) step *= COUNTER_PACE;
  // A raid comes out inside its base (the garrison fights it).
  if (nest.m_Raid && left <= step + 150)
  {
   post.m_vPos = ACR_Manager.Ground(nest.m_vGoal - to.Normalized() * Math.Min(left, nest.m_Raid.m_fRadius * 0.5));
   return true;
  }
  if (left <= step + 20)
  {
   // Arrive at the goal itself: overrun reach and the settled nest go by m_vPos (a test's
   // fast walk takes km-long steps).
   post.m_vPos = ACR_Manager.Ground(nest.m_vGoal);
   Arrive(post);
   return false;
  }
  vector next = ACR_Manager.Ground(post.m_vPos + to.Normalized() * step);
  if (BBB_Terrain.IsWaterAt(next))
  {
   vector goal;
   string goalName;
   if (!PickGoal(post.m_vPos, goal, goalName) || goal == nest.m_vGoal) { Arrive(post); return false; }
   nest.m_vGoal = goal;
   nest.m_sGoal = goalName;
   Name(post);
   return false;
  }
  post.m_vPos = next;
  return false;
 }

 protected static void Arrive(ACR_DefenderPost post)
 {
  ACR_ZombieNest nest = post.m_Nest;
  // Containment: survivors there with nobody guarding them join it.
  post.m_iReserve += ACR_Containment.HordeArrives(ACR_Manager.GetInstance(), post, ACR_Outbreak.Room());
  ACR_DefenderPost there = NestNear(nest.m_vGoal, 200, post);
  if (there && there.m_iReserve + post.m_iReserve <= MaxOf(there.m_Nest))
  {
   there.m_iReserve += post.m_iReserve;
   there.m_Nest.m_iSize = Math.Min(MaxOf(there.m_Nest), Math.Max(there.m_Nest.m_iSize, there.m_iReserve));
   PrintFormat("[ACR] ZOMBIES %1 joins %2 (%3 now)", post.m_sName, there.m_sName, there.m_iReserve);
   post.m_iReserve = 0;
   ACR_Defenders.Remove(post);
   return;
  }
  Settle(post, nest.m_sGoal);
 }

 // Stops walking: a nest where it is.
 protected static void Settle(ACR_DefenderPost post, string baseName)
 {
  ACR_ZombieNest nest = post.m_Nest;
  nest.m_bRoaming = false;
  nest.m_Raid = null;
  nest.m_sBase = baseName;
  nest.m_iSize = Math.Max(nest.m_iSize, post.m_iReserve);
  // Containment (#240): a town the dead take regrows to at least NEST_FLOOR, enough to send a full
  // horde on, so the spread doesn't thin out town by town.
  if (ACR_Containment.On()) nest.m_iSize = Math.Min(NEST_MAX, Math.Max(nest.m_iSize, ACR_Containment.NEST_FLOOR));
  float now = GetGame().GetWorld().GetWorldTime();
  nest.m_fHordeAt = now + NestHordeGap(nest);
  if (post.m_iReserve < nest.m_iSize && nest.m_fGrowAt <= 0) nest.m_fGrowAt = now + NestGrowGap(nest);
  string was = post.m_sName;
  Name(post);
  ACR_NestDressing.Add(post.m_vPos, nest.m_iSize);
  PrintFormat("[ACR] ZOMBIES %1 settles: %2 (%3)", was, post.m_sName, post.m_iReserve);
 }

 // While out: a living soldier near any of them keeps them out (they chase); a raid also stays
 // while its base's garrison is out.
 static bool StayOut(ACR_Manager manager, ACR_DefenderPost post)
 {
  ACR_Base raid = post.m_Nest.m_Raid;
  if (raid && raid.m_GarrisonGroup && manager.m_Bases.GuardsAlive(raid.m_GarrisonGroup) > 0) return true;
  // TvZ: a raid holds on in a base it can take until it falls (ACR_TvZ overrun).
  if (raid && ACR_TvZ.CanFall(raid)) return true;
  if (ACR_Defenders.EnemyWithin(manager, FACTION, post.m_vPos, STAY_RANGE)) return true;
  array<AIAgent> agents = {};
  post.m_Group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity zombie = agent.GetControlledEntity();
   if (zombie && ACR_Respawn.IsAlive(zombie) && ACR_Defenders.EnemyWithin(manager, FACTION, zombie.GetOrigin(), STAY_RANGE)) return true;
  }
  return false;
 }

 // Back in (ACR_Defenders.Tick, survivors already back in the reserve): the dead they made join;
 // with none left it's cleared; a horde walks on from where they are; a raid nests in its base.
 static void PackedUp(ACR_Manager manager, ACR_DefenderPost post, vector where)
 {
  ACR_ZombieNest nest = post.m_Nest;
  if (post.m_iReserve > 0 && nest.m_iRisen > 0)
  {
   post.m_iReserve = Math.Min(MaxOf(nest), post.m_iReserve + nest.m_iRisen);
   PrintFormat("[ACR] ZOMBIES %1: %2 of the dead join them", post.m_sName, nest.m_iRisen);
  }
  nest.m_iRisen = 0;
  if (post.m_iReserve <= 0)
  {
   PrintFormat("[ACR] ZOMBIES %1 cleared", post.m_sName);
   // Containment pays the sides that cleared it (ACR_Containment.Bounty).
   int bounty = ACR_Containment.Bounty(post);
   string place = Place(post);
   array<string> sides = {ACR_FactionSelection.West(), ACR_FactionSelection.East()};
   // Split between the sides that fought there (Jacob, 12 Nov), not paid to each in full.
   int fought;
   foreach (string present : sides) if (SideNear(manager, present, post.m_vPos, STAY_RANGE + 200)) fought++;
   int share = bounty;
   if (fought > 1) share = bounty / fought;
   foreach (string side : sides)
   {
    if (!SideNear(manager, side, post.m_vPos, STAY_RANGE + 200)) continue;
    string line = string.Format("Zombies cleared at %1", place);
    int paid = ACR_Containment.Give(manager, side, share);
    if (paid > 0) line = string.Format("Zombies cleared at %1: +%2 RP", place, paid);
    if (paid > 0) PrintFormat("[ACR] CONTAIN %1 paid %2 RP for clearing %3", side, paid, place);
    ACR_Intel intel = manager.IntelFor(side);
    if (intel) intel.AddFeed(GetGame().GetWorld().GetWorldTime(), line, post.m_vPos);
   }
   string clearedBy = "";
   foreach (string near : sides) if (clearedBy.IsEmpty() && SideNear(manager, near, post.m_vPos, STAY_RANGE + 200)) clearedBy = near;
   ACR_Containment.Cleared(post);
   ACR_NestDressing.Cleared(post.m_vPos);
   ACR_Defenders.Remove(post);
   if (!clearedBy.IsEmpty()) CounterAttack(post.m_vPos, place, null, clearedBy);
   return;
  }
  if (nest.m_Raid)
  {
   Settle(post, nest.m_Raid.m_sName);
   return;
  }
  if (nest.m_bRoaming)
  {
   if (where != vector.Zero) post.m_vPos = where;
   return;
  }
  if (post.m_iReserve < nest.m_iSize) nest.m_fGrowAt = GetGame().GetWorld().GetWorldTime() + NestGrowGap(nest);
 }

 // 'side' took a town or cleared a nest at 'at' ('base': the town, null for a country nest): the
 // dead near it come for it. Returns how many hordes set out.
 static int CounterAttack(vector at, string name, ACR_Base base, string side)
 {
  if (!s_bOn || !Replication.IsServer() || side.IsEmpty() || IsZombies(side)) return 0;
  float now = GetGame().GetWorld().GetWorldTime();
  for (int c = s_aCounterAt.Count() - 1; c >= 0; c--)
  {
   if (now - s_aCounterTime[c] > COUNTER_GAP_MS) { s_aCounterAt.Remove(c); s_aCounterTime.Remove(c); continue; }
   if (vector.DistanceXZ(s_aCounterAt[c], at) < 300) return 0;
  }
  int most = 1;
  float share = 0.34;
  if (s_bNight) { most = COUNTER_NESTS; share = 0.5; }
  // The nearest nests first.
  array<ACR_DefenderPost> near = {};
  array<float> spans = {};
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Nest || post.m_Nest.m_bRoaming || post.m_bGone || post.m_Group || post.m_iReserve < COUNTER_MIN) continue;
   float d = vector.DistanceXZ(post.m_vPos, at);
   if (d > COUNTER_REACH || d < 200) continue;
   int i = 0;
   while (i < spans.Count() && spans[i] <= d) i++;
   near.InsertAt(post, i);
   spans.InsertAt(d, i);
  }
  ACR_Base raid;
  if (base && !base.m_sOwner.IsEmpty() && !base.m_bRetired) raid = base;
  int sent;
  foreach (ACR_DefenderPost from : near)
  {
   if (sent >= most) break;
   int count = Math.Max(4, Math.Round(from.m_iReserve * share));
   // Containment (#240): a counterattack is a horde too, 15 or more where the nest can spare them.
   if (ACR_Containment.On()) count = Math.Max(count, Math.Min(ACR_Containment.HORDE_FLOOR, from.m_iReserve - 5));
   if (!SendHorde(from, count, at, name, raid)) continue;
   foreach (ACR_DefenderPost horde : ACR_Defenders.s_Posts)
    if (horde.m_Nest && horde.m_Nest.m_bRoaming && horde.m_Nest.m_vGoal == at && !horde.m_Nest.m_bCounter) { horde.m_Nest.m_bCounter = true; horde.m_sName = string.Format("Zombies counterattacking %1", name); }
   sent++;
  }
  if (sent == 0) return 0;
  s_aCounterAt.Insert(at);
  s_aCounterTime.Insert(now);
  s_iCounters += sent;
  PrintFormat("[ACR] ZOMBIES counterattack on %1 (%2 took it): %3 hordes", name, side, sent);
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(side);
  if (intel) intel.AddFeed(now, string.Format("The dead are stirring near %1. Expect them soon", name), at);
  return sent;
 }

 // A man of 'side' killed at 'pos' (ACR_Report): by zombies, he joins the nearest nest or horde
 // (within the cap), and his side marks the place.
 static void OnLoss(string side, vector pos, string by)
 {
  if (!s_bOn || !IsZombies(by) || side.IsEmpty() || IsZombies(side) || !Replication.IsServer()) return;
  ACR_DefenderPost nearest;
  float best = 400;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Nest || post.m_bGone) continue;
   float d = vector.DistanceXZ(post.m_vPos, pos);
   if (d < best) { best = d; nearest = post; }
  }
  if (nearest && Total() < s_iCap)
  {
   if (nearest.m_Group) nearest.m_Nest.m_iRisen++;
   else nearest.m_iReserve = Math.Min(MaxOf(nearest.m_Nest), nearest.m_iReserve + 1);
  }
  ACR_Insurgency.MarkZombies(side, pos, string.Format("Zombies killed our men at %1: that area is dangerous", ACR_Manager.Grid(pos)));
 }

 // 'side' saw 'count' zombies at 'pos' (ACR_Intel.Report): a pack marks a zombie area.
 static int s_iSightings; // zombie contacts reported, any side (srv:zombies)
 static void Sighted(string side, vector pos, int count)
 {
  s_iSightings++;
  if (!s_bOn || count < PACK) return;
  ACR_Insurgency.MarkZombies(side, pos, "");
 }

 // Zombies take no waypoints (Outbreak's AI picks its own way); a group with one tries to move to
 // it anyway and the game's group movement fails on it (cc107r1).
 static void NoOrders(SCR_AIGroup group)
 {
  array<AIWaypoint> waypoints = {};
  group.GetWaypoints(waypoints);
  foreach (AIWaypoint waypoint : waypoints)
  {
   group.RemoveWaypoint(waypoint);
   SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
  }
 }

 // Where its living zombies are (the middle of them), vector.Zero with none.
 static vector Centre(ACR_Manager manager, SCR_AIGroup group)
 {
  if (!group) return vector.Zero;
  vector sum = vector.Zero;
  int n;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity zombie = agent.GetControlledEntity();
   if (!zombie || !ACR_Respawn.IsAlive(zombie)) continue;
   sum = sum + zombie.GetOrigin();
   n++;
  }
  if (n == 0) return vector.Zero;
  return ACR_Manager.Ground(sum * (1.0 / n));
 }

 // A living soldier of 'side' (a squad's or a player) within range of pos.
 static bool SideNear(ACR_Manager manager, string side, vector pos, float range)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != side) continue;
   IEntity anchor = squad.GetAnchor();
   if (anchor && vector.DistanceXZ(anchor.GetOrigin(), pos) <= range) return true;
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (player && ACR_Respawn.IsAlive(player) && ACR_Intel.FactionKeyOf(player) == side && vector.DistanceXZ(player.GetOrigin(), pos) <= range) return true;
  }
  return false;
 }
}
