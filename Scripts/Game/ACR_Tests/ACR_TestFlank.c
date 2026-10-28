// Actual native squad movement plus cancellation/restore guards. Contact-trigger
// decisions are exercised separately in a real firefight; Begin here receives a
// fixed threat bearing so a sensor acquisition delay cannot hide movement bugs.
class ACR_TestFlank
{
 static ref ACR_Squad s_Squad;
 static vector s_Start, s_Objective, s_Direction;
 static ref ACR_FlankLane s_Lane;
 static bool s_bBegan;
 static float s_fLateral;
 static SCR_AIGroup s_Enemy;
 static int s_iAmmo, s_iStarted, s_iShots;
 static ref map<BaseWeaponComponent, int> s_Ammo = new map<BaseWeaponComponent, int>();
 static bool s_bFresh, s_bAutomatic, s_bBBB;
 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  if (tick == 5)
  {
   s_Squad = h.Squad();
   s_Lane = new ACR_FlankLane();
   if (!s_Squad || !s_Lane.Find(s_Start, s_Direction)) { ACR_TestRegress.Abort("flank", "no clear lane/squad"); return; }
   foreach (ACR_Squad unrelated : manager.m_Squads)
    if (unrelated != s_Squad && unrelated.m_Group != h.m_Range.m_Bravo && unrelated.m_Group) SCR_EntityHelper.DeleteEntityAndChildren(unrelated.m_Group);
   array<AIAgent> members = {};
   s_Squad.GetAgents(members);
   foreach (int i, AIAgent member : members)
   {
    IEntity body = member.GetControlledEntity();
    if (body) body.SetOrigin(ACR_Manager.Ground(s_Start + Vector(i * 2, 0, 0)));
   }
   s_Squad.SetMode(ACR_EMoveMode.COMBAT);
   s_Squad.MoveTo(ACR_Manager.Ground(s_Start + s_Direction * 180));
  }
  if (tick == 10)
  {
   s_Squad = h.Squad();
   if (!s_Squad || !s_Squad.GetAnchor()) { ACR_TestRegress.Abort("flank", "no infantry squad"); return; }
   s_Start = s_Squad.GetAnchor().GetOrigin();
   s_Objective = ACR_Manager.Ground(s_Start + s_Direction * 180);
   s_Squad.SetMode(ACR_EMoveMode.COMBAT);
   s_Squad.MoveTo(s_Objective);
   ACR_TestRegress.Check("combat infantry eligible for flanking", ACR_CombatFlank.Eligible(s_Squad));
   s_Squad.SetMode(ACR_EMoveMode.SILENT);
   ACR_TestRegress.Check("silent movement excluded", !ACR_CombatFlank.Eligible(s_Squad));
   s_Squad.SetMode(ACR_EMoveMode.FAST);
   ACR_TestRegress.Check("explicit fast movement excluded", !ACR_CombatFlank.Eligible(s_Squad));
   s_Squad.SetMode(ACR_EMoveMode.COMBAT);
   s_Squad.Defend(s_Start);
   ACR_TestRegress.Check("defence excluded", !ACR_CombatFlank.Eligible(s_Squad));
   s_Squad.Suppress(s_Objective);
   ACR_TestRegress.Check("base of fire excluded", !ACR_CombatFlank.Eligible(s_Squad));
   s_Squad.MoveTo(s_Objective);
   s_Squad.Queue(s_Objective + Vector(0, 0, -100), false);
   s_bBegan = s_Squad.m_CombatFlank.Begin(s_Squad, s_Objective, GetGame().GetWorld().GetWorldTime());
  }
  if (tick == 15 && s_Squad)
  {
   if (!s_bBegan) s_bBegan = s_Squad.m_CombatFlank.Begin(s_Squad, s_Objective, GetGame().GetWorld().GetWorldTime());
   ACR_TestRegress.Check("native flank waypoint issued", s_bBegan && s_Squad.m_bForced, s_Squad.m_MoveTarget.ToString());
   ACR_TestRegress.Check("snapshot retains original objective during manoeuvre", s_bBegan && vector.DistanceXZ(s_Squad.SnapshotOrder().m_Target, s_Objective) < 1);
   ACR_Squad cover = manager.FindSquadByGroup(h.m_Range.m_Bravo);
   bool keptCover;
   if (cover && cover.GetAnchor())
   {
    cover.SetMode(ACR_EMoveMode.COMBAT);
    cover.MoveTo(cover.GetAnchor().GetOrigin() + Vector(0, 0, 100));
    keptCover = ACR_CombatFlank.SectorBusy(cover, s_Objective) && !cover.m_CombatFlank.Begin(cover, s_Objective, GetGame().GetWorld().GetWorldTime());
    cover.Halt("Holding");
   }
   ACR_TestRegress.Check("second squad cannot leave the same firing sector simultaneously", keptCover);
  }
  if (tick > 15 && tick < 50 && s_Squad && s_Squad.GetAnchor())
   s_fLateral = Math.Max(s_fLateral, Math.AbsFloat(vector.Dot(s_Squad.GetAnchor().GetOrigin() - s_Start, Vector(-s_Direction[2], 0, s_Direction[0]))));
  if (tick == 30 && s_Squad)
   ACR_TestRegress.Check("short forced departure expires", !s_Squad.m_bForced);
  if (tick == 50 && s_Squad)
  {
   ACR_TestRegress.Check("native AI actually moved laterally", s_fLateral > 10, s_fLateral.ToString());
   array<vector> points = {};
   array<int> kinds = {};
   s_Squad.GetPlannedPoints(points, kinds);
   ACR_TestRegress.Check("queued destination survives flanking", points.Contains(s_Objective + Vector(0, 0, -100)));
   if (s_Squad.m_CombatFlank.Active()) s_Squad.m_CombatFlank.Tick(s_Squad, GetGame().GetWorld().GetWorldTime() + 65000);
   ACR_TestRegress.Check("flank completion restores original objective", !s_Squad.m_CombatFlank.Active() && vector.DistanceXZ(s_Squad.m_MoveTarget, s_Objective) < 1 && s_Squad.OrderState() == ACR_EOrderState.MOVE && !s_Squad.m_bForced);
   bool restarted = s_Squad.m_CombatFlank.Begin(s_Squad, s_Objective, GetGame().GetWorld().GetWorldTime() + 90001);
   s_Squad.Halt("Holding");
   s_Squad.m_CombatFlank.Tick(s_Squad, GetGame().GetWorld().GetWorldTime() + 70000);
   ACR_TestRegress.Check("Halt cancels manoeuvre without restoring old orders", restarted && !s_Squad.m_CombatFlank.Active() && !s_Squad.m_Group.GetCurrentWaypoint() && s_Squad.OrderState() == ACR_EOrderState.HOLD);
   s_Squad.MoveTo(s_Objective);
   bool again = s_Squad.m_CombatFlank.Begin(s_Squad, s_Objective, GetGame().GetWorld().GetWorldTime() + 180002);
   vector replacement = ACR_Manager.Ground(s_Start + Vector(0, 0, 90));
   s_Squad.MoveTo(replacement);
   s_Squad.m_CombatFlank.Tick(s_Squad, GetGame().GetWorld().GetWorldTime() + 70000);
   ACR_TestRegress.Check("new order cancels manoeuvre and keeps new destination", again && !s_Squad.m_CombatFlank.Active() && vector.DistanceXZ(s_Squad.m_MoveTarget, replacement) < 1);
   bool playerJoin = s_Squad.m_CombatFlank.Begin(s_Squad, s_Objective, GetGame().GetWorld().GetWorldTime() + 270003);
   s_Squad.m_CombatFlank.YieldToPlayer(s_Squad);
   ACR_TestRegress.Check("player joining ends our forced detour and restores objective", playerJoin && !s_Squad.m_CombatFlank.Active() && !s_Squad.m_bForced && vector.DistanceXZ(s_Squad.m_MoveTarget, replacement) < 1);
   bool nativeJoin = s_Squad.m_CombatFlank.Begin(s_Squad, s_Objective, GetGame().GetWorld().GetWorldTime() + 360004);
   AIWaypoint nativeOrder = AIWaypoint.Cast(ACR_Manager.SpawnPrefab(ACR_Squad.WP_MOVE, replacement));
   if (nativeOrder)
   {
    s_Squad.m_Group.RemoveWaypoint(s_Squad.m_Waypoint);
    s_Squad.m_Group.AddWaypoint(nativeOrder);
   }
   s_Squad.m_CombatFlank.YieldToPlayer(s_Squad);
   ACR_TestRegress.Check("a native replacement waypoint survives player handover", nativeJoin && nativeOrder && !s_Squad.m_CombatFlank.Active() && s_Squad.m_Group.GetCurrentWaypoint() == nativeOrder);
   s_Squad.Halt("Holding");
   if (nativeOrder) SCR_EntityHelper.DeleteEntityAndChildren(nativeOrder);
   // A live sensor/firing phase: the enemy is armed but holds fire, so no casualty or
   // suppression randomness decides who takes the contact (BBB, not ACA's flank: #54).
   array<AIAgent> fighters = {};
   s_Squad.GetAgents(fighters);
   foreach (int n, AIAgent fighter : fighters)
   {
    ChimeraCharacter man = ChimeraCharacter.Cast(fighter.GetControlledEntity());
    if (!man) continue;
    man.SetOrigin(ACR_Manager.Ground(s_Start + Vector(n * 2, 0, 0)));
    man.SetAngles(s_Direction.VectorToAngles());
    man.GetDamageManager().EnableDamageHandling(false);
   }
   s_Squad.m_CombatFlank = new ACR_CombatFlank();
   s_Squad.MoveTo(s_Objective);
   s_Squad.m_Group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
   s_Enemy = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab("{657590C1EC9E27D3}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et", ACR_Manager.Ground(s_Start + s_Direction * 100)));
   if (s_Enemy && s_Enemy.GetGroupUtilityComponent()) s_Enemy.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
   s_iStarted = s_Squad.m_CombatFlank.m_iStarted;
   s_iAmmo = Ammo(s_Squad);
  }
  if (tick == 55 && s_Enemy)
  {
   s_Enemy.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
   array<AIAgent> enemies = {};
   s_Enemy.GetAgents(enemies);
   foreach (AIAgent enemy : enemies)
   {
    ChimeraCharacter target = ChimeraCharacter.Cast(enemy.GetControlledEntity());
    if (target) target.GetDamageManager().EnableDamageHandling(false);
   }
  }
  if (tick > 50 && tick < 110 && s_Squad)
  {
   vector observed;
   bool pinned;
   if (ACR_CombatFlank.Contact(s_Squad, observed, pinned)) s_bFresh = true;
   if (s_Squad.m_CombatFlank.m_iStarted > s_iStarted) s_bAutomatic = true;
   // Contact is BBB's (#54): its fire and manoeuvre takes the squad, not ACA's flank.
   if (BBB_FireManeuver.Job(s_Squad.m_Group)) s_bBBB = true;
   if (tick > 65) Ammo(s_Squad);
  }
  if (tick == 65 && s_Squad)
  {
   s_Squad.m_Group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.FIRE_AT_WILL);
  }
  if (tick == 110)
  {
   ACR_TestRegress.Check("live native sensors supply fresh infantry contact", s_bFresh);
   ACR_TestRegress.Check("contact is BBB's: its fire and manoeuvre takes the squad (#54)", s_bBBB);
   ACR_TestRegress.Check("ACA's automatic flank of a stalled contact stays off (#54)", !s_bAutomatic);
   ACR_TestRegress.Check("native rifle fire continues in combat fixture", s_iShots > 0, s_iShots.ToString());
   if (s_Enemy) SCR_EntityHelper.DeleteEntityAndChildren(s_Enemy);
   if (s_Squad) s_Squad.Halt("Holding");
   ACR_TestRegress.Complete("flank", 20);
  }
 }
 static int Ammo(ACR_Squad squad)
 {
  int rounds;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (!utility || !utility.m_CombatComponent) continue;
   BaseWeaponComponent weapon = utility.m_CombatComponent.GetCurrentWeapon();
   if (weapon && weapon.GetCurrentMagazine())
   {
    int remaining = weapon.GetCurrentMagazine().GetAmmoCount();
    int previous;
    if (s_Ammo.Find(weapon, previous) && previous > remaining) s_iShots += previous - remaining;
    s_Ammo.Set(weapon, remaining);
    rounds += remaining;
   }
  }
  return rounds;
 }
}

class ACR_FlankLane : BBB_EngagementTest
{
 protected override bool LaneClear(vector from, vector dir)
 {
  if (!super.LaneClear(from, dir)) return false;
  vector lateral = Vector(-dir[2], 0, dir[0]);
  array<float> sides = {-1.0, 1.0};
  foreach (float side : sides)
  {
   vector point = from + lateral * (85 * side) + dir * 45;
   if (BBB_Terrain.IsWaterAt(point) || BBB_Terrain.WaterAlong(from, point, 5) > 0) return false;
   if (Math.AbsFloat(BBB.Ground(point)[1] - BBB.Ground(from)[1]) > 5) return false;
   if (!Visible(from, point)) return false;
  }
  return true;
 }
 bool Find(out vector start, out vector direction)
 {
  if (!FindSite()) return false;
  start = BBB.Ground(m_aSiteFrom[0]);
  direction = m_aSiteDir[0];
  NavmeshWorldComponent mesh = GetGame().GetAIWorld().GetNavmeshWorldComponent("Soldiers");
  if (!mesh) return false;
  for (int x = -2; x <= 2; x++)
   for (int z = -2; z <= 2; z++) mesh.LoadTileIn(start + Vector(x * 50, 0, z * 50));
  return true;
 }
}
