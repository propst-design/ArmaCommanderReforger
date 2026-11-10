// Builds ACA's static collections (s_All, caches, scratch arrays) when the game object is created.
// The engine runs every class-level static initializer ("static ref array<T> s_X = {};",
// "= new map<...>()") from ONE hidden function with an instruction cap shared by all loaded
// mods. Over the cap the "Game" module fails to compile ("Too many instructions per function"
// on random local consts), so ACA declares its statics bare and fills them here instead.
// tools/check_static_init.py keeps new initializers out (HANDBOOK Pitfalls, 2 Nov 2026).
class ACR_StaticInit
{
 protected static bool s_bDone;

 static void Run()
 {
  if (s_bDone) return;
  s_bDone = true;
  ACR_AICommander.ACR_InitStatics();
  ACR_AirLift.ACR_InitStatics();
  ACR_AttackGround.ACR_InitStatics();
  ACR_BBBHooks.ACR_InitStatics();
  ACR_BagHold.ACR_InitStatics();
  ACR_Base.ACR_InitStatics();
  ACR_BaseDefences.ACR_InitStatics();
  ACR_BaseLiner.ACR_InitStatics();
  ACR_BaseLines.ACR_InitStatics();
  ACR_BaseRing.ACR_InitStatics();
  ACR_BaseSurvey.ACR_InitStatics();
  ACR_BaseWorks.ACR_InitStatics();
  ACR_Containment.ACR_InitStatics();
  ACR_Evac.ACR_InitStatics();
  ACR_Airdrop.ACR_InitStatics();
  ACR_Intro.ACR_InitStatics();
  ACR_ContentSocket.ACR_InitStatics();
  ACR_Defenders.ACR_InitStatics();
  ACR_DoctrineKits.ACR_InitStatics();
  ACR_FactionCatalog.ACR_InitStatics();
  ACR_FactionSelection.ACR_InitStatics();
  ACR_FactionSettings.ACR_InitStatics();
  ACR_FIAGrowth.ACR_InitStatics();
  ACR_IED.ACR_InitStatics();
  ACR_Raid.ACR_InitStatics();
  ACR_FIAIntel.ACR_InitStatics();
  ACR_Depots.ACR_InitStatics();
  ACR_FireMissions.ACR_InitStatics();
  ACR_FobBuild.ACR_InitStatics();
  ACR_FobSiting.ACR_InitStatics();
  ACR_Fortify.ACR_InitStatics();
  ACR_Garrison.ACR_InitStatics();
  ACR_Generator.ACR_InitStatics();
  ACR_Guerrillas.ACR_InitStatics();
  ACR_GunHold.ACR_InitStatics();
  ACR_Helipad.ACR_InitStatics();
  ACR_Hud.ACR_InitStatics();
  ACR_Insurgency.ACR_InitStatics();
  ACR_IronSights.ACR_InitStatics();
  ACR_LandingZone.ACR_InitStatics();
  ACR_LocalFlyover.ACR_InitStatics();
  ACR_Logistics.ACR_InitStatics();
  ACR_MapFeed.ACR_InitStatics();
  ACR_MapLayers.ACR_InitStatics();
  ACR_Minefields.ACR_InitStatics();
  ACR_Mortars.ACR_InitStatics();
  ACR_NestDressing.ACR_InitStatics();
  ACR_Options.ACR_InitStatics();
  ACR_Outbreak.ACR_InitStatics();
  ACR_OrganEasterEgg.ACR_InitStatics();
  ACR_PanelMover.ACR_InitStatics();
  ACR_Perf.ACR_InitStatics();
  ACR_PingDisplay.ACR_InitStatics();
  ACR_QuickStart.ACR_InitStatics();
  ACR_Report.ACR_InitStatics();
  ACR_RoadblockLane.ACR_InitStatics();
  ACR_RoadblockSiting.ACR_InitStatics();
  ACR_Roadblocks.ACR_InitStatics();
  ACR_Setup.ACR_InitStatics();
  ACR_Skirmish.ACR_InitStatics();
  ACR_SpawnPoints.ACR_InitStatics();
  ACR_SquadAI.ACR_InitStatics();
  ACR_SquadMarksmen.ACR_InitStatics();
  ACR_StartingUnit.ACR_InitStatics();
  ACR_Structures.ACR_InitStatics();
  ACR_Supports.ACR_InitStatics();
  ACR_Think.ACR_InitStatics();
  ACR_Transport.ACR_InitStatics();
  ACR_TwoIC.ACR_InitStatics();
  ACR_TwoICRequests.ACR_InitStatics();
  ACR_TwoICSuggest.ACR_InitStatics();
  ACR_TwoICForecast.ACR_InitStatics();
  ACR_UALoadout.ACR_InitStatics();
  ACR_UiPrefs.ACR_InitStatics();
  ACR_Vehicles.ACR_InitStatics();
  ACR_Wings.ACR_InitStatics();
  SCR_HintUIComponent.ACR_InitStatics();
  ACR_TestHooks.Get().InitStatics();
 }
}
