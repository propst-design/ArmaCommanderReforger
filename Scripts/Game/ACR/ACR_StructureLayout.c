// Our structures' layout on every machine (27 Oct 2026). The ground tilt (ACR_GroundTilt.Place) moves a
// composition on the server after it spawns, and nothing sends those moves to clients: a client builds the
// composition from its prefab, level, so in TvT a remote player saw the pieces where the server's weren't
// (vanilla's Game Master needs its own RPC to move a placed object for clients,
// SCR_EditableEntityComponent.SetTransformBroadcast). So the server records the final layout on the
// composition's replicated component (SCR_CampaignBuildingCompositionComponent.ACR_SetLayout), and every
// machine, the server included, lays the pieces out from that record (Apply): the server when it is set,
// a client when the composition streams in (joining later too) or the record changes.
//
// The record holds every piece: pieces the server didn't move still don't rebuild alike everywhere (a
// barricade's logs settle on the ground as the composition is built: a level copy given only the root
// stayed 13 cm off). Ints, quantized: the root's position in mm and its angles in 0.01 degrees (5 ints),
// the piece count and the record's format (1), then 4 a piece (its key, its position in the root's frame
// in 2 mm steps up to 65 m, its angles in 0.01 degrees): 668 bytes for a 40-piece sandbag position, a
// third of plain floats.
//
// A piece is keyed by its place in the composition's prefab (Key: the coords its entity source and each
// of its parents' give them, and their prefabs), not by its index: a client joined to a dedicated server
// builds a composition with its pieces in another order, and without the pieces only the server has (a
// checkpoint's AI patrol points: 52 pieces on the client, 58 on the server, 4 Nov). With indices it
// refused every such record ("left as built") and players who joined saw a checkpoint's lane-moved and
// tilted pieces where the server's weren't. A piece the record doesn't have keeps its place in the
// root's frame; the record's pieces this machine doesn't have are skipped.
class ACR_StructureLayout
{
 static const float ROOT_STEP = 0.001;  // m
 static const float PART_STEP = 0.002;  // m: 16 bits reach 65 m either way
 static const float ANGLE_STEP = 0.01;  // degrees
 static const int HEADER = 6;
 static const int FORMAT_KEYED = 1;     // the header's low half: pieces keyed by their place in the prefab

 static int s_iApplied;   // pieces laid out from a record on this machine (tests)
 static int s_iMismatch;  // records that didn't fit the composition they arrived on (tests)

 // The server, once 'root' is placed (tilted, a lane opened): records its layout for every machine.
 static bool Record(IEntity root)
 {
  SCR_CampaignBuildingCompositionComponent comp = Comp(root);
  if (!comp || !Replication.IsServer()) return false;
  array<int> layout = {};
  Write(root, layout);
  comp.ACR_SetLayout(layout);
  return true;
 }

 static void Write(IEntity root, notnull array<int> layout)
 {
  vector rootT[4];
  root.GetWorldTransform(rootT);
  vector angles = Math3D.MatrixToAngles(rootT);
  vector origin = rootT[3];
  layout.Insert((int)Math.Round(origin[0] / ROOT_STEP));
  layout.Insert((int)Math.Round(origin[1] / ROOT_STEP));
  layout.Insert((int)Math.Round(origin[2] / ROOT_STEP));
  layout.Insert(Pack(Q(angles[0], ANGLE_STEP), Q(angles[1], ANGLE_STEP)));
  layout.Insert(Pack(Q(angles[2], ANGLE_STEP), 0));
  array<IEntity> parts = {};
  Parts(root, parts);
  array<int> keys = {};
  int unkeyed = Keys(root, parts, keys);
  array<int> moved = {};
  foreach (int i, IEntity part : parts)
  {
   int key = keys[i];
   if (key == 0) continue; // not from the prefab: it keeps its place
   vector local[4];
   Local(rootT, part, local);
   vector a = Math3D.MatrixToAngles(local);
   vector at = local[3];
   moved.Insert(key);
   moved.Insert(Pack(Q(at[0], PART_STEP), Q(at[2], PART_STEP)));
   moved.Insert(Pack(Q(a[0], ANGLE_STEP), Q(a[1], ANGLE_STEP)));
   moved.Insert(Pack(Q(a[2], ANGLE_STEP), Q(at[1], PART_STEP)));
  }
  layout.Insert(Pack(moved.Count() / 4, FORMAT_KEYED));
  layout.InsertAll(moved);
  if (unkeyed > 0) PrintFormat("[ACR] STRUCTURE LAYOUT %1: %2 pieces, %3 without a place in the prefab (kept where they stand)", FilePath.StripPath(root.GetPrefabData().GetPrefabName()), parts.Count(), unkeyed);
 }

 // Lays 'root' and its pieces out from a record; false (nothing moved) when it doesn't fit (none of its
 // pieces is here). Pieces not in the record keep their place in the root's frame as this machine built
 // them (the prefab's).
 static bool Apply(IEntity root, notnull array<int> layout)
 {
  if (!root || layout.Count() < HEADER) return false;
  array<IEntity> parts = {};
  Parts(root, parts);
  int count = High(layout[5]);
  if (Low(layout[5]) != FORMAT_KEYED || layout.Count() != HEADER + count * 4)
  {
   s_iMismatch++;
   PrintFormat("[ACR] STRUCTURE LAYOUT %1: a record of another format or size (%2 ints for %3 pieces); left as built", FilePath.StripPath(root.GetPrefabData().GetPrefabName()), layout.Count(), count);
   return false;
  }
  // Where each of this machine's pieces is in the record (-1: not there).
  map<int, int> byKey = new map<int, int>();
  for (int m = 0; m < count; m++) byKey.Set(layout[HEADER + m * 4], HEADER + m * 4);
  array<int> keys = {};
  Keys(root, parts, keys);
  array<int> found = {};
  int matched;
  foreach (int key : keys)
  {
   int at = -1;
   if (key != 0 && byKey.Find(key, at)) matched++;
   else at = -1;
   found.Insert(at);
  }
  if (count > 0 && matched == 0)
  {
   s_iMismatch++;
   PrintFormat("[ACR] STRUCTURE LAYOUT %1: none of the record's %2 pieces is here; left as built", FilePath.StripPath(root.GetPrefabData().GetPrefabName()), count);
   return false;
  }
  // Every piece's place in the root's frame first, as it stands.
  vector oldRoot[4];
  root.GetWorldTransform(oldRoot);
  array<vector> locals = {};
  foreach (IEntity part : parts)
  {
   vector local[4];
   Local(oldRoot, part, local);
   for (int k = 0; k < 4; k++) locals.Insert(local[k]);
  }
  foreach (int index, int at : found)
  {
   if (at < 0) continue;
   vector angles = Vector(High(layout[at + 2]) * ANGLE_STEP, Low(layout[at + 2]) * ANGLE_STEP, High(layout[at + 3]) * ANGLE_STEP);
   vector rel[4];
   Math3D.AnglesToMatrix(angles, rel);
   rel[3] = Vector(High(layout[at + 1]) * PART_STEP, Low(layout[at + 3]) * PART_STEP, Low(layout[at + 1]) * PART_STEP);
   for (int r = 0; r < 4; r++) locals[index * 4 + r] = rel[r];
  }
  vector rootT[4];
  Math3D.AnglesToMatrix(Vector(High(layout[3]) * ANGLE_STEP, Low(layout[3]) * ANGLE_STEP, High(layout[4]) * ANGLE_STEP), rootT);
  rootT[3] = Vector(layout[0] * ROOT_STEP, layout[1] * ROOT_STEP, layout[2] * ROOT_STEP);
  root.SetWorldTransform(rootT);
  // Parents before their children (hierarchy order), so a moved parent can't drag a set child.
  foreach (int i, IEntity piece : parts)
  {
   vector mine[4];
   for (int q = 0; q < 4; q++) mine[q] = locals[i * 4 + q];
   vector world[4];
   Math3D.MatrixMultiply4(rootT, mine, world);
   piece.SetWorldTransform(world);
   piece.Update();
  }
  root.Update();
  s_iApplied += matched;
  if (matched < count) PrintFormat("[ACR] STRUCTURE LAYOUT %1: %2 of the record's %3 pieces here (the rest only on the server)", FilePath.StripPath(root.GetPrefabData().GetPrefabName()), matched, count);
  return true;
 }

 // Every piece's key (0: none), in Parts order. Pieces of one prefab at one place (a prefab's grouped
 // copies give all theirs the group's coords: 18 of a sandbag position's 40 pieces) are told apart by
 // their order among themselves, which is the prefab's on every machine. Returns how many have none.
 static int Keys(IEntity root, notnull array<IEntity> parts, notnull array<int> keys)
 {
  map<int, int> seen = new map<int, int>();
  int unkeyed;
  foreach (IEntity part : parts)
  {
   int key;
   if (!Key(root, part, key))
   {
    keys.Insert(0);
    unkeyed++;
    continue;
   }
   int n = seen.Get(key);
   seen.Set(key, n + 1);
   if (n > 0) key = string.Format("%1#%2", key, n).Hash();
   if (key == 0) key = 1;
   keys.Insert(key);
  }
  return unkeyed;
 }

 // A piece's place in the composition's prefab, the same on every machine: the coords and angles the
 // entity source of the piece and of each parent below the root give them, and their prefabs, hashed. False for a piece
 // that isn't from the prefab (no entity source, or no coords in it).
 static bool Key(IEntity root, IEntity part, out int key)
 {
  string path;
  IEntity e = part;
  while (e && e != root)
  {
   if (!e.GetPrefabData()) return false;
   BaseContainer source = e.GetPrefabData().GetPrefab();
   if (!source) return false;
   vector coords, angles;
   if (!source.Get("coords", coords)) return false;
   source.Get("angles", angles); // two like pieces in one place, turned differently (stacked bags)
   BaseContainer named = source;
   while (named && named.GetResourceName().IsEmpty()) named = named.GetAncestor();
   string name;
   if (named) name = named.GetResourceName();
   path += string.Format("%1@%2,%3,%4@%5,%6,%7/", name, Math.Round(coords[0] * 1000), Math.Round(coords[1] * 1000), Math.Round(coords[2] * 1000), Math.Round(angles[0] * 100), Math.Round(angles[1] * 100), Math.Round(angles[2] * 100));
   e = e.GetParent();
  }
  if (!e) return false;
  key = path.Hash();
  return true;
 }

 // Every piece under 'root', depth first in hierarchy order: the same on every machine.
 static void Parts(IEntity root, notnull array<IEntity> parts)
 {
  IEntity child = root.GetChildren();
  while (child)
  {
   parts.Insert(child);
   Parts(child, parts);
   child = child.GetSibling();
  }
 }

 // The record's size on the wire: 4 bytes an int, plus the array's length.
 static int Bytes(int ints) { return ints * 4 + 4; }

 static void Local(vector rootT[4], IEntity part, out vector local[4])
 {
  vector world[4];
  part.GetWorldTransform(world);
  Math3D.MatrixInvMultiply4(rootT, world, local);
 }

 static SCR_CampaignBuildingCompositionComponent Comp(IEntity root)
 {
  if (!root) return null;
  return SCR_CampaignBuildingCompositionComponent.Cast(root.FindComponent(SCR_CampaignBuildingCompositionComponent));
 }

 protected static int Q(float value, float step)
 {
  return Math.ClampInt((int)Math.Round(value / step), -32768, 32767);
 }

 protected static int Pack(int high, int low)
 {
  return (high << 16) | (low & 0xFFFF);
 }

 protected static int High(int packed)
 {
  return packed >> 16;
 }

 protected static int Low(int packed)
 {
  int low = packed & 0xFFFF;
  if (low >= 32768) low -= 65536;
  return low;
 }
}

modded class SCR_CampaignBuildingCompositionComponent
{
 // The structure's layout as the server placed it (ACR_StructureLayout); empty: as its prefab.
 [RplProp(onRplName: "ACR_OnLayout")]
 protected ref array<int> m_aACRLayout = {};
 void ACR_SetLayout(notnull array<int> layout)
 {
  m_aACRLayout.Copy(layout);
  ACR_StructureLayout.Apply(GetOwner(), m_aACRLayout);
  Replication.BumpMe();
 }

 int ACR_LayoutSize() { return m_aACRLayout.Count(); }

 protected void ACR_OnLayout()
 {
  if (!m_aACRLayout.IsEmpty()) ACR_StructureLayout.Apply(GetOwner(), m_aACRLayout);
 }

 override void OnPostInit(IEntity owner)
 {
  super.OnPostInit(owner);
  // A client that streams the structure in may get the record before the pieces settle: once more.
  if (GetGame().InPlayMode() && !Replication.IsServer()) GetGame().GetCallqueue().CallLater(ACR_OnLayout, 1000);
 }

 override void OnDelete(IEntity owner)
 {
  GetGame().GetCallqueue().Remove(ACR_OnLayout);
  super.OnDelete(owner);
 }
}
