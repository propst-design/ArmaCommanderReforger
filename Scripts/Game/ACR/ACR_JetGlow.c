// Nozzle glows on AC: Actual's period jets at night (Jacob, 19 Nov 2026): a faint dull-orange ring of hot pipe in each
// turbojet's nozzle, seen only from behind. The Su-25's two R-95Sh and the recon jets' engines glow; the RF-4C and
// Su-17M4R also light their afterburners for the dash into and out of the recon corridor (a reheat plume with
// shock-diamond pinches). The A-10's turbofans run cool: none (Jacob: "Not the A-10 obviously").
//
// Purely visual and local: ACR_LocalFlyover, which flies each machine's own copy of the jet, owns one of these per
// period jet and steps it with the plane. The glow and plume are non-replicated mesh entities with no collision (the
// prefabs Prefabs/Vehicles/Aircraft/ACR_NozzleGlow / ACR_NozzleBurner, meshes from tools/blender/build_aircraft.py),
// spawned the first time one should show and deleted with the plane. A dedicated server never flies a local copy, so
// nothing here runs there, and nothing the AI or the air defence sees changes.
//
// Brightness = night x tail cone x distance (x a little more at full throttle / on the attack run):
//  - night: 0 by day, fading in over DUSK_H after sunset and out over DUSK_H before sunrise (the world's own sun
//    times for its date and latitude);
//  - tail cone: full within CONE_FULL_DEG of dead astern, gone by CONE_EDGE_DEG: a jet coming at you shows nothing;
//  - distance: full to FADE_FROM_M, gone by GONE_M, so it doesn't give the route away from far off.
// A nozzle a third of a metre across is under a pixel at a kilometre, so the ring is drawn no smaller than
// MIN_ANGLE across the viewer's eye (a faint dot far off, the ring itself close in).
class ACR_JetGlow : Managed
{
 static const ResourceName GLOW = "{AC230926A00100EC}Prefabs/Vehicles/Aircraft/ACR_NozzleGlow.et";
 static const ResourceName BURNER = "{AC230926A00100ED}Prefabs/Vehicles/Aircraft/ACR_NozzleBurner.et";
 static const float DUSK_H = 0.75;
 static const float CONE_FULL_DEG = 20;
 static const float CONE_EDGE_DEG = 32;
 static const float FADE_FROM_M = 1000;
 static const float GONE_M = 2000;
 static const float MIN_ANGLE = 0.0008;   // rad: the ring's smallest apparent radius
 static const float RECESS_M = 0.06;      // the ring sits this far up the pipe while drawn at its own size
 static const int GLOW_EMISSIVE = 24;     // ParametricMaterialInstanceComponent multipliers at full (emat LV 0)
 static const int BURNER_EMISSIVE = 320;
 static const float THROTTLE_BOOST = 1.6; // the glow at full throttle (the dash) or on the attack run
 // The recon dash: the burner lights this long before the corridor and stays lit this long after it.
 static const float DASH_LEAD_S = 5;
 static const float DASH_TAIL_S = 4;
 static const float DASH_RAMP_S = 0.8;
 static const float NIGHT_EVERY_MS = 2000;

 // Night, worked out at most every NIGHT_EVERY_MS (shared by every jet). Tests set s_bTestNight / s_fTestNight.
 protected static float s_fNight;
 protected static float s_fNightAt;
 protected static bool s_bNightKnown;
 static bool s_bTestNight;
 static float s_fTestNight;

 ref array<vector> m_aNozzles;   // model axes (x forward, y up, z across), on the exit plane
 ref array<float> m_aRadii;
 ref array<IEntity> m_aGlows;
 ref array<IEntity> m_aBurners;
 ref array<ParametricMaterialInstanceComponent> m_aGlowMats;
 ref array<ParametricMaterialInstanceComponent> m_aBurnerMats;
 bool m_bBurner;                 // has an afterburner (the recon jets)
 bool m_bShown;
 float m_fLevel;                 // tests: the last night x cone x distance
 int m_iGlowEmissive;            // tests: the last multipliers set
 int m_iBurnerEmissive;
 float m_fGlowScale;             // tests: the ring's last drawn radius

 // The nozzles of a period jet, from tools/blender/build_aircraft.py (its X, Y, Z = our x, -z, y; all symmetric),
 // each nozzle's mouth: the pipe's radius times its outlet's mouth fraction. False for a jet with none (the A-10).
 static bool Nozzles(ResourceName prefab, notnull array<vector> at, notnull array<float> radii, out bool burner)
 {
  at.Clear();
  radii.Clear();
  burner = false;
  if (prefab == ACR_ReconRun.RECON_JET_WEST)
  {
   // RF-4C: two J79s, pipes 0.44 m at 0.55 m either side, mouth 0.8
   at.Insert(Vector(-7.82, -0.05, 0.55));
   at.Insert(Vector(-7.82, -0.05, -0.55));
   radii.Insert(0.35);
   radii.Insert(0.35);
   burner = true;
  }
  else if (prefab == ACR_ReconRun.RECON_JET_EAST)
  {
   // Su-17M4R: one AL-21F-3, pipe 0.47 m, mouth 0.82
   at.Insert(Vector(-9.02, 0.05, 0));
   radii.Insert(0.385);
   burner = true;
  }
  else if (prefab == ACR_Supports.BOMB_JET_EAST)
  {
   // Su-25: two R-95Sh in the nacelles 0.98 m out, a 0.34 x 0.36 m pipe, mouth 0.78
   at.Insert(Vector(-3.57, -0.05, 0.98));
   at.Insert(Vector(-3.57, -0.05, -0.98));
   radii.Insert(0.27);
   radii.Insert(0.27);
  }
  return !at.IsEmpty();
 }

 // A jet's glows (null for one without: the A-10, RHS's FA-18).
 static ACR_JetGlow Create(ResourceName prefab)
 {
  ACR_JetGlow glow = new ACR_JetGlow();
  glow.m_aNozzles = {};
  glow.m_aRadii = {};
  if (!Nozzles(prefab, glow.m_aNozzles, glow.m_aRadii, glow.m_bBurner)) return null;
  glow.m_aGlows = {};
  glow.m_aBurners = {};
  glow.m_aGlowMats = {};
  glow.m_aBurnerMats = {};
  return glow;
 }

 // 0 by day, 1 at night, fading over DUSK_H after sunset and before sunrise.
 static float NightAt(float hour, float sunrise, float sunset)
 {
  if (hour >= sunrise && hour < sunset) return 0;
  float sinceSet = hour - sunset;
  if (sinceSet < 0) sinceSet += 24;
  float toRise = sunrise - hour;
  if (toRise < 0) toRise += 24;
  return Math.Min(Math.Clamp(sinceSet / DUSK_H, 0, 1), Math.Clamp(toRise / DUSK_H, 0, 1));
 }

 static float Night()
 {
  if (s_bTestNight) return s_fTestNight;
  float now = GetGame().GetWorld().GetWorldTime();
  if (s_bNightKnown && now - s_fNightAt < NIGHT_EVERY_MS && now >= s_fNightAt) return s_fNight;
  s_bNightKnown = true;
  s_fNightAt = now;
  s_fNight = 0;
  ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
  if (!world) return 0;
  TimeAndWeatherManagerEntity time = world.GetTimeAndWeatherManager();
  if (!time) return 0;
  float hour = time.GetTimeOfTheDay();
  float sunrise, sunset;
  if (time.GetSunriseHour(sunrise) && time.GetSunsetHour(sunset) && sunrise < sunset)
   s_fNight = NightAt(hour, sunrise, sunset);
  else if (time.IsNightHour(hour))
   s_fNight = 1;
  return s_fNight;
 }

 // Seen from `viewer`, a nozzle at `nozzle` whose tail points along `tail` (unit): 1 in the cone close in, 0 ahead,
 // out to the side or past GONE_M.
 static float Sight(vector nozzle, vector tail, vector viewer)
 {
  vector toViewer = viewer - nozzle;
  float distance = toViewer.Length();
  if (distance < 0.01) return 0;
  float cosine = vector.Dot(toViewer, tail) / distance;
  float cosEdge = Math.Cos(CONE_EDGE_DEG * Math.DEG2RAD);
  float cosFull = Math.Cos(CONE_FULL_DEG * Math.DEG2RAD);
  float cone = Math.Clamp((cosine - cosEdge) / (cosFull - cosEdge), 0, 1);
  float far = Math.Clamp((distance - FADE_FROM_M) / (GONE_M - FADE_FROM_M), 0, 1);
  return cone * (1 - far);
 }

 // The recon jet's afterburner at `elapsed` s into a recon flight (0 to 1): lit from DASH_LEAD_S before the corridor
 // to DASH_TAIL_S after it. The flight is over the target DELAY_S in, crossing the corridor at SPEED (ACR_ReconRun).
 static float Dash(float elapsed)
 {
  float half = ACR_ReconRun.CORRIDOR_LENGTH * 0.5 / ACR_ReconRun.SPEED;
  float from = ACR_ReconRun.DELAY_S - half - DASH_LEAD_S;
  float to = ACR_ReconRun.DELAY_S + half + DASH_TAIL_S;
  return Math.Clamp(Math.Min(elapsed - from, to - elapsed) / DASH_RAMP_S, 0, 1);
 }

 // Each frame on the machine flying the jet: `transform` the plane's (unit axes), `viewer` where this machine looks
 // from (seeing = false: nobody looks), `boost` 0 to 1 (full throttle / the attack run), `burner` 0 to 1.
 void Step(vector transform[4], bool seeing, vector viewer, float boost, float burner)
 {
  float level = 0;
  if (seeing)
  {
   float night = Night();
   if (night > 0) level = night * Sight(Exit(transform, 0), -transform[0], viewer);
  }
  m_fLevel = level;
  if (level <= 0)
  {
   Show(false);
   return;
  }
  if (m_aGlows.IsEmpty()) Spawn(transform);
  Show(true);
  if (!m_bBurner) burner = 0;
  float distance = vector.Distance(Exit(transform, 0), viewer);
  int glowEmissive = (int)Math.Round(GLOW_EMISSIVE * level * (1 + (THROTTLE_BOOST - 1) * Math.Clamp(boost, 0, 1)));
  int burnerEmissive = (int)Math.Round(BURNER_EMISSIVE * level * Math.Clamp(burner, 0, 1));
  foreach (int i, vector slot : m_aNozzles)
  {
   vector exit = Exit(transform, i);
   float radius = m_aRadii[i];
   float scale = Math.Max(radius, distance * MIN_ANGLE);
   vector at = exit + transform[0] * RECESS_M;
   if (scale > radius * 1.05) at = exit - transform[0] * 0.03; // grown past the pipe: just aft of the mouth
   Place(m_aGlows[i], transform, at, scale);
   m_fGlowScale = scale;
   if (i < m_aBurners.Count() && m_aBurners[i])
   {
    if (burnerEmissive > 0)
    {
     m_aBurners[i].SetFlags(EntityFlags.VISIBLE, false);
     Place(m_aBurners[i], transform, exit, radius);
    }
    else
     m_aBurners[i].ClearFlags(EntityFlags.VISIBLE, false);
   }
  }
  if (glowEmissive != m_iGlowEmissive)
  {
   m_iGlowEmissive = glowEmissive;
   foreach (ParametricMaterialInstanceComponent mat : m_aGlowMats) if (mat) mat.SetEmissiveMultiplier(glowEmissive);
  }
  if (burnerEmissive != m_iBurnerEmissive)
  {
   m_iBurnerEmissive = burnerEmissive;
   foreach (ParametricMaterialInstanceComponent mat : m_aBurnerMats) if (mat) mat.SetEmissiveMultiplier(burnerEmissive);
  }
 }

 vector Exit(vector transform[4], int i)
 {
  vector slot = m_aNozzles[i];
  return transform[3] + transform[0] * slot[0] + transform[1] * slot[1] + transform[2] * slot[2];
 }

 protected void Place(IEntity entity, vector transform[4], vector at, float scale)
 {
  if (!entity) return;
  vector m[4];
  m[0] = transform[0];
  m[1] = transform[1];
  m[2] = transform[2];
  m[3] = at;
  entity.SetTransform(m);
  entity.SetScale(scale);
  entity.Update();
 }

 protected void Spawn(vector transform[4])
 {
  foreach (int i, vector slot : m_aNozzles)
  {
   IEntity glow = ACR_Supports.SpawnLocal(GLOW, Exit(transform, i), vector.Zero);
   m_aGlows.Insert(glow);
   if (glow) m_aGlowMats.Insert(ParametricMaterialInstanceComponent.Cast(glow.FindComponent(ParametricMaterialInstanceComponent)));
   if (!m_bBurner) continue;
   IEntity burner = ACR_Supports.SpawnLocal(BURNER, Exit(transform, i), vector.Zero);
   m_aBurners.Insert(burner);
   if (burner)
   {
    m_aBurnerMats.Insert(ParametricMaterialInstanceComponent.Cast(burner.FindComponent(ParametricMaterialInstanceComponent)));
    burner.ClearFlags(EntityFlags.VISIBLE, false);
   }
  }
  m_iGlowEmissive = -1;
  m_iBurnerEmissive = -1;
  m_bShown = true;
 }

 protected void Show(bool on)
 {
  if (on == m_bShown) return;
  m_bShown = on;
  foreach (IEntity glow : m_aGlows)
  {
   if (!glow) continue;
   if (on) glow.SetFlags(EntityFlags.VISIBLE, false);
   else glow.ClearFlags(EntityFlags.VISIBLE, false);
  }
  if (!on)
   foreach (IEntity burner : m_aBurners) if (burner) burner.ClearFlags(EntityFlags.VISIBLE, false);
 }

 // Drawn now (tests): the rings and plumes that are visible.
 int Visible(bool burners)
 {
  int n = 0;
  array<IEntity> list = m_aGlows;
  if (burners) list = m_aBurners;
  foreach (IEntity e : list) if (e && (e.GetFlags() & EntityFlags.VISIBLE)) n++;
  return n;
 }

 void Clear()
 {
  foreach (IEntity glow : m_aGlows) if (glow) SCR_EntityHelper.DeleteEntityAndChildren(glow);
  foreach (IEntity burner : m_aBurners) if (burner) SCR_EntityHelper.DeleteEntityAndChildren(burner);
  m_aGlows.Clear();
  m_aBurners.Clear();
  m_aGlowMats.Clear();
  m_aBurnerMats.Clear();
  m_bShown = false;
 }
}
