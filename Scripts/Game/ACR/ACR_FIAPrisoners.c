// FIA prisoners (insurgency part 4, Jacob 19 Nov: "The taking of prisoners will be a propaganda and
// garner them support. POWs may be found at hideouts to reduce their influence.")
//  - Taken: a spoils team (ACR_FIASpoils) reaches a man of a side FIA fights who is knocked out or
//    has surrendered, with none of the enemy awake near. He is taken to the hideout that feeds the
//    area (ACR_FIAIntel.SupplierOf), HOLD_MAX at most at a hideout. The escort is off the map: he
//    leaves the field once no player is within VANISH_SIGHT (at once if none), and reaches the
//    hideout after the walk (ESCORT_SPEED, at least ESCORT_MIN_S).
//  - A player taken (ruling for Jacob): his body dies there as any death and he goes to his
//    deploy screen (or back to his command unit if he was in a soldier); he and his side are told
//    he is held at a hideout, and a soldier in his uniform waits there to be freed.
//  - Propaganda: on arrival the hideout gains PROPAGANDA_RECRUITS, and while it holds prisoners it
//    recruits a little faster (HeldFill, ACR_FIAGrowth.FillRate). The captive's side hears FIA's
//    propaganda (a feed line) and gets a lead on the hideout (ACR_FIAIntel): where to look.
//  - Found: while the hideout is out (enemies near), its prisoners sit unarmed in its middle,
//    hands empty, their AI off. A man of their side (AI or player) within FREE_REACH, with no
//    awake FIA within FREE_CLEAR, frees one; a hideout destroyed frees all it held. Freed men go
//    back to their side: REINFORCE_COST_PER_SOLDIER RP each (what replacing him costs), a feed
//    line, and the hideout loses FREED_POOL recruits each (its influence cut). They walk off and
//    leave the field once no player is near. Killed in the crossfire: lost.
//  - The AI commander sends more, and from further, to a found hideout holding its men
//    (ACR_AICommander.ManageDangerRoads).
class ACR_FIAPow : Managed
{
 ACR_DefenderPost m_Hideout;
 string m_sSide;
 ResourceName m_Prefab;
 string m_sName;        // a player's name ("" for a soldier)
 int m_iPlayer;         // his player id then
 float m_fArriveAt;
 bool m_bArrived;
 IEntity m_Body;        // shown at the hideout while it is out
 IEntity m_Captive;     // the man in the field until he leaves it (a revived man is no prisoner)
 vector m_vTakenAt;
}

class ACR_FIAPrisoners
{
 static const int HOLD_MAX = 6;               // prisoners a hideout holds at most
 static const float ESCORT_SPEED = 1.2;       // m/s to the hideout
 static const float ESCORT_MIN_S = 60;
 static const float VANISH_SIGHT = 200;       // a taken (or freed) man leaves the field once no player is this close
 static const float PROPAGANDA_RECRUITS = 1;  // to the hideout's pool when a prisoner arrives
 static const float HELD_FILL_PER_MIN = 0.02; // its recruits a minute for each prisoner held ...
 static const int HELD_FILL_MAX = 4;          // ... counting this many at most
 static const float FREE_REACH = 15;          // a man of his side this close frees him ...
 static const float FREE_CLEAR = 30;          // ... with no awake FIA this close
 static const float FREED_POOL = 1;           // a hideout's recruits lost for each man freed
 static const float SHOW_SPREAD = 5;
 static ref array<ref ACR_FIAPow> s_aPows;
 protected static ref array<IEntity> s_aVanish; // bodies leaving the field once no player is near
 static int s_iTaken;
 static int s_iFreed;
 static int s_iLost;

 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_aPows = {};
  s_aVanish = {};
 }

 // Per match (ACR_Manager.Stop) and at ACR_FIASpoils.Apply. Bodies went with the old world.
 static void Reset()
 {
  s_aPows.Clear();
  s_aVanish.Clear();
  s_iTaken = 0;
  s_iFreed = 0;
  s_iLost = 0;
 }

 // Prisoners of 'side' ("" any) a hideout holds or has on the way to it.
 static int HeldAt(ACR_DefenderPost hideout, string side = "", bool arrivedOnly = false)
 {
  int count;
  foreach (ACR_FIAPow pow : s_aPows)
  {
   if (pow.m_Hideout != hideout || (!side.IsEmpty() && pow.m_sSide != side) || (arrivedOnly && !pow.m_bArrived)) continue;
   count++;
  }
  return count;
 }

 // Extra recruits a minute from the prisoners a hideout holds (ACR_FIAGrowth.FillRate).
 static float HeldFill(ACR_DefenderPost hideout)
 {
  return Math.Min(HeldAt(hideout, "", true), HELD_FILL_MAX) * HELD_FILL_PER_MIN;
 }

 // ---------------------------------------------------------------- taken

 // A man out of the fight taken by a spoils team (false: no hideout to take him to, or it's full).
 static bool Capture(ChimeraCharacter man, ACR_DefenderPost hideout)
 {
  if (!man || !ACR_Respawn.IsAlive(man)) return false;
  string side = ACR_Intel.FactionKeyOf(man);
  if (!ACR_IED.Target(ACR_Pockets.FACTION, side)) return false;
  if (!hideout || hideout.m_bGone || !hideout.m_Cell || HeldAt(hideout) >= HOLD_MAX)
  {
   PrintFormat("[ACR] PRISONERS no hideout with room for a prisoner taken at %1: left where he lies", ACR_Manager.Grid(man.GetOrigin()));
   return false;
  }
  ACR_Manager manager = ACR_Manager.GetInstance();
  float now = GetGame().GetWorld().GetWorldTime();
  ACR_FIAPow pow = new ACR_FIAPow();
  pow.m_Hideout = hideout;
  pow.m_sSide = side;
  if (man.GetPrefabData()) pow.m_Prefab = man.GetPrefabData().GetPrefabName();
  pow.m_vTakenAt = man.GetOrigin();
  float walk = Math.Max(ESCORT_MIN_S, vector.DistanceXZ(man.GetOrigin(), hideout.m_vPos) / ESCORT_SPEED);
  pow.m_fArriveAt = now + walk * 1000;
  int player = ACR_Squad.FindController(man);
  if (player > 0)
  {
   pow.m_iPlayer = player;
   pow.m_sName = GetGame().GetPlayerManager().GetPlayerName(player);
  }
  s_aPows.Insert(pow);
  s_iTaken++;
  // Out of his squad: nothing orders him again, and his squad counts him gone.
  AIControlComponent control = AIControlComponent.Cast(man.FindComponent(AIControlComponent));
  if (player <= 0 && control && control.GetControlAIAgent() && control.GetControlAIAgent().GetParentGroup())
   control.GetControlAIAgent().GetParentGroup().RemoveAgent(control.GetControlAIAgent());
  string where = ACR_Manager.Grid(man.GetOrigin());
  if (player > 0)
  {
   // Ruling for Jacob: a captured player isn't left lying there, he redeploys (his body dies as any death).
   if (manager) manager.Notify(player, string.Format("Taken prisoner by the FIA at %1. You're held at a hideout until your side frees you; redeploy meanwhile.", where));
   ACR_DebugCommand.KillEntity(man);
   s_aVanish.Insert(man);
   ACR_Intel playerIntel;
   if (manager) playerIntel = manager.IntelFor(side);
   if (playerIntel) playerIntel.AddFeed(now, string.Format("%1 taken prisoner by the FIA at %2", pow.m_sName, where), pow.m_vTakenAt);
  }
  else
  {
   pow.m_Captive = man;
   s_aVanish.Insert(man);
  }
  PrintFormat("[ACR] PRISONERS %1 of %2 taken at %3, to %4 (%5 s on foot, %6 held there)", Who(pow), side, where, hideout.m_sName, (int)walk, HeldAt(hideout));
  return true;
 }

 protected static string Who(ACR_FIAPow pow)
 {
  if (!pow.m_sName.IsEmpty()) return pow.m_sName;
  return "a soldier";
 }

 // ---------------------------------------------------------------- every second (ACR_FIASpoils.Tick)

 static void Tick(ACR_Manager manager, float now)
 {
  TickVanish();
  for (int i = s_aPows.Count() - 1; i >= 0; i--)
  {
   ACR_FIAPow pow = s_aPows[i];
   // Died of his wounds before he left the field: no prisoner.
   if (pow.m_Captive && !ACR_Respawn.IsAlive(pow.m_Captive) && pow.m_iPlayer <= 0)
   {
    pow.m_Captive = null;
    s_aPows.Remove(i);
    s_iTaken--;
    continue;
   }
   // Still in the field and back on his feet (revived, a ruse): no prisoner after all.
   if (pow.m_Captive && ACR_Squad.IsConscious(pow.m_Captive) && !ACR_FIASpoils.OutOfFight(ChimeraCharacter.Cast(pow.m_Captive)))
   {
    s_aVanish.RemoveItem(pow.m_Captive);
    PrintFormat("[ACR] PRISONERS a prisoner taken at %1 is up again before he left the field: free", ACR_Manager.Grid(pow.m_vTakenAt));
    s_aPows.Remove(i);
    s_iTaken--;
    continue;
   }
   if (!pow.m_Hideout || pow.m_Hideout.m_bGone)
   {
    Free(manager, i, "his escort lost its hideout", now);
    continue;
   }
   if (!pow.m_bArrived)
   {
    if (now >= pow.m_fArriveAt && !pow.m_Captive) Arrive(manager, pow, now);
    continue;
   }
   Show(pow);
   if (!pow.m_Body) continue;
   if (!ACR_Respawn.IsAlive(pow.m_Body))
   {
    Lost(manager, i, now);
    continue;
   }
   vector at = pow.m_Body.GetOrigin();
   if (ACR_FIASpoils.AwakeNear(at, FREE_REACH, false, pow.m_sSide) && !ACR_FIASpoils.AwakeNear(at, FREE_CLEAR, false, ACR_Pockets.FACTION))
    Free(manager, i, "freed by his side", now);
  }
 }

 // Reached the hideout: propaganda.
 protected static void Arrive(ACR_Manager manager, ACR_FIAPow pow, float now)
 {
  pow.m_bArrived = true;
  ACR_DefenderPost hideout = pow.m_Hideout;
  hideout.m_Cell.m_fPool = Math.Min(ACR_FIAGrowth.PoolCap(), hideout.m_Cell.m_fPool + PROPAGANDA_RECRUITS);
  int held = HeldAt(hideout, pow.m_sSide, true);
  PrintFormat("[ACR] PRISONERS %1 of %2 reached %3 (%4 of theirs held there, pool %5)", Who(pow), pow.m_sSide, hideout.m_sName, held, hideout.m_Cell.m_fPool);
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(pow.m_sSide);
  if (!intel) return;
  string line = string.Format("FIA propaganda: they hold %1 of our men", held);
  if (hideout.m_Cell.m_aKnownBy.Contains(pow.m_sSide)) line = line + string.Format(" at the hideout at %1", ACR_Manager.Grid(hideout.m_vPos));
  intel.AddFeed(now, line);
  // Where it came from is a lead (a ring that tightens with each), unless they know the hideout.
  ACR_FIAIntel.Lead(pow.m_sSide, hideout, "FIA propaganda about our captured men");
  if (pow.m_iPlayer > 0 && GetGame().GetPlayerManager().GetPlayerController(pow.m_iPlayer))
   manager.Notify(pow.m_iPlayer, "The FIA parade you in their propaganda: you're held at a hideout.");
 }

 // ---------------------------------------------------------------- at the hideout

 // In sight while the hideout is out (its men in the windows), gone inside while it is packed up.
 protected static void Show(ACR_FIAPow pow)
 {
  bool shown = pow.m_Hideout.m_Group != null;
  if (shown && !pow.m_Body) pow.m_Body = SpawnPrisoner(pow);
  else if (!shown && pow.m_Body)
  {
   SCR_EntityHelper.DeleteEntityAndChildren(pow.m_Body);
   pow.m_Body = null;
  }
 }

 protected static IEntity SpawnPrisoner(ACR_FIAPow pow)
 {
  if (pow.m_Prefab.IsEmpty()) return null;
  float angle = Math.RandomFloat(0, Math.PI2);
  vector spot = ACR_Manager.Ground(pow.m_Hideout.m_vPos + Vector(Math.Cos(angle) * SHOW_SPREAD, 0, Math.Sin(angle) * SHOW_SPREAD));
  IEntity body = ACR_Manager.SpawnPrefab(pow.m_Prefab, spot);
  ChimeraCharacter man = ChimeraCharacter.Cast(body);
  if (!man)
  {
   if (body) SCR_EntityHelper.DeleteEntityAndChildren(body);
   return null;
  }
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(man.FindComponent(FactionAffiliationComponent));
  if (affiliation) affiliation.SetAffiliatedFactionByKey(pow.m_sSide);
  // Unarmed: his weapons and magazines were taken.
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(man.FindComponent(SCR_InventoryStorageManagerComponent));
  if (inventory)
  {
   array<IEntity> items = {};
   inventory.GetItems(items);
   foreach (IEntity item : items)
    if (item && (item.FindComponent(WeaponComponent) || item.FindComponent(BaseMagazineComponent))) inventory.TryDeleteItem(item);
  }
  AIControlComponent control = AIControlComponent.Cast(man.FindComponent(AIControlComponent));
  if (control && control.IsAIActivated()) control.DeactivateAI();
  // What makes AI leave him alone (as the unconscious and BBB's prisoners).
  PerceivableComponent perceivable = PerceivableComponent.Cast(man.FindComponent(PerceivableComponent));
  if (perceivable) perceivable.SetDisarmed(true);
  GetGame().GetCallqueue().CallLater(SitDown, 1500, false, man);
  return man;
 }

 protected static void SitDown(ChimeraCharacter man)
 {
  if (!man || !ACR_Squad.IsConscious(man)) return;
  SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(man.GetCharacterController());
  if (!controller) return;
  vector none[4];
  controller.StartLoitering(null, ELoiteringType.SIT, false, true, false, none);
 }

 // ---------------------------------------------------------------- freed, lost

 static void Free(ACR_Manager manager, int index, string why, float now)
 {
  ACR_FIAPow pow = s_aPows[index];
  s_aPows.Remove(index);
  s_iFreed++;
  ACR_DefenderPost hideout = pow.m_Hideout;
  if (hideout && hideout.m_Cell && !hideout.m_bGone) hideout.m_Cell.m_fPool = Math.Max(0, hideout.m_Cell.m_fPool - FREED_POOL);
  int rp = ACR_Containment.Give(manager, pow.m_sSide, ACR_Requisition.REINFORCE_COST_PER_SOLDIER);
  if (rp > 0) ACR_Report.OnIncome(pow.m_sSide, rp);
  if (pow.m_Captive) s_aVanish.RemoveItem(pow.m_Captive); // never left the field: he stays
  if (pow.m_Body)
  {
   ChimeraCharacter man = ChimeraCharacter.Cast(pow.m_Body);
   SCR_CharacterControllerComponent controller;
   if (man) controller = SCR_CharacterControllerComponent.Cast(man.GetCharacterController());
   if (controller && controller.IsLoitering()) controller.StopLoitering(true);
   s_aVanish.Insert(pow.m_Body);
  }
  string place = "the field";
  if (hideout) place = "the FIA hideout at " + ACR_Manager.Grid(hideout.m_vPos);
  PrintFormat("[ACR] PRISONERS %1 of %2 freed (%3) at %4: +%5 RP", Who(pow), pow.m_sSide, why, place, rp);
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(pow.m_sSide);
  if (intel) intel.AddFeed(now, string.Format("%1 freed from %2, back on duty (+%3 RP)", Who(pow), place, rp));
  if (manager && pow.m_iPlayer > 0 && GetGame().GetPlayerManager().GetPlayerController(pow.m_iPlayer))
   manager.Notify(pow.m_iPlayer, "Your side freed you from the FIA.");
 }

 protected static void Lost(ACR_Manager manager, int index, float now)
 {
  ACR_FIAPow pow = s_aPows[index];
  s_aPows.Remove(index);
  s_iLost++;
  s_aVanish.Insert(pow.m_Body);
  PrintFormat("[ACR] PRISONERS %1 of %2 killed at %3", Who(pow), pow.m_sSide, pow.m_Hideout.m_sName);
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(pow.m_sSide);
  if (intel) intel.AddFeed(now, string.Format("One of our men held by the FIA was killed at %1", ACR_Manager.Grid(pow.m_Hideout.m_vPos)));
 }

 // A hideout destroyed (ACR_Insurgency.HideoutDestroyed): everyone it held, there or on the way, is free.
 static void HideoutDestroyed(ACR_DefenderPost hideout)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = s_aPows.Count() - 1; i >= 0; i--)
   if (s_aPows[i].m_Hideout == hideout) Free(manager, i, "the hideout fell", now);
 }

 // Bodies leaving the field once no player is near (never one a player controls).
 protected static void TickVanish()
 {
  for (int i = s_aVanish.Count() - 1; i >= 0; i--)
  {
   IEntity body = s_aVanish[i];
   if (!body)
   {
    s_aVanish.Remove(i);
    continue;
   }
   if (ACR_Squad.FindController(body) > 0 || PlayerNear(body.GetOrigin(), VANISH_SIGHT)) continue;
   s_aVanish.Remove(i);
   foreach (ACR_FIAPow pow : s_aPows) if (pow.m_Captive == body) pow.m_Captive = null;
   SCR_EntityHelper.DeleteEntityAndChildren(body);
  }
 }

 static bool PlayerNear(vector pos, float range)
 {
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (player && ACR_Respawn.IsAlive(player) && vector.DistanceXZ(player.GetOrigin(), pos) <= range) return true;
  }
  return false;
 }

 // Tests: a prisoner as if already at 'hideout'.
 static ACR_FIAPow AddForTest(ACR_DefenderPost hideout, string side, ResourceName prefab)
 {
  ACR_FIAPow pow = new ACR_FIAPow();
  pow.m_Hideout = hideout;
  pow.m_sSide = side;
  pow.m_Prefab = prefab;
  pow.m_bArrived = true;
  s_aPows.Insert(pow);
  return pow;
 }

 static bool Vanishing(IEntity body) { return s_aVanish.Contains(body); }

 // A prisoner FIA already hold (shown at a hideout) or a man leaving the field: never taken again.
 static bool Held(IEntity body)
 {
  if (!body) return false;
  if (s_aVanish.Contains(body)) return true;
  foreach (ACR_FIAPow pow : s_aPows) if (pow.m_Body == body) return true;
  return false;
 }
}
