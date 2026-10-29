// -acrPositionSitingTest 1 (server-capable, ~40 s): the AI's Fortify spots (ACR_PositionSiting)
// round our first front base. The best spot sees down the approach with little dead ground in
// front; no spot lies within DEFENCE_STANDOFF of a (pretend) field of ours; a (pretend) roadblock
// of ours in the best spot's field of fire makes that spot score for overlooking it; the AI digs
// in at one of the best spots; and the score's terms (field of fire, high ground, dead ground, fit,
// overlooking ours). -acrPositionSitingArlandTest 1 (Arland, from ACR_TestHooksImpl.ScenarioSecond):
// no spot on its runway (RunArland).
class ACR_TestPositionSiting
{
 static const int CHECKS = 5;
 static const int CHECKS_ARLAND = 5;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range || tick != 8) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  // The checks are on the best spot: no variety (ACR_Variety).
  ACR_Variety.s_bOff = true;
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!us || !alpha)
  {
   ACR_TestRegress.Abort("possiting", "no US faction data or Alpha");
   return;
  }
  ACR_AICommander ai = new ACR_AICommander(manager, "US");
  ai.SetDoctrineForTest(ACR_AIDoctrine.Find("fortress"));
  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  ai.FrontBasesForTest(fronts, enemies);
  if (fronts.IsEmpty())
  {
   ACR_TestRegress.Abort("possiting", "no US front base");
   return;
  }
  vector basePos = fronts[0].GetOrigin();
  vector enemyPos = enemies[0];
  vector toEnemy = enemyPos - basePos;
  toEnemy[1] = 0;
  toEnemy.Normalize();

  // 1. The best spot round the base.
  array<ref ACR_PositionSite> sites = {};
  int nearFields;
  int t0 = System.GetTickCount();
  ACR_PositionSiting.Candidates(alpha, "US", basePos, enemyPos, sites, nearFields);
  int cost = System.GetTickCount() - t0;
  for (int k = 0; k < sites.Count() && k < 5; k++) PrintFormat("[ACR] POSSITING %1: %2 at %3", k, ACR_PositionSiting.Breakdown(sites[k]), sites[k].m_vPos);
  ACR_PositionSite oldFirst;
  foreach (ACR_PositionSite listed : sites)
  {
   if (!oldFirst && listed.m_sSource == "line") oldFirst = listed;
  }
  ACR_PositionSite best;
  if (!sites.IsEmpty()) best = sites[0];
  bool good = best && best.m_fSight >= 60 && best.m_iDeadFront <= 1;
  string bestDetail = "no spot";
  if (best) bestDetail = ACR_PositionSiting.Breakdown(best);
  string oldDetail = "none";
  if (oldFirst) oldDetail = ACR_PositionSiting.Breakdown(oldFirst);
  ACR_TestRegress.Check("possiting: the best Fortify spot sees 60 m or more down the approach with at most one dead spot in front", good, string.Format("%1 of %2 spots (%3 ms): %4; the old first line spot: %5", fronts[0].m_sName, sites.Count(), cost, bestDetail, oldDetail));

  // 2. A field of ours where the best spot is: no spot within DEFENCE_STANDOFF of it.
  string apartDetail = "no spot";
  bool apart;
  if (best)
  {
   ACR_Minefield field = new ACR_Minefield();
   field.m_sFaction = "US";
   field.m_vPos = best.m_vPos;
   ACR_Minefields.s_Fields.Insert(field);
   array<ref ACR_PositionSite> withField = {};
   int fieldNear;
   ACR_PositionSiting.Candidates(alpha, "US", basePos, enemyPos, withField, fieldNear);
   ACR_Minefields.s_Fields.RemoveItem(field);
   float nearest = float.MAX;
   foreach (ACR_PositionSite spot : withField) nearest = Math.Min(nearest, vector.DistanceXZ(spot.m_vPos, field.m_vPos));
   apart = fieldNear > 0 && nearest >= ACR_AICommander.DEFENCE_STANDOFF;
   apartDetail = string.Format("%1 spots left out by the field, nearest kept %2 m", fieldNear, (int)nearest);
  }
  ACR_TestRegress.Check("possiting: no Fortify spot within 60 m of our field", apart, apartDetail);

  // 3. A roadblock of ours in the best spot's field of fire (80 m out on the line to the enemy, or
  // short of where its sight ends): that spot now overlooks it and scores 2 more. (At the base's
  // road site, in woods on the test range, no spot sees it.)
  string overName = "possiting: a spot that overlooks our roadblock scores for it";
  if (!best) ACR_TestRegress.Skip(overName, "no spot");
  else
  {
   ACR_Roadblock pretend = new ACR_Roadblock();
   pretend.m_sFaction = "US";
   pretend.m_vPos = ACR_Manager.Ground(best.m_vPos + toEnemy * Math.Min(80, best.m_fSight - 20));
   ACR_Roadblocks.s_aRoadblocks.Insert(pretend);
   array<ref ACR_PositionSite> withRoadblock = {};
   int roadNear;
   ACR_PositionSiting.Candidates(alpha, "US", basePos, enemyPos, withRoadblock, roadNear);
   ACR_Roadblocks.s_aRoadblocks.RemoveItem(pretend);
   ACR_PositionSite again;
   int overlooking;
   foreach (ACR_PositionSite seeing : withRoadblock)
   {
    if (seeing.m_sOverlooks == "roadblock") overlooking++;
    if (!again && vector.DistanceXZ(seeing.m_vPos, best.m_vPos) < 1) again = seeing;
   }
   bool scored = again && again.m_sOverlooks == "roadblock" && again.m_fScore >= best.m_fScore + 1.99;
   string againDetail = "the spot is gone";
   if (again) againDetail = ACR_PositionSiting.Breakdown(again);
   ACR_TestRegress.Check(overName, scored, string.Format("roadblock %1 m in front of it; the spot now %2 (was %3); %4 of %5 spots overlook it", (int)vector.DistanceXZ(pretend.m_vPos, best.m_vPos), againDetail, best.m_fScore, overlooking, withRoadblock.Count()));
  }

  // 4. The AI digs in at one of the best spots.
  ACR_Options.Set("acrFortifyAI", "1");
  us.m_iRP = 500;
  ai.StartTickForTest();
  ai.ResetBuildTimersForTest();
  ai.ManageFortify();
  ACR_Options.Unset("acrFortifyAI");
  vector at = DiggingAt(manager);
  int index = -1;
  foreach (int s, ACR_PositionSite spotted : sites)
  {
   if (index < 0 && vector.DistanceXZ(spotted.m_vPos, at) < 2) index = s;
  }
  foreach (ACR_Squad digger : manager.m_Squads)
  {
   if (digger && digger.m_FortifyPlan) digger.Halt("Holding");
  }
  ACR_TestRegress.Check("possiting: the AI digs in at one of the best spots", at != vector.Zero && index >= 0 && index < 3, string.Format("dug at %1: spot %2 of the list", at, index));

  // 5. The score's terms.
  ACR_PositionSite plain = new ACR_PositionSite();
  plain.m_fSight = 100;
  float plainScore = ACR_PositionSiting.Score(plain);
  plain.m_fSight = 250;
  float farScore = ACR_PositionSiting.Score(plain);
  plain.m_fRise = 3;
  float highScore = ACR_PositionSiting.Score(plain);
  plain.m_iDeadFront = 2;
  float deadScore = ACR_PositionSiting.Score(plain);
  plain.m_bFits = true;
  plain.m_sOverlooks = "roadblock";
  float fitScore = ACR_PositionSiting.Score(plain);
  bool terms = farScore > plainScore && highScore > farScore && deadScore < highScore && fitScore > deadScore + 3;
  ACR_TestRegress.Check("possiting: field of fire, high ground, dead ground, fit and overlooking ours move the score", terms, string.Format("sees 100 m %1, 250 m %2, 3 m up %3, 2 dead spots %4, fits and overlooks %5", plainScore, farScore, highScore, deadScore, fitScore));
  ACR_TestRegress.Complete("possiting", CHECKS);
 }

 // Arland (pn10b, 26 Oct): Blue's Combat Engineers dug both positions in on the runway, at
 // <1323,2301> on the approach to Barracks (its longest field of fire, an old line spot) and at
 // <1310,2885> on the approach to Airfield. Those two points are paved ground, and the best spot at
 // the Airfield and at the Barracks, each facing the nearest enemy-held base, is off it.
 // What is at a point (for the runway): the trace's surface from above with the world only and with
 // entities, the road network's distance, and the entities within 12 m.
 protected static ref array<string> s_aNear = {};
 protected static void Probe(string label, vector pos)
 {
  string world = Surface(pos, TraceFlags.WORLD);
  string ents = Surface(pos, TraceFlags.ENTS);
  vector road;
  string roadAt = "none within 40 m";
  if (BBB_Terrain.RoadPointNear(pos, 40, road)) roadAt = string.Format("%1 m", (int)vector.DistanceXZ(pos, road));
  s_aNear.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(pos, 12, AddNear, null, EQueryEntitiesFlags.ALL);
  string near = "";
  foreach (string name : s_aNear) near += name + " ";
  PrintFormat("[ACR] POSSITING PROBE %1 %2: surface world '%3', entities '%4'; road %5, clearance %6 m; within 12 m: %7", label, pos, world, ents, roadAt, ACR_Minefields.RoadClearance(pos), near);
 }

 protected static string Surface(vector pos, TraceFlags flags)
 {
  TraceParam param = new TraceParam();
  param.Start = pos + Vector(0, 5, 0);
  param.End = pos - Vector(0, 2, 0);
  param.Flags = flags;
  param.LayerMask = EPhysicsLayerPresets.Projectile;
  GetGame().GetWorld().TraceMove(param, null);
  string hit = "";
  if (param.TraceEnt) hit = param.TraceEnt.ClassName() + " ";
  if (param.SurfaceProps) hit += param.SurfaceProps.GetName();
  return hit;
 }

 // A spot 15 m out from the nearest structure to the Airfield base within 300 m (taller than 4 m,
 // 6 to 80 m across, not one the base stands in), and its footprint's nearest point to the base.
 protected static IEntity s_WallBuilding;
 protected static float s_fWallBuilding;
 protected static string s_sWallBuilding;
 protected static vector s_vWallBase;
 protected static bool WallTestSpot(out vector spot, out vector wallAt)
 {
  s_WallBuilding = null;
  s_fWallBuilding = -1;
  s_sWallBuilding = "";
  s_vWallBase = vector.Zero;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base && base.m_sName == "Airfield") s_vWallBase = base.GetOrigin();
  }
  if (s_vWallBase == vector.Zero) return false;
  GetGame().GetWorld().QueryEntitiesBySphere(s_vWallBase, 300, FindWallBuilding, null, EQueryEntitiesFlags.STATIC);
  if (!s_WallBuilding) return false;
  vector mins, maxs;
  s_WallBuilding.GetWorldBounds(mins, maxs);
  wallAt = Vector(Math.Clamp(s_vWallBase[0], mins[0], maxs[0]), 0, Math.Clamp(s_vWallBase[2], mins[2], maxs[2]));
  vector away = s_vWallBase - wallAt;
  away[1] = 0;
  away.Normalize();
  spot = ACR_Manager.Ground(wallAt + away * 15);
  wallAt[1] = spot[1];
  return true;
 }

 protected static bool FindWallBuilding(IEntity e)
 {
  if (!e || e.GetParent() || Tree.Cast(e)) return true;
  vector mins, maxs;
  e.GetWorldBounds(mins, maxs);
  float ground = GetGame().GetWorld().GetSurfaceY(e.GetOrigin()[0], e.GetOrigin()[2]);
  if (maxs[1] - Math.Max(mins[1], ground) < 4) return true;
  float wide = maxs[0] - mins[0];
  float deep = maxs[2] - mins[2];
  if (wide < 6 || deep < 6 || wide > 80 || deep > 80) return true;
  vector near = Vector(Math.Clamp(s_vWallBase[0], mins[0], maxs[0]), 0, Math.Clamp(s_vWallBase[2], mins[2], maxs[2]));
  float d = vector.DistanceXZ(near, s_vWallBase);
  if (d < 20) return true;
  if (s_fWallBuilding >= 0 && d >= s_fWallBuilding) return true;
  s_WallBuilding = e;
  s_fWallBuilding = d;
  s_sWallBuilding = e.ClassName();
  if (e.GetPrefabData()) s_sWallBuilding = FilePath.StripPath(e.GetPrefabData().GetPrefabName());
  return true;
 }

 // A spot 8 m from the nearest fence or wall (a Structures/Walls prefab over 0.9 m) to the Airfield
 // base within 300 m, on the base's side, and the fence's nearest point.
 protected static IEntity s_Fence;
 protected static float s_fFence;
 protected static string s_sFence;
 protected static bool FenceTestSpot(out vector spot, out vector fenceAt)
 {
  s_Fence = null;
  s_fFence = -1;
  s_sFence = "no fence found";
  if (s_vWallBase == vector.Zero) return false;
  GetGame().GetWorld().QueryEntitiesBySphere(s_vWallBase, 300, FindFence, null, EQueryEntitiesFlags.STATIC);
  if (!s_Fence) return false;
  vector mins, maxs;
  s_Fence.GetWorldBounds(mins, maxs);
  fenceAt = Vector(Math.Clamp(s_vWallBase[0], mins[0], maxs[0]), 0, Math.Clamp(s_vWallBase[2], mins[2], maxs[2]));
  vector away = s_vWallBase - fenceAt;
  away[1] = 0;
  away.Normalize();
  spot = ACR_Manager.Ground(fenceAt + away * 8);
  fenceAt[1] = spot[1];
  return true;
 }

 protected static bool FindFence(IEntity e)
 {
  if (!e || e.GetParent() || !e.GetPrefabData()) return true;
  string name = e.GetPrefabData().GetPrefabName();
  name.ToLower();
  if (!name.Contains("structures/walls/")) return true;
  vector mins, maxs;
  e.GetWorldBounds(mins, maxs);
  float ground = GetGame().GetWorld().GetSurfaceY(e.GetOrigin()[0], e.GetOrigin()[2]);
  if (maxs[1] - Math.Max(mins[1], ground) < 0.9) return true;
  vector near = Vector(Math.Clamp(s_vWallBase[0], mins[0], maxs[0]), 0, Math.Clamp(s_vWallBase[2], mins[2], maxs[2]));
  float d = vector.DistanceXZ(near, s_vWallBase);
  if (d < 20) return true;
  if (s_fFence >= 0 && d >= s_fFence) return true;
  s_Fence = e;
  s_fFence = d;
  s_sFence = FilePath.StripPath(e.GetPrefabData().GetPrefabName());
  return true;
 }

 protected static bool AddNear(IEntity e)
 {
  if (!e || s_aNear.Count() >= 14) return s_aNear.Count() < 14;
  string name = e.ClassName();
  if (e.GetPrefabData()) name = FilePath.StripPath(e.GetPrefabData().GetPrefabName());
  if (!s_aNear.Contains(name)) s_aNear.Insert(name);
  return true;
 }

 static void RunArland(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || second != 15) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  ACR_Variety.s_bOff = true;
  vector barracksSpot = ACR_Manager.Ground(Vector(1323, 0, 2301));
  vector airfieldSpot = ACR_Manager.Ground(Vector(1310, 0, 2885));
  Probe("the Barracks spot", barracksSpot);
  Probe("the Airfield spot", airfieldSpot);
  bool barracksOff = ACR_Fortify.HardSurface(barracksSpot) || ACR_PositionSiting.OnAirfield(barracksSpot);
  bool airfieldOff = ACR_Fortify.HardSurface(airfieldSpot) || ACR_PositionSiting.OnAirfield(airfieldSpot);
  ACR_TestRegress.Check("possiting (Arland): the playtest's two runway spots read as airfield or paved ground", barracksOff && airfieldOff,
   string.Format("<1323,2301> '%1', airfield %2; <1310,2885> '%3', airfield %4", ACR_Fortify.SurfaceName(barracksSpot), ACR_PositionSiting.OnAirfield(barracksSpot), ACR_Fortify.SurfaceName(airfieldSpot), ACR_PositionSiting.OnAirfield(airfieldSpot)));

  // A spot 15 m out from the nearest structure to the Airfield base, facing it, is walled in (pn11:
  // a position faced a wall).
  vector wallSpot, wallAt;
  string wallWhat = "no building found";
  float wall = -1;
  if (WallTestSpot(wallSpot, wallAt))
  {
   vector toWall = wallAt - wallSpot;
   wall = ACR_PositionSiting.WallAhead(wallSpot, Math.Atan2(toWall[0], toWall[2]) * Math.RAD2DEG, wallWhat);
  }
  ACR_TestRegress.Check("possiting (Arland): a spot 15 m in front of a structure is walled in", wall >= 0 && wall <= 16,
   string.Format("from %1 facing %2 (%3): %4 at %5 m", wallSpot, wallAt, s_sWallBuilding, wallWhat, wall));

  // #93: a spot 10 m out from that building, facing away from it, is still too close to it (the wall
  // check only sees the arc in front); a spot 8 m behind the nearest fence, facing across it, is
  // hemmed in.
  ACR_PositionSite beside = new ACR_PositionSite();
  if (s_WallBuilding)
  {
   vector outward = wallSpot - wallAt;
   outward[1] = 0;
   outward.Normalize();
   vector besideSpot = ACR_Manager.Ground(wallAt + outward * 10);
   ACR_PositionSiting.FenceOrBuilding(besideSpot, Math.Atan2(outward[0], outward[2]) * Math.RAD2DEG, beside);
  }
  ACR_TestRegress.Check("possiting (Arland): a spot 10 m from a building, facing away from it, counts the building", beside.m_fBuilding >= 0 && beside.m_fBuilding <= 11,
   string.Format("%1: %2 at %3 m", s_sWallBuilding, beside.m_sBuilding, beside.m_fBuilding));
  ACR_PositionSite fenced = new ACR_PositionSite();
  vector fenceSpot, fenceAt;
  if (FenceTestSpot(fenceSpot, fenceAt))
  {
   vector toFence = fenceAt - fenceSpot;
   ACR_PositionSiting.FenceOrBuilding(fenceSpot, Math.Atan2(toFence[0], toFence[2]) * Math.RAD2DEG, fenced);
  }
  ACR_TestRegress.Check("possiting (Arland): a spot 8 m behind a fence, facing across it, is hemmed in", fenced.m_fFence >= 0 && fenced.m_fFence <= 9,
   string.Format("from %1 facing %2 (%3): %4 at %5 m", fenceSpot, fenceAt, s_sFence, fenced.m_sFence, fenced.m_fFence));

  bool off = true;
  int found;
  string detail = "";
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || (base.m_sName != "Airfield" && base.m_sName != "Barracks")) continue;
   string faction = base.m_sOwner;
   if (faction.IsEmpty()) faction = "US";
   ACR_Base enemy;
   float nearest = float.MAX;
   foreach (ACR_Base other : ACR_Base.s_All)
   {
    if (!other || other.m_sOwner.IsEmpty() || other.m_sOwner == faction) continue;
    float d = vector.DistanceXZ(other.GetOrigin(), base.GetOrigin());
    if (d < nearest)
    {
     nearest = d;
     enemy = other;
    }
   }
   if (!enemy) continue;
   array<ref ACR_PositionSite> sites = {};
   int nearFields;
   ACR_PositionSiting.Candidates(null, faction, base.GetOrigin(), enemy.GetOrigin(), sites, nearFields);
   if (sites.IsEmpty()) continue;
   found++;
   int paved;
   foreach (ACR_PositionSite site : sites)
   {
    if (site.m_iPaved > 0) paved++;
   }
   if (sites[0].m_iPaved > 0) off = false;
   detail += string.Format("%1 (facing %2): best of %3 at %4, %5; %6 of them on paved ground | ", base.m_sName, enemy.m_sName, sites.Count(), sites[0].m_vPos, ACR_PositionSiting.Breakdown(sites[0]), paved);
  }
  ACR_TestRegress.Check("possiting (Arland): the best spots at the Airfield and the Barracks are off paved ground", off && found == 2, detail);
  ACR_TestRegress.Complete("possiting", CHECKS_ARLAND);
 }

 protected static vector DiggingAt(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad && squad.m_FortifyPlan && squad.GetFaction() && squad.GetFaction().GetFactionKey() == "US") return squad.m_FortifyPlan.m_vSite;
  }
  return vector.Zero;
 }
}
