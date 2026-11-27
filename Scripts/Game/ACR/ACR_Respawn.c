// Command-unit respawn (server).
//  - Killed in your command unit: the deploy screen opens after the respawn delay and you
//    pick one of your side's open spawn points (ACR_SpawnPoints).
//  - Command unit killed while you control another soldier: you stay in that
//    soldier; after the delay a new command unit appears at your side's best open
//    spawn point (the HQ first) and F8 takes you there.
//  - A possessed soldier dies: you go back to your command unit (or wait for the
//    new one), never into the Game Master editor.
class ACR_PendingPossession : Managed
{
 int m_iPlayerId;
 ChimeraCharacter m_Victim;
 ChimeraCharacter m_NextTarget; // null if falling back to command unit
 ACR_Squad m_Squad;             // the victim's squad: the cascade stays in it
 float m_fTransferTime;
 string m_sSquadName;
 int m_iSquadNumber;
}

class ACR_Respawn : Managed
{
 protected ACR_Manager m_Manager;
 protected ref map<int, float> m_DueAt = new map<int, float>();         // player -> world time the new unit is ready
 protected ref map<int, bool> m_ReturnWhenReady = new map<int, bool>();
 protected ref map<int, ref ACR_PendingPossession> m_mPendingPossession = new map<int, ref ACR_PendingPossession>();

 void ACR_Respawn(ACR_Manager manager) { m_Manager = manager; }

 int DelayMs()
 {
  int seconds = m_Manager.m_iRespawnSeconds;
  if (seconds < 0) seconds = 0;
  return seconds * 1000;
 }

 // The command unit died (not while controlling a soldier): the deploy screen opens, with
 // the respawn timer (ACR_SpawnPoints.SetTimer) and the side's open spawn points.
 void OnCommandUnitKilled(int playerId)
 {
  ACR_CommandPresence.OnCommandUnitKilled(playerId);
  string extra = "";
  if (ACR_CommandPresence.On()) extra = " Off-map supports paused 30 s.";
  m_Manager.Notify(playerId, string.Format("Command unit killed. Choose where to redeploy in %1 s.%2", m_Manager.m_iRespawnSeconds, extra));
  PrintFormat("[ACR] COMMAND UNIT KILLED player=%1, respawn in %2 s", playerId, m_Manager.m_iRespawnSeconds);
 }

 // Where a new command unit goes when nobody picks (its player is controlling a soldier):
 // the side's best open spawn point (its HQ first).
 string BaseName(string factionKey)
 {
  SCR_SpawnPoint point = ACR_SpawnPoints.Best(factionKey);
  if (!point) return "your start point";
  ACR_Base base = ACR_SpawnPoints.BaseOf(point);
  if (base) return base.m_sName;
  return point.GetSpawnPointName();
 }

 protected bool RespawnPosition(string factionKey, out vector position)
 {
  SCR_SpawnPoint point = ACR_SpawnPoints.Best(factionKey);
  if (!point) return false;
  ACR_Base base = ACR_SpawnPoints.BaseOf(point);
  if (base) position = m_Manager.ClearSpotNear(base, 4, 12, 1);
  else position = ACR_Manager.Ground(point.GetOrigin());
  return true;
 }

 // A soldier the player controlled died: Tarkov-style fall and fade to black,
 // cascading into the next living squadmate (or command unit if squad is wiped).
 void OnPossessedKilled(SCR_PlayerController pc, ChimeraCharacter victim = null)
 {
  if (!pc) return;
  int id = pc.GetPlayerId();
  if (!victim) victim = ChimeraCharacter.Cast(pc.GetControlledEntity());

  ACR_Squad squad = null;
  if (victim) squad = m_Manager.FindSquadBySoldier(victim);
  if (!squad) m_Manager.m_PossessedSquad.Find(id, squad);
  if (squad && !m_Manager.m_Squads.Contains(squad)) squad = null;

  ChimeraCharacter nextSubordinate = null;
  if (squad)
  {
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
    if (!soldier || soldier == victim) continue;
    if (!IsAlive(soldier) || soldier.GetCharacterController().IsUnconscious()) continue;
    if (ACR_Squad.FindController(soldier) > 0) continue;
    nextSubordinate = soldier;
    break;
   }
  }

  float now = GetGame().GetWorld().GetWorldTime();
  ref ACR_PendingPossession pending = new ACR_PendingPossession();
  pending.m_iPlayerId = id;
  pending.m_Victim = victim;
  pending.m_NextTarget = nextSubordinate;
  pending.m_Squad = squad;
  pending.m_fTransferTime = now + 1800.0; // 1.8 s fall & collapse window

  string msg;
  if (nextSubordinate && squad)
  {
   pending.m_sSquadName = squad.GetName();
   pending.m_iSquadNumber = squad.NumberOf(nextSubordinate);
   msg = string.Format("Soldier killed. Transferring to %1-%2...", pending.m_sSquadName, pending.m_iSquadNumber);
   PrintFormat("[ACR] POSSESSED KILLED player=%1 -> cascading to %2-%3 in 1.8s", id, pending.m_sSquadName, pending.m_iSquadNumber);
  }
  else
  {
   msg = "Squad eliminated. Returning to command unit...";
   PrintFormat("[ACR] POSSESSED KILLED player=%1 -> squad eliminated -> command unit in 1.8s", id);
  }

  m_mPendingPossession.Set(id, pending);
  pc.ACR_SendDeathTransition(1.8, 1.2, msg);
 }

 void CancelPendingPossession(int playerId)
 {
  m_mPendingPossession.Remove(playerId);
 }

 // Ticked every server frame by ACR_Manager to process possession transfers.
 void Frame(float timeSlice)
 {
  if (m_mPendingPossession.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  array<int> readyIds = {};
  foreach (int id, ACR_PendingPossession pending : m_mPendingPossession)
   if (now >= pending.m_fTransferTime) readyIds.Insert(id);

  foreach (int id : readyIds)
  {
   ACR_PendingPossession pending = m_mPendingPossession.Get(id);
   m_mPendingPossession.Remove(id);
   if (!pending) continue;
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (!pc) continue;

   ChimeraCharacter target = pending.m_NextTarget;
   if (!target || !IsAlive(target) || target.GetCharacterController().IsUnconscious() || ACR_Squad.FindController(target) > 0)
   {
    target = null;
    ACR_Squad squad = pending.m_Squad;
    if (squad && m_Manager.m_Squads.Contains(squad))
    {
     array<AIAgent> agents = {};
     squad.GetAgents(agents);
     foreach (AIAgent agent : agents)
     {
      ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
      if (!soldier || soldier == pending.m_Victim) continue;
      if (!IsAlive(soldier) || soldier.GetCharacterController().IsUnconscious()) continue;
      if (ACR_Squad.FindController(soldier) > 0) continue;
      target = soldier;
      pending.m_sSquadName = squad.GetName();
      pending.m_iSquadNumber = squad.NumberOf(target);
      break;
     }
    }
   }

   bool taken = false;
   if (target)
   {
    string refused = m_Manager.m_Requests.Possess(pc, pending.m_Victim, SCR_EntityHelper.EntityToRplId(target));
    // Possess reports a refused transfer only in its text: check who he controls (it does too).
    taken = pc.GetControlledEntity() == target;
    if (taken)
    {
     m_Manager.Notify(id, string.Format("Now controlling %1-%2. F8 to return to command unit.", pending.m_sSquadName, pending.m_iSquadNumber));
     PrintFormat("[ACR] POSSESSED KILLED player=%1 -> transferred to subordinate %2-%3", id, pending.m_sSquadName, pending.m_iSquadNumber);
    }
    else PrintFormat("[ACR] POSSESSED KILLED player=%1 -> transfer to %2-%3 refused (%4)", id, pending.m_sSquadName, pending.m_iSquadNumber, refused);
   }
   if (!taken)
   {
    if (IsAlive(pc.GetMainEntity()))
    {
     m_Manager.ReleaseAuthority(pc);
     pc.SetPossessedEntity(null);
     ACR_MapHandoff.ClearCharacterInput(pc.GetControlledEntity());
     m_Manager.Notify(id, "Squad wiped out. Back in your command unit.");
     PrintFormat("[ACR] POSSESSED KILLED player=%1 -> command unit ready", id);
    }
    else
    {
     Schedule(id);
     m_ReturnWhenReady.Set(id, true);
     m_Manager.Notify(id, string.Format("Squad wiped out. Your new command unit arrives in %1 s.", SecondsLeft(id)));
     PrintFormat("[ACR] POSSESSED KILLED player=%1 -> squad wiped out -> waiting for new command unit", id);
    }
   }
  }
 }

 // F8 while the command unit is dead. Returns the message, or "" if not applicable.
 string OnReturnRequested(SCR_PlayerController pc)
 {
  if (!pc.IsPossessing() || IsAlive(pc.GetMainEntity())) return "";
  int id = pc.GetPlayerId();
  Schedule(id);
  m_ReturnWhenReady.Set(id, true);
  return string.Format("Your command unit is dead. You will switch to the new one in %1 s.", SecondsLeft(id));
 }

 static bool IsAlive(IEntity entity)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(entity);
  return character && character.GetCharacterController() && !character.GetCharacterController().IsDead();
 }

 protected void Schedule(int playerId)
 {
  if (m_DueAt.Contains(playerId)) return;
  m_DueAt.Set(playerId, GetGame().GetWorld().GetWorldTime() + DelayMs());
 }

 protected int SecondsLeft(int playerId)
 {
  float due;
  if (!m_DueAt.Find(playerId, due)) return 0;
  int left = Math.Ceil((due - GetGame().GetWorld().GetWorldTime()) / 1000);
  if (left < 0) left = 0;
  return left;
 }

 // Command units are officers (officer loadouts in the world's loadout manager)
 // ranked colonel. Checked every second, so it covers native spawns and ours.
 static void MakeColonel(SCR_PlayerController pc)
 {
  if (!pc) return;
  IEntity unit = pc.GetControlledEntity();
  if (pc.IsPossessing()) unit = pc.GetMainEntity();
  if (!IsAlive(unit)) return;
  SCR_CharacterRankComponent rank = SCR_CharacterRankComponent.GetCharacterRankComponent(unit);
  if (!rank || SCR_CharacterRankComponent.GetCharacterRank(unit) == SCR_ECharacterRank.COLONEL) return;
  rank.SetCharacterRank(SCR_ECharacterRank.COLONEL, true);
  PrintFormat("[ACR] Command unit of player %1 ranked colonel", pc.GetPlayerId());
 }

 // Freeze diagnostics (board hfvac46): log when a possessed soldier's AI is switched on or
 // off while a player controls him (the AI and the player would then fight over the body).
 protected ref map<int, int> m_PossessedAI = new map<int, int>();
 protected void WatchPossessedAI(SCR_PlayerController pc)
 {
  if (!pc) return;
  int id = pc.GetPlayerId();
  int state = -1;
  IEntity body = pc.GetControlledEntity();
  if (pc.IsPossessing() && body)
  {
   AIControlComponent ai = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
   if (ai) state = ai.IsAIActivated();
  }
  int before = -1;
  if (!m_PossessedAI.Find(id, before)) before = -1;
  if (state != before && (state == 1 || before == 1))
  {
   string name = "none";
   if (body && body.GetPrefabData()) name = FilePath.StripPath(body.GetPrefabData().GetPrefabName());
   PrintFormat("[ACR] POSSESSED AI player=%1 %2: AI %3 (possessing %4)", id, name, state == 1, pc.IsPossessing());
  }
  m_PossessedAI.Set(id, state);
 }

 // Every second: command units that died while their player was elsewhere.
 void Tick()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  ACR_SpawnPoints.Tick(m_Manager);
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   MakeColonel(pc);
   WatchPossessedAI(pc);
   if (pc) ACR_UALoadout.Check(pc.GetMainEntity());
   if (!pc || !pc.IsPossessing() || IsAlive(pc.GetMainEntity()))
   {
    m_DueAt.Remove(id);
    m_ReturnWhenReady.Remove(id);
    continue;
   }
   if (!m_DueAt.Contains(id))
   {
    Schedule(id);
    ACR_CommandPresence.OnCommandUnitKilled(id);
    Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
    string where = "your start point";
    if (faction) where = BaseName(faction.GetFactionKey());
    string extra = "";
    if (ACR_CommandPresence.On()) extra = " Off-map supports paused 30 s.";
    m_Manager.Notify(id, string.Format("Your command unit was killed. A new one arrives at %1 in %2 s (F8 to switch when ready).%3", where, SecondsLeft(id), extra));
    PrintFormat("[ACR] COMMAND UNIT LOST player=%1 (possessing)", id);
    continue;
   }
   if (now < m_DueAt.Get(id)) continue;
   IEntity unit = CreateCommandUnit(pc);
   if (!unit) continue; // retry next second
   m_DueAt.Remove(id);
   bool goNow = m_ReturnWhenReady.Contains(id) || !IsAlive(pc.GetControlledEntity());
   m_ReturnWhenReady.Remove(id);
   if (goNow)
   {
    m_Manager.ReleaseAuthority(pc);
    pc.SetPossessedEntity(null);
    ACR_MapHandoff.ClearCharacterInput(pc.GetControlledEntity());
    m_Manager.Notify(id, "You are in your new command unit.");
   }
   else m_Manager.Notify(id, "New command unit ready. F8 to switch to it.");
  }
 }

 // A fresh command unit (the player's loadout) at the respawn base, set as the
 // player's main entity while they keep controlling their current soldier.
 protected IEntity CreateCommandUnit(SCR_PlayerController pc)
 {
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction) return null;
  ResourceName prefab;
  SCR_PlayerLoadoutComponent loadoutComponent = SCR_PlayerLoadoutComponent.Cast(pc.FindComponent(SCR_PlayerLoadoutComponent));
  if (loadoutComponent && loadoutComponent.GetLoadout()) prefab = loadoutComponent.GetLoadout().GetLoadoutResource();
  if (prefab.IsEmpty())
  {
   SCR_BasePlayerLoadout loadout = GetGame().GetLoadoutManager().GetRandomFactionLoadout(faction);
   if (loadout) prefab = loadout.GetLoadoutResource();
  }
  if (prefab.IsEmpty()) return null;
  vector position;
  if (!RespawnPosition(faction.GetFactionKey(), position)) return null; // nowhere open: retry next second
  IEntity unit = ACR_Manager.SpawnPrefab(prefab, position);
  if (!unit) return null;
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(unit.FindComponent(FactionAffiliationComponent));
  if (affiliation) affiliation.SetAffiliatedFaction(faction);
  pc.ACR_SetMainEntity(unit);
  PrintFormat("[ACR] NEW COMMAND UNIT player=%1 at %2 %3", pc.GetPlayerId(), BaseName(faction.GetFactionKey()), position);
  return unit;
 }
}

// A possessed soldier dying returns the player to their command unit instead of
// opening the Game Master editor.
modded class SCR_PossessingManagerComponent
{
 override bool HandlePlayerKilled(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator instigator)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Respawn || playerId <= 0)
   return super.HandlePlayerKilled(playerId, playerEntity, killerEntity, instigator);
  if (playerEntity == GetMainEntity(playerId) || !IsPossessing(playerId))
  {
   manager.m_Respawn.OnCommandUnitKilled(playerId);
   return super.HandlePlayerKilled(playerId, playerEntity, killerEntity, instigator);
  }
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
  if (!pc) return super.HandlePlayerKilled(playerId, playerEntity, killerEntity, instigator);
  manager.m_Respawn.OnPossessedKilled(pc, ChimeraCharacter.Cast(playerEntity));
  return false;
 }
}

// Vanilla rank changes assume the Conflict game mode (renegade handling) and hit a
// null pointer anywhere else; skip that part when there is no Conflict mode.
modded class SCR_CharacterRankComponent
{
 override protected void SpecialRankHandling(SCR_ECharacterRank newRank, SCR_ECharacterRank prevRank)
 {
  if (!SCR_GameModeCampaign.GetInstance()) return;
  super.SpecialRankHandling(newRank, prevRank);
 }
}

// The game stops any possession when an editor opens, when the editor's mode changes
// and when it becomes "limited". A construction truck's build mode is a limited
// editor, and we switch the editor back to Game Master a few seconds after it closes:
// building from a possessed soldier dropped him, and closing it returned the player
// to the command unit (live tests 1 Oct, still so after OnLimitedChange alone was
// kept). Keep the possession unless full Game Master is being opened.
modded class SCR_PlayersManagerEditorComponent
{
 override protected void StopPosessing()
 {
  SCR_EditorManagerEntity manager = GetManager();
  // IsLimited() is false for a host who also has Game Master, so building mode
  // looked like full Game Master (live 1 Oct 17:53: possession dropped on opening
  // build mode). Only an open editor in Game Master (EDIT) mode stops possession.
  if (ACR_Manager.GetInstance() && manager && !ACR_Manager.EditorStopsPossession(manager)) return;
  super.StopPosessing();
 }
}
