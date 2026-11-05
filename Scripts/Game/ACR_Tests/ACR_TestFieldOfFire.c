// -acrFieldsOfFireTest 1 (server-capable, Arland, under a minute): ACR_FieldOfFire. Finds a clump of
// bushes, then: all round a spot, the bushes within ROUND_M go and those past it in other
// directions stay; a gun's cone takes the bushes in front of it out to GUN_FRONT_M and leaves the ones
// behind; trees over MAX_HEIGHT_M are never touched; another machine clears the same map bushes from
// their spots, and only once. A medium checkpoint built there clears round its post, its own cut
// branches included (hidden, colliders gone, recorded by their place in its prefab), and the record
// hides the same cut branches on another copy of the checkpoint, built from the prefab as a client does. Off by default: the test turns it on.
class ACR_TestFieldOfFire
{
 static ref ACR_TestFieldOfFire s_Test;
 protected vector m_vSpot;
 protected ref array<IEntity> m_aNear = {};
 protected ref array<IEntity> m_aFar = {};
 protected ref array<IEntity> m_aFront = {};
 protected ref array<IEntity> m_aBehind = {};
 protected vector m_vFacing;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (!s_Test) s_Test = new ACR_TestFieldOfFire();
  s_Test.Step(tick);
 }

 protected array<IEntity> Bushes(vector at, float radius)
 {
  ACR_FieldOfFireQuery query = new ACR_FieldOfFireQuery();
  GetGame().GetWorld().QueryEntitiesBySphere(at, radius, query.Add, null, EQueryEntitiesFlags.ALL);
  array<IEntity> alive = {};
  foreach (IEntity bush : query.m_aFound) if (bush) alive.Insert(bush);
  return alive;
 }

 protected bool FindClump()
 {
  vector mins, maxs;
  GetGame().GetWorld().GetBoundBox(mins, maxs);
  for (float x = mins[0] + 400; x < maxs[0] - 400; x += 60)
  {
   for (float z = mins[2] + 400; z < maxs[2] - 400; z += 60)
   {
    vector at = ACR_Manager.Ground(Vector(x, 0, z));
    if (BBB_Terrain.IsWaterAt(at)) continue;
    array<IEntity> near = Bushes(at, ACR_FieldOfFire.ROUND_M - 0.5);
    if (near.Count() < 3) continue;
    array<IEntity> ring = Bushes(at, 20);
    if (ring.Count() < near.Count() + 4) continue;
    m_vSpot = at;
    return true;
   }
  }
  return false;
 }

 protected int Alive(array<IEntity> bushes)
 {
  int alive;
  foreach (IEntity bush : bushes) if (bush) alive++;
  return alive;
 }

 protected static int VisibleCuttings(array<IEntity> parts)
 {
  int n;
  foreach (IEntity part : parts) if (IsCutting(part) && (part.GetFlags() & EntityFlags.VISIBLE)) n++;
  return n;
 }

 protected static bool IsCutting(IEntity e)
 {
  if (!e || !e.GetPrefabData()) return false;
  BaseContainer prefab = e.GetPrefabData().GetPrefab();
  return prefab && SCR_BaseContainerTools.IsKindOf(prefab, ACR_FieldOfFire.CUTTINGS);
 }

 protected void Step(int tick)
 {
  if (tick == 3)
  {
   ACR_FieldOfFire.s_bTestOn = true;
   if (!FindClump())
   {
    ACR_TestRegress.Check("field of fire: a clump of bushes found", false);
    Print("[ACR] TEST COMPLETE");
    return;
   }
   // Sort the bushes round the spot: within ROUND_M, and 9-20 m out (in front of / behind a facing
   // towards the first of those, and outside its cone).
   array<IEntity> ring = Bushes(m_vSpot, 20);
   foreach (IEntity bush : ring)
   {
    float d = vector.DistanceXZ(bush.GetOrigin(), m_vSpot);
    if (d <= ACR_FieldOfFire.ROUND_M - 0.5) m_aNear.Insert(bush);
    else if (d >= ACR_FieldOfFire.ROUND_M + 2) m_aFar.Insert(bush);
   }
   PrintFormat("[ACR] FOFTEST clump at %1: %2 bushes within %3 m, %4 farther out", m_vSpot, m_aNear.Count(), ACR_FieldOfFire.ROUND_M, m_aFar.Count());
   int cleared = ACR_FieldOfFire.Round(m_vSpot, ACR_FieldOfFire.ROUND_M, "test spot");
   ACR_TestRegress.Check("field of fire: every bush within the radius cleared", Alive(m_aNear) == 0 && cleared >= m_aNear.Count());
   ACR_TestRegress.Check("field of fire: the bushes farther out still stand", Alive(m_aFar) == m_aFar.Count());
   if (m_aFar.IsEmpty()) return;
   m_vFacing = m_aFar[0].GetOrigin() - m_vSpot;
   m_vFacing[1] = 0;
   m_vFacing.Normalize();
   float cosHalf = Math.Cos(ACR_FieldOfFire.GUN_HALF_DEG * Math.DEG2RAD);
   foreach (IEntity outer : m_aFar)
   {
    vector to = outer.GetOrigin() - m_vSpot;
    to[1] = 0;
    to.Normalize();
    float dot = vector.Dot(to, m_vFacing);
    if (dot >= cosHalf + 0.03) m_aFront.Insert(outer);
    else if (dot <= cosHalf - 0.03) m_aBehind.Insert(outer);
   }
   ACR_FieldOfFire.Cone(m_vSpot, m_vFacing, "test gun");
   PrintFormat("[ACR] FOFTEST cone: %1 in front, %2 outside it", m_aFront.Count(), m_aBehind.Count());
   ACR_TestRegress.Check("field of fire: a gun's cone takes the bushes in front of it", Alive(m_aFront) == 0 && m_aFront.Count() > 0);
   ACR_TestRegress.Check("field of fire: the bushes outside the cone still stand", Alive(m_aBehind) == m_aBehind.Count());
   // Another machine, from the spots the server records: the bushes outside the cone stand in for
   // ones the server cleared.
   array<int> spots = {};
   foreach (IEntity left : m_aBehind)
   {
    vector at = left.GetOrigin();
    spots.Insert((int)Math.Round(at[0] / ACR_FieldOfFire.SPOT_STEP));
    spots.Insert((int)Math.Round(at[2] / ACR_FieldOfFire.SPOT_STEP));
   }
   int mirrored = ACR_FieldOfFire.MirrorSpots(spots, true);
   int again = ACR_FieldOfFire.MirrorSpots(spots, true);
   PrintFormat("[ACR] FOFTEST spots: %1 recorded, %2 cleared from them, %3 the second time", m_aBehind.Count(), mirrored, again);
   ACR_TestRegress.Check("field of fire: another machine clears the same map bushes from their spots", m_aBehind.Count() > 0 && mirrored == m_aBehind.Count() && Alive(m_aBehind) == 0);
   ACR_TestRegress.Check("field of fire: a record read twice clears nothing more", again == 0);
  }
  if (tick == 6)
  {
   // A medium checkpoint on the nearest road clears round its post (ACR_Roadblocks.SpawnRoadblock),
   // its own cut branches included.
   vector road;
   int before = ACR_FieldOfFire.s_iCleared;
   bool onRoad = BBB_Terrain.RoadPointNear(m_vSpot, 400, road);
   ACR_Roadblock rb;
   if (onRoad) rb = ACR_Roadblocks.SpawnRoadblock("US", road, 0, "Field of fire test", ACR_ERoadblockType.MEDIUM);
   int left = -1;
   if (rb) left = Bushes(rb.m_vPost, ACR_FieldOfFire.POST_M - 0.5).Count();
   PrintFormat("[ACR] FOFTEST roadblock at %1: %2 bushes knocked down, %3 left within %4 m of its post", road, ACR_FieldOfFire.s_iCleared - before, left, ACR_FieldOfFire.POST_M);
   ACR_TestRegress.Check("field of fire: no bush left round a new roadblock's post", rb && left == 0);
   if (!rb)
   {
    Print("[ACR] TEST COMPLETE");
    return;
   }
   array<IEntity> parts = {};
   ACR_StructureLayout.Parts(rb.m_Entity, parts);
   int cuttings, hidden, solid;
   foreach (IEntity part : parts)
   {
    if (!IsCutting(part)) continue;
    cuttings++;
    if (part.GetFlags() & EntityFlags.VISIBLE) continue;
    hidden++;
    if (part.GetPhysics()) solid++;
   }
   array<int> record = {};
   SCR_CampaignBuildingCompositionComponent comp = ACR_StructureLayout.Comp(rb.m_Entity);
   int recorded;
   if (comp) recorded = comp.ACR_ClearedPieces(record);
   PrintFormat("[ACR] FOFTEST checkpoint %1: %2 cut branches, %3 hidden (%4 still solid), %5 in its record", FilePath.StripPath(rb.m_Entity.GetPrefabData().GetPrefabName()), cuttings, hidden, solid, recorded);
   ACR_TestRegress.Check("field of fire: a checkpoint's own cut branches are cleared", cuttings > 0 && hidden > 0 && solid == 0);
   ACR_TestRegress.Check("field of fire: the checkpoint records each one it cleared", recorded == hidden);
   // Another copy of the same checkpoint where it stands, built from the prefab as a client builds it
   // (and left as built, as a client does when the layout record doesn't fit): the record hides the
   // same cut branches there, and nothing more when read again.
   EntitySpawnParams spawnParams = new EntitySpawnParams();
   spawnParams.TransformMode = ETransformMode.WORLD;
   rb.m_Entity.GetWorldTransform(spawnParams.Transform);
   IEntity copy = GetGame().SpawnEntityPrefab(Resource.Load(rb.m_Entity.GetPrefabData().GetPrefabName()), GetGame().GetWorld(), spawnParams);
   array<IEntity> copyParts = {};
   if (copy) ACR_StructureLayout.Parts(copy, copyParts);
   int standing = VisibleCuttings(copyParts);
   int mirrored = ACR_FieldOfFire.MirrorPieces(copy, record, true);
   int again = ACR_FieldOfFire.MirrorPieces(copy, record, true);
   int remaining = VisibleCuttings(copyParts);
   int wrong;
   // Each cut branch hidden on the original has its twin (the same place in the prefab) hidden on the copy.
   foreach (IEntity original : parts)
   {
    if (!IsCutting(original) || (original.GetFlags() & EntityFlags.VISIBLE)) continue;
    int ox, oy, oz, oh;
    ACR_FieldOfFire.PieceKey(original, ox, oy, oz, oh);
    bool match;
    foreach (IEntity part : copyParts)
    {
     int cx, cy, cz, ch;
     if (IsCutting(part) && !(part.GetFlags() & EntityFlags.VISIBLE) && ACR_FieldOfFire.PieceKey(part, cx, cy, cz, ch) && cx == ox && cy == oy && cz == oz && ch == oh) match = true;
    }
    if (!match) wrong++;
   }
   PrintFormat("[ACR] FOFTEST copy: %1 pieces (the checkpoint %2), %3 hidden from the record (%4 the second time), cut branches standing %5 before and %6 after, %7 not matched", copyParts.Count(), parts.Count(), mirrored, again, standing, remaining, wrong);
   ACR_TestRegress.Check("field of fire: the record hides the same cut branches on another copy", copy && mirrored == recorded && again == 0 && standing - remaining == recorded && wrong == 0);
   if (copy) SCR_EntityHelper.DeleteEntityAndChildren(copy);
   ACR_Roadblocks.DismantleRoadblock(rb);
   ACR_FieldOfFire.s_bTestOn = false;
   Print("[ACR] TEST COMPLETE");
  }
 }
}
