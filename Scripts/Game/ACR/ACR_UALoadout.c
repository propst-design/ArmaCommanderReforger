// Shared RU/UA/USMC profile runner; legacy UA repair (Jacob, 11 Oct live: "a lot of 80s US equipment, and no pants").
// Ukraine Armed Forces RHS re-skins the vanilla US key, but only its base soldier: every
// role prefab keeps its 80s US kit, and the base's pants prefab (Pants_UA_M14_ground) is not
// shipped, so they spawn without trousers. Each UA soldier (AI and command unit) is
// re-equipped once on the server, within a second of appearing: UA M14 jacket and pants,
// a UA helmet, TV102 vests, Faradey boots, and a weapon by role. Weapons: mostly Soviet
// pattern, about one in four Western (Jacob, 11 Oct: "mostly Soviet, some West"). Magazines
// come from each new weapon's own default magazine, so they always fit.
// Field kit (map, compass, watch, flashlight, radio, meds, MPL-50 shovel, RGD-5s, smoke) is topped
// up 3 s later, once moved items have settled. Pilots keep their kit; crews keep their helmets. Legacy repair requires the UA mod; explicit profiles select their own kit.
enum ACR_EUARole
{
 RIFLEMAN,
 LEADER,
 MG,
 AR,
 GL,
 LAT,
 MARKSMAN,
 CREW,
 PILOT,
 UNARMED,
 MG_ASSISTANT,
 AT_ASSISTANT,
 MEDIC,
 ENGINEER,
 LIGHT_AT,
 SNIPER,
 SPOTTER
}

class ACR_UALoadout
{
 static bool s_bEnabled = true;
 static float s_fWestShare = 0.25;
 static int s_iEquipped;
 static int s_iFailed;
 protected static ref set<EntityID> s_Done;
 protected static ref map<string, ref array<ResourceName>> s_Pools;
 protected static ref map<IEntity, ref array<IEntity>> s_Pending;
 static int s_iKitFailed;

 // Field kit every UA soldier carries (Jacob, 11 Oct: no maps, compasses, watches, meds, grenades
 // or shovels). The vanilla US base kit spawns into the pants, and the UA mod's pants prefab is
 // missing, so it never spawned. "path part|prefab|count": a soldier holding any item whose prefab
 // path contains the part counts toward it (old kit that moved over is kept, nothing doubles).
 static ref array<string> KIT;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_Done = new set<EntityID>();
  s_Pending = new map<IEntity, ref array<IEntity>>();
  KIT = {
  "/Maps/|{13772C903CB5E4F7}Prefabs/Items/Equipment/Maps/Map_Paper_01/PaperMap_01_folded.et|1",
  "/Compass/|{7CEF68E2BC68CE71}Prefabs/Items/Equipment/Compass/Compass_Adrianov.et|1",
  "/Watches/|{6FD6C96121905202}Prefabs/Items/Equipment/Watches/Watch_Vostok.et|1",
  "/Flashlights/|{575EA58E67448C2A}Prefabs/Items/Equipment/Flashlights/Flashlight_Soviet_01/Flashlight_Soviet_01.et|1",
  "/Radios/|{73950FBA2D7DB5C5}Prefabs/Items/Equipment/Radios/Radio_ANPRC68.et|1",
  "FieldDressing|{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et|2",
  "Tourniquet|{D70216B1B2889129}Prefabs/Items/Medicine/Tourniquet_01/Tourniquet_US_01.et|1",
  "Morphine|{0D9A5DCF89AE7AA9}Prefabs/Items/Medicine/MorphineInjection_01/MorphineInjection_01.et|1",
  "ETool_|{062E2F1D7F6739D6}Prefabs/Items/Equipment/Accessories/ETool_MPL50/ETool_MPL50_FreeRoamBuilding_Gadget.et|1",
  "/Grenades/Grenade_|{645C73791ECA1698}Prefabs/Weapons/Grenades/Grenade_RGD5.et|2",
  "/Grenades/Smoke_|{77EAE5E07DC4678A}Prefabs/Weapons/Grenades/Smoke_RDG2.et|1"
 };
 }

 static int StateCount()
 {
  return s_Done.Count() + s_Pending.Count();
 }

 // Items the soldier has whose prefab path contains part (worn gadgets and weapon slots too).
 static int CountKind(IEntity soldier, SCR_InventoryStorageManagerComponent inventory, string part)
 {
  array<IEntity> items = {};
  inventory.GetItems(items);
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
  if (weapons)
  {
   array<IEntity> held = {};
   weapons.GetWeaponsList(held);
   foreach (IEntity weapon : held)
    if (weapon && !items.Contains(weapon)) items.Insert(weapon);
  }
  int count;
  foreach (IEntity item : items)
  {
   if (!item || !item.GetPrefabData()) continue;
   string path = item.GetPrefabData().GetPrefabName();
   if (path.Contains(part)) count++;
  }
  return count;
 }

 // Tops the field kit up; returns the kinds still short (empty = complete).
 static string TopUpKit(IEntity soldier)
 {
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return " inventory";
  string missing;
  foreach (string line : KIT)
  {
   array<string> parts = {};
   line.Split("|", parts, false);
   if (parts.Count() != 3) continue;
   int wanted = parts[2].ToInt();
   int have = CountKind(soldier, inventory, parts[0]);
   for (int n = have; n < wanted; n++)
   {
    if (!SpawnCarried(inventory, parts[1]))
    {
     missing += " " + FilePath.StripExtension(FilePath.StripPath(parts[1]));
     break;
    }
   }
  }
  return missing;
 }

 static void Finish(IEntity soldier)
 {
  array<IEntity> old;
  if (s_Pending.Find(soldier, old))
  {
   foreach (IEntity cloth : old)
    if (cloth) SCR_EntityHelper.DeleteEntityAndChildren(cloth);
   s_Pending.Remove(soldier);
  }
  if (!soldier) return;
  ChimeraCharacter character = ChimeraCharacter.Cast(soldier);
  if (!character || !character.GetCharacterController() || character.GetCharacterController().IsDead()) return;
  ACR_InfantryKitComponent kit = ACR_InfantryKitComponent.Of(soldier);
  if (kit)
  {
   if (!kit.m_bAppearanceReady && kit.IsUA() && !kit.IsSF())
   {
    if (kit.m_sHelmet.IsEmpty()) kit.m_sHelmet = Pick("helmet");
    if (kit.m_sBoots.IsEmpty()) kit.m_sBoots = Pick("boots");
    SCR_InventoryStorageManagerComponent appearanceInventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
    SCR_CharacterInventoryStorageComponent appearanceLoadout = appearanceInventory.GetCharacterStorage();
    array<IEntity> discarded = {};
    bool fittedHelmet = Swap(appearanceInventory, appearanceLoadout, LoadoutHeadCoverArea, kit.m_sHelmet, discarded);
    bool fittedBoots = Swap(appearanceInventory, appearanceLoadout, LoadoutBootsArea, kit.m_sBoots, discarded);
    foreach (IEntity discardedCloth : discarded) GetGame().GetCallqueue().CallLater(DiscardCloth, 3000, false, discardedCloth);
    kit.m_bAppearanceReady = fittedHelmet && fittedBoots;
    if (!kit.m_bAppearanceReady)
    {
     kit.m_iSupplyAttempts++;
     if (kit.m_iSupplyAttempts < 80) GetGame().GetCallqueue().CallLater(Finish, 250, false, soldier);
     return;
    }
   }
   if (ACR_InfantryEquipment.Finish(soldier))
   {
    IEntity readyPrimary;
    IEntity readyLauncher;
    IEntity readySidearm;
    FindWeapons(soldier, readyPrimary, readyLauncher, readySidearm);
    CharacterControllerComponent readyController = character.GetCharacterController();
    if (readyPrimary && readyController && !readyController.IsPlayerControlled() && readyController.GetCurrentItemInHands() != readyPrimary)
     readyController.TryEquipRightHandItem(readyPrimary, EEquipItemType.EEquipTypeWeapon, false);
    kit.m_bReady = true;
    return;
   }
   kit.m_iSupplyAttempts++;
   if (kit.m_iSupplyAttempts < 80)
   {
    GetGame().GetCallqueue().CallLater(Finish, 250, false, soldier);
    return;
   }
   s_iKitFailed++;
   PrintFormat("[ACR] FORMATION KIT %1 did not finish supplying within 20 s", Leaf(soldier));
   return;
  }
  string missing = TopUpKit(soldier);
  if (!missing.IsEmpty())
  {
   s_iKitFailed++;
   PrintFormat("[ACR] UA KIT %1 short:%2", Leaf(soldier), missing);
  }
 }

 static const ResourceName JACKET = "{A9D4DA189D2FBC65}Prefabs/Characters/Uniforms/Jacket_UA_M14_ground.et";
 static const ResourceName PANTS = "{846B61FF83278E38}Prefabs/Characters/Uniforms/Pants_AU_M14_piso.et";
 static const ResourceName PLATE = "{4825CC3E487EEB3A}Prefabs/Characters/Vests/Vest_TV102/Vest_TV102_OD_UA.et";
 static const ResourceName VEST_MG = "{86D26DDE3CA3694D}Prefabs/Characters/Vests/Vest_TV102/variants/Vest_TV102_MG_OD_UA.et";
 static const ResourceName VEST_LEADER = "{47E3F46C6EEEAD7B}Prefabs/Characters/Vests/Vest_TV102/variants/Vest_TV102_Comando_UA.et";
 // Belt boxes and RPG-7 rockets don't fit the TV102 pouches: a pack for those who have none.
 static const ResourceName PACK = "{16C7AD0508F53A9B}Prefabs/Items/Equipment/Backpacks/backpack_Wartech_BB102/Backpack_Wartech_BB102_OD.et";
 static const ResourceName PACK_RPG = "{0D39750E5695B9D8}Prefabs/Items/Equipment/Backpacks/Backpack_RPG_Gunner.et";

 protected static void Add(string pool, ResourceName prefab, int weight)
 {
  array<ResourceName> list;
  if (!s_Pools.Find(pool, list))
  {
   list = {};
   s_Pools.Set(pool, list);
  }
  for (int i = 0; i < weight; i++) list.Insert(prefab);
 }

 // Weighted pools (an entry appears as often as its weight). RHS Status Quo is loaded with
 // the UA mod (its dependency); vanilla entries are the donated/older stock.
 protected static void BuildPools()
 {
  if (s_Pools) return;
  s_Pools = new map<string, ref array<ResourceName>>();
  ResourceName ak74 = "{FA5C25BF66A53DCF}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et";
  ResourceName ak74rhs = "{6408F2C84AE5DBD0}Prefabs/Weapons/Rifles/AK74/Rifle_AK74_RHSMag.et";
  ResourceName ak74n = "{6733EB7DB00E7067}Prefabs/Weapons/Rifles/AK74/Rifle_AK74N_RHSMag.et";
  ResourceName ak74m = "{FA8BE6075AFFBCA8}Prefabs/Weapons/Rifles/AK74M/Rifle_AK74M.et";
  ResourceName ak74mDot = "{1B22CB7855379872}Prefabs/Weapons/Rifles/AK74M/Rifle_AK74M_1P63.et";
  ResourceName ak74mRail = "{46D829D1995F62D4}Prefabs/Weapons/Rifles/AK74M/variants/Rifle_AK74M_b10m_b19n_npz_rail_perst_aimpoint_high.et";
  ResourceName aks74u = "{BFEA719491610A45}Prefabs/Weapons/Rifles/AKS74U/Rifle_AKS74U.et";
  ResourceName aks74un = "{3FA38ECE65392FC3}Prefabs/Weapons/Rifles/AKS74UN/Rifle_AKS74UN.et";
  ResourceName aks74unDot = "{2259092E586DD78B}Prefabs/Weapons/Rifles/AKS74UN/Rifle_AKS74UN_b18_t1.et";
  ResourceName m4 = "{14BAB2D9F6F782D8}Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_BLOCK_II.et";
  ResourceName m4Dot = "{90D8238D1D8D2DC5}Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_RAS_Aimpoint.et";
  ResourceName m16a4 = "{7E9CD221A58D3ECB}Prefabs/Weapons/Rifles/M16/Rifle_M16A4.et";
  ResourceName m16a2 = "{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et";
  ResourceName hk416 = "{353D39126E677470}Prefabs/Weapons/Rifles/HK416A5/Rifle_HK416A5_Var1.et";

  Add("rifle_east", ak74, 3); Add("rifle_east", ak74rhs, 2); Add("rifle_east", ak74n, 1);
  Add("rifle_east", ak74m, 3); Add("rifle_east", ak74mRail, 1); Add("rifle_east", aks74u, 1);
  Add("rifle_west", m4, 3); Add("rifle_west", m16a2, 2); Add("rifle_west", m16a4, 2); Add("rifle_west", hk416, 1);

  Add("leader_east", ak74mDot, 2); Add("leader_east", ak74mRail, 2); Add("leader_east", ak74m, 1);
  Add("leader_west", m4Dot, 2); Add("leader_west", hk416, 1);

  Add("gl_east", "{63E8322E2ADD4AA7}Prefabs/Weapons/Rifles/AK74/Rifle_AK74_GP25.et", 2);
  Add("gl_east", "{F9DF0AA717A9A9A1}Prefabs/Weapons/Rifles/AK74M/Rifle_AK74M_GP25.et", 2);
  Add("gl_west", "{00D6DCF68C664480}Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_M203_BLOCK_II.et", 1);
  Add("gl_west", "{5A987A8A13763769}Prefabs/Weapons/Rifles/M16/Rifle_M16A2_M203.et", 1);

  Add("mg_east", "{A89BC9D55FFB4CD8}Prefabs/Weapons/MachineGuns/PKM/MG_PKM.et", 3);
  Add("mg_east", "{5D6226C0B4631749}Prefabs/Weapons/MachineGuns/PKM/MG_PKM_B51.et", 1);
  Add("mg_east", "{559290E2A7D633FE}Prefabs/Weapons/MachineGuns/PKM/MG_PKP_B51.et", 1);
  Add("mg_west", "{A87B0A22462E20B1}Prefabs/Weapons/MachineGuns/M240/MG_M240.et", 1);

  Add("ar_east", "{A7AF84C6C58BA3E8}Prefabs/Weapons/MachineGuns/RPK74/MG_RPK74.et", 2);
  Add("ar_east", "{4EF6890D756306D1}Prefabs/Weapons/MachineGuns/RPK74M/MG_RPK74M.et", 2);
  Add("ar_east", "{591519C23638AFD0}Prefabs/Weapons/MachineGuns/RPK74N/MG_RPK74N.et", 1);
  Add("ar_west", "{D2B48DEBEF38D7D7}Prefabs/Weapons/MachineGuns/M249/MG_M249.et", 1);

  Add("marksman_east", "{6415B7923DE28C1B}Prefabs/Weapons/Rifles/SVD/Rifle_SVD_PSO.et", 2);
  Add("marksman_east", "{C828F6D563E1A9BE}Prefabs/Weapons/Rifles/SVD/Rifle_SVD_PSO_7N14.et", 1);
  Add("marksman_west", "{B31929F65F0D0279}Prefabs/Weapons/Rifles/M14/Rifle_M21.et", 1);

  Add("crew_east", aks74u, 2); Add("crew_east", aks74un, 2); Add("crew_east", aks74unDot, 1);
  Add("crew_west", m4, 1);

  // Launchers: RPG-7 alongside donated disposables (M72, RPG-75) and RPG-22.
  Add("lat_east", "{7A82FE978603F137}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7.et", 2);
  Add("lat_east", "{D6BC90F8AE0AB8EF}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7_pgo7_rhs.et", 1);
  Add("lat_east", "{722CE6FEC39EE896}Prefabs/Weapons/Launchers/RPG22/Launcher_RPG22.et", 1);
  Add("lat_west", "{9C5C20FB0E01E64F}Prefabs/Weapons/Launchers/M72/Launcher_M72A3.et", 2);
  Add("lat_west", "{7C45EC94C698246B}Prefabs/Weapons/Launchers/RPG75/Launcher_RPG75.et", 1);

  // UA infantry: M14, suitable woodland/multicam and plain shells. No Russian camouflage,
  // desert patterns, Russian flag patches, old US woodland covers or flight helmets.
  // Audit inherited cover/patch slots too: default 6B47 variants carry EMR covers.
  Add("helmet", "{EED31918A22454F9}Prefabs/Characters/HeadGear/Helmet_ECH/variants/ECH_M14_ua.et", 3);
  Add("helmet", "{C8E2AC206E2F5665}Prefabs/Characters/HeadGear/Helmet_ECH/Helmet_ECH_hc_v_r_p_cmc.et", 1);
  // Donated/captured designs are allowed only in the approved covers/markings above.
  Add("helmet", "{F4B075722D275962}Prefabs/Characters/HeadGear/Helmet_6B47/Helmet_6B47_no_cover_balaclava.et", 1);
  Add("helmet", "{58A12E4317DFAA23}Prefabs/Characters/HeadGear/Helmet_6B7/Helmet_6B7.et", 1);
  Add("helmet", "{4298D95C60998247}Prefabs/Characters/HeadGear/Helmet_Caiman/Helmet_Caiman_Coy.et", 1);
  Add("helmet", "{33915036D0448C92}Prefabs/Characters/HeadGear/Helmet_ECH/Helmet_ECH_hc_v_r.et", 1);
  Add("helmet", "{B47A0A12801CEBD3}Prefabs/Characters/HeadGear/Helmet_ECH/Helmet_ECH_lc_p.et", 1);
  Add("helmet", "{54A7E4E38371553E}Prefabs/Characters/HeadGear/Helmet_Exfil/Helmet_Exfil_Blk.et", 1);
  Add("helmet", "{C9422C231B245E28}Prefabs/Characters/HeadGear/Helmet_Exfil/Variants/ION_MC_BLK/Helmet_Exfil_Blk_ION2.et", 1);
  Add("helmet", "{7DBE7102DE69CF5A}Prefabs/Characters/HeadGear/Helmet_Exfil/Variants/ION_MC_BLK/Helmet_Exfil_Blk_ION7.et", 1);
  Add("helmet", "{3F9761C87346CE69}Prefabs/Characters/HeadGear/Helmet_Exfil/Variants/ION_Parka/Helmet_Exfil_CoyBlk_ION4.et", 1);
  Add("helmet", "{C366832EA8199448}Prefabs/Characters/HeadGear/Helmet_Exfil/Variants/ION_Shirts/Helmet_Exfil_Coy_ION.et", 1);
  Add("helmet", "{D6C2E291F59E8985}Prefabs/Characters/HeadGear/Helmet_Kiver_RSP/Helmet_Kiver_RSP.et", 1);
  Add("helmet", "{049BE57E35FAB43B}Prefabs/Characters/HeadGear/Helmet_Kiver_RSP/Variants/Helmet_Kiver_RSP_VKPO3.0_2.et", 1);
  Add("helmet", "{D1D8BCC152EFDD92}Prefabs/Characters/HeadGear/Helmet_Kiver_RSP/Variants/Helmet_Kiver_RSP_VKPO3.0_3.et", 1);
  Add("helmet", "{13FF459E30D393E8}Prefabs/Characters/HeadGear/Helmet_LShZ/variants/Vkpo3.0/Helmet_LShZ_new_OD_HC_Peltor_VKPO3.et", 1);
  Add("helmet", "{5C931FC991288614}Prefabs/Characters/HeadGear/Helmet_LWH/Helmet_LWH_Shroud_aor2.et", 1);
  Add("helmet", "{1285B7FAD09DC0D3}Prefabs/Characters/HeadGear/Helmet_LWH/Helmet_LWH_Shroud_marpatWoodland.et", 1);
  Add("helmet", "{2DC5B750CBAA9337}Prefabs/Characters/HeadGear/Helmet_MICH_ACH/Helmet_MICH2000_norotos_velcro_green2.et", 1);
  Add("helmet", "{B0F22FBCFD3009E5}Prefabs/Characters/HeadGear/Helmet_MICH_ACH/Variants/FORECON/Helmet_MICH2000_norotos_rail_coyote_FORECON1.et", 1);
  Add("helmet", "{0E48CBE42D2FED92}Prefabs/Characters/HeadGear/Helmet_MICH_ACH/Variants/FORECON/Helmet_MICH2000_norotos_rail_tan_FORECON6.et", 1);
  Add("helmet", "{9070BE2FDD5F31DC}Prefabs/Characters/HeadGear/Helmet_OPSCORE/Helmet_OPSCORE_SF_BLK.et", 1);
  Add("helmet", "{A36613EAD646961D}Prefabs/Characters/HeadGear/Helmet_OPSCORE/Helmet_OPSCORE_XP_MC_AMP.et", 1);
  Add("helmet", "{60F173ECECE82BE8}Prefabs/Characters/HeadGear/Helmet_OPSCORE/variants/Helmet_OPSCORE_MT_Peltor_Marsoc1.et", 1);
  Add("helmet", "{35BB2A86100E6B3A}Prefabs/Characters/HeadGear/Helmet_OPSCORE/variants/ION/Helmet_OPSCORE_MID_GRAY_AMP_ION3.et", 1);
  Add("helmet", "{10E223B05E8874EA}Prefabs/Characters/HeadGear/Helmet_Spartan3/Helmet_Spartan3_FG.et", 1);
  Add("helmet", "{4A5D91D707E3852B}Prefabs/Characters/HeadGear/Helmet_Spartan3/Helmet_Spartan3_MC_Peltor.et", 1);
  Add("helmet", "{522497DBEF2795E6}Prefabs/Characters/HeadGear/Helmet_Spartan3/Helmet_Spartan3_OD_Sordin.et", 1);
  Add("helmet", "{4C988BFF658AA819}Prefabs/Characters/HeadGear/Helmet_TBH3A/Helmet_TBH3A_HC.et", 1);
  Add("helmet", "{CD67023CCC11EBCF}Prefabs/Characters/HeadGear/Helmet_TBH3A/Helmet_TBH3A_HC_Peltor_Blk.et", 1);
  Add("helmet", "{4587DD4547DB33A1}Prefabs/Characters/HeadGear/Helmet_TBH3A/Helmet_TBH3A_LC_PMC.et", 1);
  Add("helmet", "{5F5D712A4700FC45}Prefabs/Characters/HeadGear/Helmet_TBH3A/variants/ION/Helmet_TBH3A_HC_Peltor_Blk_ION.et", 1);
  Add("helmet", "{08D8E1E034A40C8F}Prefabs/Characters/HeadGear/Helmet_TBH3A/variants/ION/Helmet_TBH3A_HC_Peltor_Tan_ION2.et", 1);
  Add("helmet", "{3F70EA42E26BA189}Prefabs/Characters/HeadGear/Helmet_TOR/Helmet_TOR_Large.et", 1);
  Add("helmet", "{BFB15A3CBE9188BA}Prefabs/Characters/HeadGear/Helmet_TOR/variants/Helmet_TOR_medium_amp_arsenal.et", 1);
  Add("helmet", "{364FF1A1CA2CA32E}Prefabs/Characters/HeadGear/Helmet_TOR2/Helmet_TOR2L_blk.et", 1);
  Add("helmet", "{1CB7F72BBD5E2972}Prefabs/Characters/HeadGear/Helmet_TOR2/Helmet_TOR2M_AMP_olive.et", 1);
  Add("helmet", "{29C9A2C915989EFE}Prefabs/Characters/HeadGear/Helmet_TOR2/Helmet_TOR2M_olive.et", 1);
  Add("helmet", "{6A0D5DB25591CE40}Prefabs/Characters/HeadGear/Helmet_TOR2/variants/Helmet_TOR2L_blk_arsenal.et", 1);
  Add("helmet", "{C79B2ECE26646C64}Prefabs/Characters/HeadGear/Helmet_TOR2/variants/Helmet_TOR2L_olive_arsenal.et", 1);
  Add("helmet", "{A7E6D7ECD5F684D7}Prefabs/Characters/HeadGear/Helmet_SSh68_01/Helmet_SSh68_01.et", 1);
  Add("helmet", "{66196D85AB93D2BE}Prefabs/Characters/HeadGear/Helmet_SSh68_01/Helmet_SSh68_01_camo.et", 1);
  Add("helmet", "{22963D69CA50EB9E}Prefabs/Characters/HeadGear/Helmet_SSh68_01/Helmet_SSh68_01_net.et", 1);

  // Mixed practical combat boots; pilot flight kit is retained.
  Add("boots", "{64B94D8A4D92A4BD}Prefabs/Characters/Footwear/Boots_Altama/Footwear_Altama.et", 1);
  Add("boots", "{934C7275AC865817}Prefabs/Characters/Footwear/Boots_BCT/Footwear_BCT.et", 1);
  Add("boots", "{AB9E3D5524876346}Prefabs/Characters/Footwear/Boots_Faradey_1085/Footware_Faradey_1085.et", 1);
  Add("boots", "{7C828AB80ED1D9B0}Prefabs/Characters/Footwear/Boots_Faradey_906/Footware_Faradey_906.et", 1);
  Add("boots", "{24F6795DC36C79D7}Prefabs/Characters/Footwear/Boots_Luma/Footwear_Luma.et", 1);
  Add("boots", "{21B331B5FA807997}Prefabs/Characters/Footwear/Boots_Rocky_SV2/Footwear_Rocky_SV2.et", 1);
  Add("boots", "{057E63449502EBCA}Prefabs/Characters/Footwear/Boots_Salomon/Footwear_Salomon.et", 1);
  Add("boots", "{320FA1CD5B1D5601}Prefabs/Characters/Footwear/Boots_USMC_Combat/Footwear_USMC_Combat.et", 1);
  Add("boots", "{660952F979E8EBF2}Prefabs/Characters/Footwear/Boots_solomon_ultra_x/Footwear_Altama_Boots.et", 1);

  Add("vest", "{74F6035104400897}Prefabs/Characters/Vests/Vest_TV102/variants/Vest_TV102_rifleman_OD_UA.et", 2);
  Add("vest", "{E383053302D67DCA}Prefabs/Characters/Vests/Vest_TV102/variants/Vest_TV102_Basico_UA.et", 1);
 }

 static ResourceName Pick(string pool)
 {
  BuildPools();
  array<ResourceName> list;
  if (!s_Pools.Find(pool, list) || list.IsEmpty()) return "";
  return list[Math.RandomInt(0, list.Count())];
 }

 // East or west pool for a role ("rifle" -> rifle_east / rifle_west).
 static ResourceName PickSide(string role)
 {
  if (Math.RandomFloat01() < s_fWestShare) return Pick(role + "_west");
  return Pick(role + "_east");
 }

 static void HelmetPool(notnull array<ResourceName> helmets) { ClothingPool("helmet", helmets); }
 static void BootPool(notnull array<ResourceName> boots) { ClothingPool("boots", boots); }

 protected static void ClothingPool(string pool, notnull array<ResourceName> helmets)
 {
  helmets.Clear();
  BuildPools();
  ref array<ResourceName> entries;
  if (!s_Pools.Find(pool, entries)) return;
  foreach (ResourceName entry : entries)
   if (!helmets.Contains(entry)) helmets.Insert(entry);
 }

 static bool Active()
 {
  return s_bEnabled && Replication.IsServer() && ACR_FactionSelection.UkraineModLoaded();
 }

 static bool IsUA(IEntity soldier)
 {
  return ACR_EntityUtil.FactionKeyOf(soldier) == "US";
 }

 // Called every second for every AI soldier and command unit: equips each UA soldier once.
 static void Check(IEntity soldier)
 {
  if (!soldier || !Replication.IsServer()) return;
  ACR_InfantryKitComponent kit = ACR_InfantryKitComponent.Of(soldier);
  if (!kit && !Active()) return;
  if (kit && kit.m_bEquipped) return;
  if (!kit && s_Done.Contains(soldier.GetID())) return;
  ChimeraCharacter character = ChimeraCharacter.Cast(soldier);
  if (!character || (!kit && !IsUA(soldier))) return;
  CharacterControllerComponent controller = character.GetCharacterController();
  if (!controller || controller.IsDead()) return;
  if (kit) kit.m_bEquipped = true;
  else s_Done.Insert(soldier.GetID());
  if (Equip(soldier)) s_iEquipped++;
  else s_iFailed++;
 }

 static void DiscardCloth(IEntity cloth)
 {
  if (cloth) SCR_EntityHelper.DeleteEntityAndChildren(cloth);
 }

 static void Reset()
 {
  if (GetGame())
  {
   GetGame().GetCallqueue().Remove(Finish);
   GetGame().GetCallqueue().Remove(DiscardCloth);
  }
  s_Done.Clear();
  s_Pending.Clear();
 }

 static bool WasEquipped(IEntity soldier)
 {
  ACR_InfantryKitComponent kit = ACR_InfantryKitComponent.Of(soldier);
  if (kit) return kit.m_bEquipped;
  return soldier && s_Done.Contains(soldier.GetID());
 }

 static bool Settled(IEntity soldier)
 {
  ACR_InfantryKitComponent kit = ACR_InfantryKitComponent.Of(soldier);
  if (kit) return kit.m_bReady;
  return WasEquipped(soldier) && !s_Pending.Contains(soldier);
 }

 static string Leaf(IEntity entity)
 {
  if (!entity || !entity.GetPrefabData()) return "";
  return FilePath.StripExtension(FilePath.StripPath(entity.GetPrefabData().GetPrefabName()));
 }

 static ACR_EUARole RoleOf(string leaf, IEntity primary, IEntity launcher)
 {
  if (leaf.Contains("HeliPilot")) return ACR_EUARole.PILOT;
  if (leaf.EndsWith("_Crew") || leaf.Contains("HeliCrew")) return ACR_EUARole.CREW;
  if (leaf.Contains("Unarmed")) return ACR_EUARole.UNARMED;
  if (leaf.EndsWith("_SL") || leaf.EndsWith("_TL") || leaf.EndsWith("_PL") || leaf.EndsWith("_CC") || leaf.Contains("Officer") || leaf.Contains("Sergeant")) return ACR_EUARole.LEADER;
  if (launcher) return ACR_EUARole.LAT;
  if (!primary) return ACR_EUARole.RIFLEMAN;
  string weapon = Leaf(primary);
  BaseWeaponComponent component = BaseWeaponComponent.Cast(primary.FindComponent(BaseWeaponComponent));
  if (component && component.GetWeaponType() == EWeaponType.WT_MACHINEGUN)
  {
   if (weapon.Contains("M249")) return ACR_EUARole.AR;
   return ACR_EUARole.MG;
  }
  if ((component && component.GetWeaponType() == EWeaponType.WT_SNIPERRIFLE) || weapon.Contains("M21") || leaf.Contains("Sniper")) return ACR_EUARole.MARKSMAN;
  if (component)
  {
   array<BaseMuzzleComponent> muzzles = {};
   component.GetMuzzlesList(muzzles);
   if (muzzles.Count() > 1) return ACR_EUARole.GL;
  }
  return ACR_EUARole.RIFLEMAN;
 }

 static ResourceName PrimaryFor(ACR_EUARole role)
 {
  if (role == ACR_EUARole.LEADER) return PickSide("leader");
  if (role == ACR_EUARole.MG) return PickSide("mg");
  if (role == ACR_EUARole.AR) return PickSide("ar");
  if (role == ACR_EUARole.GL) return PickSide("gl");
  if (role == ACR_EUARole.MARKSMAN) return PickSide("marksman");
  if (role == ACR_EUARole.CREW) return PickSide("crew");
  if (role == ACR_EUARole.PILOT || role == ACR_EUARole.UNARMED) return "";
  return PickSide("rifle");
 }

 // Magazines carried for the main muzzle (one more is in the weapon).
 static int SpareMagazines(ACR_EUARole role, string weapon)
 {
  if (role == ACR_EUARole.MG) return 2;
  if (role == ACR_EUARole.AR)
  {
   if (weapon.Contains("M249")) return 2;
   return 5;
  }
  if (role == ACR_EUARole.CREW || role == ACR_EUARole.PILOT) return 3;
  if (role == ACR_EUARole.MARKSMAN) return 5;
  return 6;
 }

 protected static void FindWeapons(IEntity soldier, out IEntity primary, out IEntity launcher, out IEntity sidearm)
 {
  primary = null;
  launcher = null;
  sidearm = null;
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
  if (!weapons) return;
  array<IEntity> list = {};
  weapons.GetWeaponsList(list);
  foreach (IEntity weaponEntity : list)
  {
   if (!weaponEntity) continue;
   BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
   if (!weapon) continue;
   EWeaponType type = weapon.GetWeaponType();
   if (type == EWeaponType.WT_ROCKETLAUNCHER) { if (!launcher) launcher = weaponEntity; }
   else if (type == EWeaponType.WT_HANDGUN) { if (!sidearm) sidearm = weaponEntity; }
   else if (type == EWeaponType.WT_RIFLE || type == EWeaponType.WT_MACHINEGUN || type == EWeaponType.WT_SNIPERRIFLE || type == EWeaponType.WT_GRENADELAUNCHER)
   {
    if (!primary) primary = weaponEntity;
   }
  }
 }

 // Take the worn item off one loadout area (its contents stay inside it) and put the new one on.
 protected static bool Swap(SCR_InventoryStorageManagerComponent inventory, SCR_CharacterInventoryStorageComponent loadout, typename area, ResourceName prefab, notnull array<IEntity> removed)
 {
  if (prefab.IsEmpty()) return false;
  IEntity worn = loadout.GetClothFromArea(area);
  if (worn)
  {
   if (worn.GetPrefabData() && worn.GetPrefabData().GetPrefabName() == prefab) return true;
   if (!inventory.TryRemoveItemFromStorage(worn, loadout)) return false;
  }
  if (inventory.TrySpawnPrefabToStorage(prefab, loadout))
  {
   if (worn) removed.Insert(worn);
   return true;
  }
  // The new item would not go on: put the old one back rather than leave the slot bare.
  if (worn && !inventory.TryInsertItemInStorage(worn, loadout)) removed.Insert(worn);
  return false;
 }

 // Into any storage that takes it (belt boxes may only fit a pack).
 static bool SpawnCarried(SCR_InventoryStorageManagerComponent inventory, ResourceName prefab)
 {
  // Worn equipment slots first (wrist watch), so a watch is worn rather than pocketed.
  array<BaseInventoryStorageComponent> storages = {};
  inventory.GetStorages(storages);
  foreach (BaseInventoryStorageComponent slotStorage : storages)
   if (SCR_EquipmentStorageComponent.Cast(slotStorage) && inventory.CanInsertResourceInStorage(prefab, slotStorage) && inventory.TrySpawnPrefabToStorage(prefab, slotStorage)) return true;
  if (inventory.TrySpawnPrefabToStorage(prefab)) return true;
  foreach (BaseInventoryStorageComponent storage : storages)
   if (storage && inventory.TrySpawnPrefabToStorage(prefab, storage)) return true;
  return false;
 }

 static bool IsAmmo(IEntity item)
 {
  if (!item) return false;
  if (item.FindComponent(BaseMagazineComponent)) return true;
  if (!item.GetPrefabData()) return false;
  string path = item.GetPrefabData().GetPrefabName();
  return path.Contains("/Weapons/Ammo/") || path.Contains("/Weapons/Magazines/") || path.Contains("/Weapons/Warheads/");
 }

 // Default magazines (or projectiles) of every reloadable muzzle the soldier now carries.
 static void Ammunition(IEntity soldier, notnull array<ResourceName> magazines, notnull array<BaseMuzzleComponent> muzzlesOut)
 {
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
  if (!weapons) return;
  array<IEntity> list = {};
  weapons.GetWeaponsList(list);
  foreach (IEntity weaponEntity : list)
  {
   if (!weaponEntity) continue;
   BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
   if (!weapon) continue;
   array<BaseMuzzleComponent> muzzles = {};
   weapon.GetMuzzlesList(muzzles);
   foreach (BaseMuzzleComponent muzzle : muzzles)
   {
    SCR_MuzzleInMagComponent inMag = SCR_MuzzleInMagComponent.Cast(muzzle);
    if (inMag && !inMag.CanBeReloaded()) continue;
    ResourceName magazine = muzzle.GetDefaultMagazineOrProjectileName();
    if (magazine.IsEmpty()) continue;
    magazines.Insert(magazine);
    muzzlesOut.Insert(muzzle);
   }
  }
 }

 static int CountPrefab(SCR_InventoryStorageManagerComponent inventory, ResourceName prefab)
 {
  array<IEntity> items = {};
  inventory.GetItems(items);
  int count;
  foreach (IEntity item : items)
   if (item && item.GetPrefabData() && item.GetPrefabData().GetPrefabName() == prefab) count++;
  return count;
 }

 static bool Equip(IEntity soldier)
 {
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return false;
  SCR_CharacterInventoryStorageComponent loadout = inventory.GetCharacterStorage();
  if (!loadout) return false;
  string leaf = Leaf(soldier);
  IEntity primary;
  IEntity launcher;
  IEntity sidearm;
  FindWeapons(soldier, primary, launcher, sidearm);
  ACR_InfantryKitComponent kit = ACR_InfantryKitComponent.Of(soldier);
  ACR_EUARole role = RoleOf(leaf, primary, launcher);
  if (kit) role = kit.m_eRole;

  // Clothes. Items in the old clothes are moved over afterwards (old ammo is dropped).
  array<IEntity> removed = {};
  string clothFails;
  if (role != ACR_EUARole.PILOT && !kit)
  {
   if (!Swap(inventory, loadout, LoadoutJacketArea, JACKET, removed)) clothFails += " jacket";
   if (!Swap(inventory, loadout, LoadoutPantsArea, PANTS, removed)) clothFails += " pants";
   if (!Swap(inventory, loadout, LoadoutBootsArea, Pick("boots"), removed)) clothFails += " boots";
   ResourceName helmet = Pick("helmet");
   if (role != ACR_EUARole.CREW && !Swap(inventory, loadout, LoadoutHeadCoverArea, helmet, removed)) clothFails += " helmet:" + FilePath.StripPath(helmet);
   if (!Swap(inventory, loadout, LoadoutArmoredVestSlotArea, PLATE, removed)) clothFails += " plate";
   ResourceName vest = Pick("vest");
   if (role == ACR_EUARole.MG || role == ACR_EUARole.AR) vest = VEST_MG;
   else if (role == ACR_EUARole.LEADER) vest = VEST_LEADER;
   if (!Swap(inventory, loadout, LoadoutVestArea, vest, removed)) clothFails += " vest:" + FilePath.StripPath(vest);
  }

  // Most curated weapons spawn natively in the character template. Only the
  // ordinary UA mixture changes rifles; fixed roles never draw another capability.
  ResourceName newPrimary = PrimaryFor(role);
  if (kit) newPrimary = ACR_InfantryEquipment.Primary(kit);
  bool primaryOk = true;
  if (!newPrimary.IsEmpty() && (!primary || !primary.GetPrefabData() || primary.GetPrefabData().GetPrefabName() != newPrimary))
  {
   if (primary) inventory.TryDeleteItem(primary);
   primaryOk = inventory.TrySpawnPrefabToStorage(newPrimary, null, -1, EStoragePurpose.PURPOSE_WEAPON_PROXY);
  }
  ResourceName newLauncher;
  if (role == ACR_EUARole.LAT || role == ACR_EUARole.LIGHT_AT)
  {
   newLauncher = PickSide("lat");
   if (kit) newLauncher = ACR_InfantryEquipment.Launcher(kit);
   if (!launcher || !launcher.GetPrefabData() || launcher.GetPrefabData().GetPrefabName() != newLauncher)
   {
    if (launcher) inventory.TryDeleteItem(launcher);
    if (!inventory.TrySpawnPrefabToStorage(newLauncher, null, -1, EStoragePurpose.PURPOSE_WEAPON_PROXY)) primaryOk = false;
   }
  }

  // A pack for belt boxes and rockets, only where the back is empty (medic/radio packs stay).
  if (!loadout.GetClothFromArea(LoadoutBackpackArea))
  {
   ResourceName pack;
   if (role == ACR_EUARole.MG || role == ACR_EUARole.AR || (kit && (kit.IsSF() || role == ACR_EUARole.MG_ASSISTANT || role == ACR_EUARole.AT_ASSISTANT || role == ACR_EUARole.MEDIC))) pack = PACK;
   else if (newLauncher.Contains("RPG7")) pack = PACK_RPG;
   if (!pack.IsEmpty() && !inventory.TrySpawnPrefabToStorage(pack, loadout)) clothFails += " pack";
  }

  // Ammunition that fits what is carried now; everything else that was in the old clothes moves over.
  array<ResourceName> keep = {};
  array<BaseMuzzleComponent> muzzles = {};
  Ammunition(soldier, keep, muzzles);
  int moved;
  foreach (IEntity old : removed)
  {
   BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(old.FindComponent(BaseInventoryStorageComponent));
   if (!storage) continue;
   array<IEntity> inside = {};
   storage.GetAll(inside);
   foreach (IEntity item : inside)
   {
    if (!item || (IsAmmo(item) && (!item.GetPrefabData() || !keep.Contains(item.GetPrefabData().GetPrefabName())))) continue;
    BaseInventoryStorageComponent target = inventory.FindStorageForItem(item);
    if (target && inventory.TryMoveItemToStorage(item, target)) moved++;
   }
  }
  // Moves finish after this call: the old clothes go, and the field kit is topped up, 3 s later.
  array<IEntity> pending = {};
  pending.InsertAll(removed);
  s_Pending.Set(soldier, pending);
  GetGame().GetCallqueue().CallLater(Finish, 3000, false, soldier);
  // Loose old ammunition still in the inventory (backpacks, character storage).
  array<IEntity> items = {};
  inventory.GetItems(items);
  foreach (IEntity item : items)
   if (IsAmmo(item) && item.GetPrefabData() && !keep.Contains(item.GetPrefabData().GetPrefabName())) inventory.TryDeleteItem(item);

  IEntity nowPrimary;
  IEntity nowLauncher;
  IEntity nowSidearm;
  FindWeapons(soldier, nowPrimary, nowLauncher, nowSidearm);
  string weaponLeaf = Leaf(nowPrimary);
  int spawned;
  foreach (int i, ResourceName magazine : keep)
  {
   if (kit) continue; // curated kits supply only confirmed cargo after native spawn settles
   // Spares on top of the loaded one: UGL grenades 6, RPG-7 rockets 1, sidearm 2.
   int wanted = 2;
   IEntity owner = muzzles[i].GetOwner();
   if (muzzles[i].GetMuzzleType() == EMuzzleType.MT_UGLMuzzle) wanted = 6;
   else if (owner == nowPrimary) wanted = SpareMagazines(role, weaponLeaf);
   else if (owner == nowLauncher) wanted = 1;
   int have = CountPrefab(inventory, magazine);
   for (int n = have; n < wanted; n++)
   {
    if (!SpawnCarried(inventory, magazine))
    {
     clothFails += " ammo:" + FilePath.StripPath(magazine);
     break;
    }
    spawned++;
   }
  }

  CharacterControllerComponent controller = ChimeraCharacter.Cast(soldier).GetCharacterController();
  bool equipStarted;
  if (!kit && controller && nowPrimary) equipStarted = controller.TryEquipRightHandItem(nowPrimary, EEquipItemType.EEquipTypeWeapon, false);
  if (System.IsCLIParam("acrUAHandsTest")) PrintFormat("[ACR] UA HANDS initial %1 primary=%2 accepted=%3 changing=%4 hands=%5", leaf, weaponLeaf, equipStarted, controller && controller.IsChangingItem(), ACR_UALoadout.Leaf(controller.GetCurrentItemInHands()));

  PrintFormat("[ACR] UA LOADOUT %1 role=%2 weapon=%3 launcher=%4 mags+%5 moved=%6 failed:%7", leaf, typename.EnumToString(ACR_EUARole, role), weaponLeaf, Leaf(nowLauncher), spawned, moved, clothFails);
  return clothFails.IsEmpty() && primaryOk;
 }
}
