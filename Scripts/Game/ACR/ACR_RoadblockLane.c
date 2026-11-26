// A friendly lane through every roadblock (pn12d playtest, 27 Oct: the AI US side's medium
// checkpoint on its own supply road jammed its M923, its truck and its Humvee for two minutes,
// "stuck, backing up" until they gave up, and most likely ran over the checkpoint's own men).
//
// The vanilla checkpoints carry a road-network bridge for drivers: in the composition's frame (X
// across the road, Z along it) it runs on the +X half for +Z and the -X half for -Z, jogging 4 m
// across the middle. Their dragon's teeth, wire and signs leave about 3 m gaps on that line, too
// tight for a truck or an APC, and the jog has no room to turn. After the composition spawns, every
// solid piece standing in the lane is slid sideways (across the road, never along it) until it
// clears: the lane follows the bridge's line, LANE_OFFSET either side of the centre line with a
// 6 m diagonal between the halves (2 * LANE_TURN), LANE_HALF_WIDTH each side of its line (4.8 m: an
// M923, Ural, LAV-25 or BTR-70 with room to steer). It stays a chicane: the halves are 4 m apart,
// so a 2.5 m vehicle cannot drive it straight (a 1.4 m straight gap) and has to weave past the gun,
// which sits beside the lane in its sandbags (every checkpoint has it on the -X side, 6-7.5 m
// from the lane's line). A plain barricade is a line across the road: its two halves are
// staggered first (the -X half BARRICADE_STAGGER forward, the +X half back), so it becomes the
// same chicane. Decals stay where they are. The holders post beside the lane (Post), and our own
// AI drivers take it slowly (ACR_Squad.LaneSpeed).
class ACR_RoadblockLane
{
 static const float LANE_HALF_WIDTH = 2.4;
 static const float LANE_OFFSET = 2.0;       // m off the road's centre line, each half (the bridge's)
 static const float LANE_TURN = 3.0;         // the diagonal between the halves runs over Z -3..3
 static const float LANE_MARGIN = 0.3;       // a moved piece's edge this far clear of the lane
 static const float BARRICADE_STAGGER = 3.0; // each half of a plain barricade, along the road
 static const float POST_RADIUS = 5;         // the holders' Defend round their post (the gun inside it)
 static const float POST_GAP = 8;           // the post: this far past the lane's edge, so the Defend stays 3 m off it
 static bool s_bOff;                         // tests: the vanilla layout, for the before/after
 // Our vehicle coming through (ACR_Squad.LaneSpeed): holders on foot in the lane or within
 // STEP_CLEAR of its edge, within STEP_ALONG of the roadblock along the road, step out to STEP_CLEAR
 // past the edge on their own side (an auto-playtest, 27 Oct: a man the Defend had left at the
 // lane's edge was brushed by our passing truck).
 static const float STEP_CLEAR = 2.0;
 static const float STEP_ALONG = 15;
 static const float STEP_PRIORITY = 91;      // over attacking (90); danger and medical still outrank it
 static ref map<IEntity, ref SCR_AIMoveIndividuallyBehavior> s_mSteps;
 // Reforger 1.9's own dodge (SCR_AIMoveFromIncomingVehicleBehavior, danger priority, far over
 // STEP_PRIORITY) sprints a man on foot to the nearest cover within 9 m of any vehicle with a driver
 // coming at him. Beside our lane that cover is the lane's own sandbags and barriers: it put the man
 // ClearFor had just stepped out back in front of our truck, 0.1-1.2 m off its hull (srv:rblane, 6 Nov).
 // For the holders ClearFor is looking after, our vehicle in the lane is its business, not the dodge's:
 // each holder within STEP_ALONG, the vehicle it saw and when (world time, ms).
 static const float GUARD_MS = 2500;         // ClearFor runs about once a second while the vehicle comes
 static ref map<IEntity, IEntity> s_mGuardFor;
 static ref map<IEntity, float> s_mGuardAt;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_mSteps = new map<IEntity, ref SCR_AIMoveIndividuallyBehavior>();
  s_mGuardFor = new map<IEntity, IEntity>();
  s_mGuardAt = new map<IEntity, float>();
 }
 static int s_iStepsForTest;                 // step-clear moves ordered

 // The lane's centre line across the road (local X) at local Z.
 static float LaneX(float z)
 {
  if (z >= LANE_TURN) return LANE_OFFSET;
  if (z <= -LANE_TURN) return -LANE_OFFSET;
  return LANE_OFFSET * z / LANE_TURN;
 }

 // How far a footprint (local X x0..x1, Z z0..z1) is clear of the lane (negative: inside it by that).
 static float Clearance(float x0, float x1, float z0, float z1)
 {
  float lo = LaneX(z0) - LANE_HALF_WIDTH;
  float hi = LaneX(z1) + LANE_HALF_WIDTH;
  return Math.Max(lo - x1, x0 - hi);
 }

 static bool IsDecal(IEntity e)
 {
  string name = PrefabName(e);
  return name.Contains("Decal");
 }

 static string PrefabName(IEntity e)
 {
  if (!e) return "";
  EntityPrefabData data = e.GetPrefabData();
  if (!data) return "";
  // A composition's parts are instances in its prefab: the first ancestor that is a prefab file.
  string name = data.GetPrefabName();
  if (name.IsEmpty()) name = SCR_BaseContainerTools.GetPrefabResourceName(data.GetPrefab());
  return name;
 }

 // The pieces moved as one: the composition's children, but a log barricade nested in a
 // composition (the USSR medium barricade's) piece by piece.
 static void Units(IEntity root, notnull array<IEntity> units)
 {
  IEntity child = root.GetChildren();
  while (child)
  {
   if (PrefabName(child).Contains("Barricade_Logs") && child.GetChildren()) Units(child, units);
   else units.Insert(child);
   child = child.GetSibling();
  }
 }

 // A piece's footprint in the composition's frame (root): its own and its children's model
 // bounds; a piece with no model (a patrol point) is 0.8 m round its origin.
 static void Footprint(IEntity unit, vector root[4], out float x0, out float x1, out float z0, out float z1)
 {
  x0 = float.MAX;
  x1 = -float.MAX;
  z0 = float.MAX;
  z1 = -float.MAX;
  Grow(unit, root, x0, x1, z0, z1);
  if (x0 <= x1) return;
  vector at = unit.GetOrigin() - root[3];
  float x = vector.Dot(at, root[0]);
  float z = vector.Dot(at, root[2]);
  x0 = x - 0.4;
  x1 = x + 0.4;
  z0 = z - 0.4;
  z1 = z + 0.4;
 }

 protected static void Grow(IEntity e, vector root[4], inout float x0, inout float x1, inout float z0, inout float z1)
 {
  vector mins, maxs;
  e.GetBounds(mins, maxs);
  if (maxs[0] - mins[0] > 0.05 || maxs[2] - mins[2] > 0.05)
  {
   vector t[4];
   e.GetWorldTransform(t);
   for (int k = 0; k < 4; k++)
   {
    float cx = mins[0];
    if (k % 2 == 1) cx = maxs[0];
    float cz = mins[2];
    if (k >= 2) cz = maxs[2];
    vector corner = t[3] + t[0] * cx + t[2] * cz - root[3];
    float x = vector.Dot(corner, root[0]);
    float z = vector.Dot(corner, root[2]);
    x0 = Math.Min(x0, x);
    x1 = Math.Max(x1, x);
    z0 = Math.Min(z0, z);
    z1 = Math.Max(z1, z);
   }
  }
  IEntity child = e.GetChildren();
  while (child)
  {
   Grow(child, root, x0, x1, z0, z1);
   child = child.GetSibling();
  }
 }

 // Playtest 29 Oct (pn12h, both sides' checkpoints): the lane left the composition's camouflage
 // cuttings and its stop sign standing in the middle of the road (slid just clear of the lane, between
 // its halves), and slid the gun nest out of the lane without the loose end sandbag, ammo and chair
 // beside it, so the end sandbag stuck out in front of the nest. Now a cutting or a sign in the lane,
 // or anywhere on the road (inside ROAD_ASPHALT), goes out to the verge (ROAD_EDGE past the centre
 // line, and past the lane), and the nest's loose
 // pieces (within NEST_NEAR of it, on its side of the road) move with it.
 static const float ROAD_EDGE = 4.0;  // m from the centre line: the verge (the compositions' edge pieces stand at 4.3-4.5)
 static const float ROAD_ASPHALT = 3.5; // m from the centre line: a cutting or sign reaching inside this stands on the road
 static const float NEST_NEAR = 2.5;  // m: loose pieces this close to the gun nest move with it
 static int s_iDecorMoved;            // tests: cuttings and signs sent to the verge (last Open)
 static float s_fDecorInnermost;      // tests: the innermost edge of those, m from the centre line
 static int s_iNestFollowers;         // tests: loose pieces moved with the nest (last Open)

 // Dressing that has no business standing in the road: the cuttings and the checkpoint sign.
 static bool IsDecor(IEntity e)
 {
  string name = PrefabName(e);
  return name.Contains("Prefabs/Vegetation/") || name.Contains("SignCheckpoint");
 }

 static bool IsNest(IEntity e)
 {
  return PrefabName(e).Contains("Sandbag_MG_");
 }

 // Tests: the nearest a cutting or sign stands to the road's centre line (0: across it), and which.
 static float DecorInnermost(IEntity ent, out string what)
 {
  what = "none";
  float best = float.MAX;
  if (!ent) return best;
  vector root[4];
  ent.GetWorldTransform(root);
  array<IEntity> units = {};
  Units(ent, units);
  foreach (IEntity unit : units)
  {
   if (!unit || !IsDecor(unit)) continue;
   float x0, x1, z0, z1;
   Footprint(unit, root, x0, x1, z0, z1);
   float d = Math.Min(Math.AbsFloat(x0), Math.AbsFloat(x1));
   if (x0 < 0 && x1 > 0) d = 0;
   if (d >= best) continue;
   best = d;
   what = FilePath.StripPath(PrefabName(unit));
  }
  return best;
 }

 // Tests: where the first piece matching 'piece' stands from the gun nest, in the composition's frame.
 static vector FromNest(IEntity ent, string piece)
 {
  if (!ent) return vector.Zero;
  IEntity nest, other;
  array<IEntity> units = {};
  Units(ent, units);
  foreach (IEntity unit : units)
  {
   if (!nest && IsNest(unit)) nest = unit;
   if (!other && PrefabName(unit).Contains(piece)) other = unit;
  }
  if (!nest || !other) return vector.Zero;
  vector root[4];
  ent.GetWorldTransform(root);
  vector d = other.GetOrigin() - nest.GetOrigin();
  return Vector(vector.Dot(d, root[0]), 0, vector.Dot(d, root[2]));
 }

 // Opens the lane through a roadblock just spawned (before its navmesh rebuild); returns how many
 // pieces moved.
 static int Open(IEntity ent, bool barricade)
 {
  s_iDecorMoved = 0;
  s_fDecorInnermost = float.MAX;
  s_iNestFollowers = 0;
  if (!ent || s_bOff) return 0;
  vector root[4];
  ent.GetWorldTransform(root);
  array<IEntity> units = {};
  Units(ent, units);
  // Each piece's footprint and move first, then the nest's loose pieces, then the moves.
  int count = units.Count();
  array<float> fx0 = {}, fx1 = {}, fz0 = {}, fz1 = {}, dxs = {}, dzs = {};
  int nest = -1;
  for (int i = 0; i < count; i++)
  {
   IEntity unit = units[i];
   float x0, x1, z0, z1;
   if (unit) Footprint(unit, root, x0, x1, z0, z1);
   fx0.Insert(x0); fx1.Insert(x1); fz0.Insert(z0); fz1.Insert(z1);
   float dz = 0;
   float dx = 0;
   if (unit && !IsDecal(unit))
   {
    if (barricade)
    {
     dz = -BARRICADE_STAGGER;
     if (x0 + x1 < 0) dz = BARRICADE_STAGGER;
    }
    bool inLane = Clearance(x0, x1, z0 + dz, z1 + dz) < LANE_MARGIN;
    if (IsDecor(unit) && (inLane || (x1 > -ROAD_ASPHALT && x0 < ROAD_ASPHALT)))
    {
     // In the lane or anywhere on the road: out to the verge on the side it is on (from the
     // lane's line), past the lane too.
     float cz = (z0 + z1) / 2 + dz;
     if ((x0 + x1) / 2 < LaneX(cz)) dx = Math.Min(-ROAD_EDGE, LaneX(z0 + dz) - LANE_HALF_WIDTH - LANE_MARGIN) - x1;
     else dx = Math.Max(ROAD_EDGE, LaneX(z1 + dz) + LANE_HALF_WIDTH + LANE_MARGIN) - x0;
     s_iDecorMoved++;
     s_fDecorInnermost = Math.Min(s_fDecorInnermost, Math.Min(Math.AbsFloat(x0 + dx), Math.AbsFloat(x1 + dx)));
    }
    else if (inLane)
    {
     float left = LaneX(z0 + dz) - LANE_HALF_WIDTH - LANE_MARGIN - x1;   // < 0
     float right = LaneX(z1 + dz) + LANE_HALF_WIDTH + LANE_MARGIN - x0;  // > 0
     dx = right;
     if (-left < right) dx = left;
    }
    if (nest < 0 && IsNest(unit)) nest = i;
   }
   dxs.Insert(dx);
   dzs.Insert(dz);
  }
  // The nest's loose pieces (decals included: its dirt patch) go where it goes.
  if (nest >= 0 && dxs[nest] != 0)
  {
   float side = fx0[nest] + fx1[nest];
   for (int k = 0; k < count; k++)
   {
    if (k == nest || !units[k] || dxs[k] != 0 || dzs[k] != 0) continue;
    if ((fx0[k] + fx1[k]) * side <= 0) continue; // the other side of the road
    float gapX = Math.Max(0, Math.Max(fx0[nest] - fx1[k], fx0[k] - fx1[nest]));
    float gapZ = Math.Max(0, Math.Max(fz0[nest] - fz1[k], fz0[k] - fz1[nest]));
    if (gapX > NEST_NEAR || gapZ > NEST_NEAR) continue;
    dxs[k] = dxs[nest];
    s_iNestFollowers++;
   }
  }
  int moved = 0;
  for (int m = 0; m < count; m++)
  {
   if (!units[m] || (dxs[m] == 0 && dzs[m] == 0)) continue;
   Shift(units[m], root[0] * dxs[m] + root[2] * dzs[m]);
   moved++;
  }
  ent.Update();
  return moved;
 }

 // Moves a piece and everything under it by 'offset', kept at its height above the ground.
 protected static void Shift(IEntity unit, vector offset)
 {
  BaseWorld world = GetGame().GetWorld();
  vector from = unit.GetOrigin();
  vector to = from + offset;
  offset[1] = world.GetSurfaceY(to[0], to[2]) - world.GetSurfaceY(from[0], from[2]);
  array<IEntity> parts = {};
  Collect(unit, parts);
  array<vector> origins = {};
  foreach (IEntity part : parts) origins.Insert(part.GetOrigin());
  foreach (int i, IEntity part : parts)
  {
   vector t[4];
   part.GetWorldTransform(t);
   t[3] = origins[i] + offset;
   part.SetWorldTransform(t);
   // (As the game editor moves a piece: its physics goes with it.)
   GenericEntity generic = GenericEntity.Cast(part);
   if (generic) generic.OnTransformReset();
   part.Update();
  }
 }

 protected static void Collect(IEntity e, notnull array<IEntity> parts)
 {
  parts.Insert(e);
  IEntity child = e.GetChildren();
  while (child)
  {
   Collect(child, parts);
   child = child.GetSibling();
  }
 }

 // The worst clearance of any solid piece from the lane (tests; negative: a piece in it).
 static float WorstClearance(IEntity ent, out string worst)
 {
  worst = "";
  if (!ent) return 0;
  vector root[4];
  ent.GetWorldTransform(root);
  array<IEntity> units = {};
  Units(ent, units);
  float least = float.MAX;
  foreach (IEntity unit : units)
  {
   if (!unit || IsDecal(unit)) continue;
   float x0, x1, z0, z1;
   Footprint(unit, root, x0, x1, z0, z1);
   float c = Clearance(x0, x1, z0, z1);
   if (c >= least) continue;
   least = c;
   worst = FilePath.StripPath(PrefabName(unit));
  }
  return least;
 }

 // The roadblock's men on foot: the squads holding it and its defenders.
 static void Holders(ACR_Roadblock rb, notnull array<IEntity> into)
 {
  into.Clear();
  array<AIAgent> agents = {};
  array<AIAgent> some = {};
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (!squad || squad.m_ActiveRoadblock != rb) continue;
    squad.GetAgents(some);
    agents.InsertAll(some);
   }
  }
  if (rb.m_Defenders)
  {
   if (rb.m_Defenders.m_Group && rb.m_Defenders.m_Group.GetAgents(some) > 0) agents.InsertAll(some);
   if (rb.m_Defenders.m_GunCrew && rb.m_Defenders.m_GunCrew.GetAgents(some) > 0) agents.InsertAll(some);
  }
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Respawn.IsAlive(body) && !ACR_Transport.VehicleOf(body) && !EntityUtils.IsPlayer(body) && !into.Contains(body)) into.Insert(body);
  }
 }

 // One of our vehicles is within ACR_Roadblocks.LANE_SLOW_RANGE (ACR_Squad.LaneSpeed): every
 // holder on foot in its way steps clear, to the nearer side of the lane (never across it in front
 // of the vehicle). Nothing for one standing still, or past and pulling away. Returns how many were sent.
 static int ClearFor(ACR_Roadblock rb, IEntity vehicle, bool aiDriven = false)
 {
  if (!rb || !rb.m_Entity || !vehicle || s_bOff) return 0;
  vector root[4];
  rb.m_Entity.GetWorldTransform(root);
  vector at = Local(rb.m_Entity, vehicle.GetOrigin());
  // A vehicle standing still (parked by the road) is in nobody's way.
  Physics phys = vehicle.GetPhysics();
  if (!phys || phys.GetVelocity().Length() < 1) return 0;
  if (Math.AbsFloat(at[2]) > 10 && at[2] * vector.Dot(phys.GetVelocity(), root[2]) > 0) return 0;
  // (Steps done, or of men gone, are forgotten.)
  for (int k = s_mSteps.Count() - 1; k >= 0; k--)
  {
   SCR_AIMoveIndividuallyBehavior old = s_mSteps.GetElement(k);
   if (!s_mSteps.GetKey(k) || !old || old.GetActionState() == EAIActionState.COMPLETED || old.GetActionState() == EAIActionState.FAILED) s_mSteps.RemoveElement(k);
  }
  array<IEntity> men = {};
  Holders(rb, men);
  float now = GetGame().GetWorld().GetWorldTime();
  for (int g = s_mGuardAt.Count() - 1; g >= 0; g--)
  {
   if (!s_mGuardAt.GetKey(g) || now - s_mGuardAt.GetElement(g) > GUARD_MS)
   {
    s_mGuardFor.Remove(s_mGuardAt.GetKey(g));
    s_mGuardAt.RemoveElement(g);
   }
  }
  int sent = 0;
  foreach (IEntity man : men)
  {
   vector l = Local(rb.m_Entity, man.GetOrigin());
   if (Math.AbsFloat(l[2]) > STEP_ALONG) continue;
   SCR_AIUtilityComponent utility = ACR_EntityUtil.AIUtility(man);
   if (!utility) continue;
   // Every holder near the lane, for an AI driver (it keeps to the lane, slowed): the game's dodge
   // leaves this vehicle to us (Guarded), and one it had already started is called off. A player
   // can drive anywhere, so the dodge stays for his vehicle.
   if (aiDriven)
   {
    s_mGuardFor.Set(man, vehicle);
    s_mGuardAt.Set(man, now);
    SCR_AIMoveFromIncomingVehicleBehavior dodge = SCR_AIMoveFromIncomingVehicleBehavior.Cast(utility.FindActionOfType(SCR_AIMoveFromIncomingVehicleBehavior));
    if (dodge && dodge.m_DangerEntity.m_Value == vehicle && dodge.GetActionState() != EAIActionState.COMPLETED && dodge.GetActionState() != EAIActionState.FAILED) dodge.Fail();
   }
   if (Clearance(l[0] - 0.3, l[0] + 0.3, l[2], l[2]) >= STEP_CLEAR) continue;
   // A step he is carrying out stands. One that something else has put aside while he is still
   // in the way (his Defend walking him back across the lane to a post on the far side, srv:rblane
   // 27 Oct) is replaced, for as long as the vehicle comes on.
   SCR_AIMoveIndividuallyBehavior was = s_mSteps.Get(man);
   if (was && utility.GetCurrentAction() == was) continue;
   if (was && utility.GetCurrentAction()) PrintFormat("[ACR] ROADBLOCK lane: a holder's step was put aside by %1", utility.GetCurrentAction().ClassName());
   if (was && was.GetActionState() != EAIActionState.COMPLETED && was.GetActionState() != EAIActionState.FAILED) was.Fail();
   // The nearer side: the shortest walk out. (Always the post's side was tried: from the middle of
   // the lane that walk was longer and its end sat among the gun nest's sandbags, and men were still
   // in the lane when the truck came, srv:rblane 27 Oct.)
   float side = 1;
   if (l[0] < LaneX(l[2])) side = -1;
   vector to = World(rb.m_Entity, LaneX(l[2]) + side * (LANE_HALF_WIDTH + STEP_CLEAR + 0.5), l[2]);
   SCR_AIMoveIndividuallyBehavior step = new SCR_AIMoveIndividuallyBehavior(utility, null, to, priority: STEP_PRIORITY, radius: 1.0);
   utility.AddAction(step);
   s_mSteps.Set(man, step);
   sent++;
  }
  if (sent > 0)
  {
   s_iStepsForTest += sent;
   PrintFormat("[ACR] ROADBLOCK lane at %1: %2 holder(s) step clear for %3 (%4 m off along the road)", rb.m_vPos, sent, FilePath.StripPath(PrefabName(vehicle)), Math.Round(at[2]));
  }
  return sent;
 }

 // True while ClearFor looks after this man for this vehicle (it saw them within GUARD_MS).
 static bool Guarded(IEntity man, IEntity vehicle)
 {
  if (!man || !vehicle || !s_mGuardFor || s_mGuardFor.Get(man) != vehicle) return false;
  return GetGame().GetWorld().GetWorldTime() - s_mGuardAt.Get(man) <= GUARD_MS;
 }

 // A point in the composition's frame, in the world.
 static vector World(IEntity ent, float x, float z)
 {
  vector root[4];
  ent.GetWorldTransform(root);
  return ACR_Manager.Ground(root[3] + root[0] * x + root[2] * z);
 }

 // A world point in the composition's frame (x across, z along; y unused).
 static vector Local(IEntity ent, vector pos)
 {
  vector root[4];
  ent.GetWorldTransform(root);
  vector d = pos - root[3];
  return Vector(vector.Dot(d, root[0]), 0, vector.Dot(d, root[2]));
 }

 // Where the roadblock's holders and defenders post: beside the lane on the gun's side, level with
 // the gun (no gun: the -X side, level with the middle), POST_GAP past the lane's edge there.
 static vector Post(IEntity ent, IEntity turret)
 {
  if (!ent) return vector.Zero;
  float side = -1;
  float z = 0;
  if (turret)
  {
   vector gun = Local(ent, turret.GetOrigin());
   if (gun[0] > LaneX(gun[2])) side = 1;
   z = gun[2];
  }
  float x = LaneX(z) + side * (LANE_HALF_WIDTH + POST_GAP);
  return World(ent, x, z);
 }
}

// The game's dodge from a vehicle coming at a man on foot stands aside for one of our vehicles in our
// roadblock's lane while ACR_RoadblockLane.ClearFor looks after him (see GUARD_MS there). Every other
// vehicle, and every man not by the lane, keeps the game's reaction.
[BaseContainerProps()]
modded class SCR_AIDangerReaction_Vehicle
{
 override bool PerformReaction(notnull SCR_AIUtilityComponent utility, notnull SCR_AIThreatSystem threatSystem, AIDangerEvent dangerEvent, int dangerEventCount)
 {
  if (dangerEvent && !utility.m_AIInfo.HasUnitState(EUnitState.IN_VEHICLE) && ACR_RoadblockLane.Guarded(utility.m_OwnerEntity, dangerEvent.GetVictim())) return false;
  return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);
 }
}
