// Insurgent network placement (0.8.0 #97 item 8): when the insurgency is commanded (players or
// stage 3 AI commander), the commander buys and places the network pieces the insurgency raises
// by rule today — cells (road ambush or house), IED teams, sabotage raids, and technical raids.
//  - Network pieces are not squads: they never count against the soldier cap, deploy only when
//    enemies come near, cannot be given follow-up orders, and obey all existing caps and counters.
//  - Every piece needs a standing hideout within reach (3 km), or an idle depot/stash with a
//    drivable road route for technical raids.
class ACR_InsurgentNetwork
{
	static const int NET_CELL = 0;
	static const int NET_IED = 1;
	static const int NET_SABOTAGE = 2;
	static const int NET_TECH_RAID = 3;

	static const int CELL_COST = 12;
	static const int IED_COST = 10;
	static const int SABOTAGE_COST = 15;
	static const int TECH_RAID_COST = 15;

	static const float CELL_CLEAR = 400;         // m from enemy soldiers when placing a cell
	static const float SABOTAGE_PICK_RADIUS = 250; // m from click to enemy FOB HQ or roadblock

	static int CostOf(int kind)
	{
		if (kind == NET_CELL) return CELL_COST;
		if (kind == NET_IED) return IED_COST;
		if (kind == NET_SABOTAGE) return SABOTAGE_COST;
		if (kind == NET_TECH_RAID) return TECH_RAID_COST;
		return 0;
	}

	static string NameOf(int kind)
	{
		if (kind == NET_CELL) return "Place cell";
		if (kind == NET_IED) return "Place IED team";
		if (kind == NET_SABOTAGE) return "Sabotage raid";
		if (kind == NET_TECH_RAID) return "Technical raid";
		return "Network";
	}

	static string HintOf(int kind)
	{
		if (kind == NET_CELL) return "Right-click a road stretch or house within 3 km of a hideout (12 RP). Click again to cancel.";
		if (kind == NET_IED) return "Right-click a road within 3 km of a hideout, clear of enemy troops (10 RP). Click again to cancel.";
		if (kind == NET_SABOTAGE) return "Right-click an enemy FOB or roadblock within 3 km of a hideout (15 RP). Click again to cancel.";
		if (kind == NET_TECH_RAID) return "Right-click a road target within driving reach of an idle depot or stash (15 RP). Click again to cancel.";
		return "";
	}

	// Unified server dispatcher for panel RPC and AI commander.
	static bool Order(string faction, int kind, vector point, out string refusal)
	{
		refusal = "";
		if (!ACR_InsurgentSide.IsInsurgent(faction))
		{
			refusal = "only the insurgent commander can place network pieces";
			return false;
		}
		point = ACR_Manager.Ground(point);
		if (kind == NET_CELL) return PlaceCell(faction, point, refusal);
		if (kind == NET_IED) return PlaceIED(faction, point, refusal);
		if (kind == NET_SABOTAGE) return OrderSabotage(faction, point, refusal);
		if (kind == NET_TECH_RAID) return OrderTechRaid(faction, point, refusal);
		refusal = "unknown network order";
		return false;
	}

	// ---------------------------------------------------------------- cell (road ambush or house)

	static bool PlaceCell(string faction, vector point, out string refusal)
	{
		refusal = "";
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager || !manager.m_Bases) { refusal = "server unavailable"; return false; }
		if (manager.m_Bases.GetRP(faction) < CELL_COST)
		{
			refusal = string.Format("not enough RP (%1 needed)", CELL_COST);
			return false;
		}
		if (ACR_FIAGrowth.Cells() >= ACR_FIAGrowth.CellCap())
		{
			refusal = string.Format("at the cell limit (%1)", ACR_FIAGrowth.CellCap());
			return false;
		}
		ACR_DefenderPost hideout = ACR_FIAIntel.SupplierOf(point);
		if (!hideout)
		{
			refusal = "no standing hideout within 3 km to supply this cell";
			return false;
		}
		if (BBB_Terrain.IsWaterAt(point))
		{
			refusal = "cannot place a cell in water";
			return false;
		}
		string army = ACR_Coin.Army();
		if (!army.IsEmpty() && manager.m_Bases.MenWithin(point, CELL_CLEAR, army) > 0)
		{
			refusal = "enemy troops are too close";
			return false;
		}
		// Road ambush first if a road with cover snaps within 40 m; else a house within 60 m.
		vector road, dir, hide;
		float width;
		bool house = false;
		vector site = point;
		if (ACR_Guerrillas.RoadSite(point, road, dir, width, hide, false))
		{
			site = road;
		}
		else
		{
			array<IEntity> buildings = {};
			ACR_Garrison.BuildingsWithin(point, 60, buildings);
			if (buildings.IsEmpty())
			{
				refusal = "click a road with cover or a house";
				return false;
			}
			IEntity nearest;
			float bestDist = float.MAX;
			foreach (IEntity b : buildings)
			{
				float d = vector.DistanceXZ(b.GetOrigin(), point);
				if (d < bestDist) { bestDist = d; nearest = b; }
			}
			house = true;
			site = nearest.GetOrigin();
		}
		if (!ACR_Guerrillas.UsableAmbush(site))
		{
			refusal = "too close to a base or starting HQ";
			return false;
		}
		if (ACR_Guerrillas.Crowded(site, null))
		{
			refusal = "too close to another cell or hideout";
			return false;
		}
		ACR_DefenderPost created = ACR_Guerrillas.AddCellAt(house, site, faction);
		if (!created)
		{
			refusal = "could not place cell there";
			return false;
		}
		manager.m_Bases.Spend(faction, CELL_COST);
		string kindName = "road ambush cell";
		if (house) kindName = "house cell";
		manager.Command().Feed(faction, string.Format("Placed %1 at %2 (-%3 RP)", kindName, ACR_Manager.Grid(site), CELL_COST), site);
		PrintFormat("[ACR] NETWORK %1 placed %2 at %3 (supplied by %4, -%5 RP)", faction, kindName, ACR_Manager.Grid(site), hideout.m_sName, CELL_COST);
		return true;
	}

	// ---------------------------------------------------------------- IED team

	static bool PlaceIED(string faction, vector point, out string refusal)
	{
		refusal = "";
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager || !manager.m_Bases) { refusal = "server unavailable"; return false; }
		if (manager.m_Bases.GetRP(faction) < IED_COST)
		{
			refusal = string.Format("not enough RP (%1 needed)", IED_COST);
			return false;
		}
		if (ACR_IED.Live() >= ACR_IED.Cap())
		{
			refusal = string.Format("at the IED limit (%1 live)", ACR_IED.Cap());
			return false;
		}
		ACR_DefenderPost hideout = ACR_FIAIntel.SupplierOf(point);
		if (!hideout || vector.DistanceXZ(hideout.m_vPos, point) > ACR_IED.IED_REACH)
		{
			refusal = "no standing hideout within 3 km to supply an IED team";
			return false;
		}
		if (!ACR_Guerrillas.UsableAmbush(point))
		{
			refusal = "too close to a base or starting HQ";
			return false;
		}
		foreach (ACR_IED existing : ACR_IED.s_aIEDs)
		{
			if (!existing.m_bFired && !existing.m_bDisarmed && vector.DistanceXZ(existing.m_vPos, point) < ACR_IED.SPACING)
			{
				refusal = "too close to another live IED";
				return false;
			}
		}
		if (ACR_IED.HostileNear(faction, point, ACR_IED.PLANT_CLEAR))
		{
			refusal = "enemy troops are too close to plant unseen";
			return false;
		}
		ACR_IED ied = ACR_IED.Plant(point, faction, hideout);
		if (!ied)
		{
			refusal = "needs a road with nearby cover for the triggerman";
			return false;
		}
		manager.m_Bases.Spend(faction, IED_COST);
		manager.Command().Feed(faction, string.Format("IED team placed at %1 (-%2 RP)", ACR_Manager.Grid(ied.m_vPos), IED_COST), ied.m_vPos);
		PrintFormat("[ACR] NETWORK %1 placed IED at %2 (supplied by %3, -%4 RP)", faction, ACR_Manager.Grid(ied.m_vPos), hideout.m_sName, IED_COST);
		return true;
	}

	// ---------------------------------------------------------------- sabotage raid

	static bool OrderSabotage(string faction, vector point, out string refusal)
	{
		refusal = "";
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager || !manager.m_Bases) { refusal = "server unavailable"; return false; }
		if (manager.m_Bases.GetRP(faction) < SABOTAGE_COST)
		{
			refusal = string.Format("not enough RP (%1 needed)", SABOTAGE_COST);
			return false;
		}
		if (ACR_Raid.Running())
		{
			refusal = "a sabotage raid is already under way";
			return false;
		}
		// Nearest enemy built FOB HQ or roadblock within SABOTAGE_PICK_RADIUS of click.
		ACR_Base fob;
		ACR_Roadblock roadblock;
		float best = SABOTAGE_PICK_RADIUS;
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (!base.m_bBuilt || base.m_bRetired || base.m_bHqLost || !ACR_IED.Target(faction, base.m_sOwner)) continue;
			IEntity hq;
			if (manager.m_Structures) hq = manager.m_Structures.HqEntityOf(base);
			if (!hq) continue;
			float d = vector.DistanceXZ(hq.GetOrigin(), point);
			if (d < best) { best = d; fob = base; roadblock = null; }
		}
		foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
		{
			if (!rb || !rb.m_Entity || !ACR_IED.Target(faction, rb.m_sFaction)) continue;
			float gap = vector.DistanceXZ(rb.m_vPos, point);
			if (gap < best) { best = gap; roadblock = rb; fob = null; }
		}
		if (!fob && !roadblock)
		{
			refusal = "click near an enemy FOB or roadblock";
			return false;
		}
		vector targetPos = point;
		if (fob && manager.m_Structures && manager.m_Structures.HqEntityOf(fob)) targetPos = manager.m_Structures.HqEntityOf(fob).GetOrigin();
		else if (roadblock) targetPos = roadblock.m_vPos;
		ACR_DefenderPost hideout = ACR_FIAIntel.SupplierOf(targetPos);
		if (!hideout || vector.DistanceXZ(hideout.m_vPos, targetPos) > ACR_Raid.RAID_REACH)
		{
			refusal = "no standing hideout within 3 km of that target";
			return false;
		}
		if (!ACR_Raid.Start(faction, fob, roadblock, hideout))
		{
			refusal = "could not launch sabotage raid";
			return false;
		}
		manager.m_Bases.Spend(faction, SABOTAGE_COST);
		string targetName = "roadblock";
		if (fob) targetName = fob.m_sName + " HQ";
		manager.Command().Feed(faction, string.Format("Sabotage raid sent against %1 (-%2 RP)", targetName, SABOTAGE_COST), targetPos);
		PrintFormat("[ACR] NETWORK %1 ordered sabotage raid on %2 from %3 (-%4 RP)", faction, targetName, hideout.m_sName, SABOTAGE_COST);
		return true;
	}

	// ---------------------------------------------------------------- technical raid

	static bool OrderTechRaid(string faction, vector point, out string refusal)
	{
		refusal = "";
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager || !manager.m_Bases) { refusal = "server unavailable"; return false; }
		if (manager.m_Bases.GetRP(faction) < TECH_RAID_COST)
		{
			refusal = string.Format("not enough RP (%1 needed)", TECH_RAID_COST);
			return false;
		}
		ACR_Depot chosen;
		vector roadTarget;
		float bestDist = float.MAX;
		foreach (ACR_Depot depot : ACR_Depots.s_aDepots)
		{
			if (!depot || depot.m_iState != ACR_Depots.IDLE || depot.m_Crew || depot.m_aVehicles.IsEmpty()) continue;
			if (depot.m_Base && (depot.m_Base.m_bRetired || depot.m_Base.m_sOwner != faction)) continue;
			if (depot.m_Hideout && depot.m_Hideout.m_bGone) continue;
			vector candidate;
			if (!ACR_Depots.Drivable(depot, point, candidate)) continue;
			vector home = depot.m_vYard;
			if (depot.m_Base) home = depot.m_Base.GetOrigin();
			else if (depot.m_Hideout) home = depot.m_Hideout.m_vPos;
			float d = vector.DistanceXZ(home, candidate);
			if (d < bestDist) { bestDist = d; chosen = depot; roadTarget = candidate; }
		}
		if (!chosen)
		{
			refusal = "no idle depot or hideout stash can drive to that road";
			return false;
		}
		if (!ACR_Depots.Launch(chosen, roadTarget))
		{
			refusal = "depot could not launch technical raid";
			return false;
		}
		manager.m_Bases.Spend(faction, TECH_RAID_COST);
		manager.Command().Feed(faction, string.Format("Technical raid launched toward %1 (-%2 RP)", ACR_Manager.Grid(roadTarget), TECH_RAID_COST), roadTarget);
		PrintFormat("[ACR] NETWORK %1 ordered technical raid to %2 (-%3 RP)", faction, ACR_Manager.Grid(roadTarget), TECH_RAID_COST);
		return true;
	}

	// ---------------------------------------------------------------- insurgent map markers

	// Called from ACR_Guerrillas.Zones: shows the insurgent commander their own placed cells,
	// live IEDs and active sabotage raid target. Never shown to the army.
	static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
	{
		if (!ACR_InsurgentSide.IsInsurgent(faction)) return;
		foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
		{
			if (!post || post.m_bGone || !post.m_Cell || post.m_Cell.m_bHideout) continue;
			if (post.m_sFaction != faction) continue;
			positions.Insert(post.m_Cell.m_vKillZone);
			radii.Insert(35);
			if (post.m_Cell.m_bHouse) labels.Insert("House cell");
			else labels.Insert("Ambush cell");
		}
		foreach (ACR_IED ied : ACR_IED.s_aIEDs)
		{
			if (!ied || ied.m_bFired || ied.m_bDisarmed || ied.m_sOwner != faction) continue;
			positions.Insert(ied.m_vPos);
			radii.Insert(25);
			labels.Insert("IED");
		}
		ACR_Raid raid = ACR_Raid.Running();
		if (raid && raid.m_sOwner == faction)
		{
			positions.Insert(raid.m_vTarget);
			radii.Insert(45);
			labels.Insert("Sabotage raid");
		}
	}
}
