// Off-map supports the commander buys with RP and calls on the map (server side).
//  - Mortar barrages (HE, creeping, smoke) and illumination are fired by a real mortar
//    team (ACR_Mortars). The Game Master barrage modules below (spawned at the target,
//    they fire and clean up) are kept for the coming howitzer battery.
//  - Rocket strike and the default bomb run are "virtual aircraft": rockets launched
//    from a moving point in the sky / large explosions along a line. Bomb runs can
//    optionally add a flyover prefab or hand a configured prefab to the editable-entity
//    placement hook used by RHS and other scripted call-ins.
//  - Helicopter rocket run / gunship loiter use a real crewed gunship that the script
//    flies along a path (the AI cannot fly). Its door gunners fight on their own; it
//    fires rocket salvos itself. It can be shot down. The door-gunner support is the
//    transport version (no rockets) orbiting longer for less.
//  - Attack helicopter: only with a content pack that names one for the side
//    (ACR_ContentSocket.AttackHelicopter): its airframe on a longer loiter with more rockets.
enum ACR_ESupportType
{
 MORTAR_HE_SMALL,
 MORTAR_HE_LARGE,
 MORTAR_CREEPING,
 MORTAR_SMOKE,
 ILLUMINATION,
 ROCKET_STRIKE,
 BOMB_RUN,
 HELI_GUN_RUN,   // "rocket run" (the enum name stays: saved settings use it)
 HELI_LOITER,
 HELI_DOOR_GUNS, // transport helicopter, door gunners only, orbiting
 ARTILLERY_BATTERY, // battalion howitzers: no range limit, three heavy salvos over a wide area
 AIR_LIFT,         // off-map transport helicopter flies a squad to a landing zone (ACR_AirLift)
 ATTACK_HELI,      // a content pack's attack helicopter (ACR_ContentSocket.AttackHelicopter) on station
 RECON_FLIGHT      // off-map jet photographs a corridor: sightings into the side's intel (ACR_Recon)
}

enum ACR_EBombRunDelivery
{
	VIRTUAL,
	FLYOVER,
	EDITABLE_CALL_IN
}

[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class ACR_SupportEntry
{
 [Attribute("0", UIWidgets.ComboBox, "Kind of support", "", ParamEnumArray.FromEnum(ACR_ESupportType))]
 ACR_ESupportType m_eType;
 [Attribute("1", UIWidgets.CheckBox, "Offer this call-in")]
 bool m_bEnabled = true;
 [Attribute("", UIWidgets.EditBox, "Name in the command panel (empty = default)")]
 string m_sName;
 [Attribute("-1", UIWidgets.EditBox, "Cost in RP (-1 = default)")]
 int m_iCost;
 [Attribute("-1", UIWidgets.EditBox, "Seconds before it can be called again (-1 = default)")]
 int m_iCooldown;
 [Attribute("-1", UIWidgets.EditBox, "Air lift only: RP when the AI commander calls it (-1 = half the cost)")]
 int m_iAICost;
	[Attribute("0", UIWidgets.ComboBox, "Bomb run delivery: virtual blasts, flyover prefab plus virtual blasts, or an editable call-in prefab", "", ParamEnumArray.FromEnum(ACR_EBombRunDelivery))]
	ACR_EBombRunDelivery m_eBombRunDelivery;
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Bomb run aircraft/flyover prefab or editable call-in spawner prefab", "et")]
	ResourceName m_sBombRunPrefab;
	[Attribute("300", UIWidgets.EditBox, "Flyover altitude above the target in metres (at least 150 m over any hill on the way)")]
	float m_fBombRunAltitude = 300;
	[Attribute("180", UIWidgets.EditBox, "Flyover speed in metres per second")]
	float m_fBombRunSpeed = 180;
	[Attribute("4", UIWidgets.EditBox, "Number of virtual bombs in a flyover or virtual run")]
	int m_iBombRunBombs = 4;
	[Attribute("28", UIWidgets.EditBox, "Distance between virtual bomb impacts in metres")]
	float m_fBombRunSpacing = 28;
	[Attribute("5", UIWidgets.EditBox, "Random lateral scatter for virtual bomb impacts in metres")]
	float m_fBombRunScatter = 5;
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Recon flight: aircraft prefab flown over the corridor, with its own engine sound (empty = ACA's default for the side)", "et")]
	ResourceName m_sReconPrefab;

 // runtime
 float m_fReadyAt;

 void Resolve()
 {
  if (m_sName.IsEmpty()) m_sName = ACR_Supports.DefaultName(m_eType);
  if (m_iCost < 0) m_iCost = ACR_Supports.DefaultCost(m_eType);
  if (m_iCooldown < 0) m_iCooldown = ACR_Supports.DefaultCooldown(m_eType);
  // The AI commander can't fly bought helicopters (no pads of its own on generated maps),
  // so its air lifts cost half (Jacob, 14 Oct).
  if (m_iAICost < 0) m_iAICost = m_iCost / 2;
 }

 // What this call costs the caller.
 int CostFor(bool byAI)
 {
  if (byAI && m_eType == ACR_ESupportType.AIR_LIFT) return m_iAICost;
  return m_iCost;
 }

 // A faction's own copy of a configured entry (R15): cooldowns are per copy, never shared
 // with the settings entity or with another faction made from the same template.
 ACR_SupportEntry Copy()
 {
  ACR_SupportEntry copy = new ACR_SupportEntry();
  copy.m_eType = m_eType;
  copy.m_bEnabled = m_bEnabled;
  copy.m_sName = m_sName;
  copy.m_iCost = m_iCost;
  copy.m_iCooldown = m_iCooldown;
  copy.m_iAICost = m_iAICost;
  copy.m_eBombRunDelivery = m_eBombRunDelivery;
  copy.m_sBombRunPrefab = m_sBombRunPrefab;
  copy.m_fBombRunAltitude = m_fBombRunAltitude;
  copy.m_fBombRunSpeed = m_fBombRunSpeed;
  copy.m_iBombRunBombs = m_iBombRunBombs;
  copy.m_fBombRunSpacing = m_fBombRunSpacing;
  copy.m_fBombRunScatter = m_fBombRunScatter;
  copy.m_sReconPrefab = m_sReconPrefab;
  copy.m_fReadyAt = 0;
  copy.Resolve();
  return copy;
 }
}

// A support's danger area, shown on the map to that side until it is over.
class ACR_SupportZone : Managed
{
 string m_sFaction;
 vector m_Pos;
 float m_fRadius;
 string m_sName;
 float m_fImpactAt;
 float m_fEndAt;
}

class ACR_PendingLaunch : Managed
{
 IEntity m_Entity;
 vector m_Dir;
 vector m_Velocity;
 int m_iFrame;
}

class ACR_Supports : Managed
{
 static const ResourceName MORTAR_SMALL = "{5D48E2F7DB0C3714}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Small.et";
 static const ResourceName MORTAR_LARGE = "{1DDF7187608968F2}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Large.et";
 static const ResourceName MORTAR_CREEPING = "{F5FAFFDBC271088C}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Creeping.et";
 static const ResourceName MORTAR_SMOKE = "{2FDF3089DF6EC422}PrefabsEditable/EffectsModules/Mortar/Smoke/EffectModule_Zoned_MortarBarrage_Smoke_Medium.et";
 static const ResourceName FLARE = "{AA7F04F403FD2E00}PrefabsEditable/EffectsModules/Flares/EffectModule_Mortar_Flare_Illumination.et";
 static const ResourceName BOMB = "{72BEEF40AF179763}Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Large.et";
 // Howitzer battery (virtual: no guns on the map, no range limit).
 static int BATTERY_DELAY_MIN = 75;
 static int BATTERY_DELAY_MAX = 120;
 static const int BATTERY_SALVOS = 3;
 static const int BATTERY_ROUNDS = 6;
 static const int BATTERY_SALVO_SECONDS = 10;
 static const float BATTERY_RADIUS = 120;
 static const float BATTERY_DROP_HEIGHT = 700;  // the shell falls from here (~3 s): its travel sound is the incoming whistle
 static const int BATTERY_FALL_SECONDS = 3;
 static int s_iBatteryImpacts;                  // tests
 static ref array<vector> s_aBatteryImpacts;
 // Per match (ACR_Manager.Stop): the impact log grew for as long as the server ran.
 static void Reset()
 {
  s_iBatteryImpacts = 0;
  s_aBatteryImpacts.Clear();
 }
 // The match ends (ACR_Manager.Stop, before the manager goes): battery rounds, bomb runs, modules and
 // cook-offs still in the call queue never land in the next match.
 void CancelPending()
 {
  if (!GetGame()) return;
  ScriptCallQueue queue = GetGame().GetCallqueue();
  queue.Remove(FireBatteryRound);
  queue.Remove(WarnIncomingBattery);
  queue.Remove(SpawnBombRunCallIn);
  queue.Remove(Explode);
  queue.Remove(Secondary);
 }
 protected ref array<IEntity> m_BatteryShells = {};
 protected ref array<vector> m_BatteryLast = {};
 protected ref array<vector> m_BatteryAim = {};
 protected ref array<float> m_BatteryDeadline = {};
 static const ResourceName HELI_WEST = "{21E9A875C0A3C409}Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed_gunship_HE.et";
 static const ResourceName HELI_EAST = "{3C6B3ED0C3AC30D5}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_armed_gunship_HE.et";
 static const ResourceName HELI_TRANSPORT_WEST = "{DDDD9B51F1234DF3}Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed.et";
 static const ResourceName HELI_TRANSPORT_EAST = "{7BD282AF716ED639}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_armed.et";
 static const ResourceName ROCKET_WEST = "{072A755D5CB85D47}Prefabs/Weapons/Ammo/Ammo_Rocket_Hydra70_HE_M229.et";
 static const ResourceName ROCKET_EAST = "{EF17BED6DCEE4DE4}Prefabs/Weapons/Ammo/Ammo_Rocket_S5_FRAG_S5MO.et";

 ACR_Manager m_Manager;
 ref array<IEntity> m_Vehicles = {};
 protected ref ACR_HeliStation m_Station;
 // Helipads and the helicopters stationed on them (ACR_Helipads.c).
 ACR_HeliStation Station()
 {
  if (!m_Station) m_Station = new ACR_HeliStation(this);
  return m_Station;
 }
 protected ref array<ref ACR_SupportRun> m_Runs = {};
 void AddRun(ACR_SupportRun run) { m_Runs.Insert(run); }

 void Helicopters(array<ACR_HeliRun> result)
 {
  result.Clear();
  foreach (ACR_SupportRun run : m_Runs)
  {
   ACR_HeliRun heli = ACR_HeliRun.Cast(run);
   if (heli) result.Insert(heli);
   ACR_AirLift lift = ACR_AirLift.Cast(run);
   if (lift)
    foreach (ACR_LiftHeli carrier : lift.m_Helis)
     if (carrier.m_Vehicle && !carrier.m_bDestroyed && !carrier.m_bAway) result.Insert(carrier);
  }
 }

 // Test hook: the running helicopter support of a side (null if none).
 ACR_HeliRun TestHeliRun(string faction)
 {
  foreach (ACR_SupportRun run : m_Runs)
  {
   ACR_HeliRun heli = ACR_HeliRun.Cast(run);
   if (heli && heli.m_sFaction == faction) return heli;
  }
  return null;
 }
 protected ref array<ref ACR_PendingLaunch> m_Launches = {};
 protected int m_iFrame;
 ref array<ref ACR_SupportZone> m_Zones = {};

 void AddZone(string factionKey, vector pos, float radius, string name, int delaySeconds, int durationSeconds)
 {
  ACR_SupportZone zone = new ACR_SupportZone();
  float now = GetGame().GetWorld().GetWorldTime();
  zone.m_sFaction = factionKey;
  zone.m_Pos = pos;
  zone.m_fRadius = radius;
  zone.m_sName = name;
  zone.m_fImpactAt = now + delaySeconds * 1000;
  zone.m_fEndAt = zone.m_fImpactAt + durationSeconds * 1000;
  m_Zones.Insert(zone);
 }

 static float ZoneRadius(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return 30;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 60;
   case ACR_ESupportType.MORTAR_CREEPING: return 80;
   case ACR_ESupportType.MORTAR_SMOKE: return 40;
   case ACR_ESupportType.ILLUMINATION: return 90;
   case ACR_ESupportType.ROCKET_STRIKE: return 70;
   case ACR_ESupportType.BOMB_RUN: return 70;
   case ACR_ESupportType.HELI_GUN_RUN: return 40;
   case ACR_ESupportType.HELI_LOITER: return ACR_HeliRun.LOITER_RADIUS;
   case ACR_ESupportType.HELI_DOOR_GUNS: return ACR_HeliRun.LOITER_RADIUS;
   case ACR_ESupportType.ATTACK_HELI: return ACR_HeliRun.LOITER_RADIUS;
   case ACR_ESupportType.ARTILLERY_BATTERY: return BATTERY_RADIUS + 30;
  }
  return 50;
 }
 static int ZoneDuration(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.ILLUMINATION: return 80;
   case ACR_ESupportType.MORTAR_SMOKE: return 60;
   case ACR_ESupportType.HELI_LOITER: return 100;
   case ACR_ESupportType.HELI_DOOR_GUNS: return 130;
   case ACR_ESupportType.ATTACK_HELI: return 130;
   case ACR_ESupportType.MORTAR_CREEPING: return 50;
   case ACR_ESupportType.ARTILLERY_BATTERY: return BATTERY_SALVOS * BATTERY_SALVO_SECONDS + 10;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 40;
   case ACR_ESupportType.MORTAR_HE_SMALL: return 25;
  }
  return 15;
 }

 void ACR_Supports(ACR_Manager manager) { m_Manager = manager; }

 static string DefaultName(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return "Mortars: HE (small)";
   case ACR_ESupportType.MORTAR_HE_LARGE: return "Mortars: HE (large)";
   case ACR_ESupportType.MORTAR_CREEPING: return "Mortars: creeping barrage";
   case ACR_ESupportType.MORTAR_SMOKE: return "Mortars: smoke screen";
   case ACR_ESupportType.ILLUMINATION: return "Illumination flares";
   case ACR_ESupportType.ROCKET_STRIKE: return "Air: rocket strike";
   case ACR_ESupportType.BOMB_RUN: return "Air: bomb run";
   case ACR_ESupportType.HELI_GUN_RUN: return "Helicopter: rocket run";
   case ACR_ESupportType.HELI_LOITER: return "Helicopter: gunship (90 s)";
   case ACR_ESupportType.HELI_DOOR_GUNS: return "Helicopter: door gunners (120 s)";
   case ACR_ESupportType.ARTILLERY_BATTERY: return "Artillery: howitzer battery";
   case ACR_ESupportType.AIR_LIFT: return "Helicopter: air lift";
   case ACR_ESupportType.ATTACK_HELI: return "Helicopter: attack helicopter (120 s)";
   case ACR_ESupportType.RECON_FLIGHT: return "Air: recon flight";
  }
  return "Support";
 }
 static int DefaultCost(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return 10;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 25;
   case ACR_ESupportType.MORTAR_CREEPING: return 25;
   case ACR_ESupportType.MORTAR_SMOKE: return 6;
   case ACR_ESupportType.ILLUMINATION: return 4;
   case ACR_ESupportType.ROCKET_STRIKE: return 25;
   case ACR_ESupportType.BOMB_RUN: return 35;
   case ACR_ESupportType.HELI_GUN_RUN: return 30;
   case ACR_ESupportType.HELI_LOITER: return 45;
   case ACR_ESupportType.HELI_DOOR_GUNS: return 20;
   case ACR_ESupportType.ATTACK_HELI: return 55;
   case ACR_ESupportType.ARTILLERY_BATTERY: return 60;
   case ACR_ESupportType.AIR_LIFT: return 40;
   case ACR_ESupportType.RECON_FLIGHT: return 15;
  }
  return 20;
 }
 static int DefaultCooldown(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return 90;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 180;
   case ACR_ESupportType.MORTAR_CREEPING: return 180;
   case ACR_ESupportType.MORTAR_SMOKE: return 60;
   case ACR_ESupportType.ILLUMINATION: return 45;
   case ACR_ESupportType.ROCKET_STRIKE: return 180;
   case ACR_ESupportType.BOMB_RUN: return 240;
   case ACR_ESupportType.HELI_GUN_RUN: return 180;
   case ACR_ESupportType.HELI_LOITER: return 300;
   case ACR_ESupportType.HELI_DOOR_GUNS: return 150;
   case ACR_ESupportType.ATTACK_HELI: return 300;
   case ACR_ESupportType.ARTILLERY_BATTERY: return 300;
   case ACR_ESupportType.AIR_LIFT: return 240;
   case ACR_ESupportType.RECON_FLIGHT: return 180;
  }
  return 120;
 }
 // Seconds from the call until rounds land / aircraft arrive (helicopters: flight time).
 // Mortar missions take a realistic, somewhat random time: the call goes up the
 // chain, the section lays the tubes, then the rounds' time of flight.
 static int DefaultDelay(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return Math.RandomIntInclusive(45, 90);
   case ACR_ESupportType.MORTAR_HE_LARGE: return Math.RandomIntInclusive(50, 95);
   case ACR_ESupportType.MORTAR_CREEPING: return Math.RandomIntInclusive(60, 110);
   case ACR_ESupportType.MORTAR_SMOKE: return Math.RandomIntInclusive(40, 75);
   case ACR_ESupportType.ILLUMINATION: return Math.RandomIntInclusive(30, 60);
   case ACR_ESupportType.ARTILLERY_BATTERY: return Math.RandomIntInclusive(BATTERY_DELAY_MIN, BATTERY_DELAY_MAX);
   case ACR_ESupportType.ROCKET_STRIKE: return 15;
   case ACR_ESupportType.BOMB_RUN: return 20;
  }
  return 0;
 }

 // Every support type, used when a faction's settings list none.
 static void BuildDefaults(array<ref ACR_SupportEntry> entries, string factionKey)
 {
  // (ATTACK_HELI only comes with a content pack: FitAttackHelicopter.)
  for (int type = ACR_ESupportType.MORTAR_HE_SMALL; type <= ACR_ESupportType.AIR_LIFT; type++)
  {
   ACR_SupportEntry entry = new ACR_SupportEntry();
   entry.m_eType = type;
   entry.m_iCost = -1;
   entry.m_iAICost = -1;
   entry.m_iCooldown = -1;
   entry.Resolve();
   // With RHS loaded, both sides' bomb run is an audible FA-18 flyover (Jacob, 9 Oct);
   // without it, the side's own 1989 jet flies over the blasts (A-10A / Su-25), only with
   // AC: Actual's period jets on (-acrPeriodJets 1, ACR_Aircraft): off, VIRTUAL as before them.
   if (type == ACR_ESupportType.BOMB_RUN)
   {
    ResourceName jet = BOMB_JET_EAST;
    if (IsWestern(factionKey)) jet = BOMB_JET_WEST;
    if (!ACR_Aircraft.PeriodJetsOn()) jet = "";
    if (HasPrefab(FA18_FLYOVER)) jet = FA18_FLYOVER;
    if (!jet.IsEmpty() && HasPrefab(jet))
    {
     entry.m_eBombRunDelivery = ACR_EBombRunDelivery.FLYOVER;
     entry.m_sBombRunPrefab = jet;
    }
   }
   entries.Insert(entry);
  }
  entries.Insert(NewEntry(ACR_ESupportType.RECON_FLIGHT));
 }

 // A default row of one type.
 static ACR_SupportEntry NewEntry(ACR_ESupportType type)
 {
  ACR_SupportEntry entry = new ACR_SupportEntry();
  entry.m_eType = type;
  entry.m_iCost = -1;
  entry.m_iAICost = -1;
  entry.m_iCooldown = -1;
  entry.Resolve();
  return entry;
 }

 // The side's attack helicopter from a content pack (ACR_ContentSocket.AttackHelicopter), or null:
 // none named, or its prefab doesn't load.
 static ACR_SocketAttackHeli AttackHelicopterFor(string factionKey)
 {
  ACR_SocketAttackHeli heli = ACR_ContentSocket.Get().AttackHelicopter(factionKey);
  if (!heli || heli.m_sPrefab.IsEmpty() || !HasPrefab(heli.m_sPrefab)) return null;
  if (heli.m_sName.IsEmpty()) heli.m_sName = "attack helicopter";
  return heli;
 }

 // A side's support list and the pack's attack helicopter: with one, the list gets its support
 // (unless a configured row for it, on or off, decides: listed) named after the airframe; without
 // one, configured rows for it are dropped (nothing could fly them).
 static void FitAttackHelicopter(string factionKey, array<ref ACR_SupportEntry> entries, bool listed)
 {
  ACR_SocketAttackHeli heli = AttackHelicopterFor(factionKey);
  if (!heli)
  {
   for (int i = entries.Count() - 1; i >= 0; i--)
    if (entries[i].m_eType == ACR_ESupportType.ATTACK_HELI) entries.RemoveOrdered(i);
   return;
  }
  if (!listed)
  {
   ACR_SupportEntry added = new ACR_SupportEntry();
   added.m_eType = ACR_ESupportType.ATTACK_HELI;
   added.m_iCost = heli.m_iCost;
   added.m_iAICost = -1;
   added.m_iCooldown = -1;
   added.Resolve();
   entries.Insert(added);
  }
  foreach (ACR_SupportEntry entry : entries)
  {
   if (entry.m_eType != ACR_ESupportType.ATTACK_HELI || entry.m_sName != DefaultName(ACR_ESupportType.ATTACK_HELI)) continue;
   entry.m_sName = string.Format("Helicopter: %1 (120 s)", heli.m_sName);
  }
  PrintFormat("[ACR] SUPPORT %1 attack helicopter from the content pack: %2 (%3)", factionKey, heli.m_sName, heli.m_sPrefab);
 }

 static const ResourceName FA18_FLYOVER = "{76571A125EF9ECD7}Prefabs/Vehicles/Airplanes/FA18A/FA18A_Mesh.et";
 // ACA's own bomb-run jets (blockout models, tools/blender/build_aircraft.py).
 static const ResourceName BOMB_JET_WEST = "{AC230926A00100E0}Prefabs/Vehicles/Aircraft/ACR_A10A.et";
 static const ResourceName BOMB_JET_EAST = "{AC230926A00100E1}Prefabs/Vehicles/Aircraft/ACR_Su25.et";

 static bool HasPrefab(ResourceName prefab)
 {
  Resource resource = Resource.Load(prefab);
  return resource && resource.IsValid();
 }

 // ("USSR" contains "US": the old Contains test gave the Soviets the UH-1H and Hydras.)
 static bool IsWestern(string factionKey)
 {
  if (factionKey == "RHS_AFRF") return false;
  if (factionKey.StartsWith("USSR")) return false;
  return factionKey == "US" || factionKey.Contains("US") || factionKey == "NATO" || factionKey == "BLUFOR";
 }

 // Groups that have crewed a support vehicle. A crew that bails out (a helicopter shot up on
 // its pad) is no longer seated, and must still never be taken for a squad (4 Oct live: the AI
 // commander ordered a support helicopter's crew about).
 ref array<SCR_AIGroup> m_CrewGroups = {};
 ref map<IEntity, SCR_AIGroup> m_CrewOf = new map<IEntity, SCR_AIGroup>();

 void RememberCrew(IEntity vehicle)
 {
  if (!vehicle) return;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  foreach (BaseCompartmentSlot slot : slots)
  {
   // Passengers (an air lift's squad) are not crew.
   if (slot.GetType() == ECompartmentType.CARGO) continue;
   IEntity occupant = slot.GetOccupant();
   if (!occupant || !ACR_Respawn.IsAlive(occupant)) continue;
   AIControlComponent control = AIControlComponent.Cast(occupant.FindComponent(AIControlComponent));
   if (!control || !control.GetControlAIAgent()) continue;
   SCR_AIGroup group = SCR_AIGroup.Cast(control.GetControlAIAgent().GetParentGroup());
   if (!group) continue;
   m_CrewOf.Set(vehicle, group);
   if (m_CrewGroups.Contains(group)) continue;
   m_CrewGroups.Insert(group);
   PrintFormat("[ACR] SUPPORT crew group remembered for %1", vehicle.GetPrefabData().GetPrefabName());
  }
 }

 SCR_AIGroup CrewOf(IEntity vehicle)
 {
  SCR_AIGroup group;
  m_CrewOf.Find(vehicle, group);
  return group;
 }

 bool IsSupportGroup(SCR_AIGroup group)
 {
  if (m_CrewGroups.Contains(group)) return true;
  if (m_Vehicles.IsEmpty()) return false;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !character.GetCompartmentAccessComponent()) continue;
   BaseCompartmentSlot slot = character.GetCompartmentAccessComponent().GetCompartment();
   if (slot && slot.GetType() != ECompartmentType.CARGO && m_Vehicles.Contains(slot.GetVehicle())) return true;
  }
  return false;
 }

 // Called by the commander (or AI). Returns the feedback message.
 // squad: the squad to fly (air lift only).
 string Call(string factionKey, int index, vector target, bool byAI = false, int callerPlayerId = -1, ACR_Squad squad = null, ResourceName finalPrefab = "", vector finalTarget = vector.Zero, float finalRadius = -1, string finalOrder = "", int finalKind = 0)
 {
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(factionKey);
  if (index < 0 || index >= data.m_aSupports.Count()) return "Unknown support";
  ACR_SupportEntry entry = data.m_aSupports[index];
  float now = GetGame().GetWorld().GetWorldTime();
  bool waveSlot = entry.m_eType == ACR_ESupportType.AIR_LIFT && ACR_AirLift.WaveSlotAvailable(factionKey, now);
  if (now < entry.m_fReadyAt && !waveSlot) return string.Format("%1 is not ready (%2 s)", entry.m_sName, Math.Ceil((entry.m_fReadyAt - now) / 1000));
  // Mortar supports are fired by a real mortar team (ACR_Mortars): nothing is spent unless one can.
  if (ACR_Mortars.IsMortarType(entry.m_eType)) return CallMortars(factionKey, entry, target, byAI);
  if (entry.m_eType == ACR_ESupportType.AIR_LIFT) return ACR_AirLift.Call(this, factionKey, entry, target, byAI, squad, finalPrefab, finalTarget, finalRadius, finalOrder, finalKind);
  // The pack's attack helicopter (FitAttackHelicopter drops the row without one; this is a guard).
  ACR_SocketAttackHeli attack;
  if (entry.m_eType == ACR_ESupportType.ATTACK_HELI)
  {
   attack = AttackHelicopterFor(factionKey);
   if (!attack) return "No attack helicopter in this mission";
  }
  if (entry.m_eType == ACR_ESupportType.BOMB_RUN && entry.m_eBombRunDelivery != ACR_EBombRunDelivery.VIRTUAL)
  {
   if (entry.m_sBombRunPrefab.IsEmpty()) return "Choose a bomb-run prefab in this faction's support settings";
   Resource bombRunResource = Resource.Load(entry.m_sBombRunPrefab);
   if (!bombRunResource.IsValid()) return "Bomb-run prefab could not be loaded";
  }
  // With helipads in the world, helicopter supports fly a helicopter bought for a pad.
  ACR_StationedHeli home;
  if (ACR_HeliStation.IsHeliType(entry.m_eType) && ACR_HeliStation.UsesPads(factionKey))
  {
   home = Station().ReadyFor(factionKey, ACR_HeliStation.NeedsGunship(entry.m_eType), target, ACR_HeliStation.IsAttack(entry.m_eType));
   if (!home)
   {
    string why = Station().WhyNot(factionKey, ACR_HeliStation.NeedsGunship(entry.m_eType), ACR_HeliStation.IsAttack(entry.m_eType));
    PrintFormat("[ACR] SUPPORT %1 %2 refused: %3", factionKey, entry.m_sName, why);
    return why;
   }
  }
  if (!m_Manager.m_Bases.Spend(factionKey, entry.m_iCost)) return string.Format("Not enough RP for %1 (%2 needed)", entry.m_sName, entry.m_iCost);
  entry.m_fReadyAt = now + entry.m_iCooldown * 1000;
  target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);
  vector dir = ApproachDirection(factionKey, target);
  string where = ACR_Manager.Grid(target);
  int delay = DefaultDelay(entry.m_eType);
  PrintFormat("[ACR] SUPPORT %1 %2 at %3 (delay %4 s)", factionKey, entry.m_sName, target, delay);
  // A recon flight harms nobody: no danger area on the map, and the photographs land in the
  // side's intel as the jet crosses (ACR_ReconRun).
  if (entry.m_eType == ACR_ESupportType.RECON_FLIGHT)
  {
   ACR_ReconRun recon = new ACR_ReconRun();
   delay = recon.Start(entry, factionKey, target, dir);
   m_Runs.Insert(recon);
   string reconMessage = string.Format("%1 over %2, overhead in about %3 s.", entry.m_sName, where, delay);
   if (!byAI) m_Manager.NotifyFaction(factionKey, reconMessage);
   return reconMessage;
  }
  switch (entry.m_eType)
  {
   case ACR_ESupportType.ROCKET_STRIKE:
   {
    ACR_RadioIntercept.OnHeavySupport(factionKey, target, callerPlayerId);
    ACR_RocketStrikeRun strike = new ACR_RocketStrikeRun();
    strike.m_Target = target;
    strike.m_Dir = dir;
    strike.m_Pos = target - dir * (950 + 110 * delay);
    strike.m_Pos[1] = target[1] + 170;
    strike.m_Rocket = RocketFor(factionKey);
    m_Runs.Insert(strike);
    break;
   }
   case ACR_ESupportType.ARTILLERY_BATTERY:
   {
    // Three salvos of six over a wide area, each round a falling shell (its own incoming
    // whistle on every machine) that lands with a heavy blast.
    ACR_RadioIntercept.OnHeavySupport(factionKey, target, callerPlayerId);
    ResourceName shell = BatteryShellFor(factionKey);
    int warnMs = (delay - BATTERY_FALL_SECONDS) * 1000;
    if (warnMs < 0) warnMs = 0;
    GetGame().GetCallqueue().CallLater(WarnIncomingBattery, warnMs, false, target);
    if (!byAI) ACR_RadioAudio.NotifyFaction(factionKey, ACR_ERadioCue.FIRE_MISSION);
    for (int salvo = 0; salvo < BATTERY_SALVOS; salvo++)
    {
     for (int gun = 0; gun < BATTERY_ROUNDS; gun++)
     {
      float shellAngle = Math.RandomFloat(0, Math.PI2);
      float shellSpread = BATTERY_RADIUS * Math.Sqrt(Math.RandomFloat(0, 1));
      vector shellAt = target + Vector(Math.Cos(shellAngle) * shellSpread, 0, Math.Sin(shellAngle) * shellSpread);
      int shellMs = (delay - BATTERY_FALL_SECONDS) * 1000 + salvo * BATTERY_SALVO_SECONDS * 1000 + gun * Math.RandomIntInclusive(150, 600);
      if (shellMs < 0) shellMs = 0;
      GetGame().GetCallqueue().CallLater(FireBatteryRound, shellMs, false, shell, shellAt);
     }
    }
    break;
   }
   case ACR_ESupportType.BOMB_RUN:
    if (!StartBombRun(entry, target, dir, delay, callerPlayerId))
    {
     m_Manager.m_Bases.Refund(factionKey, entry.m_iCost);
     entry.m_fReadyAt = now;
     return "Bomb-run prefab could not be spawned";
    }
    ACR_RadioIntercept.OnHeavySupport(factionKey, target, callerPlayerId);
    break;
   case ACR_ESupportType.HELI_GUN_RUN:
   case ACR_ESupportType.HELI_LOITER:
   case ACR_ESupportType.HELI_DOOR_GUNS:
   case ACR_ESupportType.ATTACK_HELI:
   {
    ACR_HeliRun heli = new ACR_HeliRun();
    heli.m_sFaction = factionKey;
    heli.m_bDoorGuns = entry.m_eType == ACR_ESupportType.HELI_DOOR_GUNS;
    heli.m_bAttack = attack != null;
    heli.m_bLoiter = entry.m_eType == ACR_ESupportType.HELI_LOITER || heli.m_bDoorGuns || heli.m_bAttack;
    if (heli.m_bDoorGuns) heli.m_fLoiterMs = 120000;
    if (heli.m_bAttack) heli.m_fLoiterMs = ACR_HeliRun.ATTACK_LOITER_MS;
    heli.m_Target = target;
    heli.m_Dir = dir;
    heli.m_Rocket = RocketFor(factionKey);
    if (attack && !attack.m_sRocket.IsEmpty()) heli.m_Rocket = attack.m_sRocket;
    ResourceName helicopter = HelicopterFor(factionKey);
    if (heli.m_bDoorGuns) helicopter = TransportHelicopterFor(factionKey);
    if (attack) helicopter = attack.m_sPrefab;
    bool started;
    if (home) started = heli.StartFromPad(this, home, now);
    else started = heli.Start(this, helicopter, now);
    if (!started)
    {
     // Nothing flew: give the RP and the cooldown back.
     m_Manager.m_Bases.Refund(factionKey, entry.m_iCost);
     entry.m_fReadyAt = now;
     return "No helicopter available";
    }
    // Each helicopter in the air gets its own height band.
    array<ACR_HeliRun> flying = {};
    Helicopters(flying);
    array<int> taken = {};
    foreach (ACR_HeliRun up : flying) if (up.m_Vehicle && !up.m_bDestroyed) taken.Insert(up.m_iStack);
    while (taken.Contains(heli.m_iStack) && heli.m_iStack < 2) heli.m_iStack++;
    m_Runs.Insert(heli);
    delay = (int)(vector.DistanceXZ(heli.m_Start, target) / ACR_HeliRun.SPEED_RUN + ACR_HeliRun.SPOOL_MAX_SECONDS * 0.9);
    if (home)
    {
     Station().Launched(home);
     delay += (int)(ACR_HeliRun.LIFT_HEIGHT / ACR_HeliRun.LIFT_RATE) + 3;
    }
    break;
   }
  }
  // Fire breaks the truce (Jacob, 12 Nov): a strike called on the partner ends it now. Recon, smoke
  // and flares harm nobody; mortar teams fire real shells, caught where they hit (ACR_Truce.Hit).
  // Only once it is really on its way: a bomb run or helicopter that could not start is refunded
  // above and must not have broken the truce.
  if (entry.m_eType != ACR_ESupportType.MORTAR_SMOKE && entry.m_eType != ACR_ESupportType.ILLUMINATION)
  {
   ACR_Truce.StrikeCalled(m_Manager, factionKey, target, entry.m_sName);
   // A strike on a town grows FIA near it (ACR_FIAGrievance), counted when it lands.
   ACR_FIAGrievance.OnStrikeCalled(factionKey, target, entry.m_eType, delay);
  }
  AddZone(factionKey, target, ZoneRadius(entry.m_eType), entry.m_sName, delay, ZoneDuration(entry.m_eType));
  // Long waits are only announced roughly (to the nearest 15 s).
  string eta = string.Format("%1 s", delay);
  if (delay >= 30) eta = string.Format("about %1 s", (int)Math.Round(delay / 15.0) * 15);
  string message = string.Format("%1 on %2, ETA %3.", entry.m_sName, where, eta);
  if (!byAI) m_Manager.NotifyFaction(factionKey, message);
  return message;
 }

 protected bool StartBombRun(ACR_SupportEntry entry, vector target, vector dir, int delay, int callerPlayerId)
 {
  if (entry.m_eBombRunDelivery == ACR_EBombRunDelivery.EDITABLE_CALL_IN)
  {
   // RHS's call-in container spawns its prefab, then invokes this same engine hook so
   // editable entities can initialize their slots, timing and warhead instigator.
   GetGame().GetCallqueue().CallLater(SpawnBombRunCallIn, delay * 1000, false, entry.m_sBombRunPrefab, target, dir, callerPlayerId);
   return true;
  }

  vector start;
  float flyAltitude;
  // AC: Actual's period jets (ACR_Aircraft): their falling bombs, and blasts timed by the bombs' fall
  bool periodJets = ACR_Aircraft.PeriodJetsOn();
  if (entry.m_eBombRunDelivery == ACR_EBombRunDelivery.FLYOVER)
  {
   Resource resource = Resource.Load(entry.m_sBombRunPrefab);
   if (!resource.IsValid()) return false;
   float speed = Math.Clamp(entry.m_fBombRunSpeed, 20, 400);
   vector flat = dir;
   flat[1] = 0;
   flat.Normalize();
   start = target - flat * (speed * delay);
   // Overhead at the set height, and never lower than 150 m over a hill on the way in or
   // the first 2.5 km out.
   float altitude = target[1] + Math.Clamp(entry.m_fBombRunAltitude, 30, 1000);
   float pathLength = speed * delay + 2500;
   for (float d = 0; d <= pathLength; d += 100)
   {
    vector point = start + flat * d;
    altitude = Math.Max(altitude, GetGame().GetWorld().GetSurfaceY(point[0], point[2]) + 150);
   }
   start[1] = altitude;
   flyAltitude = altitude;
   float seconds = delay + 2500 / speed;
   if (ACR_Aircraft.MayFly(entry.m_sBombRunPrefab))
   {
    SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
    if (mode) mode.ACR_BroadcastFlyover(entry.m_sBombRunPrefab, start, flat, speed, altitude, seconds, periodJets);
    if (GetGame().GetPlayerController()) ACR_LocalFlyover.Start(entry.m_sBombRunPrefab, start, flat, speed, altitude, seconds, periodJets);
    PrintFormat("[ACR] SUPPORT flyover %1 at %2 m (%3 m over the target), %4 m/s", FilePath.StripPath(entry.m_sBombRunPrefab), Math.Round(altitude), Math.Round(altitude - target[1]), speed);
   }
   else PrintFormat("[ACR] SUPPORT flyover %1 not flown: AC: Actual's period jets are off (-acrPeriodJets)", FilePath.StripPath(entry.m_sBombRunPrefab));
  }

  int count = Math.Clamp(entry.m_iBombRunBombs, 1, 12);
  float spacing = Math.Clamp(entry.m_fBombRunSpacing, 1, 500);
  float scatter = Math.Clamp(entry.m_fBombRunScatter, 0, 100);
  vector right = Vector(dir[2], 0, -dir[0]);
  int interval = Math.Round(spacing / Math.Clamp(entry.m_fBombRunSpeed, 20, 400) * 1000);
  if (entry.m_eBombRunDelivery == ACR_EBombRunDelivery.VIRTUAL)
  {
   spacing = 28;
   interval = 350;
  }
  // what the jet drops: a high-drag bomb lands after the jet has passed over its spot (ACR_FallingBomb.Lag)
  array<vector> pylons = {};
  ResourceName dropped = ACR_LocalFlyover.Hardpoints(entry.m_sBombRunPrefab, pylons);
  for (int bomb = 0; bomb < count; bomb++)
  {
   float along = (bomb - (count - 1) * 0.5) * spacing;
   vector impact = target + dir * along + right * Math.RandomFloat(-scatter, scatter);
   int impactDelay = delay * 1000 + bomb * interval;
   if (entry.m_eBombRunDelivery == ACR_EBombRunDelivery.FLYOVER)
   {
    impactDelay = Math.Max(0, delay * 1000 + Math.Round(along / Math.Clamp(entry.m_fBombRunSpeed, 20, 400) * 1000));
    if (periodJets)
    {
     float dropHeight = flyAltitude - ACR_FallingBomb.PYLON_DROP - GetGame().GetWorld().GetSurfaceY(impact[0], impact[2]);
     impactDelay += Math.Round(ACR_FallingBomb.Lag(dropped, dropHeight, Math.Clamp(entry.m_fBombRunSpeed, 20, 400)) * 1000);
     // every machine's jet carries this bomb and lets it go to land here on time (ACR_FallingBomb)
     SCR_BaseGameMode bombMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
     if (bombMode) bombMode.ACR_BroadcastFlyoverBomb(start, impact, impactDelay / 1000.0);
     ACR_LocalFlyover flight = ACR_LocalFlyover.Find(start);
     if (GetGame().GetPlayerController() && flight) flight.AddBomb(impact, impactDelay / 1000.0);
    }
   }
   GetGame().GetCallqueue().CallLater(Explode, impactDelay, false, impact);
  }
  return true;
 }

 protected void SpawnBombRunCallIn(ResourceName prefab, vector target, vector dir, int callerPlayerId)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid())
  {
   PrintFormat("[ACR] SUPPORT bomb-run call-in prefab unavailable: %1", prefab);
   return;
  }
  vector transform[4];
  Math3D.AnglesToMatrix(Vector(Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG, 0, 0), transform);
  transform[3] = target;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  for (int i = 0; i < 4; i++) parameters.Transform[i] = transform[i];
  IEntity entity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
  if (!entity)
  {
   PrintFormat("[ACR] SUPPORT bomb-run call-in spawn failed: %1", prefab);
   return;
  }
  SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(entity.FindComponent(SCR_EditableEntityComponent));
  if (!editable)
  {
   PrintFormat("[ACR] SUPPORT bomb-run prefab has no editable call-in hook: %1", prefab);
   SCR_EntityHelper.DeleteEntityAndChildren(entity);
   return;
  }
  SCR_EditableEntityComponent parent;
  editable.EOnEditorPlace(parent, null, 0, false, callerPlayerId);
 }

 protected string CallMortars(string factionKey, ACR_SupportEntry entry, vector target, bool byAI)
 {
  if (m_Manager.m_Bases.GetRP(factionKey) < entry.m_iCost) return string.Format("Not enough RP for %1 (%2 needed)", entry.m_sName, entry.m_iCost);
  target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);
  string team;
  int eta;
  if (!m_Manager.Mortars().Fire(factionKey, entry.m_eType, target, team, eta))
  {
   PrintFormat("[ACR] SUPPORT %1 %2 refused: %3", factionKey, entry.m_sName, team);
   return team;
  }
  m_Manager.m_Bases.Spend(factionKey, entry.m_iCost);
  ACR_MortarTeam started = m_Manager.Mortars().m_LastStarted;
  if (started && started.m_bFiring)
  {
   started.m_iPaid = entry.m_iCost;
   started.m_ePaidType = entry.m_eType;
  }
  float now = GetGame().GetWorld().GetWorldTime();
  entry.m_fReadyAt = now + entry.m_iCooldown * 1000;
  PrintFormat("[ACR] SUPPORT %1 %2 at %3 by %4 (ETA %5 s)", factionKey, entry.m_sName, target, team, eta);
  if (entry.m_eType != ACR_ESupportType.MORTAR_SMOKE && entry.m_eType != ACR_ESupportType.ILLUMINATION) ACR_FIAGrievance.OnStrikeCalled(factionKey, target, entry.m_eType, eta);
  AddZone(factionKey, target, ZoneRadius(entry.m_eType), entry.m_sName, eta, ZoneDuration(entry.m_eType));
  string message = string.Format("%1 on %2: %3 firing, first rounds in about %4 s.", entry.m_sName, ACR_Manager.Grid(target), team, eta);
  if (!byAI) m_Manager.NotifyFaction(factionKey, message);
  return message;
 }

 ResourceName HelicopterFor(string factionKey)
 {
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  ACR_SupportUnitSettings unit;
  if (settings) unit = settings.SupportUnit(ACR_ESupportUnitRole.GUNSHIP);
  if (unit && !unit.m_sVehiclePrefab.IsEmpty()) return unit.m_sVehiclePrefab;
  if (settings && !settings.m_sSupportHelicopter.IsEmpty()) return settings.m_sSupportHelicopter;
  ResourceName prefab = HELI_EAST;
  if (IsWestern(factionKey)) prefab = HELI_WEST;
  return ACR_FactionCatalog.ResolvePrefab(factionKey, prefab, EEntityCatalogType.VEHICLE);
 }

 // Door-gunner support: the side's transport helicopter with its door guns.
 ResourceName TransportHelicopterFor(string factionKey)
 {
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  ACR_SupportUnitSettings unit;
  if (settings) unit = settings.SupportUnit(ACR_ESupportUnitRole.DOOR_GUN_HELI);
  if (unit && !unit.m_sVehiclePrefab.IsEmpty()) return unit.m_sVehiclePrefab;
  if (settings && !settings.m_sSupportTransportHelicopter.IsEmpty()) return settings.m_sSupportTransportHelicopter;
  ResourceName prefab = HELI_TRANSPORT_EAST;
  if (IsWestern(factionKey)) prefab = HELI_TRANSPORT_WEST;
  return ACR_FactionCatalog.ResolvePrefab(factionKey, prefab, EEntityCatalogType.VEHICLE);
 }

 ResourceName RocketFor(string factionKey)
 {
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  if (settings && !settings.m_sSupportRocket.IsEmpty()) return settings.m_sSupportRocket;
  ResourceName prefab = ROCKET_EAST;
  if (IsWestern(factionKey)) prefab = ROCKET_WEST;
  return ACR_FactionCatalog.ResolvePrefab(factionKey, prefab, EEntityCatalogType.VEHICLE);
 }

 // Aircraft and creeping barrages come from the side's nearest base.
 vector ApproachDirection(string factionKey, vector target)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != factionKey) continue;
   float distance = vector.DistanceXZ(base.GetOrigin(), target);
   if (distance > 60 && distance < bestDistance) { bestDistance = distance; best = base; }
  }
  vector dir = Vector(0, 0, 1);
  if (best) dir = target - best.GetOrigin();
  dir[1] = 0;
  if (dir.Length() < 1) dir = Vector(0, 0, 1);
  return dir.Normalized();
 }

 void ExplodeAt(vector impact) { Explode(impact); } // tests

 static int s_iExploded; // tests
 protected void Explode(vector impact)
 {
  s_iExploded++;
  impact[1] = GetGame().GetWorld().GetSurfaceY(impact[0], impact[2]) + 0.3;
  // Local on the server (damage only): the prefab replicates, and the replicated copy
  // died before it could show anything on clients. Each client makes its own (below).
  IEntity bomb = SpawnLocal(BOMB, impact, vector.Zero);
  int struck = BlastDamage(impact);
  ACR_CivilianSocket.Get().OnStrike(impact);
  PrintFormat("[ACR] SUPPORT bomb at %1 spawned=%2, blast struck %3", impact, bomb != null, struck);
  // Every client spawns its own explosion and plays the blast sound itself: the
  // explosion's own sound is cut off when the entity removes itself (1 Oct MP: the
  // bombs were seen but not heard).
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (mode) mode.ACR_BroadcastExplosion(impact);
  if (GetGame().GetPlayerController()) PlayBlast(impact); // a hosting player hears it too
 }

 // FOB HQ cook-off (Jacob, 18 Oct: a bigger blast when a demolished HQ goes up): one heavy
 // blast (the bomb run's, with its scripted lethality) shortly after the charge, then
 // COOKOFF_SECONDARIES medium blasts (vanilla Medium TNT: its own small damage radius) at
 // random spots round the tent over the next few seconds, as stored ammunition and fuel go.
 static const ResourceName COOKOFF_SECONDARY = "{564D57EA34A75775}Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Medium.et";
 static const ResourceName COOKOFF_SOUND = "{B9347A5498317F65}Sounds/Particles/Logistics/Explosion/TNT/Particles_Explosions_TNT_Medium.acp";
 static int COOKOFF_MAIN_MS = 700;
 static int COOKOFF_SECONDARIES = 4;
 static int COOKOFF_FIRST_MS = 1800;
 static int COOKOFF_LAST_MS = 7500;
 static float COOKOFF_SPREAD = 7;
 static int s_iCookOffs; // tests: cook-offs started
 static int s_iSecondaries; // tests: secondary blasts gone off
 static bool s_bCookOffOff; // tests: fixtures next to other test subjects

 // Returns the time (ms) until the last blast.
 int CookOff(vector at)
 {
  if (s_bCookOffOff) return 0;
  s_iCookOffs++;
  PrintFormat("[ACR] DEMOLITION cook-off at %1: main blast in %2 ms, %3 secondaries", at, COOKOFF_MAIN_MS, COOKOFF_SECONDARIES);
  GetGame().GetCallqueue().CallLater(Explode, COOKOFF_MAIN_MS, false, at);
  int last = COOKOFF_MAIN_MS;
  for (int i = 0; i < COOKOFF_SECONDARIES; i++)
  {
   float angle = Math.RandomFloat(0, Math.PI2);
   float dist = Math.RandomFloat(2, COOKOFF_SPREAD);
   vector spot = at + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);
   int delay = Math.RandomIntInclusive(COOKOFF_FIRST_MS, COOKOFF_LAST_MS);
   if (delay > last) last = delay;
   GetGame().GetCallqueue().CallLater(Secondary, delay, false, spot);
  }
  return last;
 }

 protected void Secondary(vector at)
 {
  s_iSecondaries++;
  at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + 0.5;
  IEntity blast = SpawnLocal(COOKOFF_SECONDARY, at, vector.Zero);
  PrintFormat("[ACR] DEMOLITION secondary blast at %1 spawned=%2", at, blast != null);
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (mode) mode.ACR_BroadcastExplosionOf(at, COOKOFF_SECONDARY, COOKOFF_SOUND);
  if (GetGame().GetPlayerController()) PlaySound(COOKOFF_SOUND, at);
 }

 // Spawns a rocket/shell pointing at the target; it is launched next frame on the
 // server and every client (projectiles are simulated locally everywhere).
 // Like the game's own weapons, each machine fires its own local projectile: the
 // server's does the damage, the clients' show and sound the flight and impact. (A
 // replicated rocket was deleted on clients when the server's hit first, often
 // before the client's copy reached the ground, so its blast never played there.)
 ResourceName BatteryShellFor(string factionKey)
 {
  ResourceName shell = "{98EC9C526AFBA282}Prefabs/Weapons/Ammo/Ammo_Shell_82mm_HE_O832DU.et";
  if (IsWestern(factionKey)) shell = "{38BAE094333E31BF}Prefabs/Weapons/Ammo/Ammo_Shell_81mm_HE_M821.et";
  return ACR_FactionCatalog.ResolvePrefab(factionKey, shell, EEntityCatalogType.VEHICLE);
 }

 protected void WarnIncomingBattery(vector target)
 {
  ACR_RadioAudio.WarnIncomingArtillery(target, BATTERY_RADIUS);
 }

 // One howitzer round: a shell dropped from high above the impact point (every machine flies
 // its own, so everyone hears it come in); where the server's lands, a heavy blast follows.
 void FireBatteryRound(ResourceName shell, vector impact)
 {
  impact[1] = GetGame().GetWorld().GetSurfaceY(impact[0], impact[2]);
  vector from = impact + Vector(Math.RandomFloat(-25, 25), BATTERY_DROP_HEIGHT, Math.RandomFloat(-25, 25));
  IEntity falling = FireProjectile(shell, from, impact);
  m_BatteryShells.Insert(falling);
  m_BatteryLast.Insert(from);
  m_BatteryAim.Insert(impact);
  m_BatteryDeadline.Insert(GetGame().GetWorld().GetWorldTime() + 20000);
 }

 protected void TrackBatteryShells(float now)
 {
  for (int i = m_BatteryShells.Count() - 1; i >= 0; i--)
  {
   IEntity falling = m_BatteryShells[i];
   if (falling && now < m_BatteryDeadline[i])
   {
    m_BatteryLast[i] = falling.GetOrigin();
    continue;
   }
   vector at = m_BatteryLast[i];
   bool landed = !falling;
   if (falling) { SCR_EntityHelper.DeleteEntityAndChildren(falling); at = m_BatteryAim[i]; }
   PrintFormat("[ACR] SUPPORT battery round down after %1 s, %2 m from its aim point (shell landed itself: %3)", ((now - m_BatteryDeadline[i] + 20000) / 1000), (int)vector.DistanceXZ(at, m_BatteryAim[i]), landed);
   // Landed (or never did): the blast where it came down.
   if (vector.DistanceXZ(at, m_BatteryAim[i]) > 60) at = m_BatteryAim[i];
   Explode(at);
   s_iBatteryImpacts++;
   s_aBatteryImpacts.Insert(at);
   m_BatteryShells.RemoveOrdered(i);
   m_BatteryLast.RemoveOrdered(i);
   m_BatteryAim.RemoveOrdered(i);
   m_BatteryDeadline.RemoveOrdered(i);
  }
 }

 IEntity FireProjectile(ResourceName prefab, vector from, vector to, vector parentVelocity = "0 0 0")
 {
  vector dir = (to - from).Normalized();
  IEntity projectile = SpawnLocal(prefab, from, dir);
  if (!projectile) return null;
  SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (gameMode) gameMode.ACR_BroadcastFire(prefab, from, dir, parentVelocity);
  ACR_PendingLaunch launch = new ACR_PendingLaunch();
  launch.m_Entity = projectile;
  launch.m_Dir = dir;
  launch.m_Velocity = parentVelocity;
  launch.m_iFrame = m_iFrame;
  m_Launches.Insert(launch);
  return projectile;
 }

 // A non-replicated copy of prefab at pos, nose along dir (zero: upright).
 // Heavy blasts (bomb run, howitzer battery) wrecked buildings but rarely killed (Jacob, 4 Oct):
 // the TNT prefab's own damage reaches only a few metres. On top of it, soldiers in the open
 // with a clear line to the blast take explosive damage that falls off with distance: lethal
 // inside BLAST_KILL, wounding (bleeding limbs and body) out to BLAST_WOUND. Cover between them
 // and the blast (a wall, a building, a rise) cuts it to BLAST_COVER; lying down halves it.
 // s_bBlastOff turns it off (the old behaviour; test comparison).
 static float BLAST_KILL = 12;
 static float BLAST_WOUND = 32;
 static float BLAST_COVER = 0.25;
 static bool s_bBlastOff;
 static const float BLAST_DAMAGE = 120; // per hit zone at the kill radius (tuned with srv:blast)
 static ref array<IEntity> s_BlastFound;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_aBatteryImpacts = {};
  s_BlastFound = {};
 }

 static bool CollectBlast(IEntity e)
 {
  if (ChimeraCharacter.Cast(e)) s_BlastFound.Insert(e);
  return true;
 }

 int BlastDamage(vector impact)
 {
  if (s_bBlastOff || System.IsCLIParam("acrBlastOldTest")) return 0;
  s_BlastFound.Clear();
  vector reach = Vector(BLAST_WOUND, BLAST_WOUND, BLAST_WOUND);
  GetGame().GetWorld().QueryEntitiesByAABB(impact - reach, impact + reach, CollectBlast);
  int struck;
  foreach (IEntity e : s_BlastFound)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(e);
   if (!character || !character.GetCharacterController()) continue;
   if (character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD) continue;
   float distance = vector.Distance(character.GetOrigin(), impact);
   if (distance > BLAST_WOUND) continue;
   SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(character.GetDamageManager());
   if (!damage) continue;
   // Inside the kill radius full damage, falling off to nothing at the wound radius.
   float share = 1;
   if (distance > BLAST_KILL) share = 1 - (distance - BLAST_KILL) / (BLAST_WOUND - BLAST_KILL);
   share = share * share;
   if (!ClearLine(impact + Vector(0, 0.5, 0), character.GetOrigin() + Vector(0, 0.9, 0), character)) share *= BLAST_COVER;
   if (character.GetCharacterController().GetStance() == ECharacterStance.PRONE) share *= 0.5;
   if (share < 0.02) continue;
   vector hit[3];
   hit[0] = character.GetOrigin() + Vector(0, 1, 0);
   hit[1] = (character.GetOrigin() - impact).Normalized();
   hit[2] = -hit[1];
   // Body and limbs nearest the blast: two to four zones depending on how close.
   int zones = 2;
   if (share > 0.4) zones = 3;
   if (share > 0.8) zones = 4;
   array<HitZone> all = {};
   damage.GetAllHitZones(all);
   array<HitZone> body = {};
   foreach (HitZone candidate : all) if (SCR_CharacterHitZone.Cast(candidate)) body.Insert(candidate);
   if (body.IsEmpty()) continue;
   for (int z = 0; z < zones; z++)
   {
    HitZone zone = body.GetRandomElement();
    SCR_DamageContext context = new SCR_DamageContext(EDamageType.EXPLOSIVE, BLAST_DAMAGE * share, hit, character, zone, Instigator.CreateInstigator(null), null, -1, -1);
    damage.HandleDamage(context);
   }
   struck++;
  }
  return struck;
 }

 protected static IEntity s_BlastTarget;
 static bool BlastFilter(notnull IEntity e)
 {
  if (e == s_BlastTarget || ChimeraCharacter.Cast(e)) return false;
  return true;
 }

 // Nothing solid between the blast and the soldier (other people don't shield).
 static bool ClearLine(vector from, vector to, IEntity target)
 {
  s_BlastTarget = target;
  TraceParam trace = new TraceParam();
  trace.Start = from;
  trace.End = to;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float hit = GetGame().GetWorld().TraceMove(trace, BlastFilter);
  return hit >= 0.99;
 }

 static IEntity SpawnLocal(ResourceName prefab, vector pos, vector dir)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return null;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  if (dir != vector.Zero) Math3D.MatrixFromForwardVec(dir, parameters.Transform);
  else Math3D.MatrixIdentity4(parameters.Transform);
  parameters.Transform[3] = pos;
  return GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), parameters);
 }

 static const ResourceName BOMB_SOUND = "{E4EF3755472EC669}Sounds/Particles/Logistics/Explosion/TNT/Particles_Explosions_TNT_Large.acp";
 // Plays the large blast at a point, independent of any entity (clients only).
 static bool PlayBlast(vector at)
 {
  return PlaySound(BOMB_SOUND, at);
 }

 static bool PlaySound(ResourceName sound, vector at)
 {
  // Straight through the audio system (the sound manager route stayed silent in the
  // 1 Oct live test); the sound graph reads distance and surroundings itself.
  vector mat[4];
  Math3D.MatrixIdentity4(mat);
  mat[3] = at;
  AudioHandle handle = AudioSystem.PlayEvent(sound, "SOUND_EXPLOSION", mat);
  bool ok = handle != AudioHandle.Invalid;
  PrintFormat("[ACR] SUPPORT blast sound at %1 played=%2", at, ok);
  return ok;
 }

 static void LaunchLocal(IEntity projectile, vector dir, vector parentVelocity)
 {
  if (!projectile) return;
  ProjectileMoveComponent move = ProjectileMoveComponent.Cast(projectile.FindComponent(ProjectileMoveComponent));
  if (move) move.Launch(dir, parentVelocity, 1, projectile, null, null, null, null);
 }

 ResourceName ConfiguredCrewPrefab(string factionKey, bool gunship)
 {
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  if (!settings) return "";
  ACR_ESupportUnitRole role = ACR_ESupportUnitRole.DOOR_GUN_HELI;
  if (gunship) role = ACR_ESupportUnitRole.GUNSHIP;
  ACR_SupportUnitSettings unit = settings.SupportUnit(role);
  if (unit) return unit.m_sCrewPrefab;
  return "";
 }

 ResourceName CrewPrefab(string factionKey, bool gunship = true)
 {
  ResourceName configured = ConfiguredCrewPrefab(factionKey, gunship);
  if (!configured.IsEmpty()) return configured;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(factionKey);
  // The pool's first soldier, with a content pack's swap (the pool keeps unswapped soldiers).
  if (!data.m_aGarrison.IsEmpty())
  {
   array<ResourceName> first = {data.m_aGarrison[0]};
   return ACR_BaseSystem.PickGarrisonSoldier(factionKey, first);
  }
  Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
  SCR_BasePlayerLoadout loadout;
  if (faction) loadout = GetGame().GetLoadoutManager().GetRandomFactionLoadout(faction);
  if (loadout) return loadout.GetLoadoutResource();
  return "";
 }

 // Living enemy soldiers (squads, garrisons and players) within radius.
 void FindEnemies(string factionKey, vector center, float radius, array<IEntity> result)
 {
  array<AIAgent> agents = {};
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   // Friendly, not equal: FIA allied to the caller (acrFIAAllegiance / leaning) is no target.
   if (!squad.GetFaction() || ACR_Insurgency.Friendly(squad.GetFaction().GetFactionKey(), factionKey)) continue;
   agents.Clear();
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents) AddIfNear(agent.GetControlledEntity(), center, radius, result);
  }
  foreach (SCR_AIGroup garrison : m_Manager.m_Bases.m_GarrisonGroups)
  {
   if (!garrison || !garrison.GetFaction() || ACR_Insurgency.Friendly(garrison.GetFaction().GetFactionKey(), factionKey)) continue;
   agents.Clear();
   garrison.GetAgents(agents);
   foreach (AIAgent guard : agents) AddIfNear(guard.GetControlledEntity(), center, radius, result);
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
   if (!faction || ACR_Insurgency.Friendly(faction.GetFactionKey(), factionKey)) continue;
   PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(id);
   if (pc) AddIfNear(pc.GetControlledEntity(), center, radius, result);
  }
 }

 protected void AddIfNear(IEntity soldier, vector center, float radius, array<IEntity> result)
 {
  if (!ACR_Respawn.IsAlive(soldier) || result.Contains(soldier)) return;
  if (vector.DistanceXZ(soldier.GetOrigin(), center) <= radius) result.Insert(soldier);
 }

 IEntity FindEnemyNear(string factionKey, vector center, float radius)
 {
  IEntity best;
  float bestDistance = radius;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == factionKey) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (!ACR_Respawn.IsAlive(soldier)) continue;
    float distance = vector.DistanceXZ(soldier.GetOrigin(), center);
    if (distance < bestDistance) { bestDistance = distance; best = soldier; }
   }
  }
  return best;
 }

 // Every server frame.
 protected float m_fNextCrewCheck;

 void Frame(float dt)
 {
  m_iFrame++;
  float now = GetGame().GetWorld().GetWorldTime();
  for (int l = m_Launches.Count() - 1; l >= 0; l--)
  {
   ACR_PendingLaunch launch = m_Launches[l];
   if (launch.m_iFrame >= m_iFrame) continue;
   m_Launches.Remove(l);
   if (!launch.m_Entity) continue;
   LaunchLocal(launch.m_Entity, launch.m_Dir, launch.m_Velocity);
  }
  for (int r = m_Runs.Count() - 1; r >= 0; r--)
  {
   ACR_SupportRun run = m_Runs[r];
   if (run.Frame(this, dt, now)) continue;
   run.Finish();
   m_Runs.Remove(r);
  }
  TrackBatteryShells(now);
  if (m_Station) m_Station.Tick(now);
  for (int v = m_Vehicles.Count() - 1; v >= 0; v--)
  {
   if (!m_Vehicles[v]) m_Vehicles.Remove(v);
   else if (now >= m_fNextCrewCheck) RememberCrew(m_Vehicles[v]);
  }
  if (now >= m_fNextCrewCheck)
  {
   m_fNextCrewCheck = now + 1000;
   for (int g = m_CrewGroups.Count() - 1; g >= 0; g--) if (!m_CrewGroups[g]) m_CrewGroups.Remove(g);
  }
  for (int z = m_Zones.Count() - 1; z >= 0; z--) if (now > m_Zones[z].m_fEndAt) m_Zones.Remove(z);
 }
}

// The game mode entity is replicated, so it carries the "launch this projectile" broadcast.
modded class SCR_BaseGameMode
{
 protected override void OnPlayerConnected(int playerId)
 {
  super.OnPlayerConnected(playerId);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && Replication.IsServer()) manager.PlayerConnected(playerId);
 }

 protected override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
 {
  // Possessing a squad soldier: hand him back first. Vanilla deletes the player's controlled
  // entity, which was that soldier, and left his command unit standing with nobody in it
  // (audit 31 Oct; as ACR_Requests.ReturnToCommandUnit).
  SCR_PlayerController possessor = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
  if (Replication.IsServer() && possessor && possessor.IsPossessing() && ACR_Manager.GetInstance())
  {
   ACR_Manager owner = ACR_Manager.GetInstance();
   ACR_MapHandoff.Release(possessor, possessor.GetControlledEntity());
   if (owner) owner.ReleaseAuthority(possessor);
   possessor.SetPossessedEntity(null);
   PrintFormat("[ACR] RETURN player=%1 (disconnected while possessing)", playerId);
  }
  super.OnPlayerDisconnected(playerId, cause, timeout);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && Replication.IsServer()) manager.PlayerDisconnected(playerId);
 }

 // Every death and destroyed vehicle goes to the cleanup list.
 protected override void OnControllableDestroyed(IEntity entity, IEntity killerEntity, notnull Instigator instigator)
 {
  super.OnControllableDestroyed(entity, killerEntity, instigator);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && manager.m_Cleanup && Replication.IsServer()) manager.m_Cleanup.OnDestroyed(entity);
 }

 void ACR_BroadcastFire(ResourceName prefab, vector from, vector dir, vector parentVelocity)
 {
  Rpc(ACR_RpcFire, prefab, from, dir, parentVelocity);
 }
 // Client: its own copy of a support projectile, launched a frame after it exists
 // (as on the server).
 [RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
 protected void ACR_RpcFire(ResourceName prefab, vector from, vector dir, vector parentVelocity)
 {
  if (Replication.IsServer()) return;
  IEntity projectile = ACR_Supports.SpawnLocal(prefab, from, dir);
  if (projectile) GetGame().GetCallqueue().CallLater(ACR_LaunchLater, 20, false, projectile, dir, parentVelocity);
 }
 protected void ACR_LaunchLater(IEntity projectile, vector dir, vector parentVelocity)
 {
  ACR_Supports.LaunchLocal(projectile, dir, parentVelocity);
 }
 // periodJets: the server's -acrPeriodJets (ACR_Aircraft), which a client can't read itself.
 void ACR_BroadcastFlyover(ResourceName prefab, vector start, vector dir, float speed, float altitude, float seconds, bool periodJets)
 {
  Rpc(ACR_RpcFlyover, prefab, start, dir, speed, altitude, seconds, periodJets);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
 protected void ACR_RpcFlyover(ResourceName prefab, vector start, vector dir, float speed, float altitude, float seconds, bool periodJets)
 {
  if (Replication.IsServer()) return;
  PrintFormat("[ACR] FLYOVER RPC received prefab=%1 at=%2", prefab, start);
  ACR_LocalFlyover.Start(prefab, start, dir, speed, altitude, seconds, periodJets);
 }
 void ACR_BroadcastFlyoverBomb(vector start, vector impact, float impactAt)
 {
  Rpc(ACR_RpcFlyoverBomb, start, impact, impactAt);
 }
 // Sent after its flyover on the same reliable channel, so the jet is already flying here.
 [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
 protected void ACR_RpcFlyoverBomb(vector start, vector impact, float impactAt)
 {
  if (Replication.IsServer()) return;
  ACR_LocalFlyover flight = ACR_LocalFlyover.Find(start);
  if (flight) flight.AddBomb(impact, impactAt);
 }
 void ACR_BroadcastExplosion(vector at)
 {
  Rpc(ACR_RpcExplosion, at);
 }
 // Any explosion prefab and its sound (FOB cook-off secondaries).
 void ACR_BroadcastExplosionOf(vector at, ResourceName prefab, ResourceName sound)
 {
  Rpc(ACR_RpcExplosionOf, at, prefab, sound);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
 protected void ACR_RpcExplosionOf(vector at, ResourceName prefab, ResourceName sound)
 {
  if (Replication.IsServer()) return;
  IEntity effect = ACR_Supports.SpawnLocal(prefab, at, vector.Zero);
  bool played = ACR_Supports.PlaySound(sound, at);
  PrintFormat("[ACR] SUPPORT explosion RPC received at=%1 prefab=%2 effect=%3 sound=%4", at, prefab, effect != null, played);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
 protected void ACR_RpcExplosion(vector at)
 {
  if (Replication.IsServer()) return;
  IEntity effect = ACR_Supports.SpawnLocal(ACR_Supports.BOMB, at, vector.Zero);
  bool sound = ACR_Supports.PlayBlast(at);
  PrintFormat("[ACR] SUPPORT explosion RPC received at=%1 effect=%2 sound=%3", at, effect != null, sound);
 }
}
