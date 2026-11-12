// Paid construction supplies live in the CP's native storage. A replicated editor
// adapter exposes that single balance without permitting truck extraction.
class ACR_FobProviderComponentClass : SCR_CampaignBuildingProviderComponentClass {}
class ACR_FobProviderComponent : SCR_CampaignBuildingProviderComponent
{
 [RplProp(onRplName: "ACR_LinkStore")]
 protected RplId m_ACRCommandPost = RplId.Invalid();
 protected int m_iACRLinkRetryMs;   // current wait between tries to link the CP store (0: linked or not trying)

 IEntity CommandPost()
 {
  RplComponent component = RplComponent.Cast(Replication.FindItem(m_ACRCommandPost));
  if (component) return component.GetEntity();
  return null;
 }

 void BindCommandPost(IEntity commandPost)
 {
  RplComponent component = RplComponent.Cast(commandPost.FindComponent(RplComponent));
  if (!component) return;
  m_ACRCommandPost = component.Id();
  Replication.BumpMe();
  ACR_LinkStore();
 }

 void ACR_LinkStore()
 {
  SCR_ResourceContainer store = ACR_FobSupply.CommandPostStore(CommandPost());
  SCR_ResourceComponent resource = GetResourceComponent();
  SCR_ResourceConsumer consumer;
  if (resource) consumer = resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
  // One retry chain at a time (the replicated id, post-init and binding each start one), backing
  // off to one try every 10 s: a client that joins after the HQ is gone kept polling 4 times a second.
  GetGame().GetCallqueue().Remove(ACR_LinkStore);
  if (!store || !consumer)
  {
   m_iACRLinkRetryMs = Math.ClampInt(m_iACRLinkRetryMs * 2, 250, 10000);
   GetGame().GetCallqueue().CallLater(ACR_LinkStore, m_iACRLinkRetryMs, false);
   return;
  }
  m_iACRLinkRetryMs = 0;
  consumer.RegisterContainerForced(store);
 }

 override void OnPostInit(IEntity owner)
 {
  super.OnPostInit(owner);
  if (GetGame().InPlayMode() && !Replication.IsServer()) ACR_LinkStore();
 }

 void ~ACR_FobProviderComponent()
 {
  GetGame().GetCallqueue().Remove(ACR_LinkStore);
 }

 [RplProp()]
 protected bool m_bBlocked;

 void SetBlocked(bool blocked)
 {
  if (m_bBlocked == blocked) return;
  m_bBlocked = blocked;
  Replication.BumpMe();
 }

 // The FOB this provider belongs to, for clients (ACR_Base exists only on the server): vanilla
 // links a truck's "Unload supplies" and the FOB arsenals through SCR_ResourceContainer.IsAllowed
 // on the client too, so ServesFrom/TakesFrom must be decidable there (0.5.13 review #3).
 [RplProp()]
 protected vector m_vACRFobCenter;
 [RplProp()]
 protected float m_fACRFobRadius;
 [RplProp()]
 protected string m_sACRFobOwner;
 [RplProp()]
 protected bool m_bACRFobOnline;

 // Server: mirror the FOB's state (FobSupply.Bind, then every structures tick).
 void ACR_SyncFob(ACR_Base base)
 {
  if (!base) return;
  vector center = base.GetOrigin();
  bool online = base.m_bBuilt && !base.m_bRetired && !base.m_bHqLost;
  if (vector.DistanceXZ(center, m_vACRFobCenter) < 0.5 && m_fACRFobRadius == base.m_fRadius && m_sACRFobOwner == base.m_sOwner && m_bACRFobOnline == online) return;
  m_vACRFobCenter = center;
  m_fACRFobRadius = base.m_fRadius;
  m_sACRFobOwner = base.m_sOwner;
  m_bACRFobOnline = online;
  Replication.BumpMe();
 }

 // Online (built, HQ standing) with its centre, radius and owner, as last mirrored.
 bool ACR_FobState(out vector center, out float radius, out string owner)
 {
  center = m_vACRFobCenter;
  radius = m_fACRFobRadius;
  owner = m_sACRFobOwner;
  return m_bACRFobOnline && radius > 0;
 }

 override int GetBudgetValue(EEditableEntityBudget type, out SCR_CampaignBuildingProviderComponent componentToUse)
 {
  if (type != EEditableEntityBudget.CAMPAIGN) return super.GetBudgetValue(type, componentToUse);
  componentToUse = this;
  if (m_bBlocked) return 0;
  return ACR_FobSupply.Stock(GetOwner());
 }

 override bool IsThereEnoughBudgetToSpawn(notnull array<ref SCR_EntityBudgetValue> budgetCosts)
 {
  if (m_bBlocked) return false;
  return super.IsThereEnoughBudgetToSpawn(budgetCosts);
 }
}

class ACR_FobSupply
{
 static const ResourceName PROVIDER = "{AC230926A001007D}Prefabs/ACR/ACR_FobSupply.et";
 static const int CAPACITY = 1000; // matches the vanilla command post's bundled storage
 static const int DELIVERY_AMOUNT = 500;
 static const int DELIVERY_COST = 20;
 static const float DELIVERY_MS = 60000;

 // The CP's real native storage is the only balance. The map provider is an
 // editor adapter, not a second bank of supplies.
 static SCR_ResourceContainer CommandPostStore(IEntity entity)
 {
  if (!entity) return null;
  SCR_ResourceComponent resource = SCR_ResourceComponent.Cast(entity.FindComponent(SCR_ResourceComponent));
  SCR_ResourceContainer store;
  if (resource) store = resource.GetContainer(EResourceType.SUPPLIES);
  if (store && store.GetMaxResourceValue() > 0) return store;
  for (IEntity child = entity.GetChildren(); child; child = child.GetSibling())
  {
   SCR_ResourceContainer found = CommandPostStore(child);
   if (found) return found;
  }
  return null;
 }

 static IEntity EditorProvider(IEntity entity)
 {
  for (IEntity ancestor = entity; ancestor; ancestor = ancestor.GetParent())
  {
   SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(ancestor.FindComponent(SCR_CampaignBuildingCompositionComponent));
   if (composition && composition.ACR_FobProvider()) return composition.ACR_FobProvider();
  }
  return entity;
 }

 static int Stock(IEntity provider)
 {
  SCR_ResourceContainer container = ACR_Logistics.SupplyContainer(provider);
  if (!container) return 0;
  return Math.Floor(container.GetResourceValue());
 }

 static int CapacityOf(ACR_Base base)
 {
  if (!base) return 0;
  SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(base.m_FobSupply);
  if (!store) return 0;
  return store.GetMaxResourceValue();
 }

 // The CP store is the representative of the HQ's resource encapsulator (vanilla
 // SCR_ResourceEncapsulator): its value and max mirror the HQ's native child crates, and
 // SetResourceValue fills or drains those crates. Raising only the store's max made the depot
 // bonus a label (the crates still held 1000, and any crate change reset the max). The bonus goes
 // on the last crate instead: generation fills crates first to last and consumption drains last to
 // first, so that crate holds what is above the others' share and empties first when it shrinks.
 static void SetCapacity(SCR_ResourceContainer store, float capacity)
 {
  if (!store) return;
  SCR_ResourceEncapsulator encapsulator = store.GetResourceEncapsulator();
  SCR_ResourceContainerQueueBase queue;
  if (encapsulator) queue = encapsulator.GetContainerQueue();
  SCR_ResourceContainer last;
  if (queue && queue.GetContainerCount() > 0) last = queue.GetContainerAt(queue.GetContainerCount() - 1);
  if (!last)
  {
   if (store.GetMaxResourceValue() != capacity) store.SetMaxResourceValue(capacity);
   if (store.GetResourceValue() > capacity) store.SetResourceValue(capacity);
   return;
  }
  float keep = Math.Min(store.GetResourceValue(), capacity);
  float size = Math.Max(0, capacity - (encapsulator.GetAggregatedMaxResourceValue() - last.GetMaxResourceValue()));
  bool changed;
  // Shrinking: drain the excess first, through the encapsulator (it comes out of the last crate).
  if (store.GetResourceValue() > keep) changed = store.SetResourceValue(keep);
  if (Math.AbsFloat(last.GetMaxResourceValue() - size) >= 0.5)
  {
   last.SetMaxResourceValue(size);
   // SetMaxResourceValue doesn't clamp the crate; anything over its new size is put back below.
   if (last.GetResourceValue() > size) last.SetResourceValueUnsafe(size);
   if (last.GetComponent()) last.GetComponent().Replicate();
   changed = true;
  }
  if (store.GetResourceValue() < keep) store.SetResourceValue(keep);
  float total = encapsulator.GetAggregatedMaxResourceValue();
  if (Math.AbsFloat(store.GetMaxResourceValue() - total) >= 0.5) changed = store.SetMaxResourceValue(total);
  if (changed && store.GetComponent()) store.GetComponent().Replicate();
 }

 static ACR_Base Nearby(string faction, vector position)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.FobOnline() && base.m_sOwner == faction && base.m_FobSupply && vector.DistanceXZ(position, base.GetOrigin()) <= base.m_fRadius) return base;
  return null;
 }

 static void Create(ACR_Base base, IEntity commandPost)
 {
  if (!base || !base.m_bBuilt || base.m_FobSupply) return;
  base.m_FobSupply = ACR_Manager.SpawnPrefab(PROVIDER, base.GetOrigin());
  if (!base.m_FobSupply) return;
  SCR_FactionAffiliationComponent faction = SCR_FactionAffiliationComponent.Cast(base.m_FobSupply.FindComponent(SCR_FactionAffiliationComponent));
  if (faction) faction.SetAffiliatedFaction(base.GetOwnerFaction());
  Bind(base, commandPost);
 }

 // Link a (re)built command post to the FOB's existing provider and its stock.
 static void Bind(ACR_Base base, IEntity commandPost)
 {
  if (!base || !base.m_FobSupply || !commandPost) return;
  ACR_FobProviderComponent provider = ACR_FobProviderComponent.Cast(base.m_FobSupply.FindComponent(ACR_FobProviderComponent));
  SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(commandPost.FindComponent(SCR_CampaignBuildingCompositionComponent));
  if (composition) composition.ACR_SetFobProvider(base.m_FobSupply);
  // A newly erected CP cannot bootstrap supplies from its native prefab.
  SCR_ResourceContainer store = CommandPostStore(commandPost);
  if (store)
  {
   store.SetResourceValue(0);
   store.SetResourceGain(0);
   // Native consumers can have discovered the completed CP before our next
   // structures tick. Revoke those cached links when adopting its storage.
   foreach (SCR_ResourceInteractor interactor : store.GetLinkedInteractorsCopy())
    if (interactor && !SCR_ResourceEncapsulator.Cast(interactor)) interactor.UnregisterContainer(store);
  }
  if (provider) provider.BindCommandPost(commandPost);
  if (provider) provider.ACR_SyncFob(base);
 }

 static ACR_Base AtProvider(IEntity provider)
 {
  if (!provider) return null;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (!base.m_bRetired && base.m_bBuilt && base.m_FobSupply == provider) return base;
  return null;
 }

 // The provider's FOB: online (built, HQ standing), with its centre, radius and owner. The server
 // reads the base itself; a client reads the provider's mirror (ACR_FobProviderComponent.ACR_SyncFob).
 static bool FobAt(IEntity provider, out vector center, out float radius, out string owner)
 {
  if (!provider) return false;
  if (Replication.IsServer())
  {
   ACR_Base base = AtProvider(provider);
   if (!base || base.m_bHqLost) return false;
   center = base.GetOrigin();
   radius = base.m_fRadius;
   owner = base.m_sOwner;
   return true;
  }
  ACR_FobProviderComponent mirror = ACR_FobProviderComponent.Cast(provider.FindComponent(ACR_FobProviderComponent));
  return mirror && mirror.ACR_FobState(center, radius, owner);
 }

 // #156: the FOB's own arsenals and resupply points (the ammo dump's arsenal boxes) draw on the
 // CP store like vanilla base arsenals, so players can take gear there. Only a DEFAULT consumer
 // inside the FOB's ring, on the FOB's side and not on a vehicle: trucks load through
 // VEHICLE_LOAD, and an arsenal truck parked here keeps using its own load.
 static bool ServesFrom(IEntity provider, notnull SCR_ResourceInteractor interactor)
 {
  SCR_ResourceConsumer consumer = SCR_ResourceConsumer.Cast(interactor);
  if (!consumer || consumer.GetGeneratorIdentifier() != EResourceGeneratorID.DEFAULT) return false;
  IEntity owner = interactor.GetOwner();
  if (!owner) return false;
  for (IEntity part = owner; part; part = part.GetParent())
   if (Vehicle.Cast(part)) return false;
  vector center;
  float radius;
  string side;
  if (!FobAt(provider, center, radius, side) || vector.DistanceXZ(owner.GetOrigin(), center) > radius) return false;
  FactionAffiliationComponent affiliation = interactor.GetComponent().GetFactionAffiliationComponent();
  if (affiliation && affiliation.GetAffiliatedFaction() && affiliation.GetAffiliatedFaction().GetFactionKey() != side) return false;
  return true;
 }

 // #107 (Jacob, 7 Nov: buy a truck of supplies, in every mode): a truck driven into the FOB's ring
 // may unload into the store with vanilla's "Unload supplies" (the truck's VEHICLE_UNLOAD
 // generator). Unloading only ever adds to the FOB's owner, so whose truck it is doesn't matter;
 // taking supplies back out (VEHICLE_LOAD) stays forbidden. A contested FOB still takes a
 // delivery (it just can't spend it); a FOB whose HQ is gone takes nothing. Only the CP store
 // itself: a depot's own crates are native containers ACA never spends from, so a load
 // unloaded into them would be lost.
 static bool TakesFrom(IEntity provider, notnull SCR_ResourceContainer container, notnull SCR_ResourceInteractor interactor)
 {
  if (container != ACR_Logistics.SupplyContainer(provider)) return false;
  SCR_ResourceGenerator generator = SCR_ResourceGenerator.Cast(interactor);
  if (!generator || generator.GetIdentifier() != EResourceGeneratorID.VEHICLE_UNLOAD) return false;
  IEntity owner = interactor.GetOwner();
  if (!owner) return false;
  // The truck itself, not its cargo box (a part's position lags a moved truck by a frame).
  IEntity truck = owner.GetRootParent();
  if (!Vehicle.Cast(truck)) return false;
  vector center;
  float radius;
  string side;
  if (!FobAt(provider, center, radius, side)) return false;
  return vector.DistanceXZ(truck.GetOrigin(), center) <= radius;
 }

 // Any other container inside a built FOB (a supply depot's crates, which vanilla lets a truck
 // unload into) refuses a truck's load: ACA builds and serves only from the CP store, so it would
 // be lost there. Park beside the HQ instead (vanilla unload reaches 12 m).
 static bool Strands(notnull SCR_ResourceContainer container, notnull SCR_ResourceInteractor interactor)
 {
  SCR_ResourceGenerator generator = SCR_ResourceGenerator.Cast(interactor);
  if (!generator || generator.GetIdentifier() != EResourceGeneratorID.VEHICLE_UNLOAD) return false;
  IEntity owner = container.GetOwner();
  if (!owner || Vehicle.Cast(owner.GetRootParent())) return false;
  vector at = owner.GetOrigin();
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_bBuilt && !base.m_bRetired && base.m_FobSupply && vector.DistanceXZ(at, base.GetOrigin()) <= base.m_fRadius)
    return container != ACR_Logistics.SupplyContainer(base.m_FobSupply);
  return false;
 }

 static bool SpendBuild(IEntity provider, int cost)
 {
  if (!Replication.IsServer() || cost < 0) return false;
  ACR_Base base = AtProvider(provider);
  if (!base || base.m_bContested || base.m_bHqLost) return false;
  SCR_ResourceContainer container = ACR_Logistics.SupplyContainer(provider);
  if (!container || Stock(provider) < cost) return false;
  container.SetResourceValue(Stock(provider) - cost);
  return true;
 }

 // All decisions are repeated on the server; base names are only lookup keys.
 static string Request(SCR_PlayerController pc, string label, bool build)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!Replication.IsServer() || !manager || !pc) return "Server unavailable";
  if (!ACR_Setup.Started()) return "Wait for the match to start";
  if (!manager.IsCommander(pc.GetPlayerId())) return "Take command to manage FOB supplies";
  IEntity body = pc.GetControlledEntity();
  if (!body || ACR_Logistics.Health(body) <= 0) return "Wait until you have spawned";
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction) return "You have no faction";
  string key = faction.GetFactionKey();
  ACR_Base base = manager.m_Requisition.FindSpawnBase(key, label);
  if (!base || !base.m_bBuilt || base.m_bRetired || !base.m_FobSupply) return "Select one of your built FOBs";
  if (base.m_bHqLost) return "FOB HQ destroyed: rebuild it with an Establish FOB order";
  if (base.m_bContested) return "FOB is contested";
  if (!build)
  {
   int before = manager.m_Bases.GetRP(key);
   string result = Order(manager, key, base);
   if (manager.m_Bases.GetRP(key) < before) manager.LogSpend(pc, key, before, "construction supplies for " + base.m_sName);
   return result;
  }
  if (!body || ACR_Logistics.Health(body) <= 0 || vector.DistanceXZ(body.GetOrigin(), base.GetOrigin()) > base.m_fRadius) return "Move inside the FOB to build";
  SCR_CampaignBuildingManagerComponent building = SCR_CampaignBuildingManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_CampaignBuildingManagerComponent));
  if (!building) return "Construction unavailable";
  if (Stock(base.m_FobSupply) <= 0) return "Order construction supplies first, or unload a construction truck by the HQ";
  building.EnterEditorMode(pc.GetPlayerId(), base.m_FobSupply, true, true);
  return "FOB construction opened";
 }

 static string Order(ACR_Manager manager, string key, ACR_Base base)
 {
  if (!Replication.IsServer() || !manager || !base || base.m_bRetired || !ACR_Base.s_All.Contains(base) || !base.m_bBuilt || base.m_sOwner != key || !base.m_FobSupply) return "Select one of your built FOBs";
  if (base.m_bHqLost) return "FOB HQ destroyed: rebuild it with an Establish FOB order";
  if (base.m_bContested) return "FOB is contested";
  if (base.m_iSupplyDelivery > 0) return "A supply delivery is already on order";
  SCR_ResourceContainer container = ACR_Logistics.SupplyContainer(base.m_FobSupply);
  if (!container || container.GetMaxResourceValue() - Stock(base.m_FobSupply) < DELIVERY_AMOUNT) return "FOB needs room for 500 supplies";
  if (!manager.m_Bases.Spend(key, DELIVERY_COST)) return "Not enough RP (20 needed)";
  base.m_iSupplyDelivery = DELIVERY_AMOUNT;
  base.m_fSupplyDeliveryLeft = DELIVERY_MS;
  base.m_fSupplyDeliveryUpdated = GetGame().GetWorld().GetWorldTime();
  base.m_sSupplyDeliveryFaction = key;
  return "500 construction supplies ordered: 20 RP, 60 seconds; pauses while contested";
 }

 static void Tick(ACR_Base base)
 {
  if (!base) return;
  float now = GetGame().GetWorld().GetWorldTime();
  float elapsed = Math.Max(0, now - base.m_fSupplyDeliveryUpdated);
  base.m_fSupplyDeliveryUpdated = now;
  Advance(base, elapsed);
 }

 static void Advance(ACR_Base base, float elapsed)
 {
  if (!base || base.m_bRetired || !base.m_FobSupply) return;
  SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(base.m_FobSupply.FindComponent(SCR_FactionAffiliationComponent));
  if (affiliation && affiliation.GetAffiliatedFaction() != base.GetOwnerFaction())
   affiliation.SetAffiliatedFaction(base.GetOwnerFaction());
  if (base.m_iSupplyDelivery > 0 && base.m_sSupplyDeliveryFaction != base.m_sOwner)
  {
   base.m_iSupplyDelivery = 0;
   base.m_fSupplyDeliveryLeft = 0;
  }
  ACR_FobProviderComponent provider = ACR_FobProviderComponent.Cast(base.m_FobSupply.FindComponent(ACR_FobProviderComponent));
  if (provider) provider.SetBlocked(base.m_bContested || base.m_bHqLost);
  if (provider) provider.ACR_SyncFob(base);
  if (base.m_iSupplyDelivery <= 0 || base.m_bContested || base.m_bHqLost) return;
  base.m_fSupplyDeliveryLeft = Math.Max(0, base.m_fSupplyDeliveryLeft - Math.Max(0, elapsed));
  if (base.m_fSupplyDeliveryLeft > 0) return;
  SCR_ResourceContainer container = ACR_Logistics.SupplyContainer(base.m_FobSupply);
  if (!container) return;
  // Capacity can shrink while a delivery is in transit. Hold the paid batch until
  // room is made or a depot is rebuilt rather than silently destroying the delivery.
  if (container.GetMaxResourceValue() - Stock(base.m_FobSupply) < base.m_iSupplyDelivery) return;
  container.SetResourceValue(Math.Min(container.GetMaxResourceValue(), Stock(base.m_FobSupply) + base.m_iSupplyDelivery));
  base.m_iSupplyDelivery = 0;
  PrintFormat("[ACR] FOB %1 received 500 paid construction supplies", base.m_sName);
 }

 static string Describe(ACR_Base base)
 {
  string detail = string.Format("\nFOB supplies: %1 / %2", Stock(base.m_FobSupply), CapacityOf(base));
  if (base.m_bHqLost) detail += "\nHQ DESTROYED: offline until an Establish FOB order rebuilds it";
  else if (Stock(base.m_FobSupply) <= 0) detail += "\nOrder supplies, or park a construction truck by the HQ and unload it";
  if (base.m_iSupplyDelivery > 0)
  {
   string state = "";
   if (base.m_bContested) state = " (paused: contested)";
   else if (base.m_fSupplyDeliveryLeft <= 0) state = " (waiting for storage space)";
   detail += string.Format("\nDelivery: %1 supplies, %2 s%3", base.m_iSupplyDelivery, Math.Ceil(base.m_fSupplyDeliveryLeft / 1000), state);
  }
  return detail;
 }
}

// Both the physical CP interaction and the map action enter the same paid editor.
modded class SCR_CampaignBuildingManagerComponent
{
 override void EnterEditorMode(int playerID, notnull IEntity provider, bool userActionActivationOnly = false, bool userActionUsed = false, bool useAllAvailableProviders = false)
 {
  IEntity actual = ACR_FobSupply.EditorProvider(provider);
  super.EnterEditorMode(playerID, actual, userActionActivationOnly, userActionUsed, useAllAvailableProviders);
 }
}

// CP stock remains native/replicated and visible, but cannot be extracted by
// trucks or vanilla generators. Its paid construction adapter and the FOB's own
// arsenals (ACR_FobSupply.ServesFrom) may connect, and trucks may unload into it
// (ACR_FobSupply.TakesFrom), never into a depot's crates (Strands); ACR services debit the
// same container explicitly.
modded class SCR_ResourceContainer
{
 override bool IsAllowed(notnull SCR_ResourceInteractor interactor)
 {
  IEntity provider = ACR_FobSupply.EditorProvider(GetOwner());
  if (provider != GetOwner() && provider && provider.FindComponent(ACR_FobProviderComponent))
  {
   if (interactor.GetOwner() == provider) return SCR_ResourceConsumer.Cast(interactor) != null;
   return (ACR_FobSupply.ServesFrom(provider, interactor) || ACR_FobSupply.TakesFrom(provider, this, interactor)) && super.IsAllowed(interactor);
  }
  if (ACR_FobSupply.Strands(this, interactor)) return false;
  return super.IsAllowed(interactor);
 }
}

// The physical engineering table's supply counter should show the paid store
// even though vanilla's military-base consumer cannot find a campaign base here.
modded class SCR_CampaignBuildingProviderComponent
{
 override SCR_ResourceComponent GetResourceComponent()
 {
  IEntity actual = ACR_FobSupply.EditorProvider(GetOwner());
  if (actual != GetOwner()) return SCR_ResourceComponent.Cast(actual.FindComponent(SCR_ResourceComponent));
  return super.GetResourceComponent();
 }
}

modded class SCR_CampaignBuildingStartUserAction
{
 override bool GetActionNameScript(out string outName)
 {
  IEntity actual = ACR_FobSupply.EditorProvider(GetOwner());
  if (actual != GetOwner())
  {
   SCR_ResourceComponent resource = SCR_ResourceComponent.Cast(actual.FindComponent(SCR_ResourceComponent));
   if (resource != m_ResourceComponent)
   {
    m_ResourceSubscriptionHandleConsumer = null;
    m_ResourceConsumer = null;
    m_ResourceComponent = resource;
   }
  }
  return super.GetActionNameScript(outName);
 }
}
