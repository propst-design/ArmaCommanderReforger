// -acrMinefieldsTest 1 (server-capable, ~3 min): the commander's minefield orders. Plans on a
// road (AT mines on it, AP mines off it), refusals that charge nothing, options and setup rows,
// a mixed field laid by Alpha (cost, every mine, signs, armed, its side's map area), the
// friendly-fire rule with a soldier of each side dropped onto a mine, a cancelled order's
// refund, Disarm mines lifting the field, and the AI commander laying one only with
// -acrMinesAI.
class ACR_TestMinefields
{
 static const int CHECKS = 30;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iRP;
 static vector s_vSite;
 static vector s_vCenter;
 static vector s_vRoad;
 static int s_iPlanned;
 static ACR_Minefield s_Field;
 static ref array<IEntity> s_Signs = {};
 static IEntity s_EnemyMine, s_FriendMine, s_Enemy, s_Friend;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("minefields", "no Alpha squad");
   return;
  }
  if (tick > 300) ACR_TestRegress.Abort("minefields", string.Format("timed out in step %1", s_iStep));
  if (tick < 5) return;
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  int waited = tick - s_iStepAt;

  if (s_iStep == 0)
  {
   ACR_Minefields.WORK_SCALE = 0.15;
   us.m_iRP = 500;
   if (!CheckPlans(alpha, us)) return;
   CheckOptions(alpha);
   // Alpha lays a mixed field next to the range.
   if (!FindSite(alpha, ACR_EMineKind.MIXED, s_vSite))
   {
    ACR_TestRegress.Abort("minefields", "no site for the live field near Alpha");
    return;
   }
   s_iRP = us.m_iRP;
   string why;
   bool ordered = ACR_Minefields.Order(alpha, ACR_EMineKind.MIXED, s_vSite, why);
   if (alpha.m_MinePlan) s_iPlanned = alpha.m_MinePlan.m_aPos.Count();
   ACR_TestRegress.Check("minefields: order charges 25 RP and starts laying", ordered && us.m_iRP == s_iRP - 25 && alpha.OrderState() == ACR_EOrderState.LAY_MINES && alpha.m_Order.StartsWith("Laying mixed minefield"), string.Format("%1 rp %2 -> %3 '%4' planned %5 at %6", why, s_iRP, us.m_iRP, alpha.m_Order, s_iPlanned, s_vSite));
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   if (alpha.m_MinePlan && waited < 100) return;
   s_Field = null;
   foreach (ACR_Minefield field : ACR_Minefields.s_Fields) if (field.m_sFaction == "US") s_Field = field;
   int laid;
   if (s_Field) laid = s_Field.m_aMines.Count();
   ACR_TestRegress.Check("minefields: finished with every planned mine down", !alpha.m_MinePlan && s_Field && laid == s_iPlanned && laid > 0 && alpha.OrderState() == ACR_EOrderState.DEFEND, string.Format("laid %1 of %2 after %3 s, order '%4'", laid, s_iPlanned, waited, alpha.m_Order));
   if (!s_Field)
   {
    ACR_TestRegress.Abort("minefields", "no field was laid");
    return;
   }
   s_vCenter = s_Field.m_vPos;
   s_Signs.Copy(s_Field.m_aSigns);
   ACR_TestRegress.Check("minefields: minefield signs at the corners", s_Signs.Count() >= 3, string.Format("%1 signs", s_Signs.Count()));
   array<vector> positions = {};
   array<float> radii = {};
   array<string> labels = {};
   ACR_Minefields.Zones("US", positions, radii, labels);
   int own = labels.Count();
   string label;
   if (own > 0) label = labels[0];
   labels.Clear();
   ACR_Minefields.Zones("USSR", positions, radii, labels);
   ACR_TestRegress.Check("minefields: the area shows on its own side's map only", own == 1 && label.Contains("mixed minefield") && labels.IsEmpty(), string.Format("US %1 '%2', USSR %3", own, label, labels.Count()));
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   if (waited < 3) return;
   int armed, ap, at;
   foreach (IEntity mine : s_Field.m_aMines)
   {
    SCR_PressureTriggerComponent trigger = SCR_PressureTriggerComponent.Cast(mine.FindComponent(SCR_PressureTriggerComponent));
    if (trigger && trigger.IsActivated()) armed++;
    string prefab = mine.GetPrefabData().GetPrefabName();
    if (prefab.Contains("E_Mine_M14")) ap++;
    if (prefab.Contains("E_Mine_M15AT")) at++;
   }
   ACR_TestRegress.Check("minefields: every mine armed, US AP and AT mines", armed == s_Field.m_aMines.Count() && ap > 0 && at > 0 && ap + at == armed, string.Format("armed %1 of %2, M14 %3, M15 %4", armed, s_Field.m_aMines.Count(), ap, at));

   // Friendly fire: an AP mine far from Alpha for an enemy, another for one of ours.
   IEntity body = alpha.GetAnchor();
   foreach (IEntity candidate : s_Field.m_aMines)
   {
    string candidatePrefab = candidate.GetPrefabData().GetPrefabName();
    if (!candidatePrefab.Contains("E_Mine_M14")) continue;
    if (!s_EnemyMine || vector.DistanceXZ(candidate.GetOrigin(), body.GetOrigin()) > vector.DistanceXZ(s_EnemyMine.GetOrigin(), body.GetOrigin())) s_EnemyMine = candidate;
   }
   foreach (IEntity other : s_Field.m_aMines)
   {
    string otherPrefab = other.GetPrefabData().GetPrefabName();
    if (!otherPrefab.Contains("E_Mine_M14") || !s_EnemyMine) continue;
    if (!s_FriendMine || vector.DistanceXZ(other.GetOrigin(), s_EnemyMine.GetOrigin()) > vector.DistanceXZ(s_FriendMine.GetOrigin(), s_EnemyMine.GetOrigin())) s_FriendMine = other;
   }
   array<ResourceName> enemies = manager.m_Bases.GetFaction("USSR").m_aGarrison;
   array<ResourceName> friends = us.m_aGarrison;
   if (!s_EnemyMine || !s_FriendMine || enemies.IsEmpty() || friends.IsEmpty())
   {
    ACR_TestRegress.Abort("minefields", "no AP mines or soldier prefabs for the friendly-fire check");
    return;
   }
   // Each drops a metre onto its mine.
   s_Enemy = ACR_Manager.SpawnPrefab(enemies[0], s_EnemyMine.GetOrigin());
   s_Friend = ACR_Manager.SpawnPrefab(friends[0], s_FriendMine.GetOrigin());
   if (s_Enemy) s_Enemy.SetOrigin(s_EnemyMine.GetOrigin() + Vector(0, 1, 0));
   if (s_Friend) s_Friend.SetOrigin(s_FriendMine.GetOrigin() + Vector(0, 1, 0));
   ACR_TestRegress.Check("minefields: our AI is spared by our mine, the enemy is not", s_Enemy && s_Friend && ACR_Minefields.Spares(s_FriendMine, body) && ACR_Minefields.Spares(s_FriendMine, s_Friend) && !ACR_Minefields.Spares(s_FriendMine, s_Enemy), string.Format("mines %1 m apart", vector.DistanceXZ(s_EnemyMine.GetOrigin(), s_FriendMine.GetOrigin())));
   ACR_Options.Set("acrMinesRealistic", "1");
   ACR_TestRegress.Check("minefields: -acrMinesRealistic 1 spares nobody", !ACR_Minefields.Spares(s_FriendMine, body) && !ACR_Minefields.Spares(s_FriendMine, s_Friend));
   ACR_Options.Unset("acrMinesRealistic");
   PrintFormat("[ACR] MINEFIELDS enemy %1 dropped onto %2, friend %3 onto %4", s_Enemy, s_EnemyMine.GetOrigin(), s_Friend, s_FriendMine.GetOrigin());
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   if (waited < 6) return;
   bool enemyBlown = !s_EnemyMine || ACR_Minefields.Spent(s_EnemyMine);
   bool friendSafe = s_FriendMine && !ACR_Minefields.Spent(s_FriendMine) && s_Friend && ACR_Respawn.IsAlive(s_Friend);
   ACR_TestRegress.Check("minefields: an enemy soldier on an AP mine sets it off", enemyBlown, string.Format("mine left %1, enemy alive %2", s_EnemyMine != null, s_Enemy && ACR_Respawn.IsAlive(s_Enemy)));
   ACR_TestRegress.Check("minefields: our own AI soldier on our mine stays safe", friendSafe, string.Format("mine left %1, friend alive %2", s_FriendMine != null, s_Friend && ACR_Respawn.IsAlive(s_Friend)));
   if (s_Enemy) SCR_EntityHelper.DeleteEntityAndChildren(s_Enemy);
   if (s_Friend) SCR_EntityHelper.DeleteEntityAndChildren(s_Friend);

   // A new order before any mine went in: the whole cost comes back.
   vector other;
   int fields = ACR_Minefields.CountFields("US");
   string why;
   bool found = FindSite(alpha, ACR_EMineKind.AP, other);
   int before = us.m_iRP;
   bool ordered = found && ACR_Minefields.Order(alpha, ACR_EMineKind.AP, other, why);
   int charged = us.m_iRP;
   alpha.Halt("Holding");
   ACR_TestRegress.Check("minefields: cancelled before any mine, refunded in full", ordered && charged == before - 15 && us.m_iRP == before && !alpha.m_MinePlan && ACR_Minefields.CountFields("US") == fields && alpha.m_Order == "Holding", string.Format("%1 rp %2 -> %3 -> %4 '%5'", why, before, charged, us.m_iRP, alpha.m_Order));

   // Like any new order, a mine order replaces a roadblock build and refunds it...
   before = us.m_iRP;
   bool building = alpha.EstablishRoadblock(s_vRoad, 0);
   bool replaced = building && alpha.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK && ACR_Minefields.Order(alpha, ACR_EMineKind.AP, other, why);
   ACR_TestRegress.Check("minefields: a mine order replaces a roadblock build and refunds it", replaced && alpha.OrderState() == ACR_EOrderState.LAY_MINES && us.m_iRP == before - 15, string.Format("%1 building %2, rp %3 -> %4, order '%5'", why, building, before, us.m_iRP, alpha.m_Order));

   // ... and Disarm mines over the field replaces that laying order (nothing laid: all 15 back).
   s_iRP = us.m_iRP;
   bool disarming = ACR_Minefields.Order(alpha, ACR_EMineKind.DISARM, s_vCenter, why);
   ACR_TestRegress.Check("minefields: Disarm mines replaces the laying order, refunds it and charges 3 RP", disarming && us.m_iRP == s_iRP + 15 - 3 && alpha.OrderState() == ACR_EOrderState.DISARM_MINES && alpha.m_MinePlan.m_eKind == ACR_EMineKind.DISARM, string.Format("%1 rp %2 -> %3 '%4'", why, s_iRP, us.m_iRP, alpha.m_Order));
   NextStep(tick);
   return;
  }

  if (s_iStep == 4)
  {
   if (alpha.m_MinePlan && waited < 150) return;
   IEntity left = ACR_Minefields.NearestMine(s_vCenter, ACR_Minefields.DISARM_RADIUS, s_vCenter);
   ACR_TestRegress.Check("minefields: every armed mine round the point disarmed and lifted", !alpha.m_MinePlan && !left && ACR_Minefields.CountMines("US") == 0 && !s_FriendMine, string.Format("after %1 s, order '%2', US mines %3, our last mine gone %4", waited, alpha.m_Order, ACR_Minefields.CountMines("US"), !s_FriendMine));
   NextStep(tick);
   return;
  }

  if (s_iStep == 5)
  {
   if (waited < 6) return;
   int signs;
   foreach (IEntity sign : s_Signs) if (sign) signs++;
   ACR_TestRegress.Check("minefields: the empty field and its signs are gone", ACR_Minefields.CountFields("US") == 0 && signs == 0, string.Format("fields %1, signs %2", ACR_Minefields.CountFields("US"), signs));

   // AI commander: off by default, on with -acrMinesAI 1.
   us.m_iRP = 500;
   ACR_AICommander ai = new ACR_AICommander(manager, "US");
   ai.StartTickForTest();
   ai.ManageMinefields();
   ACR_TestRegress.Check("minefields: the AI commander lays none without -acrMinesAI", !LayingFor(manager, "US"));
   ACR_Options.Set("acrMinesAI", "1");
   ai.StartTickForTest();
   ai.ManageMinefields();
   ACR_Squad layer = LayingFor(manager, "US");
   string where = "none";
   if (layer) where = string.Format("%1 at %2", layer.GetName(), layer.m_MinePlan.m_vSite);
   ACR_TestRegress.Check("minefields: with -acrMinesAI 1 it sends a squad to mine an approach", layer && layer.m_iAITask == ACR_EAITask.MINES && layer.OrderState() == ACR_EOrderState.LAY_MINES, where);
   if (layer) layer.Halt("Holding");
   ACR_Options.Unset("acrMinesAI");
   ACR_TestRegress.Complete("minefields", CHECKS);
   s_iStep = 6;
  }
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 // Plans on a road (nothing charged), refusals and the refund rule.
 protected static bool CheckPlans(ACR_Squad alpha, ACR_FactionData us)
 {
  vector road, roadDir;
  float width;
  bool found = FindRoad(alpha.GetAnchor().GetOrigin(), road, roadDir, width);
  s_vRoad = road;
  ACR_TestRegress.Check("minefields: a road for the plan checks", found, road.ToString());
  if (!found)
  {
   ACR_TestRegress.Abort("minefields", "no road near the range");
   return false;
  }
  string why;
  ACR_MinePlan at = ACR_Minefields.Plan(alpha, ACR_EMineKind.AT, road, why);
  int atOn, atCount;
  if (at)
  {
   foreach (int i, vector p : at.m_aPos)
   {
    if (!at.m_aAntiTank[i]) continue;
    atCount++;
    if (ACR_Minefields.RoadClearance(p) <= 0.1) atOn++;
   }
  }
  ACR_TestRegress.Check("minefields: AT plan puts every mine on the road", at && atCount >= 8 && atOn == atCount, string.Format("%1 %2 on the road of %3 (width %4)", why, atOn, atCount, width));
  ACR_TestRegress.Check("minefields: the field is turned along the road", at && Math.AbsFloat(vector.Dot(at.m_vForward, roadDir)) > 0.99);

  ACR_MinePlan ap = ACR_Minefields.Plan(alpha, ACR_EMineKind.AP, road, why);
  int apOff, apCount;
  float closest = float.MAX;
  if (ap)
  {
   foreach (int j, vector q : ap.m_aPos)
   {
    apCount++;
    if (ACR_Minefields.RoadClearance(q) >= 1) apOff++;
    for (int k = j + 1; k < ap.m_aPos.Count(); k++) closest = Math.Min(closest, vector.DistanceXZ(q, ap.m_aPos[k]));
   }
  }
  ACR_TestRegress.Check("minefields: AP plan keeps off the road, 2.5 m apart", ap && apCount >= 20 && apOff == apCount && closest >= ACR_Minefields.AP_SPACING - 0.01, string.Format("%1 %2 off the road of %3, closest pair %4 m", why, apOff, apCount, closest));

  ACR_MinePlan mixed = ACR_Minefields.Plan(alpha, ACR_EMineKind.MIXED, road, why);
  int mixedAT, mixedAP, misplaced;
  if (mixed)
  {
   foreach (int m, vector r : mixed.m_aPos)
   {
    float off = ACR_Minefields.RoadClearance(r);
    if (mixed.m_aAntiTank[m]) mixedAT++;
    else mixedAP++;
    if ((mixed.m_aAntiTank[m] && off > 0.1) || (!mixed.m_aAntiTank[m] && off < 1)) misplaced++;
   }
  }
  ACR_TestRegress.Check("minefields: mixed plan has AT mines on the road, AP mines round it", mixed && mixedAT >= 5 && mixedAP >= 12 && misplaced == 0, string.Format("%1 AT %2, AP %3, misplaced %4", why, mixedAT, mixedAP, misplaced));
  ACR_TestRegress.Check("minefields: planning charges nothing", us.m_iRP == 500, us.m_iRP.ToString());

  ACR_Options.Set("acrMines", "0");
  bool turnedOff = !ACR_Minefields.Plan(alpha, ACR_EMineKind.AP, road, why) && why.Contains("turned off") && !ACR_Minefields.CanLay(alpha);
  ACR_Options.Unset("acrMines");
  ACR_TestRegress.Check("minefields: -acrMines 0 refuses the orders", turnedOff, why);
  us.m_iRP = 10;
  bool poor = !ACR_Minefields.Plan(alpha, ACR_EMineKind.AT, road, why) && why.Contains("RP") && us.m_iRP == 10;
  us.m_iRP = 500;
  ACR_TestRegress.Check("minefields: refused without enough RP", poor, why);
  bool atBase = false;
  if (!ACR_Base.s_All.IsEmpty()) atBase = !ACR_Minefields.Plan(alpha, ACR_EMineKind.AP, ACR_Base.s_All[0].GetOrigin() + Vector(10, 0, 0), why) && why.Contains("base");
  ACR_TestRegress.Check("minefields: refused at a base", atBase, why);

  ACR_MinePlan half = new ACR_MinePlan();
  half.m_eKind = ACR_EMineKind.AT;
  half.m_iCost = 20;
  for (int n = 0; n < 12; n++) half.m_aPos.Insert(vector.Zero);
  half.m_iLaid = 6;
  ACR_MinePlan disarm = new ACR_MinePlan();
  disarm.m_eKind = ACR_EMineKind.DISARM;
  disarm.m_iCost = 3;
  int disarmNothing = ACR_Minefields.Refund(disarm);
  disarm.m_iDisarmed = 1;
  ACR_TestRegress.Check("minefields: refund is the share not laid, all of Disarm mines until one is done", ACR_Minefields.Refund(half) == 10 && disarmNothing == 3 && ACR_Minefields.Refund(disarm) == 0);
  return true;
 }

 protected static void CheckOptions(ACR_Squad alpha)
 {
  ACR_TestRegress.Check("minefields: defaults are orders on, own AI spared, AI commanders off", ACR_Minefields.Enabled() && !ACR_Minefields.Realistic() && !ACR_Minefields.AIUses() && ACR_Minefields.CanLay(alpha));
  map<string, string> header = new map<string, string>();
  header.Set("acrMines", "0");
  ACR_Options.UseHeaderForTest(header);
  bool headerOff = !ACR_Minefields.Enabled() && ACR_Options.Source("acrMines") == "header";
  ACR_Options.UseHeaderForTest(null);
  ACR_TestRegress.Check("minefields: a mission header can turn the orders off", headerOff && ACR_Minefields.Enabled());
  string rows = ACR_Setup.Rows();
  ACR_TestRegress.Check("minefields: setup screen rows with their defaults", rows.Contains("mines\tMinefield orders: on") && rows.Contains("minesreal\tOwn mines: safe for our AI") && rows.Contains("minesai\tAI commanders lay mines: off") && ACR_Setup.ValueName("minesreal", "1") == "dangerous to all" && ACR_Setup.Option("minesai") == "acrMinesAI");
  ACR_TestRegress.Check("minefields: US / USSR / FIA mine prefabs", ACR_Minefields.MineFor("US", true) == ACR_Minefields.MINE_AT_US && ACR_Minefields.MineFor("US", false) == ACR_Minefields.MINE_AP_US && ACR_Minefields.MineFor("USSR", false) == ACR_Minefields.MINE_AP_USSR && ACR_Minefields.MineFor("FIA", true) == ACR_Minefields.MINE_AT_USSR);
 }

 // A road point away from the bases with no other road within 25 m.
 protected static bool FindRoad(vector origin, out vector point, out vector dir, out float width)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return false;
  array<BaseRoad> roads = {};
  aiWorld.GetRoadNetworkManager().GetRoadsInAABB(origin - Vector(1500, 1000, 1500), origin + Vector(1500, 1000, 1500), roads);
  foreach (BaseRoad road : roads)
  {
   array<vector> points = {};
   road.GetPoints(points);
   foreach (vector p : points)
   {
    if (NearBase(p, 90) || BBB_Terrain.IsWaterAt(p)) continue;
    array<BaseRoad> near = {};
    aiWorld.GetRoadNetworkManager().GetRoadsInAABB(p - Vector(25, 50, 25), p + Vector(25, 50, 25), near);
    if (near.Count() != 1) continue;
    if (ACR_Minefields.Snap(p, 2, point, dir, width)) return true;
   }
  }
  return false;
 }

 protected static bool NearBase(vector p, float distance)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (vector.DistanceXZ(p, base.GetOrigin()) < distance) return true;
  return false;
 }

 // A spot near the squad where the order plans (away from the bases, dry, clear of our fields).
 protected static bool FindSite(ACR_Squad squad, ACR_EMineKind kind, out vector site)
 {
  vector from = squad.GetAnchor().GetOrigin();
  for (int r = 0; r < 4; r++)
  {
   for (int a = 0; a < 12; a++)
   {
    float angle = Math.PI2 * a / 12;
    vector point = ACR_Manager.Ground(from + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * (45 + r * 20));
    string why;
    if (!ACR_Minefields.Plan(squad, kind, point, why)) continue;
    site = point;
    return true;
   }
  }
  return false;
 }

 protected static ACR_Squad LayingFor(ACR_Manager manager, string faction)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad.m_MinePlan && squad.GetFaction() && squad.GetFaction().GetFactionKey() == faction) return squad;
  return null;
 }
}
