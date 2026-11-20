// srv:nightflares (about 3 min): BBB's night flares under AC: Actual (20 Nov 2026, Jacob: "I'm not
// seeing the AI firing flares at night now"). One US rifle squad defends, one Soviet rifle squad
// assaults it from 140 m, at midnight, damage off so the fight lasts. The sides' AI commanders and 2ICs
// take them on and post them to defend in FAST, as in a match: the old COMBAT-only rule left those dark
// (nf-3: both FAST within 10 s, the one flare asked for cancelled). Nothing calls into BBB_Flares: ACA's hooks decide who may light (ACR_BBBHooks.AllowFlares) and BBB fires from the
// men's own kit. Passes on a real illumination flare in the air within NIGHT_LIMIT s. Every 10 s it logs
// each gate per squad (ACA's order state, mode, flank; BBB's contact, cooldowns, kit), so a failure
// says which gate held. BBB's own end-to-end check is its nightfight preset.
class ACR_TestNightFlares
{
 static const ResourceName NIGHT_US = "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
 static const ResourceName NIGHT_USSR = "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et";
 static const string DEFENDERS = "Night Defenders";
 static const string ATTACKERS = "Night Attackers";
 static const int NIGHT_LIMIT = 150;
 static const int EXPECTED = 7;
 protected static ref ACR_TestNightFlares s_Run;
 vector m_vHold;
 vector m_vFrom;
 int m_iStart;
 int m_iDispatched;
 bool m_bLit;
 bool m_bDone;
 bool m_bFastChecked;
 ref array<IEntity> m_aSeen;

 static void Run(int second)
 {
  if (!s_Run) { s_Run = new ACR_TestNightFlares(); s_Run.m_aSeen = {}; }
  s_Run.Step(second);
 }

 void Step(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || m_bDone) return;
  if (second == 8) Setup(manager);
  if (second == 14) Orders(manager);
  if (m_iStart == 0 || second < m_iStart) return;
  int fought = second - m_iStart;
  GetGame().GetWorld().QueryEntitiesBySphere((m_vHold + m_vFrom) * 0.5, 400, CollectFlare);
  if (fought % 10 == 0) { Report(manager, DEFENDERS, fought); Report(manager, ATTACKERS, fought); }
  FastCheck(manager);
  if ((!m_bLit || !m_bFastChecked) && fought < NIGHT_LIMIT) return;
  m_bDone = true;
  if (!m_bFastChecked) ACR_TestRegress.Check("nightflares: ACA lets a squad the AI posted in FAST light", false, "no squad went FAST");
  PrintFormat("[ACR] NIGHTFLARES after %1 s: requests=%2 cancelled=%3 lit=%4", fought, BBB_Flares.s_iDispatched - m_iDispatched, BBB_Flares.s_iCancelled, m_bLit);
  ACR_TestRegress.Check("nightflares: BBB asked for a flare over the fight", BBB_Flares.s_iDispatched > m_iDispatched, "");
  ACR_TestRegress.Check("nightflares: an illumination flare went up within the limit", m_bLit, string.Format("%1 s", fought));
  ACR_TestRegress.Complete("nightflares", EXPECTED);
 }

 // Once the AI has posted a squad in FAST (it does within seconds): ACA still lets it light.
 protected void FastCheck(ACR_Manager manager)
 {
  if (m_bFastChecked) return;
  array<string> names = {DEFENDERS, ATTACKERS};
  foreach (string name : names)
  {
   ACR_Squad squad = Find(manager, name);
   if (!squad || squad.m_Mode != ACR_EMoveMode.FAST) continue;
   m_bFastChecked = true;
   ACR_TestRegress.Check("nightflares: ACA lets a squad the AI posted in FAST light", BBB.s_Hooks.AllowFlares(squad.m_Group), Gates(squad));
   return;
  }
 }

 void Setup(ACR_Manager manager)
 {
  TimeAndWeatherManagerEntity weather = ChimeraWorld.CastFrom(GetGame().GetWorld()).GetTimeAndWeatherManager();
  if (weather) weather.SetHoursMinutesSeconds(0, 0, 0);
  m_vHold = ACR_Manager.Ground("1300 0 2150");
  m_vFrom = ACR_Manager.Ground("1300 0 2290");
  manager.m_sSpawningFaction = ACR_FactionSelection.West();
  manager.SpawnUnit(DEFENDERS, NIGHT_US, "", false, "", m_vHold, m_vHold);
  manager.m_sSpawningFaction = ACR_FactionSelection.East();
  manager.SpawnUnit(ATTACKERS, NIGHT_USSR, "", false, "", m_vFrom, m_vFrom);
  manager.m_sSpawningFaction = "";
 }

 void Orders(ACR_Manager manager)
 {
  ACR_Squad defenders = Find(manager, DEFENDERS);
  ACR_Squad attackers = Find(manager, ATTACKERS);
  ACR_TestRegress.Check("nightflares: both squads are ACA squads", defenders && attackers, "");
  ACR_TestRegress.Check("nightflares: it is night", SCR_AIWorldHandling.IsLowLightEnvironment(), "");
  ACR_TestRegress.Check("nightflares: BBB's flares are on", BBB_Flares.s_bOn && BBB.IsStarted(), "");
  if (!defenders || !attackers) { m_bDone = true; ACR_TestRegress.Complete("nightflares", EXPECTED); return; }
  Toughen(defenders);
  Toughen(attackers);
  defenders.SetMode(ACR_EMoveMode.COMBAT);
  defenders.Defend(m_vHold);
  attackers.SetMode(ACR_EMoveMode.COMBAT);
  attackers.Assault(m_vHold);
  ACR_TestRegress.Check("nightflares: ACA lets both squads light (defend, assault)", BBB.s_Hooks.AllowFlares(defenders.m_Group) && BBB.s_Hooks.AllowFlares(attackers.m_Group), string.Format("%1 / %2", Gates(defenders), Gates(attackers)));
  m_iDispatched = BBB_Flares.s_iDispatched;
  m_iStart = 15;
 }

 // Damage off so the fight lasts.
 protected void Toughen(ACR_Squad squad)
 {
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (body && body.GetDamageManager()) body.GetDamageManager().EnableDamageHandling(false);
  }
 }

 protected static ACR_Squad Find(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == name) return squad;
  return null;
 }

 protected static string Gates(ACR_Squad squad)
 {
  return string.Format("%1: state %2 mode %3 flank %4 commander %5 hook %6", squad.GetName(), typename.EnumToString(ACR_EOrderState, squad.OrderState()), typename.EnumToString(ACR_EMoveMode, squad.m_Mode), squad.m_CombatFlank.Active(), squad.m_Commander, BBB.s_Hooks.AllowFlares(squad.m_Group));
 }

 bool CollectFlare(IEntity entity)
 {
  if (!entity || !entity.GetPrefabData() || m_aSeen.Contains(entity)) return true;
  string name = entity.GetPrefabData().GetPrefabName();
  if (!name.Contains("FlareEffect")) return true;
  vector point = entity.GetOrigin();
  float height = point[1] - GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
  if (height < 10) return true;
  m_aSeen.Insert(entity);
  m_bLit = true;
  PrintFormat("[ACR] NIGHTFLARES illumination %1 at %2 height %3", name, point, height);
  return true;
 }

 // The gates BBB_Flares.TryIlluminate passes, in order, for this squad.
 protected void Report(ACR_Manager manager, string name, int fought)
 {
  ACR_Squad squad = Find(manager, name);
  if (!squad || !squad.m_Group) { PrintFormat("[ACR] NIGHTFLARES t=%1 %2 gone", fought, name); return; }
  SCR_AIGroup group = squad.m_Group;
  float now = GetGame().GetWorld().GetWorldTime();
  vector target;
  BaseTarget contact;
  bool known = BBB_Flares.KnownContact(group, target, contact);
  int men, seeing, allowed, kit;
  float nearest = 9999;
  array<AIAgent> agents = {};
  BBB.Members(group, agents);
  foreach (AIAgent agent : agents)
  {
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (!utility) continue;
   men++;
   if (utility.m_PerceptionComponent)
   {
    array<BaseTarget> targets = {};
    utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
    bool sees;
    foreach (BaseTarget t : targets)
    {
     if (!t.GetTargetEntity()) continue;
     nearest = Math.Min(nearest, vector.DistanceXZ(agent.GetControlledEntity().GetOrigin(), t.GetTargetEntity().GetOrigin()));
     if (BBB_Flares.RecentTarget(t)) sees = true;
    }
    if (sees) seeing++;
   }
   if (!BBB_Flares.Allowed(utility)) continue;
   allowed++;
   BaseWeaponComponent weapon;
   BaseMagazineComponent magazine;
   int muzzle;
   if (BBB_Flares.FindEquipment(utility, weapon, magazine, muzzle)) kit++;
  }
  PrintFormat("[ACR] NIGHTFLARES t=%1 %2 | men %3 seeing %4 nearest enemy %5 m known %6 area %7 groupReady %8", fought, Gates(squad), men, seeing, nearest, known, known && BBB_Flares.AreaReady(target, now), BBB_Flares.GroupReady(group, now));
  PrintFormat("[ACR] NIGHTFLARES t=%1 %2 | allowed %3 withKit %4 handles %5", fought, name, allowed, kit, BBB_Flares.Handles(group));
 }
}
