// What a zombie nest looks like (#107, ACR_Outbreak): its site is dressed when the nest settles
// (at the start, or where a horde starts a new one), so a player knows it's a nest before the dead
// pour out (ACR_Outbreak.NEST_RANGE, 300 m) and can find one from far off.
//  - While the nest lives: a column of black smoke over a burning wreck, seen kilometres away by
//    day, and the fire's glow at night. A bigger nest burns bigger (Tier: from 30 zombies). Cleared,
//    the fire goes out and the smoke thins away: smoke over a town means the dead are still there.
//    Particles don't replicate, so each client runs its own from the sites' state (FxTick), for the
//    FX_MAX nearest live nests within FX_RANGE. A dedicated server draws nothing.
//  - A ring of burnt-out cars round the site (2-4 by tier), on clear ground off the roads. Static
//    props with no RplComponent: the server picks the spots and every machine (server too, for
//    collision) lays them itself, like the map's own props. They stay when the nest is cleared.
//  - Remains (skeletons, replicated props the server spawns) and vehicle garbage, also staying.
//  - Blood on the ground and walls round the site and a burnt patch or two: decals, laid on every
//    client from the site (seeded from its position: the same on every machine).
// Nothing here cuts the navmesh: wrecks and piles go on clear ground away from roads and buildings.
// The sites reach clients through the game mode (m_aACRNestSites and friends, joiners too).
class ACR_NestDressing
{
 static const int MAX_SITES = 120;
 static const float SAME_SITE = 30;       // m: a site this close to a dressed one isn't dressed again
 static const float REACH = 20;           // m round the site
 static const float ROAD_GAP = 7;         // m a wreck keeps from a road
 static const float FX_RANGE = 5000;      // m from this machine's camera: live nests that smoke
 static const int FX_MAX = 12;            // at most this many, the nearest
 static const float FX_TICK_MS = 5000;
 static const int WRECK_KINDS = 5;
 // Nothing static to build: the sites this machine has dressed live on the game mode, so a second
 // match in the same session dresses them again (0.5.13 review).
 static void ACR_InitStatics()
 {
 }
 static int s_iDecals;                    // laid on this machine (tests)
 static int s_iFires;                     // live sites set burning on this machine
 static int s_iPiles;                     // spawned by the server
 static int s_iWrecks;                    // laid on this machine
 static int s_iRemains;                   // spawned by the server

 protected static const ResourceName SMOKE = "{C7C126B8FC38B083}Particles/Smoke_Building_Shop.ptc";
 protected static const ResourceName FIRE_SMALL = "{C0E2E7DC28B71E2C}Particles/Enviroment/Campfire_medium_normal.ptc";
 protected static const ResourceName FIRE_BIG = "{5A8C0DFF10B4CF3D}Particles/Fire_Building_Shop.ptc";

 protected static string Blood(int i)
 {
  if (i == 0) return "{F11E9EF96AC392A7}Assets/Decals/Blood/Data/BloodStain_Large_01.emat";
  if (i == 1) return "{AFF6B7BAD54E1CA8}Assets/Decals/Blood/Data/BloodStain_Large_02.emat";
  if (i == 2) return "{FE115E2F08E5B0D8}Assets/Decals/Blood/Data/BloodStain_Drops_01.emat";
  if (i == 3) return "{D49861D036BAD131}Assets/Decals/Blood/Data/BloodStain_Small_01.emat";
  return "{34C4CC186F4F2FE3}Assets/Decals/Blood/Decal_Blood3.emat";
 }

 protected static string Burnt(int i)
 {
  if (i == 0) return "{085CF0243B09A2CF}Assets/Decals/Miscellaneous/Damage/Damage_Burnt_01.emat";
  return "{6FE49D82D358226F}Assets/Decals/Miscellaneous/Damage/Damage_Burnt_02.emat";
 }

 protected static ResourceName Pile(int i)
 {
  if (i == 0) return "{A1AA6AAF5F88D859}Prefabs/Compositions/Misc/SubCompositions/Garbage/Garbage_Vehicle_01.et";
  if (i == 1) return "{CDFE5E4FE34C5777}Prefabs/Compositions/Misc/SubCompositions/Garbage/Garbage_Vehicle_02.et";
  if (i == 2) return "{286212B6EFA9C01C}Prefabs/Compositions/Misc/SubCompositions/Garbage/Garbage_Vehicle_03.et";
  return "{D5062960B7211EB4}Prefabs/Props/Construction/GarbageStack_01/GarbageStack_01_Long.et";
 }

 // Burnt-out civilian cars (static wrecks with no RplComponent: laid on every machine).
 protected static ResourceName Wreck(int i)
 {
  if (i == 0) return "{3386EAA823BE462E}Prefabs/Props/Wrecks/UAZ469_wreck/UAZ469_wreck_static.et";
  if (i == 1) return "{1A1CE09F7BCA6650}Prefabs/Props/Wrecks/S105_wreck/S105_wreck_static.et";
  if (i == 2) return "{002B8C09F0E7E371}Prefabs/Props/Wrecks/S1203_wreck/S1203_wreck_static.et";
  if (i == 3) return "{56FFF0C990358ED2}Prefabs/Props/Wrecks/UAZ452_wreck/UAZ452_wreck_static.et";
  return "{44277710A8E45F97}Prefabs/Props/Wrecks/UAZ469_wreck/UAZ469_wreck_rusty_static.et";
 }

 // Skeletal remains (replicated destructible props: the server spawns them).
 protected static ResourceName Remains(int i)
 {
  if (i == 0) return "{5DFCD4EEFB65250B}Prefabs/Props/Remains/SkeletonRemains/SkeletonRemains_01_V1.et";
  if (i == 1) return "{3B0DB043F1B159A2}Prefabs/Props/Remains/SkeletonRemains/SkeletonRemains_02_V2.et";
  return "{195D6C2708028DC5}Prefabs/Props/Remains/SkeletonRemains/SkeletonRemains_03_V3.et";
 }

 // How big a nest looks: 1 under 15 zombies, 2 under 30, 3 from 30 (ground zero starts at 60).
 static int Tier(int size)
 {
  if (size >= 30) return 3;
  if (size >= 15) return 2;
  return 1;
 }

 // Server: a nest of 'size' settles at 'site'. A new site is dressed (every machine lays it out; the
 // server adds the props); a known one (a horde retaking a cleared town) burns again.
 static void Add(vector site, int size)
 {
  if (!Replication.IsServer() || !GetGame()) return;
  // As a client gets it (decimetres), so the seeded layout is the same everywhere.
  site = Vector(Math.Round(site[0] * 10) * 0.1, site[1], Math.Round(site[2] * 10) * 0.1);
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (!mode) return;
  int tier = Tier(size);
  int index = mode.ACR_AddNestSite(site, tier);
  if (index < 0) return;
  Piles(site);
  Wrecks(mode, index, site, tier);
  PlaceRemains(site, tier);
  mode.ACR_ApplyNestSites();
  PrintFormat("[ACR] ZOMBIES nest dressed at %1: tier %2, %3 wrecks (%4 laid here), %5 remains, %6 piles so far", ACR_Manager.Grid(site), tier, mode.ACR_NestWreckCount(index), s_iWrecks, s_iRemains, s_iPiles);
 }

 // Server: the nest at 'at' is cleared. Its fire goes out on every client; the rest stays.
 static void Cleared(vector at)
 {
  if (!Replication.IsServer() || !GetGame()) return;
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (mode && mode.ACR_ClearNestSite(at)) mode.ACR_ApplyNestSites();
 }

 // One or two garbage piles on clear ground off the roads.
 protected static void Piles(vector site)
 {
  RandomGenerator rng = Rng(site, 7);
  int count = 1;
  if (rng.RandFloat01() < 0.5) count = 2;
  for (int i = 0; i < count; i++)
  {
   vector around = site + Offset(rng, 6, REACH);
   vector spot;
   if (!ACR_BaseSystem.ClearGround(spot, around, 25, 3) || vector.DistanceXZ(spot, site) > REACH * 2 || ACR_Minefields.RoadClearance(spot) < 5 || BBB_Terrain.IsWaterAt(spot)) continue;
   Resource resource = Resource.Load(Pile((int)(rng.RandFloat01() * 3.99)));
   if (!resource.IsValid()) continue;
   EntitySpawnParams params = new EntitySpawnParams();
   params.TransformMode = ETransformMode.WORLD;
   Math3D.AnglesToMatrix(Vector(rng.RandFloat01() * 360, 0, 0), params.Transform);
   params.Transform[3] = spot;
   if (GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params)) s_iPiles++;
  }
 }

 // Server: where the site's burnt-out cars go (tier + 1 of them), recorded for every machine.
 // Clear ground (no building, tree or prop), ROAD_GAP off the roads, out of water, apart.
 protected static void Wrecks(SCR_BaseGameMode mode, int index, vector site, int tier)
 {
  RandomGenerator rng = Rng(site, 11);
  array<vector> placed = {};
  int want = tier + 1;
  for (int tries = 0; tries < want * 3 && placed.Count() < want; tries++)
  {
   vector around = site + Offset(rng, 8, REACH);
   vector spot;
   if (!ACR_BaseSystem.ClearGround(spot, around, 12, 3.5)) continue;
   if (vector.DistanceXZ(spot, site) > REACH * 1.75 || ACR_Minefields.RoadClearance(spot) < ROAD_GAP || BBB_Terrain.IsWaterAt(spot)) continue;
   bool apart = true;
   foreach (vector other : placed) if (vector.DistanceXZ(other, spot) < 7) apart = false;
   if (!apart) continue;
   placed.Insert(spot);
   mode.ACR_AddNestWreck(index, spot, (int)(rng.RandFloat01() * 359.99), (int)(rng.RandFloat01() * (WRECK_KINDS - 0.01)));
  }
 }

 // Server: tier + 1 sets of remains near the middle of the site.
 protected static void PlaceRemains(vector site, int tier)
 {
  RandomGenerator rng = Rng(site, 13);
  for (int i = 0; i <= tier; i++)
  {
   vector around = site + Offset(rng, 2, REACH * 0.6);
   vector spot;
   if (!ACR_BaseSystem.ClearGround(spot, around, 6, 1, 1.5) || ACR_Minefields.RoadClearance(spot) < 3 || BBB_Terrain.IsWaterAt(spot)) continue;
   Resource resource = Resource.Load(Remains((int)(rng.RandFloat01() * 2.99)));
   if (!resource.IsValid()) continue;
   EntitySpawnParams params = new EntitySpawnParams();
   params.TransformMode = ETransformMode.WORLD;
   Math3D.AnglesToMatrix(Vector(rng.RandFloat01() * 360, 0, 0), params.Transform);
   params.Transform[3] = Vector(spot[0], GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]), spot[2]);
   if (GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params)) s_iRemains++;
  }
 }

 // Every machine: one recorded wreck.
 static void LayWreck(float x, float z, int yaw, int kind)
 {
  Resource resource = Resource.Load(Wreck(kind));
  if (!resource.IsValid()) return;
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(yaw, 0, 0), params.Transform);
  params.Transform[3] = Vector(x, GetGame().GetWorld().GetSurfaceY(x, z) - 0.1, z);
  if (GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), params)) s_iWrecks++;
 }

 // This machine lays out the site's blood and burns (once: 'dressed' is the game mode's list).
 static void DressLocal(vector site, notnull array<vector> dressed)
 {
  foreach (vector done : dressed) if (vector.DistanceXZ(done, site) < 1) return;
  dressed.Insert(site);
  if (System.IsConsoleApp()) return;
  RandomGenerator rng = Rng(site, 1);
  int blood = 6 + (int)(rng.RandFloat01() * 3.99);
  for (int b = 0; b < blood; b++) Decal(site + Offset(rng, 1, REACH * 0.8), Blood((int)(rng.RandFloat01() * 4.99)), 1 + rng.RandFloat01() * 2, rng.RandFloat01() * 360);
  int burns = 1;
  if (rng.RandFloat01() < 0.5) burns = 2;
  for (int n = 0; n < burns; n++) Decal(site + Offset(rng, 3, REACH * 0.7), Burnt((int)(rng.RandFloat01() * 1.99)), 5 + rng.RandFloat01() * 3, rng.RandFloat01() * 360);
 }

 // A decal on whatever is under 'at' (ground, road, a floor).
 protected static void Decal(vector at, string material, float size, float angle)
 {
  World world = GetGame().GetWorld();
  float ground = world.GetSurfaceY(at[0], at[2]);
  TraceParam trace = new TraceParam();
  trace.Start = Vector(at[0], ground + 3, at[2]);
  trace.End = Vector(at[0], ground - 6, at[2]);
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float hit = world.TraceMove(trace, null);
  if (hit >= 1 || !trace.TraceEnt) return;
  vector point = trace.Start + (trace.End - trace.Start) * hit;
  if (world.CreateDecal(trace.TraceEnt, point + Vector(0, 0.5, 0), Vector(0, -1, 0), 0, 1.5, angle * Math.DEG2RAD, size, 1, material, -1, 0xFFFFFFFF)) s_iDecals++;
 }

 // This machine's live smoke and fire: one repeating check (the game mode starts it with its first site).
 static void StartFx()
 {
  if (System.IsConsoleApp() || !GetGame()) return;
  GetGame().GetCallqueue().Remove(FxTick);
  GetGame().GetCallqueue().CallLater(FxTick, FX_TICK_MS, true);
  FxTick();
 }

 // A client that joined: lay out what the record holds even if the replication callback didn't
 // run for the first snapshot (0.5.13 review). Idempotent, like every apply.
 static void ApplyLater()
 {
  if (!GetGame()) return;
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (mode) mode.ACR_ApplyNestSites();
 }

 static void FxTick()
 {
  if (!GetGame()) return;
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (!mode) return;
  vector eye;
  CameraManager cameras = GetGame().GetCameraManager();
  if (cameras && cameras.CurrentCamera()) eye = cameras.CurrentCamera().GetOrigin();
  else
  {
   PlayerController controller = GetGame().GetPlayerController();
   if (!controller || !controller.GetControlledEntity()) return;
   eye = controller.GetControlledEntity().GetOrigin();
  }
  mode.ACR_UpdateNestFx(eye);
 }

 // The smoke and fire of a live site of 'tier' at 'at' (a wreck, or the site itself).
 static void Burn(vector at, int tier, notnull array<IEntity> into)
 {
  float ground = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
  ParticleEffectEntitySpawnParams params = new ParticleEffectEntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  params.Transform[3] = Vector(at[0], ground, at[2]);
  ParticleEffectEntity smoke = ParticleEffectEntity.SpawnParticleEffect(SMOKE, params);
  if (smoke) into.Insert(smoke);
  ResourceName fire = FIRE_SMALL;
  if (tier >= 3) fire = FIRE_BIG;
  ParticleEffectEntitySpawnParams fireParams = new ParticleEffectEntitySpawnParams();
  fireParams.TransformMode = ETransformMode.WORLD;
  fireParams.Transform[3] = Vector(at[0], ground + 0.3, at[2]);
  ParticleEffectEntity flames = ParticleEffectEntity.SpawnParticleEffect(fire, fireParams);
  if (flames) into.Insert(flames);
  if (smoke || flames) s_iFires++;
 }

 // Put out: the fire stops and the smoke thins away, then the effect deletes itself.
 static void PutOut(IEntity effect)
 {
  ParticleEffectEntity particles = ParticleEffectEntity.Cast(effect);
  if (!particles) return;
  particles.SetDeleteWhenStopped(true);
  particles.StopEmission();
 }

 protected static vector Offset(RandomGenerator rng, float near, float far)
 {
  float angle = rng.RandFloat01() * Math.PI2;
  float r = near + rng.RandFloat01() * (far - near);
  return Vector(Math.Cos(angle) * r, 0, Math.Sin(angle) * r);
 }

 // The same numbers on every machine for one site.
 protected static RandomGenerator Rng(vector site, int salt)
 {
  RandomGenerator rng = new RandomGenerator();
  rng.SetSeed((int)site[0] * 7919 + (int)site[2] * 104729 + salt);
  return rng;
 }
}

// The nest sites, for every machine (ACR_NestDressing): x and z in decimetres, in pairs; each
// site's state (its tier while the nest lives, minus it once cleared); its wrecks in fours (x and z
// in decimetres, the site, yaw * 8 + kind). The rest is this machine's own: the wrecks it has laid
// and its live smoke and fire.
modded class SCR_BaseGameMode
{
 [RplProp(onRplName: "ACR_ApplyNestSites")]
 protected ref array<int> m_aACRNestSites = {};
 [RplProp(onRplName: "ACR_ApplyNestSites")]
 protected ref array<int> m_aACRNestTiers = {};
 [RplProp(onRplName: "ACR_ApplyNestSites")]
 protected ref array<int> m_aACRNestWrecks = {};
 protected int m_iACRWrecksLaid;
 protected ref array<vector> m_aACRNestDressed = {}; // sites this machine has laid decals at
 protected bool m_bACRNestFx;
 protected ref array<IEntity> m_aACRNestFx = {};
 protected ref array<int> m_aACRNestFxSite = {};     // the site of each, and the tier it burns at
 protected ref array<int> m_aACRNestFxTier = {};

 // A new site's index, or -1: a known site within SAME_SITE (it burns again, at the larger tier)
 // or no room.
 int ACR_AddNestSite(vector site, int tier)
 {
  int sites = m_aACRNestSites.Count() / 2;
  for (int i = 0; i < sites; i++)
  {
   vector known = Vector(m_aACRNestSites[i * 2] * 0.1, 0, m_aACRNestSites[i * 2 + 1] * 0.1);
   if (vector.DistanceXZ(known, site) >= ACR_NestDressing.SAME_SITE) continue;
   if (i < m_aACRNestTiers.Count())
   {
    int burning = tier;
    int was = Math.AbsInt(m_aACRNestTiers[i]);
    if (was > burning) burning = was;
    if (m_aACRNestTiers[i] != burning)
    {
     PrintFormat("[ACR] ZOMBIES nest site %1 burns again (tier %2, was %3)", i, burning, m_aACRNestTiers[i]);
     m_aACRNestTiers[i] = burning;
     Replication.BumpMe();
     ACR_ApplyNestSites();
    }
   }
   return -1;
  }
  if (sites >= ACR_NestDressing.MAX_SITES) return -1;
  m_aACRNestSites.Insert(Math.Round(site[0] * 10));
  m_aACRNestSites.Insert(Math.Round(site[2] * 10));
  m_aACRNestTiers.Insert(tier);
  Replication.BumpMe();
  return sites;
 }

 void ACR_AddNestWreck(int site, vector at, int yaw, int kind)
 {
  m_aACRNestWrecks.Insert(Math.Round(at[0] * 10));
  m_aACRNestWrecks.Insert(Math.Round(at[2] * 10));
  m_aACRNestWrecks.Insert(site);
  m_aACRNestWrecks.Insert(yaw * 8 + kind);
  Replication.BumpMe();
 }

 int ACR_NestWreckCount(int site)
 {
  int count;
  for (int i = 0; i + 3 < m_aACRNestWrecks.Count(); i += 4) if (m_aACRNestWrecks[i + 2] == site) count++;
  return count;
 }

 // The live site nearest 'at' within SAME_SITE goes out; false with none.
 bool ACR_ClearNestSite(vector at)
 {
  int best = -1;
  float bestDistance = ACR_NestDressing.SAME_SITE;
  for (int i = 0; i < m_aACRNestTiers.Count() && i * 2 + 1 < m_aACRNestSites.Count(); i++)
  {
   if (m_aACRNestTiers[i] <= 0) continue;
   float d = vector.DistanceXZ(Vector(m_aACRNestSites[i * 2] * 0.1, 0, m_aACRNestSites[i * 2 + 1] * 0.1), at);
   if (d < bestDistance) { bestDistance = d; best = i; }
  }
  if (best < 0) return false;
  m_aACRNestTiers[best] = -m_aACRNestTiers[best];
  Replication.BumpMe();
  PrintFormat("[ACR] ZOMBIES nest site %1 put out (cleared)", best);
  return true;
 }

 // Tests: the state of the site within SAME_SITE of 'at' (its tier, minus it cleared, 0 none).
 int ACR_NestSiteTier(vector at)
 {
  for (int i = 0; i < m_aACRNestTiers.Count() && i * 2 + 1 < m_aACRNestSites.Count(); i++)
   if (vector.DistanceXZ(Vector(m_aACRNestSites[i * 2] * 0.1, 0, m_aACRNestSites[i * 2 + 1] * 0.1), at) < ACR_NestDressing.SAME_SITE) return m_aACRNestTiers[i];
  return 0;
 }

 int ACR_NestFxCount()
 {
  return m_aACRNestFx.Count();
 }

 override void EOnInit(IEntity owner)
 {
  super.EOnInit(owner);
  if (!GetGame().InPlayMode() || Replication.IsServer()) return;
  GetGame().GetCallqueue().CallLater(ACR_NestDressing.ApplyLater, 5000);
  GetGame().GetCallqueue().CallLater(ACR_NestDressing.ApplyLater, 20000);
 }

 // Every machine (the server after a change, clients when the record arrives): dress new sites,
 // lay new wrecks, keep the smoke going.
 void ACR_ApplyNestSites()
 {
  for (int i = 0; i + 1 < m_aACRNestSites.Count(); i += 2)
  {
   float x = m_aACRNestSites[i] * 0.1;
   float z = m_aACRNestSites[i + 1] * 0.1;
   ACR_NestDressing.DressLocal(Vector(x, GetGame().GetWorld().GetSurfaceY(x, z), z), m_aACRNestDressed);
  }
  while (m_iACRWrecksLaid * 4 + 3 < m_aACRNestWrecks.Count())
  {
   int w = m_iACRWrecksLaid * 4;
   int look = m_aACRNestWrecks[w + 3];
   ACR_NestDressing.LayWreck(m_aACRNestWrecks[w] * 0.1, m_aACRNestWrecks[w + 1] * 0.1, look / 8, look - (look / 8) * 8);
   m_iACRWrecksLaid++;
  }
  if (System.IsConsoleApp()) return;
  if (!m_bACRNestFx)
  {
   m_bACRNestFx = true;
   ACR_NestDressing.StartFx();
  }
  else ACR_NestDressing.FxTick();
 }

 // This machine: smoke and fire on the FX_MAX nearest live sites within FX_RANGE of 'eye'; any
 // other (cleared, out of range, burning at another tier) put out.
 void ACR_UpdateNestFx(vector eye)
 {
  array<int> want = {};
  array<float> distances = {};
  for (int i = 0; i < m_aACRNestTiers.Count() && i * 2 + 1 < m_aACRNestSites.Count(); i++)
  {
   if (m_aACRNestTiers[i] <= 0) continue;
   float d = vector.DistanceXZ(Vector(m_aACRNestSites[i * 2] * 0.1, 0, m_aACRNestSites[i * 2 + 1] * 0.1), eye);
   if (d > ACR_NestDressing.FX_RANGE) continue;
   int slot = 0;
   while (slot < distances.Count() && distances[slot] <= d) slot++;
   if (slot >= ACR_NestDressing.FX_MAX) continue;
   want.InsertAt(i, slot);
   distances.InsertAt(d, slot);
   if (want.Count() > ACR_NestDressing.FX_MAX)
   {
    want.Remove(want.Count() - 1);
    distances.Remove(distances.Count() - 1);
   }
  }
  // Put out what isn't wanted any more (pruned by index, from the end).
  for (int f = m_aACRNestFx.Count() - 1; f >= 0; f--)
  {
   int site = m_aACRNestFxSite[f];
   if (m_aACRNestFx[f] && want.Contains(site) && m_aACRNestTiers[site] == m_aACRNestFxTier[f]) continue;
   if (m_aACRNestFx[f]) ACR_NestDressing.PutOut(m_aACRNestFx[f]);
   m_aACRNestFx.Remove(f);
   m_aACRNestFxSite.Remove(f);
   m_aACRNestFxTier.Remove(f);
  }
  foreach (int lit : want)
  {
   if (m_aACRNestFxSite.Contains(lit)) continue;
   // Over the site's first wreck (a burning car), else the site itself.
   vector spot = Vector(m_aACRNestSites[lit * 2] * 0.1, 0, m_aACRNestSites[lit * 2 + 1] * 0.1);
   for (int w = 0; w + 3 < m_aACRNestWrecks.Count(); w += 4)
   {
    if (m_aACRNestWrecks[w + 2] != lit) continue;
    spot = Vector(m_aACRNestWrecks[w] * 0.1, 0, m_aACRNestWrecks[w + 1] * 0.1);
    break;
   }
   array<IEntity> made = {};
   ACR_NestDressing.Burn(spot, m_aACRNestTiers[lit], made);
   foreach (IEntity effect : made)
   {
    m_aACRNestFx.Insert(effect);
    m_aACRNestFxSite.Insert(lit);
    m_aACRNestFxTier.Insert(m_aACRNestTiers[lit]);
   }
  }
 }
}
