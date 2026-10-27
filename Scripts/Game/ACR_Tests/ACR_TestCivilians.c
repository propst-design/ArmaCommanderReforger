// -acrCivilianTest 1 (server-capable, ~40 s): the civilian socket (ACR_CivilianSocket).
// A vanilla civilian stands 25 m in front of Alpha and a Soviet soldier 70 m out as the control:
// Alpha's perception must report the soldier and not the civilian. Then two civilian deaths by
// Alpha's fire go out as one line in the US feed only. With a compat add-on plugged in (Civil
// Population), its AI slots must reach the raised active AI limit and a real kill by an Alpha
// soldier must reach the US feed through the mod; unplugged that is a skip.
class ACR_TestCivilians
{
 static const ResourceName CIVILIAN = "{22E43956740A6794}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_Randomized.et";
 static IEntity s_Civilian;
 static IEntity s_Enemy;
 static bool s_bEnemySeen;
 static bool s_bCivilianSeen;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  if (!alpha || !alpha.GetAnchor()) return;
  IEntity anchor = alpha.GetAnchor();
  if (tick == 4)
  {
   vector m[4];
   anchor.GetWorldTransform(m);
   s_Civilian = ACR_Manager.SpawnPrefab(CIVILIAN, ACR_Manager.Ground(m[3] + m[2] * 25));
   array<ResourceName> pool = manager.m_Bases.GetFaction("USSR").m_aGarrison;
   if (!pool.IsEmpty()) s_Enemy = ACR_Manager.SpawnPrefab(pool[0], ACR_Manager.Ground(m[3] + m[2] * 70 + m[0] * 10));
   PrintFormat("[ACR] CIVILIANS civilian %1 (%2), control %3, socket '%4'", s_Civilian != null, ACR_Intel.FactionKeyOf(s_Civilian), s_Enemy != null, ACR_CivilianSocket.Get().Name());
  }
  ACR_Intel us = manager.IntelFor("US");
  if (tick > 4 && tick <= 16 && us)
  {
   // every tick: the control may be shot and forgotten before the checks
   if (s_Enemy && us.m_SeenTime.Contains(s_Enemy)) s_bEnemySeen = true;
   if (s_Civilian && us.m_SeenTime.Contains(s_Civilian)) s_bCivilianSeen = true;
  }
  if (tick == 16)
  {
   ACR_TestRegress.Check("civilians: vanilla CIV is friendly to US", ACR_Intel.FriendlyTo("US", "CIV"));
   ACR_TestRegress.Check("civilians: USSR is not friendly to US", !ACR_Intel.FriendlyTo("US", "USSR"));
   ACR_TestRegress.Check("civilians: Alpha's perception reports the Soviet control", s_bEnemySeen);
   ACR_TestRegress.Check("civilians: a civilian in plain view is no contact", s_Civilian && !s_bCivilianSeen);

   ACR_CivilianSocket socket = ACR_CivilianSocket.Get();
   IEntity shooter = anchor;
   socket.CivilianKilled(s_Civilian, shooter, 0);
   socket.CivilianKilled(s_Civilian, shooter, 0);
   ACR_TestRegress.Check("civilians: two deaths wait as one batch", socket.PendingDeaths() == 2, socket.PendingDeaths().ToString());
   socket.FlushNow();
  }
  if (tick == 17)
  {
   string usLine = FeedLine(manager.IntelFor("US"), "civilians killed by our fire");
   string ussrLine = FeedLine(manager.IntelFor("USSR"), "civilian");
   PrintFormat("[ACR] CIVILIANS US feed: '%1'", usLine);
   ACR_TestRegress.Check("civilians: the killer's side reads one line for both deaths", usLine.Contains("2 civilians killed by our fire"), usLine);
   ACR_TestRegress.Check("civilians: the other side isn't told", ussrLine.IsEmpty(), ussrLine);

   ACR_CivilianSocket plug = ACR_CivilianSocket.Get();
   if (!plug.Plugged())
   {
    ACR_TestRegress.Skip("civilians: compat add-on plugged in", "no civilian mod loaded");
    Finish(7);
    return;
   }
   AIWorld aiWorld = GetGame().GetAIWorld();
   int wanted = 2 * manager.m_iSoldierCap + ACR_Manager.AI_LIMIT_HEADROOM + plug.AiHeadroom();
   ACR_TestRegress.Check("civilians: plugged mod's AI slots raise the active AI limit", plug.AiHeadroom() > 0 && aiWorld && aiWorld.GetLimitOfActiveAIs() >= wanted, string.Format("%1: %2 slots, limit %3, wanted %4", plug.Name(), plug.AiHeadroom(), aiWorld.GetLimitOfActiveAIs(), wanted));
   // the whole path: Alpha's man kills the civilian, the mod records it, the socket reports it
   KillBy(s_Civilian, anchor);
  }
  if (tick == 25 && ACR_CivilianSocket.Get().Plugged())
  {
   string line = FeedLine(us, "Civilian killed by our fire");
   ACR_TestRegress.Check("civilians: a real kill reaches the killer's feed through the mod", !line.IsEmpty(), line);
   Finish(9);
  }
 }

 static void Finish(int expected)
 {
  if (s_Civilian) SCR_EntityHelper.DeleteEntityAndChildren(s_Civilian);
  if (s_Enemy) SCR_EntityHelper.DeleteEntityAndChildren(s_Enemy);
  ACR_TestRegress.Complete("civilians", expected);
 }

 static void KillBy(IEntity victim, IEntity killer)
 {
  if (!victim) return;
  SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(victim.FindComponent(SCR_DamageManagerComponent));
  if (!damage) return;
  vector hit[3];
  hit[0] = victim.GetOrigin();
  hit[1] = "0 1 0";
  hit[2] = "0 -1 0";
  SCR_DamageContext context = new SCR_DamageContext(EDamageType.KINETIC, 99999, hit, victim, damage.GetDefaultHitZone(), Instigator.CreateInstigator(killer), null, -1, -1);
  damage.HandleDamage(context);
 }

 static string FeedLine(ACR_Intel intel, string part)
 {
  if (!intel) return "";
  foreach (string line : intel.m_Feed)
   if (line.Contains(part)) return line;
  return "";
 }
}
