// How AC: Actual plugs into Better Battle Buddies (Scripts/Game/BBB): BBB looks
// after our squads' groups, names them as we do, knows their own vehicles, leaves
// gunner-driven vehicles alone and hands dropped launchers to our cleanup.
class ACR_BBBHooks : BBB_Hooks
{
 protected ACR_Manager m_Manager;

 void ACR_BBBHooks(ACR_Manager manager) { m_Manager = manager; }

 protected ACR_Squad SquadOf(SCR_AIGroup group) { return m_Manager.FindSquadByGroup(group); }

 override void GetGroups(notnull array<SCR_AIGroup> groups)
 {
  groups.Clear();
  foreach (ACR_Squad squad : m_Manager.m_Squads)
   if (squad.m_Group) groups.Insert(squad.m_Group);
 }

 // Our roster includes a leader a player commands through (he sits in the command group).
 override void GetMembers(SCR_AIGroup group, notnull array<AIAgent> agents)
 {
  ACR_Squad squad = SquadOf(group);
  if (squad) squad.GetAgents(agents);
  else super.GetMembers(group, agents);
 }

 override string NameOf(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (squad) return squad.GetName();
  return super.NameOf(group);
 }

 override IEntity AnchorOf(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (squad) return squad.GetAnchor();
  return super.AnchorOf(group);
 }

 override IEntity OwnVehicleOf(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (squad) return squad.m_OwnVehicle;
  return null;
 }

 override bool AllowFireManeuver(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (!squad || ACR_AirLift.Busy(squad) || squad.m_Mode != ACR_EMoveMode.COMBAT || squad.m_Commander != 0 || squad.m_CombatFlank.Active()) return false;
  ACR_EOrderState state = squad.OrderState();
  if (state == ACR_EOrderState.MOVE || state == ACR_EOrderState.ASSAULT || state == ACR_EOrderState.HUNT) return true;
  // Defenders bound to cover or push back an enemy inside their post, never out of it (#31).
  vector centre;
  float radius;
  return DefenderLeash(squad, centre, radius);
 }

 // ---- Squad tactics inside the objective (#31, Jacob 26 Oct). ACA's attack logic picks the
 // target, stages the squads and sets the base of fire; once a squad on its ASSAULT order is
 // inside the target's ring, BBB's squad tactics (assault, flank, hold, search) and fire and
 // manoeuvre do the small-unit work there, leashed to the ring. Defenders on a Defend order at
 // their post get fire and manoeuvre (and the tactics they had) leashed to the post's radius.
 // No tactics while staging or riding. Off by default since the 27 Oct Everon A/B showed no
 // gain (BBB #9): -acrSquadTactics 1 turns it on; off is the old policy.
 // Asked many times a tick (BBB's leash checks): the option is read again every 5 s.
 protected static bool s_bSquadTactics;
 protected static float s_fSquadTacticsRead = -1;
 static bool SquadTacticsOn()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (s_fSquadTacticsRead < 0 || now - s_fSquadTacticsRead > 5000 || now < s_fSquadTacticsRead)
  {
   s_fSquadTacticsRead = now;
   s_bSquadTactics = ACR_LogisticsRules.Choice("acrSquadTactics", "0") != "0";
  }
  return s_bSquadTactics;
 }
 static void RereadSquadTactics() { s_fSquadTacticsRead = -1; }

 // An attacker in the objective: on foot, assaulting, inside the target's ring.
 static bool AttackerLeash(ACR_Squad squad, out vector centre, out float radius)
 {
  if (!SquadTacticsOn() || !squad || squad.m_iAITask != ACR_EAITask.ATTACK || !squad.m_AITarget) return false;
  if (squad.HasPlayer() || squad.m_Commander != 0 || squad.m_bForced || squad.IsMounted() || ACR_Transport.CrewedVehicle(squad) || ACR_AirLift.Busy(squad)) return false;
  if (squad.OrderState() != ACR_EOrderState.ASSAULT || !squad.GetAnchor()) return false;
  centre = squad.m_AITarget.GetOrigin();
  radius = squad.m_AITarget.m_fRadius;
  return vector.DistanceXZ(squad.GetAnchor().GetOrigin(), centre) <= radius;
 }

 // A defender at its post: defending or posted at a base, on a Defend order, inside the base.
 static bool DefenderLeash(ACR_Squad squad, out vector centre, out float radius)
 {
  if (!SquadTacticsOn() || !squad || !squad.m_AITarget || !squad.m_Group) return false;
  if (squad.m_iAITask != ACR_EAITask.DEFEND && squad.m_iAITask != ACR_EAITask.POSTED) return false;
  if (squad.HasPlayer() || squad.m_Commander != 0 || squad.m_bForced || squad.IsMounted() || ACR_Transport.CrewedVehicle(squad) || ACR_AirLift.Busy(squad)) return false;
  if (squad.OrderState() != ACR_EOrderState.DEFEND || !squad.GetAnchor()) return false;
  // Only the plain Defend waypoint: garrisons stay in their buildings.
  AIWaypoint waypoint = squad.m_Group.GetCurrentWaypoint();
  if (!waypoint || !waypoint.GetPrefabData() || !waypoint.GetPrefabData().GetPrefabName().Contains("AIWaypoint_Defend")) return false;
  centre = squad.m_AITarget.GetOrigin();
  radius = Math.Max(squad.m_AITarget.m_fRadius, BBB_OrderLeash.DEFEND_RADIUS);
  return vector.DistanceXZ(squad.GetAnchor().GetOrigin(), centre) <= radius;
 }

 override bool HostLeash(SCR_AIGroup group, out vector centre, out float radius)
 {
  ACR_Squad squad = SquadOf(group);
  if (!squad) return false;
  return AttackerLeash(squad, centre, radius) || DefenderLeash(squad, centre, radius);
 }

 override bool LeaveAlone(IEntity vehicle) { return m_Manager.IsGunnerDriven(vehicle); }

 override bool AllowRelaxedMarch(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  return squad && !ACR_AirLift.Busy(squad) && squad.OrderState() == ACR_EOrderState.MOVE && !squad.m_CombatFlank.Active();
 }

 // BBB may react locally to ordinary autonomous foot orders. Player orders,
 // quiet/forced movement, road columns and transport remain host-owned.
 override bool IsBinding(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (!squad) return false;
  if (ACR_AirLift.Busy(squad)) return true;
  if (squad.m_Commander != 0 || squad.m_iOrderedBy != 0 || squad.m_bForced || squad.m_Mode != ACR_EMoveMode.COMBAT || squad.m_CombatFlank.Active()) return true;
  ACR_EOrderState state = squad.OrderState();
  // Riders and crews in any order stay host-owned: ACA's stall/unstick drives its vehicles,
  // so BBB armour escapes must not back a defending or assaulting tank out on their own.
  if (squad.IsMounted() || ACR_Transport.CrewedVehicle(squad)) return true;
  return state != ACR_EOrderState.MOVE && state != ACR_EOrderState.ASSAULT && state != ACR_EOrderState.HUNT && state != ACR_EOrderState.DEFEND && state != ACR_EOrderState.HOLD && state != ACR_EOrderState.SUPPRESS;
 }

 override bool ManagesVehicleDriving(SCR_AIGroup group)
 {
  return SquadOf(group) != null;
 }

 // Illumination is allowed for autonomous combat squads, never player command,
 // quiet travel, transport, regroup or a host-owned flanking maneuver.
 override bool AllowFlares(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (!squad) return true;
  if (ACR_AirLift.Busy(squad)) return false;
  if (squad.m_Commander != 0 || squad.m_Mode != ACR_EMoveMode.COMBAT || squad.m_CombatFlank.Active()) return false;
  ACR_EOrderState state = squad.OrderState();
  return state == ACR_EOrderState.DEFEND || state == ACR_EOrderState.ASSAULT || state == ACR_EOrderState.SUPPRESS || state == ACR_EOrderState.HUNT;
 }
 // ACA's native artillery, roadblock and fortified-position defence own these static-weapon
 // jobs. BBB's opportunistic quiet-time release must not take their gunners away.
 override bool AllowStaticWeapons(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  return !squad || (!ACR_AirLift.Busy(squad) && !squad.m_bMortar && !squad.m_ActiveRoadblock && !squad.m_Fortification);
 }

 // Gunners stay on the static guns at the spot a squad holds (ACR_GunHold; BBB_HeldGuns keeps
 // them there and mans an empty one again).
 override void GetHeldGuns(SCR_AIGroup group, notnull array<IEntity> guns)
 {
  guns.Clear();
  ACR_Squad squad = SquadOf(group);
  if (squad) ACR_GunHold.HeldGuns(squad, guns);
  // A position's gun crew from its reserve (its squad away on another job).
  else ACR_Defenders.CrewGunsOf(group, guns);
 }

 // Radio contact reports (BBB_ContactRelay) can make vanilla send fireteams to look. Only
 // squads waiting in place take them: free, posted or defending, on foot, not under a player's
 // own command. Movers, riders, attackers, builders and squads walking back to refit keep
 // ACA's plan.
 override bool AllowContactRelay(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (!squad) return true;
  if (ACR_AirLift.Busy(squad) || squad.m_Commander != 0 || squad.m_bForced || squad.m_Mode != ACR_EMoveMode.COMBAT) return false;
  if (squad.IsMounted() || ACR_Transport.CrewedVehicle(squad) || squad.m_CombatFlank.Active()) return false;
  int task = squad.m_iAITask;
  if (task != ACR_EAITask.FREE && task != ACR_EAITask.POSTED && task != ACR_EAITask.DEFEND) return false;
  ACR_EOrderState state = squad.OrderState();
  return state == ACR_EOrderState.NONE || state == ACR_EOrderState.HOLD || state == ACR_EOrderState.DEFEND;
 }

 // BBB's committed squad tactic (contact pause, assault, hold, reorganise) for the same waiting
 // squads as contact relay, and for attackers inside the objective (#31): its HOLD stops
 // advances, so ACA's movers, stagers and flankers keep their plan.
 override bool AllowSquadTactic(SCR_AIGroup group)
 {
  if (AllowContactRelay(group)) return true;
  vector centre;
  float radius;
  return AttackerLeash(SquadOf(group), centre, radius);
 }

 override bool AllowMorale(SCR_AIGroup group)
 {
  return SquadOf(group) == null;
 }

 override void OnLitter(IEntity item)
 {
  if (m_Manager.m_Cleanup) m_Manager.m_Cleanup.AddLitter(item);
 }
}

// A squad's end-of-unstick callback (BBB_Unstick tells it when backing out is over).
class ACR_SquadUnstick : BBB_UnstickListener
{
 protected ACR_Squad m_Squad;
 void ACR_SquadUnstick(ACR_Squad squad) { m_Squad = squad; }
 override void OnUnstickEnded(IEntity vehicle) { if (m_Squad) m_Squad.EndUnstick(); }
}
