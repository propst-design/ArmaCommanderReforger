// Fire missions, forward observers and mortar free fire (5 Oct; roadmap item 4).
//  - Observers: any squad with a radio operator (an "_RTO" soldier) or a bought "Forward observer
//    team" (spotter + radio operator). With "Fire at will" on (FO teams start with it on; the AI
//    commander's observers always), an observer that has seen an enemy group in the last
//    OBSERVED_S seconds, at least MIN_DISTANCE away, with no friendly within DANGER_CLOSE of it,
//    calls the side's small HE mortar mission on it: the normal support (RP, cooldown, a deployed
//    mortar team in range), at most once per side every SIDE_GAP_S.
//  - Mortar teams with "Fire at will" (AI mortar teams always) fire on their own: every
//    FREE_GAP_S, FREE_ROUNDS rounds at the freshest contact in reach seen by anyone of the side,
//    scattered FREE_SCATTER m, no RP, never danger close.
// Server only; ticks from ACR_Manager every second (works every CHECK_S).
class ACR_FireMissions : Managed
{
 static float OBSERVED_S = 15;
 static float MIN_DISTANCE = 150;
 static float DANGER_CLOSE = 120;
 static float SIDE_GAP_S = 45;
 static float FREE_GAP_S = 30;
 static int FREE_ROUNDS = 2;
 static float FREE_SCATTER = 30;
 static float FREE_SEEN_S = 20;
 static const int CHECK_S = 5;
 static const int COST = 12;
 static const int STOCK = 2;
 static const string ENTRY_NAME = "Forward observer team";

 static const ResourceName FO_WEST = "{1CA3D30464EE4674}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Spotter.et";
 static const ResourceName RTO_WEST = "{3726077BE60962FF}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_RTO.et";
 static const ResourceName FO_EAST = "{0DC67B4DFFD7A361}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Scout_RTO.et";
 static const ResourceName RTO_EAST = "{612F43A4D5AE765F}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_RTO.et";
 static const ResourceName RTO_FIA = "{23D81C023DBF85AC}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_RTO.et";

 protected ACR_Manager m_Manager;
 protected int m_iTicks;
 protected ref map<string, float> m_NextCall = new map<string, float>();
 protected ref map<ACR_MortarTeam, float> m_NextFree = new map<ACR_MortarTeam, float>();
 static ref array<SCR_AIGroup> s_ObserverGroups = {}; // bought FO teams (their squads start with fire at will)
 static int s_iCalls;      // tests
 static int s_iFreeFires;  // tests

 void ACR_FireMissions(ACR_Manager manager) { m_Manager = manager; }

 static ACR_RequisitionEntry MakeEntry(ACR_FactionSettings settings = null)
 {
  ACR_RequisitionEntry entry = new ACR_RequisitionEntry();
  entry.Set(ENTRY_NAME, "", "", COST, STOCK);
  entry.m_bObserverTeam = true;
  entry.m_bArmed = false;
  if (settings)
  {
   ACR_SupportUnitSettings unit = settings.SupportUnit(ACR_ESupportUnitRole.OBSERVER_TEAM);
   if (unit)
   {
    if (!unit.m_sName.IsEmpty()) entry.m_sName = unit.m_sName;
    if (unit.m_iCost >= 0) entry.m_iCost = unit.m_iCost;
    if (unit.m_iStock >= 0) entry.m_iStock = unit.m_iStock;
   }
  }
  return entry;
 }

 // A two-man team: spotter + radio operator, in the faction's base group (like the mortar team).
 bool SpawnTeam(string factionKey, string name, vector pos)
 {
  ResourceName group = ACR_Mortars.GROUP_EAST;
  array<ResourceName> men = {FO_EAST, RTO_EAST};
  if (ACR_Supports.IsWestern(factionKey)) { group = ACR_Mortars.GROUP_WEST; men = {FO_WEST, RTO_WEST}; }
  else if (factionKey.Contains("FIA")) { group = ACR_Mortars.GROUP_FIA; men = {ACR_Mortars.RIFLE_FIA, RTO_FIA}; }
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  ACR_SupportUnitSettings unit = null;
  if (settings) unit = settings.SupportUnit(ACR_ESupportUnitRole.OBSERVER_TEAM);
  if (unit && !unit.m_sGroupPrefab.IsEmpty()) group = unit.m_sGroupPrefab;
  else group = ACR_FactionCatalog.ResolvePrefab(factionKey, group, EEntityCatalogType.GROUP);
  if (unit && !unit.m_sPrimaryPrefab.IsEmpty()) men[0] = unit.m_sPrimaryPrefab;
  else men[0] = ACR_FactionCatalog.ResolvePrefab(factionKey, men[0], EEntityCatalogType.CHARACTER);
  if (unit && !unit.m_sSecondaryPrefab.IsEmpty()) men[1] = unit.m_sSecondaryPrefab;
  else men[1] = ACR_FactionCatalog.ResolvePrefab(factionKey, men[1], EEntityCatalogType.CHARACTER);
  SCR_AIGroup.IgnoreSpawning(true);
  SCR_AIGroup spawned = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(group, pos));
  SCR_AIGroup.IgnoreSpawning(false);
  if (!spawned) return false;
  if (!spawned.m_aUnitPrefabSlots) spawned.m_aUnitPrefabSlots = {};
  spawned.m_aUnitPrefabSlots.Copy(men);
  spawned.SetCustomName(name, 0);
  // Both men or nothing (R01, 5 Oct review).
  if (!ACR_Requisition.FillGroup(spawned, men, pos))
  {
   PrintFormat("[ACR] FIRE observer team %1 (%2) failed to spawn its men: removed", name, factionKey);
   return false;
  }
  s_ObserverGroups.Insert(spawned);
  PrintFormat("[ACR] FIRE observer team %1 (%2) spawned at %3, %4 men", name, factionKey, pos, spawned.GetAgentsCount());
  return true;
 }

 // A squad that can call fire: a bought FO team or anyone carrying the radio.
 static bool IsObserver(ACR_Squad squad)
 {
  if (!squad || !squad.m_Group) return false;
  if (s_ObserverGroups.Contains(squad.m_Group)) return true;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || !body.GetPrefabData()) continue;
   string prefab = body.GetPrefabData().GetPrefabName();
   if (prefab.Contains("_RTO") || prefab.Contains("Spotter")) return true;
  }
  return false;
 }

 // An AI-led side fires as often as its commander's doctrine wants (Firepower more, Blitz less),
 // and more often while rich (ACR_AICommander.FireFactor, #28).
 protected float Gap(string key, bool ai)
 {
  if (!ai) return 1;
  ACR_AICommander commander = m_Manager.AICommanderFor(key);
  if (!commander || !commander.Doctrine()) return 1;
  return commander.Doctrine().m_fFireMissionGap / commander.FireFactor();
 }

 protected bool AILed(string key)
 {
  // (m_bActive keeps its last value when the faction settings switch the AI off.)
  ACR_FactionSettings settings = ACR_FactionSettings.Find(key);
  if (!settings || !settings.m_bAICommander || (m_Manager.m_Debug && m_Manager.m_Debug.IsPaused(key))) return false;
  ACR_AICommander ai = m_Manager.AICommanderFor(key);
  return ai && ai.m_bActive;
 }

 protected bool FriendlyNear(string key, vector point, float radius)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != key) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (body && vector.DistanceXZ(body.GetOrigin(), point) < radius) return true;
   }
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (player && ACR_Intel.FactionKeyOf(player) == key && vector.DistanceXZ(player.GetOrigin(), point) < radius) return true;
  }
  return false;
 }

 void Tick()
 {
  m_iTicks++;
  if (m_iTicks % CHECK_S != 0) return;
  float now = GetGame().GetWorld().GetWorldTime() / 1000;
  array<string> sides = {};
  foreach (ACR_Squad squad : m_Manager.m_Squads)
   if (squad.GetFaction() && !sides.Contains(squad.GetFaction().GetFactionKey())) sides.Insert(squad.GetFaction().GetFactionKey());
  foreach (string key : sides)
  {
   ACR_Intel intel = m_Manager.IntelFor(key);
   if (!intel) continue;
   ObserverCalls(key, intel, now);
   FreeFire(key, intel, now);
  }
 }

 protected void ObserverCalls(string key, ACR_Intel intel, float now)
 {
  float next;
  if (m_NextCall.Find(key, next) && now < next) return;
  bool ai = AILed(key);
  if (!m_Manager.m_Supports) m_Manager.m_Supports = new ACR_Supports(m_Manager);
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(key);
  int index = -1;
  foreach (int i, ACR_SupportEntry entry : data.m_aSupports) if (entry.m_eType == ACR_ESupportType.MORTAR_HE_SMALL) index = i;
  if (index < 0) return;
  ACR_SupportEntry support = data.m_aSupports[index];
  if (GetGame().GetWorld().GetWorldTime() < support.m_fReadyAt || data.m_iRP < support.m_iCost) return;
  foreach (ACR_Squad observer : m_Manager.m_Squads)
  {
   if (!observer.GetFaction() || observer.GetFaction().GetFactionKey() != key || observer.HasPlayer()) continue;
   if (!observer.m_bFireAtWill && !ai) continue;
   if (!IsObserver(observer) || !observer.GetAnchor()) continue;
   vector eye = observer.GetAnchor().GetOrigin();
   ACR_Contact best;
   foreach (ACR_Contact contact : intel.m_Contacts)
   {
    if (contact.m_sSpotter != observer.GetName() || now - contact.m_fSeen / 1000 > OBSERVED_S) continue;
    if (contact.m_iKind == ACR_Intel.KIND_AIR || vector.DistanceXZ(contact.m_Pos, eye) < MIN_DISTANCE) continue;
    if (FriendlyNear(key, contact.m_Pos, DANGER_CLOSE)) continue;
    if (!m_Manager.Mortars().CanFire(key, ACR_ESupportType.MORTAR_HE_SMALL, contact.m_Pos)) continue;
    if (!best || contact.m_iCount > best.m_iCount) best = contact;
   }
   if (!best) continue;
   string result = m_Manager.m_Supports.Call(key, index, best.m_Pos, true);
   m_NextCall.Set(key, now + SIDE_GAP_S * Gap(key, ai));
   s_iCalls++;
   string what = ACR_Intel.Describe(best.m_iKind, best.m_iCount);
   PrintFormat("[ACR] FIRE %1 observer %2 calls mortars on %3 at %4: %5", key, observer.GetName(), what, best.m_Pos, result);
   if (!ai) m_Manager.NotifyFaction(key, string.Format("%1: fire mission on %2 %3", observer.GetName(), what, intel.Place(best.m_Pos)));
   intel.AddFeed(GetGame().GetWorld().GetWorldTime(), string.Format("%1 calls fire on %2 %3", observer.GetName(), what, intel.Place(best.m_Pos)));
   return;
  }
 }

 protected void FreeFire(string key, ACR_Intel intel, float now)
 {
  bool ai = AILed(key);
  array<ACR_MortarTeam> teams = {};
  m_Manager.Mortars().TeamsOf(key, teams);
  foreach (ACR_MortarTeam team : teams)
  {
   if (!team.m_Squad || (!team.m_Squad.m_bFireAtWill && !ai)) continue;
   if (team.m_eState != ACR_EMortarState.DEPLOYED || team.m_bFiring || !team.m_Tube) continue;
   float next;
   if (m_NextFree.Find(team, next) && now < next) continue;
   ACR_Contact best;
   float flight;
   foreach (ACR_Contact contact : intel.m_Contacts)
   {
    if (now - contact.m_fSeen / 1000 > FREE_SEEN_S || contact.m_iKind == ACR_Intel.KIND_AIR) continue;
    if (best && contact.m_fSeen <= best.m_fSeen) continue;
    string reason;
    if (!m_Manager.Mortars().Ready(team, ACR_ESupportType.MORTAR_HE_SMALL, contact.m_Pos, reason, flight)) continue;
    if (FriendlyNear(key, contact.m_Pos, DANGER_CLOSE)) continue;
    best = contact;
   }
   if (!best) continue;
   vector aim = ACR_Manager.Ground(best.m_Pos + Vector(Math.RandomFloat(-FREE_SCATTER, FREE_SCATTER), 0, Math.RandomFloat(-FREE_SCATTER, FREE_SCATTER)));
   m_Manager.Mortars().StartFreeFire(team, aim, FREE_ROUNDS);
   m_NextFree.Set(team, now + FREE_GAP_S * Gap(key, ai));
   s_iFreeFires++;
   PrintFormat("[ACR] FIRE %1 %2 fires at will: %3 rounds on %4 at %5", key, team.m_sName, FREE_ROUNDS, ACR_Intel.Describe(best.m_iKind, best.m_iCount), aim);
  }
 }
}
