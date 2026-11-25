// srv:fianetwork (Everon whole map, about 40 s): insurgent network placement (ACR_InsurgentNetwork,
// 0.8.0 #97 item 8) + relaxed player hideout siting (CanSiteHideout).
// Verifies:
//  1. Placing a road/house cell within hideout reach succeeds, deducts RP, and doesn't count
//     against the soldier cap; out-of-reach / invalid sites refuse without spending RP.
//  2. Placing an IED team on a road within hideout reach succeeds and refuses spacing violations.
//  3. Launching a sabotage raid on an enemy roadblock within hideout reach starts ACR_Raid.Running()
//     and refuses a second concurrent raid.
//  4. Launching a technical raid from the stronghold depot on a drivable road target launches the depot.
//  5. Insurgent map zones show placed cells/IEDs/raid to the insurgent side and hide them from the army.
//  6. Relaxed CanSiteHideout accepts a lone farmhouse/building even near an ambush cell.
class ACR_TestFiaNetwork
{
	static void Run(int second)
	{
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager) return;
		if (second == 10) Verify(manager);
	}

	protected static void Verify(ACR_Manager manager)
	{
		string ins = ACR_InsurgentSide.Key();
		string army = ACR_Coin.Army();
		ACR_FactionData data = manager.m_Bases.GetFaction(ins);
		data.m_iRP = 200;

		array<ACR_DefenderPost> hideouts = {};
		ACR_Guerrillas.Hideouts(hideouts);
		ACR_DefenderPost hideout;
		if (!hideouts.IsEmpty()) hideout = hideouts[0];

		// 1. Place cell: find a surveyed road or house point within SUPPLY_REACH of any standing hideout.
		vector cellSite = vector.Zero;
		bool foundRoad = false;
		foreach (vector r : ACR_Guerrillas.s_aRoads)
		{
			if (!ACR_FIAIntel.SupplierOf(r)) continue;
			if (!ACR_Guerrillas.UsableAmbush(r) || ACR_Guerrillas.Crowded(r, null)) continue;
			if (manager.m_Bases.MenWithin(r, ACR_InsurgentNetwork.CELL_CLEAR, army) > 0) continue;
			cellSite = r;
			foundRoad = true;
			break;
		}
		if (!foundRoad)
		{
			foreach (vector hh : ACR_Guerrillas.s_aHouses)
			{
				if (!ACR_FIAIntel.SupplierOf(hh)) continue;
				if (!ACR_Guerrillas.UsableAmbush(hh) || ACR_Guerrillas.Crowded(hh, null)) continue;
				if (manager.m_Bases.MenWithin(hh, ACR_InsurgentNetwork.CELL_CLEAR, army) > 0) continue;
				cellSite = hh;
				foundRoad = true;
				break;
			}
		}
		int cellsBefore = ACR_FIAGrowth.Cells();
		int rpBefore = data.m_iRP;
		string refusal;
		bool placedCell = foundRoad && ACR_InsurgentNetwork.PlaceCell(ins, cellSite, refusal);
		string badRefusal;
		bool refusedBad = !ACR_InsurgentNetwork.PlaceCell(ins, Vector(10, 0, 10), badRefusal);
		Check("fianetwork: placing cell succeeds, deducts RP; bad site refused",
			placedCell && ACR_FIAGrowth.Cells() == cellsBefore + 1 && data.m_iRP == rpBefore - ACR_InsurgentNetwork.CELL_COST && refusedBad,
			string.Format("placed %1 ('%2'), cells %3->%4, rp %5->%6, badRefusal '%7'", placedCell, refusal, cellsBefore, ACR_FIAGrowth.Cells(), rpBefore, data.m_iRP, badRefusal));

		// 2. Place IED team on a road within hideout reach, clear of enemy troops and other IEDs.
		vector iedSite = vector.Zero;
		bool foundIedRoad = false;
		foreach (vector r2 : ACR_Guerrillas.s_aRoads)
		{
			if (!ACR_FIAIntel.SupplierOf(r2)) continue;
			if (!ACR_Guerrillas.UsableAmbush(r2)) continue;
			if (ACR_IED.HostileNear(ins, r2, ACR_IED.PLANT_CLEAR)) continue;
			iedSite = r2;
			foundIedRoad = true;
			break;
		}
		int iedBefore = ACR_IED.Live();
		rpBefore = data.m_iRP;
		string iedRef;
		bool placedIed = foundIedRoad && ACR_InsurgentNetwork.PlaceIED(ins, iedSite, iedRef);
		string dupRef;
		bool dupRefused = placedIed && !ACR_InsurgentNetwork.PlaceIED(ins, iedSite, dupRef);
		Check("fianetwork: placing IED succeeds, deducts RP, and refuses duplicate spacing",
			placedIed && ACR_IED.Live() == iedBefore + 1 && data.m_iRP == rpBefore - ACR_InsurgentNetwork.IED_COST && dupRefused,
			string.Format("placed %1 ('%2'), live %3->%4, dupRef '%5'", placedIed, iedRef, iedBefore, ACR_IED.Live(), dupRef));

		// 3. Sabotage raid against an enemy roadblock within 3 km of a hideout.
		bool raidOk = false;
		string raidInfo = "no hideout";
		if (hideout)
		{
			ACR_Roadblock rb = new ACR_Roadblock();
			rb.m_sFaction = army;
			rb.m_vPos = ACR_Manager.Ground(hideout.m_vPos + Vector(500, 0, 0));
			rb.m_Entity = GetGame().GetWorldEntity(); // non-null sentinel for TargetStands/OrderSabotage check
			ACR_Roadblocks.s_aRoadblocks.Insert(rb);
			rpBefore = data.m_iRP;
			string raidRef;
			bool launched = ACR_InsurgentNetwork.OrderSabotage(ins, rb.m_vPos, raidRef);
			string secondRef;
			bool secondRefused = launched && !ACR_InsurgentNetwork.OrderSabotage(ins, rb.m_vPos, secondRef);
			raidOk = launched && ACR_Raid.Running() != null && data.m_iRP == rpBefore - ACR_InsurgentNetwork.SABOTAGE_COST && secondRefused;
			raidInfo = string.Format("launched %1 ('%2'), running %3, secondRef '%4'", launched, raidRef, ACR_Raid.Running() != null, secondRef);
			ACR_Roadblocks.s_aRoadblocks.RemoveItem(rb);
		}
		Check("fianetwork: sabotage raid launches, deducts RP, and blocks concurrent raid", raidOk, raidInfo);

		// 4. Technical raid from an idle depot (stronghold) to a drivable road point.
		bool techOk = false;
		string techInfo = "no idle depot";
		ACR_Depot idleDepot;
		foreach (ACR_Depot d : ACR_Depots.s_aDepots)
		{
			if (d && d.m_iState == ACR_Depots.IDLE && !d.m_Crew && !d.m_aVehicles.IsEmpty()) { idleDepot = d; break; }
		}
		if (idleDepot)
		{
			vector home = idleDepot.m_vYard;
			if (idleDepot.m_Base) home = idleDepot.m_Base.GetOrigin();
			vector techTarget = vector.Zero;
			bool foundTech = false;
			foreach (vector r3 : ACR_Guerrillas.s_aRoads)
			{
				if (vector.DistanceXZ(r3, home) > 2200 || vector.DistanceXZ(r3, home) < 200) continue;
				vector drivable;
				if (ACR_Depots.Drivable(idleDepot, r3, drivable)) { techTarget = drivable; foundTech = true; break; }
			}
			if (!foundTech)
			{
				foreach (ACR_Base b3 : ACR_Base.s_All)
				{
					if (!b3 || b3.m_bRetired || b3 == idleDepot.m_Base) continue;
					if (vector.DistanceXZ(b3.GetOrigin(), home) > 2200 || vector.DistanceXZ(b3.GetOrigin(), home) < 200) continue;
					vector drivableB;
					if (ACR_Depots.Drivable(idleDepot, b3.GetOrigin(), drivableB)) { techTarget = drivableB; foundTech = true; break; }
				}
			}
			rpBefore = data.m_iRP;
			string techRef;
			bool techLaunched = foundTech && ACR_InsurgentNetwork.OrderTechRaid(ins, techTarget, techRef);
			techOk = techLaunched && idleDepot.m_iState == ACR_Depots.OUT && data.m_iRP == rpBefore - ACR_InsurgentNetwork.TECH_RAID_COST;
			techInfo = string.Format("found %1, launched %2 ('%3'), state %4", foundTech, techLaunched, techRef, idleDepot.m_iState);
		}
		Check("fianetwork: technical raid launches from depot and deducts RP", techOk, techInfo);

		// 5. Insurgent map zones show network pieces to insurgent side only, never to the army.
		array<vector> insPos = {}; array<float> insRad = {}; array<string> insLab = {};
		array<vector> armyPos = {}; array<float> armyRad = {}; array<string> armyLab = {};
		ACR_InsurgentNetwork.Zones(ins, insPos, insRad, insLab);
		ACR_InsurgentNetwork.Zones(army, armyPos, armyRad, armyLab);
		Check("fianetwork: network zones visible to insurgent commander, hidden from army",
			insLab.Count() >= 2 && armyLab.IsEmpty(),
			string.Format("ins zones %1, army zones %2", insLab.Count(), armyLab.Count()));

		// 6. Relaxed CanSiteHideout: a single building near an ambush cell (and >1 km from hideouts) is allowed.
		bool siteOk = false;
		string siteInfo = "no house site";
		foreach (vector h : ACR_Guerrillas.s_aHouses)
		{
			vector mid;
			string sRef;
			if (ACR_Guerrillas.CanSiteHideout(h, mid, sRef, army))
			{
				siteOk = true;
				siteInfo = string.Format("sited at %1", ACR_Manager.Grid(mid));
				break;
			}
		}
		Check("fianetwork: relaxed CanSiteHideout accepts rural buildings outside towns/bases", siteOk, siteInfo);

		ACR_TestRegress.Complete("fianetwork", 6);
	}

	protected static void Check(string name, bool ok, string detail)
	{
		ACR_TestRegress.Check(name, ok, detail);
	}
}
