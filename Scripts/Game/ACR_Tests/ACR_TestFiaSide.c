// srv:fiaside (Everon whole map, about 40 s): insurgent side ruleset (ACR_InsurgentSide, 0.8.0 #97 item 6).
// COIN with -acrCoinInsurgency players: the absent army stays gone, both the army and the insurgent
// side are playable, the insurgent side has its own settings/caps (no supports, no aircraft/armour/
// observer/helipad entries, tighter soldier cap), map colour polarity gives insurgents green for own
// bases, and insurgent income comes from town support tiers rather than base ownership.
class ACR_TestFiaSide
{
	static void Run(int second)
	{
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager) return;
		if (second == 10) Verify(manager);
	}

	protected static void Verify(ACR_Manager manager)
	{
		string army = ACR_Coin.Army();
		string absent = ACR_OneSide.Absent();
		string ins = ACR_InsurgentSide.Key();

		Check("fiaside: COIN and insurgent side active, players allowed",
			ACR_Coin.On() && ACR_InsurgentSide.On() && ACR_InsurgentSide.PlayersAllowed() && ins == ACR_Pockets.FACTION && ACR_InsurgentSide.IsInsurgent(ins),
			string.Format("mode '%1', key '%2', army '%3'", ACR_InsurgentSide.Mode(), ins, army));

		// Playable sides: army and insurgent open; absent conventional army closed and forced side empty.
		SCR_Faction armyF = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(army));
		SCR_Faction insF = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(ins));
		SCR_Faction absF = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(absent));
		int insSpawns = SCR_SpawnPoint.GetSpawnPointCountForFaction(ins);
		Check("fiaside: army and insurgent playable with spawn point; absent army closed",
			ACR_PlayerSide.Key().IsEmpty() && armyF && armyF.IsPlayable() && insF && insF.IsPlayable() && absF && !absF.IsPlayable() && insSpawns > 0,
			string.Format("forced '%1', insSpawns %2", ACR_PlayerSide.Key(), insSpawns));

		// Settings & caps: no supports, no conventional AI commander, tighter soldier cap.
		ACR_FactionSettings insSettings = ACR_FactionSettings.Find(ins);
		int armyCap = ACR_InsurgentSide.SoldierCap(army, manager.m_iSoldierCap);
		int insCap = ACR_InsurgentSide.SoldierCap(ins, manager.m_iSoldierCap);
		Check("fiaside: insurgent settings have no supports, no conventional AI, tighter soldier cap",
			insSettings && !insSettings.m_bSupports && !insSettings.m_bAICommander && insCap < armyCap && insCap >= ACR_InsurgentSide.SOLDIER_CAP_MIN,
			string.Format("supports %1, ai %2, insCap %3 vs armyCap %4", insSettings && insSettings.m_bSupports, insSettings && insSettings.m_bAICommander, insCap, armyCap));

		// Catalog & supports: no off-map supports, no aircraft, no observer team, no helipads.
		ACR_FactionData data = manager.m_Bases.GetFaction(ins);
		int badEntries;
		foreach (ACR_RequisitionEntry entry : data.m_aRequisitions)
		{
			if (!entry) continue;
			if (entry.m_bAircraft || entry.m_bObserverTeam || entry.m_iSupportHeli > 0) badEntries++;
		}
		Check("fiaside: insurgent catalog has units, no supports/aircraft/observer/helipads",
			data.m_aRequisitions.Count() > 0 && data.m_aSupports.IsEmpty() && badEntries == 0 && !ACR_HeliStation.UsesPads(ins),
			string.Format("reqs %1, supports %2, bad %3", data.m_aRequisitions.Count(), data.m_aSupports.Count(), badEntries));

		// Map colours: insurgent viewer sees own bases in green (OTHER colour), army in red.
		Color insOwn = ACR_MapDeclutter.SideColorFor(ACR_EBaseSide.OWN, ins);
		Color armyOwn = ACR_MapDeclutter.SideColorFor(ACR_EBaseSide.OWN, army);
		Color green = ACR_MapDeclutter.SideColor(ACR_EBaseSide.OTHER);
		Check("fiaside: insurgent map shows own side in green, army in blue",
			insOwn.PackToInt() == green.PackToInt() && armyOwn.PackToInt() != green.PackToInt(),
			string.Format("insOwn %1, green %2, armyOwn %3", insOwn.PackToInt(), green.PackToInt(), armyOwn.PackToInt()));

		// Income from town support: Loyal pays 0, Hostile pays full, owning a base adds nothing extra.
		ACR_Base town;
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (ACR_TownSupport.IsTown(base) && base.m_iValue > 0) { town = base; break; }
		}
		bool incomeOk = false;
		string incomeInfo = "no town";
		if (town)
		{
			int savedTier = town.m_iSupportTier;
			string savedOwner = town.m_sOwner;
			town.m_iSupportTier = ACR_TownSupport.LOYAL;
			int incLoyal = manager.m_Bases.GetIncome(ins);
			town.m_iSupportTier = ACR_TownSupport.HOSTILE;
			int incHostile = manager.m_Bases.GetIncome(ins);
			town.m_sOwner = ins;
			int incOwned = manager.m_Bases.GetIncome(ins);
			town.m_iSupportTier = savedTier;
			town.m_sOwner = savedOwner;
			incomeOk = incHostile > incLoyal && incOwned == incHostile;
			incomeInfo = string.Format("loyal %1, hostile %2, owned %3 (town value %4)", incLoyal, incHostile, incOwned, town.m_iValue);
		}
		Check("fiaside: insurgent income scales with town support, owning a base adds no extra RP", incomeOk, incomeInfo);

		// Setup row visibility & town support visibility for insurgent viewer.
		bool setupOk = !ACR_Setup.HiddenByMatchType("coininsurgency") && ACR_Setup.Rows().Contains("coininsurgency\t");
		bool seesTier = town && ACR_TownSupport.KnownTier(town, ins) == town.m_iSupportTier;
		Check("fiaside: setup row shown in COIN and insurgent viewer sees true town tier", setupOk && seesTier,
			string.Format("setupOk %1, seesTier %2", setupOk, seesTier));

		ACR_TestRegress.Complete("fiaside", 7);
	}

	protected static void Check(string name, bool ok, string detail)
	{
		ACR_TestRegress.Check(name, ok, detail);
	}
}
