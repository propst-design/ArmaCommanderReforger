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
  return FortLeash(squad, centre, radius) || AttackerLeash(squad, centre, radius) || DefenderLeash(squad, centre, radius);
 }

 // A squad holding a fortified position of ours: BBB's own moves (fire and manoeuvre, answering
 // incoming fire) stay inside its hold behind the line, not out past the wire (#93).
 static bool FortLeash(ACR_Squad squad, out vector centre, out float radius)
 {
  if (!squad || !squad.m_Fortification || squad.HasPlayer() || squad.m_Commander != 0) return false;
  if (squad.OrderState() != ACR_EOrderState.DEFEND) return false;
  centre = ACR_Fortify.HoldCentre(squad.m_Fortification);
  radius = ACR_Fortify.HOLDER_RADIUS;
  return true;
 }

 override bool LeaveAlone(IEntity vehicle) { return m_Manager.IsGunnerDriven(vehicle); }

 override bool AllowRelaxedMarch(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  return squad && !ACR_AirLift.Busy(squad) && squad.OrderState() == ACR_EOrderState.MOVE && !squad.m_CombatFlank.Active();
 }

 // Does the squad's move mode keep it ours? Quiet moves always. A FAST march in contact has its
 // order re-issued at combat level (m_bForced cleared) and is fighting: BBB's with squad tactics
 // on (#101); with them off, any non-combat mode stays ours as before.
 protected bool HostMode(ACR_Squad squad)
 {
  if (squad.m_Mode == ACR_EMoveMode.COMBAT) return false;
  return squad.m_Mode == ACR_EMoveMode.SILENT || !SquadTacticsOn();
 }

 // BBB may react locally to ordinary autonomous foot orders. Player orders,
 // quiet/forced movement, road columns and transport remain host-owned.
 override bool IsBinding(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (!squad) return false;
  if (ACR_AirLift.Busy(squad)) return true;
  if (squad.m_Commander != 0 || squad.m_iOrderedBy != 0 || squad.m_bForced || HostMode(squad) || squad.m_CombatFlank.Active()) return true;
  ACR_EOrderState state = squad.OrderState();
  // Riders and crews in any order stay host-owned: ACA's stall/unstick drives its vehicles,
  // so BBB armour escapes must not back a defending or assaulting tank out on their own.
  if (squad.IsMounted() || ACR_Transport.CrewedVehicle(squad)) return true;
  return state != ACR_EOrderState.MOVE && state != ACR_EOrderState.ASSAULT && state != ACR_EOrderState.HUNT && state != ACR_EOrderState.DEFEND && state != ACR_EOrderState.HOLD && state != ACR_EOrderState.SUPPRESS;
 }

 // AdoptWaypoint runs every Fast order at the player's level so it outranks combat. When that
 // order is ACA's own waypoint (AI commander or the commander screen, not a squad a player
 // commands natively and not a GM waypoint), BBB keeps armed crews aboard for gates and heals (#57).
 override bool HostOrder(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (!squad || !squad.m_Waypoint || squad.m_Commander != 0 || BBB.HasPlayer(group)) return false;
  return squad.IsOrderWaypoint(group.GetCurrentWaypoint());
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

 // ---- The fight handover (#101, with BBB #15). ACA decides where a squad goes and why; BBB how
 // it fights on the way, under one intent per squad:
 //   PUSH     moving, assaulting or hunting: assault and flank inside the order's corridor (an
 //            attacker inside the objective: its ring); holds only outnumbered or flanked, and
 //            not for long. Forming up counts: it was already left to fire and manoeuvre (#54).
 //   HOLD     defending, holding, suppressing, or waiting at a post: fight inside the post.
 //   TRANSIT  riding, airlift, forced or quiet moves, ACA's own flank, transport and other jobs.
 //   NONE     a player's squad, or one a player commands or ordered.
 // With -acrSquadTactics 0 (the default until the A/B says otherwise) BBB's squad tactics stay
 // off for our squads and fire and manoeuvre keeps the old policy: PUSH for movers, attackers
 // and hunters, HOLD only for a defender at its post; waiting squads (which only ever held)
 // get neither. ACA reads back what BBB is doing in its attacks (ACR_AICommander.NoteBBBFight).
 override BBB_EIntent Intent(SCR_AIGroup group, out vector centre, out float radius)
 {
  radius = 0;
  ACR_Squad squad = SquadOf(group);
  if (!squad) return super.Intent(group, centre, radius);
  string why;
  BBB_EIntent intent = IntentOf(squad, centre, radius, why);
  // With BBB's -bbbTacticStats 1 (soaks): each squad's intent when it changes, and why.
  if (BBB_Tactic.s_bStats)
  {
   string seen;
   string now = string.Format("%1 (%2)", typename.EnumToString(BBB_EIntent, intent), why);
   if (!s_mIntentSeen.Find(group, seen) || seen != now)
   {
    s_mIntentSeen.Set(group, now);
    PrintFormat("[ACR] BBB INTENT %1: %2", squad.GetName(), now);
   }
  }
  return intent;
 }
 protected static ref map<SCR_AIGroup, string> s_mIntentSeen = new map<SCR_AIGroup, string>();
 static void ResetIntents() { s_mIntentSeen.Clear(); }

 protected BBB_EIntent IntentOf(ACR_Squad squad, out vector centre, out float radius, out string why)
 {
  radius = 0;
  if (squad.HasPlayer() || squad.m_Commander != 0 || squad.m_iOrderedBy != 0) { why = "player"; return BBB_EIntent.NONE; }
  if (ACR_AirLift.Busy(squad)) { why = "airlift"; return BBB_EIntent.TRANSIT; }
  if (squad.m_bForced) { why = "forced"; return BBB_EIntent.TRANSIT; }
  // A FAST march that met the enemy is re-issued at combat level (m_bForced cleared, "fighting its
  // way forward") but keeps its FAST label: that squad is fighting. Only a quiet move stays ours
  // (and, with squad tactics off, any non-combat mode, as before).
  if (HostMode(squad)) { why = "mode " + typename.EnumToString(ACR_EMoveMode, squad.m_Mode); return BBB_EIntent.TRANSIT; }
  if (squad.m_CombatFlank.Active()) { why = "ACA flank"; return BBB_EIntent.TRANSIT; }
  if (squad.IsMounted() || ACR_Transport.CrewedVehicle(squad)) { why = "mounted"; return BBB_EIntent.TRANSIT; }
  ACR_EOrderState state = squad.OrderState();
  why = typename.EnumToString(ACR_EOrderState, state) + " task " + squad.m_iAITask;
  if (state == ACR_EOrderState.MOVE || state == ACR_EOrderState.ASSAULT || state == ACR_EOrderState.HUNT)
  {
   if (!AttackerLeash(squad, centre, radius)) radius = 0;
   return BBB_EIntent.PUSH;
  }
  if (DefenderLeash(squad, centre, radius)) return BBB_EIntent.HOLD;
  radius = 0;
  if (!SquadTacticsOn()) return BBB_EIntent.TRANSIT;
  int task = squad.m_iAITask;
  bool waiting = state == ACR_EOrderState.NONE && (task == ACR_EAITask.FREE || task == ACR_EAITask.POSTED || task == ACR_EAITask.DEFEND);
  if (state != ACR_EOrderState.DEFEND && state != ACR_EOrderState.HOLD && state != ACR_EOrderState.SUPPRESS && !waiting) return BBB_EIntent.TRANSIT;
  // Around its base when it has one, else where it stands now. The AI commander leaves the last
  // target set when it frees or posts a squad, so an old base only counts while the squad defends
  // it or still stands there: a squad posted at X after attacking Y was leashed to Y (audit 31 Oct).
  bool atTarget = false;
  if (squad.m_AITarget)
  {
   atTarget = task == ACR_EAITask.DEFEND;
   IEntity here = squad.GetAnchor();
   if (!atTarget && here) atTarget = vector.DistanceXZ(here.GetOrigin(), squad.m_AITarget.GetOrigin()) <= squad.m_AITarget.m_fRadius + 150;
  }
  if (atTarget)
  {
   centre = squad.m_AITarget.GetOrigin();
   radius = Math.Max(squad.m_AITarget.m_fRadius, BBB_OrderLeash.DEFEND_RADIUS);
  }
  else if (squad.GetAnchor())
  {
   centre = squad.GetAnchor().GetOrigin();
   radius = BBB_OrderLeash.DEFEND_RADIUS;
  }
  return BBB_EIntent.HOLD;
 }

 // BBB's squad tactics for our squads: on with -acrSquadTactics 1 (who and where is Intent's).
 // Under BBB's -bbbHostIntent 0 the older rule: the waiting squads contact relay reaches, and
 // attackers inside the objective (#31).
 override bool AllowSquadTactic(SCR_AIGroup group)
 {
  if (!SquadOf(group)) return true;
  if (BBB_HostIntent.s_bOn) return SquadTacticsOn();
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
