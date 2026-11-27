// A rare urge to play the church organ. Only the chosen soldier's utility actions
// change: his group's orders survive, and new orders or player possession cancel it.
class ACR_OrganState : Managed
{
 IEntity m_Organ;
 SCR_PlayInstrument m_Action;
 vector m_Keyboard;
 vector m_SoundAt;
 float m_fNextRoll;
 float m_fReady;
 IEntity m_Organist;
 SCR_AIUtilityComponent m_Utility;
 SCR_AICharacterSettingsComponent m_Settings;
 ACR_Squad m_Squad;
 int m_iOrderSerial;
 float m_fDeadline;
 bool m_bPlaying;
 ref SCR_AIBehaviorBase m_Behavior;
 ref SCR_AICharacterMovementSpeedSetting m_Speed;
 ref SCR_AICharacterStanceSetting m_Stance;
}

class ACR_OrganMove : SCR_AIMoveIndividuallyBehavior
{
 override float CustomEvaluate() { return 125; }
 override int GetCause() { return SCR_EAIBehaviorCause.GROUP_GOAL; }
}

class ACR_OrganWait : SCR_AIWaitBehavior
{
 override float CustomEvaluate() { return 125; }
 override int GetCause() { return SCR_EAIBehaviorCause.GROUP_GOAL; }
}

class ACR_OrganSound : Managed
{
 vector m_At;
 AudioHandle m_Handle = AudioHandle.Invalid;
}

class ACR_OrganEasterEgg
{
 static const float RADIUS = 50;
 static const float CHANCE = 0.001; // one roll per organ per minute, independent of crowd size
 static const float ROLL_MS = 60000;
 static const float COOLDOWN_MS = 1800000;
 static const float APPROACH_MS = 60000;
 static const string TOCCATA = "SOUND_PLAY_A_TACATTA"; // vanilla event spelling
 static const ResourceName SOUND = "{2202336E751CF577}Sounds/Structures/BuildingParts/Cultural/OrganChurch/Structures_Props_Organ.acp";
 protected static ref array<ref ACR_OrganState> s_Organs;
 protected static ref array<ref ACR_OrganSound> s_Sounds;
 protected static ref array<IEntity> s_Candidates;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_Organs = {};
  s_Sounds = {};
  s_Candidates = {};
 }

 static float Now() { return GetGame().GetWorld().GetWorldTime(); }

 static ACR_OrganState Find(IEntity organ)
 {
  foreach (ACR_OrganState state : s_Organs) if (state.m_Organ == organ) return state;
  return null;
 }

 static void Register(SCR_PlayInstrument action)
 {
  if (!Replication.IsServer() || !action || Find(action.GetOwner())) return;
  ActionsManagerComponent actions = action.GetActionsManager();
  if (!actions) return;
  UserActionContext context = actions.GetContext("Play");
  if (!context) return;
  vector contextTransform[4];
  if (!context.GetTransformationWorld(contextTransform)) return;
  ACR_OrganState state = new ACR_OrganState();
  state.m_Organ = action.GetOwner();
  state.m_Action = action;
  // The context is at hand height on the keys. Stand 1 m out from it so the
  // console/bench doesn't block his feet. Keep the church-floor height, including
  // upstairs organs; a terrain-height destination would send him downstairs.
  state.m_Keyboard = contextTransform[3] - Vector(0, 1.1065, 0) - contextTransform[2];
  state.m_SoundAt = state.m_Organ.CoordToParent(Vector(0, 3, 0.5));
  state.m_fNextRoll = Now() + Math.RandomFloat(ROLL_MS, ROLL_MS * 2);
  s_Organs.Insert(state);
 }

 static SCR_AIUtilityComponent Utility(IEntity soldier)
 {
  if (!soldier) return null;
  return ACR_EntityUtil.AIUtility(soldier);
 }

 static bool Eligible(IEntity soldier)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(soldier);
  if (!character || !BBB.IsAlive(character) || character.IsInVehicle() || BBB.PlayerOf(character)) return false;
  if (character.GetCharacterController().IsUnconscious()) return false;
  SCR_AIUtilityComponent utility = Utility(character);
  return utility != null && utility.m_AIInfo != null && !utility.m_AIInfo.HasUnitState(EUnitState.UNCONSCIOUS);
 }

 protected static bool Collect(IEntity entity)
 {
  if (Eligible(entity)) s_Candidates.Insert(entity);
  return true;
 }

 static bool PlayerAtKeyboard(vector keyboard)
 {
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   PlayerController controller = GetGame().GetPlayerManager().GetPlayerController(id);
   if (controller && controller.GetControlledEntity() && vector.Distance(controller.GetControlledEntity().GetOrigin(), keyboard) < 2) return true;
  }
  return false;
 }

 static bool Start(ACR_OrganState state, IEntity soldier)
 {
  if (!Replication.IsServer() || !state || state.m_Organist || !state.m_Organ || !state.m_Action) return false;
  if (Now() < state.m_fReady || !Eligible(soldier) || vector.Distance(soldier.GetOrigin(), state.m_Keyboard) > RADIUS) return false;
  if (!state.m_Action.CanBeShown(soldier) || PlayerAtKeyboard(state.m_Keyboard)) return false;
  foreach (ACR_OrganState other : s_Organs) if (other.m_Organist == soldier) return false;
  state.m_Utility = Utility(soldier);
  AIControlComponent control = AIControlComponent.Cast(soldier.FindComponent(AIControlComponent));
  state.m_Settings = SCR_AICharacterSettingsComponent.Cast(control.GetControlAIAgent().FindComponent(SCR_AICharacterSettingsComponent));
  state.m_Organist = soldier;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) state.m_Squad = manager.FindSquadByGroup(SCR_AIGroup.Cast(control.GetControlAIAgent().GetParentGroup()));
  if (state.m_Squad) state.m_iOrderSerial = state.m_Squad.m_iOrderSerial;
  state.m_fDeadline = Now() + APPROACH_MS;
  state.m_fReady = Now() + COOLDOWN_MS;
  state.m_Behavior = new ACR_OrganMove(state.m_Utility, null, state.m_Keyboard, 125, SCR_AIActionBase.PRIORITY_LEVEL_PLAYER, null, 0.5);
  state.m_Utility.AddAction(state.m_Behavior);
  if (state.m_Settings)
  {
   state.m_Speed = SCR_AICharacterMovementSpeedSetting.Create(SCR_EAISettingOrigin.BEHAVIOR, SCR_EAIBehaviorCause.GROUP_GOAL, EMovementType.SPRINT);
   state.m_Stance = SCR_AICharacterStanceSetting.Create(SCR_EAISettingOrigin.BEHAVIOR, SCR_EAIBehaviorCause.GROUP_GOAL, ECharacterStance.STAND);
   state.m_Settings.AddCharacterSetting(state.m_Speed, false);
   state.m_Settings.AddCharacterSetting(state.m_Stance, false);
  }
  PrintFormat("[ACR] ORGAN: organist heading to %1", state.m_Keyboard);
  return true;
 }

 static void Tick()
 {
  float now = Now();
  for (int i = s_Organs.Count() - 1; i >= 0; i--)
  {
   ACR_OrganState state = s_Organs[i];
   if (!state.m_Organ || !state.m_Action)
   {
    Finish(state);
    s_Organs.Remove(i);
    continue;
   }
   if (state.m_Organist || state.m_Behavior)
   {
    Update(state, now);
    continue;
   }
   if (now < state.m_fNextRoll) continue;
   state.m_fNextRoll = now + ROLL_MS;
   if (now < state.m_fReady || Math.RandomFloat01() >= CHANCE) continue;
   s_Candidates.Clear();
   GetGame().GetWorld().QueryEntitiesBySphere(state.m_Keyboard, RADIUS, Collect);
   // Random performer, any faction. At most one gets the urge at this organ.
   while (!s_Candidates.IsEmpty())
   {
    int candidate = Math.RandomInt(0, s_Candidates.Count());
    IEntity soldier = s_Candidates[candidate];
    s_Candidates.Remove(candidate);
    if (Start(state, soldier)) break;
   }
  }
 }

 protected static void Update(ACR_OrganState state, float now)
 {
  if (!Eligible(state.m_Organist) || !state.m_Utility || !state.m_Action.CanBeShown(state.m_Organist) || PlayerAtKeyboard(state.m_Keyboard)) { Finish(state); return; }
  if (state.m_Squad && state.m_Squad.m_iOrderSerial != state.m_iOrderSerial) { Finish(state); return; }
  if (now >= state.m_fDeadline) { Finish(state); return; }
  float distance = vector.Distance(state.m_Organist.GetOrigin(), state.m_Keyboard);
  if (state.m_bPlaying)
  {
   if (distance > 2) { Finish(state); return; }
   state.m_Utility.LookAt(state.m_Organ.CoordToParent(Vector(0, 1.1, 1.5)), 2);
   return;
  }
  if (distance > 1.25)
  {
   if (state.m_Behavior.GetActionState() == EAIActionState.FAILED || state.m_Behavior.GetActionState() == EAIActionState.COMPLETED) Finish(state);
   return;
  }
  state.m_Behavior.Complete();
  if (state.m_Settings && state.m_Speed) state.m_Settings.RemoveSetting(state.m_Speed);
  state.m_Speed = null;
  state.m_Behavior = new ACR_OrganWait(state.m_Utility, null);
  state.m_Behavior.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
  state.m_Utility.AddAction(state.m_Behavior);
  ChimeraCharacter.Cast(state.m_Organist).GetCharacterController().SetWeaponRaised(false);
  state.m_Utility.LookAt(state.m_Organ.CoordToParent(Vector(0, 1.1, 1.5)), state.m_Action.GetActionDuration());
  state.m_bPlaying = true;
  state.m_fDeadline = now + state.m_Action.GetActionDuration() * 1000;
  Broadcast(state, true);
  PrintFormat("[ACR] ORGAN: toccata started at %1 for %2 s", state.m_Keyboard, state.m_Action.GetActionDuration());
 }

 static void Finish(ACR_OrganState state)
 {
  if (!state) return;
  if (state.m_bPlaying) Broadcast(state, false);
  if (state.m_Behavior) state.m_Behavior.Complete();
  if (state.m_Settings)
  {
   if (state.m_Speed) state.m_Settings.RemoveSetting(state.m_Speed);
   if (state.m_Stance) state.m_Settings.RemoveSetting(state.m_Stance);
  }
  if (state.m_Organist || state.m_Behavior) Print("[ACR] ORGAN: organist released");
  state.m_Organist = null;
  state.m_Utility = null;
  state.m_Settings = null;
  state.m_Squad = null;
  state.m_Behavior = null;
  state.m_Speed = null;
  state.m_Stance = null;
  state.m_bPlaying = false;
 }

 protected static void Broadcast(ACR_OrganState state, bool playing)
 {
  vector at = state.m_SoundAt;
  // Local sound on every machine, like supports: replicated short-lived sound
  // entities can disappear before clients hear them. The game mode carries the RPC.
  PlayLocal(at, playing);
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (mode) mode.ACR_BroadcastOrgan(at, playing);
 }

 static void PlayLocal(vector at, bool playing)
 {
  for (int i = s_Sounds.Count() - 1; i >= 0; i--)
  {
   if (vector.Distance(s_Sounds[i].m_At, at) > 0.1) continue;
   AudioSystem.TerminateSound(s_Sounds[i].m_Handle);
   s_Sounds.Remove(i);
  }
  if (!playing) return;
  vector transform[4];
  Math3D.MatrixIdentity4(transform);
  transform[3] = at;
  ACR_OrganSound sound = new ACR_OrganSound();
  sound.m_At = at;
  sound.m_Handle = AudioSystem.PlayEvent(SOUND, TOCCATA, transform);
  s_Sounds.Insert(sound);
 }

 static void Reset()
 {
  foreach (ACR_OrganState state : s_Organs) Finish(state);
  s_Organs.Clear();
  foreach (ACR_OrganSound sound : s_Sounds) AudioSystem.TerminateSound(sound.m_Handle);
  s_Sounds.Clear();
  s_Candidates.Clear();
 }
}

// Discover only organs with the actual toccata action, including map and GM props.
modded class SCR_PlayInstrument
{
 override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
 {
  super.Init(pOwnerEntity, pManagerComponent);
  if (m_eInstrumentType == SCR_EInstrumentType.ORGAN && m_AudioSourceConfiguration && m_AudioSourceConfiguration.m_sSoundEventName == ACR_OrganEasterEgg.TOCCATA)
   ACR_OrganEasterEgg.Register(this);
 }
}

modded class SCR_BaseGameMode
{
 void ACR_BroadcastOrgan(vector at, bool playing)
 {
  Rpc(ACR_RpcOrgan, at, playing);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
 protected void ACR_RpcOrgan(vector at, bool playing)
 {
  if (!Replication.IsServer()) ACR_OrganEasterEgg.PlayLocal(at, playing);
 }
}
