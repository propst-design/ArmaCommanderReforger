// Fortify position (0.5.7): the commander digs an infantry squad in where it is sent. Like FOB
// Defences, a position has fixed spots in its own frame (x across, z towards the threat) and
// three tiers; ordering Fortify again on one of our positions upgrades it (in whole or not at
// all, checked when it is ordered; its pieces look further afield than a new position's):
//   I, Hasty:         a line of three sandbag positions, a barbed-wire line in front of it.
//   II, Dug in:       a machine-gun nest replaces the middle position, a covered post behind it.
//   III, Strongpoint: a bunker replaces the covered post, a heavy MG goes on a flank.
// ACR_Requests (and the AI commander) call Order: the server plans the tier here, the squad pays
// and works it in its FORTIFY state (ACR_Squad). The pieces go in when the work is done and no
// vehicle stands on a spot; a blocked spot tries points beside it, then is skipped (FOB rules:
// trees felled, slopes up to MAX_RISE), and asphalt or concrete (a runway, a car park) only
// takes a piece when nothing else will. Work pauses while contested and goes twice as fast with
// sappers / engineers or a construction truck. The squad then holds the position and mans its
// guns; the pieces stay when it leaves. A position shows on its own side's command map until
// every piece is gone. Tier II and III hold defenders in reserve there (ACR_Defenders).
class ACR_Fortification : Managed
{
 int m_iId;
 string m_sFaction;
 vector m_vPos;
 float m_fYaw;                       // compass yaw the position faces
 int m_iTier;                        // 1-3
 ref array<IEntity> m_aPieces = {};  // one per spot (null: none)
 ref array<IEntity> m_aWire = {};    // the wire line's segments (tier I)
 ref array<IEntity> m_aTurrets = {}; // the guns among its pieces (ACR_Fortify.RefreshTurrets)
 ACR_DefenderPost m_Defenders;       // its defenders (tier II and III, ACR_Defenders)
 float m_fUpgradeHeldUntil;          // world ms: an upgrade here was refused, the AI leaves it till then
 string m_sUpgradeRefused;           // why the last upgrade here was refused
 int m_iUpgradeRefusals;             // ... and how many times in a row for that reason
}

// One Fortify order: the tier it builds and how far the work is. The squad keeps it while it
// works (ACR_Squad.m_FortifyPlan).
class ACR_FortifyPlan : Managed
{
 string m_sFaction;
 vector m_vSite;
 float m_fYaw;
 int m_iTier;
 ACR_Fortification m_Target;         // the position it upgrades (null: a new one)
 vector m_vStage;                    // where the squad gathers, behind the position
 float m_fRadius;                    // squad members this close to the site work
 float m_fDuration;
 int m_iCost;
 float m_fProgress;
 float m_fLastTick;
 float m_fLastProgress;              // world ms of the last progress (3 min without it at the site: give up)
 float m_fDeadline;                  // world ms by which the squad must reach the site
 bool m_bArrived;
 vector m_vWalkFrom;                 // where the squad last was on its walk ...
 float m_fWalkAt;                    // ... and when (ACR_Squad.KeepWalking)
 int m_iWalkRetries;
 ACR_BaseWork m_Work;                // base works: the town it is part of (ACR_BaseWorks)
 ACR_BaseSite m_Site;                // ... and the site
}

class ACR_Fortify
{
 static ref array<ref ACR_Fortification> s_Positions = {};
 protected static int s_iNextId;
 protected static int s_iTicks;

 // A barbed-tape fence the guns fire through: a triple concertina stopped a projectile trace
 // anywhere below about 1.5 m, and both guns' muzzles are about 1.1 m up (25 Oct); the tape stops
 // one only at its stakes.
 static const ResourceName WIRE = "{365E4BF1FFFD8B30}PrefabsEditable/Auto/Structures/Walls/BarbedWire/E_BarbedTape_Long.et";
 // Tier II's covered post: a sandbag bunker under a camo net (a bare net over bare ground and
 // rocks looked like a piece was missing, playtest 25 Oct).
 static const ResourceName NET_US = "{F3A7C84173BF2D1E}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Sandbag_Bunker_Camouflaged_US_01.et";
 static const ResourceName NET_USSR = "{51AB40A39B944585}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Sandbag_Bunker_Camouflaged_USSR_01.et";
 static const ResourceName NET_FIA = "{4963886AA7747103}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/Fortifications/E_Sandbag_Bunker_Camouflaged_FIA_01.et";
 static const ResourceName HMG_US = "{43F0E0DFF01BFCC1}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Sandbag_MG_US_01_M2HB.et";
 static const ResourceName HMG_USSR = "{008F787E7BD00867}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Sandbag_MG_USSR_01_NSV.et";
 static const ResourceName HMG_FIA = "{BB201FF4DC275DBB}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_MachineGunNest_Scoped_S_FIA_01.et";

 static const int SPOT_COUNT = 5;
 static const int MAX_TIER = 3;
 static const float UPGRADE_RADIUS = 25;   // Fortify this close to one of our positions upgrades it
 static const float SPACING = 40;          // between our positions
 static const float MIN_BASE_DIST = 30;    // units deploy at a base's centre
 static const float MAX_RISE = 4;          // as FOB defences
 static const int MAX_POSITIONS = 12;      // per side
 static const float ROCK_MIN_HEIGHT = 0.8; // a rock this tall in a footprint blocks it
 static const float FIRE_RANGE = 30;       // a gun's line of fire is traced this far ...
 static const float FIRE_CLEAR = 15;       // ... and must be clear this far
 static const int GUN_PLACES = 6;          // places a gun tries for a clear line of fire
 static const int GUN_PLACES_UPGRADE = 12; // ... in an upgrade (its wider ring)
 static const float MUZZLE_UP = 1.1;       // a gun's muzzle above the ground (measured, 25 Oct)
 static const float MAX_AIM_UP = 15;       // a gun aims this far up rising ground in front of it
 static const float UPGRADE_HOLD_S = 900;  // a refused upgrade: the AI leaves the position this long
 static const int UPGRADE_GIVE_UP = 2;     // ... and for good once refused this often for one reason
 static const float HOLD_RADIUS = 24;      // the position's ring on the map, and its reserve defenders' area
 // #93 (playtest 29 Oct): a Defend of HOLD_RADIUS round the centre reached 10 m past the wire, and the
 // holders stood in the field in front. They hold a circle behind the line instead (HoldCentre): its
 // front edge is at the sandbags, short of the wire (6-14 m out); the guns are held by ACR_GunHold.
 static const float HOLDER_BACK = 8;       // m behind the line's centre
 static const float HOLDER_RADIUS = 16;
 static const float FLANK_TURN = 20;       // the heavy MG turns in this far from its flank
 static float WORK_SCALE = 1;              // tests shorten the work

 // ---------------------------------------------------------------- options

 static bool Enabled() { return ACR_LogisticsRules.Choice("acrFortify", "1") != "0"; }
 static bool AIUses() { return ACR_Options.Flag("acrFortifyAI"); }
 // Defenders held in reserve at a position of this tier: tier I 1, tier II 2, tier III 4 by
 // default (-acrFortifyDefenders1 / 2 / 3, 0 = none, at most 12). (Tier I had none: an AI's hasty
 // position stood empty once its squad moved on, playtest 25 Oct.)
 static int Defenders(int tier)
 {
  if (tier <= 0) return 0;
  if (tier == 1) return Math.ClampInt(ACR_LogisticsRules.Choice("acrFortifyDefenders1", "1").ToInt(), 0, 12);
  if (tier == 2) return Math.ClampInt(ACR_LogisticsRules.Choice("acrFortifyDefenders2", "2").ToInt(), 0, 12);
  if (tier >= 3) return Math.ClampInt(ACR_LogisticsRules.Choice("acrFortifyDefenders3", "4").ToInt(), 0, 12);
  return 0;
 }

 // Infantry that could take the order (the map feed's flag for the panel button).
 static bool CanFortify(ACR_Squad squad)
 {
  return Enabled() && squad && Infantry(squad) && squad.CountAlive() > 0;
 }

 // Not a vehicle crew. A gunner on a gun of one of the side's positions still counts as
 // infantry (the squad can upgrade it; a new order takes him off it).
 static bool Infantry(ACR_Squad squad)
 {
  IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
  if (!vehicle) return true;
  if (!squad.GetFaction()) return false;
  string faction = squad.GetFaction().GetFactionKey();
  foreach (ACR_Fortification fort : s_Positions)
   if (fort.m_sFaction == faction && fort.m_aTurrets.Contains(vehicle)) return true;
  return ACR_BaseWorks.OwnGun(faction, vehicle);
 }

 // ---------------------------------------------------------------- tiers and spots

 static int Cost(int tier)
 {
  if (tier <= 1) return 10;
  if (tier == 2) return 15;
  return 25;
 }

 static float Duration(int tier)
 {
  if (tier <= 1) return 45;
  if (tier == 2) return 60;
  return 90;
 }

 static string TierName(int tier)
 {
  if (tier <= 1) return "Fortified position I";
  if (tier == 2) return "Fortified position II";
  return "Fortified position III";
 }

 static string TierWords(int tier)
 {
  if (tier <= 1) return "hasty";
  if (tier == 2) return "dug in";
  return "strongpoint";
 }

 static string Hint()
 {
  return string.Format("Right-click: dig in there, facing the nearest known enemy or enemy base (%1 RP); again on our position to upgrade it (%2 / %3 RP); on the middle of our base to fortify the town (click Fortify again to cancel)", Cost(1), Cost(2), Cost(3));
 }

 // Each spot's place in the position's frame (x across, z towards the threat) and its piece's
 // footprint. Tier I is a line of three sandbag positions (each about 11 m wide, end to end), the
 // middle one a little ahead; tier II's MG nest (as wide) takes the middle one's place, so the gun
 // is in the centre of the line, joined to the posts on both sides (the user wanted the MG in the
 // middle, not on a flank, playtest 25 Oct). The heavy MG (III) goes past the right end of the
 // line, turned in across the front to cross the nest's fire, or past the left end when the right
 // has no clear place (SpotBases). The covered post (II) / bunker (III) is just behind the middle,
 // clear of the nest. The wire line runs in front (WireLine).
 static vector SpotOffset(int spot)
 {
  switch (spot)
  {
   case 0: return Vector(-10.5, 0, 0);
   case 1: return Vector(10.5, 0, 0);
   case 2: return Vector(0, 0, 1.5);
   case 3: return Vector(20, 0, 1);
   case 4: return Vector(0, 0, -10);
  }
  return vector.Zero;
 }

 // The places a spot's piece is tried around, best first: the heavy MG's right flank, then the
 // left one.
 static void SpotBases(int spot, notnull array<vector> bases)
 {
  vector offset = SpotOffset(spot);
  bases.Insert(offset);
  if (spot == 3) bases.Insert(Vector(-offset[0], 0, offset[2]));
 }

 // The MG nest's spot (the middle sandbag position at tier I) and the heavy MG's.
 static bool IsGun(int spot)
 {
  return spot == 2 || spot == 3;
 }

 // How far right of the position's centre a point is (negative: left).
 static float LocalX(vector site, float yaw, vector pos)
 {
  float rad = yaw * Math.DEG2RAD;
  vector offset = pos - site;
  return offset[0] * Math.Cos(rad) - offset[2] * Math.Sin(rad);
 }

 // The turn of a spot's piece standing at 'pos': the heavy MG turns in from whichever flank it is on.
 static float TurnAt(vector site, float yaw, int spot, vector pos)
 {
  if (spot == 3 && LocalX(site, yaw, pos) < 0) return FLANK_TURN;
  return SpotTurn(spot);
 }

 static float SpotRadius(int spot)
 {
  if (spot == 3) return 3;
  if (spot == 4) return 6;
  return 4;
 }

 // The end posts turn outwards along the arc; the heavy MG on the right flank turns in (TurnAt).
 static float SpotTurn(int spot)
 {
  if (spot == 0) return -15;
  if (spot == 1) return 15;
  if (spot == 3) return -FLANK_TURN;
  return 0;
 }

 // The spots a tier places or replaces a piece on (tier II's nest replaces the middle post).
 static bool SpotChangesAt(int spot, int tier)
 {
  if (spot <= 1) return tier == 1;
  if (spot == 2) return tier == 1 || tier == 2;
  if (spot == 3) return tier == 3;
  return tier == 2 || tier == 3;
 }

 // A spot's piece at a tier, for refusals the player reads.
 static string PieceName(int spot, int tier)
 {
  if (spot <= 1 || (spot == 2 && tier <= 1)) return "a sandbag position";
  if (spot == 2) return "the MG nest";
  if (spot == 3) return "the heavy MG";
  if (tier >= 3) return "the bunker";
  return "the covered post";
 }

 static ResourceName PrefabFor(int spot, string faction, int tier)
 {
  bool east = faction == "USSR" || faction.Contains("AFRF") || faction.Contains("RU") || faction.Contains("SLA");
  bool fia = !east && faction.Contains("FIA");
  if (spot <= 1 || (spot == 2 && tier <= 1))
  {
   if (east) return ACR_FobDefences.SANDBAGS_USSR;
   if (fia) return ACR_FobDefences.SANDBAGS_FIA;
   return ACR_FobDefences.SANDBAGS_US;
  }
  if (spot == 2)
  {
   if (east) return ACR_FobDefences.NEST_USSR;
   if (fia) return ACR_FobDefences.NEST_FIA;
   return ACR_FobDefences.NEST_US;
  }
  if (spot == 3)
  {
   if (east) return HMG_USSR;
   if (fia) return HMG_FIA;
   return HMG_US;
  }
  if (tier >= 3)
  {
   if (east) return ACR_FobDefences.BUNKER_USSR;
   if (fia) return ACR_FobDefences.BUNKER_FIA;
   return ACR_FobDefences.BUNKER_US;
  }
  if (east) return NET_USSR;
  if (fia) return NET_FIA;
  return NET_US;
 }

 // The wire line in the position's frame: across the front, bent back at both ends past the
 // heavy MG on either flank.
 static void WireLine(notnull array<vector> points)
 {
  points.Insert(Vector(-24, 0, 6));
  points.Insert(Vector(-11, 0, 14));
  points.Insert(Vector(11, 0, 14));
  points.Insert(Vector(24, 0, 6));
 }

 // The ground under a point (the terrain's or a road / runway surface's material name).
 static string SurfaceName(vector pos)
 {
  TraceParam param = new TraceParam();
  param.Start = pos + Vector(0, 2, 0);
  param.End = pos - Vector(0, 2, 0);
  param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  param.LayerMask = EPhysicsLayerPresets.Projectile;
  GetGame().GetWorld().TraceMove(param, null);
  SurfaceProperties surface = param.SurfaceProps;
  if (!surface) return "";
  string name = surface.GetName();
  name.ToLower();
  return name;
 }

 // Asphalt or concrete (a runway, a car park, a road off the road network).
 static bool HardSurface(vector pos)
 {
  string name = SurfaceName(pos);
  return name.Contains("asphalt") || name.Contains("concrete") || name.Contains("tarmac") || name.Contains("runway");
 }

 // Local (x across, z towards the threat) to world, for a position facing compass 'yaw'.
 static vector ToWorld(vector origin, float yaw, vector local)
 {
  float rad = yaw * Math.DEG2RAD;
  vector forward = Vector(Math.Sin(rad), 0, Math.Cos(rad));
  vector world = origin + Vector(forward[2], 0, -forward[0]) * local[0] + forward * local[2];
  world[1] = GetGame().GetWorld().GetSurfaceY(world[0], world[2]);
  return world;
 }

 // Where the holding squad's Defend goes (HOLDER_RADIUS round it): behind the line, or its centre
 // when the ground behind is water.
 static vector HoldCentre(ACR_Fortification fort)
 {
  vector centre = ToWorld(fort.m_vPos, fort.m_fYaw, Vector(0, 0, -HOLDER_BACK));
  if (BBB_Terrain.IsWaterAt(centre)) return fort.m_vPos;
  return centre;
 }

 // Where a spot's piece goes: where its current piece stands, else the spot itself or the first
 // clear point beside it, off asphalt and concrete if any will do. The position's own pieces
 // never block it. 'wide': an upgrade's piece also tries the wider ring (WideOffsets).
 static bool FindSpot(vector site, float yaw, int spot, ACR_Fortification target, out vector pos, out string reason, bool wide = false)
 {
  array<vector> places = {};
  if (!FindSpots(site, yaw, spot, target, places, reason, 1, -1, wide)) return false;
  pos = places[0];
  return true;
 }

 // An upgrade's pieces look further than a new position's (playtest 25 Oct: a covered post
 // behind a position by a road and a nest facing a rising bank were refused on every try, the
 // first fixed spots being the only ones): offsets past the near ones round a spot's base,
 // nearest first. The covered post / bunker moves back, or behind either end post (never
 // forward into the line); a gun moves up to 6 m along the line, a step back or 6 m forward.
 static void WideOffsets(int spot, notnull array<vector> offsets)
 {
  if (spot != 4 && !IsGun(spot)) return;
  array<float> across = {0, 2, -2, 4, -4, 6, -6};
  array<float> along = {0, 2, 4, -2, 6};
  array<float> postAcross = {0, 4, -4, 7, -7, 10.5, -10.5};
  array<float> postAlong = {0, -3, -6, -9, -12};
  if (spot == 4)
  {
   across = postAcross;
   along = postAlong;
  }
  array<vector> wide = {};
  foreach (float dz : along)
  {
   foreach (float dx : across)
   {
    vector offset = Vector(dx, 0, dz);
    if (!offsets.Contains(offset)) wide.Insert(offset);
   }
  }
  // Nearest first (a plain insertion sort: a few dozen offsets).
  for (int i = 1; i < wide.Count(); i++)
  {
   vector held = wide[i];
   int j = i - 1;
   while (j >= 0)
   {
    if (wide[j].Length() <= held.Length()) break;
    wide[j + 1] = wide[j];
    j--;
   }
   wide[j + 1] = held;
  }
  foreach (vector offset : wide) offsets.Insert(offset);
 }

 // ... up to 'most' clear places, best first (around each of SpotBases, or only the one numbered
 // 'onlyBase'). A gun never moves back (behind the posts) unless 'wide'.
 static bool FindSpots(vector site, float yaw, int spot, ACR_Fortification target, notnull array<vector> places, out string reason, int most, int onlyBase = -1, bool wide = false)
 {
  reason = "";
  if (s_bBlockSpotsForTest)
  {
   reason = "blocked (test)";
   return false;
  }
  IEntity old;
  if (target && spot < target.m_aPieces.Count()) old = target.m_aPieces[spot];
  array<IEntity> ignore = {};
  if (target)
  {
   foreach (IEntity piece : target.m_aPieces) if (piece) ignore.Insert(piece);
  }
  array<float> across = {0, 2, -2, 4, -4};
  array<float> along = {0, -2, 2};
  if (IsGun(spot))
  {
   along.Clear();
   along.Insert(0);
   along.Insert(2);
   along.Insert(4);
  }
  array<vector> offsets = {};
  foreach (float dz : along)
  {
   foreach (float dx : across) offsets.Insert(Vector(dx, 0, dz));
  }
  int near = offsets.Count();
  if (wide) WideOffsets(spot, offsets);
  array<vector> bases = {};
  SpotBases(spot, bases);
  for (int pass = 0; pass < 2; pass++)
  {
   foreach (int b, vector around : bases)
   {
    if (onlyBase >= 0 && b != onlyBase) continue;
    if (b == s_iBlockBaseForTest)
    {
     if (reason.IsEmpty()) reason = "blocked (test)";
     continue;
    }
    foreach (int k, vector offset : offsets)
    {
     if (k < near && spot == s_iBlockNearForTest)
     {
      if (reason.IsEmpty()) reason = "blocked (test)";
      continue;
     }
     vector candidate;
     bool kept = old && k == 0 && b == 0;
     if (kept) candidate = old.GetOrigin();
     else candidate = ToWorld(site, yaw, around + offset);
     if (pass == 0 && !kept && !s_bSkipSpotChecksForTest && HardSurface(candidate)) continue;
     string why;
     // A piece may reach a road's edge, not onto it (FOB buildings keep 1 m more for access; the
     // playtest's Airfield covered post fitted only there).
     if (!s_bSkipSpotChecksForTest && !ACR_FobBuild.FootprintClear(candidate, SpotRadius(spot), ignore, why, true, MAX_RISE, 0))
     {
      if (reason.IsEmpty()) reason = why;
      continue;
     }
     if (!s_bSkipSpotChecksForTest && RockIn(candidate, SpotRadius(spot)))
     {
      if (reason.IsEmpty()) reason = "big rocks in the way";
      continue;
     }
     if (places.Contains(candidate)) continue;
     places.Insert(candidate);
     reason = "";
     if (places.Count() >= most) return true;
    }
   }
  }
  return !places.IsEmpty();
 }

 // A big rock or boulder in a footprint (FOB footprints only count buildings and walls; a net
 // over rocks looked wrong, playtest 25 Oct). A stone in the grass doesn't count.
 protected static bool s_bRock;
 static bool RockIn(vector spot, float radius)
 {
  s_bRock = false;
  GetGame().GetWorld().QueryEntitiesBySphere(spot, radius, CollectRock, null, EQueryEntitiesFlags.STATIC);
  return s_bRock;
 }

 protected static bool CollectRock(IEntity e)
 {
  if (!e || e.GetParent()) return true;
  string name;
  if (e.GetPrefabData()) name = e.GetPrefabData().GetPrefabName();
  if (!name.Contains("/Rocks/") && e.GetVObject()) name = e.GetVObject().GetResourceName();
  name.ToLower();
  if (!name.Contains("/rocks/") && !name.Contains("boulder") && !name.Contains("cliff")) return true;
  vector mins, maxs;
  e.GetWorldBounds(mins, maxs);
  if (maxs[1] - mins[1] < ROCK_MIN_HEIGHT) return true;
  s_bRock = true;
  return false;
 }

 // A gun's line of fire (the user saw guns facing a sandbag wall, playtest 25 Oct): from just
 // above its mount, along its facing and 10 degrees either side, nothing but wire and people
 // within FIRE_CLEAR m: no other piece, building, tree or rising ground. Not a gun: true.
 protected static IEntity s_FirePiece;
 static bool LineOfFire(IEntity piece, out string detail)
 {
  detail = "";
  IEntity turret = ACR_Roadblocks.FindTurretInHierarchy(piece);
  if (!turret) return true;
  if (s_bBlockFireForTest)
  {
   detail = "line of fire blocked (test)";
   return false;
  }
  vector mins, maxs;
  turret.GetWorldBounds(mins, maxs);
  vector transform[4];
  turret.GetWorldTransform(transform);
  vector forward = transform[2];
  forward[1] = 0;
  if (forward.Length() < 0.01) return true;
  forward.Normalize();
  vector muzzle = turret.GetOrigin();
  muzzle[1] = Math.Max(muzzle[1], maxs[1] - 0.1);
  float facing = Math.Atan2(forward[0], forward[2]) * Math.RAD2DEG;
  return FireClear(muzzle, facing, piece, detail);
 }

 // The same check before the piece exists (an order's dry run, UpgradeBlocker): a gun facing
 // compass 'facing' at 'place', its muzzle MUZZLE_UP above the ground; 'replaced' is the piece it
 // takes the place of (the middle post), which doesn't count.
 static bool LineOfFireAt(vector place, float facing, IEntity replaced, out string detail)
 {
  detail = "";
  if (s_bBlockFireForTest)
  {
   detail = "line of fire blocked (test)";
   return false;
  }
  vector muzzle = place;
  muzzle[1] = GetGame().GetWorld().GetSurfaceY(place[0], place[2]) + MUZZLE_UP;
  return FireClear(muzzle, facing, replaced, detail);
 }

 // Rising ground in front only blocks a gun that can't aim up it (playtest 25 Oct: a nest's seven
 // places all "blocked 5-6 m out by GenericTerrain", about 11 degrees up): a trace that hits the
 // terrain is tried again aimed at a man's height above the ground FIRE_CLEAR m out, up to
 // MAX_AIM_UP degrees.
 protected static bool FireClear(vector muzzle, float facing, IEntity own, out string detail)
 {
  s_FirePiece = own;
  array<float> turns = {0, -10, 10};
  foreach (float turn : turns)
  {
   float rad = (facing + turn) * Math.DEG2RAD;
   vector dir = Vector(Math.Sin(rad), 0, Math.Cos(rad));
   TraceParam trace = new TraceParam();
   trace.Start = muzzle + dir * 1.2;
   trace.End = muzzle + dir * FIRE_RANGE;
   trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
   float hit = GetGame().GetWorld().TraceMove(trace, FireFilter);
   float clear = 1.2 + hit * (FIRE_RANGE - 1.2);
   if (hit >= 1 || clear >= FIRE_CLEAR) continue;
   bool ground = !trace.TraceEnt || GenericTerrainEntity.Cast(trace.TraceEnt) != null;
   if (ground && !s_bNoAimUpForTest && AimsUpClear(muzzle, dir)) continue;
   string what = "the ground";
   if (trace.TraceEnt && trace.TraceEnt.GetPrefabData()) what = FilePath.StripPath(trace.TraceEnt.GetPrefabData().GetPrefabName());
   else if (trace.TraceEnt) what = trace.TraceEnt.ClassName();
   detail = string.Format("line of fire blocked %1 m out by %2 (%3 deg)", (int)clear, what, turn);
   s_FirePiece = null;
   return false;
  }
  s_FirePiece = null;
  return true;
 }

 protected static bool AimsUpClear(vector muzzle, vector dir)
 {
  vector aim = muzzle + dir * FIRE_CLEAR;
  aim[1] = GetGame().GetWorld().GetSurfaceY(aim[0], aim[2]) + 1.0;
  float rise = aim[1] - muzzle[1];
  if (rise <= 0 || Math.Atan2(rise, FIRE_CLEAR) * Math.RAD2DEG > MAX_AIM_UP) return false;
  vector up = aim - muzzle;
  up.Normalize();
  TraceParam trace = new TraceParam();
  trace.Start = muzzle + up * 1.2;
  trace.End = aim;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  return GetGame().GetWorld().TraceMove(trace, FireFilter) >= 1;
 }

 protected static bool FireFilter(notnull IEntity e)
 {
  if (ChimeraCharacter.Cast(e) || Vehicle.Cast(e)) return false;
  IEntity root = e;
  while (root.GetParent()) root = root.GetParent();
  if (root == s_FirePiece) return false;
  if (root.GetPrefabData() && root.GetPrefabData().GetPrefabName().Contains("Barbed")) return false;
  return true;
 }

 static bool s_bSkipSpotChecksForTest;
 static bool s_bBlockSpotsForTest;
 static bool s_bBlockFireForTest;
 static int s_iBlockBaseForTest = -1;
 static int s_iBlockNearForTest = -1;     // this spot's near places are blocked (its wide ring isn't)
 static bool s_bNoAimUpForTest;           // guns don't aim up rising ground

 // A tier can be built when at least one of its spots is clear.
 static bool CheckTier(vector site, float yaw, int tier, ACR_Fortification target, out string reason)
 {
  reason = "";
  string first;
  for (int spot = 0; spot < SPOT_COUNT; spot++)
  {
   if (!SpotChangesAt(spot, tier)) continue;
   vector pos;
   string why;
   if (FindSpot(site, yaw, spot, target, pos, why)) return true;
   if (first.IsEmpty()) first = why;
  }
  reason = "every spot blocked (" + first + ")";
  return false;
 }

 // An upgrade goes in whole or not at all (Build), so before anything is charged: every piece it
 // changes needs a clear place, and each gun a clear line of fire from one of the places Build
 // will try (playtest 25 Oct: one clear spot passed the order, the squad paid, worked and was
 // refunded half a minute later, and the player saw only the RP go). "" when it fits, else what
 // stops it, for the player.
 static string UpgradeBlocker(vector site, float yaw, int tier, ACR_Fortification target)
 {
  for (int spot = 0; spot < SPOT_COUNT; spot++)
  {
   if (!SpotChangesAt(spot, tier)) continue;
   array<vector> places = {};
   string why;
   int most = 1;
   if (IsGun(spot)) most = GUN_PLACES_UPGRADE;
   if (!FindSpots(site, yaw, spot, target, places, why, most, -1, true)) return string.Format("%1 has no clear place (%2)", PieceName(spot, tier), why);
   if (!IsGun(spot)) continue;
   IEntity replaced;
   if (target && spot < target.m_aPieces.Count()) replaced = target.m_aPieces[spot];
   bool fires;
   string fireWhy;
   foreach (vector place : places)
   {
    if (!LineOfFireAt(place, yaw + TurnAt(site, yaw, spot, place), replaced, fireWhy)) continue;
    fires = true;
    break;
   }
   if (!fires) return string.Format("%1 has no clear line of fire (%2)", PieceName(spot, tier), fireWhy);
  }
  return "";
 }

 // A new position's tiers II and III would fit too (the AI commander's choice of sites).
 static bool UpgradesFit(vector site, float yaw)
 {
  return UpgradeBlocker(site, yaw, 2, null).IsEmpty() && UpgradeBlocker(site, yaw, 3, null).IsEmpty();
 }

 // A refused upgrade: the AI commander leaves the position alone for a while, and for good once
 // it has been refused UPGRADE_GIVE_UP times in a row for the same reason (playtest 25 Oct: an AI
 // tier I by big rocks was refused, nothing charged, every 15 min all game). Players may order it
 // again at any time.
 static void HoldUpgrades(ACR_Fortification fort, string why)
 {
  if (!fort) return;
  if (why == fort.m_sUpgradeRefused) fort.m_iUpgradeRefusals++;
  else
  {
   fort.m_sUpgradeRefused = why;
   fort.m_iUpgradeRefusals = 1;
  }
  fort.m_fUpgradeHeldUntil = GetGame().GetWorld().GetWorldTime() + UPGRADE_HOLD_S * 1000;
  if (fort.m_iUpgradeRefusals < UPGRADE_GIVE_UP) return;
  if (fort.m_fUpgradeHeldUntil < float.MAX) PrintFormat("[ACR] FORTIFY %1 position at %2: the AI gives up upgrading it (refused %3 times: %4)", fort.m_sFaction, fort.m_vPos, fort.m_iUpgradeRefusals, why);
  fort.m_fUpgradeHeldUntil = float.MAX;
 }

 static bool UpgradesHeld(ACR_Fortification fort)
 {
  return fort && GetGame().GetWorld().GetWorldTime() < fort.m_fUpgradeHeldUntil;
 }

 static bool VehicleOnSpots(ACR_FortifyPlan plan)
 {
  if (plan.m_Work) return ACR_BaseWorks.VehicleOnSite(plan);
  for (int spot = 0; spot < SPOT_COUNT; spot++)
  {
   if (!SpotChangesAt(spot, plan.m_iTier)) continue;
   if (ACR_FobSlots.VehicleOnSpot(ToWorld(plan.m_vSite, plan.m_fYaw, SpotOffset(spot)), SpotRadius(spot))) return true;
  }
  return false;
 }

 // ---------------------------------------------------------------- orders

 // The server side of the order (requests, AI commander, tests): plan the tier, then the squad
 // pays and starts; like any new order it replaces the squad's current job (laying mines,
 // fortifying, a FOB or roadblock build), which refunds itself. faceTowards: a point the
 // position faces (zero: the default facing). false + refusal, nothing charged, when it can't.
 static bool Order(ACR_Squad squad, vector point, out string refusal, vector faceTowards = vector.Zero)
 {
  ACR_FortifyPlan plan = Plan(squad, point, refusal, faceTowards);
  if (!plan) return false;
  if (!squad.Fortify(plan))
  {
   refusal = "unable to start the work";
   return false;
  }
  return true;
 }

 static ACR_FortifyPlan Plan(ACR_Squad squad, vector point, out string refusal, vector faceTowards = vector.Zero)
 {
  refusal = "";
  if (!Enabled())
  {
   refusal = "fortifying is turned off for this match";
   return null;
  }
  if (!squad || squad.CountAlive() <= 0 || !squad.GetAnchor() || !squad.GetFaction())
  {
   refusal = "squad is unavailable";
   return null;
  }
  if (!Infantry(squad))
  {
   refusal = "vehicles cannot fortify";
   return null;
  }
  point = ACR_Manager.Ground(point);
  if (BBB_Terrain.IsWaterAt(point))
  {
   refusal = "that spot is in water";
   return null;
  }
  string faction = squad.GetFaction().GetFactionKey();
  ACR_FortifyPlan plan = new ACR_FortifyPlan();
  plan.m_sFaction = faction;
  plan.m_Target = NearestOwn(faction, point, UPGRADE_RADIUS);
  if (plan.m_Target)
  {
   if (plan.m_Target.m_iTier >= MAX_TIER)
   {
    refusal = "that position is already a strongpoint";
    return null;
   }
   if (BeingBuilt(plan.m_Target, squad))
   {
    refusal = "another squad is already upgrading that position";
    return null;
   }
   plan.m_vSite = plan.m_Target.m_vPos;
   plan.m_fYaw = plan.m_Target.m_fYaw;
   plan.m_iTier = plan.m_Target.m_iTier + 1;
  }
  else
  {
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (!base || base.m_bRetired || vector.DistanceXZ(point, base.GetOrigin()) >= MIN_BASE_DIST) continue;
    refusal = "too close to a base (units deploy there)";
    return null;
   }
   if (NearestOwn(faction, point, SPACING))
   {
    refusal = "too close to another of our positions (click on it to upgrade it)";
    return null;
   }
   if (CountPositions(faction) >= MAX_POSITIONS)
   {
    refusal = string.Format("our side already has %1 fortified positions", MAX_POSITIONS);
    return null;
   }
   plan.m_vSite = point;
   plan.m_fYaw = FacingYaw(squad, point, faction, faceTowards);
   plan.m_iTier = 1;
  }
  plan.m_iCost = Cost(plan.m_iTier);
  // The order replaces the squad's current job, whose refund counts (ACR_Squad.Fortify pays
  // after NewOrder).
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || manager.m_Bases.GetRP(faction) + squad.PendingRP() < plan.m_iCost)
  {
   refusal = string.Format("not enough RP (%1 needed)", plan.m_iCost);
   return null;
  }
  if (plan.m_Target)
  {
   string blocker = UpgradeBlocker(plan.m_vSite, plan.m_fYaw, plan.m_iTier, plan.m_Target);
   if (!blocker.IsEmpty())
   {
    refusal = "upgrade not possible here: " + blocker;
    HoldUpgrades(plan.m_Target, blocker);
    PrintFormat("[ACR] FORTIFY %1 upgrade to %2 at %3 refused when ordered: %4", faction, TierName(plan.m_iTier), plan.m_vSite, blocker);
    return null;
   }
  }
  else if (!CheckTier(plan.m_vSite, plan.m_fYaw, plan.m_iTier, plan.m_Target, refusal)) return null;
  // The squad works from behind the position.
  plan.m_vStage = ToWorld(plan.m_vSite, plan.m_fYaw, Vector(0, 0, -22));
  if (BBB_Terrain.IsWaterAt(plan.m_vStage)) plan.m_vStage = plan.m_vSite;
  plan.m_fRadius = 30;
  plan.m_fDuration = Duration(plan.m_iTier) * WORK_SCALE;
  float now = GetGame().GetWorld().GetWorldTime();
  plan.m_fLastTick = now;
  plan.m_fLastProgress = now;
  plan.m_fDeadline = now + vector.DistanceXZ(squad.GetAnchor().GetOrigin(), plan.m_vStage) / ACR_Minefields.TRAVEL_SPEED * 1000 + ACR_Squad.FOB_GIVE_UP_MS;
  return plan;
 }

 // At the threat (ACR_Intel.ThreatPoint, as a player's roadblock), else away from the squad (the
 // way it was going); the AI commander passes the enemy base it faces.
 static float FacingYaw(ACR_Squad squad, vector point, string faction, vector faceTowards)
 {
  vector target = faceTowards;
  if (target == vector.Zero) target = ACR_Intel.ThreatPoint(faction, point);
  vector dir = point - squad.GetAnchor().GetOrigin();
  if (target != vector.Zero) dir = target - point;
  dir[1] = 0;
  if (dir.Length() < 1) dir = "0 0 1";
  return Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
 }

 static ACR_Fortification NearestOwn(string faction, vector pos, float radius)
 {
  ACR_Fortification best;
  float bestDistance = radius;
  foreach (ACR_Fortification fort : s_Positions)
  {
   if (fort.m_sFaction != faction) continue;
   float d = vector.DistanceXZ(fort.m_vPos, pos);
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = fort;
  }
  return best;
 }

 // A squad (other than 'except', whose new order replaces its own) is already working on it.
 static bool BeingBuilt(ACR_Fortification fort, ACR_Squad except = null)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad != except && squad.m_FortifyPlan && squad.m_FortifyPlan.m_Target == fort) return true;
  return false;
 }

 static int CountPositions(string faction)
 {
  int count;
  foreach (ACR_Fortification fort : s_Positions) if (fort.m_sFaction == faction) count++;
  return count;
 }

 // ---------------------------------------------------------------- building

 // The finished work: the tier's pieces on every clear spot. The position (new or upgraded),
 // or null + reason when no piece could go in.
 static ACR_Fortification Build(ACR_FortifyPlan plan, out string reason)
 {
  reason = "";
  ACR_Fortification fort = plan.m_Target;
  if (fort && !s_Positions.Contains(fort)) fort = null;
  bool created = !fort;
  if (created)
  {
   fort = new ACR_Fortification();
   fort.m_sFaction = plan.m_sFaction;
   fort.m_vPos = plan.m_vSite;
   fort.m_fYaw = plan.m_fYaw;
   for (int i = 0; i < SPOT_COUNT; i++) fort.m_aPieces.Insert(null);
  }
  // Where each changed piece goes, before anything is placed. An upgrade goes in whole or not at
  // all (refunded, the tier unchanged, so it can be ordered again once the spot is clear); a new
  // position skips a blocked spot.
  int placed, skipped;
  array<vector> found = {};
  array<bool> clear = {};
  for (int spot = 0; spot < SPOT_COUNT; spot++)
  {
   vector pos;
   string why;
   bool changes = SpotChangesAt(spot, plan.m_iTier);
   bool fits = changes && FindSpot(fort.m_vPos, fort.m_fYaw, spot, fort, pos, why, !created);
   found.Insert(pos);
   clear.Insert(fits);
   if (!changes || fits) continue;
   if (!created)
   {
    reason = string.Format("%1 has no clear place (%2)", PieceName(spot, plan.m_iTier), why);
    HoldUpgrades(fort, reason);
    PrintFormat("[ACR] FORTIFY %1 upgrade to %2 at %3 refused: spot %4 blocked (%5)", fort.m_sFaction, TierName(plan.m_iTier), fort.m_vPos, spot, why);
    return null;
   }
   skipped++;
   if (reason.IsEmpty()) reason = why;
   PrintFormat("[ACR] FORTIFY spot %1 skipped (%2)", spot, why);
  }
  // Guns first (a gun with no clear line of fire anywhere fails an upgrade before anything else
  // goes in): each tries its spot's places, around each of its bases, until LineOfFire is clear.
  array<int> order = {2, 3, 0, 1, 4};
  int gunPlaces = GUN_PLACES;
  if (!created) gunPlaces = GUN_PLACES_UPGRADE;
  foreach (int s : order)
  {
   if (!clear[s]) continue;
   array<vector> tries = {found[s]};
   string tryWhy;
   if (IsGun(s))
   {
    array<vector> bases = {};
    SpotBases(s, bases);
    for (int b = 0; b < bases.Count(); b++) FindSpots(fort.m_vPos, fort.m_fYaw, s, fort, tries, tryWhy, tries.Count() + gunPlaces, b, !created);
   }
   // What stood there goes back if the upgrade is refused (the middle post the nest replaces).
   ResourceName oldPrefab;
   vector oldAt;
   float oldYaw;
   IEntity old = fort.m_aPieces[s];
   if (old && old.GetPrefabData())
   {
    oldPrefab = old.GetPrefabData().GetPrefabName();
    oldAt = old.GetOrigin();
    vector oldAngles = old.GetYawPitchRoll();
    oldYaw = oldAngles[0];
   }
   RemovePiece(fort, s);
   IEntity piece;
   foreach (vector place : tries)
   {
    ACR_FobBuild.FellTrees(place, SpotRadius(s));
    piece = SpawnPiece(PrefabFor(s, fort.m_sFaction, plan.m_iTier), place, fort.m_fYaw + TurnAt(fort.m_vPos, fort.m_fYaw, s, place), fort.m_sFaction);
    if (!piece) continue;
    string fireWhy;
    if (LineOfFire(piece, fireWhy))
    {
     found[s] = place;
     break;
    }
    PrintFormat("[ACR] FORTIFY spot %1 at %2: %3, trying another place", s, place, fireWhy);
    DeletePiece(piece);
    piece = null;
    reason = fireWhy;
   }
   if (!piece && IsGun(s) && !created)
   {
    if (!oldPrefab.IsEmpty())
    {
     fort.m_aPieces[s] = SpawnPiece(oldPrefab, oldAt, oldYaw, fort.m_sFaction);
     RefreshTurrets(fort);
    }
    PrintFormat("[ACR] FORTIFY %1 upgrade to %2 at %3 refused: no clear line of fire for spot %4", fort.m_sFaction, TierName(plan.m_iTier), fort.m_vPos, s);
    if (reason.IsEmpty()) reason = "no clear line of fire";
    reason = string.Format("%1 has no clear line of fire (%2)", PieceName(s, plan.m_iTier), reason);
    HoldUpgrades(fort, reason);
    return null;
   }
   fort.m_aPieces[s] = piece;
   if (!piece) continue;
   placed++;
   PrintFormat("[ACR] FORTIFY spot %1 at %2 on '%3'", s, found[s], SurfaceName(found[s]));
  }
  // The upkeep counts pieces, not wire: a position needs at least one.
  if (placed == 0)
  {
   if (reason.IsEmpty()) reason = "no piece could be placed";
   return null;
  }
  if (plan.m_iTier == 1) LayWire(fort);
  RefreshTurrets(fort);
  int wasTier = fort.m_iTier;
  fort.m_iTier = plan.m_iTier;
  if (created)
  {
   s_iNextId++;
   fort.m_iId = s_iNextId;
   s_Positions.Insert(fort);
  }
  PrintFormat("[ACR] FORTIFY %1 %2 at %3 facing %4: %5 placed (+%6 wire), %7 skipped", fort.m_sFaction, TierName(fort.m_iTier), fort.m_vPos, (int)fort.m_fYaw, placed, fort.m_aWire.Count(), skipped);
  RaiseDefenders(fort, wasTier);
  return fort;
 }

 // Tier I's wire: segments joined end to end (a little overlap) along WireLine. A segment that
 // would stand in water, on a road or in a building is left out. Returns the segments laid.
 protected static int LayWire(ACR_Fortification fort)
 {
  array<vector> line = {};
  WireLine(line);
  float length = WireLength(fort.m_vPos);
  int laid;
  for (int i = 0; i + 1 < line.Count(); i++)
  {
   vector run = line[i + 1] - line[i];
   float runLength = run.Length();
   int count = (int)Math.Ceil(runLength / length);
   float runYaw = Math.Atan2(run[0], run[2]) * Math.RAD2DEG;
   float yaw = fort.m_fYaw + runYaw;
   if (s_bWireAlongX) yaw -= 90;
   for (int k = 0; k < count; k++)
   {
    vector pos = ToWorld(fort.m_vPos, fort.m_fYaw, line[i] + run * ((k + 0.5) / count));
    string why;
    if (!s_bSkipSpotChecksForTest && (!ACR_FobBuild.FootprintClear(pos, 1.5, null, why, true, MAX_RISE) || RockIn(pos, 1.5))) continue;
    IEntity segment = SpawnPiece(WIRE, pos, yaw, fort.m_sFaction);
    if (!segment) continue;
    fort.m_aWire.Insert(segment);
    laid++;
   }
  }
  return laid;
 }

 // The wire segment's length and long axis, measured once from a segment spawned and removed.
 protected static float s_fWireLength = -1;
 protected static bool s_bWireAlongX;
 static bool WireAlongX() { return s_bWireAlongX; }
 static float WireLength(vector at)
 {
  if (s_fWireLength > 0) return s_fWireLength;
  s_fWireLength = 4;
  Resource resource = Resource.Load(WIRE);
  if (!resource.IsValid()) return s_fWireLength;
  EntitySpawnParams spawnParams = new EntitySpawnParams();
  spawnParams.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(vector.Zero, spawnParams.Transform);
  spawnParams.Transform[3] = at;
  IEntity probe = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
  if (!probe) return s_fWireLength;
  vector mins, maxs;
  probe.GetBounds(mins, maxs);
  SCR_EntityHelper.DeleteEntityAndChildren(probe);
  float x = maxs[0] - mins[0];
  float z = maxs[2] - mins[2];
  s_bWireAlongX = x > z;
  s_fWireLength = Math.Max(1, Math.Max(x, z));
  PrintFormat("[ACR] FORTIFY wire segment: %1 m (along %2)", s_fWireLength, s_bWireAlongX);
  return s_fWireLength;
 }

 static IEntity SpawnPiece(ResourceName prefab, vector pos, float yaw, string faction)
 {
  if (prefab.IsEmpty()) return null;
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return null;
  EntitySpawnParams spawnParams = new EntitySpawnParams();
  spawnParams.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(yaw, 0, 0), spawnParams.Transform);
  spawnParams.Transform[3] = pos;
  IEntity piece = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
  if (!piece) return null;
  piece.SetYawPitchRoll(Vector(yaw, 0, 0));
  ACR_GroundTilt.Place(piece, prefab, pos, yaw); // on a slope sandbags and wire lie on the ground
  SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(piece.FindComponent(SCR_CampaignBuildingCompositionComponent));
  if (comp)
  {
   comp.ACR_SetFaction(faction);
   comp.ACR_MarkBuildCharged();
   comp.ACR_SetCompositionSpawned(true);
  }
  ACR_Navmesh.RebuildAfterSpawn(piece);
  return piece;
 }

 protected static void RemovePiece(ACR_Fortification fort, int spot)
 {
  IEntity piece = fort.m_aPieces[spot];
  fort.m_aPieces[spot] = null;
  DeletePiece(piece);
  RefreshTurrets(fort);
 }

 protected static void DeletePiece(IEntity piece)
 {
  if (!piece) return;
  SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(piece.FindComponent(SCR_CampaignBuildingCompositionComponent));
  if (comp) ACR_Structures.s_Registered.RemoveItem(comp);
  ACR_Navmesh.DeleteStructure(piece);
 }

 // The position's guns (the MG nest, the heavy MG).
 static void Turrets(ACR_Fortification fort, notnull array<IEntity> turrets)
 {
  if (!fort) return;
  foreach (IEntity turret : fort.m_aTurrets) if (turret) turrets.Insert(turret);
 }

 // Kept with the position (Infantry runs for every vehicle crew on the map feed): after its
 // pieces change.
 static void RefreshTurrets(ACR_Fortification fort)
 {
  fort.m_aTurrets.Clear();
  foreach (IEntity piece : fort.m_aPieces)
  {
   if (!piece) continue;
   IEntity turret = ACR_Roadblocks.FindTurretInHierarchy(piece);
   if (turret) fort.m_aTurrets.Insert(turret);
  }
 }

 // A tier with defenders adds its share to the position's reserve (an upgrade: the difference).
 static void RaiseDefenders(ACR_Fortification fort, int wasTier)
 {
  int more = Defenders(fort.m_iTier) - Defenders(wasTier);
  if (more <= 0) return;
  if (!fort.m_Defenders) fort.m_Defenders = ACR_Defenders.Add(fort.m_sFaction, fort.m_vPos, string.Format("%1 position %2", fort.m_sFaction, fort.m_iId), HOLD_RADIUS);
  fort.m_Defenders.m_iReserve += more;
  PrintFormat("[ACR] FORTIFY %1 %2 at %3: %4 defenders in reserve", fort.m_sFaction, TierName(fort.m_iTier), fort.m_vPos, fort.m_Defenders.m_iReserve);
 }

 static int Standing(ACR_Fortification fort)
 {
  int count;
  foreach (IEntity piece : fort.m_aPieces) if (piece) count++;
  return count;
 }

 // ---------------------------------------------------------------- upkeep

 // From ACR_Manager.Tick: every 5 s drop pieces that were dismantled or destroyed; a position
 // with none left goes off the map.
 static void Tick()
 {
  s_iTicks++;
  if (s_iTicks % 5 != 0) return;
  ACR_Defenders.Tick();
  ACR_BaseWorks.Tick();
  if (s_Positions.IsEmpty()) return;
  for (int f = s_Positions.Count() - 1; f >= 0; f--)
  {
   ACR_Fortification fort = s_Positions[f];
   for (int i = 0; i < fort.m_aPieces.Count(); i++)
   {
    IEntity piece = fort.m_aPieces[i];
    if (piece && ACR_Logistics.Health(piece) <= 0) RemovePiece(fort, i);
   }
   if (Standing(fort) > 0) continue;
   PrintFormat("[ACR] FORTIFY %1 %2 at %3 is gone (every piece destroyed or dismantled)", fort.m_sFaction, TierName(fort.m_iTier), fort.m_vPos);
   // Its wire goes with it.
   foreach (IEntity segment : fort.m_aWire) DeletePiece(segment);
   fort.m_aWire.Clear();
   ACR_Defenders.Remove(fort.m_Defenders);
   s_Positions.RemoveOrdered(f);
  }
 }

 // The owning side's command map: a pale ring per position ("~": not a danger area).
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  foreach (ACR_Fortification fort : s_Positions)
  {
   if (fort.m_sFaction != faction) continue;
   positions.Insert(fort.m_vPos);
   radii.Insert(HOLD_RADIUS);
   labels.Insert("~" + TierName(fort.m_iTier));
  }
  ACR_BaseWorks.Zones(faction, positions, radii, labels);
 }

 static void Reset()
 {
  s_Positions.Clear();
  ACR_Defenders.Reset();
  ACR_BaseWorks.Reset();
  s_iNextId = 0;
  s_iTicks = 0;
  WORK_SCALE = 1;
  s_bSkipSpotChecksForTest = false;
  s_bBlockSpotsForTest = false;
  s_bBlockFireForTest = false;
  s_iBlockBaseForTest = -1;
 }
}
