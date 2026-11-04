// Recon flight and radio intercept (Jacob, 4 Nov 2026: ordered recon teams don't hold still or
// keep quiet, so intel needs a source the commander can rely on, and the AI commander uses it too).
//
// Recon flight (ACR_ESupportType.RECON_FLIGHT): an off-map jet photographs a corridor CORRIDOR_LENGTH
// long and 2 x HALF_WIDTH wide, centred on the called point and flown from the side's nearest base.
// What it sees goes into the side's ACR_Intel like any other sighting (so the map, the feed and the
// AI commander all read it), with these differences:
//  - Fuzzed: each group is placed off its true spot by up to FUZZ_INFANTRY / FUZZ_VEHICLE, the same
//    offset for the whole group (a per-man offset would average out over a squad).
//  - Photo intel: kept RECON_MEMORY_MS (the eye's 90 s would forget it before anyone could plan).
//  - Not everything is seen: vehicles and men in the open mostly, men under a roof or trees rarely.
//  - Hidden FOBs in the corridor are found; enemy bases in it count as photographed, so the AI
//    commander stops padding their defence with its blind guess (ACR_AICommander.Assess).
// No anti-air can stop it (Jacob, 4 Nov: the game has no units suited to that role). The side flown
// over hears the jet and is told it may have been seen.
//
// Aircraft model and sound: the flight is server-side and needs neither. Each machine with a player
// flies its own copy of a prefab along the track (ACR_LocalFlyover, as the bomb run); the prefab
// carries its own engine sound (a looping sound component, as RHS's FA-18 does). ReconPrefab picks
// it: the support entry's m_sReconPrefab, else RECON_JET_WEST / RECON_JET_EAST (ACA's own jets: the
// RF-4C and the Su-17M4R, silent blockouts for now), else RHS's FA-18 for a western side. With none
// the flight is unseen and unheard but still works.
//
// Radio intercept: when a side's soldiers radio a contact report or an observer calls a fire
// mission, each hostile side may (INTERCEPT_CHANCE) pick up the transmission: a pale ring of
// INTERCEPT_RADIUS on its map that holds the transmitter somewhere inside, never at the centre,
// for INTERCEPT_MS. The AI commander flies its recon over a fresh intercept near its bases.
class ACR_ReconRun : ACR_SupportRun
{
 static const float CORRIDOR_LENGTH = 2400;
 static const float HALF_WIDTH = 300;
 static const float SPEED = 220;              // m/s
 static const float ALTITUDE = 500;           // m over the target; never under MIN_CLEARANCE over the ground
 static const float MIN_CLEARANCE = 200;
 static const int DELAY_S = 30;               // call to the plane overhead the target
 static const float SCAN_MS = 500;
 static const float FUZZ_INFANTRY = 50;
 static const float FUZZ_VEHICLE = 20;
 static const float RECON_MEMORY_MS = 300000; // photo intel is kept 5 min
 static float SEE_VEHICLE = 0.95;
 static float SEE_OPEN = 0.75;
 static float SEE_COVERED = 0.15;
 static float SEE_FOB = 0.85;
 static const string SPOTTER = "Recon flight";
 // ACA's own 1989 recon jets (blockout models, tools/blender/build_aircraft.py): RF-4C and Su-17M4R.
 static const ResourceName RECON_JET_WEST = "{AC230926A00100E2}Prefabs/Vehicles/Aircraft/ACR_RF4C.et";
 static const ResourceName RECON_JET_EAST = "{AC230926A00100E3}Prefabs/Vehicles/Aircraft/ACR_Su17M4R.et";
 static int s_iSeen; // tests: sightings made by recon flights

 string m_sFaction;
 vector m_Target;
 vector m_Dir;                 // flat, normalised
 float m_fCorridorAt;          // world ms the plane reaches the corridor's start
 float m_fScanned;             // metres of the corridor swept so far
 float m_fNextScan;
 protected ref set<IEntity> m_Checked = new set<IEntity>();
 protected ref map<AIGroup, vector> m_GroupFuzz = new map<AIGroup, vector>();
 protected ref array<string> m_Warned = {};
 int m_iSeen;
 int m_iFobs;

 vector CorridorStart() { return m_Target - m_Dir * (CORRIDOR_LENGTH * 0.5); }

 // Along and across the corridor (along from its start, across either side of the track).
 static bool InCorridor(vector point, vector start, vector dir, float length, float halfWidth)
 {
  vector rel = point - start;
  rel[1] = 0;
  float along = vector.Dot(rel, dir);
  if (along < 0 || along > length) return false;
  vector right = Vector(dir[2], 0, -dir[0]);
  return Math.AbsFloat(vector.Dot(rel, right)) <= halfWidth;
 }

 // A random offset within radius (uniform over the disc).
 static vector Fuzz(float radius)
 {
  float angle = Math.RandomFloat(0, Math.PI2);
  float spread = radius * Math.Sqrt(Math.RandomFloat(0, 1));
  return Vector(Math.Cos(angle) * spread, 0, Math.Sin(angle) * spread);
 }

 static float SeeChance(int kind, bool covered)
 {
  if (kind != ACR_Intel.KIND_INFANTRY) return SEE_VEHICLE;
  if (covered) return SEE_COVERED;
  return SEE_OPEN;
 }

 // The plane model for a side's recon flight ("" = none: the flight is unseen).
 static ResourceName ReconPrefab(ACR_SupportEntry entry, string factionKey)
 {
  if (entry && !entry.m_sReconPrefab.IsEmpty() && ACR_Supports.HasPrefab(entry.m_sReconPrefab)) return entry.m_sReconPrefab;
  bool west = ACR_Supports.IsWestern(factionKey);
  ResourceName own = RECON_JET_EAST;
  if (west) own = RECON_JET_WEST;
  if (!own.IsEmpty() && ACR_Supports.HasPrefab(own)) return own;
  // RHS's FA-18 (the bomb run's flyover) for a western side only: no Soviet flies a Hornet.
  if (west && ACR_Supports.HasPrefab(ACR_Supports.FA18_FLYOVER)) return ACR_Supports.FA18_FLYOVER;
  return "";
 }

 // Starts the flight (server). Returns the seconds until the plane is over the target.
 int Start(ACR_SupportEntry entry, string factionKey, vector target, vector dir)
 {
  m_sFaction = factionKey;
  m_Target = target;
  m_Dir = dir;
  m_Dir[1] = 0;
  m_Dir.Normalize();
  float now = GetGame().GetWorld().GetWorldTime();
  m_fCorridorAt = now + (DELAY_S - CORRIDOR_LENGTH * 0.5 / SPEED) * 1000;
  m_fNextScan = m_fCorridorAt;
  ResourceName prefab = ReconPrefab(entry, factionKey);
  if (!prefab.IsEmpty())
  {
   vector start = target - m_Dir * (SPEED * DELAY_S);
   float altitude = target[1] + ALTITUDE;
   float pathLength = SPEED * DELAY_S + 2500;
   for (float d = 0; d <= pathLength; d += 100)
   {
    vector point = start + m_Dir * d;
    altitude = Math.Max(altitude, GetGame().GetWorld().GetSurfaceY(point[0], point[2]) + MIN_CLEARANCE);
   }
   start[1] = altitude;
   float seconds = DELAY_S + 2500 / SPEED;
   SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
   if (mode) mode.ACR_BroadcastFlyover(prefab, start, m_Dir, SPEED, altitude, seconds);
   if (GetGame().GetPlayerController()) ACR_LocalFlyover.Start(prefab, start, m_Dir, SPEED, altitude, seconds);
  }
  PrintFormat("[ACR] RECON %1 flight over %2 heading %3, model %4", factionKey, target, Math.Round(ACR_Intel.Bearing(Vector(0, 0, 0), m_Dir)), FilePath.StripPath(prefab));
  return DELAY_S;
 }

 override bool Frame(ACR_Supports supports, float dt, float now)
 {
  if (now < m_fNextScan) return true;
  m_fNextScan = now + SCAN_MS;
  float along = Math.Min(SPEED * (now - m_fCorridorAt) / 1000, CORRIDOR_LENGTH);
  if (along > m_fScanned)
  {
   Sweep(supports, m_fScanned, along, now);
   m_fScanned = along;
  }
  return along < CORRIDOR_LENGTH;
 }

 // Everything between from and to metres along the corridor.
 protected void Sweep(ACR_Supports supports, float from, float to, float now)
 {
  ACR_Manager manager = supports.m_Manager;
  ACR_Intel intel = manager.IntelFor(m_sFaction);
  if (!intel) return;
  vector start = CorridorStart();
  vector middle = start + m_Dir * ((from + to) * 0.5);
  float reach = Math.Sqrt(HALF_WIDTH * HALF_WIDTH + (to - from) * (to - from) * 0.25);
  array<IEntity> found = {};
  supports.FindEnemies(m_sFaction, middle, reach, found);
  vector right = Vector(m_Dir[2], 0, -m_Dir[0]);
  foreach (IEntity body : found)
  {
   if (m_Checked.Contains(body)) continue;
   vector rel = body.GetOrigin() - start;
   float bodyAlong = vector.Dot(rel, m_Dir);
   if (bodyAlong < from || bodyAlong > to || Math.AbsFloat(vector.Dot(rel, right)) > HALF_WIDTH) continue;
   m_Checked.Insert(body);
   WarnOverflown(manager, body, now);
   int kind = ACR_Intel.KindOf(body);
   bool covered = kind == ACR_Intel.KIND_INFANTRY && !ACR_Intel.SkyView(body.GetOrigin() - m_Dir * 150 + Vector(0, 300, 0), body);
   if (Math.RandomFloat01() > SeeChance(kind, covered)) continue;
   intel.NoteFromAir(body, now, SPOTTER, body.GetOrigin() + GroupFuzz(body, kind), now + RECON_MEMORY_MS);
   m_iSeen++;
   s_iSeen++;
  }
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner.IsEmpty() || ACR_Insurgency.Friendly(base.m_sOwner, m_sFaction)) continue;
   vector baseRel = base.GetOrigin() - start;
   float baseAlong = vector.Dot(baseRel, m_Dir);
   if (baseAlong < from || baseAlong > to || Math.AbsFloat(vector.Dot(baseRel, right)) > HALF_WIDTH + base.m_fRadius) continue;
   intel.Photographed(base, now);
   if (!base.m_bBuilt || base.IsVisibleTo(m_sFaction) || Math.RandomFloat01() > SEE_FOB) continue;
   intel.FobFound(base, SPOTTER, now);
   m_iFobs++;
  }
 }

 // One offset per group, so a squad's men stay together where it is shown.
 protected vector GroupFuzz(IEntity body, int kind)
 {
  float radius = FUZZ_INFANTRY;
  if (kind != ACR_Intel.KIND_INFANTRY) radius = FUZZ_VEHICLE;
  AIGroup group = ACR_Intel.GroupOfBody(body);
  if (!group) return Fuzz(radius);
  vector offset;
  if (!m_GroupFuzz.Find(group, offset))
  {
   offset = Fuzz(radius);
   m_GroupFuzz.Insert(group, offset);
  }
  return offset;
 }

 // A side under the track hears the jet, once per flight.
 protected void WarnOverflown(ACR_Manager manager, IEntity body, float now)
 {
  string side = ACR_Intel.FactionKeyOf(body);
  if (side.IsEmpty() || m_Warned.Contains(side)) return;
  m_Warned.Insert(side);
  ACR_Intel theirs = manager.IntelFor(side);
  if (!theirs) return;
  theirs.AddFeed(now, string.Format("Enemy reconnaissance aircraft overhead %1", theirs.Place(body.GetOrigin())), body.GetOrigin());
  manager.NotifyFaction(side, "Enemy reconnaissance aircraft overhead: you may have been spotted.");
 }

 override void Finish()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Intel intel = manager.IntelFor(m_sFaction);
  if (!intel) return;
  string what = string.Format("%1 seen", m_iSeen);
  if (m_iSeen == 0) what = "nothing seen";
  if (m_iFobs > 0) what += string.Format(", %1 FOB found", m_iFobs);
  intel.AddFeed(GetGame().GetWorld().GetWorldTime(), string.Format("%1 over %2: %3", SPOTTER, intel.Place(m_Target), what), m_Target);
  PrintFormat("[ACR] RECON %1 flight done over %2: %3 seen, %4 FOBs, swept %5 m", m_sFaction, m_Target, m_iSeen, m_iFobs, Math.Round(m_fScanned));
 }
}

// A transmission one side picked up from another: a ring that holds the transmitter.
class ACR_Intercept : Managed
{
 vector m_vCentre;
 float m_fHeard;
 string m_sKey; // who transmitted (internal: the same transmitter refreshes its ring)
}

class ACR_RadioIntercept
{
 static float INTERCEPT_CHANCE = 0.35;
 static const float INTERCEPT_RADIUS = 400;
 static const float INTERCEPT_MS = 180000;
 static const float REFRESH_MS = 60000;      // the same transmitter again within this: the ring moves, no new line
 static const int MAX_RINGS = 6;
 static int s_iHeard; // tests

 // A side transmitted from 'from' (key: who). Each hostile side may hear it.
 static void Transmitted(string side, vector from, string key)
 {
  if (side.IsEmpty() || from == vector.Zero) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All)
  {
   string listener = settings.m_sFaction;
   if (listener.IsEmpty() || listener == side || ACR_Insurgency.Friendly(listener, side)) continue;
   if (Math.RandomFloat01() > INTERCEPT_CHANCE) continue;
   ACR_Intel intel = manager.IntelFor(listener);
   if (intel) Heard(intel, from, side + ":" + key);
  }
 }

 // The ring: centred off the transmitter (between a quarter and two thirds of the radius) so
 // the middle of the ring is never where he stands.
 static void Heard(ACR_Intel intel, vector from, string key)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  float angle = Math.RandomFloat(0, Math.PI2);
  float offset = INTERCEPT_RADIUS * Math.RandomFloat(0.25, 0.66);
  vector centre = from + Vector(Math.Cos(angle) * offset, 0, Math.Sin(angle) * offset);
  centre[1] = GetGame().GetWorld().GetSurfaceY(centre[0], centre[2]);
  s_iHeard++;
  foreach (ACR_Intercept known : intel.m_Intercepts)
  {
   if (known.m_sKey != key || now - known.m_fHeard > REFRESH_MS) continue;
   known.m_vCentre = centre;
   known.m_fHeard = now;
   return;
  }
  ACR_Intercept ring = new ACR_Intercept();
  ring.m_vCentre = centre;
  ring.m_fHeard = now;
  ring.m_sKey = key;
  intel.m_Intercepts.Insert(ring);
  while (intel.m_Intercepts.Count() > MAX_RINGS) intel.m_Intercepts.RemoveOrdered(0);
  intel.AddFeed(now, string.Format("Radio intercept: enemy transmission %1", intel.Place(centre)), centre);
 }

 static void Forget(ACR_Intel intel, float now)
 {
  for (int i = intel.m_Intercepts.Count() - 1; i >= 0; i--)
   if (now - intel.m_Intercepts[i].m_fHeard > INTERCEPT_MS) intel.m_Intercepts.RemoveOrdered(i);
 }

 // Pale rings on the side's map (the "~" label draws a range ring, not a danger area).
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Intel intel = manager.IntelFor(faction);
  if (!intel) return;
  foreach (ACR_Intercept ring : intel.m_Intercepts)
  {
   positions.Insert(ring.m_vCentre);
   radii.Insert(INTERCEPT_RADIUS);
   labels.Insert("~Radio intercept");
  }
 }
}
