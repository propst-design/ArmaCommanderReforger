// Minefields (0.5.7): the commander's Lay minefield (AP / AT / mixed) and Disarm mines orders
// for a whole squad. ACR_Requests (and the AI commander) call Order: the server plans the field
// here, the squad pays and works it through its LAY_MINES / DISARM_MINES state (ACR_Squad), and
// calls back here to lay or disarm mines as the work progresses. Work pauses while the site is
// contested and goes twice as fast with sappers / engineers or a construction truck, like FOB
// upgrades. Mines are the game's own editable ones, bought with RP and armed with the game's
// own trigger: AT mines in both lanes of the road through the field, AP mines off the roads,
// minefield signs at the corners, and a red area on the owning side's command map. Disarm
// mines switches mines off with the game's own disarming (like AiO Command Menu's soldier-level
// Disarm mines) and lifts them. The owning side's AI never sets off its own mines unless
// -acrMinesRealistic 1; its players do (they see the marked area).
enum ACR_EMineKind
{
 AP,
 AT,
 MIXED,
 DISARM
}

// One live minefield: what its order laid there.
class ACR_Minefield : Managed
{
 int m_iId;
 ACR_EMineKind m_eKind;
 string m_sFaction;
 vector m_vPos;
 bool m_bLaying;              // its squad is still laying it
 ref array<IEntity> m_aMines = {};
 ref array<IEntity> m_aSigns = {};
}

// One Lay minefield / Disarm mines order: where its mines go and how far the work is. The
// squad keeps it while it works (ACR_Squad.m_MinePlan).
class ACR_MinePlan : Managed
{
 ACR_EMineKind m_eKind;
 string m_sFaction;
 vector m_vSite;              // field centre (on the road through it for AT / mixed)
 vector m_vForward;           // along that road, else the squad's approach
 vector m_vStage;             // where the squad gathers, outside the near edge
 float m_fRadius;             // squad members this close to the site work
 float m_fDuration;           // seconds of work (disarming: at least this long)
 int m_iCost;
 ref array<vector> m_aPos = {};
 ref array<bool> m_aAntiTank = {};
 int m_iLaid;
 int m_iDisarmed;
 float m_fCredit;             // disarming work towards the next mine
 float m_fProgress;
 float m_fLastTick;
 float m_fLastProgress;       // world ms of the last progress (3 min without it at the site: give up)
 float m_fDeadline;           // world ms by which the squad must reach the site
 bool m_bArrived;
 ACR_Minefield m_Field;
}

class ACR_Minefields
{
 static ref array<ref ACR_Minefield> s_Fields = {};
 protected static ref map<IEntity, string> s_Owner = new map<IEntity, string>(); // live mine -> faction that laid it
 protected static int s_iNextId;
 protected static int s_iTicks;

 static const ResourceName MINE_AP_US = "{C72C9E090CED8B05}PrefabsEditable/Mines/E_Mine_M14.et";
 static const ResourceName MINE_AT_US = "{B63D38376558C9DD}PrefabsEditable/Mines/E_Mine_M15AT.et";
 static const ResourceName MINE_AP_USSR = "{4F6673E18A5A5974}PrefabsEditable/Mines/E_Mine_PMN4.et";
 static const ResourceName MINE_AT_USSR = "{44DCCCB56D42C43B}PrefabsEditable/Mines/E_Mine_TM62M.et";
 static const ResourceName SIGN = "{BF4F9228DD61216B}PrefabsEditable/Auto/Structures/Signs/Military/SignMinefield_01/E_SignMinefield_01.et";

 static const float SIZE = 30;                 // m, a square turned along the road through it
 static const float ROAD_SNAP = 20;            // a road this close to the click runs through the field
 static const float AP_SPACING = 2.5;
 static const float AT_SPACING = 4;
 static const float DISARM_RADIUS = 25;        // Disarm mines: every armed mine this close to the click
 static const float DISARM_SECONDS_PER_MINE = 8;
 static const float ENGINEER_SPEED = 2;
 static const float MIN_BASE_DIST = 50;        // units deploy at a base's centre
 static const int MAX_MINES = 400;             // live mines per side
 static const float TRAVEL_SPEED = 1;          // m/s allowed for the walk, plus FOB_GIVE_UP_MS
 static float WORK_SCALE = 1;                  // tests shorten the work

 // ---------------------------------------------------------------- options

 static bool Enabled() { return ACR_LogisticsRules.Choice("acrMines", "1") != "0"; }
 static bool Realistic() { return ACR_Options.Flag("acrMinesRealistic"); }
 static bool AIUses() { return ACR_Options.Flag("acrMinesAI"); }

 // ---------------------------------------------------------------- orders

 static int Cost(ACR_EMineKind kind)
 {
  if (kind == ACR_EMineKind.AP) return 15;
  if (kind == ACR_EMineKind.AT) return 20;
  if (kind == ACR_EMineKind.MIXED) return 25;
  return 3;
 }

 static float Duration(ACR_EMineKind kind)
 {
  if (kind == ACR_EMineKind.AP) return 60;
  if (kind == ACR_EMineKind.AT) return 75;
  if (kind == ACR_EMineKind.MIXED) return 90;
  return 30;
 }

 static string Name(ACR_EMineKind kind)
 {
  if (kind == ACR_EMineKind.AP) return "AP minefield";
  if (kind == ACR_EMineKind.AT) return "AT minefield";
  if (kind == ACR_EMineKind.MIXED) return "mixed minefield";
  return "mine disarming";
 }

 // The order's button on the map panel.
 static string Button(ACR_EMineKind kind)
 {
  if (kind == ACR_EMineKind.AP) return "AP mines";
  if (kind == ACR_EMineKind.AT) return "AT mines";
  if (kind == ACR_EMineKind.MIXED) return "Mixed mines";
  return "Disarm mines";
 }

 // The map panel's hint while the order's cursor is armed.
 static string Hint(ACR_EMineKind kind)
 {
  if (kind == ACR_EMineKind.AP) return string.Format("Right-click: lay a %1 m AP minefield off the roads (%2 RP; click AP mines again to cancel)", (int)SIZE, Cost(kind));
  if (kind == ACR_EMineKind.AT) return string.Format("Right-click a road: AT mines in both lanes, across the area without one (%1 RP; click AT mines again to cancel)", Cost(kind));
  if (kind == ACR_EMineKind.MIXED) return string.Format("Right-click a road: AT mines on it, AP mines round it (%1 RP; click Mixed mines again to cancel)", Cost(kind));
  return string.Format("Right-click: disarm and lift every armed mine within %1 m (%2 RP; click Disarm mines again to cancel)", (int)DISARM_RADIUS, Cost(kind));
 }

 static ACR_EMineKind KindOf(int command)
 {
  if (command == ACR_ERequest.ORDER_MINES_AT) return ACR_EMineKind.AT;
  if (command == ACR_ERequest.ORDER_MINES_MIXED) return ACR_EMineKind.MIXED;
  if (command == ACR_ERequest.ORDER_DISARM_MINES) return ACR_EMineKind.DISARM;
  return ACR_EMineKind.AP;
 }

 // The order a minefield cursor mode arms (-1: not a minefield cursor).
 static int KindOfCursor(int cursorMode)
 {
  switch (cursorMode)
  {
   case ACR_ECursorMode.MINES_AP: return ACR_EMineKind.AP;
   case ACR_ECursorMode.MINES_AT: return ACR_EMineKind.AT;
   case ACR_ECursorMode.MINES_MIXED: return ACR_EMineKind.MIXED;
   case ACR_ECursorMode.DISARM_MINES: return ACR_EMineKind.DISARM;
  }
  return -1;
 }

 static bool IsRequest(int command)
 {
  return command == ACR_ERequest.ORDER_MINES_AP || command == ACR_ERequest.ORDER_MINES_AT || command == ACR_ERequest.ORDER_MINES_MIXED || command == ACR_ERequest.ORDER_DISARM_MINES;
 }

 // Infantry that could take the order (the map feed's flag for the panel buttons).
 static bool CanLay(ACR_Squad squad)
 {
  return Enabled() && squad && !squad.IsVehicle() && squad.CountAlive() > 0;
 }

 // Sappers / engineers, or a construction truck with the squad, work twice as fast.
 static float Speed(ACR_Squad squad)
 {
  if (ACR_SapperDemo.IsSapperSquad(squad) || squad.ConstructionTruck() != null) return ENGINEER_SPEED;
  return 1;
 }

 // The server side of the order (requests, AI commander, tests): plan the site, then the
 // squad pays and starts; like any new order it replaces the squad's current job (laying,
 // a FOB or roadblock build), which refunds itself. false + refusal, nothing charged, when it
 // can't.
 static bool Order(ACR_Squad squad, ACR_EMineKind kind, vector point, out string refusal)
 {
  ACR_MinePlan plan = Plan(squad, kind, point, refusal);
  if (!plan) return false;
  if (!squad.LayMinefield(plan))
  {
   refusal = "unable to start the work";
   return false;
  }
  return true;
 }

 static ACR_MinePlan Plan(ACR_Squad squad, ACR_EMineKind kind, vector point, out string refusal)
 {
  refusal = "";
  if (!Enabled())
  {
   refusal = "minefields are turned off for this match";
   return null;
  }
  if (!squad || squad.CountAlive() <= 0 || !squad.GetAnchor() || !squad.GetFaction())
  {
   refusal = "squad is unavailable";
   return null;
  }
  if (squad.IsVehicle())
  {
   refusal = "vehicles cannot lay or disarm mines";
   return null;
  }
  string faction = squad.GetFaction().GetFactionKey();
  int cost = Cost(kind);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || manager.m_Bases.GetRP(faction) < cost)
  {
   refusal = string.Format("not enough RP (%1 needed)", cost);
   return null;
  }
  point = ACR_Manager.Ground(point);
  if (BBB_Terrain.IsWaterAt(point))
  {
   refusal = "that spot is in water";
   return null;
  }
  vector from = squad.GetAnchor().GetOrigin();
  vector approach = point - from;
  approach[1] = 0;
  if (approach.Length() < 1) approach = "0 0 1";
  approach.Normalize();

  ACR_MinePlan plan = new ACR_MinePlan();
  plan.m_eKind = kind;
  plan.m_sFaction = faction;
  plan.m_iCost = cost;
  plan.m_vForward = approach;
  if (kind == ACR_EMineKind.DISARM)
  {
   plan.m_vSite = point;
   plan.m_vStage = ACR_Manager.Ground(point - approach * (DISARM_RADIUS * 0.6));
   plan.m_fRadius = DISARM_RADIUS + 15;
   plan.m_fDuration = Duration(kind) * WORK_SCALE;
  }
  else if (!PlanField(plan, point, refusal)) return null;
  if (BBB_Terrain.IsWaterAt(plan.m_vStage)) plan.m_vStage = plan.m_vSite;
  float now = GetGame().GetWorld().GetWorldTime();
  plan.m_fLastTick = now;
  plan.m_fLastProgress = now;
  plan.m_fDeadline = now + vector.DistanceXZ(from, plan.m_vStage) / TRAVEL_SPEED * 1000 + ACR_Squad.FOB_GIVE_UP_MS;
  return plan;
 }

 protected static bool PlanField(ACR_MinePlan plan, vector click, out string refusal)
 {
  float half = SIZE * 0.5;
  vector roadPoint, roadDir;
  float roadWidth;
  bool onRoad = Snap(click, ROAD_SNAP, roadPoint, roadDir, roadWidth);
  // The field lies along the road through it, else square to the squad's approach.
  vector approach = plan.m_vForward;
  if (onRoad) plan.m_vForward = roadDir;
  plan.m_vSite = click;
  if (onRoad && plan.m_eKind != ACR_EMineKind.AP) plan.m_vSite = roadPoint;

  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || vector.DistanceXZ(plan.m_vSite, base.GetOrigin()) >= MIN_BASE_DIST) continue;
   refusal = "too close to a base (units deploy there)";
   return false;
  }
  foreach (ACR_Minefield field : s_Fields)
  {
   if (field.m_sFaction != plan.m_sFaction || vector.DistanceXZ(field.m_vPos, plan.m_vSite) >= SIZE * 1.5) continue;
   refusal = "there is already a minefield of ours there";
   return false;
  }
  if (CountMines(plan.m_sFaction) >= MAX_MINES)
  {
   refusal = string.Format("our side already has %1 mines down", MAX_MINES);
   return false;
  }

  int at, ap;
  if (plan.m_eKind == ACR_EMineKind.AP) ap = 30;
  if (plan.m_eKind == ACR_EMineKind.AT) at = 12;
  if (plan.m_eKind == ACR_EMineKind.MIXED)
  {
   at = 8;
   ap = 20;
  }
  // AT: staggered in both lanes of the road (as many as fit); without one, across the area.
  // AP: off the roads.
  if (at > 0 && onRoad) RoadMines(plan, roadWidth, half, at);
  if (at > 0 && !onRoad) ScatterMines(plan, half, at, true, AT_SPACING, false);
  if (ap > 0) ScatterMines(plan, half, ap, false, AP_SPACING, true);
  if (plan.m_aPos.IsEmpty())
  {
   refusal = "no room for mines there (water or buildings)";
   return false;
  }
  // The squad works from the near edge, outside the field.
  plan.m_vStage = ACR_Manager.Ground(plan.m_vSite - approach * (half + 8));
  plan.m_fRadius = half + 24;
  // A field cut short by water or buildings takes less time.
  plan.m_fDuration = Duration(plan.m_eKind) * WORK_SCALE * Math.Max(0.25, plan.m_aPos.Count() / (float)(at + ap));
  return true;
 }

 // Alternate lanes every step: AT_SPACING or more between mines in the same lane.
 protected static void RoadMines(ACR_MinePlan plan, float roadWidth, float half, int count)
 {
  float step = Math.Max(AT_SPACING * 0.5, half * 2 / count);
  float lane = Math.Max(0.6, roadWidth * 0.25);
  float along = -half + step * 0.5;
  int placed;
  for (int i = 0; i < count * 2 && placed < count && along <= half; i++)
  {
   float across = lane;
   if (i % 2 == 1) across = -lane;
   vector guess = ToWorld(plan.m_vSite, plan.m_vForward, Vector(across, 0, along + Math.RandomFloat(-0.8, 0.8)));
   along += step;
   // Follow the road where it bends; past its end (no road there) no AT mine.
   vector onRoad, dir;
   float width;
   if (!Snap(guess, 8, onRoad, dir, width)) continue;
   guess = onRoad + Vector(dir[2], 0, -dir[0]) * Math.Clamp(across, -width * 0.3, width * 0.3);
   guess[1] = GetGame().GetWorld().GetSurfaceY(guess[0], guess[2]);
   if (!Usable(plan, guess, AT_SPACING * 0.6)) continue;
   plan.m_aPos.Insert(guess);
   plan.m_aAntiTank.Insert(true);
   placed++;
  }
 }

 // clearOfRoads: kept a metre clear of every road.
 protected static void ScatterMines(ACR_MinePlan plan, float half, int count, bool antiTank, float spacing, bool clearOfRoads)
 {
  int placed;
  for (int tries = 0; tries < count * 20 && placed < count; tries++)
  {
   vector pos = ToWorld(plan.m_vSite, plan.m_vForward, Vector(Math.RandomFloat(-half, half), 0, Math.RandomFloat(-half, half)));
   if (clearOfRoads && RoadClearance(pos) < 1) continue;
   if (!Usable(plan, pos, spacing)) continue;
   plan.m_aPos.Insert(pos);
   plan.m_aAntiTank.Insert(antiTank);
   placed++;
  }
 }

 // Dry, not inside a building or wall, and clear of the mines already planned.
 protected static bool Usable(ACR_MinePlan plan, vector pos, float spacing)
 {
  if (BBB_Terrain.IsWaterAt(pos)) return false;
  foreach (vector other : plan.m_aPos)
   if (vector.DistanceXZ(other, pos) < spacing) return false;
  s_bBlocked = false;
  GetGame().GetWorld().QueryEntitiesBySphere(pos, 1.0, CollectBlocker, null, EQueryEntitiesFlags.STATIC);
  return !s_bBlocked;
 }

 protected static bool s_bBlocked;
 protected static bool CollectBlocker(IEntity e)
 {
  if (!e || e.GetParent()) return true;
  if (SCR_DestructibleBuildingEntity.Cast(e) || BaseBuilding.Cast(e) || WallGeneratorEntity.Cast(e))
  {
   s_bBlocked = true;
   return false;
  }
  return true;
 }

 // ---------------------------------------------------------------- roads

 // point: on the nearest road's centre line; dir: its heading there (flat, unit).
 static bool Snap(vector near, float maxDistance, out vector point, out vector dir, out float width)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return false;
  BaseRoad road;
  float roadDistance;
  aiWorld.GetRoadNetworkManager().GetClosestRoad(near, road, roadDistance);
  if (!road || roadDistance > maxDistance) return false;
  array<vector> nodes = {};
  road.GetPoints(nodes);
  float best = float.MAX;
  vector bestPoint, bestDir;
  for (int i = 0; i + 1 < nodes.Count(); i++)
  {
   vector closest;
   if (!Project(near, nodes[i], nodes[i + 1], closest)) continue;
   float d = vector.DistanceXZ(closest, near);
   if (d >= best) continue;
   best = d;
   bestPoint = closest;
   bestDir = nodes[i + 1] - nodes[i];
  }
  if (best > maxDistance) return false;
  bestDir[1] = 0;
  bestDir.Normalize();
  point = bestPoint;
  point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
  dir = bestDir;
  width = RoadWidth(road);
  return true;
 }

 // Metres from p to the edge of the nearest road (negative: on it); float.MAX without one.
 static float RoadClearance(vector p)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return float.MAX;
  BaseRoad road;
  float distance;
  aiWorld.GetRoadNetworkManager().GetClosestRoad(p, road, distance);
  if (!road) return float.MAX;
  return distance - RoadWidth(road) * 0.5;
 }

 protected static float RoadWidth(BaseRoad road)
 {
  float width = road.GetWidth();
  if (width <= 0) width = 6;
  return width;
 }

 // The closest point to p on the segment a-b (flat); false for a degenerate segment.
 protected static bool Project(vector p, vector a, vector b, out vector closest)
 {
  float abx = b[0] - a[0];
  float abz = b[2] - a[2];
  float lengthSq = abx * abx + abz * abz;
  if (lengthSq < 0.0001) return false;
  float t = Math.Clamp(((p[0] - a[0]) * abx + (p[2] - a[2]) * abz) / lengthSq, 0, 1);
  closest = Vector(a[0] + abx * t, a[1] + (b[1] - a[1]) * t, a[2] + abz * t);
  return true;
 }

 // Local (x across, z along) to world, for a frame facing 'forward' at 'origin'.
 protected static vector ToWorld(vector origin, vector forward, vector local)
 {
  vector world = origin + Vector(forward[2], 0, -forward[0]) * local[0] + forward * local[2];
  world[1] = GetGame().GetWorld().GetSurfaceY(world[0], world[2]);
  return world;
 }

 // ---------------------------------------------------------------- laying

 // Mines go in as the work progresses: placed = planned x fraction. user: the soldier the
 // triggers credit with what they hit.
 static void Lay(ACR_MinePlan plan, float fraction, IEntity user)
 {
  int wanted = Math.ClampInt((int)Math.Floor(plan.m_aPos.Count() * fraction), 0, plan.m_aPos.Count());
  while (plan.m_iLaid < wanted)
  {
   ACR_Minefield field = FieldOf(plan);
   int index = plan.m_iLaid;
   plan.m_iLaid++;
   IEntity mine = SpawnMine(MineFor(plan.m_sFaction, plan.m_aAntiTank[index]), plan.m_aPos[index]);
   if (!mine) continue;
   Arm(mine, user);
   field.m_aMines.Insert(mine);
   s_Owner.Set(mine, plan.m_sFaction);
  }
 }

 // FIA and the east lay Soviet mines; everyone else American ones.
 static ResourceName MineFor(string faction, bool antiTank)
 {
  bool soviet = faction == "USSR" || faction.Contains("RU") || faction.Contains("AFRF") || faction.Contains("SLA") || faction.Contains("FIA");
  if (antiTank && soviet) return MINE_AT_USSR;
  if (antiTank) return MINE_AT_US;
  if (soviet) return MINE_AP_USSR;
  return MINE_AP_US;
 }

 // The order's field, made with its signs when the first mine goes in.
 protected static ACR_Minefield FieldOf(ACR_MinePlan plan)
 {
  if (plan.m_Field) return plan.m_Field;
  ACR_Minefield field = new ACR_Minefield();
  s_iNextId++;
  field.m_iId = s_iNextId;
  field.m_eKind = plan.m_eKind;
  field.m_sFaction = plan.m_sFaction;
  field.m_vPos = plan.m_vSite;
  field.m_bLaying = true;
  s_Fields.Insert(field);
  plan.m_Field = field;
  PlaceSigns(field, plan.m_vForward);
  PrintFormat("[ACR] MINEFIELD %1 %2 started at %3 (%4 mines planned)", field.m_iId, plan.m_sFaction, field.m_vPos, plan.m_aPos.Count());
  return field;
 }

 static IEntity SpawnMine(ResourceName prefab, vector pos)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return null;
  EntitySpawnParams spawnParams = new EntitySpawnParams();
  spawnParams.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(Math.RandomFloat(-180, 180), 0, 0), spawnParams.Transform);
  pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
  spawnParams.Transform[3] = pos;
  SCR_TerrainHelper.OrientToTerrain(spawnParams.Transform);
  return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
 }

 // The game's own arming, as when a soldier lays a mine (AiO's Lay a mine does the same).
 protected static void Arm(IEntity mine, IEntity user)
 {
  SCR_PressureTriggerComponent trigger = SCR_PressureTriggerComponent.Cast(mine.FindComponent(SCR_PressureTriggerComponent));
  if (!trigger) return;
  if (user) trigger.SetUser(user);
  if (!trigger.IsActivated()) trigger.ActivateTrigger();
 }

 // Minefield signs at the four corners, facing out.
 protected static void PlaceSigns(ACR_Minefield field, vector forward)
 {
  Resource resource = Resource.Load(SIGN);
  if (!resource.IsValid()) return;
  float edge = SIZE * 0.5 + 2;
  array<vector> corners = {};
  corners.Insert(Vector(edge, 0, edge));
  corners.Insert(Vector(-edge, 0, edge));
  corners.Insert(Vector(edge, 0, -edge));
  corners.Insert(Vector(-edge, 0, -edge));
  foreach (vector corner : corners)
  {
   vector pos = ToWorld(field.m_vPos, forward, corner);
   if (BBB_Terrain.IsWaterAt(pos)) continue;
   vector outward = pos - field.m_vPos;
   EntitySpawnParams spawnParams = new EntitySpawnParams();
   spawnParams.TransformMode = ETransformMode.WORLD;
   Math3D.AnglesToMatrix(Vector(Math.Atan2(outward[0], outward[2]) * Math.RAD2DEG, 0, 0), spawnParams.Transform);
   spawnParams.Transform[3] = pos;
   IEntity sign = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
   if (sign) field.m_aSigns.Insert(sign);
  }
 }

 // The order is over (done or called off): a field that never got a mine goes again.
 static void Finish(ACR_MinePlan plan)
 {
  ACR_Minefield field = plan.m_Field;
  if (!field) return;
  field.m_bLaying = false;
  if (field.m_aMines.IsEmpty()) RemoveField(field);
 }

 // RP back for an order called off: the share of the mines not laid, or the whole of a
 // Disarm mines order that has disarmed nothing yet.
 static int Refund(ACR_MinePlan plan)
 {
  if (plan.m_eKind == ACR_EMineKind.DISARM)
  {
   if (plan.m_iDisarmed > 0) return 0;
   return plan.m_iCost;
  }
  if (plan.m_aPos.IsEmpty()) return plan.m_iCost;
  return (int)Math.Round(plan.m_iCost * (plan.m_aPos.Count() - plan.m_iLaid) / (float)plan.m_aPos.Count());
 }

 // ---------------------------------------------------------------- disarming

 // Disarming work (seconds): every DISARM_SECONDS_PER_MINE switches off the armed mine nearest
 // the stage. true once none is left and the work took its time.
 static bool Disarm(ACR_MinePlan plan, float work)
 {
  plan.m_fCredit += work;
  float perMine = DISARM_SECONDS_PER_MINE * WORK_SCALE;
  IEntity mine = NearestMine(plan.m_vSite, DISARM_RADIUS, plan.m_vStage);
  if (mine && plan.m_fCredit >= perMine)
  {
   plan.m_fCredit -= perMine;
   DisarmMine(mine);
   plan.m_iDisarmed++;
   mine = NearestMine(plan.m_vSite, DISARM_RADIUS, plan.m_vStage);
  }
  if (mine) return false;
  plan.m_fCredit = Math.Min(plan.m_fCredit, perMine);
  return plan.m_fProgress >= plan.m_fDuration;
 }

 // Armed mines lying round a point (anyone's, Game Master's too), the one nearest 'from'.
 protected static ref array<IEntity> s_Found = {};
 static IEntity NearestMine(vector center, float radius, vector from)
 {
  s_Found.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(center, radius, CollectMine);
  IEntity best;
  float bestDistance = float.MAX;
  foreach (IEntity mine : s_Found)
  {
   float d = vector.DistanceXZ(mine.GetOrigin(), from);
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = mine;
  }
  s_Found.Clear();
  return best;
 }

 // Like AiO's: a mine item lying in the world (not in a kit) with its trigger armed.
 protected static bool CollectMine(IEntity e)
 {
  if (!e || e.GetParent() || !e.FindComponent(SCR_MineInventoryItemComponent)) return true;
  if (Armed(e)) s_Found.Insert(e);
  return true;
 }

 static bool Armed(IEntity mine)
 {
  SCR_PressureTriggerComponent trigger = SCR_PressureTriggerComponent.Cast(mine.FindComponent(SCR_PressureTriggerComponent));
  return trigger && trigger.IsActivated() && !trigger.WasTriggered();
 }

 // The game's own disarming, then the mine is lifted (deleted): players expect a cleared field
 // to be empty.
 static void DisarmMine(IEntity mine)
 {
  if (!mine) return;
  SCR_PressureTriggerComponent trigger = SCR_PressureTriggerComponent.Cast(mine.FindComponent(SCR_PressureTriggerComponent));
  if (trigger) trigger.DisarmTrigger();
  s_Owner.Remove(mine);
  foreach (ACR_Minefield field : s_Fields) field.m_aMines.RemoveItem(mine);
  SCR_EntityHelper.DeleteEntityAndChildren(mine);
 }

 // ---------------------------------------------------------------- friendly fire

 // A mine of ours touched by 'other': true = it stays quiet. The owning side's AI is spared
 // (it knows the lanes), its players are not, nobody is with -acrMinesRealistic 1.
 static bool Spares(IEntity mine, IEntity other)
 {
  if (!mine || !other || s_Owner.IsEmpty()) return false;
  string owner;
  if (!s_Owner.Find(mine, owner) || Realistic()) return false;
  IEntity root = other.GetRootParent();
  if (!root) root = other;
  ChimeraCharacter person = ChimeraCharacter.Cast(root);
  if (!person && Vehicle.Cast(root)) person = ACR_Squad.DriverOf(root);
  if (!person || ACR_Squad.FindController(person) != 0) return false;
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(person.FindComponent(FactionAffiliationComponent));
  return affiliation && affiliation.GetAffiliatedFaction() && affiliation.GetAffiliatedFaction().GetFactionKey() == owner;
 }

 // ---------------------------------------------------------------- upkeep

 // From ACR_Manager.Tick: every 5 s forget mines that went off, were disarmed (by anyone) or
 // are gone; a finished field with none left goes with its signs.
 static void Tick()
 {
  s_iTicks++;
  if (s_iTicks % 5 != 0 || s_Fields.IsEmpty()) return;
  for (int f = s_Fields.Count() - 1; f >= 0; f--)
  {
   ACR_Minefield field = s_Fields[f];
   for (int m = field.m_aMines.Count() - 1; m >= 0; m--)
   {
    IEntity mine = field.m_aMines[m];
    if (mine && Armed(mine)) continue;
    if (mine) s_Owner.Remove(mine);
    field.m_aMines.RemoveOrdered(m);
   }
   if (field.m_bLaying || !field.m_aMines.IsEmpty()) continue;
   PrintFormat("[ACR] MINEFIELD %1 %2 is gone (every mine went off or was disarmed)", field.m_iId, field.m_sFaction);
   RemoveField(field);
  }
  // Deleted mines leave empty keys behind: rebuild the lookup from the live fields.
  if (s_Owner.Count() <= CountMines("")) return;
  s_Owner.Clear();
  foreach (ACR_Minefield live : s_Fields)
  {
   foreach (IEntity liveMine : live.m_aMines) s_Owner.Set(liveMine, live.m_sFaction);
  }
 }

 // A mine that went off.
 static bool Spent(IEntity mine)
 {
  BaseTriggerComponent trigger = BaseTriggerComponent.Cast(mine.FindComponent(BaseTriggerComponent));
  return trigger && trigger.WasTriggered();
 }

 static void RemoveField(ACR_Minefield field)
 {
  foreach (IEntity sign : field.m_aSigns) if (sign) SCR_EntityHelper.DeleteEntityAndChildren(sign);
  field.m_aSigns.Clear();
  foreach (IEntity mine : field.m_aMines) if (mine) s_Owner.Remove(mine);
  s_Fields.RemoveItem(field);
 }

 // Live mines of a side ("" = every side).
 static int CountMines(string faction)
 {
  int count;
  foreach (ACR_Minefield field : s_Fields)
   if (faction.IsEmpty() || field.m_sFaction == faction) count += field.m_aMines.Count();
  return count;
 }

 static int CountFields(string faction)
 {
  int count;
  foreach (ACR_Minefield field : s_Fields) if (field.m_sFaction == faction) count++;
  return count;
 }

 // The owning side's command map: a red area per field.
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  foreach (ACR_Minefield field : s_Fields)
  {
   if (field.m_sFaction != faction || field.m_aMines.IsEmpty()) continue;
   positions.Insert(field.m_vPos);
   radii.Insert(SIZE * 0.7);
   labels.Insert(string.Format("%1 (%2)", Name(field.m_eKind), field.m_aMines.Count()));
  }
 }

 static void Reset()
 {
  s_Fields.Clear();
  s_Owner.Clear();
  s_iNextId = 0;
  s_iTicks = 0;
  WORK_SCALE = 1;
 }
}

// The owning side's AI never sets off its own mines (ACR_Minefields.Spares). AP mines reach
// this through SCR_AntiPersonnelMineCollisionHandlerComponent, AT mines through physics
// contacts; both before the game's weight check.
modded class SCR_PressureTriggerComponent
{
 override void EOnContact(IEntity owner, IEntity other, Contact contact)
 {
  if (ACR_Minefields.Spares(owner, other)) return;
  super.EOnContact(owner, other, contact);
 }
}
