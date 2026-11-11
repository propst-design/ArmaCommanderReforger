// Containment airdrops (#107, Jacob 7 Nov 2026): income in a contained outbreak. Each living side gets a
// cargo pallet dropped about every DROP_MS somewhere roughly at random DROP_MIN-DROP_MAX from its HQ,
// in the open, away from bases. The plane can't be shot down: it is a flyover, a model each machine flies
// along the track on its own (ACR_LocalFlyover via SCR_BaseGameMode.ACR_BroadcastFlyover), as the bomb run's.
// STAND-IN: AC: Actual has no C-130 or An-12 model yet (tools/blender/build_aircraft.py builds the jets) and
// vanilla has no fixed-wing aircraft, so the airlifter is flown with AC: Actual's own A-10A (West) or Su-25
// (East) model, slow and high, and the pallet lands LAND_S after it passes (no parachute is drawn).
//  - A pallet on the ground draws the dead near it (ACR_Outbreak.CounterAttack), so recovery is a small fight.
//  - A soldier of a side within RECOVER_RANGE of it with no zombie within ZOMBIE_CLEAR recovers it: RP_PALLET
//    RP for that side (up to the cap). In TvTvZ the other side can steal it.
//  - One nobody recovers within PALLET_MS is gone (looted). At most MOST_PALLETS lie out per side.
// AI commanders send one combat squad at a time for the nearest pallet they may take (ManagePallets).
class ACR_Pallet : Managed
{
 int m_iId;
 vector m_vPos;
 string m_sFor;   // the side it was dropped for
 IEntity m_Prop;
 float m_fLanded;
}

class ACR_Airdrop
{
 static const ResourceName PALLET_WEST = "{AE0FFC699F429214}Prefabs/Props/Military/Camps/PalletAmmo_01_US.et";
 static const ResourceName PALLET_EAST = "{47A2F2F3C8FAACD5}Prefabs/Props/Military/Camps/PalletAmmo_01_Soviet.et";
 static const float DROP_MS = 720000;       // a pallet per side about this often (12 min, +-25 %)
 static const float FIRST_MIN_MS = 300000;  // the first 5-8 min in
 static const float FIRST_MAX_MS = 480000;
 static const float DROP_MIN = 600;         // m from the side's HQ
 static const float DROP_MAX = 2500;
 static const float BASE_CLEAR = 250;       // m: never this close to a base
 static const float RECOVER_RANGE = 25;     // m: a soldier this close recovers it ...
 static const float ZOMBIE_CLEAR = 40;      // ... with no zombie this close
 static const int RP_PALLET = 30;
 static const float PALLET_MS = 2400000;    // gone after 40 min
 static const int MOST_PALLETS = 3;
 static const float PLANE_SPEED = 90;       // m/s
 static const float PLANE_HEIGHT = 300;     // m over the drop spot
 static const float PLANE_LEAD = 3000;      // m flown before the drop spot
 static const float LAND_S = 20;            // s after the plane passes, the pallet is down

 protected static ref array<ref ACR_Pallet> s_aPallets;
 protected static ref map<string, float> s_NextAt;
 protected static int s_iNextId;
 static int s_iDropped;    // tests
 static int s_iRecovered;  // tests
 static int s_iStolen;     // tests

 static void ACR_InitStatics()
 {
  s_aPallets = {};
  s_NextAt = new map<string, float>();
 }

 static void Reset()
 {
  foreach (ACR_Pallet pallet : s_aPallets) if (pallet.m_Prop) SCR_EntityHelper.DeleteEntityAndChildren(pallet.m_Prop);
  s_aPallets.Clear();
  s_NextAt.Clear();
  s_iNextId = 0;
  s_iDropped = 0;
  s_iRecovered = 0;
  s_iStolen = 0;
 }

 static int Count() { return s_aPallets.Count(); }
 static ACR_Pallet At(int i) { return s_aPallets[i]; }
 static bool Has(ACR_Pallet pallet) { return pallet != null && s_aPallets.Contains(pallet); }

 static int CountFor(string side)
 {
  int n;
  foreach (ACR_Pallet pallet : s_aPallets) if (pallet.m_sFor == side) n++;
  return n;
 }

 // Whether 'side' may take this pallet now: its own, or the other side's (TvTvZ: theft).
 static bool MayTake(ACR_Pallet pallet, string side)
 {
  return true;
 }

 // ---------------------------------------------------------------- every second (ACR_Containment.Tick)

 static void Tick(ACR_Manager manager, int seconds)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  array<string> sides = ACR_Containment.Sides();
  foreach (string side : sides)
  {
   float due;
   if (!s_NextAt.Find(side, due))
   {
    s_NextAt.Set(side, now + Math.RandomFloat(FIRST_MIN_MS, FIRST_MAX_MS));
    continue;
   }
   if (now < due) continue;
   s_NextAt.Set(side, now + DROP_MS * Math.RandomFloat(0.75, 1.25));
   if (CountFor(side) >= MOST_PALLETS) continue;
   vector spot;
   if (!Spot(side, spot)) continue;
   Fly(side, spot);
   GetGame().GetCallqueue().CallLater(LandLater, (PLANE_LEAD / PLANE_SPEED + LAND_S) * 1000, false, side, spot);
  }
  if (seconds % 2 == 0) Check(manager, now);
 }

 // Somewhere in the open DROP_MIN-DROP_MAX from the side's HQ, on land, clear of bases.
 static bool Spot(string side, out vector spot)
 {
  ACR_Base hq = ACR_DoctrineKits.HQOf(side);
  if (!hq) return false;
  IEntity navAgent = ACR_Garrison.AnyAgent();
  for (int attempt = 0; attempt < 12; attempt++)
  {
   float angle = Math.RandomFloat(0, Math.PI2);
   float reach = Math.RandomFloat(DROP_MIN, DROP_MAX);
   vector at = hq.GetOrigin() + Vector(Math.Cos(angle) * reach, 0, Math.Sin(angle) * reach);
   if (!ACR_Manager.OnMap(at)) continue;
   at = ACR_Manager.Ground(at);
   if (at[1] < 1) continue; // the sea
   bool clear = true;
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base && !base.m_bRetired && vector.DistanceXZ(base.GetOrigin(), at) < BASE_CLEAR) { clear = false; break; }
   if (!clear) continue;
   spot = ACR_BaseSystem.SoldierSpot(at, 20, navAgent);
   return true;
  }
  return false;
 }

 protected static bool West(string side) { return side == ACR_FactionSelection.West(); }

 // The airlifter over the spot (a flyover each machine flies itself; see the stand-in note above).
 protected static void Fly(string side, vector spot)
 {
  float angle = Math.RandomFloat(0, Math.PI2);
  vector dir = Vector(Math.Cos(angle), 0, Math.Sin(angle));
  vector start = spot - dir * PLANE_LEAD;
  float altitude = spot[1] + PLANE_HEIGHT;
  for (float d = 0; d <= PLANE_LEAD * 2; d += 200)
  {
   vector point = start + dir * d;
   altitude = Math.Max(altitude, GetGame().GetWorld().GetSurfaceY(point[0], point[2]) + 150);
  }
  start[1] = altitude;
  float seconds = PLANE_LEAD * 2 / PLANE_SPEED;
  ResourceName plane = ACR_Supports.BOMB_JET_EAST;
  if (West(side)) plane = ACR_Supports.BOMB_JET_WEST;
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (mode) mode.ACR_BroadcastFlyover(plane, start, dir, PLANE_SPEED, altitude, seconds, false);
  if (GetGame().GetPlayerController()) ACR_LocalFlyover.Start(plane, start, dir, PLANE_SPEED, altitude, seconds, false);
  string name = "An-12";
  if (West(side)) name = "C-130";
  PrintFormat("[ACR] AIRDROP %1 %2 inbound to %3", side, name, ACR_Manager.Grid(spot));
  ACR_Containment.Feed(side, string.Format("%1 supply drop inbound near %2", name, ACR_Manager.Grid(spot)), spot);
 }

 protected static void LandLater(string side, vector spot) { Land(side, spot); }

 // The pallet is down: it shows on both maps, and the dead near it stir.
 static ACR_Pallet Land(string side, vector spot)
 {
  if (!ACR_Containment.On()) return null;
  ResourceName prefab = PALLET_EAST;
  if (West(side)) prefab = PALLET_WEST;
  ACR_Pallet pallet = new ACR_Pallet();
  pallet.m_iId = ++s_iNextId;
  pallet.m_vPos = spot;
  pallet.m_sFor = side;
  pallet.m_fLanded = GetGame().GetWorld().GetWorldTime();
  pallet.m_Prop = ACR_Manager.SpawnPrefab(prefab, spot);
  s_aPallets.Insert(pallet);
  s_iDropped++;
  PrintFormat("[ACR] AIRDROP %1 pallet %2 down at %3 (prop %4)", side, pallet.m_iId, ACR_Manager.Grid(spot), pallet.m_Prop != null);
  array<string> sides = ACR_Containment.Sides();
  foreach (string other : sides)
  {
   if (other == side) ACR_Containment.Feed(other, string.Format("Supply pallet down at %1: recover it for %2 RP. The dead will have heard it", ACR_Manager.Grid(spot), RP_PALLET), spot);
   else ACR_Containment.Feed(other, string.Format("Enemy supply pallet down at %1", ACR_Manager.Grid(spot)), spot);
  }
  ACR_Outbreak.CounterAttack(spot, "the pallet at " + ACR_Manager.Grid(spot), null, side);
  return pallet;
 }

 protected static void Check(ACR_Manager manager, float now)
 {
  array<string> sides = ACR_Containment.Sides();
  for (int i = s_aPallets.Count() - 1; i >= 0; i--)
  {
   ACR_Pallet pallet = s_aPallets[i];
   if (now - pallet.m_fLanded > PALLET_MS)
   {
    PrintFormat("[ACR] AIRDROP pallet %1 at %2 looted, nobody came", pallet.m_iId, ACR_Manager.Grid(pallet.m_vPos));
    Remove(i);
    continue;
   }
   if (ACR_Containment.ZombiesNear(pallet.m_vPos, ZOMBIE_CLEAR)) continue;
   // Its own side first, if both are there.
   string taker;
   if (ACR_Outbreak.SideNear(manager, pallet.m_sFor, pallet.m_vPos, RECOVER_RANGE)) taker = pallet.m_sFor;
   else
   {
    foreach (string side : sides)
     if (side != pallet.m_sFor && MayTake(pallet, side) && ACR_Outbreak.SideNear(manager, side, pallet.m_vPos, RECOVER_RANGE)) { taker = side; break; }
   }
   if (taker.IsEmpty()) continue;
   Recover(manager, i, taker);
  }
 }

 protected static void Recover(ACR_Manager manager, int i, string side)
 {
  ACR_Pallet pallet = s_aPallets[i];
  int rp = ACR_Containment.Give(manager, side, RP_PALLET);
  string where = ACR_Manager.Grid(pallet.m_vPos);
  if (side == pallet.m_sFor)
  {
   s_iRecovered++;
   PrintFormat("[ACR] AIRDROP %1 recovered pallet %2 at %3 (+%4 RP)", side, pallet.m_iId, where, rp);
   ACR_Containment.Feed(side, string.Format("Supply pallet recovered at %1: +%2 RP", where, rp), pallet.m_vPos);
  }
  else
  {
   s_iStolen++;
   PrintFormat("[ACR] AIRDROP %1 stole %2's pallet %3 at %4 (+%5 RP)", side, pallet.m_sFor, pallet.m_iId, where, rp);
   ACR_Containment.Feed(side, string.Format("Enemy supply pallet taken at %1: +%2 RP", where, rp), pallet.m_vPos);
   ACR_Containment.Feed(pallet.m_sFor, string.Format("The enemy took our supply pallet at %1", where), pallet.m_vPos);
  }
  Remove(i);
 }

 protected static void Remove(int i)
 {
  ACR_Pallet pallet = s_aPallets[i];
  if (pallet.m_Prop) SCR_EntityHelper.DeleteEntityAndChildren(pallet.m_Prop);
  s_aPallets.Remove(i);
 }

 // Both sides' maps: every pallet on the ground (a supply drop is seen from far off).
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  foreach (ACR_Pallet pallet : s_aPallets)
  {
   positions.Insert(pallet.m_vPos);
   radii.Insert(RECOVER_RANGE);
   if (pallet.m_sFor == faction) labels.Insert(string.Format("Supply pallet (%1 RP)", RP_PALLET));
   else labels.Insert("Enemy supply pallet");
  }
 }

 // Tests: a drop at 'spot' now, flyover included; the pallet lands at once.
 static ACR_Pallet DropForTest(string side, vector spot)
 {
  Fly(side, spot);
  return Land(side, spot);
 }
}
