// Insurgent side ruleset (0.8.0 #97 item 6): the role a side plays in a counter-insurgency
// match when the insurgency is commanded (by players, or by an insurgent AI commander in stage 3)
// instead of running on its own rules.
//  - Role, not faction (#244 ruling 1): FIA by default (-acrCoinInsurgent overrides), so any
//    guerrilla faction a host or mission maker names gets the same ruleset.
//  - "Insurgency run by" (-acrCoinInsurgency rules|players|ai, setup row under Match type):
//    "rules" (default) is stage 1; "players" opens the insurgent seat on the deploy screen;
//    "ai" reserves the seat for the Guerrilla AI commander (stage 3, item 11).
//  - Settings, prices and caps: no off-map supports (no artillery, air, air lift or helipads),
//    lower starting RP, tighter soldier cap (bands are few and small), own catalog without
//    aircraft or armour, mortars and mines only where the faction has its own.
//  - Income from town support (-acrCoinInsurgentIncome, default 1, 0 = base income): each town
//    pays by its support tier rather than ownership (ACR_BaseSystem.GetIncome / ACR_TownSupport).
class ACR_InsurgentSide
{
	static const int START_RP = 50;
	static const int AI_UNIT_CAP = 5;
	static const float SOLDIER_CAP_SHARE = 0.4;
	static const int SOLDIER_CAP_MIN = 16;

	protected static bool s_bApplied;
	protected static string s_sKey;

	static void Reset()
	{
		s_bApplied = false;
		s_sKey = "";
	}

	// -acrCoinInsurgency: "rules" (default), "players" or "ai".
	static string Mode()
	{
		string value = ACR_TestHooks.Get().CoinInsurgency();
		if (value.IsEmpty() && !ACR_Options.Get("acrCoinInsurgency", value)) value = "";
		value.TrimInPlace();
		value.ToLower();
		if (value == "players" || value == "ai") return value;
		return "rules";
	}

	// A commanded insurgent side is active (COIN + not running on its own rules).
	static bool On()
	{
		return ACR_Coin.On() && Mode() != "rules";
	}

	static bool PlayersAllowed()
	{
		return On() && Mode() == "players";
	}

	static bool AIControlled()
	{
		return On() && Mode() == "ai";
	}

	// The faction key holding the insurgent role (default FIA).
	static string Key()
	{
		if (!s_sKey.IsEmpty()) return s_sKey;
		string wanted = ACR_Pockets.FACTION;
		string opt;
		if (ACR_Options.Get("acrCoinInsurgent", opt))
		{
			opt.TrimInPlace();
			if (!opt.IsEmpty())
			{
				FactionManager factions = GetGame().GetFactionManager();
				if (factions && factions.GetFactionByKey(opt)) wanted = opt;
			}
		}
		return wanted;
	}

	static bool IsInsurgent(string factionKey)
	{
		return On() && !factionKey.IsEmpty() && factionKey == Key();
	}

	// Client-safe colour check: FIA is always green, and any custom insurgent key on server/host.
	static bool IsInsurgentColor(string factionKey)
	{
		if (factionKey.IsEmpty()) return false;
		if (factionKey == ACR_Pockets.FACTION) return true;
		return IsInsurgent(factionKey);
	}

	// -acrCoinInsurgentIncome: multiplier on town-support income (default 1; 0 = base income).
	static float IncomeScale()
	{
		string value;
		if (!ACR_Options.Get("acrCoinInsurgentIncome", value) || value.IsEmpty()) return 1.0;
		return Math.Max(0, value.ToFloat());
	}

	static bool UsesSupportIncome(string factionKey)
	{
		return IsInsurgent(factionKey) && ACR_TownSupport.On() && IncomeScale() > 0;
	}

	// Per-side living soldier cap: bands are few and small compared to the conventional army.
	static int SoldierCap(string factionKey, int baseCap)
	{
		if (!IsInsurgent(factionKey)) return baseCap;
		return Math.Max(SOLDIER_CAP_MIN, Math.Round(baseCap * SOLDIER_CAP_SHARE));
	}

	// Whether the insurgent faction has its own mortar tube (catalog or explicit prefab mapping).
	static bool HasOwnMortar(string factionKey)
	{
		if (!IsInsurgent(factionKey)) return true;
		ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
		if (settings && settings.m_aPrefabMappings)
		{
			foreach (ACR_PrefabMapping m : settings.m_aPrefabMappings)
				if (m && (m.m_sFrom == ACR_Mortars.TUBE_EAST || m.m_sFrom == ACR_Mortars.TUBE_WEST) && !m.m_sTo.IsEmpty()) return true;
		}
		return CatalogHasKeyword(factionKey, EEntityCatalogType.VEHICLE, "Mortar");
	}

	// Whether the insurgent faction has its own mines in its catalog.
	static bool HasOwnMines(string factionKey)
	{
		if (!IsInsurgent(factionKey)) return true;
		return CatalogHasKeyword(factionKey, EEntityCatalogType.ITEM, "Mine_");
	}

	protected static bool CatalogHasKeyword(string factionKey, EEntityCatalogType type, string keyword)
	{
		FactionManager fm = GetGame().GetFactionManager();
		if (!fm) return false;
		SCR_Faction faction = SCR_Faction.Cast(fm.GetFactionByKey(factionKey));
		if (!faction) return false;
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(type, false);
		if (!catalog) return false;
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (!entry || !entry.IsEnabled()) continue;
			string prefab = entry.GetPrefab();
			if (prefab.Contains(keyword)) return true;
		}
		return false;
	}

	// Ensure the insurgent faction has an ACR_FactionSettings entity with insurgent rules
	// (no off-map supports, own caps/starting RP, no conventional AI commander).
	static void EnsureSettings()
	{
		if (!On()) return;
		string key = Key();
		ACR_FactionSettings settings = ACR_FactionSettings.Find(key);
		if (!settings)
		{
			EntitySpawnParams parameters = new EntitySpawnParams();
			parameters.TransformMode = ETransformMode.WORLD;
			settings = ACR_FactionSettings.Cast(GetGame().SpawnEntity(ACR_FactionSettings, GetGame().GetWorld(), parameters));
			if (!settings) return;
			if (!ACR_FactionSettings.s_All.Contains(settings)) ACR_FactionSettings.s_All.Insert(settings);
			settings.m_sFaction = key;
			settings.m_iStartingRP = START_RP;
			settings.m_iAIUnitCap = AI_UNIT_CAP;
			settings.m_iAIAttacks = 1;
			settings.m_ePricing = ACR_EPricing.STEPPED;
			settings.m_bAutoFill = true;
			settings.m_fAIReactionScale = 1;
		}
		// Insurgents never get conventional off-map supports (artillery, air, air lift, helipads).
		settings.m_bSupports = false;
		// Conventional AI commander never runs the insurgent side (Guerrilla doctrine arrives in stage 3).
		settings.m_bAICommander = false;
		PrintFormat("[ACR] INSURGENT settings for %1: mode=%2, startRP=%3, supports=0, unitCap=%4", key, Mode(), settings.m_iStartingRP, settings.m_iAIUnitCap);
	}

	// Server, right after ACR_Coin.Apply placed the stronghold (ACR_Skirmish.ApplyOnce).
	static void Apply()
	{
		if (s_bApplied || !On()) return;
		s_bApplied = true;
		s_sKey = Key();
		EnsureSettings();
		if (PlayersAllowed()) EnsureSpawnPoint();
		PrintFormat("[ACR] INSURGENT side %1 active (mode %2, support income x%3)", s_sKey, Mode(), IncomeScale());
	}

	// Item 6 deploy bridge (replaced by hideout deploy points in item 7): ensure players who pick
	// the insurgent side have a spawn point at the stronghold or first insurgent base.
	static void EnsureSpawnPoint()
	{
		if (!PlayersAllowed()) return;
		string key = Key();
		if (SCR_SpawnPoint.GetSpawnPointCountForFaction(key) > 0) return;
		ACR_Base anchor = ACR_Coin.Stronghold();
		if (!anchor || anchor.m_bRetired)
		{
			foreach (ACR_Base base : ACR_Base.s_All)
				if (base && !base.m_bRetired && base.m_sOwner == key) { anchor = base; break; }
		}
		if (!anchor) return;
		SCR_SpawnPoint point = SCR_SpawnPoint.Cast(ACR_Manager.SpawnPrefab(ACR_PlayerSide.SPAWN_POINT, ACR_Manager.Ground(anchor.GetOrigin() + Vector(6, 0, 6))));
		if (point) point.SetFactionKey(key);
		PrintFormat("[ACR] INSURGENT spawn point for %1 placed at %2: %3", key, anchor.m_sName, point != null);
	}
}
