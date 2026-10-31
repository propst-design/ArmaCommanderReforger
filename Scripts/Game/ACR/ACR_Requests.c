// Player requests to the server: possession, orders, rides, landing zones, unloading
// (split out of ACR_Manager, 2 Oct; the manager forwards its old entry points).
// What became of a player's order (R03, 5 Oct review): accepted or refused, plus the
// text for the player. Callers decide on m_bAccepted, never on the wording.
class ACR_OrderOutcome : Managed
{
 bool m_bAccepted;
 string m_sMessage;
 ACR_OrderOutcome Accept(string message) { m_bAccepted = true; m_sMessage = message; return this; }
 ACR_OrderOutcome Refuse(string message) { m_bAccepted = false; m_sMessage = message; return this; }
}

class ACR_Requests : Managed
{
 protected ACR_Manager m_Manager;

 void ACR_Requests(ACR_Manager manager) { m_Manager = manager; }

 string HandleRequest(SCR_PlayerController pc, int command, RplId targetId, vector point)
 {
  if (!Replication.IsServer() || !pc) return "Server unavailable";
  ChimeraCharacter body = ChimeraCharacter.Cast(pc.GetControlledEntity());
  // F8 must also work while the player is attached to the dead ragdoll.
  if (command == ACR_ERequest.RETURN) return ReturnToCommandUnit(pc, body);
  if (!body || body.GetCharacterController().IsDead()) return "Wait until you have spawned";
  switch (command)
  {
   case ACR_ERequest.POSSESS: return Possess(pc, body, targetId);
   case ACR_ERequest.SWITCH_TO_GROUP: return SwitchToGroup(pc, body, targetId);
   case ACR_ERequest.TAKE_GUNNER: return TakeGunner(pc, body, targetId);
   case ACR_ERequest.TAKE_COMMAND: return m_Manager.TakeCommand(pc);
   case ACR_ERequest.REINFORCE: return m_Manager.m_Requisition.Reinforce(pc, targetId);
   case ACR_ERequest.LEAVE_COMMAND: return m_Manager.LeaveCommand(pc);
   case ACR_ERequest.UNLOAD_HERE: return UnloadHere(pc, body);
  }
  return Order(pc, command, targetId, point);
 }

 string ReturnToCommandUnit(SCR_PlayerController pc, ChimeraCharacter body)
 {
  pc.ACR_CancelDeathTransition();
  if (!pc.IsPossessing()) return "You are already in your command unit";
  if (m_Manager.m_Respawn)
  {
   m_Manager.m_Respawn.CancelPendingPossession(pc.GetPlayerId());
   string waiting = m_Manager.m_Respawn.OnReturnRequested(pc);
   if (!waiting.IsEmpty()) return waiting;
  }
  ACR_MapHandoff.Release(pc, body);
  m_Manager.ReleaseAuthority(pc);
  pc.SetPossessedEntity(null);
  ACR_MapHandoff.SanitizePossessedWeapons(pc, ChimeraCharacter.Cast(pc.GetMainEntity()), true);
  ACR_MapHandoff.ClearCharacterInput(pc.GetControlledEntity());
  PrintFormat("[ACR] RETURN player=%1", pc.GetPlayerId());
  return "Returned to your command unit";
 }

 string Possess(SCR_PlayerController pc, ChimeraCharacter body, RplId targetId)
 {
  ChimeraCharacter target = ChimeraCharacter.Cast(SCR_EntityHelper.RplIdToEntity(targetId));
  if (!target) return "Aim directly at a friendly soldier, or select its map marker";
  if (target == body) return "You already control this soldier";
  ACR_Squad squad = m_Manager.FindSquadBySoldier(target);
  if (!squad) return "That soldier is not in a commandable squad";
  if (squad.GetFaction() != SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId())) return "That squad belongs to another faction";
  if (target.GetCharacterController().IsDead()) return "That soldier is dead";
  if (ACR_Squad.FindController(target)) return "Another player controls that soldier";
  SCR_PossessingManagerComponent possessing = SCR_PossessingManagerComponent.GetInstance();
  if (possessing && possessing.GetIdFromMainEntity(target) != 0) return "That soldier is a player's command unit";

  // The engine supports chained possession and preserves the command unit.
  ACR_MapHandoff.Release(pc, body);
  m_Manager.ReleaseAuthority(pc);
  pc.SetPossessedEntity(target);
  squad.Refresh();
  ACR_MapHandoff.SanitizePossessedWeapons(pc, target, true);
  ACR_MapHandoff.ClearCharacterInput(target);
  m_Manager.m_LastSquad.Set(pc.GetPlayerId(), squad);
  m_Manager.m_PossessedSquad.Set(pc.GetPlayerId(), squad);
  PrintFormat("[ACR] POSSESS player=%1 squad=%2 index=%3", pc.GetPlayerId(), squad.GetName(), squad.IndexOf(target));
  if (pc.GetControlledEntity() != target) return "Control transfer failed; try again";
  if (squad.IsLeader(pc))
   return string.Format("Now leading %1. Native squad commands are available. F7 swaps again; F8 returns.", squad.GetName());
  return string.Format("Now controlling %1-%2. F7 swaps again; F8 returns to your command unit.", squad.GetName(), squad.NumberOf(target));
 }

 // Arma Commander's Switch button: the leader if free, otherwise any free soldier.
 // Straight into the turret of the squad's armed vehicle. Plain possession: the
 // squad keeps its leader and order; drive from the turret (W/A/S/D) or give
 // the squad map orders as usual.
 string TakeGunner(SCR_PlayerController pc, ChimeraCharacter body, RplId groupId)
 {
  ACR_Squad squad = m_Manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
  if (!squad) return "Select a friendly squad first";
  ChimeraCharacter gunner = ACR_GunnerDrive.SquadGunner(squad);
  if (!gunner) return "That squad has no manned turret";
  if (gunner == body) return "You are already the gunner";
  return Possess(pc, body, SCR_EntityHelper.EntityToRplId(gunner));
 }

 string SwitchToGroup(SCR_PlayerController pc, ChimeraCharacter body, RplId groupId)
 {
  ACR_Squad squad = m_Manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
  if (!squad) return "Select a friendly squad first";
  array<IEntity> candidates = {};
  IEntity leader = squad.GetLeader();
  if (leader) candidates.Insert(leader);
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (agent.GetControlledEntity() && !candidates.Contains(agent.GetControlledEntity())) candidates.Insert(agent.GetControlledEntity());
  foreach (IEntity candidate : candidates)
  {
   ChimeraCharacter soldier = ChimeraCharacter.Cast(candidate);
   if (!soldier || soldier == body || soldier.GetCharacterController().IsDead() || ACR_Squad.FindController(soldier)) continue;
   return Possess(pc, body, SCR_EntityHelper.EntityToRplId(soldier));
  }
  return string.Format("%1 has no free soldier to take over", squad.GetName());
 }

 // The squad an order is for, resolved once (R04, 5 Oct review). A request that names a
 // group which no longer resolves (removed, not a squad any more) is refused: it never
 // falls through to another squad. Only a request without a group (the test hotkeys)
 // uses the player's current/last/first squad.
 ACR_Squad ResolveOrderSquad(SCR_PlayerController pc, RplId groupId, out string why)
 {
  if (groupId.IsValid())
  {
   ACR_Squad squad = m_Manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
   if (!squad || !m_Manager.m_Squads.Contains(squad))
   {
    why = "That squad is gone (select it again)";
    return null;
   }
   return squad;
  }
  ACR_Squad fallback;
  if (pc) fallback = m_Manager.GetSelectedSquad(pc);
  if (!fallback) why = "No squad selected";
  return fallback;
 }

 // A player's order, credited to him (shared command: who ordered what). Credit, the
 // command note and the re-tasked feed line follow only an ACCEPTED order that changed
 // the squad (new order serial, a queued waypoint or a ride); refusals and no-ops
 // (modes, LZs) never touch them (R03, 5 Oct review). Acceptance comes from
 // ACR_OrderOutcome, never from the message text.
 string Order(SCR_PlayerController pc, int command, RplId groupId, vector point)
 {
  string why;
  ACR_Squad squad = ResolveOrderSquad(pc, groupId, why);
  if (!squad)
  {
   PrintFormat("[ACR] ORDER refused player=%1 command=%2: %3", pc.GetPlayerId(), command, why);
   return why;
  }
  return OrderSquad(pc, command, squad, point).m_sMessage;
 }

 // One order to exactly this squad. A squad handed to the AI: the player's order takes it back
 // first (ACR_SquadAI), and a refused order (no RP, a bad site...) hands it back as it was, so a
 // refusal never leaves a squad nobody runs (#60 review). pc null (tests): refused.
 ACR_OrderOutcome OrderSquad(SCR_PlayerController pc, int command, ACR_Squad squad, vector point)
 {
  bool wasClaimed = squad.m_bClaimed;
  // Only someone who may order the squad takes it back: anyone else's order is refused anyway,
  // and taking it back and handing it over again would drop it out of the AI's attack plans.
  bool mayOrder = pc && squad.GetFaction() == SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId()) && m_Manager.IsCommander(pc.GetPlayerId());
  bool tookBack = mayOrder && ACR_SquadAI.TakeBack(squad, command);
  int serial = squad.m_iOrderSerial;
  int previous = squad.m_iOrderedBy;
  float previousAt = squad.m_fOrderedAt;
  ACR_OrderOutcome outcome = GiveOrderTo(pc, command, squad, point);
  ACR_SquadAI.AfterOrder(squad, command, outcome.m_bAccepted, tookBack, wasClaimed);
  if (tookBack && outcome.m_bAccepted) outcome.m_sMessage += " (taken back from the AI)";
  s_LastOutcome = outcome;
  if (pc) Credit(pc.GetPlayerId(), squad, command, point, outcome, serial, previous, previousAt);
  return outcome;
 }

 // Shared-command bookkeeping for one order (split out so srv:regress can check it
 // without a connected player). Returns true when the order was credited.
 bool Credit(int id, ACR_Squad squad, int command, vector point, ACR_OrderOutcome outcome, int serial, int previous, float previousAt)
 {
  if (!outcome || !outcome.m_bAccepted || !squad) return false;
  if (!m_Manager.m_Squads.Contains(squad) || !squad.GetFaction()) return false;
  bool queued = command == ACR_ERequest.QUEUE_MOVE || command == ACR_ERequest.QUEUE_DEFEND || command == ACR_ERequest.QUEUE_GARRISON;
  bool ride = command == ACR_ERequest.ORDER_RIDE;
  bool unload = command == ACR_ERequest.ORDER_UNLOAD;
  if (squad.m_iOrderSerial == serial && !queued && !ride && !unload) return false;
  float now = GetGame().GetWorld().GetWorldTime();
  squad.m_iOrderedBy = id;
  squad.m_fOrderedAt = now;
  string key = squad.GetFaction().GetFactionKey();
  string what = OrderWords(command);
  string place = "";
  ACR_Intel intel = m_Manager.IntelFor(key);
  if (intel && command != ACR_ERequest.ORDER_HALT && command != ACR_ERequest.ORDER_GET_OUT && command != ACR_ERequest.ORDER_UNLOAD) place = " " + intel.Place(point);
  m_Manager.Command().Note(id, string.Format("%1 %2%3", squad.GetName(), what, place));
  // Someone else's fresh order replaced: tell the side (the client asked first).
  if (previous != 0 && previous != id && now - previousAt < ACR_Command.RECENT_MS)
   m_Manager.Command().Feed(key, string.Format("%1 re-tasked %2 (was %3's order)", m_Manager.Command().TagOf(id), squad.GetName(), m_Manager.Command().TagOf(previous)));
  return true;
 }

 // The last order's outcome (tests read it).
 static ref ACR_OrderOutcome s_LastOutcome;

 // Drag to aim: where the player dragged the order being handled to face (ACR_CommandServer
 // sets it for the request), vector.Zero for none. Only player orders have one.
 static vector s_vAim;

 // The orders a drag can aim (ACR_PlayerController.ACR_Aimable): roadblocks, minefields, Fortify.
 static bool TakesFacing(int command)
 {
  return ACR_Roadblocks.TypeOfRequest(command) >= 0 || ACR_Minefields.IsRequest(command) || command == ACR_ERequest.ORDER_FORTIFY;
 }
 static const float AIM_MIN = 5;
 static const float AIM_MAX = 3000;

 // The dragged facing for an order at point, or vector.Zero (none, or nonsense from a client).
 static vector AimFor(vector point)
 {
  if (s_vAim == vector.Zero) return vector.Zero;
  float d = vector.DistanceXZ(s_vAim, point);
  if (d < AIM_MIN || d > AIM_MAX) return vector.Zero;
  return s_vAim;
 }

 // What an order at point faces: the dragged facing, else the threat (ACR_Intel.ThreatPoint).
 static vector FacingFor(string faction, vector point)
 {
  vector aim = AimFor(point);
  if (aim != vector.Zero) return aim;
  return ACR_Intel.ThreatPoint(faction, point);
 }

 static string OrderWords(int command)
 {
  switch (command)
  {
   case ACR_ERequest.ORDER_DEFEND: return "defending";
   case ACR_ERequest.QUEUE_DEFEND: return "then defending";
   case ACR_ERequest.ORDER_GARRISON: return "garrisoning";
   case ACR_ERequest.QUEUE_GARRISON: return "then garrisoning";
   case ACR_ERequest.QUEUE_MOVE: return "then moving";
   case ACR_ERequest.ORDER_GET_IN: return "getting in";
   case ACR_ERequest.ORDER_GET_OUT: return "getting out";
   case ACR_ERequest.ORDER_RIDE: return "riding";
   case ACR_ERequest.ORDER_UNLOAD: return "unloading";
   case ACR_ERequest.ORDER_HUNT: return "hunting";
   case ACR_ERequest.ORDER_SET_LZ: return "unloading";
   case ACR_ERequest.ORDER_HALT: return "halted";
   case ACR_ERequest.ORDER_ESTABLISH_FOB: return "establishing FOB";
   case ACR_ERequest.ORDER_ROADBLOCK: return "establishing roadblock";
   case ACR_ERequest.ORDER_MINES_AP: return "laying AP mines";
   case ACR_ERequest.ORDER_MINES_AT: return "laying AT mines";
   case ACR_ERequest.ORDER_MINES_MIXED: return "laying mines";
   case ACR_ERequest.ORDER_DISARM_MINES: return "disarming mines";
   case ACR_ERequest.ORDER_ROADBLOCK_S: return "establishing roadblock";
   case ACR_ERequest.ORDER_ROADBLOCK_L: return "establishing roadblock";
   case ACR_ERequest.ORDER_BARRICADE: return "building barricade";
   case ACR_ERequest.ORDER_FORTIFY: return "fortifying";
   case ACR_ERequest.ORDER_QRF: return "standing by as a QRF";
  }
  return "moving";
 }

 // Legacy wrapper: the message only.
 string GiveOrder(SCR_PlayerController pc, int command, RplId groupId, vector point)
 {
  string why;
  ACR_Squad squad = ResolveOrderSquad(pc, groupId, why);
  if (!squad) return why;
  return GiveOrderTo(pc, command, squad, point).m_sMessage;
 }

 // Validates and carries out one order on exactly this squad. The outcome says whether
 // it was accepted; the message is only for the player.
 ACR_OrderOutcome GiveOrderTo(SCR_PlayerController pc, int command, ACR_Squad squad, vector point)
 {
  ACR_OrderOutcome outcome = new ACR_OrderOutcome();
  if (!squad) return outcome.Refuse("No squad selected");
  if (!pc) return outcome.Refuse("No player");
  if (squad.GetFaction() != SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId())) return outcome.Refuse("That squad belongs to another faction");
  if (!m_Manager.IsCommander(pc.GetPlayerId())) return outcome.Refuse("Only the commander can give map orders. Take command first.");
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return outcome.Refuse("That squad is not ready");
  bool usesPoint = command == ACR_ERequest.MOVE_TO || command == ACR_ERequest.ORDER_DEFEND || command == ACR_ERequest.ORDER_GET_IN || command == ACR_ERequest.ORDER_SET_LZ || command == ACR_ERequest.ORDER_RIDE || command == ACR_ERequest.QUEUE_MOVE || command == ACR_ERequest.QUEUE_DEFEND || command == ACR_ERequest.ORDER_HUNT || command == ACR_ERequest.ORDER_GARRISON || command == ACR_ERequest.QUEUE_GARRISON || command == ACR_ERequest.ORDER_ESTABLISH_FOB || ACR_Roadblocks.TypeOfRequest(command) >= 0 || ACR_Minefields.IsRequest(command) || command == ACR_ERequest.ORDER_FORTIFY || command == ACR_ERequest.ORDER_QRF;
  // Reject nonsense coordinates from a client.
  if (usesPoint && (vector.Distance(point, anchor.GetOrigin()) > 20000 || !ACR_Manager.OnMap(point))) return outcome.Refuse("Order is too far away");
  // Map clicks made inside Game Master / build mode are not orders (the client checks too).
  if (usesPoint && ACR_Manager.EditorOpenFor(pc.GetPlayerId())) return outcome.Refuse("Close the editor to give map orders");
  if (usesPoint) point = ACR_Manager.Ground(point);
  string name = squad.GetName();
  // Helicopter crews only hold or get out (AI can't fly; any move makes them get out and walk).
  bool crewSafe = command == ACR_ERequest.ORDER_FIRE_AT_WILL || command == ACR_ERequest.ORDER_HALT || command == ACR_ERequest.ORDER_GET_OUT || command == ACR_ERequest.ORDER_UNLOAD || command == ACR_ERequest.MODE_COMBAT || command == ACR_ERequest.MODE_FAST || command == ACR_ERequest.MODE_SILENT;
  if (!crewSafe && squad.Aircraft()) return outcome.Refuse(name + ": AI can't fly helicopters. Take the pilot's seat to fly it, or order Get out");
  if (ACR_AirLift.Busy(squad)) return outcome.Refuse(name + " is on an air lift: orders again once it has landed");
  m_Manager.m_LastSquad.Set(pc.GetPlayerId(), squad);
  int serial = squad.m_iOrderSerial;
  // Lay minefield / Disarm mines: ACR_Minefields plans and validates the site, the squad pays.
  if (ACR_Minefields.IsRequest(command))
  {
   string mineWhy;
   ACR_EMineKind mineKind = ACR_Minefields.KindOf(command);
   if (!ACR_Minefields.Order(squad, mineKind, point, mineWhy, AimFor(point))) return outcome.Refuse(name + ": " + mineWhy);
   PrintFormat("[ACR] MINEFIELD order player=%1 squad=%2 %3 at %4", pc.GetPlayerId(), name, ACR_Minefields.Name(mineKind), point);
   return outcome.Accept(name + ": " + OrderWords(command));
  }
  int roadblock = ACR_Roadblocks.TypeOfRequest(command);
  if (roadblock >= 0) return OrderRoadblock(pc, squad, point, roadblock, outcome);
  // Fortify: ACR_Fortify plans the tier (a new position, or an upgrade of ours there).
  if (command == ACR_ERequest.ORDER_FORTIFY)
  {
   string fortifyWhy;
   // An upgrade keeps the position's facing (a dragged one only turns a new position).
   string fortifyFaction;
   if (squad.GetFaction()) fortifyFaction = squad.GetFaction().GetFactionKey();
   bool upgrade = ACR_Fortify.NearestOwn(fortifyFaction, point, ACR_Fortify.UPGRADE_RADIUS) != null;
   vector fortifyAim = AimFor(point);
   // On the middle of a base of ours: the whole town (ACR_BaseWorks).
   ACR_Base worksBase;
   if (!upgrade) worksBase = ACR_BaseWorks.BaseAt(point, fortifyFaction);
   if (worksBase)
   {
    if (!ACR_BaseWorks.Order(squad, worksBase, fortifyWhy)) return outcome.Refuse(name + ": " + fortifyWhy);
    PrintFormat("[ACR] BASEWORKS order player=%1 squad=%2 at %3", pc.GetPlayerId(), name, worksBase.m_sName);
    return outcome.Accept(name + ": " + ACR_BaseWorks.Accepted(worksBase));
   }
   if (!ACR_Fortify.Order(squad, point, fortifyWhy, fortifyAim)) return outcome.Refuse(name + ": " + fortifyWhy);
   PrintFormat("[ACR] FORTIFY order player=%1 squad=%2 at %3 aim %4", pc.GetPlayerId(), name, point, fortifyAim);
   if (upgrade && fortifyAim != vector.Zero) return outcome.Accept(name + ": " + OrderWords(command) + " (an upgrade keeps the position's facing)");
   return outcome.Accept(name + ": " + OrderWords(command));
  }
  switch (command)
  {
   case ACR_ERequest.ADVANCE:
    if (squad.MoveTo(ACR_Manager.Ground(squad.m_Home + Vector(35, 0, 0)))) return outcome.Accept(name + ": moving");
    break;
   case ACR_ERequest.RALLY:
    if (squad.MoveTo(ACR_Manager.Ground(squad.m_Home))) return outcome.Accept(name + ": rallying");
    break;
   case ACR_ERequest.MOVE_TO:
    // Vehicle crews drive long moves along the roads (to the clicked point at the end):
    // across country the game's driver got stuck and the crew walked (7 Oct live).
    bool moved;
    if (ACR_Transport.CrewedVehicle(squad) && squad.IsMounted()) moved = squad.RouteTo(point, ACR_Squad.WP_MOVE, -1, "Moving", ACR_EOrderKind.MOVE, true);
    else moved = squad.MoveTo(point);
    if (moved)
    {
     PrintFormat("[ACR] MOVE player=%1 squad=%2 target=%3", pc.GetPlayerId(), name, point);
     return outcome.Accept(name + ": moving");
    }
    break;
   case ACR_ERequest.ORDER_DEFEND:
   {
    // On a road: an ambush there (#12); anywhere else, or with no ambush to be had, a Defend.
    // Dragged: posts where the drag began facing where it ended, an ambush when it ended by a road.
    vector ambushSite, killZone;
    string ambushWhy;
    vector aim = AimFor(point);
    bool ambush;
    if (aim != vector.Zero && ACR_Ambush.DefendAimed(squad, point, aim, ambushSite, killZone, ambush, ambushWhy))
    {
     PrintFormat("[ACR] AMBUSH player=%1 squad=%2 site=%3 facing=%4 ambush=%5 (Defend dragged)", pc.GetPlayerId(), name, ambushSite, killZone, ambush);
     if (ambush) return outcome.Accept(name + ": lying in wait, facing the road");
     return outcome.Accept(name + ": defending, facing the way you dragged");
    }
    if (ACR_Ambush.DefendOnRoad(squad, point, ambushSite, killZone, ambushWhy))
    {
     PrintFormat("[ACR] AMBUSH player=%1 squad=%2 site=%3 kill zone=%4 (Defend on a road)", pc.GetPlayerId(), name, ambushSite, killZone);
     return outcome.Accept(name + ": lying in wait by the road");
    }
    if (squad.Defend(point)) return outcome.Accept(name + ": defending");
    break;
   }
   case ACR_ERequest.ORDER_GARRISON:
    if (squad.Garrison(point)) return outcome.Accept(name + ": garrisoning");
    // Garrison falls back to a Defend there by itself.
    if (ACR_Transport.CrewedVehicle(squad)) return outcome.Accept(name + ": vehicle crews can't garrison, defending there");
    return outcome.Accept(name + ": no building with room there, defending");
   case ACR_ERequest.ORDER_GET_IN:
   {
    bool boarding = squad.GetIn(point);
    PrintFormat("[ACR] GET IN player=%1 squad=%2 target=%3 -> %4", pc.GetPlayerId(), name, point, boarding);
    if (boarding) return outcome.Accept(name + ": getting in");
    return outcome.Refuse(name + ": no vehicle found");
   }
   case ACR_ERequest.ORDER_GET_OUT:
   {
    bool leaving = squad.GetOut();
    PrintFormat("[ACR] GET OUT player=%1 squad=%2 -> %3", pc.GetPlayerId(), name, leaving);
    if (leaving) return outcome.Accept(name + ": getting out");
    return outcome.Refuse(name + " is not in a vehicle");
   }
   case ACR_ERequest.ORDER_UNLOAD:
   {
    bool accepted;
    string unloaded = Unload(squad, accepted);
    outcome.m_bAccepted = accepted;
    outcome.m_sMessage = unloaded;
    return outcome;
   }
   case ACR_ERequest.ORDER_SET_LZ:
    return outcome.Accept(SetLZ(squad, point));
   case ACR_ERequest.ORDER_RIDE:
   {
    bool going;
    string ridden = Ride(squad, point, going);
    outcome.m_bAccepted = going;
    outcome.m_sMessage = ridden;
    return outcome;
   }
   case ACR_ERequest.ORDER_HUNT:
   {
    string hunted = m_Manager.Hunt(squad, point);
    outcome.m_bAccepted = squad.m_iOrderSerial != serial;
    outcome.m_sMessage = hunted;
    return outcome;
   }
   case ACR_ERequest.ORDER_HALT:
    squad.Halt("Halted");
    return outcome.Accept(name + ": halted");
   case ACR_ERequest.QUEUE_MOVE:
    if (squad.Queue(point, false)) return outcome.Accept(name + ": waypoint added");
    return outcome.Refuse(name + ": waypoint list is full");
   case ACR_ERequest.QUEUE_DEFEND:
    if (squad.Queue(point, true)) return outcome.Accept(name + ": will defend there after its current order");
    return outcome.Refuse(name + ": waypoint list is full");
   case ACR_ERequest.QUEUE_GARRISON:
    if (squad.Queue(point, false, true)) return outcome.Accept(name + ": will garrison there after its current order");
    return outcome.Refuse(name + ": waypoint list is full");
   case ACR_ERequest.MODE_COMBAT:
    squad.SetMode(ACR_EMoveMode.COMBAT);
    return outcome.Accept(name + ": combat");
   case ACR_ERequest.MODE_FAST:
    squad.SetMode(ACR_EMoveMode.FAST);
    return outcome.Accept(name + ": fast");
   case ACR_ERequest.ORDER_SQUAD_AI:
   {
    string handed = ACR_SquadAI.Toggle(squad);
    PrintFormat("[ACR] SQUAD AI player=%1 %2: %3", pc.GetPlayerId(), name, squad.m_bAICommanded);
    if (squad.m_bAICommanded || handed.Contains("back under your command")) return outcome.Accept(handed);
    return outcome.Refuse(handed);
   }
   case ACR_ERequest.ORDER_PATROL:
   {
    string patrolWhy;
    if (!squad.TogglePatrol(patrolWhy)) return outcome.Refuse(name + ": " + patrolWhy);
    PrintFormat("[ACR] PATROL player=%1 %2 patrol %3", pc.GetPlayerId(), name, squad.m_bPatrol);
    if (!squad.m_bPatrol) return outcome.Accept(name + ": patrol off (finishes the current leg)");
    return outcome.Accept(name + ": patrolling its points in a loop");
   }
   case ACR_ERequest.ORDER_FIRE_AT_WILL:
   {
    bool mortar = m_Manager.Mortars().TeamOf(squad) != null;
    if (!mortar && !ACR_FireMissions.IsObserver(squad)) return outcome.Refuse(name + ": needs a radio operator or a mortar to fire at will");
    squad.m_bFireAtWill = !squad.m_bFireAtWill;
    PrintFormat("[ACR] FIRE player=%1 %2 fire at will %3", pc.GetPlayerId(), name, squad.m_bFireAtWill);
    if (!squad.m_bFireAtWill) return outcome.Accept(name + ": fire at will off");
    if (mortar) return outcome.Accept(name + ": fire at will on (fires a few rounds of its own at spotted enemies in reach, no RP)");
    return outcome.Accept(name + ": fire at will on (calls mortar fire on enemies it sees; usual RP and cooldown)");
   }
   case ACR_ERequest.MODE_SILENT:
    squad.SetMode(ACR_EMoveMode.SILENT);
    return outcome.Accept(name + ": silent");
   case ACR_ERequest.ORDER_ESTABLISH_FOB:
   {
    string canWhy;
    if (!ACR_FobBuild.CanOrder(squad, canWhy)) return outcome.Refuse(name + ": " + canWhy);
    vector validSite;
    string siteWhy;
    string fobFaction = "";
    if (squad.GetFaction()) fobFaction = squad.GetFaction().GetFactionKey();
    if (!ACR_FobBuild.ValidateSite(point, validSite, siteWhy, fobFaction)) return outcome.Refuse(name + ": " + siteWhy);
    if (squad.EstablishFob(validSite))
    {
     PrintFormat("[ACR] ESTABLISH FOB player=%1 squad=%2 target=%3", pc.GetPlayerId(), name, validSite);
     return outcome.Accept(name + ": establishing FOB");
    }
    return outcome.Refuse(name + ": unable to establish FOB");
   }
   case ACR_ERequest.ORDER_QRF:
   {
    string qrfWhy;
    if (!ACR_Qrf.CanOrder(squad, qrfWhy)) return outcome.Refuse(name + ": " + qrfWhy);
    if (!squad.Qrf(point)) return outcome.Refuse(name + ": could not stand by there");
    PrintFormat("[ACR] QRF player=%1 squad=%2 post=%3", pc.GetPlayerId(), name, point);
    return outcome.Accept(string.Format("%1: %2, covering %3 m", name, OrderWords(command), ACR_Qrf.RADIUS));
   }
   default:
    return outcome.Refuse("Unknown command");
  }
  return outcome.Refuse(name + ": could not create that order");
 }

 // A roadblock order of any size (ACR_ERoadblockType).
 protected ACR_OrderOutcome OrderRoadblock(SCR_PlayerController pc, ACR_Squad squad, vector point, int type, ACR_OrderOutcome outcome)
 {
  string name = squad.GetName();
  string rbWhy;
  if (!ACR_Roadblocks.CanOrder(squad, rbWhy, type)) return outcome.Refuse(name + ": " + rbWhy);
  vector validRoadPos;
  float validYaw;
  string siteWhy;
  string squadFaction = "";
  if (squad.GetFaction()) squadFaction = squad.GetFaction().GetFactionKey();
  // Face the threat, as the AI commander's roadblocks face the enemy base (or the dragged facing).
  if (!ACR_Roadblocks.ValidateSite(point, validRoadPos, validYaw, siteWhy, squadFaction, FacingFor(squadFaction, point))) return outcome.Refuse(name + ": " + siteWhy);
  if (squad.EstablishRoadblock(validRoadPos, validYaw, type))
  {
   PrintFormat("[ACR] ESTABLISH ROADBLOCK player=%1 squad=%2 target=%3 type=%4", pc.GetPlayerId(), name, validRoadPos, ACR_Roadblocks.Name(squad.m_iRoadblockType));
   return outcome.Accept(string.Format("%1: building %2", name, ACR_Roadblocks.Name(squad.m_iRoadblockType)));
  }
  return outcome.Refuse(name + ": unable to establish roadblock");
 }

 // A free transport nearby (never fetched from far away) picks the squad up,
 // drops it at the point and it takes positions there (walking the rest if the ride fails).
 // The player's order may take the side's own APC or IFV too (ACR_Rides.WhyNotTaxi).
 // going: a ride was arranged, or the squad walks instead (it moves either way).
 string Ride(ACR_Squad squad, vector point, out bool going)
 {
  going = false;
  if (!m_Manager.m_Rides) m_Manager.m_Rides = new ACR_Rides(m_Manager);
  string why;
  if (!m_Manager.m_Rides.Request(squad, point, 0, ACR_Squad.WP_MOVE, -1, "Moving", 0, why, false, true))
  {
   // Walking beats the road there: just go.
   if (why.Contains("walking is faster") && squad.MoveTo(point)) { going = true; return squad.GetName() + ": " + why + ", on foot"; }
   return squad.GetName() + ": no ride, " + why;
  }
  going = true;
  ACR_Ride ride = m_Manager.m_Rides.RideOf(squad);
  string taxi = "a transport";
  if (ride) taxi = ride.m_Taxi.GetName();
  return string.Format("%1: %2 is on its way to pick them up", squad.GetName(), taxi);
 }

 // Queue an unload: the passengers get out by themselves when their vehicle
 // sets down near the point. Selecting the transport (its crew) applies it to
 // everyone riding in the back.
 string SetLZ(ACR_Squad squad, vector point)
 {
  array<ACR_Squad> targets = {};
  IEntity crewed = ACR_Transport.CrewedVehicle(squad);
  if (crewed) ACR_Transport.PassengerSquads(m_Manager, crewed, targets);
  if (targets.IsEmpty()) targets.Insert(squad);
  string names = "";
  foreach (ACR_Squad passenger : targets)
  {
   passenger.m_LZ = point;
   passenger.m_bHasLZ = true;
   passenger.m_fLZSetAt = GetGame().GetWorld().GetWorldTime();
   if (!names.IsEmpty()) names += ", ";
   names += passenger.GetName();
  }
  PrintFormat("[ACR] LZ set for %1 at %2", names, point);
  return string.Format("%1: will unload at the LZ (%2)", names, ACR_Manager.Grid(point));
 }

 // F6 while in a vehicle. Not settled: warn, and a second press within 3 s drops them anyway.
 protected ref map<int, float> m_UnloadConfirm = new map<int, float>();
 string UnloadHere(SCR_PlayerController pc, ChimeraCharacter body)
 {
  IEntity vehicle = ACR_Transport.VehicleOf(body);
  if (!vehicle) return "You are not in a vehicle";
  array<ACR_Squad> passengers = {};
  ACR_Transport.PassengerSquads(m_Manager, vehicle, passengers);
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  for (int i = passengers.Count() - 1; i >= 0; i--)
   if (passengers[i].GetFaction() != faction || passengers[i].HasPlayer()) passengers.Remove(i);
  if (passengers.IsEmpty()) return "No passenger squads aboard";
  float height, speed;
  bool settled = ACR_Transport.Settled(vehicle, height, speed);
  float now = GetGame().GetWorld().GetWorldTime();
  float asked;
  bool confirmed = m_UnloadConfirm.Find(pc.GetPlayerId(), asked) && now - asked < 3000;
  if (!settled && !confirmed)
  {
   m_UnloadConfirm.Set(pc.GetPlayerId(), now);
   return string.Format("%1 m up at %2 km/h. Press F6 again to drop them anyway.", (int)height, (int)(speed * 3.6));
  }
  m_UnloadConfirm.Remove(pc.GetPlayerId());
  int soldiers;
  foreach (ACR_Squad squad : passengers) soldiers += squad.DismountNow();
  PrintFormat("[ACR] UNLOAD HERE player=%1 squads=%2 soldiers=%3 height=%4 speed=%5", pc.GetPlayerId(), passengers.Count(), soldiers, height, speed);
  return string.Format("Unloading %1 squad(s)", passengers.Count());
 }

 // Queued unloads (server, several times a second).
 protected float m_fNextLZCheck;
 void CheckLandingZones()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextLZCheck) return;
  m_fNextLZCheck = now + 250;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.m_bHasLZ) continue;
   if (now - squad.m_fLZSetAt > 900000) { squad.m_bHasLZ = false; continue; }
   array<IEntity> vehicles = {};
   squad.GetVehicles(vehicles);
   if (vehicles.IsEmpty()) continue; // not aboard yet
   IEntity vehicle = vehicles[0];
   if (vector.DistanceXZ(vehicle.GetOrigin(), squad.m_LZ) > ACR_Transport.LZ_RADIUS) continue;
   float height, speed;
   if (!ACR_Transport.Settled(vehicle, height, speed)) continue;
   int soldiers = squad.DismountNow();
   PrintFormat("[ACR] LZ UNLOAD %1: %2 soldiers out (height %3, speed %4)", squad.GetName(), soldiers, height, speed);
   Faction faction = squad.GetFaction();
   if (faction) m_Manager.NotifyFaction(faction.GetFactionKey(), squad.GetName() + ": at the LZ, getting out");
  }
 }

 // Every other squad riding in this squad's vehicles dismounts.
 string Unload(ACR_Squad transport, out bool accepted)
 {
  accepted = false;
  array<IEntity> vehicles = {};
  transport.GetVehicles(vehicles);
  if (vehicles.IsEmpty()) return transport.GetName() + " is not in a vehicle";
  int unloaded;
  array<IEntity> passengerVehicles = {};
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad == transport) continue;
   squad.GetVehicles(passengerVehicles);
   foreach (IEntity vehicle : passengerVehicles)
   {
    if (!vehicles.Contains(vehicle)) continue;
    if (squad.GetOut()) unloaded++;
    break;
   }
  }
  if (!unloaded) return transport.GetName() + " has no passengers";
  accepted = true;
  return string.Format("%1: unloading %2 squad(s)", transport.GetName(), unloaded);
 }

}
