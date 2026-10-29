// Place one of these in any world to run Arma Commander there. Starts the
// mode, spawns ACR_StartingUnit entities, and drives the local input.
class ACR_ModeClass : GenericEntityClass {}
class ACR_Mode : GenericEntity
{
 [Attribute("1", UIWidgets.CheckBox, "Game Master for the host and server admins")]
 bool m_bHostGameMaster;
 [Attribute("1", UIWidgets.CheckBox, "Shared command: every player of a faction may command (host option -acrSharedCommand 0|1)")]
 bool m_bSharedCommand;

 [Attribute("70", UIWidgets.ComboBox, "Skill of all Arma Commander AI soldiers", "", ParamEnumArray.FromEnum(EAISkill))]
 int m_iAISkill;
 [Attribute("1.6", UIWidgets.EditBox, "AI reaction speed: how fast AI spot and identify targets (game default 1)")]
 float m_fAIPerception;
 [Attribute("15", UIWidgets.EditBox, "Seconds before a killed command unit respawns at base")]
 int m_iRespawnSeconds;

 static ACR_Mode s_Instance;
 protected bool m_bSummary;
 protected bool m_bSoak;
 protected bool m_bPerf;
 protected int m_iSoakEnd = 1200;
 protected int m_iSeconds;

 void ACR_Mode(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT | EntityEvent.FRAME | EntityEvent.POSTFRAME | EntityEvent.FIXEDFRAME);
 }

 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode()) return;
  s_Instance = this;
  // Repeatable larger-battle profiling preset; ordinary matches retain their host options.
  if (System.IsCLIParam("acrPerfScaleTest"))
  {
   ACR_Options.Set("acrThreat", "high");
   ACR_Options.Set("acrStartRP", "500");
   ACR_Options.Set("acrSoldierCap", "300");
   ACR_Options.Set("acrUnitCap", "40");
  }
  ACR_FactionSelection.Apply();
  ACR_Manager manager = ACR_Manager.Start();
  manager.m_bHostGameMaster = m_bHostGameMaster;
  manager.m_SharedCommand = m_bSharedCommand;
  manager.m_iRespawnSeconds = m_iRespawnSeconds;
  if (m_iAISkill > 0) manager.m_iAISkill = m_iAISkill;
  if (m_fAIPerception > 0) manager.m_fAIPerception = m_fAIPerception;
  string respawnFlag;
  if (ACR_Options.Get("acrRespawnSeconds", respawnFlag)) manager.m_iRespawnSeconds = respawnFlag.ToInt();
  if (Replication.IsServer())
  {
   GetGame().GetCallqueue().CallLater(SpawnStartingUnits, 2000, false);
   string flag;
   // -acrSkirmishPlayTest: the scenario summary on the Montignac flashpoint (AI vs AI).
   m_bSummary = System.GetCLIParam("acrScenarioTest", flag) || System.IsCLIParam("acrSkirmishPlayTest");
   m_bSoak = System.GetCLIParam("acrSoakTest", flag);
   // -acrPerfTest: the soak battle for 3 minutes with tick timings in the log.
   m_bPerf = System.GetCLIParam("acrPerfTest", flag) || System.IsCLIParam("acrPerfScaleTest");
   if (m_bPerf) { m_bSoak = true; ACR_Perf.s_bOn = true; }
   // -acrEveronSoakTest: the 20-minute soak with tick timings (Everon at scale).
   if (System.IsCLIParam("acrEveronSoakTest")) { m_bSoak = true; ACR_Perf.s_bOn = true; }
   // -acrLongSoakTest: the same soak for 2 hours (long-session stability and drift).
   if (System.IsCLIParam("acrLongSoakTest")) { m_bSoak = true; ACR_Perf.s_bOn = true; m_iSoakEnd = 7200; }
   if (m_bSoak || m_bSummary) ACR_Perf.s_bChecks = true;
   GetGame().GetCallqueue().CallLater(Second, 1000, true);
  }
  Print("[ACR] Scenario started");
 }

 protected void SpawnStartingUnits()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  // Setup phase: the layout (and so the HQs) is only known once the host presses Start.
  if (!ACR_Setup.Started() || !ACR_Skirmish.Applied())
  {
   GetGame().GetCallqueue().CallLater(SpawnStartingUnits, 1000, false);
   return;
  }
  // Company kits (#38): the host options (the setup's company rows) first.
  if (ACR_DoctrineKits.Enabled() && !manager.HostOptionsApplied())
  {
   GetGame().GetCallqueue().CallLater(SpawnStartingUnits, 1000, false);
   return;
  }
  ACR_DoctrineKits.Plan(manager);
  foreach (ACR_StartingUnit unit : ACR_StartingUnit.s_All)
  {
   string factionKey = "";
   float nearest = 1000000;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner.IsEmpty()) continue;
    float distance = vector.Distance(unit.GetOrigin(), base.GetOrigin());
    if (distance < nearest) { nearest = distance; factionKey = base.m_sOwner; }
   }
   ResourceName group = unit.m_sGroup;
   ResourceName vehicle = unit.m_sVehicle;
   ResourceName passengers = unit.m_sPassengers;
   if (!factionKey.IsEmpty())
   {
    group = ACR_FactionCatalog.ResolvePrefab(factionKey, group, EEntityCatalogType.GROUP);
    vehicle = ACR_FactionCatalog.ResolvePrefab(factionKey, vehicle, EEntityCatalogType.VEHICLE);
    passengers = ACR_FactionCatalog.ResolvePrefab(factionKey, passengers, EEntityCatalogType.GROUP);
   }
   vector pos = unit.GetOrigin();
   vector vehiclePos = ACR_Manager.Ground(pos);
   vector groupPos = ACR_Manager.Ground(pos + Vector(6, 0, 6));
   // A skirmish layout moved this side's HQ: start at the new one.
   if (unit.m_MovedTo)
   {
    vehiclePos = manager.ClearSpotNear(unit.m_MovedTo, 10, 30, 4);
    if (vehicle.IsEmpty()) groupPos = manager.ClearSpotNear(unit.m_MovedTo, 5, 20, 1.5);
    else groupPos = ACR_Manager.Ground(vehiclePos + Vector(6, 0, 6));
   }
   // A mechanized company's kit: its carrier in place of this truck (bought with the rest of the kit).
   if (ACR_DoctrineKits.SwapsTruck(manager, factionKey, unit.m_sName, group, vehicle, unit.m_bCrewed, passengers, vehiclePos, groupPos)) continue;
   // The side owns its starting units like bought ones: crews and members join it (with a
   // mod pair the crews kept the vehicle's vanilla US/USSR side and kit, 8 Oct).
   manager.m_sSpawningFaction = factionKey;
   manager.SpawnUnit(unit.m_sName, group, vehicle, unit.m_bCrewed, passengers, vehiclePos, groupPos);
   manager.m_sSpawningFaction = "";
   ACR_DoctrineKits.CountStarting(factionKey, group, vehicle, unit.m_bCrewed, passengers);
   PrintFormat("[ACR] Starting unit: %1 (%2) at %3: %4 %5 %6", unit.m_sName, factionKey, groupPos, FilePath.StripPath(group), FilePath.StripPath(vehicle), FilePath.StripPath(passengers));
  }
  if (ACR_DoctrineKits.Enabled()) ACR_DoctrineKits.GiveAll();
  ACR_QuickStart.Fortify();
 }

 // Optional log summary for automated runs (-acrScenarioTest 1).
 protected void Second()
 {
  m_iSeconds++;
  ACR_TestHooks.Get().ScenarioSecond(m_iSeconds);
  if (m_bSoak) { Soak(); return; }
  if (!m_bSummary || m_iSeconds % 30 != 0) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  PrintFormat("[ACR] SCENARIO t=%1 squads=%2 US rp=%3 USSR rp=%4", m_iSeconds, manager.m_Squads.Count(), manager.m_Bases.GetRP("US"), manager.m_Bases.GetRP("USSR"));
  foreach (ACR_Base base : ACR_Base.s_All)
   PrintFormat("[ACR] SCENARIO   %1: %2", base.m_sName, manager.m_Bases.Describe(base));
  if (m_iSeconds >= 180) Print("[ACR] TEST COMPLETE");
 }

 // -acrSoakTest: AI against AI for 20 minutes (the player hands command to the
 // AI), with a summary every minute for reading the whole battle afterwards.
 protected void Soak()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (m_iSeconds == 5)
  {
   array<int> players = {};
   GetGame().GetPlayerManager().GetPlayers(players);
   foreach (int id : players)
   {
    SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
    if (pc) PrintFormat("[ACR] SOAK player %1: %2", id, manager.LeaveCommand(pc));
   }
  }
  if (m_iSeconds % 60 == 0)
  {
   int us, ussr, usMen, ussrMen;
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (!squad.GetFaction()) continue;
    if (squad.GetFaction().GetFactionKey() == "US") { us++; usMen += squad.CountAlive(); }
    else { ussr++; ussrMen += squad.CountAlive(); }
   }
   PrintFormat("[ACR] SOAK t=%1 | US %2 squads/%3 men rp %4 | USSR %5 squads/%6 men rp %7", m_iSeconds, us, usMen, manager.m_Bases.GetRP("US"), ussr, ussrMen, manager.m_Bases.GetRP("USSR"));
   foreach (ACR_Base base : ACR_Base.s_All)
    PrintFormat("[ACR] SOAK   %1: %2", base.m_sName, manager.m_Bases.Describe(base));
  }
  if (m_iSeconds >= m_iSoakEnd || (m_bPerf && m_iSeconds >= 185)) Print("[ACR] TEST COMPLETE");
 }

 // After the frame's other updates (gunner drives re-assert their inputs; horn governor mutes latch loops).
 override void EOnPostFrame(IEntity owner, float timeSlice)
 {
  ACR_Transport.UpdateHorns();
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (s_Instance == this && manager) manager.SimulateStep();
 }

 // Every fixed (physics) frame, several per frame at low fps: gunner drives write their
 // inputs again. (EOnSimulate never fires for this entity.)
 override void EOnFixedFrame(IEntity owner, float timeSlice)
 {
  ACR_GunnerDrive.s_iFixedCalls++;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (s_Instance == this && manager) manager.SimulateStep(1);
 }

 override void EOnFrame(IEntity owner, float timeSlice)
 {
  // Skirmish size: the chosen layout is set up before anything else runs (ACR_Skirmish).
  if (s_Instance == this && Replication.IsServer() && ACR_Setup.Started()) ACR_Skirmish.ApplyOnce();
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (s_Instance == this && manager)
  {
   manager.ServerFrame(timeSlice);
   manager.OnFrame();
  }
 }

 void ~ACR_Mode()
 {
  if (GetGame())
  {
   GetGame().GetCallqueue().Remove(SpawnStartingUnits);
   GetGame().GetCallqueue().Remove(Second);
  }
  if (s_Instance == this)
  {
   s_Instance = null;
   ACR_Manager.Stop();
  }
 }
}

// A unit that exists at the start of the match, placed where it should appear.
// Same fields as a requisition entry; the name is used as-is (no number).
class ACR_StartingUnitClass : GenericEntityClass {}
class ACR_StartingUnit : GenericEntity
{
 [Attribute("", UIWidgets.EditBox, "Name shown on the map, e.g. 1st Squad")]
 string m_sName;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Infantry group", "et")]
 ResourceName m_sGroup;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Vehicle (optional)", "et")]
 ResourceName m_sVehicle;
 [Attribute("1", UIWidgets.CheckBox, "Vehicle comes with its driver and gunners")]
 bool m_bCrewed;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Group riding in the vehicle (optional)", "et")]
 ResourceName m_sPassengers;

 static ref array<ACR_StartingUnit> s_All = {};
 ACR_Base m_MovedTo; // runtime: a skirmish layout put this side's HQ at another base

 void ACR_StartingUnit(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT);
 }
 override void EOnInit(IEntity owner)
 {
  if (GetGame().InPlayMode()) s_All.Insert(this);
 }
 void ~ACR_StartingUnit()
 {
  if (s_All) s_All.RemoveItem(this);
 }
}
