// Fields of fire (3 Nov 2026, the user's playtest: a roadblock's sandbag nest had a bush right in front
// of it, and the man behind the bags couldn't see past it). A gun or a post that is built knocks down
// the bushes and saplings round it: all round close in (ROUND_M), where any of them blocks a kneeling
// man whichever way he looks, and further out in a cone in front of a gun (GUN_FRONT_M, GUN_HALF_DEG).
// Vegetation under MAX_HEIGHT_M only: ACR_FobBuild.FellTrees fells the trees, under a footprint.
//
// Two kinds of bush:
//  - the map's (BaseTree): deleted. Most of Arland's have destruction switched off in their prefabs
//    (Bush_Base "Enabled 0"), so neither vanilla's kill (SCR_DestructibleEntity.ForceKill_S) nor a
//    vehicle-style collision hit takes them down (srv:fofprobe, 3 Nov: 0 of 17 and 1 of 7; deleting, 3 of 3).
//  - a structure's own: vanilla's medium and large checkpoints carry cut branches as camouflage
//    (Prefabs/Vegetation/Cuttings, GenericEntity with a static collider), two of them either side of
//    the MG nest, just in front of it (the user's pn36 playtest: "the bushes are still there"). Hidden
//    and their collider removed, not deleted: the structure's layout record counts every piece
//    (ACR_StructureLayout), so its pieces must stay the same on every machine.
//
// The AI runs on the server, which clears them; the server records what it cleared so every machine
// clears the same (a map entity's deletion isn't replicated, and each machine builds a structure's
// pieces from its prefab). A built structure's own pieces are recorded on it by their place in its
// prefab: the coords their entity source gives them and their own prefab, the same on every machine
// (they go when it is dismantled). Not by index or by where they stand: a client joined to a dedicated
// server builds a checkpoint with its pieces in another order and 6 fewer (52 vs 58, -acrFofMPTest,
// 4 Nov), and leaves it as built, since ACR_StructureLayout refuses a record whose count differs.
// Everything else (map bushes, and pieces of what the map
// itself placed) by its spot on the game mode, gone for the whole mission (up to MAX_SPOTS of them).
// A client clears from a record when the structure (or the game) streams in and whenever it grows.
// Host option -acrFieldsOfFire 1 turns it on; off by default (an AI option until it has been played,
// Jacob's options policy), the bushes stay as before.
class ACR_FieldOfFire
{
 static const float ROUND_M = 7;          // all round a gun, a post or a piece
 static const float POST_M = 10;          // all round a roadblock's post (its holders spread round it)
 static const float GUN_FRONT_M = 35;     // in front of a gun
 static const float GUN_HALF_DEG = 35;    // the cone either side of the gun's line
 static const float MAX_HEIGHT_M = 4;     // taller is a tree
 static const ResourceName CUTTINGS = "{B3400E8AAE81A0E8}Prefabs/Vegetation/Core/VegetationCuttings_base.et";
 static const float SPOT_STEP = 0.01;     // m: a cleared map bush's spot in the record, in cm
 static const float MATCH_M = 0.05;       // a map bush is where the server's was, to the cm
 static const int MAX_SPOTS = 4000;       // ints in the game mode's record (2 a bush): 16 KB on the wire
 static int s_iCleared;                   // tests and the log
 static int s_iMirrored;                  // cleared on this machine from a record (tests)
 static bool s_bTestOn;                   // srv:fieldsoffire runs it without the host option

 static bool On()
 {
  if (s_bTestOn) return true;
  string value;
  if (System.GetCLIParam("acrFieldsOfFire", value)) return value == "1";
  return false;
 }

 // A gun: all round it, and its cone along the way it points.
 static int ForGun(IEntity gun)
 {
  if (!gun) return 0;
  vector mat[4];
  gun.GetWorldTransform(mat);
  return Cone(gun.GetOrigin(), mat[2], "gun");
 }

 // All round a spot and in the cone along facing (a gun's, or a test's).
 static int Cone(vector at, vector facing, string what)
 {
  facing[1] = 0;
  return Clear(at, facing, ROUND_M, GUN_FRONT_M, what);
 }

 // A post or a spot: all round it.
 static int Round(vector at, float radius, string what)
 {
  return Clear(at, vector.Zero, radius, 0, what);
 }

 // A built piece (sandbags, a nest, a bunker...): all round it, and as a gun for a gun in it.
 static int ForPiece(IEntity piece)
 {
  if (!piece) return 0;
  int cleared = Round(piece.GetOrigin(), ROUND_M, "piece");
  IEntity gun = ACR_Roadblocks.FindTurretInHierarchy(piece);
  if (gun) cleared += ForGun(gun);
  return cleared;
 }

 // The structure an entity is a piece of (its topmost parent), or null for one on its own.
 static IEntity Structure(IEntity e)
 {
  if (!e || !e.GetParent()) return null;
  IEntity root = e.GetParent();
  while (root.GetParent()) root = root.GetParent();
  return root;
 }

 protected static int Clear(vector centre, vector facing, float round, float front, string what)
 {
  if (!Replication.IsServer() || !On()) return 0;
  BaseWorld world = GetGame().GetWorld();
  if (!world) return 0;
  ACR_FieldOfFireQuery query = new ACR_FieldOfFireQuery();
  world.QueryEntitiesBySphere(centre, Math.Max(round, front), query.Add, null, EQueryEntitiesFlags.ALL);
  bool cone = front > 0 && facing.LengthSq() > 0.001;
  if (cone) facing.Normalize();
  float cosHalf = Math.Cos(GUN_HALF_DEG * Math.DEG2RAD);
  array<int> spots = {};
  int cleared;
  foreach (IEntity bush : query.m_aFound)
  {
   if (!bush) continue;
   vector to = bush.GetOrigin() - centre;
   to[1] = 0;
   float distance = to.Length();
   bool inside = distance <= round;
   if (!inside && cone && distance <= front && distance > 0.01)
   {
    to.Normalize();
    inside = vector.Dot(to, facing) >= cosHalf;
   }
   if (!inside) continue;
   IEntity owner = Structure(bush);
   SCR_CampaignBuildingCompositionComponent comp = ACR_StructureLayout.Comp(owner);
   if (comp)
   {
    // A built structure's piece (a checkpoint's cut branches): hidden, recorded by its place in the
    // structure's prefab.
    if (!Hide(bush)) continue;
    array<IEntity> parts = {};
    ACR_StructureLayout.Parts(owner, parts);
    array<int> keys = {};
    PieceKeys(parts, keys);
    int k = parts.Find(bush) * 4;
    if (k >= 0 && keys[k + 3] != 0) comp.ACR_AddClearedPiece(keys[k], keys[k + 1], keys[k + 2], keys[k + 3]);
    cleared++;
    continue;
   }
   // A map bush (deleted), or a piece of something the map placed (hidden): recorded by its spot.
   vector at = bush.GetOrigin();
   spots.Insert((int)Math.Round(at[0] / SPOT_STEP));
   spots.Insert((int)Math.Round(at[2] / SPOT_STEP));
   if (owner) Hide(bush);
   else SCR_EntityHelper.DeleteEntityAndChildren(bush);
   cleared++;
  }
  if (!spots.IsEmpty()) Record(spots);
  s_iCleared += cleared;
  if (cleared > 0) PrintFormat("[ACR] FIELD OF FIRE: %1 bushes cleared round a %2 at %3", cleared, what, centre);
  return cleared;
 }

 // The map bushes' spots for every machine, on the game mode.
 protected static void Record(notnull array<int> spots)
 {
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (mode) mode.ACR_AddClearedSpots(spots);
 }

 // A piece of a structure out of sight and out of the way: invisible, its collider gone.
 static bool Hide(IEntity piece)
 {
  if (!piece || !(piece.GetFlags() & EntityFlags.VISIBLE)) return false;
  piece.ClearFlags(EntityFlags.VISIBLE, true);
  Physics physics = piece.GetPhysics();
  if (physics) physics.Destroy();
  piece.Update();
  return true;
 }

 // A structure's piece by its place in the structure's prefab: the coords its entity source gives it
 // (cm, in its parent's frame) and a hash of its own prefab. False for a piece without them.
 static bool PieceKey(IEntity piece, out int x, out int y, out int z, out int prefab)
 {
  if (!piece || !piece.GetPrefabData()) return false;
  BaseContainer source = piece.GetPrefabData().GetPrefab();
  if (!source) return false;
  vector coords;
  if (!source.Get("coords", coords)) return false;
  BaseContainer named = source;
  while (named && named.GetResourceName().IsEmpty()) named = named.GetAncestor();
  if (!named) return false;
  string name = named.GetResourceName();
  x = (int)Math.Round(coords[0] / SPOT_STEP);
  y = (int)Math.Round(coords[1] / SPOT_STEP);
  z = (int)Math.Round(coords[2] / SPOT_STEP);
  prefab = name.Hash();
  return true;
 }

 // Every piece's key (4 ints each, all 0 for a piece without one), in Parts order. Pieces of one prefab
 // at one place (a prefab's grouped copies give all theirs the group's coords) are told apart by their
 // order among themselves, which is the prefab's on every machine.
 static void PieceKeys(notnull array<IEntity> parts, notnull array<int> keys)
 {
  map<string, int> seen = new map<string, int>();
  foreach (IEntity part : parts)
  {
   int x, y, z, h;
   if (!PieceKey(part, x, y, z, h))
   {
    keys.Insert(0); keys.Insert(0); keys.Insert(0); keys.Insert(0);
    continue;
   }
   string place = string.Format("%1,%2,%3,%4", x, y, z, h);
   int n = seen.Get(place);
   seen.Set(place, n + 1);
   if (n > 0) h = string.Format("%1#%2", h, n).Hash();
   if (h == 0) h = 1;
   keys.Insert(x); keys.Insert(y); keys.Insert(z); keys.Insert(h);
  }
 }

 // Another machine: a structure's pieces the server cleared, by their place in its prefab (4 ints a
 // piece): each of its bushes and cut branches with a recorded key is hidden. Run again, a record hides
 // nothing more. 'test' runs it on the server.
 static int MirrorPieces(IEntity root, notnull array<int> pieces, bool test = false)
 {
  if (!root || pieces.Count() < 4 || (Replication.IsServer() && !test)) return 0;
  array<IEntity> parts = {};
  ACR_StructureLayout.Parts(root, parts);
  array<int> keys = {};
  PieceKeys(parts, keys);
  int hidden;
  foreach (int p, IEntity part : parts)
  {
   if (!part || !(part.GetFlags() & EntityFlags.VISIBLE) || !ACR_FieldOfFireQuery.IsBush(part)) continue;
   int x = keys[p * 4];
   int y = keys[p * 4 + 1];
   int z = keys[p * 4 + 2];
   int h = keys[p * 4 + 3];
   if (h == 0) continue;
   for (int i = 0; i + 3 < pieces.Count(); i += 4)
   {
    if (pieces[i] != x || pieces[i + 1] != y || pieces[i + 2] != z || pieces[i + 3] != h) continue;
    if (Hide(part)) hidden++;
    break;
   }
  }
  s_iMirrored += hidden;
  return hidden;
 }

 // Another machine: the map bushes (and map-placed pieces) the server cleared, by their spots. Run
 // again, a spot finds nothing: the next bush is never within MATCH_M. 'test' runs it on the server.
 static int MirrorSpots(notnull array<int> spots, bool test = false)
 {
  if (spots.IsEmpty() || (Replication.IsServer() && !test)) return 0;
  BaseWorld world = GetGame().GetWorld();
  if (!world) return 0;
  int cleared;
  for (int i = 0; i + 1 < spots.Count(); i += 2)
  {
   vector at = Vector(spots[i] * SPOT_STEP, 0, spots[i + 1] * SPOT_STEP);
   at[1] = world.GetSurfaceY(at[0], at[2]);
   ACR_FieldOfFireQuery query = new ACR_FieldOfFireQuery();
   world.QueryEntitiesBySphere(at, 2, query.Add, null, EQueryEntitiesFlags.ALL);
   IEntity nearest;
   float best = MATCH_M;
   foreach (IEntity bush : query.m_aFound)
   {
    if (!bush) continue;
    float d = vector.DistanceXZ(bush.GetOrigin(), at);
    if (d > best) continue;
    best = d;
    nearest = bush;
   }
   if (!nearest) continue;
   if (nearest.GetParent()) Hide(nearest);
   else SCR_EntityHelper.DeleteEntityAndChildren(nearest);
   cleared++;
  }
  s_iMirrored += cleared;
  return cleared;
 }
}

// The bushes, saplings and cut branches a query finds, still standing (an instance: no static array
// for the static-init budget).
class ACR_FieldOfFireQuery : Managed
{
 ref array<IEntity> m_aFound = {};

 bool Add(IEntity e)
 {
  if (!(e.GetFlags() & EntityFlags.VISIBLE)) return true; // already cleared
  if (!IsBush(e)) return true;
  vector mins, maxs;
  e.GetBounds(mins, maxs);
  if (maxs[1] - mins[1] >= ACR_FieldOfFire.MAX_HEIGHT_M) return true;
  m_aFound.Insert(e);
  return true;
 }

 static bool IsBush(IEntity e)
 {
  if (BaseTree.Cast(e))
  {
#ifdef ENABLE_DESTRUCTION
   if (SCR_DestructibleTreeV2.Cast(e)) return false; // a real tree: ACR_FobBuild fells those
#endif
   return true;
  }
  // Cut branches (vanilla's checkpoints carry them).
  EntityPrefabData data = e.GetPrefabData();
  if (!data) return false;
  BaseContainer prefab = data.GetPrefab();
  return prefab && SCR_BaseContainerTools.IsKindOf(prefab, ACR_FieldOfFire.CUTTINGS);
 }
}

// A structure's record of its own pieces it cleared, for every machine (ACR_FieldOfFire).
modded class SCR_CampaignBuildingCompositionComponent
{
 [RplProp(onRplName: "ACR_OnCleared")]
 protected ref array<int> m_aACRClearedPieces = {};

 // A piece by its place in the structure's prefab (ACR_FieldOfFire.PieceKey): 4 ints.
 void ACR_AddClearedPiece(int x, int y, int z, int prefab)
 {
  m_aACRClearedPieces.Insert(x);
  m_aACRClearedPieces.Insert(y);
  m_aACRClearedPieces.Insert(z);
  m_aACRClearedPieces.Insert(prefab);
  Replication.BumpMe();
 }

 // The record (4 ints a piece); returns how many pieces.
 int ACR_ClearedPieces(notnull array<int> pieces)
 {
  pieces.Copy(m_aACRClearedPieces);
  return pieces.Count() / 4;
 }

 protected void ACR_OnCleared()
 {
  ACR_FieldOfFire.MirrorPieces(GetOwner(), m_aACRClearedPieces);
 }

 override void OnPostInit(IEntity owner)
 {
  super.OnPostInit(owner);
  // A client that streams the structure in: again once its pieces are built, and once more later.
  if (GetGame().InPlayMode() && !Replication.IsServer())
  {
   GetGame().GetCallqueue().CallLater(ACR_OnCleared, 1500);
   GetGame().GetCallqueue().CallLater(ACR_OnCleared, 4000);
  }
 }

 override void OnDelete(IEntity owner)
 {
  // Both later calls (Remove takes one at a time).
  GetGame().GetCallqueue().Remove(ACR_OnCleared);
  GetGame().GetCallqueue().Remove(ACR_OnCleared);
  super.OnDelete(owner);
 }
}

// The map bushes cleared this mission (ACR_FieldOfFire), for every machine and every player who joins.
modded class SCR_BaseGameMode
{
 [RplProp(onRplName: "ACR_OnClearedSpots")]
 protected ref array<int> m_aACRClearedSpots = {};

 void ACR_AddClearedSpots(notnull array<int> spots)
 {
  if (m_aACRClearedSpots.Count() + spots.Count() > ACR_FieldOfFire.MAX_SPOTS)
  {
   PrintFormat("[ACR] FIELD OF FIRE: the record is full (%1 bushes); %2 more cleared on the server only", m_aACRClearedSpots.Count() / 2, spots.Count() / 2);
   return;
  }
  m_aACRClearedSpots.InsertAll(spots);
  Replication.BumpMe();
 }

 protected void ACR_OnClearedSpots()
 {
  ACR_FieldOfFire.MirrorSpots(m_aACRClearedSpots);
 }
}
