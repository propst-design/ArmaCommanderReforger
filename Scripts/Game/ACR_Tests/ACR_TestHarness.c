// Optional automated regression runs for the test range, driven by startup flags:
//   -acrAutoTest 1        possession, chained swaps, map markers, input state
//   -acrCommandTest 1     native squad command response (Move / Follow)
//   -acrMapHandoffTest 1  map gadget + view-control cleanup across swaps
// Runs on the host only. Results are "[ACR] ..." lines in console.log.
class ACR_TestHarness : Managed
{
 ref ACR_TestCommand m_Command = new ACR_TestCommand(this);
 ref ACR_TestVehicles m_Vehicles = new ACR_TestVehicles(this);
 ref ACR_TestBattle m_Battle = new ACR_TestBattle(this);
 ref ACR_TestLogistics m_Logistics = new ACR_TestLogistics(this);

 static const int MODE_VARIETY = 89; // the dynamism picker (ACR_Variety)
 static const int MODE_AUTO = 1;
 static const int MODE_COMMAND = 2;
 static const int MODE_MAP_HANDOFF = 3;
 static const int MODE_VEHICLE = 4;
 static const int MODE_BASES = 5;
 static const int MODE_ASSAULT = 6;
 static const int MODE_REQUISITION = 7;
 static const int MODE_GAMEMASTER = 8;
 static const int MODE_AI = 9;
 static const int MODE_GUNNER = 10;
 static const int MODE_PROBE = 11;
 static const int MODE_RESPAWN = 12;
 static const int MODE_SUPPORT = 13;
 static const int MODE_GUNSHIP = 14;
 static const int MODE_TACTICS = 15;
 static const int MODE_TRANSPORT = 16;
 static const int MODE_STALL = 17;
 static const int MODE_STALLWAIT = 118; // #53: a vehicle waiting for its own crew to board isn't stuck
 static const int MODE_RIDE = 18;
 static const int MODE_LOGISTICS = 19;
 static const int MODE_QUEUE = 20;
 static const int MODE_CLEANUP = 21;
 static const int MODE_SIDE = 22;
 static const int MODE_INTEL = 23;
 static const int MODE_STALLED = 24;
 static const int MODE_HUNT = 25;
 static const int MODE_LAUNCHER = 26;
 static const int MODE_SUPPLIES = 27;
 static const int MODE_DRIVE = 28;
 static const int MODE_ROADS = 29;
 static const int MODE_SPAWNSPOT = 30;
 static const int MODE_TAGS = 31;
 static const int MODE_SHARED = 32;
 static const int MODE_REGRESS = 33;
 static const int MODE_MAPPOLYGON = 117; // the command map's filled-shape guard (ACR_OrderDisplay.PolygonDrawable)
 static const int MODE_GETOUT = 34;
 static const int MODE_MORTAR = 35;
 static const int MODE_CREW = 36;
 static const int MODE_BATTERY = 37;
 static const int MODE_PADS = 38;
 static const int MODE_HELIPAD = 39;
 static const int MODE_GARRISON = 40;
 static const int MODE_STRUCTURES = 41;
 static const int MODE_BLAST = 42;
 static const int MODE_MOTORIZED = 43;
 static const int MODE_FIRE = 44;
 static const int MODE_FACTIONS = 45;
 static const int MODE_ORGAN = 46;
 static const int MODE_FLANK = 47;
 static const int MODE_AIRLIFT = 48;
 static const int MODE_ATTACKPLAN = 49;
 static const int MODE_DRIVELESSGUN = 50;
 static const int MODE_MINEFIELDS = 51;
 static const int MODE_FORTIFY = 52;
 static const int MODE_ROADBLOCKS = 53;
 static const int MODE_FORTIFYFIT = 66;
 static const int MODE_DEFENDERS = 59;
 static const int MODE_BUDGET = 60;
 static const int MODE_AIDEFENCES = 65;
 static const int MODE_ROADSITING = 67;
 static const int MODE_BUILDRIDE = 73;
 static const int MODE_FOBSITING = 76; // 74-75: the slope tilt, the 2IC (own branches)
 static const int MODE_GUNHOLD = 64; // 54-63: the playtest branches
 static const int MODE_POSITIONGUN = 88; // 80-87: the playtest branches, the 2IC and the LAV fixes
 static const int MODE_RIDEJOB = 68; // 54-67: Fortify and the playtest branches
 static const int MODE_MINESITING = 71; // 68-70: ride job, defend swap, LAV ride (own branches)
 static const int MODE_GROUNDTILT = 74; // 69-73: the playtest branches
 static const int MODE_POSSITING = 72; // 68-71: ride job, defend swap, LAV ride, mine siting (own branches)
 static const int MODE_ATTACKREACH = 79; // 69-78: the playtest branches
 static const int MODE_GRABS = 77; // 69-76: the playtest branches
 static const int MODE_MORTARSPOT = 78; // 69-77: the playtest branches
 static const int MODE_RIDELAV = 70; // 69: the playtest branches
 static const int MODE_AIM = 62; // 55-61: patrol, ambush, QRF, FOB AI, defenders, budget, cap scale (own branches)

 ACR_TestRange m_Range;
 int m_Mode;
 int m_Ticks;

 static ACR_TestHarness CreateFromCommandLine(ACR_TestRange range)
 {
  string flag;
  int mode;
  if (System.GetCLIParam("acrAutoTest", flag)) mode = MODE_AUTO;
  if (System.GetCLIParam("acrCommandTest", flag)) mode = MODE_COMMAND;
  if (System.GetCLIParam("acrMapHandoffTest", flag)) mode = MODE_MAP_HANDOFF;
  if (System.GetCLIParam("acrVehicleTest", flag)) mode = MODE_VEHICLE;
  if (System.GetCLIParam("acrBasesTest", flag)) mode = MODE_BASES;
  if (System.GetCLIParam("acrAssaultTest", flag)) mode = MODE_ASSAULT;
  if (System.GetCLIParam("acrRequisitionTest", flag)) mode = MODE_REQUISITION;
  if (System.GetCLIParam("acrGameMasterTest", flag)) mode = MODE_GAMEMASTER;
  if (System.GetCLIParam("acrAITest", flag)) mode = MODE_AI;
  if (System.GetCLIParam("acrGunnerTest", flag)) mode = MODE_GUNNER;
  if (System.GetCLIParam("acrProbeTest", flag)) mode = MODE_PROBE;
  if (System.GetCLIParam("acrRespawnTest", flag) || System.GetCLIParam("acrDeathFadeTest", flag) || System.GetCLIParam("acrRTSCameraTest", flag) || System.GetCLIParam("acrThinkOverlayTest", flag)) mode = MODE_RESPAWN;
  if (System.GetCLIParam("acrSupportTest", flag)) mode = MODE_SUPPORT;
  if (System.GetCLIParam("acrGunshipTest", flag)) mode = MODE_GUNSHIP;
  // -acrHeliTest 1: door gunners (Mi-8) and a rocket run (UH-1H) instead of two gunships.
  if (System.GetCLIParam("acrHeliTest", flag)) { mode = MODE_GUNSHIP; s_bHeliVariant = true; }
  if (System.GetCLIParam("acrTacticsTest", flag)) mode = MODE_TACTICS;
  if (System.GetCLIParam("acrTransportTest", flag)) mode = MODE_TRANSPORT;
  if (System.GetCLIParam("acrStallTest", flag)) mode = MODE_STALL;
  if (System.GetCLIParam("acrStallWaitTest", flag)) mode = MODE_STALLWAIT;
  if (System.GetCLIParam("acrRideTest", flag)) mode = MODE_RIDE;
  if (System.GetCLIParam("acrLogisticsTest", flag)) mode = MODE_LOGISTICS;
  if (System.GetCLIParam("acrQueueTest", flag)) mode = MODE_QUEUE;
  if (System.GetCLIParam("acrCleanupTest", flag)) mode = MODE_CLEANUP;
  if (System.GetCLIParam("acrSideTest", flag)) mode = MODE_SIDE;
  if (System.GetCLIParam("acrIntelTest", flag)) mode = MODE_INTEL;
  if (System.GetCLIParam("acrStalledTest", flag)) mode = MODE_STALLED;
  if (System.GetCLIParam("acrHuntTest", flag)) mode = MODE_HUNT;
  if (System.GetCLIParam("acrLauncherTest", flag)) mode = MODE_LAUNCHER;
  if (System.GetCLIParam("acrSuppliesTest", flag)) mode = MODE_SUPPLIES;
  if (System.GetCLIParam("acrDriveTest", flag)) mode = MODE_DRIVE;
  if (System.GetCLIParam("acrRoadsTest", flag)) mode = MODE_ROADS;
  if (System.GetCLIParam("acrSpawnSpotTest", flag)) mode = MODE_SPAWNSPOT;
  if (System.GetCLIParam("acrTagsTest", flag)) mode = MODE_TAGS;
  if (System.GetCLIParam("acrSharedTest", flag)) mode = MODE_SHARED;
  if (System.GetCLIParam("acrRegressTest", flag)) mode = MODE_REGRESS;
  if (System.GetCLIParam("acrMapPolygonTest", flag)) mode = MODE_MAPPOLYGON;
  if (System.GetCLIParam("acrMortarTest", flag)) mode = MODE_MORTAR;
  if (System.GetCLIParam("acrBatteryTest", flag)) mode = MODE_BATTERY;
  if (System.GetCLIParam("acrBlastTest", flag) || System.GetCLIParam("acrBlastOldTest", flag)) mode = MODE_BLAST;
  if (System.GetCLIParam("acrMotorizedTest", flag)) mode = MODE_MOTORIZED;
  if (System.GetCLIParam("acrFireTest", flag)) mode = MODE_FIRE;
  if (System.GetCLIParam("acrFactionTest", flag)) mode = MODE_FACTIONS;
  if (System.GetCLIParam("acrOrganTest", flag)) mode = MODE_ORGAN;
  if (System.GetCLIParam("acrFlankTest", flag)) mode = MODE_FLANK;
  if (System.GetCLIParam("acrAttackPlanTest", flag)) mode = MODE_ATTACKPLAN;
  if (System.GetCLIParam("acrDrivelessGunTest", flag)) mode = MODE_DRIVELESSGUN;
  if (System.GetCLIParam("acrMinefieldsTest", flag)) mode = MODE_MINEFIELDS;
  if (System.GetCLIParam("acrRoadblocksTest", flag)) mode = MODE_ROADBLOCKS;
  if (System.GetCLIParam("acrFortifyTest", flag)) mode = MODE_FORTIFY;
  if (System.GetCLIParam("acrFortifyFitTest", flag)) mode = MODE_FORTIFYFIT;
  if (System.GetCLIParam("acrDefendersTest", flag)) mode = MODE_DEFENDERS;
  if (System.GetCLIParam("acrBuildBudgetTest", flag)) mode = MODE_BUDGET;
  if (System.GetCLIParam("acrAIDefencesTest", flag)) mode = MODE_AIDEFENCES;
  if (System.GetCLIParam("acrRoadSitingTest", flag)) mode = MODE_ROADSITING;
  if (System.GetCLIParam("acrBuildRideTest", flag)) mode = MODE_BUILDRIDE;
  if (System.GetCLIParam("acrFobSitingTest", flag)) mode = MODE_FOBSITING;
  if (System.GetCLIParam("acrGunHoldTest", flag)) mode = MODE_GUNHOLD;
  if (System.GetCLIParam("acrPositionGunTest", flag)) mode = MODE_POSITIONGUN;
  if (System.GetCLIParam("acrRideJobTest", flag)) mode = MODE_RIDEJOB;
  if (System.GetCLIParam("acrVarietyTest", flag)) mode = MODE_VARIETY;
  if (System.GetCLIParam("acrMineSitingTest", flag)) mode = MODE_MINESITING;
  if (System.GetCLIParam("acrGroundTiltTest", flag)) mode = MODE_GROUNDTILT;
  if (System.GetCLIParam("acrPositionSitingTest", flag)) mode = MODE_POSSITING;
  if (System.GetCLIParam("acrAttackReachTest", flag)) mode = MODE_ATTACKREACH;
  if (System.GetCLIParam("acrGrabsTest", flag)) mode = MODE_GRABS;
  if (System.GetCLIParam("acrMortarSpotTest", flag)) mode = MODE_MORTARSPOT;
  if (System.GetCLIParam("acrRideLavTest", flag) || System.GetCLIParam("acrRideTruckTest", flag)) mode = MODE_RIDELAV;
  if (System.GetCLIParam("acrRideLavKillTest", flag) || System.GetCLIParam("acrRideLavGetOutTest", flag)) mode = MODE_RIDELAV;
  if (System.GetCLIParam("acrAimTest", flag)) mode = MODE_AIM;
  if (System.GetCLIParam("acrAirLiftContactTest", flag)) { mode = MODE_AIRLIFT; ACR_LiftHeli.s_iTestSeats = 2; }
  if (System.GetCLIParam("acrAirLiftTest", flag) || System.GetCLIParam("acrAirLiftBoardingTest", flag)) mode = MODE_AIRLIFT;
  if (System.GetCLIParam("acrAirLiftTwoTest", flag)) { mode = MODE_AIRLIFT; ACR_LiftHeli.s_iTestSeats = 2; }
  if (System.GetCLIParam("acrPadsTest", flag)) mode = MODE_PADS;
  if (System.GetCLIParam("acrHelipadTest", flag)) mode = MODE_HELIPAD;
  if (System.GetCLIParam("acrGarrisonTest", flag)) mode = MODE_GARRISON;
  if (System.GetCLIParam("acrGarrisonOfficeTest", flag)) { mode = MODE_GARRISON; ACR_TestGarrison.s_bOffice = true; }
  if (System.GetCLIParam("acrStructuresTest", flag)) mode = MODE_STRUCTURES;
  if (System.GetCLIParam("acrCrewTest", flag) || System.GetCLIParam("acrCrewOldTest", flag)) mode = MODE_CREW;
  if (System.GetCLIParam("acrGetOutTest", flag) || System.GetCLIParam("acrGetOutOldTest", flag)) mode = MODE_GETOUT;
  if (System.GetCLIParam("acrDriveAITest", flag) || System.GetCLIParam("acrDriveLockTest", flag) || System.GetCLIParam("acrDriveFixedTest", flag) || System.GetCLIParam("acrDriveAttachedTest", flag)) mode = MODE_DRIVE;
  if (!mode) return null;
  ACR_TestHarness harness = new ACR_TestHarness();
  harness.m_Range = range;
  harness.m_Mode = mode;
  PrintFormat("[ACR] Test harness mode=%1", mode);
  ACR_Perf.s_bChecks = true;
  // Garrison counts in the fixtures assume no FOB patrol; ACR_TestFobPatrol switches it on.
  ACR_BaseSystem.s_bNoFobPatrols = true;
  return harness;
 }

 ACR_Squad Squad()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !m_Range) return null;
  return manager.FindSquadByGroup(m_Range.m_Group);
 }

 // Dedicated-server runs have no local player: the tests look through Alpha's
 // eyes instead, and client-side checks report -1.
 IEntity Eye(SCR_PlayerController pc)
 {
  if (pc && pc.GetControlledEntity()) return pc.GetControlledEntity();
  ACR_Squad alpha = Squad();
  if (alpha) return alpha.GetAnchor();
  return null;
 }
 int ClientContacts(SCR_PlayerController pc)
 {
  int n = -1;
  if (pc) n = pc.ACR_ContactCount();
  return n;
 }
 int ClientFeed(SCR_PlayerController pc)
 {
  int n = -1;
  if (pc) n = pc.ACR_FeedCount();
  return n;
 }
 int ClientVehicles(SCR_PlayerController pc)
 {
  int n = -1;
  if (pc) n = pc.ACR_VehicleCount();
  return n;
 }

 vector Ground(string point) { return ACR_Manager.Ground(point.ToVector()); }

 void Tick()
 {
  m_Ticks++;
  // Lets the launcher script close the game as soon as the run is over.
  if ((m_Mode == MODE_AUTO && m_Ticks == 66) || (m_Mode == MODE_COMMAND && m_Ticks == 92) || (m_Mode == MODE_MAP_HANDOFF && m_Ticks == 66))
   Print("[ACR] TEST COMPLETE");
  if (m_Mode == MODE_VEHICLE && m_Ticks == 140) Print("[ACR] TEST COMPLETE");
  if (m_Mode == MODE_VEHICLE) { m_Vehicles.TestVehicle(); return; }
  if (m_Mode == MODE_BASES) { m_Battle.TestBases(); return; }
  if (m_Mode == MODE_ASSAULT) { m_Battle.TestAssault(); return; }
  if (m_Mode == MODE_REQUISITION) { m_Logistics.TestRequisition(); return; }
  if (m_Mode == MODE_GAMEMASTER) { m_Command.TestGameMaster(); return; }
  if (m_Mode == MODE_AI) { m_Battle.TestAI(); return; }
  if (m_Mode == MODE_GUNNER) { m_Vehicles.TestGunner(); return; }
  if (m_Mode == MODE_PROBE) { TestProbe(); return; }
  if (m_Mode == MODE_RESPAWN) { m_Command.TestRespawn(); return; }
  if (m_Mode == MODE_SUPPORT) { m_Battle.TestSupport(); return; }
  if (m_Mode == MODE_GUNSHIP) { m_Battle.TestGunship(); return; }
  if (m_Mode == MODE_TACTICS) { m_Battle.TestTactics(); return; }
  if (m_Mode == MODE_TRANSPORT) { m_Vehicles.TestTransport(); return; }
  if (m_Mode == MODE_STALL) { m_Vehicles.TestStall(); return; }
  if (m_Mode == MODE_STALLWAIT) { ACR_TestStallWait.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_RIDE) { m_Vehicles.TestRide(); return; }
  if (m_Mode == MODE_LOGISTICS) { m_Logistics.TestLogistics(); return; }
  if (m_Mode == MODE_QUEUE) { m_Battle.TestQueue(); return; }
  if (m_Mode == MODE_CLEANUP) { m_Logistics.TestCleanup(); return; }
  if (m_Mode == MODE_SIDE) { m_Command.TestSide(); return; }
  if (m_Mode == MODE_INTEL) { m_Battle.TestIntel(); return; }
  if (m_Mode == MODE_HUNT) { m_Battle.TestHunt(); return; }
  if (m_Mode == MODE_LAUNCHER) { m_Logistics.TestLauncher(); return; }
  if (m_Mode == MODE_SUPPLIES) { m_Logistics.TestSupplies(); return; }
  if (m_Mode == MODE_DRIVE) { m_Vehicles.TestDrive(); return; }
  if (m_Mode == MODE_SPAWNSPOT) { m_Logistics.TestSpawnSpot(); return; }
  if (m_Mode == MODE_TAGS) { ACR_TestShared.TestTags(this, m_Ticks); return; }
  if (m_Mode == MODE_SHARED) { ACR_TestShared.TestShared(this, m_Ticks); return; }
  if (m_Mode == MODE_REGRESS) { ACR_TestRegress.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_MAPPOLYGON) { ACR_TestMapPolygon.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_GETOUT) { ACR_TestGetOut.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_MORTAR) { ACR_TestMortar.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_CREW) { ACR_TestCrew.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_BATTERY) { ACR_TestBattery.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_AIRLIFT)
  {
   if (System.IsCLIParam("acrAirLiftBoardingTest")) ACR_TestLiftBoarding.Run(this, m_Ticks);
   else ACR_TestAirLift.Run(this, m_Ticks);
   return;
  }
  if (m_Mode == MODE_BLAST) { ACR_TestBlast.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_MOTORIZED) { ACR_TestMotorized.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_FIRE) { ACR_TestFire.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_FACTIONS) { ACR_TestFactionPairs.Run(m_Ticks); return; }
  if (m_Mode == MODE_ORGAN) { ACR_TestOrgan.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_FLANK) { ACR_TestFlank.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_ATTACKPLAN) { ACR_TestAttackPlan.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_DRIVELESSGUN) { ACR_TestDrivelessGun.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_MINEFIELDS) { ACR_TestMinefields.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_ROADBLOCKS) { ACR_TestRoadblocks.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_FORTIFY) { ACR_TestFortify.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_FORTIFYFIT) { ACR_TestFortifyFit.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_DEFENDERS) { ACR_TestDefenders.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_BUDGET) { ACR_TestBuildBudget.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_AIDEFENCES) { ACR_TestAIDefences.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_ROADSITING) { ACR_TestRoadSiting.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_BUILDRIDE) { ACR_TestBuildRide.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_FOBSITING) { ACR_TestFobSiting.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_GUNHOLD) { ACR_TestGunHold.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_POSITIONGUN) { ACR_TestPositionGun.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_RIDEJOB) { ACR_TestRideJob.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_VARIETY) { ACR_TestVariety.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_MINESITING) { ACR_TestMineSiting.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_GROUNDTILT) { ACR_TestGroundTilt.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_POSSITING) { ACR_TestPositionSiting.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_ATTACKREACH) { ACR_TestAttackReach.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_GRABS) { ACR_TestGrabs.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_MORTARSPOT) { ACR_TestMortarSpot.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_RIDELAV) { ACR_TestRideLav.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_AIM) { ACR_TestAim.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_PADS) { ACR_TestPads.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_HELIPAD) { ACR_TestHelipad.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_GARRISON) { ACR_TestGarrison.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_STRUCTURES) { ACR_TestStructures.Run(this, m_Ticks); return; }
  if (m_Mode == MODE_ROADS) { m_Vehicles.TestRoads(); return; }
  if (m_Mode == MODE_STALLED)
  {
   // The tactics scenario with attack time limits cut to seconds: attacks
   // that don't take their base must be called off, not hang forever.
   if (m_Ticks == 1)
   {
    ACR_AICommander.ASSAULT_TIMEOUT = 25;
    ACR_AICommander.ASSAULT_HARD_TIMEOUT = 50;
    ACR_AICommander.GRAB_TIMEOUT = 30;
    ACR_AICommander.s_bGrabByDistance = false;
    ACR_AICommander.STAGING_TIMEOUT = 20;
   }
   m_Battle.TestTactics();
   return;
  }
  if (m_Mode == MODE_MAP_HANDOFF) m_Command.TestMapHandoff();
  else if (m_Mode == MODE_COMMAND) m_Command.TestCommandResponse();
  else m_Command.TestAuto();
 }
 void PrintComplete() { Print("[ACR] TEST COMPLETE"); }

 // ---------------------------------------------------------------- -acrProbeTest: obstacle map (32 m cells)
 static const int PROBE_CELL = 32;
 static const int PROBE_CELLS = 128;
 int m_ProbeRow;
 int m_ProbeHits;

 bool ProbeCount(IEntity e)
 {
  if (!e || !e.GetPhysics()) return true;
  if (GenericTerrainEntity.Cast(e)) return true;
  m_ProbeHits++;
  return true;
 }

 void TestProbe()
 {
  if (m_Ticks < 5) return;
  BaseWorld world = GetGame().GetWorld();
  for (int n = 0; n < 16 && m_ProbeRow < PROBE_CELLS; n++)
  {
   string row = "";
   string heights = "";
   float z = m_ProbeRow * PROBE_CELL + PROBE_CELL * 0.5;
   for (int c = 0; c < PROBE_CELLS; c++)
   {
    float x = c * PROBE_CELL + PROBE_CELL * 0.5;
    float y = world.GetSurfaceY(x, z);
    heights += ((int)y).ToString() + ",";
    if (y < 1) { row += "~"; continue; }
    m_ProbeHits = 0;
    world.QueryEntitiesBySphere(Vector(x, y + 1, z), PROBE_CELL * 0.5, ProbeCount, null, EQueryEntitiesFlags.STATIC);
    if (m_ProbeHits == 0) row += ".";
    else if (m_ProbeHits > 9) row += "#";
    else row += m_ProbeHits.ToString();
   }
   PrintFormat("[ACR] PROBE %1 %2", m_ProbeRow, row);
   PrintFormat("[ACR] PROBEH %1 %2", m_ProbeRow, heights);
   m_ProbeRow++;
  }
  if (m_ProbeRow >= PROBE_CELLS) { Print("[ACR] TEST COMPLETE"); m_ProbeRow++; }
 }

 ChimeraCharacter AlphaMember(int index)
 {
  if (!m_Range.m_Group) return null;
  array<AIAgent> agents = {};
  m_Range.m_Group.GetAgents(agents);
  if (index >= agents.Count()) return null;
  return ChimeraCharacter.Cast(agents[index].GetControlledEntity());
 }

 void Kill(IEntity entity)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(entity);
  if (character && character.GetCharacterController()) character.GetCharacterController().ForceDeath();
 }

 int SupportIndex(ACR_Manager manager, ACR_ESupportType type)
 {
  foreach (int i, ACR_SupportEntry entry : manager.m_Bases.GetFaction("US").m_aSupports)
   if (entry.m_eType == type) return i;
  return -1;
 }

 int AliveIn(SCR_AIGroup group)
 {
  if (!group) return 0;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  int alive = 0;
  foreach (AIAgent agent : agents) if (ACR_Respawn.IsAlive(agent.GetControlledEntity())) alive++;
  return alive;
 }

 ResourceName FireTeamOf(ACR_Manager manager, string key)
 {
  foreach (ACR_RequisitionEntry entry : manager.m_Bases.GetFaction(key).m_aRequisitions)
   if (entry.m_sVehicle.IsEmpty() && (entry.m_sName.Contains("Fire Team") || entry.m_sName.Contains("Fire Group"))) return entry.m_sGroup;
  return "";
 }

 static bool s_bHeliVariant;

 // ---------------------------------------------------------------- -acrTransportTest
 // Alpha and Bravo board a crewed UH-1H. Alpha gets a queued LZ: nothing while
 // the helicopter hovers high, out once it sets down. Bravo is dropped with F6
 // from a 10 m hover (warning first, second press drops them).
 IEntity m_Huey;

 int Aboard(SCR_AIGroup group)
 {
  if (!group) return 0;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  int aboard;
  foreach (AIAgent agent : agents) if (ACR_Transport.VehicleOf(agent.GetControlledEntity()) == m_Huey) aboard++;
  return aboard;
 }

 ACR_Squad NamedSquad(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad.GetName() == name) return squad;
  return null;
 }
}
