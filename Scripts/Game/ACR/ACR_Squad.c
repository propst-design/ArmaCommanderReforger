// What a squad's order looks like on the map (order line colour).
enum ACR_EOrderKind
{
 MOVE,
 DEFEND,
 GET_IN,
 GET_OUT,
 HUNT
}

// What a squad is doing, for the logic (m_Order is only the label shown on the
// map). NONE: a label that carries no state of its own (or one written
// directly to m_Order, which nothing does any more).
enum ACR_EOrderState
{
 NONE,
 HOLD,       // halted, dismounting, waiting...
 MOVE,       // "Moving"
 DEFEND,     // defending / holding position
 ASSAULT,
 SUPPRESS,
 GET_IN,
 GET_OUT,
 HUNT,
 REGROUP,
 RIDE,       // waiting for, boarding or riding a transport (ACR_Rides)
 RETURN,     // a transport going home after a ride
 DISMOUNT,   // everyone getting out now
 COMMANDED,  // a player commands it through the game's own command menu
 DRIVEN,     // its vehicle is driven by a player in the gunner seat
 BUILD_FOB,   // establishing a forward base
 BUILD_ROADBLOCK, // establishing a road checkpoint
 LAY_MINES,   // laying a minefield (ACR_Minefields)
 DISARM_MINES, // disarming the armed mines round a point
 FORTIFY      // digging in (ACR_Fortify)
}

enum ACR_EMoveMode
{
 COMBAT,
 FAST,
 SILENT
}

// A squad's order, saved to be put back later (native command set-aside,
// regroup, resume after a dismount, land/road routes).
class ACR_OrderSnapshot : Managed
{
 ResourceName m_sPrefab;
 IEntity m_Entity; // boarding target must survive saving/restoring the order
 vector m_Target;
 float m_fRadius = -1;
 string m_sLabel;
 ACR_EOrderState m_eState;
 int m_iKind;

 static ACR_OrderSnapshot Make(ResourceName prefab, vector target, float radius, string label, ACR_EOrderState state, int kind)
 {
  ACR_OrderSnapshot snapshot = new ACR_OrderSnapshot();
  snapshot.m_sPrefab = prefab;
  snapshot.m_Target = target;
  snapshot.m_fRadius = radius;
  snapshot.m_sLabel = label;
  snapshot.m_eState = state;
  snapshot.m_iKind = kind;
  return snapshot;
 }
}

modded class SCR_AIGetVehicleWaypointParameters
{
 override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
 {
  SCR_AIGroup group = SCR_AIGroup.Cast(owner);
  if (group)
  {
   IEntity waypointEntity;
   if (!GetVariableIn(PORT_WAYPOINT_IN, waypointEntity)) waypointEntity = group.GetCurrentWaypoint();
   SCR_BoardingEntityWaypoint boarding = SCR_BoardingEntityWaypoint.Cast(waypointEntity);
   if (boarding && (!boarding.GetEntity() || ACR_Logistics.Health(boarding.GetEntity()) <= 0))
   {
    ACR_Manager manager = ACR_Manager.GetInstance();
    ACR_Squad squad;
    if (manager) squad = manager.FindSquadByGroup(group);
    if (squad && squad.m_Waypoint == boarding)
    {
     if (group.GetCurrentWaypoint() == boarding) squad.CancelLostBoarding();
     return ENodeResult.FAIL;
    }
   }
  }
  return super.EOnTaskSimulate(owner, dt);
 }
}

// One commandable AI squad. Server-side only.
//
// Native squad commands need a player-led playable group, so each squad owns a
// hidden playable "command group" linked to it. When a player possesses the
// squad's designated leader, they join that command group and gain the native
// command menu. The possessed leader's AI agent temporarily moves into the
// command group, because a player-held agent inside the AI group stops the rest
// of the formation from following waypoints. The logical roster (GetAgents)
// always includes that detached leader.
class ACR_Squad : Managed
{
 SCR_AIGroup m_Group;
 SCR_AIGroup m_CommandGroup;
 IEntity m_Leader;            // designated leader; survives possession
 AIAgent m_DetachedLeader;    // leader agent parked in m_CommandGroup while possessed
 int m_Commander;             // player id holding native command authority, 0 = none
 int m_PreviousGroup = -1;    // commander's own playable group before taking command
 bool m_PreviousDeleteIfEmpty;
 int m_iStatusPhase; // stable registration phase; removals never shift other squads
 string m_Name;
 vector m_Home;               // position at registration; used by the test RALLY order
 AIWaypoint m_Waypoint;       // last waypoint we spawned, deleted soon after it is replaced
 ref array<IEntity> m_Numbering = {}; // soldiers in first-seen order; gives stable "Alpha 1-2" labels

 void ACR_Squad(SCR_AIGroup group, int number)
 {
  m_Group = group;
  m_iStatusPhase = number % 5;
  m_Name = group.GetCustomName();
  if (m_Name.IsEmpty()) m_Name = SCR_GroupHelperUI.GetTranslatedGroupName(group);
  if (m_Name.IsEmpty()) m_Name = string.Format("Squad %1", number);
  IEntity leader = group.GetLeaderEntity();
  if (leader) m_Home = leader.GetOrigin();
 }

 void Init()
 {
  if (!m_Group) return;
  // We clean up empty squads ourselves, so a squad whose only survivor is the
  // detached (possessed) leader is not deleted out from under us.
  m_Group.SetDeleteWhenEmpty(false);
  SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
  if (!manager) return;
  m_CommandGroup = manager.CreateNewPlayableGroup(m_Group.GetFaction(), SCR_EGroupRole.ASSAULT);
  if (!m_CommandGroup) return;
  m_CommandGroup.SetDeleteWhenEmpty(false);
  m_CommandGroup.SetCanDeleteIfNoPlayer(false);
  m_CommandGroup.SetPrivate(true);
  m_CommandGroup.SetPrivacyChangeable(false);
  m_CommandGroup.SetCustomName(m_Name, 0);
  // CreateNewPlayableGroup links an empty subgroup on a delayed RPC. Link our
  // squad after that initialization has completed.
  GetGame().GetCallqueue().CallLater(LinkSquad, 3000, false);
 }

 void LinkSquad()
 {
  if (!m_CommandGroup || !m_Group) return;
  SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
  manager.RequestSetGroupSlave(SCR_EntityHelper.EntityToRplId(m_CommandGroup), SCR_EntityHelper.EntityToRplId(m_Group));
 }

 // ---------------------------------------------------------------- roster

 void Rename(string name)
 {
  m_Name = name;
  if (m_Group) m_Group.SetCustomName(name, 0);
  if (m_CommandGroup) m_CommandGroup.SetCustomName(name, 0);
 }

 // Highest number of living soldiers seen per prefab: what Reinforce refills to.
 protected ref map<ResourceName, int> m_FullStrength = new map<ResourceName, int>();

 protected void CountByPrefab(map<ResourceName, int> counts)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!soldier || soldier.GetCharacterController().IsDead() || !soldier.GetPrefabData()) continue;
   ResourceName prefab = soldier.GetPrefabData().GetPrefabName();
   int count;
   counts.Find(prefab, count);
   counts.Set(prefab, count + 1);
  }
 }

 void TrackStrength()
 {
  map<ResourceName, int> now = new map<ResourceName, int>();
  CountByPrefab(now);
  foreach (ResourceName prefab, int count : now)
  {
   int best;
   m_FullStrength.Find(prefab, best);
   if (count > best) m_FullStrength.Set(prefab, count);
  }
 }

 // Prefabs of fallen soldiers, one entry per missing man.
 void GetMissing(array<ResourceName> missing)
 {
  missing.Clear();
  map<ResourceName, int> now = new map<ResourceName, int>();
  CountByPrefab(now);
  foreach (ResourceName prefab, int full : m_FullStrength)
  {
   int alive;
   now.Find(prefab, alive);
   for (int i = alive; i < full; i++) missing.Insert(prefab);
  }
 }

 string GetName() { return m_Name; }
 Faction GetFaction()
 {
  if (!m_Group) return null;
  return m_Group.GetFaction();
 }

 void GetAgents(array<AIAgent> agents)
 {
  agents.Clear();
  if (m_Group) m_Group.GetAgents(agents);
  if (m_DetachedLeader && !agents.Contains(m_DetachedLeader)) agents.Insert(m_DetachedLeader);
 }

 // 0-based position of a soldier in the roster, -1 if not a member.
 int IndexOf(IEntity soldier)
 {
  if (!soldier) return -1;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
   if (agent.GetControlledEntity() == soldier) return i;
  return -1;
 }

 // Stable 1-based number for a member. Roster order changes when the leader is
 // detached for possession, so labels must not come from roster position.
 int NumberOf(IEntity soldier)
 {
  if (!soldier) return 0;
  int index = m_Numbering.Find(soldier);
  if (index < 0) index = m_Numbering.Insert(soldier);
  return index + 1;
 }

 bool IsAlive()
 {
  if (m_DetachedLeader) return true;
  return m_Group && m_Group.GetAgentsCount() > 0;
 }

 IEntity GetAnchor()
 {
  IEntity leader = GetConsciousLeader();
  if (leader) return leader;
  if (!m_Group) return null;
  return m_Group.GetLeaderEntity();
 }

 // Resolve occupation from controllers' current bodies, including possession.
 static int FindController(IEntity entity)
 {
  if (!entity) return 0;
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(id);
   if (pc && pc.GetControlledEntity() == entity) return id;
  }
  return 0;
 }

 // ---------------------------------------------------------------- leadership

 // Keep the designated leader across possession: the engine may temporarily
 // choose another active AI agent as its internal formation leader.
 IEntity GetLeader()
 {
  ChimeraCharacter leader = ChimeraCharacter.Cast(m_Leader);
  if (!leader || leader.GetCharacterController().IsDead())
  {
   if (m_Group) m_Leader = m_Group.GetLeaderEntity();
  }
  return m_Leader;
 }

 static bool IsConscious(IEntity entity)
 {
  if (!ACR_Respawn.IsAlive(entity)) return false;
  return !ChimeraCharacter.Cast(entity).GetCharacterController().IsUnconscious();
 }

 // The squad's reference point: the leader, or while an AI leader is knocked out (he
 // can't walk, so measuring the squad from his body reads as strung out) the first
 // member who is awake. A player leader stays the reference.
 IEntity GetConsciousLeader()
 {
  IEntity leader = GetLeader();
  if (!leader || IsConscious(leader) || FindController(leader) || !m_Group) return leader;
  array<AIAgent> agents = {};
  m_Group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (IsConscious(body)) return body;
  }
  return leader;
 }

 bool IsLeader(SCR_PlayerController pc)
 {
  if (!pc || !m_Group) return false;
  ChimeraCharacter body = ChimeraCharacter.Cast(pc.GetControlledEntity());
  return body && !body.GetCharacterController().IsDead() && body == GetLeader();
 }

 protected ref ACR_OrderSnapshot m_Aside; // map order set aside while a player commands the squad

 void Release()
 {
  if (!m_Commander) return;
  SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
  SCR_PlayerControllerGroupComponent component = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(m_Commander);
  SCR_AIGroup previous = manager.FindGroup(m_PreviousGroup);
  if (component && m_CommandGroup && component.GetGroupID() == m_CommandGroup.GetGroupID())
  {
   if (!previous && GetFaction()) previous = manager.CreateNewPlayableGroup(GetFaction(), SCR_EGroupRole.ASSAULT);
   if (previous) component.RPC_AskJoinGroup(previous.GetGroupID());
  }
  if (m_DetachedLeader)
  {
   if (m_CommandGroup) m_CommandGroup.RemoveAgent(m_DetachedLeader);
   if (m_Group) m_Group.AddAgent(m_DetachedLeader);
   m_DetachedLeader = null;
  }
  if (previous) previous.SetCanDeleteIfNoPlayer(m_PreviousDeleteIfEmpty);
  // Back to the map order it had before, unless the player left it busy with one of his own.
  if (m_Aside && m_Group)
  {
   array<AIWaypoint> current = {};
   m_Group.GetWaypoints(current);
   if (current.IsEmpty())
   {
    RestoreOrder(m_Aside);
    PrintFormat("[ACR] %1: resumes its map order (%2)", m_Name, m_Order);
   }
  }
  m_Aside = null;
  PrintFormat("[ACR] %1: native command released player=%2", m_Name, m_Commander);
  m_Commander = 0;
  m_PreviousGroup = -1;
 }

 void UpdateFormationLeader()
 {
  // A player-held AI leader cannot navigate the group's formation to a waypoint.
  // Keep the RTS commander's identity, but let an AI lead formation movement.
  if (!m_Group) return;
  array<AIAgent> agents = {};
  m_Group.GetAgents(agents);
  AIAgent next;
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || !IsConscious(body) || FindController(body)) continue;
   if (!next || body == GetLeader()) next = agent;
  }
  if (next && next != m_Group.GetLeaderAgent()) m_Group.SetNewLeader(next);
 }

 // Grant or revoke native command authority to match who controls the leader.
 // Reforger disables AI far from every player ("max LOD"). Arma Commander
 // battles happen anywhere on the map, so commanded squads stay fully simulated.
 static void KeepSimulated(AIGroup group)
 {
  if (!group) return;
  WakeUp(group);
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents) WakeUp(agent);
 }

 // Reforger lowers AI detail with distance from players, and far AI stop
 // perceiving and fighting. Pin commanded AI and garrisons at full detail.
 static void WakeUp(AIAgent agent)
 {
  if (agent.GetPermanentLOD() != 0) agent.SetPermanentLOD(0);
  agent.PreventMaxLOD();
 }

 protected bool m_bHadPlayer;

 // A player just left one of our soldiers (F8, F7 to someone else, death): make sure the
 // crew's AI and the vehicle's own AI are back on. Live report 3 Oct: after dropping control
 // the LAV sat still and ignored orders (no log of it; this logs what it finds).
 protected void WatchPlayerLeft()
 {
  bool has = HasPlayer();
  if (m_bHadPlayer && !has) GetGame().GetCallqueue().CallLater(WakeCrew, 2000, false, "player left");
  m_bHadPlayer = has;
 }

 void WakeCrew(string why)
 {
  if (HasPlayer()) return;
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (vehicle && manager && manager.IsGunnerDriven(vehicle)) return;
  int woke;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || FindController(body) || !ACR_Respawn.IsAlive(body)) continue;
   AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
   if (control && !control.IsAIActivated()) { control.ActivateAI(); woke++; }
  }
  bool vehicleAIWasOff, pilotDetached;
  if (vehicle)
  {
   AIControlComponent vehicleAI = AIControlComponent.Cast(vehicle.FindComponent(ChimeraAIVehicleControlComponent));
   if (vehicleAI && !vehicleAI.IsAIActivated()) { vehicleAI.ActivateAI(); vehicleAIWasOff = true; }
   CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
   pilotDetached = car && !car.GetPilotCompartmentSlot();
  }
  array<AIWaypoint> waypoints = {};
  if (m_Group) m_Group.GetWaypoints(waypoints);
  PrintFormat("[ACR] %1: %2: woke %3 crew AI, vehicle %4 (vehicle AI was off %5, pilot seat unhooked %6), waypoints %7, order '%8'", m_Name, why, woke, vehicle != null, vehicleAIWasOff, pilotDetached, waypoints.Count(), m_Order);
  // A helicopter crew must not keep a waypoint once the player is out of it (they'd get out).
  if (Aircraft())
  {
   if (!waypoints.IsEmpty() || OrderState() != ACR_EOrderState.HOLD) Halt("Holding");
   return;
  }
  // Left "Following you" with nothing to do: plain Holding, so the panel and AI read it right.
  if (OrderState() == ACR_EOrderState.COMMANDED && waypoints.IsEmpty()) SetOrder("Holding", ACR_EOrderState.HOLD);
 }

 void Refresh()
 {
  WatchPlayerLeft();
  KeepSimulated(m_Group);
  TrackStrength();
  if (!m_Group || !m_CommandGroup) return;
  UpdateFormationLeader();
  int leaderPlayer;
  IEntity leader = GetLeader();
  if (leader) leaderPlayer = FindController(leader);
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(leaderPlayer));
  if (!IsLeader(pc)) leaderPlayer = 0;
  if (m_Commander == leaderPlayer) return;
  Release();
  if (!leaderPlayer) return;
  SCR_PlayerControllerGroupComponent component = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(leaderPlayer);
  if (!component) return;
  m_PreviousGroup = component.GetGroupID();
  SCR_AIGroup previous = SCR_GroupsManagerComponent.GetInstance().FindGroup(m_PreviousGroup);
  if (previous)
  {
   m_PreviousDeleteIfEmpty = previous.GetDeleteIfNoPlayer();
   previous.SetCanDeleteIfNoPlayer(false);
  }
  component.RPC_AskJoinGroup(m_CommandGroup.GetGroupID());
  if (component.GetGroupID() != m_CommandGroup.GetGroupID())
  {
   if (previous) previous.SetCanDeleteIfNoPlayer(m_PreviousDeleteIfEmpty);
   return;
  }
  AIControlComponent aiControl = AIControlComponent.Cast(leader.FindComponent(AIControlComponent));
  if (aiControl)
  {
   m_DetachedLeader = aiControl.GetAIAgent();
   if (m_DetachedLeader)
   {
    m_Group.RemoveAgent(m_DetachedLeader);
    m_CommandGroup.AddAgent(m_DetachedLeader);
   }
  }
  m_CommandGroup.SetGroupLeader(leaderPlayer);
  m_CommandGroup.SetCustomName(m_Name, 0);
  m_Commander = leaderPlayer;
  // Set our standing map order aside, or the squad keeps doing it and the
  // player's command-wheel orders queue up behind it. It resumes when he leaves.
  // (Not a ride's: the ride re-issues its own order when he leaves.)
  m_Aside = null;
  if (m_Waypoint != null && !m_sOrderPrefab.IsEmpty() && OrderState() != ACR_EOrderState.RIDE) m_Aside = SnapshotOrder();
  FollowCommander();
  PrintFormat("[ACR] %1: native command granted player=%2 group=%3 linked=%4", m_Name, leaderPlayer, component.GetGroupID(), m_CommandGroup.GetSlave() == m_Group);
 }

 // The squad follows the player leading it. Not an order from anyone: a ride under way
 // sees m_iCommandSerial and carries on.
 int m_iCommandSerial = -1;
 void FollowCommander()
 {
  Halt("Following you", ACR_EOrderState.COMMANDED);
  m_iCommandSerial = m_iOrderSerial;
 }

 // ---------------------------------------------------------------- orders

 static const ResourceName WP_MOVE = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";
 static const ResourceName WP_DEFEND = "{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et";
 static const ResourceName WP_GET_IN_NEAREST = "{B049D4C74FBC0C4D}Prefabs/AI/Waypoints/AIWaypoint_GetInNearest.et";
 static const ResourceName WP_GET_IN = "{712F4795CF8B91C7}Prefabs/AI/Waypoints/AIWaypoint_GetIn.et";
 static const ResourceName WP_GET_OUT = "{C40316EE26846CAB}Prefabs/AI/Waypoints/AIWaypoint_GetOut.et";
 static const ResourceName WP_SEARCH_DESTROY = "{B3E7B8DC2BAB8ACC}Prefabs/AI/Waypoints/AIWaypoint_SearchAndDestroy.et";
 static const ResourceName WP_SUPPRESS = "{ED8277F35B46B4AA}Prefabs/AI/Waypoints/AIWaypoint_Suppress.et";
 static const ResourceName WP_GARRISON = "{AC230926A00100A2}Prefabs/ACR/ACR_Waypoint_Garrison.et"; // see ACR_Garrison

 vector m_MoveTarget;   // where the current order points
 bool m_bForced;        // current order outranks combat (Fast)
 ResourceName m_sOrderPrefab;
 int m_iOrderSerial;     // bumped by every new order from a player or the AI commander (not by internal re-issues)
 // Who gave the current order (shared command): a player id, or 0 for the AI
 // commander and the squad's own decisions. Set by the manager after a player's order.
 int m_iOrderedBy;
 float m_fOrderedAt;
 float m_fOrderRadius = -1;
 float m_fContactLogged;
 protected float m_fLastContactTime = -1;

 ACR_EMoveMode m_Mode = ACR_EMoveMode.COMBAT;
 string m_Order = "Holding";   // label for the map and panel; logic uses OrderState()
 protected ACR_EOrderState m_eOrderState = ACR_EOrderState.HOLD;
 protected string m_sStateLabel = "Holding"; // the label m_eOrderState belongs to

 // Set the order label and what it means together.
 void SetOrder(string label, ACR_EOrderState state)
 {
  m_Order = label;
  m_eOrderState = state;
  m_sStateLabel = label;
 }

 // The current order, to put back later with RestoreOrder.
 ref ACR_CombatFlank m_CombatFlank = new ACR_CombatFlank();

 ACR_OrderSnapshot SnapshotOrder()
 {
  if (m_CombatFlank.Active()) return m_CombatFlank.Original();
  ACR_OrderSnapshot snapshot = ACR_OrderSnapshot.Make(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius, m_Order, OrderState(), m_OrderKind);
  SCR_BoardingEntityWaypoint boarding = SCR_BoardingEntityWaypoint.Cast(m_Waypoint);
  if (boarding) snapshot.m_Entity = boarding.GetEntity();
  return snapshot;
 }

 // Issue a saved order again (at its own target, or at another point, e.g. a route leg).
 void RestoreOrder(ACR_OrderSnapshot snapshot)
 {
  RestoreOrderAt(snapshot, snapshot.m_sPrefab, snapshot.m_Target, snapshot.m_fRadius);
 }
 protected void RestoreOrderAt(ACR_OrderSnapshot snapshot, ResourceName prefab, vector target, float radius)
 {
  // A garrison put back (after a dismount, a set-aside...) needs its posts again.
  if (prefab == WP_GARRISON && m_Group && !ACR_Garrison.HasPosts(m_Group))
  {
   vector center;
   if (ACR_Garrison.Assign(m_Group, m_Group.GetLeaderAgent(), target, Math.Max(1, CountAlive()), center, radius, false) <= 0) prefab = WP_DEFEND;
  }
  if (!IssueWaypoint(prefab, target, radius, false, snapshot.m_Entity))
  {
   Halt("Order target lost");
   return;
  }
  SetOrder(snapshot.m_sLabel, snapshot.m_eState);
  m_OrderKind = snapshot.m_iKind;
  if (prefab == WP_GET_IN && snapshot.m_Entity) TrackBoarding(snapshot.m_Entity);
 }

 // What the squad is doing. NONE when m_Order no longer matches the label the
 // state was set with (every order goes through SetOrder/Halt now).
 ACR_EOrderState OrderState()
 {
  if (m_Order != m_sStateLabel) return ACR_EOrderState.NONE;
  return m_eOrderState;
 }

 // For the order labels that callers pass in as text (RouteTo and the dismount
 // hand-overs, used by the AI commander and rides): the state a label stands for.
 static ACR_EOrderState StateOfLabel(string label)
 {
  if (label == "Moving" || label == "Patrolling" || label == "Air lift: to the landing zone") return ACR_EOrderState.MOVE;
  if (label == "Defending" || label == "Holding position" || label == "Garrisoning") return ACR_EOrderState.DEFEND;
  if (label == "Assaulting") return ACR_EOrderState.ASSAULT;
  if (label == "Suppressing") return ACR_EOrderState.SUPPRESS;
  if (label == "Getting in") return ACR_EOrderState.GET_IN;
  if (label == "Getting out") return ACR_EOrderState.GET_OUT;
  if (label == "Returning") return ACR_EOrderState.RETURN;
  if (label.StartsWith("Establishing FOB")) return ACR_EOrderState.BUILD_FOB;
  return ACR_EOrderState.NONE;
 }

 // Test runs: the state must say what the old label tests said (any mismatch
 // is printed, and counted in ACR_Perf.s_iInvariantErrors).
 void CheckOrderState()
 {
  ACR_EOrderState state = OrderState();
  string label = m_Order;
  if (state == ACR_EOrderState.BUILD_FOB && label.StartsWith("Establishing FOB")) return;
  bool oldMoving = label == "Moving" || label == "Patrolling" || label == "Air lift: to the landing zone";
  bool oldFinished = oldMoving || label == "Assaulting" || label == "Suppressing" || label == "Getting in" || label == "Getting out";
  if ((state == ACR_EOrderState.MOVE) == oldMoving && FinishesOnArrival(state) == oldFinished) return;
  ACR_Perf.s_iInvariantErrors++;
  PrintFormat("[ACR] INVARIANT order state %1 doesn't match label '%2' (%3)", typename.EnumToString(ACR_EOrderState, state), label, m_Name);
 }

 // An order the squad finishes by arriving (shown as "Holding" afterwards).
 static bool FinishesOnArrival(ACR_EOrderState state)
 {
  return state == ACR_EOrderState.MOVE || state == ACR_EOrderState.ASSAULT || state == ACR_EOrderState.SUPPRESS || state == ACR_EOrderState.GET_IN || state == ACR_EOrderState.GET_OUT;
 }
 // AI commander bookkeeping
 int m_iAITask;          // ACR_EAITask
 IEntity m_OwnVehicle;   // the vehicle this squad came with (crews), even when dismounted
 ACR_Base m_AITarget;
 bool m_bMortar;    // a mortar team (ACR_Mortars runs its set-up, packing and fire missions)
 bool m_bFireAtWill; // observers call fire missions / mortar teams fire on their own (ACR_FireMissions)
 bool m_bAICommanded; // handed to the AI commander while a player commands the side (ACR_SquadAI)
 bool m_bClaimed;     // a player led or ordered it: the 2IC leaves it alone until handed back (ACR_SquadAI)
 ref array<IEntity> m_aHeldGuns = {}; // static guns at the spot it holds that its men sat in (ACR_GunHold)
 bool m_bPatrol;     // the queued points loop: each goes back at the end once the squad sets off for it
 int m_OrderKind;   // ACR_EOrderKind, for map symbology
 protected float m_fNextWaypointRecovery;
 protected int m_iWaypointRecoveries;
 vector m_vFobSite;
 float m_fFobBuildProgress;
 float m_fFobLastTick;
 IEntity m_ReservedTruck;
 float m_fReservedLoads;
 float m_fFobLastProgress;     // world ms of the last build progress (3 min without it: give up)
 float m_fFobNearest;          // on the way: the truck's nearest yet to the site (closing in is progress)
 bool m_bFobSiteCleared;       // trees felled once work starts
 ACR_Base m_FobRestore;        // HQ-less FOB whose HQ this order rebuilds
 vector m_vFobGaveUpSite;      // last abandoned site (AI commander avoids it for a while)
 float m_fFobGaveUpAt;
 static const float FOB_GIVE_UP_MS = 180000;
 static const float FOB_CLOSING_M = 10;   // on the way, this much nearer the site counts as progress
 vector m_vRoadblockSite;
 float m_fRoadblockYaw;
 float m_fRoadblockProgress;
 float m_fRoadblockLastTick;
 float m_fRoadblockLastProgress;
 IEntity m_RoadblockTurret;
 ref ACR_Roadblock m_ActiveRoadblock;
 ref ACR_AmbushPlan m_Ambush;  // lying in wait by a road (Defend on a road, ACR_Ambush)
 ref ACR_MinePlan m_MinePlan;  // the Lay minefield / Disarm mines order at work
 int m_iRoadblockType = ACR_ERoadblockType.MEDIUM;
 int m_iRoadblockPaidRP;       // RP the build in progress was paid with (refunded if called off)
 IEntity m_RoadblockTruck;     // ... or the construction truck it took supplies from
 float m_fRoadblockLoads;
 bool m_bRoadblockArrived;     // the give-up timer runs from the squad's arrival
 float m_fRoadblockDeadline;   // world ms by which the squad must reach the road
 vector m_vRoadblockStage;     // beside the road, where the squad (and a truck it brings) waits
 ref ACR_FortifyPlan m_FortifyPlan;  // the Fortify order at work
 ACR_Fortification m_Fortification;  // the position it holds (its gunners stay on the guns)

 static string ModeName(ACR_EMoveMode mode)
 {
  if (mode == ACR_EMoveMode.FAST) return "Fast";
  if (mode == ACR_EMoveMode.SILENT) return "Silent";
  return "Combat";
 }

 // Movement behaviour, like Arma Commander's Combat / Transport / Silent:
 //  Combat: normal pace, engage at will, orders can be interrupted by contact.
 //  Fast:   run, orders take priority over contact (like a forced move).
 //  Silent: walk, only return fire.
 void SetMode(ACR_EMoveMode mode)
 {
  m_Mode = mode;
  if (!m_Group) return;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  if (utility)
  {
   if (mode == ACR_EMoveMode.SILENT) utility.SetCombatMode(EAIGroupCombatMode.RETURN_FIRE);
   else utility.SetCombatMode(EAIGroupCombatMode.FIRE_AT_WILL);
  }
  SCR_AIGroupSettingsComponent settings = SCR_AIGroupSettingsComponent.Cast(m_Group.FindComponent(SCR_AIGroupSettingsComponent));
  if (settings)
  {
   settings.RemoveSettingsOfTypeAndOrigin(SCR_AIGroupCharactersMovementSpeedSettingBase, SCR_EAISettingOrigin.COMMANDING);
   if (mode == ACR_EMoveMode.FAST)
    settings.AddSetting(SCR_AIGroupCharactersMovementSpeedSetting.Create(SCR_EAISettingOrigin.COMMANDING, EMovementType.RUN), false, true);
   if (mode == ACR_EMoveMode.SILENT)
    settings.AddSetting(SCR_AIGroupCharactersMovementSpeedSetting.Create(SCR_EAISettingOrigin.COMMANDING, EMovementType.WALK), false, true);
  }
 }

 // Replace every queued order (ours or native) with a single waypoint.
 static bool s_bGameDefend; // test (srv:crewold): crews get the game's Defend waypoint as before
 AIWaypoint IssueWaypoint(ResourceName prefab, vector position, float completionRadius = -1, bool forced = false, IEntity entity = null)
 {
  if (!m_Group) return null;
  if (prefab != WP_GET_OUT && Aircraft())
  {
   PrintFormat("[ACR] %1: no waypoint for a helicopter crew (AI can't fly; they would get out)", m_Name);
   return null;
  }
  // Vehicle crews never get the game's Defend waypoint: it orders everyone in a turret out,
  // when it starts and again when it is replaced by the next order (LAV crew bailing out for
  // no reason, 2 Oct live). A plain move there holds the vehicle just the same; the order
  // keeps WP_DEFEND as its kind (m_sOrderPrefab) and its "Defending" label.
  ResourceName spawned = prefab;
  if (prefab == WP_DEFEND && !s_bGameDefend && !m_ActiveRoadblock && ACR_Transport.CrewedVehicle(this)) spawned = WP_MOVE;
  Resource resource = Resource.Load(spawned);
  if (!resource.IsValid()) return null;
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  params.Transform[3] = position;
  AIWaypoint waypoint = AIWaypoint.Cast(GetGame().SpawnEntityPrefabLocal(resource, null, params));
  if (!waypoint) return null;
  SCR_BoardingEntityWaypoint boarding = SCR_BoardingEntityWaypoint.Cast(waypoint);
  if (boarding)
  {
   // Configure before AddWaypoint exposes it to the group's behavior tree.
   if (!entity || ACR_Logistics.Health(entity) <= 0)
   {
    SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
    return null;
   }
   boarding.SetEntity(entity);
   if (boarding.GetEntity() != entity)
   {
    SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
    return null;
   }
  }
  return AdoptWaypoint(waypoint, prefab, position, completionRadius, forced);
 }

 // Replace every queued order with a waypoint the caller spawned (and configured) itself.
 AIWaypoint AdoptWaypoint(AIWaypoint waypoint, ResourceName prefab, vector position, float completionRadius = -1, bool forced = false)
 {
  if (!m_Group || !waypoint) return null;
  SCR_AIWaypoint scripted = SCR_AIWaypoint.Cast(waypoint);
  m_bForced = forced || m_Mode == ACR_EMoveMode.FAST;
  m_MoveTarget = position;
  m_sOrderPrefab = prefab;
  m_fOrderRadius = completionRadius;
  if (scripted && m_bForced) scripted.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
  if (completionRadius >= 0) waypoint.SetCompletionRadius(completionRadius);
  if (prefab != WP_GARRISON) ACR_Garrison.Release(m_Group);
  array<AIWaypoint> previousOrders = {};
  m_Group.GetWaypoints(previousOrders);
  foreach (AIWaypoint previousOrder : previousOrders) m_Group.RemoveWaypoint(previousOrder);
  DeleteWaypointSoon(m_Waypoint);
  DropQueuedHop();
  m_Waypoint = waypoint;
  m_Group.AddWaypoint(waypoint);
  return waypoint;
 }

 // Every new order: a fresh serial, and nobody (yet) to credit it to.
 protected void NewOrder()
 {
  if (m_fReservedLoads > 0 || OrderState() == ACR_EOrderState.BUILD_FOB) CancelEstablishFob();
  // By payment, not state: Halt sets its own state first.
  if (RoadblockPending() || OrderState() == ACR_EOrderState.BUILD_ROADBLOCK) CancelRoadblock("replaced by a new order");
  ACR_GunHold.Release(this); // before the gun is vacated: nobody climbs back on
  if (m_RoadblockTurret) VacateRoadblockTurret();
  if (m_MinePlan) CancelMineWork();
  if (m_Ambush) ACR_Ambush.End(this);
  m_ActiveRoadblock = null; // a barricade or small checkpoint has no gun to vacate
  if (m_FortifyPlan) CancelFortify();
  if (m_Fortification) LeaveFortification();
  m_CombatFlank.Cancel();
  m_bGarrisonPending = false;
  m_iGarrisonTries = 0;
  m_iGarrisonReissues = 0;
  m_iWaypointRecoveries = 0;
  m_fNextWaypointRecovery = 0;
  m_bRecoveryLast = false;
  m_iOrderSerial++;
  m_iOrderedBy = 0;
  m_fOrderedAt = GetGame().GetWorld().GetWorldTime();
  // A new order restarts the stall clock (#53: a player's order to a waiting LAV counted as stuck 9 s later).
  m_fStallSince = m_fOrderedAt;
  m_aRecentOrders.Insert(m_fOrderedAt);
  if (m_aRecentOrders.Count() > THINK_ORDERS) m_aRecentOrders.RemoveOrdered(0);
  // A new order cut a regroup short: no new regroup for a while, or an order re-issued
  // on a timer and a regroup take turns for good (20 Oct live: a reserve stood at the
  // same door for 15 min, ordered on and called back every 15 s).
  if (m_bRegrouping)
  {
   m_fNextRegroup = m_fOrderedAt + REGROUP_FAILED_COOLDOWN_MS;
   PrintFormat("[ACR] TACTICS %1: regroup cut short by a new order", m_Name);
  }
 }

 // Drop every queued order and stand still (gunner drives, the Halt button).
 void Halt(string label, ACR_EOrderState state = ACR_EOrderState.HOLD)
 {
  if (!m_Group) return;
  ClearOrders();
  SetOrder(label, state);
  NewOrder();
  ClearPlans();
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (vehicle) ACR_Transport.SanitizeVehicleHorn(vehicle, true);
 }

 void ClearOrders()
 {
  if (!m_Group) return;
  ACR_Garrison.Release(m_Group);
  array<AIWaypoint> previousOrders = {};
  m_Group.GetWaypoints(previousOrders);
  foreach (AIWaypoint previousOrder : previousOrders) m_Group.RemoveWaypoint(previousOrder);
  DeleteWaypointSoon(m_Waypoint);
  DropQueuedHop();
  m_Waypoint = null;
 }

 // A replaced waypoint goes a moment later, not in the frame it is removed: the group's running
 // activity (a Defend) still reads it until it notices the removal (crash.log 25 Oct: "Defend
 // Waypoint not provided to the node!" as an ambush ended and as a roadblock squad was re-tasked).
 static const int WAYPOINT_DELETE_DELAY_MS = 1000;
 protected static void DeleteWaypointSoon(AIWaypoint waypoint)
 {
  if (waypoint) GetGame().GetCallqueue().CallLater(DeleteWaypointNow, WAYPOINT_DELETE_DELAY_MS, false, waypoint);
 }

 protected static void DeleteWaypointNow(AIWaypoint waypoint)
 {
  if (waypoint) SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
 }

 bool MoveTo(vector destination)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_MOVE, destination)) return false;
  SetOrder("Moving", ACR_EOrderState.MOVE);
  m_OrderKind = ACR_EOrderKind.MOVE;
  return true;
 }

 // Lie in wait at site, watching the kill zone (ACR_Ambush plans both and runs it); defend: a
 // dragged Defend on posts at site facing killZone, without the waiting (ACR_Ambush.DefendAimed).
 bool Ambush(vector site, vector killZone, bool defend = false)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_DEFEND, site, ACR_Ambush.SET_RADIUS)) return false;
  m_Ambush = ACR_Ambush.Start(this, site, killZone, defend);
  SetOrder(ACR_Ambush.OrderText(m_Ambush, "moving in"), ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  ACR_Ambush.Apply(this, m_Ambush, true);
  return true;
 }

 bool Defend(vector position)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_DEFEND, position, 15)) return false;
  SetOrder("Defending", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  return true;
 }

 // Move into the building at (or next to) the point and hold its windows and doorways,
 // spilling into neighbours (ACR_Garrison). Vehicle crews and a point with no usable
 // building get a plain Defend; returns false then too, so callers can say so.
 bool Garrison(vector position)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  return PlaceGarrison(position);
 }

 // The soldier navmesh streams in tiles (about a second): a building whose tile isn't
 // loaded yet is surveyed again next second while the squad heads there; after a few
 // tries it is surveyed without the navmesh check.
 protected bool m_bGarrisonPending;
 protected vector m_GarrisonPending;
 protected int m_iGarrisonTries;
 protected int m_iGarrisonReissues;

 protected bool PlaceGarrison(vector position)
 {
  vector center;
  float radius;
  int placed;
  if (m_Group && !ACR_Transport.CrewedVehicle(this))
   placed = ACR_Garrison.Assign(m_Group, m_Group.GetLeaderAgent(), position, Math.Max(1, CountAlive()), center, radius, m_iGarrisonTries < 4);
  if (placed < 0)
  {
   m_iGarrisonTries++;
   if (!m_bGarrisonPending)
   {
    IssueWaypoint(WP_MOVE, position, 20);
    SetOrder("Garrisoning", ACR_EOrderState.DEFEND);
    m_OrderKind = ACR_EOrderKind.DEFEND;
   }
   m_bGarrisonPending = true;
   m_GarrisonPending = position;
   return true;
  }
  m_bGarrisonPending = false;
  m_iGarrisonTries = 0;
  if (placed == 0)
  {
   if (IssueWaypoint(WP_DEFEND, position, 15)) SetOrder("Defending", ACR_EOrderState.DEFEND);
   m_OrderKind = ACR_EOrderKind.DEFEND;
   return false;
  }
  if (!IssueWaypoint(WP_GARRISON, center, radius)) return false;
  SetOrder("Garrisoning", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  return true;
 }

 // Construction truck assigned to this squad or nearby (within 50 m)
 IEntity ConstructionTruck()
 {
  if (m_ReservedTruck && (ACR_Logistics.ServiceOf(m_ReservedTruck) & ACR_EService.CONSTRUCTION) != 0 && ACR_Logistics.Health(m_ReservedTruck) > 0) return m_ReservedTruck;
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (!vehicle) vehicle = ACR_Transport.VehicleOf(GetLeader());
  if (vehicle && (ACR_Logistics.ServiceOf(vehicle) & ACR_EService.CONSTRUCTION) != 0 && ACR_Logistics.Health(vehicle) > 0) return vehicle;
  IEntity anchor = GetAnchor();
  if (anchor)
  {
   ACR_Manager manager = ACR_Manager.GetInstance();
   array<IEntity> vehicles = {};
   ACR_Vehicles.All(vehicles);
   foreach (IEntity candidate : vehicles)
   {
    if ((ACR_Logistics.ServiceOf(candidate) & ACR_EService.CONSTRUCTION) != 0 && ACR_Logistics.Health(candidate) > 0)
    {
     if (vector.DistanceXZ(candidate.GetOrigin(), anchor.GetOrigin()) <= 50)
     {
      bool claimedByOther = false;
      if (manager)
      {
       foreach (ACR_Squad other : manager.m_Squads)
       {
        if (!other || other == this) continue;
        if (ACR_Transport.CrewedVehicle(other) == candidate || other.FobReservedTruck() == candidate)
        {
         claimedByOther = true;
         break;
        }
       }
      }
      if (!claimedByOther) return candidate;
     }
    }
   }
  }
  return null;
 }

 bool CanEstablishFob()
 {
  if (CountAlive() <= 0 || OrderState() == ACR_EOrderState.BUILD_FOB) return false;
  IEntity truck = ConstructionTruck();
  if (!truck) return false;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Logistics) return false;
  return manager.m_Logistics.StockOf(truck) >= ACR_FobBuild.LOADS_RESERVED;
 }

 bool EstablishFob(vector site)
 {
  string refusal;
  if (!ACR_FobBuild.CanOrder(this, refusal)) return false;
  string factionKey = "";
  if (GetFaction()) factionKey = GetFaction().GetFactionKey();
  ACR_Base restore = ACR_FobBuild.HqLostFobAt(site, factionKey);
  if (restore) site = restore.GetOrigin();
  else if (!ACR_FobBuild.CheckSpot(site, refusal)) return false;
  IEntity truck = ConstructionTruck();
  if (!truck) return false;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Logistics) return false;

  NewOrder();
  ClearPlans();
  m_bRegrouping = false;

  if (!manager.m_Logistics.Use(truck, ACR_FobBuild.LOADS_RESERVED)) return false;

  m_ReservedTruck = truck;
  m_fReservedLoads = ACR_FobBuild.LOADS_RESERVED;
  m_vFobSite = site;
  m_fFobBuildProgress = 0.0;
  m_fFobLastTick = GetGame().GetWorld().GetWorldTime();
  m_fFobLastProgress = m_fFobLastTick;
  m_fFobNearest = float.MAX;
  m_bFobSiteCleared = false;
  m_FobRestore = restore;

  SetOrder("Establishing FOB (0%)", ACR_EOrderState.BUILD_FOB);
  m_OrderKind = ACR_EOrderKind.MOVE;
  // Park clear of the HQ and every upgrade spot (the HQ used to spawn on the truck).
  m_MoveTarget = ACR_FobBuild.ParkingSpot(site);
  IssueWaypoint(WP_MOVE, m_MoveTarget, 5.0);
  if (restore) PrintFormat("[ACR] SQUAD %1: rebuilding the HQ of %2, reserved 500 supplies from %3", m_Name, restore.m_sName, truck);
  else PrintFormat("[ACR] SQUAD %1: establishing FOB at %2, reserved 500 supplies from %3", m_Name, site, truck);
  return true;
 }

 protected void ClosingInOnFob(float now)
 {
  IEntity mover = m_ReservedTruck;
  if (!mover) mover = GetAnchor();
  if (!mover) return;
  float away = vector.DistanceXZ(mover.GetOrigin(), m_vFobSite);
  if (away > m_fFobNearest - FOB_CLOSING_M) return;
  m_fFobNearest = away;
  m_fFobLastProgress = now;
 }

 void CancelEstablishFob()
 {
  if (m_fReservedLoads > 0 && m_ReservedTruck && ACR_Logistics.Health(m_ReservedTruck) > 0)
  {
   ACR_Manager manager = ACR_Manager.GetInstance();
   if (manager && manager.m_Logistics)
   {
    manager.m_Logistics.AddStock(m_ReservedTruck, m_fReservedLoads);
    PrintFormat("[ACR] SQUAD %1: cancelled FOB build, refunded %2 loads to truck", m_Name, m_fReservedLoads);
   }
  }
  m_fReservedLoads = 0;
  m_ReservedTruck = null;
  m_fFobBuildProgress = 0;
  m_FobRestore = null;
  if (m_iAITask == ACR_EAITask.BUILD_FOB) m_iAITask = ACR_EAITask.FREE;
  if (OrderState() == ACR_EOrderState.BUILD_FOB)
  {
   ClearOrders();
   SetOrder("Holding position", ACR_EOrderState.HOLD);
  }
 }

 void TickEstablishFob(float now)
 {
  if (m_fFobLastTick <= 0) m_fFobLastTick = now;
  float dt = (now - m_fFobLastTick) / 1000.0;
  m_fFobLastTick = now;
  if (dt <= 0) return;
  if (dt > 5.0) dt = 1.0;

  if (m_ReservedTruck && ACR_Logistics.Health(m_ReservedTruck) <= 0)
  {
   m_ReservedTruck = null;
   m_fReservedLoads = 0;
   if (m_iAITask == ACR_EAITask.BUILD_FOB) m_iAITask = ACR_EAITask.FREE;
   PrintFormat("[ACR] SQUAD %1: construction truck destroyed during FOB build", m_Name);
   Halt("Halted (truck lost)");
   return;
  }

  if (CountAlive() <= 0)
  {
   CancelEstablishFob();
   return;
  }

  bool engineerPresent = false;
  if (m_ReservedTruck && ACR_Logistics.Health(m_ReservedTruck) > 0 && vector.DistanceXZ(m_ReservedTruck.GetOrigin(), m_vFobSite) <= ACR_FobBuild.CP_RADIUS)
  {
   engineerPresent = true;
  }
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   if (vector.DistanceXZ(body.GetOrigin(), m_vFobSite) <= ACR_FobBuild.CP_RADIUS)
   {
    engineerPresent = true;
    break;
   }
  }

  string factionKey = "";
  if (GetFaction()) factionKey = GetFaction().GetFactionKey();
  bool contested = ACR_FobBuild.IsContested(m_vFobSite, factionKey) || KnownEnemies(60, true) > 0;

  int pct = (int)((m_fFobBuildProgress / ACR_FobBuild.BUILD_DURATION_S) * 100);
  // Three minutes without progress (fighting, stuck, no engineer): give up and refund.
  if (now - m_fFobLastProgress >= FOB_GIVE_UP_MS)
  {
   PrintFormat("[ACR] SQUAD %1: FOB build at %2 abandoned after 3 minutes without progress", m_Name, m_vFobSite);
   m_vFobGaveUpSite = m_vFobSite;
   m_fFobGaveUpAt = now;
   CancelEstablishFob();
   return;
  }
  if (m_FobRestore && (m_FobRestore.m_bRetired || !m_FobRestore.m_bHqLost || m_FobRestore.m_sOwner != factionKey))
  {
   PrintFormat("[ACR] SQUAD %1: HQ rebuild cancelled (the FOB changed)", m_Name);
   CancelEstablishFob();
   return;
  }
  if (contested)
  {
   SetOrder(string.Format("Establishing FOB (%1%% - contested)", pct), ACR_EOrderState.BUILD_FOB);
   return;
  }

  if (!engineerPresent)
  {
   // On the way, closing in on the site is progress: the 3 minutes run once the truck stops
   // getting nearer (a pn11 Everon soak, 27 Oct: 8 builds 2-4 km off were given up while their
   // trucks were still driving, and USSR built no FOB in 2 h).
   ClosingInOnFob(now);
   if (m_Group && !m_Group.GetCurrentWaypoint() && !HasPlayer())
   {
    IssueWaypoint(WP_MOVE, ACR_FobBuild.ParkingSpot(m_vFobSite), 5.0);
   }
   SetOrder(string.Format("Establishing FOB (%1%% - waiting for engineer)", pct), ACR_EOrderState.BUILD_FOB);
   return;
  }

  if (m_fFobBuildProgress >= ACR_FobBuild.BUILD_DURATION_S && ACR_FobSlots.VehicleOnSpot(m_vFobSite, ACR_FobBuild.HQ_RADIUS))
  {
   SetOrder("Establishing FOB (move vehicles off the HQ spot)", ACR_EOrderState.BUILD_FOB);
   return;
  }

  if (!m_bFobSiteCleared)
  {
   m_bFobSiteCleared = true;
   ACR_FobBuild.FellTrees(m_vFobSite, ACR_FobBuild.HQ_RADIUS);
  }
  m_fFobLastProgress = now;
  m_fFobBuildProgress += dt;
  pct = Math.ClampInt((int)((m_fFobBuildProgress / ACR_FobBuild.BUILD_DURATION_S) * 100), 0, 100);
  SetOrder(string.Format("Establishing FOB (%1%%)", pct), ACR_EOrderState.BUILD_FOB);

  if (m_fFobBuildProgress >= ACR_FobBuild.BUILD_DURATION_S)
  {
   CompleteEstablishFob();
  }
 }

 void CompleteEstablishFob()
 {
  string refusal;
  if (!m_FobRestore && !ACR_FobBuild.CheckSpot(m_vFobSite, refusal, this))
  {
   PrintFormat("[ACR] SQUAD %1: FOB site no longer clear: %2", m_Name, refusal);
   CancelEstablishFob();
   return;
  }
  string factionKey = "";
  if (GetFaction()) factionKey = GetFaction().GetFactionKey();
  ResourceName prefab = ACR_FobBuild.HqPrefabFor(factionKey);
  IEntity hq = ACR_Manager.SpawnPrefab(prefab, m_vFobSite);
  if (!hq)
  {
   CancelEstablishFob();
   return;
  }
  m_fReservedLoads = 0;
  m_ReservedTruck = null;
  m_FobRestore = null;
  m_FobBuiltHq = hq;
  if (hq)
  {
   SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(hq.FindComponent(SCR_CampaignBuildingCompositionComponent));
   if (comp)
   {
    comp.ACR_SetFaction(factionKey);
    comp.ACR_MarkBuildCharged();
    comp.ACR_SetCompositionSpawned(true);
   }
  }
  PrintFormat("[ACR] SQUAD %1: FOB established at %2 (entity=%3)", m_Name, m_vFobSite, hq != null);
  ClearOrders();
  if (m_iAITask == ACR_EAITask.BUILD_FOB) m_iAITask = ACR_EAITask.FREE;
  SetOrder("Holding position", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  IssueWaypoint(WP_DEFEND, m_vFobSite, 20);
 }

 // The HQ this squad's last FOB build spawned. Its FOB only appears on the next structures tick
 // (every 2 s), so until then this is how the AI commander knows the build was finished.
 protected IEntity m_FobBuiltHq;
 bool BuiltFobAt(vector site) { return m_FobBuiltHq && vector.DistanceXZ(m_FobBuiltHq.GetOrigin(), site) < 20; }
 IEntity FobBuiltHqForTest() { return m_FobBuiltHq; }

 float FobBuildProgress() { return m_fFobBuildProgress; }
 void SetFobLastProgressForTest(float time) { m_fFobLastProgress = time; }
 ACR_Base FobRestore() { return m_FobRestore; }
 void SetFobBuildProgressForTest(float progress) { m_fFobBuildProgress = progress; }
 IEntity FobReservedTruck() { return m_ReservedTruck; }
 void SetFobReservedTruckForTest(IEntity truck) { m_ReservedTruck = truck; }
 float FobReservedLoads() { return m_fReservedLoads; }
 vector FobSite() { return m_vFobSite; }
 bool IsVehicle() { return ACR_Transport.CrewedVehicle(this) != null; }

 // ================================================================ roadblocks (aca-f23)
 // type: ACR_ERoadblockType. A construction truck with the squad pays in supplies when it has
 // them (truckMayPay: the AI commander pays RP, as before), otherwise RP.
 bool EstablishRoadblock(vector site, float yaw, int type = ACR_ERoadblockType.MEDIUM, bool truckMayPay = true)
 {
  string refusal;
  if (!ACR_Roadblocks.CanOrder(this, refusal, type, truckMayPay)) return false;
  string factionKey = "";
  if (GetFaction()) factionKey = GetFaction().GetFactionKey();
  type = ACR_Roadblocks.TypeFor(type, factionKey);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  // Who pays is settled first, counting what NewOrder gives back from the current job, so that
  // job is only replaced once this order is sure to be paid for.
  IEntity truck;
  float loads;
  if (!ACR_Roadblocks.Payer(this, type, truckMayPay, truck, loads)) return false;
  vector from = site;
  if (GetAnchor()) from = GetAnchor().GetOrigin();

  NewOrder();
  ClearPlans();
  m_bRegrouping = false;

  string paid;
  if (truck && manager.m_Logistics.Use(truck, loads))
  {
   m_RoadblockTruck = truck;
   m_fRoadblockLoads = loads;
   paid = string.Format("%1 supplies from the construction truck at %2", ACR_Roadblocks.Supplies(type), truck.GetOrigin());
  }
  else if (manager.m_Bases.Spend(factionKey, ACR_Roadblocks.Cost(type)))
  {
   m_iRoadblockPaidRP = ACR_Roadblocks.Cost(type);
   paid = string.Format("%1 RP", m_iRoadblockPaidRP);
  }
  else
  {
   // Payer counted only what NewOrder gave back, so this should not happen.
   SetOrder("Holding position", ACR_EOrderState.HOLD);
   PrintFormat("[ACR] SQUAD %1: could not pay for the %2", m_Name, ACR_Roadblocks.Name(type));
   return false;
  }

  m_iRoadblockType = type;
  m_vRoadblockSite = site;
  m_fRoadblockYaw = yaw;
  m_fRoadblockProgress = 0.0;
  m_fRoadblockLastTick = GetGame().GetWorld().GetWorldTime();
  m_fRoadblockLastProgress = m_fRoadblockLastTick;
  m_bRoadblockArrived = false;
  m_fRoadblockDeadline = m_fRoadblockLastTick + vector.DistanceXZ(from, site) / ACR_Roadblocks.TRAVEL_SPEED * 1000 + FOB_GIVE_UP_MS;
  m_vRoadblockStage = ACR_Roadblocks.Stage(site, yaw, type, from);
  m_RoadblockTurret = null;
  m_ActiveRoadblock = null;

  SetOrder(RoadblockLabel(), ACR_EOrderState.BUILD_ROADBLOCK);
  m_OrderKind = ACR_EOrderKind.MOVE;
  m_MoveTarget = m_vRoadblockStage;
  IssueWaypoint(WP_MOVE, m_vRoadblockStage, 4.0);
  PrintFormat("[ACR] SQUAD %1: building %2 at %3 (yaw %4), paid %5", m_Name, ACR_Roadblocks.Name(type), site, yaw, paid);
  return true;
 }

 // "Building Roadblock L (40% - contact)"
 protected string RoadblockLabel(string status = "")
 {
  int pct = Math.ClampInt((int)(m_fRoadblockProgress / ACR_Roadblocks.Duration(m_iRoadblockType) * 100), 0, 100);
  string label = string.Format("Building %1 (%2%%", ACR_Roadblocks.Name(m_iRoadblockType), pct);
  if (status != "") label += " - " + status;
  return label + ")";
 }

 bool RoadblockPending() { return m_iRoadblockPaidRP > 0 || m_fRoadblockLoads > 0; }

 // What NewOrder gives back from the squad's current job, so a new order can count it: RP from
 // a roadblock, minefield or Fortify order ...
 int PendingRP()
 {
  int rp = m_iRoadblockPaidRP;
  if (m_MinePlan) rp += ACR_Minefields.Refund(m_MinePlan);
  if (m_FortifyPlan) rp += m_FortifyPlan.m_iCost;
  return rp;
 }

 // ... and loads back to this truck from a roadblock or FOB build.
 float PendingLoads(IEntity truck)
 {
  if (!truck) return 0;
  float loads;
  if (m_RoadblockTruck == truck) loads += m_fRoadblockLoads;
  if (m_ReservedTruck == truck) loads += m_fReservedLoads;
  return loads;
 }

 // Called off before the composition went in: the price goes back where it came from (a
 // destroyed truck's supplies are lost).
 void CancelRoadblock(string why = "cancelled")
 {
  bool working = OrderState() == ACR_EOrderState.BUILD_ROADBLOCK;
  if (!working && !RoadblockPending()) return;
  string refund = "nothing";
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (m_iRoadblockPaidRP > 0 && manager)
  {
   string factionKey = "";
   if (GetFaction()) factionKey = GetFaction().GetFactionKey();
   manager.m_Bases.Refund(factionKey, m_iRoadblockPaidRP);
   refund = string.Format("%1 RP", m_iRoadblockPaidRP);
  }
  if (m_fRoadblockLoads > 0 && m_RoadblockTruck && ACR_Logistics.Health(m_RoadblockTruck) > 0 && manager && manager.m_Logistics)
  {
   manager.m_Logistics.AddStock(m_RoadblockTruck, m_fRoadblockLoads);
   refund = string.Format("%1 supplies to the construction truck", Math.Round(m_fRoadblockLoads * ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION));
  }
  m_iRoadblockPaidRP = 0;
  m_RoadblockTruck = null;
  m_fRoadblockLoads = 0;
  m_fRoadblockProgress = 0.0;
  EquipShovels(false);
  if (m_iAITask == ACR_EAITask.ROADBLOCK) m_iAITask = ACR_EAITask.FREE;
  if (working)
  {
   ClearOrders();
   SetOrder("Holding position", ACR_EOrderState.HOLD);
  }
  PrintFormat("[ACR] SQUAD %1: roadblock build %2, refunded %3", m_Name, why, refund);
 }

 void TickEstablishRoadblock(float now)
 {
  if (m_fRoadblockLastTick <= 0) m_fRoadblockLastTick = now;
  float dt = (now - m_fRoadblockLastTick) / 1000.0;
  m_fRoadblockLastTick = now;
  if (dt <= 0) return;
  if (dt > 5.0)
  {
   // The order was set aside (a regroup, a player taking the squad): that time counts for
   // neither the walk's limit nor the give-up timer.
   m_fRoadblockDeadline += (dt - 1.0) * 1000;
   m_fRoadblockLastProgress += (dt - 1.0) * 1000;
   dt = 1.0;
  }

  if (CountAlive() <= 0)
  {
   CancelRoadblock("abandoned: the squad is gone");
   return;
  }

  bool squadPresent = false;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   if (vector.DistanceXZ(body.GetOrigin(), m_vRoadblockSite) <= ACR_Roadblocks.CP_RADIUS || vector.DistanceXZ(body.GetOrigin(), m_vRoadblockStage) <= 6)
   {
    squadPresent = true;
    break;
   }
  }

  // The give-up timer runs from the squad's arrival; the walk there has its own limit.
  if (squadPresent && !m_bRoadblockArrived)
  {
   m_bRoadblockArrived = true;
   m_fRoadblockLastProgress = now;
  }
  if (!m_bRoadblockArrived && m_fRoadblockDeadline > 0 && now > m_fRoadblockDeadline)
  {
   CancelRoadblock("abandoned: the squad could not reach the road");
   return;
  }
  if (m_bRoadblockArrived && now - m_fRoadblockLastProgress >= FOB_GIVE_UP_MS)
  {
   PrintFormat("[ACR] SQUAD %1: roadblock at %2 abandoned after 3 minutes without progress", m_Name, m_vRoadblockSite);
   CancelRoadblock("abandoned");
   return;
  }

  bool contested = KnownEnemies(60, true) > 0;
  if (contested)
  {
   EquipShovels(false);
   SetOrder(RoadblockLabel("contact"), ACR_EOrderState.BUILD_ROADBLOCK);
   return;
  }

  if (!squadPresent)
  {
   if (m_Group && !m_Group.GetCurrentWaypoint() && !HasPlayer())
   {
    IssueWaypoint(WP_MOVE, m_vRoadblockStage, 4.0);
   }
   SetOrder(RoadblockLabel("moving to road"), ACR_EOrderState.BUILD_ROADBLOCK);
   return;
  }

  float duration = ACR_Roadblocks.Duration(m_iRoadblockType);
  if (m_fRoadblockProgress < duration)
  {
   EquipShovels(true);
   m_fRoadblockLastProgress = now;
   m_fRoadblockProgress = Math.Min(m_fRoadblockProgress + dt, duration);
  }
  if (m_fRoadblockProgress < duration)
  {
   SetOrder(RoadblockLabel(), ACR_EOrderState.BUILD_ROADBLOCK);
   return;
  }
  // Done: it goes in once no vehicle stands on the spot (3 minutes, then it gives up).
  if (ACR_FobSlots.VehicleOnSpot(m_vRoadblockSite, ACR_Roadblocks.Footprint(m_iRoadblockType)))
  {
   EquipShovels(false);
   SetOrder(RoadblockLabel("move vehicles off the road"), ACR_EOrderState.BUILD_ROADBLOCK);
   return;
  }
  CompleteRoadblock();
 }

 void CompleteRoadblock()
 {
  EquipShovels(false);
  string factionKey = "";
  if (GetFaction()) factionKey = GetFaction().GetFactionKey();
  // Stop the walk to the site first: the composition and its navmesh rebuild land on the
  // builders, and a move still running there fails natively (Everon soak 23 Oct: VM exception
  // "Failed move ... to waypoint" 1.7 s after a roadblock completed).
  ClearOrders();
  ACR_Roadblock rb = ACR_Roadblocks.SpawnRoadblock(factionKey, m_vRoadblockSite, m_fRoadblockYaw, m_Name, m_iRoadblockType);
  if (!rb)
  {
   CancelRoadblock("failed: the composition did not spawn");
   return;
  }
  // Built: nothing left to refund (a dismantle gives half back to whoever paid).
  rb.m_PaidTruck = m_RoadblockTruck;
  rb.m_fPaidLoads = m_fRoadblockLoads;
  m_iRoadblockPaidRP = 0;
  m_RoadblockTruck = null;
  m_fRoadblockLoads = 0;

  m_ActiveRoadblock = rb;
  m_RoadblockTurret = rb.m_Turret;
  // Native Defend owns turret allocation; do not seat a soldier before assigning it.

  SetOrder("Holding roadblock", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  IssueWaypoint(WP_DEFEND, rb.m_vPost, rb.m_fPostRadius);
  PrintFormat("[ACR] SQUAD %1: completed %2 at %3", m_Name, ACR_Roadblocks.Name(rb.m_iType), m_vRoadblockSite);
 }

 void ManRoadblockTurret()
 {
  if (!m_RoadblockTurret || !m_Group || m_Commander != 0 || HasPlayer()) return;
  // Defend reserves compartments and sends native get-in behaviors. Replacing it
  // each tick sends native get-out messages and starts the departure loop again.
  // Beside the lane, off the road (ACR_RoadblockLane.Post), the gun inside the Defend's radius.
  vector post = m_vRoadblockSite;
  float radius = 15.0;
  if (m_ActiveRoadblock && m_ActiveRoadblock.m_vPost != vector.Zero)
  {
   post = m_ActiveRoadblock.m_vPost;
   radius = m_ActiveRoadblock.m_fPostRadius;
  }
  if (!m_Group.GetCurrentWaypoint()) IssueWaypoint(WP_DEFEND, post, radius);
 }

 void VacateRoadblockTurret()
 {
  if (m_RoadblockTurret)
  {
   // This departure belongs to our new order, not to a spontaneous AI decision.
   array<AIAgent> postAgents = {};
   GetAgents(postAgents);
   foreach (AIAgent postAgent : postAgents)
   {
    IEntity postBody = postAgent.GetControlledEntity();
    if (postBody && ACR_Transport.VehicleOf(postBody) == m_RoadblockTurret) m_CrewSeats.Set(postBody, -1);
   }
   ACR_Roadblocks.VacateTurret(m_RoadblockTurret);
   m_RoadblockTurret = null;
   m_ActiveRoadblock = null;
   StepOntoNavmeshSoon();
   PrintFormat("[ACR] SQUAD %1: vacated roadblock machine gun", m_Name);
  }
 }

 // After a gunner's teleport get-out (a roadblock's or a fortified position's gun). The exit can
 // be inside the gun's sandbags, and Defend can leave a soldier on top of them: with the leader at
 // either spot, the next order's move failed natively ("Failed move ... to waypoint", within 8
 // leave-the-gun cycles in each Fortify stress run). Once the get-out has landed, everyone off
 // the navmesh steps onto the closest point of it. Waypoint recovery runs it too: a post's
 // Defend can leave a soldier on sandbags long after any gun (Everon 26 Oct: an RU MG team's
 // last man on a FOB 1 sandbag pile lost every move order for 34 min).
 void StepOntoNavmeshSoon()
 {
  GetGame().GetCallqueue().Remove(StepOntoNavmesh);
  GetGame().GetCallqueue().CallLater(StepOntoNavmesh, 100, false, 0);
 }

 protected void StepOntoNavmesh(int tries)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  bool seated = false;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || EntityUtils.IsPlayer(body)) continue;
   if (ACR_Transport.VehicleOf(body))
   {
    seated = true;
    continue;
   }
   AIPathfindingComponent pathing = AIPathfindingComponent.Cast(agent.FindComponent(AIPathfindingComponent));
   vector at = body.GetOrigin();
   vector onMesh;
   if (!pathing || !pathing.GetClosestPositionOnNavmesh(at, Vector(3, 3, 3), onMesh)) continue;
   if (vector.DistanceXZ(onMesh, at) < 0.5 && Math.AbsFloat(onMesh[1] - at[1]) < 0.6) continue;
   PrintFormat("[ACR] SQUAD %1: soldier off the navmesh stepped %2 m onto it", m_Name, vector.Distance(onMesh, at));
   body.SetOrigin(onMesh);
  }
  // A gunner still getting off: again shortly.
  if (seated && tries < 4) GetGame().GetCallqueue().CallLater(StepOntoNavmesh, 250, false, tries + 1);
 }

 void TickRoadblockDefence(float now)
 {
  if (!m_ActiveRoadblock || m_Commander != 0 || HasPlayer()) return;
  if (m_RoadblockTurret && !ACR_Roadblocks.IsTurretOccupied(m_RoadblockTurret))
  {
   ManRoadblockTurret();
  }

  // Tripwire intel detection: alert if enemies seen near road
  if (!m_ActiveRoadblock.m_bAlerted && KnownEnemies(250, true) > 0)
  {
   m_ActiveRoadblock.m_bAlerted = true;
   ACR_Manager mgr = ACR_Manager.GetInstance();
   if (mgr)
   {
    ACR_Intel intel = mgr.IntelFor(m_ActiveRoadblock.m_sFaction);
    if (intel)
    {
     intel.RadioRoadblockAlert(m_Name, m_ActiveRoadblock.m_vPos);
    }
   }
  }
 }

 // ================================================================ minefields (0.5.7)
 // ACR_Minefields plans the order; the squad pays, walks to the near edge of the site and
 // works there: mines go in (or are disarmed) as the work progresses. NewOrder calls off and
 // refunds whatever job it had.
 bool LayMinefield(ACR_MinePlan plan)
 {
  if (!plan) return false;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases.Spend(plan.m_sFaction, plan.m_iCost)) return false;

  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  m_MinePlan = plan;
  SetOrder(MineLabel("moving to the site"), MineState());
  m_OrderKind = ACR_EOrderKind.MOVE;
  m_MoveTarget = plan.m_vStage;
  IssueWaypoint(WP_MOVE, plan.m_vStage, 4.0);
  PrintFormat("[ACR] SQUAD %1: %2 at %3, paid %4 RP", m_Name, ACR_Minefields.Name(plan.m_eKind), plan.m_vSite, plan.m_iCost);
  return true;
 }

 ACR_EOrderState MineState()
 {
  if (m_MinePlan && m_MinePlan.m_eKind == ACR_EMineKind.DISARM) return ACR_EOrderState.DISARM_MINES;
  return ACR_EOrderState.LAY_MINES;
 }

 // "Laying AT minefield (40% - contact)", "Disarming mines (3 disarmed)"
 protected string MineLabel(string status = "")
 {
  string label;
  if (m_MinePlan.m_eKind == ACR_EMineKind.DISARM) label = string.Format("Disarming mines (%1 disarmed", m_MinePlan.m_iDisarmed);
  else label = string.Format("Laying %1 (%2%%", ACR_Minefields.Name(m_MinePlan.m_eKind), (int)(m_MinePlan.m_fProgress / m_MinePlan.m_fDuration * 100));
  if (status != "") label += " - " + status;
  return label + ")";
 }

 // The order called off (a new order, the squad gone, no progress): the unused share of its
 // cost comes back (ACR_Minefields.Refund); mines already laid stay.
 void CancelMineWork(string why = "cancelled")
 {
  ACR_MinePlan plan = m_MinePlan;
  if (!plan) return;
  bool working = OrderState() == MineState();
  m_MinePlan = null;
  int refund = ACR_Minefields.Refund(plan);
  ACR_Minefields.Finish(plan);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && refund > 0) manager.m_Bases.Refund(plan.m_sFaction, refund);
  if (m_iAITask == ACR_EAITask.MINES) m_iAITask = ACR_EAITask.FREE;
  if (working)
  {
   EquipShovels(false);
   ClearOrders();
   SetOrder("Holding position", ACR_EOrderState.HOLD);
  }
  PrintFormat("[ACR] SQUAD %1: %2 %3, refunded %4 RP", m_Name, ACR_Minefields.Name(plan.m_eKind), why, refund);
 }

 void TickMineWork(float now)
 {
  ACR_MinePlan plan = m_MinePlan;
  // A regroup or a player taking the squad sets the order aside and restores it: the work
  // pauses meanwhile and stays paid for (NewOrder calls it off and refunds).
  if (OrderState() != MineState()) return;
  float dt = (now - plan.m_fLastTick) / 1000.0;
  plan.m_fLastTick = now;
  if (dt <= 0) return;
  if (dt > 5.0)
  {
   // The order was set aside (a regroup, a player taking the squad): that time counts for
   // neither the walk's limit nor the give-up timer.
   plan.m_fDeadline += (dt - 1.0) * 1000;
   plan.m_fLastProgress += (dt - 1.0) * 1000;
   dt = 1.0;
  }
  if (CountAlive() <= 0)
  {
   CancelMineWork("abandoned: the squad is gone");
   return;
  }

  bool present = false;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || vector.DistanceXZ(body.GetOrigin(), plan.m_vSite) > plan.m_fRadius) continue;
   present = true;
   break;
  }
  if (present && !plan.m_bArrived)
  {
   plan.m_bArrived = true;
   plan.m_fLastProgress = now;
  }
  if (!plan.m_bArrived && now > plan.m_fDeadline)
  {
   CancelMineWork("abandoned: the squad could not reach the site");
   return;
  }
  if (plan.m_bArrived && now - plan.m_fLastProgress >= FOB_GIVE_UP_MS)
  {
   PrintFormat("[ACR] SQUAD %1: minefield work at %2 abandoned after 3 minutes without progress", m_Name, plan.m_vSite);
   CancelMineWork("abandoned");
   return;
  }

  if (ACR_FobBuild.IsContested(plan.m_vSite, plan.m_sFaction) || KnownEnemies(60, true) > 0)
  {
   EquipShovels(false);
   SetOrder(MineLabel("contact"), MineState());
   return;
  }
  if (!present)
  {
   if (m_Group && !m_Group.GetCurrentWaypoint() && !HasPlayer()) IssueWaypoint(WP_MOVE, plan.m_vStage, 4.0);
   SetOrder(MineLabel("moving to the site"), MineState());
   return;
  }

  EquipShovels(true);
  float work = dt * ACR_Minefields.Speed(this);
  plan.m_fLastProgress = now;
  plan.m_fProgress = Math.Min(plan.m_fProgress + work, plan.m_fDuration);
  bool done;
  if (plan.m_eKind == ACR_EMineKind.DISARM) done = ACR_Minefields.Disarm(plan, work);
  else
  {
   ACR_Minefields.Lay(plan, plan.m_fProgress / plan.m_fDuration, GetAnchor());
   done = plan.m_fProgress >= plan.m_fDuration;
  }
  if (done) CompleteMineWork();
  else SetOrder(MineLabel(), MineState());
 }

 protected void CompleteMineWork()
 {
  ACR_MinePlan plan = m_MinePlan;
  m_MinePlan = null;
  EquipShovels(false);
  string done;
  if (plan.m_eKind == ACR_EMineKind.DISARM) done = string.Format("%1 finished disarming mines: %2 disarmed", m_Name, plan.m_iDisarmed);
  else
  {
   int laid;
   if (plan.m_Field) laid = plan.m_Field.m_aMines.Count();
   done = string.Format("%1 finished the %2: %3 mines", m_Name, ACR_Minefields.Name(plan.m_eKind), laid);
  }
  ACR_Minefields.Finish(plan);
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(plan.m_sFaction);
  if (intel) done += " " + intel.Place(plan.m_vSite);
  if (manager) manager.Command().Feed(plan.m_sFaction, done);
  ACR_Report.Event(plan.m_sFaction, done);
  PrintFormat("[ACR] SQUAD %1: %2", m_Name, done);

  // Hold just outside the field (its near edge is 8 m from the stage).
  ClearOrders();
  SetOrder("Holding position", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  IssueWaypoint(WP_DEFEND, plan.m_vStage, 8.0);
 }

 // ================================================================ fortify (0.5.7)
 // ACR_Fortify plans the tier; the squad pays, works from behind the position and holds it once
 // the pieces are in.
 bool Fortify(ACR_FortifyPlan plan)
 {
  if (!plan) return false;
  // Affordable counting what NewOrder gives back from the current job; paid after it.
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || manager.m_Bases.GetRP(plan.m_sFaction) + PendingRP() < plan.m_iCost) return false;

  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!manager.m_Bases.Spend(plan.m_sFaction, plan.m_iCost))
  {
   SetOrder("Holding position", ACR_EOrderState.HOLD);
   return false;
  }
  m_FortifyPlan = plan;
  SetOrder(FortifyLabel("moving to the site"), ACR_EOrderState.FORTIFY);
  m_OrderKind = ACR_EOrderKind.MOVE;
  m_MoveTarget = plan.m_vStage;
  IssueWaypoint(WP_MOVE, plan.m_vStage, 4.0);
  PrintFormat("[ACR] SQUAD %1: fortifying (%2) at %3 facing %4, paid %5 RP", m_Name, ACR_Fortify.TierName(plan.m_iTier), plan.m_vSite, (int)plan.m_fYaw, plan.m_iCost);
  return true;
 }

 // "Fortifying: dug in (40% - contact)"
 protected string FortifyLabel(string status = "")
 {
  string label = string.Format("Fortifying: %1 (%2%%", ACR_Fortify.TierWords(m_FortifyPlan.m_iTier), (int)(m_FortifyPlan.m_fProgress / m_FortifyPlan.m_fDuration * 100));
  if (status != "") label += " - " + status;
  return label + ")";
 }

 // The order called off before the pieces went in: the whole cost comes back.
 void CancelFortify(string why = "cancelled")
 {
  ACR_FortifyPlan plan = m_FortifyPlan;
  if (!plan) return;
  bool working = OrderState() == ACR_EOrderState.FORTIFY;
  m_FortifyPlan = null;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) manager.m_Bases.Refund(plan.m_sFaction, plan.m_iCost);
  if (m_iAITask == ACR_EAITask.FORTIFY) m_iAITask = ACR_EAITask.FREE;
  if (working)
  {
   EquipShovels(false);
   ClearOrders();
   SetOrder("Holding position", ACR_EOrderState.HOLD);
  }
  PrintFormat("[ACR] SQUAD %1: fortifying %2, refunded %3 RP", m_Name, why, plan.m_iCost);
 }

 void TickFortify(float now)
 {
  ACR_FortifyPlan plan = m_FortifyPlan;
  // A regroup or a player taking the squad sets the order aside and restores it: the work
  // pauses meanwhile and stays paid for (NewOrder calls it off and refunds).
  if (OrderState() != ACR_EOrderState.FORTIFY) return;
  float dt = (now - plan.m_fLastTick) / 1000.0;
  plan.m_fLastTick = now;
  if (dt <= 0) return;
  if (dt > 5.0)
  {
   // The order was set aside (a regroup, a player taking the squad): that time counts for
   // neither the walk's limit nor the give-up timer.
   plan.m_fDeadline += (dt - 1.0) * 1000;
   plan.m_fLastProgress += (dt - 1.0) * 1000;
   dt = 1.0;
  }
  if (CountAlive() <= 0)
  {
   CancelFortify("abandoned: the squad is gone");
   return;
  }

  bool present = false;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || vector.DistanceXZ(body.GetOrigin(), plan.m_vSite) > plan.m_fRadius) continue;
   present = true;
   break;
  }
  if (present && !plan.m_bArrived)
  {
   plan.m_bArrived = true;
   plan.m_fLastProgress = now;
  }
  if (!plan.m_bArrived && now > plan.m_fDeadline)
  {
   CancelFortify("abandoned: the squad could not reach the site");
   return;
  }
  if (plan.m_bArrived && now - plan.m_fLastProgress >= FOB_GIVE_UP_MS)
  {
   PrintFormat("[ACR] SQUAD %1: fortifying at %2 abandoned after 3 minutes without progress", m_Name, plan.m_vSite);
   CancelFortify("abandoned");
   return;
  }

  if (ACR_FobBuild.IsContested(plan.m_vSite, plan.m_sFaction) || KnownEnemies(60, true) > 0)
  {
   EquipShovels(false);
   SetOrder(FortifyLabel("contact"), ACR_EOrderState.FORTIFY);
   return;
  }
  if (!present)
  {
   if (m_Group && !m_Group.GetCurrentWaypoint() && !HasPlayer()) IssueWaypoint(WP_MOVE, plan.m_vStage, 4.0);
   SetOrder(FortifyLabel("moving to the site"), ACR_EOrderState.FORTIFY);
   return;
  }

  if (plan.m_fProgress < plan.m_fDuration)
  {
   EquipShovels(true);
   plan.m_fProgress = Math.Min(plan.m_fProgress + dt * ACR_Minefields.Speed(this), plan.m_fDuration);
   plan.m_fLastProgress = now;
  }
  if (plan.m_fProgress < plan.m_fDuration)
  {
   SetOrder(FortifyLabel(), ACR_EOrderState.FORTIFY);
   return;
  }
  // Done: the pieces go in once no vehicle stands on a spot (3 minutes, then it gives up).
  if (ACR_Fortify.VehicleOnSpots(plan))
  {
   EquipShovels(false);
   SetOrder(FortifyLabel("move vehicles off the site"), ACR_EOrderState.FORTIFY);
   return;
  }
  CompleteFortify();
 }

 protected void CompleteFortify()
 {
  ACR_FortifyPlan plan = m_FortifyPlan;
  m_FortifyPlan = null;
  EquipShovels(false);
  // Stop the walk first: the pieces and their navmesh rebuild land round the builders.
  ClearOrders();
  string why;
  ACR_Fortification fort = ACR_Fortify.Build(plan, why);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!fort)
  {
   if (manager) manager.m_Bases.Refund(plan.m_sFaction, plan.m_iCost);
   if (m_iAITask == ACR_EAITask.FORTIFY) m_iAITask = ACR_EAITask.FREE;
   SetOrder("Holding position", ACR_EOrderState.HOLD);
   PrintFormat("[ACR] SQUAD %1: could not fortify (%2), refunded %3 RP", m_Name, why, plan.m_iCost);
   // Said where the player sees it (playtest 25 Oct: only the RP was seen to go).
   string lost = string.Format("%1 could not fortify: %2 (%3 RP refunded)", m_Name, why, plan.m_iCost);
   if (manager) manager.Command().Feed(plan.m_sFaction, lost);
   if (manager && m_iOrderedBy != 0) manager.Notify(m_iOrderedBy, lost);
   return;
  }
  m_Fortification = fort;
  string done = string.Format("%1 dug in: %2 (%3)", m_Name, ACR_Fortify.TierName(fort.m_iTier), ACR_Fortify.TierWords(fort.m_iTier));
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(plan.m_sFaction);
  if (intel) done += " " + intel.Place(fort.m_vPos);
  if (manager) manager.Command().Feed(plan.m_sFaction, done);
  ACR_Report.Event(plan.m_sFaction, done);
  PrintFormat("[ACR] SQUAD %1: %2", m_Name, done);
  // Hold it: native Defend puts gunners on the position's guns.
  SetOrder("Holding position", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  IssueWaypoint(WP_DEFEND, fort.m_vPos, ACR_Fortify.HOLD_RADIUS);
 }

 // Back to a position of ours that stands empty (ACR_AICommander.ManPositions): hold it as its
 // builder did, gunners on its guns (native Defend).
 bool ManFortification(ACR_Fortification fort)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_DEFEND, fort.m_vPos, ACR_Fortify.HOLD_RADIUS)) return false;
  m_Fortification = fort;
  // Its reserve's gun crew, if any, steps back (ACR_Defenders).
  if (fort.m_Defenders) ACR_Defenders.StandDownCrew(fort.m_Defenders);
  SetOrder("Holding position", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  return true;
 }

 // Back on a roadblock of ours (ACR_AICommander.ReturnToPosts): held as when its builder
 // completed it, its gun by native Defend.
 bool ManRoadblock(ACR_Roadblock rb)
 {
  if (!rb) return false;
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  m_vRoadblockSite = rb.m_vPos;
  if (!IssueWaypoint(WP_DEFEND, m_vRoadblockSite, 15.0)) return false;
  m_ActiveRoadblock = rb;
  m_RoadblockTurret = rb.m_Turret;
  SetOrder("Holding roadblock", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  return true;
 }

 // A new order: the position stays; our gunners leave its guns (as from a roadblock's).
 void LeaveFortification()
 {
  ACR_Fortification fort = m_Fortification;
  m_Fortification = null;
  if (!fort) return;
  array<IEntity> turrets = {};
  ACR_Fortify.Turrets(fort, turrets);
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (IEntity turret : turrets)
  {
   bool ours = false;
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (!body || ACR_Transport.VehicleOf(body) != turret) continue;
    m_CrewSeats.Set(body, -1);
    ours = true;
   }
   if (ours) ACR_Roadblocks.VacateTurret(turret);
  }
  // Off to another job: the position's reserve defenders man its guns while it is away.
  if (fort.m_Defenders && CountAlive() > 0) ACR_Defenders.CrewGuns(fort.m_Defenders, turrets);
  // Off the nest's sandbags and onto the navmesh (the shared helper, as for roadblock guns).
  StepOntoNavmeshSoon();
 }

 protected static IEntity FindShovelItem(IEntity character)
 {
  if (!character) return null;
  SCR_InventoryStorageManagerComponent inv = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inv) return null;
  array<IEntity> items = {};
  inv.GetItems(items);
  foreach (IEntity item : items)
  {
   if (!item || !item.GetPrefabData()) continue;
   string prefab = item.GetPrefabData().GetPrefabName();
   if (prefab.Contains("ETool") && !prefab.Contains("carrier")) return item;
  }
  return null;
 }

 void EquipShovels(bool equip)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  string factionKey = "";
  if (GetFaction()) factionKey = GetFaction().GetFactionKey();
  ResourceName shovelPrefab = ACR_Requisition.SHOVEL_EAST;
  if (ACR_Supports.IsWestern(factionKey)) shovelPrefab = ACR_Requisition.SHOVEL_WEST;

  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !ACR_Respawn.IsAlive(character)) continue;
   CharacterControllerComponent controller = character.GetCharacterController();
   if (!controller) continue;

   if (equip)
   {
    if (controller.GetStance() == ECharacterStance.STAND) controller.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOCROUCH);
    IEntity shovel = FindShovelItem(character);
    if (!shovel)
    {
     SCR_InventoryStorageManagerComponent inv = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
     if (inv) inv.TrySpawnPrefabToStorage(shovelPrefab, null, -1, EStoragePurpose.PURPOSE_GADGET_PROXY);
     shovel = FindShovelItem(character);
    }
    SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(character);
    if (gadgets && shovel) gadgets.SetGadgetMode(shovel, EGadgetMode.IN_HAND);
   }
   else
   {
    if (controller.GetStance() != ECharacterStance.STAND) controller.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOERECTED);
    IEntity shovel = FindShovelItem(character);
    SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(character);
    if (gadgets && shovel) gadgets.SetGadgetMode(shovel, EGadgetMode.IN_SLOT);
   }
  }
 }

 // Clear an objective: move in and fight through it (search and destroy).
 bool Assault(vector position)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_SEARCH_DESTROY, position, 30)) return false;
  SetOrder("Assaulting", ACR_EOrderState.ASSAULT);
  m_OrderKind = ACR_EOrderKind.MOVE;
  return true;
 }

 // Put fire on an area from where the squad stands (support by fire).
 bool Suppress(vector position)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_SUPPRESS, position)) return false;
  SetOrder("Suppressing", ACR_EOrderState.SUPPRESS);
  m_OrderKind = ACR_EOrderKind.MOVE;
  return true;
 }

 // Living enemies this squad knows about within radius of it.
 // awakeOnly: leave out the unconscious (they don't contest a FOB site).
 int KnownEnemies(float radius, bool awakeOnly = false)
 {
  if (!m_Group) return 0;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  IEntity anchor = GetAnchor();
  if (!utility || !utility.m_Perception || !anchor) return 0;
  int count;
  foreach (IEntity enemy : utility.m_Perception.m_aTargetEntities)
  {
   if (!ACR_Respawn.IsAlive(enemy) || (awakeOnly && !ACR_FobBuild.Awake(enemy))) continue;
   if (vector.DistanceXZ(enemy.GetOrigin(), anchor.GetOrigin()) <= radius) count++;
  }
  return count;
 }

 bool IsMounted()
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && character.IsInVehicle()) return true;
  }
  return false;
 }

 IEntity MountedVehicle()
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && character.IsInVehicle())
   {
    IEntity v = ACR_Transport.VehicleOf(character);
    if (v) return v;
   }
  }
  return null;
 }

 bool IsMountedAsCargo()
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && character.IsInVehicle())
   {
    if (ACR_Transport.Passenger(character)) return true;
   }
  }
  return false;
 }

 // Is anyone of this squad aboard its own vehicle?
 bool InOwnVehicle()
 {
  if (!m_OwnVehicle) return false;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Respawn.IsAlive(body) && ACR_Transport.VehicleOf(body) == m_OwnVehicle) return true;
  }
  return false;
 }

 // The vehicle that defines this squad's map symbol: its own (if still
 // intact), else whatever it is crewing.
 IEntity MarkVehicle()
 {
  if (m_OwnVehicle && ACR_Logistics.Health(m_OwnVehicle) > 0) return m_OwnVehicle;
  return ACR_Transport.CrewedVehicle(this);
 }

 // ---------------------------------------------------------------- hunt
 // Doggedly pursue one enemy group: follow its latest known position (our
 // side's intel) until it is destroyed, the trail goes cold, or we are no
 // longer fit to fight.
 protected ACR_Squad m_HuntSquad;
 protected IEntity m_HuntBody;
 protected bool m_bHunting;
 protected int m_iHuntSerial;
 protected string m_sHuntWhat;
 protected vector m_HuntPos;
 protected float m_fHuntSeen;       // world time of the latest sighting we are following
 protected float m_fNextHuntCheck;
 static const float HUNT_CHECK_MS = 8000;
 static const float HUNT_COLD_MS = 180000;   // no new sighting this long after reaching the last one: lost
 static const float HUNT_GIVE_UP_MS = 420000; // no new sighting at all this long: lost

 void Hunt(ACR_Squad target, IEntity body, vector position, string what)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  m_HuntSquad = target;
  m_HuntBody = body;
  m_sHuntWhat = what;
  m_HuntPos = position;
  m_fHuntSeen = GetGame().GetWorld().GetWorldTime();
  m_fNextHuntCheck = 0;
  m_bHunting = true;
  IssueWaypoint(WP_SEARCH_DESTROY, position, 30);
  SetOrder("Hunting " + what, ACR_EOrderState.HUNT);
  m_OrderKind = ACR_EOrderKind.HUNT;
  m_iHuntSerial = m_iOrderSerial;
 }

 bool IsHunting() { return m_bHunting; }

 protected void EndHunt(string reason)
 {
  m_bHunting = false;
  m_HuntSquad = null;
  m_HuntBody = null;
  PrintFormat("[ACR] HUNT %1: %2", m_Name, reason);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && GetFaction()) manager.NotifyFaction(GetFaction().GetFactionKey(), m_Name + ": " + reason);
  IEntity anchor = GetAnchor();
  if (anchor) Defend(anchor.GetOrigin());
 }

 protected void HuntTick(float now)
 {
  if (!m_bHunting) return;
  // Any other order ends the hunt quietly.
  if (m_iOrderSerial != m_iHuntSerial) { m_bHunting = false; m_HuntSquad = null; m_HuntBody = null; return; }
  if (now < m_fNextHuntCheck) return;
  m_fNextHuntCheck = now + HUNT_CHECK_MS;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !GetFaction()) return;
  bool gone;
  if (m_HuntSquad) gone = !manager.m_Squads.Contains(m_HuntSquad) || m_HuntSquad.CountAlive() == 0;
  else gone = !m_HuntBody || !ACR_Respawn.IsAlive(m_HuntBody);
  if (gone) { EndHunt("target destroyed, holding here"); return; }
  if (StrengthRatio() < 0.35 || !IsCombatUnit() || (m_iAmmoPct >= 0 && m_iAmmoPct < 15))
  {
   EndHunt("breaking off the hunt, no longer fit to fight");
   return;
  }
  ACR_Intel intel = manager.IntelFor(GetFaction().GetFactionKey());
  vector known;
  float seen;
  if (intel && intel.LastKnown(m_HuntSquad, m_HuntBody, known, seen) && seen > m_fHuntSeen)
  {
   m_fHuntSeen = seen;
   // Follow it when it has moved on.
   if (vector.DistanceXZ(known, m_HuntPos) > 40)
   {
    m_HuntPos = known;
    IssueWaypoint(WP_SEARCH_DESTROY, known, 30);
    SetOrder("Hunting " + m_sHuntWhat, ACR_EOrderState.HUNT);
    m_OrderKind = ACR_EOrderKind.HUNT;
   }
   return;
  }
  IEntity anchor = GetAnchor();
  bool atLastKnown = anchor && vector.DistanceXZ(anchor.GetOrigin(), m_HuntPos) < 60;
  if ((atLastKnown && now - m_fHuntSeen > HUNT_COLD_MS) || now - m_fHuntSeen > HUNT_GIVE_UP_MS)
   EndHunt("lost track of the " + m_sHuntWhat);
 }

 float StrengthRatio()
 {
  int full;
  foreach (ResourceName prefab, int count : m_FullStrength) full += count;
  if (full <= 0) return 1;
  return CountAlive() / (float)full;
 }

 // Small-unit sense, every second for squads no player is leading:
 //  - a Fast (forced) move stops ignoring the enemy once the squad makes contact;
 //  - a squad that reaches its destination takes up positions instead of standing around;
 //  - soldiers get the mission's AI skill.
 void Tactics(int skill, float now)
 {
  WatchCrew();
  // A builder fights normally (Jacob, 18 Oct); its progress pauses while in contact.
  if (OrderState() == ACR_EOrderState.BUILD_FOB) TickEstablishFob(now);
  // A regroup or a player taking the squad sets the order aside and restores it: the build
  // pauses meanwhile and stays paid for (NewOrder refunds it).
  if (OrderState() == ACR_EOrderState.BUILD_ROADBLOCK) TickEstablishRoadblock(now);
  // ... but a squad wiped out meanwhile still gets the payment back.
  else if (RoadblockPending() && CountAlive() <= 0) CancelRoadblock("abandoned: the squad is gone");
  if (m_ActiveRoadblock) TickRoadblockDefence(now);
  if (m_MinePlan) TickMineWork(now);
  if (m_Ambush) ACR_Ambush.Tick(this, now);
  if (m_FortifyPlan) TickFortify(now);
  if (!m_Group || m_Commander != 0) return;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(body.FindComponent(SCR_AICombatComponent));
   if (combat && combat.GetAISkill() != skill) combat.SetAISkill(skill);
   // Faster target identification: less standing and staring at a player
   // while deciding whether he's an enemy.
   ACR_Manager manager = ACR_Manager.GetInstance();
   if (combat && manager && combat.GetPerceptionFactor() != manager.m_fAIPerception) combat.SetPerceptionFactor(manager.m_fAIPerception);
  }
  // A player is leading (or riding with) this squad: they give the orders, e.g.
  // with the command wheel. Re-issuing our last map order would wipe theirs.
  if (HasPlayer())
  {
   m_CombatFlank.YieldToPlayer(this);
   return;
  }
  WatchBoardingTarget();
  HuntTick(now);
  ACR_GunHold.Tick(this);
  if (m_bGarrisonPending && m_Order == "Garrisoning") PlaceGarrison(m_GarrisonPending);
  else if (m_sOrderPrefab == WP_GARRISON && m_Order == "Garrisoning" && m_Group.GetCurrentWaypoint() == null && m_iGarrisonReissues < 2)
  {
   // The game completed the garrison waypoint (a failed group move does that): again.
   m_iGarrisonReissues++;
   PrintFormat("[ACR] GARRISON %1: the game dropped the order, issuing it again", m_Name);
   if (!ACR_Garrison.HasPosts(m_Group)) PlaceGarrison(m_MoveTarget);
   else IssueWaypoint(WP_GARRISON, m_MoveTarget, m_fOrderRadius);
  }
  bool contact = KnownEnemies(350) > 0;
  if (contact) m_fLastContactTime = now;
  if (m_CombatFlank.Tick(this, now)) return;
  FollowRoute();
  Cohesion(now, contact);
  RecoverMovementWaypoint(now);
  ReportUnreachable();
  bool movingOrder = m_sOrderPrefab == WP_MOVE || m_sOrderPrefab == WP_DEFEND || m_sOrderPrefab == WP_SEARCH_DESTROY || m_sOrderPrefab == WP_GARRISON;
  // (not after a Halt: m_Waypoint is gone then, and the old order must not come back)
  if (m_bForced && movingOrder && contact && m_Waypoint && !IsMounted() && !m_bRegrouping)
  {
   ACR_EMoveMode mode = m_Mode;
   m_Mode = ACR_EMoveMode.COMBAT;
   IssueWaypoint(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius);
   m_Mode = mode;
   PrintFormat("[ACR] TACTICS %1: contact, fighting its way forward", m_Name);
  }
  else if (m_Mode == ACR_EMoveMode.FAST && movingOrder && !contact && m_Waypoint && !IsMounted() && !m_bRegrouping && m_fLastContactTime > 0 && now - m_fLastContactTime >= 15000)
  {
   m_fLastContactTime = -1;
   IssueWaypoint(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius, true);
   PrintFormat("[ACR] TACTICS %1: contact clear, resuming fast march", m_Name);
  }
  if (OrderState() == ACR_EOrderState.MOVE && m_OrderKind == ACR_EOrderKind.MOVE && !IsMounted())
  {
   array<AIWaypoint> waypoints = {};
   m_Group.GetWaypoints(waypoints);
   IEntity anchor = GetAnchor();
   if (waypoints.IsEmpty() && anchor && vector.DistanceXZ(anchor.GetOrigin(), m_MoveTarget) < 60)
   {
    IssueWaypoint(WP_DEFEND, m_MoveTarget, 20);
    SetOrder("Holding position", ACR_EOrderState.DEFEND);
    m_OrderKind = ACR_EOrderKind.DEFEND;
    PrintFormat("[ACR] TACTICS %1: arrived, taking positions", m_Name);
   }
  }
  DriveWatch(now, contact);
  CheckTacticalDismount(now, contact);
 }

 protected void CheckTacticalDismount(float now, bool contact)
 {
  if (m_bUnsticking || !m_Group || HasPlayer()) return;
  ACR_EOrderState state = OrderState();
  if (state == ACR_EOrderState.DISMOUNT || state == ACR_EOrderState.GET_OUT) return;

  if (IsMountedAsCargo())
  {
   IEntity vehicle = MountedVehicle();
   if (!vehicle) return;
   if (!CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent))) return;

   bool hitRecently = BBB_Transport.VehicleHitRecently(vehicle, 6000);
   int enemiesNear = KnownEnemies(250);
   bool underFire = (contact && enemiesNear > 0) || hitRecently;
   if (!underFire) return;

   ChimeraCharacter driver = DriverOf(vehicle);
   bool driverLost = !driver || !ACR_Respawn.IsAlive(driver);
   Physics phys = vehicle.GetPhysics();
   float speed = 0;
   if (phys) speed = phys.GetVelocity().Length();

   bool dismount = false;
   string reason = "";

   if (driverLost && speed < 3.0)
   {
    dismount = true;
    reason = "driver lost under fire";
   }
   else if (hitRecently && speed < 5.0)
   {
    dismount = true;
    reason = "vehicle taking fire";
   }
   else if (speed < 3.5)
   {
    dismount = true;
    reason = "ambushed under fire";
   }

   if (dismount)
   {
    PrintFormat("[ACR] TACTICS %1: tactical dismount (%2), continuing on foot", m_Name, reason);
    DismountAndContinue(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius, m_Order, m_OrderKind);
   }
  }
 }

 // Crewmen leaving a driver or gunner seat when no order of ours asked for it are logged
 // (2 Oct live: a LAV crew got out twice "for no reason"; not reproduced on the server).
 int m_iUnexpectedCrewDepartures; // diagnostics and physical retention tests
 // Its driver got out on his own and nobody drives its vehicle since: the AI commander puts
 // him back (ACR_AICommander.Recrew). Jacob's 27 Oct game: a Typhoon's driver and gunner got
 // out, only the gunner went back, and every move of the next 37 min failed on the spot.
 bool m_bDriverBailed;
 // A crew that keeps bailing (the game's AI takes it out to walk whenever its vehicle finds no
 // path) is taken as stuck after BAIL_LIMIT bails in BAIL_WINDOW_MS: Recrew and the logistics
 // runs leave it for a while (m_fStuckAt). Jacob's 26 Oct game: a BTR-70's crew got out within
 // 5 s of every grab order, Recrew put them back in (cancelling the walk) and the commander
 // ordered the grab again, for 90 min.
 static const int BAIL_LIMIT = 3;
 static const float BAIL_WINDOW_MS = 600000;
 protected ref array<float> m_aDriverBails = {};
 protected void NoteDriverBail()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = m_aDriverBails.Count() - 1; i >= 0; i--)
   if (now - m_aDriverBails[i] > BAIL_WINDOW_MS) m_aDriverBails.Remove(i);
  m_aDriverBails.Insert(now);
  if (m_aDriverBails.Count() < BAIL_LIMIT) return;
  m_aDriverBails.Clear();
  m_fStuckAt = now;
  PrintFormat("[ACR] CREW %1: its driver keeps getting out (%2 times in %3 min), taken as stuck", m_Name, BAIL_LIMIT, (int)(BAIL_WINDOW_MS / 60000));
 }
 protected ref map<IEntity, int> m_CrewSeats = new map<IEntity, int>();
 protected void WatchCrew()
 {
  if (m_bDriverBailed && (!m_OwnVehicle || DriverOf(m_OwnVehicle)))
  {
   m_bDriverBailed = false;
   m_iDriverReseats = 0;
  }
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   IEntity vehicle = ACR_Transport.VehicleOf(body);
   int seat = -1;
   if (vehicle) seat = ACR_Transport.SeatOf(body);
   int before;
   bool known = m_CrewSeats.Find(body, before);
   m_CrewSeats.Set(body, seat);
   if (!known || before < 0 || before == ECompartmentType.CARGO || seat >= 0) continue;
   ACR_EOrderState state = OrderState();
   if (state == ACR_EOrderState.GET_OUT || state == ACR_EOrderState.DISMOUNT) continue;
   ACR_Manager manager = ACR_Manager.GetInstance();
   if (m_bMortar && manager && manager.Mortars().IsLoadingDeparture(this, agent)) continue;
   m_iUnexpectedCrewDepartures++;
   if (before == ECompartmentType.PILOT && m_OwnVehicle && !DriverOf(m_OwnVehicle))
   {
    m_bDriverBailed = true;
    NoteDriverBail();
   }
   string where = "";
   if (m_OwnVehicle) where = string.Format(", vehicle health %1, driver %2", ACR_Logistics.Health(m_OwnVehicle), DriverOf(m_OwnVehicle) != null);
   PrintFormat("[ACR] CREW LEFT %1: %2 got out of the %3 seat on his own (order '%4', alive %5, enemies near %6, player %7%8)", m_Name, NumberOf(body), typename.EnumToString(ECompartmentType, before), m_Order, ACR_Respawn.IsAlive(body), KnownEnemies(350), FindController(body), where);
  }
 }

 // ---------------------------------------------------------------- land routes
 // An order whose straight line crosses open water is walked leg by leg along a
 // land route instead (see BBB_Terrain), so squads go round a bay, not across it.
 protected ref array<vector> m_aRoute = {};
 protected ref ACR_OrderSnapshot m_Route; // the order at the end of the route (target unused)

 // exactEnd (player orders): road hops on the way, but the last leg is the clicked point.
 bool RouteTo(vector destination, ResourceName prefab, float radius, string order, int kind, bool exactEnd = false)
 {
  // Vehicle crews: aim for the nearest road point, which AI drivers can reach.
  vector road;
  vector clicked = destination;
  IEntity driven = ACR_Transport.CrewedVehicle(this);
  // A roadblock gunner is infantry for this order: the new order takes him off the gun.
  // Planned as a crew, the road point nearest the target can be the roadblock itself.
  bool onPost = driven != null && driven == m_RoadblockTurret;
  if (onPost) driven = null;
  if (driven && BBB_Terrain.RoadPointNear(destination, 250, road)) destination = road;
  IEntity anchor = GetAnchor();
  array<vector> legs = {};
  // ...and on longer trips, along the roads in short hops.
  if (driven && anchor && vector.DistanceXZ(anchor.GetOrigin(), destination) > 400)
  {
   int nodes, millis;
   if (BBB_Roads.Route(driven.GetOrigin(), destination, legs, nodes, millis) && legs.Count() > 1)
   {
    PrintFormat("[ACR] ROAD %1: %2 hops over %3 road points (%4 ms)", m_Name, legs.Count(), nodes, millis);
    if (exactEnd && vector.DistanceXZ(legs[legs.Count() - 1], clicked) > 15) legs.Insert(clicked);
    if (System.IsCLIParam("acrRideTest")) foreach (vector hop : legs) PrintFormat("[ACR] ROAD   hop %1", hop);
    StartRoute(legs, prefab, radius, order, kind);
    return true;
   }
   legs.Clear();
  }
  if (exactEnd) destination = clicked;
  bool landRoute;
  if (anchor && (onPost || !IsMounted()))
  {
   int landStarted = System.GetTickCount();
   landRoute = BBB_Terrain.LandRoute(anchor.GetOrigin(), destination, legs);
   ACR_Perf.Lap("route.land", landStarted);
  }
  if (!landRoute)
  {
   if (prefab == WP_GARRISON) { Garrison(destination); return true; }
   if (prefab == WP_DEFEND) return Defend(destination);
   if (prefab == WP_SEARCH_DESTROY) return Assault(destination);
   return MoveTo(destination);
  }
  PrintFormat("[ACR] ROUTE %1: straight line crosses water, going round in %2 legs", m_Name, legs.Count());
  StartRoute(legs, prefab, radius, order, kind);
  return true;
 }

 protected void StartRoute(array<vector> legs, ResourceName prefab, float radius, string order, int kind)
 {
  NewOrder();
  m_bRegrouping = false;
  ClearPlans();
  foreach (vector leg : legs) m_aRoute.Insert(leg);
  vector finalDestination = vector.Zero;
  if (!legs.IsEmpty()) finalDestination = legs[legs.Count() - 1];
  m_Route = ACR_OrderSnapshot.Make(prefab, finalDestination, radius, order, StateOfLabel(order), kind);
  NextLeg();
 }

 // Walks a route the AI commander planned (a covered approach), leg by leg like a land route.
 void WalkLegs(array<vector> legs, ResourceName prefab, float radius, string order, int kind)
 {
  if (!legs || legs.IsEmpty()) return;
  StartRoute(legs, prefab, radius, order, kind);
 }

 vector RouteDestination()
 {
  if (m_Route && m_Route.m_Target != vector.Zero) return m_Route.m_Target;
  if (!m_aRoute.IsEmpty()) return m_aRoute[m_aRoute.Count() - 1];
  return m_MoveTarget;
 }

 bool HasActiveRoute()
 {
  return m_Route != null || !m_aRoute.IsEmpty();
 }

 protected void NextLeg()
 {
  m_iWaypointRecoveries = 0;
  m_fNextWaypointRecovery = 0;
  SkipStraightHops();
  vector leg = m_aRoute[0];
  m_aRoute.RemoveOrdered(0);
  if (m_aRoute.IsEmpty()) RestoreOrderAt(m_Route, m_Route.m_sPrefab, leg, m_Route.m_fRadius);
  else RestoreOrderAt(m_Route, WP_MOVE, leg, 25);
 }

 // #86: the game's driver brakes as it nears the end of a waypoint and plans a new path
 // before it sets off for the next, so every road hop (BBB_Roads.LEG, 160 m) was a stop.
 // On a straight stretch a crewed vehicle aims for the farthest hop up to ROAD_AIM_M ahead
 // whose skipped hops all lie within ROAD_STRAIGHT_M of the line there; hops at bends and
 // junctions are kept, so it still follows the planned roads. -acrRoadFlow 0: every hop.
 static const float ROAD_AIM_M = 600;
 static const float ROAD_STRAIGHT_M = 60; // cc86q2: 25 m merged 1 hop of 15 on a winding road
 static const float ROAD_HANDOVER_S = 6; // next hop this many seconds out at the current speed

 static bool s_bRoadFlowOff; // tests: the old hop-by-hop driving for an A/B
 static bool RoadFlow()
 {
  return !s_bRoadFlowOff && ACR_LogisticsRules.Choice("acrRoadFlow", "1") != "0";
 }

 protected void SkipStraightHops()
 {
  if (m_aRoute.Count() < 2 || !IsMounted() || !RoadFlow()) return;
  IEntity driven = ACR_Transport.CrewedVehicle(this);
  if (!driven) return;
  vector from = driven.GetOrigin();
  // Handing over short of the current hop (not in m_aRoute any more): it must be on the line too.
  bool viaCurrent = m_MoveTarget != vector.Zero && vector.DistanceXZ(from, m_MoveTarget) < 200;
  int aim = 0;
  for (int k = 1; k < m_aRoute.Count(); k++)
  {
   if (vector.DistanceXZ(from, m_aRoute[k]) > ROAD_AIM_M) break;
   bool straight = !viaCurrent || ACR_AICommander.DistanceToSegment(m_MoveTarget, from, m_aRoute[k]) <= ROAD_STRAIGHT_M;
   for (int j = 0; j < k && straight; j++)
    straight = ACR_AICommander.DistanceToSegment(m_aRoute[j], from, m_aRoute[k]) <= ROAD_STRAIGHT_M;
   if (!straight) break;
   aim = k;
  }
  if (aim > 0) PrintFormat("[ACR] ROAD %1: straight road, skipping %2 hops (%3 left)", m_Name, aim, m_aRoute.Count() - aim);
  for (int skip = 0; skip < aim; skip++) m_aRoute.RemoveOrdered(0);
 }

 // The next hop waits behind the current waypoint, so the group goes on to it when the
 // current one completes instead of us deleting it and making a new one (which stopped the
 // driver to re-plan). Not the last hop: the order's own waypoint ends the route as before.
 protected AIWaypoint m_QueuedHop;

 // The order's own waypoint, or the road hop queued behind it (the group may be on it
 // for up to a squad tick before FlowHops makes it the order's waypoint).
 bool IsOrderWaypoint(AIWaypoint waypoint)
 {
  return waypoint && (waypoint == m_Waypoint || waypoint == m_QueuedHop);
 }

 protected void DropQueuedHop()
 {
  if (!m_QueuedHop) return;
  if (m_Group) m_Group.RemoveWaypoint(m_QueuedHop);
  DeleteWaypointSoon(m_QueuedHop);
  m_QueuedHop = null;
 }

 // True when it looked after the route this tick.
 protected bool FlowHops(IEntity driven, float handover)
 {
  if (!RoadFlow()) return false;
  AIWaypoint current = m_Group.GetCurrentWaypoint();
  if (m_QueuedHop)
  {
   // The group moved on to it: it is the order's waypoint now.
   if (current == m_QueuedHop)
   {
    DeleteWaypointSoon(m_Waypoint);
    m_Waypoint = m_QueuedHop;
    m_MoveTarget = m_QueuedHop.GetOrigin();
    m_QueuedHop = null;
    if (!m_aRoute.IsEmpty()) m_aRoute.RemoveOrdered(0);
    m_iWaypointRecoveries = 0;
    m_fNextWaypointRecovery = 0;
    return true;
   }
   if (current == m_Waypoint && current) return true;
   // Something else replaced the waypoints (a player, a recovery): the queued hop is stale.
   DropQueuedHop();
   return false;
  }
  if (m_aRoute.Count() < 2 || !current || current != m_Waypoint || m_sOrderPrefab != WP_MOVE) return false;
  if (vector.DistanceXZ(driven.GetOrigin(), m_MoveTarget) > handover + 80) return true;
  SkipStraightHops();
  if (m_aRoute.Count() < 2) return false;
  Resource resource = Resource.Load(WP_MOVE);
  if (!resource.IsValid()) return false;
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  params.Transform[3] = m_aRoute[0];
  AIWaypoint hop = AIWaypoint.Cast(GetGame().SpawnEntityPrefabLocal(resource, null, params));
  if (!hop) return false;
  hop.SetCompletionRadius(25);
  SCR_AIWaypoint scripted = SCR_AIWaypoint.Cast(hop);
  if (scripted && m_bForced) scripted.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
  m_Group.AddWaypoint(hop);
  m_QueuedHop = hop;
  return true;
 }

 // How near the current hop a crewed vehicle gets the next one: far enough out that the
 // driver hasn't started braking yet, with a squad tick (1 s) to spare.
 protected float RoadHandover(IEntity driven)
 {
  if (!RoadFlow() || !driven.GetPhysics()) return 70;
  return Math.Max(70, driven.GetPhysics().GetVelocity().Length() * ROAD_HANDOVER_S);
 }

 // ---- queued orders (Shift + right-click on the map)
 static const int MAX_QUEUED = 8;
 protected ref array<vector> m_aQueuePos = {};
 protected ref array<int> m_aQueueKind = {};   // 0 move, 1 defend, 2 garrison

 void ClearPlans()
 {
  m_aRoute.Clear();
  m_aQueuePos.Clear();
  m_aQueueKind.Clear();
  m_bPatrol = false; // a new order ends a patrol
 }

 // Patrol (map panel): the squad walks its queued points (Shift + right-click) in a loop, the
 // point it is moving to now included; Defend and Garrison points are walked through. With one
 // point only it walks between where it stands and that point. A squad that isn't moving
 // (holding where it arrived, say) sets off at once. A new order ends it, as does toggling it
 // again: then it finishes the current leg only (the loop's points are dropped, #65 review: they
 // were all still queued, so it walked one more lap).
 bool TogglePatrol(out string refusal)
 {
  refusal = "";
  if (m_bPatrol)
  {
   m_bPatrol = false;
   m_aQueuePos.Clear();
   m_aQueueKind.Clear();
   return true;
  }
  bool moving = OrderState() == ACR_EOrderState.MOVE && m_Group && m_Group.GetCurrentWaypoint() && m_MoveTarget != vector.Zero && !m_aQueuePos.Contains(m_MoveTarget);
  int points = m_aQueuePos.Count();
  if (moving) points++;
  IEntity anchor = GetAnchor();
  if (points < 1 || !anchor)
  {
   refusal = "give it a move order or Shift + right-click a point first";
   return false;
  }
  // One point: there and back, from where it stands.
  if (points < 2 && m_aQueuePos.Count() < MAX_QUEUED)
  {
   m_aQueuePos.Insert(anchor.GetOrigin());
   m_aQueueKind.Insert(0);
  }
  // Past MAX_QUEUED if need be: the point it is heading for is never left out of the loop.
  if (moving)
  {
   m_aQueuePos.Insert(m_MoveTarget);
   m_aQueueKind.Insert(0);
  }
  m_bPatrol = true;
  PrintFormat("[ACR] SQUAD %1: patrolling %2 points (moving %3)", m_Name, m_aQueuePos.Count(), moving);
  if (!moving) NextQueued();
  return true;
 }

 // Add an order after the current one (or carry it out now if there's nothing to do).
 // Waiting where it ended up ("Holding position": arrived, dismounted, a build done): its hold
 // waypoint never completes, so nothing queued behind it would ever start.
 bool HoldingInPlace() { return m_Order == "Holding position"; }

 bool Queue(vector position, bool defend, bool garrison = false)
 {
  if (!m_Group) return false;
  bool busy = (m_Group.GetCurrentWaypoint() != null && !HoldingInPlace()) || !m_aRoute.IsEmpty() || !m_aQueuePos.IsEmpty();
  if (!busy)
  {
   if (garrison) { Garrison(position); return true; }
   if (defend) return Defend(position);
   return MoveTo(position);
  }
  if (m_aQueuePos.Count() >= MAX_QUEUED) return false;
  m_aQueuePos.Insert(position);
  int kind = 0;
  if (defend) kind = 1;
  if (garrison) kind = 2;
  m_aQueueKind.Insert(kind);
  return true;
 }

 // Every point still ahead of the squad, in order: route legs, then queued orders.
 void GetPlannedPoints(notnull array<vector> points, notnull array<int> kinds)
 {
  if (m_CombatFlank.Active())
  {
   points.Insert(m_CombatFlank.Original().m_Target);
   kinds.Insert(m_CombatFlank.Original().m_iKind);
  }
  int routeKind;
  if (m_Route) routeKind = m_Route.m_iKind;
  foreach (vector leg : m_aRoute)
  {
   points.Insert(leg);
   kinds.Insert(routeKind);
  }
  foreach (int i, vector queued : m_aQueuePos)
  {
   points.Insert(queued);
   int queuedKind = m_aQueueKind[i];
   if (queuedKind == 2) queuedKind = ACR_EOrderKind.DEFEND;
   kinds.Insert(queuedKind);
  }
 }

 protected void FollowQueue()
 {
  if (m_aQueuePos.IsEmpty() || !m_aRoute.IsEmpty() || m_bRegrouping || !m_Group) return;
  if (m_Group.GetCurrentWaypoint()) return;
  // An engine-completed waypoint is not proof that the squad reached it.
  if (MovementStillPending()) return;
  NextQueued();
 }

 // The next queued order now (a patrol puts the point back at the end).
 protected void NextQueued()
 {
  if (m_aQueuePos.IsEmpty() || !m_Group) return;
  vector next = m_aQueuePos[0];
  int kind = m_aQueueKind[0];
  m_aQueuePos.RemoveOrdered(0);
  m_aQueueKind.RemoveOrdered(0);
  // A patrol: the point goes back at the end, and every point is walked through.
  if (m_bPatrol)
  {
   m_aQueuePos.Insert(next);
   m_aQueueKind.Insert(0);
   kind = 0;
  }
  m_iWaypointRecoveries = 0;
  m_fNextWaypointRecovery = 0;
  if (kind == 2)
  {
   PlaceGarrison(next);
   kind = ACR_EOrderKind.DEFEND;
  }
  else if (kind == 1)
  {
   IssueWaypoint(WP_DEFEND, next, 15);
   SetOrder("Defending", ACR_EOrderState.DEFEND);
  }
  else
  {
   IssueWaypoint(WP_MOVE, next);
   if (m_bPatrol) SetOrder("Patrolling", ACR_EOrderState.MOVE);
   else SetOrder("Moving", ACR_EOrderState.MOVE);
  }
  m_OrderKind = kind;
 }

 protected void FollowRoute()
 {
  FollowQueue();
  if (m_aRoute.IsEmpty() || m_bRegrouping || !m_Group) return;
  array<AIWaypoint> waypoints = {};
  m_Group.GetWaypoints(waypoints);
  IEntity leader = GetLeader();
  // Vehicles get the next road hop well before reaching this one: the AI driver
  // brakes as it nears the end of a waypoint, so handing over at 30 m made it stop
  // for a second at every hop (1 Oct live test).
  float handover = 30;
  IEntity driven;
  if (IsMounted()) driven = ACR_Transport.CrewedVehicle(this);
  if (driven) handover = RoadHandover(driven);
  if (driven && FlowHops(driven, handover)) return;
  if (!waypoints.IsEmpty())
  {
   if (leader && vector.DistanceXZ(leader.GetOrigin(), m_MoveTarget) < handover) NextLeg();
   return;
  }
  if (!leader) return;
  float toLeg = vector.DistanceXZ(leader.GetOrigin(), m_MoveTarget);
  if (toLeg < handover)
  {
   NextLeg();
   return;
  }
  // Recovery runs after route/queue handling, including the final mounted leg.
 }

 protected bool MovementStillPending()
 {
  ACR_EOrderState state = OrderState();
  if (state != ACR_EOrderState.MOVE && state != ACR_EOrderState.DEFEND && state != ACR_EOrderState.ASSAULT && state != ACR_EOrderState.RETURN) return false;
  if (m_sOrderPrefab != WP_MOVE && m_sOrderPrefab != WP_DEFEND && m_sOrderPrefab != WP_SEARCH_DESTROY && m_sOrderPrefab != WP_GARRISON) return false;
  IEntity anchor = GetAnchor();
  if (!anchor) return true;
  float arrival = Math.Max(60, m_fOrderRadius);
  if (IsMounted()) arrival = Math.Max(70, arrival);
  if (!m_aRoute.IsEmpty()) arrival = 30;
  if (!m_aRoute.IsEmpty() && IsMounted() && ACR_Transport.CrewedVehicle(this)) arrival = 70;
  return vector.DistanceXZ(anchor.GetOrigin(), m_MoveTarget) >= arrival;
 }

 protected void RecoverMovementWaypoint(float now)
 {
  if (!m_Group || m_bRegrouping || m_bGarrisonPending || m_Group.GetCurrentWaypoint() || !MovementStillPending()) return;
  if (now < m_fNextWaypointRecovery || m_iWaypointRecoveries >= 4) return;
  m_fNextWaypointRecovery = now + 10000;
  m_iWaypointRecoveries++;
  // Jacob 18 Oct: orders were dropped after four identical reissues to a spot the
  // engine could not path to. The first retry repeats the order (a transient drop);
  // later ones aim for the nearest walkable point round it, searching wider each time.
  // The start can be the problem as much as the target: a soldier stranded off the navmesh.
  if (!IsMounted()) StepOntoNavmesh(0);
  vector target = m_MoveTarget;
  string how = "same spot";
  if (m_iWaypointRecoveries >= 2)
  {
   vector reachable;
   bool found = ReachableNear(m_MoveTarget, 5 + 15 * (m_iWaypointRecoveries - 2), reachable);
   // #30: the spot itself is walkable, so the way in from this side is the problem.
   if ((!found || vector.DistanceXZ(reachable, m_MoveTarget) < 3) && DetourRound()) return;
   if (found)
   {
    how = string.Format("nearest walkable point, %1 m off", (int)vector.DistanceXZ(reachable, m_MoveTarget));
    target = reachable;
    m_MoveTarget = reachable;
   }
  }
  IssueWaypoint(m_sOrderPrefab, target, m_fOrderRadius, m_bForced);
  PrintFormat("[ACR] MOVE %1: waypoint disappeared before arrival; recovery %2/4 (%3), group vehicles %4", m_Name, m_iWaypointRecoveries, how, GroupVehicleCount());
  if (m_iWaypointRecoveries >= 4) m_bRecoveryLast = true;
 }

 // #30 (Everon): some base centres sit in walled yards the engine paths into from some sides
 // only (srv:pathprobe: Gallows Base from the south, the move dropped every 10 s while the
 // spot is on the navmesh; the south approach is a dead end, and a leg to a point 80 m round
 // the side failed as well). A foot squad goes round instead: back out DETOUR_M the way it came,
 // to a point DETOUR_M out on one flank (where the probes from there got in), then the same
 // order again. A detour leg that fails too tries the other flank; after two the usual
 // recoveries run out, so a spot no side reaches still ends as unreachable.
 static const float DETOUR_M = 180;
 protected int m_iDetourSerial = -1;
 protected int m_iDetours;
 protected vector m_vDetourGoal;
 protected vector m_vDetourAway; // from the goal towards where the squad came from
 protected bool DetourRound()
 {
  if (IsMounted()) return false;
  IEntity anchor = GetAnchor();
  if (!anchor) return false;
  bool onDetour = m_iDetourSerial == m_iOrderSerial && vector.DistanceXZ(m_MoveTarget, m_vDetourGoal) > 1;
  if (m_iDetourSerial != m_iOrderSerial)
  {
   vector away = anchor.GetOrigin() - m_MoveTarget;
   away[1] = 0;
   if (away.LengthSq() < 1) return false;
   away.Normalize();
   m_iDetourSerial = m_iOrderSerial;
   m_iDetours = 0;
   m_vDetourGoal = m_MoveTarget;
   m_vDetourAway = away;
  }
  if (m_iDetours >= 2) return false;
  vector goal = m_vDetourGoal;
  vector side = Vector(m_vDetourAway[2], 0, -m_vDetourAway[0]);
  if (m_iDetours == 1) side = side * -1;
  vector back, flank;
  if (!ReachableNear(goal + side * DETOUR_M, 30, flank)) return false;
  bool backOut = vector.DistanceXZ(anchor.GetOrigin(), goal) < DETOUR_M - 30 && ReachableNear(goal + m_vDetourAway * DETOUR_M, 30, back);
  // The legs of the last detour still ahead go; the goal and the order's own route after it stay.
  if (onDetour)
  {
   while (!m_aRoute.IsEmpty() && vector.DistanceXZ(m_aRoute[0], goal) > 1) m_aRoute.RemoveOrdered(0);
  }
  else
  {
   if (m_aRoute.IsEmpty()) m_Route = ACR_OrderSnapshot.Make(m_sOrderPrefab, goal, m_fOrderRadius, m_Order, OrderState(), m_OrderKind);
   m_aRoute.InsertAt(goal, 0);
  }
  m_aRoute.InsertAt(flank, 0);
  if (backOut) m_aRoute.InsertAt(back, 0);
  m_iDetours++;
  string how = "";
  if (backOut) how = string.Format("back out to %1, then ", ACR_Manager.Grid(back));
  PrintFormat("[ACR] MOVE %1: can't get in to %2 from this side, going round (%3%4, detour %5/2)", m_Name, ACR_Manager.Grid(goal), how, ACR_Manager.Grid(flank), m_iDetours);
  // The commander who gave the order sees why it walks away from the target.
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && m_iOrderedBy > 0) manager.Notify(m_iOrderedBy, string.Format("%1 can't get in to %2 from this side, going round", m_Name, ACR_Manager.Grid(goal)));
  NextLeg();
  return true;
 }

 // Said once when the last recovery also fails, so an order is never dropped silently.
 protected bool m_bRecoveryLast;
 protected void ReportUnreachable()
 {
  if (!m_bRecoveryLast || !m_Group || m_Group.GetCurrentWaypoint() || !MovementStillPending()) return;
  if (GetGame().GetWorld().GetWorldTime() < m_fNextWaypointRecovery) return;
  m_bRecoveryLast = false;
  string message = string.Format("%1 can't reach %2, holding", m_Name, ACR_Manager.Grid(m_MoveTarget));
  PrintFormat("[ACR] MOVE %1: gave up after 4 recoveries at %2", m_Name, m_MoveTarget);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && m_iOrderedBy > 0) manager.Notify(m_iOrderedBy, message);
  m_iGaveUpMoveSerial = m_iOrderSerial;
  m_fGaveUpAt = GetGame().GetWorld().GetWorldTime();
  Halt("Holding (unreachable)");
  m_iUnreachableSerial = m_iOrderSerial;
 }

 // Holding since its last order turned out unreachable (no order since): the AI commander
 // gives up on that attack at once instead of waiting out its time limit (Everon night soak
 // 26 Oct: Gallows Base can't be pathed into from the south; a grab's only squad held there
 // "unreachable" for 17 min). Moving the target short of the spot instead was tried and
 // dropped: the squad then "arrived" without moving, outside the base (srv:pathprobe).
 protected int m_iUnreachableSerial = -1;
 protected int m_iGaveUpMoveSerial = -1; // the move order that was given up (the hold has its own serial)
 float m_fGaveUpAt = -1000000; // world time it last gave up a move as unreachable
 bool GaveUpUnreachable() { return m_iUnreachableSerial == m_iOrderSerial; }
 // A move order given since this serial (this one included) was given up as unreachable.
 bool GaveUpSince(int serial) { return m_iGaveUpMoveSerial >= serial; }
 // srv:mortargiveup: as ReportUnreachable, without waiting out four recoveries.
 void GiveUpForTest()
 {
  m_iGaveUpMoveSerial = m_iOrderSerial;
  m_fGaveUpAt = GetGame().GetWorld().GetWorldTime();
  Halt("Holding (unreachable)");
  m_iUnreachableSerial = m_iOrderSerial;
 }
 // Its last move order still stands, though the engine may have dropped the waypoint: the
 // squad's own recovery owns it, and a fresh order would reset that recovery.
 bool MovePending() { return MovementStillPending() && !GaveUpUnreachable(); }

 // Vehicles the game's group AI counts as its own (it plans moves with them).
 int GroupVehicleCount()
 {
  if (!m_Group) return 0;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  if (!utility || !utility.m_VehicleMgr) return 0;
  return utility.m_VehicleMgr.GetVehiclesCount();
 }

 // The nearest point on the soldiers' navmesh or road within radius of 'at'.
 protected bool ReachableNear(vector at, float radius, out vector result)
 {
  if (IsMounted())
  {
   vector roadPoint;
   if (BBB_Terrain.RoadPointNear(at, radius, roadPoint))
   {
    result = roadPoint;
    return true;
   }
   return false;
  }
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   AIPathfindingComponent pathing = AIPathfindingComponent.Cast(agent.FindComponent(AIPathfindingComponent));
   if (!pathing) continue;
   NavmeshWorldComponent mesh = pathing.GetNavmeshComponent();
   if (mesh && !mesh.IsTileLoaded(at)) mesh.LoadTileIn(at);
   return pathing.GetClosestPositionOnNavmesh(at, Vector(radius, 10, radius), result);
  }
  return false;
 }

 // ---------------------------------------------------------------- AC:A Think (ACR_Think)
 // When its newest orders came: a squad given a fresh order every few seconds is looping
 // (Everon 26 Oct: an MG team re-ordered every 15 s for 34 min).
 static const int THINK_ORDERS = 12;
 protected ref array<float> m_aRecentOrders = {};
 int OrdersSince(float since)
 {
  int count;
  foreach (float at : m_aRecentOrders) if (at >= since) count++;
  return count;
 }

 // Its movement state in a few words: waypoint recovery, regrouping, gave up.
 string ThinkMovement()
 {
  string state;
  if (m_bRegrouping) state += " regrouping";
  if (m_bGarrisonPending) state += " garrison pending";
  if (m_iWaypointRecoveries > 0) state += string.Format(" recovery %1/4", m_iWaypointRecoveries);
  if (GaveUpUnreachable()) state += " GAVE UP (unreachable)";
  else if (MovementStillPending()) state += string.Format(" moving, %1 m to go", (int)DistanceToOrder());
  if (m_Group && !m_Group.GetCurrentWaypoint() && MovementStillPending()) state += " NO WAYPOINT";
  return state.Trim();
 }

 protected float DistanceToOrder()
 {
  IEntity anchor = GetAnchor();
  if (!anchor) return 0;
  return vector.DistanceXZ(anchor.GetOrigin(), m_MoveTarget);
 }

 // ---------------------------------------------------------------- cohesion
 // On a long move the leader keeps his pace while the rest string out behind
 // him. When a third of the squad is more than 60 m off, the squad stops (no
 // orders: the game's formation pulls everyone back to the leader) until they
 // are within 25 m, then carries on. Not in contact, and at most once a minute.
 static const float STRAGGLER_DISTANCE = 60;
 static const float REGROUPED_DISTANCE = 25;
 static const float REGROUP_TIMEOUT_MS = 60000;
 static const float REGROUP_COOLDOWN_MS = 60000;
 static const float REGROUP_FAILED_COOLDOWN_MS = 150000; // didn't close up: don't stop-start every minute
 static const float ORDER_SETTLE_MS = 20000; // a new order gets this long before a regroup can stop it

 protected bool m_bRegrouping;
 protected float m_fRegroupSince;
 protected float m_fNextRegroup;
 void HoldRegroupForTest(float until) { m_fNextRegroup = until; m_bRegrouping = false; }
 void StartRegroupForTest() { m_bRegrouping = true; }
 bool RegroupingForTest() { return m_bRegrouping; }
 float NextRegroupForTest() { return m_fNextRegroup; }
 static bool OrderSettled(float now, float orderedAt) { return now - orderedAt >= ORDER_SETTLE_MS; }
 protected ref ACR_OrderSnapshot m_Regroup; // the order resumed once regrouped

 // Middle of the living members on foot (where a strung-out squad meets up).
 bool Centroid(out vector center)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  vector sum;
  int n;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !IsConscious(body) || ACR_Transport.VehicleOf(body)) continue;
   sum = sum + body.GetOrigin();
   n++;
  }
  if (n == 0) return false;
  center = ACR_Manager.Ground(sum * (1.0 / n));
  return true;
 }

 // Living members on foot farther than this from the leader.
 int Stragglers(float radius)
 {
  IEntity leader = GetConsciousLeader();
  if (!leader) return 0;
  int count;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || body == leader || !IsConscious(body) || ACR_Transport.VehicleOf(body)) continue;
   if (vector.DistanceXZ(body.GetOrigin(), leader.GetOrigin()) > radius) count++;
  }
  return count;
 }

 protected void Cohesion(float now, bool contact)
 {
  if (m_bRegrouping)
  {
   bool closed = Stragglers(REGROUPED_DISTANCE) == 0;
   if (!closed && !contact && now - m_fRegroupSince < REGROUP_TIMEOUT_MS) return;
   m_bRegrouping = false;
   m_fNextRegroup = now + REGROUP_COOLDOWN_MS;
   if (!closed) m_fNextRegroup = now + REGROUP_FAILED_COOLDOWN_MS;
   RestoreOrder(m_Regroup);
   PrintFormat("[ACR] TACTICS %1: regrouped (%2 s), moving on", m_Name, (int)((now - m_fRegroupSince) / 1000));
   return;
  }
  if (contact || now < m_fNextRegroup || !OrderSettled(now, m_fOrderedAt) || !m_Group || HasPlayer() || IsMounted()) return;
  bool movingOrder = m_sOrderPrefab == WP_MOVE || m_sOrderPrefab == WP_DEFEND || m_sOrderPrefab == WP_SEARCH_DESTROY || m_sOrderPrefab == WP_GARRISON;
  if (!movingOrder) return;
  array<AIWaypoint> waypoints = {};
  m_Group.GetWaypoints(waypoints);
  IEntity leader = GetConsciousLeader();
  if (waypoints.IsEmpty() || !leader || vector.DistanceXZ(leader.GetOrigin(), m_MoveTarget) < 200) return;
  // The knocked-out can't walk to a regroup point: count only those awake.
  int alive = CountConscious();
  if (alive < 3) return;
  int behind = Stragglers(STRAGGLER_DISTANCE);
  if (behind * 3 < alive) return;
  m_Regroup = SnapshotOrder();
  ClearOrders();
  // Everyone heads for the middle of the squad: the front waits and walks
  // back a little, the stragglers catch up, instead of the leader standing
  // still while the tail wanders.
  vector gather;
  if (Centroid(gather)) IssueWaypoint(WP_MOVE, gather, 10);
  SetOrder("Regrouping", ACR_EOrderState.REGROUP);
  m_bRegrouping = true;
  m_fRegroupSince = now;
  PrintFormat("[ACR] TACTICS %1: %2 of %3 strung out, regrouping", m_Name, behind, alive);
  LogStragglers(leader);
 }

 // Who is behind and what he is doing (21 Oct live, Kolguyev: the same two or three men every
 // regroup, the heavily loaded ones by eye). One line per straggler, plus the leader for scale.
 protected void LogStragglers(IEntity leader)
 {
  PrintFormat("[ACR] STRAGGLER %1 leader %2: load %3 kg", m_Name, StragglerKind(leader), Math.Round(LoadOf(leader)));
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || body == leader || !IsConscious(body) || ACR_Transport.VehicleOf(body)) continue;
   float distance = vector.DistanceXZ(body.GetOrigin(), leader.GetOrigin());
   if (distance <= STRAGGLER_DISTANCE) continue;
   string doing = "idle";
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (utility && utility.GetCurrentBehavior()) doing = utility.GetCurrentBehavior().ClassName();
   string stance = "?";
   ChimeraCharacter character = ChimeraCharacter.Cast(body);
   if (character && character.GetCharacterController()) stance = typename.EnumToString(ECharacterStance, character.GetCharacterController().GetStance());
   PrintFormat("[ACR] STRAGGLER %1 %2: %3 m behind, load %4 kg, %5, %6, climb %7 m", m_Name, StragglerKind(body), Math.Round(distance), Math.Round(LoadOf(body)), doing, stance, Math.Round(leader.GetOrigin()[1] - body.GetOrigin()[1]));
  }
 }

 static float LoadOf(IEntity body)
 {
  if (!body) return 0;
  SCR_CharacterInventoryStorageComponent storage = SCR_CharacterInventoryStorageComponent.Cast(body.FindComponent(SCR_CharacterInventoryStorageComponent));
  if (!storage) return 0;
  return storage.GetTotalWeight();
 }

 static string StragglerKind(IEntity body)
 {
  if (!body || !body.GetPrefabData()) return "?";
  string name = body.GetPrefabData().GetPrefabName();
  int slash = name.LastIndexOf("/");
  if (slash >= 0) name = name.Substring(slash + 1, name.Length() - slash - 1);
  name.Replace(".et", "");
  name.Replace("Character_", "");
  return name;
 }

 // ---------------------------------------------------------------- stalled vehicles
 // The game's AI drivers get stuck on obstacles (and sit there honking), or stop
 // dead under fire. A driven vehicle with somewhere to go that hasn't moved for a
 // while is backed up and turned (scripted, like "gunner drives") and sent on its
 // way again; after two tries it is re-routed once over the roads from where it
 // stands. Still stuck: the crew stays aboard and holds ("Stuck, holding") and any
 // AI passengers carry on on foot (7 Oct live: drivers walked away from vehicles that
 // only needed another way round, and the AI re-crewed them into the same spot).
 // Only an unarmed vehicle stopped under fire is abandoned.
 // Jacob 18 Oct (Kolguyev): 60 back-ups for 12 departures. AI drivers take several seconds
 // to pull away and stop short of their waypoint, so 8 s / 15 m (15 Oct) fired on healthy
 // vehicles; back to 15 s / 40 m. Under fire, unarmed: 3 s stays.
 static const float STALL_SECONDS = 15;
 static float s_fStallSeconds = STALL_SECONDS; // test hook: srv:stall shortens it so its enemy team arrives after the soft truck has given up
 static const float STALL_SECONDS_FIGHTING = 40; // armed and in contact: the gunner may be busy, give it longer
 static const float STALL_SECONDS_HELPLESS = 3;  // under fire with no gunner
 static const float STALL_ARRIVED = 40;          // closer than this to the target counts as there
 static const int UNSTICK_TRIES = 2;
 static const int REROUTE_TRIES = 1;

 protected IEntity m_StallVehicle;
 protected vector m_StallPos;
 protected float m_fStallSince;
 protected int m_iUnsticks;
 int m_iStuckCount; // every time its vehicle was judged stuck (tests)
 protected int m_iReroutes;
 // #53 (27 Oct, Arland): an LAV waiting at a gate for its gunner to get back in was taken as
 // stuck, backed up away from him twice and held, dropping the player's order. While one of the
 // crew is getting in or out, or stands by the vehicle, the stall clock waits (up to
 // BOARDING_WAIT_MS, so a man who never makes it doesn't hide a real stall). Back-ups count
 // towards the hold only until the vehicle gets UNSTICK_CLEAR m past the spot it was stuck at.
 static const float BOARDING_WAIT_MS = 60000;
 static const float BOARDING_RANGE = 40;
 static const float UNSTICK_CLEAR = 40;
 protected float m_fBoardingSince = -1;
 protected vector m_vUnstickFrom;
 float m_fStuckAt = -1000000; // world time the squad last gave up on a stuck vehicle (AI re-crewing waits)
 protected bool m_bUnsticking;
 protected ref ACR_SquadUnstick m_UnstickListener; // told by BBB_Unstick when backing out is over
 protected bool m_bResumeOnFoot;
 int UnstickTries() { return m_iUnsticks; }
 int Reroutes() { return m_iReroutes; }
 bool IsUnsticking() { return m_bUnsticking; }

 // A driver who bailed (m_bDriverBailed): the nearest of its men on foot goes for the driver's
 // seat himself. The group's Get in waypoint seats nobody while part of the crew is aboard
 // (srv:recrew: 50 s on 'Getting in', the seat still empty). Close by he climbs in; further off
 // he walks there (the game's own get-in), and the next try places him (BoardCargo does too).
 protected int m_iDriverReseats;
 bool ReseatDriver(IEntity vehicle)
 {
  CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
  if (!car) return false;
  BaseCompartmentSlot seat = car.GetPilotCompartmentSlot();
  if (!seat || seat.GetOccupant()) return false;
  ChimeraCharacter best;
  AIAgent bestAgent;
  float bestDistance = 60;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter man = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!man || !ACR_Respawn.IsAlive(man) || man.IsInVehicle() || FindController(man) != 0) continue;
   if (!man.GetCompartmentAccessComponent() || man.GetCompartmentAccessComponent().IsGettingIn()) continue;
   if (BBB_Claims.Committed(man)) continue; // BBB has him on a rescue, a static gun or a rearm
   float distance = vector.Distance(man.GetOrigin(), vehicle.GetOrigin());
   if (distance >= bestDistance) continue;
   bestDistance = distance;
   best = man;
   bestAgent = agent;
  }
  if (!best) return false;
  m_iDriverReseats++;
  CompartmentAccessComponent access = best.GetCompartmentAccessComponent();
  SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(bestAgent.FindComponent(SCR_AIUtilityComponent));
  bool walk = bestDistance > 4 && m_iDriverReseats == 1 && utility;
  PrintFormat("[ACR] CREW %1: back to the driver's seat, %2 m off (try %3, %4)", m_Name, (int)bestDistance, m_iDriverReseats, walk);
  if (!walk) return access.GetInVehicle(vehicle, seat, bestDistance > 4, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
  utility.AddAction(new SCR_AIGetInVehicle(utility, null, vehicle, seat, EAICompartmentType.Pilot));
  return true;
 }

 static ChimeraCharacter DriverOf(IEntity vehicle)
 {
  if (!vehicle) return null;
  CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
  if (!car || !car.GetPilotCompartmentSlot()) return null;
  return ChimeraCharacter.Cast(car.GetPilotCompartmentSlot().GetOccupant());
 }

 protected bool IsMember(IEntity body)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (agent.GetControlledEntity() == body) return true;
  return false;
 }

 // A living member of this squad is manning one of the vehicle's guns.
 bool HasGunner(IEntity vehicle)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body)) continue;
   if (ACR_Transport.VehicleOf(body) == vehicle && ACR_Transport.SeatOf(body) == ECompartmentType.TURRET) return true;
  }
  return false;
 }

 protected IEntity m_DriverlessGun;

 // A living squad member in a turret of this vehicle whose gun still has ammunition.
 bool HasArmedGunner(IEntity vehicle)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || !ACR_Respawn.IsAlive(body) || body.GetCharacterController().IsUnconscious()) continue;
   if (ACR_Transport.VehicleOf(body) != vehicle || ACR_Transport.SeatOf(body) != ECompartmentType.TURRET) continue;
   CompartmentAccessComponent access = body.GetCompartmentAccessComponent();
   if (access && BBB_VehicleHelpers.TurretHasAmmo(access.GetCompartment())) return true;
  }
  return false;
 }

 static bool HasTurret(IEntity vehicle)
 {
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return false;
  array<BaseCompartmentSlot> turrets = {};
  compartments.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
  return !turrets.IsEmpty();
 }

 // The helicopter this squad crews (pilot/gunner seats), if any. AI can't fly in Reforger:
 // any waypoint makes a helicopter crew get out and walk (4 Oct MP: a player flying a
 // Game Master helicopter gave it a map move, the co-pilot bailed; after he left the seat the
 // pilot bailed too, walking to that move). Such a crew only ever holds or gets out.
 IEntity Aircraft()
 {
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (!vehicle)
  {
   IEntity leader = GetLeader();
   if (leader && !ACR_Transport.Passenger(leader)) vehicle = ACR_Transport.VehicleOf(leader);
  }
  if (vehicle && vehicle.FindComponent(VehicleHelicopterSimulation)) return vehicle;
  return null;
 }

 static bool NativeCommandAllowed(bool aircraftCrew)
 {
  return !aircraftCrew;
 }

 // Can fight: infantry of two or more, or a vehicle crew with a manned gun.
 // A lone driver or an unarmed truck crew is not sent to take or hold ground.
 bool IsCombatUnit()
 {
  if (m_bMortar) return false; // fire support, kept out of the fighting (ACR_Mortars)
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (vehicle) return HasGunner(vehicle);
  return CountAlive() >= 2;
 }

 // Suitability as a base of fire (support-role teams are not).
 int FirePower()
 {
  if (m_Group && m_Group.m_eACRFormation != ACR_EFormation.NONE)
  {
   if (ACR_InfantryCatalog.Specialist(m_Group.m_eACRFormation)) return 0;
   return CountAlive();
  }
  string name = m_Name;
  name.ToLower();
  if (name.Contains("logistic") || name.Contains("medic") || name.Contains("engineer") || name.Contains("sapper") || name.Contains("demolition") || name.Contains("pioneer") || name.Contains("supply") || name.Contains("sniper"))
   return 0;
  int value = CountAlive();
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (vehicle && HasGunner(vehicle)) value += 6;
  return value;
 }

 protected void DriveWatch(float now, bool contact)
 {
  if (m_bUnsticking || !m_Group) return;
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (!vehicle) return;
  if (!CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent))) return;

  Physics phys = vehicle.GetPhysics();
  float speed = 0;
  if (phys) speed = phys.GetVelocity().Length();

  ChimeraCharacter driver = DriverOf(vehicle);
  ACR_Manager manager = ACR_Manager.GetInstance();

  bool hitRecently = BBB_Transport.VehicleHitRecently(vehicle, 6000);
  bool underFire = (contact && KnownEnemies(300) > 0) || hitRecently;

  if (underFire && (!driver || !ACR_Respawn.IsAlive(driver)))
  {
   if (speed < 2.0)
   {
    // A gunner with ammo keeps a stopped gun in the fight (20 Oct Everon: a man on a
    // parked HMMWV M2 was pulled out every second, BTR and M1025 crews abandoned
    // working guns when the driver fell). Only other squads riding along get out.
    if (HasArmedGunner(vehicle))
    {
     if (m_DriverlessGun != vehicle)
     {
      m_DriverlessGun = vehicle;
      PrintFormat("[ACR] DRIVE %1: no driver under fire, gunner keeps fighting", m_Name);
     }
     if (manager)
     {
      array<ACR_Squad> cargo = {};
      ACR_Transport.PassengerSquads(manager, vehicle, cargo);
      foreach (ACR_Squad rider : cargo)
       if (rider != this && !rider.HasPlayer() && rider.OrderState() != ACR_EOrderState.DISMOUNT) rider.DismountNow();
     }
     return;
    }
    Bail(vehicle, "driver lost under fire", true);
    return;
   }
  }
  m_DriverlessGun = null;

  bool eligible = driver != null && ACR_Respawn.IsAlive(driver) && IsMember(driver) && FindController(driver) == 0;
  if (eligible && manager && manager.IsGunnerDriven(vehicle)) eligible = false;
  LaneSpeed(vehicle, eligible);
  bool wantsToMove;
  if (eligible)
  {
   array<AIWaypoint> waypoints = {};
   m_Group.GetWaypoints(waypoints);
   bool movingOrder = m_sOrderPrefab == WP_MOVE || m_sOrderPrefab == WP_DEFEND || m_sOrderPrefab == WP_SEARCH_DESTROY || m_sOrderPrefab == WP_GARRISON;
   wantsToMove = !waypoints.IsEmpty() && movingOrder && vector.DistanceXZ(vehicle.GetOrigin(), m_MoveTarget) > STALL_ARRIVED;
  }
  ACR_Transport.SanitizeVehicleHorn(vehicle, !eligible || !wantsToMove);
  if (!wantsToMove || vehicle != m_StallVehicle || (speed > 1.0 && vector.DistanceXZ(vehicle.GetOrigin(), m_StallPos) > 5))
  {
   if (vehicle != m_StallVehicle || !wantsToMove) { m_iUnsticks = 0; m_iReroutes = 0; }
   else if (m_iUnsticks > 0 && vector.DistanceXZ(vehicle.GetOrigin(), m_vUnstickFrom) > UNSTICK_CLEAR) m_iUnsticks = 0; // got past it
   m_StallVehicle = vehicle;
   if (vehicle) m_StallPos = vehicle.GetOrigin();
   m_fStallSince = now;
   m_fBoardingSince = -1;
   return;
  }
  // Waiting for its own men to get in (or out) isn't being stuck; under fire the usual limits apply.
  if (!underFire && CrewBoarding(vehicle))
  {
   if (m_fBoardingSince < 0)
   {
    m_fBoardingSince = now;
    PrintFormat("[ACR] DRIVE %1: waiting for its crew to get in or out, not stuck", m_Name);
   }
   if (now - m_fBoardingSince < BOARDING_WAIT_MS)
   {
    m_fStallSince = now;
    return;
   }
  }
  else m_fBoardingSince = -1;
  float stalled = (now - m_fStallSince) / 1000;
  bool armed = HasGunner(vehicle);
  // Fighting vehicles (anything with a turret: LAV, BTR, BRDM, gun trucks) keep
  // their crew aboard; only soft, unarmed vehicles are abandoned. A taxi isn't one: a ride never
  // takes a vehicle with a manned gun, so holding it stuck only stranded its riders (#23: a stuck
  // LAV taxi held, a new order that dropped the ride); it re-routes like a soft vehicle.
  bool fightingVehicle = HasTurret(vehicle) && !(manager && manager.m_Rides && manager.m_Rides.RideOf(this) != null);

  if (underFire && speed < 3.5 && manager)
  {
   array<ACR_Squad> riders = {};
   ACR_Transport.PassengerSquads(manager, vehicle, riders);
   foreach (ACR_Squad rider : riders)
   {
    if (rider != this && !rider.HasPlayer() && rider.OrderState() != ACR_EOrderState.DISMOUNT)
    {
     PrintFormat("[ACR] DRIVE %1: ordering cargo riders %2 to dismount under fire", m_Name, rider.GetName());
     rider.DismountAndContinue(rider.m_sOrderPrefab, rider.m_MoveTarget, rider.m_fOrderRadius, rider.m_Order, rider.m_OrderKind);
    }
   }
  }

  if (underFire && !fightingVehicle && stalled >= STALL_SECONDS_HELPLESS) { Bail(vehicle, "stopped under fire, unarmed", false); return; }
  float limit = s_fStallSeconds;
  if (underFire || armed) limit = STALL_SECONDS_FIGHTING;
  if (stalled < limit) return;
  if (m_iUnsticks >= UNSTICK_TRIES)
  {
   if (fightingVehicle)
   {
    // Hold where it is and fight from there instead of walking away from the vehicle.
    Halt("Stuck, holding");
    m_iUnsticks = 0;
    m_iReroutes = 0;
    m_fStuckAt = now;
    PrintFormat("[ACR] DRIVE %1: stuck, holding position (armed vehicle)", m_Name);
    return;
   }
   if (m_iReroutes < REROUTE_TRIES && Reroute(vehicle))
   {
    m_iReroutes++;
    m_iUnsticks = 0;
    m_StallPos = vehicle.GetOrigin();
    m_fStallSince = now;
    return;
   }
   StuckHold(vehicle, now);
   return;
  }
  m_iUnsticks++;
  BeginUnstick(vehicle, driver);
 }

 // Our AI driver takes our own roadblock's lane slowly (ACR_Roadblocks.LANE_SPEED_KMH within
 // LANE_SLOW_RANGE of it): its chicane has a truck's width to spare, not a truck at road speed. The
 // limit goes once it is past, or when the squad changes vehicle or drivers.
 protected IEntity m_LaneSlowVehicle;
 protected void LaneSpeed(IEntity vehicle, bool aiDriven)
 {
  ACR_Roadblock near = null;
  if (vehicle && GetFaction()) near = ACR_Roadblocks.OwnNear(vehicle.GetOrigin(), GetFaction().GetFactionKey());
  // Its holders step out of the way of any of our vehicles, AI-driven or not.
  if (near) ACR_RoadblockLane.ClearFor(near, vehicle);
  ACR_Roadblock rb = null;
  if (aiDriven) rb = near;
  if (m_LaneSlowVehicle && (m_LaneSlowVehicle != vehicle || !rb))
  {
   AICarMovementComponent was = AICarMovementComponent.Cast(m_LaneSlowVehicle.FindComponent(AICarMovementComponent));
   if (was) was.ResetCruiseSpeed();
   PrintFormat("[ACR] DRIVE %1: past the roadblock lane, road speed again", m_Name);
   m_LaneSlowVehicle = null;
  }
  if (!rb) return;
  AICarMovementComponent movement = AICarMovementComponent.Cast(vehicle.FindComponent(AICarMovementComponent));
  if (!movement) return;
  movement.SetCruiseSpeed(ACR_Roadblocks.LANE_SPEED_KMH);
  if (m_LaneSlowVehicle == vehicle) return;
  m_LaneSlowVehicle = vehicle;
  PrintFormat("[ACR] DRIVE %1: slowing to %2 km/h through our roadblock lane at %3", m_Name, ACR_Roadblocks.LANE_SPEED_KMH, rb.m_vPos);
 }

 // A fresh road route from where the vehicle stands to where it was going, carried
 // out as route legs of the current order (no new serial: rides stay on).
 protected bool Reroute(IEntity vehicle)
 {
  vector here = vehicle.GetOrigin();
  vector goal = m_MoveTarget;
  if (!m_aRoute.IsEmpty()) goal = m_aRoute[m_aRoute.Count() - 1];
  ResourceName prefab = m_sOrderPrefab;
  float radius = m_fOrderRadius;
  if (m_Route && !m_aRoute.IsEmpty()) { prefab = m_Route.m_sPrefab; radius = m_Route.m_fRadius; }
  if (prefab.IsEmpty()) return false;
  array<vector> legs = {};
  int nodes, millis;
  if (!BBB_Roads.Route(here, goal, legs, nodes, millis) || legs.IsEmpty())
  {
   legs.Clear();
   vector road;
   if (!BBB_Terrain.RoadPointNear(here, 300, road))
   {
    PrintFormat("[ACR] DRIVE %1: stuck, no road within 300 m to re-route by", m_Name);
    return false;
   }
   legs.Insert(road);
  }
  // Don't hand back the spot it is stuck at as the first hop.
  if (legs.Count() > 1 && vector.DistanceXZ(legs[0], here) < 20) legs.RemoveOrdered(0);
  if (vector.DistanceXZ(legs[legs.Count() - 1], goal) > 15) legs.Insert(goal);
  if (legs.Count() < 2 && vector.DistanceXZ(legs[0], here) < 20)
  {
   PrintFormat("[ACR] DRIVE %1: stuck, re-route leads back here", m_Name);
   return false;
  }
  ACR_OrderSnapshot order = ACR_OrderSnapshot.Make(prefab, vector.Zero, radius, m_Order, OrderState(), m_OrderKind);
  m_aRoute.Clear();
  foreach (vector leg : legs) m_aRoute.Insert(leg);
  m_Route = order;
  NextLeg();
  PrintFormat("[ACR] DRIVE %1: stuck, re-routed over the roads (%2 hops)", m_Name, legs.Count());
  return true;
 }

 // Given up on driving: the crew stays with the vehicle and holds; AI passengers get
 // out and carry on (a ride sends its riders on to the objective on foot).
 protected void StuckHold(IEntity vehicle, float now)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager)
  {
   array<ACR_Squad> riders = {};
   ACR_Transport.PassengerSquads(manager, vehicle, riders);
   foreach (ACR_Squad rider : riders) if (rider != this && !rider.HasPlayer()) rider.DismountNow();
  }
  Halt("Stuck, holding");
  m_iStuckCount++;
  m_StallVehicle = null;
  m_iUnsticks = 0;
  m_iReroutes = 0;
  m_fStuckAt = now;
  if (vehicle) ACR_Transport.SanitizeVehicleHorn(vehicle, true);
  PrintFormat("[ACR] DRIVE %1: stuck, crew stays aboard and holds", m_Name);
 }

 // A living, conscious member of this squad is getting in or out of a vehicle, or is on foot
 // within BOARDING_RANGE of this one (#53).
 bool CrewBoarding(IEntity vehicle)
 {
  if (!vehicle) return false;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || !ACR_Respawn.IsAlive(body) || body.GetCharacterController().IsUnconscious()) continue;
   CompartmentAccessComponent access = body.GetCompartmentAccessComponent();
   if (access && (access.IsGettingIn() || access.IsGettingOut())) return true;
   if (!ACR_Transport.VehicleOf(body) && vector.DistanceXZ(body.GetOrigin(), vehicle.GetOrigin()) < BOARDING_RANGE) return true;
  }
  return false;
 }

 protected void BeginUnstick(IEntity vehicle, ChimeraCharacter driver)
 {
  m_bUnsticking = true;
  m_vUnstickFrom = vehicle.GetOrigin();
  m_iStuckCount++;
  float steer = 0.8;
  if (m_iUnsticks % 2 == 0) steer = -0.8;
  if (!m_UnstickListener) m_UnstickListener = new ACR_SquadUnstick(this);
  // BBB_Unstick parks the driver's AI and reverses (inputs applied every frame).
  BBB_Unstick.Begin(vehicle, driver, steer, m_UnstickListener);
  PrintFormat("[ACR] DRIVE %1: stuck, backing up (try %2)", m_Name, m_iUnsticks);
 }

 // Backing out is over (BBB_Unstick has given the driver his AI back).
 void EndUnstick()
 {
  m_bUnsticking = false;
  if (m_StallVehicle)
  {
   m_StallPos = m_StallVehicle.GetOrigin();
   ACR_Transport.SanitizeVehicleHorn(m_StallVehicle, true);
  }
  m_fStallSince = GetGame().GetWorld().GetWorldTime();
  // Fresh order, so the driver plans a new route from here.
  if (m_Group && !m_sOrderPrefab.IsEmpty())
  {
   // Label and state stay as they are.
   int kind = m_OrderKind;
   IssueWaypoint(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius);
   m_OrderKind = kind;
  }
 }

 // Everyone out of the vehicle, passengers included. With resume the crew then
 // carries on to its destination on foot; otherwise it takes cover where it is.
 protected void Bail(IEntity vehicle, string why, bool resume)
 {
  PrintFormat("[ACR] DRIVE %1: %2, crew bails out", m_Name, why);
  if (vehicle) ACR_Transport.SanitizeVehicleHorn(vehicle, true);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager)
  {
   array<ACR_Squad> riders = {};
   ACR_Transport.PassengerSquads(manager, vehicle, riders);
   foreach (ACR_Squad rider : riders) if (rider != this && !rider.HasPlayer()) rider.DismountNow();
  }
  // Anyone whose AI was parked (backing up, gunner drive) must be able to walk away.
  array<AIAgent> crew = {};
  GetAgents(crew);
  foreach (AIAgent member : crew)
  {
   IEntity body = member.GetControlledEntity();
   if (!body || FindController(body) != 0) continue;
   AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
   if (control && !control.IsAIActivated()) control.ActivateAI();
  }
  ResourceName prefab = m_sOrderPrefab;
  if (!resume) prefab = "";
  DismountAndContinue(prefab, m_MoveTarget, m_fOrderRadius, m_Order, m_OrderKind);
  m_StallVehicle = null;
  m_iUnsticks = 0;
 }

 // Everyone out now; then carry out this order on foot (or, with no prefab,
 // take positions where they got out).
 void DismountAndContinue(ResourceName prefab, vector target, float radius, string order, int kind)
 {
  DismountNow();
  ContinueAfterDismount(prefab, target, radius, order, kind);
 }

 // For a squad already getting out: what to do once everyone is on the ground.
 void ContinueAfterDismount(ResourceName prefab, vector target, float radius, string order, int kind)
 {
  m_bResumeOnFoot = !prefab.IsEmpty();
  m_Resume = ACR_OrderSnapshot.Make(prefab, target, radius, order, StateOfLabel(order), kind);
 }
 protected ref ACR_OrderSnapshot m_Resume; // carried on on foot after a dismount

 // ---------------------------------------------------------------- transport
 vector m_LZ;
 bool m_bHasLZ;
 float m_fLZSetAt;

 // Everyone out now, however the vehicle is sitting (hover, roof, water):
 // the normal dismount first, a teleport-out if that is refused.
 int DismountNow()
 {
  int dropped = GetEveryoneOut();
  m_bHasLZ = false;
  Halt("Dismounting", ACR_EOrderState.DISMOUNT);
  m_iDismountSweeps = 0;
  // One follow-up per squad: a second dismount (bail-out during a ride, F6 twice)
  // used to leave two queued, and the later one replaced the resumed order with
  // "Holding position".
  GetGame().GetCallqueue().Remove(HoldAfterDismount);
  GetGame().GetCallqueue().CallLater(HoldAfterDismount, 3000, false);
  return dropped;
 }

 // A job (mines, a roadblock) given while aboard a ride: everyone out, the job left as it is to
 // carry on on foot. DismountNow's Halt would end it, refunded (25 Oct playtest: a minefield
 // ordered to a squad on a truck was paid and refunded in the same second, the squad then
 // staying aboard under the job's label).
 void GetOutForJob()
 {
  GetEveryoneOut();
  m_bHasLZ = false;
  m_iDismountSweeps = 0;
  GetGame().GetCallqueue().Remove(SweepOutForJob);
  GetGame().GetCallqueue().CallLater(SweepOutForJob, 3000, false);
 }

 protected void SweepOutForJob()
 {
  if (!IsMounted() || m_iDismountSweeps >= 3) return;
  m_iDismountSweeps++;
  PushOutStillAboard();
  GetGame().GetCallqueue().CallLater(SweepOutForJob, 3000, false);
 }

 // An order of its own the squad works at (paid when given, refunded if a new order ends it).
 bool HasJob()
 {
  return m_MinePlan != null || m_FortifyPlan != null || RoadblockPending() || OrderState() == ACR_EOrderState.BUILD_ROADBLOCK;
 }

 protected int GetEveryoneOut()
 {
  int dropped;
  array<IEntity> leaving = {};
  GetVehicles(leaving);
  ForgetVehicles(leaving);
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !ACR_Respawn.IsAlive(character) || FindController(character) != 0) continue; // a player gets out himself
   CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
   if (!access) continue;
   // Still climbing in: stop, so nobody is left behind on board.
   if (!character.IsInVehicle())
   {
    if (access.IsGettingIn()) access.InterruptVehicleActionQueue(true, true, true);
    continue;
   }
   if (!access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.LEAVE_OPEN, false))
    access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   dropped++;
  }
  return dropped;
 }

 // Put members near the vehicle into its free passenger seats. The game's AI
 // won't board a vehicle another group is driving, so this is done directly:
 // climbing in normally when close, or placed in once they've had time to walk up.
 // forceAll: also seat soldiers who are walking up but stuck (the last one or two of a
 // squad often never made it in, 1 Oct live test).
 int BoardCargo(IEntity vehicle, float reach, bool placeFar, bool forceAll = false)
 {
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
  int started;
  array<BaseCompartmentSlot> taken = {};
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   // (a player gets in himself: never placed into a seat)
   if (!character || !ACR_Respawn.IsAlive(character) || character.IsInVehicle() || FindController(character) != 0) continue;
   CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
   float distance = vector.DistanceXZ(character.GetOrigin(), vehicle.GetOrigin());
   if (!access || (access.IsGettingIn() && !forceAll)) continue;
   bool teleport = distance > 4 || forceAll;
   if (distance > reach || (teleport && !placeFar)) continue;
   foreach (BaseCompartmentSlot seat : seats)
   {
    if (seat.GetOccupant() || taken.Contains(seat) || (seat.IsReserved() && !seat.IsReservedBy(character))) continue;
    taken.Insert(seat);
    bool ok = access.GetInVehicle(vehicle, seat, teleport, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
    if (System.IsCLIParam("acrRideTest")) PrintFormat("[ACR] BOARD %1 m, seat %2, placed %3 -> %4", (int)distance, seat.GetCompartmentName(), teleport, ok);
    if (ok)
    {
     started++;
     break;
    }
   }
  }
  return started;
 }

 // The game's AI treats vehicles it has used as its own and climbs back in;
 // once we've ordered a squad out, it should stay out.
 void ForgetVehicles(array<IEntity> vehicles)
 {
  if (!m_Group || vehicles.IsEmpty()) return;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  if (!utility) return;
  foreach (IEntity vehicle : vehicles)
  {
   SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
   if (usage && utility.IsUsableVehicle(usage)) utility.RemoveUsableVehicle(usage);
  }
 }

 protected int m_iDismountSweeps;

 // Once out, take positions where they landed (sweeping out any late boarder first).
 // Members still aboard after a dismount: out again, placed out from the second sweep (6 s: the
 // animated exit was accepted but never happened, 5 Oct: LAV passengers stayed in after a drop-off).
 protected void PushOutStillAboard()
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !character.IsInVehicle() || !character.GetCompartmentAccessComponent() || FindController(character) != 0) continue;
   SweepOut(character, character.GetCompartmentAccessComponent(), m_iDismountSweeps);
  }
 }

 // One man still aboard out, by the sweep's step (the drop's PushOutStillAboard, and a Get out
 // order's EnsureGetOut from its fourth check). True when an exit was taken.
 protected bool SweepOut(ChimeraCharacter character, CompartmentAccessComponent access, int step)
 {
  // A seat whose exits are all refused (the LAV-25's CARGO seats, #23: animated, teleport,
  // no-door and the game's own eject alike, 26 Oct): what the game says of him, then his vehicle
  // action queue cleared (a stuck action blocks every new one) and out by teleport on the fourth
  // sweep, without a door on the fifth; on the sixth he moves to a free seat, to go out from there.
  if ((step == 4 || step == 5) && ClearAndGetOut(character, access, step == 5)) return true;
  if (step == 6 && MoveToFreeSeat(character, access)) return true;
  // Still aboard on the third sweep (9 s): the teleport through a door left him in too (an LAV's
  // rear seats, 25 Oct): out without a door, as vanilla clears a seat (SCR_AIChangeCompartment).
  if (step >= 3)
  {
   vector spot[4];
   access.FindSuitableTeleportLocation(spot);
   bool noDoor = access.GetOutVehicle_NoDoor(spot, false, false);
   PrintFormat("[ACR] GET OUT %1: %2 still aboard after a dismount, taken out without a door", m_Name, character);
   return noDoor;
  }
  if (step >= 2)
  {
   bool placed = access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   PrintFormat("[ACR] GET OUT %1: %2 still aboard after a dismount, placed out", m_Name, character);
   return placed;
  }
  if (access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.LEAVE_OPEN, false)) return true;
  return access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
 }

 // His vehicle action queue cleared, then out by teleport (noDoor: without a door, where the game
 // finds room). Logs what the game says of him first. True when the exit was taken.
 protected bool ClearAndGetOut(ChimeraCharacter character, CompartmentAccessComponent access, bool noDoor)
 {
  BaseCompartmentSlot seat = access.GetCompartment();
  string seatName = "none";
  if (seat) seatName = seat.GetCompartmentName();
  IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(character);
  float speed = -1;
  if (vehicle && vehicle.GetPhysics()) speed = vehicle.GetPhysics().GetVelocity().Length();
  PrintFormat("[ACR] GET OUT %1: %2 in seat %3: can get out %4, can jump out %5, getting in %6, getting out %7, switching seats %8, vehicle speed %9 m/s", m_Name, character, seatName, access.CanGetOutVehicle(), access.CanJumpOutVehicle(), access.IsGettingIn(), access.IsGettingOut(), access.IsSwitchingSeatsAnim(), speed);
  access.InterruptVehicleActionQueue(true, true, true);
  bool gotOut;
  if (noDoor)
  {
   vector spot[4];
   access.FindSuitableTeleportLocation(spot);
   gotOut = access.GetOutVehicle_NoDoor(spot, false, false);
  }
  else gotOut = access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
  PrintFormat("[ACR] GET OUT %1: %2 action queue cleared, out (no door %3): %4", m_Name, character, noDoor, gotOut);
  return gotOut;
 }

 // Into another free passenger seat of the same vehicle (one in his own section first), as the
 // game's switch-seat action does it (GetInVehicle while aboard); false when there is none.
 protected bool MoveToFreeSeat(ChimeraCharacter character, CompartmentAccessComponent access)
 {
  BaseCompartmentSlot current = access.GetCompartment();
  IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(character);
  if (!current || !vehicle) return false;
  BaseCompartmentManagerComponent seats = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
  if (!seats) return false;
  array<BaseCompartmentSlot> slots = {};
  seats.GetCompartments(slots);
  BaseCompartmentSlot pick;
  foreach (BaseCompartmentSlot slot : slots)
  {
   if (!slot || slot == current || slot.IsOccupied() || !slot.IsCompartmentAccessible() || slot.GetType() != ECompartmentType.CARGO) continue;
   if (!pick || (slot.GetCompartmentSection() == current.GetCompartmentSection() && pick.GetCompartmentSection() != current.GetCompartmentSection())) pick = slot;
  }
  if (!pick) return false;
  bool moved = access.GetInVehicle(vehicle, pick, true, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
  PrintFormat("[ACR] GET OUT %1: %2 can't leave seat %3, moved to seat %4 first: %5", m_Name, character, current.GetCompartmentName(), pick.GetCompartmentName(), moved);
  return moved;
 }

 void HoldAfterDismount()
 {
  // Seven sweeps: the fourth and fifth clear the action queue of a man no exit gets out, the
  // sixth moves him to another seat, the seventh gets him out from there.
  if (IsMounted() && m_iDismountSweeps < 7)
  {
   m_iDismountSweeps++;
   PushOutStillAboard();
   GetGame().GetCallqueue().CallLater(HoldAfterDismount, 3000, false);
   return;
  }
  IEntity anchor = GetAnchor();
  if (!m_Group || !anchor) return;
  if (m_bResumeOnFoot)
  {
   m_bResumeOnFoot = false;
   RestoreOrder(m_Resume);
   return;
  }
  // Led by a player: out and with him, not a defend order his command-wheel orders queue behind.
  if (m_Commander != 0)
  {
   FollowCommander();
   return;
  }
  IssueWaypoint(WP_DEFEND, anchor.GetOrigin(), 20);
  SetOrder("Holding position", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
 }

 int CountConscious()
 {
  int awake;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (IsConscious(agent.GetControlledEntity())) awake++;
  return awake;
 }

 int CountAlive()
 {
  int alive;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && !character.GetCharacterController().IsDead()) alive++;
  }
  return alive;
 }

 // Soldiers the engine took away for its active-AI limit (group dormant); they still count
 // toward the soldier cap so the AI doesn't keep buying empty squads.
 int CountDormant()
 {
  if (!m_Group || !m_Group.IsDormant()) return 0;
  return Math.Max(0, m_Group.GetDormantAliveCount());
 }

 // Rough fighting value: soldiers, plus 5 per armed (manned gun) vehicle (Arma Commander's default vehicle value).
 int Strength()
 {
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  int armed;
  foreach (IEntity vehicle : vehicles) if (HasGunner(vehicle)) armed++;
  return CountAlive() + armed * 5;
 }

 bool HasPlayer()
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (FindController(agent.GetControlledEntity())) return true;
  return false;
 }

 // Board a specific vehicle if one is within 30 m of the point, otherwise the nearest free one.
 bool GetIn(vector position)
 {
  return BoardVehicle(FindVehicleNear(position, 30), position);
 }

 // Board this vehicle (or, without one, the nearest free vehicle to the point).
 bool BoardVehicle(Vehicle vehicle, vector position)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (vehicle)
  {
   if (!IssueWaypoint(WP_GET_IN, vehicle.GetOrigin(), -1, true, vehicle)) return false;
  }
  else if (!IssueWaypoint(WP_GET_IN_NEAREST, position, 20, true)) return false;
  SetOrder("Getting in", ACR_EOrderState.GET_IN);
  m_OrderKind = ACR_EOrderKind.GET_IN;
  TrackBoarding(vehicle);
  return true;
 }

 protected void TrackBoarding(IEntity vehicle)
 {
  GetGame().GetCallqueue().Remove(EnsureBoardVehicle);
  m_BoardTarget = vehicle;
  m_iBoardSerial = m_iOrderSerial;
  float approach = 0;
  IEntity boardAnchor = GetAnchor();
  if (vehicle && boardAnchor) approach = vector.DistanceXZ(boardAnchor.GetOrigin(), vehicle.GetOrigin()) / 1.2 * 1000;
  m_fBoardUntil = GetGame().GetWorld().GetWorldTime() + Math.Clamp(approach + 60000, 60000, 900000);
  if (vehicle && m_OwnVehicle != vehicle) GetGame().GetCallqueue().CallLater(EnsureBoardVehicle, 8000, false);
 }

 void CancelLostBoarding()
 {
  Halt("Boarding vehicle lost");
  PrintFormat("[ACR] BOARD %1: target lost, order cancelled", m_Name);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && GetFaction()) manager.NotifyFaction(GetFaction().GetFactionKey(), m_Name + ": boarding cancelled, vehicle lost.");
 }

 protected void WatchBoardingTarget()
 {
  if (OrderState() != ACR_EOrderState.GET_IN || !m_Group || m_Group.GetCurrentWaypoint() != m_Waypoint) return;
  SCR_BoardingEntityWaypoint boarding = SCR_BoardingEntityWaypoint.Cast(m_Waypoint);
  if (boarding && (!boarding.GetEntity() || ACR_Logistics.Health(boarding.GetEntity()) <= 0)) CancelLostBoarding();
 }

 protected IEntity m_BoardTarget;
 protected int m_iBoardSerial;
 protected float m_fBoardUntil;

 // Native Get In can refuse passengers when another group owns the driver.
 // After allowing its normal approach/animation, seat only nearby AI passengers.
 // A new order, player control or a lost vehicle cancels this fallback.
 protected void EnsureBoardVehicle()
 {
  if (!m_Group || !m_BoardTarget || m_iBoardSerial != m_iOrderSerial || OrderState() != ACR_EOrderState.GET_IN || HasPlayer() || ACR_Logistics.Health(m_BoardTarget) <= 0) return;
  if (GetGame().GetWorld().GetWorldTime() >= m_fBoardUntil) return;
  int remaining;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (agent && ACR_Respawn.IsAlive(agent.GetControlledEntity()) && ACR_Transport.VehicleOf(agent.GetControlledEntity()) != m_BoardTarget) remaining++;
  if (remaining == 0) { ClearOrders(); return; }
  IEntity anchor = GetAnchor();
  if (anchor && !IsMounted() && vector.DistanceXZ(anchor.GetOrigin(), m_BoardTarget.GetOrigin()) > 20)
  {
   if (!m_Group.GetCurrentWaypoint() || (m_sOrderPrefab == WP_MOVE && vector.DistanceXZ(m_MoveTarget, m_BoardTarget.GetOrigin()) > 20))
    IssueWaypoint(WP_MOVE, m_BoardTarget.GetOrigin(), 15, true);
  }
  float height, speed;
  int seated;
  if (BBB_Transport.Settled(m_BoardTarget, height, speed)) seated = BoardCargo(m_BoardTarget, 20, true, true);
  if (seated > 0) PrintFormat("[ACR] BOARD %1: %2 nearby passengers seated in assigned vehicle", m_Name, seated);
  GetGame().GetCallqueue().CallLater(EnsureBoardVehicle, 3000, false);
 }

 bool GetOut()
 {
  NewOrder();
  ClearPlans();
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  if (vehicles.IsEmpty()) return false;
  if (!IssueWaypoint(WP_GET_OUT, vehicles[0].GetOrigin(), -1, true)) return false;
  SetOrder("Getting out", ACR_EOrderState.GET_OUT);
  m_OrderKind = ACR_EOrderKind.GET_OUT;
  m_iGetOutChecks = 0;
  m_iGetOutSerial = m_iOrderSerial;
  GetGame().GetCallqueue().Remove(EnsureGetOut);
  if (!s_bNoGetOutFallback) GetGame().GetCallqueue().CallLater(EnsureGetOut, 4000, false);
  return true;
 }

 // The game's Get out only asks each passenger to leave. Passengers in a vehicle
 // another squad crews (a requisitioned Humvee's driver) stayed aboard (1 Oct MP),
 // so whoever is still inside once the vehicle has stopped is taken out directly.
 static bool s_bNoGetOutFallback; // tests: the game's waypoint alone
 static bool s_bNoGetOutSweeps;   // tests: the fallback without the drop's sweeps (as before)
 protected int m_iGetOutChecks;
 protected int m_iGetOutSerial;
 void EnsureGetOut()
 {
  if (!m_Group || m_iGetOutSerial != m_iOrderSerial || OrderState() != ACR_EOrderState.GET_OUT || !IsMounted()) return;
  m_iGetOutChecks++;
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  ForgetVehicles(vehicles);
  int stillIn, moved;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !character.IsInVehicle() || !ACR_Respawn.IsAlive(character)) continue;
   CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
   if (!access) continue;
   stillIn++;
   IEntity vehicle = access.GetVehicleIn(character);
   float speed;
   Physics physics;
   if (vehicle) physics = vehicle.GetPhysics();
   if (physics) speed = physics.GetVelocity().Length();
   if (speed > 1.5) continue; // the driver is someone else's: wait until it stops
   bool ok;
   // From the fourth check (13 s) the drop's sweeps (SweepOut): the LAV-25's cargo seats refuse
   // the game's exits (#23), and a player's Get out only ever tried those (pn10b, 26 Oct: "check
   // 1-5, 2 still aboard, 0 sent out" for Fire Team 2's LAV, until the stuck-soldier fallback set
   // them down). Out by the seventh check (22 s).
   if (m_iGetOutChecks >= 4 && !s_bNoGetOutSweeps) ok = SweepOut(character, access, m_iGetOutChecks);
   else
   {
    if (access.IsGettingOut()) continue;
    ok = access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
    if (!ok && m_iGetOutChecks >= 3) ok = access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   }
   if (ok) moved++;
  }
  PrintFormat("[ACR] GET OUT %1: check %2, %3 still aboard, %4 sent out", m_Name, m_iGetOutChecks, stillIn, moved);
  if (m_iGetOutChecks < 10) GetGame().GetCallqueue().CallLater(EnsureGetOut, 3000, false);
 }

 // Vehicles any member currently sits in.
 void GetVehicles(array<IEntity> vehicles)
 {
  vehicles.Clear();
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !character.IsInVehicle()) continue;
   CompartmentAccessComponent compartment = character.GetCompartmentAccessComponent();
   if (!compartment) continue;
   IEntity vehicle = compartment.GetVehicleIn(character);
   if (vehicle && !vehicles.Contains(vehicle)) vehicles.Insert(vehicle);
  }
 }

 protected Vehicle m_FoundVehicle;
 protected vector m_SearchVehicleAt;
 Vehicle FindVehicleNear(vector position, float radius)
 {
  m_FoundVehicle = null;
  m_SearchVehicleAt = position;
  m_fVehicleSearchDistance = radius;
  GetGame().GetWorld().QueryEntitiesBySphere(position, radius, CollectNearestVehicle);
  return m_FoundVehicle;
 }

 protected float m_fVehicleSearchDistance;
 protected bool CollectNearestVehicle(IEntity entity)
 {
  Vehicle vehicle = Vehicle.Cast(entity);
  if (!vehicle || ACR_Logistics.Health(vehicle) <= 0) return true;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments || !HasOpenSeat(compartments)) return true;
  float distance = vector.DistanceXZ(vehicle.GetOrigin(), m_SearchVehicleAt);
  if (distance <= m_fVehicleSearchDistance)
  {
   m_fVehicleSearchDistance = distance;
   m_FoundVehicle = vehicle;
  }
  return true;
 }

 protected bool HasOpenSeat(SCR_BaseCompartmentManagerComponent compartments)
 {
  array<ECompartmentType> types = {ECompartmentType.PILOT, ECompartmentType.TURRET, ECompartmentType.CARGO};
  foreach (ECompartmentType type : types)
  {
   array<BaseCompartmentSlot> slots = {};
   compartments.GetCompartmentsOfType(slots, type);
   foreach (BaseCompartmentSlot slot : slots)
    if (slot && !slot.GetOccupant()) return true;
  }
  return false;
 }

 // Where the squad is currently headed (any waypoint, including native orders).
 bool GetOrderTarget(out vector target, out int kind)
 {
  if (!m_Group) return false;
  AIWaypoint current = m_Group.GetCurrentWaypoint();
  if (!current) return false;
  target = current.GetOrigin();
  kind = 0;
  if (current == m_Waypoint) kind = m_OrderKind;
  return true;
 }

 // One line for the command panel, e.g. "4/4 alive | Combat | Moving".
 string Describe()
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  int alive;
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && !character.GetCharacterController().IsDead()) alive++;
  }
  string order = m_Order;
  if (m_Group && !m_Group.GetCurrentWaypoint() && FinishesOnArrival(OrderState())) order = "Holding";
  string text = string.Format("%1 alive | %2 | %3", alive, ModeName(m_Mode), order);
  text += " | HP " + m_iHealthPct.ToString() + "%";
  if (m_bFireAtWill) text += " | fire at will";
  if (m_bPatrol) text += string.Format(" | patrolling %1 points", m_aQueuePos.Count());
  if (m_iAmmoPct >= 0) text += " | ammo " + m_iAmmoPct.ToString() + "%";
  if (m_iFuelPct >= 0 && m_iFuelPct < 30) text += " | fuel " + m_iFuelPct.ToString() + "%";
  if (m_iStockPct >= 0) text += " | " + m_sServiceName + " stock " + m_iStockPct.ToString() + "%";
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  if (!vehicles.IsEmpty()) text += " | mounted";
  array<ResourceName> missing = {};
  GetMissing(missing);
  if (!missing.IsEmpty()) text += string.Format(" | %1 missing", missing.Count());
  return text;
 }

 // ---------------------------------------------------------------- status
 // Refreshed every few seconds on the server for the map markers and panel:
 // health (wounds and losses against full strength, and the vehicle's state),
 // ammo (magazines against what each soldier started with; guns for crews),
 // fuel, and for service vehicles their remaining stock.
 int m_iHealthPct = 100;
 int m_iAmmoPct = -1;
 int m_iFuelPct = -1;
 int m_iStockPct = -1;
 int m_iAvgHpPct = 100;       // average health of the living (for medics)
 string m_sServiceName;
 protected ref map<IEntity, float> m_MagazineBaseline = new map<IEntity, float>();

 int MagazineTarget(IEntity soldier)
 {
  float baseline;
  if (m_MagazineBaseline.Find(soldier, baseline)) return (int)Math.Round(baseline);
  return 6;
 }

 void UpdateStatus(ACR_Logistics logistics)
 {
  int full;
  foreach (ResourceName prefab, int count : m_FullStrength) full += count;
  array<AIAgent> agents = {};
  GetAgents(agents);
  float health;
  float have;
  float want;
  int alive;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   alive++;
   health += ACR_Logistics.Health(body);
   float magazines = ACR_Logistics.SoldierMagazines(body);
   float baseline;
   if (!m_MagazineBaseline.Find(body, baseline))
   {
    baseline = Math.Max(magazines, 1);
    m_MagazineBaseline.Set(body, baseline);
   }
   have += Math.Min(magazines, baseline);
   want += baseline;
  }
  int slots = Math.Max(full, alive);
  m_iHealthPct = 0;
  if (slots > 0) m_iHealthPct = (int)(100 * health / slots);
  m_iAvgHpPct = 100;
  if (alive > 0) m_iAvgHpPct = (int)(100 * health / alive);
  m_iAmmoPct = -1;
  if (want > 0) m_iAmmoPct = (int)(100 * have / want);
  m_iFuelPct = -1;
  m_iStockPct = -1;
  m_sServiceName = "";
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (!vehicle) return;
  m_iHealthPct = (int)Math.Min(m_iHealthPct, 100 * ACR_Logistics.Health(vehicle));
  float guns = ACR_Logistics.VehicleAmmo(vehicle);
  if (guns >= 0 && HasGunner(vehicle))
  {
   if (m_iAmmoPct < 0) m_iAmmoPct = (int)(100 * guns);
   else m_iAmmoPct = (int)Math.Min(m_iAmmoPct, 100 * guns);
  }
  float fuel = ACR_Logistics.Fuel(vehicle);
  if (fuel >= 0) m_iFuelPct = (int)(100 * fuel);
  if (logistics && ACR_Logistics.ServiceOf(vehicle) != 0)
  {
   m_iStockPct = logistics.StockPercent(vehicle);
   m_sServiceName = ACR_Logistics.ServiceName(ACR_Logistics.ServiceOf(vehicle));
  }
 }

 // ---------------------------------------------------------------- teardown

 void Dispose()
 {
  ACR_SquadAI.Forget(this);
  CancelEstablishFob();
  CancelRoadblock();
  if (m_RoadblockTurret) VacateRoadblockTurret();
  CancelMineWork("abandoned: the squad is gone");
  CancelFortify("abandoned: the squad is gone");
  m_Fortification = null;
  m_bUnsticking = false;
  if (m_UnstickListener) BBB_Unstick.Cancel(m_UnstickListener);
  m_Aside = null; // being removed: nothing to resume
  Release();
  GetGame().GetCallqueue().Remove(LinkSquad);
  GetGame().GetCallqueue().Remove(StepOntoNavmesh);
  GetGame().GetCallqueue().Remove(HoldAfterDismount);
  GetGame().GetCallqueue().Remove(EnsureGetOut);
  GetGame().GetCallqueue().Remove(EnsureBoardVehicle);
  GetGame().GetCallqueue().Remove(WakeCrew);
  // Detach orders before deleting them, including queued artillery/boarding orders.
  // A surviving native group must never keep following a deleted owned waypoint.
  ClearOrders();
  SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
  if (manager && m_CommandGroup) manager.DeleteAndUnregisterGroup(m_CommandGroup);
  if (m_Group && m_Group.GetAgentsCount() == 0) SCR_EntityHelper.DeleteEntityAndChildren(m_Group);
  m_CommandGroup = null;
 }

 void ~ACR_Squad()
 {
  if (!GetGame()) return;
  GetGame().GetCallqueue().Remove(LinkSquad);
  GetGame().GetCallqueue().Remove(StepOntoNavmesh);
  GetGame().GetCallqueue().Remove(HoldAfterDismount);
  GetGame().GetCallqueue().Remove(EnsureGetOut);
  GetGame().GetCallqueue().Remove(EnsureBoardVehicle);
  GetGame().GetCallqueue().Remove(WakeCrew);
 }
}
