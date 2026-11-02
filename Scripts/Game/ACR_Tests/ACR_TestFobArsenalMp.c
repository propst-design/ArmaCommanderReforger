// -acrFobArsenalMpTest 1 on a dedicated server AND on a client joined to it (audit #6, #156): does a
// joined client's FOB ammo dump arsenal see the FOB's supplies? Not a watcher preset: the server
// and the client are started by hand (see the PR). The server builds a FOB with an ammo dump,
// fills its store to 1000 and gives the first player a body beside the arsenal. The client logs
// what the arsenal's supply consumer shows while subscribed the way the inventory UI subscribes.
// Verdict lines: "[ACR] FOBMP client ..." in the client's console.log.
class ACR_TestFobArsenalMp
{
 static const ResourceName HQ = "{16AF2FE47A6C297B}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Headquarters_S_Conflict_US_01.et";
 static const ResourceName AMMO = "{1BED8F81D2FA2137}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_AmmoStorage_S_US_01.et";
 static const ResourceName US_RIFLEMAN = "{26A9756790131354}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Rifleman.et";
 static ACR_Base s_Fob;
 static IEntity s_Ammo;
 static bool s_bGiven;

 // ---------------------------------------------------------------- server

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
  if (tick == 3)
  {
   vector home = alpha.GetAnchor().GetOrigin();
   for (int i = 0; i < 24; i++)
   {
    float angle = i * 15 * Math.DEG2RAD;
    vector spot = ACR_Manager.Ground(home + Vector(Math.Cos(angle) * 450, 0, Math.Sin(angle) * 450));
    if (ACR_TestStructures.InsideABase(spot, 60) || BBB_Terrain.IsWaterAt(spot)) continue;
    ACR_TestStructures.Place(HQ, spot);
    break;
   }
  }
  if (tick == 8)
  {
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base.m_bBuilt && !base.m_bRetired && base.m_FobSupply) s_Fob = base;
   if (!s_Fob) { Print("[ACR] FOBMP server: no FOB was built"); return; }
   s_Ammo = ACR_TestStructures.Place(AMMO, s_Fob.GetOrigin() + Vector(-14, 0, 14));
  }
  if (tick == 12 && s_Fob)
  {
   SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(s_Fob.m_FobSupply);
   if (store) store.SetResourceValue(1000);
   SCR_ResourceConsumer arsenal = ACR_TestStructures.ArsenalConsumer(s_Ammo);
   if (arsenal) GetGame().GetResourceGrid().UpdateInteractor(arsenal);
   bool allowed = store && arsenal && store.IsAllowed(arsenal);
   float seen = -1;
   if (arsenal) seen = arsenal.GetAggregatedResourceValue();
   PrintFormat("[ACR] FOBMP server: FOB %1 at %2, store %3, arsenal %4, allowed %5, arsenal sees %6", s_Fob.m_sName, s_Fob.GetOrigin(), Stock(store), arsenal != null, allowed, seen);
  }
  if (tick > 12 && s_Fob && !s_bGiven) GiveBody();
  if (tick % 10 == 0 && s_Fob && s_Ammo)
  {
   SCR_ResourceConsumer arsenalNow = ACR_TestStructures.ArsenalConsumer(s_Ammo);
   float seenNow = -1;
   if (arsenalNow) seenNow = arsenalNow.GetAggregatedResourceValue();
   PrintFormat("[ACR] FOBMP server tick %1: arsenal sees %2", tick, seenNow);
  }
  if (tick == 240) Print("[ACR] TEST COMPLETE");
 }

 // The first player who joins gets a rifleman beside the arsenal, so the FOB streams to him.
 protected static void GiveBody()
 {
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  if (players.IsEmpty()) return;
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(players[0]));
  if (!pc) return;
  vector at = ACR_Manager.Ground(s_Fob.GetOrigin() + Vector(-8, 0, 8));
  IEntity body = ACR_Manager.SpawnPrefab(US_RIFLEMAN, at);
  if (!body) return;
  s_bGiven = true;
  if (pc.GetControlledEntity()) pc.SetPossessedEntity(body);
  else pc.SetInitialMainEntity(body);
  PrintFormat("[ACR] FOBMP server: player %1 given a body at %2 (had one: %3)", players[0], at, pc.GetControlledEntity() != null);
 }

 protected static int Stock(SCR_ResourceContainer store)
 {
  if (!store) return -1;
  return store.GetResourceValue();
 }

 // ---------------------------------------------------------------- client

 static ACR_FobProviderComponent s_Provider;
 static int s_iPolls;
 static ref SCR_ResourceSystemSubscriptionHandleBase s_Handle;
 static ref array<IEntity> s_aFound = {};

 static void StartClient(ACR_FobProviderComponent provider)
 {
  if (s_Provider) return;
  s_Provider = provider;
  Print("[ACR] FOBMP client: FOB provider streamed in");
  GetGame().GetCallqueue().CallLater(PollClient, 2000, true);
 }

 protected static bool Collect(IEntity entity)
 {
  if (ACR_TestStructures.ArsenalConsumer(entity) && !s_aFound.Contains(entity)) s_aFound.Insert(entity);
  return true;
 }

 static void PollClient()
 {
  s_iPolls++;
  if (!s_Provider || s_iPolls > 60)
  {
   GetGame().GetCallqueue().Remove(PollClient);
   Print("[ACR] FOBMP client: done");
   return;
  }
  IEntity provider = s_Provider.GetOwner();
  SCR_ResourceContainer store = ACR_FobSupply.CommandPostStore(s_Provider.CommandPost());
  s_aFound.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(provider.GetOrigin(), 40, Collect);
  SCR_ResourceConsumer arsenal;
  foreach (IEntity found : s_aFound)
  {
   arsenal = ACR_TestStructures.ArsenalConsumer(found);
   if (arsenal) break;
  }
  IEntity body = SCR_PlayerController.GetLocalControlledEntity();
  string where = "no body";
  if (body) where = string.Format("%1 m from the FOB", (int)vector.DistanceXZ(body.GetOrigin(), provider.GetOrigin()));
  if (!arsenal)
  {
   PrintFormat("[ACR] FOBMP client poll %1: store %2, no arsenal streamed yet (%3)", s_iPolls, Stock(store), where);
   return;
  }
  // Subscribe the way the inventory UI does when the arsenal is opened (SCR_InventoryStorageBaseUI).
  if (!s_Handle && GetGame().GetPlayerController())
  {
   SCR_ResourcePlayerControllerInventoryComponent inventory = SCR_ResourcePlayerControllerInventoryComponent.Cast(GetGame().GetPlayerController().FindComponent(SCR_ResourcePlayerControllerInventoryComponent));
   if (inventory)
   {
    RplId inventoryId = Replication.FindItemId(inventory);
    if (inventoryId.IsValid()) s_Handle = GetGame().GetResourceSystemSubscriptionManager().RequestSubscriptionListenerHandle(arsenal, inventoryId);
   }
  }
  bool allowed = store && store.IsAllowed(arsenal);
  bool serves = ACR_FobSupply.ServesFrom(provider, arsenal);
  float seen = arsenal.GetAggregatedResourceValue();
  // (No UpdateInteractor here: a client's arsenal consumer has no container queue, so the grid
  // throws; the value it shows is the server's, replicated while subscribed. 2 Nov: 1000 seen.)
  PrintFormat("[ACR] FOBMP client poll %1: store %2, arsenal sees %3 (containers %4), IsAllowed %5, ServesFrom %6, subscribed %7, %8",
   s_iPolls, Stock(store), seen, arsenal.GetContainerCount(), allowed, serves, s_Handle != null, where);
 }
}

modded class ACR_FobProviderComponent
{
 override void OnPostInit(IEntity owner)
 {
  super.OnPostInit(owner);
  if (GetGame().InPlayMode() && !Replication.IsServer() && System.IsCLIParam("acrFobArsenalMpTest")) ACR_TestFobArsenalMp.StartClient(this);
 }
}
