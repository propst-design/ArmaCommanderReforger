// One buyable unit. Mission makers add these in the faction settings entity's
// Requisition list (with the + button and the prefab browser), or leave the
// list empty and let it fill from the faction's Game Master catalog.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class ACR_RequisitionEntry
{
 [Attribute("", UIWidgets.EditBox, "Name shown in the buy list and on the map (numbered per purchase)")]
 string m_sName;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Infantry group to spawn", "et")]
 ResourceName m_sGroup;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Vehicle to spawn (optional)", "et")]
 ResourceName m_sVehicle;
 [Attribute("1", UIWidgets.CheckBox, "Spawn the vehicle with its driver and gunners")]
 bool m_bCrewed;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Group that rides in the vehicle (optional)", "et")]
 ResourceName m_sPassengers;
 [Attribute("20", UIWidgets.EditBox, "Cost in requisition points")]
 int m_iCost;
 [Attribute("2", UIWidgets.EditBox, "How many can be bought per match")]
 int m_iStock;
 bool m_bAircraft;   // set automatically; the AI commander never buys these (its AI cannot fly)
 bool m_bTransport;  // set automatically: an unarmed truck, which the AI commander buys to ferry squads
 bool m_bArmed = true; // set automatically for vehicles: has a weapon (the AI doesn't buy unarmed cars to fight with)
 bool m_bSquadCarrier; // set automatically: a carrier that seats a squad (TRAIT_PASSENGERS_LARGE: a BTR-70, not a BRDM-2)
 bool m_bCarrier;     // set automatically: an armed APC or IFV (Blitz's starting kit, ACR_DoctrineKits)
 bool m_bMortarTeam;  // the built-in mortar team (ACR_Mortars), added to every faction's list
 bool m_bObserverTeam; // the built-in forward observer team (ACR_FireMissions)
 bool m_bRare;        // set automatically: aircraft, armour, mortar team, support helicopters keep a fixed stock
 int m_iSupportHeli;  // support helicopter for a helipad (ACR_HeliStation): 1 gunship, 2 door gunners

 ACR_EFormation m_eFormation; // independent of display names
 ref array<ResourceName> m_aSoldiers; // native templates retained for reinforcement

 // runtime
 int m_iRemaining;
 int m_iBought;
 int m_iNamed;        // purchases ever made, for unit names (refunds don't reuse a number)
 int m_iBaseCost;     // the price before any purchase (m_iCost is the current price)
 bool m_bStepped;     // stepped pricing: unlimited, the price climbs with each purchase (ACR_Pricing)

 void Set(string name, ResourceName group, ResourceName vehicle, int cost, int stock)
 {
  m_sName = name;
  m_sGroup = group;
  m_sVehicle = vehicle;
  m_bCrewed = true;
  m_iCost = cost;
  m_iStock = stock;
 }
}

// How a faction's buy list limits purchases (ACR_Pricing).
enum ACR_EPricing
{
 STEPPED,     // ordinary units unlimited, price climbs a step per purchase; rare units fixed stock
 FIXED_STOCK  // every unit has a stock per match (scaled by threat level); prices never change
}

[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sFrom")]
class ACR_PrefabMapping
{
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Vanilla prefab to replace", "et")]
 ResourceName m_sFrom;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Faction-specific prefab", "et")]
 string m_sTo; // optional resource; resolved only when this faction is used
}

enum ACR_ESupportUnitRole
{
	MORTAR_TEAM,
	OBSERVER_TEAM,
	GUNSHIP,
	DOOR_GUN_HELI
}

// Editable per-faction requisition support. Empty prefab fields keep the current
// faction defaults; resource pickers let mission makers replace only what they need.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class ACR_SupportUnitSettings
{
 [Attribute("0", UIWidgets.ComboBox, "Support unit role", "", ParamEnumArray.FromEnum(ACR_ESupportUnitRole))]
 ACR_ESupportUnitRole m_eRole;
 [Attribute("1", UIWidgets.CheckBox, "Offer this unit for requisition")]
 bool m_bEnabled = true;
 [Attribute("", UIWidgets.EditBox, "Name shown in the requisition list (empty = default)")]
 string m_sName;
 [Attribute("-1", UIWidgets.EditBox, "Requisition cost in RP (-1 = default)")]
 int m_iCost;
 [Attribute("-1", UIWidgets.EditBox, "Quantity available per match (-1 = default)")]
 int m_iStock;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Observer team base group prefab", "et")]
 ResourceName m_sGroupPrefab;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Observer spotter character prefab", "et")]
 ResourceName m_sPrimaryPrefab;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Observer radio operator character prefab", "et")]
 ResourceName m_sSecondaryPrefab;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Support helicopter airframe prefab", "et")]
 ResourceName m_sVehiclePrefab;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Support helicopter crew prefab used for pilots and gunners", "et")]
 ResourceName m_sCrewPrefab;
}

// Per-faction Arma Commander setup, placed in World Editor: starting RP, what
// the commander can buy, and which soldiers garrison bases.
class ACR_FactionSettingsClass : GenericEntityClass {}
class ACR_FactionSettings : GenericEntity
{
 [Attribute("US", UIWidgets.EditBox, "Faction key this applies to (e.g. US, USSR, FIA)")]
 string m_sFaction;
 [Attribute("100", UIWidgets.EditBox, "Requisition points at the start")]
 int m_iStartingRP;
 [Attribute("1", UIWidgets.CheckBox, "An AI commander leads this faction whenever no player is in command")]
 bool m_bAICommander;
 [Attribute("8", UIWidgets.EditBox, "AI commander: most squads it keeps in the field")]
 int m_iAIUnitCap;
 [Attribute("1", UIWidgets.EditBox, "AI commander: how many base attacks it runs at once")]
 int m_iAIAttacks;
 [Attribute("0", UIWidgets.ComboBox, "AI commander doctrine: how it fights. Blank = random each match; the host's setup screen or -acrDoctrineWest/-acrDoctrineEast override it.", "", ParamEnumArray.FromEnum(ACR_EAIDoctrineChoice))]
 ACR_EAIDoctrineChoice m_eAIDoctrine;
 // The chosen doctrine's key, "" = random (ACR_AIDoctrine.Find).
 string AIDoctrineKey()
 {
  if (m_eAIDoctrine == ACR_EAIDoctrineChoice.BALANCED) return "balanced";
  if (m_eAIDoctrine == ACR_EAIDoctrineChoice.BLITZ) return "blitz";
  if (m_eAIDoctrine == ACR_EAIDoctrineChoice.FIREPOWER) return "firepower";
  if (m_eAIDoctrine == ACR_EAIDoctrineChoice.FORTRESS) return "fortress";
  if (m_eAIDoctrine == ACR_EAIDoctrineChoice.RAIDER) return "raider";
  return "";
 }
 // Skill and reaction of this side's soldiers (ACR_AISkill).
 [Attribute("0", UIWidgets.ComboBox, "AI skill of this side's soldiers (NONE = the mode's)", "", ParamEnumArray.FromEnum(EAISkill))]
 int m_iAISkill;
 [Attribute("1", UIWidgets.EditBox, "AI reaction scale: multiplies the mode's reaction speed for this side, e.g. 0.8 spots 20% slower")]
 float m_fAIReactionScale;
 [Attribute("0", UIWidgets.EditBox, "Percent of this side's soldiers one skill level higher (0-100)")]
 int m_iAIStrongerShare;
 [Attribute(desc: "Skill for particular units (e.g. Delta among Rangers). A soldier gets the first entry whose prefab he is, else the first whose prefab his group is; unset values fall back to the side's.")]
 ref array<ref ACR_AISkillProfile> m_aAISkillProfiles;
 [Attribute("0", UIWidgets.ComboBox, "Stepped prices: infantry, cars and trucks can be bought without limit but each purchase raises that unit's price (threat level sets the step); aircraft, armour, mortar teams and support helicopters keep their stock. Fixed stock: every unit has a stock per match, scaled by threat level, at a fixed price.", "", ParamEnumArray.FromEnum(ACR_EPricing))]
 ACR_EPricing m_ePricing;
 [Attribute("1", UIWidgets.CheckBox, "Add every group and vehicle from the faction's Game Master catalog with automatic prices. Entries below override or add to it.")]
 bool m_bAutoFill;
 [Attribute(desc: "Hand-picked units. With auto fill on, an entry using the same prefab replaces the automatic one.")]
 ref array<ref ACR_RequisitionEntry> m_aRequisitions;
 [Attribute(desc: "Automatic entries to leave out (group or vehicle prefabs)", params: "et")]
 ref array<ResourceName> m_aExclude;
 [Attribute(desc: "Soldiers used for base garrisons. Empty = the faction's riflemen, gunners and AT soldiers from its catalog.", params: "et")]
 ref array<ResourceName> m_aGarrisonSoldiers;
 [Attribute(desc: "Optional exact prefab replacements used for starting units and built-in support teams.")]
 ref array<ref ACR_PrefabMapping> m_aPrefabMappings;

 [Attribute("1", UIWidgets.CheckBox, "The commander can call off-map supports (artillery, air, helicopters)")]
 bool m_bSupports;
 [Attribute(desc: "Supports on offer. Empty = all of them with default prices and cooldowns.")]
 ref array<ref ACR_SupportEntry> m_aSupports;
 [Attribute(desc: "Optional editable requisition support units. Add a row to override defaults; each row can enable/disable a role and set its price, stock and prefabs.")]
 ref array<ref ACR_SupportUnitSettings> m_aSupportUnits;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Legacy gunship fallback; m_aSupportUnits airframe overrides it (empty = UH-1H west, Mi-8 east)", "et")]
 ResourceName m_sSupportHelicopter;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Legacy door-gunner helicopter fallback; m_aSupportUnits airframe overrides it (empty = armed UH-1H west, Mi-8 east)", "et")]
 ResourceName m_sSupportTransportHelicopter;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Rocket for rocket strikes and gunships (empty = Hydra 70 / S-5)", "et")]
 ResourceName m_sSupportRocket;

 ACR_SupportUnitSettings SupportUnit(ACR_ESupportUnitRole role)
 {
  if (!m_aSupportUnits) return null;
  foreach (ACR_SupportUnitSettings unit : m_aSupportUnits)
   if (unit && unit.m_eRole == role) return unit;
  return null;
 }

 static ref array<ACR_FactionSettings> s_All = {};

 void ACR_FactionSettings(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT);
 }
 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode()) return;
  s_All.Insert(this);
 }
 void ~ACR_FactionSettings()
 {
  if (s_All) s_All.RemoveItem(this);
 }

 static ACR_FactionSettings Find(string factionKey)
 {
  foreach (ACR_FactionSettings settings : s_All)
   if (settings.m_sFaction == factionKey) return settings;
  return null;
 }

 // A selected faction the world has no settings entity for (any faction mod, or RHS on Everon)
 // gets one copied from the authored side it replaces (US for the west side, USSR for the east):
 // RP, AI commander, caps, pricing and supports. Prefab-specific lists (hand-picked buys,
 // exclusions, garrisons, mappings, support units) aren't copied; its own catalog fills them.
 static void EnsureFor(string factionKey, string templateKey)
 {
  if (factionKey.IsEmpty() || Find(factionKey)) return;
  ACR_FactionSettings template = Find(templateKey);
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  if (template) parameters.Transform[3] = template.GetOrigin();
  ACR_FactionSettings made = ACR_FactionSettings.Cast(GetGame().SpawnEntity(ACR_FactionSettings, GetGame().GetWorld(), parameters));
  if (!made) return;
  if (!s_All.Contains(made)) s_All.Insert(made);
  made.m_sFaction = factionKey;
  made.m_iStartingRP = 100;
  made.m_bAICommander = true;
  made.m_iAIUnitCap = 8;
  made.m_iAIAttacks = 1;
  made.m_ePricing = ACR_EPricing.STEPPED;
  made.m_bAutoFill = true;
  made.m_bSupports = true;
  made.m_fAIReactionScale = 1;
  if (template)
  {
   made.m_iAISkill = template.m_iAISkill;
   made.m_fAIReactionScale = template.m_fAIReactionScale;
   made.m_iAIStrongerShare = template.m_iAIStrongerShare;
   made.m_iStartingRP = template.m_iStartingRP;
   made.m_bAICommander = template.m_bAICommander;
   made.m_iAIUnitCap = template.m_iAIUnitCap;
   made.m_iAIAttacks = template.m_iAIAttacks;
   made.m_eAIDoctrine = template.m_eAIDoctrine;
   made.m_ePricing = template.m_ePricing;
   made.m_bSupports = template.m_bSupports;
   if (template.m_aSupports)
   {
    made.m_aSupports = {};
    foreach (ACR_SupportEntry entry : template.m_aSupports)
     if (entry) made.m_aSupports.Insert(entry.Copy());
   }
  }
  PrintFormat("[ACR] FACTION settings for %1 copied from %2 (none in the world)", factionKey, templateKey);
 }
}

// Builds the runtime buy list and garrison pool for a faction.
class ACR_FactionCatalog
{
 // Resolve explicit mission-maker mappings first. Otherwise find a faction catalog
 // prefab with the same role/name suffix, which covers RHS group variants.
 // Results are cached (Jacob 18 Oct, Kolguyev with RHS: the mortar range ring resolved its shell
 // through a full catalog scan about four times a second, logging a fallback line each time).
 static ref map<string, ResourceName> s_Resolved = new map<string, ResourceName>();
 static void ForgetResolved() { s_Resolved.Clear(); }

 static ResourceName ResolvePrefab(string factionKey, ResourceName source, EEntityCatalogType type)
 {
  if (source.IsEmpty()) return source;
  string key = string.Format("%1|%2|%3", factionKey, type, source);
  ResourceName cached;
  if (s_Resolved.Find(key, cached)) return cached;
  ResourceName resolved = ResolveUncached(factionKey, source, type);
  s_Resolved.Set(key, resolved);
  return resolved;
 }

 protected static ResourceName ResolveUncached(string factionKey, ResourceName source, EEntityCatalogType type)
 {
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  if (settings && settings.m_aPrefabMappings)
   foreach (ACR_PrefabMapping mapping : settings.m_aPrefabMappings)
    if (mapping && mapping.m_sFrom == source && !mapping.m_sTo.IsEmpty()) return mapping.m_sTo;
  string sourceLeaf = FilePath.StripExtension(FilePath.StripPath(source));
  string wanted = sourceLeaf;
  if (wanted.StartsWith("Group_USSR_")) wanted.Replace("Group_USSR_", "");
  else if (wanted.StartsWith("Group_US_")) wanted.Replace("Group_US_", "");
  SCR_EntityCatalog catalog = GetCatalog(factionKey, type);
  if (!catalog) return source;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  // The same vehicle in the faction's own paint (M923A1_transport -> M923A1_transport_MERDC).
  if (type == EEntityCatalogType.VEHICLE)
  {
   array<string> paints = {};
   PaintSuffixes(factionKey, paints);
   foreach (string paint : paints)
    foreach (SCR_EntityCatalogEntry entry : entries)
     if (entry.IsEnabled() && FilePath.StripExtension(FilePath.StripPath(entry.GetPrefab())) == sourceLeaf + paint && Allowed(factionKey, type, entry.GetPrefab())) return entry.GetPrefab();
  }
  // The faction's standard subunit first; any other match only if it has none.
  ResourceName found = MatchCatalog(entries, sourceLeaf, wanted, factionKey, type, true);
  if (found.IsEmpty()) found = MatchCatalog(entries, sourceLeaf, wanted, factionKey, type, false);
  if (!found.IsEmpty()) return found;
  PrintFormat("[ACR] FACTION fallback faction=%1 entry=%2 source prefab unchanged", factionKey, source);
  return source;
 }

 // RHS has no mortar-specific character prefabs. Use its standard rifleman for
 // the crew so purchased mortar teams keep the selected faction's appearance.
 static ResourceName StandardRifleman(string factionKey, ResourceName fallback)
 {
  SCR_EntityCatalog catalog = GetCatalog(factionKey, EEntityCatalogType.CHARACTER);
  if (!catalog) return fallback;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  foreach (SCR_EntityCatalogEntry entry : entries)
  {
   if (!entry.IsEnabled() || !IsStandard(factionKey, entry.GetPrefab(), EEntityCatalogType.CHARACTER)) continue;
   if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_RIFLEMAN)) return entry.GetPrefab();
  }
  return fallback;
 }

 // Standard Marines use the 4x RCO instead of the issued 8x VC18DSCO.
 // Covers native garrisons/officers and authored groups as well as bought squads;
 // dedicated M38/M40 sights and other weapon families retain their issued optics.
 static void TrimUSMCRiflemanOptic(IEntity soldier)
 {
  if (!soldier || !soldier.GetPrefabData()) return;
  string characterPrefab = soldier.GetPrefabData().GetPrefabName();
  if (!characterPrefab.Contains("/RHS_USAF_USMC_MEF/")) return;
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
  if (!weapons) return;
  array<IEntity> list = {};
  weapons.GetWeaponsList(list);
  foreach (IEntity weaponEntity : list)
  {
   if (!weaponEntity || !weaponEntity.GetPrefabData() || !weaponEntity.GetPrefabData().GetPrefabName().Contains("Rifle_M27IAR_VC18DSCO")) continue;
   BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
   if (!weapon) continue;
   array<AttachmentSlotComponent> attachments = {};
   weapon.GetAttachments(attachments);
   foreach (AttachmentSlotComponent attachmentSlot : attachments)
   {
    IEntity optic = attachmentSlot.GetAttachedEntity();
    if (!optic || !optic.GetPrefabData() || !optic.GetPrefabData().GetPrefabName().Contains("optic_vc18dsco")) continue;
    Resource resource = Resource.Load("{2518CA6044D6BBDD}Prefabs/Weapons/Attachments/Optics/ta31rco/Optic_TA31RCO.et");
    if (!resource || !resource.IsValid()) continue;
    EntitySpawnParams parameters = new EntitySpawnParams();
    parameters.TransformMode = ETransformMode.WORLD;
    parameters.Transform[3] = soldier.GetOrigin();
    IEntity replacement = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
    if (!replacement) continue;
    if (!attachmentSlot.CanSetAttachment(replacement)) { SCR_EntityHelper.DeleteEntityAndChildren(replacement); continue; }
    attachmentSlot.SetAttachment(null);
    attachmentSlot.SetAttachment(replacement);
    if (attachmentSlot.GetAttachedEntity() == replacement) SCR_EntityHelper.DeleteEntityAndChildren(optic);
    else
    {
     attachmentSlot.SetAttachment(optic);
     SCR_EntityHelper.DeleteEntityAndChildren(replacement);
    }
   }
  }
 }

 protected static ResourceName MatchCatalog(array<SCR_EntityCatalogEntry> entries, string sourceLeaf, string wanted, string factionKey, EEntityCatalogType type, bool standardOnly)
 {
  ResourceName suffixMatch;
  foreach (SCR_EntityCatalogEntry entry : entries)
  {
   if (!entry.IsEnabled()) continue;
   ResourceName candidate = entry.GetPrefab();
   if (!Allowed(factionKey, type, candidate)) continue;
   if (standardOnly && !IsStandard(factionKey, candidate, type)) continue;
   string leaf = FilePath.StripExtension(FilePath.StripPath(candidate));
   if (leaf == sourceLeaf) return candidate;
   if (wanted.Length() > 3 && leaf.Contains(wanted))
   {
    if (leaf.EndsWith(wanted)) return candidate;
    if (suffixMatch.IsEmpty()) suffixMatch = candidate;
   }
  }
  return suffixMatch;
 }

 // RHS catalogs mix several subunits and camouflages (RHS_USAF: USMC MEF woodland, MEF
 // desert and MARSOC; RHS_AFRF: MSV in four uniforms plus special forces). Jacob, 8 Oct:
 // one consistent look per side. Soldiers and groups come from one subunit; vehicles take
 // the faction's paint where RHS has one and never the desert/tan versions.
 static string StandardPath(string factionKey)
 {
  if (factionKey == "RHS_USAF") return "/RHS_USAF_USMC_MEF/";
  if (factionKey == "RHS_AFRF") return "/MSV/VKPO_Summer/";
  return "";
 }

 static void PaintSuffixes(string factionKey, notnull array<string> result)
 {
  result.Clear();
  if (factionKey == "RHS_USAF") { result.Insert("_USAF"); result.Insert("_MERDC"); }
  else if (factionKey == "RHS_AFRF") { result.Insert("_camo1"); result.Insert("_Camo"); }
 }

 static bool IsStandard(string factionKey, ResourceName prefab, EEntityCatalogType type)
 {
  string path = prefab;
  if (path.Contains("NotSpawned") || !Allowed(factionKey, type, prefab)) return false;
  string leaf = FilePath.StripExtension(FilePath.StripPath(prefab));
  if (type == EEntityCatalogType.VEHICLE)
  {
   if (factionKey == "RHS_USAF" && leaf.EndsWith("_D")) return false;
   if (factionKey == "RHS_AFRF" && leaf.EndsWith("_tan")) return false;
   return true;
  }
  string standard = StandardPath(factionKey);
  return standard.IsEmpty() || path.Contains(standard);
 }

 // A plugged-in content pack (ACR_ContentSocket) may keep mod entries out of the catalogs.
 static bool Allowed(string factionKey, EEntityCatalogType type, ResourceName prefab)
 {
  return ACR_ContentSocket.Get().AllowsCatalogEntry(factionKey, type, prefab);
 }

 protected static ref map<string, ResourceName> s_Crewmen = new map<string, ResourceName>();

 // The standard subunit's crewman (helicopter pilot for helicopters), "" when the faction
 // has no standard subunit or no such soldier.
 static ResourceName CrewmanFor(string factionKey, IEntity vehicle)
 {
  if (!vehicle || !vehicle.GetPrefabData()) return "";
  string vehiclePath = vehicle.GetPrefabData().GetPrefabName();
  // UA (the US key under Ukraine Armed Forces RHS): the mod's vehicles (BTR-25, armed Mi-8)
  // keep the vanilla Soviet default crew, so a bought BTR-25 was crewed by USSR riflemen
  // (11 Oct: no gunner for Take over gunner). US crewmen; the UA kit pass dresses them.
  if (factionKey == "US" && ACR_FactionSelection.UkraineModLoaded())
  {
   if (vehiclePath.Contains("/Helicopters/")) return "{42A502E3BB727CEB}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_HeliPilot.et";
   return "{E1CB513B8B9B08F4}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Crew.et";
  }
  if (StandardPath(factionKey).IsEmpty()) return "";
  string role = "_Crew";
  if (vehiclePath.Contains("/Helicopters/")) role = "_HeliPilot";
  string key = factionKey + role;
  ResourceName crewman;
  if (s_Crewmen.Find(key, crewman)) return crewman;
  SCR_EntityCatalog catalog = GetCatalog(factionKey, EEntityCatalogType.CHARACTER);
  if (catalog)
  {
   array<SCR_EntityCatalogEntry> entries = {};
   catalog.GetEntityList(entries);
   foreach (SCR_EntityCatalogEntry entry : entries)
   {
    if (!entry.IsEnabled() || !IsStandard(factionKey, entry.GetPrefab(), EEntityCatalogType.CHARACTER)) continue;
    if (FilePath.StripExtension(FilePath.StripPath(entry.GetPrefab())).EndsWith(role)) { crewman = entry.GetPrefab(); break; }
   }
  }
  s_Crewmen.Set(key, crewman);
  PrintFormat("[ACR] FACTION crewman %1 %2: %3", factionKey, role, crewman);
  return crewman;
 }

 static int PaintRank(string factionKey, ResourceName prefab)
 {
  string leaf = FilePath.StripExtension(FilePath.StripPath(prefab));
  array<string> paints = {};
  PaintSuffixes(factionKey, paints);
  for (int i = 0; i < paints.Count(); i++)
   if (leaf.EndsWith(paints[i])) return paints.Count() - i;
  return 0;
 }

 static void BuildRequisitions(string factionKey, ACR_FactionSettings settings, array<ref ACR_RequisitionEntry> result)
 {
  result.Clear();
  array<ResourceName> used = {};
  if (settings && settings.m_aRequisitions)
  {
   foreach (ACR_RequisitionEntry manual : settings.m_aRequisitions)
   {
    if (manual.m_sGroup.IsEmpty() && manual.m_sVehicle.IsEmpty()) continue;
    ACR_RequisitionEntry copy = new ACR_RequisitionEntry();
    copy.Set(manual.m_sName, manual.m_sGroup, manual.m_sVehicle, manual.m_iCost, manual.m_iStock);
    copy.m_bCrewed = manual.m_bCrewed;
    copy.m_sPassengers = manual.m_sPassengers;
    ApplyCatalogFlags(factionKey, copy);
    if (copy.m_sName.IsEmpty()) copy.m_sName = "Unit";
    result.Insert(copy);
    if (!manual.m_sGroup.IsEmpty()) used.Insert(manual.m_sGroup);
    if (!manual.m_sVehicle.IsEmpty()) used.Insert(manual.m_sVehicle);
   }
  }
  if (!settings || settings.m_bAutoFill)
  {
   array<ResourceName> exclude = {};
   if (settings && settings.m_aExclude) exclude.Copy(settings.m_aExclude);
   if (ACR_InfantryCatalog.UseDefaults(factionKey, settings)) ACR_InfantryCatalog.Build(factionKey, result);
   else AddCatalog(factionKey, EEntityCatalogType.GROUP, used, exclude, result);
   AddCatalog(factionKey, EEntityCatalogType.VEHICLE, used, exclude, result);
   ACR_ContentSocket.Get().AddRequisitions(factionKey, result);
   EnsureWestGunship(factionKey, exclude, result);
  }
  EnsureRUAFVehicles(factionKey, result);
  // Mission-authored lists retain their prices.
  if (!settings || !settings.m_aRequisitions || settings.m_aRequisitions.IsEmpty()) ACR_InfantryCatalog.VehiclePrices(factionKey, result);
  // Mission makers can edit support requisitions per faction; an absent row keeps defaults.
  ACR_RequisitionEntry supportEntry = ACR_Mortars.MakeEntry();
  if (ApplySupportUnit(supportEntry, settings, ACR_ESupportUnitRole.MORTAR_TEAM)) result.Insert(supportEntry);
  supportEntry = ACR_FireMissions.MakeEntry(settings);
  if (ApplySupportUnit(supportEntry, settings, ACR_ESupportUnitRole.OBSERVER_TEAM)) result.Insert(supportEntry);
  // Worlds with helipads: support helicopters are bought for a pad (a lost one is bought again).
  if (ACR_HeliStation.UsesPads(factionKey))
  {
   supportEntry = ACR_HeliStation.MakeEntry(true, settings);
   if (ApplySupportUnit(supportEntry, settings, ACR_ESupportUnitRole.GUNSHIP)) result.Insert(supportEntry);
   supportEntry = ACR_HeliStation.MakeEntry(false, settings);
   if (ApplySupportUnit(supportEntry, settings, ACR_ESupportUnitRole.DOOR_GUN_HELI)) result.Insert(supportEntry);
  }
  bool stepped = ACR_Pricing.Stepped(settings);
  foreach (ACR_RequisitionEntry entry : result)
  {
   if (entry.m_bMortarTeam || entry.m_bObserverTeam || entry.m_iSupportHeli > 0 || entry.m_bAircraft) entry.m_bRare = true;
   ACR_Pricing.Setup(entry, stepped);
  }
  PrintFormat("[ACR] Requisition list for %1: %2 entries (%3 helipads in the world), pricing %4", factionKey, result.Count(), ACR_Helipad.s_All.Count(), ACR_Pricing.Name(stepped));
 }

 // The catalogue's VEHICLE_TURRET label can filter the armed UH-1H out of the
 // normal requisition list even though the support variant is still offered.
 // Keep the standard US gunship purchasable as a rare, crewed vehicle.
 protected static void EnsureWestGunship(string factionKey, array<ResourceName> exclude, array<ref ACR_RequisitionEntry> result)
 {
  if (factionKey != "US" || exclude.Contains(ACR_Supports.HELI_WEST)) return;
  foreach (ACR_RequisitionEntry existing : result)
   if (existing.m_sVehicle.Contains("UH1H_armed_gunship")) return;
  ACR_RequisitionEntry heli = new ACR_RequisitionEntry();
  heli.Set("Gunship Huey", "", ACR_Supports.HELI_WEST, 60, 1);
  heli.m_bAircraft = true;
  heli.m_bArmed = true;
  heli.m_bRare = true;
  result.Insert(heli);
 }

 // Missing config rows preserve the built-in offer. A row can override only the values
 // the mission maker cares about, or disable that support requisition entirely.
 protected static bool ApplySupportUnit(ACR_RequisitionEntry entry, ACR_FactionSettings settings, ACR_ESupportUnitRole role)
 {
  if (!entry || !settings) return true;
  ACR_SupportUnitSettings unit = settings.SupportUnit(role);
  if (!unit) return true;
  if (!unit.m_bEnabled) return false;
  if (!unit.m_sName.IsEmpty()) entry.m_sName = unit.m_sName;
  if (unit.m_iCost >= 0) entry.m_iCost = unit.m_iCost;
  if (unit.m_iStock >= 0) entry.m_iStock = unit.m_iStock;
  return true;
 }

 // Hand-added entries get the same automatic flags as catalog ones (aircraft, armed,
 // transport, rare) from the faction's catalog entry for their vehicle, when it has one.
 protected static void ApplyCatalogFlags(string factionKey, ACR_RequisitionEntry item)
 {
  if (item.m_sVehicle.IsEmpty()) return;
  SCR_EntityCatalog catalog = GetCatalog(factionKey, EEntityCatalogType.VEHICLE);
  if (!catalog) return;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  foreach (SCR_EntityCatalogEntry entry : entries)
  {
   if (entry.GetPrefab() != item.m_sVehicle) continue;
   SetVehicleFlags(entry, item);
   return;
  }
 }

 protected static void SetVehicleFlags(SCR_EntityCatalogEntry entry, ACR_RequisitionEntry item)
 {
  item.m_bAircraft = entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_HELICOPTER) || entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_AIRPLANE);
  item.m_bArmed = entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED);
  item.m_bTransport = entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TRUCK) && !entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED) && !item.m_bAircraft;
  item.m_bRare = item.m_bAircraft || entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_APC) || entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMOR);
  item.m_bCarrier = item.m_bArmed && !item.m_bAircraft && entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_APC);
  item.m_bSquadCarrier = item.m_bCarrier && entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_PASSENGERS_LARGE);
 }

 protected static SCR_EntityCatalog GetCatalog(string factionKey, EEntityCatalogType type)
 {
  SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(factionKey));
  if (!faction) return null;
  return faction.GetFactionEntityCatalogOfType(type, false);
 }

 protected static void AddCatalog(string factionKey, EEntityCatalogType type, array<ResourceName> used, array<ResourceName> exclude, array<ref ACR_RequisitionEntry> result)
 {
  SCR_EntityCatalog catalog = GetCatalog(factionKey, type);
  if (!catalog) return;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  foreach (SCR_EntityCatalogEntry entry : entries)
  {
   if (!entry.IsEnabled()) continue;
   ResourceName prefab = entry.GetPrefab();
   if (prefab.IsEmpty() || used.Contains(prefab) || exclude.Contains(prefab)) continue;
   if (type == EEntityCatalogType.VEHICLE && IsRUAFCatalogVehicleExcluded(factionKey, prefab)) continue;
   if (!IsStandard(factionKey, prefab, type)) continue;
   // Guards can't be hired in vanilla either; static guns become supports later.
   if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_GUARD) || entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_GUARD)) continue;
   if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TURRET) || entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_AIRPLANE)) continue;
   string name = WidgetManager.Translate(entry.GetEntityName());
   if (name.IsEmpty()) name = FilePath.StripExtension(FilePath.StripPath(prefab));
   if (type == EEntityCatalogType.GROUP && name == "Group") name = GroupName(prefab);
   // The catalog lists camouflage variants under the same name; offer each unit once,
   // in the faction's own paint when it has one.
   ACR_RequisitionEntry same = FindByName(result, name);
   if (same)
   {
    if (type == EEntityCatalogType.VEHICLE && !same.m_sVehicle.IsEmpty() && PaintRank(factionKey, prefab) > PaintRank(factionKey, same.m_sVehicle))
     same.m_sVehicle = prefab;
    continue;
   }
   ACR_RequisitionEntry item = new ACR_RequisitionEntry();
   if (type == EEntityCatalogType.GROUP)
   {
    int cost = GroupCost(entry);
    item.Set(name, prefab, "", cost, StockFor(cost, 60, 6));
   }
   else
   {
    int cost = VehicleCost(entry);
    item.Set(name, "", prefab, cost, StockFor(cost, 90, 4));
    SetVehicleFlags(entry, item);
   }
   used.Insert(prefab);
   result.Insert(item);
  }
 }

 protected static string GroupName(ResourceName prefab)
 {
  string name = FilePath.StripExtension(FilePath.StripPath(prefab));
  if (name.StartsWith("Group_")) name = name.Substring(6, name.Length() - 6);
  array<string> prefixes = {"USAF_USMC_MEF_", "RHS_RF_MSV_VKPO_S_", "RHS_USAF_USMC_MEF_", "USSR_", "US_"};
  foreach (string prefix : prefixes)
   if (name.StartsWith(prefix)) { name = name.Substring(prefix.Length(), name.Length() - prefix.Length()); break; }
  if (name.Contains("Sniper")) return "Sniper Team";
  name.Replace("_", " ");
  name.Replace("Team", " Team");
  name.Replace("Squad", " Squad");
  return name;
 }

 protected static bool IsRUAFCatalogVehicleExcluded(string factionKey, ResourceName prefab)
 {
  if (factionKey != "RHS_AFRF") return false;
  string leaf = FilePath.StripExtension(FilePath.StripPath(prefab));
  return leaf.StartsWith("APC_K17");
 }

 protected static void EnsureRUAFVehicles(string factionKey, array<ref ACR_RequisitionEntry> result)
 {
  if (factionKey != "RHS_AFRF") return;
  bool btr70, brdm2;
  foreach (ACR_RequisitionEntry entry : result)
  {
   string leaf = FilePath.StripExtension(FilePath.StripPath(entry.m_sVehicle));
   if (leaf.Contains("BTR70")) btr70 = true;
   if (leaf.Contains("BRDM2")) brdm2 = true;
  }
  if (!btr70)
  {
   ACR_RequisitionEntry btr = new ACR_RequisitionEntry();
   btr.Set("BTR-70 Armored Personnel Carrier", "", "{C012BB3488BEA0C2}Prefabs/Vehicles/Wheeled/BTR70/BTR70.et", 76, 1);
   btr.m_bArmed = true;
   btr.m_bRare = true;
   btr.m_bTransport = true;
   result.Insert(btr);
  }
  if (!brdm2)
  {
   ACR_RequisitionEntry brdm = new ACR_RequisitionEntry();
   brdm.Set("BRDM-2 Reconnaissance Vehicle", "", "{254289B9C09904AB}Prefabs/Vehicles/Wheeled/BRDM2/BRDM2.et", 76, 1);
   brdm.m_bArmed = true;
   brdm.m_bRare = true;
   result.Insert(brdm);
  }
 }

 protected static ACR_RequisitionEntry FindByName(array<ref ACR_RequisitionEntry> list, string name)
 {
  foreach (ACR_RequisitionEntry item : list)
   if (item.m_sName == name) return item;
  return null;
 }

 // 3 RP per soldier (Game Master's AI budget is one per soldier).
 protected static int GroupCost(SCR_EntityCatalogEntry entry)
 {
  int soldiers;
  SCR_EditableGroupUIInfo info = SCR_EditableGroupUIInfo.Cast(entry.GetEntityUiInfo());
  if (info)
  {
   array<ref SCR_EntityBudgetValue> budgets = {};
   info.GetGroupBudgetCost(budgets);
   foreach (SCR_EntityBudgetValue budget : budgets)
    if (budget.GetBudgetType() == EEditableEntityBudget.AI) soldiers += budget.GetBudgetValue();
  }
  if (soldiers <= 0)
  {
   soldiers = 4;
   if (entry.HasEditableEntityLabel(EEditableEntityLabel.GROUPSIZE_SMALL)) soldiers = 2;
   if (entry.HasEditableEntityLabel(EEditableEntityLabel.GROUPSIZE_LARGE)) soldiers = 8;
  }
  return Math.Max(6, soldiers * 3);
 }

 protected static int VehicleCost(SCR_EntityCatalogEntry entry)
 {
  int cost = 15;
  if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_CAR)) cost = 12;
  if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TRUCK)) cost = 15;
  if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_APC)) cost = 45;
  if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_HELICOPTER)) cost = 60;
  if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED)) cost += 10;
  if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMOR)) cost += 15;
  return cost + 6; // crew
 }

 // Cheaper units are more plentiful.
 protected static int StockFor(int cost, int budget, int maximum)
 {
  return Math.Clamp(Math.Round(budget / cost), 1, maximum);
 }

 static void BuildGarrisonPool(string factionKey, ACR_FactionSettings settings, array<ResourceName> result)
 {
  result.Clear();
  if (settings && settings.m_aGarrisonSoldiers && !settings.m_aGarrisonSoldiers.IsEmpty())
  {
   result.Copy(settings.m_aGarrisonSoldiers);
   return;
  }
  SCR_EntityCatalog catalog = GetCatalog(factionKey, EEntityCatalogType.CHARACTER);
  if (!catalog) return;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  AddGarrisonSoldiers(factionKey, entries, true, result);
  if (result.IsEmpty()) AddGarrisonSoldiers(factionKey, entries, false, result);
  // A content pack's soldier swaps (per entry, so a varied swap gives a varied pool).
  ACR_ContentSocket socket = ACR_ContentSocket.Get();
  for (int i = 0; i < result.Count(); i++)
  {
   ResourceName swapped = socket.Soldier(factionKey, result[i]);
   if (!swapped.IsEmpty()) result[i] = swapped;
  }
 }

 protected static void AddGarrisonSoldiers(string factionKey, array<SCR_EntityCatalogEntry> entries, bool standardOnly, array<ResourceName> result)
 {
  foreach (SCR_EntityCatalogEntry entry : entries)
  {
   if (!entry.IsEnabled() || !Allowed(factionKey, EEntityCatalogType.CHARACTER, entry.GetPrefab())) continue;
   if (standardOnly && !IsStandard(factionKey, entry.GetPrefab(), EEntityCatalogType.CHARACTER)) continue;
   if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_RIFLEMAN))
   {
    // riflemen weighted double
    result.Insert(entry.GetPrefab());
    result.Insert(entry.GetPrefab());
   }
   else if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_MACHINEGUNNER) || entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_ANTITANK) || entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_GRENADIER))
    result.Insert(entry.GetPrefab());
  }
 }
}

// Buy-list pricing (Jacob, 4 Oct: Everon pays more RP than either side can spend before
// first contact). Stepped (default): ordinary units have no stock limit, but each side's
// price for a unit climbs a step with every purchase of it and never comes back down; the
// threat level sets the step (low +25%, normal +15%, high +8% of the base price). Rare units
// (aircraft, armour, mortar teams, support helicopters) keep finite stock scaled by threat
// at a fixed price. Fixed stock: the old system, every entry has threat-scaled stock.
class ACR_Pricing
{
 static const int UNLIMITED = 9999;

 // -acrPricing stepped|stock overrides the faction settings' Pricing attribute.
 static bool Stepped(ACR_FactionSettings settings)
 {
  string value;
  if (ACR_Options.Get("acrPricing", value))
  {
   value.ToLower();
   if (value == "stock") return false;
   if (value == "stepped") return true;
  }
  if (settings) return settings.m_ePricing == ACR_EPricing.STEPPED;
  return true;
 }

 static string Name(bool stepped)
 {
  if (stepped) return string.Format("stepped, +%1 percent of base per purchase", Math.Round(Step() * 100));
  return "fixed stock";
 }

 // Price step per purchase as a share of the base price: a low-threat war gets dear fastest.
 static float Step()
 {
  int threat = ACR_Manager.ThreatOption();
  if (threat == 0) return 0.25;
  if (threat == 2) return 0.08;
  return 0.15;
 }

 static void Setup(ACR_RequisitionEntry entry, bool stepped)
 {
  entry.m_iBaseCost = entry.m_iCost;
  entry.m_bStepped = stepped && !entry.m_bRare;
  if (entry.m_bStepped) entry.m_iRemaining = UNLIMITED;
  else
  {
   entry.m_iRemaining = ACR_Manager.ThreatStock(entry.m_iStock, !entry.m_sVehicle.IsEmpty());
   // Armor/support use four times base stock on High. SF keeps the ordinary
   // threat multiplier, giving only 1/2/4 teams across Low/Normal/High.
   if (entry.m_bRare && entry.m_eFormation != ACR_EFormation.SF && ACR_Manager.ThreatOption() == 2) entry.m_iRemaining *= 2;
  }
 }

 static int PriceAfter(int baseCost, int bought)
 {
  float climb = Step() * baseCost * bought;
  return baseCost + (int)Math.Round(climb);
 }

 // After a purchase (or a refund): the current price from the number bought.
 static void Reprice(ACR_RequisitionEntry entry)
 {
  if (!entry.m_bStepped) return;
  entry.m_iCost = PriceAfter(entry.m_iBaseCost, entry.m_iBought);
 }

 static bool Unlimited(int remaining)
 {
  return remaining >= UNLIMITED / 2;
 }
}
