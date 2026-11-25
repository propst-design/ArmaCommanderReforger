// srv:fiahome (Everon whole map, about 45 s): hideouts as homes + site dressing + starting HQ works
// (ACR_HideoutHomes, ACR_FIAIntel, ACR_BaseWorks.ApplyHQs, 0.8.0 #97 item 7).
class ACR_TestFiaHome
{
	static void Run(int second)
	{
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager) return;
		if (second == 12) Verify(manager);
	}

	protected static void Verify(ACR_Manager manager)
	{
		string army = ACR_Coin.Army();
		string ins = ACR_InsurgentSide.Key();

		// 1. Living army HQ starts pre-fortified with tier-1 works and site dressing.
		ACR_Base hq = ACR_OneSide.Home();
		ACR_BaseWork hqWork;
		if (hq) hqWork = ACR_BaseWorks.WorkFor(hq, army);
		string hqName = "none";
		if (hq) hqName = hq.m_sName;
		int hqTier = 0;
		if (hqWork) hqTier = hqWork.m_iTier;
		Check("fiahome: living army HQ starts with tier-1 fortifications and dressing",
			hq && hqWork && hqTier >= 1,
			string.Format("hq '%1', tier %2", hqName, hqTier));

		// 2. Starting hideouts exist, have guerrilla site dressing, and each has a deploy spawn point.
		array<ACR_DefenderPost> hideouts = {};
		ACR_Guerrillas.Hideouts(hideouts);
		int dressed, spawnCount;
		foreach (ACR_DefenderPost h : hideouts)
			if (ACR_HideoutHomes.DressingCount(h) > 0) dressed++;
		foreach (SCR_SpawnPoint sp : SCR_SpawnPoint.GetSpawnPoints())
			if (sp && sp.GetFactionKey() == ins && ACR_HideoutHomes.IsHideoutPoint(sp)) spawnCount++;
		Check("fiahome: hideouts have guerrilla site dressing and deploy spawn points",
			hideouts.Count() > 0 && dressed == hideouts.Count() && spawnCount == hideouts.Count(),
			string.Format("hideouts %1, dressed %2, spawns %3", hideouts.Count(), dressed, spawnCount));

		ACR_DefenderPost first;
		if (!hideouts.IsEmpty()) first = hideouts[0];

		// 3. Intel heat progression: cold -> warm -> hot -> found, and staleness widens back to cold.
		bool heatOk = false;
		string heatInfo = "no hideout";
		if (first)
		{
			string h0 = ACR_FIAIntel.Heat(first, army);
			ACR_FIAIntel.Lead(army, first, "test cell 1");
			string h1 = ACR_FIAIntel.Heat(first, army);
			ACR_FIAIntel.Lead(army, first, "test cell 2");
			ACR_FIAIntel.Lead(army, first, "test cell 3");
			string h3 = ACR_FIAIntel.Heat(first, army);
			// Age past 3 staleness steps -> back to cold.
			ACR_FIAIntel.AgeForTest(ACR_FIAIntel.STALE_S + 10);
			string hAfter1 = ACR_FIAIntel.Heat(first, army);
			ACR_FIAIntel.AgeForTest(ACR_FIAIntel.STALE_S + 10);
			ACR_FIAIntel.AgeForTest(ACR_FIAIntel.STALE_S + 10);
			string hColdAgain = ACR_FIAIntel.Heat(first, army);
			ACR_Guerrillas.Know(first, army);
			string hFound = ACR_FIAIntel.Heat(first, army);
			first.m_Cell.m_aKnownBy.RemoveItem(army);
			heatOk = h0 == "cold" && h1 == "warm" && h3 == "hot" && hAfter1 == "warm" && hColdAgain == "cold" && hFound == "found";
			heatInfo = string.Format("%1 -> %2 -> %3 -> %4 -> %5; found=%6", h0, h1, h3, hAfter1, hColdAgain, hFound);
		}
		Check("fiahome: intel heat cold -> warm -> hot -> stale -> cold, and found", heatOk, heatInfo);

		// 4. Buying a band at a hideout deploys it and deducts RP.
		bool buyOk = false;
		string buyInfo = "no hideout";
		if (first)
		{
			ACR_FactionData data = manager.m_Bases.GetFaction(ins);
			data.m_iRP = 200;
			int pickIdx = -1;
			foreach (int idx, ACR_RequisitionEntry e : data.m_aRequisitions)
			{
				if (e && !e.m_sGroup.IsEmpty() && e.m_sVehicle.IsEmpty() && e.m_iRemaining > 0) { pickIdx = idx; break; }
			}
			int rpBefore = data.m_iRP;
			string res = "";
			if (pickIdx >= 0) res = manager.m_Requisition.PurchaseAtHideout(ins, pickIdx, first);
			buyOk = pickIdx >= 0 && data.m_iRP < rpBefore && res.Contains("deploying at");
			buyInfo = string.Format("idx %1, rp %2->%3, res '%4'", pickIdx, rpBefore, data.m_iRP, res);
		}
		Check("fiahome: buying a band at a hideout deploys it and spends RP", buyOk, buyInfo);

		// 5. Hideout siting refuses water / open ground cleanly.
		vector mid;
		string refWater;
		bool waterOk = !ACR_Guerrillas.CanSiteHideout(Vector(0, 0, 0), mid, refWater, army) && !refWater.IsEmpty();
		Check("fiahome: hideout siting refuses invalid spot with a reason", waterOk, refWater);

		// 6. Go to ground: empties hideout, deletes spawn point, refunds RP, clears army leads.
		bool groundOk = false;
		string groundInfo = "no hideout";
		if (first)
		{
			ACR_FIAIntel.Lead(army, first, "before go to ground");
			int rpBeforeG = manager.m_Bases.GetRP(ins);
			string refG;
			bool did = ACR_HideoutHomes.GoToGround(ins, first.m_sName, refG);
			int rpAfterG = manager.m_Bases.GetRP(ins);
			bool leadGone = ACR_FIAIntel.LeadOf(army, first) == null;
			int spawnsAfter = 0;
			foreach (SCR_SpawnPoint sp2 : SCR_SpawnPoint.GetSpawnPoints())
				if (sp2 && sp2.GetFactionKey() == ins && ACR_HideoutHomes.IsHideoutPoint(sp2)) spawnsAfter++;
			groundOk = did && first.m_bGone && rpAfterG == rpBeforeG + ACR_HideoutHomes.GO_TO_GROUND_REFUND && leadGone && spawnsAfter == spawnCount - 1;
			groundInfo = string.Format("did %1, gone %2, rp %3->%4, leadGone %5, spawns %6->%7, ref '%8'", did, first.m_bGone, rpBeforeG, rpAfterG, leadGone, spawnCount, spawnsAfter, refG);
		}
		Check("fiahome: go to ground empties hideout, removes spawn point, refunds RP, clears lead", groundOk, groundInfo);

		ACR_TestRegress.Complete("fiahome", 6);
	}

	protected static void Check(string name, bool ok, string detail)
	{
		ACR_TestRegress.Check(name, ok, detail);
	}
}
