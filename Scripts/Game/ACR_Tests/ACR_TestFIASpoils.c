// srv:fiaspoils (Everon whole map, about 3.5 min): FIA spoils, insurgency part 4 (ACR_FIASpoils,
// ACR_FIAPrisoners, ACR_Depots stolen vehicles). Checks:
//  - the dead (with gear) and the knocked-out of a side FIA fights are spoils; a body taken loses its
//    weapons and magazines, and their arms points go to the pool of the hideout feeding the area;
//  - a knocked-out man taken is a prisoner on his way to that hideout and leaves the field (no
//    player near); on arrival the hideout gains a recruit and recruits faster while it holds him,
//    and the US hears FIA's propaganda and gets a lead on the hideout;
//  - an empty, sound vehicle of a side is stealable; FIA's own technical is not;
//  - while the hideout is out its prisoners sit there unarmed and disarmed;
//  - its AI sends one more sweep than usual to a found hideout holding its men;
//  - after the fight a spoils team from the hideout steals the empty UAZ beside it (driven to a depot
//    or the hideout's stash, and on its way there), loots the dead, and comes back;
//  - the hideout out again later: some of its men carry weapon types it looted;
//  - a US man beside the prisoners with no FIA near frees them (RP, feed, the hideout's pool falls);
//    the hideout destroyed frees everyone it still held.
class ACR_TestFIASpoils
{
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 static const ResourceName CAR = "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et";
 protected static ref ACR_DefenderPost s_Hideout; // kept: the post leaves s_Posts once destroyed
 protected static vector s_vVictims;
 protected static IEntity s_Dead;
 protected static IEntity s_Downed;
 protected static IEntity s_Car;
 protected static ResourceName s_Rifleman;
 protected static ref ACR_FIAPow s_Pow;
 protected static float s_fPool;
 protected static int s_iBodies;
 protected static float s_fToYard = -1;
 protected static ACR_Depot s_Depot;
 protected static int s_iRP;
 protected static int s_iFreed;
 protected static bool s_bDone;
 protected static bool s_bCleared;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || s_bDone) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 10) Setup(manager);
  if (!s_Hideout) return;
  if (second == 16) Victims(manager);
  if (second == 20) LootAndTake(manager);
  if (second == 22) OnTheWay(manager);
  if (second == 24) Arrived(manager);
  if (second == 25) Vehicles(manager);
  if (second == 45) AtHideout(manager);
  if (second == 46) Rescue(manager);
  if (second == 47) Fight(manager);
  if (second == 110) Stolen(manager);
  if (second == 150) DrivesHome(manager);
  if (second == 175) Rescuers(manager);
  // Once the hideout is out for them (spawned a few seconds later).
  if (second > 178 && second < 200 && !s_bCleared && s_Hideout.m_Group) Clear(manager);
  if (second == 203) Armed(manager);
  if (second == 205) Freed(manager);
  if (second == 207) Destroy(manager);
  if (second == 216) Destroyed(manager);
 }

 // ---------------------------------------------------------------- setup

 protected static void Setup(ACR_Manager manager)
 {
  ACR_FIASpoils.SetOnForTest(true);
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  foreach (ACR_DefenderPost hideout : hideouts)
  {
   if (s_Hideout) break;
   if (FindVictimsSpot(hideout, s_vVictims)) s_Hideout = hideout;
  }
  if (!s_Hideout)
  {
   ACR_TestRegress.Abort("fiaspoils", string.Format("no hideout with a quiet spot round it (%1 hideouts)", hideouts.Count()));
   return;
  }
  PrintFormat("[ACR] TEST fiaspoils: hideout %1, victims at %2", s_Hideout.m_sName, ACR_Manager.Grid(s_vVictims));
  Spawn(manager, "Spoils Victims", s_vVictims, 0);
  Spawn(manager, "Hideout Bait", s_Hideout.m_vPos, 90);
  s_Car = ACR_Manager.SpawnPrefab(CAR, ACR_Manager.Ground(s_Hideout.m_vPos + Vector(0, 0, 60)));
 }

 // 1-2.5 km from the hideout, fed by it, away from every other cell, base and hideout.
 protected static bool FindVictimsSpot(ACR_DefenderPost hideout, out vector spot)
 {
  for (int i = 0; i < 48; i++)
  {
   float angle = i * Math.PI2 / 16;
   int ring = i / 16;
   float d = 1200 + ring * 600;
   vector at = ACR_Manager.Ground(hideout.m_vPos + Vector(Math.Cos(angle) * d, 0, Math.Sin(angle) * d));
   if (ACR_FIAIntel.SupplierOf(at) != hideout) continue;
   bool near;
   foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
   {
    if (post.m_bGone || !post.m_Cell) continue;
    if (vector.DistanceXZ(post.m_Cell.m_vKillZone, at) < 1000 || vector.DistanceXZ(post.m_vPos, at) < 1000) near = true;
   }
   foreach (ACR_Base base : ACR_Base.s_All) if (!base.m_bRetired && vector.DistanceXZ(base.GetOrigin(), at) < base.m_fRadius + 700) near = true;
   if (near) continue;
   spot = at;
   return true;
  }
  return false;
 }

 protected static void Spawn(ACR_Manager manager, string name, vector at, float distance)
 {
  vector spot = ACR_Manager.Ground(at + Vector(distance, 0, 0));
  manager.m_sSpawningFaction = ACR_FactionSelection.West();
  manager.SpawnUnit(name, FIRE_TEAM, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
 }

 protected static ACR_Squad Find(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == name) return squad;
  return null;
 }

 // One dead, one knocked out, the rest gone: nobody of theirs awake near.
 protected static void Victims(ACR_Manager manager)
 {
  ACR_Squad squad = Find(manager, "Spoils Victims");
  if (!squad) return;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  array<IEntity> men = {};
  foreach (AIAgent agent : agents) if (agent && agent.GetControlledEntity()) men.Insert(agent.GetControlledEntity());
  foreach (int i, IEntity man : men)
  {
   if (i == 0)
   {
    s_Dead = man;
    ACR_DebugCommand.KillEntity(man);
    continue;
   }
   if (i == 1)
   {
    s_Downed = man;
    SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(ChimeraCharacter.Cast(man).GetDamageManager());
    if (damage) damage.ForceUnconsciousness(0);
    continue;
   }
   if (s_Rifleman.IsEmpty() && man.GetPrefabData()) s_Rifleman = man.GetPrefabData().GetPrefabName();
   SCR_EntityHelper.DeleteEntityAndChildren(man);
  }
 }

 // ---------------------------------------------------------------- loot and capture

 protected static void LootAndTake(ACR_Manager manager)
 {
  bool deadSpoil = s_Dead && ACR_FIASpoils.Spoil(s_Dead);
  bool downedSpoil = s_Downed && ACR_FIASpoils.Spoil(s_Downed);
  bool clear = s_Downed && !ACR_FIASpoils.AwakeNear(s_Downed.GetOrigin(), ACR_FIASpoils.FIELD_CLEAR, true);
  ACR_TestRegress.Check("fiaspoils: the dead with gear and the knocked-out of a side FIA fights are spoils, the ground clear", deadSpoil && downedSpoil && clear, string.Format("dead %1, knocked out %2, clear %3", deadSpoil, downedSpoil, clear));
  if (!s_Dead || !s_Downed) return;
  ACR_GuerrillaCell cell = s_Hideout.m_Cell;
  cell.m_fPool = 0;
  int weapons = ACR_FIASpoils.s_iWeapons;
  ACR_FIASpoils.TakeBody(ChimeraCharacter.Cast(s_Dead));
  bool stripped = !ACR_FIASpoils.Spoil(s_Dead) && WeaponsOn(s_Dead) == 0;
  ACR_TestRegress.Check("fiaspoils: looting takes the dead man's weapons and arms recruits at the hideout feeding the area", ACR_FIASpoils.s_iWeapons > weapons && cell.m_fPool > 0 && stripped, string.Format("weapons %1 -> %2, pool %3, stripped %4, arms %5", weapons, ACR_FIASpoils.s_iWeapons, cell.m_fPool, stripped, cell.m_iArms));
  s_fPool = cell.m_fPool;
  ACR_FIASpoils.TakeBody(ChimeraCharacter.Cast(s_Downed));
  foreach (ACR_FIAPow pow : ACR_FIAPrisoners.s_aPows) if (pow.m_Hideout == s_Hideout) s_Pow = pow;
  ACR_TestRegress.Check("fiaspoils: a knocked-out man taken is a prisoner bound for that hideout", s_Pow && !s_Pow.m_bArrived && s_Pow.m_sSide == ACR_FactionSelection.West(), string.Format("prisoners %1, held there %2", ACR_FIAPrisoners.s_aPows.Count(), ACR_FIAPrisoners.HeldAt(s_Hideout)));
 }

 protected static void OnTheWay(ACR_Manager manager)
 {
  bool gone = !s_Downed;
  ACR_TestRegress.Check("fiaspoils: the prisoner leaves the field (no player near) and walks to the hideout", gone && s_Pow && !s_Pow.m_bArrived, string.Format("still in the field %1, arrived %2", !gone, s_Pow && s_Pow.m_bArrived));
  s_fPool = s_Hideout.m_Cell.m_fPool;
  if (s_Pow) s_Pow.m_fArriveAt = 0;
 }

 protected static void Arrived(ACR_Manager manager)
 {
  string us = ACR_FactionSelection.West();
  ACR_Intel intel = manager.IntelFor(us);
  bool told;
  if (intel) foreach (string line : intel.m_Feed) if (line.Contains("FIA propaganda")) told = true;
  bool lead = ACR_FIAIntel.LeadOf(us, s_Hideout) != null || s_Hideout.m_Cell.m_aKnownBy.Contains(us);
  float gain = s_Hideout.m_Cell.m_fPool - s_fPool;
  float fill = ACR_FIAPrisoners.HeldFill(s_Hideout);
  bool arrived = s_Pow && s_Pow.m_bArrived;
  ACR_TestRegress.Check("fiaspoils: propaganda: the hideout gains a recruit and recruits faster while it holds him; the US hears it and gets a lead", arrived && gain > 0.99 && fill > 0 && told && lead, string.Format("arrived %1, pool +%2, held fill %3/min, told %4, lead %5", arrived, gain, fill, told, lead));
 }

 protected static void Vehicles(ACR_Manager manager)
 {
  bool car = s_Car && ACR_Depots.Stealable(s_Car);
  IEntity technical;
  foreach (ACR_Depot depot : ACR_Depots.s_aDepots) foreach (IEntity parked : depot.m_aVehicles) if (parked && !technical) technical = parked;
  bool own = !technical || !ACR_Depots.Stealable(technical);
  ACR_TestRegress.Check("fiaspoils: an empty, sound vehicle of a side is stealable; FIA's own technical is not", car && own, string.Format("UAZ %1, FIA technical %2 (%3)", car, !own, technical != null));
  // A second prisoner at the hideout (a US rifleman's uniform) for the rescue checks.
  if (!s_Rifleman.IsEmpty()) ACR_FIAPrisoners.AddForTest(s_Hideout, ACR_FactionSelection.West(), s_Rifleman);
 }

 protected static int WeaponsOn(IEntity body)
 {
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(body.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return 0;
  array<IEntity> items = {};
  inventory.GetItems(items);
  int count;
  foreach (IEntity item : items) if (item && item.FindComponent(WeaponComponent)) count++;
  return count;
 }

 // ---------------------------------------------------------------- at the hideout

 protected static void AtHideout(ACR_Manager manager)
 {
  int shown, held;
  string detail = string.Format("hideout out %1; ", s_Hideout.m_Group != null);
  foreach (ACR_FIAPow pow : ACR_FIAPrisoners.s_aPows)
  {
   if (pow.m_Hideout != s_Hideout) continue;
   held++;
   if (!pow.m_Body) continue;
   PerceivableComponent perceivable = PerceivableComponent.Cast(pow.m_Body.FindComponent(PerceivableComponent));
   bool disarmed = perceivable && perceivable.IsDisarmed();
   int weapons = WeaponsOn(pow.m_Body);
   if (ACR_Respawn.IsAlive(pow.m_Body) && disarmed && weapons == 0) shown++;
   detail += string.Format("body alive %1, disarmed %2, weapons %3; ", ACR_Respawn.IsAlive(pow.m_Body), disarmed, weapons);
  }
  ACR_TestRegress.Check("fiaspoils: while the hideout is out its prisoners sit there, unarmed and out of the fight", s_Hideout.m_Group != null && held == 2 && shown == 2, detail);
 }

 // Its AI: a found hideout holding US men is swept beyond the usual three.
 protected static void Rescue(ACR_Manager manager)
 {
  ACR_DangerRoad mark;
  foreach (ACR_DangerRoad road : ACR_Insurgency.s_aRoads) if (road.m_Hideout == s_Hideout && road.m_sSide == ACR_FactionSelection.West()) mark = road;
  ACR_AICommander ai = manager.FobAICommanderFor(ACR_FactionSelection.West());
  if (!mark || !ai)
  {
   ACR_TestRegress.Skip("fiaspoils: its AI sweeps a found hideout holding its men beyond the usual three", "the US hasn't found the hideout");
   return;
  }
  mark.m_iSweeps = 3;
  if (mark.m_Sweeper && mark.m_Sweeper.m_iAITask == ACR_EAITask.SWEEP) mark.m_Sweeper.SetAITask(ACR_EAITask.FREE);
  mark.m_Sweeper = null;
  ai.ManageDangerRoadsForTest();
  ACR_Squad sent = mark.m_Sweeper;
  ACR_TestRegress.Check("fiaspoils: its AI sweeps a found hideout holding its men beyond the usual three", sent != null, string.Format("held %1, sweeps %2, sweeper %3", ACR_FIAPrisoners.HeldAt(s_Hideout, ACR_FactionSelection.West()), mark.m_iSweeps, sent));
  // Not now: the spoils checks want nobody of theirs awake near.
  if (sent)
  {
   sent.SetAITask(ACR_EAITask.FREE);
   sent.Halt("Test");
  }
  mark.m_Sweeper = null;
  mark.m_iSweeps = 99;
 }

 // The bait dies: the hideout holds the field, and its spoils teams go out.
 protected static void Fight(ACR_Manager manager)
 {
  ACR_Squad bait = Find(manager, "Hideout Bait");
  if (bait)
  {
   array<AIAgent> agents = {};
   bait.GetAgents(agents);
   foreach (AIAgent agent : agents) if (agent && ACR_Respawn.IsAlive(agent.GetControlledEntity())) ACR_DebugCommand.KillEntity(agent.GetControlledEntity());
  }
  s_iBodies = ACR_FIASpoils.s_iBodies;
  ACR_FIASpoils.s_bLookForTest = true;
 }

 protected static void Stolen(ACR_Manager manager)
 {
  foreach (ACR_Depot depot : ACR_Depots.s_aDepots) if (s_Car && depot.m_aVehicles.Contains(s_Car)) s_Depot = depot;
  bool fia = s_Car && ACR_Vehicles.BoughtBy(s_Car) == ACR_Pockets.FACTION;
  if (s_Depot && s_Car) s_fToYard = vector.DistanceXZ(s_Car.GetOrigin(), ACR_Depots.Yard(s_Depot));
  string where = "none";
  if (s_Depot) where = ACR_Depots.NameOf(s_Depot);
  ACR_TestRegress.Check("fiaspoils: a spoils team steals the empty UAZ by the hideout for a depot or the hideout's stash", fia && s_Depot != null, string.Format("FIA's %1, depot %2, %3 m from its yard", fia, where, (int)s_fToYard));
  ACR_TestRegress.Check("fiaspoils: a spoils team loots the dead after the fight", ACR_FIASpoils.s_iBodies > s_iBodies, string.Format("bodies looted %1 -> %2, teams out %3", s_iBodies, ACR_FIASpoils.s_iBodies, ACR_FIASpoils.s_aTeams.Count()));
 }

 protected static void DrivesHome(ACR_Manager manager)
 {
  if (!s_Depot || !s_Car)
  {
   ACR_TestRegress.Skip("fiaspoils: the stolen vehicle drives to its yard, or parks by it", "nothing stolen");
  }
  else
  {
   float now = vector.DistanceXZ(s_Car.GetOrigin(), ACR_Depots.Yard(s_Depot));
   bool home = s_Depot.m_iState == ACR_Depots.IDLE && now <= ACR_Depots.PARK_NEAR;
   ACR_TestRegress.Check("fiaspoils: the stolen vehicle drives to its yard, or parks by it", home || now < s_fToYard - 20, string.Format("%1 m -> %2 m, state %3, home %4", (int)s_fToYard, (int)now, s_Depot.m_iState, home));
  }
  ACR_TestRegress.Check("fiaspoils: spoils teams come back and rejoin", ACR_FIASpoils.s_iRejoined > 0, string.Format("rejoined %1, out %2", ACR_FIASpoils.s_iRejoined, ACR_FIASpoils.s_aTeams.Count()));
 }

 // ---------------------------------------------------------------- freed

 protected static void Rescuers(ACR_Manager manager)
 {
  ACR_FIASpoils.s_bLookForTest = false;
  s_iRP = manager.m_Bases.GetFaction(ACR_FactionSelection.West()).m_iRP;
  s_iFreed = ACR_FIAPrisoners.s_iFreed;
  s_fPool = s_Hideout.m_Cell.m_fPool;
  Spawn(manager, "Rescuers", s_Hideout.m_vPos, 4);
 }

 // The hideout's men away from their prisoners (as if out of the windows fighting elsewhere).
 protected static void Clear(ACR_Manager manager)
 {
  s_bCleared = true;
  if (!s_Hideout.m_Group) return;
  array<AIAgent> agents = {};
  s_Hideout.m_Group.GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man && ACR_Respawn.IsAlive(man)) man.SetOrigin(ACR_Manager.Ground(s_Hideout.m_vPos + Vector(250 + i * 3, 0, 0)));
  }
 }

 // The hideout came out again after its looting: some of its men carry weapon types it looted.
 protected static void Armed(ACR_Manager manager)
 {
  array<ResourceName> tally = s_Hideout.m_Cell.m_aLootedArms;
  int carrying, men;
  if (s_Hideout.m_Group)
  {
   array<AIAgent> agents = {};
   s_Hideout.m_Group.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity man = agent.GetControlledEntity();
    if (!man || !ACR_Respawn.IsAlive(man)) continue;
    men++;
    IEntity primary = ACR_SquadMarksmen.Primary(man);
    if (primary && primary.GetPrefabData() && tally.Contains(primary.GetPrefabData().GetPrefabName())) carrying++;
   }
  }
  ACR_TestRegress.Check("fiaspoils: a looted weapon type turns up in the hands of the hideout's later recruits", tally.Count() > 0 && carrying > 0, string.Format("%1 in the tally, %2 of %3 men carry one, %4 swaps confirmed", tally.Count(), carrying, men, ACR_FIASpoils.s_iArmed));
 }

 protected static void Freed(ACR_Manager manager)
 {
  int held = ACR_FIAPrisoners.HeldAt(s_Hideout);
  int freed = ACR_FIAPrisoners.s_iFreed - s_iFreed;
  int rp = manager.m_Bases.GetFaction(ACR_FactionSelection.West()).m_iRP - s_iRP;
  ACR_Intel intel = manager.IntelFor(ACR_FactionSelection.West());
  bool told;
  if (intel) foreach (string line : intel.m_Feed) if (line.Contains("freed from")) told = true;
  bool fell = s_Hideout.m_Cell.m_fPool < s_fPool || s_fPool < 0.01;
  ACR_TestRegress.Check("fiaspoils: US men beside the prisoners, no FIA near: freed, 3 RP each, a feed line, the hideout's pool falls", freed >= 2 && held == 0 && rp >= freed * ACR_Requisition.REINFORCE_COST_PER_SOLDIER && told && fell, string.Format("freed %1, still held %2, RP +%3, told %4, pool %5 -> %6, hideout out %7", freed, held, rp, told, s_fPool, s_Hideout.m_Cell.m_fPool, s_Hideout.m_Group != null));
 }

 protected static void Destroy(ACR_Manager manager)
 {
  s_iFreed = ACR_FIAPrisoners.s_iFreed;
  if (!s_Rifleman.IsEmpty()) ACR_FIAPrisoners.AddForTest(s_Hideout, ACR_FactionSelection.West(), s_Rifleman);
  s_Hideout.m_iReserve = 0;
  if (!s_Hideout.m_Group) return;
  array<AIAgent> agents = {};
  s_Hideout.m_Group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man) SCR_EntityHelper.DeleteEntityAndChildren(man);
  }
 }

 protected static void Destroyed(ACR_Manager manager)
 {
  int freed = ACR_FIAPrisoners.s_iFreed - s_iFreed;
  ACR_TestRegress.Check("fiaspoils: the hideout destroyed frees everyone it held", s_Hideout.m_bGone && freed == 1 && ACR_FIAPrisoners.HeldAt(s_Hideout) == 0, string.Format("gone %1, freed %2, held %3", s_Hideout.m_bGone, freed, ACR_FIAPrisoners.HeldAt(s_Hideout)));
  s_bDone = true;
  ACR_TestRegress.Complete("fiaspoils", 15);
 }
}
