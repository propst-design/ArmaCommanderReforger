// Command Presence: rewards taking the primary command unit (pc.GetMainEntity()) into battle.
// Server-authoritative, ticked once per second from ACR_Manager.Tick().
//  - Active while the commander is alive, conscious, not disarmed, and either directly in their
//    command unit (!IsPossessing) or within POSSESS_GRACE_MS (15 s) of leaving it via F7.
//  - Leadership aura (COMMAND_RADIUS = 200 m): +12% reaction scale (respecting mission-pinned
//    skills), 1.25x squad Veterancy XP, and silent local comms within SILENT_ORDER_RADIUS (150 m).
//  - Base capture: living active CO inside a base ring counts +2 extra (3 total).
//  - Eyes-on fire support (OBSERVE_RADIUS = 650 m): faster call-in (0.75x delay) and tighter
//    scatter (0.70x) for artillery battery, bomb run, and rocket strike.
//  - Stakes: losing your command unit pauses your off-map supports for DISRUPT_MS (30 s) while
//    command reorganizes (per-player in MP so co-commanders can cover).
// Host toggle: -acrCommandPresence 1 (default on).
class ACR_CommandPresence
{
 static const float COMMAND_RADIUS = 200;
 static const float SILENT_ORDER_RADIUS = 150;
 static const float OBSERVE_RADIUS = 650;
 static const float POSSESS_GRACE_MS = 15000;
 static const float DISRUPT_MS = 30000;
 static const float REACTION_SCALE = 1.12;
 static const float XP_SCALE = 1.25;
 static const float OBSERVE_DELAY_SCALE = 0.75;
 static const float OBSERVE_SCATTER_SCALE = 0.70;
 static const int CAPTURE_BONUS = 2;

 protected static ref map<int, float> s_LastInBody;
 protected static ref map<int, float> s_DisruptedUntil;
 protected static ref map<string, ref array<vector>> s_ActivePositions;
 protected static ref map<int, IEntity> s_ActiveBodies;

 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_LastInBody = new map<int, float>();
  s_DisruptedUntil = new map<int, float>();
  s_ActivePositions = new map<string, ref array<vector>>();
  s_ActiveBodies = new map<int, IEntity>();
 }

 static void Reset()
 {
  if (s_LastInBody) s_LastInBody.Clear();
  if (s_DisruptedUntil) s_DisruptedUntil.Clear();
  if (s_ActivePositions) s_ActivePositions.Clear();
  if (s_ActiveBodies) s_ActiveBodies.Clear();
 }

 static bool On()
 {
  return ACR_LogisticsRules.Choice("acrCommandPresence", "1") != "0";
 }

 // Refreshed every 1 s on the server from ACR_Manager.Tick().
 static void Tick(ACR_Manager manager)
 {
  if (!s_ActivePositions || !s_ActiveBodies) return;
  s_ActivePositions.Clear();
  s_ActiveBodies.Clear();
  if (!On() || !manager) return;
  PlayerManager pm = GetGame().GetPlayerManager();
  if (!pm) return;
  float now = GetGame().GetWorld().GetWorldTime();
  array<int> players = {};
  pm.GetPlayers(players);
  foreach (int id : players)
  {
   if (!manager.IsCommander(id)) continue;
   SCR_PlayerController pc = SCR_PlayerController.Cast(pm.GetPlayerController(id));
   if (!pc) continue;
   IEntity main = pc.GetMainEntity();
   if (!main || !ACR_Respawn.IsAlive(main)) continue;
   ChimeraCharacter ch = ChimeraCharacter.Cast(main);
   if (!ch || !ch.GetCharacterController() || ch.GetCharacterController().IsUnconscious()) continue;
   PerceivableComponent perc = PerceivableComponent.Cast(ch.FindComponent(PerceivableComponent));
   if (perc && perc.IsDisarmed()) continue;
   if (!pc.IsPossessing())
   {
    s_LastInBody.Set(id, now);
   }
   else
   {
    float lastIn;
    if (!s_LastInBody.Find(id, lastIn) || now - lastIn > POSSESS_GRACE_MS) continue;
   }
   FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(main.FindComponent(FactionAffiliationComponent));
   if (!aff || !aff.GetAffiliatedFaction()) continue;
   string key = aff.GetAffiliatedFaction().GetFactionKey();
   if (key.IsEmpty()) continue;
   array<vector> list;
   if (!s_ActivePositions.Find(key, list))
   {
    list = {};
    s_ActivePositions.Set(key, list);
   }
   list.Insert(main.GetOrigin());
   s_ActiveBodies.Set(id, main);
  }
 }

 static bool IsActiveBody(IEntity body)
 {
  if (!On() || !body || !s_ActiveBodies || s_ActiveBodies.IsEmpty()) return false;
  foreach (int id, IEntity active : s_ActiveBodies)
   if (active == body) return true;
  return false;
 }

 static bool Near(string factionKey, vector pos, float radius)
 {
  if (!On() || factionKey.IsEmpty() || !s_ActivePositions) return false;
  array<vector> list;
  if (!s_ActivePositions.Find(factionKey, list) || !list) return false;
  foreach (vector coPos : list)
   if (vector.DistanceXZ(coPos, pos) <= radius) return true;
  return false;
 }

 static bool InCommandRange(ACR_Squad squad)
 {
  if (!On() || !squad || !squad.GetFaction()) return false;
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return false;
  return Near(squad.GetFaction().GetFactionKey(), anchor.GetOrigin(), COMMAND_RADIUS);
 }

 static float ReactionScale(ACR_Squad squad)
 {
  if (InCommandRange(squad)) return REACTION_SCALE;
  return 1.0;
 }

 static float XPScale(ACR_Squad squad)
 {
  if (InCommandRange(squad)) return XP_SCALE;
  return 1.0;
 }

 // True when the calling commander's active command unit is within OBSERVE_RADIUS of target.
 static bool ObservedBy(int callerPlayerId, string factionKey, vector target)
 {
  if (!On()) return false;
  if (callerPlayerId > 0 && s_ActiveBodies)
  {
   IEntity body;
   if (s_ActiveBodies.Find(callerPlayerId, body) && body)
    return vector.DistanceXZ(body.GetOrigin(), target) <= OBSERVE_RADIUS;
   return false;
  }
  return Near(factionKey, target, OBSERVE_RADIUS);
 }

 static void OnCommandUnitKilled(int playerId)
 {
  if (!On() || playerId <= 0) return;
  if (s_ActiveBodies) s_ActiveBodies.Remove(playerId);
  if (s_LastInBody) s_LastInBody.Remove(playerId);
  if (s_DisruptedUntil) s_DisruptedUntil.Set(playerId, GetGame().GetWorld().GetWorldTime() + DISRUPT_MS);
 }

 static int DisruptedSeconds(int playerId)
 {
  if (!On() || playerId <= 0 || !s_DisruptedUntil) return 0;
  float until;
  if (!s_DisruptedUntil.Find(playerId, until)) return 0;
  float left = until - GetGame().GetWorld().GetWorldTime();
  if (left <= 0)
  {
   s_DisruptedUntil.Remove(playerId);
   return 0;
  }
  return Math.Ceil(left / 1000.0);
 }
}
