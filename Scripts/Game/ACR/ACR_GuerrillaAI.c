// Insurgent AI commander — Guerrilla doctrine (0.8.0 #97 item 11, #244 stage 3):
// runs the insurgent side when -acrCoinInsurgency ai is active. Built directly on the
// insurgency's own spending order and Stage 2 APIs rather than bending the conventional
// ACR_AICommander (which is built around base ownership, grabs, FOBs and off-map fire):
//  - Never land-grabs conventional bases, never calls off-map supports, and never runs
//    ACR_AICommander on the insurgent side.
//  - Sees only what an insurgent player sees: hideout heat, road traffic/surveyed roads,
//    town support tiers, weakly guarded enemy FOBs/roadblocks, and idle depots/stashes.
//  - Priority each 15 s tick:
//    1. Hideout survival: pack up hot/found hideouts (never the last one) and send a
//       foot band to establish a new hideout when below cap.
//    2. Network spending: cells on busy roads, sabotage raids (preferring night) on
//       weakly guarded FOBs/roadblocks, IED teams on busy roads, technical raids from
//       idle depots/stashes.
//    3. Band mustering: keep up to BAND_CAP mobile foot bands mustered in cover near
//       turning towns or busy roads.
//    4. Band orders & dispersal: work unguarded non-Hostile towns to turn support, lay
//       road ambushes (which auto-fade on contact), and disperse depleted bands.
class ACR_GuerrillaAI
{
	static const int TICK_SECONDS = 15;
	static const int BAND_CAP = 2;                 // baseline mobile foot bands kept in the field
	static const int BAND_CAP_MAX = 4;             // escalated band cap as towns turn Hostile and cells grow
	static const float REORDER_MS = 180000;        // min time before reassigning a band
	static const float DISTRACTION_GAP = 800;      // m: prefer working towns away from an active ambush to stretch QRF
	static const float TOWN_ARMY_AVOID = 250;      // m: avoid sending a band to a town held by army troops
	static const float MUSTER_OFFSET_MIN = 180;    // m from target town/road to search for muster cover
	static const float MUSTER_OFFSET_MAX = 450;

	protected static int s_iSeconds;
	protected static ref map<ACR_Squad, string> s_BandRole;
	protected static ref map<ACR_Squad, float> s_BandOrderedAt;
	protected static ref map<ACR_Squad, ACR_Base> s_BandTown;

	// Called by ACR_StaticInit at game start.
	static void ACR_InitStatics()
	{
		s_BandRole = new map<ACR_Squad, string>();
		s_BandOrderedAt = new map<ACR_Squad, float>();
		s_BandTown = new map<ACR_Squad, ACR_Base>();
	}

	static void Reset()
	{
		s_iSeconds = 0;
		if (s_BandRole) s_BandRole.Clear();
		if (s_BandOrderedAt) s_BandOrderedAt.Clear();
		if (s_BandTown) s_BandTown.Clear();
	}

	static bool Active()
	{
		return ACR_InsurgentSide.AIControlled();
	}

	static string BandRole(ACR_Squad squad)
	{
		string role = "";
		if (squad && s_BandRole) s_BandRole.Find(squad, role);
		return role;
	}

	// Server, every second (ACR_Manager.Tick).
	static void Tick(ACR_Manager manager)
	{
		if (!Replication.IsServer() || !Active() || !manager || !manager.m_Bases) return;
		string faction = ACR_InsurgentSide.Key();
		if (manager.m_Debug && manager.m_Debug.IsPaused(faction)) return;
		s_iSeconds++;
		if (s_iSeconds % TICK_SECONDS != 0) return;
		Step(manager, faction);
	}

	// Tests: run one strategic decision step immediately.
	static void RunTickForTest(ACR_Manager manager)
	{
		if (!Replication.IsServer() || !Active() || !manager || !manager.m_Bases) return;
		Step(manager, ACR_InsurgentSide.Key());
	}

	protected static void Step(ACR_Manager manager, string faction)
	{
		string army = ACR_Coin.Army();
		PruneBands(manager);
		ManageHideouts(manager, faction, army);
		ManageNetwork(manager, faction, army);
		ManageMuster(manager, faction, army);
		ManageBands(manager, faction, army);
	}

	// ---------------------------------------------------------------- hideout survival & relocation

	protected static void ManageHideouts(ACR_Manager manager, string faction, string army)
	{
		array<ACR_DefenderPost> hideouts = {};
		ACR_Guerrillas.Hideouts(hideouts);
		// Never pack up the last standing hideout.
		if (hideouts.Count() > 1)
		{
			foreach (ACR_DefenderPost h : hideouts)
			{
				string heat = ACR_FIAIntel.Heat(h, army);
				if (heat != "hot" && heat != "found") continue;
				string refusal;
				if (ACR_HideoutHomes.GoToGround(faction, h.m_sName, refusal))
				{
					PrintFormat("[ACR] GUERRILLA AI %1: packed up %2 (%3)", faction, h.m_sName, heat);
					break;
				}
			}
		}

		// If below cap and we have RP + a free foot band, establish a replacement hideout.
		hideouts.Clear();
		ACR_Guerrillas.Hideouts(hideouts);
		if (hideouts.Count() >= ACR_HideoutHomes.MaxHideouts()) return;
		if (manager.m_Bases.GetRP(faction) < ACR_HideoutHomes.HIDEOUT_COST) return;
		// Only relocate proactively when down to 1 hideout or carrying comfortable RP surplus.
		if (hideouts.Count() >= 2 && manager.m_Bases.GetRP(faction) < ACR_HideoutHomes.HIDEOUT_COST + ACR_InsurgentNetwork.CELL_COST) return;
		ACR_Squad builder = FreeFootBand(manager, faction, 2);
		if (!builder) return;
		vector site;
		if (!PickNewHideoutSite(army, site)) return;
		string why;
		if (ACR_HideoutHomes.OrderSetup(builder, site, why))
		{
			SetBandRole(builder, "relocate", null);
			PrintFormat("[ACR] GUERRILLA AI %1: %2 sent to set up hideout at %3", faction, builder.GetName(), ACR_Manager.Grid(site));
		}
	}

	protected static bool PickNewHideoutSite(string army, out vector site)
	{
		if (!ACR_Guerrillas.s_aHouses || ACR_Guerrillas.s_aHouses.IsEmpty()) return false;
		int count = ACR_Guerrillas.s_aHouses.Count();
		int start = Math.RandomInt(0, count);
		int tries = Math.Min(count, 12);
		for (int i = 0; i < tries; i++)
		{
			vector cand = ACR_Guerrillas.s_aHouses[(start + i) % count];
			vector middle;
			string why;
			if (ACR_Guerrillas.CanSiteHideout(cand, middle, why, army))
			{
				site = middle;
				return true;
			}
		}
		return false;
	}

	// ---------------------------------------------------------------- network spending

	protected static void ManageNetwork(ACR_Manager manager, string faction, string army)
	{
		int rp = manager.m_Bases.GetRP(faction);
		if (rp < ACR_InsurgentNetwork.IED_COST) return;

		// 1. Cells first while under cap.
		if (ACR_FIAGrowth.Cells() < ACR_FIAGrowth.CellCap() && rp >= ACR_InsurgentNetwork.CELL_COST)
		{
			if (TryPlaceCell(faction)) return;
		}

		// 2. Sabotage raids (prioritized at night) and IED teams (first IED prioritized by day).
		bool night = ACR_Raid.IsNight();
		if (night && TryOrderSabotage(manager, faction)) return;
		if (!night && ACR_IED.Live() == 0 && TryPlaceIED(faction)) return;
		if (!night && TryOrderSabotage(manager, faction)) return;
		if (TryPlaceIED(faction)) return;

		// 3. Technical raid from an idle depot or hideout stash.
		TryOrderTechRaid(faction, army);
	}

	protected static bool TryPlaceCell(string faction)
	{
		string refusal;
		// Prefer the busiest road in traffic records within hideout reach.
		vector busyRoad;
		if (PickBusyRoad(busyRoad) && ACR_InsurgentNetwork.PlaceCell(faction, busyRoad, refusal)) return true;

		// Fallback to surveyed roads within 3 km of a standing hideout.
		if (ACR_Guerrillas.s_aRoads && !ACR_Guerrillas.s_aRoads.IsEmpty())
		{
			int count = ACR_Guerrillas.s_aRoads.Count();
			int start = Math.RandomInt(0, count);
			int tries = Math.Min(count, 10);
			for (int i = 0; i < tries; i++)
			{
				vector cand = ACR_Guerrillas.s_aRoads[(start + i) % count];
				if (!ACR_FIAIntel.SupplierOf(cand)) continue;
				if (ACR_InsurgentNetwork.PlaceCell(faction, cand, refusal)) return true;
			}
		}

		// Fallback to rural houses in reach.
		if (ACR_Guerrillas.s_aHouses && !ACR_Guerrillas.s_aHouses.IsEmpty())
		{
			int hCount = ACR_Guerrillas.s_aHouses.Count();
			int hStart = Math.RandomInt(0, hCount);
			int hTries = Math.Min(hCount, 8);
			for (int j = 0; j < hTries; j++)
			{
				vector house = ACR_Guerrillas.s_aHouses[(hStart + j) % hCount];
				if (!ACR_FIAIntel.SupplierOf(house)) continue;
				if (ACR_InsurgentNetwork.PlaceCell(faction, house, refusal)) return true;
			}
		}
		return false;
	}

	protected static bool TryPlaceIED(string faction)
	{
		if (ACR_IED.Live() >= ACR_IED.Cap()) return false;
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager || !manager.m_Bases || manager.m_Bases.GetRP(faction) < ACR_InsurgentNetwork.IED_COST) return false;
		string refusal;
		vector busyRoad;
		if (PickBusyRoad(busyRoad) && ACR_InsurgentNetwork.PlaceIED(faction, busyRoad, refusal)) return true;

		if (ACR_Guerrillas.s_aRoads && !ACR_Guerrillas.s_aRoads.IsEmpty())
		{
			int count = ACR_Guerrillas.s_aRoads.Count();
			int start = Math.RandomInt(0, count);
			int tries = Math.Min(count, 10);
			for (int i = 0; i < tries; i++)
			{
				vector cand = ACR_Guerrillas.s_aRoads[(start + i) % count];
				if (!ACR_FIAIntel.SupplierOf(cand)) continue;
				if (ACR_InsurgentNetwork.PlaceIED(faction, cand, refusal)) return true;
			}
		}
		return false;
	}

	protected static bool TryOrderSabotage(ACR_Manager manager, string faction)
	{
		if (ACR_Raid.Running()) return false;
		if (manager.m_Bases.GetRP(faction) < ACR_InsurgentNetwork.SABOTAGE_COST) return false;
		array<ACR_DefenderPost> hideouts = {};
		ACR_Guerrillas.Hideouts(hideouts);
		foreach (ACR_DefenderPost h : hideouts)
		{
			ACR_Base fob;
			ACR_Roadblock rb;
			if (!ACR_Raid.PickTarget(faction, h.m_vPos, fob, rb)) continue;
			vector targetPos;
			if (fob && manager.m_Structures && manager.m_Structures.HqEntityOf(fob))
				targetPos = manager.m_Structures.HqEntityOf(fob).GetOrigin();
			else if (rb)
				targetPos = rb.m_vPos;
			else
				continue;
			string refusal;
			if (ACR_InsurgentNetwork.OrderSabotage(faction, targetPos, refusal)) return true;
		}
		return false;
	}

	protected static bool TryOrderTechRaid(string faction, string army)
	{
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager || !manager.m_Bases || manager.m_Bases.GetRP(faction) < ACR_InsurgentNetwork.TECH_RAID_COST) return false;
		if (!ACR_Depots.s_aDepots || ACR_Depots.s_aDepots.IsEmpty()) return false;
		bool anyIdle;
		foreach (ACR_Depot depot : ACR_Depots.s_aDepots)
		{
			if (!depot || depot.m_iState != ACR_Depots.IDLE || depot.m_Crew || depot.m_aVehicles.IsEmpty()) continue;
			if (depot.m_Base && (depot.m_Base.m_bRetired || depot.m_Base.m_sOwner != faction)) continue;
			if (depot.m_Hideout && depot.m_Hideout.m_bGone) continue;
			anyIdle = true;
			break;
		}
		if (!anyIdle) return false;

		string refusal;
		vector busyRoad;
		if (PickBusyRoad(busyRoad) && ACR_InsurgentNetwork.OrderTechRaid(faction, busyRoad, refusal)) return true;

		// Otherwise raid near an army-held or contested town.
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (!ACR_TownSupport.IsTown(base) || base.m_sOwner != army) continue;
			if (ACR_InsurgentNetwork.OrderTechRaid(faction, base.GetOrigin(), refusal)) return true;
		}
		return false;
	}

	protected static bool PickBusyRoad(out vector road)
	{
		if (!ACR_Guerrillas.s_mTraffic || ACR_Guerrillas.s_mTraffic.IsEmpty()) return false;
		float best = -1;
		foreach (int key, float unused : ACR_Guerrillas.s_mTraffic)
		{
			vector at = ACR_Guerrillas.s_mTrafficAt.Get(key);
			if (!ACR_FIAIntel.SupplierOf(at)) continue;
			float busy = ACR_Guerrillas.TrafficNear(at);
			if (busy > best) { best = busy; road = at; }
		}
		return best >= 0;
	}

	// ---------------------------------------------------------------- band mustering

	// Escalate mobile band cap as the insurgency gains Hostile towns and cells (2 -> 3 -> 4).
	static int EffectiveBandCap()
	{
		int hostileTowns = 0;
		foreach (ACR_Base base : ACR_Base.s_All)
			if (base && !base.m_bRetired && ACR_TownSupport.IsTown(base) && base.m_iSupportTier == ACR_TownSupport.HOSTILE) hostileTowns++;
		int cells = ACR_FIAGrowth.Cells();
		int cap = BAND_CAP;
		if (hostileTowns >= 4 || cells >= 4) cap = BAND_CAP_MAX;
		else if (hostileTowns >= 2 || cells >= 2) cap = Math.Min(BAND_CAP_MAX, BAND_CAP + 1);
		return cap;
	}

	// True when the insurgent side has sighted army armed vehicles (via fog-of-war ACR_Intel).
	protected static bool ArmyArmorSeen(ACR_Manager manager, string faction, string army)
	{
		if (!manager || army.IsEmpty()) return false;
		ACR_Intel intel = manager.IntelFor(faction);
		if (!intel) return false;
		foreach (ACR_Squad squad : manager.m_Squads)
		{
			if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != army) continue;
			IEntity veh = ACR_Transport.CrewedVehicle(squad);
			if (!veh || !squad.HasGunner(veh)) continue;
			array<AIAgent> agents = {};
			squad.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				IEntity body = agent.GetControlledEntity();
				if (body && intel.m_SeenTime.Contains(body)) return true;
			}
		}
		return false;
	}

	protected static void ManageMuster(ACR_Manager manager, string faction, string army)
	{
		int bands = CountBands(manager, faction);
		if (ACR_Muster.s_Jobs)
		{
			foreach (ACR_MusterJob j : ACR_Muster.s_Jobs)
				if (j && j.m_sFaction == faction) bands++;
		}
		int cap = EffectiveBandCap();
		if (bands >= cap) return;

		int entryIdx = PickFootEntry(manager, faction, army, cap);
		if (entryIdx < 0) return;

		vector spot;
		if (!PickMusterSpot(manager, faction, army, entryIdx, spot)) return;
		string refusal;
		if (ACR_Muster.Order(faction, entryIdx, spot, refusal))
			PrintFormat("[ACR] GUERRILLA AI %1: mustering band at %2 (cap %3)", faction, ACR_Manager.Grid(spot), cap);
	}

	protected static int PickFootEntry(ACR_Manager manager, string faction, string army = "", int cap = BAND_CAP)
	{
		ACR_FactionData data = manager.m_Bases.GetFaction(faction);
		if (!data || !data.m_aRequisitions) return -1;
		int rp = manager.m_Bases.GetRP(faction);
		bool wantAT = ArmyArmorSeen(manager, faction, army);
		bool escalated = cap >= 3;
		int bestIdx = -1;
		int bestCost = int.MAX;
		int atIdx = -1;
		int atCost = int.MAX;
		int heavyIdx = -1;
		int heavyCost = -1;
		foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
		{
			if (!ACR_Muster.IsFootEntry(entry) || entry.m_iRemaining <= 0) continue;
			int cost = ACR_Muster.MusterCost(entry);
			if (cost > rp) continue;
			if (cost < bestCost) { bestCost = cost; bestIdx = i; }
			if (wantAT && ACR_AIRequisition.IsATEntry(entry) && cost < atCost) { atCost = cost; atIdx = i; }
			if (escalated && cost * 2 <= rp && cost > heavyCost) { heavyCost = cost; heavyIdx = i; }
		}
		if (atIdx >= 0) return atIdx;
		if (heavyIdx >= 0) return heavyIdx;
		return bestIdx;
	}

	protected static bool PickMusterSpot(ACR_Manager manager, string faction, string army, int entryIdx, out vector spot)
	{
		// Prefer cover near a Quiet/Restless town we can work, else near a standing cold/warm hideout.
		ACR_Base town = PickTargetTown(manager, army, null);
		if (town && FindCoverMusterNear(faction, entryIdx, town.GetOrigin(), spot)) return true;

		array<ACR_DefenderPost> hideouts = {};
		ACR_Guerrillas.Hideouts(hideouts);
		foreach (ACR_DefenderPost h : hideouts)
		{
			string heat = ACR_FIAIntel.Heat(h, army);
			if (heat == "hot" || heat == "found" || ACR_Muster.HideoutBusy(h)) continue;
			if (FindCoverMusterNear(faction, entryIdx, h.m_vPos, spot)) return true;
		}
		return false;
	}

	protected static bool FindCoverMusterNear(string faction, int entryIdx, vector anchor, out vector spot)
	{
		ACR_DefenderPost h;
		float slow;
		string refusal;
		for (int i = 0; i < 10; i++)
		{
			float ang = Math.RandomFloat(0, Math.PI2);
			float dist = Math.RandomFloat(MUSTER_OFFSET_MIN, MUSTER_OFFSET_MAX);
			vector cand = ACR_Manager.Ground(anchor + Vector(Math.Cos(ang) * dist, 0, Math.Sin(ang) * dist));
			if (ACR_Muster.CanMuster(faction, entryIdx, cand, h, slow, refusal))
			{
				spot = cand;
				return true;
			}
		}
		// Fallback to surveyed rural houses in reach of a hideout.
		if (ACR_Guerrillas.s_aHouses && !ACR_Guerrillas.s_aHouses.IsEmpty())
		{
			int count = ACR_Guerrillas.s_aHouses.Count();
			int start = Math.RandomInt(0, count);
			int tries = Math.Min(count, 8);
			for (int j = 0; j < tries; j++)
			{
				vector house = ACR_Guerrillas.s_aHouses[(start + j) % count];
				if (ACR_Muster.CanMuster(faction, entryIdx, house, h, slow, refusal))
				{
					spot = house;
					return true;
				}
			}
		}
		return false;
	}

	// ---------------------------------------------------------------- band orders & dispersal

	protected static void ManageBands(ACR_Manager manager, string faction, string army)
	{
		float now = GetGame().GetWorld().GetWorldTime();
		// Track active ambushes and town workers so bands split across sectors (distraction play).
		int activeTownWorkers = 0;
		int activeAmbushes = 0;
		vector ambushPos = vector.Zero;
		foreach (ACR_Squad scan : manager.m_Squads)
		{
			if (!IsBand(scan, faction)) continue;
			if (scan.m_Ambush && !scan.m_Ambush.m_bFaded)
			{
				activeAmbushes++;
				if (scan.GetAnchor()) ambushPos = scan.GetAnchor().GetOrigin();
			}
			else if (BandRole(scan) == "town") activeTownWorkers++;
		}
		foreach (ACR_Squad squad : manager.m_Squads)
		{
			if (!IsBand(squad, faction)) continue;
			if (ACR_HideoutHomes.IsSettingUp(squad)) continue;

			// Disperse badly depleted foot bands (down to 1 fighter) out of contact.
			if (squad.CountAlive() <= 1 && ACR_Fortify.Infantry(squad) && squad.KnownEnemies(300) == 0)
			{
				string dRefusal;
				if (ACR_Muster.Disperse(squad, dRefusal))
				{
					PrintFormat("[ACR] GUERRILLA AI %1: dispersed depleted %2", faction, squad.GetName());
					continue;
				}
			}

			// Keep active ambushes until they fade or end.
			if (squad.m_Ambush && !squad.m_Ambush.m_bFaded) continue;

			// Keep town workers until the town is Hostile, army arrives, or hold timer expires.
			string role = BandRole(squad);
			float orderedAt;
			s_BandOrderedAt.Find(squad, orderedAt);
			if (role == "town")
			{
				ACR_Base assigned;
				if (s_BandTown.Find(squad, assigned) && assigned && assigned.m_iSupportTier < ACR_TownSupport.HOSTILE)
				{
					bool armyHere = !army.IsEmpty() && manager.m_Bases.MenWithin(assigned.GetOrigin(), TOWN_ARMY_AVOID, army) > 0;
					if (!armyHere && now - orderedAt < REORDER_MS) continue;
				}
			}
			else if (orderedAt > 0 && now - orderedAt < REORDER_MS)
			{
				continue;
			}

			// Distraction play: if another band is already turning a town and no ambush is out yet,
			// lay a road ambush first to draw and pin the army's QRF.
			if (activeTownWorkers > 0 && activeAmbushes == 0)
			{
				vector distractRoad;
				if (PickAmbushRoad(squad, distractRoad))
				{
					vector dSite, dKz;
					string dWhy;
					if (ACR_Ambush.DefendOnRoad(squad, distractRoad, dSite, dKz, dWhy))
					{
						SetBandRole(squad, "ambush", null);
						activeAmbushes++;
						ambushPos = dSite;
						PrintFormat("[ACR] GUERRILLA AI %1: %2 lays distraction ambush on road %3", faction, squad.GetName(), ACR_Manager.Grid(distractRoad));
						continue;
					}
				}
			}

			// Priority 1: work an unguarded Quiet/Restless town to turn it Hostile (preferring a
			// different sector from any active ambush).
			ACR_Base town = PickTargetTown(manager, army, squad, ambushPos);
			if (town)
			{
				squad.RouteTo(town.GetOrigin(), ACR_Squad.WP_MOVE, -1, "Work town " + town.m_sName, ACR_EOrderKind.MOVE);
				SetBandRole(squad, "town", town);
				activeTownWorkers++;
				PrintFormat("[ACR] GUERRILLA AI %1: %2 works town %3 (%4)", faction, squad.GetName(), town.m_sName, ACR_TownSupport.TierName(town.m_iSupportTier));
				continue;
			}

			// Priority 2: lay a road ambush on a busy or surveyed road (fades after contact).
			vector road;
			if (PickAmbushRoad(squad, road))
			{
				vector site, kz;
				string aWhy;
				if (ACR_Ambush.DefendOnRoad(squad, road, site, kz, aWhy))
				{
					SetBandRole(squad, "ambush", null);
					activeAmbushes++;
					ambushPos = site;
					PrintFormat("[ACR] GUERRILLA AI %1: %2 ambushes road %3", faction, squad.GetName(), ACR_Manager.Grid(road));
				}
			}
		}
	}

	protected static ACR_Base PickTargetTown(ACR_Manager manager, string army, ACR_Squad forSquad, vector ambushPos = vector.Zero)
	{
		vector refPos = vector.Zero;
		if (forSquad && forSquad.GetAnchor()) refPos = forSquad.GetAnchor().GetOrigin();
		ACR_Base best;
		float bestScore = -1000000;
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (!ACR_TownSupport.IsTown(base) || base.m_iSupportTier < 0) continue;
			// Already Hostile or held by army troops: skip (never land-grab an occupied army base).
			if (base.m_iSupportTier == ACR_TownSupport.HOSTILE) continue;
			if (!army.IsEmpty() && base.m_sOwner == army) continue;
			if (!army.IsEmpty() && manager.m_Bases.MenWithin(base.GetOrigin(), TOWN_ARMY_AVOID, army) > 0) continue;
			// Avoid stacking two bands on the same town.
			bool taken;
			foreach (ACR_Squad other, ACR_Base at : s_BandTown)
				if (other && other != forSquad && at == base && other.CountAlive() > 0) taken = true;
			if (taken) continue;
			// Prefer Restless (closest to flipping Hostile), then Quiet, then higher value.
			float score = base.m_iSupportTier * 50 + base.m_iValue * 10;
			if (ambushPos != vector.Zero && vector.DistanceXZ(ambushPos, base.GetOrigin()) >= DISTRACTION_GAP) score += 40;
			if (refPos != vector.Zero) score -= vector.DistanceXZ(refPos, base.GetOrigin()) / 150;
			if (score > bestScore) { bestScore = score; best = base; }
		}
		return best;
	}

	protected static bool PickAmbushRoad(ACR_Squad squad, out vector road)
	{
		if (PickBusyRoad(road)) return true;
		if (!ACR_Guerrillas.s_aRoads || ACR_Guerrillas.s_aRoads.IsEmpty()) return false;
		vector from = vector.Zero;
		if (squad && squad.GetAnchor()) from = squad.GetAnchor().GetOrigin();
		float bestDist = float.MAX;
		bool found;
		int count = ACR_Guerrillas.s_aRoads.Count();
		int step = Math.Max(1, count / 16);
		for (int i = 0; i < count; i += step)
		{
			vector cand = ACR_Guerrillas.s_aRoads[i];
			if (!ACR_Guerrillas.UsableAmbush(cand)) continue;
			float d = 0;
			if (from != vector.Zero) d = vector.DistanceXZ(from, cand);
			if (d < bestDist) { bestDist = d; road = cand; found = true; }
		}
		return found;
	}

	// ---------------------------------------------------------------- helpers

	protected static bool IsBand(ACR_Squad squad, string faction)
	{
		if (!squad || squad.CountAlive() <= 0 || squad.PlayerSteers()) return false;
		if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction) return false;
		return true;
	}

	protected static int CountBands(ACR_Manager manager, string faction)
	{
		int n;
		foreach (ACR_Squad squad : manager.m_Squads)
			if (IsBand(squad, faction)) n++;
		return n;
	}

	protected static ACR_Squad FreeFootBand(ACR_Manager manager, string faction, int minAlive)
	{
		foreach (ACR_Squad squad : manager.m_Squads)
		{
			if (!IsBand(squad, faction) || squad.CountAlive() < minAlive || !ACR_Fortify.Infantry(squad)) continue;
			if (ACR_HideoutHomes.IsSettingUp(squad)) continue;
			if (squad.m_Ambush && !squad.m_Ambush.m_bFaded) continue;
			return squad;
		}
		return null;
	}

	protected static void SetBandRole(ACR_Squad squad, string role, ACR_Base town)
	{
		if (!squad) return;
		s_BandRole.Set(squad, role);
		s_BandOrderedAt.Set(squad, GetGame().GetWorld().GetWorldTime());
		if (town) s_BandTown.Set(squad, town);
		else s_BandTown.Remove(squad);
	}

	protected static void PruneBands(ACR_Manager manager)
	{
		array<ACR_Squad> dead = {};
		foreach (ACR_Squad sq, string unused : s_BandRole)
			if (!sq || !manager.m_Squads.Contains(sq) || sq.CountAlive() <= 0) dead.Insert(sq);
		foreach (ACR_Squad gone : dead)
		{
			s_BandRole.Remove(gone);
			s_BandOrderedAt.Remove(gone);
			s_BandTown.Remove(gone);
		}
	}
}
